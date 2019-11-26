/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "mmthread.h"
#include "mmerrno.h"
#include "mmlog.h"

#include <pthread.h>
#include <string.h>


/**
 * mm_thr_mutex_init() - Initialize a mutex
 * @mutex:      mutex to initialize
 * @flags:      OR-combination of flags indicating the type of mutex
 *
 * Use this function to initialize @mutex. The type of mutex is controlled
 * by @flags which must contains one or several of the following :
 *
 * MM_THR_PSHARED: init a mutex shareable by other processes. When a mutex
 * is process shared, it is also a robust mutex.
 *
 * If no flags is provided, the type of initialized mutex just a normal
 * mutex and a call to this function could be avoided if the data pointed by
 * @mutex has been statically initialized with MM_MTX_INITIALIZER.
 *
 * Currently, a robust mutex can only be initialized if is a process shared
 * mutex.
 *
 * It is undefined behavior if a mutex is reinitialized before getting
 * destroyed first.
 *
 * Return:
 *
 * 0
 *   The mutex has been initialized
 *
 * EINVAL
 *   @flags set the robust mutex attribute without the process-shared
 *   attribute.
 */
API_EXPORTED
int mm_thr_mutex_init(mm_thr_mutex_t* mutex, int flags)
{
	int ret;
	pthread_mutexattr_t attr;

	if (flags) {
		pthread_mutexattr_init(&attr);

		if (flags & MM_THR_PSHARED) {
			pthread_mutexattr_setpshared(&attr,
			                             PTHREAD_PROCESS_SHARED);

#if HAVE_PTHREAD_MUTEX_CONSISTENT
			pthread_mutexattr_setrobust(&attr,
			                            PTHREAD_MUTEX_ROBUST);
#else
			mm_log_warn("Process shared mutex are supposed to be "
			            "robust as well. But I do not how to have "
			            "a robust mutex on this platform");
#endif
		}
	}

	ret = pthread_mutex_init(mutex, flags ? &attr : NULL);
	if (ret)
		mm_raise_error(ret, "Failed initializing mutex: %s",
		               strerror(ret));

	if (flags)
		pthread_mutexattr_destroy(&attr);

	return ret;
}


/**
 * mm_thr_mutex_lock() - lock a mutex
 * @mutex:      initialized mutex
 *
 * The mutex object referenced by @mutex is locked by a successful
 * call to mm_thr_mutex_lock(). If the mutex is already locked by another
 * thread, the calling thread blocks until the mutex becomes available. If
 * the mutex is already locked by the calling thread, the function will
 * never return.
 *
 * If @mutex is a robust mutex, and the previous owner has died while
 * holding the lock, the return value EOWNERDEAD will indicate the calling
 * thread of this situation. In this case, the mutex is locked by the
 * calling thread but the state it protects is marked as inconsistent. The
 * application should ensure that the state is made consistent for reuse and
 * when that is complete call mm_thr_mutex_consistent(). If the application is
 * unable to recover the state, it should unlock the mutex without a prior
 * call to mm_thr_mutex_consistent(), after which the mutex is marked
 * permanently unusable.
 *
 * NOTE: If @mutex is a robust mutex and actually used across different
 * processes, the return value must not be ignored.
 *
 * Return:
 *
 * 0
 *   The mutex has been successfully locked by the calling thread
 *
 * EOWNERDEAD
 *   The mutex is a robust mutex and the previous owning thread terminated
 *   while holding the mutex lock. The mutex lock is acquired by the calling
 *   thread and it is up to the new owner to make the state consistent (see
 *   mm_thr_mutex_consistent()).
 *
 * ENOTRECOVERABLE
 *   The mutex is a robust mutex and the state protected by it is not
 *   recoverable.
 */
API_EXPORTED
int mm_thr_mutex_lock(mm_thr_mutex_t* mutex)
{
	return pthread_mutex_lock(mutex);
}


