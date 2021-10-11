#ifndef __LIB_LED_H__
#define __LIB_LED_H__

#include <libubus.h>

typedef struct ledcmd_ctx_st ledcmd_ctx_st;

ledcmd_ctx_st *
led_init(struct ubus_context * ubus_ctx);

void
led_deinit(ledcmd_ctx_st *ctx);

#endif /* __LIB_LED_H__ */
