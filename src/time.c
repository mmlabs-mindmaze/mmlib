/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmtime.h"
#include "mmerrno.h"


/**
 * mm_relative_sleep_ns() - relative sleep in nanoseconds
 * @duration_ns:        duration of sleep in nanoseconds
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
API_EXPORTED
int mm_relative_sleep_ns(int64_t duration_ns)
{
	struct timespec ts;

	mm_gettime(MM_CLK_MONOTONIC, &ts);
	mm_timeadd_ns(&ts, duration_ns);

	return mm_nanosleep(MM_CLK_MONOTONIC, &ts);
}


/**
 * mm_relative_sleep_us() - relative sleep in microseconds
 * @duration_us:        duration of sleep in microseconds
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
API_EXPORTED
int mm_relative_sleep_us(int64_t duration_us)
{
	struct timespec ts;

	mm_gettime(MM_CLK_MONOTONIC, &ts);
	mm_timeadd_us(&ts, duration_us);

	return mm_nanosleep(MM_CLK_MONOTONIC, &ts);
}


/**
 * mm_relative_sleep_ms() - relative sleep in milliseconds
 * @duration_ms:        duration of sleep in milliseconds
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
API_EXPORTED
int mm_relative_sleep_ms(int64_t duration_ms)
{
	struct timespec ts;

	mm_gettime(MM_CLK_MONOTONIC, &ts);
	mm_timeadd_ms(&ts, duration_ms);

	return mm_nanosleep(MM_CLK_MONOTONIC, &ts);
}
