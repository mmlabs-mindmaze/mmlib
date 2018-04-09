/*
   @mindmaze_header@
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "tests-child-proc.h"

#include <stdlib.h>
#include <check.h>

#include <mmsysio.h>
#include <mmthread.h>


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
	int rv;
	struct mm_remap_fd fdmap;
	int shm_fd;
	void * map;
	char * argv[] = {TESTS_CHILD_BIN, fn_name, "mapfile-3-4096", NULL};

	switch (run_mode) {
	case RUN_AS_THREAD:
		rv = mmthr_create(&id->thread_id, (void*(*)(void*))fn, args);
		ck_assert_msg(rv == 0, "can't create thread for %s", fn_name);
		break;

	case RUN_AS_PROCESS:
		shm_fd = mm_anon_shm();
		ck_assert(shm_fd != -1);
		mm_ftruncate(shm_fd, MM_PAGESZ);

		map = mm_mapfile(shm_fd, 0, MM_PAGESZ, MM_MAP_RDWR|MM_MAP_SHARED);
		ck_assert(map != NULL);
		memcpy(map, args, argslen);

		fdmap.child_fd = 3;
		fdmap.parent_fd = shm_fd;
		rv = mm_spawn(&id->proc_id, argv[0], 1, &fdmap, 0, argv, NULL);
		ck_assert_msg(rv == 0, "can't create process for %s", fn_name);

		mm_unmap(map);
		mm_close(shm_fd);
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
