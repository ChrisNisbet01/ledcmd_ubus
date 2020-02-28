#include "led_daemon_ubus.h"
#include "led_control.h"
#include "led_pattern_control.h"
#include "led_lock.h"

#include <lib_log/log.h>
#include <lib_led/string_constants.h>

#include <libubus_utils/ubus_connection.h>
#include <libubus_utils/ubus_utils.h>

#include <stddef.h>
#include <unistd.h>

struct ledcmd_ubus_context_st {
	struct ubus_connection_ctx_st ubus_connection;
	struct led_ops_st const *led_ops;
	void *led_ops_context;
};

static uint32_t const default_flash_ms = 0;

static void
append_led_data(
	char const * const led_name,
	bool const success,
	char const * const state,
	char const * const lock_id,
	char const * const led_priority,
	char const * const error_msg,
	struct blob_buf * const response)
{
	void * const cookie = blobmsg_open_table(response, NULL);

	blobmsg_add_string(response, _led_name, led_name);
	blobmsg_add_u8(response, _led_success, success);

	if (state != NULL) {
		blobmsg_add_string(response, _led_state, state);
	}
	if (lock_id != NULL) {
		blobmsg_add_string(response, _led_lock_id, lock_id);
	}
	if (led_priority != NULL) {
		blobmsg_add_string(response, _led_priority, led_priority);
	}
	if (error_msg != NULL) {
		blobmsg_add_string(response, _led_error, error_msg);
	}

	blobmsg_close_table(response, cookie);
}

#define append_activate_response(name, result, lock_id, msg, response) \
	append_led_data(name, result, NULL, lock_id, NULL, msg, response)

#define append_get_response(name, result, state, lock_id, priority, msg, response) \
	append_led_data(name, result, state, lock_id, priority, msg, response)

#define append_set_response(name, result, state, msg, response) \
	append_led_data(name, result, state, NULL, NULL, msg, response)

static void
append_play_pattern_result(
	bool const success,
	char const * const error_msg,
	struct blob_buf * const response)
{
	blobmsg_add_u8(response, _led_success, success);

	if (error_msg != NULL) {
		blobmsg_add_string(response, _led_error, error_msg);
	}
}

static void
append_led_details(
	char const * const led_name,
	char const * const led_colour,
	struct blob_buf * const response)
{
	void * const cookie = blobmsg_open_table(response, NULL);

	blobmsg_add_string(response, _led_name, led_name);
	blobmsg_add_string(response, _led_colour, led_colour);

	blobmsg_close_table(response, cookie);
}

static void
get_state_cb(
	char const *const led_name,
	bool const success,
	char const * const led_state,
	char const * const lock_id,
	char const * const led_priority,
	char const * const error_msg,
	void * const result_context)
{
	struct blob_buf * const response = result_context;

	append_get_response(
		led_name, success, led_state, lock_id, led_priority, error_msg, response);
}

static bool
process_get_request(
	struct led_ops_st const * const led_ops,
	struct led_ops_handle_st * const led_ops_handle,
	char const * const led_name,
	struct blob_buf * const response)
{
	bool success;

	if (led_name == NULL) {
		success = false;
		goto done;
	}

	led_ops->get_state(led_ops_handle, led_name, get_state_cb, response);

	success = true;

done:
	return success;
}

static bool
process_get_state_attr(
	struct led_ops_st const * const led_ops,
	struct led_ops_handle_st * const led_ops_handle,
	struct blob_attr const * const attr,
	struct blob_buf * const response)
{
	enum {
		LED_NAME,
		__LED_MAX
	};
	struct blobmsg_policy const get_led_policy[__LED_MAX] = {
		[LED_NAME] = { .name = _led_name, .type = BLOBMSG_TYPE_STRING }
	};
	struct blob_attr *fields[__LED_MAX];

	blobmsg_parse(get_led_policy, ARRAY_SIZE(get_led_policy), fields,
				  blobmsg_data(attr), blobmsg_data_len(attr));

