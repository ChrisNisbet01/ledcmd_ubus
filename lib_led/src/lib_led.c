#include "lib_led.h"
#include "lib_led_private.h"
#include "string_constants.h"

#include <ubus_utils/ubus_utils.h>
#include <libubox/blobmsg_json.h>

#include <stdlib.h>
#include <string.h>

struct ledcmd_ctx_st {
	struct ubus_context * ubus_ctx;
	uint32_t ledcmd_ubus_id;
};

bool
ledcmd_ubus_invoke(
	char const * const cmd,
	struct blob_buf * const msg,
	ubus_cb const cb,
	void * const cb_context,
	struct ledcmd_ctx_st const * const ledcmd_ctx)
{
	int const ubus_req_timeout_ms = 1000;

	return ubus_invoke(
		ledcmd_ctx->ubus_ctx,
		ledcmd_ctx->ledcmd_ubus_id,
		cmd,
		msg->head,
		cb,
		cb_context,
		ubus_req_timeout_ms) == UBUS_STATUS_OK;
}

void
led_deinit(struct ledcmd_ctx_st * const ctx)
{
    free(ctx);
}

struct ledcmd_ctx_st *
led_init(struct ubus_context * const ubus_ctx)
{
	bool success;
	struct ledcmd_ctx_st * ctx = calloc(1, sizeof *ctx);

	if (ctx == NULL) {
		success = false;
		goto done;
	}

	ctx->ubus_ctx = ubus_ctx;

	success =
		ubus_lookup_id(
			ctx->ubus_ctx, _led_ledcmd, &ctx->ledcmd_ubus_id) == UBUS_STATUS_OK;

done:
	if (!success) {
		led_deinit(ctx);
		ctx = NULL;
	}

	return ctx;
}

