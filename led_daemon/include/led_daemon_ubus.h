#ifndef LED_DAEMON_UBUS_H__
#define LED_DAEMON_UBUS_H__

#include "led_control.h"

#include <ubus_utils/ubus_connection.h>

#include <stdbool.h>

typedef struct ledcmd_ubus_context_st ledcmd_ubus_context_st;

ledcmd_ubus_context_st *
ledcmd_ubus_init(
    char const * const ubus_path,
    struct led_ops_st const * const led_ops,
    void * const led_ops_context);

void
ledcmd_ubus_deinit(ledcmd_ubus_context_st * ledcmd_ubus_context);

#endif /* LED_DAEMON_UBUS_H__ */

