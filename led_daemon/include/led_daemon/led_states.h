#ifndef LED_STATES_H__
#define LED_STATES_H__

enum led_state_t
{
    LED_STATE_UNKNOWN,
    LED_STATE_FIRST,
    LED_OFF = LED_STATE_FIRST,
    LED_ON,
    LED_SLOW_FLASH,
    LED_FAST_FLASH,
    LED_STATE_MAX
};

/*
 * For some reason the legacy ledcmd application printed the supported
 * state strings one way (i.e. with the '-L' option),
 * but printed the state with different text with the '-q' option.
 */

char const *
led_state_name(enum led_state_t state);

char const *
led_state_query_name(enum led_state_t state);

enum led_state_t
led_state_by_query_name(char const * name);

#endif /* LED_STATES_H__ */

