/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmsysio.h"
#include "mmerrno.h"
#include "utils-win32.h"
#include "clock-win32.h"

#include <windows.h>
#include <io.h>
#include <string.h>
#include <stdio.h>

#define PIPE_PREFIX	"\\\\.\\pipe\\"
#define MAX_PIPENAME	256
#define MAX_DATA_SIZE	MM_PAGESZ
#define BUFSIZE		MAX_DATA_SIZE
#define MAX_ATTEMPTS	16

/**
 * struct fd_data - data to serialize file descriptor information
 * @hnd:        WIN32 handle of the file
 * @fd_info:    mmlib fd info of the associated file
 */
struct fd_data {
	HANDLE hnd;
	int fd_info;
};

/**
 * struct ancillary_data - structure for serializing several of file descriptor
 * @num_fds:    number of file descriptor information that are serialized
 * @array:      array of file descriptor information of length @num_fds.
 */
struct ancillary_data {
	int num_fds;
	struct fd_data array[];
};

struct mmipc_srv {
	HANDLE hpipe;
	char pipe_name[MAX_PIPENAME];
};


/* doc in posix implementation */
API_EXPORTED
struct mmipc_srv* mmipc_srv_create(const char* addr)
{
	HANDLE hpipe;
	DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE;
	DWORD pipe_mode = PIPE_TYPE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS;
	char pipe_name[MAX_PIPENAME];
	struct mmipc_srv* srv;

	if (strlen(addr) > (MAX_PIPENAME - strlen(PIPE_PREFIX))) {
		mm_raise_error(ENAMETOOLONG, "server name too long");
		return NULL;
	}

	// Format actual named pipe name (must start with prefix)
	snprintf(pipe_name, sizeof(pipe_name), PIPE_PREFIX "%s", addr);

	if (!(srv = malloc(sizeof(*srv)))) {
		mm_raise_from_errno("Failed to alloc server data for %s", addr);
		return NULL;
	}

	// Create the server named pipe
	hpipe = CreateNamedPipe(pipe_name, open_mode, pipe_mode,
	                        PIPE_UNLIMITED_INSTANCES,
	                        BUFSIZE, BUFSIZE, 0, NULL);
	if (hpipe == INVALID_HANDLE_VALUE) {
		if (GetLastError() == ERROR_ACCESS_DENIED) {
			mm_raise_error(EADDRINUSE, "addr %s already in use "
			               "in a named pipe server", addr);
		} else {
			mm_raise_from_w32err("Failed to create server "
			                     "Named pipe at %s", addr);
		}
		free(srv);
		return NULL;
	}

	srv->hpipe = hpipe;
	strcpy(srv->pipe_name, pipe_name);
	return srv;
}


/* doc in posix implementation */
API_EXPORTED
void mmipc_srv_destroy(struct mmipc_srv* srv)
{
	if (!srv)
		return;

	CloseHandle(srv->hpipe);
	free(srv);
}


/* doc in posix implementation */
API_EXPORTED
int mmipc_srv_accept(struct mmipc_srv* srv)
{
	DWORD open_mode = PIPE_ACCESS_DUPLEX;
	DWORD pipe_mode = PIPE_TYPE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS;
	HANDLE hpipe;
	int fd;

	// connect to client, It is harmless if error pipe connected is
	// returned
	if (!ConnectNamedPipe(srv->hpipe, NULL)
          && (GetLastError() != ERROR_PIPE_CONNECTED)) {
		mm_raise_from_w32err("Failed to connect client");
		return -1;
	}

	// Wrap into a file descriptor
	if (wrap_handle_into_fd(srv->hpipe, &fd, FD_TYPE_IPCDGRAM))
		return -1;

	// Create the new listening server named pipe instance
	hpipe = CreateNamedPipe(srv->pipe_name, open_mode, pipe_mode,
	                        PIPE_UNLIMITED_INSTANCES, 0, 0, 0, NULL);
	if (hpipe == INVALID_HANDLE_VALUE) {
		mm_raise_from_w32err("Failed to create new listening Named pipe");
		return -1;
	}

	// Replace the connected pipe instance by the new listening pipe
	srv->hpipe = hpipe;
	return fd;
}


