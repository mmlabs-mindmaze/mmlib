/*
   @mindmaze_header@
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "tests-child-proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#include <mmsysio.h>
#include <mmthread.h>

#define FDMAP_MAXLEN    16


LOCAL_SYMBOL
int run_as_process(mm_pid_t* pid_ptr, char * fn_name,
                   void* args, size_t argslen, int last_fd_kept)
{
	struct mm_remap_fd fdmap[FDMAP_MAXLEN];
	int rv, i, shm_fd, fdmap_len;
	char mapfile_arg_str[64];
	char* argv[] = {TESTS_CHILD_BIN, fn_name, mapfile_arg_str, NULL};
	void* map;

	shm_fd = mm_anon_shm();
	ck_assert(shm_fd != -1);
	mm_ftruncate(shm_fd, MM_PAGESZ);

	map = mm_mapfile(shm_fd, 0, MM_PAGESZ, MM_MAP_RDWR|MM_MAP_SHARED);
	ck_assert(map != NULL);
	memcpy(map, args, argslen);

	// Configure fdmap to keep all fd in child up to last_fd_kept
	for (i = 0; i <= last_fd_kept; i++) {
		fdmap[i].child_fd = i;
		fdmap[i].parent_fd = i;
	}

	// Ensure shm_fd is inherited in child
	fdmap_len = last_fd_kept+2;
	sprintf(mapfile_arg_str, "mapfile-%i-4096", last_fd_kept+1);
	fdmap[last_fd_kept+1].child_fd = last_fd_kept+1;
	fdmap[last_fd_kept+1].parent_fd = shm_fd;

	// Start process
	rv = mm_spawn(pid_ptr, argv[0], fdmap_len, fdmap, 0, argv, NULL);

	mm_unmap(map);
	mm_close(shm_fd);

	return rv;
}


/*
 * _run_function(): takes a function and run it as a thread or a process.
 * @id: id of the thread or the process returned
 * @fn: the function to run
 * @fn_name: the function name as a string. Filled by the run_function macro
 * @args: arguments that will be passed to the function
 * @argslen: length of the arguments. Filled  by the run_function macro
 * @run_mode: can be RUN_AS_THREAD, or RUN_AS_PROCESS
 *
 * This should not be called directly, use the run_function macro below instead.
 *
 * See mmthr_create() and mm_spawn().
 *
 * the function ran should follow the prototype:
 *   intptr_t custom_fn(void *)
 * The function is expected to return zero on success.
 *
 * Returns: 0 on success, aborts otherwise
 */
LOCAL_SYMBOL
int _run_function(thread_proc_id * id, intptr_t (*fn)(void*),
                  char * fn_name, void * args, size_t argslen, int run_mode)
{
	union {
		intptr_t (*proc)(void*);
		void* (*thread)(void*);
	} cast_fn;
	int rv;

	// Use union to cast function type (no compiler should warn this)
	cast_fn.proc = fn;

	switch (run_mode) {
	case RUN_AS_THREAD:
		rv = mmthr_create(&id->thread_id, cast_fn.thread, args);
		ck_assert_msg(rv == 0, "can't create thread for %s", fn_name);
		break;

	case RUN_AS_PROCESS:
		rv = run_as_process(&id->proc_id, fn_name, args, argslen, 2);
		ck_assert_msg(rv == 0, "can't create process for %s", fn_name);
		break;

	default:
		ck_assert_msg(0, "invalid mode : %s", fn_name, run_mode);
	}

	return 0;
}

/**
 * clean_function(): clean a thread or a process that has been launched using run_function()
 * @id: the thread or process id as filled by run_function()
 * @run_mode: can be RUN_AS_THREAD, or RUN_AS_PROCESS
 */
LOCAL_SYMBOL
void clean_function(thread_proc_id id, int run_mode)
{
	intptr_t iptrval;
	int ival;

	switch (run_mode) {
	case RUN_AS_THREAD:
		mmthr_join(id.thread_id, (void**)&iptrval);
		if (iptrval != 0) {
			ck_abort_msg("thread returned %i", (int)iptrval);
		}
		break;

	case RUN_AS_PROCESS:
		if (  mm_wait_process(id.proc_id, &ival) != 0
		   || (ival ^ MM_WSTATUS_EXITED) != 0  ) {
			ck_abort_msg("process returned %i\n", ival);
		}
		break;

	default:
		ck_assert_msg(0, "invalid mode %i", run_mode);
		break;
	}
}
