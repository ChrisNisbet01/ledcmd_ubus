#include "led_control.h"
#include "led_daemon_ubus.h"
#include "led_lock.h"
#include "led_pattern_control.h"
#include "led_aliases.h"
#include "platform_leds_plugin.h"

#include <lib_led/string_constants.h>
#include <lib_log/log.h>
#include <ubus_utils/ubus_utils.h>

#include <stdlib.h>
#include <string.h>

struct ledcmd_ctx_st
{
    struct ledcmd_ubus_context_st * ubus_context;
    void * platform_leds_handle;
    struct avl_tree all_leds;
    platform_leds_st * platform_leds;
    bool supported_states[LED_STATE_MAX];
    struct platform_led_methods_st const * methods;
    struct led_patterns_context_st * patterns_context;
    led_aliases_st const * led_aliases;
};

static void
free_led_ctx(struct led_ctx_st * const led_ctx)
{
    if (led_ctx == NULL)
    {
        goto done;
    }

    led_priority_free(led_ctx->priority_context);
    destroy_led_lock_id(led_ctx);
    free(led_ctx);

done:
    return;
}

static void
update_flash_timer(struct flash_context_st * const flash_ctx)
{
    bool const timer_required = flash_ctx->current_time > 0;

    if (timer_required)
    {
        uloop_timeout_set(&flash_ctx->timer, flash_ctx->current_time);
    }
    else
    {
        uloop_timeout_cancel(&flash_ctx->timer);
    }
}

static bool
set_state(
    struct platform_led_methods_st const * const methods,
    led_handle_st * const led_handle,
    struct led_ctx_st * const led_ctx,
    struct led_state_context_st * const led_priority_ctx,
    enum led_state_t const state)
{
    /*
     * Note that the new state might not be any different to the current state,
     * but updating the LED will ensure that each state assignment puts the
     * LED in the desired state - some other process might have twiddled the
     * LED.
     */
    bool const priority_is_less = priority_compare(
            led_priority_ctx->priority,
            led_priority_highest_priority(led_ctx->priority_context))
        == PRIORITY_LESS;
    bool const state_set =
        priority_is_less
        || methods->set_led_state(led_handle, led_ctx->led, state);

    if (state_set)
    {
        led_priority_ctx->state = state;
        update_flash_timer(&led_priority_ctx->flash);
    }

    return state_set;
}

static void
stop_flashing(struct flash_context_st * const flash_ctx)
{
    flash_ctx->type = LED_FLASH_TYPE_NONE;
    flash_ctx->final_state = LED_STATE_UNKNOWN;
    flash_ctx->current_time = 0;
    update_flash_timer(flash_ctx);
}

static bool
led_ctx_activate_priority(
    struct platform_led_methods_st const * const methods,
    struct led_ctx_st * const led_ctx,
    led_handle_st * const led_handle,
    enum led_priority_t const priority)
{
    bool priority_set;
    enum led_priority_t const highest_priority =
        led_priority_priority_activate(led_ctx->priority_context, priority);
    struct led_state_context_st * const led_priority_ctx =
        &led_ctx->priorities[priority];
    bool const should_turn_led_off =
        priority == LED_PRIORITY_LOCKED
        || priority == LED_PRIORITY_ALTERNATE
        || led_priority_ctx->state == LED_STATE_UNKNOWN;

    if (should_turn_led_off)
    {
        led_priority_ctx->state = LED_OFF;
        stop_flashing(&led_priority_ctx->flash);
    }

    if (priority_compare(highest_priority, priority) == PRIORITY_EQUAL)
    {
        /* This is now the current priority, so update the physical LED. */
        set_state(
            methods, led_handle, led_ctx, led_priority_ctx, led_priority_ctx->state);
    }

    priority_set = true;

    return priority_set;
}

static bool
led_ctx_deactivate_priority(
    struct platform_led_methods_st const * const methods,
    struct led_ctx_st * const led_ctx,
    led_handle_st * const led_handle,
    enum led_priority_t const priority)
{
    enum led_priority_t const new_highest_priority =
        led_priority_priority_deactivate(led_ctx->priority_context, priority);

    if (priority_compare(new_highest_priority, priority) == PRIORITY_LESS)
    {
        /*
         * The priority just deactivated must have been the highest,
         * so set the physical LED to the new highest priority state.
         */
        struct led_state_context_st * const led_priority_ctx =
            &led_ctx->priorities[new_highest_priority];
        enum led_state_t const state = led_priority_ctx->state;

        set_state(methods, led_handle, led_ctx, led_priority_ctx, state);
    }

    bool const priority_set = true;

    return priority_set;
}

static bool
led_ctx_get_priority_to_update(
    struct led_ctx_st * const led_ctx,
    char const * const lock_id,
    char const * const led_priority,
    enum led_priority_t * const led_priority_to_update,
    char const ** const error_msg)
{
    bool got_priority;

    /*
     * If no lock ID is supplied, the request will update the priority determined
     * by the priority name.
     * If a lock_id is supplied, the request will update the 'locked' priority,
     * but only if the LED is locked and the lock_id matches the ID used to
     * lock the LED.
     */
    if (!led_priority_by_name(led_priority, led_priority_to_update))
    {
        got_priority = false;
        *error_msg = "Unknown LED priority";
    }
    else if (*led_priority_to_update != LED_PRIORITY_LOCKED)
    {
        got_priority = true;
    }
    else
    {
        if (lock_id == NULL)
        {
            got_priority = false;
            *error_msg = "No lock ID supplied";
        }
        else if (led_ctx->lock_id == NULL)
        {
            got_priority = false;
            *error_msg = "LED isn't locked";
        }
        else if (strcmp(lock_id, led_ctx->lock_id) != 0)
        {
            got_priority = false;
            *error_msg = "Incorrect_lock_id";
        }
        else
        {
            got_priority = true;
        }
    }

    return got_priority;
}

