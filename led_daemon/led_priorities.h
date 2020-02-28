#ifndef __LED_PRIORITIES_H__
#define __LED_PRIORITIES_H__

#include <stdbool.h>

enum led_priority_t {
    LED_PRIORITY_CRITICAL,
    LED_PRIORITY_LOCKED,
    LED_PRIORITY_ALTERNATE,
    LED_PRIORITY_NORMAL,
    LED_PRIORITY_COUNT, /* Must be <= PRIORITY_LIMIT. */
};

bool
led_priority_by_name(
	char const *priority_name, enum led_priority_t *led_priority);

char const *
led_priority_to_name(enum led_priority_t led_priority);


#endif /* __LED_PRIORITIES_H__ */
