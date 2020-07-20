/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "api-testcases.h"
#include "mmerrno.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "mmsysio.h"
#include "socket-testlib.h"
#include "tests-child-proc.h"

#define PORT	32145
#define MULTIMSG_LEN	6
#define IOV_MAXLEN	8

static size_t socket_data_len[] = {
	4, 16, 64, 325, 512, 1000, 4096, 3000, 6951, 2412, 8192,
};

struct testsocket_config_case {
	int domain;
	int socktype;
};

static
struct testsocket_config_case test_cases[] = {
	{.domain = AF_INET, .socktype = SOCK_STREAM},
	{.domain = AF_INET, .socktype = SOCK_DGRAM},
	{.domain = AF_INET6, .socktype = SOCK_STREAM},
	{.domain = AF_INET6, .socktype = SOCK_DGRAM},
};
#define FIRST_IPV6_TEST_CASE	2


struct childproc {
	int wr_pipe_fd;
	int rd_pipe_fd;
	mm_pid_t pid;
	int sockfd;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
};


/**
 * spawn_childproc() - spawn a child test process with connected pipes
 * @child;      childproc structure to init
 * ...:         NULL terminated variable argument list to be supplied to child
 *              process spawn
 *
 * Execute the program tests-child-proc in a child process and connect it with
 * 2 pipe, one for reading and one for writing. The program will execute the
 * function "run_socket_client" with the argument passed in the variable
 * argument list (terminated by NULL). The data regarding child and pipes are
 * stored in @child.
 */
static
void spawn_childproc(struct childproc* child, ...)
{
	int i, pipe_fds_tochild[2], pipe_fds_fromchild[2];
	struct mm_remap_fd fdmap[2];
	va_list args;
	char *argv[16];
	char* arg;

	// Setup argument list array for passing to child creation function
	i = 0;
	argv[i++] = TESTS_CHILD_BIN;
	argv[i++] = "run_socket_client";
	va_start(args, child);
	do {
		arg = va_arg(args, char*);
		argv[i++] = arg;
	} while (arg);
	va_end(args);

	// Create pipe
	ck_assert(mm_pipe(pipe_fds_tochild) == 0);
	ck_assert(mm_pipe(pipe_fds_fromchild) == 0);

	child->wr_pipe_fd = pipe_fds_tochild[1];
	child->rd_pipe_fd = pipe_fds_fromchild[0];

	fdmap[0] = (struct mm_remap_fd){WR_PIPE_FD, pipe_fds_fromchild[1]};
	fdmap[1] = (struct mm_remap_fd){RD_PIPE_FD, pipe_fds_tochild[0]};

	// spawn child process
	ck_assert(mm_spawn(&child->pid, argv[0], 2, fdmap, 0, argv, NULL) == 0);

	// Close pipe end meant for child process
	mm_close(fdmap[0].parent_fd);
	mm_close(fdmap[1].parent_fd);
}


/**
 * clean_childproc() - stop and clean child
 * @child:      pointer to childproc holding the data relative to child
 *
 * Call this function when you want to stop the child process. The order of the
 * connection of connection stop will make the child process to stop normally.
 *
 * Typically called in the teardown function of the tests
 */
static
void clean_childproc(struct childproc* child)
{
	mm_close(child->wr_pipe_fd);
	mm_close(child->sockfd);
	mm_close(child->rd_pipe_fd);

	child->wr_pipe_fd = -1;
	child->sockfd = -1;
	child->rd_pipe_fd = -1;

	if (child->pid)
		mm_wait_process(child->pid, NULL);

	child->pid = 0;
}


/**
 * childproc_order_read() - instruct child to read from its socket end and return data to pipe
 * @child:      pointer to childproc holding the data relative to child
 * @len:        length of the data to try to read from socket
 * @data:       socket_data structure to use to get the data that child will return
 */
static
void childproc_order_read(struct childproc* child, size_t len, struct socket_data* data)
{
	ssize_t rsz;
	struct testsocket_order order = {
		.cmd = READ_SOCKET_DATA,
		.opt_len = len,
	};

	// Instruct (over pipe) child to read data from its socket endpoint
	rsz = mm_write(child->wr_pipe_fd, &order, sizeof(order));
	ck_assert(rsz == (ssize_t)sizeof(order));

	// The child should have returned what it has read from socket onto the
	// pipe, so get it
	ck_assert(pipe_read_socketdata(child->rd_pipe_fd, data) == 0);
}


/**
 * childproc_order_read() - instruct child to write data on its socket end
 * @child:      pointer to childproc holding the data relative to child
 * @data:       socket_data structure containining data child has to send to socket
 */
