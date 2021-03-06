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
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "queue_buffer.h"
#include "util.h"

/*
*/
#define TAG                   "SPI-ADC"

/*
PRECISION  4
100g == 3200
0.031g == 1
0.062g == 2
0.093g == 3
0.124g == 4
0.155g == 5
0.186g == 6

PRECISION  5
100g == 1600
0.063g == 1
0.126g == 2
0.189g == 3
0.252g == 4

PRECISION  6
100g == 800
0.125g == 1
0.25g  == 2
*/
#define PRECISION             4

#define PIN_NUM_DATA          23
#define PIN_NUM_CLK           18

#define DATA_PIN_FUNC_SPI     FUNC_GPIO23_VSPID
#define DATA_PIN_FUNC_GPIO    FUNC_GPIO23_GPIO23

#define REFO_ON               0x0<<6
#define REFO_OFF              0x1<<6

#define SPEED_SEL_10HZ        0x0<<4
#define SPEED_SEL_40HZ        0x1<<4
#define SPEED_SEL_640HZ       0x2<<4
#define SPEED_SEL_1280HZ      0x3<<4

#define PGA_SEL_1             0x0<<2
#define PGA_SEL_2             0x1<<2
#define PGA_SEL_64            0x2<<2
#define PGA_SEL_128           0x3<<2

#define CH_SEL_A              0x0
#define CH_SEL_B              0x1
#define CH_SEL_TEMP           0x2
#define CH_SEL_SHORT          0x3

#define CS1237_CONFIG         (0x00|REFO_ON|SPEED_SEL_10HZ|PGA_SEL_1|CH_SEL_A)

#define USE_QUEUE_BUFFER      1
#define BUFFER_SIZE           10
#define DEBUG_ISR_INTVAL      0

#define INT_VALID_INTERVAL_10HZ      (240000 * 50)   //50ms for 10HZ
#define INT_VALID_INTERVAL_40HZ      (240000 * 15)   //15ms for 40HZ

//The semaphore indicating the data is ready.
static SemaphoreHandle_t rdySem = NULL;
static spi_device_handle_t spi;
static uint32_t lastDataReadyTime;
#if DEBUG_ISR_INTVAL
static uint32_t lastIsrTime=0;
static uint32_t isrInterval=0;
#endif
static TaskHandle_t xHandle = NULL;
static int32_t spi_adc_value = 0;
#if USE_QUEUE_BUFFER
static queue_buffer_t qb_SpiAdcData;
static int32_t spiDataBuffer[BUFFER_SIZE];
#endif

/*
This ISR is called when the data line goes low.
*/
static void data_isr_handler(void* arg)
{
    //Sometimes due to interference or ringing or something, we get two irqs after eachother. This is solved by
    //looking at the time between interrupts and refusing any interrupt too close to another one.
    uint32_t currtime=xthal_get_ccount();
#if DEBUG_ISR_INTVAL
    isrInterval = currtime - lastIsrTime;
    lastIsrTime = currtime;
#endif
    uint32_t diff=currtime-lastDataReadyTime;
    if (diff < INT_VALID_INTERVAL_40HZ) return; //ignore everything between valid interval
    lastDataReadyTime=currtime;
    //Give the semaphore.
    BaseType_t mustYield=false;
    xSemaphoreGiveFromISR(rdySem, &mustYield);
    if (mustYield) portYIELD_FROM_ISR();
}

static void gpio_spi_switch(uint8_t mode)
{
    esp_err_t ret;
    if (mode == DATA_PIN_FUNC_GPIO) {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_NUM_DATA], mode);
        ret = gpio_isr_handler_add(PIN_NUM_DATA, data_isr_handler, NULL);
        assert(ret==ESP_OK);
        //ret=gpio_set_direction(PIN_NUM_DATA,GPIO_MODE_INPUT);
        //assert(ret==ESP_OK);
        //ret=gpio_intr_enable(PIN_NUM_DATA);
        //assert(ret==ESP_OK);
    }else if (mode == DATA_PIN_FUNC_SPI) {
        //ret=gpio_intr_disable(PIN_NUM_DATA);
        ret = gpio_isr_handler_remove(PIN_NUM_DATA);
        assert(ret==ESP_OK);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_NUM_DATA], mode);
    }
}

static int32_t parse_adc(uint8_t data[4])
{
    int32_t value = 0;

    if (data[0] & 0x80) {
        value = 0xFF000000|data[0]<<16|data[1]<<8|data[2];
    }else{
        value = data[0]<<16|data[1]<<8|data[2];
    }

    //if (abs(spi_adc_value - (value >> PRECISION)) >=2 ){
    //    ESP_LOGD(TAG,"spi adc value: (int)%d  ", value >> PRECISION );
    //    print_bin(value, 3);
    //}
    //value = value >> PRECISION;
    return value;
}

