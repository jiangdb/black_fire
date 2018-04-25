/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "config.h"
#include "display.h"
#include "queue_buffer.h"

/*
 * defines
 */

#define TAG  "DISPLAY"

#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 12
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15

#define COMMAND_DATA_MODE_ADDRESS_AUTO    0x40
#define COMMAND_DATA_MODE_ADDRESS_FIX     0x44

#define COMMAND_ADDRESS_0       0xC0
#define COMMAND_ADDRESS_1       0xC1
#define COMMAND_ADDRESS_2       0xC2
#define COMMAND_ADDRESS_3       0xC3
#define COMMAND_ADDRESS_4       0xC4
#define COMMAND_ADDRESS_5       0xC5
#define COMMAND_ADDRESS_6       0xC6
#define COMMAND_ADDRESS_7       0xC7
#define COMMAND_ADDRESS_8       0xC8
#define COMMAND_ADDRESS_9       0xC9
#define COMMAND_ADDRESS_10      0xCA
#define COMMAND_ADDRESS_11      0xCB
#define COMMAND_ADDRESS_12      0xCC
#define COMMAND_ADDRESS_13      0xCD
#define COMMAND_ADDRESS_14      0xCE
#define COMMAND_ADDRESS_15      0xCF

#define COMMAND_DISPLAY_OFF     0x80
#define COMMAND_DISPLAY_ON      0x8F

#define DECIMAL_POINT           0x80
#define NUMBER_0                0x3F
#define NUMBER_1                0x06
#define NUMBER_2                0x5B
#define NUMBER_3                0x4F
#define NUMBER_4                0x66
#define NUMBER_5                0x6D
#define NUMBER_6                0x7D
#define NUMBER_7                0x07
#define NUMBER_8                0x7F
#define NUMBER_9                0x6F
#define NUMBER_OFF              0x00
#define OPT_DASH                0x40

#define CHAR_C                  0x39
#define CHAR_E                  0x79
#define CHAR_F                  0x71

#define DIGITAL_NUMBER          4
#define ICON_ADDRESS_1          5
#define ICON_ADDRESS_2          6

