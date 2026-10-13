#include "display_panel.h"
#include "board_config.h"
#include <stdio.h>
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define WIDTH 240
#define HEIGHT 240
#define STRIP_ROWS 40
static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t transfer_done;

static bool bus_installed;

static bool color_done(esp_lcd_panel_io_handle_t handle,esp_lcd_panel_io_event_data_t *event,void *user)
{
    (void)handle;(void)event;
    BaseType_t awakened=pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user,&awakened);
    return awakened==pdTRUE;
}
static esp_err_t parameter(uint8_t command,const uint8_t *data,size_t size)
{
    return esp_lcd_panel_io_tx_param(io,command,data,size);
}
static esp_err_t initialize_panel(void)
{
    esp_err_t err=parameter(0x01,NULL,0); // Software reset: no exposed RST on V1.3 connector.
    if(err!=ESP_OK)return err;
    vTaskDelay(pdMS_TO_TICKS(150));
    err=parameter(0x11,NULL,0);
    if(err!=ESP_OK)return err;
    vTaskDelay(pdMS_TO_TICKS(120));
    // Register values from the supplied TK013F2261 display profile, independent driver.
    static const struct {uint8_t command,length,data[14];} settings[]={
        {0xB2,5,{0x1F,0x1F,0x00,0x33,0x33}},
        {0xB7,1,{0x12}},{0xBB,1,{0x66}}, {0xC0,1,{0x2C}},
        {0xC2,1,{0x01}},{0xC3,1,{0x15}},{0xC4,1,{0x20}},
        {0xC6,1,{0x13}},{0xD0,2,{0xA4,0xA1}},{0xD6,1,{0xA1}},
        {0xE0,14,{0xF0,0x06,0x0D,0x0B,0x0A,0x07,0x2E,0x43,0x45,0x38,0x14,0x13,0x25,0x29}},
        {0xE1,14,{0xF0,0x07,0x0A,0x08,0x07,0x23,0x2E,0x33,0x44,0x3A,0x16,0x17,0x26,0x2C}},
        {0x3A,1,{0x55}}, // RGB565 / 16-bit pixels.
    };
    for(size_t j=0;j<sizeof(settings)/sizeof(settings[0]);++j) {
        err=parameter(settings[j].command,settings[j].data,settings[j].length);
        if(err!=ESP_OK)return err;
    }
    uint8_t access=(BOARD_LCD_BGR ? 0x08 : 0) |
                   (BOARD_LCD_MIRROR_X ? 0x40 : 0) |
                   (BOARD_LCD_MIRROR_Y ? 0x80 : 0);
    err=parameter(0x36,&access,1);
    if(err==ESP_OK)err=parameter(BOARD_LCD_INVERT ? 0x21 : 0x20,NULL,0);
    if(err==ESP_OK)err=parameter(0x29,NULL,0);
    vTaskDelay(pdMS_TO_TICKS(20));
    return err;
}
static void release_setup(void)
{
    if(io) {esp_lcd_panel_io_del(io);io=NULL;}
    if(bus_installed) {spi_bus_free(SPI2_HOST);bus_installed=false;}
    if(transfer_done) {vSemaphoreDelete(transfer_done);transfer_done=NULL;}
}
esp_err_t display_panel_init(void)
{
    if(io)return ESP_ERR_INVALID_STATE;
    transfer_done=xSemaphoreCreateBinary();
    if(!transfer_done) {release_setup();return ESP_ERR_NO_MEM;}
    const spi_bus_config_t bus={.mosi_io_num=BOARD_LCD_MOSI_GPIO,.miso_io_num=-1,
        .sclk_io_num=BOARD_LCD_SCK_GPIO,.quadwp_io_num=-1,.quadhd_io_num=-1,
        .max_transfer_sz=WIDTH*STRIP_ROWS*2};
    esp_err_t err=spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO);
    if(err!=ESP_OK) {release_setup();return err;}
    bus_installed=true;
    const esp_lcd_panel_io_spi_config_t config={.cs_gpio_num=BOARD_LCD_CS_GPIO,
        .dc_gpio_num=BOARD_LCD_DC_GPIO,.spi_mode=0,.pclk_hz=BOARD_LCD_CLOCK_HZ,
        .trans_queue_depth=1,.lcd_cmd_bits=8,.lcd_param_bits=8,
        .on_color_trans_done=color_done,.user_ctx=transfer_done};
    err=esp_lcd_new_panel_io_spi(SPI2_HOST,&config,&io);
    if(err!=ESP_OK) {release_setup();return err;}
    printf("# LCD: TK013F2261 V1.3, 240x240, SCK=IO12 MOSI=IO11 CS=IO13 DC=IO14, clock=%dHz; software reset, no BL GPIO\n",BOARD_LCD_CLOCK_HZ);
    err=initialize_panel();
    if(err!=ESP_OK)release_setup();
    return err;
}

esp_err_t display_panel_flush_rgb565(int x0,int y0,int x1,int y1,const void *msb_pixels)
{
    if(!io || !msb_pixels || x0<0 || y0<0 || x1>=WIDTH || y1>=HEIGHT || x1<x0 || y1<y0 || (x1-x0+1)*(y1-y0+1)>WIDTH*STRIP_ROWS)
        return ESP_ERR_INVALID_ARG;
    unsigned left=x0+BOARD_LCD_X_GAP,right=x1+BOARD_LCD_X_GAP;
    unsigned top=y0+BOARD_LCD_Y_GAP,bottom=y1+BOARD_LCD_Y_GAP;
    const uint8_t columns[]={left>>8,left&255,right>>8,right&255};
    const uint8_t rows[]={top>>8,top&255,bottom>>8,bottom&255};
    esp_err_t err=parameter(0x2A,columns,sizeof(columns));
    if(err==ESP_OK)err=parameter(0x2B,rows,sizeof(rows));
    if(err==ESP_OK)err=esp_lcd_panel_io_tx_color(io,0x2C,msb_pixels,(x1-x0+1)*(y1-y0+1)*2);
    if(err!=ESP_OK)return err;
    // Storage remains owned by the caller until DMA completion.
    return xSemaphoreTake(transfer_done,pdMS_TO_TICKS(1000))==pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