static void config(int8_t config)
{
    esp_err_t ret;
    //Prepare spi receive buffer
    spi_transaction_t trans[2];
    spi_transaction_t *rtrans;

    memset(trans, 0, sizeof(trans));
    trans[0].rxlength=29;
    trans[0].flags=SPI_TRANS_USE_RXDATA;
    ret=spi_device_queue_trans(spi, &trans[0], portMAX_DELAY);
    assert(ret==ESP_OK);

    trans[1].length=17;
    trans[1].tx_data[0]=0xCA;
    trans[1].tx_data[1]=config;
    trans[1].tx_data[2]=0x00;
    trans[1].flags=SPI_TRANS_USE_TXDATA;
    ret=spi_device_queue_trans(spi, &trans[1], portMAX_DELAY);
    assert(ret==ESP_OK);

    //Wait for all 2 transactions to be done and get back the results.
    for (int x=0; x<2; x++) {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
    }
}

static int32_t read_only()
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));          //Zero out the transaction
    t.rxlength=27;                     //read is 8 bits
    t.flags=SPI_TRANS_USE_RXDATA;      //use rxdata to receive data
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);               //Should have had no issues.
    return parse_adc(t.rx_data);
}

static void push_to_buffer(int32_t value)
{
#if USE_QUEUE_BUFFER
    queue_buffer_push(&qb_SpiAdcData, value);
    value = queue_get_value(&qb_SpiAdcData, ALG_MEDIAN_VALUE);
#endif
    if (abs(spi_adc_value - value) > 3 ) {
        spi_adc_value = value;
        //ESP_LOGD(TAG,"spi_adc_value: %d\n", spi_adc_value);
    }
}

static void spi_init()
{
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num=-1,
        .mosi_io_num=PIN_NUM_DATA,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=100000,                //Clock out at 100KHz
        .mode=1,                                //SPI mode 1
        .spics_io_num=-1,                       //CS pin
        .queue_size=2,                          //We want to be able to queue 2 transactions at a time
        .flags=SPI_DEVICE_3WIRE|SPI_DEVICE_HALFDUPLEX,
    };

    //Initialize the SPI bus, no DMA
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 0);
    assert(ret==ESP_OK);
    //Attach the sensor to the SPI bus
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);
}

static void adc_gpio_init()
{
    //GPIO config for the data line.
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_NEGEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=1,
        .pin_bit_mask=(1<<PIN_NUM_DATA)
    };

    //Set up handshake line interrupt.
    gpio_config(&io_conf);
    // gpio_install_isr_service(0);
    //gpio_set_intr_type(PIN_NUM_DATA, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(PIN_NUM_DATA, data_isr_handler, NULL);
}

static void spi_adc_loop()
{
    int32_t v = 0;
    bool configed = false;
    while(1) {
        //Wait until data is ready
        xSemaphoreTake( rdySem, portMAX_DELAY );

#if DEBUG_ISR_INTVAL
        int data_level = gpio_get_level(PIN_NUM_DATA);
        ESP_LOGD(TAG,"spi isr interval: %d data level: %d",isrInterval/240000, data_level);
#endif
        /*
        if(gpio_get_level(PIN_NUM_DATA) == 1) {
            continue;
        }
        */
        //Disable gpio and enable spi
        gpio_spi_switch(DATA_PIN_FUNC_SPI);
        if (!configed) {
            config(CS1237_CONFIG);
            configed  = true;
        }else{
            v = read_only();
            push_to_buffer(v);
        }

        vTaskDelay(10/portTICK_RATE_MS);

        //Enable gpio again and wait for data
        gpio_spi_switch(DATA_PIN_FUNC_GPIO);
    }
}

int32_t spi_adc_get_value()
{
    return spi_adc_value;
}

void spi_adc_init()
{
    ESP_LOGI(TAG, "%s: CS1237 start!!!\n", __func__);

    //Create the semaphore.
    rdySem=xSemaphoreCreateBinary();

    //SPI config
    spi_init();

    //GPIO config
    adc_gpio_init();

#if USE_QUEUE_BUFFER
    // Queue Buffer init
    memset(spiDataBuffer,0,sizeof(spiDataBuffer));
    queue_buffer_init(&qb_SpiAdcData, spiDataBuffer, BUFFER_SIZE);
#endif

    //Create task
    xTaskCreate(&spi_adc_loop, "spi_adc_task", 4096, NULL, 2, &xHandle);
}
