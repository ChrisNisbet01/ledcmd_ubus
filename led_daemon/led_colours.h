#ifndef __LED_COLOURS_H__
#define __LED_COLOURS_H__

enum led_colour_t {
	LED_COLOUR_UNKNOWN,
	LED_COLOUR_RED,
	LED_COLOUR_GREEN,
	LED_COLOUR_BLUE,
	LED_COLOUR_YELLOW,
	__LED_COLOUR_MAX,
};

char const *
led_colour_by_type(enum led_colour_t colour);

#endif /* __LED_COLOURS_H__ */
