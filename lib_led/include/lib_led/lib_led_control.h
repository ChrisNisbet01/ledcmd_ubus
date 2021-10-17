#ifndef LIB_LED_CONTROL_H__
#define LIB_LED_CONTROL_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct ledcmd_ctx_st ledcmd_ctx_st;

struct led_lock_result_st
{
    char const * error_msg;
    char const * lock_id;
    bool success;
};

typedef void (*led_lock_result_cb)(
    struct led_lock_result_st const * lock_result,
    void * user_context);

typedef void (*led_names_cb)(
    char const * led_name,
    void * user_context);

typedef void (*led_states_cb)(
    char const * led_state,
    void * user_context);

struct led_get_set_result_st
{
    bool success;
    char const * led_name;
    char const * led_state;
    char const * error_msg;
    char const * lock_id;
    char const * led_priority;
};

typedef void (*led_get_set_cb)(
    struct led_get_set_result_st const * result,
    void * user_context);

bool
led_activate_or_deactivate(
    ledcmd_ctx_st const * ledcmd_ctx,
    bool lock_it,
    char const * led_name,
    char const * const led_priority,
    char const * lock_id,
    led_lock_result_cb cb,
    void * cb_context);

bool
led_get_names(
    ledcmd_ctx_st const * ledcmd_ctx,
    led_names_cb cb,
    void * cb_context);

bool
led_get_states(
    ledcmd_ctx_st const * ledcmd_ctx,
    led_names_cb cb,
    void * cb_context);

bool
led_get_set_request(
    ledcmd_ctx_st const * ledcmd_ctx,
    char const * cmd,
    char const * state,
    char const * led_name,
    char const * lock_id,
    char const * led_priority,
    char const * flash_type,
    uint32_t flash_time_ms,
    led_get_set_cb cb,
    void * cb_context);

#endif /* LIB_LED_CONTROL_H__ */