static void
update_flash_time_remaining(struct flash_context_st * const flash_ctx)
{
    if (!flash_ctx->flash_forever)
    {
        if (flash_ctx->current_time >= flash_ctx->remaining_time_ms)
        {
            flash_ctx->remaining_time_ms = 0;
        }
        else if (flash_ctx->current_time > 0)
        {
            flash_ctx->remaining_time_ms -= flash_ctx->current_time;
        }
    }
}

static void
set_next_state(
    struct platform_led_methods_st const * const methods,
    struct led_state_context_st * const led_priority_ctx,
    enum led_state_t const next_state)
{
    led_handle_st * const led_handle = methods->open();

    if (led_handle != NULL)
    {
        struct led_ctx_st * const led_ctx =
            container_of(led_priority_ctx,
                         struct led_ctx_st,
                         priorities[led_priority_ctx->priority]);

        set_state(methods, led_handle, led_ctx, led_priority_ctx, next_state);

        methods->close(led_handle);
    }
}

static void
update_flashing(struct flash_context_st * const flash_ctx)
{
    struct platform_led_methods_st const * const methods = flash_ctx->methods;
    struct led_state_context_st * const led_priority_ctx =
        container_of(flash_ctx, struct led_state_context_st, flash);
    enum led_state_t next_state;

    if (!flash_ctx->flash_forever && flash_ctx->remaining_time_ms == 0)
    {
        next_state = flash_ctx->final_state;
        stop_flashing(flash_ctx);
    }
    else
    {
        next_state = (led_priority_ctx->state == LED_ON) ? LED_OFF : LED_ON;
        flash_ctx->current_time =
            (next_state == LED_ON) ? flash_ctx->times->on_time_ms : flash_ctx->times->off_time_ms;
    }

    set_next_state(methods, led_priority_ctx, next_state);
}

static void
led_flash_timeout(struct uloop_timeout * const timeout)
{
    struct flash_context_st * const flash_ctx =
        container_of(timeout, struct flash_context_st, timer);

    update_flash_time_remaining(flash_ctx);
    update_flashing(flash_ctx);
}

static enum led_state_t
initialise_flashing(
    struct ledcmd_ctx_st * const context,
    struct flash_context_st * const flash_ctx,
    struct set_state_req_st const * const request)
{
    struct platform_led_methods_st const * const methods = context->methods;
    enum led_state_t initial_state;

    flash_ctx->methods = methods;
    flash_ctx->timer.cb = led_flash_timeout;
    flash_ctx->final_state =
        (request->state != LED_STATE_UNKNOWN) ? request->state : LED_ON;

    flash_ctx->times = led_flash_times_lookup(request->flash_type);

    if (request->flash_type == LED_FLASH_TYPE_ONE_SHOT)
    {
        /* The LED starts in the opposite state to the final state. */
        initial_state = (flash_ctx->final_state == LED_OFF) ? LED_ON : LED_OFF;
        /* If the caller doesn't supply the one-shot time, use the default. */
        flash_ctx->current_time =
            (request->flash_time_ms > 0) ? request->flash_time_ms : flash_ctx->times->on_time_ms;
    }
    else if (flash_ctx->times->on_time_ms > 0 &&
             (request->flash_forever || request->flash_time_ms > 0))
    {
        /* The LED starts in the opposite state to the final state. */
        initial_state = (flash_ctx->final_state == LED_OFF) ? LED_ON : LED_OFF;
        flash_ctx->current_time = flash_ctx->times->on_time_ms;
    }
    else
    {
        flash_ctx->current_time = 0;
        initial_state = request->state;
    }

    flash_ctx->remaining_time_ms = request->flash_time_ms;
    flash_ctx->flash_forever = request->flash_forever;

    return initial_state;
}

static bool
led_ctx_set_state(
    struct ledcmd_ctx_st * const context,
    struct led_ctx_st * const led_ctx,
    led_handle_st * const led_handle,
    struct set_state_req_st const * const request_in,
    char const * * const error_msg)
{
    bool success;
    enum led_priority_t priority_to_update;
    struct set_state_req_st request = *request_in;

    if (!led_ctx_get_priority_to_update(
            led_ctx,
            request.lock_id,
            request.led_priority,
            &priority_to_update,
            error_msg))
    {
        success = false;
        goto done;
    }

    /*
     * If the requested state isn't supported by the platform, map it to one of
     * the flash types supported by this daemon.
     */
    if (!context->supported_states[request.state]
        && request.flash_type == LED_FLASH_TYPE_NONE)
    {
        request.flash_type = led_flash_type_from_state(request.state);
        if (request.flash_type == LED_FLASH_TYPE_NONE)
        {
            success = false;
            goto done;
        }
        request.state = LED_ON;
        request.flash_forever = true;
    }

    struct led_state_context_st * const led_priority_ctx =
        &led_ctx->priorities[priority_to_update];
    enum led_state_t const initial_state =
        initialise_flashing(context, &led_priority_ctx->flash, &request);

    if (!set_state(
            context->methods, led_handle, led_ctx, led_priority_ctx, initial_state))
    {
        *error_msg = "Can't set LED state";
        success = false;
        goto done;
    }

    success = true;

done:
    return success;
}

static bool
ledcmd_ctx_any_led_locked(struct ledcmd_ctx_st * const context)
{
    return led_ctx_any_led_locked(&context->all_leds);
}

