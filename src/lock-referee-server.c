/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>

#include "lock-referee-proto.h"
#include "mmpredefs.h"
#include "clock-win32.h"
#include "mmtime.h"

#include <stdlib.h>
#include <stdio.h>
#include <aclapi.h>
#include <accctrl.h>
#include <stdbool.h>

#define SRV_TIMEOUT_MS          200
#define LOCK_LIST_INITIAL_LEN   128

#define NSEC_IN_MSEC (NS_IN_SEC / MS_IN_SEC)

#define ROUND_UP(x, y) ( (((x)+(y)-1) / (y)) * (y) )

#ifndef DWORD_MAX
#define DWORD_MAX	0xFFFFFFFF
#endif

struct queue_node {
	struct queue_node* prev;
	struct queue_node* next;
};

struct queue {
	struct queue_node* first;
	struct queue_node* last;
};

struct list_node {
	struct list_node* prev;
	struct list_node* next;
};

struct list {
	struct list_node head;
};

struct thread_client {
	struct queue_node lock_node;
	HANDLE pipe;
	int num_pending_request;
	bool being_destroyed;
	int64_t wakeup_val;
	int64_t waiting_lock_key;
	struct lockref_req_msg msg_read;
	struct lockref_resp_msg msg_write;
	OVERLAPPED overlapped_read;
	OVERLAPPED overlapped_write;
	struct robust_data* robust_data;
	struct list_node timeout_node;
	struct timespec wait_timeout;
};
#define GET_TC_FROM_OVERLAPPED_READ(lpo)	((struct thread_client*)(((char*)lpo)-offsetof(struct thread_client, overlapped_read)))
#define GET_TC_FROM_OVERLAPPED_WRITE(lpo)	((struct thread_client*)(((char*)lpo)-offsetof(struct thread_client, overlapped_write)))
#define GET_TC_FROM_LOCK_NODE(node)	((struct thread_client*)(((char*)node)-offsetof(struct thread_client, lock_node)))
#define GET_TC_FROM_TIMEOUT_NODE(node)	((struct thread_client*)(((char*)node)-offsetof(struct thread_client, timeout_node)))


struct lock {
	int64_t key;
	int64_t max_wakeup_val;
	int nwaiter;
	struct queue waiters_queue;
	struct mutex_cleanup_job* job;
	HANDLE job_mapping_hnd;
};

struct lock_array {
	int num_used;
	int num_max;
	struct lock* sorted_array;
};

struct lockref_server {
	int is_init;
	HANDLE connect_evt;
	OVERLAPPED conn_overlapped;
	HANDLE pipe;
	SECURITY_ATTRIBUTES sec_attr;
	struct lock_array watched_locks;
	struct list realtime_timeout_list;
	struct list monotonic_timeout_list;
	int num_thread_client;
	int quit;
};

static struct lockref_server server = {.is_init = 0};


static struct lock* lockref_server_get_lock(struct lockref_server* srv, int64_t key, bool create);
static struct list* lockref_server_get_timeout_list(struct lockref_server* srv, int clk_flags);
static void lockref_server_update_thread_count(struct lockref_server* srv, int adjustment);

static void thread_client_wakeup(struct thread_client* tc, bool timedout);
static void thread_client_process_timeout(struct thread_client* tc);
static void thread_client_do_cleanup_job(struct thread_client* tc, HANDLE cleanup_mapping_hnd);
static void thread_client_queue_read(struct thread_client* tc);
static void thread_client_queue_write(struct thread_client* tc);
static HANDLE thread_client_add_robust_data(struct thread_client* tc, int locked_list_size);
static void thread_client_destroy( struct thread_client* tc);
static struct thread_client* thread_client_create(HANDLE pipe);


/**
 * gen_pshared_key() - generate a key for process shared lock
 *
 * The generated is made in a way that it is unique during the lifetime of the
 * server. Even if it is restarted, the probability to regenerate the same key
 * that a previous server instance is very close to 0.
 *
 * Return: the generated key
 */
static
int64_t gen_pshared_key(void)
{
	static uint32_t last_value = 0;
	int64_t key;
	FILETIME curr;

	key = last_value++;
	key <<= 32;

	GetSystemTimeAsFileTime(&curr);
	key |= curr.dwLowDateTime;

	return key;
}


static
DWORD clamp_i64_to_dword(int64_t i64val)
{
	if (i64val > DWORD_MAX)
		return DWORD_MAX;

	if (i64val < 0)
		return 0;

	return (DWORD)i64val;
}

