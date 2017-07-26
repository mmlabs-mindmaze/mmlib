/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE		// for pipe2()

#include "mmsysio.h"
#include "mmerrno.h"
#include "mmlog.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdnoreturn.h>


/**
 * struct startproc_opts - holder for argument passed to mm_spawn()
 * @path:       path to the executable file
 * @num_map     number of element in the @fd_map array
 * @fd_map:     array of file descriptor remapping to pass into the child
 * @flags:      spawn flags
 * @argv:       null-terminated array of string containing the command
 *              arguments (starting with command).
 * @envp:       null-terminated array of strings specifying the environment
 *              of the executed program. can be NULL
 *
 * This structure is meant to hold the argument passed in the call to
 * mm_spawn() and forward them to the deeper layer of the mm_spawn()
 * implementation.
 */
struct startproc_opts {
	const char* path;
	int flags;
	int num_map;
	const struct mm_remap_fd* fd_map;
        char* const* argv;
	char* const* envp;
};

/**
 * set_fd_cloexec() - Set/unset FD_CLOEXEC flag to file descriptor
 * @fd:         file descriptor to modify
 * @cloexec:    flag indicating whether FD_CLOEXEC must set or unset
 *
 * Return: 0 in case of success, -1 otherwise with errno set.
 */
static
int set_fd_cloexec(int fd, int cloexec)
{
	int curr_flags;

	curr_flags = fcntl(fd, F_GETFD);
	if (curr_flags == -1)
		return -1;

	if (cloexec)
		curr_flags |= FD_CLOEXEC;
	else
		curr_flags &= ~FD_CLOEXEC;

	return fcntl(fd, F_SETFD, curr_flags);
}



/**
 * set_cloexec_all_fds() - Add FD_CLOEXEC to all FDs excepting a list
 * @min_fd:    minimal file descriptor to close (all lower fds will be kept untouched)
 *
 * This function will set the FD_CLOEXEC flag to all file descriptor in the
 * caller process excepting below @min_fd. If a file descriptor has
 * the FD_CLOEXEC flag, it will be closed across a call to exec*().
 *
 * This is implemented by scanning the /proc/self/fd or /dev/fd directories
 * which contains information about all open file descriptor in the current
 * process.
 *
 * NOTE: There is no portable way to know all open file descriptors in a
 * process. This function will function only on Linux, Solaris, AIX, Cygwin,
 * NetBSD (/proc/self/fd) and FreeBSD, Darwin, OS X (/dev/fd)
 */
static
void set_cloexec_all_fds(int min_fd)
{
	int fd;
	DIR* dir;
	struct dirent* entry;

	if ( !(dir = opendir("/proc/self/fd"))
	  && !(dir = opendir("/dev/fd")) ) {
		mmlog_warn("Cannot find list of open file descriptors."
		           "Leaving maybe some fd opened in the child...");
		return;
	}

	// Loop over entries in the directory
	while ((entry = readdir(dir))) {

		// Convert name into file descriptor value
		if (!sscanf(entry->d_name, "%i", &fd))
			continue;

		if (fd >= min_fd)
			set_fd_cloexec(fd, 1);
	}

	closedir(dir);
}


/**
 * remap_file_descriptors() - setup the open files descriptor in the child
 * @num_map:    number of element in @fd_map
 * @fd_map:     array of file descriptor remapping to pass into the child
 *
 * Meant to be called in the child process, between the fork() and exec(),
 * this function will move (actually duplicate) the inherited file
 * descriptor to the specified child fd, and mark as cloexec those that are
 * specifically noted for closing, ie, those associated with
 * &mm_remap_fd.parent_fd == -1.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 */
static
int remap_file_descriptors(int num_map, const struct mm_remap_fd* fd_map)
{
	int i, child_fd, parent_fd;

	// Tag all fd beyond STDERR for closing at exec
	set_cloexec_all_fds(3);

	for (i = 0; i < num_map; i++) {
		child_fd = fd_map[i].child_fd;
		parent_fd = fd_map[i].parent_fd;

		if (parent_fd == -1)
			set_fd_cloexec(child_fd, 1);

		// If parend and child have same fd, we need to unset
		// CLOEXEC directly because dup2() will leave fd untouch in
		// such a case.
		if (parent_fd == child_fd) {
			set_fd_cloexec(child_fd, 0);
			continue;
		}

		// Duplicate parent_fd to child_fd. We use dup2, so if parent_fd has CLOEXEC
		// flag, it will be removed in the duplicated fd
		if (dup2(parent_fd, child_fd) < 0) {
			mm_raise_from_errno("dup2(%i, %i) failed", parent_fd, child_fd);
			return -1;
		}
	}

	return 0;
}