static void
append_led_state(
    led_handle_st * const led_handle,
    struct platform_led_methods_st const * const methods,
    struct led_ctx_st * const led_ctx,
    get_state_result_cb const result_cb,
    void * const result_context)
{
    /*
     * If the physical LED state can't be determined by the platform-dependent
     * code, return the last state written by this driver.
     */
    led_st const * const led = led_ctx->led;
    enum led_priority_t const current_priority =
        led_priority_highest_priority(led_ctx->priority_context);
    enum led_state_t const physical_led_state =
        methods->get_led_state(led_handle, led);
    enum led_state_t const led_state =
        (physical_led_state == LED_STATE_UNKNOWN)
        ? led_ctx->priorities[current_priority].state
        : physical_led_state;

    result_cb(
        methods->get_led_name(led_ctx->led),
        true,
        led_state_query_name(led_state),
        led_ctx->lock_id,
        led_priority_to_name(current_priority),
        NULL,
        result_context);
}

struct set_state_alias_st
{
    struct ledcmd_ctx_st * context;
    led_handle_st * led_handle;
    struct set_state_req_st const * request;
    set_state_result_cb result_cb;
    void * result_context;
};

static bool
led_alias_set_state_cb(char const * const led_name, void * user_ctx)
{
    struct set_state_alias_st const * const set_state_alias = user_ctx;
    struct ledcmd_ctx_st * const context = set_state_alias->context;
    led_handle_st * const led_handle = set_state_alias->led_handle;
    struct set_state_req_st const * const request = set_state_alias->request;
    set_state_result_cb result_cb = set_state_alias->result_cb;
    void * result_context = set_state_alias->result_context;
    struct led_ctx_st * const led_ctx =
        avl_find_element(&context->all_leds, led_name, led_ctx, node);

    /*
     * Don't generate an error if the aliased LED isn't found. Some LED names
     * may exist on some platforms, but not others.
     */
    if (led_ctx == NULL)
    {
        goto done;
    }

    char const * error_msg = NULL;
    bool const success = led_ctx_set_state(
        context, led_ctx, led_handle, request, &error_msg);

    if (result_cb != NULL)
    {
        result_cb(
            led_name, success, led_state_name(request->state), error_msg, result_context);
    }

done:;
    bool const continue_iteration = true;

    return continue_iteration;
}

static bool
set_aliased_led_states(
    struct ledcmd_ctx_st * const context,
    led_handle_st * const led_handle,
    struct set_state_req_st const * const request,
    set_state_result_cb result_cb,
    void * result_context)
{
    led_alias_st const * const led_alias =
        led_alias_lookup(context->led_aliases, request->led_name);
    bool const found_aliased_leds = led_alias != NULL;

    if (led_alias != NULL)
    {
        struct set_state_alias_st set_state_alias =
        {
            .context = context,
            .led_handle = led_handle,
            .request = request,
            .result_cb = result_cb,
            .result_context = result_context
        };
        led_alias_iterate(led_alias, led_alias_set_state_cb, (void *)&set_state_alias);
    }

    return found_aliased_leds;
}

struct get_state_alias_st
{
    struct ledcmd_ctx_st * context;
    led_handle_st * led_handle;
    get_state_result_cb result_cb;
    void * result_context;
};

static bool
led_alias_get_state_cb(char const * const led_name, void * user_ctx)
{
    struct get_state_alias_st const * const get_state_alias = user_ctx;
    struct ledcmd_ctx_st * const context = get_state_alias->context;
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = get_state_alias->led_handle;
    get_state_result_cb result_cb = get_state_alias->result_cb;
    void * result_context = get_state_alias->result_context;
    struct led_ctx_st * const led_ctx =
        avl_find_element(&context->all_leds, led_name, led_ctx, node);

    /*
     * Don't generate an error if the aliased LED isn't found. Some LED names
     * may exist on some platforms, but not others.
     */
    if (led_ctx == NULL)
    {
        goto done;
    }

    append_led_state(led_handle, methods, led_ctx, result_cb, result_context);

done:;
    bool const continue_iteration = true;

    return continue_iteration;
}

static bool
get_aliased_led_states(
    struct ledcmd_ctx_st * const context,
    led_handle_st * const led_handle,
    char const * const led_name,
    get_state_result_cb result_cb,
    void * result_context)
{
    led_alias_st const * const led_alias =
        led_alias_lookup(context->led_aliases, led_name);
    bool const found_aliased_leds = led_alias != NULL;

    if (led_alias != NULL)
    {
        struct get_state_alias_st get_state_alias =
        {
            .context = context,
            .led_handle = led_handle,
            .result_cb = result_cb,
            .result_context = result_context
        };
        led_alias_iterate(led_alias, led_alias_get_state_cb, (void *)&get_state_alias);
    }

    return found_aliased_leds;
}

struct activate_alias_st
{
    struct ledcmd_ctx_st * context;
    led_handle_st * led_handle;
    enum led_priority_t priority;
    char const * lock_id;
    activate_priority_result_cb const result_cb;
    void * const result_context;
};

static bool
led_alias_deactivate_cb(char const * const led_name, void * user_ctx)
{
    struct activate_alias_st const * const activate_alias = user_ctx;
    struct ledcmd_ctx_st * const context = activate_alias->context;
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = activate_alias->led_handle;
    enum led_priority_t const priority = activate_alias->priority;
    char const * const lock_id = activate_alias->lock_id;
    set_state_result_cb result_cb = activate_alias->result_cb;
    void * result_context = activate_alias->result_context;
    struct led_ctx_st * const led_ctx =
        avl_find_element(&context->all_leds, led_name, led_ctx, node);

    /*
     * Don't generate an error if the aliased LED isn't found. Some LED names
     * may exist on some platforms, but not others.
     */
    if (led_ctx == NULL)
    {
        goto done;
    }

    char const * error_msg = NULL;
    bool const unlocked =
        priority != LED_PRIORITY_LOCKED
        || led_ctx_unlock_led(led_ctx, lock_id, &error_msg);

    if (unlocked)
    {
        led_ctx_deactivate_priority(methods, led_ctx, led_handle, priority);
    }

    result_cb(led_name, unlocked, led_ctx->lock_id, error_msg, result_context);

done:;
    bool const continue_iteration = true;

    return continue_iteration;
}