static
HANDLE dup_handle_for_pipeclient(HANDLE hpipe, HANDLE src_hnd)
{
	HANDLE dst_proc, src_proc;
	HANDLE dst_hnd;
	ULONG pid;

	// Get PID of the other end of the named pipe
	GetNamedPipeClientProcessId(hpipe, &pid);

	// Get process handle of source and destination for handle passing
	src_proc = GetCurrentProcess();
	dst_proc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
	if (dst_proc == INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;

	if (!DuplicateHandle(src_proc, src_hnd, dst_proc, &dst_hnd,
	                     0, FALSE, DUPLICATE_SAME_ACCESS)) {
		dst_hnd = INVALID_HANDLE_VALUE;
	}

	CloseHandle(dst_proc);
	return dst_hnd;
}


static
size_t get_robust_data_size(int num_keys)
{
	struct robust_data* data;
	size_t len;
	static size_t pagesize;

	if (num_keys <= 0)
		num_keys = 1;

	// Get page size
	if (!pagesize) {
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		pagesize = sysinfo.dwPageSize;
	}

	// Get the minimal size to map the robust data with at least
	// locked_list_size element in locked_keys array, round up to the next
	// multiple of page size
	len = sizeof(*data) + num_keys*sizeof(data->locked_keys[0]);
	return ROUND_UP(len, pagesize);
}



/**************************************************************************
 *                                                                        *
 *                           Lock manipulation                            *
 *                                                                        *
 **************************************************************************/


/**
 * append_to_queue() - add the specified node to the end of the queue
 * @queue:       queue to update
 * @node:       node to add
 */
static
void append_to_queue(struct queue* queue, struct queue_node* node)
{
	struct queue_node* last = queue->last;

	if (!queue->first)
		queue->first = node;
	else
		last->next = node;

	node->prev = last;
	node->next = NULL;

	queue->last = node;
}


/**
 * drop_from_queue() - remove the specified node from the queue
 * @queue:       queue to update
 * @node:       node to remove
 */
static
void drop_from_queue(struct queue* queue, struct queue_node* node)
{
	struct queue_node *prev, *next;

	prev = node->prev;
	next = node->next;

	if (prev)
		prev->next = next;
	else
		queue->first = next;

	if (next)
		next->prev = prev;
	else
		queue->last = prev;
}

/**
 * lock_init() - initialize the internals of a newly created lock
 * @lock:       pointer to lock to initialize
 * @key:        key associated with the lock
 */
static
void lock_init(struct lock* lock, int64_t key)
{
	*lock = (struct lock) {
		.key = key,
		.job_mapping_hnd = INVALID_HANDLE_VALUE,
	};
}

/**
 * lock_add_waiter() - add a thread waiting for the lock
 * @lock:      lock to which the thread must be adding in wait list
 * @tc:        thread client to turn as a waiter
 *
 * Return: true if the thread client is now actually waiting for lock, false if
 * the thread client is not waiting due to immediate wakeup.
 */
static
bool lock_add_waiter(struct lock* lock, struct thread_client* tc)
{
	// Check that lock has not been unheld in advance
	if ((++lock->nwaiter <= 0) && (lock->max_wakeup_val >= tc->wakeup_val)) {
		thread_client_wakeup(tc, false);
		return false;
	}

	// Append thread client to the end of lk_arr of waiters
	append_to_queue(&lock->waiters_queue, &tc->lock_node);

	return true;
}


/**
 * lock_wake_waiters() - wakeup some threads waiting for a lock
 * @lock:      lock for which the thread are waiting
 * @num:       number of thread to wake up
 * @val:       wakeup minimal value
 *
 * Select @num thread waiting for the lock referenced by @lock and whose wakeup
 * condition is fulfilled and wake them up. The wakeup condition corresponds to
 * a wakeup value of a thread client being equal or smaller to @val.
 *
 * Due to race for going to sleep, it might be possible that the current number
 * of thread waiting for the lock is smaller than the number of thread to
 * wakeup. This is not a issue since the lock will store in advance the wakeup
 * condition and adjust the number of thread that must sleep. When the missing
 * waiters will actually request to wait for lock, they will be immediately
 * waken up.
 */
static
void lock_wake_waiters(struct lock* lock, int num, int64_t val)
{
	struct thread_client *tc;
	struct queue_node* node;

	// Adjust in advance the wakeup, it will be here if we don't find
	// enough waiter to wake
	lock->nwaiter -= num;
	if (lock->max_wakeup_val < val)
		lock->max_wakeup_val = val;

	// Wakeup num first thread whose wakeup condition match
	for (node = lock->waiters_queue.first; node && num; node = node->next) {
		tc = GET_TC_FROM_LOCK_NODE(node);
		if (tc->wakeup_val > val)
			continue;

		// Remove from waiter queue and wake it up
		drop_from_queue(&lock->waiters_queue, &tc->lock_node);
		thread_client_wakeup(tc, false);
		num--;
	}
}


/**
 * lock_drop_waiter() - remove a thread waiting for a lock
 * @lock:      lock for which the thread are waiting
 * @tc:        thread client to drop from the waiter queue
 *
 * This function is meant to be used when a thread client is being destroyed or
 * a wait has timedout. It will remove it from the waiter queue of @lock.
 * Contrary to lock_wake_waiters(), this does not wake up the thread.
 */
static
void lock_drop_waiter(struct lock* lock, struct thread_client* tc)
{
	// Remove from waiter queue and adjust the wakeup count
	lock->nwaiter--;
	drop_from_queue(&lock->waiters_queue, &tc->lock_node);
}


/**
 * lock_create_cleanup_job() - create a cleanup job associated with a lock
 * @lock:       lock associated to which the cleanup job must be created
 *
 * This function creates a cleanup job associated with @lock. This structure is
 * mean to keep count of dead thread that were in relation with the lock at the
 * moment of their death. It is a special data that is meant to be shared with
 * a client process which will do the actual cleanup. As such it is not memory
 * allocated as usual but created through a file mapping.
 *
 * Return: 0 in case of success, -1 otherwise.
 */
static
int lock_create_cleanup_job(struct lock* lock)
{
	HANDLE hnd;
	struct mutex_cleanup_job* job;

	hnd = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
	                        PAGE_READWRITE, 0, MM_PAGESZ, NULL);
	if (hnd == INVALID_HANDLE_VALUE)
		return -1;

	// Map into memory
	job = MapViewOfFile(hnd, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!job) {
		CloseHandle(hnd);
		return -1;
	}

	job->num_dead = 0;
	lock->job = job;
	lock->job_mapping_hnd = hnd;

	return 0;
}


/**
 * lock_destroy_cleanup_job() - destroy a cleanup job associated with a lock
 * @lock:       lock whose cleanup job must be destroyed
 */