/* doc in posix implementation */
API_EXPORTED
int mmipc_connect(const char* addr)
{
	HANDLE hpipe = INVALID_HANDLE_VALUE;
	char pipe_name[MAX_PIPENAME];
	int fd;

	// Format actual named pipe name (must start with prefix)
	snprintf(pipe_name, sizeof(pipe_name), PIPE_PREFIX "%s", addr);

	// Connect named pipe to server
	do {
		if (WaitNamedPipe(pipe_name, NMPWAIT_WAIT_FOREVER) == 0
		    && GetLastError() == ERROR_FILE_NOT_FOUND) {
			// server does not exist
			mm_raise_from_w32err("Server %s not found\n", addr);
			return -1;
		}

		hpipe = CreateFile(pipe_name, GENERIC_READ|GENERIC_WRITE,
				0, NULL, OPEN_EXISTING, 0,
				NULL);
		if (hpipe == INVALID_HANDLE_VALUE) {
			if (GetLastError() == ERROR_PIPE_BUSY)
				continue;

			mm_raise_from_w32err("Connection to %s failed", addr);
			return -1;
		}
	} while (hpipe == INVALID_HANDLE_VALUE);


	// Wrap into a file descriptor
	if (wrap_handle_into_fd(hpipe, &fd, FD_TYPE_IPCDGRAM)) {
		CloseHandle(hpipe);
		return -1;
	}

	return fd;
}


/**
 * open_peer_process_handle() - get win32 handle of peer process of pipe
 * @hpipe:      handle of named pipe instance
 *
 * Return: HANDLE of process in case of success, INVALID_HANDLE_VALUE
 * otherwise with error state set accordingly.
 */
static
HANDLE open_peer_process_handle(HANDLE hpipe)
{
	HANDLE dst_proc;
	ULONG pid;
	DWORD flags;
	BOOL r = TRUE;

	// Get PID of the other end of the named pipe
	r &= GetNamedPipeInfo(hpipe, &flags, NULL, NULL, NULL);
	if (flags & PIPE_SERVER_END)
		r &= GetNamedPipeClientProcessId(hpipe, &pid);
	else
		r &= GetNamedPipeServerProcessId(hpipe, &pid);

	if (r == FALSE) {
		mm_raise_from_w32err("Failed to get endpoint process id");
		return INVALID_HANDLE_VALUE;
	}

	// Get process handle from pid
	dst_proc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
	if (dst_proc == INVALID_HANDLE_VALUE)
		mm_raise_from_w32err("Cannot open endpoint process handle");

	return dst_proc;
}


/**
 * write_ancillary_data() - write the fd passed into an ancillary data
 * @data:       ancillary_data that must be written
 * @msg:        message data containing the fd that must be passed
 * @hpipe:      handle to pipe to which the message is going to be sent
 *
 * Return: size of ancillary data in case of success, -1 otherwise with
 * error state set accordingly
 */
static
ssize_t write_ancillary_data(struct ancillary_data* data,
                             const struct mmipc_msg* msg, HANDLE hpipe)
{
	HANDLE dst_proc, src_proc;
	HANDLE hnd, dst_hnd;
	int i;
	ssize_t retsize;

	data->num_fds = msg->num_fds;
	retsize = sizeof(*data);

	if (msg->num_fds == 0)
		return retsize;

	// Get current and peer process handle
	src_proc = GetCurrentProcess();
	dst_proc = open_peer_process_handle(hpipe);
	if (dst_proc == INVALID_HANDLE_VALUE)
		return -1;

	// Duplicate WIN32 handle to send into the other endpoint process
	for (i = 0; i < msg->num_fds; i++) {
		if (unwrap_handle_from_fd(&hnd, msg->fds[i])
		   || !DuplicateHandle(src_proc, hnd, dst_proc, &dst_hnd,
		                       0, FALSE, DUPLICATE_SAME_ACCESS)) {
			mm_raise_from_w32err("Failed to duplicate handle");
			while (--i >= 0) {
				CloseHandle(data->array[i].hnd);
			}
			retsize = -1;
			break;
		}

		// Store file descriptor info in element of ancillary data
		data->array[i].hnd = dst_hnd;
		data->array[i].fd_info = get_fd_info(msg->fds[i]);
		retsize += sizeof(data->array[i]);
	}

	CloseHandle(dst_proc);
	return retsize;
}


