#include "lib_led_pattern.h"
#include "lib_led_private.h"
#include "string_constants.h"

#include <libubus_utils/ubus_utils.h>

static bool
send_ubus_led_pattern_request(
	struct ledcmd_ctx_st const * const ctx,
	char const * const cmd,
	ubus_cb const cb,
	void * const cb_context)
{
	struct blob_buf msg;

	blob_buf_full_init(&msg, 0);

	bool const success = ledcmd_ubus_invoke(cmd, &msg, cb, cb_context, ctx);

	blob_buf_free(&msg);

	return success;
}

struct ledcmd_led_pattern_ctx_st {
	bool success;
	led_list_patterns_cb const cb;
	void * const cb_context;
};

static void
process_led_pattern_response(
	struct blob_attr const * const array_blob,
	struct ledcmd_led_pattern_ctx_st * const ctx)
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
led_list_pattern_response_handler(
	struct ubus_request * const req,
	int type, struct blob_attr * const response)
{
	UNUSED_ARG(type);

	struct ledcmd_led_pattern_ctx_st * const ctx = req->priv;
	enum {
		PATTERN_RESPONSE,
		__PATTERN_MAX
	};
	struct blob_attr *fields[__PATTERN_MAX];
	struct blobmsg_policy const pattern_reply_policy[] = {
		[PATTERN_RESPONSE] = {.name = _led_patterns, .type = BLOBMSG_TYPE_ARRAY}
	};

	blobmsg_parse(
		pattern_reply_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	process_led_pattern_response(fields[PATTERN_RESPONSE], ctx);
}

bool
led_list_patterns(
	struct ledcmd_ctx_st const * const ledcmd_ctx,
	led_list_patterns_cb const cb,
	void * const cb_context)
{
	struct ledcmd_led_pattern_ctx_st ctx = {
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};

	send_ubus_led_pattern_request(
		ledcmd_ctx,
		_led_pattern_list,
		led_list_pattern_response_handler,
		&ctx);

	return ctx.success;
}

bool
led_list_playing_patterns(
	struct ledcmd_ctx_st const * const ledcmd_ctx,
	led_list_patterns_cb const cb,
	void * const cb_context)
{
	struct ledcmd_led_pattern_ctx_st ctx = {
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};

	send_ubus_led_pattern_request(
		ledcmd_ctx,
		_led_pattern_list_playing,
		led_list_pattern_response_handler,
		&ctx);

	return ctx.success;
}

static bool
send_ubus_led_play_pattern_request(
	struct ledcmd_ctx_st const * const ctx,
	char const * const cmd,
	char const * const pattern_name,
	bool const retrigger,
	ubus_cb const cb,
	void * const cb_context)
{
	struct blob_buf msg;

	blob_buf_full_init(&msg, 0);

	blobmsg_add_string(&msg, _led_pattern_name, pattern_name);
	blobmsg_add_u8(&msg, _led_pattern_retrigger, retrigger);

	bool const success = ledcmd_ubus_invoke(cmd, &msg, cb, cb_context, ctx);

	blob_buf_free(&msg);

	return success;
}

struct ledcmd_led_play_pattern_ctx_st {
	bool success;
	led_play_pattern_cb const cb;
	void * const cb_context;
};

static void
led_play_pattern_response_handler(
	struct ubus_request * const req,
	int type, struct blob_attr * const response)
{
	UNUSED_ARG(type);

	struct ledcmd_led_play_pattern_ctx_st * const ctx = req->priv;
	enum {
		PLAY_SUCCESS,
		PLAY_ERROR_MSG,
		__PLAY_MAX
	};
	struct blob_attr *fields[__PLAY_MAX];
	struct blobmsg_policy const pattern_reply_policy[] = {
		[PLAY_SUCCESS] = {.name = _led_success, .type = BLOBMSG_TYPE_BOOL},
		[PLAY_ERROR_MSG] = {.name = _led_success, .type = BLOBMSG_TYPE_STRING}
	};

	blobmsg_parse(
		pattern_reply_policy, ARRAY_SIZE(fields), fields,
		blobmsg_data(response), blobmsg_len(response));

	bool const success =
		blobmsg_get_bool_or_default(fields[PLAY_SUCCESS], false);
	char const * const error_msg = blobmsg_get_string(fields[PLAY_ERROR_MSG]);

	ctx->cb(success, error_msg, ctx->cb_context);
}

bool
led_play_pattern(
	struct ledcmd_ctx_st const * ledcmd_ctx,
	char const * pattern_name,
	bool retrigger,
	led_play_pattern_cb cb,
	void * cb_context)
{
	struct ledcmd_led_play_pattern_ctx_st ctx = {
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};

	send_ubus_led_play_pattern_request(
		ledcmd_ctx,
		_led_pattern_play,
		pattern_name,
		retrigger,
		led_play_pattern_response_handler,
		&ctx);

	return ctx.success;
}

bool
led_stop_pattern(
	struct ledcmd_ctx_st const * ledcmd_ctx,
	char const * pattern_name,
	led_play_pattern_cb cb,
	void * cb_context)
{
	struct ledcmd_led_play_pattern_ctx_st ctx = {
		.success = false,
		.cb = cb,
		.cb_context = cb_context
	};

	send_ubus_led_play_pattern_request(
		ledcmd_ctx,
		_led_pattern_stop,
		pattern_name,
		false,
		led_play_pattern_response_handler,
		&ctx);

	return ctx.success;
}

