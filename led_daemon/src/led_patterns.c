#include "led_patterns.h"
#include "iterate_files.h"

#include <lib_log/log.h>
#include <lib_led/string_constants.h>
#include <ubus_utils/ubus_utils.h>

#include <json-c/json.h>
#include <libubox/blobmsg_json.h>
#include <linux/limits.h>

struct led_patterns_st
{
    struct avl_tree all_patterns;
};

typedef void (*new_pattern_fn)
    (struct led_pattern_st * const led_pattern, void * const user_ctx);

static void
free_const(void const * const mem)
{
    free(UNCONST(mem));
}

static void
free_led_state(struct led_state_st const * const led_state)
{
    free_const(led_state->led_name);
    free_const(led_state->priority);
}

static void
free_pattern_step(struct pattern_step_st const * const step)
{
    if (step->leds != NULL)
    {
        for (size_t i = 0; i < step->num_leds; i++)
        {
            struct led_state_st const * const led_state = &step->leds[i];

            free_led_state(led_state);
        }

        free(step->leds);
    }
}

static void
free_led_pattern(struct led_pattern_st const * const pattern)
{
    if (pattern == NULL)
    {
        goto done;
    }

    if (pattern->steps != NULL)
    {
        for (size_t i = 0; i < pattern->num_steps; i++)
        {
            struct pattern_step_st const * const step = &pattern->steps[i];

            free_pattern_step(step);
        }

        free(pattern->steps);
    }

    free_pattern_step(&pattern->start_step);
    free_pattern_step(&pattern->end_step);
    free_const(pattern);

done:
    return;
}

static void free_led_daemon_led_pattern(
    struct led_daemon_led_pattern_st * const led_daemon_led_pattern)
{
    if (led_daemon_led_pattern == NULL)
    {
        goto done;
    }

    free_led_pattern(led_daemon_led_pattern->led_pattern);
    free(led_daemon_led_pattern);

done:
    return;
}

static void
led_patterns_free(struct led_patterns_st * const led_patterns)
{
    if (led_patterns == NULL)
    {
        goto done;
    }

    struct avl_tree * const tree = &led_patterns->all_patterns;
    struct led_daemon_led_pattern_st * led_daemon_led_pattern;
    struct led_daemon_led_pattern_st * tmp;

    avl_remove_all_elements(tree, led_daemon_led_pattern, node, tmp)
    {
        free_led_daemon_led_pattern(led_daemon_led_pattern);
    }

    free(led_patterns);

done:
    return;
}

static bool
parse_step_led(
    struct led_state_st * const state, struct blob_attr const * const attr)
{
    bool success;
    enum
    {
        LED_NAME,
        LED_STATE,
        LED_PRIORITY,
        LED_MAX__
    };
    struct blobmsg_policy const led_policy[LED_MAX__] =
    {
        [LED_NAME] =
        { .name = _led_name, .type = BLOBMSG_TYPE_STRING },
        [LED_STATE] =
        { .name = _led_state, .type = BLOBMSG_TYPE_STRING },
        [LED_PRIORITY] =
        { .name = _led_priority, .type = BLOBMSG_TYPE_STRING }
    };
    struct blob_attr * fields[LED_MAX__];

    blobmsg_parse(led_policy, ARRAY_SIZE(led_policy), fields,
                  blobmsg_data(attr), blobmsg_data_len(attr));

    state->led_state =
        led_state_by_query_name(blobmsg_get_string(fields[LED_STATE]));
    state->led_name =
        strdup(blobmsg_get_string_or_default(fields[LED_NAME], ""));
    state->priority =
        (fields[LED_PRIORITY] != NULL)
        ? strdup(blobmsg_get_string(fields[LED_PRIORITY]))
        : NULL;

    success = true;

    return success;
}

static struct led_state_st *
append_state(struct pattern_step_st * const step)
{
    struct led_state_st * new_state;
    size_t const new_count = step->num_leds + 1;
    struct led_state_st * const new_states =
        realloc(step->leds, new_count * sizeof *new_states);

    if (new_states == NULL)
    {
        new_state = NULL;
        goto done;
    }

    step->num_leds = new_count;
    step->leds = new_states;

    new_state = &step->leds[step->num_leds - 1];

    memset(new_state, 0, sizeof *new_state);

done:
    return new_state;
}

