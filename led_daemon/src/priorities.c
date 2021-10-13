#include "priorities.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define BIT(x) (1 << (x))
#define BITS_PER_BYTE 8

typedef uint8_t priority_t;

struct priority_context_st
{
    size_t num_priorities;
    size_t ready_table_size;
    priority_t ready_group;
    priority_t ready_table[]; /* Must be the last field in the structure. */
};

static priority_t const priority_map_table[] = {
    BIT(0), BIT(1), BIT(2), BIT(3), BIT(4), BIT(5), BIT(6), BIT(7)
};

static uint8_t const priority_unmap_table[] = {
    0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

static bool
priority_is_valid(
    struct priority_context_st const * const priority_context,
    enum priority_values_t const priority)
{
    bool const is_valid =
        priority < priority_context->num_priorities
        && priority < PRIORITY_LIMIT;

    return is_valid;
}

static void
priority_context_init(struct priority_context_st * const priority_context)
{
    priority_context->ready_group = 0;
    for (size_t i = 0; i < priority_context->ready_table_size; i++)
    {
        priority_context->ready_table[i] = 0;
    }
}

enum priority_values_t
priority_context_highest_priority(
    struct priority_context_st const * const priority_context)
{
    /* Unmap the ready group to determine the highest priority group number. */
    uint8_t const y = priority_unmap_table[priority_context->ready_group];
    /* Then determine the highest priority in that group. */
    uint8_t const x = priority_unmap_table[priority_context->ready_table[y]];
    /*
     * There are 8 priorities per group, so the final priority is
     * (group number * 8) + bit number.
     */
    enum priority_values_t const highest_reservation = (y << 3) + x;

    return highest_reservation;
}

bool
priority_context_priority_is_active(
    struct priority_context_st const * const priority_context,
    enum priority_values_t const priority)
{
    bool is_active;

    if (!priority_is_valid(priority_context, priority))
    {
        is_active = false;
        goto done;
    }

    uint8_t const x = priority & 0x07;
    priority_t const bitx = priority_map_table[x];
    uint8_t const y = priority >> 3;

    is_active = (priority_context->ready_table[y] & bitx) != 0;

done:
    return is_active;
}


enum priority_values_t
priority_context_priority_activate(
    struct priority_context_st * const priority_context,
    enum priority_values_t const priority)
{
    if (!priority_is_valid(priority_context, priority))
    {
        goto done;
    }

    uint8_t const x = priority & 0x07;
    uint8_t const y = priority >> 3;
    priority_t const bity = priority_map_table[y];
    priority_t const bitx = priority_map_table[x];

    priority_context->ready_group |= bity;
    priority_context->ready_table[y] |= bitx;

done:
    return priority_context_highest_priority(priority_context);
}

enum priority_values_t
priority_context_priority_deactivate(
    struct priority_context_st * const priority_context,
    enum priority_values_t const priority)
{
    if (!priority_is_valid(priority_context, priority))
    {
        goto done;
    }

    uint8_t const x = priority & 0x07;
    uint8_t const y = priority >> 3;
    priority_t const bity = priority_map_table[y];
    priority_t const bitx = priority_map_table[x];

    priority_context->ready_table[y] &= ~bitx;
    if (priority_context->ready_table[y] == 0)
    {
        priority_context->ready_group &= ~bity;
    }

done:
    return priority_context_highest_priority(priority_context);
}

void
priority_context_free(priority_context_st * const priority_context)
{
    free(priority_context);
}

priority_context_st *
priority_context_allocate(size_t const num_priorities)
{
    priority_context_st * priority_context;

    if (num_priorities == 0 || num_priorities >= PRIORITY_LIMIT)
    {
        priority_context = NULL;
        goto done;
    }
    size_t const bits_per_priority_t = sizeof(priority_t) * BITS_PER_BYTE;
    size_t const priority_table_count =
        (num_priorities + (bits_per_priority_t - 1)) / bits_per_priority_t;
    size_t const priority_table_size =
        priority_table_count * sizeof(priority_t);
    size_t required_size = sizeof *priority_context + priority_table_size;

        priority_context = calloc(1, required_size);
    if (priority_context == NULL)
    {
        goto done;
    }

    priority_context->num_priorities = num_priorities;
    priority_context->ready_table_size = priority_table_size;

    priority_context_init(priority_context);

done:
    return priority_context;
}