static
void lock_destroy_cleanup_job(struct lock* lock)
{
	if (!lock->job)
		return;

	UnmapViewOfFile(lock->job);
	CloseHandle(lock->job_mapping_hnd);

	lock->job_mapping_hnd = INVALID_HANDLE_VALUE;
	lock->job = NULL;
}


/**
 * lock_start_or_remove_cleanup_job() - start pending cleanup job or remove empty one
 * @lock:       lock whose cleanup job must be inspected
 */
static
void lock_start_or_remove_cleanup_job(struct lock* lock)
{
	struct mutex_cleanup_job* job = lock->job;
	struct thread_client* tc;

	if (!job || job->in_progress)
		return;

	if (job->num_dead == 0) {
		lock_destroy_cleanup_job(lock);
		return;
	}

	// Start it if there is something to do and a waiter is available for it
	if (lock->waiters_queue.first) {
		lock->job->in_progress = 1;
		tc = GET_TC_FROM_LOCK_NODE(lock->waiters_queue.first);
		thread_client_do_cleanup_job(tc, lock->job_mapping_hnd);
	}
}


/**
 * lock_report_cleanup_job_done() - report a mutex cleanup has been finished
 * @lock:       lock whose cleanup job is done
 * @num_wakeup: number of thread that must be wakeup
 *
 * Called when a thread has finished a mutex cleanup job, this mark the job has
 * done making it possibly eligible for removal during the next garbage
 * collection.
 */
static
void lock_report_cleanup_job_done(struct lock* lock)
{
	struct mutex_cleanup_job* job = lock->job;

	job->in_progress = 0;
}


/**
 * lock_report_dead() - add a thread reported as dead in a lock
 * @lock:       lock to which the dead muyst be reported
 * @is_waiter:  flag from thread robust data indicating it was waiting
 * @thread_id:  thread id of the dead thread
 *
 * Notify @lock that the thread whose ID was @thread_id and which has been
 * detected as dead was in relation with the lock. The lock can then create or
 * update a cleanup job to submit later to a thread client which will allows to
 * undo the possible change the dead has made to the shared lock.
 */
static
void lock_report_dead(struct lock* lock, int is_waiter, DWORD thread_id)
{
	struct mutex_cleanup_job* job;
	int i;

	// Create a cleanup job associated with the lock if none were available
	// before
	if (!lock->job) {
		if (lock_create_cleanup_job(lock))
			return;
	}

	job = lock->job;

	// Add the thread to the cleanup job
	i = job->num_dead++;
	job->deadlist[i].tid = thread_id;
	job->deadlist[i].is_waiter = is_waiter;
}


/**
 * lock_is_unused() - check that a lock still need to be referenced
 * @lock:	lock to check
 *
 * Return: true if it is actually unused, false otherwise.
 */
static
bool lock_is_unused(const struct lock* lock)
{
	if (!lock->nwaiter && !lock->waiters_queue.first && !lock->job)
		return true;

	return false;
}


/**************************************************************************
 *                                                                        *
 *                           Lock array  manipulation                     *
 *                                                                        *
 **************************************************************************/
#define KEY_NOTFOUND_FLAG  0x80000000

/**
 * search_key_index() - get the index of lock of given key in the list
 * @locks:      sorted array of locks
 * @len:        number of element in @locks
 * @key:        key of lock to search
 *
 * Return: the index if lock using @key is found. Otherwise the position
 * where to insert the key (insert before) is provided by a negative value
 * (-index is returned means that lock must be inserted before position
 * index)
 */
static
int search_key_index(const struct lock* locks, int len, int64_t key)
{
	int m, l, h;

	// Binary search in the space of the sorted list of lock keys
	l = 0;
	h = len-1;
	while (l <= h) {
		m = (l+h)/2;
		if (key == locks[m].key)
			return m;

		if (key < locks[m].key) {
			h = m-1;
		} else {
			l = m+1;
		}
	}

	// Not found... The location of insertion (before) is given by l
	return l | KEY_NOTFOUND_FLAG;
}


/**
 * lock_array_get_lock() - get the lock (maybe new) of the specified key
 * @lk_arr:     lock_array in which the corresponding lock should be found
 * @key:        pshared key corresponding to the lock to find
 * @create:     flag indicating whether the lock must be created if not found
 *
 * Return: pointer to the lock found or newly created. 
 */
static
struct lock* lock_array_get_lock(struct lock_array* lk_arr, int64_t key, bool create)
{
	int index, len;
	struct lock* locks = lk_arr->sorted_array;

	index = search_key_index(locks, lk_arr->num_used, key);

	// Handle the case when lock must be inserted.
	if (index & KEY_NOTFOUND_FLAG) {
		if (!create)
			return NULL;

		index &= ~KEY_NOTFOUND_FLAG;

		// Resize lock array if there is no space to add one
		if (UNLIKELY(lk_arr->num_used+1 > lk_arr->num_max)) {
			lk_arr->num_max *= 2;
			locks = realloc(locks, lk_arr->num_max*sizeof(*locks));
			if (!locks)
				return NULL;

			lk_arr->sorted_array = locks;
		}

		// Actually insert the new lock and initialize it
		lk_arr->num_used++;
		len = lk_arr->num_used - index;
		memmove(locks+index+1, locks+index, len*sizeof(*locks));
		lock_init(&locks[index], key);
	}

	return locks + index;
}


/**
 * lock_array_drop_unused() - drop the locks that are not used any longer
 * @lk_arr:     lock array whose locks should be checked
 */