static bool
deactivate_aliased_leds(
    struct ledcmd_ctx_st * const context,
    led_handle_st * const led_handle,
    char const * const led_name,
    char const * const lock_id,
    enum led_priority_t const priority,
    activate_priority_result_cb const result_cb,
    void * const result_context)
{
    led_alias_st const * const led_alias = led_alias_lookup(context->led_aliases, led_name);
    bool const found_aliased_leds = led_alias != NULL;

    if (led_alias != NULL)
    {
        struct activate_alias_st const activate_alias =
        {
            .context = context,
            .led_handle = led_handle,
            .priority = priority,
            .lock_id = lock_id,
            .result_cb = result_cb,
            .result_context = result_context,
        };

        led_alias_iterate(led_alias, led_alias_deactivate_cb, (void *)&activate_alias);
    }

    return found_aliased_leds;
}

static bool
led_alias_activate_cb(char const * const led_name, void * user_ctx)
{
    struct activate_alias_st const * const activate_alias = user_ctx;
    struct ledcmd_ctx_st * const context = activate_alias->context;
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = activate_alias->led_handle;
    enum led_priority_t const priority = activate_alias->priority;
    char const * const lock_id = activate_alias->lock_id;
    set_state_result_cb result_cb = activate_alias->result_cb;
    void * result_context = activate_alias->result_context;
    struct led_ctx_st * const led_ctx =
        avl_find_element(&context->all_leds, led_name, led_ctx, node);

    /*
     * Don't generate an error if the aliased LED isn't found. Some LED names
     * may exist on some platforms, but not others.
     */
    if (led_ctx == NULL)
    {
        goto done;
    }

    char const * error_msg = NULL;
    bool const locked =
        (priority != LED_PRIORITY_LOCKED) || led_ctx_lock_led(led_ctx, lock_id, &error_msg);

    if (locked)
    {
        led_ctx_activate_priority(methods, led_ctx, led_handle, priority);
    }

    result_cb(led_name, locked, led_ctx->lock_id, error_msg, result_context);

done:;
    bool const continue_iteration = true;

    return continue_iteration;
}

static bool
activate_aliased_leds(
    struct ledcmd_ctx_st * const context,
    led_handle_st * const led_handle,
    char const * const led_name,
    char const * const lock_id,
    enum led_priority_t const priority,
    activate_priority_result_cb const result_cb,
    void * const result_context)
{
    led_alias_st const * const led_alias = led_alias_lookup(context->led_aliases, led_name);
    bool const found_aliased_leds = led_alias != NULL;

    if (led_alias != NULL)
    {
        struct activate_alias_st const activate_alias =
        {
            .context = context,
            .led_handle = led_handle,
            .priority = priority,
            .lock_id = lock_id,
            .result_cb = result_cb,
            .result_context = result_context,
        };

        led_alias_iterate(led_alias, led_alias_activate_cb, (void *)&activate_alias);
    }

    return found_aliased_leds;
}

struct list_aliased_leds_st
{
    list_leds_cb result_cb;
    void * result_context;
};

static bool
list_aliases_cb(
    led_alias_st const * const led_alias, void * const user_ctx)
{
    char const * const alias_name = led_alias_name(led_alias);
    struct list_aliased_leds_st const * const list_aliased_leds = user_ctx;
    list_leds_cb const result_cb = list_aliased_leds->result_cb;
    void * const result_context = list_aliased_leds->result_context;

    result_cb(alias_name, "", result_context);

    bool const continue_iteration = true;

    return continue_iteration;
}

static void list_all_aliased_leds(
    struct ledcmd_ctx_st * const context,
    list_leds_cb const result_cb,
    void * const result_context)
{
    struct list_aliased_leds_st const aliased_leds =
    {
        .result_cb = result_cb,
        .result_context = result_context
    };

    led_aliases_iterate(context->led_aliases, list_aliases_cb, (void *)&aliased_leds);

    return;
}

struct compare_aliased_leds_st
{
    struct ledcmd_ctx_st * context;
    struct led_ctx_st * led_ctx;
    bool found_match;
};

static bool
compare_aliased_leds_cb(char const * const led_name, void * user_ctx)
{
    struct compare_aliased_leds_st * const compare_aliased_leds = user_ctx;
    struct ledcmd_ctx_st * const context = compare_aliased_leds->context;
    struct led_ctx_st * const led_ctx = compare_aliased_leds->led_ctx;
    struct led_ctx_st * const led_b_ctx =
        avl_find_element(&context->all_leds, led_name, led_b_ctx, node);

    if (led_ctx == led_b_ctx)
    {
        compare_aliased_leds->found_match = true;
    }

    /* Continue with the iteration until a match is found. */
    return !compare_aliased_leds->found_match;
}

static bool
compare_led_ctx_to_aliased_leds(
    struct ledcmd_ctx_st * const context,
    struct led_ctx_st * led_ctx,
    char const * const led_name)
{
    bool found_match;
    led_alias_st const * const led_alias = led_alias_lookup(context->led_aliases, led_name);

    if (led_alias == NULL)
    {
        found_match = NULL;
        goto done;
    }

    struct compare_aliased_leds_st compare_aliased_leds =
    {
        .context = context,
        .led_ctx = led_ctx,
        .found_match = false
    };

    led_alias_iterate(led_alias, compare_aliased_leds_cb, &compare_aliased_leds);
    found_match = compare_aliased_leds.found_match;

done:
    return found_match;
}

