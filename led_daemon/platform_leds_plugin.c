#include "platform_leds_plugin.h"

#include <dlfcn.h>
#include <linux/limits.h>
#include <stdio.h>

static struct platform_led_methods_st const *
get_platform_methods(void *plugin_handle)
{
	struct platform_led_methods_st const *methods;
	char const _platform_leds_methods[] = "platform_leds_methods";
	platform_leds_methods_fn platform_leds_methods;

	*(void **)(&platform_leds_methods) =
		dlsym(plugin_handle, _platform_leds_methods);

	if (platform_leds_methods == NULL) {
		methods = NULL;
		goto done;
	}

	methods = platform_leds_methods(LED_DAEMON_PLUGIN_VERSION);

done:
	return methods;
}

void
platform_leds_plugin_unload(void * const handle)
{
	if (handle != NULL) {
		dlclose(handle);
	}
}

void *
platform_leds_plugin_load(
	char const * const backend_directory,
	struct platform_led_methods_st const * * const platform_methods)
{
	char plugin_path[PATH_MAX];
    char const plugin_name[] = "led_daemon_backend_plugin.so";

	if (backend_directory == NULL) {
		snprintf(plugin_path, sizeof plugin_path, "%s", plugin_name);
	} else {
		snprintf(plugin_path, sizeof plugin_path,
				 "%s/%s", backend_directory, plugin_name);
	}

	void * const plugin_handle = dlopen(plugin_path, RTLD_NOW);

    if (plugin_handle == NULL) {
        *platform_methods = NULL;
		goto done;
    }

	*platform_methods = get_platform_methods(plugin_handle);

done:
	return plugin_handle;
}