static
void lock_array_drop_unused(struct lock_array* lk_arr)
{
	int src, dst;
	struct lock* locks = lk_arr->sorted_array;

	// Copy element (lock) wise. Skip copy if lock is unused
	for (src = 0, dst = 0; src < lk_arr->num_used; src++, dst++) {
		// Check that the lock is still in use
		if (lock_is_unused(&locks[src])) {
			dst--;
			continue;
		}

		// No modification so far, so skip copy
		if (src == dst)
			continue;

		locks[dst] = locks[src];
	}

	lk_arr->num_used += (dst - src);
}


/**
 * lock_array_update_cleanup_jobs() - start or remove cleanup job of locks
 * @lk_arr:     lock array
 *
 * This function will inspect the cleanup job of each lock (if any) and start
 * the cleanup job if something has to be done, or remove it if nothing has to
 * be done.
 */
static
void lock_array_update_cleanup_jobs(struct lock_array* lk_arr)
{
	int i;
	struct lock* locks = lk_arr->sorted_array;

	for (i = 0; i < lk_arr->num_used; i++)
		lock_start_or_remove_cleanup_job(&locks[i]);
}


/**
 * lock_array_deinit_() - deallocate internal of a lock array
 * @lk_arr:     lock array to deinit
 */
static
void lock_array_deinit(struct lock_array* lk_arr)
{
	free(lk_arr->sorted_array);
	*lk_arr = (struct lock_array){0};
}


/**
 * lock_array_init() - Initialize a lock array
 * @lk_arr:     lock array to initialize
 *
 * Return: 0 in case of success, -1 otherwise
 */
