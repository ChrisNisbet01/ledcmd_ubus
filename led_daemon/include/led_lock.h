#ifndef LED_LOCK_H__
#define LED_LOCK_H__

#include "led_control.h"

#include <libubox/avl.h>

#include <stdbool.h>

bool
led_ctx_lock_led(
    struct led_ctx_st * led_ctx, char const * lock_id, char const ** error_msg);

bool
led_ctx_unlock_led(
    struct led_ctx_st * led_ctx, char const * lock_id, char const ** const error_msg);

bool
led_ctx_any_led_locked(struct avl_tree const * const tree);

void
destroy_led_lock_id(struct led_ctx_st * const led_ctx);

#endif /* LED_LOCK_H__ */

