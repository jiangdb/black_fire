#ifndef _BF_CONFIG_H_
#define _BF_CONFIG_H_

#include <stdio.h>

#define MANUFACTURER            "Timemore"
#define FW_VERSION              "0.80.22"
#define MODEL_NUMBER            "TES05PL"

typedef struct {
    char* host;
    uint8_t port;
    char* path;
}firmware_t;

void config_init();
void config_load();
void config_close();
void config_reset();

int32_t config_read(char* name, int32_t default_value);
bool config_write(char* name, int32_t value);

// num must at least 13 bytes: mac 12 + 1 \0
void config_get_serial_num(char* serial_num, int len);

uint8_t config_get_key_sound();
bool config_set_key_sound(uint8_t enable);

int config_get_target_temperature();
bool config_set_target_temperature(int temp);

char* config_get_wifi_name();
bool config_set_wifi_name(char* name, size_t len);

char* config_get_wifi_pass();
bool config_set_wifi_pass(char* pass, size_t len);

firmware_t* config_get_firmware_upgrade();
bool config_set_firmware_upgrade(char* host, uint8_t host_len, uint8_t port, char* path, uint8_t path_len);

char* config_get_device_name();
bool config_set_device_name(char* name, size_t len);

#endif  /*_BF_CONFIG_H_*/
