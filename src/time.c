/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmtime.h"
#include "mmerrno.h"


API_EXPORTED
int mm_relative_sleep_ns(int64_t duration_ns)
{
	struct timespec ts;

	mm_gettime(MM_CLK_MONOTONIC, &ts);
	mm_timeadd_ns(&ts, duration_ns);

	return mm_nanosleep(MM_CLK_MONOTONIC, &ts);
}


API_EXPORTED
int mm_relative_sleep_us(int64_t duration_us)
{
	struct timespec ts;

	mm_gettime(MM_CLK_MONOTONIC, &ts);
	mm_timeadd_us(&ts, duration_us);

	return mm_nanosleep(MM_CLK_MONOTONIC, &ts);
}


API_EXPORTED
int mm_relative_sleep_ms(int64_t duration_ms)
{
	struct timespec ts;

	mm_gettime(MM_CLK_MONOTONIC, &ts);
	mm_timeadd_ms(&ts, duration_ms);

	return mm_nanosleep(MM_CLK_MONOTONIC, &ts);
}