static bool
compare_led_ctx_to_led_name(
    struct ledcmd_ctx_st * const context,
    struct led_ctx_st * led_ctx,
    char const * const led_name)
{
    bool leds_match;

    bool const all_leds = strcasecmp(led_name, _led_all) == 0;

    if (all_leds)
    {
        leds_match = true;
        goto done;
    }

    bool const found_matching_aliased_leds =
        compare_led_ctx_to_aliased_leds(context, led_ctx, led_name);

    if (found_matching_aliased_leds)
    {
        leds_match = true;
        goto done;
    }

    struct led_ctx_st * const led_b_ctx =
        avl_find_element(&context->all_leds, led_name, led_b_ctx, node);

    if (led_ctx == led_b_ctx)
    {
        leds_match = true;
        goto done;
    }

    leds_match = false;

done:
    return leds_match;
}

struct led_to_aliased_leds_st
{
    struct ledcmd_ctx_st * context;
    char const * led_b_name;
    bool found_match;
};

static bool
compare_led_to_aliased_leds_cb(
    char const * const led_name, void * const user_ctx)
{
    struct led_to_aliased_leds_st * const led_to_aliased_leds = user_ctx;
    struct ledcmd_ctx_st * const context = led_to_aliased_leds->context;
    struct led_ctx_st * const led_a_ctx =
        avl_find_element(&context->all_leds, led_name, led_a_ctx, node);

    if (led_a_ctx == NULL)
    {
        goto done;
    }

    char const * const led_b_name = led_to_aliased_leds->led_b_name;

    if (compare_led_ctx_to_led_name(context, led_a_ctx, led_b_name))
    {
        led_to_aliased_leds->found_match = true;
    }

done:
    return !led_to_aliased_leds->found_match;
}

static bool
compare_led_to_aliased_leds(
    struct ledcmd_ctx_st * const context,
    char const * const led_a_name,
    char const * const led_b_name)
{
    bool found_match;
    led_alias_st const * const led_alias = led_alias_lookup(context->led_aliases, led_a_name);

    if (led_alias == NULL)
    {
        found_match = false;
        goto done;
    }

    struct led_to_aliased_leds_st led_to_aliased_leds =
    {
        .context = context,
        .led_b_name = led_b_name,
        .found_match = false
    };

    led_alias_iterate(led_alias, compare_led_to_aliased_leds_cb, &led_to_aliased_leds);
    found_match = led_to_aliased_leds.found_match;

done:
    return found_match;
}

struct led_ops_handle_st
{
    struct ledcmd_ctx_st * ledcmd_context;
    led_handle_st * led_handle;
};

static bool
led_ops_compare_leds(
    void * const led_ops_context,
    char const * const led_a_name,
    char const * const led_a_priority,
    char const * const led_b_name,
    char const * const led_b_priority)
{
    bool leds_match;

    /* Compare priorities first. If they don't match there is no need to
     * compare the LEDs themselves.
     */
    enum led_priority_t priority_a = 0;
    bool const got_priority_a = led_priority_by_name(led_a_priority, &priority_a);
    enum led_priority_t priority_b = 0;
    bool const got_priority_b = led_priority_by_name(led_b_priority, &priority_b);

    if (!got_priority_a || !got_priority_b || priority_a != priority_b)
    {
        leds_match = false;
        goto done;
    }

    if (led_ops_context == NULL)
    {
        leds_match = false;
        goto done;
    }

    struct ledcmd_ctx_st * const context = led_ops_context;
    bool const match_all_leds = strcasecmp(led_a_name, _led_all) == 0;

    if (match_all_leds)
    {
        struct led_ctx_st * led_ctx;

        avl_for_each_element(&context->all_leds, led_ctx, node)
        {
            if (compare_led_ctx_to_led_name(context, led_ctx, led_b_name))
            {
                leds_match = true;
                goto done;
            }
        }
    }
    else
    {
        bool const found_matching_aliased_leds =
            compare_led_to_aliased_leds(context, led_a_name, led_b_name);

        if (found_matching_aliased_leds)
        {
            leds_match = true;
            goto done;
        }

        struct led_ctx_st * const led_ctx =
            avl_find_element(&context->all_leds, led_a_name, led_ctx, node);

        if (led_ctx != NULL)
        {
            if (compare_led_ctx_to_led_name(context, led_ctx, led_b_name))
            {
                leds_match = true;
                goto done;
            }
        }
    }

    leds_match = false;

done:
    return leds_match;
}

static void
led_ops_close(struct led_ops_handle_st * const led_ops_handle)
{
    if (led_ops_handle == NULL)
    {
        goto done;
    }

    led_handle_st * const led_handle = led_ops_handle->led_handle;

    if (led_handle != NULL)
    {
        struct ledcmd_ctx_st * const ledcmd_context =
            led_ops_handle->ledcmd_context;
        struct platform_led_methods_st const * const methods =
            ledcmd_context->methods;

        methods->close(led_handle);
    }

    free(led_ops_handle);

done:
    return;
}

static struct led_ops_handle_st *
led_ops_open(void * const led_ops_context)
{
    struct led_ops_handle_st * led_ops_handle = calloc(1, sizeof *led_ops_handle);

    if (led_ops_handle == NULL)
    {
        goto done;
    }

    struct ledcmd_ctx_st * const context = led_ops_context;
    led_ops_handle->ledcmd_context = context;

    struct platform_led_methods_st const * const methods = context->methods;

    led_ops_handle->led_handle = methods->open();
    if (led_ops_handle->led_handle == NULL)
    {
        led_ops_close(led_ops_handle);
        led_ops_handle = NULL;
        goto done;
    }

done:
    return led_ops_handle;
}