/**
 * mm_thr_mutex_trylock() - try to lock a mutex
 * @mutex:      initialized mutex
 *
 * This function is equivalent to mm_thr_mutex_lock(), except that if the
 * mutex object referenced by @mutex is currently locked (by any thread,
 * including the current thread), the call returns immediately.
 *
 * If @mutex is a robust mutex, and the previous owner has died while
 * holding the lock, the return value EOWNERDEAD will indicate the calling
 * thread of this situation. In this case, the mutex is locked by the
 * calling thread but the state it protects is marked as inconsistent. The
 * application should ensure that the state is made consistent for reuse and
 * when that is complete call mm_thr_mutex_consistent(). If the application is
 * unable to recover the state, it should unlock the mutex without a prior
 * call to mm_thr_mutex_consistent(), after which the mutex is marked
 * permanently unusable.
 *
 * NOTE: If @mutex is a robust mutex and actually used across different
 * processes, the return value must not be ignored.
 *
 * Return:
 *
 * 0
 *   The mutex has been locked by the calling thread
 *
 * EOWNERDEAD
 *   The mutex is a robust mutex and the previous owning thread terminated
 *   while holding the mutex lock. The mutex lock is acquired by the calling
 *   thread and it is up to the new owner to make the state consistent (see
 *   mm_thr_mutex_consistent()).
 *
 * ENOTRECOVERABLE
 *   The mutex is a robust mutex and the state protected by it is not
 *   recoverable.
 *
 * EBUSY
 *   The mutex could not be acquired because it has already been locked by a
 *   thread.
 */
API_EXPORTED
int mm_thr_mutex_trylock(mm_thr_mutex_t* mutex)
{
	return pthread_mutex_trylock(mutex);
}


/**
 * mm_thr_mutex_consistent() - mark state protected by mutex as consistent
 * @mutex:      initialized robust mutex
 *
 * If mutex is a robust mutex in an inconsistent state, this function can be
 * used to mark the state protected by the mutex referenced by @mutex as
 * consistent again.
 *
 * If an owner of a robust mutex terminates while holding the mutex, the
 * mutex becomes inconsistent and the next thread that acquires the mutex
 * lock is notified of the state by the return value EOWNERDEAD. In this
 * case, the mutex does not become normally usable again until the state is
 * marked consistent.
 *
 * If the new owner is not able to make the state consistent, do not call
 * mm_thr_mutex_consistent() for the mutex, but simply unlock the mutex. All
 * waiters will then be woken up and all subsequent calls to
 * mm_thr_mutex_lock() will fail to acquire the mutex by returning
 * ENOTRECOVERABLE error code.
 *
 * If the thread which acquired the mutex lock with the return value
 * EOWNERDEAD terminates before calling either mm_thr_mutex_consistent() or
 * mm_thr_mutex_unlock(), the next thread that acquires the mutex lock shall be
 * notified about the state of the mutex by the return value EOWNERDEAD.
 *
 * Return:
 *
 * 0
 *   in case of success
 *
 * EINVAL
 *   the mutex object is not robust or does not protect an inconsistent
 *   state
 *
 * EPERM
 *   the mutex is a robust mutex, and the current thread does not own the
 *   mutex.
 */
API_EXPORTED
int mm_thr_mutex_consistent(mm_thr_mutex_t* mutex)
{
#if HAVE_PTHREAD_MUTEX_CONSISTENT
	return pthread_mutex_consistent(mutex);
#else
	(void) mutex;
	mm_raise_error(ENOTSUP, "Robust mutex not supported on this platform");
	return ENOTSUP;
#endif
}


/**
 * mm_thr_mutex_unlock() - Unlock a mutex
 * @mutex:      mutex owned by the calling thread
 *
 * This releases the mutex object referenced by @mutex. If there are threads
 * blocked on the mutex object referenced by @mutex when mm_thr_mutex_unlock()
 * is called, one of these thread will be unblocked.
 *
 * Unlocking a mutex not owned by the calling thread, or not initialized
 * will result in undefined behavior.
 *
 * Return:
 *
 * 0
 *   in case of success
 *
 * EPERM
 *   the mutex is a robust mutex, and the current thread does not own the
 *   mutex.
 */
