#include "lcd35_test.h"
#include "lcd_test_pattern.h"
#include "board_config.h"
#include "watch_ui35.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <limits.h>
#include <stdlib.h>
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>

enum { WIDTH = WATCH35_WIDTH, HEIGHT = WATCH35_HEIGHT, ROWS = 16, STRIP_BYTES = WIDTH * ROWS * 2 };

static bool color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *user)
{
    (void)io;
    (void)event;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user, &wake);
    return wake == pdTRUE;
}

static esp_err_t panel_init(esp_lcd_panel_io_handle_t io)
{
    // Electrical settings checked against the standalone Waveshare 3.5-inch LCD Pi demo.
    static const struct { uint8_t cmd, size, data[14]; uint16_t delay_ms; } init[] = {
        {0x11, 0, {0}, 120},
        {0x36, 1, {0x48}, 0},  // MX=1 mirrors X; preserve BGR.
        {0x3A, 1, {0x05}, 0},  // 16-bit pixels on the SPI interface.
        {0xF0, 1, {0xC3}, 0}, {0xF0, 1, {0x96}, 0},
        {0xB4, 1, {0x01}, 0}, {0xB7, 1, {0xC6}, 0},
        {0xC0, 2, {0x80, 0x45}, 0}, {0xC1, 1, {0x13}, 0},
        {0xC2, 1, {0xA7}, 0}, {0xC5, 1, {0x0A}, 0},
        {0xE8, 8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 0},
        {0xE0, 14, {0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31}, 0},
        {0xE1, 14, {0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32}, 0},
        {0xF0, 1, {0x3C}, 0}, {0xF0, 1, {0x69}, 120},
        {0x21, 0, {0}, 0}, {0x29, 0, {0}, 20},
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); ++i) {
        esp_err_t err = esp_lcd_panel_io_tx_param(io, init[i].cmd, init[i].data, init[i].size);
        if (err != ESP_OK) return err;
        if (init[i].delay_ms) vTaskDelay(pdMS_TO_TICKS(init[i].delay_ms));
    }
    return ESP_OK;
}