static
void childproc_order_write(struct childproc* child, struct socket_data* data)
{
	ssize_t rsz;
	struct testsocket_order order = {
		.cmd = WRITE_SOCKET_DATA,
	};

	// Instruct (over pipe) child to write data on its socket endpoint
	rsz = mm_write(child->wr_pipe_fd, &order, sizeof(order));
	ck_assert(rsz == (ssize_t)sizeof(order));

	// Send over the pipe the data that child must send on the socket
	ck_assert(pipe_write_socketdata(child->wr_pipe_fd, data) == 0);
}


/**
 * create_connected_socket_and_child() - setup child and socket to test
 * @child:      pointer to childproc holding the data relative to child
 * @domain:     communications domain in which a socket is to be created (AF_INET or AF_INET6)
 * @socktype:   type of socket to create (SOCK_STREAM or SOCK_DGRAM)
 *
 * This function creates a server socket according to @domain and @socktype,
 * spawn a child process executing tests-child-proc with the function
 * "run_socket_client" which will try to connect the server socket. This
 * process will then:
 *
 * - if TCP: accept the connection and return the connected socket
 * - if UDP: read the first datagram that client is supposed to send in order
 *           to bind the server socket to it.
 *
 * This means that in any case, this function return a connected socket to
 * client (no matter it is SOCK_DGRAM or SOCK_STREAM).
 *
 * This internal data use to communicate with child process is stored in
 * @child.
 *
 * Return: file descriptor of connected socket to child
 */
