#ifndef PRIORITIES_H__
#define PRIORITIES_H__

#include <stddef.h>
#include <stdbool.h>

typedef struct priority_context_st priority_context_st;

enum priority_compare_t
{
    PRIORITY_LESS = -1,
    PRIORITY_EQUAL,
    PRIORITY_GREATER
};

/* Note that a lower value represents a higher priority. */
enum priority_values_t
{
    PRIORITY_HIGHEST = 0,
    PRIORITY_LIMIT = 64 /* And may not be used. */
};

static __inline__ enum priority_compare_t priority_compare(
    enum priority_values_t const priority_a,
    enum priority_values_t const priority_b)
{
    enum priority_compare_t const comparison =
        (priority_a < priority_b)
        ? PRIORITY_GREATER
        : (priority_a > priority_b) ? PRIORITY_LESS : PRIORITY_EQUAL;

    return comparison;
}

enum priority_values_t
priority_context_highest_priority(
    struct priority_context_st const * priority_context);

bool
priority_context_priority_is_active(
    struct priority_context_st const * priority_context,
    enum priority_values_t priority);

/*
 * Activate a priority.
 * Returns the highest activated priority.
 */
enum priority_values_t
priority_context_priority_activate(
    struct priority_context_st * priority_context,
    enum priority_values_t priority);

/*
 * Deactivate a priority.
 * Returns the highest activated priority.
 */
enum priority_values_t
priority_context_priority_deactivate(
    struct priority_context_st * priority_context,
    enum priority_values_t priority);

void
priority_context_free(priority_context_st * const priority_context);

priority_context_st *
priority_context_allocate(size_t const num_priorities);

#endif /* PRIORITIES_H__ */