API_EXPORTED
int mm_thr_mutex_unlock(mm_thr_mutex_t* mutex)
{
	return pthread_mutex_unlock(mutex);
}


/**
 * mm_thr_mutex_deinit() - cleanup an initialized mutex
 * @mutex:      initialized mutex to destroy
 *
 * This destroys the mutex object referenced by @mutex; the mutex object
 * becomes, in effect, uninitialized. A destroyed mutex object can be
 * reinitialized using mm_thr_mutex_init(); the results of otherwise
 * referencing the object after it has been destroyed are undefined.
 *
 * It is safe to destroy an initialized mutex that is unlocked. Attempting
 * to destroy a locked mutex, or a mutex that another thread is attempting
 * to lock, or a mutex that is being used in a mm_thr_cond_timedwait() or
 * mm_thr_cond_wait() call by another thread, results in undefined behavior.
 *
 * Return: 0
 */
API_EXPORTED
int mm_thr_mutex_deinit(mm_thr_mutex_t* mutex)
{
	return pthread_mutex_destroy(mutex);
}


/**
 * mm_thr_cond_init() - Initialize a condition variable
 * @cond:       condition variable to initialize
 * @flags:      OR-combination of flags indicating the type of @cond
 *
 * Use this function to initialize @cond. The type of condition is
 * controlled by @flags which must contains one or several of the following:
 *
 * - MM_THR_PSHARED: init a condition shareable by other processes
 * - MM_THR_WAIT_MONOTONIC: the clock base used in mm_thr_cond_timedwait() is
 *   MM_CLK_MONOTONIC instead of the default MM_CLK_REALTIME.
 *
 * If 0 is passed, a call to this function could have be avoided if the data
 * pointed by @cond had been statically initialized with
 * MM_COND_INITIALIZER.
 *
 * It is undefined behavior if a condition variable is reinitialized before
 * getting destroyed first.
 *
 * Return: 0
 */
API_EXPORTED
int mm_thr_cond_init(mm_thr_cond_t* cond, int flags)
{
	int ret;
	pthread_condattr_t attr;

	if (flags) {
		pthread_condattr_init(&attr);

		if (flags & MM_THR_PSHARED)
			pthread_condattr_setpshared(&attr,
			                            PTHREAD_PROCESS_SHARED);

		if (flags & MM_THR_WAIT_MONOTONIC)
			pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	}

	ret = pthread_cond_init(cond, flags ? &attr : NULL);
	if (ret)
		mm_raise_error(ret, "Failed initializing cond: %s",
		               strerror(ret));

	if (flags)
		pthread_condattr_destroy(&attr);

	return ret;
}


/**
 * mm_thr_cond_wait() - wait on a condition
 * @cond:       Condition to wait
 * @mutex:      mutex protecting the condition wait update
 *
 * This function blocks on a condition variable. The application shall
 * ensure that this function is called with @mutex locked by the calling
 * thread; otherwise, undefined behavior results.
 *
 * It atomically releases @mutex and cause the calling thread to block on
 * the condition variable @cond. Upon successful return, the mutex shall
 * have been locked and shall be owned by the calling thread. If mutex is a
 * robust mutex where an owner terminated while holding the lock and the
 * state is recoverable, the mutex shall be acquired even though the
 * function returns an error code.
 *
 * When a thread waits on a condition variable, having specified a
 * particular mutex to either the mm_thr_cond_wait(), a dynamic binding is
 * formed between that mutex and condition variable that remains in effect
 * as long as at least one thread is blocked on the condition variable.
 * During this time, the effect of an attempt by any thread to wait on that
 * condition variable using a different mutex is undefined.
 *
 * The behavior is undefined if the value specified by the @cond or @mutex
 * argument to these functions does not refer to an initialized condition
 * variable or an initialized mutex object, respectively.
 *
 * NOTE: If @mutex is a robust mutex and actually used across different
 * processes, the return value must not be ignored.
 *
 * Return:
 *
 * 0
 *   in case success
 *
 * Other errors
 *   Any error that mm_thr_mutex_unlock() and mm_thr_mutex_lock() can return.
 */