static bool
parse_step_leds(
    struct pattern_step_st * const step, struct blob_attr const * const attr)
{
    bool success;

    if (attr == NULL)
    {
        success = false;
        goto done;
    }

    if (!blobmsg_array_is_type(attr, BLOBMSG_TYPE_TABLE))
    {
        success = false;
        goto done;
    }


    struct blob_attr * cur;
    int rem;

    blobmsg_for_each_attr(cur, attr, rem)
    {
        struct led_state_st * const new_state = append_state(step);

        if (new_state == NULL)
        {
            success = false;
            goto done;
        }

        if (!parse_step_led(new_state, cur))
        {
            success = false;
            goto done;
        }
    }

    success = true;

done:
    return success;
}

static bool
parse_pattern_step(
    struct pattern_step_st * const new_step, struct blob_attr const * const attr)
{
    bool success;
    enum
    {
        STEP_TIME_MS,
        STEP_LEDS,
        STEP_MAX__
    };
    struct blobmsg_policy const step_policy[STEP_MAX__] =
    {
        [STEP_TIME_MS] =
        { .name = _led_pattern_step_time_ms, .type = BLOBMSG_TYPE_INT32 },
        [STEP_LEDS] =
        { .name = _led_pattern_step_leds, .type = BLOBMSG_TYPE_ARRAY }
    };
    struct blob_attr * fields[STEP_MAX__];

    blobmsg_parse(step_policy, ARRAY_SIZE(step_policy), fields,
                  blobmsg_data(attr), blobmsg_data_len(attr));

    new_step->time_ms = blobmsg_get_u32_or_default(fields[STEP_TIME_MS], 0);
    if (!parse_step_leds(new_step, fields[STEP_LEDS]))
    {
        success = false;
        goto done;
    }

    success = true;

done:
    return success;
}

static struct pattern_step_st *
append_step(struct led_pattern_st * const pattern)
{
    struct pattern_step_st * new_step;
    size_t const new_count = pattern->num_steps + 1;
    struct pattern_step_st * const new_steps =
        realloc(pattern->steps, new_count * sizeof * new_steps);

    if (new_steps == NULL)
    {
        new_step = NULL;
        goto done;
    }

    pattern->num_steps = new_count;
    pattern->steps = new_steps;

    new_step = &pattern->steps[pattern->num_steps - 1];

    memset(new_step, 0, sizeof *new_step);

done:
    return new_step;
}

static bool
parse_pattern_steps(
    struct led_pattern_st * const pattern, struct blob_attr const * const attr)
{
    bool success;

    if (attr == NULL)
    {
        success = false;
        goto done;
    }

    if (!blobmsg_array_is_type(attr, BLOBMSG_TYPE_TABLE))
    {
        success = false;
        goto done;
    }

    struct blob_attr * cur;
    int rem;

    blobmsg_for_each_attr(cur, attr, rem)
    {
        struct pattern_step_st * const new_step = append_step(pattern);

        if (new_step == NULL)
        {
            success = false;
            goto done;
        }

        if (!parse_pattern_step(new_step, cur))
        {
            success = false;
            goto done;
        }
    }

    success = true;

done:
    return success;
}

