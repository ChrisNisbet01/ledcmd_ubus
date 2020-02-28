#include "flash_types.h"

#include <lib_led/string_constants.h>
#include <libubus_utils/ubus_utils.h>

#include <ubus_common.h>

#include <stdbool.h>
#include <string.h>

struct flash_type_st {
	char const *name;
	struct flash_times_st flash_times;
};

static struct flash_type_st const flash_types[] =
{
	[LED_FLASH_TYPE_NONE] = {
		.name = _led_flash_type_none,
		.flash_times = {
			.on_time_ms = 0,
			.off_time_ms = 0
		}
	},
	[LED_FLASH_TYPE_ONE_SHOT] = {
		.name = _led_flash_type_one_shot,
		.flash_times = {
			.on_time_ms = 50,
			.off_time_ms = 0
		}
	},
	[LED_FLASH_TYPE_SLOW] = {
		.name = _led_flash_type_slow,
		.flash_times = {
			.on_time_ms = 500,
			.off_time_ms = 500
		}
	},
	[LED_FLASH_TYPE_FAST] = {
		.name = _led_flash_type_fast,
		.flash_times = {
			.on_time_ms = 250,
			.off_time_ms = 250
		}
	}
	/*
	 * Support other flash types?
	 * e.g. long/short cadence?
	 */
};

static bool
is_valid_flash_type(enum flash_type_t const flash_type)
{
	bool const is_valid =
		flash_type >= __LED_FLASH_TYPE_FIRST
		&& flash_type < ARRAY_SIZE(flash_types);

	return is_valid;
}

enum flash_type_t
led_flash_type_lookup(char const * const name)
{
	enum flash_type_t flash_type;

	if (name == NULL) {
		flash_type = LED_FLASH_TYPE_NONE;
		goto done;
	}

	for (enum flash_type_t i = 0; i < ARRAY_SIZE(flash_types); i++) {
		struct flash_type_st const * const candidate = &flash_types[i];
		bool const matches = strcasecmp(name, candidate->name) == 0;

		if (matches) {
			flash_type = i;
			goto done;
		}
	}

	flash_type = LED_FLASH_TYPE_NONE;

done:
	return flash_type;
}

struct flash_times_st const *
led_flash_times_lookup(enum flash_type_t const flash_type_in)
{
	enum flash_type_t const flash_type =
		is_valid_flash_type(flash_type_in)
		? flash_type_in
		: LED_FLASH_TYPE_NONE;

	return &flash_types[flash_type].flash_times;
}

enum flash_type_t
led_flash_type_from_state(enum led_state_t const state)
{
	enum flash_type_t flash_type;

	if (state == LED_SLOW_FLASH) {
		flash_type = LED_FLASH_TYPE_SLOW;
	} else if (state == LED_FAST_FLASH) {
		flash_type = LED_FLASH_TYPE_FAST;
	} else {
		flash_type = LED_FLASH_TYPE_NONE;
	}

	return flash_type;
}

