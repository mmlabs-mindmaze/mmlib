/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmsysio.h"
#include "mmerrno.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BACKLOG_LENGTH	5

struct mmipc_srv {
	int listenfd;
};


/**
 * mmipc_srv_create() - Create a IPC server
 * @addr:       path to which the server must listen
 *
 * This creates a server instance that will listen to the path specified by
 * argument @path. This path does not have necessarily a connection with the
 * filesystem pathnames. However it obey to the same syntax.
 *
 * Only one IPC server instance can listen to same address. If there is
 * already another server, this function will fail.
 *
 * Return: pointer to IPC server in case of success. NULL otherwise with
 * error state set accordingly
 */
API_EXPORTED
struct mmipc_srv* mmipc_srv_create(const char* addr)
{
	int fd = -1;
	struct sockaddr_un address = {.sun_family = AF_UNIX};
	struct mmipc_srv* srv;

	if (strlen(addr) > (sizeof(address.sun_path) - 1)) {
		mm_raise_error(ENAMETOOLONG, "server name too long");
		return NULL;
	}

	// Copy the socket address. It must start with a null byte ('\0') to
	// obtain an abstract socket address. Without it would be bound to the
	// filesystem (which we do not want)
	address.sun_path[0] = '\0';
	strncpy(address.sun_path+1, addr, sizeof(address.sun_path) - 1);

