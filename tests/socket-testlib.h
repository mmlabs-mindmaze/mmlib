/*
   @mindmaze_header@
*/
#ifndef SOCKET_TESTLIB_H
#define SOCKET_TESTLIB_H

#include <stddef.h>

#define DGRAM_MAXSIZE	8192

struct socket_data {
	size_t len;
	char buf[DGRAM_MAXSIZE];
};

struct testsocket_order {
	enum {READ_SOCKET_DATA, WRITE_SOCKET_DATA} cmd;
	size_t opt_len;
};

int create_server_socket(int domain, int type, int port);
int create_client_socket(int domain, int type, const char* host, int port);

int pipe_write_socketdata(int pipefd, const struct socket_data* data);
int pipe_read_socketdata(int pipefd, struct socket_data* data);

void run_socket_client(int wr_fd, int rd_fd, int narg, char* argv[]);

#endif
