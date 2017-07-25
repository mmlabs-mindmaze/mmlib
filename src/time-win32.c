/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmtime.h"
#include "mmerrno.h"
#include "clock-win32.h"

#include <windows.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>


/**************************************************************************
 *                                                                        *
 *                      Clock gettime implementation                      *
 *                                                                        *
 **************************************************************************/

API_EXPORTED
int mm_gettime(clockid_t clock_id, struct timespec *ts)
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
	};

	return 0;
}


API_EXPORTED
int mm_getres(clockid_t clock_id, struct timespec *res)
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
	};

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                       Nanosleep implementation                         *
 *                                                                        *
 **************************************************************************/
static
int relative_microsleep(HANDLE htimer, int64_t delta_ns)
{
	ULARGE_INTEGER bigint;
	FILETIME ft;

	bigint.QuadPart = -(LONGLONG)(delta_ns + 50)/100;
	ft.dwLowDateTime = bigint.LowPart;
	ft.dwHighDateTime = bigint.HighPart;

	SetWaitableTimer(htimer, (LARGE_INTEGER*)&ft, 0, NULL, NULL, FALSE);

	WaitForSingleObject(htimer, INFINITE);

	return 0;
}


API_EXPORTED
int mm_nanosleep(clockid_t clock_id, const struct timespec *target)
{
	HANDLE htimer = NULL;
	struct timespec now;
	int64_t delta_ns;

	if (clock_id == MM_CLK_CPU_THREAD || clock_id == MM_CLK_CPU_PROCESS)
		return mm_raise_error(EINVAL, "Sleep cannot be done with CPU clock");

	if (mm_gettime(clock_id, &now))
		return -1;

	// Compute the delta in nanosecond to reach the request
	delta_ns = mm_timediff_ns(target, &now);
	if (delta_ns <= 0)
		return 0;

	htimer = CreateWaitableTimer(NULL, TRUE, NULL);

	// Wait until the target timestamp is reached
	do {
		relative_microsleep(htimer, delta_ns);
		mm_gettime(clock_id, &now);
		delta_ns = mm_timediff_ns(target, &now);
	} while (delta_ns > 0);

	CloseHandle(htimer);

	return 0;
}
