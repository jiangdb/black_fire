/*
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "gpio_key.h"
#include "key_event.h"

#define TAG                   "CPT112S"

#define CPT112S_SLAVE_ADDR          0x70
#define I2C_MASTER_SCL_IO           4    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           0    /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          400000     /*!< I2C master clock frequency */
#define PIN_NUM_INT                 2

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

#define EVENT_TOUCH                     0x00
#define EVENT_TOUCH_RELEASE             0x01
#define EVENT_SLIDER                    0x02
#define EVENT_TYPE(x)                   (x&0x0f)
#define EVENT_COUNTER(x)                ((x&0xf0)>>4)
#define EVENT_SLIDER_POSITION(x,y)      ((x<<8|y))
#define SLIDER_THRESHOLD                5 

//The semaphore indicating the data is ready.
static SemaphoreHandle_t rdySem = NULL;

/*
This ISR is called when the data line goes low.
*/
static void data_isr_handler(void* arg)
{
    //Sometimes due to interference or ringing or something, we get two irqs after eachother. This is solved by
    //looking at the time between interrupts and refusing any interrupt too close to another one.
    /*
    uint32_t currtime=xthal_get_ccount();
#if DEBUG_ISR_INTVAL
    isrInterval = currtime - lastIsrTime;
    lastIsrTime = currtime;
#endif
    uint32_t diff=currtime-lastDataReadyTime;
    lastDataReadyTime=currtime;
    */

    //Give the semaphore.
    BaseType_t mustYield=false;
    xSemaphoreGiveFromISR(rdySem, &mustYield);
    if (mustYield) portYIELD_FROM_ISR();
}

/**
 * @brief test code to read esp-i2c-slave
 *        We need to fill the buffer of esp slave device, then master can read them out.
 *
 * _______________________________________________________________________________________
 * | start | slave_addr + rd_bit +ack | read n-1 bytes + ack | read 1 byte + nack | stop |
 * --------|--------------------------|----------------------|--------------------|------|
 *
 */
static esp_err_t i2c_read(i2c_port_t i2c_num, uint8_t* data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( CPT112S_SLAVE_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

#define DIFF_100MS  (240000 * 200) 
static void cpt112s_parse_event(uint8_t* event)
{
    static int16_t lastSliderPos = -1;
    static int64_t lastSliderTime = -1;
    static int16_t sliderStartPos = -1;
    static int64_t sliderTouchTime = 0;
    static int16_t sliderSpped = 0;
    int eventType = EVENT_TYPE(event[0]);
    key_event_t keyEvent;

    keyEvent.key_type = KEY_TYPE_MAX;
    if (EVENT_TOUCH == eventType) {
        keyEvent.key_type = RIGHT_KEY;
        keyEvent.key_value = KEY_DOWN;
        send_key_event(keyEvent, false);
    } else if (EVENT_TOUCH_RELEASE == eventType) {
        keyEvent.key_type = RIGHT_KEY;
        keyEvent.key_value = KEY_UP;
        send_key_event(keyEvent, false);
        //beap(0,100);
    } else if (EVENT_SLIDER == eventType) {
        int currentPos =  EVENT_SLIDER_POSITION(event[1], event[2]);
        int64_t currentTime = esp_timer_get_time();
        if (currentPos == 0xffff) {
            //slider release, skip it
            ESP_LOGI(TAG, "%s: slider release ###################### \n", __func__);
            //send event
            keyEvent.key_type = SLIDER_KEY;
            keyEvent.key_value = KEY_UP;
            send_key_event(keyEvent, false);

            lastSliderPos = -1;
            lastSliderTime = 0;
            //TODO handle auto scroll
            //int64_t sliderReleaseTime = esp_timer_get_time();
            //int32_t speed = (currentPos - sliderStartPos);
            return;
        } else if (lastSliderPos < 0) {
            //slider touch
            ESP_LOGI(TAG, "%s: slider touch: ********************* %d\n", __func__, currentPos);
            //send event
            keyEvent.key_type = SLIDER_KEY;
            keyEvent.key_value = KEY_DOWN;
            send_key_event(keyEvent, false);

            sliderStartPos = lastSliderPos = currentPos;
            lastSliderTime = currentTime;
            return;
        }

        //if (abs(currentPos-lastSliderPos) < SLIDER_THRESHOLD) return;

        int distance = abs(currentPos - lastSliderPos);
        int intervalMs = (int)(currentTime - lastSliderTime)/1000;
        int speed = distance * 1000 / intervalMs;
        ESP_LOGI(TAG, "%s: slider: =================> %d (%d, %d)\n", __func__, currentPos, currentPos-lastSliderPos, intervalMs);

        /*
        //handle steps
        if (currentPos > lastSliderPos) {
            //move right
            for (; (lastSliderPos+SLIDER_THRESHOLD) <= currentPos; lastSliderPos+=SLIDER_THRESHOLD ) {
                keyEvent.key_type = SLIDER_KEY;
                keyEvent.key_value = KEY_SLIDE;
                keyEvent.key_extra[0] = 1;
                ESP_LOGI(TAG, "%s: send slider key\n", __func__);
                send_key_event(keyEvent, false);
            }
        } else {
            //move left
            for (; (lastSliderPos-SLIDER_THRESHOLD) >= currentPos; lastSliderPos-=SLIDER_THRESHOLD ) {
                keyEvent.key_type = SLIDER_KEY;
                keyEvent.key_value = KEY_SLIDE;
                keyEvent.key_extra[0] = -1;
                ESP_LOGI(TAG, "%s: send slider key\n", __func__);
                send_key_event(keyEvent, false);
            }
        }
        */
        lastSliderPos = currentPos;
        lastSliderTime = currentTime;
    }
}

static void main_loop()
{
    uint8_t event[3];
    while(1) {
        //Wait until data is ready
        xSemaphoreTake( rdySem, portMAX_DELAY );

        //ESP_LOGI(TAG, "%s: *****    interrupt come in\n", __func__);
        //gpio_isr_handler_remove(PIN_NUM_INT);

        while(!gpio_get_level(PIN_NUM_INT)) {
            memset(event, 0, sizeof(event));
            if (ESP_OK == i2c_read(I2C_MASTER_NUM, event, sizeof(event))) {
                // parse event and queue
                cpt112s_parse_event(event);
            }
        }

        //ESP_LOGI(TAG, "%s: *****     no more event\n", __func__);
        //gpio_isr_handler_add(PIN_NUM_INT);
        //ESP_LOGI(TAG, "%s: release\n", __func__);
    }
}

/**
 * @brief i2c master initialization
 */
static void i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);
}

static void interrupt_init() {
    //GPIO config for the data line.
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_NEGEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=1,
        .pin_bit_mask=(1<<PIN_NUM_INT)
    };

    gpio_config(&io_conf);
    // gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_NUM_INT, data_isr_handler, NULL);
}

void cpt112s_init()
{
    ESP_LOGI(TAG, "%s: CPT112S start!!!\n", __func__);

    //Create the semaphore.
    rdySem=xSemaphoreCreateBinary();

    i2c_master_init();
    interrupt_init();

    //Create task
    xTaskCreate(&main_loop, "cpt112s_main_loop", 4096, NULL, 2, NULL);
} 
