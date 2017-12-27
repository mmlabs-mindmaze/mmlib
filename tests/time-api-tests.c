/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>

#include "mmtime.h"
#include "mmpredefs.h"
#include "mmlib.h"
#include <strings.h>
#include <stdio.h>

/**************************************************************************
 *                                                                        *
 *                       timespec manipulation tests                      *
 *                                                                        *
 **************************************************************************/
struct timespec_case {
	struct timespec t1;
	struct timespec t2;
	int64_t ns, us, ms;
};
static const struct timespec_case ts_cases[] = {
	{
		.t1 = {.tv_sec = 1, .tv_nsec = 0},
		.t2 = {.tv_sec = 0, .tv_nsec = 0},
		.ns = 1000000000, .us = 1000000, .ms = 1000,
	},
	{
		.t1 = {.tv_sec = 1, .tv_nsec = 999999999},
		.t2 = {.tv_sec = 0, .tv_nsec = 999999999},
		.ns = 1000000000, .us = 1000000, .ms = 1000,
	},
	{
		.t1 = {.tv_sec = 42, .tv_nsec = 500000000},
		.t2 = {.tv_sec = 42, .tv_nsec = 500000000},
		.ns = 0, .us = 0, .ms = 0,
	},
	{
		.t1 = {.tv_sec = 100, .tv_nsec = 499999000},
		.t2 = {.tv_sec = 42, .tv_nsec = 500000000},
		.ns = 57999999000, .us = 57999999, .ms = 58000,
	},
};

/*
 * 
 */
START_TEST(diff_time_ns)
{
	struct timespec t1 = ts_cases[_i].t1;
	struct timespec t2 = ts_cases[_i].t2;
	int64_t diff = ts_cases[_i].ns;

	ck_assert_int_eq(mm_timediff_ns(&t1, &t2), diff);
	ck_assert_int_eq(mm_timediff_ns(&t2, &t1), -diff);
}
END_TEST


START_TEST(diff_time_us)
{
	struct timespec t1 = ts_cases[_i].t1;
	struct timespec t2 = ts_cases[_i].t2;
	int64_t diff = ts_cases[_i].us;

	ck_assert_int_eq(mm_timediff_us(&t1, &t2), diff);
	ck_assert_int_eq(mm_timediff_us(&t2, &t1), -diff);
}
END_TEST


START_TEST(diff_time_ms)
{
	struct timespec t1 = ts_cases[_i].t1;
	struct timespec t2 = ts_cases[_i].t2;
	int64_t diff = ts_cases[_i].ms;

	ck_assert_int_eq(mm_timediff_ms(&t1, &t2), diff);
	ck_assert_int_eq(mm_timediff_ms(&t2, &t1), -diff);
}
END_TEST


START_TEST(add_time_ns)
{
	struct timespec t1;
	struct timespec t2; 
	int64_t diff = ts_cases[_i].ns;

	t1 = ts_cases[_i].t1;
	t2 = ts_cases[_i].t2;
	mm_timeadd_ns(&t2, diff);
	ck_assert(mm_timediff_ns(&t2, &t1) == 0);

	t1 = ts_cases[_i].t1;
	t2 = ts_cases[_i].t2;
	mm_timeadd_ns(&t1, -diff);
	ck_assert(mm_timediff_ns(&t1, &t2) == 0);
}
END_TEST


START_TEST(add_time_us)
{
	struct timespec t1;
	struct timespec t2; 
	int64_t diff = ts_cases[_i].us;

	t1 = ts_cases[_i].t1;
	t2 = ts_cases[_i].t2;
	mm_timeadd_us(&t2, diff);
	ck_assert(mm_timediff_us(&t2, &t1) == 0);

	t1 = ts_cases[_i].t1;
	t2 = ts_cases[_i].t2;
	mm_timeadd_us(&t1, -diff);
	ck_assert(mm_timediff_us(&t1, &t2) == 0);
}
END_TEST


