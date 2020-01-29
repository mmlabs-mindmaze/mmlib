/*
 * @mindmaze_header@
 */
#ifndef MMTHREAD_H
#define MMTHREAD_H


#include <errno.h>

#include "mmtime.h"
#include "mmpredefs.h"

#ifndef _WIN32

#include <pthread.h>

typedef pthread_mutex_t mm_thr_mutex_t;
typedef pthread_cond_t mm_thr_cond_t;
typedef pthread_once_t mm_thr_once_t;
typedef pthread_t mm_thread_t;

#define MM_THR_MTX_INITIALIZER  PTHREAD_MUTEX_INITIALIZER
#define MM_THR_COND_INITIALIZER PTHREAD_COND_INITIALIZER
#define MM_THR_ONCE_INIT        PTHREAD_ONCE_INIT

#else // _WIN32

#include <stdint.h>

typedef union {
	struct mm_thr_mutex_pshared {
		int flag;
		int padding;
		int64_t lock;
		int64_t pshared_key;
	} pshared;

	struct mm_thr_mutex_swr {
		int flag;
		int padding;
		void * srw_lock;
	} srw;
} mm_thr_mutex_t;

typedef union {
	struct mm_thr_cond_pshared {
		int flag;
		int padding;
		int64_t pshared_key;
		int64_t waiter_seq;
		int64_t wakeup_seq;
	} pshared;

	struct mm_thr_cond_swr {
		int flag;
		int padding;
		void* cv;
	} srw;
} mm_thr_cond_t;

typedef int mm_thr_once_t;

#define MM_THR_MTX_INITIALIZER {0}
#define MM_THR_COND_INITIALIZER {0}
#define MM_THR_ONCE_INIT 0

typedef struct mm_thread* mm_thread_t;

#endif // !_WIN32

#define MM_THR_PSHARED 0x00000001
#define MM_THR_WAIT_MONOTONIC 0x00000002


#ifdef __cplusplus
extern "C" {
#endif

MMLIB_API int mm_thr_mutex_init(mm_thr_mutex_t* mutex, int flags);
MMLIB_API int mm_thr_mutex_lock(mm_thr_mutex_t* mutex);
MMLIB_API int mm_thr_mutex_trylock(mm_thr_mutex_t* mutex);
MMLIB_API int mm_thr_mutex_consistent(mm_thr_mutex_t* mutex);
MMLIB_API int mm_thr_mutex_unlock(mm_thr_mutex_t* mutex);
MMLIB_API int mm_thr_mutex_deinit(mm_thr_mutex_t* mutex);
MMLIB_API int mm_thr_cond_init(mm_thr_cond_t* cond, int flags);
MMLIB_API int mm_thr_cond_wait(mm_thr_cond_t* cond, mm_thr_mutex_t* mutex);
MMLIB_API int mm_thr_cond_timedwait(mm_thr_cond_t* cond, mm_thr_mutex_t* mutex,
                                    const struct mm_timespec* abstime);

MMLIB_API int mm_thr_cond_signal(mm_thr_cond_t* cond);
MMLIB_API int mm_thr_cond_broadcast(mm_thr_cond_t* cond);
MMLIB_API int mm_thr_cond_deinit(mm_thr_cond_t* cond);
MMLIB_API int mm_thr_once(mm_thr_once_t* once, void (* once_routine)(void));
MMLIB_API int mm_thr_create(mm_thread_t* thread, void* (*proc)(void*),
                            void* arg);
MMLIB_API int mm_thr_join(mm_thread_t thread, void** value_ptr);
MMLIB_API int mm_thr_detach(mm_thread_t thread);
MMLIB_API mm_thread_t mm_thr_self(void);

#ifdef __cplusplus
}
#endif

#endif /* ifndef MMTHREAD_H */
