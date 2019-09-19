/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "pshared-lock.h"
#include "lock-referee-proto.h"
#include "mmsysio.h"
#include "mmpredefs.h"
#include "utils-win32.h"
#include "atomic-win32.h"
#include "mutex-lockval.h"
#include "mmlog.h"
#include "mmlib.h"

#include <windows.h>

#define LOCK_REFEREE_SERVER_BIN		LIBEXECDIR"/lock-referee.exe"
#define PIPE_WAIT_MS    50


#ifdef LOCKSERVER_IN_MMLIB_DLL

static
void spawn_lockserver_thread(void)
{
	mmthread_t thid;

	mmthr_create(&thid, lockserver_thread_routine, NULL);
	mmthr_detach(thid);
}

static
void try_spawn_lockserver(void)
{
	static mmthr_once_t lockserver_once = MMTHR_ONCE_INIT;

	mmthr_once(&lockserver_once, spawn_lockserver_thread);
}

#else //!LOCKSERVER_IN_MMLIB_DLL

static
void try_spawn_lockserver(void)
{
	const char* binpath;
	struct mm_remap_fd fd_map[] = {
		{.child_fd = 0, .parent_fd = -1},
		{.child_fd = 1, .parent_fd = -1},
		{.child_fd = 2, .parent_fd = -1},
	};

	// Determine the path of the referee server
	binpath = mm_getenv("MMLIB_LOCKREF_BIN", LOCK_REFEREE_SERVER_BIN);

	mm_spawn(NULL, binpath, MM_NELEM(fd_map), fd_map,
	         MM_SPAWN_DAEMONIZE, NULL, NULL);
}

#endif //!LOCKSERVER_IN_MMLIB_DLL


/**
 * connect_to_lockref_server() - establish connection to lock referee server
 *
 * This function will try and retry to establish the connection to the lock
 * referee server. If it fails at the first attempt, it will try to spawn
 * the server process here. It is safe to try creating the server
 * concurrently in the different threads/processes because only one instance
 * can get the server endpoint of the lock referee named pipe.
 *
 * Return: handle of connection to the server
 */
static
HANDLE connect_to_lockref_server(void)
{
	HANDLE pipe = INVALID_HANDLE_VALUE;
	BOOL ret;
	DWORD open_mode = GENERIC_READ|GENERIC_WRITE;
	DWORD pipe_mode = PIPE_READMODE_MESSAGE;

	while (pipe == INVALID_HANDLE_VALUE) {
		// Wait until a server instance of named pipe is available.
		// If failed, try to spawn a server instance and retry
		ret = WaitNamedPipe(referee_pipename, PIPE_WAIT_MS);
		if (!ret) {
			try_spawn_lockserver();
			continue;
		}

		// Try Connect named pipe to server. If failed, it will
		// restart over.
		open_mode= GENERIC_READ|GENERIC_WRITE;
		pipe = CreateFile(referee_pipename, open_mode, 0, NULL, OPEN_EXISTING, 0, NULL);
	}

	SetNamedPipeHandleState(pipe, &pipe_mode, NULL, NULL);
	return pipe;
}


/**
 * get_lockval_change() - get change to undo the effect of one dead thread
 * @lockval:    current lock value
 * @is_waiter:  non zero if the dead thread was reported as waiter of mutex
 * @tid:        ID of the dead thread
 *
 * Return: the different to add to the lock value to undo the effect of the
 * dead thread.
 */
static
int64_t get_lockval_change(int64_t lockval, int is_waiter, DWORD tid)
{
	if (is_thread_mtx_owner(lockval, tid)) {
		return MTX_NEED_RECOVER_MASK - mtx_lockval(tid, 0, 0);
	}

	if (is_thread_mtx_waiterlist_owner(lockval, tid))
		return -mtx_lockval(0, tid, 1);

	if (is_waiter)
		return -mtx_lockval(0, 0, 1);

	return 0;
}


/**
 * cleanup_mutex_lock() - perform mutex cleanup job
 * @hmap:       handle of file mapping containing the cleanup job
 * @shlock:     description of shared lock (contains pointer of lock value)
 * @ack_msg:    data to use to reply to server.
 *
 * Perform a cleanup job sent by the lock referee server. This function
 * inspect the state of the mutex lock value and check it against the list
 * of dead thread provided by lock referee server. For each dead thread, it
 * will compute the change to apply to the lock value to undo the effect on
 * it introduced by the dead thread. At the end, it prepares the response to
 * sent back to server (need of waiter wakeup will be indicated here).
 */
