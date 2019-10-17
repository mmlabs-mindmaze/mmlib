/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "threaddata-manipulation.h"
#include "mmpredefs.h"
#include "mmtime.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>

#ifdef _MSC_VER

#  include <intrin.h>

#  define _Atomic
#  define atomic_store_i64(obj, val)	_InterlockedExchange64(obj, val)
#  define atomic_fetch_sub_i64(obj, val)  _InterlockedExchangeAdd64(obj, -val)

#else

#  include <stdatomic.h>

#  define atomic_store_i64(obj, val)	atomic_store(obj, val)
#  define atomic_fetch_sub_i64(obj, val)    atomic_fetch_sub(obj, val)

#endif


/**
 * touch_data() - modify data and restore it
 * @data:       address of shared data to modify
 * @tid:        data identifying uniquely the calling thread
 * @do_sleep:   if true, sleep between data modification and data value restoration
 *
 * Return: true if no inconsistency has been detected, false otherwise
 */
static
bool touch_data(_Atomic int64_t* data, int64_t tid, bool do_sleep)
{
	int64_t prev;

	atomic_store_i64(data, tid);

	if (do_sleep)
		mm_relative_sleep_ms(1);

	prev = atomic_fetch_sub_i64(data, tid);

	return (prev == tid) ? true : false;
}


/**
 * run_write_shared_data() - run thread function for concurrent write test
 * @shdata:     structure holding test setup and shared data
 *
 * In each iteration, lock the mutex, touch the shared data (modify and
 * restore its value), and unlock the mutex. If an inconsistent state of the
 * shared value is detect while touch the data, it means the mutex failed to
 * protect and the test fails (reported in @shdata->failed).
 */
API_EXPORTED
intptr_t run_write_shared_data(struct shared_write_data* shdata)
{
	int i;
	int num_iter = shdata->num_iteration;
	_Atomic int64_t* data = &shdata->value;
	int64_t tid = (int64_t)mmthr_self();
	bool match, do_sleep;

	do_sleep = shdata->sleep_in_touch;

	for (i = 0; i < num_iter; i++) {
		mmthr_mtx_lock(&shdata->mutex);
		match = touch_data(data, tid, do_sleep);
		mmthr_mtx_unlock(&shdata->mutex);

		if (!match) {
			shdata->failed = true;
			break;
		}
	}

	atomic_fetch_sub_i64(&shdata->num_runner_remaining, 1);

	return 0;
}


API_EXPORTED
intptr_t run_notif_data(struct notif_data* ndata)
{
	mmthr_mtx_lock(&ndata->mutex);

	// Notify that the runner is ready
	ndata->nwaiter += 1;
	mmthr_cond_signal(&ndata->cv1);

	// Wait for being signal or asked to exit
	while (!ndata->todo && !ndata->quit)
		mmthr_cond_wait(&ndata->cv2, &ndata->mutex);

	if (!ndata->quit)
		ndata->done += 1;

	ndata->numquit += 1;
	mmthr_mtx_unlock(&ndata->mutex);

	return 0;
}


API_EXPORTED
intptr_t run_robust_mutex_write_data(struct robust_mutex_write_data* rdata)
{
	int r, iter;
	mmthr_mtx_t* mtx = &rdata->mutex;

	r = mmthr_mtx_lock(mtx);
	if (r == EOWNERDEAD) {
		rdata->detected_iter_after_crash = rdata->iter_finished;
		mmthr_mtx_consistent(mtx);
	} else if (r != 0) {
		return -1;
	}

	iter = rdata->iter++;

	if (iter == 0 && rdata->sleep_after_first_lock)
		mm_relative_sleep_ms(50);

	if (iter == rdata->crash_at_iter) {
		abort();
	}

	rdata->iter_finished++;

	mmthr_mtx_unlock(mtx);

	return 0;
}