	if ( !(srv = malloc(sizeof(*srv)))
	 || (fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0
	 || bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0
	 || listen(fd, BACKLOG_LENGTH) < 0) {
		mm_raise_from_errno("Fail create listening socket on %s", addr);
		mm_close(fd);
		free(srv);
		return NULL;
	}

	srv->listenfd = fd;
	return srv;
}


/**
 * mmipc_srv_destroy() - Destroy IPC server
 * @srv:        server to destroy
 *
 * This function destroy the server referenced to by @srv. The path to which
 * server was listening become available for new call to mmipc_srv_create().
 * However, if there were accepted connection still opened, it is
 * unspecified whether the name will be available before all connection are
 * closed or not. If there were client connection pending that had not been
 * accepted by the server yet, those will be dropped.
 *
 * Destroying a server does not affect accepted connections which will
 * survive until they are specifically closed with mm_close().
 */
API_EXPORTED
void mmipc_srv_destroy(struct mmipc_srv* srv)
{
	if (!srv)
		return;

	mm_close(srv->listenfd);
	free(srv);
}


/**
 * mmipc_srv_accept() - accept a incoming connection
 * @srv:        IPC server
 *
 * This function extracts the first connection on the queue of pending
 * connections, and allocate a new file descriptor for that connection (the
 * lowest number available). If there are no connection pending, the
 * function will block until one arrives.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
API_EXPORTED
int mmipc_srv_accept(struct mmipc_srv* srv)
{
	int fd;

	fd = accept(srv->listenfd, NULL, NULL);
	if (fd == -1) {
		mm_raise_from_errno("Failed to accept connection");
		return -1;
	}

	return fd;
}


/**
 * mmipc_connect() - connect a client to an IPC server
 * @addr:       path to which the client must connect
 *
 * Client-side counterpart of mmipc_srv_accept(), this functions attempts to
 * connect to a server listening to @addr if there are any. If one is found,
 * it allocates a new file descriptor for that connection (the lowest number
 * available). If there are no server listening to @addr, the function will
 * block until one server does.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
API_EXPORTED
int mmipc_connect(const char* addr)
{
	int fd;
	struct sockaddr_un address = {.sun_family = AF_UNIX};

	address.sun_path[0] = '\0';
	strncpy(address.sun_path+1, addr, sizeof(address.sun_path) - 1);

	if ( (fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0
	 || connect(fd, (struct sockaddr*)&address, sizeof(address)) < 0 ) {
		mm_raise_from_errno("Failed to connect to local IPC server");
		mm_close(fd);
		return -1;
	}

	return fd;
}


/**
 * mmipc_sendmsg() - send message to ICP endpoint
 * @fd:         file descriptor of an IPC connection endpoint
 * @msg:        IPC message
 *
 * If space is not available at the sending endpoint to hold the message to be
 * transmitted, the function will block until space is available. The
 * message sent in @msg is a datagram: either all could have been
 * transmitted, either none and the function would fail.
 *
 * File descriptors can also be transmitted to the receiving endpoint along a
 * message with the @msg->fds and @msg->num_fds fields. The file descriptors
 * listed here are duplicated for the process holding the other endpoint.
 *
 * Return: the number of bytes sent in case of success, -1 otherwise with
 * error state set accordingly.
 */
API_EXPORTED
ssize_t mmipc_sendmsg(int fd, const struct mmipc_msg* msg)
{
	ssize_t rsz;
	struct msghdr dgram = {
		.msg_iov = msg->iov,
		.msg_iovlen = msg->num_iov,
	};
	size_t fd_array_len = msg->num_fds*sizeof(int);
	char cbuf[CMSG_SPACE(fd_array_len)];

	if (msg->num_fds > 0) {
		struct cmsghdr *cmsg;

		dgram.msg_control = cbuf;
		dgram.msg_controllen = sizeof(cbuf);

		cmsg = CMSG_FIRSTHDR(&dgram);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(fd_array_len);
		memcpy(CMSG_DATA(cmsg), msg->fds, fd_array_len);

		dgram.msg_controllen = cmsg->cmsg_len;
	}

	rsz = sendmsg(fd, &dgram, 0);
	if (rsz < 0)
		mm_raise_from_errno("Failed to receive datagram");

	return rsz;
}


/**
 * mmipc_recvmsg() - recv message from IPC endpoint
 * @fd:         file descriptor of an IPC connection endpoint
 * @msg:        IPC message
 *
 * This function receives a message. The message received is a datagram: if
 * the data received is smaller that requested, the function will return a
 * smaller message and its size will be reported by the return value.
 * Controversy if a message is too long to fit in the supplied buffers in
 * @msg->iov, the excess bytes will be discarded and the flag %MSG_TRUNC
 * will be set in @msg->flags.
 *
 * You can receive file descriptors along with the message in @msg->fds and
 * @msg->num_fds fields. Similarly to the data message, if the buffer
 * holding the file descriptor is too small, the files descriptor in excess
 * will be discarded (implicitly closing them, ensuring no descriptor leak to
 * occur) and the flag %MSG_CTRUNC will be set in @msg->flags.
 *
 * Return: the number of bytes received in case of success, -1 otherwise with
 * error state set accordingly.
 */
API_EXPORTED
ssize_t mmipc_recvmsg(int fd, struct mmipc_msg* msg)
{
	ssize_t rsz;
	int i, num_fd, passed_fd;
	size_t fd_array_len = msg->num_fds_max*sizeof(int);
	const unsigned char* cmsg_data;
	char cbuf[CMSG_SPACE(fd_array_len)];
	struct msghdr dgram = {
		.msg_iov = msg->iov,
		.msg_iovlen = msg->num_iov,
		.msg_control = cbuf,
		.msg_controllen = sizeof(cbuf),
	};
	struct cmsghdr *cmsg;

	// Get datagram from socket
	rsz = recvmsg(fd, &dgram, MSG_CMSG_CLOEXEC);
	if (rsz <= 0) {
		mm_raise_from_errno("Failed to receive datagram");
		return -1;
	}

	msg->flags = dgram.msg_flags & (MSG_TRUNC | MSG_CTRUNC);

	// Read ancillary data of the datagram and get file descriptor that
	// might be sent along the datagram
	cmsg = CMSG_FIRSTHDR(&dgram);
	if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
		// use of memcpy necessary because CMSG_DATA
		// does not have type alignment guarantees
		num_fd = (cmsg->cmsg_len - CMSG_LEN(0))/sizeof(int);
		msg->num_fds = (num_fd <= msg->num_fds_max) ? num_fd : msg->num_fds_max;
		cmsg_data = CMSG_DATA(cmsg);
		memcpy(msg->fds, cmsg_data, msg->num_fds*sizeof(int));

		// Close any fd that could not have been passed in mmipc_msg
		for (i = msg->num_fds_max; i < num_fd; i++) {
			memcpy(&passed_fd, cmsg_data + i*sizeof(int), sizeof(int));
			close(passed_fd);
			msg->flags |= MSG_CTRUNC;
		}
	}

	return rsz;
}


/**
 * mmipc_connected_pair() - create a pair of connected IPC endpoints
 * @fds:        array receiving the file descriptor of the 2 endpoints
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mmipc_connected_pair(int fds[2])
{
	int rv;

	rv = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds);
	if (rv != 0) {
		mm_raise_from_errno("Failed to connect to local IPC server");
		return -1;
	}

	return 0;
}
