#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stdio.h>

enum {
    ICON_HEAT,
    ICON_HOLD,
    ICON_SETTING,
    ICON_WIFI,
    ICON_ALL,
};

enum {
    OPERATION_CALIBRATION,
    OPERATION_UPGRADE,
};

void display_init();
void display_turn_onoff(bool on);
void display_set_icon(int icon, bool on);
void display_set_temperature(int32_t temp);
void display_set_operation(int opation, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3);
void display_set_error(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3);

#endif  /*_DISPLAY_H_*/