static
int lock_array_init(struct lock_array* lk_arr)
{
	size_t arrsize;

	*lk_arr = (struct lock_array){.num_max = LOCK_LIST_INITIAL_LEN};

	arrsize = lk_arr->num_max*sizeof(*lk_arr->sorted_array);
	lk_arr->sorted_array = malloc(arrsize);
	if (!lk_arr->sorted_array)
		return -1;

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                          Timeout list manipulation                     *
 *                                                                        *
 **************************************************************************/
static
void detach_list_node(struct list_node* node)
{
	if (node->next) {
		node->next->prev = node->prev;
		node->next = NULL;
	}

	if (node->prev) {
		node->prev->next = node->next;
		node->prev = NULL;
	}
}


static
void insert_list_node_after(struct list_node* node_to_insert,
                            struct list_node* after)
{
	node_to_insert->prev = after;
	node_to_insert->next = after->next;

	if (after->next)
		after->next->prev = node_to_insert;

	after->next = node_to_insert;
}


/**
 * timeout_list_add_waiter() - add a thread client to a timeout list
 * @timeout_list:       list to update
 * @tc:                 thread client to add
 *
 * When adding a thread, the element is inserted in the middle in the list such
 * a way the list is kept sorted according to the timeout, ie the first element
 * in the list is always the one whose timeout is the most immediate.
 */
static
void timeout_list_add_waiter(struct list* timeout_list,
                             struct thread_client *tc)
{
	struct list_node *node, *next;
	struct thread_client* next_tc;
	struct timespec* timeout = &tc->wait_timeout;

	node = &timeout_list->head;
	next = node->next;

	// Find the position of thread in the list of increasing timeout
	// After this loop, the thread must be inserted after node
	while (next) {

		next_tc = GET_TC_FROM_TIMEOUT_NODE(next);
		if (mm_timediff_ns(timeout, &next_tc->wait_timeout) < 0)
			break;

		node = next;
		next = next->next;
	}

	insert_list_node_after(&tc->timeout_node, node);
}


/**
 * timeout_list_update() - wakeup thread whose timeout is elapsed
 * @timeout_list:       list to update
 * @now:                timestamp indicating current time
 *
 * Return: the time difference in nanosecond from @now to the timeout of the
 * next thread client node remaining in the list after update. If the timeout
 * list is empty INT64_MAX is returned.
 */
static
int64_t timeout_list_update(struct list* timeout_list, struct timespec* now)
{
	struct thread_client* tc;
	struct list_node *node, *next;
	int64_t diff_ns;

	for (node = timeout_list->head.next; node != NULL; node = next) {
		tc = GET_TC_FROM_TIMEOUT_NODE(node);
		diff_ns = mm_timediff_ns(&tc->wait_timeout, now);
		if (diff_ns >= 0)
			return diff_ns;

		// Store the next node now, because, waking thread client up
		// will remove it from its timeout list (hence, node->next
		// will become NULL during wakeup).
		next = node->next;

		thread_client_process_timeout(tc);
	}

	// No waiter left with this clock base
	return INT64_MAX;
}


/**************************************************************************
 *                                                                        *
 *                       Thread connection handler                        *
 *                                                                        *
 **************************************************************************/

/**
 * DOC: Thread client communication
 *
 * The communication with the thread client is handle asynchronously over the
 * connected named pipe. For each thread to handle, there is a connected pipe.
 * The whole communication resolves around handling the few request it can
 * make :
 *
 * - LOCKREF_OP_INITLOCK: request a lock to be initialized (ie, a key to be
 *     generated). The response will contain the generated key.
 *
 * - LOCKREF_OP_WAIT: the thread has requested to wait on the specified key.
 *     The response will not be issued until the thread must be waken up
 *
 * - LOCKREF_OP_WAKE: The thread request to wakeup a certain number of thread
 *     waiting for the specified key. This request do not get any response from
 *     the server.
 *
 * - LOCKREF_OP_GETROBUST: request the lock server to provide robust data to
 *     the client. This data will shared though memory map between client and
 *     server.
 *
 * - LOCKREF_OP_CLEANUP_DONE: response to a request initiated by the server.
 *     The thread client notifies that the mutex cleanup job that has been
 *     requested is finished.
 *
 * Since the communication is done asynchronously, the request and response
 * data are local to the thread_client struct. The read or write request is
 * queued and later the associated completion callback will be executed.
 * Logically the client thread can be engaged in only one request/response
 * exchange at a time, so normally, it would be correct to requeue a read when
 * the completion of a write happens and same for write when read complete.
 *
 * However, the only wait to detect the other endpoint of a named pipe being
 * closed is to read from it. Consequently there is always a read being queued
 * (whenever a read complete a new one is queued) and the write (response) are
 * done in parallel. There is no problem because only one request can run at a
 * time.
 */

static
void thread_client_handle_req_wake(struct thread_client* tc)
{
	int64_t key, val;
	int num_wakeup;
	struct lock* lock;

	// Get parameters from incoming message
	key = tc->msg_read.wake.key;
	val = tc->msg_read.wake.val;
	num_wakeup = tc->msg_read.wake.num_wakeup;

	// Find the lock and do the wakeup on it
	lock = lockref_server_get_lock(&server, key, true);
	lock_wake_waiters(lock, num_wakeup, val);

	// Send wake done ack
	tc->msg_write.respcode = LOCKREF_OP_WAKE;
	thread_client_queue_write(tc);
}


static
void thread_client_handle_req_wait(struct thread_client* tc)
{
	int64_t key, val;
	int clk_flags;
	struct timespec* timeout;
	struct lock* lock;
	struct list* timeout_list;
	bool is_waiting;

	// Get parameters from incoming message
	key = tc->msg_read.wait.key;
	val = tc->msg_read.wait.val;
	clk_flags = tc->msg_read.wait.clk_flags;
	timeout = &tc->msg_read.wait.timeout;

	// Add thread client to the waiter queue of the lock. If it returns
	// immediatemely, the lock has been waken up is advance and the thread
	// client is not waiting, thus must not be added to the timeout list
	tc->wakeup_val = val;
	lock = lockref_server_get_lock(&server, key, true);
	is_waiting = lock_add_waiter(lock, tc);
	if (!is_waiting)
		return;

	// Find timeout list if applicable
	timeout_list = lockref_server_get_timeout_list(&server, clk_flags);
	if (!timeout_list)
		return;

	// Add thread client to the timeout list
	tc->wait_timeout = *timeout;
	timeout_list_add_waiter(timeout_list, tc);
}


static
void thread_client_handle_req_initlock(struct thread_client* tc)
{
	int64_t key;

	key = gen_pshared_key();

	// Reply with the generated key
	tc->msg_write.key = key;
	tc->msg_write.respcode = LOCKREF_OP_INITLOCK;
	thread_client_queue_write(tc);
}


static
void thread_client_handle_req_getrobust(struct thread_client* tc)
{
	int num_keys;
	HANDLE client_hmap;

	// Get parameters from incoming message
	num_keys = tc->msg_read.getrobust.num_keys;

	client_hmap = thread_client_add_robust_data(tc, num_keys);

	// Send robust data in reply (or report failure)
	if (client_hmap != INVALID_HANDLE_VALUE) {
		tc->msg_write.respcode = LOCKREF_OP_GETROBUST;
		tc->msg_write.hmap = client_hmap;
	} else {
		tc->msg_write.respcode = LOCKREF_OP_ERROR;
	}
	thread_client_queue_write(tc);
}


static
void thread_client_handle_req_cleanup_done(struct thread_client* tc)
{
	int64_t key;
	int num_wakeup;
	struct lock* lock;

	// Get parameters from incoming message
	key = tc->msg_read.cleanup.key;
	num_wakeup = tc->msg_read.cleanup.num_wakeup;

	lock = lockref_server_get_lock(&server, key, false);
	lock_report_cleanup_job_done(lock);
	lock_wake_waiters(lock, 1, num_wakeup);
}


/**
 * read_completed() - callback when asyncIO on client pipe read a request
 * @errcode:    win32 error code in case of failure of the read
 * @xfer_sz:    amount of data that has been read
 * @lpo:        pointer to the overlapped array used in the async read
 */
static
VOID CALLBACK read_completed(DWORD errcode, DWORD xfer_sz, LPOVERLAPPED lpo)
{
	struct thread_client* tc = GET_TC_FROM_OVERLAPPED_READ(lpo);

	tc->num_pending_request--;

	// If any problem occurs, it is likely to be due to the client has
	// closed the connection (willingly or not), ie, errcode=109. Even if
	// is something different, the only sensible thing to do is to close
	// the connection
	if (errcode || xfer_sz < sizeof(tc->msg_read) ||  tc->being_destroyed) {
		thread_client_destroy(tc);
		return;
	}

	// Handle request
	switch(tc->msg_read.opcode) {
	case LOCKREF_OP_WAKE:
		thread_client_handle_req_wake(tc);
		break;

	case LOCKREF_OP_WAIT:
		thread_client_handle_req_wait(tc);
		break;

	case LOCKREF_OP_INITLOCK:
		thread_client_handle_req_initlock(tc);
		break;

	case LOCKREF_OP_GETROBUST:
		thread_client_handle_req_getrobust(tc);
		break;

	case LOCKREF_OP_CLEANUP_DONE:
		thread_client_handle_req_cleanup_done(tc);
		break;

	default:
		tc->msg_write.respcode = LOCKREF_OP_ERROR;
		thread_client_queue_write(tc);
		break;
	}

	// immediately requeue a read to handle the next request. This harmless
	// since write and read use different buffer and it allows use to
	// detect when a client get disconnected even if we don't have written
	// any response to the current request yet (this is the case of wait
	// request)
	thread_client_queue_read(tc);
}


/**
 * thread_client_queue_read() - queue a read asynchronously
 * @tc:         thread client with which the read must be done
 *
 * This start an asynchronous read from the associated pipe in @tc->msg_read.
 */
static
void thread_client_queue_read(struct thread_client* tc)
{
	BOOL res;

	if (tc->being_destroyed)
		return;

	res = ReadFileEx(tc->pipe, &tc->msg_read, sizeof(tc->msg_read),
	                 &tc->overlapped_read, read_completed);
	if (!res) {
		// Failure path
		thread_client_destroy(tc);
		return;
	}

	tc->num_pending_request++;
}


/**
 * write_completed() - callback when asyncIO on client pipe write a response
 * @errcode:    win32 error code in case of failure of the write
 * @xfer_sz:    amount of data that has been written
 * @lpo:        pointer to the overlapped array used in the async write
 */
static
VOID CALLBACK write_completed(DWORD errcode, DWORD xfer_sz, LPOVERLAPPED lpo)
{
	struct thread_client* tc = GET_TC_FROM_OVERLAPPED_WRITE(lpo);

	tc->num_pending_request--;

	if (errcode || xfer_sz < sizeof(tc->msg_write) || tc->being_destroyed)
		thread_client_destroy(tc);
}


/**
 * thread_client_queue_write() - queue a write asynchronously
 * @tc:         thread client with which the write must be done
 *
 * This start an asynchronous write with the associated pipe with the data in
 * @tc->msg_write. Prior to this call, the content of @tc->msg_write must be
 * set appropriately.
 */
static
void thread_client_queue_write(struct thread_client* tc)
{
	BOOL res;

	if (tc->being_destroyed)
		return;

	res = WriteFileEx(tc->pipe, &tc->msg_write, sizeof(tc->msg_write),
	                  &tc->overlapped_write, write_completed);
	if (!res) {
		// Failure path
		thread_client_destroy(tc);
		return;
	}

	tc->num_pending_request++;
}


/**
 * thread_client_wakeup() - wake a thread that was waiting
 * @tc:         thread client to wakeup
 * @timedout:   true if the client is waken up due to timeout
 *
 * This function must be used on a thread client that has issue previously a
 * wait request. This will issue the response to its wait request which in
 * effect will wake the thread up since it was waiting for the response.
 *
 * This function is meant to be called as soon as the thread client has been
 * removed from the waiter queue of the lock it was waiting for, so by
 * lock_wake_waiters() or lock_add_waiter() if it has been wake up in
 * advance.
 */
static
void thread_client_wakeup(struct thread_client* tc, bool timedout)
{
	// Remove thread client from timeout list (if any)
	detach_list_node(&tc->timeout_node);

	tc->waiting_lock_key = 0;

	// wake up correspond to the reply to the wait request
	tc->msg_write.respcode = LOCKREF_OP_WAIT;
	tc->msg_write.timedout = timedout ? 0 : 1;
	thread_client_queue_write(tc);
}


/**
 * thread_client_process_timeout() - do necessary wakeup when a wait is timed out
 * @tc:         thread client that wait engaged in a timed wait
 */
static
void thread_client_process_timeout(struct thread_client* tc)
{
	struct lock* lock;

	// Find the lock the thread is waiting for
	lock = lockref_server_get_lock(&server, tc->waiting_lock_key, false);

	// Remove from lock waiter queue and wake it up
	lock_drop_waiter(lock, tc);
	thread_client_wakeup(tc, true);
}


/**
 * thread_client_do_cleanup_job() - send cleanup job request to thread client
 * @tc:         thread client to use
 * @cleanup_mapping_hnd: handle to file mapping of the cleanup job
 */
static
void thread_client_do_cleanup_job(struct thread_client* tc, HANDLE cleanup_mapping_hnd)
{
	HANDLE client_hmap;

	// Generate a file mapping handle usable by client
	client_hmap = dup_handle_for_pipeclient(tc->pipe, cleanup_mapping_hnd);
	if (client_hmap == INVALID_HANDLE_VALUE)
		return;

	// Send request to thread client
	tc->msg_write.respcode = LOCKREF_OP_CLEANUP;
	tc->msg_write.hmap = client_hmap;
	thread_client_queue_write(tc);
}


/**
 * thread_client_add_robust_data() - create robust data for a thread client
 * @tc:         thread client whose robust data must be created
 * @num_keys:   minimal number of locked keys that the robust data must support
 *
 * Return: handle to the file mapping of the robust data that must be passed to
 * the thread client. In case of failure, INVALID_HANDLE_VALUE is returned.
 */
static
HANDLE thread_client_add_robust_data(struct thread_client* tc, int num_keys)
{
	size_t data_len, keylocked_max;
	HANDLE hmap = INVALID_HANDLE_VALUE;
	HANDLE client_hmap = INVALID_HANDLE_VALUE;

	// Create a file mapping backed by the system paging file
	data_len = get_robust_data_size(num_keys);
	hmap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, data_len, NULL);
	if (hmap == INVALID_HANDLE_VALUE)
		goto exit;

	// Map into memory
	tc->robust_data = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!tc->robust_data)
		goto exit;

	// Set maximum number of key that a thread a locked at the same time
	// according to the size of mapped robust_data
	keylocked_max = (data_len - sizeof(*tc->robust_data))/sizeof(tc->robust_data->locked_keys[0]);
	tc->robust_data->num_locked_max = keylocked_max;

	// Generate a file mapping handle usable by client
	client_hmap = dup_handle_for_pipeclient(tc->pipe, hmap);