/**
 * report_to_parent_and_exit() - report error from child and terminate
 * @report_pipe:        pipe to use to write error info
 *
 * This function is used when an error is detected in the child process
 * preventing the child to execute the specified executable with the
 * specified argument. @report_pipe is meant to be the write end of a pipe
 * connected to the parent that will be watched there after the fork.
 */
static
noreturn void report_to_parent_and_exit(int report_pipe)
{
	struct mm_error_state errstate;
	char* cbuf;
	size_t len;
	ssize_t rsz;

	mm_save_errorstate(&errstate);

	// Do full write to report_pipe
	cbuf = (char*)&errstate;
	len = sizeof(errstate);
	while (len > 0) {
		rsz = write(report_pipe, cbuf, len);
		if (rsz < 0) {
			// Retry if call has simply been interrupted by
			// signal delivery
			if (errno == EINTR)
				continue;

			// If anything else happens, there is nothing we can
			// do. However, given we write on a clean pipe, the
			// only possible error should be EPIPE due to parent
			// dead. In such a case, we don't care (reporting a
			// death of a child to a dead parent?!? o_O)
			break;
		}

		len -= rsz;
		cbuf += rsz;
	}

	exit(EXIT_FAILURE);
}


/**
 * load_new_proc_img() - configure current process for new binary
 * @opts:	data holding argument passed to mm_spawn()
 * @report_pipe: pipe end to use if an error is detected
 *
 * Meant to be called in the new forked child process, this set it up with
 * the right file descriptors opened and replace the current executable
 * image with the one specified in @opts. If any of the step fails, the
 * error will be reported through @report_pipe.
 */
static
noreturn void load_new_proc_img(const struct startproc_opts* opts,
                                int report_pipe)
{
	// Perform remapping if MM_SPAWN_KEEP_FDS is not set in flags
	if ( !(opts->flags & MM_SPAWN_KEEP_FDS)
	  && remap_file_descriptors(opts->num_map, opts->fd_map))
		goto failure;

	execve(opts->path, opts->argv, opts->envp);

	// If we read here, execve has failed
	mm_raise_from_errno("Cannot run \"%s\"", opts->path);

failure:
	report_to_parent_and_exit(report_pipe);
}


/**
 * wait_for_load_process_result() - wait child failure or success
 * @watch_fd:	pipe end to watch
 *
 * This function is meant to be called in the parent process. @watch_fd is
 * supposed to be the read end of a pipe whose the write end is accessible
 * to the child, ie, after fork(). If any error occurs before or during
 * child process call exec(), it will be reported by the through this pipe
 * with report_to_parent_and_exit(). Both end of the pipe are supposed to
 * have the CLOEXEC flag set. If the child end of the pipe is closed before
 * any data can be read from @watch_fd, this means that the write end has
 * been closed by the successfull call to exec().
 *
 * Return: 0 if the exec call is successfull in child process, -1 otherwise
 * and the error state will be the one reported by child.
 */
static
int wait_for_load_process_result(int watch_fd)
{
	ssize_t rsz;
	int ret;
	struct mm_error_state errstate;

	ret = -1;

	rsz = read(watch_fd, &errstate, sizeof(errstate));
	if (rsz < 0) {
		mm_raise_from_errno("Cannot read from result pipe");
		goto exit;
	}

	// If read return and nothing is read, it means the write end of the
	// pipe is closed before any data has been written. This means that
	// exec was successful
	if (rsz == 0) {
		ret = 0;
		goto exit;
	}

	// If something has been read, the exec has failed. The error state
	// must have been passed in the pipe
	if (rsz < (ssize_t)sizeof(errstate)) {
		mm_raise_error(EIO, "Incomplete error state from child");
		goto exit;
	}

	mm_set_errorstate(&errstate);

exit:
	close(watch_fd);
	return ret;
}


