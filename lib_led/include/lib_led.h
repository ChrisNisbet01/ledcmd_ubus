#ifndef __LIB_LED_H__
#define __LIB_LED_H__

typedef struct ledcmd_ctx_st ledcmd_ctx_st;

ledcmd_ctx_st const *
led_init(void);

void
led_deinit(ledcmd_ctx_st const *ctx);

#endif /* __LIB_LED_H__ */
