/*
   @mindmaze_header@
 */
#ifndef TESTS_CHILD_PROC_H
#define TESTS_CHILD_PROC_H

#include <mmsysio.h>
#include <mmthread.h>

#define TESTS_CHILD_BIN BUILDDIR "/"LT_OBJDIR "tests-child-proc"EXEEXT
#define WR_PIPE_FD 3
#define RD_PIPE_FD 4

typedef union {
	mmthread_t thread_id;
	mm_pid_t proc_id;
} thread_proc_id;

enum {
	RUN_AS_THREAD,
	RUN_AS_PROCESS,
};

#define run_function(id, f, args, mode)\
	_run_function((id), (f), #f, (args), sizeof(*args), (mode))

int _run_function(thread_proc_id * id, intptr_t (*fn)(void*),
                  char * fn_name, void * args, size_t argslen, int run_mode);
void clean_function(thread_proc_id id, int run_mode);

#endif /* ifndef TESTS_CHILD_PROC_H */
