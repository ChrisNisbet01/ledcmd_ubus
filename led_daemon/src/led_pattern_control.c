#include "led_pattern_control.h"
#include "led_patterns.h"
#include "led_priorities.h"

#include <ubus_utils/ubus_utils.h>
#include <lib_led/string_constants.h>
#include <lib_log/log.h>

#include <libubox/avl.h>

#include <string.h>
#include <sys/queue.h>

struct led_patterns_context_st;

struct led_pattern_context_st
{
    TAILQ_ENTRY(led_pattern_context_st) entry;
    size_t times_played;
    size_t next_step_number;
    struct uloop_timeout timer;
    struct led_pattern_st const * led_pattern;
    struct led_patterns_context_st * patterns_context;
};

TAILQ_HEAD(playing_pattern_st, led_pattern_context_st);

struct led_patterns_context_st
{

    struct led_ops_st const * led_ops;
    void * led_ops_context;

    led_patterns_st const * led_patterns;

    struct playing_pattern_st playing_patterns;
};

static void pattern_timeout(struct uloop_timeout * t);

static void
led_pattern_set_state_setup(
    struct led_state_st const * const led_step,
    char const * const priority,
    struct set_state_req_st * const req)
{
    memset(req, 0, sizeof *req);
    req->flash_forever = false;
    req->flash_time_ms = 0;
    req->flash_type = LED_FLASH_TYPE_NONE;
    req->led_priority = priority;
    req->led_name = led_step->led_name;
    req->lock_id = NULL;
    req->state = led_step->led_state;
}

static void
led_activate_cb(
    char const * led_name,
    bool success,
    char const * lock_id,
    char const * error_msg,
    void * result_context)
{
    UNUSED_ARG(led_name);
    UNUSED_ARG(success);
    UNUSED_ARG(lock_id);
    UNUSED_ARG(error_msg);
    UNUSED_ARG(result_context);
}

static void
set_led_states_from_step(
    struct led_pattern_context_st * const pattern_context,
    struct pattern_step_st const * const step,
    bool const starting,
    bool const stopping)
{
    struct led_patterns_context_st * const patterns_context =
        pattern_context->patterns_context;

    struct led_ops_st const * const led_ops = patterns_context->led_ops;
    struct led_ops_handle_st * const led_ops_handle =
        led_ops->open(patterns_context->led_ops_context);

    if (led_ops_handle == NULL)
    {
        goto done;
    }

    for (size_t led_index = 0; led_index < step->num_leds; led_index++)
    {
        struct led_state_st * const led_step = &step->leds[led_index];
        struct set_state_req_st set_state_req;

        if (starting && led_step->priority != NULL)
        {
            led_ops->activate_priority(
                led_ops_handle,
                led_step->led_name,
                led_step->priority,
                NULL,
                led_activate_cb,
                NULL);
        }

        if (led_step->led_state != LED_STATE_UNKNOWN)
        {
            led_pattern_set_state_setup(
                led_step, led_step->priority, &set_state_req);
            led_ops->set_state(led_ops_handle, &set_state_req, NULL, NULL);
        }

        if (stopping && led_step->priority != NULL)
        {
            led_ops->deactivate_priority(
                led_ops_handle,
                led_step->led_name,
                led_step->priority,
                NULL,
                led_activate_cb,
                NULL);
        }
    }

    led_ops->close(led_ops_handle);

done:
    return;
}

static void
led_pattern_stop(struct led_pattern_context_st * const pattern_context)
{
    log_info("Stop pattern: %s", pattern_context->led_pattern->name);

    struct pattern_step_st const * const end_step =
        &pattern_context->led_pattern->end_step;

    if (end_step->num_leds > 0)
    {
        bool const starting = false;
        bool const stopping = true;

        set_led_states_from_step(pattern_context, end_step, starting, stopping);
    }
    pattern_context->led_pattern = NULL;
    uloop_timeout_cancel(&pattern_context->timer);

    struct led_patterns_context_st * const patterns_context =
        pattern_context->patterns_context;

    TAILQ_REMOVE(&patterns_context->playing_patterns, pattern_context, entry);

    free(pattern_context);
}

