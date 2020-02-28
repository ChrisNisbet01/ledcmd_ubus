#include "lib_led_control.h"
#include "lib_led_private.h"
#include "string_constants.h"

#include <libubus_utils/ubus_utils.h>

static void
append_led_request_data(
	char const * const led_name,
	char const * const state,
	char const * const lock_id,
	char const * const led_priority,
	char const * const flash_type,
	uint32_t const flash_time_ms,
	struct blob_buf * const msg)
{
	void * const cookie = blobmsg_open_table(msg, NULL);

	blobmsg_add_string(msg, _led_name, led_name);
	if (flash_type != NULL) {
		blobmsg_add_string(msg, _led_flash_type, flash_type);
	}
	if (flash_time_ms) {
		blobmsg_add_u32(msg, _led_flash_time_ms, flash_time_ms);
	}
	if (state != NULL) {
		blobmsg_add_string(msg, _led_state, state);
	}
	if (lock_id != NULL) {
		blobmsg_add_string(msg, _led_lock_id, lock_id);
	}
	if (led_priority != NULL) {
		blobmsg_add_string(msg, _led_priority, led_priority);
	}

	blobmsg_close_table(msg, cookie);
}

static bool
send_ubus_led_request(
	struct ledcmd_ctx_st const * const ctx,
	char const * const cmd,
	char const * const state,
	char const * const led_name,
	char const * const lock_id,
	char const * const led_priority,
	char const * const flash_type,
	uint32_t const flash_time_ms,
	ubus_cb const cb,
	void * const cb_context)
{
	struct blob_buf msg;

	blob_buf_full_init(&msg, 0);

	void * const cookie = blobmsg_open_array(&msg, _led_leds);

	append_led_request_data(led_name,
							state,
							lock_id,
							led_priority,
							flash_type,
							flash_time_ms,
							&msg);
	blobmsg_close_array(&msg, cookie);

	bool const success = ledcmd_ubus_invoke(cmd, &msg, cb, cb_context, ctx);

	blob_buf_free(&msg);

	return success;
}

struct ledcmd_lock_ctx_st {
	bool success;
	led_lock_result_cb const cb;
	void * const cb_context;
};

static void
populate_lock_led_result(
	struct led_lock_result_st * const result,
	struct blob_attr * const response)
{
	enum {
		SUCCESS,
		ERROR_MSG,
		LOCK_ID,
		__LOCK_MAX
	};
	struct blob_attr *fields[__LOCK_MAX];
	struct blobmsg_policy const get_lock_policy[__LOCK_MAX] = {
		[SUCCESS] =
			{.name = _led_success, .type = BLOBMSG_TYPE_BOOL},
		[ERROR_MSG] =
			{.name = _led_error, .type = BLOBMSG_TYPE_STRING},
		[LOCK_ID] =
			{.name = _led_lock_id, .type = BLOBMSG_TYPE_STRING}
	};

	blobmsg_parse(
		get_lock_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	result->success = blobmsg_get_bool_or_default(fields[SUCCESS], false);
	result->error_msg = blobmsg_get_string(fields[ERROR_MSG]);
	result->lock_id = blobmsg_get_string(fields[LOCK_ID]);
}

static void
process_lock_led_response(
	struct blob_attr const * const array_blob,
	struct ledcmd_lock_ctx_st * const ctx)
{
	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_TABLE)) {
		goto done;
	}

	ctx->success = true;

	if (ctx->cb == NULL) {
		goto done;
	}

	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		struct led_lock_result_st lock_result;

		populate_lock_led_result(&lock_result, cur);

		ctx->cb(&lock_result, ctx->cb_context);
	}

done:
	return;
}

static void
lock_led_response_handler(
	struct ubus_request * const req,
	int const type,
	struct blob_attr * const response)
{
	UNUSED_ARG(type);

	struct ledcmd_lock_ctx_st * const ctx = req->priv;
	enum {
		LOCK_RESPONSE,
		__LOCK_MAX
	};
	struct blob_attr *fields[__LOCK_MAX];
	struct blobmsg_policy const lock_policy[] = {
		[LOCK_RESPONSE] = {.name = _led_leds, .type = BLOBMSG_TYPE_ARRAY}
	};

