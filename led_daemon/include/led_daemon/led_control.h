#ifndef LED_CONTROL_H__
#define LED_CONTROL_H__

#include "led_states.h"
#include "led_priority_context.h"
#include "led_pattern_list.h"
#include "flash_types.h"
#include "platform_specific.h"
#include "priorities.h"

#include <ubus_utils/ubus_connection.h>
#include <libubus.h>
#include <libubox/avl.h>
#include <libubox/uloop.h>
#include <libubox/runqueue.h>

#include <stdbool.h>
#include <stdint.h>

struct flash_context_st
{
    struct flash_times_st const * times;
    bool flash_forever;
    uint32_t remaining_time_ms;
    enum led_state_t final_state;
    enum flash_type_t type;
    uint32_t current_time;

    struct uloop_timeout timer;
    struct platform_led_methods_st const * methods;
};

struct led_state_context_st
{
    /* Required so that the parent led_context can be found. */
    enum led_priority_t priority;
    /* Save the last state set in each priority. */
    enum led_state_t state;

    struct flash_context_st flash;
};

struct led_ctx_st
{
    struct avl_node node;

    led_st * led; /* platform specific LED context. */

    char const * lock_id; /* non-NULL when the LED is locked. */

    led_priority_st * priority_context;
    struct led_state_context_st priorities[LED_PRIORITY_COUNT];
};

typedef struct ledcmd_ctx_st ledcmd_ctx_st;

struct set_state_req_st
{
    char const * led_name;
    char const * lock_id;
    /* Used as the final state when doing timed flashing. */
    enum led_state_t state;
    char const * led_priority;
    enum flash_type_t flash_type;
    uint32_t flash_time_ms;
    bool flash_forever;
};

typedef struct led_ops_handle_st led_ops_handle;

typedef led_ops_handle * (*led_ops_open_fn)(void * led_ops_context);

typedef void (*led_ops_close_fn)(led_ops_handle * led_ops_handle);

typedef void (*set_state_result_cb)(
    char const * led_name,
    bool success,
    char const * state,
    char const * error_msg,
    void * user_context);

typedef bool (*led_ops_set_state_fn)(
    led_ops_handle * led_ops_handle,
    struct set_state_req_st const * set_state_req,
    set_state_result_cb result_cb,
    void * result_context);

typedef void (*get_state_result_cb)(
    char const * led_name,
    bool success,
    char const * led_state,
    char const * lock_id,
    char const * led_priority,
    char const * error_msg,
    void * result_context);

typedef bool (*led_ops_get_state_fn)(
    led_ops_handle * led_ops_handle,
    char const * led_name,
    get_state_result_cb result_cb,
    void * result_context);

typedef void (*activate_priority_result_cb)(
    char const * led_name,
    bool success,
    char const * lock_id,
    char const * error_msg,
    void * result_context);

typedef bool (*led_ops_activate_priority_fn)(
    led_ops_handle * led_ops_handle,
    char const * led_name,
    char const * led_priority,
    char const * lock_id,
    activate_priority_result_cb result_cb,
    void * result_context);

typedef void (*deactivate_priority_result_cb)(
    char const * led_name,
    bool success,
    char const * lock_id,
    char const * error_msg,
    void * result_context);

typedef bool (*led_ops_deactivate_priority_fn)(
    led_ops_handle * led_ops_handle,
    char const * led_name,
    char const * led_priority,
    char const * lock_id,
    deactivate_priority_result_cb result_cb,
    void * result_context);

typedef void (*list_leds_cb)(
    char const * led_name,
    char const * led_colour,
    void * result_context);

typedef void (*led_ops_list_leds_fn)(
    void * led_ops_context,
    list_leds_cb result_cb,
    void * result_context);

typedef void (*led_ops_list_patterns_fn)(
    void * led_ops_context, list_patterns_cb cb, void * user_ctx);

typedef void (*led_ops_list_playing_patterns_fn)(
    void * led_ops_context, list_patterns_cb cb, void * user_ctx);

typedef void (*play_pattern_cb)(
    bool success,
    char const * error_msg,
    void * result_context);

typedef bool (*led_ops_play_pattern_fn)(
    void * led_ops_context,
    char const * pattern_name,
    bool retrigger,
    play_pattern_cb result_cb,
    void * user_ctx);

typedef bool (*led_ops_stop_pattern_fn)(
    void * led_ops_context,
    char const * pattern_name,
    play_pattern_cb result_cb,
    void * user_ctx);

typedef bool (*led_ops_compare_leds_fn)(
    void * led_ops_context,
    char const * const led_a_name,
    char const * const led_a_priority,
    char const * const led_b_name,
    char const * const led_b_priority);

struct led_ops_st
{
    led_ops_open_fn open;
    led_ops_close_fn close;
    led_ops_set_state_fn set_state;
    led_ops_get_state_fn get_state;
    led_ops_activate_priority_fn activate_priority;
    led_ops_deactivate_priority_fn deactivate_priority;
    led_ops_list_leds_fn list_leds;
    led_ops_list_patterns_fn list_patterns;
    led_ops_list_playing_patterns_fn list_playing_patterns;
    led_ops_play_pattern_fn play_pattern;
    led_ops_stop_pattern_fn stop_pattern;
    led_ops_compare_leds_fn compare_leds;
};

ledcmd_ctx_st *
ledcmd_init(
    char const * ubus_path,
    char const * patterns_directory,
    char const * aliases_directory,
    char const * const backend_path);

void
ledcmd_deinit(ledcmd_ctx_st * context);

#endif /* LED_CONTROL_H__ */