/**
 * spawn_child() - spawn a direct child of the calling process
 * @pid:        pointer to a variable that will receive process ID of child
 * @opts:	data holding argument passed to mm_spawn()
 *
 * Return: 0 in case success, -1 otherwise with error state set accordingly
 */
static
int spawn_child(mm_pid_t* child_pid, const struct startproc_opts* opts)
{
	pid_t pid;
	int pipefd[2], watch_fd, report_fd;

	// Create pipe connecting child to parent
	if (pipe2(pipefd, O_CLOEXEC))
		return -1;

	watch_fd = pipefd[0];
	report_fd = pipefd[1];

	pid = fork();
	if (pid == -1) {
		mm_raise_from_errno("unable to fork");
		close(watch_fd);
		close(report_fd);
		return -1;
	}

	// if pid is 0, this means that we are in the child process of the fork
	if (pid == 0) {
		close(watch_fd);
		load_new_proc_img(opts, report_fd);
	}

	// we are in the parent part of the fork
	close(report_fd);

	if (child_pid)
		*child_pid = pid;

	// watch_fd is going to be closed here
	return wait_for_load_process_result(watch_fd);
}


/**
 * spawn_daemon() - spawn a background process
 * @opts:	data holding argument passed to mm_spawn()
 *
 * Return: 0 in case success, -1 otherwise with error state set accordingly
 */
static
int spawn_daemon(const struct startproc_opts* opts)
{
	pid_t pid;
	int status;
	int pipefd[2], watch_fd, report_fd;

	// Create pipe connecting child to parent
	if (pipe2(pipefd, O_CLOEXEC))
		return -1;

	watch_fd = pipefd[0];
	report_fd = pipefd[1];

	pid = fork();
	if (pid == -1) {
		mm_raise_from_errno("unable to do first fork");
		close(watch_fd);
		close(report_fd);
		return -1;
	}

	// if pid > 0, this means that we are in the parent process of the first fork
	if (pid > 0) {
		close(report_fd);
		waitpid(pid, &status, 0);
		// watch_fd is going to be closed here
		return wait_for_load_process_result(watch_fd);
	}

	// We are in the child process of the first fork so setup for daemon
	close(watch_fd);
	if (chdir("/")) {
		mm_raise_from_errno("Unable to chdir(\"/\")");
		report_to_parent_and_exit(report_fd);
	}
	umask(0);
	setsid();

	// Do second fork
	pid = fork();
	if (pid == -1) {
		mm_raise_from_errno("unable to do second fork");
		report_to_parent_and_exit(report_fd);
	}

	// Parent side of the second fork)
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	load_new_proc_img(opts, report_fd);
}


API_EXPORTED
int mm_spawn(mm_pid_t* child_pid, const char* path,
             int num_map, const struct mm_remap_fd* fd_map,
             int flags, char* const* argv, char* const* envp)
{
	char* default_argv[] = {(char*)path, NULL};
	int ret;
	struct startproc_opts proc_opts = {
		.path = path,
		.flags = flags,
		.num_map = num_map,
		.fd_map = fd_map,
		.argv = argv,
		.envp = envp,
	};

	if (!argv)
		proc_opts.argv = default_argv;

	if (!envp)
		proc_opts.envp = environ;


	if (flags & MM_SPAWN_DAEMONIZE) {
		ret = spawn_daemon(&proc_opts);
	} else {
		ret = spawn_child(child_pid, &proc_opts);
	}

	return ret;
}


API_EXPORTED
int mm_wait_process(mm_pid_t pid, int* status)
{
	int posix_status, mm_status;

	if (waitpid(pid, &posix_status, 0) < 0) {
		mm_raise_from_errno("waitpid(%lli) failed", pid);
		return -1;
	}

	if (WIFEXITED(posix_status)) {
		mm_status = MM_WSTATUS_EXITED | WEXITSTATUS(posix_status);
	} else if (WIFSIGNALED(posix_status)) {
		mm_status = MM_WSTATUS_SIGNALED | WTERMSIG(posix_status);
	} else {
		mm_crash("waitpid() must return exited or signaled status");
	}

	if (status)
		*status = mm_status;

	return 0;
}