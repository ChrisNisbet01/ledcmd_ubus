#ifndef LIB_LED_PATTERN_H__
#define LIB_LED_PATTERN_H__

#include <stdbool.h>

typedef struct ledcmd_ctx_st ledcmd_ctx_st;

typedef void (*led_list_patterns_cb)(
    char const * pattern_name, void * user_context);

typedef void (*led_play_pattern_cb)(
    bool const success, char const * error_msg, void * user_ctx);

bool
led_list_patterns(
    struct ledcmd_ctx_st const * ledcmd_ctx,
    led_list_patterns_cb cb,
    void * cb_context);

bool
led_list_playing_patterns(
    struct ledcmd_ctx_st const * ledcmd_ctx,
    led_list_patterns_cb cb,
    void * cb_context);

bool
led_play_pattern(
    struct ledcmd_ctx_st const * ledcmd_ctx,
    char const * pattern_name,
    bool retrigger,
    led_play_pattern_cb cb,
    void * cb_context);

bool
led_stop_pattern(
    struct ledcmd_ctx_st const * ledcmd_ctx,
    char const * pattern_name,
    led_play_pattern_cb cb,
    void * cb_context);

#endif /* LIB_LED_PATTERN_H__ */