/**
 * serialize_msg() - serialize IPC message for consumption in WriteFile*()
 * @hpipe:      handle to which the message is intended
 * @msg:        user provided scatter/gather buffer to be serialize
 * @msg_data:   buffer receiving the serialized message
 * @p_hdr_sz:   pointer to a variable receiving the ancillary data size
 *
 * Return: total size of the serialized message in case of success, -1
 * otheriwse with error state set accordingly
 */
static
ssize_t serialize_msg(HANDLE hpipe, const struct mmipc_msg* msg,
                      void* msg_data, size_t* p_hdr_sz)
{
	char* buff = msg_data;
	size_t len;
	int i, truncated;
	ssize_t total_sz;

	total_sz = write_ancillary_data(msg_data, msg, hpipe);
	if (total_sz < 0)
		return -1;

	*p_hdr_sz = total_sz;
	buff = msg_data;
	buff += total_sz;

	truncated = 0;
	for (i = 0; i < msg->num_iov && !truncated; i++) {
		len = msg->iov[i].iov_len;
		if (len + total_sz >= MAX_DATA_SIZE) {
			len = MAX_DATA_SIZE - total_sz;
			// Force break at the end of iteration
			truncated = 1;
		}
		memcpy(buff, msg->iov[i].iov_base, len);
		buff += len;
		total_sz += len;
	}

	return total_sz;
}


/**
 * read_ancillary_data() - get the fd passed from an ancillary data
 * @data:       ancillary_data that must be read
 * @msg:        message data that will contain the fd passed
 *
 * Return: size of ancillary data in case of success, -1 otherwise with
 * error state set accordingly
 */
static
size_t read_ancillary_data(const struct ancillary_data* data, struct mmipc_msg* msg)
{
	int i;
	HANDLE hnd;
	int fd_info;

	// Wrap handles in the ancillary data of datagram
	for (i = 0; i < data->num_fds && i < msg->num_fds_max; i++) {
		hnd = data->array[i].hnd;
		fd_info = data->array[i].fd_info;
		if (wrap_handle_into_fd(hnd, &msg->fds[i], fd_info))
			break;
	}

	msg->num_fds = i;

	// Close handle that could not be wrapped
	for (; i < data->num_fds; i++)
		CloseHandle(data->array[i].hnd);

	if (msg->num_fds < data->num_fds)
		msg->flags |= MSG_CTRUNC;

	return sizeof(*data) + data->num_fds * sizeof(data->array[0]);
}


/**
 * deserialize_msg() - deserialize IPC message read with ReadFile*()
 * @buff_sz:    size of the serialized message buffer
 * @msg_data:   serialized message buffer
 * @msg:        user provided scatter/gather data receiving the message
 *
 * Return: size of the data excluding the ancillary data
 */
static
ssize_t deserialize_msg(size_t buff_sz, const void* msg_data, struct mmipc_msg* msg)
{
	const char* buff;
	size_t len, msg_sz;
	int i, truncated;

	// Reset flags
	msg->flags = 0;

	// Read passed file descriptor data and set buff pointer passed to
	// this ancillary data
	len = read_ancillary_data(msg_data, msg);
	buff = msg_data;
	buff += len;
	buff_sz -= len;

	truncated = 0;
	msg_sz = buff_sz;
	for (i = 0; i < msg->num_iov && !truncated; i++) {
		len = msg->iov[i].iov_len;
		if (len > buff_sz) {
			len = buff_sz;
			msg->flags |= MSG_TRUNC;
			// Force break at the end of iteration
			truncated = 1;
		}
		memcpy(msg->iov[i].iov_base, buff, len);
		buff += len;
		buff_sz -= len;
	}

	return msg_sz;
}


