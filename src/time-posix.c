/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmtime.h"
#include "mmerrno.h"
#include <string.h>


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
API_EXPORTED
int mm_gettime(clockid_t clock_id, struct timespec *ts)
{
	int ret;

	ret = clock_gettime(clock_id, ts);
	if (ret) {
		mm_raise_error(ret, "clock_gettime failed: %s", strerror(ret));
		return -1;
	}

	return 0;
}


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
API_EXPORTED
int mm_getres(clockid_t clock_id, struct timespec *res)
{
	int ret;

	ret = clock_getres(clock_id, res);
	if (ret) {
		mm_raise_error(ret, "clock_getres failed: %s", strerror(ret));
		return -1;
	}

	return 0;
}


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
API_EXPORTED
int mm_nanosleep(clockid_t clock_id, const struct timespec *ts)
{
	int ret;

	ret = clock_nanosleep(clock_id, TIMER_ABSTIME, ts, NULL);
	if (ret) {
		mm_raise_error(ret, "clock_nanosleep failed: %s", strerror(ret));
		return -1;
	}

	return 0;
}