static struct led_pattern_st *
parse_led_pattern(struct blob_attr const * const attr)
{
    bool success;
    struct led_pattern_st * pattern = NULL;
    enum
    {
        PATTERN_NAME,
        PATTERN_REPEAT,
        PATTERN_PLAY_COUNT,
        PATTERN_LED_PATTERN,
        PATTERN_LED_END_STATE,
        PATTERN_LED_START_STATE,
        PATTERN_MAX__
    };
    struct blobmsg_policy const pattern_policy[PATTERN_MAX__] =
    {
        [PATTERN_NAME] =
        { .name = _led_pattern_name, .type = BLOBMSG_TYPE_STRING },
        [PATTERN_REPEAT] =
        { .name = _led_pattern_repeat, .type = BLOBMSG_TYPE_BOOL },
        [PATTERN_PLAY_COUNT] =
        { .name = _led_pattern_play_count, .type = BLOBMSG_TYPE_INT32 },
        [PATTERN_LED_PATTERN] =
        { .name = _led_pattern_pattern, .type = BLOBMSG_TYPE_ARRAY },
        [PATTERN_LED_END_STATE] =
        { .name = _led_pattern_end_state, .type = BLOBMSG_TYPE_TABLE },
        [PATTERN_LED_START_STATE] =
        { .name = _led_pattern_start_state, .type = BLOBMSG_TYPE_TABLE }
    };
    struct blob_attr * fields[PATTERN_MAX__];

    blobmsg_parse(pattern_policy, ARRAY_SIZE(pattern_policy), fields,
                  blobmsg_data(attr), blobmsg_data_len(attr));

    if (fields[PATTERN_NAME] == NULL)
    {
        success = false;
        goto done;
    }

    pattern = calloc(1, sizeof *pattern);
    if (pattern == NULL)
    {
        success = false;
        goto done;
    }

    pattern->name = strdup(blobmsg_get_string(fields[PATTERN_NAME]));
    if (pattern->name == NULL)
    {
        success = false;
        goto done;
    }

    log_info("Load pattern: %s", pattern->name);

    pattern->repeat =
        blobmsg_get_bool_or_default(fields[PATTERN_REPEAT], false);
    pattern->play_count =
        blobmsg_get_u32_or_default(
        fields[PATTERN_PLAY_COUNT], pattern->repeat ? 0 : 1);

    if (fields[PATTERN_LED_END_STATE] != NULL)
    {
        if (!parse_pattern_step(
                &pattern->end_step, fields[PATTERN_LED_END_STATE]))
        {
            success = false;
            goto done;
        }
    }

    if (fields[PATTERN_LED_START_STATE] != NULL)
    {
        if (!parse_pattern_step(
                &pattern->start_step, fields[PATTERN_LED_START_STATE]))
        {
            success = false;
            goto done;
        }
    }

    if (!parse_pattern_steps(pattern, fields[PATTERN_LED_PATTERN]))
    {
        success = false;
        goto done;
    }

    success = true;

done:
    if (!success)
    {
        log_error("Failed to load pattern");
        free_led_pattern(pattern);
        pattern = NULL;
    }

    return pattern;
}

static int
pattern_name_cmp(void const * const k1, void const * const k2, void * const ptr)
{
    UNUSED_ARG(ptr);

    return strcasecmp(k1, k2);
}

static void
parse_patterns_array(
    struct blob_attr const * const attr,
    new_pattern_fn const new_pattern_cb,
    void * const user_ctx)
{
    if (!blobmsg_array_is_type(attr, BLOBMSG_TYPE_TABLE))
    {
        goto done;
    }

    struct blob_attr * cur;
    int rem;

    blobmsg_for_each_attr(cur, attr, rem)
    {
        struct led_pattern_st * const pattern = parse_led_pattern(cur);

        if (pattern != NULL)
        {
            new_pattern_cb(pattern, user_ctx);
        }
    }

done:
    return;
}

static void
parse_led_patterns(
    struct blob_attr const * const attr,
    new_pattern_fn const new_pattern_cb,
    void * const user_ctx)
{
    enum
    {
        PATTERNS,
        PATTERNS_MAX__
    };
    struct blobmsg_policy const patterns_policy[PATTERNS_MAX__] =
    {
        [PATTERNS] =
        { .name = "patterns", .type = BLOBMSG_TYPE_ARRAY }
    };
    struct blob_attr * fields[PATTERNS_MAX__];

    blobmsg_parse(patterns_policy, ARRAY_SIZE(patterns_policy), fields,
                  blobmsg_data(attr), blobmsg_data_len(attr));

    parse_patterns_array(fields[PATTERNS], new_pattern_cb, user_ctx);
}

