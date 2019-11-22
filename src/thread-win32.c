/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <windows.h>
#include <synchapi.h>
#include <stdbool.h>
#include <stdlib.h>
#include <process.h>


#include "mmthread.h"
#include "mmerrno.h"
#include "mmtime.h"
#include "mmlog.h"
#include "pshared-lock.h"
#include "atomic-win32.h"
#include "error-internal.h"
#include "mutex-lockval.h"
#include "utils-win32.h"

#ifdef _MSC_VER
#define restrict __restrict
#endif

#define STATE_STOPPED   0x01
#define STATE_DETACHED  0x02

#define NS_IN_MS    (NS_IN_SEC / MS_IN_SEC)

/**
 * struct mmthread - data structure to manipulate thread
 * @hnd:        WIN32 thread object handle
 * @routine:    pointer to routine to execute in the thread (NULL if thread not
 *              created by mmlib)
 * @arg:        argument passed to @routine (NULL if thread not created by mmlib)
 * @retval:     value returned by the thread when it terminates
 * @state:      state of the thread indicating if it is stopped or detached
 *
 * This structure represents the data necessary to manipulate thread through
 * the API of mmlib. The &typedef mmthread_t type corresponds to a pointer to this
 * structure layout.
 *
 * This structure should be freed when:
 * - the thread terminates in the case of a detached thread
 * - mmthr_join() is called in case of joinable thread
 */
struct mmthread {
	HANDLE hnd;
	void* (*routine)(void*);
	void* arg;
	void* retval;
	int64_t state;
};


/**
 * struct thread_local_data - thread local data handling threading in mmlib
 * @lockref:    data maintaining the communication with the lock referee.
 * @last_error: error state of the thread
 * @thread:     pointer to the thread manipulation structure. Can be NULL if
 *              thread has been created externally and mmthr_self() has not
 *              been called for the thread.
 *
 * This structure represents the actual thread local data used in mmlib with
 * respect with thread manipulation and thread synchronization.
 *
 * The lifetime of this structure must not be confused with the one of struct
 * mmthread. This structure is destroyed (if allocated) always when the thread
 * terminates.
 */
struct thread_local_data {
	struct lockref_connection lockref;
	struct error_info last_error;
	struct mmthread* thread;
};


/**************************************************************************
 *                                                                        *
 *                 Provide thread local data                              *
 *                                                                        *
 **************************************************************************/

static DWORD threaddata_tls_index;

/**
 * safe_alloc() - memory allocation callable from DllMain
 * @len:        size of the memory block to allocate.
 *
 * Allocate memory of size @len and initialize it to 0. This function is in
 * essence equivalent to calloc() but can safely be called from DllMain().
 * To free the allocated memory use safe_free(). The typical use is for
 * allocating thread local data.
 *
 * Return: the pointer to allocated memory in case of success, NULL
 * otherwise.
 */
static
void* safe_alloc(size_t len)
{
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}


/**
 * safe_free() - memory deallocation
 * @ptr:        pointer to data to be freed
 *
 * Free memory block @ptr that has been allocated by safe_alloc(). This
 * function can be called from DllMain(). This is typically where it is
 * used in DLL_THREAD_DETACH case, for example deallocating thread local
 * data.
 */
static
void safe_free(void* ptr)
{
	if (!ptr)
		return;

	HeapFree(GetProcessHeap(), 0, ptr);
}


static
struct mmthread* create_mmthread_data(void)
{
	struct mmthread* th;

	th = safe_alloc(sizeof(*th));
	if (!th) {
		mm_raise_from_w32err("Cannot allocate data for thread");
		return NULL;
	}

	th->state = STATE_DETACHED;
	th->hnd = NULL;

	return th;
}


static
void destroy_mmthread_data(struct mmthread* th)
{
	if (!th)
		return;

	if (th->hnd)
		CloseHandle(th->hnd);

	safe_free(th);
}



static NOINLINE
struct thread_local_data* allocate_thread_local_data(void)
{
	struct thread_local_data* data;

	data = safe_alloc(sizeof(*data));
	if (!data) {
		mmlog_fatal("Cannot allocate thread private data");
		abort();
	}

	// Set allocated data as thread local data
	TlsSetValue(threaddata_tls_index, data);

	return data;
}


