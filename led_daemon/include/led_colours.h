#ifndef LED_COLOURS_H__
#define LED_COLOURS_H__

enum led_colour_t
{
    LED_COLOUR_UNKNOWN,
    LED_COLOUR_RED,
    LED_COLOUR_GREEN,
    LED_COLOUR_BLUE,
    LED_COLOUR_YELLOW,
    LED_COLOUR_MAX,
};

char const *
led_colour_by_type(enum led_colour_t colour);

#endif /* LED_COLOURS_H__ */

