/*
   @mindmaze_header@
*/
#ifndef PROCESS_TESTLIB_H
#define PROCESS_TESTLIB_H

#include "mmsysio.h"


#define NUM_FILE        3
#define NUM_ARGS_MAX    4
#define NUM_FDS         (2*NUM_FILE)
#define NUM_FDMAP       (NUM_FILE+1)

struct process_test_data {
	mm_pid_t pid;
	int fds[NUM_FDS];
	int pipe_wr;
	int pipe_rd;
	struct mm_remap_fd fd_map[NUM_FDMAP];
	int argv_data_len;
	char argv_data[NUM_ARGS_MAX][32];
	char cmd[128];
};

intptr_t test_execv_process(void * arg);

#endif