	return process_get_request(
		led_ops,
		led_ops_handle,
		blobmsg_get_string(fields[LED_NAME]),
		response);
}

enum {
	GET_LEDS,
	__GET_MAX
};

static struct blobmsg_policy const get_state_policy[__GET_MAX] = {
	[GET_LEDS] = { .name = _led_leds, .type = BLOBMSG_TYPE_ARRAY }
};

static int
process_get_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	int result;
	struct led_ops_st const * const led_ops = ubus_context->led_ops;
	struct led_ops_handle_st * const led_ops_handle =
		led_ops->open(ubus_context->led_ops_context);

	if (led_ops_handle == NULL) {
		result = UBUS_STATUS_UNKNOWN_ERROR;
		goto done;
	}

	struct blob_attr *fields[__GET_MAX];

	blobmsg_parse(get_state_policy, ARRAY_SIZE(get_state_policy), fields,
				  blobmsg_data(msg), blobmsg_len(msg));

	struct blob_attr const * const array_blob = fields[GET_LEDS];

	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_TABLE)) {
		/* The array needs to contain a set of tables, and it doesn't.*/
		result = UBUS_STATUS_INVALID_ARGUMENT;
		goto done;
	}

	void * const cookie = blobmsg_open_array(response, _led_leds);
	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		if (!process_get_state_attr(
				led_ops, led_ops_handle, cur, response)) {
			result = UBUS_STATUS_INVALID_ARGUMENT;
			goto done;
		}
	}

	blobmsg_close_array(response, cookie);

	result = UBUS_STATUS_OK;

done:
	led_ops->close(led_ops_handle);

	return result;
}

static int
get_state_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);

	int const result = process_get_msg(ubus_context, msg, &response);

	if (result == UBUS_STATUS_OK) {
		ubus_send_reply(ctx, req, response.head);
	}
	blob_buf_free(&response);

	return result;
}

static void
populate_led_set_state_request(
	struct blob_attr const * const request,
	struct set_state_req_st * const set_state_req)
{
	enum {
		LED_NAME,
		LED_STATE,
		LOCK_ID,
		LED_PRIORITY,
		FLASH_TYPE,
		FLASH_TIME_MS,
		__LED_MAX
	};
	struct blobmsg_policy const set_led_policy[__LED_MAX] = {
		[LED_NAME] =
			{.name = _led_name, .type = BLOBMSG_TYPE_STRING },
		[LED_STATE] =
			{.name = _led_state, .type = BLOBMSG_TYPE_STRING },
		[LOCK_ID] =
			{.name = _led_lock_id, .type = BLOBMSG_TYPE_STRING },
		[LED_PRIORITY] =
			{.name = _led_priority, .type = BLOBMSG_TYPE_STRING },
		[FLASH_TYPE] =
			{.name = _led_flash_type, .type = BLOBMSG_TYPE_STRING },
		[FLASH_TIME_MS] =
			{.name = _led_flash_time_ms, .type = BLOBMSG_TYPE_INT32 },
	};
	struct blob_attr *fields[__LED_MAX];

	blobmsg_parse(set_led_policy, ARRAY_SIZE(set_led_policy), fields,
				  blobmsg_data(request), blobmsg_data_len(request));

	memset(set_state_req, 0, sizeof *set_state_req);
	set_state_req->led_name = blobmsg_get_string(fields[LED_NAME]);
	set_state_req->lock_id = blobmsg_get_string(fields[LOCK_ID]);
	set_state_req->led_priority = blobmsg_get_string(fields[LED_PRIORITY]);
	set_state_req->state =
		led_state_by_query_name(blobmsg_get_string(fields[LED_STATE]));
	set_state_req->flash_type =
		led_flash_type_lookup(blobmsg_get_string(fields[FLASH_TYPE]));
	set_state_req->flash_time_ms =
		blobmsg_get_u32_or_default(fields[FLASH_TIME_MS], default_flash_ms);
}

