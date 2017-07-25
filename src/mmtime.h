/*
   @mindmaze_header@
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

#  define MM_CLK_REALTIME         CLOCK_REALTIME
#  define MM_CLK_MONOTONIC        CLOCK_MONOTONIC
#  define MM_CLK_CPU_PROCESS      CLOCK_PROCESS_CPUTIME_ID
#  define MM_CLK_CPU_THREAD       CLOCK_THREAD_CPUTIME_ID

#  ifdef CLOCK_MONOTONIC_RAW
#    define MM_CLK_MONOTONIC_RAW  CLOCK_MONOTONIC_RAW
#  else
#    define MM_CLK_MONOTONIC_RAW  CLOCK_MONOTONIC
#  endif

#else // WIN32

#  define MM_CLK_REALTIME              0
#  define MM_CLK_MONOTONIC             1
#  define MM_CLK_CPU_PROCESS           2
#  define MM_CLK_CPU_THREAD            3
#  define MM_CLK_MONOTONIC_RAW         4

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
typedef int clockid_t;
#endif

/**
 * mm_gettime() - Get clock value
 * @clock_id:   ID clock type (one of the MM_CLK_* value)
 * @ts:         location that must receive the clock value
 *
 * This function get the current value @ts for the specified clock @clock_id.
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
MMLIB_API int mm_gettime(clockid_t clock_id, struct timespec *ts);


/**
 * mm_getres() - Get clock resolution
 * @clock_id:   ID clock type (one of the MM_CLK_* value)
 * @res:        location that must receive the clock resolution
 *
 * This function get the resolution of the specified clock @clock_id. The
 * resolution of the specified clock shall be stored in the location pointed
 * to by @res.
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 *
 * NOTE:
 * The resolution of a clock is the minimal time difference that a clock can
 * observe. If two measure points are taken closer than the resolution step,
 * the difference between the 2 reported values will be either 0 or the
 * resolution step. The resolution must not be confused with the accuracy
 * which refers to how much the sytem deviates from the truth. The accuracy
 * of a system can never exceed its resolution! However it is possible to a
 * accuracy much worse than its resolution.
 */
MMLIB_API int mm_getres(clockid_t clock_id, struct timespec *res);


/**
 * mm_nanosleep() - Absolute time sleep with specifiable clock
 * @clock_id:   ID clock type (one of the MM_CLK_* value)
 * @ts:         absolute time when execution must resume
 *
 * This function cause the current thread to be suspended from execution
 * until either the time value of the clock specified by @clock_id reaches
 * the absolute time specified by the @ts argument. If, at the time of the
 * call, the time value specified by @ts is less than or equal to the time
 * value of the specified clock, then mm_nanosleep() shall return
 * immediately and the calling process shall not be suspended.
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
MMLIB_API int mm_nanosleep(clockid_t clock_id, const struct timespec *ts);


/**
 * mm_relative_sleep_ms() - relative sleep in milliseconds
 * @duration_ms:        duration of sleep in milliseconds
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
MMLIB_API int mm_relative_sleep_ms(int64_t duration_ms);


/**
 * mm_relative_sleep_us() - relative sleep in microseconds
 * @duration_us:        duration of sleep in microseconds
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
MMLIB_API int mm_relative_sleep_us(int64_t duration_us);


/**
 * mm_relative_sleep_ns() - relative sleep in nanoseconds
 * @duration_ns:        duration of sleep in nanoseconds
 *
 * Return: 0 in case of success, -1 otherwise with error state set to
 * indicate the error.
 */
MMLIB_API int mm_relative_sleep_ns(int64_t duration_ns);

/**************************************************************************
 *                     Timespec manipulation helpers                      *
 **************************************************************************/
#ifndef NS_IN_SEC
#define NS_IN_SEC       1000000000
#endif

#ifndef US_IN_SEC
#define US_IN_SEC       1000000
#endif

#ifndef MS_IN_SEC
#define MS_IN_SEC       1000
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

	dt = (ts->tv_sec - orig->tv_sec) * NS_IN_SEC;
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

	dt = (ts->tv_sec - orig->tv_sec) * US_IN_SEC;
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

	dt = (ts->tv_sec - orig->tv_sec) * MS_IN_SEC;
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

#endif
