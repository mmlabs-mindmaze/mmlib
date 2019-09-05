#ifndef IPC_API_TESTS_EXPORTED_H
#define IPC_API_TESTS_EXPORTED_H

#include <stdio.h>

#include <mmsysio.h>
#include <mmthread.h>

API_EXPORTED intptr_t test_client_process(void * arg);

enum {
	SHARED_FILE,
	SHARED_MEM,
	SHARED_PIPE,
	SHARED_IPC,
};

struct ipc_test_ctx {
	int nclients;
	int run_mode;
	int index;
	int shared_object;
	int fd;
};

/* small helper for debug purposes */
static inline void
dump_ipc_test_ctx(const struct ipc_test_ctx * c)
{
	if (c == NULL)
		fprintf(stdout, "{NULL}\n");
	else
		fprintf(stdout, "{nclients=%d, run_mode=%d, index=%d, "
		        "shared_object=%d, fd=%d}\n",
		        c->nclients, c->run_mode, c->index,
		        c->shared_object, c->fd);
}

#define IPC_ADDR "mmlib-test-ipc-addr"
#define IPC_TMPFILE "ipc-test-tmp-file"

/* small helper to send a mmipc msg with a file descriptor in the metadatas */
static inline
ssize_t mmipc_build_send_msg(int fd, const void * data, size_t len, int sentfd)
{
	struct iovec vec = {.iov_len = len, .iov_base = (void*) data};
	struct mmipc_msg msg = {
		.iov = &vec,
		.num_iov = 1,
	};
	if (sentfd > 0) {
		msg.fds = &sentfd;
		msg.num_fds = 1;
	}

	return mmipc_sendmsg(fd, &msg);
}

/* small helper to receive a mmipc msg with a file descriptor in the metadatas */
static inline
ssize_t recv_msg_and_fd(int fd, void* data, size_t len, int* recvfd)
{
	struct iovec vec = {.iov_len = len, .iov_base = data};
	struct mmipc_msg msg = {
		.iov = &vec,
		.num_iov = 1,
		.fds = recvfd,
		.num_fds_max = 1,
	};

	return mmipc_recvmsg(fd, &msg);
}

/* Can open 1 OR 2 file descriptors.
 * Will return the one intended to be sent to the client */
static inline
int open_shared_object_of_type(const struct ipc_test_ctx * ctx,
                               int * rvfd)
{
	char filename[64];

	switch (ctx->shared_object) {
	case SHARED_FILE:
		sprintf(filename, "%s-%d", IPC_TMPFILE, ctx->index);
		*rvfd = mm_open(filename, O_CREAT|O_TRUNC|O_RDWR,
		                S_IWUSR|S_IRUSR);
		return *rvfd;

	case SHARED_MEM:
		*rvfd = mm_anon_shm();
		return *rvfd;

	case SHARED_PIPE:
		mm_pipe(rvfd);
		return rvfd[1];

	case SHARED_IPC:
		mmipc_connected_pair(rvfd);
		return rvfd[1];

	default:
		fprintf(stderr, "Test bug: no shared object type given");
		return -1;
	}
}

#endif /* IPC_API_TESTS_EXPORTED_H */