static
void thread_local_data_on_exit(void)
{
	struct thread_local_data* data;
	struct mmthread* self;
	int64_t prev_state;

	// Retrieve currently currently set thread local data
	data = tls_get_value(threaddata_tls_index);
	if (!data)
		return;

	// If thread structure is initialized and is detached, it must be
	// cleaned up now
	self = data->thread;
	if (self) {
		prev_state = atomic_fetch_add(&self->state, STATE_STOPPED);
		if (prev_state & STATE_DETACHED)
			destroy_mmthread_data(self);
	}

	// Close connection of the thread with lock server if any.
	deinit_lock_referee_connection(&data->lockref);

	safe_free(data);
}


LOCAL_SYMBOL
BOOL WINAPI DllMain(HINSTANCE hdll, DWORD reason, LPVOID reserved)
{
	(void)hdll;
	(void)reserved;

	switch(reason) {
	case DLL_PROCESS_ATTACH:
		threaddata_tls_index = TlsAlloc();
		if (threaddata_tls_index == TLS_OUT_OF_INDEXES) {
			return FALSE;
		}
		break;

	case DLL_PROCESS_DETACH:
		TlsFree(threaddata_tls_index);
		break;

	case DLL_THREAD_DETACH:
		thread_local_data_on_exit();
		break;
	}

	return TRUE;
}


static
struct thread_local_data* get_thread_local_data(void)
{
	struct thread_local_data* data;

	data = tls_get_value(threaddata_tls_index);
	if (LIKELY(data))
		return data;

	return allocate_thread_local_data();
}


static
struct lockref_connection* get_thread_lockref_data(void)
{
	return &(get_thread_local_data()->lockref);
}


LOCAL_SYMBOL
struct error_info* get_thread_last_error(void)
{
	return &(get_thread_local_data()->last_error);
}


/**************************************************************************
 *                                                                        *
 *                           Process shared mutex                         *
 *                                                                        *
 **************************************************************************/

/**
 * DOC: process shared mutex implementation
 *
 * The process shared mutex is based on a 64bit lock val which is shared
 * (memory mapped if over different process) and whose change are all done
 * atomically. This 64bits lock value is divided in 2 parts of 32bits: - the
 * lower one indicates the owning thread id (32bit on windows) - the higher one
 * encodes the number of thread waiting for the lock
 *
 * By manipulating the lock value only through atomic addition/subtraction,
 * the lock value is updated only we avoid any problem of lost wake-up
 *
 * When the lock is tried to be obtained, a thread attempts to update
 * atomically the lock value with its thread ID combined with the number of
 * thread it believes that are waiting for the lock. This defines a new lock
 * value that the thread will try to store atomically in the address of the
 * lock. If the number of waiting thread is not the expected one or another
 * thread has already obtained the lock the expected lock value will mismatch
 * and the store will fail. With this a thread can know if it has obtained the
 * lock or not. If not it will increase atomically the number of waiting thread
 * and will go sleeping using pshared_wait_on_lock().
 *
 * When unlocking, a thread just has to subtract its thread ID from the lock
 * value: a null value in the owner part of the lock value indicates the lock
 * in unused (but some other threads may still wait for it). The unlocking
 * thread just has to examine the number of thread waiting to know if it must
 * wakeup any thread (using pshared_wake_lock())
 */


/**
 * start_mtx_operation() - Initiate a lock/unlock operation on a mutex
 * @mtx_key:    key of the pshared lock of the mutex
 * @robust_data: robust data of the calling thread
 *
 * Indicates the beginning of a mutex lock or mutex unlock and is the
 * symmetric of finish_mtx_lock() or finish_mtx_unlock(). This will register
 * the mutex lock key into the robust data of the thread.
 */
static
void start_mtx_operation(int64_t mtx_key,
                         struct robust_data* robust_data)
{
	robust_data->attempt_key = mtx_key;
}


/**
 * register_waiter_in_mtx() - register a thread trying to lock a mutex.
 * @lock:       pointer to the shared lock
 * @poldval:    pointer to a variable that old the previous value of the lock.
 *              The pointed variable will be updated, this acts as input and
 *              output of the function.
 * @robust_data: robust data of the calling thread
 *
 * This function is meant to be called when the calling fails to lock the
 * mutex. It update the lock of a mutex referenced in @lock, as well as the
 * robust data of the calling thread. This function must be called to be called
 * between start_mtx_operation() and finish_mtx_lock().
 *
 * There are 2 reasons why a wait registration fails:
 * - the mutex has reached its maximal count of registered waiter in the mutex
 * - the mutex is permanently unusable.
 * If the registration fails due to waiter list full, it can be attempted later
 * if still needed.
 *
 * Return: true if the thread is now registered as waiter of mutex, false
 * otherwise.
 */
