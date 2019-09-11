/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "socket-testlib.h"
#include "mmsysio.h"
#include "mmerrno.h"
#include "mmlog.h"

#include <stdio.h>

static
int full_mm_read(int fd, void* buf, size_t len)
{
	char* cbuf = buf;
	ssize_t rsz;
	struct mm_error_state errstate;

	mm_save_errorstate(&errstate);

	while (len) {
		rsz = mm_read(fd, cbuf, len);
		if (rsz <= 0) {
			if (mm_get_lasterror_number() == EINTR)
				continue;

			return -1;
		}

		cbuf += rsz;
		len -= rsz;
	}
	mm_set_errorstate(&errstate);

	return 0;
}


static
int full_mm_write(int fd, const void* buf, size_t len)
{
	const char* cbuf = buf;
	ssize_t rsz;
	struct mm_error_state errstate;

	mm_save_errorstate(&errstate);

	while (len) {
		rsz = mm_write(fd, cbuf, len);
		if (rsz < 0) {
			if (mm_get_lasterror_number() == EINTR)
				continue;

			return -1;
		}

		cbuf += rsz;
		len -= rsz;
	}

	mm_set_errorstate(&errstate);

	return 0;
}


LOCAL_SYMBOL
int create_server_socket(int domain, int type, int port)
{
	int fd = -1, reuse = 1;
	char service[16];
	struct addrinfo *res = NULL;
	struct addrinfo hints = {.ai_family = domain, .ai_socktype = type};

	// Name resolution
	snprintf(service, sizeof(service), "%i", port);
	if (mm_getaddrinfo(NULL, service, &hints, &res))
		goto error;

	// Create bound socket
	if ((fd = mm_socket(domain, type, 0)) < 0
	  || mm_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
	  || mm_bind(fd, res->ai_addr, res->ai_addrlen) )
		goto error;

	mm_freeaddrinfo(res);
	return fd;

error:
	mm_print_lasterror("%s() failed", __func__);
	mm_freeaddrinfo(res);
	mm_close(fd);
	return -1;
}


LOCAL_SYMBOL
int create_client_socket(int domain, int type, const char* host, int port)
{
	int fd = -1;
	char service[16];
	int timeout = 500;
	struct addrinfo *res;
	struct addrinfo hints = {.ai_family = domain, .ai_socktype = type};


	// Name resolution
	snprintf(service, sizeof(service), "%i", port);
	if (mm_getaddrinfo(host, service, &hints, &res))
		goto error;

	// Create connected socket
	if ((fd = mm_socket(domain, type, 0)) < 0
	  || mm_connect(fd, res->ai_addr, res->ai_addrlen)
	  || mm_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) )
		goto error;

	mm_freeaddrinfo(res);

	// If UDP send an empty datagram so that peer can get its address
	if (type == SOCK_DGRAM) {
		if (mm_send(fd, NULL, 0, 0) != 0)
			goto error;
	}

	return fd;

error:
	mm_print_lasterror("%s() failed", __func__);
	mm_freeaddrinfo(res);
	mm_close(fd);
	return -1;
}


LOCAL_SYMBOL
int pipe_write_socketdata(int pipefd, const struct socket_data* data)
{
	mm_check(data->len <= sizeof(data->buf));

	if (full_mm_write(pipefd, &data->len, sizeof(data->len))
	  || full_mm_write(pipefd, data->buf, data->len) )
		return -1;

	return 0;
}


LOCAL_SYMBOL
int pipe_read_socketdata(int pipefd, struct socket_data* data)
{
	if (full_mm_read(pipefd, &data->len, sizeof(data->len)))
		return -1;

	mm_check(data->len <= sizeof(data->buf));

	if (full_mm_read(pipefd, data->buf, data->len))
		return -1;

	return 0;
}


static
int handle_read_data(int sockfd, int wr_pipe_fd, size_t len)
{
	ssize_t rsz;
	struct socket_data data;

	mm_check(len <= sizeof(data.buf));

	rsz = mm_recv(sockfd, data.buf, len, 0);
	if (rsz < 0)
		return -1;

	data.len = rsz;
	if (pipe_write_socketdata(wr_pipe_fd, &data))
		return -1;

	return 0;
}


static
int handle_write_data(int sockfd, int rd_pipe_fd)
{
	ssize_t rsz;
	struct socket_data data;

	if (pipe_read_socketdata(rd_pipe_fd, &data)
	  || (rsz = mm_send(sockfd, data.buf, data.len, 0)) < 0 )
		return -1;

	if (rsz < (ssize_t)data.len) {
		mmlog_error("data sent too short");
		return -1;
	}

	return 0;
}


LOCAL_SYMBOL
void run_socket_client(int wr_fd, int rd_fd, int narg, char* argv[])
{
	int sockfd, ret;
	int domain, socktype, port;
	const char* host;
	struct testsocket_order order;
	ssize_t rsz;

	if (narg < 4) {
		fprintf(stderr, "not enough argument");
		return;
	}

	domain = atoi(argv[0]);
	socktype = atoi(argv[1]);
	host = argv[2];
	port = atoi(argv[3]);

	sockfd = create_client_socket(domain, socktype, host, port);
	if (sockfd == -1)
		return;

	ret = 0;
	while (ret == 0) {
		rsz = mm_read(rd_fd, &order, sizeof(order));
		if (rsz < (ssize_t)sizeof(order))
			break;

		switch (order.cmd) {
		case WRITE_SOCKET_DATA:
			ret = handle_write_data(sockfd, rd_fd);
			break;

		case READ_SOCKET_DATA:
			ret = handle_read_data(sockfd, wr_fd, order.opt_len);
			break;
		}
	}

	if (mm_get_lasterror_number() != 0)
		mm_print_lasterror("%s() failed", __func__);
}