static
int create_connected_socket_and_child(struct childproc* child,
                                      int domain, int socktype)
{
	int fd;
	int timeout = 500;
	char domain_str[16], socktype_str[16];

	// Create listening socket
	fd = create_server_socket(domain, socktype, PORT);
	ck_assert(fd != -1);

	// Make server socket listening in case of TCP
	if (socktype == SOCK_STREAM)
		ck_assert(mm_listen(fd, 10) == 0);

	// Spawn child for the test
	sprintf(domain_str, "%i", domain);
	sprintf(socktype_str, "%i", socktype);
	spawn_childproc(child,
	                domain_str, socktype_str, "localhost", MM_STRINGIFY(PORT), NULL);

	child->peer_addr_len = sizeof(child->peer_addr);
	if (socktype == SOCK_DGRAM) {
		struct msghdr msg = {
			.msg_name = &child->peer_addr,
			.msg_namelen = child->peer_addr_len,
		};

		// Get first packet from client (when UDP, the child process
		// send a first packet with empty payload). And use it to know
		// the address and connect to the client socket
		ck_assert(mm_recvmsg(fd, &msg, 0) == 0);
		ck_assert(mm_connect(fd, msg.msg_name, msg.msg_namelen) == 0);
		child->peer_addr_len = msg.msg_namelen;

		child->sockfd = fd;
	} else {
		// Accept incoming TCP connection and close listening socket
		child->sockfd = mm_accept(fd, (struct sockaddr*)&child->peer_addr,
		                              &child->peer_addr_len);
		mm_close(fd);
		ck_assert(child->sockfd != -1);
	}

	ck_assert(mm_setsockopt(child->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
	return child->sockfd;
}


/**
 * gen_random_buffer() - fill buffer with random data
 * @buffer:     pointer to buffer to fill
 * @len:        size of buffer
 */
static
void gen_random_buffer(void* buffer, size_t len)
{
	int* int_buf = buffer;
	int rand_val;

	while (len >= sizeof(int)) {
		*(int_buf++) = rand();
		len -= sizeof(int);
	}

	rand_val = rand();
	memcpy(int_buf, &rand_val, len);
}


/**
 * gen_socket_data() - generate random a piece of specific size
 * @data:       socket data to initialize
 * @len:        length of the data
 */
static
void gen_socket_data(struct socket_data* data, size_t len)
{
	ck_assert(len <= sizeof(data->buf));
	data->len = len;
	gen_random_buffer(data->buf, len);
}


/**
 * gen_random_int() - draw a random int from uniform distribution of [0,max]
 * @max:        maximum value that the function can generate
 *
 * Return: random int between 0 and @max
 */
static
int gen_random_int(int max)
{
	return (max+1)*((double)rand()/RAND_MAX);
}


/**
 * gen_random_msg_iov() - generate random split of scatter/gather buffers of msg
 * @num_iov_max:        maximum number of scatter/gather buffers
 * @msg:                msghdr structure whose iov should be configured
 * @buf:                buffer that will back the scatter/gather buffers
 * @buflen:             size of @buf
 *
 * This function configures the msg_iov split of @msg with a random lookup
 * partition of a buffer @buf of size @buflen. The partition is random, nothing
 * prevents that the elements in @msg->msg_iov will not overlap.
 *
 * It is however guaranteed that:
 * - the element in @msg->msg_iov will always point inside @buf
 * - the combined size of @msg->msg_iov elements will be less or equal to @buflen
 */
static
void gen_random_msg_iov(int num_iov_max, struct msghdr* msg, void* buf, size_t buflen)
{
	int i, offset;
	size_t len, rem_sz;
	char* cbuf = buf;

	rem_sz = buflen;
	for (i = 0; i < num_iov_max; i++) {
		offset = gen_random_int(buflen-1);
		len = gen_random_int(buflen-offset);
		if (len > rem_sz)
			len = rem_sz;

		msg->msg_iov[i].iov_base = cbuf + offset;
		msg->msg_iov[i].iov_len = len;

		rem_sz -= len;
		if (rem_sz == 0) {
			i += 1;
			break;
		}
	}

	msg->msg_iovlen = i;
}


/**
 * copy_msg_data_to_buffer() - copy scatter/gather buffers content to flat buffer
 * @msg:        msghdr whose scatter/gather buffers must be copied from
 * @buffer:     flat buffer to which the data must be copied
 */
static
void copy_msg_data_to_buffer(const struct msghdr* msg, void* buffer)
{
	int i;
	size_t iovlen;
	char* dstbuf;
	int num_iov = msg->msg_iovlen;

	dstbuf = buffer;
	for (i = 0; i < num_iov; i++) {
		iovlen = msg->msg_iov[i].iov_len;
		memcpy(dstbuf, msg->msg_iov[i].iov_base, iovlen);
		dstbuf += iovlen;
	}
}


/**
 * copy_buffer_to_msg_data() - copy flat buffer to scatter/gather buffers
 * @buffer:     flat buffer from which which the data must be copied
 * @msg:        msghdr whose scatter/gather buffers must receive the data
 */
static
void copy_buffer_to_msg_data(const void* buffer, const struct msghdr* msg)
{
	int i;
	size_t iovlen;
	const char* srcbuf;

	srcbuf = buffer;
	for (i = 0; i < (int)msg->msg_iovlen; i++) {
		iovlen = msg->msg_iov[i].iov_len;
		memcpy(msg->msg_iov[i].iov_base, srcbuf, iovlen);
		srcbuf += iovlen;
	}
}


/**
 * cmp_msg_dat() compare content of scatter/gather buffers of 2 msghdr
 * msg1:        first message to compare
 * msg2:        second message to compare
 *
 * Return: 0 if the data contained in scatter/gather buffers of both message
 * match, non-zero otherwise.
 */
static
int cmp_msg_data(const struct msghdr* msg1, const struct msghdr* msg2)
{
	int i, r;
	size_t iovlen;
	struct iovec *iov1 = msg1->msg_iov;
	struct iovec *iov2 = msg2->msg_iov;
	int num_iov = msg1->msg_iovlen;

	if (msg1->msg_iovlen != msg2->msg_iovlen)
		return -1;

	for (i = 0; i < num_iov; i++) {
		iovlen = iov1[i].iov_len;
		if (iovlen != iov2[i].iov_len)
			return -1;

		r = memcmp(iov1[i].iov_base, iov2[i].iov_base, iovlen);
		if (r != 0)
			return r;
	}

	return 0;
}


/**
 * get_iov_size() - compute the total size held scatter/gather buffers
 * @msg:        msghdr whose scatter/gather buffers must be inspected
 *
 * Return: summed size of @msg->msg_iov[*].iov_len.
 */
static
size_t get_iov_size(const struct msghdr* msg)
{
	size_t sz;
	int i;
	int num_iov = msg->msg_iovlen;

	sz = 0;
	for (i = 0; i < num_iov; i++)
		sz += msg->msg_iov[i].iov_len;

	return sz;
}


/**
 * copy_msg_iov() - copy the split of scatter/gather buffer of a message
 * @src:        source message
 * @dst:	message to which the split must be copied
 *
 * This function copy the split of @scr into @dst. The buffers element are
 * still backed by the buffer used in @src. Use retarget_msg_iov() to change
 * this.
 */
static
void copy_msg_iov(const struct msghdr* src, struct msghdr* dst)
{
	memcpy(dst->msg_iov, src->msg_iov,
	       src->msg_iovlen*sizeof(src->msg_iov[0]));

	dst->msg_iovlen = src->msg_iovlen;
}


/**
 * retarget_msg_iov() - change the backing flat buffer of the scatter/gather buffers
 * @msg:        message whose iov structures must be changed
 * @prev:       pointer to the previous backing buffer
 * @new:        pointer to the new backing buffer
 *
 * This function assumes that the scatter/gather buffers of the message
 * point inside the same buffer. While this is not the case in general, it
 * is true in the context of those current tests.
 *
 * This takes the @new and @prev argument to compute the offset to apply
 * to the element @msg->msg_iov[*].iov_base so that the new element now
 * point inside the buffer @new. (this assumes that the allocated size of
 * @new and @prev are consistent).
 */
static
void retarget_msg_iov(struct msghdr* msg, const void* prev, const void* new)
{
	int i;
	char* base_ptr;
	int num_iov = msg->msg_iovlen;
	intptr_t offset = (intptr_t)new - (intptr_t)prev;

	for (i = 0; i < num_iov; i++) {
		base_ptr = msg->msg_iov[i].iov_base;
		msg->msg_iov[i].iov_base = base_ptr + offset;
	}
}


/**
 * setup_recvmsg_test_iteration() - prepare messages for recvmsg and request write from child
 * @child:      pointer to childproc holding the data relative to child
 * @size_in_iov: size held by scatter/buffer in messages
 * @msg_ref:    reference message
 * @buff_ref:   flat buffer backing @msg_ref
 * @msg_test:   test message
 * @buff_rest:  flat buffer backing @msg_test
 *
 * setup 2 message using the same split for their scatter/gather buffers but
 * backed by 2 different flat buffers. Generate random data that is copied
 * into @msg_ref (simulating read from net interface into @msg_ref) and
 * order the child to write the same random data onto the socket.
 */
static
void setup_recvmsg_test_iteration(struct childproc* child, size_t size_in_iov,
                                  struct msghdr* msg_ref, void* buff_ref,
                                  struct msghdr* msg_test, void* buff_test)
{
	struct socket_data data;

	memset(buff_ref, 0, DGRAM_MAXSIZE);
	memset(buff_test, 0, DGRAM_MAXSIZE);

	// Generate 2 msg with identical iov split, but backed by 2 different
	// buffers (ie, iov element have same size and same offset but point to
	// 2 different buffers)
	gen_random_msg_iov(IOV_MAXLEN, msg_ref, buff_ref, size_in_iov);
	copy_msg_iov(msg_ref, msg_test);
	retarget_msg_iov(msg_test, buff_ref, buff_test);

	// generate random data that child will feed to socket
	gen_socket_data(&data, get_iov_size(msg_ref));
	copy_buffer_to_msg_data(data.buf, msg_ref);

	// instruct child to read data from pipe and issue a send call to
	// socket with it
	childproc_order_write(child, &data);
}


/**************************************************************************
 *                                                                        *
 *                       socket tests implementation                      *
 *                                                                        *
 **************************************************************************/
static struct childproc child = {
	.wr_pipe_fd = -1,
	.rd_pipe_fd = -1,
	.pid = 0,
	.sockfd = -1,
};


START_TEST(recv_on_localhost)
{
	int sockfd, i, flags;
	size_t req_sz;
	ssize_t rsz;
	struct socket_data data;
	char buffer[DGRAM_MAXSIZE];
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;

	flags = (socktype == SOCK_STREAM) ? MSG_WAITALL : 0;

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to pipe and check that child has sent this
	// data on socket
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		gen_socket_data(&data, socket_data_len[i]);
		childproc_order_write(&child, &data);

		// If not SOCK_STREAM, message boundaries must be kept. So in
		// such a case, received size must equal equal to what we have
		// sent, ie if we request more, we must received what has been
		// sent into the pipe
		req_sz = (socktype == SOCK_STREAM) ? data.len : sizeof(buffer);
		rsz = mm_recv(sockfd, buffer, req_sz, flags);

		ck_assert_int_eq(rsz, data.len);
		ck_assert(memcmp(data.buf, buffer, data.len) == 0);
	}
}
END_TEST


START_TEST(send_on_localhost)
{
	int sockfd, i;
	ssize_t rsz;
	struct socket_data data;
	char buffer[DGRAM_MAXSIZE];
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;
	size_t buf_sz;

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to pipe and check that child has sent this
	// data on socket
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		buf_sz = socket_data_len[i];
		gen_random_buffer(buffer, buf_sz);
		rsz = mm_send(sockfd, buffer, buf_sz, 0);
		ck_assert(rsz >= 0);
		childproc_order_read(&child, sizeof(data.buf), &data);

		ck_assert_int_eq(rsz, data.len);
		ck_assert(memcmp(data.buf, buffer, data.len) == 0);
	}
}
END_TEST


