/*
   @mindmaze_header@
*/
#ifndef MUTEX_LOCK_H
#define MUTEX_LOCK_H

#include <intsafe.h>

#define MTX_OWNER_TID_MASK              0x00000000FFFFFFFFLL
#define MTX_WAITER_TID_MASK             0xFFFFFE0000000000LL
#define MTX_WAITER_TID_SHIFTLEN         41
#define MTX_NEED_RECOVER_MASK           0x0000010000000000LL
#define MTX_NWAITER_MASK                0x000000FF00000000LL
#define MTX_NWAITER_SHIFTLEN            32
#define MTX_NWAITER_INCREMENT           (1LL << MTX_NWAITER_SHIFTLEN)
#define MTX_UNRECOVERABLE_MASK		(MTX_NEED_RECOVER_MASK | MTX_OWNER_TID_MASK)

/**
 * is_mtx_waited() - test thread waiting mutex to be unlocked
 * @lockval:    the value in the lock associated with mutex
 *
 * Return: true if there is at least one thread waiting for the mutex, false
 * otherwise.
 */
static inline
bool is_mtx_waited(int64_t lockval)
{
	return (lockval & MTX_NWAITER_MASK);
}


/**
 * is_mtx_locked() - test any thread has locked the mutex
 * @lockval:    the value in the lock associated with mutex
 *
 * Return: true if there is a thread that has locked the mutex, false
 * otherwise.
 */
static inline
bool is_mtx_locked(int64_t lockval)
{
	return (lockval & MTX_OWNER_TID_MASK);
}


/**
 * is_mtx_waiterlist_full() - test a mutex has its waiter count full
 * @lockval:    the value in the lock associated with mutex
 *
 * This helper indicates whether the mutex can register more waiter. If the
 * waiter count is maximal, a new thread that wait for the mutex will have
 * to revert to spin with scheduling yielding.
 *
 * Return: true if the waiter count of the mutex is maximal, false
 * otherwise.
 */
static inline
bool is_mtx_waiterlist_full(int64_t lockval)
{
	return ((lockval & MTX_NWAITER_MASK) == MTX_NWAITER_MASK);
}


/**
 * mtx_lockval() - generate lock value
 * @owner_tid:  ID of the thread owning the mutex
 * @waiter_tid: ID of the thread that is updating the wait count
 * @nwaiter:    number of thread waiting for the mutex being unlocked
 *
 * Return: the lock value
 */
static inline
int64_t mtx_lockval(DWORD owner_tid, DWORD waiter_tid, int nwaiter)
{
	return (owner_tid & MTX_OWNER_TID_MASK)
	    | ((int64_t)nwaiter << MTX_NWAITER_SHIFTLEN)
	    | ((int64_t)waiter_tid << MTX_WAITER_TID_SHIFTLEN);
}


/**
 * is_thread_mtx_owner() - test whether a thread own a mutex
 * @lockval:    the value in the lock associated with mutex
 * @tid:        ID of the thread to test
 *
 * Return: true is the thread whose ID is @tid appears to own the mutex lock
 * given the value in @lockval, false otherwise.
 */
static inline
bool is_thread_mtx_owner(int64_t lockval, DWORD tid)
{
	lockval &= MTX_OWNER_TID_MASK;
	return (mtx_lockval(tid, 0, 0) == lockval);
}


/**
 * is_thread_mtx_waiterlist_owner() - test a thread locking the wait list
 * @lockval:    the value in the lock associated with mutex
 * @tid:        ID of the thread to test
 *
 * Return: true is the thread whose ID is @tid appears to own the change on
 * wait list count of the mutex given the value in @lockval, false otherwise.
 */
static inline
bool is_thread_mtx_waiterlist_owner(int64_t lockval, DWORD tid)
{
	lockval &= MTX_WAITER_TID_MASK;
	return (mtx_lockval(0, tid, 0) == lockval);
}


/**
 * is_mtx_ownerdead() - test whether robust mutex is in inconsistent state
 * @lockval:    the value in the lock associated with mutex
 *
 * If an owner of a robust mutex terminates while holding the mutex, the
 * mutex becomes inconsistent and the next thread that acquires the mutex
 * lock shall be notified of it. This helper allows to identify this from
 * its lock value @lockval
 *
 * Return: true if previous owner has died with the mutex locked, or in
 * other word, the mutex is in inconsistent state. false otherwise.
 */
static inline
bool is_mtx_ownerdead(int64_t lockval)
{
	return (lockval & MTX_NEED_RECOVER_MASK) ? true : false;
}


/**
 * is_mtx_unrecoverable() - test whether robust mutex is permanently unusable
 * @lockval:    the value in the lock associated with mutex
 *
 * If a robust mutex is in inconsistent and the owning thead unlock it
 * without a recover it, ie calling to mmthr_mtx_consistent() before
 * mm_thr_mutex_unlock(), the mutex is marked permanently unusable. This helper
 * allows to identify this state from the mutex lock value @lockval.
 *
 * Return: true if mutex was inconsistent and previous owner did not recover
 * it before unlocking. false otherwise.
 */
static inline
bool is_mtx_unrecoverable(int64_t lockval)
{
	return ((lockval & MTX_UNRECOVERABLE_MASK) == MTX_UNRECOVERABLE_MASK);
}


/**
 * mtx_num_waiter() - get number of registered waiters of a mutex
 * @lockval:    the value in the lock associated with mutex
 *
 * Return: the number of thread that are registered waiting for the lock.
 */
static inline
int mtx_num_waiter(int64_t lockval)
{
	return (int)((lockval & MTX_NWAITER_MASK) >> MTX_NWAITER_SHIFTLEN);
}

#endif