static esp_err_t send_rect(esp_lcd_panel_io_handle_t io, SemaphoreHandle_t done,
                           int x0, int y0, int x1, int y1, const uint8_t *pixels, size_t len)
{
    const uint8_t columns[] = {x0 >> 8, x0 & 255, x1 >> 8, x1 & 255};
    const uint8_t rows[] = {y0 >> 8, y0 & 255, y1 >> 8, y1 & 255};
    esp_err_t err = esp_lcd_panel_io_tx_param(io, 0x2A, columns, sizeof(columns));
    if (err == ESP_OK) err = esp_lcd_panel_io_tx_param(io, 0x2B, rows, sizeof(rows));
    if (err == ESP_OK) err = esp_lcd_panel_io_tx_color(io, 0x2C, pixels, len);
    if (err != ESP_OK) return err;
    return xSemaphoreTake(done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}


static esp_err_t draw(esp_lcd_panel_io_handle_t io, SemaphoreHandle_t done,
                      uint8_t *pixels, bool pattern, uint16_t color)
{
    for (int y0 = 0; y0 < HEIGHT; y0 += ROWS) {
        for (int y = 0; y < ROWS; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                // Preserve the orientation markers and full border of the known test pattern.
                uint16_t c = pattern ? lcd_test_pattern_pixel(x * 240 / WIDTH, (y0 + y) * 240 / HEIGHT) : color;
                unsigned offset = (y * WIDTH + x) * 2;
                pixels[offset] = c >> 8;
                pixels[offset + 1] = c & 255;
            }
        }
        esp_err_t err = send_rect(io, done, 0, y0, WIDTH - 1, y0 + ROWS - 1, pixels, STRIP_BYTES);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}


typedef struct {
    esp_lcd_panel_io_handle_t io;
    SemaphoreHandle_t done;
    uint8_t *pixels;
} touch_display_t;

typedef struct { int x, y; } point_t;

static const point_t targets[] = {
    {40, 40}, {WIDTH - 41, 40}, {WIDTH - 41, HEIGHT - 41}, {40, HEIGHT - 41},
};

static bool map_touch(point_t raw, unsigned transform, point_t *screen)
{
    const int x = transform & 1 ? raw.y : raw.x;
    const int y = transform & 1 ? raw.x : raw.y;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
    screen->x = transform & 2 ? WIDTH - 1 - x : x;
    screen->y = transform & 4 ? HEIGHT - 1 - y : y;
    return true;
}

static int find_transform(const point_t measured[4])
{
    int best = -1;
    int best_error = INT_MAX;
    for (unsigned i = 0; i < 8; ++i) {
        int error = 0;
        bool valid = true;
        for (unsigned j = 0; j < 4; ++j) {
            point_t screen;
            if (!map_touch(measured[j], i, &screen)) { valid = false; break; }
            int dx = screen.x - targets[j].x;
            int dy = screen.y - targets[j].y;
            if (abs(dx) > 55 || abs(dy) > 55) { valid = false; break; }
            error += dx * dx + dy * dy;
        }
        if (valid && error < best_error) { best_error = error; best = (int)i; }
    }
    return best;
}

static esp_err_t draw_marker(const touch_display_t *lcd, point_t p, bool target, bool erase)
{
    const int r = target ? 17 : 11;
    const int x0 = p.x >= r ? p.x - r : 0;
    const int y0 = p.y >= r ? p.y - r : 0;
    const int x1 = p.x + r < WIDTH ? p.x + r : WIDTH - 1;
    const int y1 = p.y + r < HEIGHT ? p.y + r : HEIGHT - 1;
    size_t n = 0;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            uint16_t c = lcd_test_pattern_pixel(x * 240 / WIDTH, y * 240 / HEIGHT);
            if (!erase) {
                int dx = abs(x - p.x);
                int dy = abs(y - p.y);
                if (target) {
                    int rr = dx * dx + dy * dy;
                    if (rr >= 10 * 10 && rr <= 17 * 17) {
                        c = rr >= 12 * 12 && rr <= 15 * 15 ? 0xFFE0 : 0;
                    }
                } else if (dx <= 2 || dy <= 2) {
                    c = dx <= 1 || dy <= 1 ? 0xFFFF : 0;
                }
            }
            lcd->pixels[n++] = c >> 8;
            lcd->pixels[n++] = c & 255;
        }
    }
    return send_rect(lcd->io, lcd->done, x0, y0, x1, y1, lcd->pixels, n);
}

static esp_err_t start_ft6336u(i2c_master_bus_handle_t *bus, i2c_master_dev_handle_t *dev)
{
    const gpio_config_t rst = {
        .pin_bit_mask = 1ULL << BOARD_LCD35_TP_RST_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    const gpio_config_t irq = {
        .pin_bit_mask = 1ULL << BOARD_LCD35_TP_INT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    esp_err_t err = gpio_config(&rst);
    if (err == ESP_OK) err = gpio_config(&irq);
    if (err != ESP_OK) return err;
    gpio_set_level(BOARD_LCD35_TP_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(25));
    gpio_set_level(BOARD_LCD35_TP_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(220));

    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0, .sda_io_num = BOARD_LCD35_TP_SDA_GPIO,
        .scl_io_num = BOARD_LCD35_TP_SCL_GPIO, .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = 1,
    };
    err = i2c_new_master_bus(&config, bus);
    if (err != ESP_OK) return err;
    err = i2c_master_probe(*bus, 0x38, 100);
    if (err != ESP_OK) return err;
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x38, .scl_speed_hz = 100000,
    };
    return i2c_master_bus_add_device(*bus, &device, dev);
}

typedef struct {
    touch_display_t lcd;
    i2c_master_dev_handle_t dev;
    unsigned transform;
} rev_b_port_t;

