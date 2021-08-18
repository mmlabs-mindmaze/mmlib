/*
 * @mindmaze_header@
 */
#ifndef ATOMIC_WIN32_H
#define ATOMIC_WIN32_H

#include <intrin.h>
#include <stdbool.h>

/**************************************************************************
 *                                                                        *
 *                 Define set of atomic macro/function                    *
 *                                                                        *
 **************************************************************************/
/**
 * atomic_cmp_exchange() - atomically swap if expected value is in object
 * @obj:        pointer of int64_t to change
 * @expected:   pointer to expected value in input, receive the previous value
 *              of *@obj in output
 * @desired:    new value that must be swapped into @obj
 *
 * Return: If @expected equal @obj, the swap occurs and true is returned.
 * Otherwise false.
 */
static inline
bool atomic_cmp_exchange(volatile int64_t* obj,
                         int64_t* expected,
                         int64_t desired)
{
	int64_t prev_val;
	bool has_swapped;

	prev_val = _InterlockedCompareExchange64(obj, desired, *expected);
	has_swapped = (*expected == prev_val) ? true : false;

	*expected = prev_val;
	return has_swapped;
}

// Same as atomic_fetch_add() of C11's stdatomic.h
#ifndef atomic_fetch_add
#define atomic_fetch_add(obj, arg)  (_InterlockedExchangeAdd64(obj, arg))
#endif

// Same as atomic_fetch_sub() of C11's stdatomic.h
#ifndef atomic_fetch_sub
#define atomic_fetch_sub(obj, arg)  (_InterlockedExchangeAdd64(obj, -arg))
#endif

// Same as atomic_fetch_sub() but return value is ignored
#define atomic_sub(obj, arg)        ((void)_InterlockedExchangeAdd64(obj, -arg))

// Same as atomic_load() of C11's stdatomic.h
#ifndef atomic_load
#define atomic_load(obj)            (*obj)
#endif

#endif /* ifndef ATOMIC_WIN32_H */
