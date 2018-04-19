/*
   @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <check.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>

#include "mmerrno.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "mmsysio.h"
#include "mmthread.h"
#include "mmtime.h"

#include "tests-child-proc.h"
#include "ipc-api-tests-exported.h"

#define MAX_NCLIENTS 32
static int nclients = 0;

static struct mmipc_srv * srv;

static
void test_teardown(void)
{
	int i;
	char filename[64];

	mmipc_srv_destroy(srv);
	srv = NULL;

	for (i = 0 ; i < 5 ; i ++) {
		sprintf(filename, "%s-%d", IPC_TMPFILE, i);
		mm_unlink(filename);
	}
}

START_TEST(ipc_create_simple)
{
	struct mmipc_srv * srv = mmipc_srv_create(IPC_ADDR);
	ck_assert(srv != NULL);
	mmipc_srv_destroy(srv);
}
END_TEST

/* check what happens if you give a null-terminated string longer that the maximum */
START_TEST(ipc_create_invalid)
{
	char name[257];
	memset(name, 'a', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';

	struct mmipc_srv * srv = mmipc_srv_create(name);
	ck_assert(srv == NULL);
	ck_assert(mm_get_lasterror_number() == ENAMETOOLONG);

	mmipc_srv_destroy(srv);
}
END_TEST


START_TEST(ipc_create_double)
{
	struct mmipc_srv * srv1, * srv2;

	srv1 = mmipc_srv_create(IPC_ADDR);
	ck_assert(srv1 != NULL);

	srv2 = mmipc_srv_create(IPC_ADDR);
	ck_assert(srv2 == NULL);
	ck_assert(mm_get_lasterror_number() == EADDRINUSE);

	mmipc_srv_destroy(srv1);
	mmipc_srv_destroy(srv2);
}
END_TEST

static
void* test_handle_client(void * arg)
{
	struct ipc_test_ctx * ctx = arg;
	char buf[256];
	int recvfd = -1;
	int pipe[2] = {-1, -1}; /* 0:read, 1:write */
	int tmpfd;
	const char data[] = "ipc server test msg";
	char line[80];
	void * exit_value = (void*) -1;

	if (recv_msg_and_fd(ctx->fd, buf, sizeof(buf), &recvfd) < 0)
		goto cleanup;

	/* send the message with a file descriptor */
	tmpfd = open_shared_object_of_type(ctx, (int*) &pipe);
	if (tmpfd < 0)
		goto cleanup;

	if (mmipc_build_send_msg(ctx->fd, data, sizeof(data), tmpfd) < 0)
		goto cleanup;

	tmpfd = pipe[0];
	mm_close(pipe[1]);
	pipe[1] = -1;

	/* get another message from the client.
	 * (sent after the client finished writing to tmpfile)*/
	if (recv_msg_and_fd(ctx->fd, buf, sizeof(buf), &recvfd) < 0)
		goto cleanup;

	/* check the client message */
	if (  ctx->shared_object == SHARED_FILE
	   || ctx->shared_object == SHARED_MEM)
		mm_seek(tmpfd, 0, SEEK_SET);

	if (  mm_read(tmpfd, line, sizeof(line)) < 0
	   || strncmp(line, "client message in shared object\n",
	              sizeof("client message in shared object\n") - 1)) {
		ck_abort_msg(stderr,
		             "server failed to read the message written by the "
		             "client in the shared file");
		goto cleanup;
	}

	exit_value = NULL;

cleanup:
	if (exit_value != NULL) {
		exit_value = (void*)(intptr_t) mm_get_lasterror_number();
		mm_print_lasterror("%s() failed", __func__);
	}

	mm_close(pipe[0]);
	mm_close(pipe[1]);
	mm_close(recvfd);

	return exit_value;
}

/*
 * accept up to nclients connections, then return
 */
static
void* test_server_process(void * arg)
{
	int i;
	struct ipc_test_ctx * global_ctx = arg;
	struct ipc_test_ctx ctx[MAX_NCLIENTS];
	mmthread_t thid[MAX_NCLIENTS];

	for (i = 0; i < global_ctx->nclients; i++)
		ctx[i] = *global_ctx;

	for (i = 0; i < global_ctx->nclients; i++) {

		ctx[i].index = i;
		ctx[i].fd = mmipc_srv_accept(srv);
		if (ctx[i].fd == -1)
			goto cleanup;

		mmthr_create(&thid[i], test_handle_client, &ctx[i]);
	}

	for (i = 0; i < global_ctx->nclients; i++) {
		intptr_t rv = 0;
		mmthr_join(thid[i], (void**)rv);
		mm_close(ctx[i].fd);
		ctx[i].fd = 0;
	}

	test_teardown();
	return NULL;

cleanup:
	if (mm_get_lasterror_number() != 0)
		mm_print_lasterror("%s() failed", __func__);

	while (--i > 0) {
		mmthr_join(thid[i], NULL);
		mm_close(ctx[i].fd);
		ctx[i].fd = 0;
	}

	test_teardown();
	return (void*) -1;
}

static
intptr_t test_server_process_pending(void * arg)
{
	int i;
	struct ipc_test_ctx * global_ctx = arg;
	struct ipc_test_ctx ctx[MAX_NCLIENTS];
	mmthread_t thid[MAX_NCLIENTS];

	srv = mmipc_srv_create(IPC_ADDR);
	if (srv == NULL)
		return -1;

	for (i = 0; i < global_ctx->nclients; i++) {
		ctx[i] = *global_ctx;
		ctx[i].index = i;
		ctx[i].fd = mmipc_srv_accept(srv);
		if (ctx[i].fd == -1)
			goto cleanup;

		mmthr_create(&thid[i], test_handle_client, &ctx[i]);
	}

	for (i = 0; i < global_ctx->nclients; i++) {
		mmthr_join(thid[i], NULL);
		mm_close(ctx[i].fd);
		ctx[i].fd = 0;
	}

	test_teardown();
	return 0;

cleanup:
	if (mm_get_lasterror_number() != 0)
		mm_print_lasterror("%s() failed", __func__);

	while (--i > 0) {
		mmthr_join(thid[i], NULL);
		mm_close(ctx[i].fd);
		ctx[i].fd = 0;
	}

	test_teardown();
	return -1;
}

/*
 * Create a server
 * Create nclients children, each child connect to ipc server
 * Check the server read the expected pattern (datagram boundaries and order)
 * Exchange file descriptor
 * Client writes, server checks.
 *
 * The ipc server handling is split in two; and shared amongst all threads.
 * (only works with threads)
 */
static void
run_test_core_connected_file(struct ipc_test_ctx * ctx)
{
	int i;
	thread_proc_id clt_id[MAX_NCLIENTS];

	srv = mmipc_srv_create(IPC_ADDR);
	ck_assert_msg(srv != NULL, "failed to create ipc server");

	/* prepare N clients waiting for the server to launch */
	for (i = 0; i < ctx->nclients; i++) {
		run_function(&clt_id[i], test_client_process,
		             ctx, RUN_AS_THREAD);
	}

	/* wait just a little to make sure the clients are all waiting
	 * (this seems to always be the case anyway) */
	mm_relative_sleep_ms(100);

	/*
	 * start and launch the server. It will:
	 * - handle the N pending clients immediatly
	 * - clean and return
	 *
	 * test_server_process() makes uses of the global srv variable !
	 */
	if (test_server_process(ctx) != NULL) {
		ck_abort_msg("test_server_process failed");
	}

	/* wait for the clients to return */
	for (i = 0; i < ctx->nclients; i++)
		clean_function(clt_id[i], RUN_AS_THREAD);

	test_teardown();
}


static void
run_test_core_pending(struct ipc_test_ctx * ctx)
{
	int i;
	thread_proc_id srv_id;
	thread_proc_id clt_id[MAX_NCLIENTS];

	/* launch the server. It will:
	 * - enter a waiting state
	 * - handle N new clients
	 * - clean and return
	 */
	run_function(&srv_id, test_server_process_pending,
	             ctx, RUN_AS_THREAD);

	/* wait just a little to make sure the server is ready */
	mm_relative_sleep_ms(100);

	/* launch the clients to attack the server */
	for (i = 0; i < ctx->nclients; i++) {
		run_function(&clt_id[i], test_client_process, ctx,
		             ctx->run_mode);
	}

	/* wait for the clients to return */
	for (i = 0; i < ctx->nclients; i++)
		clean_function(clt_id[i], ctx->run_mode);

	/* wait for the server to return, then clean */
	clean_function(srv_id, RUN_AS_THREAD);
}


/*
 * Create IPC connected pair and ensure that data communication is really
 * full duplex
 */
#define TEST_STR1 "test string for pair"
#define TEST_STR2 "second test string for pair"
START_TEST(full_duplex)
{
	int fds[2];
	ssize_t rsz;
	char buffer[42];

	ck_assert(mmipc_connected_pair(fds) == 0);

	rsz = mm_write(fds[0], TEST_STR1, sizeof(TEST_STR1));
	ck_assert_int_eq(rsz, sizeof(TEST_STR1));
	rsz = mm_write(fds[1], TEST_STR2, sizeof(TEST_STR2));
	ck_assert_int_eq(rsz, sizeof(TEST_STR2));

	rsz = mm_read(fds[1], buffer, sizeof(buffer));
	ck_assert_int_eq(rsz, sizeof(TEST_STR1));
	ck_assert(memcmp(buffer, TEST_STR1, sizeof(TEST_STR1)) == 0);

	rsz = mm_read(fds[0], buffer, sizeof(buffer));
	ck_assert_int_eq(rsz, sizeof(TEST_STR2));
	ck_assert(memcmp(buffer, TEST_STR2, sizeof(TEST_STR2)) == 0);

	mm_close(fds[0]);
	mm_close(fds[1]);
}
END_TEST

START_TEST(broken_pipe)
{
	int fds[2];
	ssize_t rsz;
	char buffer[42];

	ck_assert(mmipc_connected_pair(fds) == 0);

	rsz = mm_write(fds[0], TEST_STR1, sizeof(TEST_STR1));
	ck_assert_int_eq(rsz, sizeof(TEST_STR1));

	rsz = mm_read(fds[1], buffer, sizeof(buffer));
	ck_assert_int_eq(rsz, sizeof(TEST_STR1));
	ck_assert(memcmp(buffer, TEST_STR1, sizeof(TEST_STR1)) == 0);

	mm_close(fds[1]);
	rsz = mm_write(fds[0], TEST_STR1, sizeof(TEST_STR1));
	ck_assert(rsz == -1);
	ck_assert(mm_get_lasterror_number() == EPIPE);
	mm_close(fds[0]);
}
END_TEST


/*
 * test to pass msg and file descriptors
 *
 * Create a server
 * Create nclients children, each child connect to ipc server
 * Check the server read the expected pattern (datagram boundaries and order)
 * Exchange file descriptor
 * Client writes, server checks.
 */
START_TEST(test_file_connected_thr)
{
	ck_assert(nclients > 0);

	struct ipc_test_ctx ctx = {
		.nclients = nclients,
		.run_mode = RUN_AS_THREAD,
		.shared_object = SHARED_FILE,
	};
	run_test_core_connected_file(&ctx);
}
END_TEST


static
const struct ipc_test_ctx test_pending_cases[] = {
	{.run_mode = RUN_AS_THREAD,  .shared_object = SHARED_PIPE},
	{.run_mode = RUN_AS_PROCESS, .shared_object = SHARED_PIPE},
	{.run_mode = RUN_AS_THREAD,  .shared_object = SHARED_FILE},
	{.run_mode = RUN_AS_PROCESS, .shared_object = SHARED_FILE},
	{.run_mode = RUN_AS_THREAD,  .shared_object = SHARED_MEM},
	{.run_mode = RUN_AS_PROCESS, .shared_object = SHARED_MEM},
	{.run_mode = RUN_AS_THREAD,  .shared_object = SHARED_IPC},
	{.run_mode = RUN_AS_PROCESS, .shared_object = SHARED_IPC},
};
#define NUM_PENDING_CASE (MM_NELEM(test_pending_cases))


/*
 * Create a server
 * Create nclients children, each child connect to ipc server
 * Check the server read the expected pattern (datagram boundaries and order)
 * Parent create a shared object and pass it to child
 * Children write a pattern, parent assert the pattern
 */
START_TEST(test_core_pending)
{
	ck_assert(nclients > 0);

	struct ipc_test_ctx ctx = {
		.nclients = nclients,
		.run_mode = test_pending_cases[_i].run_mode,
		.shared_object = test_pending_cases[_i].shared_object,
	};
	run_test_core_pending(&ctx);
}
END_TEST


LOCAL_SYMBOL
TCase* create_ipc_tcase(void)
{
	TCase * tc;

	nclients = atoi(mm_getenv("TC_IPC_NCLIENTS", "5"));

	tc = tcase_create("ipc");
	tcase_add_checked_fixture(tc, NULL, test_teardown);

	tcase_add_test(tc, ipc_create_simple);
	tcase_add_test(tc, ipc_create_invalid);
	tcase_add_test(tc, ipc_create_double);

	tcase_add_test(tc, full_duplex);
	tcase_add_test(tc, broken_pipe);

	/* test the ipc server with both
	 * connections pending before accept
	 * server ready before accepting connections */
	tcase_add_test(tc, test_file_connected_thr);

	/* server is up and running before starting the clients */
	tcase_add_loop_test(tc, test_core_pending, 0, NUM_PENDING_CASE);

	return tc;
}
