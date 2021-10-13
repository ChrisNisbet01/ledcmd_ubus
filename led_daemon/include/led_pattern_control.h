#ifndef LED_PATTERN_CONTROL_H__
#define LED_PATTERN_CONTROL_H__

#include "led_control.h"

#include <stdbool.h>

typedef struct led_patterns_context_st led_patterns_context_st;

bool
led_pattern_stop_pattern(
    led_patterns_context_st * patterns_context,
    char const * pattern_name,
    char const ** error_msg);

bool
led_pattern_play_pattern(
    led_patterns_context_st * patterns_context,
    char const * pattern_name,
    bool const retrigger,
    char const ** error_msg);

void
led_pattern_list_playing_patterns(
    struct led_patterns_context_st * const patterns_context,
    list_patterns_cb const cb,
    void * const user_ctx);

void
led_pattern_list_patterns(
    led_patterns_context_st * patterns_context,
    list_patterns_cb cb,
    void * user_ctx);

void
led_patterns_deinit(led_patterns_context_st * patterns_context);

/*
 * patterns_dir: The path to the JSON patterns directory.
 * led_ops: callbacks for opening/getting/setting/closing LEDs
 * led_ops_context: The context to pass to led_ops->opn().
 * Returns: An opaque type to be passed when requesting a pattern to be played,
 * and when de-initialising.
 */
led_patterns_context_st *
led_patterns_init(
    char const * patterns_directory,
    struct led_ops_st const * led_ops,
    void * led_ops_context);

#endif /* LED_PATTERN_CONTROL_H__ */

