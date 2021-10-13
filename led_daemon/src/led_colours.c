#include "led_colours.h"

#include <ubus_utils/ubus_utils.h>

#include <ubus_common.h>

#include <stdbool.h>
#include <stddef.h>

struct led_colour_st
{
    char const * name;
};

static char const invalid_colour[] = "INVALID";

static struct led_colour_st const led_colours[] =
{
    [LED_COLOUR_UNKNOWN] = { .name = invalid_colour },
    [LED_COLOUR_RED] = { .name = "RED" },
    [LED_COLOUR_GREEN] = { .name = "GREEN" },
    [LED_COLOUR_BLUE] = { .name = "BLUE" },
    [LED_COLOUR_YELLOW] = { .name = "YELLOW" },
};

char const *
led_colour_by_type(enum led_colour_t const colour)
{
    bool const is_known_colour = colour < ARRAY_SIZE(led_colours);
    char const * colour_name =
        is_known_colour ? led_colours[colour].name : invalid_colour;

    return colour_name;
}