static
bool register_waiter_in_mtx(int64_t* restrict lock, int64_t* restrict poldval,
                            struct robust_data* robust_data)
{
	int64_t newval, incval;
	DWORD tid = 0;

	tid = robust_data->thread_id;
	incval = mtx_lockval(0, tid, 1);

	// Register as waiter: update waiter count and get its lock
	while (1) {
		// Check waiter count has not reached the max. If so, give
		// other thread the chance to run and retry when the thread
		// is rescheduled
		if (UNLIKELY(is_mtx_waiterlist_full(*poldval))) {
			Sleep(1);
			*poldval = atomic_load(lock);
			return false;
		}

		// try to update the waiter count
		newval = (*poldval & ~MTX_WAITER_TID_MASK) + incval;
		if (atomic_cmp_exchange(lock, poldval, newval))
			break;

		if (is_mtx_unrecoverable(*poldval))
			return false;
	}

	// Notify thread's robust data that it is a waiter of the key
	robust_data->is_waiter = 1;

	// Now that robust data has been updated, we can release the waiter
	// count lock
	*poldval = atomic_fetch_sub(lock, mtx_lockval(0, tid, 0)) & ~MTX_WAITER_TID_MASK;

	return true;
}


/**
 * finish_mtx_lock() - finish the mutex lock operation
 * @robust_data: robust data of the calling thread
 * @locked:     true if calling thread owns the mutex lock
 * @oldval:     last lock value of the mutex observed
 *
 * This function is meant to be called at the end of mutex lock attempt
 * after start_mtx_operation(). It will update the robust data of the mutex.
 * It will also return the return code that must be returned to the lock
 * function.
 *
 * Return: the return value to report to the calling lock function.
 */
static
int finish_mtx_lock(struct robust_data* robust_data, bool locked, int64_t oldval)
{
	int64_t mtx_key;
	int retval;

	retval = locked ? 0 : EBUSY;

	if (locked) {
		mtx_key = robust_data->attempt_key;
		robust_data->locked_keys[robust_data->num_locked] = mtx_key;
		robust_data->num_locked++;
		if (is_mtx_ownerdead(oldval))
			retval = EOWNERDEAD;
	}

	robust_data->is_waiter = 0;
	robust_data->attempt_key = 0;

	return retval;
}


/**
 * finish_mtx_unlock() - finish the mutex unlock operation
 * @robust_data: robust data of the calling thread
 *
 * This function is meant to be called at the end of mutex unlock after
 * start_mtx_operation(). It will update the robust data of the thread.
 */
static
void finish_mtx_unlock(struct robust_data* robust_data)
{
	robust_data->num_locked--;
	robust_data->attempt_key = 0;
}


/**
 * pshared_mtx_init() - Mutex init in case of MMTHR_PSHARED
 * @mutex:      mutex to initialize
 *
 * Implementation of mmthr_mtx_init() in the case of process shared mutex.
 *
 * Return: always 0
 */
static
int pshared_mtx_init(struct mmthr_mtx_pshared * mutex)
{
	struct lockref_connection* lockref = get_thread_lockref_data();

	mutex->lock = 0;
	mutex->pshared_key = pshared_init_lock(lockref);

	return 0;
}


/**
 * pshared_mtx_lock() - Mutex lock in case of MMTHR_PSHARED
 * @mutex:      mutex to lock
 *
 * Implementation of mmthr_mtx_lock() in the case of process shared mutex.
 *
 * Return: Always 0 excepting if @mutex refers to a robust mutex in
 * inconsistent state (EOWNERDEAD) or a robust mutex marked permanently
 * unusable (ENOTRECOVERABLE).
 */