API_EXPORTED
int mm_thr_cond_wait(mm_thr_cond_t* cond, mm_thr_mutex_t* mutex)
{
	return pthread_cond_wait(cond, mutex);
}


/**
 * mm_thr_cond_timedwait() - wait on a condition with timeout
 * @cond:       Condition to wait
 * @mutex:      mutex protecting the condition wait update
 * @abstime:    absolute time indicating the timeout
 *
 * This function is the equivalent to mm_thr_cond_wait(), except that an
 * error is returned if the absolute time specified by @time passes (that
 * is, clock time equals or exceeds @time) before the condition cond is
 * signaled or broadcasted, or if the absolute time specified by @time has
 * already been passed at the time of the call. When such timeouts occur,
 * mm_thr_cond_timedwait() will nonetheless release and re-acquire the mutex
 * referenced by mutex, and may consume a condition signal directed
 * concurrently at the condition variable.
 *
 * The clock ID to measure timeout is determined at the initialization of
 * the condition with pthread_cond_init(). If @cond has been initialized
 * statically with MM_MTX_INITIALIZER, the clock used is MM_CLK_REALTIME.
 *
 * NOTE: If @mutex is a robust mutex and actually used across different
 * processes, the return value must not be ignored.
 *
 * Return:
 *
 * 0
 *   in case success
 *
 * ETIMEDOUT
 *   The time specified by @time has passed
 *
 * EINVAL
 *   @time argument specifies a nanosecond value less than zero or greater
 *   than or equal to 1000 million.
 *
 * Other errors
 *   Any error that mm_thr_mutex_unlock() and mm_thr_mutex_lock() can return.
 */
API_EXPORTED
int mm_thr_cond_timedwait(mm_thr_cond_t* cond, mm_thr_mutex_t* mutex,
                          const struct timespec* abstime)
{
	return pthread_cond_timedwait(cond, mutex, abstime);
}


/**
 * mm_thr_cond_signal() - signal a condition
 * @cond:       condition variable to signal
 *
 * This function unblocks at least one of the threads that are blocked on
 * the specified condition variable @cond (if any threads are blocked on
 * @cond).
 *
 * mm_thr_cond_signal() functions may be called by a thread whether or not it
 * currently owns the mutex associated to the condition variable
 * (association made by threads calling mth_cond_wait() or
 * mm_thr_cond_timedwait() with the condition variable); however, if
 * predictable scheduling behavior is required, then that mutex shall be
 * locked by the thread calling mm_thr_cond_signal().
 *
 * The behavior is undefined if the value specified by the @cond argument
 * does not refer to an initialized condition variable.
 *
 * Return: 0
 */
API_EXPORTED
int mm_thr_cond_signal(mm_thr_cond_t* cond)
{
	return pthread_cond_signal(cond);
}


/**
 * mm_thr_cond_broadcast() - broadcast a condition
 * @cond:       condition variable to broadcast
 *
 * This function unblocks all threads currently blocked on the specified
 * condition variable @cond (if any threads are blocked on @cond).
 *
 * mm_thr_cond_broadcast() functions may be called by a thread whether or not
 * it currently owns the mutex associated to the condition variable
 * (association made by threads calling mth_cond_wait() or
 * mm_thr_cond_timedwait() with the condition variable); however, if
 * predictable scheduling behavior is required, then that mutex shall be
 * locked by the thread calling mm_thr_cond_broadcast().
 *
 * The behavior is undefined if the value specified by the @cond argument
 * does not refer to an initialized condition variable.
 *
 * Return: 0
 */
API_EXPORTED
int mm_thr_cond_broadcast(mm_thr_cond_t* cond)
{
	return pthread_cond_broadcast(cond);
}