static void
led_pattern_play_step(struct led_pattern_context_st * const pattern_context)
{
    struct led_pattern_st const * const led_pattern =
        pattern_context->led_pattern;
    struct pattern_step_st const * const pattern_step =
        &led_pattern->steps[pattern_context->next_step_number];

    pattern_context->next_step_number++;

    bool const starting = false;
    bool const stopping = false;

    set_led_states_from_step(pattern_context, pattern_step, starting, stopping);

    if (pattern_step->time_ms > 0)
    {
        uloop_timeout_set(&pattern_context->timer, pattern_step->time_ms);
    }
    else
    {
        led_pattern_stop(pattern_context);
    }
}

static bool
led_pattern_play_start_step(struct led_pattern_context_st * const pattern_context)
{
    bool start_step_completed;
    struct led_pattern_st const * const led_pattern =
        pattern_context->led_pattern;
    struct pattern_step_st const * const pattern_step =
        &led_pattern->start_step;
    bool const starting = true;
    bool const stopping = false;

    set_led_states_from_step(pattern_context, pattern_step, starting, stopping);

    if (pattern_step->time_ms > 0)
    {
        uloop_timeout_set(&pattern_context->timer, pattern_step->time_ms);
        start_step_completed = false;
    }
    else
    {
        start_step_completed = true;
    }

    return start_step_completed;
}

static void
pattern_context_initialise(
    struct led_patterns_context_st * const patterns_context,
    struct led_pattern_context_st * const pattern_context,
    struct led_pattern_st const * const led_pattern)
{
    pattern_context->patterns_context = patterns_context;
    pattern_context->led_pattern = led_pattern;
    pattern_context->timer.cb = pattern_timeout;
    pattern_context->next_step_number = 0;
    /*
     * Start the play count at 1. If the pattern gets retriggered the play
     * count gets set to 0, which means that when the play count is incremented
     * at the end of the pattern, it will still be <= the play count and will
     * play again. The desired behaviour is that if a retrigger occurs before
     * the end of the pattern it will play the pattern one more time.
     */
    pattern_context->times_played = 1;
    TAILQ_INSERT_TAIL(&patterns_context->playing_patterns, pattern_context, entry);
}

static void
led_pattern_start(struct led_pattern_context_st * const pattern_context)
{
    struct led_pattern_st const * const led_pattern =
        pattern_context->led_pattern;

    log_info("Start pattern: %s", led_pattern->name);

    bool const start_step_required = led_pattern->start_step.num_leds > 0;
    bool const start_step_completed =
        !start_step_required || led_pattern_play_start_step(pattern_context);

    if (start_step_completed)
    {
        led_pattern_play_step(pattern_context);
    }
}

static void
pattern_timeout(struct uloop_timeout * t)
{
    struct led_pattern_context_st * const pattern_context =
        container_of(t, struct led_pattern_context_st, timer);
    struct led_pattern_st const * const led_pattern =
        pattern_context->led_pattern;
    bool const all_steps_completed =
        pattern_context->next_step_number == led_pattern->num_steps;

    if (all_steps_completed)
    {
        pattern_context->next_step_number = 0;
        pattern_context->times_played++;
    }

    if (led_pattern->repeat
        || led_pattern->play_count == 0
        || pattern_context->times_played <= led_pattern->play_count)
    {
        led_pattern_play_step(pattern_context);
    }
    else
    {
        led_pattern_stop(pattern_context);
    }
}

static struct led_pattern_context_st *
lookup_playing_pattern(
    struct led_patterns_context_st * const patterns_context,
    char const * const pattern_name)
{
    struct led_pattern_context_st * pattern_context;

    TAILQ_FOREACH(pattern_context, &patterns_context->playing_patterns, entry)
    {
        bool const found_pattern =
            strcasecmp(pattern_context->led_pattern->name, pattern_name) == 0;

        if (found_pattern)
        {
            goto done;
        }
    }

    pattern_context = NULL;

done:
    return pattern_context;
}

