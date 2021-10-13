#ifndef LED_ALIASES_H__
#define LED_ALIASES_H__

#include "led_states.h"
#include "led_pattern_list.h"

#include <libubox/uloop.h>
#include <libubox/avl.h>

#include <stddef.h>
#include <stdbool.h>

typedef struct led_aliases_st led_aliases_st;
typedef struct led_alias_st led_alias_st;

char const *
led_alias_name(led_alias_st const * led_alias);

/* Call the callback for each alias in led_alias. */
void
led_alias_iterate(
    led_alias_st const * led_alias,
    bool (*cb)(char const * led_name, void * user_ctx),
    void * user_ctx);

void
led_aliases_iterate(
    led_aliases_st const * led_aliases,
    bool (*cb)(led_alias_st const * led_alias, void * user_ctx),
    void * user_ctx);

led_alias_st const *
led_alias_lookup(
    led_aliases_st const * led_aliases, char const * logical_led_name);

void
led_aliases_free(led_aliases_st const * led_aliases);

led_aliases_st const *
led_aliases_load(char const * aliases_directory);

#endif /* LED_ALIASES_H__ */

