#include "yahboom_imu.h"
#include "imu_protocol.h"
#include "board_config.h"
#include <inttypes.h>
#include <stdio.h>
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static QueueHandle_t events;
static imu_parser_t parser;
static uint64_t sequence;
static uint32_t uart_errors, pressure_frames, angle_frames;
static int64_t receive_time, last_frame_time;
static bool configured;

static void frame_received(const imu_frame_t *frame, void *user)
{
    (void)user;
    last_frame_time=receive_time;
    if(frame->type==0x56) ++pressure_frames;
    if(frame->type==0x53) ++angle_frames;
    if(frame->type==0x5F) {
        printf("# IMU: register response RSW=0x%04X RATE=0x%04X BAUD=0x%04X\n",
               (unsigned)frame->values[0],(unsigned)frame->values[1],(unsigned)frame->values[2]);
    }
    if(frame->type==0x5F && frame->values[0]==0x005E && frame->values[1]==6) {
        configured=true;
        printf("# IMU: output configuration readback confirmed (ACC/GYRO/ANGLE/MAG/PRESS, 10Hz)\n");
    }
    printf("IMU,%s,%" PRIu64 ",%" PRId64 ",%.5f,%.5f,%.5f,%.5f,%u\n",
           frame->kind,sequence++,receive_time,frame->values[0],frame->values[1],
           frame->values[2],frame->values[3],(unsigned)frame->valid_mask);
}
static esp_err_t command(uint8_t address,uint16_t value)
{
    const uint8_t bytes[]={0xFF,0xAA,address,(uint8_t)value,(uint8_t)(value>>8)};
    int sent=uart_write_bytes(UART_NUM_1,bytes,sizeof(bytes));
    return sent==(int)sizeof(bytes) ? uart_wait_tx_done(UART_NUM_1,pdMS_TO_TICKS(100)) : ESP_FAIL;
}
static esp_err_t write_unlocked(uint8_t address,uint16_t value)
{
    // Match the vendor SDK: unlock separately for each configuration register.
    esp_err_t result=command(0x69,0xB588);
    vTaskDelay(pdMS_TO_TICKS(20));
    if(result==ESP_OK) result=command(address,value);
    vTaskDelay(pdMS_TO_TICKS(20));
    return result;
}
static void request_configuration(unsigned attempt)
{
    printf("# IMU: configuration attempt %u/3, RX=IO18 TX=IO17\n",attempt);
    // Reading RSW before writing also checks whether the return path accepts commands.
    esp_err_t read_result=command(0x27,0x0002);
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_err_t content_result=write_unlocked(0x02,0x005E);
    esp_err_t rate_result=write_unlocked(0x03,0x0006);
    esp_err_t verify_result=command(0x27,0x0002);
    printf("# IMU: TX status read=%s content=%s rate=%s verify=%s; ESP_OK only means locally transmitted\n",
           esp_err_to_name(read_result),esp_err_to_name(content_result),
           esp_err_to_name(rate_result),esp_err_to_name(verify_result));
}
static void receive_task(void *arg)
{
    (void)arg;
    int64_t start=esp_timer_get_time(), last_status=start, last_warning=start;
    unsigned attempts=0;
    int64_t next_request=start+1200000;
    uint8_t bytes[128];
    while(1) {
        int64_t now=esp_timer_get_time();
        if(attempts<3 && (!configured || !pressure_frames) && now>=next_request) {
            request_configuration(++attempts);
            next_request=esp_timer_get_time()+3000000;
        }
        uart_event_t event;
        if(xQueueReceive(events,&event,pdMS_TO_TICKS(20))==pdTRUE) {
            if(event.type==UART_DATA) {
                size_t remaining=event.size;
                while(remaining) {
                    size_t length=remaining>sizeof(bytes)?sizeof(bytes):remaining;
                    int count=uart_read_bytes(UART_NUM_1,bytes,length,pdMS_TO_TICKS(20));
                    if(count<=0) break;
                    receive_time=esp_timer_get_time();
                    imu_parser_feed(&parser,bytes,count,frame_received,NULL);
                    remaining-=count;
                }
            } else if(event.type==UART_FIFO_OVF || event.type==UART_BUFFER_FULL) {
                ++uart_errors;
                uart_flush_input(UART_NUM_1); xQueueReset(events); parser.used=0;
                printf("# IMU: UART overflow; stream resynchronized\n");
            } else if(event.type==UART_PARITY_ERR || event.type==UART_FRAME_ERR) {
                ++uart_errors; parser.used=0;
            }
        }
        now=esp_timer_get_time();
        if(now-last_status>=1000000) {
            printf("IMU_STAT,%" PRId64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu32
                   ",%" PRIu32 ",%" PRIu32 ",%u,%u\n",now,parser.frames,parser.checksum_errors,
                   parser.discarded_bytes,uart_errors,pressure_frames,angle_frames,
                   (unsigned)(last_frame_time && now-last_frame_time<2000000),(unsigned)configured);
            last_status=now;
            if(now-start>5000000 && now-last_warning>=10000000 && !pressure_frames) {
                printf("# IMU: no PRESS frames; configured=%u attempts=%u. If no register response, verify TX/RX command path or use vendor USB utility.\n",
                       (unsigned)configured,attempts);
                // Keep status every second but limit explanatory messages to about 10 seconds.
                last_warning=now;
            }
        }
    }
}
esp_err_t yahboom_imu_start(void)
{
    if(events) return ESP_ERR_INVALID_STATE;
    const uart_config_t config={.baud_rate=BOARD_IMU_BAUD,.data_bits=UART_DATA_8_BITS,
        .parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,
        .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
    esp_err_t err=uart_param_config(UART_NUM_1,&config);
    if(err==ESP_OK) err=uart_set_pin(UART_NUM_1,BOARD_IMU_TX_GPIO,BOARD_IMU_RX_GPIO,
                                   UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    if(err!=ESP_OK) return err;
    err=uart_driver_install(UART_NUM_1,4096,0,32,&events,0);
    if(err!=ESP_OK) { events=NULL; return err; }
    imu_parser_init(&parser);
    if(xTaskCreate(receive_task,"imu_receive",4096,NULL,4,NULL)!=pdPASS) {
        uart_driver_delete(UART_NUM_1); events=NULL; return ESP_ERR_NO_MEM;
    }
    printf("# IMU: UART1 RX=IO%d TX=IO%d baud=%d; receive timestamps, not sensor sample timestamps\n",
           BOARD_IMU_RX_GPIO,BOARD_IMU_TX_GPIO,BOARD_IMU_BAUD);
    printf("# IMU_FIELDS: type,kind,seq,rx_us,v1,v2,v3,v4,valid_mask\n");
    return ESP_OK;
}
