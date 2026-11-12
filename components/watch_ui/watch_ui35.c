#include "watch_ui35.h"
#include "watch_pages.h"
#include "watch_map.h"
#include "gnss_lc76g.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct { const watch_ui35_port_t *port; esp_err_t error; } render_t;
static uint32_t tick_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *pixels)
{
    render_t *state = lv_display_get_user_data(display);
    if (state->error == ESP_OK) {
        uint32_t count = lv_area_get_size(a);
        lv_draw_sw_rgb565_swap(pixels, count);
        state->error = state->port->flush(state->port->context, a->x1, a->y1, a->x2, a->y2, pixels, count * 2);
        if (state->error != ESP_OK) {
            // Stop the only LVGL task: an outstanding DMA may still own the buffer.
            printf("# UI35: display transfer stopped: %s\n", esp_err_to_name(state->error));
            vTaskDelete(NULL);
            return;
        }
    }
    lv_display_flush_ready(display);
}

esp_err_t watch_ui35_run(const watch_ui35_port_t *port)
{
    const gpio_config_t boot = {.pin_bit_mask = 1ULL << GPIO_NUM_0, .mode = GPIO_MODE_INPUT,
                                .pull_up_en = GPIO_PULLUP_ENABLE};
    esp_err_t err = gpio_config(&boot);
    if (err != ESP_OK) return err;
    const size_t bytes = WATCH35_WIDTH * WATCH35_DRAW_ROWS * 2;
    void *buffer = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buffer) return ESP_ERR_NO_MEM;
    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *display = lv_display_create(WATCH35_WIDTH, WATCH35_HEIGHT);
    if (!display) { heap_caps_free(buffer); return ESP_ERR_NO_MEM; }
    render_t render = {.port = port, .error = ESP_OK};
    lv_display_set_user_data(display, &render);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);

    esp_err_t gnss = gnss_lc76g_start();
    if (gnss != ESP_OK) printf("# GNSS: UART unavailable: %s\n", esp_err_to_name(gnss));
    uint32_t last_fix_refresh = 0;
    unsigned page = 0;
    bool autoplay = false, touch_down = false, touch_blocked = false;
    int start_x = 0, start_y = 0, last_x = 0, last_y = 0;
    bool boot_raw = false, boot_down = false, boot_long = false;
    uint32_t start = tick_ms(), last_page = start, boot_changed = start, boot_at = start;
    watch_pages_show(page, true, autoplay);
    printf("# UI35: 320x480 demo; tap footer arrows or swipe; BOOT short=next, hold=autoplay\n");
    while (render.error == ESP_OK) {
        uint32_t now = tick_ms();
        int direction = 0, jump = -1;
        bool redraw = false, pressed = false;
        int x = 0, y = 0;
        if (!port->touch(port->context, &pressed, &x, &y)) {
            touch_down = false;
            touch_blocked = true;
        } else if (pressed && !touch_blocked) {
            if (!touch_down) { start_x = x; start_y = y; }
            touch_down = true;
            last_x = x; last_y = y;
        } else if (!pressed) {
            if (touch_down) {
                int dx = last_x - start_x, dy = last_y - start_y;
                if (watch_map_touch(start_x, start_y, last_x, last_y)) {last_page = now;}
                else if (abs(dx) >= 65 && abs(dx) > abs(dy) * 2) direction = dx < 0 ? 1 : -1;
                else if (abs(dx) < 25 && abs(dy) < 25 && start_y >= WATCH35_NAV_TOP &&
                         start_y < WATCH35_NAV_BOTTOM && last_y >= WATCH35_NAV_TOP &&
                         last_y < WATCH35_NAV_BOTTOM) {
                    if (start_x < 100 && last_x < 100) direction = -1;
                    if (start_x >= 220 && last_x >= 220) direction = 1;
                } else if (abs(dx) < 25 && abs(dy) < 25 && start_x >= 16 && start_x < 304 &&
                           last_x >= 16 && last_x < 304) {
                    if (page == 0 && start_y >= 238 && start_y < 338 && last_y >= 238 && last_y < 338) {
                        if (start_x < 154 && last_x < 154) jump = 1;
                        else if (start_x >= 166 && last_x >= 166) jump = 3;
                    } else if (page == 6 && start_y >= 82 && start_y < 171 && last_y >= 82 && last_y < 171) {
                        autoplay = !autoplay;
                        redraw = true;
                        last_page = now;
                    }
                }
            }
            touch_down = touch_blocked = false;
        }
        bool raw = gpio_get_level(GPIO_NUM_0) == 0;
        if (raw != boot_raw) { boot_raw = raw; boot_changed = now; }
        if (raw != boot_down && now - boot_changed >= 40) {
            boot_down = raw;
            if (boot_down) { boot_at = now; boot_long = false; }
            else if (!boot_long) direction = 1;
        }
        if (boot_down && !boot_long && now - boot_at >= 1000) {
            boot_long = true; autoplay = !autoplay; redraw = true; last_page = now;
        }
        if (!direction && autoplay && page != 4 && !touch_down && !boot_down && now - last_page >= 8000) direction = 1;
        if (direction || jump >= 0) {
            page = jump >= 0 ? (unsigned)jump : (page + (direction > 0 ? 1 : WATCH_PAGE_COUNT - 1)) % WATCH_PAGE_COUNT;
            last_page = now;
            redraw = true;
            printf("# UI35: page=%u %s\n", page, watch_pages_name(page));
        }
        if (redraw) watch_pages_show(page, true, autoplay);
        if (now - last_fix_refresh >= 500) {
            int32_t lat, lon;
            bool valid = gnss_lc76g_fix(&lat, &lon);
            watch_map_fix(valid, lat, lon);
            last_fix_refresh = now;
        }
        watch_pages_tick((now - start) / 1000);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return render.error;
}