static uint8_t display_data[]={
    COMMAND_ADDRESS_8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

static uint8_t numbers[] = {
    NUMBER_0,
    NUMBER_1,
    NUMBER_2,
    NUMBER_3,
    NUMBER_4,
    NUMBER_5,
    NUMBER_6,
    NUMBER_7,
    NUMBER_8,
    NUMBER_9,
};

static spi_device_handle_t spi;
static bool display_enable = true;

static void spi_trassfer_display() 
{
    esp_err_t ret;
    spi_transaction_t trans[3];        //total 3 transactions
    spi_transaction_t *rtrans;
    memset(trans, 0, sizeof(trans));   //Zero out the transaction

    //AUTO address command
    trans[0].length=8;                                      //Command is 8 bits
    trans[0].tx_data[0]=COMMAND_DATA_MODE_ADDRESS_AUTO;     //command
    trans[0].flags=SPI_TRANS_USE_TXDATA;
    ret=spi_device_queue_trans(spi, &trans[0], portMAX_DELAY);
    assert(ret==ESP_OK);               //Should have had no issues.

    //Address + data
    trans[1].length=15*8;                     //15bytes is 15*8 bits
    trans[1].tx_buffer=display_data;          //command + data
    ret=spi_device_queue_trans(spi, &trans[1], portMAX_DELAY);
    assert(ret==ESP_OK);               //Should have had no issues.

    //Turn on display command
    trans[2].length=8;                          //Command is 8 bits
    trans[2].tx_data[0]=display_enable?COMMAND_DISPLAY_ON:COMMAND_DISPLAY_OFF;     //command
    trans[2].flags=SPI_TRANS_USE_TXDATA;
    ret=spi_device_queue_trans(spi, &trans[2], portMAX_DELAY);
    assert(ret==ESP_OK);               //Should have had no issues.

    //Wait for all 2 transactions to be done and get back the results.
    for (int x=0; x<3; x++) {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
    }

}


static void clear_display_data(bool leaveBattery)
{
    for (int i = 1; i < sizeof(display_data); ++i)
    {
        display_data[i] = 0;
    }
}

void display_set_temperature(int32_t temp)
{
    //ESP_LOGI(TAG, "%s(%d)\n", __func__, temp);
    int8_t data[DIGITAL_NUMBER];
    //convert to array, LSB mode
    memset(data, -2, sizeof(data));
    int i;
    for(i=DIGITAL_NUMBER-1; i>=0; i--) {
        data[i] = temp % 10;
        temp/=10;
        if (temp==0) {
            /*
            if (i == DIGITAL_NUMBER-1) {
                continue;
            }
            */
            break;
        }
    }

    //set display data
    int start = 1;
    for (int j=0; j<DIGITAL_NUMBER; j++) {
        if (data[j] == -2){
            display_data[start+j] = NUMBER_OFF;
        }else if (data[j] == -1){
            display_data[start+j] = OPT_DASH;
        }else{
            display_data[start+j] = numbers[data[j]];
        }
        //display_data[3] |= 0x80;
    }

    spi_trassfer_display();
}

void display_set_operation(int operation, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
    clear_display_data(true);
    int timerPos = 1+DIGITAL_NUMBER*2;
    int digitPos = 1+DIGITAL_NUMBER;
    switch(operation) {
        case OPERATION_UPGRADE:
            // show C1 at timer
            timerPos+=2;        //start from the third digit
            display_data[timerPos++] = CHAR_C;
            display_data[timerPos] = NUMBER_1;
            break;
        default:
            return;
            break;
    }

    if (d0 < 10) {
        display_data[digitPos] = numbers[d0];
    }
    digitPos++;
    if (d1 < 10) {
        display_data[digitPos] = numbers[d1];
    }
    digitPos++;
    if (d2 < 10) {
        display_data[digitPos] = numbers[d2];
    }
    digitPos++;
    if (d3 < 10) {
        display_data[digitPos] = numbers[d3];
    }
}

void display_set_error(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
    clear_display_data(true);
    int timerPos = 1+DIGITAL_NUMBER*2+3;
    int digitPos = 1+DIGITAL_NUMBER;
    // show E at timer
    display_data[timerPos] = CHAR_E;

    if (d0 < 10) {
        display_data[digitPos] = numbers[d0];
    }
    digitPos++;
    if (d1 < 10) {
        display_data[digitPos] = numbers[d1];
    }
    digitPos++;
    if (d2 < 10) {
        display_data[digitPos] = numbers[d2];
    }
    digitPos++;
    if (d3 < 10) {
        display_data[digitPos] = numbers[d3];
    }
}

void display_set_icon(int icon, bool on)
{
   // ESP_LOGI(TAG, "%s(%d, %d)\n", __func__, icon, on);

    int iconAddress = ICON_ADDRESS_1;
    if (icon >= ICON_SETTING) {
        iconAddress = ICON_ADDRESS_2;
        icon -= ICON_SETTING;
    }
    uint8_t val = display_data[iconAddress];

    if (on) {
        val |= 1<<icon;
    }else{
        val &= (~(1<<icon));
    }
    display_data[iconAddress] = val;
}

void display_turn_onoff(bool on)
{
    if (on) {
        display_enable = true;
    } else {
        display_enable = false;
    }
}

void display_init()
{
    ESP_LOGD(TAG,"Display Init!!!\n");

    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=1000000,                //Clock out at 1MHz
        .mode=3,                                //SPI mode 3
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .cs_ena_posttrans=3,                    //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=5,                          //We want to be able to queue 5 transactions at a time
        .flags=SPI_DEVICE_TXBIT_LSBFIRST|SPI_DEVICE_RXBIT_LSBFIRST,
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    assert(ret==ESP_OK);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);
}