static bool
led_used_by_step(
    struct led_patterns_context_st * const patterns_context,
    struct led_state_st const * const led_state,
    struct pattern_step_st const * const step)
{
    bool used_by_step;

    for (size_t i = 0; i < step->num_leds; i++)
    {
        struct led_state_st const * const a = &step->leds[i];
        struct led_ops_st const * const led_ops = patterns_context->led_ops;
        bool const matches =
            led_ops->compare_leds(
                patterns_context->led_ops_context,
                led_state->led_name,
                led_state->priority,
                a->led_name,
                a->priority);

        if (matches)
        {
            used_by_step = true;
            goto done;
        }
    }

    used_by_step = false;

done:
    return used_by_step;
}

static bool
led_used_by_pattern(
    struct led_patterns_context_st * const patterns_context,
    struct led_state_st const * const led_state,
    struct led_pattern_st const * const pattern)
{
    bool used_by_pattern;

    if (led_used_by_step(patterns_context, led_state, &pattern->start_step))
    {
        used_by_pattern = true;
        goto done;
    }

    if (led_used_by_step(patterns_context, led_state, &pattern->end_step))
    {
        used_by_pattern = true;
        goto done;
    }

    for (size_t i = 0; i < pattern->num_steps; i++)
    {
        if (led_used_by_step(patterns_context, led_state, &pattern->steps[i]))
        {
            used_by_pattern = true;
            goto done;
        }
    }

    used_by_pattern = false;

done:
    return used_by_pattern;
}

static bool
step_shares_leds_with_pattern(
    struct led_patterns_context_st * const patterns_context,
    struct pattern_step_st const * const step,
    struct led_pattern_st const * const pattern)
{
    bool shares_led;

    for (size_t i = 0; i < step->num_leds; i++)
    {
        if (led_used_by_pattern(patterns_context, &step->leds[i], pattern))
        {
            shares_led = true;
            goto done;
        }
    }

    shares_led = false;

done:
    return  shares_led;
}

static bool
patterns_share_leds(
    struct led_patterns_context_st * const patterns_context,
    struct led_pattern_st const * const a,
    struct led_pattern_st const * const b)
{
    bool shares_leds;

    if (step_shares_leds_with_pattern(patterns_context, &a->start_step, b))
    {
        shares_leds = true;
        goto done;
    }

    if (step_shares_leds_with_pattern(patterns_context, &a->end_step, b))
    {
        shares_leds = true;
        goto done;
    }

    for (size_t i = 0; i < a->num_steps; i++)
    {
        if (step_shares_leds_with_pattern(patterns_context, &a->steps[i], b))
        {
            shares_leds = true;
            goto done;
        }
    }

    shares_leds = false;

done:
    return shares_leds;
}

static void
stop_other_patterns_using_this_patterns_leds(
    struct led_patterns_context_st * const patterns_context,
    struct led_pattern_st const * const led_pattern)
{
    /*
     * For each playing pattern, if it shares any of the same LEDs as the
     * supplied pattern, with the same priority, stop the pattern.
     */
    struct led_pattern_context_st * pattern_context;
    struct led_pattern_context_st * tmp;

    TAILQ_FOREACH_SAFE(
        pattern_context, &patterns_context->playing_patterns, entry, tmp)
    {
        /*
         * It is assumed that led_pattern isn't already playing, so there's no need
         * to check that this isn't the same pattern as the one being checked.
         */
        if (patterns_share_leds(
                patterns_context, pattern_context->led_pattern, led_pattern))
        {
            led_pattern_stop(pattern_context);
        }
    }
}

