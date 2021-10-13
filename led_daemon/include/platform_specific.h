#ifndef PLATFORM_SPECIFIC_H__
#define PLATFORM_SPECIFIC_H__

#include "led_states.h"
#include "led_colours.h"

#include <stdbool.h>

#define LED_DAEMON_PLUGIN_VERSION 1

typedef struct led led_st;
typedef struct led_handle_st led_handle_st;
typedef struct platform_leds_st platform_leds_st;

typedef enum led_state_t
(* platform_led_get_state_fn)(led_handle_st * led_handle, led_st const * led);

typedef bool
(*platform_led_set_state_fn)(
    led_handle_st * led_handle, led_st * led, enum led_state_t state);

typedef led_handle_st *
(*platform_led_open_fn)(void);

typedef void
(*platform_led_close_fn)(led_handle_st * led_handle);

typedef led_st *
(*platform_leds_iterate_fn)(
    platform_leds_st * const platform_leds,
    bool (*cb)(led_st * led, void * user_ctx),
    void * user_ctx);

typedef void
(*platform_leds_iterate_supported_states_fn)(
    void (*cb)(enum led_state_t state, void * user_ctx),
    void * user_ctx);

typedef char const *
(*platform_led_get_name_fn)(led_st const * led);

typedef enum led_colour_t
(* platform_led_get_colour_fn)(led_st const * led);

typedef platform_leds_st *
(*platform_leds_init_fn)(void);

typedef void
(*platform_leds_deinit_fn)(platform_leds_st * const platform_leds);

struct platform_led_methods_st
{
    /* Get the state of the specified LED. */
    platform_led_get_state_fn get_led_state;

    /* Set the state of the specified LED. */
    platform_led_set_state_fn set_led_state;

    /* Get the system name for the specified LED. */
    platform_led_get_name_fn get_led_name;

    /* Get the colour (if known) of the specified LED. */
    platform_led_get_colour_fn get_led_colour;

    /* Call before getting/setting LEDs. */
    platform_led_open_fn open;

    /* Call when finished getting/setting LEDs. */
    platform_led_close_fn close;

    /*
     * Call the callback for each supported LED. To terminate the iteration
     * early, return false from the callback, else return true.
     */
    platform_leds_iterate_fn iterate_leds;

    /* Call the callback for each supported state. */
    platform_leds_iterate_supported_states_fn iterate_supported_states;

    /*
     * Call at init time so that the driver can reserve the required resources.
     */
    platform_leds_init_fn init;

    /*
     * Call at cleanup time so that the driver can release the resources
     * reserved by platform_leds_init().
     */
    platform_leds_deinit_fn deinit;
};

typedef struct platform_led_methods_st const *
    (* platform_leds_methods_fn)(int plugin_version);

struct platform_led_methods_st const *
platform_leds_methods(int plugin_version);

#endif /* PLATFORM_SPECIFIC_H__ */

