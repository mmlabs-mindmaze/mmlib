/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <string.h>

#include "mmerrno.h"
#include "mmsysio.h"
#include "mmthread.h"
#include "mmlog.h"

#include "process-testlib.h"

#define TEST_PATTERN    "++test pattern++"

static
void* write_pipe_thread(void* data)
{
	int* pipe_fds = data;
	char buffer[] = TEST_PATTERN;
	ssize_t rsz;

	while (1) {
		rsz = mm_write(pipe_fds[1], buffer, sizeof(buffer));
		if (rsz != sizeof(buffer))
			break;
	}

	return NULL;
}

static
void* read_pipe_thread(void* data)
{
	int* pipe_fds = data;
	char buffer[sizeof(TEST_PATTERN)];
	ssize_t rsz;

	while (1) {
		memset(buffer, 0, sizeof(buffer));
		rsz = mm_read(pipe_fds[0], buffer, sizeof(buffer));
		if (rsz != sizeof(buffer))
			break;

		mm_check(memcmp(buffer, TEST_PATTERN, sizeof(buffer)) == 0);
	}

	return NULL;
}

static
int exec_child(struct process_test_data* data)
{
	char* argv[NUM_ARGS_MAX+2] = {NULL};
	int i;

	argv[0] = data->cmd;
	for (i = 0; i < data->argv_data_len; i++)
		argv[i+1] = data->argv_data[i];

	if (mm_execv(data->cmd, NUM_FDMAP, data->fd_map,
	             0, argv, NULL) != 0) {
		mm_print_lasterror(NULL);
		return -1;
	}

	return 0;
}


API_EXPORTED
intptr_t test_execv_process(void * arg)
{
	int pipe_fds[2];
	mmthread_t t1, t2;

	// Create a pipe and read and write thread to create process
	// activity in other threads when mm_execv() will be called
	mm_check(mm_pipe(pipe_fds) == 0);
	mm_check(mmthr_create(&t1, read_pipe_thread, pipe_fds) == 0);
	mm_check(mmthr_create(&t2, write_pipe_thread, pipe_fds) == 0);

	return exec_child(arg);
}

