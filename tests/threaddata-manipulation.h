/*
   @mindmaze_header@
*/
#ifndef THREADDATA_MANIPULATION_H
#define THREADDATA_MANIPULATION_H

#include "mmthread.h"
#include <stdint.h>
#include <stdbool.h>

#define SHARED_WRITE_INIT_VALUE 0

struct shared_write_data {
	int64_t value;
	bool failed;
	int num_runner_remaining;
	int num_iteration;
	bool sleep_in_touch;
	mmthr_mtx_t mutex;
};

struct robust_mutex_write_data {
	int iter;
	int iter_finished;
	bool sleep_after_first_lock;
	int crash_at_iter;
	int detected_iter_after_crash;
	mmthr_mtx_t mutex;
};

struct notif_data {
	bool todo;
	int done;
	bool quit;
	int nwaiter;
	int numquit;
	mmthr_mtx_t mutex;
	mmthr_cond_t cv1;
	mmthr_cond_t cv2;
};

void run_write_shared_data(struct shared_write_data* shdata);
void run_notif_data(struct notif_data* ndata);
void run_robust_mutex_write_data(struct robust_mutex_write_data* rdata);

#endif