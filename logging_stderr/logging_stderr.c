#include <lib_log/logging_plugin.h>
#include <ubus_utils/ubus_utils.h>

#include <stdbool.h>
#include <stdlib.h>

static int
do_log(char const * const prefix, char const * const fmt, va_list ap)
{
    fprintf(stderr, "%s: ", prefix);
    int const res = vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    return res;
}

static int
plugin_log_error(
    logging_plugin_context_st * const context, char const * const fmt, va_list ap)
{
    UNUSED_ARG(context);

    return do_log("error", fmt, ap);
}

static int
plugin_log_info(
    logging_plugin_context_st * const context, char const * const fmt, va_list ap)
{
    UNUSED_ARG(context);

    return do_log("info", fmt, ap);
}

static void
plugin_unload(logging_plugin_context_st * const context)
{
    UNUSED_ARG(context);
}

static logging_plugin_context_st *
plugin_log_init(char const * const program_name, int const a, int const b)
{
    UNUSED_ARG(program_name);
    UNUSED_ARG(a);
    UNUSED_ARG(b);

    return NULL;
}

struct logging_plugin_methods_st const *
log_methods(int const plugin_version)
{
    static struct logging_plugin_methods_st const methods =
    {
        .log_init = plugin_log_init,
        .log_unload = plugin_unload,
        .log_error = plugin_log_error,
        .log_info = plugin_log_info
    };
    struct logging_plugin_methods_st const * plugin_methods;
    bool const version_ok = plugin_version == LIB_LOG_PLUGIN_VERSION;

    if (!version_ok)
    {
        plugin_methods = NULL;
        goto done;
    }

    plugin_methods = &methods;

done:
    return plugin_methods;
}