static
int pshared_mtx_lock(struct mmthr_mtx_pshared * mutex)
{
	struct lockref_connection* lockref;
	struct robust_data* robust_data;
	int64_t oldval, newval, incval;
	int64_t* lockptr = &mutex->lock;
	int is_waiting_notified;
	struct shared_lock shlock = {
		.key = mutex->pshared_key,
		.ptr = &mutex->lock,
	};

	lockref = get_thread_lockref_data();
	robust_data = pshared_get_robust_data(lockref);

	start_mtx_operation(mutex->pshared_key, robust_data);

	// Assume initially that no other thread are waiting. If it is not the
	// case, we will discover this the next time the lock value will be
	// read
	incval = mtx_lockval(get_tid(), 0, 0);
	oldval = 0;
	is_waiting_notified = 0;
	while (1) {
		// Try to get the lock. New value is the current number of
		// waiter observed + the thread ID of this thread
		newval = oldval + incval;
		if (LIKELY(atomic_cmp_exchange(lockptr, &oldval, newval))) {
			break;  // we got the lock
		}

		if (UNLIKELY(is_mtx_unrecoverable(oldval))) {
			finish_mtx_lock(robust_data, false, oldval);
			return ENOTRECOVERABLE;
		}

		// If this is the first time we failed to get the lock, we need
		// to indicate in the lock value that we are waiting for it,
		// ie, increasing the number of waiters part of the lock
		if (!is_waiting_notified) {
			if (!register_waiter_in_mtx(lockptr, &oldval, robust_data))
				continue;

			oldval &= ~MTX_OWNER_TID_MASK;
			incval -= mtx_lockval(0, 0, 1);
			is_waiting_notified = 1;
			continue;
		}

		// We are ready to wait
		pshared_wait_on_lock(lockref, shlock, 1, NULL);
		oldval = atomic_load(lockptr) & ~MTX_OWNER_TID_MASK;
	}

	return finish_mtx_lock(robust_data, true, oldval);
}


/**
 * pshared_mtx_trylock() - Mutex try lock in case of MMTHR_PSHARED
 * @mutex:      mutex to lock
 *
 * Implementation of mmthr_mtx_trylock() in the case of process shared mutex.
 *
 * Return: Always 0 excepting if @mutex is in inconsistent state
 * (EOWNERDEAD) or marked permanently unusable (ENOTRECOVERABLE).
 */
static
int pshared_mtx_trylock(struct mmthr_mtx_pshared * mutex)
{
	struct robust_data* robust_data = NULL;
	int64_t oldval, newval;
	int i;
	bool locked;

	robust_data = pshared_get_robust_data(get_thread_lockref_data());

	start_mtx_operation(mutex->pshared_key, robust_data);

	oldval = 0;
	newval = mtx_lockval(get_tid(), 0, 0);
	locked = false;

	// try lock twice. The first time, it assumes the lock value is totally
	// clean. The second time, it will try to base the new value with what
	// has been seen during the first attempt. The second time is
	// particularly necessary for robust mutex that need recovery (the need
	// recover bit is then set)
	for (i = 0; i < 2; i++) {
		if (atomic_cmp_exchange(&mutex->lock, &oldval, newval)) {
			locked = 1;
			break;
		}

		if (UNLIKELY(is_mtx_unrecoverable(oldval))) {
			finish_mtx_lock(robust_data, 0, oldval);
			return ENOTRECOVERABLE;
		}

		oldval &= ~MTX_OWNER_TID_MASK;
		newval += oldval;
	}

	return finish_mtx_lock(robust_data, locked, oldval);
}


/**
 * pshared_mtx_unlock() - Mutex try unlock in case of MMTHR_PSHARED
 * @mutex:      mutex to unlock
 *
 * Implementation of mmthr_mtx_unlock() in the case of process shared mutex.
 *
 * Return: Always 0.
 */
static
int pshared_mtx_unlock(struct mmthr_mtx_pshared * mutex)
{
	struct lockref_connection* lockref = NULL;
	struct robust_data* robust_data = NULL;
	int num_unlock, unrecoverable = 0;
	int64_t oldval, unlock_val;
	struct shared_lock shlock = {
		.key = mutex->pshared_key,
		.ptr = &mutex->lock,
	};

	unlock_val = mtx_lockval(get_tid(), 0, 0);
	num_unlock = 1;

	lockref = get_thread_lockref_data();
	robust_data = pshared_get_robust_data(lockref);

	// Check that inconsistent state has been removed by now
	if (is_mtx_ownerdead(atomic_load(&mutex->lock))) {
		unlock_val -= MTX_OWNER_TID_MASK;
		unrecoverable = 1;
	}

	start_mtx_operation(mutex->pshared_key, robust_data);

	// Do actual unlock (This is performed by subtracting the Thread ID
	// from the lock value. After this one, the owner part of the lock shall
	// be null, thus indicating that no one hold the lock
	oldval = atomic_fetch_sub(&mutex->lock, unlock_val);

	// Wake up a waiter if there is any
	if (is_mtx_waited(oldval)) {
		if (unrecoverable)
			num_unlock = mtx_num_waiter(oldval);

		pshared_wake_lock(lockref, shlock, 1, num_unlock);
	}

	finish_mtx_unlock(robust_data);
	return 0;
}


