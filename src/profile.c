/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include "mmprofile.h"

#define SEC_IN_NSEC	1000000000
#define NUM_TS_MAX	16
#define MAX_LABEL_LEN	64

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/**************************************************************************
 *                                                                        *
 *                             Profile data                               *
 *                                                                        *
 **************************************************************************/
static int clock_id;    // Clock type to use to measure time
static int num_ts;      // Maximum number of points of measure used so far
static int next_ts;     // Index of the next point of measure slot. 0 is for
                        // the measure done by mmtic()

static int num_iter;            // Number of iteration recorded so far
static int64_t toc_overhead;    // Overhead of a mmtic()/mmtoc() call
static struct timespec timestamps[NUM_TS_MAX];  // current iteration measure
static int64_t max_diff_ts[NUM_TS_MAX];         // max time difference
static int64_t min_diff_ts[NUM_TS_MAX];         // min time difference
static int64_t sum_diff_ts[NUM_TS_MAX];  // sum of time difference overall
static char* labels[NUM_TS_MAX];
static char label_storage[MAX_LABEL_LEN*NUM_TS_MAX];


/**************************************************************************
 *                                                                        *
 *                       Internal implementation                          *
 *                                                                        *
 **************************************************************************/

/**
 * get_diff_ts() - Estimate the time difference between 2 consecutive points
 * @i:  Index of the point. The difference will be computed between the
 *      (i-1)-th and the (i)-th timestamp. (index 0 correspond to mmtic())
 *
 * Compute the time difference between 2 consecutive points at indices (i-1)
 * and (i). If it is not called at the initialization, the computed
 * difference takes into account the overhead of mmtic() and mmtoc(). This
 * means that if the difference correspond to 2 consecutive call to mmtoc(),
 * the estimated difference (in abscence of cold cache) should be close to 0
 * ns (depends on the clock used).
 *
 * There is no argument validation, so 0 must NOT be passed.
 *
 * Returns: the time differences in nanoseconds
 */
static
int64_t get_diff_ts(int i)
{
	int64_t diff;

	diff = (timestamps[i].tv_sec - timestamps[i-1].tv_sec)*SEC_IN_NSEC;
	diff += timestamps[i].tv_nsec - timestamps[i-1].tv_nsec;
	diff -= toc_overhead;

	return diff;
}


/**
 * update_diffs() - Update the statistics of timestamp difference
 *
 * This function is meant to be called at the end of all tic/toc iteration.
 * It updates the min, max and sum (for mean) of the time difference based
 * on the previous iteration.
 */
static
void update_diffs(void)
{
	int i;
	int64_t diff;

	for (i = 1; i < next_ts; i++) {
		diff = get_diff_ts(i);
		min_diff_ts[i] = MIN(diff, min_diff_ts[i]);
		max_diff_ts[i] = MAX(diff, max_diff_ts[i]);
		sum_diff_ts[i] += diff;
	}
}


/**
 * reset_diffs() - reset the statistics of timestamp difference
 *
 * Reset the min, max, sum of the time differences. Also the maximum number
 * of timestamps that have been used so far.
 */
static
void reset_diffs(void)
{
	int i;

	next_ts = 0;
	num_ts = 0;
	num_iter = 0;

	for (i = 0; i < NUM_TS_MAX; i++) {
		min_diff_ts[i] = INT64_MAX;
		max_diff_ts[i] = 0L;
		sum_diff_ts[i] = 0L;
	}
}


/**
 * estimate_toc_overhead() - Estimate the overhead of call to mmtic/mmtoc
 *
 * The estimation is done by several call to mmtic mmtoc after resetting the
 * toc overhead to 0. Only the min value provide insight of the actual
 * overhead.
 *
 * NOTE: This approach of measuring call overhead can work only if the calls
 * to mmtoc() and mmtic() are not optimized, ie, the prologues are not
 * skipped because the functions are in the same dynamic shared object. This
 * is ensured by setting a default visibility (ie API_EXPORTED_RELOCATABLE)
 * to the mmtic() and mmtoc() functions.
 */
static
void estimate_toc_overhead()
{
	int i;

	reset_diffs();
	toc_overhead = 0;
	for (i = 0; i < 1000; i++) {
		mmtic();
		mmtoc();
		mmtoc();

		mmtic();
		mmtoc_label("");
		mmtoc_label("");

		// Remove the first measure to avoid cold cache effect
		if (i == 0)
			reset_diffs();
	}

	toc_overhead = MIN(min_diff_ts[1], min_diff_ts[2]);
}


/**
 * local_toc() - Measure the current timestamp
 *
 * Measures the current time into the next timestamp and advances it. If
 * applicable, increase the maximum number of timestamps that have been
 * measured within a same iteration.
 */
static inline
void local_toc(void)
{
	struct timespec ts;

	clock_gettime(clock_id, &ts);

	if (next_ts == NUM_TS_MAX-1)
		return;

	timestamps[next_ts] = ts;
	if (next_ts >= num_ts)
		num_ts = next_ts+1;
	next_ts++;
}


/**
 * init_profile() - init profiling to use CPU based timer
 *
 * INIT: This function is called AUTOMATICALLY at the beginning of a program using
 * mmlib and before the main().
 */
static __attribute__ ((constructor))
void init_profile(void)
{
	mmprofile_reset(1);
}


static
int max_label_len(void)
{
	int i, max, len;

	max = 0;
	for (i = 1; i < num_ts; i++) {
		len = 0;
		if (labels[i])
			len = strlen(labels[i]);
		max = MAX(max, len);
	}

	return max;
}

/**************************************************************************
 *                                                                        *
 *                           API implementation                           *
 *                                                                        *
 **************************************************************************/

