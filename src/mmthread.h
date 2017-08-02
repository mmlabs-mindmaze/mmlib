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

/**
 * mmthr_mtx_init() - Initialize a mutex
 * @mutex:      mutex to initialize
 * @flags:      OR-combination of flags indicating the type of mutex
 *
 * Use this function to initialize @mutex. The type of mutex is controlled
 * by @flags which must contains one or several of the following:
 *
 * - MMTHR_PSHARED: init a mutex shareable by other processes. When a mutex
 * is process shared, it is also a robust mutex.
 *
 * If no flags is provided, the type of initialized mutex just a normal
 * mutex and a call to this function could be avoided if the data pointed by
 * @mutex has been statically initialized with MMTHR_MTX_INITIALIZER.
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
MMLIB_API int mmthr_mtx_init(mmthr_mtx_t* mutex, int flags);


/**
 * mmthr_mtx_lock() - lock a mutex
 * @mutex:      initialized mutex
 *
 * The mutex object referenced by @mutex is locked by a successful
 * call to mmthr_mtx_lock(). If the mutex is already locked by another
 * thread, the calling thread blocks until the mutex becomes available. If
 * the mutex is already locked by the calling thread, the function will
 * never return.
 *
 * If @mutex is a robust mutex, and the previous owner has died while
 * holding the lock, the return value EOWNERDEAD will indicate the calling
 * thread of this situation. In this case, the mutex is locked by the
 * calling thread but the state it protects is marked as inconsistent. The
 * application should ensure that the state is made consistent for reuse and
 * when that is complete call mmthr_mtx_consistent(). If the application is
 * unable to recover the state, it should unlock the mutex without a prior
 * call to mmthr_mtx_consistent(), after which the mutex is marked
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
 *   mmthr_mtx_consistent()).
 *
 * ENOTRECOVERABLE
 *   The mutex is a robust mutex and the state protected by it is not
 *   recoverable.
 */
MMLIB_API int mmthr_mtx_lock(mmthr_mtx_t* mutex);


/**
 * mmthr_mtx_trylock() - try to lock a mutex
 * @mutex:      initialized mutex
 *
 * This function is equivalent to mmthr_mtx_lock(), except that if the
 * mutex object referenced by @mutex is currently locked (by any thread,
 * including the current thread), the call returns immediately.
 *
 * If @mutex is a robust mutex, and the previous owner has died while
 * holding the lock, the return value EOWNERDEAD will indicate the calling
 * thread of this situation. In this case, the mutex is locked by the
 * calling thread but the state it protects is marked as inconsistent. The
 * application should ensure that the state is made consistent for reuse and
 * when that is complete call mmthr_mtx_consistent(). If the application is
 * unable to recover the state, it should unlock the mutex without a prior
 * call to mmthr_mtx_consistent(), after which the mutex is marked
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
 *   mmthr_mtx_consistent()).
 *
 * ENOTRECOVERABLE
 *   The mutex is a robust mutex and the state protected by it is not
 *   recoverable.
 *
 * EBUSY
 *   The mutex could not be acquired because it has already been locked by a
 *   thread.
 */
MMLIB_API int mmthr_mtx_trylock(mmthr_mtx_t* mutex);


/**
 * mmthr_mtx_consistent() - mark state protected by mutex as consistent
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
 * mmthr_mtx_consistent() for the mutex, but simply unlock the mutex. All
 * waiters will then be woken up and all subsequent calls to
 * mmthr_mtx_lock() will fail to acquire the mutex by returning
 * ENOTRECOVERABLE error code.
 *
 * If the thread which acquired the mutex lock with the return value
 * EOWNERDEAD terminates before calling either mmthr_mtx_consistent() or
 * mmthr_mtx_unlock(), the next thread that acquires the mutex lock shall be
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
MMLIB_API int mmthr_mtx_consistent(mmthr_mtx_t* mutex);


/**
 * mmthr_mtx_unlock() - Unlock a mutex
 * @mutex:      mutex owned by the calling thread
 *
 * This releases the mutex object referenced by @mutex. If there are threads
 * blocked on the mutex object referenced by @mutex when mmthr_mtx_unlock()
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
MMLIB_API int mmthr_mtx_unlock(mmthr_mtx_t* mutex);


/**
 * mmthr_mtx_deinit() - cleanup an initialized mutex
 * @mutex:      intialized mutex to destroy
 *
 * This destroys the mutex object referenced by @mutex; the mutex object
 * becomes, in effect, uninitialized. A destroyed mutex object can be
 * reinitialized using mmthr_mtx_init(); the results of otherwise
 * referencing the object after it has been destroyed are undefined.
 *
 * It is safe to destroy an initialized mutex that is unlocked. Attempting
 * to destroy a locked mutex, or a mutex that another thread is attempting
 * to lock, or a mutex that is being used in a mmthr_cond_timedwait() or
 * mmthr_cond_wait() call by another thread, results in undefined behavior.
 *
 * Return: 0
 */
