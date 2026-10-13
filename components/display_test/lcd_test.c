#include "lcd_test.h"
#include "lcd_test_pattern.h"
#include "display_panel.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
static uint8_t *pixels;
static esp_err_t render(bool pattern,uint16_t solid)
{
    for(int top=0;top<240;top+=20) {
        for(int y=0;y<20;y++)for(int x=0;x<240;x++) {
            uint16_t c=pattern ? lcd_test_pattern_pixel(x,top+y) : solid;
            unsigned i=(y*240+x)*2;pixels[i]=c>>8;pixels[i+1]=c&255;
        }
        esp_err_t err=display_panel_flush_rgb565(0,top,239,top+19,pixels);
        if(err!=ESP_OK)return err;
    }
    return ESP_OK;
}
static void task(void *arg)
{
    (void)arg;esp_err_t err=ESP_OK;
    const uint16_t colors[]={0xF800,0x07E0,0x001F};
    for(unsigned i=0;i<3 && err==ESP_OK;i++) {err=render(false,colors[i]);vTaskDelay(pdMS_TO_TICKS(700));}
    if(err==ESP_OK)err=render(true,0);
    printf("# LCD: diagnostic pattern result=%s; verify direction and RGB visually\n",esp_err_to_name(err));
    // Keep storage alive even if a DMA completion is uncertain.
    vTaskDelete(NULL);
}
esp_err_t lcd_test_start(void)
{
    if(pixels)return ESP_ERR_INVALID_STATE;
    esp_err_t err=display_panel_init();if(err!=ESP_OK)return err;
    pixels=heap_caps_malloc(240*20*2,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    if(!pixels)return ESP_ERR_NO_MEM;
    return xTaskCreate(task,"lcd_test",4096,NULL,1,NULL)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