static bool
led_ops_set_state(
    led_ops_handle * const led_ops_handle,
    struct set_state_req_st const * const set_state_req_in,
    set_state_result_cb result_cb,
    void * result_context)
{
    bool success;

    if (led_ops_handle == NULL)
    {
        success = false;
        goto done;
    }

    struct ledcmd_ctx_st * const context = led_ops_handle->ledcmd_context;
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = led_ops_handle->led_handle;

    if (led_handle == NULL)
    {
        success = false;
        goto done;
    }

    struct set_state_req_st const * const request = set_state_req_in;

    if (request->led_name == NULL || request->state == LED_STATE_UNKNOWN)
    {
        success = false;
        goto done;
    }

    bool const set_all = strcasecmp(request->led_name, _led_all) == 0;

    if (set_all)
    {
        struct led_ctx_st * led_ctx;

        avl_for_each_element(&context->all_leds, led_ctx, node)
        {
            struct set_state_req_st set_all_request = *request;

            set_all_request.led_name = methods->get_led_name(led_ctx->led);

            char const * error_msg = NULL;
            bool const result = led_ctx_set_state(
                context,
                led_ctx,
                led_handle,
                &set_all_request,
                &error_msg);

            if (result_cb != NULL)
            {
                result_cb(
                    set_all_request.led_name,
                    result,
                    led_state_name(set_all_request.state),
                    error_msg,
                    result_context);
            }
        }
        success = true;
    }
    else
    {
        bool const found_aliased_leds =
            set_aliased_led_states(context, led_handle, request, result_cb, result_context);
        struct led_ctx_st * const led_ctx =
            avl_find_element(&context->all_leds, request->led_name, led_ctx, node);
        char const * error_msg = NULL;

        if (led_ctx != NULL)
        {
            success = led_ctx_set_state(
                context,
                led_ctx,
                led_handle,
                request,
                &error_msg);
        }
        else if (!found_aliased_leds)
        {
            error_msg = _led_unknown_led;
            success = false;
        }
        else
        {
            success = true;
        }

        if (result_cb != NULL)
        {
            result_cb(
                request->led_name,
                success,
                led_state_name(request->state),
                error_msg,
                result_context);
        }
    }

done:
    return success;
}

static bool
led_ops_get_state(
    led_ops_handle * const led_ops_handle,
    char const * const led_name,
    get_state_result_cb result_cb,
    void * result_context)
{
    bool success;

    if (led_ops_handle == NULL)
    {
        success = false;
        goto done;
    }

    struct ledcmd_ctx_st * const context = led_ops_handle->ledcmd_context;
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = led_ops_handle->led_handle;

    if (led_handle == NULL)
    {
        success = false;
        goto done;
    }

    bool const get_all_leds = strcasecmp(led_name, _led_all) == 0;

    if (get_all_leds)
    {
        struct led_ctx_st * led_ctx;

        avl_for_each_element(&context->all_leds, led_ctx, node)
        {
            append_led_state(
                led_handle, methods, led_ctx, result_cb, result_context);
        }
    }
    else
    {
        bool const found_aliased_leds =
            get_aliased_led_states(context, led_handle, led_name, result_cb, result_context);
        struct led_ctx_st * const led_ctx =
            avl_find_element(&context->all_leds, led_name, led_ctx, node);

        if (led_ctx != NULL)
        {
            append_led_state(
                led_handle, methods, led_ctx, result_cb, result_context);
        }
        else if (!found_aliased_leds)
        {
            result_cb(
                led_name, false, NULL, NULL, NULL, _led_unknown_led, result_context);
        }
    }

    success = true;

done:
    return success;
}

static void
led_ops_list_leds(
    void * const led_ops_context,
    list_leds_cb const result_cb,
    void * const result_context)
{
    struct ledcmd_ctx_st * const context = led_ops_context;
    struct platform_led_methods_st const * const methods = context->methods;


    struct led_ctx_st * led_ctx;

    avl_for_each_element(&context->all_leds, led_ctx, node)
    {
        led_st const * const led = led_ctx->led;

        result_cb(
            methods->get_led_name(led),
            led_colour_by_type(methods->get_led_colour(led)),
            result_context);
    }
    list_all_aliased_leds(context, result_cb, result_context);
    result_cb(_led_all, "", result_context);
}

static bool
led_ops_deactivate_priority(
    led_ops_handle * const led_ops_handle,
    char const * const led_name,
    char const * const led_priority,
    char const * const lock_id,
    activate_priority_result_cb const result_cb,
    void * const result_context)
{
    bool success;

    if (led_ops_handle == NULL)
    {
        success = false;
        goto done;
    }

    enum led_priority_t priority;

    if (!led_priority_by_name(led_priority, &priority))
    {
        char const * error_msg = "Unknown priority";

        result_cb(led_name, false, NULL, error_msg, result_context);
        success = true;
        goto done;
    }

    if (priority == LED_PRIORITY_LOCKED && lock_id == NULL)
    {
        char const * error_msg = "Lock ID missing";

        result_cb(led_name, false, NULL, error_msg, result_context);
        success = true;
        goto done;
    }

    struct ledcmd_ctx_st * const context = led_ops_handle->ledcmd_context;
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = led_ops_handle->led_handle;

    if (led_handle == NULL)
    {
        success = false;
        goto done;
    }

    bool const do_all = strcasecmp(led_name, _led_all) == 0;

    if (do_all)
    {
        struct led_ctx_st * led_ctx;

        avl_for_each_element(&context->all_leds, led_ctx, node)
        {
            char const * const led_name = methods->get_led_name(led_ctx->led);
            char const * error_msg = NULL;
            bool const unlocked =
                priority != LED_PRIORITY_LOCKED
                || led_ctx_unlock_led(led_ctx, lock_id, &error_msg);

            if (unlocked)
            {
                led_ctx_deactivate_priority(methods, led_ctx, led_handle, priority);
            }

            result_cb(led_name, unlocked, led_ctx->lock_id, error_msg, result_context);
        }
    }
    else
    {
        bool const deactivated_aliased_leds =
            deactivate_aliased_leds(
            context,
            led_handle,
            led_name,
            lock_id,
            priority,
            result_cb,
            result_context);
        struct led_ctx_st * const led_ctx =
            avl_find_element(&context->all_leds, led_name, led_ctx, node);

        if (led_ctx == NULL)
        {
            if (!deactivated_aliased_leds)
            {
                result_cb(led_name, false, NULL, _led_unknown_led, result_context);
            }
            success = true;
            goto done;
        }
        char const * const led_name = methods->get_led_name(led_ctx->led);
        char const * error_msg = NULL;
        bool const unlocked =
            priority != LED_PRIORITY_LOCKED
            || led_ctx_unlock_led(led_ctx, lock_id, &error_msg);

        if (unlocked)
        {
            led_ctx_deactivate_priority(methods, led_ctx, led_handle, priority);
        }

        result_cb(led_name, unlocked, led_ctx->lock_id, error_msg, result_context);
    }

    success = true;

done:
    return success;
}

