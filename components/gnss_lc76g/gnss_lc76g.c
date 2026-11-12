#include "gnss_lc76g.h"
#include "nmea_rmc.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
static int32_t lat_e7,lon_e7;
static int64_t last_fix;
static bool valid,started;

static void receive(void *arg)
{
    (void)arg;
    char line[112];
    unsigned len=0;
    uint8_t buf[128];
    for(;;) {
        int n=uart_read_bytes(UART_NUM_2,buf,sizeof(buf),pdMS_TO_TICKS(200));
        for(int i=0;i<n;++i) {
            unsigned c=buf[i];
            if(c=='$') {line[0]='$';len=1;}
            else if(!len) continue;
            else if(c=='\r'||c=='\n') {
                line[len]=0;len=0;
                int32_t lat=0,lon=0;
                nmea_rmc_result_t r=nmea_rmc_parse(line,&lat,&lon);
                if(r==NMEA_IGNORED)continue;
                int64_t now=esp_timer_get_time();
                portENTER_CRITICAL(&guard);
                valid=r==NMEA_FIX;lat_e7=lat;lon_e7=lon;last_fix=now;
                portEXIT_CRITICAL(&guard);
            } else if(c>=32 && c<=126 && len<sizeof(line)-1) line[len++]=(char)c;
            else len=0;
        }
    }
}

bool gnss_lc76g_fix(int32_t *lat,int32_t *lon)
{
    int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&guard);
    bool ready=valid && now-last_fix<5000000;
    *lat=lat_e7;*lon=lon_e7;
    portEXIT_CRITICAL(&guard);
    return ready;
}

esp_err_t gnss_lc76g_start(void)
{
    if(started)return ESP_OK;
    const uart_config_t cfg={.baud_rate=115200,.data_bits=UART_DATA_8_BITS,
        .parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,
        .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
    esp_err_t err=uart_param_config(UART_NUM_2,&cfg);
    if(err==ESP_OK)err=uart_set_pin(UART_NUM_2,UART_PIN_NO_CHANGE,BOARD_LC76G_RX_GPIO,
                                  UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    if(err==ESP_OK)err=gpio_pullup_en(BOARD_LC76G_RX_GPIO);
    if(err==ESP_OK)err=uart_driver_install(UART_NUM_2,2048,0,0,NULL,0);
    if(err!=ESP_OK)return err;
    if(xTaskCreate(receive,"lc76g_rx",3072,NULL,1,NULL)!=pdPASS) {
        uart_driver_delete(UART_NUM_2);return ESP_ERR_NO_MEM;
    }
    started=true;
    printf("# GNSS: LC76G UART2 RX IO%d 115200; waiting for valid RMC\n",BOARD_LC76G_RX_GPIO);
    return ESP_OK;
}
