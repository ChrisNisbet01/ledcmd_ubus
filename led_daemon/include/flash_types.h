#ifndef __FLASH_TYPES_H__
#define __FLASH_TYPES_H__

#include "led_states.h"

#include <stddef.h>

enum flash_type_t {
	__LED_FLASH_TYPE_FIRST = 0,
	LED_FLASH_TYPE_NONE = __LED_FLASH_TYPE_FIRST,
	LED_FLASH_TYPE_ONE_SHOT,
	LED_FLASH_TYPE_FAST,
	LED_FLASH_TYPE_SLOW,
	__LED_FLASH_TYPE_MAX
};

struct flash_times_st {
	size_t on_time_ms;
	size_t off_time_ms;
};

enum flash_type_t
led_flash_type_lookup(char const *name);

struct flash_times_st const *
led_flash_times_lookup(enum flash_type_t flash_type);

enum flash_type_t
led_flash_type_from_state(enum led_state_t const state);

#endif /* __FLASH_TYPES_H__ */