START_TEST(read_on_localhost)
{
	int sockfd, i, k;
	ssize_t rsz, rsz_tmp;
	size_t req_sz;
	struct socket_data data;
	char buffer[DGRAM_MAXSIZE];
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to pipe and check that child has sent this
	// data on socket
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		gen_socket_data(&data, socket_data_len[i]);
		childproc_order_write(&child, &data);

		if (socktype == SOCK_STREAM) {
			// If SOCK_STREAM (TCP), the mm_read() call is allowed
			// to read less than requested
			req_sz = data.len;
			rsz = 0;
			for (k = 0; (k < 100) && (rsz < (ssize_t)req_sz); k++) {
				rsz_tmp = mm_read(sockfd, buffer+rsz, req_sz-rsz);
				if (rsz_tmp < 0)
					break;

				rsz += rsz_tmp;
			}
		} else {
			// If not SOCK_STREAM, message boundaries must be kept.
			// So in such a case, received size must equal to what
			// we have sent, ie if we request more, we must
			// received what has been sent into the pipe
			rsz = mm_read(sockfd, buffer, sizeof(buffer));
		}
		ck_assert_int_eq(rsz, data.len);
		ck_assert(memcmp(data.buf, buffer, data.len) == 0);
	}
}
END_TEST


START_TEST(write_on_localhost)
{
	int sockfd, i;
	ssize_t rsz;
	struct socket_data data;
	char buffer[DGRAM_MAXSIZE];
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;
	size_t buf_sz;

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to pipe and check that child has sent this
	// data on socket
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		buf_sz = socket_data_len[i];
		gen_random_buffer(buffer, buf_sz);
		rsz = mm_write(sockfd, buffer, buf_sz);
		ck_assert_int_eq(rsz, buf_sz);
		childproc_order_read(&child, sizeof(data.buf), &data);

		ck_assert_int_eq(rsz, data.len);
		ck_assert(memcmp(data.buf, buffer, data.len) == 0);
	}
}
END_TEST