static void
parse_led_patterns_json(
    json_object * const json_obj,
    new_pattern_fn const new_pattern_cb,
    void * const user_ctx)
{
    struct blob_buf blob;

    blob_buf_full_init(&blob, 0);

    if (!blobmsg_add_json_element(&blob, "", json_obj))
    {
        goto done;
    }

    parse_led_patterns(blob_data(blob.head), new_pattern_cb, user_ctx);

done:
    blob_buf_free(&blob);

    return;
}

static void
load_patterns_from_file(
    char const * const filename,
    new_pattern_fn const new_pattern_cb,
    void * const user_ctx)
{
    log_info("Load patterns from file: %s", filename);

    json_object * const json_obj = json_object_from_file(filename);

    if (json_obj == NULL)
    {
        log_error("Failed to load JSON file: %s", filename);
        goto done;
    }

    parse_led_patterns_json(json_obj, new_pattern_cb, user_ctx);

done:
    json_object_put(json_obj);

    return;
}

static void
new_pattern_cb(
    struct led_pattern_st * const led_pattern, void * const user_ctx)
{
    bool success;
    struct avl_tree * const tree = user_ctx;
    struct led_daemon_led_pattern_st * const led_daemon_led_pattern =
        calloc(1,  sizeof *led_daemon_led_pattern);

    if (led_daemon_led_pattern == NULL)
    {
        success = false;
        goto done;
    }

    led_daemon_led_pattern->node.key = led_pattern->name;

    if (avl_insert(tree, &led_daemon_led_pattern->node) != 0)
    {
        success = false;
        goto done;
    }

    led_daemon_led_pattern->led_pattern = led_pattern;
    success = true;

done:
    if (!success)
    {
        log_error("Failed to insert pattern: %s", led_pattern->name);
        free_led_pattern(led_pattern);
        free_led_daemon_led_pattern(led_daemon_led_pattern);
    }
}

static void
load_pattern_cb(char const * const filename, void * const user_ctx)
{
    load_patterns_from_file(filename, new_pattern_cb, user_ctx);
}

static void
load_patterns_from_directory(
    char const * const patterns_directory, struct avl_tree * const tree)
{
    char path[PATH_MAX];

    snprintf(path, sizeof path, "%s/*.json", patterns_directory);
    iterate_files(path, load_pattern_cb, tree);
}

struct led_pattern_st *
led_pattern_lookup(
    led_patterns_st const * const led_patterns, char const * const pattern_name)
{
    struct led_daemon_led_pattern_st * led_daemon_led_pattern;
    struct led_pattern_st * led_pattern;

    if (led_patterns == NULL)
    {
        led_pattern = NULL;
        goto done;
    }

    led_daemon_led_pattern = avl_find_element(
        &led_patterns->all_patterns, pattern_name, led_daemon_led_pattern, node);
    if (led_daemon_led_pattern == NULL)
    {
        led_pattern = NULL;
        goto done;
    }

    led_pattern = led_daemon_led_pattern->led_pattern;

done:
    return led_pattern;
}

void
led_pattern_list(
    struct led_patterns_st const * const led_patterns,
    list_patterns_cb const cb,
    void * const user_ctx)
{
    struct led_daemon_led_pattern_st * led_daemon_led_pattern;

    avl_for_each_element(&led_patterns->all_patterns, led_daemon_led_pattern, node)
    {
        cb(led_daemon_led_pattern->led_pattern, user_ctx);
    }
}

void
free_patterns(struct led_patterns_st const * const led_patterns)
{
    led_patterns_free(UNCONST(led_patterns));
}

struct led_patterns_st const *
load_patterns(char const * const patterns_directory)
{
    struct led_patterns_st * led_patterns;

    if (patterns_directory == NULL)
    {
        led_patterns = NULL;
        goto done;
    }

    led_patterns = calloc(1, sizeof *led_patterns);

    if (led_patterns == NULL)
    {
        goto done;
    }

    struct avl_tree * const tree = &led_patterns->all_patterns;
    bool const duplicates_allowed = false;

    avl_init(tree, pattern_name_cmp, duplicates_allowed, NULL);

    load_patterns_from_directory(patterns_directory, tree);

done:
    return led_patterns;
}