static bool
led_ops_activate_priority(
    led_ops_handle * const led_ops_handle,
    char const * const led_name,
    char const * const led_priority,
    char const * const lock_id,
    activate_priority_result_cb const result_cb,
    void * const result_context)
{
    bool success;

    if (led_ops_handle == NULL)
    {
        success = false;
        goto done;
    }

    enum led_priority_t priority;

    if (!led_priority_by_name(led_priority, &priority))
    {
        char const * error_msg = "Unknown priority";

        result_cb(led_name, false, NULL, error_msg, result_context);
        success = true;
        goto done;
    }

    if (priority == LED_PRIORITY_LOCKED && lock_id == NULL)
    {
        char const * error_msg = "Lock ID missing";

        result_cb(led_name, false, NULL, error_msg, result_context);
        success = true;
        goto done;
    }

    struct ledcmd_ctx_st * const context = led_ops_handle->ledcmd_context;
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = led_ops_handle->led_handle;

    if (led_handle == NULL)
    {
        success = false;
        goto done;
    }

    bool const do_all = strcasecmp(led_name, _led_all) == 0;

    if (do_all)
    {
        struct led_ctx_st * led_ctx;

        /*
         * To remain functionally equivalent to the previous implementation,
         * disallow locking all LEDS if any leds are already locked.
         */
        if (priority == LED_PRIORITY_LOCKED && ledcmd_ctx_any_led_locked(context))
        {
            char const * error_msg = "Some LEDs are already locked";

            /* TODO: Include an array of all locked LEDs and their IDs. */
            result_cb(_led_all, false, NULL, error_msg, result_context);
        }
        else
        {
            avl_for_each_element(&context->all_leds, led_ctx, node)
            {
                char const * const led_name = methods->get_led_name(led_ctx->led);
                char const * error_msg = NULL;

                bool const locked =
                    priority != LED_PRIORITY_LOCKED
                    || led_ctx_lock_led(led_ctx, lock_id, &error_msg);

                if (locked)
                {
                    led_ctx_activate_priority(
                        methods, led_ctx, led_handle, priority);
                }
                result_cb(led_name, locked, led_ctx->lock_id, error_msg, result_context);
            }
        }
    }
    else
    {
        bool const activated_aliased_leds =
            activate_aliased_leds(
            context,
            led_handle,
            led_name,
            lock_id,
            priority,
            result_cb,
            result_context);
        struct led_ctx_st * const led_ctx =
            avl_find_element(&context->all_leds, led_name, led_ctx, node);

        if (led_ctx == NULL)
        {
            if (!activated_aliased_leds)
            {
                result_cb(led_name, false, NULL, _led_unknown_led, result_context);
            }
            success = true;
            goto done;
        }

        char const * const led_name = methods->get_led_name(led_ctx->led);
        char const * error_msg = NULL;
        bool const locked =
            (priority != LED_PRIORITY_LOCKED)
            || led_ctx_lock_led(led_ctx, lock_id, &error_msg);

        if (locked)
        {
            led_ctx_activate_priority(methods, led_ctx, led_handle, priority);
        }

        result_cb(led_name, locked, led_ctx->lock_id, error_msg, result_context);
    }

    success = true;

done:
    return success;
}

static void
led_ops_list_playing_patterns(
    void * const led_ops_context, list_patterns_cb const cb, void * const user_ctx)
{
    struct ledcmd_ctx_st * const context = led_ops_context;

    led_pattern_list_playing_patterns(context->patterns_context, cb, user_ctx);
}

static void
led_ops_list_patterns(
    void * const led_ops_context, list_patterns_cb const result_cb, void * const user_ctx)
{
    struct ledcmd_ctx_st * const context = led_ops_context;

    led_pattern_list_patterns(context->patterns_context, result_cb, user_ctx);
}

static bool
led_ops_stop_pattern(
    void * const led_ops_context,
    char const * const pattern_name,
    play_pattern_cb const result_cb,
    void * const user_ctx)
{
    struct ledcmd_ctx_st * const context = led_ops_context;
    char const * error_msg = NULL;
    bool const success =
        led_pattern_stop_pattern(context->patterns_context, pattern_name, &error_msg);

    result_cb(success, error_msg, user_ctx);

    return true;
}

static bool
led_ops_play_pattern(
    void * const led_ops_context,
    char const * const pattern_name,
    bool const retrigger,
    play_pattern_cb const result_cb,
    void * const user_ctx)
{
    struct ledcmd_ctx_st * const context = led_ops_context;
    char const * error_msg = NULL;
    bool const success =
        led_pattern_play_pattern(
        context->patterns_context,
        pattern_name,
        retrigger,
        &error_msg);

    result_cb(success, error_msg, user_ctx);

    return true;
}

