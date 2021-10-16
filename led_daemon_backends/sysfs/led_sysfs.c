#include <led_daemon/platform_specific.h>

#include <ubus_utils/ubus_utils.h>

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define error(fmt, ...) do {} while(0)

#define LEDS_DIR	"/sys/class/leds"

#define DELAY_SLOW	500
#define DELAY_FAST	250

#define LOCK_DIR	"/var/lock/leds"
#define SYS_LEDS_PREFIX "/sys/class/leds/"

enum
{
    CMD_ON = 0,     /* turn LED on permanently */
    CMD_OFF,        /* turn LED off permanently */
    CMD_FLASH,      /* flash this LED */
    CMD_FLASH_FAST  /* flash this LED quickly */
};

struct platform_leds_st
{
    size_t count;
    char const ** leds;
};

static DIR *
foreach_led_names_init()
{
    DIR * dir;

    dir = opendir(LEDS_DIR);
    if (dir == NULL)
    {
        error("failed to open LEDs directory");
    }

    return dir;
}

static char const *
foreach_led_names(DIR * const dir)
{
    struct dirent * ent;
    char const * name;

    if (dir == NULL)
    {
        name = NULL;
        goto done;
    }

    /* Go through on all symbolic links */
    do
    {
        ent = readdir(dir);
    }
    while (ent != NULL && ent->d_type != DT_LNK);

    if (ent == NULL)
    {
        name = NULL;
        goto done;
    }

    name = ent->d_name;

done:
    return name;
}

static void
foreach_led_names_finish(DIR * const dir)
{
    if (dir != NULL)
    {
        closedir(dir);
    }
}

static bool
append_led_name(
    struct platform_leds_st * const platform_leds,
    char const * const led_name)
{
    bool success;
    size_t const new_led_count = platform_leds->count + 1;
    size_t const required_mem =
    new_led_count * sizeof *platform_leds->leds;
    char const **new_leds = realloc(platform_leds->leds, required_mem);

    if (new_leds == NULL)
    {
        success = false;
        goto done;
    }

    new_leds[platform_leds->count] = strdup(led_name);
    platform_leds->leds = new_leds;
    platform_leds->count = new_led_count;

    success = true;

done:
    return success;
}

static void append_led_names(struct platform_leds_st * const platform_leds)
{
    DIR * const dir = foreach_led_names_init();

    if (dir != NULL)
    {
        char const * name;

        while ((name = foreach_led_names(dir)) != NULL)
        {
            append_led_name(platform_leds, name);
        }

        foreach_led_names_finish(dir);
    }
}

static bool
set_trigger_in_file(
    char const * const led, char const * const filename, char const * const trigger)
{
    UNUSED_ARG(led);

    FILE * const f = fopen(filename, "w");
    bool success;

    if (f == NULL)
    {
        error("failed to open 'trigger' property of LED '%s'", led);
        success = false;
        goto err;
    }

    char trigger_buf[50];

    snprintf(trigger_buf, sizeof(trigger_buf), "%s\n", trigger);

    int const ret = fputs(trigger_buf, f);

    if (ret < 0)
    {
        error("failed to write 'trigger' property of LED '%s' (ret=%d)",
              led, ret);
        success = false;
        goto err;
    }

    success = true;

err:
    if (f != NULL)
    {
        fclose(f);
    }

    return success;
}

static bool
set_trigger(char const * const led, char const * const trigger)
{
    char filename[PATH_MAX];

    snprintf(filename, sizeof(filename), SYS_LEDS_PREFIX "%s/trigger", led);

    bool const success = set_trigger_in_file(led, filename, trigger);

    return success;
}

static bool
set_delay_in_file(
    char const * const led, char const * const filename, int const delay)
{
    FILE * f = fopen(filename, "w");
    bool success;

    UNUSED_ARG(led);

    if (f == NULL)
    {
        f = fopen(filename, "w");
        if (f == NULL)
        {
            error("failed to open '%s' property LED '%s'", filename, led);
            success = false;
            goto err;
        }
    }

    int const ret = fprintf(f, "%d\n", delay);

    if (ret < 0)
    {
        error("failed to write property of LED '%s' (ret=%d)", led, ret);
        success = false;
        goto err;
    }

    success = true;

err:
    if (f != NULL)
    {
        fclose(f);
    }

    return success;
}

static bool
set_delay(char const * const led, char const * const file, int const delay)
{
    char filename[PATH_MAX];

    snprintf(filename, sizeof(filename), SYS_LEDS_PREFIX "%s/%s", led, file);

    bool const success = set_delay_in_file(led, filename, delay);

    return success;
}

static void
unlock(int const fd)
{
    if (fd >= 0)
    {
        close(fd);
    }
}

static int
lock(char const * const led)
{
    bool locked;
    int fd = -1;
    int ret;
    char filename[PATH_MAX];

    /* Create directory if doesn't exist */
    ret = mkdir(LOCK_DIR, 0755);
    if (ret < 0 && errno != EEXIST)
    {
        error("couldn't create lock dir (%d)", errno);
        locked = false;
        goto done;
    }

    snprintf(filename, sizeof(filename), LOCK_DIR "/%s", led);
    fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0664);
    if (fd < 0)
    {
        error("couldn't create lock file for %s", led);
        locked = false;
        goto done;
    }

    ret = flock(fd, LOCK_EX);
    if (ret != 0)
    {
        error("failed to obtain lock for %s", led);
        locked = false;
        goto done;
    }

    locked = true;

done:
    if (!locked)
    {
        unlock(fd);
        fd = -1;
    }

    return fd;
}

