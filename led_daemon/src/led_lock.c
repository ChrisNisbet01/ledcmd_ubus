#include "led_lock.h"

#include <ubus_utils/ubus_utils.h>

static bool
create_led_lock_id(
	struct led_ctx_st * const led_ctx, char const * const lock_id)
{
	led_ctx->lock_id = strdup(lock_id);

	return led_ctx->lock_id != NULL;
}

static void
remove_lock_id(struct led_ctx_st * const led_ctx)
{
	free(UNCONST(led_ctx->lock_id));
	led_ctx->lock_id = NULL;
}

static bool
assign_lock_id(
	struct led_ctx_st * const led_ctx,
	char const * const lock_id,
	char const ** const error_msg)
{
	bool locked;

	if (led_ctx->lock_id != NULL) {
		locked = false;
		*error_msg = "LED already_locked";
	} else if (create_led_lock_id(led_ctx, lock_id)) {
		locked = true;
	} else {
		*error_msg = "Resource shortage";
		locked = false;
	}

	return locked;
}

static bool
unlock_id_is_correct(
	struct led_ctx_st * const led_ctx,
	char const * const lock_id,
	char const ** const error_msg)
{
	bool is_valid;

	if (led_ctx->lock_id == NULL) {
		*error_msg = "LED not locked";
		is_valid = false;
	} else if (lock_id == NULL) {
		*error_msg = "No lock ID supplied";
		is_valid = false;
	} else if (strcmp(led_ctx->lock_id, lock_id) != 0) {
		*error_msg = "Incorrect lock ID";
		is_valid = false;
	} else {
		is_valid = true;
	}

	return is_valid;
}

static bool
led_is_locked(struct led_ctx_st const * const led_ctx)
{
	bool const is_locked = led_priority_priority_is_active(
		led_ctx->priority_context, LED_PRIORITY_LOCKED);

	return is_locked;
}

bool
led_ctx_any_led_locked(struct avl_tree const * const tree)
{
	bool any_locked;
	struct led_ctx_st const *led_ctx;

	avl_for_each_element(tree, led_ctx, node) {
		if (led_is_locked(led_ctx)) {
			any_locked = true;
			goto done;
		}
	}

	any_locked = false;

done:
	return any_locked;
}

void
destroy_led_lock_id(struct led_ctx_st * const led_ctx)
{
	remove_lock_id(led_ctx);
}

bool
led_ctx_lock_led(
	struct led_ctx_st * const led_ctx,
	char const * const lock_id,
	char const ** const error_msg)
{
	return assign_lock_id(led_ctx, lock_id, error_msg);
}

bool
led_ctx_unlock_led(
	struct led_ctx_st * const led_ctx,
	char const * const lock_id,
	char const ** const error_msg)
{
	bool const id_is_correct = unlock_id_is_correct(led_ctx, lock_id, error_msg);

	if (id_is_correct) {
		destroy_led_lock_id(led_ctx);
	}

	return id_is_correct;
}