static bool
leds_init(led_st * const led, void * const user_ctx)
{
    bool success;
    struct ledcmd_ctx_st * const context = user_ctx;
    struct platform_led_methods_st const * const methods = context->methods;
    struct avl_tree * const tree = &context->all_leds;
    struct led_ctx_st * led_ctx = calloc(1, sizeof *led_ctx);

    if (led_ctx == NULL)
    {
        success = false;
        goto done;
    }

    led_ctx->priority_context = led_priority_allocate(LED_PRIORITY_COUNT);
    if (led_ctx->priority_context == NULL)
    {
        success = false;
        goto done;
    }

    led_ctx->led = led;
    for (size_t i = 0; i < ARRAY_SIZE(led_ctx->priorities); i++)
    {
        struct led_state_context_st * const led_priority_ctx =
            &led_ctx->priorities[i];

        led_priority_ctx->priority = i;
        led_priority_ctx->state = LED_OFF;
    }
    led_ctx->node.key = methods->get_led_name(led);

    avl_insert(tree, &led_ctx->node);

    success = true;

done:
    return success;
}

static int
led_name_cmp(void const * const k1, void const * const k2, void * const ptr)
{
    UNUSED_ARG(ptr);

    return strcasecmp(k1, k2);
}

static void
free_led_ctx_tree(struct avl_tree * const tree)
{
    struct led_ctx_st * led_ctx;
    struct led_ctx_st * tmp;

    avl_remove_all_elements(tree, led_ctx, node, tmp)
    {
        free_led_ctx(led_ctx);
    }
}

static void
free_led_ctxs(struct ledcmd_ctx_st * const context)
{
    free_led_ctx_tree(&context->all_leds);
}

static bool
populate_led_ctxs(struct ledcmd_ctx_st * const context)
{
    struct platform_led_methods_st const * const methods = context->methods;
    platform_leds_st * const platform_leds = context->platform_leds;

    bool const success =
        methods->iterate_leds(platform_leds, leds_init, context) == NULL;

    return success;
}

static void led_ctxs_init(struct ledcmd_ctx_st * const context)
{
    struct avl_tree * const tree = &context->all_leds;
    bool const duplicates_allowed = false;

    avl_init(tree, led_name_cmp, duplicates_allowed, NULL);
}

static void
get_all_led_states(struct ledcmd_ctx_st * const context)
{
    struct platform_led_methods_st const * const methods = context->methods;
    led_handle_st * const led_handle = methods->open();

    if (led_handle != NULL)
    {
        struct led_ctx_st * led_ctx;

        avl_for_each_element(&context->all_leds, led_ctx, node)
        {
            enum led_priority_t const current_priority =
                led_priority_highest_priority(led_ctx->priority_context);

            led_ctx->priorities[current_priority].state =
                methods->get_led_state(led_handle, led_ctx->led);
        }

        methods->close(led_handle);
    }
}

static void
supported_states_cb(
    enum led_state_t const state, void * const user_ctx)
{
    struct ledcmd_ctx_st * context = user_ctx;

    context->supported_states[state] = true;
}

static void
get_all_supported_states(struct ledcmd_ctx_st * context)
{
    struct platform_led_methods_st const * const methods = context->methods;

    methods->iterate_supported_states(supported_states_cb, context);
}

void
ledcmd_deinit(struct ledcmd_ctx_st * const context)
{
    if (context == NULL)
    {
        goto done;
    }

    ledcmd_ubus_deinit(context->ubus_context);
    free_led_ctxs(context);

    struct platform_led_methods_st const * const methods = context->methods;

    if (methods != NULL)
    {
        methods->deinit(context->platform_leds);
    }
    led_patterns_deinit(context->patterns_context);
    platform_leds_plugin_unload(context->platform_leds_handle);

    free(context);

done:
    return;
}

struct ledcmd_ctx_st *
ledcmd_init(
    char const * const ubus_path,
    char const * const patterns_directory,
    char const * const aliases_directory,
    char const * const backend_path)
{
    bool success;
    struct ledcmd_ctx_st * context = calloc(1, sizeof *context);

    if (context == NULL)
    {
        success = false;
        goto done;
    }

    static struct led_ops_st const ops =
    {
        .open = led_ops_open,
        .close = led_ops_close,
        .set_state = led_ops_set_state,
        .get_state = led_ops_get_state,
        .activate_priority = led_ops_activate_priority,
        .deactivate_priority = led_ops_deactivate_priority,
        .list_leds = led_ops_list_leds,
        .list_patterns = led_ops_list_patterns,
        .list_playing_patterns = led_ops_list_playing_patterns,
        .play_pattern = led_ops_play_pattern,
        .stop_pattern = led_ops_stop_pattern,
        .compare_leds = led_ops_compare_leds
    };

    led_ctxs_init(context);

    context->patterns_context = led_patterns_init(patterns_directory, &ops, context);

    context->led_aliases = led_aliases_load(aliases_directory);

    struct platform_led_methods_st const * methods;

    context->platform_leds_handle = platform_leds_plugin_load(backend_path, &methods);

    if (methods == NULL)
    {
        log_error("Failed to load backend LEDs methods\n");
        success = false;
        goto done;
    }

    context->methods = methods;
    context->platform_leds = methods->init();

    if (!populate_led_ctxs(context))
    {
        success = false;
        goto done;
    }
    get_all_supported_states(context);
    get_all_led_states(context);
    context->ubus_context = ledcmd_ubus_init(ubus_path, &ops, context);

    success = true;

done:
    if (!success)
    {
        ledcmd_deinit(context);
        context = NULL;
    }

    return context;
}

