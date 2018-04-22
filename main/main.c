/*
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "config.h"
#include "display.h"
#include "gpio_key.h"
#include "key_event.h"
#include "spi_adc.h"
#include "cpt112s.h"
#include "temperature.h"

#define TAG  "MAIN"

#define GPIO_HEAT_IO                        16      //active high
#define MAIN_LOOP_SPEED                     50      //50ms
#define SETTING_WAIT_TIME                   (2000/MAIN_LOOP_SPEED)    //2000ms

static xQueueHandle eventQueue;
static bool mainDone = false;
static int targetTemperature = 0;
static bool heatEnable = false;
static bool holdEnable = false;
static bool setTargetTemp = false;
static int setting_tick = -1;

static void heat_init()
{
    //GPIO config for the data line.
    gpio_config_t io_conf={
        .intr_type=GPIO_PIN_INTR_DISABLE,
        .mode=GPIO_MODE_OUTPUT,
        .pull_down_en=0,
        .pull_up_en=0,
        .pin_bit_mask=(1<<GPIO_HEAT_IO)
    };

    //Set up handshake line interrupt.
    gpio_config(&io_conf);

    gpio_set_level(GPIO_HEAT_IO, 0);
}

static void toggle_heat()
{
    if (heatEnable) {
        gpio_set_level(GPIO_HEAT_IO, 0);
        heatEnable = false;
        display_set_icon(ICON_HEAT, false);
    } else {
        gpio_set_level(GPIO_HEAT_IO, 1);
        heatEnable = true;
        display_set_icon(ICON_HEAT, true);
    }
}

static void toggle_hold()
{
    if (holdEnable) {
        holdEnable = false;
        display_set_icon(ICON_HOLD, false);
    } else {
        holdEnable = true;
        display_set_icon(ICON_HOLD, true);
    }
}

static void enable_setting(bool enable)
{
    if (enable) {
        setTargetTemp = true;
        display_set_icon(ICON_SETTING, true);
        setting_tick = 0; 
    } else {
        setTargetTemp = false;
        display_set_icon(ICON_SETTING, false);
        setting_tick = -1; 
    }
}

void send_key_event(key_event_t keyEvent, bool fromIsr)
{
    ESP_LOGI(TAG,"%s(%d, %d, %d)\n", __func__, keyEvent.key_type, keyEvent.key_value, fromIsr);
    if (fromIsr) {
        xQueueSendToBackFromISR(eventQueue, &keyEvent, ( TickType_t ) 0 );
    }else{
        xQueueSendToBack(eventQueue, &keyEvent, ( TickType_t ) 0 );
    }
    ESP_LOGI(TAG,"exit %s\n", __func__);
}

static void handle_key_event(void *arg)
{
    while(1) {
        key_event_t keyEvent;
        ESP_LOGI(TAG,"%s: wait for key evnet\n", __func__);
        xQueueReceive(eventQueue, &keyEvent, portMAX_DELAY);

        ESP_LOGI(TAG,"%s: handle key evnet(%d, %d, %d) !!!\n", __func__, keyEvent.key_type, keyEvent.key_value, keyEvent.key_data);

        switch(keyEvent.key_type){
            case LEFT_KEY: 
                if (keyEvent.key_value == KEY_UP) {
                    toggle_hold();
                }
                break;
            case RIGHT_KEY: 
                if (keyEvent.key_value == KEY_UP) {
                    toggle_heat();
                }
                break;
            case SLIDER_LEFT_KEY:
                targetTemperature--;
                if (targetTemperature < 0) {
                    targetTemperature = 0;
                }
                enable_setting(true);
                break;
            case SLIDER_RIGHT_KEY:
                targetTemperature++;
                if (targetTemperature > 100) {
                    targetTemperature = 100;
                }
                //ESP_LOGI(TAG, "%s: target: %d\n", __func__, targetTemperature);
                enable_setting(true);
                break;
            default:
                break;
        }
    }
}

void app_main()
{
    ESP_LOGI(TAG, "BLACK FIRE!!!");
    ESP_LOGI(TAG, "version: %s", FW_VERSION);
    ESP_LOGI(TAG, "model: %s", MODEL_NUMBER);

    /* start event queue */
    eventQueue = xQueueCreate(10, sizeof(key_event_t));
    xTaskCreate(&handle_key_event, "key_event_task", 4096, NULL, 3, NULL);

    esp_timer_init();
    config_init();
    gpio_key_init();
    spi_adc_init();
    display_init();
    cpt112s_init();
    heat_init();

    //int direction = 1;
    targetTemperature = config_get_target_temperature();

    int64_t time = 0;
    while(!mainDone) {
        vTaskDelay(MAIN_LOOP_SPEED/portTICK_RATE_MS);
        time = esp_timer_get_time() - time;
        ESP_LOGI(TAG, "time passed %ld", time);

        /*
        if (setting_tick >= 0) {
            display_set_temperature(targetTemperature);
            setting_tick++;
            if (setting_tick >= SETTING_WAIT_TIME) {
                enable_setting(false);
            }
            continue;
        }

        int32_t val = spi_adc_get_value();
        int32_t temp = convert_temp(val);
        display_set_temperature(temp);
        */

        /*
        if (direction == 1) {
            targetTemperature++;
            if (targetTemperature >= 100) {
                direction = 0;
            }
        } else {
            targetTemperature--;
            if (targetTemperature <= 0) {
                direction = 1;
            }
        }
        */
        //display_set_temperature(targetTemperature);

    }
}
