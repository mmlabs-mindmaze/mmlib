/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmtime.h"
#include "mmerrno.h"
#include <string.h>


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