static void
set_state_cb(
	char const * const led_name,
	bool const success,
	char const * const state,
	char const * const error_msg,
	void *user_context)
{
	struct blob_buf * const response = user_context;

	append_set_response(led_name, success, state, error_msg, response);
}

static bool
process_set_state_attr(
	struct led_ops_st const * const led_ops,
	struct led_ops_handle_st * const led_ops_handle,
	struct blob_attr const * const request,
	struct blob_buf * const response)
{
	bool success;
	struct set_state_req_st set_state_req;

	populate_led_set_state_request(request, &set_state_req);

	if (set_state_req.led_name == NULL
		|| set_state_req.state == LED_STATE_UNKNOWN) {
		success = false;
		goto done;
	}

	led_ops->set_state(led_ops_handle, &set_state_req, set_state_cb, response);

	success = true;

done:
	return success;
}

enum {
	SET_LEDS,
	__SET_MAX
};

static struct blobmsg_policy const set_state_policy[__SET_MAX] = {
	[SET_LEDS] = { .name = _led_leds, .type = BLOBMSG_TYPE_ARRAY }
};

static bool
process_set_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	struct led_ops_st const * const led_ops = ubus_context->led_ops;
	int result;
	struct blob_attr *fields[__SET_MAX];
	struct led_ops_handle_st * const led_ops_handle =
		led_ops->open(ubus_context->led_ops_context);

	if (led_ops_handle == NULL) {
		result = UBUS_STATUS_UNKNOWN_ERROR;
		goto done;
	}

	blobmsg_parse(set_state_policy, ARRAY_SIZE(set_state_policy), fields,
				  blobmsg_data(msg), blobmsg_len(msg));

	struct blob_attr const * const array_blob = fields[SET_LEDS];

	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_TABLE)) {
		/* The array needs to contain a set of tables, and it doesn't.*/
		result = UBUS_STATUS_INVALID_ARGUMENT;
		goto done;
	}

	void * const cookie = blobmsg_open_array(response, _led_leds);
	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		if (!process_set_state_attr(led_ops, led_ops_handle, cur, response)) {
			result = UBUS_STATUS_INVALID_ARGUMENT;
			goto done;
		}
	}

	blobmsg_close_array(response, cookie);

	result = UBUS_STATUS_OK;

done:
	led_ops->close(led_ops_handle);

	return result;
}

static int
set_state_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);

	int const result = process_set_msg(ubus_context, msg, &response);

	if (result == UBUS_STATUS_OK) {
		ubus_send_reply(ctx, req, response.head);
	}
	blob_buf_free(&response);

	return result;
}

static void
activate_led_cb(
	char const * const led_name,
	bool const success,
	char const * const lock_id,
	char const * const error_msg,
	void * const result_context)
{
	struct blob_buf * const response = result_context;

	append_activate_response(led_name, success, lock_id, error_msg, response);
}

static bool
process_activate_request(
	struct led_ops_st const * const led_ops,
	struct led_ops_handle_st * const led_ops_handle,
	char const * const led_name,
	char const * const led_priority,
	char const * const lock_id,
	struct blob_buf * const response)
{
	bool success;

	if (led_name == NULL) {
		success = false;
		goto done;
	}
	success =
		led_ops->activate_priority(
			led_ops_handle,
			led_name,
			led_priority,
			lock_id,
			activate_led_cb,
			response);

done:
	return success;
}

static bool
process_activate_attr(
	struct led_ops_st const * const led_ops,
	struct led_ops_handle_st * const led_ops_handle,
	struct blob_attr const * const attr,
	struct blob_buf * const response)
{
	enum {
		LED_NAME,
		LED_PRIORITY,
		LOCK_ID,
		__LED_MAX
	};
	struct blobmsg_policy const activate_led_policy[__LED_MAX] = {
		[LED_NAME] = { .name = _led_name, .type = BLOBMSG_TYPE_STRING },
		[LED_PRIORITY] = { .name = _led_priority, .type = BLOBMSG_TYPE_STRING },
		[LOCK_ID] = { .name = _led_lock_id, .type = BLOBMSG_TYPE_STRING }
	};
	struct blob_attr *fields[__LED_MAX];

