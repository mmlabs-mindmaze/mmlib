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

#include "process-testlib.h"
#include "tests-child-proc.h"

#define UNSET_PID_VALUE ((mm_pid_t) -23)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// workaround for libtool on windows: we need to execute directly the
// binary (the folder of mmlib dll is added at startup of testapi). On
// other platform we must use the normal wrapper script located in BUILDDIR
#if defined(_WIN32)
#  define CHILDPROC_BINPATH     BUILDDIR"/"LT_OBJDIR"/child-proc.exe"
#else
#  define CHILDPROC_BINPATH     BUILDDIR"/child-proc"
#endif

#define TEST_DATADIR "process-data"

#define NOEXEC_FILE     "file-noexec"
#define UNKBINFMT_FILE  "file-unkfmt"
#define NOTEXIST_FILE   "does-not-exists"

static struct process_test_data* curr_data_in_test;
static char* path_envvar_saved = NULL;

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
int check_expected_fd_content(struct process_test_data* data)
{
	int i;
	int parent_fd, child_fd;
	size_t exp_sz;
	char line[128], expected[128];

	for (i = 0; i < NUM_FILE; i++) {
		parent_fd = data->fd_map[i].parent_fd;
		child_fd = data->fd_map[i].child_fd;

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
struct process_test_data* create_process_test_data(const char* file)
{
	int i, child_last_fd, pipe_fds[2];
	char name[32];
	const char* argv[] = {"opt1", "another opt2", "Whi opt3",
	                      MM_STRINGIFY(NUM_FILE)};
	struct process_test_data* data;

	data = malloc(sizeof(*data));
	*data = (struct process_test_data) {
		.pid = UNSET_PID_VALUE,
		.pipe_wr = -1,
		.pipe_rd = -1,
	};

	// Initial process command and args
	strcpy(data->cmd, file);
	for (i = 0; i < MM_NELEM(argv); i++)
		strcpy(data->argv_data[data->argv_data_len++], argv[i]);

	// Create open file descriptor (to pass to child for some of them)
	for (i = 0; i < MM_NELEM(data->fds); i++) {
		sprintf(name, "file-test-%i", i);
		data->fds[i] = mm_open(name, O_RDWR|O_TRUNC|O_CREAT, S_IRWXU);
		mm_unlink(name);
	}

	// Initialize fd parent->child fd mapping
	printf("map_fd = [");
	child_last_fd = 3; // first fd after STDERR
	for (i = 0; i < NUM_FILE; i++) {
		data->fd_map[i].child_fd = child_last_fd++;
		data->fd_map[i].parent_fd = data->fds[NUM_FILE+i];
		printf(" %i:%i", data->fd_map[i].child_fd,
		                 data->fd_map[i].parent_fd);
	}
	printf(" ]\n");
	fflush(stdout);

	// Add final remap for pipe
	mm_pipe(pipe_fds);
	data->pipe_rd = pipe_fds[0];
	data->pipe_wr = pipe_fds[1];
	data->fd_map[NUM_FILE] = (struct mm_remap_fd) {
		.child_fd = child_last_fd++,
		.parent_fd = data->pipe_wr,
	};

	// Store current process data for later in teardown
	curr_data_in_test = data;

	return data;
}


static
int spawn_child(int spawn_flags, struct process_test_data* data)
{
	char* argv[NUM_ARGS_MAX+2] = {NULL};
	int i;

	argv[0] = data->cmd;
	for (i = 0; i < data->argv_data_len; i++)
		argv[i+1] = data->argv_data[i];

	if (mm_spawn(&data->pid, data->cmd,
	             NUM_FDMAP, data->fd_map,
	             spawn_flags, argv, NULL) != 0) {
		mm_print_lasterror(NULL);
		return -1;
	}

	return 0;
}


static
int wait_pipe_close(struct process_test_data* data)
{
	char unused_buffer[1];
	ssize_t rsz;

	if (data->pipe_wr != -1) {
		mm_close(data->pipe_wr);
		data->pipe_wr = -1;
	}

	rsz = mm_read(data->pipe_rd, unused_buffer, sizeof(unused_buffer));
	return (rsz < 0) ? -1 : 0;
}


static
int wait_child(struct process_test_data* data)
{
	int rv;

	if (data->pid == UNSET_PID_VALUE)
		return 0;

	rv = mm_wait_process(data->pid, NULL);
	data->pid = UNSET_PID_VALUE;

	return rv;
}


static
void close_fds(struct process_test_data* data)
{
	int i;

	for (i = 0; i < MM_NELEM(data->fds); i++) {
		if (data->fds[i] == -1)
			continue;

		mm_close(data->fds[i]);
		data->fds[i] = -1;
	}

	if (data->pipe_wr != -1) {
		mm_close(data->pipe_wr);
		data->pipe_wr = -1;
	}

	if (data->pipe_rd != -1) {
		mm_close(data->pipe_rd);
		data->pipe_rd = -1;
	}
}


static
void destroy_process_test_data(struct process_test_data* data)
{
	if (data == NULL)
		return;

	wait_child(data);
	close_fds(data);

	free(data);

	curr_data_in_test = NULL;
}


static
void test_teardown(void)
{
	destroy_process_test_data(curr_data_in_test);
}


static
void case_setup(void)
{
	int i, fd;
	int garbage_data[128];
	char childproc_dir[sizeof(CHILDPROC_BINPATH)];

	// backup PATH environment for the tests
	path_envvar_saved = strdup(mm_getenv("PATH", NULL));

	// prepend PATH folders specific for this test case to PATH envvar
	mm_dirname(childproc_dir, CHILDPROC_BINPATH);
	mm_setenv("PATH", childproc_dir, MM_ENV_PREPEND);
	mm_setenv("PATH", BUILDDIR"/"TEST_DATADIR, MM_ENV_PREPEND);

	for (i = 0; i < MM_NELEM(garbage_data); i+=2) {
		garbage_data[i] = 0xDEADBEEF;
		garbage_data[i+1] = 0x7E1705;
	}

	mm_mkdir(TEST_DATADIR, 0777, MM_RECURSIVE);

	// Write a regular file that DOES NOT have the exec permission
	fd = mm_open(TEST_DATADIR "/" NOEXEC_FILE, O_CREAT|O_TRUNC|O_RDWR, 0666);
	mm_write(fd, garbage_data, sizeof(garbage_data));
	mm_close(fd);

	// Write a regular executable file with unknown binary format
	fd = mm_open(TEST_DATADIR "/" UNKBINFMT_FILE, O_CREAT|O_TRUNC|O_RDWR, 0777);
	mm_write(fd, garbage_data, sizeof(garbage_data));
	mm_close(fd);
}


static
void case_teardown(void)
{
	mm_remove(TEST_DATADIR, MM_DT_ANY|MM_RECURSIVE);

	// Restore PATH environment variable
	mm_setenv("PATH", path_envvar_saved, 1);
	free(path_envvar_saved);
}

/**************************************************************************
 *                                                                        *
 *                       process tests implementation                     *
 *                                                                        *
 **************************************************************************/

static
const char* binpath_cases[] = {
	CHILDPROC_BINPATH,
	"child-proc"EXEEXT,
	"child-proc",
};

START_TEST(spawn_simple)
{
	int rv ;
	const char* file = binpath_cases[_i];
	struct process_test_data* data = create_process_test_data(file);

	rv = spawn_child(0, data);
	ck_assert(rv == 0);
	ck_assert(data->pid != UNSET_PID_VALUE);
	ck_assert(wait_child(data) == 0);

	ck_assert(check_expected_fd_content(data) == 0);
}
END_TEST


START_TEST(execv_simple)
{
	int i, rv, last_kept_fd;
	const char* file = binpath_cases[_i];
	struct process_test_data* data = create_process_test_data(file);

	// Find the latest fd in the fds array of data
	last_kept_fd = MAX(data->pipe_rd, data->pipe_wr);
	for (i = 0; i < MM_NELEM(data->fds); i++) {
		if (data->fds[i] > last_kept_fd)
			last_kept_fd = data->fds[i];
	}

	rv = run_as_process(&data->pid, "test_execv_process",
	                    data, sizeof(*data), last_kept_fd);
	ck_assert(rv == 0);
	ck_assert(wait_child(data) == 0);
	ck_assert(check_expected_fd_content(data) == 0);
}
END_TEST


START_TEST(spawn_daemon)
{
	const char* file = binpath_cases[_i];
	struct process_test_data* data = create_process_test_data(file);

	ck_assert(spawn_child(MM_SPAWN_DAEMONIZE, data) == 0);
	wait_pipe_close(data);  // wait for the daemon process to finish
	ck_assert(check_expected_fd_content(data) == 0);
}
END_TEST


static
const struct {
	const char* path;
	int exp_err;
	int rv;
} error_cases[] = {
	{.path = "/",                                      .exp_err = EACCES},
	{.path = "./"TEST_DATADIR"/"NOEXEC_FILE,           .exp_err = EACCES},
	{.path = "./"TEST_DATADIR"/"UNKBINFMT_FILE,        .exp_err = ENOEXEC},
	{.path = "./"TEST_DATADIR"/"NOTEXIST_FILE,         .exp_err = ENOENT},
	{.path = BUILDDIR"/"TEST_DATADIR"/"NOEXEC_FILE,    .exp_err = EACCES},
	{.path = BUILDDIR"/"TEST_DATADIR"/"UNKBINFMT_FILE, .exp_err = ENOEXEC},
	{.path = BUILDDIR"/"TEST_DATADIR"/"NOTEXIST_FILE,  .exp_err = ENOENT},
	{.path = NOEXEC_FILE,                              .exp_err = EACCES},
	{.path = NOTEXIST_FILE,                            .exp_err = ENOENT},
};

START_TEST(spawn_error)
{
	int rv;
	mm_pid_t pid = UNSET_PID_VALUE;
	const char* path = error_cases[_i].path;

	/* will spawn an immediately defunct process: path to process is NULL */
	rv = mm_spawn(&pid, path, 0, NULL, 0, NULL, NULL);
	ck_assert(rv != 0);
	ck_assert(pid == UNSET_PID_VALUE);  // no process should be able to launch
	ck_assert_int_eq(mm_get_lasterror_number(), error_cases[_i].exp_err);
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
	struct process_test_data* data = create_process_test_data(CHILDPROC_BINPATH);

	/* set RLIMIT_NPROC to 1 process */
	ck_assert(getrlimit(RLIMIT_NPROC, &rlim_orig) == 0);
	rlim = rlim_orig;
	rlim.rlim_cur = 1;
	ck_assert(setrlimit(RLIMIT_NPROC, &rlim) == 0);

	rv = spawn_child(spawn_mode, data);

	/* restore RLIMIT_NPROC to original value */
	ck_assert(setrlimit(RLIMIT_NPROC, &rlim_orig) == 0);

	ck_assert(rv != 0);
	ck_assert(data->pid == UNSET_PID_VALUE);
	ck_assert(mm_get_lasterror_number() == EAGAIN);
}
END_TEST

#endif /* _WIN32 */

START_TEST(spawn_daemon_error)
{
	int rv;
	mm_pid_t pid = UNSET_PID_VALUE;
	const char* path = error_cases[_i].path;

	/* will spawn an immediately defunct process: path to process is NULL */
	rv = mm_spawn(&pid, path, 0, NULL, MM_SPAWN_DAEMONIZE, NULL, NULL);
	ck_assert(rv == -1);
	ck_assert(pid == UNSET_PID_VALUE);  // no process should be able to launch
	ck_assert_int_eq(mm_get_lasterror_number(), error_cases[_i].exp_err);
}
END_TEST


static
const struct {
	const char* path;
	int flags;
} inval_cases[] = {
	{.path = NULL, .flags = 0},
	{.path = NULL, .flags = MM_SPAWN_KEEP_FDS},
	{.path = NULL, .flags = MM_SPAWN_DAEMONIZE},
	{.path = NULL, .flags = MM_SPAWN_KEEP_FDS | MM_SPAWN_DAEMONIZE},
	{.path = CHILDPROC_BINPATH, .flags = (MM_SPAWN_KEEP_FDS << 2)},
#if defined(_WIN32)
	{.path = CHILDPROC_BINPATH, .flags = MM_SPAWN_KEEP_FDS},
	{.path = CHILDPROC_BINPATH, .flags = MM_SPAWN_KEEP_FDS | MM_SPAWN_DAEMONIZE},
#endif
};

START_TEST(spawn_invalid_args)
{
	int rv;
	mm_pid_t pid = UNSET_PID_VALUE;
	const char* path = inval_cases[_i].path;
	int flags = inval_cases[_i].flags;

	/* cannot run "/" */
	rv = mm_spawn(&pid, path, 0, NULL, flags, NULL, NULL);
	ck_assert(rv == -1);
	ck_assert(pid == UNSET_PID_VALUE);  // no process should be able to launch

#if defined(_WIN32)
	if (path && flags & MM_SPAWN_KEEP_FDS) {
		ck_assert_int_eq(mm_get_lasterror_number(), ENOTSUP);
		return;
	}
#endif
	ck_assert_int_eq(mm_get_lasterror_number(), EINVAL);
}
END_TEST

/* this is to make sure that mm_wait_process() fails if the process
 * does not exists.
 * The system SHOULD not have given the same pid again just after we finish
 * waiting for it */
START_TEST(wait_twice)
{
	const char* file = CHILDPROC_BINPATH;
	struct process_test_data* data = create_process_test_data(file);
	mm_pid_t pid;

	ck_assert(spawn_child(0, data) == 0);
	ck_assert(data->pid != UNSET_PID_VALUE);

	pid = data->pid;
	data->pid = UNSET_PID_VALUE;

	/* wait once */
	ck_assert(mm_wait_process(pid, NULL) == 0);
	ck_assert(check_expected_fd_content(data) == 0);

	/* wait a sacond time with the same pid */
	ck_assert(mm_wait_process(pid, NULL) != 0);
}
END_TEST


/**************************************************************************
 *                                                                        *
 *                       process test suite setup                         *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
TCase* create_process_tcase(void)
{
	TCase * tc;

	tc = tcase_create("process");
	tcase_add_unchecked_fixture(tc, case_setup, case_teardown);
	tcase_add_checked_fixture(tc, NULL, test_teardown);

	tcase_add_loop_test(tc, spawn_simple, 0, MM_NELEM(binpath_cases));
	tcase_add_loop_test(tc, execv_simple, 0, MM_NELEM(binpath_cases));
	tcase_add_loop_test(tc, spawn_daemon, 0, MM_NELEM(binpath_cases));
	tcase_add_loop_test(tc, spawn_error, 0, MM_NELEM(error_cases));
	tcase_add_loop_test(tc, spawn_daemon_error, 0, MM_NELEM(error_cases));
	tcase_add_loop_test(tc, spawn_invalid_args, 0, MM_NELEM(inval_cases));
	tcase_add_test(tc, wait_twice);

#ifndef _WIN32
	tcase_add_loop_test(tc, spawn_error_limits, 0, NUM_ERRLIMITS_CASES);
#endif

	return tc;
}
