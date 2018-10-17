/*
   @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <check.h>

#include <stdio.h>
#include <strings.h>

#include "mmerrno.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "mmsysio.h"

#include "tests-child-proc.h"

#define NUM_FILE 3

static mm_pid_t pid;
static int fds[2*NUM_FILE+3];
static struct mm_remap_fd fd_map[NUM_FILE];
static char filename[MM_NELEM(fds)][32];

static
int full_read(int fd, void* buf, size_t len)
{
	char* cbuf = buf;
	ssize_t rsz;

	while (len > 0) {
		rsz = mm_read(fd, cbuf, len);
		if (rsz < 0) {
			perror("failed to read file");
			return -1;
		}

		if (rsz == 0) {
			fprintf(stderr, "EOF reached (missing %i bytes)\n",
			                (unsigned int)len);
			return -1;
		}

		cbuf += rsz;
		len -= rsz;
	}

	return 0;
}

static
int check_expected_fd_content(int num_map, const struct mm_remap_fd* fd_map)
{
	int i;
	int parent_fd, child_fd;
	size_t exp_sz;
	char line[128], expected[128];

	for (i = 0; i < num_map; i++) {
		parent_fd = fd_map[i].parent_fd;
		child_fd = fd_map[i].child_fd;

		exp_sz = sprintf(expected, "fd = %i", child_fd);

		lseek(parent_fd, 0, SEEK_SET);
		if (full_read(parent_fd, line, exp_sz))
			return -1;

		if (memcmp(line, expected, exp_sz)) {
			fprintf(stderr, "failure:\nexpected: %s\ngot: %s\n", expected, line);
			return -1;
		}
	}

	return 0;
}

static
int spawn_child(int spawn_flags)
{
	int i;
	char cmd[] = BUILDDIR"/child-proc"EXEEXT;
	char* argv[] = {cmd, "opt1", "another opt2", "Whi opt3", MM_STRINGIFY(NUM_FILE), NULL};

	for (i = 3; i < MM_NELEM(fds); i++) {
		sprintf(filename[i], "file-test-%i", i);
		fds[i] = mm_open(filename[i], O_RDWR|O_TRUNC|O_CREAT, S_IRWXU);
	}

	printf("map_fd = [");
	for (i = 0; i < MM_NELEM(fd_map); i++) {
		fd_map[i].child_fd = i+3;
		fd_map[i].parent_fd = i+3+NUM_FILE;
		printf(" %i:%i", fd_map[i].child_fd, fd_map[i].parent_fd);
	}
	printf(" ]\n");
	fflush(stdout);

	if (mm_spawn(&pid, cmd, MM_NELEM(fd_map), fd_map, spawn_flags, argv, NULL) != 0) {
		mm_print_lasterror(NULL);
		return 1;
	}

	return 0;
}

static
void close_unlink(void)
{
	int i;
	for (i = 3; i < MM_NELEM(fds); i++) {
		mm_close(fds[i]);
		mm_unlink(filename[i]);
	}
}

static void test_teardown(void)
{
	int ival;

	close_unlink();

	if (pid != 0)
		mm_wait_process(pid, &ival);
	pid = 0;

	mm_unsetenv("TC_SPAWN_MODE");
}

START_TEST(spawn_simple)
{
	int rv ;
	pid = -23;
	rv = spawn_child(0);
	ck_assert(rv == 0);
	ck_assert(pid > 0);
	ck_assert(mm_wait_process(pid, NULL) == 0);
	pid = 0;

	ck_assert(check_expected_fd_content(MM_NELEM(fd_map), fd_map) == 0);
	close_unlink();
}
END_TEST

START_TEST(spawn_daemon)
{
	ck_assert(spawn_child(MM_SPAWN_DAEMONIZE) == 0);
	mm_relative_sleep_ms(100);  // wait for the daemon process to finish
	ck_assert(check_expected_fd_content(MM_NELEM(fd_map), fd_map) == 0);
	close_unlink();
}
END_TEST

START_TEST(spawn_error)
{
	int rv;

	/* will spawn an immediately defunct process: path to process is NULL */
	pid = -23;
	rv = mm_spawn(&pid, NULL, 0, NULL, MM_SPAWN_KEEP_FDS, NULL, NULL);
	ck_assert(rv != 0);
	ck_assert(pid == (mm_pid_t) -23);  // no process should be able to launch
	pid = 0;
}
END_TEST