	blobmsg_parse(activate_led_policy, ARRAY_SIZE(activate_led_policy), fields,
				  blobmsg_data(attr), blobmsg_data_len(attr));

	return process_activate_request(
		led_ops,
		led_ops_handle,
		blobmsg_get_string(fields[LED_NAME]),
		blobmsg_get_string(fields[LED_PRIORITY]),
		blobmsg_get_string(fields[LOCK_ID]),
		response);
}

enum {
	ACTIVATE_LEDS,
	__ACTIVATE_MAX
};

static struct blobmsg_policy const activate_policy[__ACTIVATE_MAX] = {
	[ACTIVATE_LEDS] = { .name = _led_leds, .type = BLOBMSG_TYPE_ARRAY }
};

static int
process_activate_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	int result;
	struct blob_attr *fields[__ACTIVATE_MAX];
	struct led_ops_st const * const led_ops = ubus_context->led_ops;
	struct led_ops_handle_st * const led_ops_handle =
		led_ops->open(ubus_context->led_ops_context);

	if (led_ops_handle == NULL) {
		result = UBUS_STATUS_UNKNOWN_ERROR;
		goto done;
	}

	blobmsg_parse(activate_policy, ARRAY_SIZE(activate_policy), fields,
				  blobmsg_data(msg), blobmsg_len(msg));

	struct blob_attr const * const array_blob = fields[ACTIVATE_LEDS];

	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_TABLE)) {
		/* The array needs to contain a set of tables, and it doesn't.*/
		result = UBUS_STATUS_INVALID_ARGUMENT;
		goto done;
	}

	void * const cookie = blobmsg_open_array(response, _led_leds);
	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		if (!process_activate_attr(led_ops, led_ops_handle, cur, response)) {
			result = UBUS_STATUS_INVALID_ARGUMENT;
			goto done;
		}
	}

	blobmsg_close_array(response, cookie);

	result = UBUS_STATUS_OK;

done:
	led_ops->close(led_ops_handle);

	return result;
}

static int
activate_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);

	int const result = process_activate_msg(ubus_context, msg, &response);

	if (result == UBUS_STATUS_OK) {
		ubus_send_reply(ctx, req, response.head);
	}
	blob_buf_free(&response);

	return result;
}

static void
deactivate_led_cb(
	char const * const led_name,
	bool const success,
	char const * const lock_id,
	char const * const error_msg,
	void * const result_context)
{
	struct blob_buf * const response = result_context;

	append_activate_response(led_name, success, lock_id, error_msg, response);
}

static bool
process_deactivate_request(
	struct led_ops_st const * const led_ops,
	struct led_ops_handle_st * const led_ops_handle,
	char const * const led_name,
	char const * const led_priority,
	char const * const lock_id,
	struct blob_buf * const response)
{
	bool success;

	if (led_name == NULL) {
		success = false;
		goto done;
	}

	success =
		led_ops->deactivate_priority(
			led_ops_handle,
			led_name,
			led_priority,
			lock_id,
			deactivate_led_cb,
			response);

done:
	return success;
}

static bool
process_deactivate_attr(
	struct led_ops_st const * const led_ops,
	struct led_ops_handle_st * const led_ops_handle,
	struct blob_attr * const attr,
	struct blob_buf * const response)
{
	enum {
		LED_NAME,
		LED_PRIORITY,
		LOCK_ID,
		__LOCK_MAX
	};
	struct blobmsg_policy const deactivate_led_policy[__LOCK_MAX] = {
		[LED_NAME] =
		{ .name = _led_name, .type = BLOBMSG_TYPE_STRING },
		[LED_PRIORITY] =
		{ .name = _led_priority, .type = BLOBMSG_TYPE_STRING },
		[LOCK_ID] =
		{ .name = _led_lock_id, .type = BLOBMSG_TYPE_STRING }
	};
	struct blob_attr *fields[__LOCK_MAX];

