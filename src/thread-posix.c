/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmthread.h"
#include "mmerrno.h"
#include "mmlog.h"

#include <pthread.h>
#include <string.h>


API_EXPORTED
int mmthr_mtx_init(mmthr_mtx_t* mutex, int flags)
{
	int ret;
	pthread_mutexattr_t attr;

	if (flags) {
		pthread_mutexattr_init(&attr);

		if (flags & MMTHR_PSHARED) {
			pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

#if HAVE_PTHREAD_MUTEX_CONSISTENT
			pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
#else
			mmlog_warn("Process shared mutex are supposed to be"
				   "robust as well. But I do not how to have"
				   "a robust mutex on this platform");
#endif
		}
	}

	ret = pthread_mutex_init(mutex, flags ? &attr : NULL);
	if (ret)
		mm_raise_error(ret, "Failed initializing mutex: %s", strerror(ret));

	if (flags)
		pthread_mutexattr_destroy(&attr);

	return ret;
}


API_EXPORTED
int mmthr_mtx_lock(mmthr_mtx_t* mutex)
{
	return pthread_mutex_lock(mutex);
}


API_EXPORTED
int mmthr_mtx_trylock(mmthr_mtx_t* mutex)
{
	return pthread_mutex_trylock(mutex);
}


API_EXPORTED
int mmthr_mtx_consistent(mmthr_mtx_t* mutex)
{
#if HAVE_PTHREAD_MUTEX_CONSISTENT
	return pthread_mutex_consistent(mutex);
#else
	mm_raise_error(ENOTSUP, "Robust mutex not supported on this platform");
	return ENOTSUP;
#endif
}


API_EXPORTED
int mmthr_mtx_unlock(mmthr_mtx_t* mutex)
{
	return pthread_mutex_unlock(mutex);
}


API_EXPORTED
int mmthr_mtx_deinit(mmthr_mtx_t* mutex)
{
	return pthread_mutex_destroy(mutex);
}


API_EXPORTED
int mmthr_cond_init(mmthr_cond_t* cond, int flags)
{
	int ret;
	pthread_condattr_t attr;

	if (flags) {
		pthread_condattr_init(&attr);

		if (flags & MMTHR_PSHARED)
			pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

		if (flags & MMTHR_WAIT_MONOTONIC)
			pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	}

	ret = pthread_cond_init(cond, flags ? &attr : NULL);
	if (ret)
		mm_raise_error(ret, "Failed initializing cond: %s", strerror(ret));

	if (flags)
		pthread_condattr_destroy(&attr);

	return ret;
}


API_EXPORTED
int mmthr_cond_wait(mmthr_cond_t* cond, mmthr_mtx_t* mutex)
{
	return pthread_cond_wait(cond, mutex);
}


API_EXPORTED
int mmthr_cond_timedwait(mmthr_cond_t* cond, mmthr_mtx_t* mutex,
                         const struct timespec* abstime)
{
	return pthread_cond_timedwait(cond, mutex, abstime);
}


API_EXPORTED
int mmthr_cond_signal(mmthr_cond_t* cond)
{
	return pthread_cond_signal(cond);
}


API_EXPORTED
int mmthr_cond_broadcast(mmthr_cond_t* cond)
{
	return pthread_cond_broadcast(cond);
}


API_EXPORTED
int mmthr_cond_deinit(mmthr_cond_t* cond)
{
	return pthread_cond_destroy(cond);
}


API_EXPORTED
int mmthr_once(mmthr_once_t* once, void (*once_routine)(void))
{
	return pthread_once(once, once_routine);
}


API_EXPORTED
int mmthr_create(mmthread_t* thread, void* (*proc)(void*), void* arg)
{
	int ret;

	ret = pthread_create(thread, NULL, proc, arg);
	if (ret)
		mm_raise_error(ret, "Failed creating thread: %s", strerror(ret));

	return ret;
}


API_EXPORTED
int mmthr_join(mmthread_t thread, void** value_ptr)
{
	int ret;

	ret = pthread_join(thread, value_ptr);
	if (ret)
		mm_raise_error(ret, "Failed to join thread: %s", strerror(ret));

	return ret;
}


API_EXPORTED
int mmthr_detach(mmthread_t thread)
{
	int ret;

	ret = pthread_detach(thread);
	if (ret)
		mm_raise_error(ret, "Failed to detach thread: %s", strerror(ret));

	return ret;
}


API_EXPORTED
mmthread_t mmthr_self(void)
{
	return pthread_self();
}