/* doc in posix implementation */
API_EXPORTED
ssize_t mmipc_sendmsg(int fd, const struct mmipc_msg* msg)
{
	HANDLE hpipe;
	DWORD send_sz;
	ssize_t len;
	size_t hdr_sz;
	uintptr_t msg_data[MAX_DATA_SIZE];

	if (unwrap_handle_from_fd(&hpipe, fd)
	   || (len = serialize_msg(hpipe, msg, msg_data, &hdr_sz)) < 0)
		return -1;

	if (!WriteFile(hpipe, msg_data, (DWORD)len, &send_sz, NULL))
		return mm_raise_from_w32err("WriteFile failed");

	return send_sz - hdr_sz;
}


/* doc in posix implementation */
API_EXPORTED
ssize_t mmipc_recvmsg(int fd, struct mmipc_msg* msg)
{
	HANDLE hpipe;
	DWORD recv_sz;
	uintptr_t msg_data[MAX_DATA_SIZE];

	if (unwrap_handle_from_fd(&hpipe, fd))
		return -1;

	if (!ReadFile(hpipe, msg_data, sizeof(msg_data), &recv_sz, NULL))
		return mm_raise_from_w32err("ReadFile failed");

	return deserialize_msg(recv_sz, msg_data, msg);
}


/* doc in posix implementation */
API_EXPORTED
int mmipc_connected_pair(int fds[2])
{
	DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE;
	DWORD pipe_mode = PIPE_TYPE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS;
	char name[MAX_PIPENAME];
	int srv_fd, client_fd;
	HANDLE hsrv, hclient;
	struct timespec ts;
	int attempt;

	// Try and retry to create a named pipe server handle using a
	// new pipe name until one can be used (fail on other errors)
	attempt = 0;
	do {
		// generate a name based on thread ID and current time
		gettimespec_monotonic_w32(&ts);
		snprintf(name, sizeof(name), PIPE_PREFIX "anon-%lu%lx%lx",
		         get_tid(), (long)ts.tv_sec, (long)ts.tv_nsec);

		hsrv = CreateNamedPipe(name, open_mode, pipe_mode,
		                       1, BUFSIZE, BUFSIZE, 0, NULL);
		if (hsrv == INVALID_HANDLE_VALUE) {
			// retry if we couldn't create a first instance
			if (GetLastError() == ERROR_ACCESS_DENIED
			   && ++attempt < MAX_ATTEMPTS)
				continue;

			mm_raise_from_w32err("Can't create named pipe");
			return -1;
		}
	} while (hsrv == INVALID_HANDLE_VALUE);

	if (wrap_handle_into_fd(hsrv, &srv_fd, FD_TYPE_IPCDGRAM)) {
		CloseHandle(hsrv);
		return -1;
	}

	// Create connected client pipe endpoint
	hclient = CreateFile(name, GENERIC_READ|GENERIC_WRITE,
	                     0, NULL, OPEN_EXISTING, 0, NULL);
	if (hclient == INVALID_HANDLE_VALUE) {
		mm_raise_from_w32err("Can't connect named pipe");
		mm_close(srv_fd); // This closes hsrv implicitly
		return -1;
	}

	if (wrap_handle_into_fd(hclient, &client_fd, FD_TYPE_IPCDGRAM)) {
		CloseHandle(hclient);
		mm_close(srv_fd); // This closes hsrv implicitly
		return -1;
	}

	fds[0] = srv_fd;
	fds[1] = client_fd;
	return 0;
}
