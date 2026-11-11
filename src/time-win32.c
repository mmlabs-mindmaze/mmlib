/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmtime.h"
#include "mmerrno.h"
#include "clock-win32.h"
#include "utils-win32.h"

#include <windows.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>


/**************************************************************************
 *                                                                        *
 *                      Clock gettime implementation                      *
 *                                                                        *
 **************************************************************************/

/* doc in posix implementation */
API_EXPORTED
int mm_gettime(clockid_t clock_id, struct mm_timespec* ts)
{
	switch (clock_id) {
	case MM_CLK_REALTIME:
		gettimespec_wallclock_w32(ts);
		break;

	case MM_CLK_MONOTONIC:
	case MM_CLK_MONOTONIC_RAW:
		gettimespec_monotonic_w32(ts);
		break;

	case MM_CLK_CPU_THREAD:
		gettimespec_thread_w32(ts);
		break;

	case MM_CLK_CPU_PROCESS:
		gettimespec_process_w32(ts);
		break;

	default:
		mm_raise_error(EINVAL, "Invalid clock id: %i", clock_id);
		return -1;
	}

	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mm_getres(clockid_t clock_id, struct mm_timespec* res)
{
	switch (clock_id) {
	case MM_CLK_REALTIME:
		getres_wallclock_w32(res);
		break;

	case MM_CLK_MONOTONIC:
	case MM_CLK_MONOTONIC_RAW:
		getres_monotonic_w32(res);
		break;

	case MM_CLK_CPU_THREAD:
		getres_thread_w32(res);
		break;

	case MM_CLK_CPU_PROCESS:
		getres_process_w32(res);
		break;

	default:
		mm_raise_error(EINVAL, "Invalid clock id: %i", clock_id);
		return -1;
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                       Nanosleep implementation                         *
 *                                                                        *
 **************************************************************************/

static CALLBACK
void timer_apc_completion(void* data, DWORD timer_low, DWORD timer_high)
{
	(void)timer_low;
	(void)timer_high;
	int* done = data;

	*done = 1;
}


static
int relative_microsleep(int64_t delta_ns)
{
	int64_t ft_i64;
	int done;
	HANDLE timer_hnd;
	BOOL success;

	timer_hnd = get_thread_timer_hnd();
	if (timer_hnd == NULL)
		return mm_raise_from_w32err("Unable to create timer");

	// ft_i64 must be negative to request a relative wait in
	// SetWaitableTimer()
	ft_i64 = -((delta_ns + 99) / 100);

	// Set deadline of waitable timer
	done = 0;
	success = SetWaitableTimer(timer_hnd, (LARGE_INTEGER*)&ft_i64, 0,
	                           timer_apc_completion, &done, FALSE);
	if (!success)
		return mm_raise_from_w32err("Failed to wait on timer");

	// Do sleep until timer APC completion is executed
	do {
		SleepEx(INFINITE, TRUE);
	} while (done == 0);

	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mm_nanosleep(clockid_t clock_id, const struct mm_timespec* target)
{
	struct mm_timespec now;
	int64_t delta_ns;

	if (clock_id != MM_CLK_REALTIME
	    && clock_id != MM_CLK_MONOTONIC
	    && clock_id != MM_CLK_MONOTONIC_RAW)
		return mm_raise_error(EINVAL, "Invalid clock (%i)", clock_id);

	// Wait until the target timestamp is reached
	while (1) {
		// Compute the delta in nanosecond to reach the request
		mm_gettime(clock_id, &now);
		delta_ns = mm_timediff_ns(target, &now);
		if (delta_ns <= 0)
			break;

		if (relative_microsleep(delta_ns))
			return -1;
	}

	return 0;
}
