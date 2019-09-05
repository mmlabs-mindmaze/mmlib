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

API_EXPORTED
intptr_t test_client_process(void * arg)
{
	int fd;
	char buf[256];
	int exit_value = -1;
	int recvfd = -1;
	struct ipc_test_ctx * ctx = arg;
	const char data[] = "ipc client test msg";
	char line[80] = "client message in shared object\n";

	fd = mmipc_connect(IPC_ADDR);
	if (fd == -1)
		goto cleanup;

	if (mmipc_build_send_msg(fd, data, sizeof(data), -1) < 0
	   || recv_msg_and_fd(fd, buf, sizeof(buf), &recvfd) < 0)
		goto cleanup;

	if (ctx->shared_object == SHARED_FILE
	   || ctx->shared_object == SHARED_MEM)
		mm_seek(recvfd, 0, SEEK_SET);

	mm_write(recvfd, line, strlen(line));
	mm_close(recvfd);
	recvfd = -1;

	/* send another message after we finished writing */
	if (mmipc_build_send_msg(fd, data, sizeof(data), -1) < 0)
		goto cleanup;

	exit_value = 0;

cleanup:
	if (exit_value != 0) {
		exit_value = mm_get_lasterror_number();
		mm_print_lasterror("%s() failed", __func__);
	}

	mm_close(fd);
	mm_close(recvfd);

	return exit_value;
}