static esp_err_t read_contact(i2c_master_dev_handle_t dev, unsigned *contacts, unsigned *event, point_t *raw)
{
    const uint8_t status = 0x02;
    uint8_t data[5];
    esp_err_t err = i2c_master_transmit_receive(dev, &status, 1, data, sizeof(data), 30);
    if (err != ESP_OK) return err;
    *contacts = data[0] & 0x0F;
    *event = data[1] >> 6;
    raw->x = ((data[1] & 0x0F) << 8) | data[2];
    raw->y = ((data[3] & 0x0F) << 8) | data[4];
    return ESP_OK;
}

static esp_err_t ui_flush(void *ctx, int x0, int y0, int x1, int y1,
                          const uint8_t *pixels, size_t bytes)
{
    const rev_b_port_t *p = ctx;
    return send_rect(p->lcd.io, p->lcd.done, x0, y0, x1, y1, pixels, bytes);
}

static bool ui_touch(void *ctx, bool *pressed, int *x, int *y)
{
    const rev_b_port_t *p = ctx;
    *pressed = false;
    if (!p->dev) return true;  // BOOT navigation remains available.
    unsigned contacts, event;
    point_t raw, screen;
    if (read_contact(p->dev, &contacts, &event, &raw) != ESP_OK) return false;
    if (contacts == 0 || (contacts == 1 && event == 1)) return true;
    if (contacts != 1 || (event != 0 && event != 2) || !map_touch(raw, p->transform, &screen)) return false;
    *pressed = true;
    *x = screen.x; *y = screen.y;
    return true;
}

