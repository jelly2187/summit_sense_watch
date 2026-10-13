#include "max30102.h"
#include "board_config.h"
#include "physiology.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define ADDRESS 0x57
#define PERIOD_US 10000
#define TIMEOUT_MS 100
#define QUEUE_LENGTH 128

typedef struct {
    uint64_t seq;
    int64_t time_us;
    uint32_t red, ir, flags, fifo_lost_min, queue_lost, errors, epoch;
} sample_t;

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;
static QueueHandle_t queue;
// Single output task owns the analysis state; keep window buffers off task stack.
static physiology_context_t physiology;

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t size)
{
    return i2c_master_transmit_receive(device, &reg, 1, data, size, TIMEOUT_MS);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), TIMEOUT_MS);
}

static esp_err_t configure(void)
{
    esp_err_t err = write_reg(0x09, 0x40); // Software reset.
    if (err != ESP_OK) return err;
    uint8_t mode = 0x40;
    for (int i = 0; i < 20; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
        err = read_regs(0x09, &mode, 1);
        if (err != ESP_OK) return err;
        if (!(mode & 0x40)) break;
    }
    if (mode & 0x40) return ESP_ERR_TIMEOUT;

    // No averaging, no FIFO rollover; 17 unread frames set almost-full status.
    // ADC range 4096 nA, 100 paired samples/s, 411 us / 18-bit resolution.
    // LED current 0x24 * 0.2 mA = nominal 7.2 mA per channel.
    const uint8_t settings[][2] = {
        {0x02, 0x00}, {0x03, 0x00},
        {0x04, 0x00}, {0x05, 0x00}, {0x06, 0x00},
        {0x08, 0x0F}, {0x0A, 0x27}, {0x0C, 0x24}, {0x0D, 0x24},
        {0x09, 0x03}, // SpO2 mode: RED followed by IR; does not calculate SpO2.
    };
    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i) {
        err = write_reg(settings[i][0], settings[i][1]);
        if (err != ESP_OK) return err;
    }
    // Verify configuration rather than silently accepting unexpected settings.
    for (size_t i = 6; i < sizeof(settings) / sizeof(settings[0]); ++i) {
        uint8_t value = 0;
        err = read_regs(settings[i][0], &value, 1);
        if (err != ESP_OK) return err;
        if (value != settings[i][1]) return ESP_ERR_INVALID_RESPONSE;
    }
    uint8_t status[2];
    return read_regs(0x00, status, sizeof(status)); // Clear power-ready status before polling.
}

