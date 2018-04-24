/*
   @mindmaze_header@
*/
#ifndef MMTHREAD_H
#define MMTHREAD_H


#include <errno.h>

#include "mmtime.h"
#include "mmpredefs.h"

#ifndef _WIN32

#include <pthread.h>

typedef pthread_mutex_t mmthr_mtx_t;
typedef pthread_cond_t mmthr_cond_t;
typedef pthread_once_t mmthr_once_t;
typedef pthread_t mmthread_t;

#define MMTHR_MTX_INITIALIZER           PTHREAD_MUTEX_INITIALIZER
#define MMTHR_COND_INITIALIZER          PTHREAD_COND_INITIALIZER
#define MMTHR_ONCE_INIT                 PTHREAD_ONCE_INIT

#else // _WIN32

#include <stdint.h>

typedef struct mmthr_mtx {
	int type;
	union {
		void* srw_lock;
		struct {
			int64_t lock;
			int64_t pshared_key;
		};
	};
} mmthr_mtx_t;

typedef struct mmthr_cond {
	int type;
	union {
		void* cv;
		struct {
			int64_t pshared_key;
			int64_t waiter_seq;
			int64_t wakeup_seq;
		};
	};
} mmthr_cond_t;

typedef int mmthr_once_t;

#define MMTHR_MTX_INITIALIZER           {0}
#define MMTHR_COND_INITIALIZER          {0}
#define MMTHR_ONCE_INIT                 0

typedef struct mmthread* mmthread_t;

#endif // !_WIN32

#define MMTHR_PSHARED           0x00000001
#define MMTHR_WAIT_MONOTONIC    0x00000002


#ifdef __cplusplus
extern "C" {
#endif

MMLIB_API int mmthr_mtx_init(mmthr_mtx_t* mutex, int flags);
MMLIB_API int mmthr_mtx_lock(mmthr_mtx_t* mutex);
MMLIB_API int mmthr_mtx_trylock(mmthr_mtx_t* mutex);
MMLIB_API int mmthr_mtx_consistent(mmthr_mtx_t* mutex);
MMLIB_API int mmthr_mtx_unlock(mmthr_mtx_t* mutex);
MMLIB_API int mmthr_mtx_deinit(mmthr_mtx_t* mutex);
MMLIB_API int mmthr_cond_init(mmthr_cond_t* cond, int flags);
MMLIB_API int mmthr_cond_wait(mmthr_cond_t* cond, mmthr_mtx_t* mutex);
MMLIB_API int mmthr_cond_timedwait(mmthr_cond_t* cond, mmthr_mtx_t* mutex,
                                   const struct timespec* abstime);

MMLIB_API int mmthr_cond_signal(mmthr_cond_t* cond);
MMLIB_API int mmthr_cond_broadcast(mmthr_cond_t* cond);
MMLIB_API int mmthr_cond_deinit(mmthr_cond_t* cond);
MMLIB_API int mmthr_once(mmthr_once_t* once, void (*once_routine)(void));
MMLIB_API int mmthr_create(mmthread_t* thread, void* (*proc)(void*), void* arg);
MMLIB_API int mmthr_join(mmthread_t thread, void** value_ptr);
MMLIB_API int mmthr_detach(mmthread_t thread);
MMLIB_API mmthread_t mmthr_self(void);

#ifdef __cplusplus
}
#endif

#endif