	blobmsg_parse(deactivate_led_policy, ARRAY_SIZE(deactivate_led_policy), fields,
				  blobmsg_data(attr), blobmsg_data_len(attr));

	return process_deactivate_request(
		led_ops,
		led_ops_handle,
		blobmsg_get_string(fields[LED_NAME]),
		blobmsg_get_string(fields[LED_PRIORITY]),
		blobmsg_get_string(fields[LOCK_ID]),
		response);
}

enum {
	DEACTIVATE_LEDS,
	__DEACTIVATE_MAX
};

static struct blobmsg_policy const deactivate_policy[__DEACTIVATE_MAX] = {
	[DEACTIVATE_LEDS] = { .name = _led_leds, .type = BLOBMSG_TYPE_ARRAY }
};

static int
process_deactivate_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	int result;
	struct blob_attr *fields[__DEACTIVATE_MAX];
	struct led_ops_st const * const led_ops = ubus_context->led_ops;
	struct led_ops_handle_st * const led_ops_handle =
		led_ops->open(ubus_context->led_ops_context);

	if (led_ops_handle == NULL) {
		result = UBUS_STATUS_UNKNOWN_ERROR;
		goto done;
	}

	blobmsg_parse(deactivate_policy, ARRAY_SIZE(deactivate_policy), fields,
				  blobmsg_data(msg), blobmsg_len(msg));

	struct blob_attr * const array_blob = fields[DEACTIVATE_LEDS];

	if (!blobmsg_array_is_type(array_blob, BLOBMSG_TYPE_TABLE)) {
		/* The array needs to contain a set of tables, and it doesn't.*/
		result = UBUS_STATUS_INVALID_ARGUMENT;
		goto done;
	}

	void * const cookie = blobmsg_open_array(response, _led_leds);
	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, array_blob, rem) {
		if (!process_deactivate_attr(led_ops, led_ops_handle, cur, response)) {
			result = UBUS_STATUS_INVALID_ARGUMENT;
			goto done;
		}
	}

	blobmsg_close_array(response, cookie);

	result = UBUS_STATUS_OK;

done:
	led_ops->close(led_ops_handle);

	return result;
}

static int
deactivate_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);

	int const result = process_deactivate_msg(ubus_context, msg, &response);

	if (result == UBUS_STATUS_OK) {
		ubus_send_reply(ctx, req, response.head);
	}
	blob_buf_free(&response);

	return result;
}

static void
list_names_cb(
	char const * const led_name,
	char const * const led_colour,
	void * const result_context)
{
	struct blob_buf * const response = result_context;

	append_led_details(led_name, led_colour, response);
}

static void
process_list_names_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	UNUSED_ARG(msg);

	void * const cookie = blobmsg_open_array(response, _led_leds);
	struct led_ops_st const * const led_ops = ubus_context->led_ops;

	led_ops->list_leds(ubus_context->led_ops_context, list_names_cb, response);

	blobmsg_close_array(response, cookie);
}

static int
list_led_names_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);
	process_list_names_msg(ubus_context, msg, &response);
	ubus_send_reply(ctx, req, response.head);
	blob_buf_free(&response);

	return UBUS_STATUS_OK;
}


static void append_supported_state(
	enum led_state_t const state, void * const user_ctx)
{
	struct blob_buf * const response = user_ctx;

	blobmsg_add_string(response, NULL, led_state_name(state));
}

static void
process_list_supported_states_msg(
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	UNUSED_ARG(msg);

	void * const cookie = blobmsg_open_array(response, _led_supported_states);

	for (enum led_state_t state =
		 __LED_STATE_FIRST;
		 state < __LED_STATE_MAX;
		 state++) {
			append_supported_state(state, response);
	}

	blobmsg_close_array(response, cookie);
}

static int
list_supported_states_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct blob_buf response;

	blob_buf_full_init(&response, 0);
	process_list_supported_states_msg(msg, &response);
	ubus_send_reply(ctx, req, response.head);
	blob_buf_free(&response);

	return UBUS_STATUS_OK;
}

