#ifndef _BF_KEY_EVENT_H_
#define _BF_KEY_EVENT_H_

#include <stdio.h>

/* KEY TYPE */
enum {
	LEFT_KEY,
	RIGHT_KEY,
	SLIDER_KEY,
    KEY_TYPE_MAX
};

/* KEY VALUE */
enum {
    KEY_UP,
    KEY_HOLD,
    KEY_DOWN,
    KEY_SLIDE,
    KEY_VALUE_MAX
};

typedef struct {
	int32_t key_type;
	int32_t key_value;
	int32_t key_extra[4];
} key_event_t;

void send_key_event(key_event_t keyEvent, bool fromIsr);

#endif  /*_BF_KEY_EVENT_H_*/