static NOINLINE
void cleanup_mutex_lock(HANDLE hmap, struct shared_lock shlock, struct lockref_req_msg* ack_msg)
{
	struct mutex_cleanup_job* job;
	int i;
	int64_t lockval, cleanup_val;
	DWORD tid;
	int is_waiter;

	// Map the cleanup job send by the lock referee
	job = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	CloseHandle(hmap);

	// Inspect the current value of the shared lock
	lockval = atomic_load(shlock.ptr);

	// Find the modification to apply for each dead thread onto the shared lock value
	cleanup_val = 0;
	for (i = 0; i < job->num_dead; i++) {
		tid =  job->deadlist[i].tid;
		is_waiter = job->deadlist[i].is_waiter;
		cleanup_val += get_lockval_change(lockval, is_waiter, tid);
	}

	// Apply the combined cleanup and compute the resulting value
	// (atomic_fetch_add() returns the previous value before the add)
	lockval = atomic_fetch_add(shlock.ptr, cleanup_val);
	lockval += cleanup_val;

	// Notify the cleanup job is done
	job->num_dead = 0;

	// Prepare reply to sent to server
	ack_msg->opcode = LOCKREF_OP_CLEANUP_DONE;
	ack_msg->cleanup.key = shlock.key;
	ack_msg->cleanup.num_wakeup = 0;
	if (is_mtx_waited(lockval) && !is_mtx_locked(lockval))
		ack_msg->cleanup.num_wakeup = 1;
}


/**
 * create_robust_data() - request robust data to lock referee server
 * @pipe:       handle to connection pipe to the server
 *
 * Return: pointer to the robust data (memory shared with the server)
 */
static
struct robust_data* create_robust_data(HANDLE pipe)
{
	DWORD rsz;
	BOOL ret;
	struct lockref_resp_msg response;
	struct lockref_req_msg request;
	void* ptr;
	struct robust_data* robust_data;

	// Setup the request to the server
	request.opcode = LOCKREF_OP_GETROBUST;
	request.getrobust.num_keys = 0;

	// Send the request and get reply
	ret = TransactNamedPipe(pipe, &request, sizeof(request),
	                        &response, sizeof(response), &rsz, NULL);
	mm_check(ret && (rsz == sizeof(response)), "ret=%i, rsz=%lu", ret, rsz);

	// map the handle retuned by server
	ptr = MapViewOfFile(response.hmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	mm_check(ptr != NULL);
	CloseHandle(response.hmap);
	robust_data = ptr;

	// Setup the robust data with thread info that the server could not
	// have.
	robust_data->thread_id = get_tid();

	return robust_data;
}


/**
 * init_lock_referee_connection() - Initialize lock referee connection
 * @conn:       connection data to initialize
 *
 * This function establish the connection with the lock referee server
 * (spawning it on the way if necessary) and get the robust data to use. This
 * function is meant to be called once for each thread that need interact
 * with process shared mutex or conditions, ie, @conn is assumed to point
 * to thread local data.
 */
static NOINLINE
void init_lock_referee_connection(struct lockref_connection* conn)
{
	if (conn->is_init)
		return;

	conn->pipe = connect_to_lockref_server();
	conn->robust_data = create_robust_data(conn->pipe);
	conn->is_init = 1;
}


/**
 * deinit_lock_referee_connection() - cleanup a connection to lock server
 * @conn:       connection data to cleanup
 *
 * Finish the connection pointed in @conn. This function is meant to be
 * called at the termination of a thread. Terminating the connection will
 * indicate the lock server that the thread is terminated and will inspect
 * in the robust data of the terminated thread (the server can access it
 * since it is memory mapped there as well) and launch cleanup if necessary
 * (like a mutex kept locked at thread termination).
 */
LOCAL_SYMBOL
void deinit_lock_referee_connection(struct lockref_connection* conn)
{
	if (!conn->is_init)
		return;

	// Close connection to the lock server
	CloseHandle(conn->pipe);
	conn->pipe = INVALID_HANDLE_VALUE;

	// Unmap robust data
	UnmapViewOfFile(conn->robust_data);
	conn->robust_data = NULL;

	conn->is_init = 0;
}


/**
 * pshared_get_robust_data() - get the robust data
 * @conn:       data of connection to the lock server
 */
LOCAL_SYMBOL
struct robust_data* pshared_get_robust_data(struct lockref_connection* conn)
{
	// Init lock server connection if not done yet
	if (UNLIKELY(!conn->is_init))
		init_lock_referee_connection(conn);

