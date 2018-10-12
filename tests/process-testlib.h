/*
   @mindmaze_header@
*/
#ifndef PROCESS_TESTLIB_H
#define PROCESS_TESTLIB_H

#include "mmsysio.h"


#define NUM_FILE        3
#define NUM_ARGS_MAX    4
#define NUM_FDS         (2*NUM_FILE)

struct process_test_data {
	mm_pid_t pid;
	int fds[NUM_FDS];
	struct mm_remap_fd fd_map[NUM_FILE];
	int argv_data_len;
	char argv_data[NUM_ARGS_MAX][32];
	char cmd[128];
};

#endif