exit:
	// Cleanup
	if (hmap != INVALID_HANDLE_VALUE)
		CloseHandle(hmap);

	return client_hmap;
}


/**
 * thread_client_cleanup_robust_data() - process robust data and destroy it
 * @tc:         thread being destroy and whose robust data must be processed
 *
 * This function, meant to be called when the thread client is destroy, analyse
 * the robust data (if any) and will notify the lock referenced in it that the
 * thread has been destroyed.
 */
static
void  thread_client_cleanup_robust_data(struct thread_client* tc)
{
	int i;
	struct robust_data* rdata;
	struct lock* lock;
	int64_t key;
	DWORD tid;

	rdata = tc->robust_data;
	if (!rdata)
		return;

	tid = rdata->thread_id;

	// Cleanup all mutex that dead is owning
	for (i = 0; i < rdata->num_locked; i++) {
		key =  tc->robust_data->locked_keys[i];
		lock = lockref_server_get_lock(&server, key, true);
		lock_report_dead(lock, 0, tid);
	}

	// Cleanup mutex that dead is attempting to lock
	key = rdata->attempt_key;
	if (key != 0) {
		lock = lockref_server_get_lock(&server, key, true);
		lock_report_dead(lock, rdata->is_waiter, tid);
	}

	UnmapViewOfFile(tc->robust_data);
	tc->robust_data = NULL;
}