/**
 * pshared_mtx_consistent() - recover mutex from inconsistent state
 * @mutex:      robust mutex to recover
 *
 * Implementation of mmthr_mtx_consistent() in the case of process shared
 * robust mutex.
 *
 * Return: 0 in case of success. EINVAL if @mutex does not protect an
 * inconsistent state.
 */
static
int pshared_mtx_consistent(struct mmthr_mtx_pshared * mutex)
{
	int64_t lockval;

	lockval = atomic_load(&mutex->lock);

	if (!is_mtx_ownerdead(lockval))
		return EINVAL;

	atomic_sub(&mutex->lock, MTX_NEED_RECOVER_MASK);
	return 0;
}

/**************************************************************************
 *                                                                        *
 *                   Process shared condition variable                    *
 *                                                                        *
 **************************************************************************/

/**
 * pshared_cond_wait() - process shared condition variable wait
 * @cond:       condition variable initialized with MMTHR_PSHARED
 * @mutex:      mutex protecting the condition wait update
 * @abstime:    absolute time indicating the timeout or NULL in case of
                infinite wait.
 *
 * Implementation of mmthr_cond_wait() and mmthr_cond_timedwait() in the case
 * of process shared condition.
 *
 * Return: 0 in case of success, any error that mmthr_mtx_lock() can return, or
 * ETIMEDOUT if @abstime is not NULL and the wait has timedout
 */
static
int pshared_cond_wait(struct mmthr_cond_pshared * cond,
                      mmthr_mtx_t * mutex,
                      const struct timespec* abstime)
{
	struct lockref_connection* lockref = get_thread_lockref_data();
	int64_t wakeup_val;
	int wait_ret, ret;
	struct shared_lock shlock = {.key = cond->pshared_key};
	struct lock_timeout timeout, *timeout_ptr;

	timeout_ptr = NULL;
	if (abstime) {
		timeout.clk_flags = WAITCLK_FLAG_REALTIME;
		timeout.ts = *abstime;
		timeout_ptr = &timeout;
	}

	wakeup_val = atomic_fetch_add(&cond->waiter_seq, 1);

	mmthr_mtx_unlock(mutex);
	wait_ret = pshared_wait_on_lock(lockref, shlock, wakeup_val, timeout_ptr);
	ret = mmthr_mtx_lock(mutex);

	// Report return value of timed wait operation only if there is nothing
	// to report from mutex lock. This way, we cannot miss EOWNERDEAD or
	// ENOTRECOVERABLE that might be returned by mmthr_mtx_lock().
	if (abstime && !ret)
		ret = wait_ret;

	return ret;
}


/**
 * pshared_cond_signal() - signal process shared condition variable
 * @cond:       condition variable initialized with MMTHR_PSHARED
 *
 * Implementation of mmthr_cond_signal() in the case of process shared
 * condition.
 *
 * Return: always 0
 */
static
int pshared_cond_signal(struct mmthr_cond_pshared * cond)
{
	struct lockref_connection* lockref;
	int64_t wakeup_val, waiter_val, num_waiter;
	struct shared_lock shlock = {.key = cond->pshared_key};

	waiter_val = atomic_load(&cond->waiter_seq);
	wakeup_val = atomic_load(&cond->wakeup_seq);
	num_waiter = waiter_val - wakeup_val;

	if (num_waiter <= 0)
		return 0;

	lockref = get_thread_lockref_data();
	wakeup_val = atomic_fetch_add(&cond->wakeup_seq, 1);
	pshared_wake_lock(lockref, shlock, wakeup_val, 1);

	return 0;
}


/**
 * pshared_cond_broadcast() - broadcast process shared condition variable
 * @cond:       condition variable initialized with MMTHR_PSHARED
 *
 * Implementation of mmthr_cond_broadcast() in the case of process shared
 * condition.
 *
 * Return: always 0
 */
static
int pshared_cond_broadcast(struct mmthr_cond_pshared * cond)
{
	struct lockref_connection* lockref;
	int64_t wakeup_val, waiter_val, num_waiter;
	struct shared_lock shlock = {.key = cond->pshared_key};

	waiter_val = atomic_load(&cond->waiter_seq);
	wakeup_val = atomic_load(&cond->wakeup_seq);
	num_waiter = waiter_val - wakeup_val;

	if (num_waiter <= 0)
		return 0;

	lockref = get_thread_lockref_data();
	wakeup_val = atomic_fetch_add(&cond->wakeup_seq, num_waiter);
	wakeup_val += num_waiter - 1;
	pshared_wake_lock(lockref, shlock, wakeup_val, num_waiter);

	return 0;
}


