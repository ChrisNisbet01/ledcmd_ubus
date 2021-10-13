#include "led_priority_context.h"
#include "priorities.h"

enum led_priority_t
led_priority_highest_priority(led_priority_st const * const priority_context)
{
    return priority_context_highest_priority(
        (priority_context_st *)priority_context);
}

bool
led_priority_priority_is_active(
    led_priority_st const * const priority_context, enum led_priority_t const priority)
{
    return priority_context_priority_is_active(
        (priority_context_st *)priority_context, priority);
}


enum led_priority_t
led_priority_priority_activate(
    led_priority_st * const priority_context, enum led_priority_t const priority)
{
    return priority_context_priority_activate(
        (priority_context_st *)priority_context, priority);
}

enum led_priority_t
led_priority_priority_deactivate(
    led_priority_st * const priority_context, enum led_priority_t const priority)
{
    /* The lowest priority can't be deactivated. */
    enum led_priority_t const highest_priority =
        (priority == LED_PRIORITY_NORMAL)
        ? priority_context_highest_priority(
            (priority_context_st *)priority_context)
        : priority_context_priority_deactivate(
            (priority_context_st *)priority_context, priority);

    return highest_priority;
}

void
led_priority_free(led_priority_st * const priority_context)
{
    priority_context_free((priority_context_st *)priority_context);
}

led_priority_st *
led_priority_allocate(size_t const num_priorities)
{
    led_priority_st * const priority_context =
        (led_priority_st *)priority_context_allocate(num_priorities);

    if (priority_context == NULL){
        goto done;
    }

    /* The lowest priority is always active. */
    priority_context_priority_activate(
        (priority_context_st *)priority_context, LED_PRIORITY_NORMAL);

done:
    return priority_context;
}

