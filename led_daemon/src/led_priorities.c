#include "led_priorities.h"

#include <lib_led/string_constants.h>
#include <ubus_utils/ubus_utils.h>

#include <stddef.h>
#include <string.h>

struct led_priority_mapping_st {
	char const *name;
	enum led_priority_t priority;
};

static struct led_priority_mapping_st const led_priority_aliases[] = {
	[LED_PRIORITY_NORMAL] = {
		.name = _led_priority_normal,
		.priority = LED_PRIORITY_NORMAL
	},
	[LED_PRIORITY_ALTERNATE] = {
		.name = _led_priority_alternate,
		.priority = LED_PRIORITY_ALTERNATE
	},
	[LED_PRIORITY_LOCKED] = {
		.name = _led_priority_locked,
		.priority = LED_PRIORITY_LOCKED
	},
	[LED_PRIORITY_CRITICAL] = {
		.name = _led_priority_critical,
		.priority = LED_PRIORITY_CRITICAL
	}
};

static struct led_priority_mapping_st const *
led_priority_mapping_by_name(char const * const priority_name_in)
{
	/* If the priority name isn't supplied, default to 'normal'. */
	char const *priority_name = priority_name_in;
	struct led_priority_mapping_st const * mapping;

	if (priority_name == NULL) {
		priority_name = _led_priority_normal;
	}

	for (size_t i = 0; i < ARRAY_SIZE(led_priority_aliases); i++) {
		struct led_priority_mapping_st const * const candidate =
			&led_priority_aliases[i];
		bool const matched =
			candidate->name != NULL
			&& strcasecmp(priority_name, candidate->name) == 0;

		if (matched) {
			mapping = candidate;
			goto done;
		}
	}

	mapping = NULL;

done:
	return mapping;
}

bool
led_priority_by_name(
	char const * const priority_name, enum led_priority_t * const led_priority)
{
	bool success;
	struct led_priority_mapping_st const * const mapping =
		led_priority_mapping_by_name(priority_name);

	if (mapping == NULL) {
		success = false;
		goto done;
	}

	*led_priority = mapping->priority;
	success = true;

done:
	return success;
}

char const *
led_priority_to_name(enum led_priority_t const led_priority)
{
	char const *priority_name;

	if (led_priority >= ARRAY_SIZE(led_priority_aliases)) {
		priority_name = NULL;
		goto done;
	}

	priority_name = led_priority_aliases[led_priority].name;

done:
	return priority_name;
}