#ifndef _WIN32
#include <sys/resource.h>

static const int errlimits_spawn_mode_cases[] = {0, MM_SPAWN_DAEMONIZE};

#define NUM_ERRLIMITS_CASES  MM_NELEM(errlimits_spawn_mode_cases)

START_TEST(spawn_error_limits)
{
	int rv;
	struct rlimit rlim_orig, rlim;
	int spawn_mode = errlimits_spawn_mode_cases[_i];

	/* set RLIMIT_NPROC to 1 process */
	ck_assert(getrlimit(RLIMIT_NPROC, &rlim_orig) == 0);
	rlim = rlim_orig;
	rlim.rlim_cur = 1;
	ck_assert(setrlimit(RLIMIT_NPROC, &rlim) == 0);

	pid = -23;
	rv = spawn_child(spawn_mode);

	/* restore RLIMIT_NPROC to original value */
	ck_assert(setrlimit(RLIMIT_NPROC, &rlim_orig) == 0);

	ck_assert(rv != 0);
	ck_assert(pid == (mm_pid_t) -23);
	ck_assert(mm_get_lasterror_number() == EAGAIN);

	pid = 0;
}
END_TEST

#endif /* _WIN32 */

START_TEST(spawn_daemon_error)
{
	int rv;

	/* will spawn an immediately defunct process: path to process is NULL */
	pid = -23;
	rv = mm_spawn(&pid, NULL, 0, NULL, MM_SPAWN_DAEMONIZE, NULL, NULL);
	ck_assert(rv == -1);
	ck_assert(pid == (mm_pid_t) -23);  // no process should be able to launch
	pid = 0;
}
END_TEST


START_TEST(spawn_invalid_args)
{
	int rv;

	/* cannot run "/" */
	pid = -23;
	rv = mm_spawn(&pid, "/", 0, NULL, MM_SPAWN_KEEP_FDS, NULL, NULL);
	ck_assert(rv == -1);
	ck_assert(pid == (mm_pid_t) -23);  // no process should be able to launch
	pid = 0;
}
END_TEST

/* this is to make sure that mm_wait_process() fails if the process
 * does not exists.
 * The system SHOULD not have given the same pid again just after we finish
 * waiting for it */
START_TEST(wait_twice)
{
	pid = -23;
	ck_assert(spawn_child(0) == 0);
	ck_assert(pid > 0);

	/* wait once */
	ck_assert(mm_wait_process(pid, NULL) == 0);
	ck_assert(check_expected_fd_content(MM_NELEM(fd_map), fd_map) == 0);
	close_unlink();

	/* wait a sacond time with the same pid */
	ck_assert(mm_wait_process(pid, NULL) != 0);

	pid = 0;
}
END_TEST

LOCAL_SYMBOL
TCase* create_process_tcase(void)
{
	TCase * tc;

	tc = tcase_create("process");
	tcase_add_checked_fixture(tc, NULL, test_teardown);

	tcase_add_test(tc, spawn_simple);
	tcase_add_test(tc, spawn_daemon);
	tcase_add_test(tc, spawn_error);
	tcase_add_test(tc, spawn_daemon_error);
	tcase_add_test(tc, spawn_invalid_args);
	tcase_add_test(tc, wait_twice);

#ifndef _WIN32
	tcase_add_loop_test(tc, spawn_error_limits, 0, NUM_ERRLIMITS_CASES);
#endif

	return tc;
}