START_TEST(recvmsg_on_localhost)
{
	int sockfd, i, flags;
	ssize_t rsz;
	char buffer_test[DGRAM_MAXSIZE], buffer_ref[DGRAM_MAXSIZE];
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;
	struct iovec iov_test[IOV_MAXLEN], iov_ref[IOV_MAXLEN];
	struct msghdr msg_test = {.msg_iov = iov_test};
	struct msghdr msg_ref = {.msg_iov = iov_ref};

	flags = (socktype == SOCK_STREAM) ? MSG_WAITALL : 0;

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to pipe and check that child has sent this
	// data on socket
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		setup_recvmsg_test_iteration(&child, socket_data_len[i],
		                             &msg_ref, buffer_ref,
		                             &msg_test, buffer_test);

		rsz = mm_recvmsg(sockfd, &msg_test, flags);

		// Check data receing from mm_recvmsg match expected results
		ck_assert_int_eq(rsz, get_iov_size(&msg_ref));
		ck_assert(cmp_msg_data(&msg_ref, &msg_test) == 0);
		ck_assert(memcmp(buffer_ref, buffer_test, sizeof(buffer_test)) == 0);
	}
}
END_TEST


START_TEST(sendmsg_on_localhost)
{
	int sockfd, i;
	ssize_t rsz;
	struct socket_data data;
	char data_src[DGRAM_MAXSIZE], buffer_ref[DGRAM_MAXSIZE];
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;
	struct iovec iov[IOV_MAXLEN];
	struct msghdr msg = {.msg_iov = iov};

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to socket and check that child has received
	// it properly (check this by inspecting what child send over pipe
	// which should match what we sent on socket)
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		// Generate random data and iov (within size limits)
		gen_random_msg_iov(IOV_MAXLEN, &msg, data_src, socket_data_len[i]);
		gen_random_buffer(data_src, DGRAM_MAXSIZE);

		// order child to read data from socket obtained through
		// mm_sendmsg
		rsz = mm_sendmsg(sockfd, &msg, 0);
		ck_assert(rsz >= 0);
		childproc_order_read(&child, sizeof(data.buf), &data);

		// Simulate what does sendmsg do
		copy_msg_data_to_buffer(&msg, buffer_ref);

		// Check data received from mm_recvmsg match expected results
		ck_assert_int_eq(rsz, get_iov_size(&msg));
		ck_assert_int_eq(rsz, data.len);
		ck_assert(memcmp(data.buf, buffer_ref, data.len) == 0);
	}
}
END_TEST


START_TEST(recv_multimsg_on_localhost)
{
	int sockfd, i, j, num_msg, flags;
	size_t len;
	char fbuffer_test[MULTIMSG_LEN*DGRAM_MAXSIZE];
	char fbuffer_ref[MULTIMSG_LEN*DGRAM_MAXSIZE];
	char *buff_test, *buff_ref;
	struct iovec iov[MULTIMSG_LEN*IOV_MAXLEN];
	struct iovec iov_ref[MULTIMSG_LEN*IOV_MAXLEN];
	struct msghdr *msg_test, *msg_ref;
	struct mm_sock_multimsg msgvec_ref[MULTIMSG_LEN] = {{.datalen = 0}};
	struct mm_sock_multimsg msgvec_test[MULTIMSG_LEN] = {{.datalen = 0}};
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;

	flags = (socktype == SOCK_STREAM) ? MSG_WAITALL : 0;

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to pipe and check that child has sent this
	// data on socket
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		// Setup multiple msg (ref and test) and issue write order to
		// child so that we can retrieve all those msg data in one call
		// to mm_recv_multimsg()
		for (j = 0; j < MULTIMSG_LEN; j++) {
			msg_ref = &msgvec_ref[j].msg;
			msg_test = &msgvec_test[j].msg;
			buff_ref = fbuffer_ref + j*DGRAM_MAXSIZE;
			buff_test = fbuffer_test + j*DGRAM_MAXSIZE;

			msg_ref->msg_iov = iov_ref + j*IOV_MAXLEN;
			msg_test->msg_iov = iov + j*IOV_MAXLEN;

			setup_recvmsg_test_iteration(&child, socket_data_len[i],
                                                     msg_ref, buff_ref,
			                             msg_test, buff_test);
		}

		// Read all message data
		num_msg = mm_recv_multimsg(sockfd, MULTIMSG_LEN, msgvec_test, flags, NULL);
		ck_assert_int_eq(num_msg, MULTIMSG_LEN);

		// Check data receing from each individual msg match expected results
		for (j = 0; j < MULTIMSG_LEN; j++) {
			msg_ref = &msgvec_ref[j].msg;
			msg_test = &msgvec_test[j].msg;
			len = msgvec_test[j].datalen;
			ck_assert_int_eq(len, get_iov_size(msg_ref));
			ck_assert(cmp_msg_data(msg_ref, msg_test) == 0);
		}
		ck_assert(memcmp(fbuffer_ref, fbuffer_test, sizeof(fbuffer_test)) == 0);
	}
}
END_TEST


