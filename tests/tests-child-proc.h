/*
   @mindmaze_header@
 */
#ifndef TESTS_CHILD_PROC_H
#define TESTS_CHILD_PROC_H

#include <mmsysio.h>
#include <mmthread.h>

// workaround for libtool on windows: we need to execute directly the
// binary (the folder of mmlib dll is added at startup of testapi). On
// other platform we must use the normal wrapper script located in BUILDDIR
#if defined(_WIN32)
#  define TESTS_CHILD_BIN BUILDDIR "/.libs/tests-child-proc.exe"
#else
#  define TESTS_CHILD_BIN BUILDDIR "/tests-child-proc"
#endif

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
int run_as_process(mm_pid_t* pid_ptr, char * fn_name,
                   void* args, size_t argslen, int last_fd_kept);
void clean_function(thread_proc_id id, int run_mode);

#endif /* ifndef TESTS_CHILD_PROC_H */