static esp_err_t calibrate_touch(rev_b_port_t *p)
{
    esp_err_t nvs_result = nvs_flash_init();
    nvs_handle_t handle;
    bool have_nvs = nvs_result == ESP_OK && nvs_open("watch35", NVS_READWRITE, &handle) == ESP_OK;
    uint8_t saved = 0;
    if (have_nvs && nvs_get_u8(handle, "tp_dir_v1", &saved) == ESP_OK && saved < 8) {
        p->transform = saved;
        nvs_close(handle);
        printf("# TOUCH35: loaded orientation=%u from NVS\n", saved);
        return ESP_OK;
    }

    const touch_display_t *lcd = &p->lcd;
    printf("# TOUCH35: first setup; tap each yellow target once and lift\n");
    esp_err_t err = draw(lcd->io, lcd->done, lcd->pixels, true, 0);
    if (err == ESP_OK) err = draw_marker(lcd, targets[0], true, false);
    unsigned next = 0, errors = 0;
    bool down = true;
    int transform = -1;
    point_t measured[4];
    while (err == ESP_OK) {
        unsigned count, event;
        point_t raw;
        esp_err_t io_err = read_contact(p->dev, &count, &event, &raw);
        if (io_err != ESP_OK) {
            if (++errors >= 5) { err = io_err; break; }
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        errors = 0;
        if (count == 0 || (count == 1 && event == 1)) {
            down = false;
            if (transform >= 0) break;  // Do not pass the last calibration tap to the UI.
        } else if (transform < 0 && !down && count == 1 && (event == 0 || event == 2)) {
            printf("# TOUCH35: corner=%u raw=%d,%d\n", next + 1, raw.x, raw.y);
            measured[next] = raw;
            err = draw_marker(lcd, targets[next++], true, true);
            if (next == 4) {
                transform = find_transform(measured);
                if (transform < 0) {
                    printf("# TOUCH35: no orientation matched; retry targets\n");
                    next = 0;
                }
            }
            if (err == ESP_OK && transform < 0) err = draw_marker(lcd, targets[next], true, false);
            down = true;
        } else {
            down = true;
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    if (err == ESP_OK) {
        p->transform = (unsigned)transform;
        esp_err_t save = have_nvs ? nvs_set_u8(handle, "tp_dir_v1", (uint8_t)transform) : ESP_ERR_INVALID_STATE;
        if (save == ESP_OK) save = nvs_commit(handle);
        printf("# TOUCH35: orientation=%u %s\n", p->transform,
               save == ESP_OK ? "saved to NVS" : "not saved; setup required after reboot");
    }
    if (have_nvs) nvs_close(handle);
    return err;
}

static void touch_test_task(void *arg)
{
    rev_b_port_t port = {.lcd = *(touch_display_t *)arg};
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = start_ft6336u(&bus, &port.dev);
    if (err == ESP_OK) {
        printf("# TOUCH35: FT6336U ACK at 0x38\n");
        err = calibrate_touch(&port);
    } else {
        printf("# TOUCH35: %s; falling back to BOOT navigation\n", esp_err_to_name(err));
        port.dev = NULL;
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        const watch_ui35_port_t ui = {.context = &port, .flush = ui_flush, .touch = ui_touch};
        err = watch_ui35_run(&ui);
    }
    printf("# UI35: stopped: %s; heartbeat continues\n", esp_err_to_name(err));
    if (port.dev) i2c_master_bus_rm_device(port.dev);
    if (bus) i2c_del_master_bus(bus);
    if (esp_lcd_panel_io_del(port.lcd.io) == ESP_OK) {
        spi_bus_free(SPI2_HOST);
        vSemaphoreDelete(port.lcd.done);
        heap_caps_free(port.lcd.pixels);
    }
    vTaskDelete(NULL);
}


esp_err_t lcd35_test_run(void)
{
    const gpio_config_t control = {
        .pin_bit_mask = (1ULL << BOARD_LCD35_RST_GPIO) | (1ULL << BOARD_LCD35_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&control);
    if (err != ESP_OK) return err;
    gpio_set_level(BOARD_LCD35_BL_GPIO, 0);
    gpio_set_level(BOARD_LCD35_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(BOARD_LCD35_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(BOARD_LCD35_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t *pixels = heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    esp_lcd_panel_io_handle_t io = NULL;
    bool bus_ready = false;
    if (!pixels || !done) {
        err = ESP_ERR_NO_MEM;
        goto release;
    }
    const spi_bus_config_t bus = {
        .mosi_io_num = BOARD_LCD_MOSI_GPIO, .miso_io_num = -1,
        .sclk_io_num = BOARD_LCD_SCK_GPIO, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = WIDTH * WATCH35_DRAW_ROWS * 2,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) goto release;
    bus_ready = true;
    const esp_lcd_panel_io_spi_config_t config = {
        .cs_gpio_num = BOARD_LCD_CS_GPIO, .dc_gpio_num = BOARD_LCD_DC_GPIO,
        .spi_mode = 0, .pclk_hz = BOARD_LCD35_TEST_CLOCK_HZ,
        .trans_queue_depth = 1, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .on_color_trans_done = color_done, .user_ctx = done,
    };
    err = esp_lcd_new_panel_io_spi(SPI2_HOST, &config, &io);
    if (err != ESP_OK) goto release;
    err = panel_init(io);
    if (err != ESP_OK) goto release;
    printf("# LCD35: ST7796S 320x480, SPI mode 0, %d Hz; Rev B touch UI\n", BOARD_LCD35_TEST_CLOCK_HZ);
    gpio_set_level(BOARD_LCD35_BL_GPIO, 1);
    if (err == ESP_OK) {
        static touch_display_t touch_display;
        touch_display = (touch_display_t){.io = io, .done = done, .pixels = pixels};
        if (xTaskCreate(touch_test_task, "lcd35_ui", 8192, &touch_display, 1, NULL) == pdPASS) {
            return ESP_OK;  // The task now owns the display resources.
        }
        err = ESP_ERR_NO_MEM;
    }

release:
    // Removing panel IO drains queued SPI transfers before freeing their buffers.
    if (io && esp_lcd_panel_io_del(io) != ESP_OK) {
        return err == ESP_OK ? ESP_FAIL : err;
    }
    if (bus_ready) spi_bus_free(SPI2_HOST);
    if (done) vSemaphoreDelete(done);
    heap_caps_free(pixels);
    return err;
}
