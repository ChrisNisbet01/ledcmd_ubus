#ifndef __LED_PRIORITY_CONTEXT_H__
#define __LED_PRIORITY_CONTEXT_H__

#include <stddef.h>
#include <stdbool.h>

#include "led_priorities.h"

typedef struct led_priority_st led_priority_st;

enum led_priority_t
led_priority_highest_priority(led_priority_st const *priority_context);

bool
led_priority_priority_is_active(
    led_priority_st const *priority_context, enum led_priority_t priority);

enum led_priority_t
led_priority_priority_activate(
    led_priority_st * priority_context, enum led_priority_t priority);

enum led_priority_t
led_priority_priority_deactivate(
    led_priority_st *priority_context, enum led_priority_t priority);

void
led_priority_free(led_priority_st *priority_context);

led_priority_st *
led_priority_allocate(size_t num_priorities);

#endif /* __LED_PRIORITY_CONTEXT_H__ */
