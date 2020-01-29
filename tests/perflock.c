/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>

#include "mmlib.h"
#include "mmpredefs.h"
#include "mmprofile.h"
#include "mmthread.h"

#define TEST_LOCK_REFEREE_SERVER_BIN    TOP_BUILDDIR"/src/"LT_OBJDIR"/lock-referee.exe"

/*************************************************************************
 *                                                                       *
 *                          Performance tests                            *
 *                                                                       *
 *************************************************************************/

struct perf_data {
	mm_thr_mutex_t mtx;
	int iter;
	char fill_up[64-sizeof(mm_thr_mutex_t)+sizeof(int)];
};

#define NUM_ITERATION	10000
#define NUM_THREAD_PER_LOCK_DEFAULT	16
#define NUM_LOCK_DEFAULT		4
#define NUM_THREAD_PER_LOCK_MAX         128
#define NUM_LOCK_MAX                    32
static struct perf_data data_array[32];
static int num_lock = NUM_LOCK_DEFAULT;
static int num_thread_per_lock = NUM_THREAD_PER_LOCK_DEFAULT;

static
void* lock_perf_routine(void* arg)
{
	struct perf_data* data;
	int i, ind;

	ind = *(int*)arg;

	for (i = 0; i < NUM_ITERATION; i++) {
		data = &data_array[ind];

		mm_thr_mutex_lock(&data->mtx);
		if (ind == 0)
			mm_toc();

		data->iter++;

		if (ind == 0)
			mm_tic();

		mm_thr_mutex_unlock(&data->mtx);

		if (++ind == num_lock)
			ind = 0;
	}

	return NULL;
}


static
int run_perf_lock_contended(int flags)
{
	mm_thread_t thids[NUM_THREAD_PER_LOCK_MAX*NUM_LOCK_MAX];
	int initial_lock_index[MM_NELEM(thids)];
	int i;
	int num_thids = num_thread_per_lock*num_lock;

	mm_profile_reset(0);

	for (i = 0; i < num_lock; i++) {
		mm_thr_mutex_init(&data_array[i].mtx, flags);
		mm_thr_mutex_lock(&data_array[i].mtx);
	}

	// Spawn threads
	for (i = 0; i < num_thids; i++) {
		initial_lock_index[i] = i % num_lock;
		mm_thr_create(&thids[i], lock_perf_routine, &initial_lock_index[i]);
	}

	mm_relative_sleep_ms(100);

	// Unlock mutex now
	for (i = 0; i < num_lock; i++) {
		if (i == 0)
			mm_tic();

		mm_thr_mutex_unlock(&data_array[i].mtx);
	}

	// Wait until all theads have finished
	for (i = 0; i < num_thids; i++)
		mm_thr_join(thids[i], NULL);

	for (i = 0; i < num_lock; i++)
		mm_thr_mutex_deinit(&data_array[0].mtx);

	printf("\ncontended case with flags=0x%08x:\n", flags);
	fflush(stdout);
	mm_profile_print(PROF_DEFAULT, 1);
	return 0;
}


static
int run_perf_lock_uncontended(int flags)
{
	int i;

	mm_profile_reset(0);

	mm_thr_mutex_init(&data_array[0].mtx, flags);

	for (i = 0; i < NUM_ITERATION; i++) {
		mm_tic();
		mm_thr_mutex_lock(&data_array[0].mtx);
		mm_thr_mutex_unlock(&data_array[0].mtx);
		mm_toc();
	}

	mm_thr_mutex_deinit(&data_array[0].mtx);

	printf("\nuncontended case with flags=0x%08x\n", flags);
	fflush(stdout);
	mm_profile_print(PROF_DEFAULT, 1);
	return 0;
}


int main(int argc, char* argv[])
{
#if _WIN32
	mm_setenv("MMLIB_LOCKREF_BIN", TEST_LOCK_REFEREE_SERVER_BIN, 1);
	setbuf(stdout, NULL);
#endif
	if (argc > 1)
		num_lock = atoi(argv[1]);

	if (argc > 2)
		num_thread_per_lock = atoi(argv[2]);

	printf("num_lock=%i num_thread_per_lock=%i\n",
	       num_lock, num_thread_per_lock);

	run_perf_lock_uncontended(0);
	run_perf_lock_uncontended(MM_THR_PSHARED);

	printf("\n\n");

	run_perf_lock_contended(0);
	run_perf_lock_contended(MM_THR_PSHARED);

	return EXIT_SUCCESS;
}

