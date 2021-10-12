#include "log.h"
#include "logging_plugin.h"

#include <dlfcn.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>

struct logging_context_st {
	void *lib_handle;
	struct logging_plugin_methods_st const *plugin_methods;
	void *plugin_context;
};

/*
 * Logging is the sort of thing that people want to do from anywhere and
 * everywhere, and I don't think it's reasonable to expect them to pass a
 * logging context around to every function that might want to log something.
 * I think it's reasonable to make an exception to the "don't use
 * global/file scope /'hidden' variables" rule in this case.
 */
struct logging_context_st *logging_context;

static struct logging_plugin_methods_st const *
get_plugin_methods(void * const lib_handle)
{
	struct logging_plugin_methods_st const *methods;
	char const _logging_plugin_methods[] = "log_methods";
	logging_plugin_methods_fn logging_plugin_methods;

	*(void **)(&logging_plugin_methods) =
		dlsym(lib_handle, _logging_plugin_methods);

	if (logging_plugin_methods == NULL) {
		methods = NULL;
		goto done;
	}

	methods = logging_plugin_methods(LIB_LOG_PLUGIN_VERSION);

done:
	return methods;
}

int
log_error(char const * const fmt, ...)
{
	int res;

	if (logging_context == NULL) {
		res = -1;
		goto done;
	}

	struct logging_plugin_methods_st const * const plugin_methods =
	    logging_context->plugin_methods;

	if (plugin_methods == NULL) {
		res = -1;
		goto done;
	}

	va_list args;

	va_start(args, fmt);
	res = plugin_methods->log_error(
	    logging_context->plugin_context, fmt, args);
	va_end(args);

done:
	return res;
}

int
log_info(char const * const fmt, ...)
{
	int res;

	if (logging_context == NULL) {
		res = -1;
		goto done;
	}

	struct logging_plugin_methods_st const * const plugin_methods =
	    logging_context->plugin_methods;

	if (plugin_methods == NULL) {
		res = -1;
		goto done;
	}

	va_list args;

	va_start(args, fmt);
	res = plugin_methods->log_info(
	    logging_context->plugin_context, fmt, args);
	va_end(args);

done:
	return res;
}

static int
log_init(char const * const program_name, int const a, int const b)
{
	int res;

	if (logging_context == NULL) {
		res = -1;
		goto done;
	}

	struct logging_plugin_methods_st const * const plugin_methods =
	    logging_context->plugin_methods;

	if (plugin_methods == NULL) {
		res = -1;
		goto done;
	}

	logging_context->plugin_context =
		plugin_methods->log_init(program_name, a, b);

	res = 0;

done:
	return res;
}

void
logging_plugin_unload(void)
{
	if (logging_context == NULL) {
		goto done;
	}

	struct logging_plugin_methods_st const * const plugin_methods =
	    logging_context->plugin_methods;

	if (plugin_methods != NULL) {
		plugin_methods->log_unload(logging_context->plugin_context);
	}

	if (logging_context->lib_handle != NULL) {
		dlclose(logging_context->lib_handle);
	}

	free(logging_context);
	logging_context = NULL;

done:
	return;
}

bool
logging_plugin_load(
	char const * const plugin_path,
	char const * const program_name,
	int const a,
	int const b)
{
	bool success;
	logging_context = calloc(1, sizeof *logging_context);

	if (logging_context == NULL) {
		success = false;
		goto done;
	}

	void * const lib_handle = dlopen(plugin_path, RTLD_NOW);

	if (lib_handle == NULL)
	{
		success = false;
		goto done;
	}

	logging_context->lib_handle = lib_handle;
	logging_context->plugin_methods = get_plugin_methods(lib_handle);

	if (log_init(program_name, a, b) < 0) {
		success = false;
		goto done;
	}

	success = true;

done:
	if (!success) {
		logging_plugin_unload();
		logging_context = NULL;
	}

	return success;
}

