#ifndef __LIB_LED_PRIVATE_H__
#define __LIB_LED_PRIVATE_H__

#include <libubus.h>

typedef struct ledcmd_ctx_st ledcmd_ctx_st;

typedef void (*ubus_cb)
	(struct ubus_request *req, int type, struct blob_attr *response);

bool
ledcmd_ubus_invoke(
    char const * const cmd,
    struct blob_buf * const msg,
    ubus_cb const cb,
    void * const cb_context,
    struct ledcmd_ctx_st const * const ledcmd_ctx);

#endif /* __LIB_LED_PRIVATE_H__ */
