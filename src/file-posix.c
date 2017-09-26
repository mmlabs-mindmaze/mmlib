/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmsysio.h"
#include "mmerrno.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>


API_EXPORTED
int mm_open(const char* path, int oflag, int mode)
{
	int fd;

	// Make file opened by mm_open automatically non inheritable
	oflag |= O_CLOEXEC;

	fd = open(path, oflag, mode);
	if (fd < 0)
		return mm_raise_from_errno("open(%s, %08x) failed", path, oflag);

	return fd;
}


API_EXPORTED
int mm_close(int fd)
{
	if (fd == -1)
		return 0;

	if (close(fd) < 0)
		return mm_raise_from_errno("close(%i) failed", fd);

	return 0;
}


API_EXPORTED
ssize_t mm_read(int fd, void* buf, size_t nbyte)
{
	ssize_t rsz;

	rsz = read(fd, buf, nbyte);
	if (rsz < 0)
		return mm_raise_from_errno("read(%i, ...) failed", fd);

	return rsz;
}


API_EXPORTED
ssize_t mm_write(int fd, const void* buf, size_t nbyte)
{
	ssize_t rsz;

	rsz = write(fd, buf, nbyte);
	if (rsz < 0)
		return mm_raise_from_errno("write(%i, ...) failed", fd);

	return rsz;
}


API_EXPORTED
int mm_dup(int fd)
{
	int newfd;

	newfd = dup(fd);
	if (newfd < 0)
		return mm_raise_from_errno("dup(%i) failed", fd);

	return newfd;
}


API_EXPORTED
int mm_dup2(int fd, int newfd)
{
	if (dup2(fd, newfd) < 0)
		return mm_raise_from_errno("dup2(%i, %i) failed", fd, newfd);

	return newfd;
}


API_EXPORTED
int mm_pipe(int pipefd[2])
{
	if (pipe(pipefd) < 0)
		return mm_raise_from_errno("pipe() failed");

	return 0;
}


API_EXPORTED
int mm_unlink(const char* path)
{
	if (unlink(path) < 0)
		return mm_raise_from_errno("unlink(%p) failed", path);

	return 0;
}


API_EXPORTED
int mm_link(const char* oldpath, const char* newpath)
{
	if (link(oldpath, newpath) < 0)
		return mm_raise_from_errno("link(%p, %p) failed", oldpath, newpath);

	return 0;
}


API_EXPORTED
int mm_symlink(const char* oldpath, const char* newpath)
{
	if (symlink(oldpath, newpath) < 0)
		return mm_raise_from_errno("symlink(%p, %p) failed", oldpath, newpath);

	return 0;
}


API_EXPORTED
int mm_check_access(const char* path, int amode)
{
	int prev_error = errno;
	int ret, errnum;

	ret = access(path, amode);
	if (ret) {
		// If errno is EACCESS or ENOENT, this is not a real error,
		// but just that either the file do not exist, either the
		// mode requested could not be obtained. Hence, revert back
		// the previous errno
		errnum = errno;
		if (errnum == EACCES || errnum == ENOENT) {
			errno = prev_error;
			return errnum;
		}

		// If not EACCESS, the error is a real one and worth being
		// reported
		return mm_raise_from_errno("access(\"%s\", %02x) failed",
		                           path, amode);
	}

	// File exist and requested mode could be granted
	return 0;
}