START_TEST(send_multimsg_on_localhost)
{
	int sockfd, i, j, num_msg;
	ssize_t len;
	struct socket_data data;
	char fdata_src[MULTIMSG_LEN*DGRAM_MAXSIZE], buff_ref[DGRAM_MAXSIZE];
	char* data_src;
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;
	struct iovec iov[DGRAM_MAXSIZE * IOV_MAXLEN];
	struct msghdr* msg;
	struct mm_sock_multimsg msgvec[MULTIMSG_LEN] = {{.datalen = 0}};

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	// Send a piece of data to pipe and check that child has sent this
	// data on socket
	for (i = 0; i < MM_NELEM(socket_data_len); i++) {
		for (j = 0; j < MULTIMSG_LEN; j++) {
			msg = &msgvec[j].msg;
			data_src = fdata_src + j*DGRAM_MAXSIZE;
			msg->msg_iov = iov + j*IOV_MAXLEN;

			// Generate random data and iov (within size limits)
			gen_random_msg_iov(IOV_MAXLEN, msg, data_src, socket_data_len[i]);
			gen_random_buffer(data_src, DGRAM_MAXSIZE);
		}

		num_msg = mm_send_multimsg(sockfd, MULTIMSG_LEN, msgvec, 0);
		ck_assert_int_eq(num_msg, MULTIMSG_LEN);

		for (j = 0; j < MULTIMSG_LEN; j++) {
			msg = &msgvec[j].msg;
			len = msgvec[j].datalen;
			ck_assert_int_eq(len, get_iov_size(msg));

			// Simulate what does sendmsg do
			childproc_order_read(&child, len, &data);
			copy_msg_data_to_buffer(msg, buff_ref);

			// Check data receing from mm_recvmsg match expected results
			ck_assert_int_eq(len, data.len);
			ck_assert(memcmp(data.buf, buff_ref, data.len) == 0);
		}
	}
}
END_TEST

START_TEST(test_poll_all_negative)
{
	struct mm_pollfd pollfds = {
		.fd = -1,
		.events = POLLIN | POLLOUT,
	};

	ck_assert(mm_poll(&pollfds, 1, 100) == 0);
}
END_TEST

START_TEST(test_poll_simple)
{
	struct mm_pollfd pollfds = {
		.fd = create_server_socket(AF_INET, SOCK_DGRAM, PORT),
		.events = POLLIN | POLLOUT,
	};

	ck_assert(pollfds.fd > 0);
	ck_assert(mm_poll(&pollfds, 1, 100) == 1);
	ck_assert((pollfds.revents & POLLOUT) != 0);  // can write
	mm_close(pollfds.fd);
}
END_TEST

/* test that if the fd is negative then the corresponding events field is
 * ignored and the revents field returns zero */
START_TEST(test_poll_ignore_negative_socket)
{
	struct mm_pollfd pollfds[] = {
		{
			.fd = -1,
			.events = POLLIN | POLLOUT,
		},
		{
			.fd = create_server_socket(AF_INET, SOCK_DGRAM, PORT),
			.events = POLLIN | POLLOUT,
		},
	};
	ck_assert(mm_poll(pollfds, 2, 100) == 1);  // only one valid fd
	ck_assert((pollfds[0].revents) == 0);  // empty event
	ck_assert((pollfds[1].revents & POLLOUT) != 0);  // can write
	mm_close(pollfds[0].fd);
	mm_close(pollfds[1].fd);
}
END_TEST