	return conn->robust_data;
}


/**
 * pshared_init_lock() - generates a key for a new lock
 * @conn:       data of connection to the lock server
 *
 * Return: key of a new lock
 */
LOCAL_SYMBOL
int64_t pshared_init_lock(struct lockref_connection* conn)
{
	DWORD rsz;
	BOOL ret;
	struct lockref_resp_msg response;
	struct lockref_req_msg request;

	// Init lock server connection if not done yet
	if (UNLIKELY(!conn->is_init))
		init_lock_referee_connection(conn);

	// Prepare INITLOCK request, send it and wait for reply
	request.opcode = LOCKREF_OP_INITLOCK;
	ret = TransactNamedPipe(conn->pipe, &request, sizeof(request),
	                        &response, sizeof(response), &rsz, NULL);
	mm_check(ret && (rsz == sizeof(response)), "ret=%i, rsz=%lu", ret, rsz);

	return response.key;
}


/**
 * pshared_wait_on_lock() - make calling thread wait on specified lock
 * @conn:       data of connection to the lock server
 * @lock:       lock description
 * @wakeup_val: threshold for waking the thread
 * @timeout:    pointer to lock timeout (NULL if infinite wait)
 *
 * This sets the calling thread asleep until the deadline provided in
 * @timeout is reached or another thread has waken up the lock referenced by
 * the key @lock.key with a wakeup value equal or bigger than @wakeup_val.
 *
 * Return: 0 if the thread has been waken up due to normal wakeup, ETIMEDOUT
 * the wait has timed out.
 */
LOCAL_SYMBOL
int pshared_wait_on_lock(struct lockref_connection* conn, struct shared_lock lock,
                         int64_t wakeup_val, const struct lock_timeout* timeout)
{
	DWORD rsz;
	BOOL ret;
	struct lockref_resp_msg response;
	struct lockref_req_msg request;

	// Prepare wait request
	request.opcode = LOCKREF_OP_WAIT;
	request.wait.key = lock.key;
	request.wait.val = wakeup_val;
	if (timeout) {
		request.wait.timeout = timeout->ts;
		request.wait.clk_flags = timeout->clk_flags;
	} else {
		request.wait.clk_flags = 0;
	}

	// Send wait request and wait for server reply. Server might
	// (occasionally) reply with a request to cleanup the lock. If this
	// happens, do the job and acknowedge it. Redo this until the server
	// reply without a cleanup job request.
	while (1) {
		ret = TransactNamedPipe(conn->pipe, &request, sizeof(request),
		                        &response, sizeof(response), &rsz, NULL);
		mm_check(ret && (rsz == sizeof(response)), "ret=%i, rsz=%lu", ret, rsz);

		if (UNLIKELY(response.respcode == LOCKREF_OP_CLEANUP))
			cleanup_mutex_lock(response.hmap, lock, &request);
		else
			break;
	}

	return response.timedout ? ETIMEDOUT : 0;
}


/**
 * pshared_wake_lock() - wake a process shared lock
 * @conn:       connection of the thread to the lock referee server
 * @shlock:     shared lock
 * @val:        wakeup value
 * @num_wakeup: minimum number of thread to wakeup
 *
 * This function indicates the lock referee server to wakeup at least
 * @num_wakeup thread that are waiting for the lock described by @shlock and
 * whose wakeup value are lower or equal to @val.
 */
LOCAL_SYMBOL
void pshared_wake_lock(struct lockref_connection* conn, struct shared_lock shlock,
                      int64_t val, int num_wakeup)
{
	DWORD rsz;
	BOOL ret;
	struct lockref_resp_msg response;
	struct lockref_req_msg request;

	// Prepare wake request
	request.opcode = LOCKREF_OP_WAKE;
	request.wake.key = shlock.key;
	request.wake.val = val;
	request.wake.num_wakeup = num_wakeup;

	// Send request to lock server and wait for reply
	ret = TransactNamedPipe(conn->pipe, &request, sizeof(request),
	                        &response, sizeof(response), &rsz, NULL);
	mm_check(ret && (rsz == sizeof(response)), "ret=%i, rsz=%lu", ret, rsz);
}

