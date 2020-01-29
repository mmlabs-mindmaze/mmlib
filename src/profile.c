/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "mmprofile.h"
#include "mmpredefs.h"
#include "mmtime.h"
#include "mmsysio.h"

#define SEC_IN_NSEC 1000000000
#define NUM_TS_MAX          16
#define MAX_LABEL_LEN       64
#define VALUESTR_LEN         8
#define UNITSTR_LEN          2
#define UNIT_MASK  \
	(PROF_FORCE_NSEC|PROF_FORCE_USEC|PROF_FORCE_MSEC|PROF_FORCE_SEC)
#define NUM_COL_MAX          5

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(a) ((a) > 0 ? (a) : (-a))

/**
 * struct unit - time unit definition
 * @scale:      scale at which the unit must be preferred
 * @name:       name which must appear in the textual results
 * @forcemask:  mask which force the unit to be used.
 */
struct unit {
	int64_t scale;
	char name[8];
	int forcemask;
};

static
const struct unit unit_list[] = {
	{1L, "ns", PROF_FORCE_NSEC},
	{1000L, "us", PROF_FORCE_USEC},
	{1000000L, "ms", PROF_FORCE_MSEC},
	{1000000000L, "s", PROF_FORCE_SEC},
};
#define NUM_UNIT ((int)(sizeof(unit_list)/sizeof(unit_list[0])))

/**************************************************************************
 *                                                                        *
 *                       Approximate median estimate                      *
 *                                                                        *
 **************************************************************************/
/**
 * DOC:
 * The approximate median algorithm use the FAME algorithm. This is a streaming
 * estimation of the median which converge to the actual median. Details can be
 * found at:
 * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.108.7376&rep=rep1&type=pdf
 */

#define STEP_NEED_INIT INT64_MAX

struct median_estimator {
	int64_t median;
	int64_t step;
};

static
void median_estimator_init(struct median_estimator* me)
{
	// Defer initialization to when the first data will be seen
	me->step = STEP_NEED_INIT;
}

static
void median_estimator_update(struct median_estimator* me, int64_t data)
{
	int64_t diff;
	int64_t step = me->step;
	int64_t median = me->median;

	// Perform actual initialization
	if (UNLIKELY(step == STEP_NEED_INIT)) {
		me->median = data;
		me->step = MAX(ABS(data/2), SEC_IN_NSEC);
		return;
	}

	diff = data - median;
	if (diff > 0) {
		median += step;
	} else if (diff < 0) {
		median -= step;
	}

	if (ABS(diff) < me->step) {
		step /= 2;
	} else {
		// Increase step by a epsilon (epsilon = step / 16)
		step += MAX((step >> 4), 1);
	}

	me->step = step;
	me->median = median;
}


static
int64_t median_estimator_getvalue(struct median_estimator* me)
{
	return me->median;
}

/**************************************************************************
 *                                                                        *
 *                             Profile data                               *
 *                                                                        *
 **************************************************************************/
static int clock_id;    // Clock type to use to measure time
static int num_ts;      // Maximum number of points of measure used so far
static int next_ts;     // Index of the next point of measure slot. 0 is for
                        // the measure done by mm_tic()

static int num_iter;            // Number of iteration recorded so far
static int64_t toc_overhead;    // Overhead of a mm_tic()/mm_toc() call
static struct mm_timespec timestamps[NUM_TS_MAX];  // current iteration measure
static int64_t max_diff_ts[NUM_TS_MAX];         // max time difference
static int64_t min_diff_ts[NUM_TS_MAX];         // min time difference
static int64_t sum_diff_ts[NUM_TS_MAX];  // sum of time difference overall
static struct median_estimator median_diff_ts[NUM_TS_MAX];  // approximate
                                                            // median of time
                                                            // diff
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
 *      (i-1)-th and the (i)-th timestamp. (index 0 correspond to mm_tic())
 *
 * Compute the time difference between 2 consecutive points at indices (i-1)
 * and (i). If it is not called at the initialization, the computed
 * difference takes into account the overhead of mm_tic() and mm_toc(). This
 * means that if the difference correspond to 2 consecutive call to mm_toc(),
 * the estimated difference (in absence of cold cache) should be close to 0
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
		median_estimator_update(&median_diff_ts[i], diff);
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
		median_estimator_init(&median_diff_ts[i]);
	}
}


/**
 * estimate_toc_overhead() - Estimate the overhead of call to mm_tic/mm_toc
 *
 * The estimation is done by several call to mm_tic mm_toc after resetting the
 * toc overhead to 0. Only the min value provide insight of the actual
 * overhead.
 *
 * NOTE: This approach of measuring call overhead can work only if the calls
 * to mm_toc() and mm_tic() are not optimized, ie, the prologues are not
 * skipped because the functions are in the same dynamic shared object. This
 * is ensured by setting a default visibility (ie API_EXPORTED_RELOCATABLE)
 * to the mm_tic() and mm_toc() functions.
 */