START_TEST(test_getsockname)
{
	int rv, fd;
	char service[16];
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	struct addrinfo *res = NULL;
	struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM,};

	snprintf(service, sizeof(service), "%i", PORT);
	rv = mm_getaddrinfo(NULL, service, &hints, &res);
	ck_assert(rv == 0 && res != NULL);

	fd = create_server_socket(AF_INET, SOCK_DGRAM, PORT);
	ck_assert(fd > 0);

	rv = mm_getsockname(fd,(struct sockaddr *) &addr, &addrlen);
	ck_assert(rv == 0);

	ck_assert(addr.sin_family == AF_INET);
	ck_assert(ntohs(addr.sin_port) == PORT);
	ck_assert(addr.sin_addr.s_addr == ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
	mm_freeaddrinfo(res);

	mm_close(fd);
}
END_TEST


START_TEST(test_getpeername)
{
	int sockfd;
	int domain = test_cases[_i].domain;
	int socktype = test_cases[_i].socktype;
	struct sockaddr_storage addr;
	socklen_t addrlen =  sizeof(addr);

	// Create connected socket and child process (the created child and
	// socket are cleaned up in teardown)
	sockfd = create_connected_socket_and_child(&child, domain, socktype);

	ck_assert(mm_getpeername(sockfd, (struct sockaddr*)&addr, &addrlen) != -1);
	ck_assert_int_eq(addrlen, child.peer_addr_len);
	ck_assert(memcmp(&addr, &child.peer_addr, addrlen) == 0);
}
END_TEST


static
void assert_addrinfo(struct addrinfo* rp, int exp_socktype, short exp_port)
{
	struct sockaddr_in* addrin;
	struct sockaddr_in6* addrin6;

	ck_assert(rp->ai_socktype == exp_socktype);
	if (rp->ai_family == AF_INET) {
		addrin = (struct sockaddr_in*)rp->ai_addr;
		ck_assert_int_eq(htons(addrin->sin_port), exp_port);
	} else if (rp->ai_family == AF_INET6) {
		addrin6 = (struct sockaddr_in6*)rp->ai_addr;
		ck_assert_int_eq(htons(addrin6->sin6_port), exp_port);
	} else {
		ck_abort_msg("Not matching address family");
	}
}


START_TEST(getaddrinfo_valid)
{
	struct addrinfo *rp, *res = NULL;
	struct addrinfo hints = {.ai_family = AF_UNSPEC};

	ck_assert(mm_getaddrinfo("localhost", "ssh", &hints, &res) == 0);
	for (rp = res; rp != NULL; rp = rp->ai_next)
		assert_addrinfo(rp, SOCK_STREAM, 22);
	mm_freeaddrinfo(res);

	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_socktype = SOCK_DGRAM;
	ck_assert(mm_getaddrinfo("localhost", "42", &hints, &res) == 0);
	for (rp = res; rp != NULL; rp = rp->ai_next)
		assert_addrinfo(rp, SOCK_DGRAM, 42);
	mm_freeaddrinfo(res);
}
END_TEST


START_TEST(getaddrinfo_error)
{
	struct addrinfo *res = NULL;
	struct addrinfo hints = {.ai_family = AF_INET};

	ck_assert(mm_getaddrinfo("notanhost.localdomain", "ssh", &hints, &res) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), MM_ENONAME);

	ck_assert(mm_getaddrinfo("localhost", "joke", &hints, &res) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), MM_ENOTFOUND);

	hints.ai_socktype = SOCK_DGRAM;
	ck_assert(mm_getaddrinfo("localhost", "ssh", &hints, &res) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), MM_ENOTFOUND);
	hints.ai_socktype = 0;

	ck_assert(mm_getaddrinfo(NULL, NULL, &hints, &res) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EINVAL);

	hints.ai_flags = AI_CANONNAME;
	ck_assert(mm_getaddrinfo(NULL, "ssh", &hints, &res) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EINVAL);
	hints.ai_flags = 0;

	hints.ai_flags = AI_NUMERICSERV;
	ck_assert(mm_getaddrinfo("localhost", "123joke", &hints, &res) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EINVAL);
	hints.ai_flags = 0;

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_UDP;
	ck_assert(mm_getaddrinfo("localhost", "ssh", &hints, &res) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EPROTOTYPE);
	hints.ai_socktype = 0;
	hints.ai_protocol = 0;
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                           socket helper tests                          *
 *                                                                        *
 **************************************************************************/
static int num_fd_to_close = 0;
static int fds_to_close[8];

static
void clean_helper_test_data(void)
{
	while (num_fd_to_close)
		mm_close(fds_to_close[--num_fd_to_close]);
}


static
void add_fd_to_close(int fd)
{
	ck_assert(num_fd_to_close != MM_NELEM(fds_to_close));
	fds_to_close[num_fd_to_close++] = fd;
}


static
int get_socktype(int fd)
{
	int socktype = -1;
	socklen_t len = sizeof(socktype);

	if (mm_getsockopt(fd, SOL_SOCKET, SO_TYPE, &socktype, &len)
	   || len != sizeof(socktype))
		return -1;

	return socktype;
}