MMLIB_API int mmthr_mtx_deinit(mmthr_mtx_t* mutex);


/**
 * mmthr_cond_init() - Initialize a condition variable
 * @cond:       condition variable to initialize
 * @flags:      OR-combination of flags indicating the type of @cond
 *
 * Use this function to initialize @cond. The type of condition is
 * controlled by @flags which must contains one or several of the following:
 *
 * - MMTHR_PSHARED: init a condition shareable by other processes
 * - MMTHR_WAIT_MONOTONIC: the clock base used in mmthr_cond_timedwait() is
 *   MM_CLK_MONOTONIC instead of the default MM_CLK_REALTIME.
 *
 * If 0 is passed, a call to this function could have be avoided if the data
 * pointed by @cond had been statically initialized with
 * MMTHR_COND_INITIALIZER.
 *
 * It is undefined behavior if a condition variable is reinitialized before getting
 * destroyed first.
 *
 * Return: 0
 */
MMLIB_API int mmthr_cond_init(mmthr_cond_t* cond, int flags);


/**
 * mmthr_cond_wait() - wait on a condition
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
 * particular mutex to either the mmthr_cond_wait(), a dynamic binding is
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
 *   Any error that mmthr_mtx_unlock() and mmthr_mtx_lock() can return.
 */
MMLIB_API int mmthr_cond_wait(mmthr_cond_t* cond, mmthr_mtx_t* mutex);


/**
 * mmthr_cond_timedwait() - wait on a condition with timeout
 * @cond:       Condition to wait
 * @mutex:      mutex protecting the condition wait update
 * @abstime:    absolute time indicating the timeout
 *
 * This function is the equivalent to mmthr_cond_wait(), except that an
 * error is returned if the absolute time specified by @time passes (that
 * is, clock time equals or exceeds @time) before the condition cond is
 * signaled or broadcasted, or if the absolute time specified by @time has
 * already been passed at the time of the call. When such timeouts occur,
 * mmthr_cond_timedwait() will nonetheless release and re-acquire the mutex
 * referenced by mutex, and may consume a condition signal directed
 * concurrently at the condition variable.
 *
 * The clock ID to measure timeout is determined at the initialization of
 * the condition with pthread_cond_init(). If @cond has been initialized
 * statically with MMTHR_MTX_INITIALIZER, the clock used is MM_CLK_REALTIME.
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
 *   Any error that mmthr_mtx_unlock() and mmthr_mtx_lock() can return.
 */
MMLIB_API int mmthr_cond_timedwait(mmthr_cond_t* cond, mmthr_mtx_t* mutex,
                                   const struct timespec* abstime);

/**
 * mmthr_cond_signal() - signal a condition
 * @cond:       condition variable to signal
 *
 * This function unblocks at least one of the threads that are blocked on
 * the specified condition variable @cond (if any threads are blocked on
 * @cond).
 *
 * mmthr_cond_signal() functions may be called by a thread whether or not it
 * currently owns the mutex associated to the condition variable
 * (association made by threads calling mth_cond_wait() or
 * mmthr_cond_timedwait() with the condition variable); however, if
 * predictable scheduling behavior is required, then that mutex shall be
 * locked by the thread calling mmthr_cond_signal().
 *
 * The behavior is undefined if the value specified by the @cond argument
 * does not refer to an initialized condition variable.
 *
 * Return: 0
 */
MMLIB_API int mmthr_cond_signal(mmthr_cond_t* cond);


