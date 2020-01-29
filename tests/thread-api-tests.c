/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdatomic.h>

#include "api-testcases.h"
#include "mmpredefs.h"
#include "mmsysio.h"
#include "mmthread.h"
#include "mmtime.h"
#include "tests-child-proc.h"
#include "threaddata-manipulation.h"

#define EXPECTED_VALUE	(int)0xdeadbeef
#define NUM_CONCURRENCY	12

static
int mutex_type_flags[] = {
	0,
	MM_THR_PSHARED,
};
#define NUM_MUTEX_TYPE	MM_NELEM(mutex_type_flags)
#define FIRST_PSHARED_MUTEX_TYPE	1

static
void* simple_write_proc(void* arg)
{
	int* pval = arg;

	*pval = EXPECTED_VALUE;

	return NULL;
}


/*
 * Test that a write in another thread affect shared data
 */
START_TEST(data_write_in_thread)
{
	int value = 0;
	mm_thread_t thid;

	ck_assert(mm_thr_create(&thid, simple_write_proc, &value) == 0);
	mm_thr_join(thid, NULL);

	ck_assert(value == EXPECTED_VALUE);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                         mutex tests                                    *
 *                                                                        *
 **************************************************************************/
static
void* protected_write_threadproc(void* arg)
{
	run_write_shared_data(arg);
	return NULL;
}


static
void init_shared_write_data(struct shared_write_data* shdata, int mutex_flags,
                            int num_runner, int num_iter, bool do_sleep)
{
	shdata->sleep_in_touch = do_sleep;
	shdata->num_iteration = num_iter;
	shdata->value = SHARED_WRITE_INIT_VALUE;
	shdata->failed = false;
	shdata->num_runner_remaining = num_runner;

	ck_assert(mm_thr_mutex_init(&shdata->mutex, mutex_flags) == 0);
}


/**
 * runtest_mutex_protection_on_write() - test a concurrent write on shared data
 * @mutex_flags: flags to use for mutex in the test
 * @num_iter:   number of times each runner has to touch the shared data
 * @do_sleep:   true if runner has to sleep between setting shared data and restore
 *
 * This function initialized the shared data and mutex and spawn the thread
 * that will concurrently modify the shared data (protected by mutex). If
 * the mutex protection fails, one (or several) thread will see inconsistent
 * state in shared data and will report failure in shdata->failed.
 */
static
void runtest_mutex_protection_on_write(int mutex_flags, int num_iter, bool do_sleep)
{
	int i, num_thread;
	mm_thread_t thid[NUM_CONCURRENCY];
	struct shared_write_data shdata;

	init_shared_write_data(&shdata, mutex_flags, NUM_CONCURRENCY, num_iter, do_sleep);

	// Spawn all thread for the test
	num_thread = 0;
	for (i = 0; i < NUM_CONCURRENCY; i++) {
		if (mm_thr_create(&thid[i], protected_write_threadproc, &shdata) != 0)
			break;

		num_thread++;
	}


	// Join only threads that have been created
	for (i = 0; i < num_thread; i++)
		mm_thr_join(thid[i], NULL);

	ck_assert(num_thread == NUM_CONCURRENCY);
	ck_assert(shdata.num_runner_remaining == 0);
	ck_assert(shdata.failed == false);
}


/**
 * runtest_mutex_on_pshared_write() - test a concurrent write on shared data
 * @shdata:     structure holding test setup and shared data
 * @mutex_flags: flags to use for mutex in the test
 *
 * This function initialized the shared data and mutex and spawn the thread
 * that will concurrently modify the shared data (protected by mutex). If
 * the mutex protection fails, one (or several) thread will see inconsistent
 * state in shared data and will report failure in shdata->failed.
 */
static
void runtest_mutex_on_pshared_write(int mutex_flags,
                                         bool sleep_in_touch, int num_iter)
{
	int i, num_proc = 0;
	int shm_fd, num_runner_remaining = 0;
	mm_pid_t pid[NUM_CONCURRENCY];
	struct shared_write_data* shdata;
	void* map = NULL;
	bool failed = true;
	struct mm_remap_fd fdmap;
	char* argv[] = {TESTS_CHILD_BIN, "run_write_shared_data", "mapfile-3-4096", NULL};

	if ((shm_fd = mm_anon_shm()) == -1
	  || mm_ftruncate(shm_fd, MM_PAGESZ)
	  || !(map = mm_mapfile(shm_fd, 0, MM_PAGESZ, MM_MAP_RDWR|MM_MAP_SHARED)) ) {
		goto exit;
	}

	shdata = map;
	init_shared_write_data(shdata, mutex_flags, NUM_CONCURRENCY, num_iter, sleep_in_touch);

	// Spawn all children for the test
	fdmap.child_fd = 3;
	fdmap.parent_fd = shm_fd;
	num_proc = 0;
	for (i = 0; i < NUM_CONCURRENCY; i++) {
		if (mm_spawn(&pid[i], argv[0], 1, &fdmap, 0, argv, NULL))
			break;

		num_proc++;
	}

	// Wait that all children finish
	for (i = 0; i < num_proc; i++)
		mm_wait_process(pid[i], NULL);

	failed = shdata->failed;
	num_runner_remaining = shdata->num_runner_remaining;

exit:
	mm_unmap(map);
	if (shm_fd != -1)
		mm_close(shm_fd);

	ck_assert(num_proc == NUM_CONCURRENCY);
	ck_assert(num_runner_remaining == 0);
	ck_assert(failed == false);
}


/*
 * perform concurrent modification on a shared data protected by mutex. The
 * access protected by the mutex should restore the value as it at the
 * beginning if the mutex protection works.
 */
START_TEST(mutex_protection_on_write_normal)
{
	runtest_mutex_protection_on_write(mutex_type_flags[_i], 100000, false);
}
END_TEST


/*
 * Same test as mutex_protection_on_write_normal but introduce a sleep while
 * a thread is holding the lock and is modifying shared data. This increases
 * further the probability of contended lock situation.
 */
START_TEST(mutex_protection_on_write_sleep)
{
	runtest_mutex_protection_on_write(mutex_type_flags[_i], 10, true);
}
END_TEST


START_TEST(mutex_protection_on_pshared_write_normal)
{
	runtest_mutex_on_pshared_write(mutex_type_flags[_i], false, 100000);
}
END_TEST


START_TEST(mutex_protection_on_pshared_write_sleep)
{
	runtest_mutex_on_pshared_write(mutex_type_flags[_i], true, 10);
}
END_TEST


static const int robust_sleep_on_lock_cases[] = {false, true};
#define NUM_ROBUST_CASES MM_NELEM(robust_sleep_on_lock_cases)
#define EXPECTED_CRASH_ITER	(NUM_CONCURRENCY/2)

START_TEST(robust_mutex)
{
	int i, num_proc = 0;
	int num_iter = 0, num_iter_finished = 0, detected_crash_iter = 0;
	int shm_fd;
	mm_pid_t pid[NUM_CONCURRENCY];
	struct robust_mutex_write_data* rdata;
	void* map = NULL;
	struct mm_remap_fd fdmap;
	char* argv[] = {TESTS_CHILD_BIN, "run_robust_mutex_write_data", "mapfile-3-4096", NULL};

	if ((shm_fd = mm_anon_shm()) == -1
	  || mm_ftruncate(shm_fd, MM_PAGESZ)
	  || !(map = mm_mapfile(shm_fd, 0, MM_PAGESZ, MM_MAP_RDWR|MM_MAP_SHARED)) ) {
		goto exit;
	}

	rdata = map;
	rdata->iter = 0;
	rdata->iter_finished = 0;
	rdata->sleep_after_first_lock = robust_sleep_on_lock_cases[_i];
	rdata->crash_at_iter = EXPECTED_CRASH_ITER;
	rdata->detected_iter_after_crash = -1;
	ck_assert(mm_thr_mutex_init(&rdata->mutex, MM_THR_PSHARED) == 0);

	// Spawn all children for the test
	fdmap.child_fd = 3;
	fdmap.parent_fd = shm_fd;
	num_proc = 0;
	for (i = 0; i < NUM_CONCURRENCY; i++) {
		if (mm_spawn(&pid[i], argv[0], 1, &fdmap, 0, argv, NULL))
			break;

		num_proc++;
	}

	// Wait that all children finish
	for (i = 0; i < num_proc; i++)
		mm_wait_process(pid[i], NULL);

	num_iter = rdata->iter;
	num_iter_finished = rdata->iter_finished;
	detected_crash_iter = rdata->detected_iter_after_crash;

exit:
	mm_unmap(map);
	if (shm_fd != -1)
		mm_close(shm_fd);

	ck_assert(num_proc == NUM_CONCURRENCY);
	ck_assert(num_iter == NUM_CONCURRENCY);
	ck_assert(num_iter_finished == NUM_CONCURRENCY-1);
	ck_assert(detected_crash_iter == EXPECTED_CRASH_ITER);
}
END_TEST

/**************************************************************************
 *                                                                        *
 *                     condition variable tests                           *
 *                                                                        *
 **************************************************************************/
static
void* notif_var_threadproc(void* arg)
{
	run_notif_data(arg);
	return NULL;
}


static
void init_notif_data(struct notif_data* ndata, int mutex_flags)
{
	memset(ndata, 0, sizeof(*ndata));

	mm_thr_mutex_init(&ndata->mutex, mutex_flags);
	mm_thr_cond_init(&ndata->cv1, mutex_flags);
	mm_thr_cond_init(&ndata->cv2, mutex_flags);
}


static
void deinit_notif_data(struct notif_data* ndata)
{
	mm_thr_mutex_deinit(&ndata->mutex);
	mm_thr_cond_deinit(&ndata->cv1);
	mm_thr_cond_deinit(&ndata->cv2);
}


static
void do_notif_and_inspect(struct notif_data* ndata, bool do_broadcast,
                          int num_runner)
{
	mm_thr_mutex_lock(&ndata->mutex);

	// Wait for all started thread be ready
	while (ndata->nwaiter < num_runner)
		mm_thr_cond_wait(&ndata->cv1, &ndata->mutex);

	if (!do_broadcast) {
		// Notify one thread to wakeup and give it time respond
		ndata->todo = true;
		mm_thr_cond_signal(&ndata->cv2);
		mm_thr_mutex_unlock(&ndata->mutex);
		mm_relative_sleep_ms(500);
		mm_thr_mutex_lock(&ndata->mutex);
	}

	// Copy done value for later inspection and notify all threads to
	// quit
	ndata->quit = true;
	mm_thr_cond_broadcast(&ndata->cv2);
	mm_thr_mutex_unlock(&ndata->mutex);
}


/**
 * runtest_signal_or_broadcast_thread() - test thread notification
 * @mutex_flags: flags to use for mutex in the test
 * @do_broadcast: if true, use broadcast instead of signal
 */
static
void runtest_signal_or_broadcast_thread(int mutex_flags, bool do_broadcast)
{
	int i, num_thread;
	mm_thread_t thid[NUM_CONCURRENCY];
	struct notif_data ndata;

	init_notif_data(&ndata, mutex_flags);

	// Spawn all thread for the test
	num_thread = 0;
	for (i = 0; i < NUM_CONCURRENCY; i++) {
		if (mm_thr_create(&thid[i], notif_var_threadproc, &ndata) != 0)
			break;

		num_thread++;
	}

	do_notif_and_inspect(&ndata, do_broadcast, num_thread);

	// Join only threads that have been created
	for (i = 0; i < num_thread; i++)
		mm_thr_join(thid[i], NULL);

	// Verify expected outcome
	if (do_broadcast)
		ck_assert(ndata.numquit == NUM_CONCURRENCY);
	else
		ck_assert(ndata.done >= 1);

	ck_assert(num_thread == NUM_CONCURRENCY);

	deinit_notif_data(&ndata);
}


/**
 * runtest_signal_or_broadcast_thread() - test thread notification
 * @mutex_flags: flags to use for mutex in the test
 * @do_broadcast: if true, use broadcast instead of signal
 */
static
void runtest_signal_or_broadcast_process(int mutex_flags, bool do_broadcast)
{
	int i, num_proc = 0, value_done = 0, value_numquit = 0;
	struct notif_data* ndata;
	mm_pid_t pid[NUM_CONCURRENCY];
	int shm_fd;
	void* map = NULL;
	struct mm_remap_fd fdmap;
	char* argv[] = {TESTS_CHILD_BIN, "run_notif_data", "mapfile-3-4096", NULL};

	if ((shm_fd = mm_anon_shm()) == -1
	  || mm_ftruncate(shm_fd, MM_PAGESZ)
	  || !(map = mm_mapfile(shm_fd, 0, MM_PAGESZ, MM_MAP_RDWR|MM_MAP_SHARED)) ) {
		goto exit;
	}

	ndata = map;
	init_notif_data(ndata, mutex_flags);

	// Spawn all children for the test
	fdmap.child_fd = 3;
	fdmap.parent_fd = shm_fd;
	num_proc = 0;
	for (i = 0; i < NUM_CONCURRENCY; i++) {
		if (mm_spawn(&pid[i], argv[0], 1, &fdmap, 0, argv, NULL))
			break;

		num_proc++;
	}

	do_notif_and_inspect(ndata, do_broadcast, num_proc);

	// Wait that all children finish
	for (i = 0; i < num_proc; i++)
		mm_wait_process(pid[i], NULL);

	value_numquit = ndata->numquit;
	value_done = ndata->done;
	deinit_notif_data(ndata);

exit:
	mm_unmap(map);
	if (shm_fd != -1)
		mm_close(shm_fd);

	// Verify expected outcome
	if (do_broadcast)
		ck_assert(value_numquit == NUM_CONCURRENCY);
	else
		ck_assert(value_done >= 1);

	ck_assert(num_proc == NUM_CONCURRENCY);
}


START_TEST(signal_thread_data)
{
	runtest_signal_or_broadcast_thread(mutex_type_flags[_i], false);
}
END_TEST


START_TEST(broadcast_thread_data)
{
	runtest_signal_or_broadcast_thread(mutex_type_flags[_i], true);
}
END_TEST


START_TEST(signal_pshared_data)
{
	runtest_signal_or_broadcast_process(mutex_type_flags[_i], false);
}
END_TEST


START_TEST(broadcast_pshared_data)
{
	runtest_signal_or_broadcast_process(mutex_type_flags[_i], true);
}
END_TEST

/**************************************************************************
 *                                                                        *
 *                           one-time init tests                          *
 *                                                                        *
 **************************************************************************/
static atomic_int once_val1 = 0;
static atomic_int once_val2 = 0;
static mm_thr_once_t once = MM_THR_ONCE_INIT;

static
void one_time_proc(void)
{
	int readval;

	readval = atomic_fetch_add(&once_val1, 1);
	mm_relative_sleep_ms(1);
	atomic_fetch_add(&once_val2, readval+1);
}


static
void* once_test_proc(void* data)
{
	(void)data;

	mm_thr_once(&once, one_time_proc);

	return NULL;
}


START_TEST(concurrent_once)
{
	mm_thread_t thid[NUM_CONCURRENCY];
	int i;

	for (i = 0; i < MM_NELEM(thid); i++)
		ck_assert(mm_thr_create(&thid[i], once_test_proc, NULL) == 0);

	for (i = 0; i < MM_NELEM(thid); i++)
		mm_thr_join(thid[i], NULL);

	ck_assert_int_eq(once_val1, 1);
	ck_assert_int_eq(once_val2, 1);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_thread_tcase(void)
{
	TCase *tc = tcase_create("thread");
	tcase_add_test(tc, data_write_in_thread);
	tcase_add_loop_test(tc, mutex_protection_on_write_normal, 0, NUM_MUTEX_TYPE);
	tcase_add_loop_test(tc, mutex_protection_on_write_sleep, 0, NUM_MUTEX_TYPE);
	tcase_add_loop_test(tc, mutex_protection_on_pshared_write_normal, FIRST_PSHARED_MUTEX_TYPE, NUM_MUTEX_TYPE);
	tcase_add_loop_test(tc, mutex_protection_on_pshared_write_sleep, FIRST_PSHARED_MUTEX_TYPE, NUM_MUTEX_TYPE);
	tcase_add_loop_test(tc, robust_mutex, 0, NUM_ROBUST_CASES);
	tcase_add_loop_test(tc, signal_thread_data, 0, NUM_MUTEX_TYPE);
	tcase_add_loop_test(tc, broadcast_thread_data, 0, NUM_MUTEX_TYPE);
	tcase_add_loop_test(tc, signal_pshared_data, FIRST_PSHARED_MUTEX_TYPE, NUM_MUTEX_TYPE);
	tcase_add_loop_test(tc, broadcast_pshared_data, FIRST_PSHARED_MUTEX_TYPE, NUM_MUTEX_TYPE);
	tcase_add_test(tc, concurrent_once);

	return tc;
}