START_TEST(add_time_ms)
{
	struct timespec t1;
	struct timespec t2; 
	int64_t diff = ts_cases[_i].ms;

	t1 = ts_cases[_i].t1;
	t2 = ts_cases[_i].t2;
	mm_timeadd_ms(&t2, diff);
	ck_assert(mm_timediff_ms(&t2, &t1) == 0);

	t1 = ts_cases[_i].t1;
	t2 = ts_cases[_i].t2;
	mm_timeadd_ms(&t1, -diff);
	ck_assert(mm_timediff_ms(&t1, &t2) == 0);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                          timespec query tests                          *
 *                                                                        *
 **************************************************************************/
static const int monotonic_clks[] = {
	MM_CLK_MONOTONIC,
	MM_CLK_MONOTONIC_RAW,
	MM_CLK_CPU_THREAD,
};

static const int all_clks[] = {
	MM_CLK_REALTIME,
	MM_CLK_MONOTONIC,
	MM_CLK_CPU_PROCESS,
	MM_CLK_CPU_THREAD,
	MM_CLK_MONOTONIC_RAW,
};


START_TEST(clock_resolution)
{
	struct timespec res, ts, prev_ts, start;
	int64_t diff, res_increment;
	clockid_t id = all_clks[_i];

	// Get clock resolution
	ck_assert(mm_getres(id, &res) == 0);
	res_increment = res.tv_sec * NS_IN_SEC + res.tv_nsec;

	mm_gettime(id, &start);
	prev_ts = start;

	do {
		// Acquire time and compute delta with previous measure
		ck_assert(mm_gettime(id, &ts) == 0);
		diff = mm_timediff_ns(&ts, &prev_ts);
		prev_ts = ts;

		// Check that when clock increments, it is at least by the
		// clock resolution
		if (diff > 0)
			ck_assert_int_ge(diff, res_increment);

	} while (mm_timediff_ns(&ts, &start) < NS_IN_SEC);

}
END_TEST


START_TEST(wallclock_time)
{
	int i;
	int64_t diff_ns;
	struct timespec ts_realtime, ts_utc = {.tv_nsec = 0};

	for (i = 0; i < 100000; i++) {
		if (mm_gettime(MM_CLK_REALTIME, &ts_realtime) != 0)
			ck_abort_msg("mm_gettime() failed");

		ts_utc.tv_sec = time(NULL);

		diff_ns = mm_timediff_ns(&ts_utc, &ts_realtime);
		diff_ns = (diff_ns > 0) ? diff_ns : -diff_ns;
		if (diff_ns > NS_IN_SEC)
			ck_abort_msg("mm_gettime(MM_CLK_REALTIME) differs"
			             " from time(NULL): diff_ns = %li",
			             (long)diff_ns);
	}
}
END_TEST


START_TEST(monotonic_update)
{
	int i;
	struct timespec ts, prev_ts;
	int64_t diff_ns;
	clockid_t id = monotonic_clks[_i];

	mm_gettime(id, &prev_ts);

	for (i = 0; i < 100000; i++) {
		if (mm_gettime(id, &ts) != 0)
			ck_abort_msg("mm_gettime(%i) failed", id);

		diff_ns = mm_timediff_ns(&ts, &prev_ts);
		if (diff_ns < 0)
			ck_abort_msg("mm_gettime(%i) not monotomic "
			             "(diff_ns = %li)", id, (long)diff_ns);

		prev_ts = ts;
	}
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                              sleep tests                               *
 *                                                                        *
 **************************************************************************/
static const int waitable_clks[] = {
	MM_CLK_REALTIME,
	MM_CLK_MONOTONIC,
};


START_TEST(absolute_sleep)
{
	struct timespec ts, now;
	clockid_t id = waitable_clks[_i];
	int64_t delay;

	for (delay = 100; delay <= 10000000; delay *= 100) {
		mm_gettime(id, &ts);
		mm_timeadd_ns(&ts, delay);

		ck_assert(mm_nanosleep(id, &ts) == 0);

		mm_gettime(id, &now);
		ck_assert(mm_timediff_ns(&now, &ts) >= 0);
	}
}
END_TEST


START_TEST(relative_sleep_ns)
{
	struct timespec ts, start;
	int i;
	int64_t delays[] = {50, 500, 10000, 1000000};

	for (i = 0; i < MM_NELEM(delays); i++) {
		mm_gettime(MM_CLK_MONOTONIC, &start);
		ck_assert(mm_relative_sleep_ns(delays[i]) == 0);
		mm_gettime(MM_CLK_MONOTONIC, &ts);
		ck_assert_int_ge(mm_timediff_ns(&ts, &start), delays[i]);
	}
}
END_TEST


START_TEST(relative_sleep_us)
{
	struct timespec ts, start;
	int i;
	int64_t delays[] = {50, 500, 10000};

	for (i = 0; i < MM_NELEM(delays); i++) {
		mm_gettime(MM_CLK_MONOTONIC, &start);
		ck_assert(mm_relative_sleep_us(delays[i]) == 0);
		mm_gettime(MM_CLK_MONOTONIC, &ts);
		ck_assert_int_ge(mm_timediff_us(&ts, &start), delays[i]);
	}
}
END_TEST


START_TEST(relative_sleep_ms)
{
	struct timespec ts, start;
	int i;
	int64_t delays[] = {1, 5, 20, 100, 300};

	for (i = 0; i < MM_NELEM(delays); i++) {
		mm_gettime(MM_CLK_MONOTONIC, &start);
		ck_assert(mm_relative_sleep_ms(delays[i]) == 0);
		mm_gettime(MM_CLK_MONOTONIC, &ts);
		ck_assert_int_ge(mm_timediff_ms(&ts, &start), delays[i]);
	}
}
END_TEST

/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_time_tcase(void)
{
	TCase *tc = tcase_create("time");

	tcase_add_loop_test(tc, diff_time_ns, 0, MM_NELEM(ts_cases));
	tcase_add_loop_test(tc, diff_time_us, 0, MM_NELEM(ts_cases));
	tcase_add_loop_test(tc, diff_time_ms, 0, MM_NELEM(ts_cases));
	tcase_add_loop_test(tc, add_time_ns, 0, MM_NELEM(ts_cases));
	tcase_add_loop_test(tc, add_time_us, 0, MM_NELEM(ts_cases));
	tcase_add_loop_test(tc, add_time_ms, 0, MM_NELEM(ts_cases));

	if (!strcmp(mm_getenv("MMLIB_DISABLE_CLOCK_TESTS", "no"), "yes")) {
		fputs("Disable clock based tests\n", stderr);
		return tc;
	}

	tcase_add_loop_test(tc, clock_resolution, 0, MM_NELEM(all_clks));
	tcase_add_test(tc, wallclock_time);
	tcase_add_loop_test(tc, monotonic_update, 0, MM_NELEM(monotonic_clks));
	tcase_add_loop_test(tc, absolute_sleep, 0, MM_NELEM(waitable_clks));
	tcase_add_test(tc, relative_sleep_ns);
	tcase_add_test(tc, relative_sleep_us);
	tcase_add_test(tc, relative_sleep_ms);

	return tc;
}