/**
 * pshared_cond_init() - initialize process shared condition variable
 * @cond:       condition variable to init
 *
 * Implementation of mmthr_cond_init() in the case of process shared condition.
 *
 * Return: always 0
 */
static
int pshared_cond_init(struct mmthr_cond_pshared * cond)
{
	struct lockref_connection* lockref;

	lockref = get_thread_lockref_data();
	cond->pshared_key = pshared_init_lock(lockref);

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                Exported API of synchronization primitives              *
 *                                                                        *
 **************************************************************************/

/**
 * struct mmthr_mtx - mutex structure behind &typedef mmthr_mtx_t
 * @flag:	flags indicating the type of mutex (0 or MMTHR_PSHARED)
 * @srw_lock:   data aliasing with SRWLOCK (used if @flag is 0)
 * @lock:       variable whose update indicate the owner and
 *              contended state. Must be updated only through atomic
 *              operation. This field is used if @flag is MMTHR_PSHARED.
 * @pshared_key: Identifier of the process-shared lock as known by the lock
 *              referee service process. used if @flag MMTHR_PSHARED.
 *
 * This structure is the container of &typedef mmthr_mtx_t on Win32.
 * Depending on @flag at mutex initialization, static or with
 * mmthr_mtx_init(), @srw_lock or @lock/@pshared_key will be used.
 *
 * If @flag is 0, the mutex is a normal one, ie, not shared across process
 * nor robust. In such a case @srw_lock field is used with the
 * AcquireSRWLockExclusive() and ReleaseSRWLockExclusive(). Performance will
 * be similar to using them directly.
 *
 * Please note that @srw_lock is defined as void* and not as SRWLOCK to
 * avoid the need of including synchapi.h or winbase.h (depends on the
 * Windows version)... The Windows header have many side effect that could
 * lead to a lot of trouble in the user code depending which headers are
 * included there and in which order. Declaring as void* is completely safe
 * (same size and alignment) since a SRWLOCK is a simple structure
 * containing only one pointer.
 *
 * If @flag has MMTHR_PSHARED set, the mutex will use @lock and @pshared_key
 * fields. See the "process shared mutex implementation" doc for more
 * details.
 */


static
int mmthr_mtx_is_pshared(mmthr_mtx_t * mutex)
{
	/*  pshared and srw flag fields are aliased */
	return ((mutex->pshared.flag & MMTHR_PSHARED) == MMTHR_PSHARED);
}

static
int mmthr_cond_is_pshared(mmthr_cond_t * cond)
{
	/*  pshared and srw flag fields are aliased */
	return ((cond->pshared.flag & MMTHR_PSHARED) == MMTHR_PSHARED);
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_mtx_lock(mmthr_mtx_t* mutex)
{
	if (mmthr_mtx_is_pshared(mutex))
		return pshared_mtx_lock(&mutex->pshared);

	AcquireSRWLockExclusive((SRWLOCK*)(&mutex->srw.srw_lock));
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_mtx_trylock(mmthr_mtx_t* mutex)
{
	if (mmthr_mtx_is_pshared(mutex))
		return pshared_mtx_trylock(&mutex->pshared);

	TryAcquireSRWLockExclusive((SRWLOCK*)(&mutex->srw.srw_lock));
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_mtx_consistent(mmthr_mtx_t* mutex)
{
	if (mmthr_mtx_is_pshared(mutex))
		return pshared_mtx_consistent(&mutex->pshared);

	mm_raise_error(EINVAL, "The mutex type is not process shared");
	return EINVAL;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_mtx_unlock(mmthr_mtx_t* mutex)
{
	if (mmthr_mtx_is_pshared(mutex))
		return pshared_mtx_unlock(&mutex->pshared);

	ReleaseSRWLockExclusive((SRWLOCK*)(&mutex->srw.srw_lock));
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_mtx_deinit(mmthr_mtx_t* mutex)
{
	(void)mutex;
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_mtx_init(mmthr_mtx_t* mutex, int flags)
{
	/* pshared and srw flag fields are aliased */
	mutex->pshared.flag = flags;

	if (mmthr_mtx_is_pshared(mutex))
		return pshared_mtx_init(&mutex->pshared);

	InitializeSRWLock((SRWLOCK*)(&mutex->srw.srw_lock));
	return 0;
}


/**
 * struct mmthr_cond - structure on Win32 behind &typedef mmthr_cond_t
 * @flag:       flags indicating behavior (any combination of MMTHR_PSHARED
 *              and MMTHR_WAIT_MONOTONIC)
 * @cv:         data aliased to CONDITION_VARIABLE (used if @flag has
 *              MMTHR_PSHARED flag set)
 * @pshared_key: Identifier of the process-shared lock as known by the lock
 *              referee service process. Used MMTHR_PSHARED if set in @flag.
 * @waiter_seq: wakeup value of the last waiter queued.  Used MMTHR_PSHARED
 *              if set in @flag.
 * @wakeup_seq: wakeup value of the last wakeup operation that has been
 *              signaled. Used MMTHR_PSHARED if set in @flag.
 *
 * This structure is the container of &typedef mmthr_cond_t on Win32.
 * Depending on MMTHR_PSHARED flag is set in @flag, the implementation will
 * differ radically.
 *
 * If MMTHR_PSHARED is not set in @flag, the implementation of wait, signal,
 * broadcast will use the Win32 SleepConditionVariableSRW(),
 * WakeConditionVariable() and WakeAllConditionVariable() and @cv field will
 * be used. For the same reason as for &mmthr_mtx.srw, @cv is declared as
 * void* and not CONDITION_VARIABLE.
 *
 * If MMTHR_PSHARED is set in @flag, the condition will be shareable across
 * processes. The wait, signal, broadcast operations will use the lock
 * referee process with the @pshared_key, @waiter_seq and @wakeup_seq
 * fields.
 *
 * The MMTHR_WAIT_MONOTONIC flag in @flag will indicate whether the timeout
 * in mmthr_cond_timedwait() will be based on MM_CLK_REALTIME or
 * MM_CLK_MONOTONIC clock.
 */


static
int sleep_win32cv(CONDITION_VARIABLE* cv, SRWLOCK* srwlock, DWORD timeout_ms)
{
	BOOL res;

	res = SleepConditionVariableSRW(cv, srwlock, timeout_ms, 0);
	if (res == TRUE)
		return 0;

	return (GetLastError() != ERROR_TIMEOUT) ? EINVAL : ETIMEDOUT;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_cond_wait(mmthr_cond_t* cond, mmthr_mtx_t* mutex)
{
	if (mmthr_cond_is_pshared(cond))
		return pshared_cond_wait(&cond->pshared, mutex, NULL);

	CONDITION_VARIABLE* cv = (CONDITION_VARIABLE*)(&cond->srw.cv);
	SRWLOCK* srwlock = (SRWLOCK*)(&mutex->srw.srw_lock);

	return sleep_win32cv(cv, srwlock, INFINITE);
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_cond_timedwait(mmthr_cond_t* _cond, mmthr_mtx_t* mutex,
                         const struct timespec* abstime)
{
	struct timespec now;
	int ret;
	int64_t delta_ns;
	DWORD timeout_ms;
	clockid_t clk_id;
	CONDITION_VARIABLE* cv;
	SRWLOCK* srwlock;
	struct mmthr_cond_swr * cond;

	if (mmthr_cond_is_pshared(_cond))
		return pshared_cond_wait(&_cond->pshared, mutex, abstime);

	cond = &_cond->srw;
	cv = (CONDITION_VARIABLE*)(&cond->cv);
	srwlock = (SRWLOCK*)(&mutex->srw.srw_lock);

	// Find the type of clock to use for timeout
	clk_id = CLOCK_REALTIME;
	if (cond->flag & WAITCLK_FLAG_MONOTONIC)
		clk_id = CLOCK_MONOTONIC;

	do {
		// Compute the relative delay to reach timeout
		mm_gettime(clk_id, &now);
		delta_ns = mm_timediff_ns(abstime, &now);
		if (delta_ns < 0)
			return ETIMEDOUT;

		// Do actual wait
		timeout_ms = delta_ns/NS_IN_MS;
		ret = sleep_win32cv(cv, srwlock, timeout_ms);
	} while (ret == ETIMEDOUT);

	return ret;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_cond_signal(mmthr_cond_t* cond)
{
	if (mmthr_cond_is_pshared(cond))
		return pshared_cond_signal(&cond->pshared);

	WakeConditionVariable((CONDITION_VARIABLE*)(&cond->srw.cv));
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_cond_broadcast(mmthr_cond_t* cond)
{
	if (mmthr_cond_is_pshared(cond))
		return pshared_cond_broadcast(&cond->pshared);

	WakeAllConditionVariable((CONDITION_VARIABLE*)(&cond->srw.cv));
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_cond_deinit(mmthr_cond_t* cond)
{
	(void) cond;
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_cond_init(mmthr_cond_t * _cond, int flags)
{
	struct mmthr_cond_swr * cond = &_cond->srw;

	/* pshared and srw flag fields are aliased */
	cond->flag = flags & ~WAITCLK_MASK;

	if (flags & MMTHR_WAIT_MONOTONIC)
		cond->flag |= WAITCLK_FLAG_MONOTONIC;
	else
		cond->flag |= WAITCLK_FLAG_REALTIME;

	if (flags & MMTHR_PSHARED)
		return pshared_cond_init(&_cond->pshared);

	InitializeConditionVariable((CONDITION_VARIABLE*)(&cond->cv));
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_once(mmthr_once_t* once, void (*once_routine)(void))
{
	static SRWLOCK once_global_lock = SRWLOCK_INIT;
	static int global_lock_recursion_level = 0;
	static DWORD global_lock_owner = 0;
	DWORD tid;

	if (LIKELY(*once != MMTHR_ONCE_INIT))
		return 0;

	// Acquire lock allowing recursion
	tid = get_tid();
	if (global_lock_owner != tid) {
		AcquireSRWLockExclusive(&once_global_lock);
		global_lock_owner = tid;
	}
	global_lock_recursion_level++;

	// Execute once routine
	if (*once == MMTHR_ONCE_INIT) {
		*once = !MMTHR_ONCE_INIT;
		once_routine();
	}

	// Unlock recursive lock
	if (--global_lock_recursion_level == 0) {
		global_lock_owner = 0;
		ReleaseSRWLockExclusive(&once_global_lock);
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                          thread manipulation                           *
 *                                                                        *
 **************************************************************************/

static
unsigned __stdcall thread_proc_wrapper(void* param)
{
	struct mmthread* thread = param;

	// Set mm_thread passed as self
	get_thread_local_data()->thread = thread;

	thread->retval = thread->routine(thread->arg);

	_endthreadex(0);
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_create(mmthread_t* thread, void* (*proc)(void*), void* arg)
{
	struct mmthread* th;

	th = create_mmthread_data();
	if (!th)
		return errno;

	// Mark thread as joinable and register the thread routine
	th->state &= ~STATE_DETACHED;
	th->routine = proc;
	th->arg = arg;

	th->hnd = (HANDLE)_beginthreadex(NULL, 0, thread_proc_wrapper, th, 0, NULL);
	if (!th->hnd) {
		mm_raise_from_errno("Failed to begin thread");
		destroy_mmthread_data(th);
		return errno;
	}

	*thread = th;
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_join(mmthread_t thread, void** value_ptr)
{
	if (atomic_load(&thread->state) & STATE_DETACHED) {
		mm_raise_error(EINVAL, "The thread is detached");
		return EINVAL;
	}

	WaitForSingleObject(thread->hnd, INFINITE);

	if (value_ptr)
		*value_ptr = thread->retval;

	destroy_mmthread_data(thread);
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mmthr_detach(mmthread_t thread)
{
	int64_t prev_state;

	// Add detached state
	prev_state = atomic_fetch_add(&thread->state, STATE_DETACHED);
	if (prev_state & STATE_DETACHED) {
		mm_raise_error(EINVAL, "The thread is already detached");
		return EINVAL;
	}

	// Cleanup resources since thread is terminated: resources were not
	// cleaned at thread termination because it was then known as
	// joinable
	if (prev_state & STATE_STOPPED)
		destroy_mmthread_data(thread);

	return 0;

}


/* doc in posix implementation */
API_EXPORTED
mmthread_t mmthr_self(void)
{
	struct mmthread* self;

	self = get_thread_local_data()->thread;
	if (LIKELY(self))
		return self;

	// If we reach here, the mmthread structure of current thread is not
	// set because it has not been created by mmthread_create(). So we
	// must create a mmthread structure usable by other thread. It is
	// not necessary to create a usable thread handle because this
	// thread is not joinable
	self = create_mmthread_data();
	get_thread_local_data()->thread = self;

	return self;
}