/**
 * mm_thr_cond_deinit() - cleanup an initialized condition variable
 * @cond:       initialized condition variable to destroy
 *
 * This destroys the condition variable object referenced by @cond which
 * becomes, in effect, uninitialized. A destroyed condition variable object
 * can be reinitialized using mm_thr_cond_init(); the results of otherwise
 * referencing the object after it has been destroyed are undefined.
 *
 * It is safe to destroy an initialized condition variable that is not
 * waited. Attempting to destroy a condition variable waited by another
 * thread undefined behavior.
 *
 * Return: 0
 */
API_EXPORTED
int mm_thr_cond_deinit(mm_thr_cond_t* cond)
{
	return pthread_cond_destroy(cond);
}


/**
 * mm_thr_once() - One-time initialization
 * @once:               control data of the one-time call
 * @once_routine:       routine to call only once
 *
 * The first call to mm_thr_once() by any thread in a process, with a given
 * once_control, shall call @once_routine with no arguments. Subsequent
 * calls of mm_thr_once() with the same once_control shall not call
 * @once_routine. On return from mm_thr_once(), init_routine shall have
 * completed. The once_control parameter shall determine whether the
 * associated initialization routine has been called.
 *
 * Return: 0
 */
API_EXPORTED
int mm_thr_once(mm_thr_once_t* once, void (* once_routine)(void))
{
	return pthread_once(once, once_routine);
}


/**
 * mm_thr_create() - thread creation
 * @thread:      location to store the ID of the new thread
 * @proc:        routine to execute in the thread
 * @arg:         argument passed to @proc
 *
 * This functions create a new thread. The thread is created executing
 * start_routine with arg as its sole argument. Upon successful creation,
 * mm_thr_create() shall store the ID of the created thread in the location
 * referenced by thread.
 *
 * Once a thread has been successfully created, its resources will have
 * eventually to be reclaimed. This is achieved by calling
 * mm_thr_join() or mm_thr_detach() later.
 *
 * Return: 0 in case of success, otherwise the associated error code with
 * error state set accordingly.
 */
API_EXPORTED
int mm_thr_create(mm_thread_t* thread, void* (*proc)(void*), void* arg)
{
	int ret;

	ret = pthread_create(thread, NULL, proc, arg);
	if (ret)
		mm_raise_error(ret, "Failed creating thread: %s",
		               strerror(ret));

	return ret;
}


/**
 * mm_thr_join() - wait for thread termination
 * @thread:     ID of the thread to wait
 * @value_ptr:  location receiving the return value
 *
 * The mm_thr_join() function suspends execution of the calling thread until
 * the target thread terminates, unless the target thread has already
 * terminated. On return from a successful mm_thr_join() call with a
 * non-NULL @value_ptr argument, the value returned by the terminating
 * thread shall be made available in the location referenced by @value_ptr.
 *
 * The behavior is undefined if the value specified by the thread argument
 * to mm_thr_join() does not refer to a joinable thread as well if the
 * @thread argument refers to the calling thread.
 *
 * Return: 0
 */
API_EXPORTED
int mm_thr_join(mm_thread_t thread, void** value_ptr)
{
	int ret;

	ret = pthread_join(thread, value_ptr);
	if (ret)
		mm_raise_error(ret, "Failed to join thread: %s", strerror(ret));

	return ret;
}


/**
 * mm_thr_detach() - Detach a thread
 * @thread:     ID of the thread to detach
 *
 * This function indicates that thread storage for the thread @thread can be
 * reclaimed when that thread terminates. In other words, this makes @thread
 * detached or not joinable.
 *
 * The behavior is undefined if the value specified by the thread argument
 * to mm_thr_detach() does not refer to a joinable thread.
 *
 * Return: 0 in case of success, otherwise the associated error code with
 * error state set accordingly.
 */
API_EXPORTED
int mm_thr_detach(mm_thread_t thread)
{
	int ret;

	ret = pthread_detach(thread);
	if (ret)
		mm_raise_error(ret, "Failed to detach thread: %s",
		               strerror(ret));

	return ret;
}


/**
 * mm_thr_self() - get the calling thread ID
 *
 * Return: thread ID of the calling thread.
 */
API_EXPORTED
mm_thread_t mm_thr_self(void)
{
	return pthread_self();
}
