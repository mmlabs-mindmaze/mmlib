/*
   @mindmaze_header@
*/
#ifndef LOCK_REFEREE_PROTO_H
#define LOCK_REFEREE_PROTO_H

#include "mmtime.h"
#include "mmthread.h"

#include <windows.h>
#include <stdint.h>

#define referee_pipename        "\\\\.\\pipe\\mmlib-lock-referee"

enum {
	LOCKREF_OP_WAKE,
	LOCKREF_OP_WAIT,
	LOCKREF_OP_INITLOCK,
	LOCKREF_OP_GETROBUST,
	LOCKREF_OP_CLEANUP,
	LOCKREF_OP_CLEANUP_DONE,
	LOCKREF_OP_ERROR,
};

#define WAITCLK_FLAG_MONOTONIC         (MMTHR_PSHARED << 1)
#define WAITCLK_FLAG_REALTIME          (MMTHR_PSHARED << 2)
#define WAITCLK_MASK	(WAITCLK_FLAG_MONOTONIC | WAITCLK_FLAG_REALTIME)


struct lockref_req_msg {
	int opcode;
	union {
		struct lockref_reqdata_wake {
			int num_wakeup;
			int64_t key;
			int64_t val;
		} wake;
		struct lockref_reqdata_wait {
			int64_t key;
			int64_t val;
			int clk_flags;
			struct timespec timeout;
		} wait;
		struct lockref_reqdata_getrobust {
			int num_keys;
		} getrobust;
		struct lockref_reqdata_cleanup_done {
			int64_t key;
			int num_wakeup;
		} cleanup;
	};
};

struct lockref_resp_msg {
	int respcode;
	union {
		int64_t key;
		HANDLE hmap;
		int timedout;
	};
};

struct robust_data {
	DWORD thread_id;
	int64_t attempt_key;
	int is_waiter;
	int num_locked;
	int num_locked_max;
	int64_t locked_keys[];
};

struct dead_thread {
	int is_waiter;
	DWORD tid;
};


struct mutex_cleanup_job {
	int in_progress;
	int num_dead;
	struct dead_thread deadlist[];
};


#ifdef LOCKSERVER_IN_MMLIB_DLL
void* lockserver_thread_routine(void* arg);
#endif

#endif