enum {
	PATTERN_PLAY_NAME,
	PATTERN_RETRIGGER,
	__PATTERN_PLAY_MAX
};

static struct blobmsg_policy const pattern_play_policy[__PATTERN_PLAY_MAX] = {
	[PATTERN_PLAY_NAME] =
	{.name = _led_pattern_name, .type = BLOBMSG_TYPE_STRING },
	[PATTERN_RETRIGGER] =
	{.name = _led_pattern_retrigger, .type = BLOBMSG_TYPE_BOOL }
};

static void play_pattern_result_cb(
	bool const success,
	char const * const error_msg,
	void * const result_context)
{
	struct blob_buf * const response = result_context;

	append_play_pattern_result(success, error_msg, response);
}

static int
process_pattern_play_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	struct blob_attr *fields[__PATTERN_PLAY_MAX];

	blobmsg_parse(pattern_play_policy, ARRAY_SIZE(pattern_play_policy), fields,
				  blobmsg_data(msg), blobmsg_len(msg));

	char const * const pattern_name =
		blobmsg_get_string(fields[PATTERN_PLAY_NAME]);
	bool const retrigger =
		blobmsg_get_bool_or_default(fields[PATTERN_RETRIGGER], false);
	struct led_ops_st const * const led_ops = ubus_context->led_ops;

	led_ops->play_pattern(
		ubus_context->led_ops_context,
		pattern_name,
		retrigger,
		play_pattern_result_cb,
		response);

	return UBUS_STATUS_OK;
}

static int
pattern_play_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);

	int const result = process_pattern_play_msg(ubus_context, msg, &response);

	if (result == UBUS_STATUS_OK) {
		ubus_send_reply(ctx, req, response.head);
	}
	blob_buf_free(&response);

	return result;
}

enum {
	PATTERN_STOP_NAME,
	__PATTERN_STOP_MAX
};

static struct blobmsg_policy const pattern_stop_policy[__PATTERN_STOP_MAX] = {
	[PATTERN_STOP_NAME] =
	{.name = _led_pattern_name, .type = BLOBMSG_TYPE_STRING }
};

static int
process_pattern_stop_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	struct blob_attr *fields[__PATTERN_STOP_MAX];

	blobmsg_parse(pattern_stop_policy, ARRAY_SIZE(pattern_stop_policy), fields,
				  blobmsg_data(msg), blobmsg_len(msg));

	char const * const pattern_name =
		blobmsg_get_string(fields[PATTERN_STOP_NAME]);
	struct led_ops_st const * const led_ops = ubus_context->led_ops;

	led_ops->stop_pattern(
		ubus_context->led_ops_context,
		pattern_name,
		play_pattern_result_cb,
		response);

	return UBUS_STATUS_OK;
}

static int
pattern_stop_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);

	int const result = process_pattern_stop_msg(ubus_context, msg, &response);

	if (result == UBUS_STATUS_OK) {
		ubus_send_reply(ctx, req, response.head);
	}
	blob_buf_free(&response);

	return result;
}

static void
append_pattern_cb(char const * const pattern_name, void * const user_ctx)
{
	struct blob_buf * const response = user_ctx;

	blobmsg_add_string(response, NULL, pattern_name);
}

static void
process_pattern_list_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	UNUSED_ARG(msg);

	void * const cookie = blobmsg_open_array(response, _led_patterns);
	struct led_ops_st const * const led_ops = ubus_context->led_ops;

	led_ops->list_patterns(
		ubus_context->led_ops_context, append_pattern_cb, response);

	blobmsg_close_array(response, cookie);
}

static int
pattern_list_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);
	process_pattern_list_msg(ubus_context, msg, &response);
	ubus_send_reply(ctx, req, response.head);
	blob_buf_free(&response);

	return UBUS_STATUS_OK;
}

static void
process_pattern_list_playing_msg(
	struct ledcmd_ubus_context_st * const ubus_context,
	struct blob_attr const * const msg,
	struct blob_buf * const response)
{
	UNUSED_ARG(msg);