/**
 * thread_client_destroy() - destroy a thread client
 * @tc:         thread client to destroy
 *
 * This function is meant to be called when the client endpoint of thread pipe
 * appears to be closed (or if a problem occurs). This will drop the thread
 * client from any waiter queue it might belong to.
 */
static
void thread_client_destroy(struct thread_client* tc)
{
	int64_t key;
	struct lock* lock;

	tc->being_destroyed = true;

	// Do actual cleanup only when all request have returned
	if (tc->num_pending_request)
		return;

	// Remove thread from any wait list
	key = tc->waiting_lock_key;
	if (key) {
		lock = lockref_server_get_lock(&server, key, false);
		lock_drop_waiter(lock, tc);

		// Remove thread client from timeout list (if any)
		detach_list_node(&tc->timeout_node);
	}

	thread_client_cleanup_robust_data(tc);
	CloseHandle(tc->pipe);
	free(tc);

	lockref_server_update_thread_count(&server, -1);
}


/**
 * thread_client_create() - create a thread client
 * @pipe:       handle to the newly connected pipe
 *
 * This creates a new thread client structure to handle communication with the
 * client thread. This function must be used when a new pipe gets connected.
 *
 * Return: the pointer to the new thread client
 */
static
struct thread_client* thread_client_create(HANDLE pipe)
{
	struct thread_client* tc;

	tc = malloc(sizeof(*tc));
	if (!tc)
		return NULL;

	*tc = (struct thread_client) {.pipe = pipe};

	lockref_server_update_thread_count(&server, 1);

	return tc;
}


/**************************************************************************
 *                                                                        *
 *                            lockref server                              *
 *                                                                        *
 **************************************************************************/


static
void deinit_pipe_security_attrs(SECURITY_ATTRIBUTES* sec_attr)
{
	PSECURITY_DESCRIPTOR sd;
	PACL acl;
	BOOL has_acl, default_acl;

	if (!sec_attr->lpSecurityDescriptor)
		return;

	sd = sec_attr->lpSecurityDescriptor;

	GetSecurityDescriptorDacl(sd, &has_acl, &acl, &default_acl);
	if (has_acl && !default_acl)
		LocalFree(acl);

	LocalFree(sd);

	*sec_attr = (SECURITY_ATTRIBUTES){0};
}

static
int init_pipe_security_attrs(SECURITY_ATTRIBUTES* sec_attr)
{
	EXPLICIT_ACCESS ea[2] = {{0}};
	PSECURITY_DESCRIPTOR sd;
	PACL acl;
	int i;

	for (i = 0; i < MM_NELEM(ea); i++) {
		ea[i].grfAccessMode = SET_ACCESS;
		ea[i].grfAccessPermissions = GENERIC_READ | FILE_WRITE_DATA;
		ea[i].grfInheritance = NO_INHERITANCE;
		ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	}

	// Everyone can create and use a client endpoint of pipe
	ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea[0].Trustee.ptstrName = "EVERYONE";

	// Only the current user can create a new instance server endpoint
	ea[1].grfAccessPermissions |= FILE_CREATE_PIPE_INSTANCE;
	ea[1].Trustee.TrusteeType = TRUSTEE_IS_USER;
	ea[1].Trustee.ptstrName = "CURRENT_USER";

	SetEntriesInAcl(MM_NELEM(ea), ea, NULL, &acl);

	sd = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(sd, TRUE, acl, FALSE);

	*sec_attr = (SECURITY_ATTRIBUTES){0};
	sec_attr->nLength = sizeof(SECURITY_ATTRIBUTES);
	sec_attr->bInheritHandle = FALSE;
	sec_attr->lpSecurityDescriptor = sd;

	return 0;
}


static
HANDLE create_srv_pipe(bool first_instance, SECURITY_ATTRIBUTES* sec_attr)
{
	HANDLE hnd;
	DWORD open_mode, pipe_mode;

	open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
	if (first_instance)
		open_mode |= FILE_FLAG_FIRST_PIPE_INSTANCE;

	pipe_mode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE
	          | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;

	hnd = CreateNamedPipe(referee_pipename, open_mode, pipe_mode,
	                      PIPE_UNLIMITED_INSTANCES, MM_PAGESZ, MM_PAGESZ,
			      SRV_TIMEOUT_MS, sec_attr);

	return hnd;
}