/**
 * mmthr_cond_broadcast() - broadcast a condition
 * @cond:       condition variable to broadcast
 *
 * This function unblocks all threads currently blocked on the specified
 * condition variable @cond (if any threads are blocked on @cond).
 *
 * mmthr_cond_broadcast() functions may be called by a thread whether or not
 * it currently owns the mutex associated to the condition variable
 * (association made by threads calling mth_cond_wait() or
 * mmthr_cond_timedwait() with the condition variable); however, if
 * predictable scheduling behavior is required, then that mutex shall be
 * locked by the thread calling mmthr_cond_broadcast().
 *
 * The behavior is undefined if the value specified by the @cond argument
 * does not refer to an initialized condition variable.
 *
 * Return: 0
 */
MMLIB_API int mmthr_cond_broadcast(mmthr_cond_t* cond);


/**
 * mmthr_cond_deinit() - cleanup an initialized condition variable
 * @cond:       intialized condition variable to destroy
 *
 * This destroys the condition variable object referenced by @cond which
 * becomes, in effect, uninitialized. A destroyed condition variable object
 * can be reinitialized using mmthr_cond_init(); the results of otherwise
 * referencing the object after it has been destroyed are undefined.
 *
 * It is safe to destroy an initialized condition variable that is not
 * waited. Attempting to destroy a condition variable waited by another
 * thread undefined behavior.
 *
 * Return: 0
 */
MMLIB_API int mmthr_cond_deinit(mmthr_cond_t* cond);


/**
 * mmthr_once() - One-time initialization
 * @once:               control data of the one-time call
 * @once_routine:       routine to call only once
 *
 * The first call to mmthr_once() by any thread in a process, with a given
 * once_control, shall call @once_routine with no arguments. Subsequent
 * calls of mmthr_once() with the same once_control shall not call
 * @once_routine. On return from mmthr_once(), init_routine shall have
 * completed. The once_control parameter shall determine whether the
 * associated initialization routine has been called.
 *
 * Return: 0
 */
MMLIB_API int mmthr_once(mmthr_once_t* once, void (*once_routine)(void));


/**
 * mmthr_create() - thread creation
 * @thread:      location to store the ID of the new thread
 * @proc:        routine to execute in the thread
 * @arg:         argument passed to @proc
 *
 * This functions create a new thread. The thread is created executing
 * start_routine with arg as its sole argument. Upon successful creation,
 * mmthr_create() shall store the ID of the created thread in the location
 * referenced by thread.
 *
 * Once a thread has been successfully created, its resources will have
 * eventually to be reclaimed. This is achieved by calling
 * mmthr_join() or mmthr_detach() later.
 *
 * Return: 0 in case of success, otherwise the associated error code with
 * error state set accordingly.
 */
MMLIB_API int mmthr_create(mmthread_t* thread, void* (*proc)(void*), void* arg);


/**
 * mmthr_join() - wait for thread termination
 * @thread:     ID of the thread to wait
 * @value_ptr:  location receiving the return value
 *
 * The mmthr_join() function suspends execution of the calling thread until
 * the target thread terminates, unless the target thread has already
 * terminated. On return from a successful mmthr_join() call with a
 * non-NULL @value_ptr argument, the value returned by the terminating
 * thread shall be made available in the location referenced by @value_ptr.
 *
 * The behavior is undefined if the value specified by the thread argument
 * to mmthr_join() does not refer to a joinable thread as well if the
 * @thread argument refers to the calling thread.
 *
 * Return: 0
 */
MMLIB_API int mmthr_join(mmthread_t thread, void** value_ptr);


/**
 * mmthr_detach() - Detach a thread
 * @thread:     ID of the thread to detach
 *
 * This function indicates that thread storage for the thread @thread can be
 * reclaimed when that thread terminates. In other words, this makes @thread
 * detached or not joinable.
 *
 * The behavior is undefined if the value specified by the thread argument
 * to mmthr_detach() does not refer to a joinable thread.
 *
 * Return: 0 in case of success, otherwise the associated error code with
 * error state set accordingly.
 */
MMLIB_API int mmthr_detach(mmthread_t thread);


/**
 * mmthr_self() - get the calling thread ID
 *
 * Return: thread ID of the calling thread.
 */
MMLIB_API mmthread_t mmthr_self(void);

#ifdef __cplusplus
}
#endif

#endif