	void * const cookie = blobmsg_open_array(response, _led_patterns);
	struct led_ops_st const * const led_ops = ubus_context->led_ops;

	led_ops->list_playing_patterns(
		ubus_context->led_ops_context, append_pattern_cb, response);

	blobmsg_close_array(response, cookie);
}

static int
pattern_list_playing_handler(
	struct ubus_context * const ctx,
	struct ubus_object * const obj,
	struct ubus_request_data * const req,
	char const * const method,
	struct blob_attr * const msg)
{
	UNUSED_ARG(obj);
	UNUSED_ARG(method);

	struct ledcmd_ubus_context_st * const ubus_context =
		container_of(ctx, struct ledcmd_ubus_context_st, ubus_connection.context);
	struct blob_buf response;

	blob_buf_full_init(&response, 0);
	process_pattern_list_playing_msg(ubus_context, msg, &response);
	ubus_send_reply(ctx, req, response.head);
	blob_buf_free(&response);

	return UBUS_STATUS_OK;
}

static struct ubus_method const ledd_methods[] = {
	UBUS_METHOD(_led_get, get_state_handler, get_state_policy),
	UBUS_METHOD(_led_set, set_state_handler, set_state_policy),
	UBUS_METHOD_NOARG(_led_list, list_led_names_handler),
	UBUS_METHOD_NOARG(_led_list_supported_states, list_supported_states_handler),
	UBUS_METHOD(_led_activate, activate_handler, activate_policy),
	UBUS_METHOD(_led_deactivate, deactivate_handler, deactivate_policy),
	UBUS_METHOD(_led_pattern_play, pattern_play_handler, pattern_play_policy),
	UBUS_METHOD(_led_pattern_stop, pattern_stop_handler, pattern_stop_policy),
	UBUS_METHOD_NOARG(_led_pattern_list, pattern_list_handler),
	UBUS_METHOD_NOARG(_led_pattern_list_playing, pattern_list_playing_handler)
};

static struct ubus_object_type ledd_object_type =
	UBUS_OBJECT_TYPE(_led_ledcmd, ledd_methods);

static struct ubus_object ledd_object = {
	.name = _led_ledcmd,
	.type = &ledd_object_type,
	.methods = ledd_methods,
	.n_methods = ARRAY_SIZE(ledd_methods),
};

static void
ubus_reconnected(struct ubus_connection_ctx_st * const connection_context)
{
	struct ubus_context * const ubus_ctx = &connection_context->context;

	log_info("Reconnected to ubus");
	ubus_add_uloop(ubus_ctx);
}

static void
ubus_connected(struct ubus_connection_ctx_st * const connection_context)
{
	struct ubus_context * const ubus_ctx = &connection_context->context;

	log_info("Connected to ubus");
	ubus_add_object(ubus_ctx, &ledd_object);
	ubus_add_uloop(ubus_ctx);
}

void
ledcmd_ubus_deinit(
	struct ledcmd_ubus_context_st * const ledcmd_ubus_context)
{
	if (ledcmd_ubus_context == NULL) {
		goto done;
	}

	ubus_connection_shutdown(&ledcmd_ubus_context->ubus_connection);
	free(ledcmd_ubus_context);

done:
	return;
}

struct ledcmd_ubus_context_st *
ledcmd_ubus_init(
	char const * const ubus_path,
	struct led_ops_st const * const led_ops,
	void * const led_ops_context)
{
	struct ledcmd_ubus_context_st * const ledcmd_ubus_context =
		calloc(1, sizeof *ledcmd_ubus_context);

	if (ledcmd_ubus_context == NULL) {
		goto done;
	}

	ledcmd_ubus_context->led_ops = led_ops;
	ledcmd_ubus_context->led_ops_context = led_ops_context;

	ubus_connection_init(
		&ledcmd_ubus_context->ubus_connection,
		ubus_path,
		ubus_connected,
		ubus_reconnected);

done:
	return ledcmd_ubus_context;
}


