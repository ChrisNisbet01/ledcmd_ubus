#ifndef __PLATFORM_LEDS_PLUGIN_H__
#define __PLATFORM_LEDS_PLUGIN_H__

#include "platform_specific.h"

void
platform_leds_plugin_unload(void * const handle);

void *
platform_leds_plugin_load(
	char const * const backend_path,
	struct platform_led_methods_st const ** const platform_methods);

#endif /* __PLATFORM_LEDS_PLUGIN_H__ */
