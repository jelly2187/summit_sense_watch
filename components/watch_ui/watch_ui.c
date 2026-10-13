#include "watch_ui.h"
#include "watch_pages.h"
#include "display_panel.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define BOOT_GPIO GPIO_NUM_0
#define BUFFER_BYTES (240*40*2)
static bool started,failed;
static uint32_t tick_ms(void) {return (uint32_t)(esp_timer_get_time()/1000);}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{
    if(failed)return; // Keep outstanding storage alive; stop rendering after any transfer failure.
    lv_draw_sw_rgb565_swap(pixels,lv_area_get_size(area));
    esp_err_t err=display_panel_flush_rgb565(area->x1,area->y1,area->x2,area->y2,pixels);
    if(err!=ESP_OK) {
        failed=true;printf("# UI: display transfer failed: %s; UI paused, sensor tasks continue\n",esp_err_to_name(err));
        // Deleting this sole LVGL owner here prevents LVGL from busy-waiting on
        // flushing forever or touching a buffer still owned by uncertain DMA.
        vTaskDelete(NULL);
        return;
    }
    lv_display_flush_ready(display);
}
static void ui_task(void *argument)
{
    (void)argument;
    lv_init();lv_tick_set_cb(tick_ms);
    void *a=heap_caps_malloc(BUFFER_BYTES,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    void *b=heap_caps_malloc(BUFFER_BYTES,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    if(!a || !b) {heap_caps_free(a);heap_caps_free(b);printf("# UI: draw buffer allocation failed\n");vTaskDelete(NULL);return;}
    lv_display_t *display=lv_display_create(240,240);
    if(!display) {heap_caps_free(a);heap_caps_free(b);printf("# UI: display allocation failed\n");vTaskDelete(NULL);return;}
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,a,b,BUFFER_BYTES,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    unsigned page=0;bool autoplay=true;
    bool raw=false,stable=false,long_done=false;
    uint32_t start=tick_ms(),last_page=start,changed=start,pressed_at=start;
    watch_pages_show(page,true,autoplay);
    printf("# UI: DEMO ONLY; BOOT short=next page, hold 1s=toggle autoplay, interval=8s\n");
    while(!failed) {
        uint32_t now=tick_ms();bool down=gpio_get_level(BOOT_GPIO)==0;
        if(down!=raw) {raw=down;changed=now;}
        if(raw!=stable && now-changed>=40) {
            stable=raw;
            if(stable) {pressed_at=now;long_done=false;}
            else if(!long_done) {
                page=(page+1)%WATCH_PAGE_COUNT;last_page=now;
                watch_pages_show(page,true,autoplay);
                printf("# UI: page=%u %s\n",page,watch_pages_name(page));
            }
        }
        if(stable && !long_done && now-pressed_at>=1000) {
            long_done=true;autoplay=!autoplay;last_page=now;
            watch_pages_show(page,true,autoplay);
            printf("# UI: autoplay=%d\n",autoplay);
        }
        if(autoplay && !stable && now-last_page>=8000) {
            page=(page+1)%WATCH_PAGE_COUNT;last_page=now;watch_pages_show(page,true,autoplay);
        }
        watch_pages_tick((now-start)/1000);
        lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(20));
    }
    // Never free DMA buffers after an uncertain transfer. A reset recovers the panel.
    vTaskDelete(NULL);
}
esp_err_t watch_ui_start(void)
{
    if(started)return ESP_ERR_INVALID_STATE;
    const gpio_config_t button={.pin_bit_mask=1ULL<<BOOT_GPIO,.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    esp_err_t err=gpio_config(&button);if(err!=ESP_OK)return err;
    err=display_panel_init();if(err!=ESP_OK)return err;
    started=true;
    return xTaskCreate(ui_task,"watch_ui",8192,NULL,1,NULL)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
