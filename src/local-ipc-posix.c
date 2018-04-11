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

API_EXPORTED
struct mmipc_srv* mmipc_srv_create(const char* addr)
{
	int fd = -1;
	struct sockaddr_un address = {.sun_family = AF_UNIX};
	struct mmipc_srv* srv;

	// Copy the socket address. It must start with a null byte ('\0') to
	// obtain an abstract socket address. Without it would be bound to the
	// filesystem (which we do not want)
	address.sun_path[0] = '\0';
	strncpy(address.sun_path+1, addr, sizeof(address.sun_path)-1);

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


API_EXPORTED
void mmipc_srv_destroy(struct mmipc_srv* srv)
{
	if (!srv)
		return;

	mm_close(srv->listenfd);
	free(srv);
}



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
		// does not have type alignment garantees
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