static int
get_led_state_from_file(char const * const filename)
{
    enum led_state_t led_state;
    int const fd = open(filename, O_RDONLY);

    if (fd < 0)
    {
        led_state = LED_STATE_UNKNOWN;
        goto done;
    }

    char buf[10];

    memset(buf, 0, sizeof buf);
    (void)TEMP_FAILURE_RETRY(read(fd, buf, sizeof buf - 1));

    errno = 0;

    int const brightness = strtol(buf, NULL, 10);

    if (errno != 0)
    {
        led_state = LED_STATE_UNKNOWN;
    }
    else
    {
        led_state = brightness == 0 ? LED_OFF : LED_ON;
    }

done:
    if (fd >= 0)
    {
        close(fd);
    }

    return led_state;
}

static int
get_led(char const * const led_name)
{
    char filename[PATH_MAX];

    snprintf(filename, sizeof(filename),
             SYS_LEDS_PREFIX "%s/brightness", led_name);

    enum led_state_t const led_state = get_led_state_from_file(filename);

    return led_state;
}

static bool
set_led_locked(
    int const cmd,
    char const * const led,
    unsigned const delay_on,
    unsigned const delay_off)
{
    bool result;

    if (!set_trigger(led, "timer"))
    {
        result = false;
        goto done;
    }

    /*
     * If this is an ON command, write delay_on first, as setting both
     * delay_on and delay_off to 0 will set the LED to blinking with default
     * delays. (delay_off might already be 0).
     */
    if (cmd == CMD_ON)
    {
        if (!set_delay(led, "delay_on", delay_on)
            || !set_delay(led, "delay_off", delay_off))
        {
            result = false;
            goto done;
        }
    }
    /* Otherwise set delay_off first */
    else
    {
        if (!set_delay(led, "delay_off", delay_off)
            || !set_delay(led, "delay_on", delay_on))
        {
            result = false;
            goto done;
        }
    }

    result = true;

done:
    return result;
}

static bool
set_led(int const cmd, char const * const led)
{
    bool result;
    unsigned delay_on;
    unsigned delay_off;
    int lock_fd = -1;

    switch (cmd)
    {
    case CMD_ON:
        delay_on = 1; /* Any non-zero value will do. */
        delay_off = 0;
        break;

    case CMD_OFF:
        delay_on = 0;
        delay_off = 1; /* Use 1 so the LED turns off ASAP. */
        break;

    case CMD_FLASH:
        delay_on = DELAY_SLOW;
        delay_off = DELAY_SLOW;
        break;

    case CMD_FLASH_FAST:
        delay_on = DELAY_FAST;
        delay_off = DELAY_FAST;
        break;

    default:
        error("invalid command");
        result = false;
        goto done;
    }

    lock_fd = lock(led);

    result = set_led_locked(cmd, led, delay_on, delay_off);

    unlock(lock_fd);

done:
    return result;
}

static enum led_state_t
get_led_state(
    led_handle_st * const led_handle, led_st const * const led)
{
    UNUSED_ARG(led_handle);

    char const * const led_name = (char *)led;
    enum led_state_t const led_state = get_led(led_name);

    return led_state;
}

static bool
set_led_state(
    led_handle_st * const led_handle,
    led_st * const led,
    enum led_state_t const state)
{
    UNUSED_ARG(led_handle);

    int ret;
    int cmd;

    switch (state)
    {
    case LED_OFF:
        cmd = CMD_OFF;
        break;

    case LED_ON:
        cmd = CMD_ON;
        break;

    case LED_SLOW_FLASH:
        cmd = CMD_FLASH;
        break;

    case LED_FAST_FLASH:
        cmd = CMD_FLASH_FAST;
        break;

    default:
        ret = false;
        goto done;
    }

    ret = set_led(cmd, (char *)led);

done:
    return ret;
}

static led_handle_st *
led_open()
{
    static int const dummy = 0;
    /* Nothing to do. Don't return NULL though as that indicates error. */

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

    for (size_t i = 0; i < platform_leds->count; i++)
    {
        char const * const led_name = platform_leds->leds[i];

        if (!cb((led_st *)led_name, user_ctx))
        {
            led = (led_st *)led_name;
            goto done;
        }
    }

    led = NULL;

done:
    return led;
}

static void
iterate_supported_states(
    void (*cb)(enum led_state_t state, void * user_ctx), void * const user_ctx)
{
    cb(LED_OFF, user_ctx);
    cb(LED_ON, user_ctx);
    cb(LED_SLOW_FLASH, user_ctx);
    cb(LED_FAST_FLASH, user_ctx);
}

static char const *
get_led_name(led_st const * const led)
{
    return (char *)led;
}

static enum led_colour_t
get_led_colour(led_st const * const led)
{
    UNUSED_ARG(led);

    return LED_COLOUR_UNKNOWN;
}

static platform_leds_st *
leds_init()
{
    /*
     * Create whatever context is required and return an opaque pointer to it
     * to the caller.
     */
    struct platform_leds_st * const platform_leds =
        calloc(1, sizeof *platform_leds);

    if (platform_leds != NULL)
    {
        append_led_names(platform_leds);
    }

    return platform_leds;
}

static void
leds_deinit(platform_leds_st * const platform_leds)
{
    if (platform_leds != NULL)
    {
        for (size_t i = 0; i < platform_leds->count; i++)
        {
            free(UNCONST(platform_leds->leds[i]));
        }
        free(platform_leds->leds);
        free(platform_leds);
    }
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

