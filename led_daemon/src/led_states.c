#include "led_states.h"

#include <lib_led/string_constants.h>
#include <ubus_utils/ubus_utils.h>

#include <ubus_common.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct led_state_st {
	enum led_state_t state;
	char const *name;
	char const *query_name;
};

static char const _invalid_state[] = "INVALID";

static struct led_state_st const led_states[] = {
	[LED_STATE_UNKNOWN] =
		{.state = LED_STATE_UNKNOWN,
		 .name = _invalid_state,
		 .query_name = _invalid_state },
	[LED_OFF] =
		{.state = LED_OFF,
		 .name = "OFF",
		 .query_name = _led_off },
	[LED_ON] =
		{.state = LED_ON,
		 .name = "ON",
		 .query_name = _led_on},
	[LED_SLOW_FLASH] =
		{.state = LED_SLOW_FLASH,
		 .name = "FLASH",
		 .query_name = _led_flash},
	[LED_FAST_FLASH] =
		{.state = LED_FAST_FLASH,
		 .name = "FLASH_FAST",
		 .query_name = _led_fast_flash}
};

static bool
is_valid_state(enum led_state_t const state)
{
	return state >= __LED_STATE_FIRST && state < ARRAY_SIZE(led_states);
}

static struct led_state_st const *
led_state_entry_lookup(char const * const name)
{
	struct led_state_st const * entry;

	if (name == NULL) {
		entry = NULL;
		goto done;
	}

	for (enum led_state_t i = __LED_STATE_FIRST;
		  i < ARRAY_SIZE(led_states);
		  i++) {
		struct led_state_st const * const candidate = &led_states[i];
		bool const matches = strcasecmp(name, candidate->query_name) == 0;

		if (matches) {
			entry = candidate;
			goto done;
		}
	}

	entry = NULL;

done:
	return entry;
}

char const *
led_state_name(enum led_state_t const state)
{
	return is_valid_state(state) ? led_states[state].name : _invalid_state;
}

char const *
led_state_query_name(enum led_state_t const state)
{
	char const * const query_name =
		is_valid_state(state) ? led_states[state].query_name : _invalid_state;

	return query_name;
}

enum led_state_t
led_state_by_query_name(char const * const name)
{
	struct led_state_st const * const state_entry =
		led_state_entry_lookup(name);
	enum led_state_t const state =
		(state_entry != NULL) ? state_entry->state : LED_STATE_UNKNOWN;

    return state;
}