	blobmsg_parse(
		lock_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	process_lock_led_response(fields[LOCK_RESPONSE], ctx);
}

bool
led_activate_or_deactivate(
	struct ledcmd_ctx_st const * const ledcmd_ctx,
	bool const activate_it,
	char const * const led_name,
	char const * const led_priority,
	char const * const lock_id,
	led_lock_result_cb const cb,
	void * const cb_context)
{
	char const * const ubus_cmd = activate_it ? _led_activate : _led_deactivate;
	struct ledcmd_lock_ctx_st ctx = {
		/* Assume failure unless the message gets a response.*/
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};

	send_ubus_led_request(
		ledcmd_ctx, ubus_cmd, NULL,
		led_name, lock_id, led_priority, NULL, 0,
		lock_led_response_handler, &ctx);

	return ctx.success;
}

struct ledcmd_names_ctx_st {
	bool success;
	led_names_cb const cb;
	void * const cb_context;
};

static void
process_led_names_response(
	struct blob_attr const * const array_blob,
	struct ledcmd_names_ctx_st * const ctx)
{
	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_TABLE)) {
		goto done;
	}

	ctx->success = true;

	if (ctx->cb == NULL) {
		goto done;
	}

	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		enum {
			GET_NAME,
			__GET_NAME_MAX
		};
		struct blob_attr *fields[__GET_NAME_MAX];
		struct blobmsg_policy const get_state_response_policy[] = {
			[GET_NAME] =
				{.name = _led_name, .type = BLOBMSG_TYPE_STRING}
		};

		blobmsg_parse(
			get_state_response_policy, ARRAY_SIZE(fields), fields,
			blobmsg_data(cur), blobmsg_len(cur));

		ctx->cb(blobmsg_get_string(fields[GET_NAME]), ctx->cb_context);
	}

done:
	return;
}

static void
get_led_names_response_handler(
	struct ubus_request * const req,
	int const type,
	struct blob_attr * const response)
{
	UNUSED_ARG(type);

	struct ledcmd_names_ctx_st * const ctx = req->priv;
	enum {
		NAMES_RESPONSE,
		__NAMES_MAX
	};
	struct blob_attr *fields[__NAMES_MAX];
	struct blobmsg_policy const get_names_policy[] = {
		[NAMES_RESPONSE] = {.name = _led_leds, .type = BLOBMSG_TYPE_ARRAY}
	};

	blobmsg_parse(
		get_names_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	process_led_names_response(fields[NAMES_RESPONSE], ctx);
}

bool
led_get_names(
	struct ledcmd_ctx_st const * const ledcmd_ctx,
	led_names_cb cb,
	void * const cb_context)
{
	struct ledcmd_names_ctx_st ctx = {
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};
	struct blob_buf msg;

	blob_buf_full_init(&msg, 0);
	ledcmd_ubus_invoke(_led_list,
			   &msg,
			   get_led_names_response_handler, &ctx,
			   ledcmd_ctx);
	blob_buf_free(&msg);

	return ctx.success;
}

struct ledcmd_states_ctx_st {
	bool success;
	led_states_cb const cb;
	void * const cb_context;
};

static void
process_led_states_response(
	struct blob_attr const * const array_blob,
	struct ledcmd_states_ctx_st * const ctx)
{
	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_STRING)) {
		goto done;
	}

	ctx->success = true;

	if (ctx->cb == NULL) {
		goto done;
	}

	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		ctx->cb(blobmsg_get_string(cur), ctx->cb_context);
	}

done:
	return;
}

static void
led_states_response_handler(
	struct ubus_request *req, int type, struct blob_attr *response)
{
	UNUSED_ARG(type);

	struct ledcmd_states_ctx_st * const ctx = req->priv;
	enum {
		STATES_RESPONSE,
		__STATES_MAX
	};
	struct blob_attr *fields[__STATES_MAX];
	struct blobmsg_policy const get_supported_states_policy[] = {
		[STATES_RESPONSE] =
			{.name = _led_supported_states, .type = BLOBMSG_TYPE_ARRAY}
	};