static
int get_peer_port(int fd)
{
	struct sockaddr_in6 addr = {.sin6_port = 0};
	socklen_t len = sizeof(addr);

	if (mm_getpeername(fd, (struct sockaddr*)&addr, &len)
	   || len != sizeof(addr))
		return -1;

	return ntohs(addr.sin6_port);
}


static const struct {
	const char* uri;
	int exp_socktype;
	int exp_port;
} sockclient_cases[] = {
#if _WIN32
	{"msnp://localhost", SOCK_STREAM, 1863},
#else
	{"socks://localhost", SOCK_STREAM, 1080},
#endif
	{"ntp://localhost", SOCK_DGRAM, 123},
	{"tcp://localhost:" MM_STRINGIFY(PORT), SOCK_STREAM, PORT},
	{"udp://localhost:" MM_STRINGIFY(PORT), SOCK_DGRAM, PORT},
};


START_TEST(create_sockclient)
{
	int fd, socktype, port;
	int exp_socktype = sockclient_cases[_i].exp_socktype;
	int exp_port = sockclient_cases[_i].exp_port;
	const char* uri = sockclient_cases[_i].uri;

	// Try to create listening server socket (stream). If the address
	// is already bound, the port is already opened out of the process:
	// this is fine for us, we can use this one.
	if (exp_socktype == SOCK_STREAM) {
		fd = create_server_socket(AF_INET6, SOCK_STREAM, exp_port);
		if (fd != -1) {
			ck_assert(mm_listen(fd, 1) == 0);
			add_fd_to_close(fd);
		} else if (mm_get_lasterror_number() != EADDRINUSE) {
			ck_abort_msg("Cannot create server socket");
		}
	}

	fd = mm_create_sockclient(uri);
	if (fd == -1) {
		fprintf(stderr, "Failed to create server socker with uri: %s, port %d\n"
		        "If you do not support this protocol, or if this port is closed "
		        "you can ignore this failure\n",
		        uri, exp_port);
	}
	ck_assert(fd != -1);

	socktype = get_socktype(fd);
	port = get_peer_port(fd);
	mm_close(fd);

	ck_assert_int_eq(socktype, exp_socktype);
	ck_assert_int_eq(port, exp_port);
}
END_TEST


START_TEST(create_invalid_sockclient)
{
	ck_assert(mm_create_sockclient(NULL) == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EINVAL);

	ck_assert(mm_create_sockclient("localhost") == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EINVAL);

	ck_assert(mm_create_sockclient("dummy://localhost") == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), MM_ENOTFOUND);

	ck_assert(mm_create_sockclient("ssh://localhost:10") == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), ECONNREFUSED);

	ck_assert(mm_create_sockclient("tcp://localhost") == -1);
	ck_assert_int_eq(mm_get_lasterror_number(), EINVAL);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                          Test suite setup                              *
 *                                                                        *
 **************************************************************************/

static
void socket_test_teardown(void)
{
	int flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_IGNORE);
	clean_childproc(&child);
	clean_helper_test_data();
	mm_error_set_flags(flags, MM_ERROR_IGNORE);
}


LOCAL_SYMBOL
TCase* create_socket_tcase(void)
{
	int num_cases;

	TCase *tc = tcase_create("socket");

	tcase_add_checked_fixture(tc, NULL, socket_test_teardown);

	num_cases = MM_NELEM(test_cases);
	if (!strcmp(mm_getenv("MMLIB_DISABLE_IPV6_TESTS", "no"), "yes")) {
		fputs("Disable IPv6 socket tests\n", stderr);
		num_cases = FIRST_IPV6_TEST_CASE;
	}

	tcase_add_loop_test(tc, recv_on_localhost, 0, num_cases);
	tcase_add_loop_test(tc, send_on_localhost, 0, num_cases);
	tcase_add_loop_test(tc, read_on_localhost, 0, num_cases);
	tcase_add_loop_test(tc, write_on_localhost, 0, num_cases);
	tcase_add_loop_test(tc, recvmsg_on_localhost, 0, num_cases);
	tcase_add_loop_test(tc, sendmsg_on_localhost, 0, num_cases);
	tcase_add_loop_test(tc, recv_multimsg_on_localhost, 0, num_cases);
	tcase_add_loop_test(tc, send_multimsg_on_localhost, 0, num_cases);

	tcase_add_test(tc, test_poll_all_negative);
	tcase_add_test(tc, test_poll_simple);
	tcase_add_test(tc, test_poll_ignore_negative_socket);
	tcase_add_test(tc, test_getsockname);
	tcase_add_loop_test(tc, test_getpeername, 0, num_cases);
	tcase_add_test(tc, getaddrinfo_valid);
	tcase_add_test(tc, getaddrinfo_error);

	tcase_add_loop_test(tc, create_sockclient,
	                    0, MM_NELEM(sockclient_cases));
	tcase_add_test(tc, create_invalid_sockclient);

	return tc;
}