/**
 * mmtic() - Start a iteration of profiling
 *
 * Update the timing statistics with the previous data if applicable and
 * reset the metadata for a new timing iteration. Finally measure the
 * timestamp of the iteration start.
 *
 * NOTE: Contrary to the usual API functions, mmtic() uses the attribute
 * API_EXPORTED_RELOCATABLE. This is done on purpose. See NOTE of
 * estimate_toc_overhead().
 */
API_EXPORTED_RELOCATABLE
void mmtic(void)
{
	update_diffs();
	next_ts = 0;
	num_iter++;
	local_toc();
}


/**
 * mmtoc() - Add a new point of measure to the current timing iteration
 *
 * NOTE: Contrary to the usual API functions, mmtoc() uses the attribute
 * API_EXPORTED_RELOCATABLE. This is done on purpose. See NOTE of
 * estimate_toc_overhead().
 */
API_EXPORTED_RELOCATABLE
void mmtoc(void)
{
	local_toc();
}


/**
 * mmtoc_label() - Add a new point of measure associated with a label
 * @label:      string to appear in front of measure point at result display
 *
 * This function is the same as mmtoc() excepting it provides a way to label
 * the meansure point. Beware than only the first occurence of a label
 * associated with a measure point will be retained. Any subsequent call to
 * mmtoc_label() at the same measure point index will be the same as calling
 * mmtoc().
 *
 * NOTE: Contrary to the usual API functions, mmtoc_label() uses the
 * attribute API_EXPORTED_RELOCATABLE. This is done on purpose. See NOTE of
 * estimate_toc_overhead().
 */
API_EXPORTED_RELOCATABLE
void mmtoc_label(const char* label)
{
	// Copy label if it the first time to appear
	if (!labels[next_ts]) {
		labels[next_ts] = &label_storage[next_ts*MAX_LABEL_LEN];
		strncpy(labels[next_ts], label, MAX_LABEL_LEN-1);
	}
	local_toc();
}


/**
 * mmprofile_print() - Print the timing statistics gathered so far
 * @mask:       combination of flags indicating statistics must be printed
 * @fd:         file descriptor to which the statistics must be printed
 *
 * Print the timing statistics on the file descriptor specified by fd. The
 * printed statistics between each consecutive point of measure is
 * controlled by the mask parameter which will a bitwise-or'd combination of
 * the following flags:
 *   - PROF_CURR: display the value of the current iteration
 *   - PROF_MIN:  display the min value since the last reset
 *   - PROF_MAX:  display the max value since the last reset
 *   - PROF_MEAN: display the average value since the last reset
 *
 * Returns: 0 in case of success, -1 otherwise with errno set accordingly
 */
API_EXPORTED
int mmprofile_print(int mask, int fd)
{
	int i, label_width;
	int64_t dt;
	char str[512], *buf;
	size_t len;
	ssize_t r;
	double mean;

	update_diffs();
	label_width = max_label_len();

	for (i = 0; i < num_ts; i++) {
		if (label_width)
			sprintf(str, "%*s: ", label_width, labels[i]);
		else
			sprintf(str, "%2i: ", i);
		len = strlen(str);

		if (mask & PROF_CURR) {
			if (i != 0) {
				dt = get_diff_ts(i);
				sprintf(str+len, "%12"PRIi64" ns, ", dt);
			} else
				strcpy(str+len, "      curr     , ");
			len = strlen(str);
		}

		if (mask & PROF_MEAN) {
			if (i != 0) {
				mean = (double)sum_diff_ts[i] / num_iter;
				sprintf(str+len, "%12f ns, ", mean);
			} else
				strcpy(str+len, "      mean     , ");
			len = strlen(str);
		}

		if (mask & PROF_MIN) {
			if (i != 0) {
				dt = min_diff_ts[i];
				sprintf(str+len, "%12"PRIi64" ns, ", dt);
			} else
				strcpy(str+len, "       min     , ");
			len = strlen(str);
		}

		if (mask & PROF_MAX) {
			if (i != 0) {
				dt = max_diff_ts[i];
				sprintf(str+len, "%12"PRIi64" ns, ", dt);
			} else
				strcpy(str+len, "       max     , ");
			len = strlen(str);
		}

		str[len++] = '\n';

		// Write line to file
		buf = str;
		do {
			if ((r = write(fd, buf, len)) < 0)
				return -1;
			len -= r;
			buf += r;
		} while (len);
	}

	return 0;
}


/**
 * mmprofile_reset() - Reset the statistics and change the timer
 * @cputime:    Indicates whether the time must be based on CPU or wall clock
 *
 * Reset the timing statistics, ie, reset the min, max, mean values as well
 * as the number of point used in one iteration. Additionally it provides a
 * ways to change the type of timer used for measure.
 *
 * If the cputime argument is non zero, it will use a timer based on CPU's
 * instruction counter. This timer has a very fine granularity (of the order
 * of very few nanoseconds) but it does not take measure time spent on
 * sleeping while waiting certain event (disk io, mutex/cond, etc...). This
 * timer indicates the processing power spent on tasks.
 *
 * Alternatively if cputime is zero, it will use a timer based on wall
 * clock. This timer has bigger granularity (order of hundred nanoseconds)
 * and report time spent at sleeping. The indicates the realtime update will
 * performing certain task.
 *
 * At startup, the function are configured to use CPU based timer.
 */
API_EXPORTED
void mmprofile_reset(int cputime)
{
	unsigned int i;

	clock_id = cputime ? CLOCK_PROCESS_CPUTIME_ID : CLOCK_MONOTONIC_RAW;

	estimate_toc_overhead();
	reset_diffs();

	labels[0] = label_storage;
	for (i = 1; i < sizeof(labels)/sizeof(labels[0]); i++)
		labels[i] = NULL;
}