	blobmsg_parse(
		get_supported_states_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	process_led_states_response(fields[STATES_RESPONSE], ctx);
}

bool
led_get_states(
	struct ledcmd_ctx_st const * const ledcmd_ctx,
	led_names_cb const cb,
	void * const cb_context)
{
	struct blob_buf msg;
	struct ledcmd_states_ctx_st ctx = {
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};

	blob_buf_full_init(&msg, 0);
	ledcmd_ubus_invoke(
		_led_list_supported_states,
		&msg,
		led_states_response_handler,
		&ctx,
		ledcmd_ctx);
	blob_buf_free(&msg);

	return ctx.success;
}

struct ledcmd_led_ctx_st {
	bool success;
	led_get_set_cb const cb;
	void * const cb_context;
};

static void populate_get_set_result(
	struct led_get_set_result_st * const result,
	struct blob_attr * const response)
{
	enum {
		SUCCESS,
		LED_NAME,
		LED_STATE,
		LED_ERROR,
		LOCK_ID,
		LED_PRIORITY,
		__LED_MAX
	};
	struct blob_attr *fields[__LED_MAX];
	struct blobmsg_policy const set_state_response_policy[] = {
		[SUCCESS] =
			{.name = _led_success, .type = BLOBMSG_TYPE_BOOL},
		[LED_NAME] =
			{.name = _led_name, .type = BLOBMSG_TYPE_STRING},
		[LED_STATE] =
			{.name = _led_state, .type = BLOBMSG_TYPE_STRING},
		[LED_ERROR] =
			{.name = _led_error, .type = BLOBMSG_TYPE_STRING},
		[LOCK_ID] =
			{.name = _led_lock_id, .type = BLOBMSG_TYPE_STRING},
		[LED_PRIORITY] =
			{.name = _led_priority, .type = BLOBMSG_TYPE_STRING}
	};

	blobmsg_parse(
		set_state_response_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	result->success = blobmsg_get_bool_or_default(fields[SUCCESS], false);
	result->led_name = blobmsg_get_string(fields[LED_NAME]);
	result->error_msg = blobmsg_get_string(fields[LED_ERROR]);
	result->led_state = blobmsg_get_string(fields[LED_STATE]);
	result->lock_id = blobmsg_get_string(fields[LOCK_ID]);
	result->led_priority = blobmsg_get_string(fields[LED_PRIORITY]);
}

static void
process_led_response(
	struct blob_attr const * const array_blob,
	struct ledcmd_led_ctx_st * const ctx)
{
	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_TABLE)) {
		goto done;
	}

	ctx->success = true;

	if (ctx->cb == NULL) {
		goto done;
	}

	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		struct led_get_set_result_st result;

		populate_get_set_result(&result, cur);

		ctx->cb(&result, ctx->cb_context);
	}

done:
	return;
}

static void
led_response_handler(
	struct ubus_request *req, int type, struct blob_attr *response)
{
	UNUSED_ARG(type);

	struct ledcmd_led_ctx_st * const ctx = req->priv;
	enum {
		SET_RESPONSE,
		__SET_MAX
	};
	struct blob_attr *fields[__SET_MAX];
	struct blobmsg_policy const set_reply_policy[] = {
		[SET_RESPONSE] = {.name = _led_leds, .type = BLOBMSG_TYPE_ARRAY}
	};

	blobmsg_parse(
		set_reply_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	process_led_response(fields[SET_RESPONSE], ctx);
}

bool
led_get_set_request(
	struct ledcmd_ctx_st const * const ledcmd_ctx,
	char const * const cmd,
	char const * const state,
	char const * const led_name,
	char const * const lock_id,
	char const * const led_priority,
	char const * const flash_type,
	uint32_t const flash_time_ms,
	led_get_set_cb const cb,
	void * const cb_context)
{
	struct ledcmd_led_ctx_st ctx = {
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};

	send_ubus_led_request(
		ledcmd_ctx,
		cmd,
		state,
		led_name,
		lock_id,
		led_priority,
		flash_type,
		flash_time_ms,
		led_response_handler,
		&ctx);

	return ctx.success;
}