static uint32_t decode_channel(const uint8_t *data)
{
    return (((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2]) & 0x3FFFF;
}

static void output_task(void *arg)
{
    (void)arg;
    sample_t sample;
    physiology_init(&physiology);
    while (1) {
        if (xQueueReceive(queue, &sample, portMAX_DELAY) == pdTRUE) {
            printf("PPG,%" PRIu64 ",%" PRId64 ",%" PRIu32 ",%" PRIu32
                   ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\n",
                   sample.seq, sample.time_us, sample.red, sample.ir, sample.flags,
                   sample.fifo_lost_min, sample.queue_lost, sample.errors, sample.epoch);
            physiology_result_t result;
            if (physiology_feed(&physiology, sample.seq, sample.time_us, sample.red,
                                sample.ir, sample.flags, sample.epoch, &result)) {
                printf("METRIC,%" PRId64 ",%.2f,%u,%.2f,%u,0,%.1f,%.4f,%.3f,%s\n",
                       result.time_us, result.hr_bpm, (unsigned)result.hr_available,
                       result.spo2_est_pct, (unsigned)result.spo2_est_available,
                       result.sqi, result.ratio, result.pi_ir_pct,
                       physiology_reason_name(result.reason));
            }
        }
    }
}

static void acquire_task(void *arg)
{
    (void)arg;
    uint64_t seq = 0;
    uint32_t fifo_lost_min = 0, queue_lost = 0, errors = 0, epoch = 0;
    uint32_t pending_flags = 0;
    int64_t last_time = -1;
    bool recovery = false;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_err_t err;
        if (recovery) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            err = configure();
            if (err != ESP_OK) {
                ++errors;
                printf("# PPG: recovery failed: %s; heartbeat continues\n", esp_err_to_name(err));
                continue;
            }
            ++epoch;
            pending_flags |= 8; // Reinitialized: unknown acquisition gap.
            recovery = false;
            printf("# PPG: recovered, epoch=%" PRIu32 "\n", epoch);
        }
        uint8_t status[2], pointers[3];
        err = read_regs(0x00, status, sizeof(status));
        if (err == ESP_OK && (status[0] & 1)) {
            err = ESP_ERR_INVALID_STATE; // Sensor brownout reset its configuration.
        }
        if (err == ESP_OK) err = read_regs(0x04, pointers, sizeof(pointers));
        if (err != ESP_OK) goto failed;
        const int64_t snapshot_us = esp_timer_get_time();
        const uint8_t lost = pointers[1] & 31;
        size_t count = ((pointers[0] & 31) - (pointers[2] & 31) + 32) % 32;
        // With rollover disabled, equal pointers may denote a full 32-frame FIFO.
        if (count == 0 && (lost || (status[0] & 0x80))) count = 32;
        if (!count) continue;
        if (lost) {
            fifo_lost_min += lost; // Counter saturates at 31: lower bound, not exact loss.
            pending_flags |= 1;
            printf("# PPG: FIFO overflow, observed=%u, lost_min_total=%" PRIu32 "\n",
                   (unsigned)lost, fifo_lost_min);
        }
        uint8_t raw[32 * 6];
        err = read_regs(0x07, raw, count * 6);
        if (err != ESP_OK) goto failed;
        for (size_t i = 0; i < count; ++i) {
            sample_t sample = {
                .seq = seq++,
                // Estimated from the FIFO snapshot and nominal period, not chip timestamps.
                .time_us = snapshot_us - (int64_t)(count - 1 - i) * PERIOD_US,
                .red = decode_channel(raw + i * 6),
                .ir = decode_channel(raw + i * 6 + 3),
                .flags = pending_flags,
                .fifo_lost_min = fifo_lost_min,
                .queue_lost = queue_lost,
                .errors = errors,
                .epoch = epoch,
            };
            if (sample.time_us <= last_time) {
                sample.time_us = last_time + 1;
                sample.flags |= 16; // Timestamp adjusted to preserve monotonicity.
            }
            last_time = sample.time_us;
            if (sample.red == 0x3FFFF || sample.ir == 0x3FFFF) sample.flags |= 4;
            if (xQueueSend(queue, &sample, 0) != pdTRUE) {
                ++queue_lost;
                pending_flags |= 2;
            } else {
                pending_flags = 0;
            }
        }
        continue;
failed:
        ++errors;
        recovery = true; // Reset FIFO/config after uncertain or partial I2C reads.
        pending_flags |= 8;
        printf("# PPG: I2C/sensor error=%s, total=%" PRIu32 "; retrying in 1s\n",
               esp_err_to_name(err), errors);
    }
}

static void release_resources(void)
{
    if (device) { i2c_master_bus_rm_device(device); device = NULL; }
    if (bus) { i2c_del_master_bus(bus); bus = NULL; }
    if (queue) { vQueueDelete(queue); queue = NULL; }
}

esp_err_t max30102_start_sampling(void)
{
    if (bus || queue) return ESP_ERR_INVALID_STATE;
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_I2C_SDA_GPIO, .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    esp_err_t err = i2c_new_master_bus(&config, &bus);
    if (err != ESP_OK) return err;
    const i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = ADDRESS,
        .scl_speed_hz = BOARD_I2C_SPEED_HZ,
    };
    err = i2c_master_bus_add_device(bus, &dev_config, &device);
    if (err == ESP_OK) err = configure();
    if (err != ESP_OK) { release_resources(); return err; }
    queue = xQueueCreate(QUEUE_LENGTH, sizeof(sample_t));
    if (!queue) { write_reg(0x09, 0x80); release_resources(); return ESP_ERR_NO_MEM; }
    TaskHandle_t logger = NULL;
    if (xTaskCreate(output_task, "ppg_output", 4096, NULL, 3, &logger) != pdPASS) {
        write_reg(0x09, 0x80); release_resources(); return ESP_ERR_NO_MEM;
    }
    printf("# PPG: 100 Hz, RED+IR, 18-bit, average=1, ADC=4096nA, LED=7.2mA, polling=10ms\n");
    printf("# PPG_FIELDS: type,seq,time_us_est,red,ir,flags,fifo_lost_min,queue_lost,i2c_errors,epoch\n");
    printf("# METRIC_FIELDS: type,time_us,hr_bpm,hr_available,spo2_est_pct,spo2_est_available,calibrated,sqi,ratio,pi_ir_pct,reason\n");
    if (xTaskCreate(acquire_task, "ppg_acquire", 4096, NULL, 5, NULL) != pdPASS) {
        vTaskDelete(logger);
        write_reg(0x09, 0x80); release_resources(); return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
