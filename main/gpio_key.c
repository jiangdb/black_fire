/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "gpio_key.h"
#include "key_event.h"

#define TAG  "KEY"

static TaskHandle_t xHandle = NULL;
static xQueueHandle gpio_evt_queue = NULL;

static void gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void beap(int wait, int duration)
{
    if (wait > 0) {
        vTaskDelay(wait/portTICK_RATE_MS);
    }
    gpio_set_level(GPIO_OUTPUT_IO_SPEAKER, 1);
    vTaskDelay(duration/portTICK_RATE_MS);
    gpio_set_level(GPIO_OUTPUT_IO_SPEAKER, 0);
}

static void gpio_key_task(void* arg)
{
    uint32_t io_num;
    int tick = -1;
    TickType_t tick_type = portMAX_DELAY;

    key_event_t keyEvent;
    while(1) {
        if(xQueueReceive(gpio_evt_queue, &io_num, tick_type)) {
            int val = gpio_get_level(io_num);
            if (io_num == GPIO_INPUT_IO_KEY_LEFT) {
                int tick_value = -1;

                if (val == 1) {
                    //beap and vibrate
                    beap(0,100);
                    tick_value = 0;
                }

                // detect key
                keyEvent.key_type = LEFT_KEY;
                keyEvent.key_value = val==1?KEY_DOWN:KEY_UP;
                tick = tick_value;
                send_key_event(keyEvent, false);

                if (tick == -1) {
                    //stop tick
                    tick_type = portMAX_DELAY;
                } else {
                    //start tick
                    tick_type = ( TickType_t ) 50/portTICK_RATE_MS;
                }
            }
        }
        if (tick >= 0) {
            tick++;
            if (tick > 10) {
                //50*10 ms
                keyEvent.key_value = KEY_HOLD;
                keyEvent.key_type = LEFT_KEY;
                send_key_event(keyEvent, false);
                tick = 1;
            }
        }
    }
}

void gpio_key_init()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //interrupt of both edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull_down_en mode
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_key_task, "gpio_key_task", 2048, NULL, 4, &xHandle);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_KEY_LEFT, gpio_isr_handler, (void*) GPIO_INPUT_IO_KEY_LEFT);

    //set reset high
    gpio_set_level(GPIO_OUTPUT_IO_RESET, 1);
    gpio_set_level(GPIO_OUTPUT_IO_LED0, 1);
    gpio_set_level(GPIO_OUTPUT_IO_LED1, 1);
}