bool
led_pattern_stop_pattern(
    struct led_patterns_context_st * const patterns_context,
    char const * const pattern_name,
    char const ** const error_msg)
{
    bool success;

    if (patterns_context == NULL)
    {
        *error_msg = "No patterns are loaded";
        success = false;
        goto done;
    }

    if (pattern_name == NULL)
    {
        *error_msg = "No pattern name specified";
        success = false;
        goto done;
    }

    struct led_pattern_st const * const led_pattern =
        led_pattern_lookup(patterns_context->led_patterns, pattern_name);

    if (led_pattern == NULL)
    {
        *error_msg = "Pattern not found";
        success = false;
        goto done;
    }

    /*
     * Check playing patterns.
     */
    struct led_pattern_context_st * pattern_context =
        lookup_playing_pattern(patterns_context, pattern_name);

    if (pattern_context == NULL)
    {
        *error_msg = "Pattern not playing";
        success = false;
        goto done;
    }

    led_pattern_stop(pattern_context);

    success = true;

done:
    return success;
}

bool
led_pattern_play_pattern(
    struct led_patterns_context_st * const patterns_context,
    char const * const pattern_name,
    bool const retrigger,
    char const ** const error_msg)
{
    bool success;

    if (patterns_context == NULL)
    {
        *error_msg = "No patterns are loaded";
        success = false;
        goto done;
    }

    if (pattern_name == NULL)
    {
        *error_msg = "No pattern name specified";
        success = false;
        goto done;
    }

    struct led_pattern_st const * const led_pattern =
        led_pattern_lookup(patterns_context->led_patterns, pattern_name);

    if (led_pattern == NULL)
    {
        *error_msg = "Pattern not found";
        success = false;
        goto done;
    }

    /*
     * Check playing patterns.
     */
    struct led_pattern_context_st * pattern_context =
        lookup_playing_pattern(patterns_context, pattern_name);

    if (pattern_context != NULL)
    {
        if (retrigger)
        {
            pattern_context->times_played = 0;
            success = true;
        }
        else
        {
            *error_msg = "Pattern already playing";
            success = false;
        }
        goto done;
    }

    pattern_context = calloc(1, sizeof *pattern_context);
    if (pattern_context == NULL)
    {
        *error_msg = "Out of memory";
        success = false;
        goto done;
    }

    /*
     * It isn't desirable for multiple patterns to control the same LEDS,
     * so stop any other patterns that use the same LEDs that this pattern uses.
     * The priority must also match.
     */

    stop_other_patterns_using_this_patterns_leds(patterns_context, led_pattern);
    pattern_context_initialise(patterns_context, pattern_context, led_pattern);
    led_pattern_start(pattern_context);

    success = true;

done:
    return success;
}

void
led_pattern_list_playing_patterns(
    struct led_patterns_context_st * const patterns_context,
    list_patterns_cb const cb,
    void * const user_ctx)
{
    struct led_pattern_context_st * pattern_context;

    TAILQ_FOREACH(pattern_context, &patterns_context->playing_patterns, entry)
    {
        cb(pattern_context->led_pattern, user_ctx);
    }
}

void
led_pattern_list_patterns(
    struct led_patterns_context_st * const patterns_context,
    list_patterns_cb const cb,
    void * const user_ctx)
{
    led_pattern_list(patterns_context->led_patterns, cb, user_ctx);
}

void
led_patterns_deinit(struct led_patterns_context_st * const patterns_context)
{
    if (patterns_context == NULL)
    {
        goto done;
    }

    while (TAILQ_FIRST(&patterns_context->playing_patterns) != NULL)
    {
        struct led_pattern_context_st * const pattern_context =
            TAILQ_FIRST(&patterns_context->playing_patterns);

        led_pattern_stop(pattern_context);
    }

    free_patterns(patterns_context->led_patterns);

done:
    return;
}

struct led_patterns_context_st *
led_patterns_init(
    char const * const patterns_directory,
    struct led_ops_st const * const led_ops,
    void * const led_ops_context)
{
    struct led_patterns_context_st * const patterns_context =
        calloc(1, sizeof * patterns_context);

    if (patterns_context == NULL)
    {
        goto done;
    }

    TAILQ_INIT(&patterns_context->playing_patterns);

    patterns_context->led_ops = led_ops;
    patterns_context->led_ops_context = led_ops_context;
    patterns_context->led_patterns = load_patterns(patterns_directory);

done:
    return patterns_context;
}

