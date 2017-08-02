/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmprofile.h"
#include "mmpredefs.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include "mmthread.h"
#include "mmprofile.h"
#include "mmlib.h"

#define TEST_LOCK_REFEREE_SERVER_BIN    TOP_BUILDDIR"/src/"LT_OBJDIR"lock-referee.exe"

/*************************************************************************
 *                                                                       *
 *                          Performance tests                            *
 *                                                                       *
 *************************************************************************/

struct perf_data {
	mmthr_mtx_t mtx;
	int iter;
	char fill_up[64-sizeof(mmthr_mtx_t)+sizeof(int)];
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

		mmthr_mtx_lock(&data->mtx);
		if (ind == 0)
			mmtoc();

		data->iter++;

		if (ind == 0)
			mmtic();

		mmthr_mtx_unlock(&data->mtx);

		if (++ind == num_lock)
			ind = 0;
	}

	return NULL;
}


static
int run_perf_lock_contended(int flags)
{
	mmthread_t thids[NUM_THREAD_PER_LOCK_MAX*NUM_LOCK_MAX];
	int initial_lock_index[MM_NELEM(thids)];
	int i;
	int num_thids = num_thread_per_lock*num_lock;

	mmprofile_reset(0);

	for (i = 0; i < num_lock; i++) {
		mmthr_mtx_init(&data_array[i].mtx, flags);
		mmthr_mtx_lock(&data_array[i].mtx);
	}

	// Spawn threads
	for (i = 0; i < num_thids; i++) {
		initial_lock_index[i] = i % num_lock;
		mmthr_create(&thids[i], lock_perf_routine, &initial_lock_index[i]);
	}

	mm_relative_sleep_ms(100);

	// Unlock mutex now
	for (i = 0; i < num_lock; i++) {
		if (i == 0)
			mmtic();

		mmthr_mtx_unlock(&data_array[i].mtx);
	}

	// Wait until all theads have finished
	for (i = 0; i < num_thids; i++)
		mmthr_join(thids[i], NULL);

	for (i = 0; i < num_lock; i++)
		mmthr_mtx_deinit(&data_array[0].mtx);

	printf("\ncontended case with flags=0x%08x:\n", flags);
	fflush(stdout);
	mmprofile_print(PROF_DEFAULT, 1);
	return 0;
}


static
int run_perf_lock_uncontended(int flags)
{
	int i;

	mmprofile_reset(0);

	mmthr_mtx_init(&data_array[0].mtx, flags);

	for (i = 0; i < NUM_ITERATION; i++) {
		mmtic();
		mmthr_mtx_lock(&data_array[0].mtx);
		mmthr_mtx_unlock(&data_array[0].mtx);
		mmtoc();
	}

	mmthr_mtx_deinit(&data_array[0].mtx);

	printf("\nuncontended case with flags=0x%08x\n", flags);
	fflush(stdout);
	mmprofile_print(PROF_DEFAULT, 1);
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
	run_perf_lock_uncontended(MMTHR_PSHARED);

	printf("\n\n");

	run_perf_lock_contended(0);
	run_perf_lock_contended(MMTHR_PSHARED);

	return EXIT_SUCCESS;
}

