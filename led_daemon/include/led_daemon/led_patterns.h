#ifndef LED_PATTERNS_H__
#define LED_PATTERNS_H__

#include "led_states.h"

#include <libubox/uloop.h>
#include <libubox/avl.h>

#include <stddef.h>
#include <stdbool.h>

typedef struct led_patterns_st led_patterns_st;

struct led_state_st
{
    char const * led_name;
    enum led_state_t led_state;
    char const * priority;
};

struct pattern_step_st
{
    unsigned time_ms;

    size_t num_leds;
    struct led_state_st * leds;
};

struct led_pattern_st
{
    char const * name;
    bool repeat;
    unsigned play_count;
    size_t num_steps;
    struct pattern_step_st * steps;
    struct pattern_step_st start_step;
    struct pattern_step_st end_step;
};

struct led_daemon_led_pattern_st
{
    struct avl_node node;
    struct led_pattern_st * led_pattern;
};

typedef void
(*list_patterns_cb)(
    struct led_pattern_st const * const led_pattern, void * const user_ctx);

char const *
led_pattern_name(struct led_pattern_st const * led_pattern);

struct led_pattern_st *
led_pattern_lookup(
    led_patterns_st const * led_patterns, char const * pattern_name);

void
led_pattern_list(
    led_patterns_st const * led_patterns, list_patterns_cb cb, void * user_ctx);

void
free_patterns(led_patterns_st const * led_patterns);

led_patterns_st const *
load_patterns(char const * patterns_directory);

#endif /* LED_PATTERNS_H__ */

