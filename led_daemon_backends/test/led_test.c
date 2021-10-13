#include <led_daemon/platform_specific.h>

#include <ubus_utils/ubus_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

enum
{
    CMD_QUERY = 0,  /* query the state of the LEDs */
    CMD_ON,     /* turn LED on permanently */
    CMD_OFF,    /* turn LED off permanently */
    CMD_FLASH,  /* flash this LED */
    CMD_FLASH_FAST  /* flash this LED quickly */
};

struct led
{
    const char * name;
    enum led_colour_t colour;
    enum led_state_t state;
};

static struct led leds[] =
{
    {
        .name = "1",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "2",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "3",
        .colour = LED_COLOUR_YELLOW,
    },
    {
        .name = "4",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "5",
        .colour = LED_COLOUR_YELLOW,
    },
    {
        .name = "6",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "7",
        .colour = LED_COLOUR_YELLOW,
    },
    {
        .name = "8",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "9",
        .colour = LED_COLOUR_YELLOW,
    },
    {
        .name = "10",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "11",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "12",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "13",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "14",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "15",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "16",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "17",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "18",
        .colour = LED_COLOUR_GREEN,
    },
    {
        .name = "19",
        .colour = LED_COLOUR_GREEN,
    }
};


static enum led_state_t
get_led_state(
    led_handle_st * const led_handle, led_st const * const led)
{
    enum led_state_t const state = (led != NULL) ? led->state : LED_STATE_UNKNOWN;

    UNUSED_ARG(led_handle);

    return state;
}

static bool
set_led_state(
    led_handle_st * const led_handle,
    led_st * const led,
    enum led_state_t const state)
{
    int value = atoi(led->name) * 2;
    char cmd = (state == LED_ON) ? '1' :
               (state == LED_SLOW_FLASH) ? 'f' :
               (state == LED_FAST_FLASH) ? 'F' : '0';

    UNUSED_ARG(led_handle);

    fprintf(stdout, "\033[24;%dH%c", value, cmd);
    fflush(stdout);
    led->state = state;

    return true;
}

static led_handle_st *
led_open()
{
    static int const dummy = 0;
    /* Nothing to do. Don't return NULL though, as that indicates error. */

    return (led_handle_st *)&dummy;
}

static void
led_close(led_handle_st * const led_handle)
{
    UNUSED_ARG(led_handle);
    /* Nothing to do. */
}

static led_st *
iterate_leds(
    platform_leds_st * const platform_leds,
    bool (*cb)(led_st * led, void * user_ctx),
    void * user_ctx)
{
    led_st * led;

    UNUSED_ARG(platform_leds);

    for (size_t i = 0; i < ARRAY_SIZE(leds); i++)
    {
        if (!cb(&leds[i], user_ctx))
        {
            led = &leds[i];
            goto done;
        }
    }

    led = NULL;

done:
    return led;
}

static void
iterate_supported_states(
    void (*cb)(enum led_state_t state, void * user_ctx), void * user_ctx)
{
    cb(LED_OFF, user_ctx);
    cb(LED_ON, user_ctx);
    cb(LED_SLOW_FLASH, user_ctx);
    cb(LED_FAST_FLASH, user_ctx);
}

static char const *
get_led_name(led_st const * const led)
{
    return led->name;
}

static enum led_colour_t
get_led_colour(led_st const * const led)
{
    enum led_colour_t const colour =
        (led != NULL) ? led->colour : (enum led_colour_t)-1;

    return colour;
}

static platform_leds_st *
leds_init()
{
    static int const dummy = 0;
    /* Nothing to do. Don't return NULL though as that indicates error. */
    fprintf(stdout, "\e[?25l");
    fflush(stdout);
    return (platform_leds_st *)&dummy;
}

static void
leds_deinit(platform_leds_st * const platform_leds)
{
    UNUSED_ARG(platform_leds);

    /* Nothing to do. */
    fprintf(stdout, "\e[?25h");
    fflush(stdout);
}

struct platform_led_methods_st const *
platform_leds_methods(int const plugin_version)
{
    static struct platform_led_methods_st const methods =
    {
        .get_led_state = get_led_state,
        .set_led_state = set_led_state,
        .get_led_name = get_led_name,
        .get_led_colour = get_led_colour,
        .open = led_open,
        .close = led_close,
        .iterate_leds = iterate_leds,
        .iterate_supported_states = iterate_supported_states,
        .init = leds_init,
        .deinit = leds_deinit
    };
    bool const version_ok = plugin_version == LED_DAEMON_PLUGIN_VERSION;
    struct platform_led_methods_st const * const platform_methods =
        version_ok ? &methods : NULL;

    return platform_methods;
}

