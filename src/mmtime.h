/*
 * @mindmaze_header@
 */
#ifndef MMTIME_H
#define MMTIME_H

#include <time.h>
#include <stdint.h>
#include "mmpredefs.h"

/**
 * DOC: clock types
 *
 * MM_CLK_REALTIME
 *   system-wide clock measuring real time. This clock is affected by
 *   discontinuous jumps in the system time (e.g., if the system
 *   administrator manually changes the clock), and by the incremental
 *   adjustments performed by NTP.
 *
 * MM_CLK_MONOTONIC
 *   represents monotonic time since some unspecified starting point.  This
 *   clock is not affected by discontinuous jumps in the system  time (e.g.,
 *   if the system administrator manually changes the clock), but it may by
 *   affected by the incremental adjustments performed by NTP. In other
 *   word, this clock may be sped up/slowed down by the kernel as necessary
 *   to match real time through NTP to ensure that 1s with MM_CLK_MONOTONIC is
 *   really 1s.
 *
 * MM_CLK_CPU_PROCESS
 *   Per-process CPU-time clock (measures CPU time consumed by all threads
 *   in the process).
 *
 * MM_CLK_CPU_THREAD
 *   Thread-specific CPU-time clock.
 *
 * MM_CLK_MONOTONIC_RAW
 *   Similar to MM_CLK_MONOTONIC, but provides access to a raw
 *   hardware-based time that is not subject to NTP adjustments. On modern
 *   CPU, this clock is often based on the cycle counter of the CPU (when a
 *   reliable one is available) which makes it a good basis for code
 *   profile.
 */
#ifndef _WIN32

#  define MM_CLK_REALTIME    CLOCK_REALTIME
#  define MM_CLK_MONOTONIC   CLOCK_MONOTONIC
#  define MM_CLK_CPU_PROCESS CLOCK_PROCESS_CPUTIME_ID
#  define MM_CLK_CPU_THREAD  CLOCK_THREAD_CPUTIME_ID

#  ifdef CLOCK_MONOTONIC_RAW
#    define MM_CLK_MONOTONIC_RAW CLOCK_MONOTONIC_RAW
#  else
#    define MM_CLK_MONOTONIC_RAW CLOCK_MONOTONIC
#  endif

#else // WIN32

#  define MM_CLK_REALTIME      0
#  define MM_CLK_MONOTONIC     1
#  define MM_CLK_CPU_PROCESS   2
#  define MM_CLK_CPU_THREAD    3
#  define MM_CLK_MONOTONIC_RAW 4

#endif /* ifndef _WIN32 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
typedef int clockid_t;
#endif

MMLIB_API int mm_gettime(clockid_t clock_id, struct timespec * ts);
MMLIB_API int mm_getres(clockid_t clock_id, struct timespec * res);
MMLIB_API int mm_nanosleep(clockid_t clock_id, const struct timespec * ts);
MMLIB_API int mm_relative_sleep_ms(int64_t duration_ms);
MMLIB_API int mm_relative_sleep_us(int64_t duration_us);
MMLIB_API int mm_relative_sleep_ns(int64_t duration_ns);

/**************************************************************************
 *                     Timespec manipulation helpers                      *
 **************************************************************************/
#ifndef NS_IN_SEC
#define NS_IN_SEC 1000000000
#endif

#ifndef US_IN_SEC
#define US_IN_SEC 1000000
#endif

#ifndef MS_IN_SEC
#define MS_IN_SEC 1000
#endif

/**
 * mm_timediff_ns() - compute time difference in nanoseconds
 * @ts:         time point
 * @orig:       time reference
 *
 * Return: the interval @ts - @orig in nanoseconds
 */
static inline
int64_t mm_timediff_ns(const struct timespec* ts,
                       const struct timespec* orig)
{
	int64_t dt;

	dt = (ts->tv_sec - orig->tv_sec) * (int64_t)NS_IN_SEC;
	dt += ts->tv_nsec - orig->tv_nsec;

	return dt;
}


/**
 * mm_timediff_us() - compute time difference in microseconds
 * @ts:         time point
 * @orig:       time reference
 *
 * Return: the interval @ts - @orig in microseconds
 */
static inline
int64_t mm_timediff_us(const struct timespec* ts,
                       const struct timespec* orig)
{
	int64_t dt;

	dt = (ts->tv_sec - orig->tv_sec) * (int64_t)US_IN_SEC;
	dt += (ts->tv_nsec - orig->tv_nsec) / (NS_IN_SEC/US_IN_SEC);

	return dt;
}


/**
 * mm_timediff_ms() - compute time difference in milliseconds
 * @ts:         time point
 * @orig:       time reference
 *
 * Return: the interval @ts - @orig in milliseconds
 */
static inline
int64_t mm_timediff_ms(const struct timespec* ts,
                       const struct timespec* orig)
{
	int64_t dt;

	dt = (ts->tv_sec - orig->tv_sec) * (int64_t)MS_IN_SEC;
	dt += (ts->tv_nsec - orig->tv_nsec) / (NS_IN_SEC/MS_IN_SEC);

	return dt;
}


/**
 * mm_timeadd_ns() - apply nanosecond offset to timestamp
 * @ts:         time point
 * @dt:         offset in nanoseconds to apply to @ts
 */
static inline
void mm_timeadd_ns(struct timespec* ts, int64_t dt)
{
	ts->tv_sec += dt / NS_IN_SEC;
	ts->tv_nsec += dt % NS_IN_SEC;

	if (ts->tv_nsec >= NS_IN_SEC) {
		ts->tv_nsec -= NS_IN_SEC;
		ts->tv_sec++;
	} else if (ts->tv_nsec < 0) {
		ts->tv_nsec += NS_IN_SEC;
		ts->tv_sec--;
	}
}


/**
 * mm_timeadd_us() - apply microsecond offset to timestamp
 * @ts:         time point
 * @dt:         offset in microseconds to apply to @ts
 */
static inline
void mm_timeadd_us(struct timespec* ts, int64_t dt)
{
	ts->tv_sec += dt / US_IN_SEC;
	ts->tv_nsec += (dt % US_IN_SEC) * (NS_IN_SEC / US_IN_SEC);

	if (ts->tv_nsec >= NS_IN_SEC) {
		ts->tv_nsec -= NS_IN_SEC;
		ts->tv_sec++;
	} else if (ts->tv_nsec < 0) {
		ts->tv_nsec += NS_IN_SEC;
		ts->tv_sec--;
	}
}


/**
 * mm_timeadd_ms() - apply millisecond offset to timestamp
 * @ts:         time point
 * @dt:         offset in milliseconds to apply to @ts
 */
static inline
void mm_timeadd_ms(struct timespec* ts, int64_t dt)
{
	ts->tv_sec += dt / MS_IN_SEC;
	ts->tv_nsec += (dt % MS_IN_SEC) * (NS_IN_SEC / MS_IN_SEC);

	if (ts->tv_nsec >= NS_IN_SEC) {
		ts->tv_nsec -= NS_IN_SEC;
		ts->tv_sec++;
	} else if (ts->tv_nsec < 0) {
		ts->tv_nsec += NS_IN_SEC;
		ts->tv_sec--;
	}
}

#ifdef __cplusplus
}
#endif

#endif /* ifndef MMTIME_H */
