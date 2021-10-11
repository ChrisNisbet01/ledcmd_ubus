#ifndef __LOGGING_PLUGIN_H__
#define __LOGGING_PLUGIN_H__

#include <stdarg.h>

#define LIB_LOG_PLUGIN_VERSION 1

typedef struct logging_plugin_context_st logging_plugin_context_st;

typedef int (*plugin_log_fn)
	(logging_plugin_context_st * context, char const * fmt, va_list ap);

typedef logging_plugin_context_st * (*plugin_log_init_fn)
	(char const * program_name, int a, int b);

typedef void (*plugin_log_unload_fn)(logging_plugin_context_st *context);

struct logging_plugin_methods_st {
	plugin_log_init_fn log_init;
	plugin_log_unload_fn log_unload;
	plugin_log_fn log_error;
	plugin_log_fn log_info;
};

typedef struct logging_plugin_methods_st const *
	(*logging_plugin_methods_fn)(int plugin_version);

struct logging_plugin_methods_st const *
log_methods(int plugin_version);

#endif /* __LOGGING_PLUGIN_H__ */