static
int lockref_server_handle_new_connection(struct lockref_server* srv)
{
	struct thread_client* tc;

	tc = thread_client_create(srv->pipe);
	if (!tc)
		return -1;

	thread_client_queue_read(tc);

	// Create a new server pipe to replace the one that just has been
	// connected
	srv->pipe = create_srv_pipe(0, &srv->sec_attr);
	if (srv->pipe == INVALID_HANDLE_VALUE)
		return -1;

	return 0;
}


static
int lockref_server_queue_connect(struct lockref_server* srv)
{
	if (ConnectNamedPipe(srv->pipe, &srv->conn_overlapped))
		return -1;

	switch(GetLastError()) {
	case ERROR_IO_PENDING:
		return 1;

	case ERROR_PIPE_CONNECTED:
		SetEvent(srv->connect_evt);
		return 0;
	}

	return -1;
}


static
struct lock* lockref_server_get_lock(struct lockref_server* srv,
                                     int64_t key, bool create)
{
	return lock_array_get_lock(&srv->watched_locks, key, create);
}


static
struct list* lockref_server_get_timeout_list(struct lockref_server* srv,
                                             int clk_flags)
{
	switch(clk_flags & WAITCLK_MASK) {
	case 0:
		return NULL;

	case WAITCLK_FLAG_MONOTONIC:
		return &srv->monotonic_timeout_list;

	case WAITCLK_FLAG_REALTIME:
		return &srv->realtime_timeout_list;

	default:
		// Invalid parameter, just ignore the timeout
		return NULL;
	}
}


static
void lockref_server_garbage_collection(struct lockref_server* srv)
{
	lock_array_drop_unused(&srv->watched_locks);
	lock_array_update_cleanup_jobs(&srv->watched_locks);
}


static
int64_t lockref_server_update_next_timeout_ns(struct lockref_server* srv)
{
	struct timespec now;
	int64_t diff_ns, timeout = INT64_MAX;

	// Wake up timed out waits based on wallclock
	gettimespec_wallclock_w32(&now);
	diff_ns = timeout_list_update(&srv->realtime_timeout_list, &now);
	if (timeout > diff_ns)
		timeout = diff_ns;

	// Wake up timed out waits based on monotonic
	gettimespec_monotonic_w32(&now);
	diff_ns = timeout_list_update(&srv->monotonic_timeout_list, &now);
	if (timeout > diff_ns)
		timeout = diff_ns;

	return timeout;
}


static
void lockref_server_update_thread_count(struct lockref_server* srv, int adjustment)
{
	srv->num_thread_client += adjustment;

	if (srv->num_thread_client != 0)
		srv->quit = 0;
	else
		srv->quit = 1;
}


static
void lockref_server_mainloop(struct lockref_server* srv)
{
	int io_pending;
	DWORD wait_res, byte_ret, timeout_ms;
	int64_t timeout_ns;
	BOOL ret;

	io_pending = lockref_server_queue_connect(srv);
	if (io_pending < 0)
		goto failure;

	while (!srv->quit) {

		timeout_ns = lockref_server_update_next_timeout_ns(srv);
		timeout_ms = clamp_i64_to_dword(timeout_ns / NSEC_IN_MSEC);
		wait_res = WaitForSingleObjectEx(srv->connect_evt, timeout_ms, TRUE);

		switch(wait_res) {
		case 0:
			if (io_pending) {
				ret = GetOverlappedResult(srv->pipe, &srv->conn_overlapped, &byte_ret, FALSE);
				if (!ret)
					goto failure;
			}

			if (lockref_server_handle_new_connection(srv))
				goto failure;

			io_pending = lockref_server_queue_connect(srv);
			if (io_pending < 0)
				goto failure;


		case WAIT_IO_COMPLETION:
		case WAIT_TIMEOUT:
			break;

		default:
			goto failure;
		}

		lockref_server_garbage_collection(srv);
	}
	return;

failure:
	fprintf(stderr, "Failed to get connected instance: %lu\n", GetLastError());
}


static
void lockref_server_deinit(struct lockref_server* srv)
{
	if (!srv->is_init)
		return;

	deinit_pipe_security_attrs(&srv->sec_attr);

	if (srv->pipe != INVALID_HANDLE_VALUE)
		 CloseHandle(srv->pipe);

	if (srv->connect_evt != INVALID_HANDLE_VALUE)
		 CloseHandle(srv->connect_evt);
}


static
int lockref_server_init(struct lockref_server* srv)
{
	HANDLE evt = NULL;
	HANDLE pipe = INVALID_HANDLE_VALUE;

	evt = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (evt == NULL)
		goto failure;

	if ( init_pipe_security_attrs(&srv->sec_attr)
	  || lock_array_init(&srv->watched_locks) )
		goto failure;

	pipe = create_srv_pipe(1, &srv->sec_attr);
	if (pipe == INVALID_HANDLE_VALUE)
		goto failure;

	srv->connect_evt = evt;
	srv->pipe = pipe;
	srv->conn_overlapped.hEvent = evt;

	return 0;

failure:
	if (evt != NULL)
		CloseHandle(evt);

	if (pipe != INVALID_HANDLE_VALUE)
		CloseHandle(pipe);

	deinit_pipe_security_attrs(&srv->sec_attr);
	lock_array_deinit(&srv->watched_locks);
	return -1;
}


static
int run_lockserver(void)
{
	setbuf(stderr, NULL);

	if (lockref_server_init(&server))
		return -1;

	lockref_server_mainloop(&server);

	lockref_server_deinit(&server);
	return 0;
}

#ifndef LOCKSERVER_IN_MMLIB_DLL

int main(void)
{
	if (run_lockserver())
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

#else

LOCAL_SYMBOL
void* lockserver_thread_routine(void* arg)
{
	(void)arg;

	run_lockserver();

	return NULL;
}

#endif

