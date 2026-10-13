#include "as6221.h"
#include "as6221_codec.h"
#include "board_config.h"
#include <inttypes.h>
#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;
static uint16_t config_readback;

static esp_err_t read_word(uint8_t reg,uint16_t *value)
{
    uint8_t bytes[2];
    esp_err_t err=i2c_master_transmit_receive(device,&reg,1,bytes,2,100);
    if(err==ESP_OK)*value=((uint16_t)bytes[0]<<8)|bytes[1];
    return err;
}
static esp_err_t configure(void)
{
    esp_err_t err=i2c_master_probe(bus,BOARD_AS6221_ADDRESS,100);
    if(err!=ESP_OK)return err;
    uint16_t original;
    err=read_word(0x01,&original);
    if(err!=ESP_OK)return err;
    // Preserve unrelated/RO fields; CR=10 (4 conversions/s), SM=0, SS=0.
    uint16_t requested=(original & (uint16_t)~0x81C0U)|0x0080U;
    const uint8_t bytes[]={0x01,(uint8_t)(requested>>8),(uint8_t)requested};
    err=i2c_master_transmit(device,bytes,sizeof(bytes),100);
    if(err==ESP_OK)err=read_word(0x01,&config_readback);
    if(err!=ESP_OK)return err;
    if((config_readback & 0x01C0U)!=0x0080U)return ESP_ERR_INVALID_RESPONSE;
    printf("# AS6221: address=0x%02X ACK, CONFIG=0x%04X, continuous conversion target=4Hz\n",
           BOARD_AS6221_ADDRESS,(unsigned)config_readback);
    // Avoid publishing the initial 0 C before a configured conversion has completed.
    vTaskDelay(pdMS_TO_TICKS(350));
    return ESP_OK;
}
static void temperature_task(void *arg)
{
    (void)arg;
    bool ready=false;
    uint64_t seq=0;
    uint32_t errors=0;
    int64_t next_retry=0;
    TickType_t last_wake=xTaskGetTickCount();
    while(1) {
        const char *status="NOT_READY";
        bool valid=false;
        float temperature=-999;
        int64_t now=esp_timer_get_time();
        if(!ready && now>=next_retry) {
            esp_err_t err=configure();
            if(err==ESP_OK)ready=true;
            else {
                ++errors;next_retry=esp_timer_get_time()+5000000;
                status=err==ESP_ERR_NOT_FOUND ? "NOT_FOUND" : "CONFIG_ERROR";
                printf("# AS6221: initialization=%s; check 3V3/GND SDA=IO4 SCL=IO5 ADD0/ALERT=GND; retry in 5s\n",esp_err_to_name(err));
            }
        }
        if(ready) {
            uint16_t value;
            esp_err_t err=read_word(0x00,&value);
            if(err==ESP_OK) {
                temperature=as6221_decode_temperature(value>>8,value & 255);
                valid=as6221_temperature_in_range(temperature);
                status=valid ? "OK" : "OUT_OF_RANGE";
            } else {
                ++errors;ready=false;next_retry=esp_timer_get_time()+5000000;
                status="I2C_ERROR";
                printf("# AS6221: temperature read failed: %s\n",esp_err_to_name(err));
            }
        }
        printf("TEMP,%" PRIu64 ",%" PRId64 ",%.5f,%u,%" PRIu32 ",0x%04X,%s\n",
               seq++,esp_timer_get_time(),temperature,(unsigned)valid,errors,
               (unsigned)config_readback,status);
        // Restart the cadence after initialization/recovery instead of catch-up bursts.
        if(esp_timer_get_time()-now>(int64_t)BOARD_TEMPERATURE_PERIOD_MS*1000)last_wake=xTaskGetTickCount();
        vTaskDelayUntil(&last_wake,pdMS_TO_TICKS(BOARD_TEMPERATURE_PERIOD_MS));
    }
}
esp_err_t as6221_start(void)
{
    if(bus)return ESP_ERR_INVALID_STATE;
    const i2c_master_bus_config_t config={.i2c_port=I2C_NUM_1,
        .sda_io_num=BOARD_TEMPERATURE_SDA_GPIO,.scl_io_num=BOARD_TEMPERATURE_SCL_GPIO,
        .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,
        .flags.enable_internal_pullup=false};
    esp_err_t err=i2c_new_master_bus(&config,&bus);
    if(err!=ESP_OK)return err;
    const i2c_device_config_t dev={.dev_addr_length=I2C_ADDR_BIT_LEN_7,
        .device_address=BOARD_AS6221_ADDRESS,.scl_speed_hz=100000};
    err=i2c_master_bus_add_device(bus,&dev,&device);
    if(err!=ESP_OK) { i2c_del_master_bus(bus);bus=NULL;return err; }
    if(xTaskCreate(temperature_task,"temperature",3072,NULL,2,NULL)!=pdPASS) {
        i2c_master_bus_rm_device(device);i2c_del_master_bus(bus);device=NULL;bus=NULL;
        return ESP_ERR_NO_MEM;
    }
    printf("# TEMP_FIELDS: type,seq,rx_us,temperature_c,valid,i2c_errors,config,status\n");
    return ESP_OK;
}