static
void estimate_toc_overhead(void)
{
	int i;

	reset_diffs();
	toc_overhead = 0;
	for (i = 0; i < 1000; i++) {
		mm_tic();
		mm_toc();
		mm_toc();

		mm_tic();
		mm_toc_label("");
		mm_toc_label("");

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
	struct mm_timespec ts;

	mm_gettime(clock_id, &ts);

	if (next_ts == NUM_TS_MAX-1)
		return;

	timestamps[next_ts] = ts;
	if (next_ts >= num_ts)
		num_ts = next_ts+1;

	next_ts++;
}


/**************************************************************************
 *                                                                        *
 *                           result display helpers                       *
 *                                                                        *
 **************************************************************************/

/**
 * max_label_len() - Get the maximum length of registered labels
 *
 * Returns: the maximum length
 */
static
int max_label_len(void)
{
	int i, max, len;

	max = 0;
	for (i = 1; i < num_ts; i++) {
		len = 2;
		if (labels[i])
			len = strlen(labels[i]);

		max = MAX(max, len);
	}

	// Ensure that return length will not overflow (even by mistake). BTW,
	// this helps the compiler to know that sprintf("%*s"...) will not
	// overflow
	if (max > MAX_LABEL_LEN-1)
		max = MAX_LABEL_LEN-1;

	return max;
}


/**
 * compute_requested_timings() - Compute and store result in an array
 * @mask:       mask of the requested timings computation
 * @num_points: number of time measure (ie number of call to mm_toc())
 * @data:       array (num_col x @num_points) receiving the results
 *
 * Returns: the number of different timing computation requested, i.e. the
 * number of columns in @data array.
 */
static
int compute_requested_timings(int mask, int num_points, int64_t data[])
{
	int i, icol = 0;
	double mean;
	int64_t median;

	if (mask & PROF_CURR) {
		for (i = 0; i < num_points; i++) {
			data[i + icol*num_points] = get_diff_ts(i+1);
		}

		icol++;
	}

	if (mask & PROF_MEAN) {
		for (i = 0; i < num_points; i++) {
			mean = (double)sum_diff_ts[i+1] / num_iter;
			data[i + icol*num_points] = mean;
		}

		icol++;
	}

	if (mask & PROF_MIN) {
		for (i = 0; i < num_points; i++) {
			data[i + icol*num_points] = min_diff_ts[i+1];
		}

		icol++;
	}

	if (mask & PROF_MAX) {
		for (i = 0; i < num_points; i++) {
			data[i + icol*num_points] = max_diff_ts[i+1];
		}

		icol++;
	}

	if (mask & PROF_MEDIAN) {
		for (i = 0; i < num_points; i++) {
			median =
				median_estimator_getvalue(&median_diff_ts[i+1]);
			data[i + icol*num_points] = median;
		}

		icol++;
	}

	return icol;
}


/**
 * get_display_unit() - get the index of suitable unit
 * @num_points: number of rows in @data (number of call to mm_toc())
 * @num_cols:   number of columns in @data
 * @data:       array (num_col x @num_points) holding the results
 * @mask:       mask supplied by user to possibly force use of a unit
 *
 * Returns: index of the suitable unit in unit_list array
 */
static
int get_display_unit(int num_points, int num_cols, int64_t data[], int mask)
{
	int i;
	int64_t minval, maxval, scale;

	// Use the specified unit if one has been forced by the mask
	for (i = 0; i < NUM_UNIT; i++) {
		if (unit_list[i].forcemask == (mask & UNIT_MASK))
			return i;
	}

	minval = INT64_MAX;
	maxval = 0;

	for (i = 0; i < num_cols*num_points; i++) {
		minval = MIN(minval, data[i]);
		maxval = MAX(maxval, data[i]);
	}

	// select the most suitable unit based on the min and max value
	for (i = 0; i < NUM_UNIT-1; i++) {
		scale = unit_list[i].scale;
		if ((minval < scale*100 && maxval < scale*10000)
		    || (maxval - minval) < scale)
			break;
	}

	return i;
}


/**
 * format_header_line() - print the result table header in string
 * @mask:               the requested timing computations
 * @label_width:        maximum length of a registered label
 * @str:                output string
 *
 * Returns: number of bytes written in the output string
 */
static
int format_header_line(int mask, int label_width, char str[])
{
	int len;

	len = sprintf(str, "%*s |", label_width, "");

	if (mask & PROF_CURR) {
		len += sprintf(str+len, "%*s %*s |",
		               VALUESTR_LEN, "current",
		               UNITSTR_LEN, "");
	}

	if (mask & PROF_MEAN) {
		len += sprintf(str+len, "%*s %*s |",
		               VALUESTR_LEN, "mean",
		               UNITSTR_LEN, "");
	}

	if (mask & PROF_MIN) {
		len += sprintf(str+len, "%*s %*s |",
		               VALUESTR_LEN, "min",
		               UNITSTR_LEN, "");
	}

	if (mask & PROF_MAX) {
		len += sprintf(str+len, "%*s %*s |",
		               VALUESTR_LEN, "max",
		               UNITSTR_LEN, "");
	}

	if (mask & PROF_MEDIAN) {
		len += sprintf(str+len, "%*s %*s |",
		               VALUESTR_LEN, "median",
		               UNITSTR_LEN, "");
	}

	str[len++] = '\n';
	memset(str+len, '-', len-1);
	len += len-1;
	str[len++] = '\n';

	return len;
}


/**
 * format_result_line() - print a line of the result table
 * @ncol:       number of columns in @data
 * @num_points: number of rows in @data (number of call to mm_toc())
 * @v:          index of the desired line in the table (first is 0)
 * @unit_index: index of the unit to use to display the result
 * @label_width:        maximum length of a registered label
 * @data:       array (num_col x @num_points) containing the results
 * @str:        output string
 *
 * Returns: number of bytes written in the output string
 */
static
int format_result_line(int ncol, int num_points, int v, int unit_index,
                       int label_width, const int64_t data[], char str[])
{
	int i, len;
	double value, scale = unit_list[unit_index].scale;
	const char* unitname = unit_list[unit_index].name;

	if (labels[v+1])
		len = sprintf(str, "%*s |", label_width, labels[v+1]);
	else
		len = sprintf(str, "%*i |", label_width, v+1);

	for (i = 0; i < ncol; i++) {
		value = data[i*num_points+v]/scale;
		len += sprintf(str+len, "%*.2f %*s |",
		               VALUESTR_LEN, value,
		               UNITSTR_LEN, unitname);
	}

	str[len++] = '\n';
	return len;
}


/**
 * full_mm_write() - full write of buffer succed or error is reported
 * @fd:         file descriptor to write to
 * @buf:        buffer to transfer
 * @len:        size of @buf
 *
 * Return: 0 if full buffer has been written to @fd, -1 otherwise with error
 * state set accordingly
 */
static
int full_mm_write(int fd, const void* buf, size_t len)
{
	const char* cbuf = buf;
	ssize_t rsz;

	while (len) {
		rsz = mm_write(fd, cbuf, len);
		if (rsz < 0)
			return -1;

		len -= rsz;
		cbuf += rsz;
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                           API implementation                           *
 *                                                                        *
 **************************************************************************/

/**
 * mm_tic() - Start a iteration of profiling
 *
 * Update the timing statistics with the previous data if applicable and
 * reset the metadata for a new timing iteration. Finally measure the
 * timestamp of the iteration start.
 *
 * NOTE: Contrary to the usual API functions, mm_tic() uses the attribute
 * API_EXPORTED_RELOCATABLE. This is done on purpose. See NOTE of
 * estimate_toc_overhead().
 */
API_EXPORTED_RELOCATABLE
void mm_tic(void)
{
	update_diffs();
	next_ts = 0;
	num_iter++;
	local_toc();
}


/**
 * mm_toc() - Add a new point of measure to the current timing iteration
 *
 * NOTE: Contrary to the usual API functions, mm_toc() uses the attribute
 * API_EXPORTED_RELOCATABLE. This is done on purpose. See NOTE of
 * estimate_toc_overhead().
 */
API_EXPORTED_RELOCATABLE
void mm_toc(void)
{
	local_toc();
}


/**
 * mm_toc_label() - Add a new point of measure associated with a label
 * @label:      string to appear in front of measure point at result display
 *
 * This function is the same as mm_toc() excepting it provides a way to label
 * the meansure point. Beware than only the first occurrence of a label
 * associated with a measure point will be retained. Any subsequent call to
 * mm_toc_label() at the same measure point index will be the same as calling
 * mm_toc().
 *
 * NOTE: Contrary to the usual API functions, mm_toc_label() uses the
 * attribute API_EXPORTED_RELOCATABLE. This is done on purpose. See NOTE of
 * estimate_toc_overhead().
 */
API_EXPORTED_RELOCATABLE
void mm_toc_label(const char* label)
{
	// Copy label if it the first time to appear
	if (!labels[next_ts]) {
		labels[next_ts] = &label_storage[next_ts*MAX_LABEL_LEN];
		strncpy(labels[next_ts], label, MAX_LABEL_LEN-1);
	}

	local_toc();
}


/**
 * mm_profile_print() - Print the timing statistics gathered so far
 * @mask:       combination of flags indicating statistics must be printed
 * @fd:         file descriptor to which the statistics must be printed
 *
 * Print the timing statistics on the file descriptor specified by fd. The
 * printed statistics between each consecutive point of measure is
 * controlled by the mask parameter which will a bitwise-or'd combination of
 * the following flags :
 *
 * - PROF_CURR: display the value of the current iteration
 * - PROF_MIN:  display the min value since the last reset
 * - PROF_MAX:  display the max value since the last reset
 * - PROF_MEDIAN: display the median value since the last reset
 * - PROF_FORCE_NSEC: force result display in nanoseconds
 * - PROF_FORCE_USEC: force result display in microseconds
 * - PROF_FORCE_MSEC: force result display in milliseconds
 * - PROF_FORCE_SEC: force result display in seconds
 *
 * Returns: 0 in case of success, -1 otherwise with errno set accordingly
 *
 * See: mm_profile_reset(), mm_tic(), write()
 */
API_EXPORTED
int mm_profile_print(int mask, int fd)
{
	int i, ncol, num_points, label_width, unit_index;
	char str[512];
	size_t len;
	int64_t data[NUM_COL_MAX*NUM_TS_MAX];

	update_diffs();

	label_width = max_label_len();
	num_points = num_ts-1;
	ncol = compute_requested_timings(mask, num_points, data);
	unit_index = get_display_unit(ncol, num_points, data, mask);

	for (i = 0; i < num_ts; i++) {
		if (i == 0)
			len = format_header_line(mask, label_width, str);
		else
			len = format_result_line(ncol, num_points, i-1,
			                         unit_index, label_width,
			                         data, str);


		// Write line to file
		if (full_mm_write(fd, str, len))
			return -1;
	}

	sprintf(str, "toc overhead = %li ns\n", (long)toc_overhead);
	return full_mm_write(fd, str, strlen(str));
}


/**
 * mm_profile_get_data - Retrieve profile result programmatically
 * @measure_point:      measure point whose statistic must be get
 * @type:               type of statistic (PROF_[CURR|MIN|MEAN|MAX|MEDIAN])
 *
 * Return: statistic value in nanosecond
 */
API_EXPORTED
int64_t mm_profile_get_data(int measure_point, int type)
{
	int64_t data[NUM_TS_MAX];
	int num_points, mask;

	if (measure_point >= num_ts-1)
		return -1;

	// Validate input type (can be only one measure type, not
	// combination of multiple flags)
	switch (type) {
	case PROF_CURR:
	case PROF_MIN:
	case PROF_MEAN:
	case PROF_MAX:
	case PROF_MEDIAN:
		break;

	default:
		return -1;
	}

	num_points = num_ts-1;
	mask = type|PROF_FORCE_NSEC;
	compute_requested_timings(mask, num_points, data);

	return data[measure_point];
}


/**
 * mm_profile_reset() - Reset the statistics and change the timer
 * @flags:	bit-OR combination of flags influencing the reset behavior.
 *
 * Reset the timing statistics, ie, reset the min, max, mean values as well
 * as the number of point used in one iteration and the associated labels.
 * Additionally it provides a ways to change the type of timer used for
 * measure.
 *
 * The @flags arguments allows to change the behavior of the reset.  If the
 * PROF_RESET_CPUCLOCK flag is set, it will use a timer based on CPU's
 * instruction counter. This timer has a very fine granularity (of the order
 * of very few nanoseconds) but it does not take measure time spent on
 * sleeping while waiting certain event (disk io, mutex/cond, etc...). This
 * timer indicates the processing power spent on tasks.
 *
 * Alternatively if PROF_RESET_CPUCLOCK is not set, it will use a timer
 * based on wall clock. This timer has bigger granularity (order of hundred
 * nanoseconds) and report time spent at sleeping. The indicates the
 * realtime update will performing certain task.
 *
 * If the PROF_RESET_KEEPLABEL flag is set in the @flags argument, the
 * labels associated with each measure point will be kept over the reset.
 * In practice, this provides a way to avoid the overhead of of label copy
 * when using mm_toc_label(): an initial iteration will copy the label and
 * the measurements are reset after this first iteration while keeping the
 * label. Then the subsequent call to mm_toc_label() will not be affected by
 * the string copy overhead.
 *
 * At startup, the function are configured to use CPU based timer.
 *
 * See: mm_profile_print(), mm_tic(), mm_toc_label()
 */
API_EXPORTED
void mm_profile_reset(int flags)
{
	unsigned int i;

	if (flags & PROF_RESET_CPUCLOCK)
		clock_id = MM_CLK_CPU_PROCESS;
	else
		clock_id = MM_CLK_MONOTONIC;

	estimate_toc_overhead();
	reset_diffs();

	if (!(flags & PROF_RESET_KEEPLABEL)) {
		labels[0] = label_storage;
		for (i = 1; i < sizeof(labels)/sizeof(labels[0]); i++)
			labels[i] = NULL;
	}
}
