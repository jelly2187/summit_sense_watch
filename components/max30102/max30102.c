#include "max30102.h"
#include "board_config.h"

#include <stdio.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



#define MAX30102_ADDRESS 0x57
#define MAX30102_PART_ID_REG 0xFF
#define MAX30102_REV_ID_REG 0xFE
#define MAX30102_EXPECTED_PART_ID 0x15
#define I2C_TIMEOUT_MS 100

esp_err_t max30102_probe(void)
{
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_dev_handle_t device = NULL;
    esp_err_t result;

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        // The Wildfire module already provides external pull-up resistors.
        .flags.enable_internal_pullup = false,
    };
    printf("# MAX30102: SDA=IO%d, SCL=IO%d, address=0x%02X, speed=%d Hz\n",
           BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO, MAX30102_ADDRESS, BOARD_I2C_SPEED_HZ);
    result = i2c_new_master_bus(&bus_config, &bus);
    if (result != ESP_OK) {
        printf("# MAX30102: bus initialization failed: %s\n", esp_err_to_name(result));
        return result;
    }

    // Allow module power-up; retry with a bounded delay for loose/startup connections.
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int attempt = 1; attempt <= 3; ++attempt) {
        result = i2c_master_probe(bus, MAX30102_ADDRESS, I2C_TIMEOUT_MS);
        if (result == ESP_OK) {
            break;
        }
        printf("# MAX30102: probe attempt %d failed: %s\n",
               attempt, esp_err_to_name(result));
        if (attempt < 3) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    if (result != ESP_OK) {
        printf("# MAX30102: CHECK FAILED; check 3V3, GND, SDA(IO%d), SCL(IO%d). Heartbeat continues.\n",
               BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO);
        goto cleanup;
    }
    printf("# MAX30102: address 0x57 ACK\n");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX30102_ADDRESS,
        .scl_speed_hz = BOARD_I2C_SPEED_HZ,
    };
    result = i2c_master_bus_add_device(bus, &device_config, &device);
    if (result != ESP_OK) {
        printf("# MAX30102: add device failed: %s\n", esp_err_to_name(result));
        goto cleanup;
    }

    uint8_t reg = MAX30102_PART_ID_REG;
    uint8_t part_id = 0;
    uint8_t revision_id = 0;
    result = i2c_master_transmit_receive(device, &reg, 1, &part_id, 1, I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        printf("# MAX30102: PART_ID read failed: %s\n", esp_err_to_name(result));
        goto cleanup;
    }
    reg = MAX30102_REV_ID_REG;
    result = i2c_master_transmit_receive(device, &reg, 1, &revision_id, 1, I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        printf("# MAX30102: REV_ID read failed: %s\n", esp_err_to_name(result));
        goto cleanup;
    }
    printf("# MAX30102: PART_ID=0x%02X (expected 0x15), REV_ID=0x%02X\n",
           (unsigned)part_id, (unsigned)revision_id);
    if (part_id != MAX30102_EXPECTED_PART_ID) {
        result = ESP_ERR_INVALID_RESPONSE;
        printf("# MAX30102: CHECK FAILED; unexpected PART_ID. Heartbeat continues.\n");
        goto cleanup;
    }
    printf("# MAX30102: communication check PASSED; sampling not enabled\n");

cleanup:
    // This is a one-shot check. Release resources before the heartbeat starts.
    if (device != NULL) {
        esp_err_t cleanup_result = i2c_master_bus_rm_device(device);
        if (cleanup_result != ESP_OK) {
            printf("# MAX30102: remove device failed: %s\n", esp_err_to_name(cleanup_result));
            return cleanup_result;
        }
    }
    if (bus != NULL) {
        esp_err_t cleanup_result = i2c_del_master_bus(bus);
        if (cleanup_result != ESP_OK) {
            printf("# MAX30102: delete bus failed: %s\n", esp_err_to_name(cleanup_result));
            if (result == ESP_OK) {
                result = cleanup_result;
            }
        }
    }
    return result;
}
