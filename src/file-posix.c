/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

// Needed for copy_file_range if available
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mmlib.h"
#include "mmerrno.h"
#include "mmpredefs.h"
#include "mmsysio.h"
#include "file-internal.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>


/**
 * mm_open() - Open file
 * @path:       path to file to open
 * @oflag:      control flags how to open the file
 * @mode:       access permission bits is file is created
 *
 * This function creates an open file description that refers to a file and
 * a file descriptor that refers to that open file description. The file
 * descriptor is used by other I/O functions to refer to that file. The
 * @path argument points to a pathname naming the file.
 *
 * The file status flags and file access modes of the open file description
 * are set according to the value of @oflag, which is constructed by a
 * bitwise-inclusive OR of flags from the following list. It must specify
 * exactly one of the first 3 values.
 *
 * %O_RDONLY
 *   Open for reading only.
 * %O_WRONLY
 *   Open for writing only.
 * %O_RDWR
 *   Open for reading and writing. The result is undefined if this flag is
 *   applied to a FIFO.
 *
 * Any combination of the following may be used
 *
 * %O_APPEND
 *   If set, the file offset shall be set to the end of the file prior to
 *   each write.
 * %O_CREAT
 *   If the file exists, this flag has no effect except as noted under
 *   %O_EXCL below. Otherwise, the file is created as a regular file; the
 *   user ID of the file shall be set to the effective user ID of the
 *   process; the group ID of the file shall be set to the group ID of the
 *   file's parent directory or to the effective group ID of the process;
 *   and the access permission bits of the file mode are set by the @mode
 *   argument modified by a bitwise AND with the umask of the process. This
 *   @mode argument does not affect whether the file is open for reading,
 *   writing, or for both.
 * %O_TRUNC
 *   If the file exists and is a regular file, and the file is successfully
 *   opened %O_RDWR or %O_WRONLY, its length is truncated to 0, and the
 *   mode and owner are unchanged. The result of using O_TRUNC without
 *   either %O_RDWR or %O_WRONLY is undefined.
 * %O_EXCL
 *   If %O_CREAT and %O_EXCL are set, mm_open() fails if the file exists.
 *   The check for the existence of the file and the creation of the file if
 *   it does not exist is atomic with respect to other threads executing
 *   mm_open() naming the same filename in the same directory with %O_EXCL
 *   and %O_CREAT set.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
API_EXPORTED
int mm_open(const char* path, int oflag, int mode)
{
	int fd;

	// Make file opened by mm_open automatically non inheritable
	oflag |= O_CLOEXEC;

	fd = open(path, oflag, mode);
	if (fd < 0)
		return mm_raise_from_errno("open(%s, %08x) failed", path,
		                           oflag);

	return fd;
}


/**
 * mm_rename() - rename a file
 * @oldpath: old pathname of the file
 * @newpath: new pathname of the file
 *
 * The mm_rename() function changes the name of a file. @oldpath is the path of
 * the file to be renamed, and @newpath is the new pathname of the file.
 *
 * If @newpath corresponds to the path of an existing file/directory, then it is
 * removed and @oldpath is renamed to @newpath. Therefore, write access
 * permission is required for both the directory containing @oldpath and the
 * directory containing @newpath. Note that, in case @newpath corresponds to the
 * path of an existing directory, this directory is required to be empty.
 *
 * If either @oldpath or @newpath is a path of a symbolic link, mm_rename()
 * operates on the symbolic link itself.
 *
 * If @oldpath and @newpath are identical paths mm_rename() returns successfully
 * and performs no other action.
 *
 * mm_rename() will non-trivially fail if:
 *   - Either @oldpath or @newpath refers to a path whose final component is
 *     either dot or dot-dot.
 *   - @newpath is a path toward a non empty directory.
 *   - @newpath contains a subpath (different from the path) toward a non
 *     existing directory.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_rename(const char * oldpath, const char * newpath)
{
	if (rename(oldpath, newpath) != 0)
		return mm_raise_from_errno("rename %s into %s failed", oldpath,
		                           newpath);

	return 0;
}


/**
 * mm_close() - Close a file descriptor
 * @fd:         file descriptor to close
 *
 * This function deallocates the file descriptor indicated by @fd, ie it
 * makes the file descriptor available for return by subsequent calls to
 * mm_open() or other system functions that allocate file descriptors.
 *
 * If a memory mapped file or a shared memory object remains referenced at
 * the last close (that is, a process has it mapped), then the entire
 * contents of the memory object persists until the memory object becomes
 * unreferenced. If this is the last close of a memory mapped file or a
 * shared memory object and the close results in the memory object becoming
 * unreferenced, and the memory object has been unlinked, then the memory
 * object will be removed.
 *
 * If @fd refers to a socket, mm_close() causes the socket to be destroyed.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_close(int fd)
{
	if (fd == -1)
		return 0;

	if (close(fd) < 0)
		return mm_raise_from_errno("close(%i) failed", fd);

	return 0;
}


/**
 * mm_read() - Reads data from a file descriptor
 * @fd:         file descriptor to read from
 * @buf:        storage location for data
 * @nbyte:      maximum size to read
 *
 * mm_read() attempts to read @nbyte bytes from the file associated with the
 * open file descriptor, @fd, into the buffer pointed to by @buf.
 *
 * On files that support seeking (for example, a regular file), the
 * mm_read() starts at a position in the file given by the file offset
 * associated with @fd. The file offset will incremented by the number of
 * bytes actually read.  * No data transfer will occur past the current
 * end-of-file. If the starting position is at or after the end-of-file, 0
 * is returned.
 *
 * If @fd refers to a socket, mm_read() shall be equivalent to mm_recv()
 * with no flags set.
 *
 * Return: Upon successful completion, a non-negative integer is returned
 * indicating the number of bytes actually read. Otherwise, -1 is returned
 * and error state is set accordingly
 */
API_EXPORTED
ssize_t mm_read(int fd, void* buf, size_t nbyte)
{
	ssize_t rsz;

	rsz = read(fd, buf, nbyte);
	if (rsz < 0)
		return mm_raise_from_errno("read(%i, ...) failed", fd);

	return rsz;
}


/**
 * mm_write() - Write data to a file descriptor
 * @fd:         file descriptor to write to
 * @buf:        storage location for data
 * @nbyte:      amount of data to write
 *
 * The mm_write() function attempts to write @nbyte bytes from the buffer
 * pointed to by @buf to the file associated with the open file descriptor,
 * @fd.
 *
 * On a regular file or other file capable of seeking, the actual writing of
 * data shall proceed from the position in the file indicated by the file
 * offset associated with @fd. Before successful return from mm_write(), the
 * file offset is incremented by the number of bytes actually written.
 *
 * If the %O_APPEND flag of the file status flags is set, the file offset is set
 * to the end of the file prior to each write and no intervening file
 * modification operation will occur between changing the file offset and the
 * write operation.
 *
 * Write requests to a pipe or FIFO shall be handled in the same way as a
 * regular file with the following exceptions
 *
 * - there is no file offset associated with a pipe, hence each write request
 *   shall append to the end of the pipe.
 * - write requests of pipe buffer size bytes or less will not be interleaved
 *   with data from other processes doing writes on the same pipe.
 * - a write request may cause the thread to block, but on normal completion it
 *   shall return @nbyte.
 *
 * Return: Upon successful completion, a non-negative integer is returned
 * indicating the number of bytes actually written. Otherwise, -1 is returned
 * and error state is set accordingly
 */
API_EXPORTED
ssize_t mm_write(int fd, const void* buf, size_t nbyte)
{
	ssize_t rsz;

	rsz = write(fd, buf, nbyte);
	if (rsz < 0)
		return mm_raise_from_errno("write(%i, ...) failed", fd);

	return rsz;
}


/**
 * mm_dup() - duplicate an open file descriptor
 * @fd:         file descriptor to duplicate
 *
 * This function creates a new file descriptor referencing the same file
 * description as the one referenced by @fd.
 *
 * Note that the two file descriptors point to the same file. They will share
 * the same file pointer.
 *
 * Return: a non-negative integer representing the new file descriptor in case
 * of success. The return file descriptor value is then guaranteed to be the
 * lowest available at the time of the call. In case of error, -1 is returned
 * with error state set accordingly.
 */
API_EXPORTED
int mm_dup(int fd)
{
	int newfd;

	newfd = dup(fd);
	if (newfd < 0)
		return mm_raise_from_errno("dup(%i) failed", fd);

	return newfd;
}


/**
 * mm_dup2() - duplicate an open file descriptor to a determined file descriptor
 * @fd:         file descriptor to duplicate
 * @newfd:      file descriptor number that will become the duplicate
 *
 * This function duplicates an open file descriptor @fd and assign it to the
 * file descriptor @newfd. In other word, this function is similar to mm_dup()
 * but in case of success, the returned value is ensured to be @newfd.
 *
 * Return: a non-negative integer representing the new file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
API_EXPORTED
int mm_dup2(int fd, int newfd)
{
	if (dup2(fd, newfd) < 0)
		return mm_raise_from_errno("dup2(%i, %i) failed", fd, newfd);

	return newfd;
}


/**
 * mm_pipe() - creates an interprocess channel
 * @pipefd:     array of two in receiving the read and write endpoints
 *
 * The mm_pipe() function creates a pipe and place two file descriptors, one
 * each into the arguments @pipefd[0] and @pipefd[1], that refer to the open
 * file descriptions for the read and write ends of the pipe. Their integer
 * values will be the two lowest available at the time of the mm_pipe() call.
 *
 * Data can be written to the file descriptor @pipefd[1] and read from the file
 * descriptor @pipefd[0]. A read on the file descriptor @pipefd[0] shall access
 * data written to the file descriptor @pipefd[1] on a first-in-first-out
 * basis.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_pipe(int pipefd[2])
{
	if (pipe(pipefd) < 0)
		return mm_raise_from_errno("pipe() failed");

	return 0;
}


/**
 * mm_unlink() -  remove a directory entry
 * @path:       location to remove from file system
 *
 * The mm_unlink() function removes a link to a file. If @path names a symbolic
 * link, it removes the symbolic link named by @path and does not affect any
 * file or directory named by the contents of the symbolic link. Otherwise,
 * mm_unlink() remove the link named by the pathname pointed to by @path and
 * decrements the link count of the file referenced by the link.
 *
 * When the file's link count becomes 0 and no process has the file open, the
 * space occupied by the file will be freed and the file will no longer be
 * accessible. If one or more processes have the file open when the last link
 * is removed, the link will be removed before mm_unlink() returns, but the
 * removal of the file contents is postponed until all references to the
 * file are closed (ie when all file descriptors referencing it are closed).
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 *
 * NOTE: On Windows platform, it is usually believed that an opened file is not
 * permitted to be deleted. This is not true. This is only due to the fact that
 * many libraries/application open file missing the right share mode
 * (FILE_SHARE_DELETE). If you access the file through mmlib APIs, you will be
 * able to unlink your file before it is closed (even if memory mapped...).
 */
API_EXPORTED
int mm_unlink(const char* path)
{
	if (unlink(path) < 0)
		return mm_raise_from_errno("unlink(%s) failed", path);

	return 0;
}


/**
 * mm_link() - create a hard link to a file
 * @oldpath:    existing path for the file to link
 * @newpath:    new path of the file
 *
 * The mm_link() function creates a new link (directory entry) for the existing
 * file, @oldpath.
 *
 * The @oldpath argument points to a pathname naming an existing file. The
 * @newpath argument points to a pathname naming the new directory entry to be
 * created. The mm_link() function create atomically a new link for the
 * existing file and the link count of the file shall be incremented by one.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_link(const char* oldpath, const char* newpath)
{
	if (link(oldpath, newpath) < 0)
		return mm_raise_from_errno("link(%s, %s) failed", oldpath,
		                           newpath);

	return 0;
}


/**
 * mm_symlink() - create a symbolic link to a file
 * @oldpath:    existing path for the file to link
 * @newpath:    new path of the file
 *
 * The mm_link() function creates a new symbolinc link for the existing file,
 * @oldpath. The @oldpath argument do not need to point to a pathname naming an
 * existing file.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_symlink(const char* oldpath, const char* newpath)
{
	if (symlink(oldpath, newpath) < 0)
		return mm_raise_from_errno("symlink(%s, %s) failed", oldpath,
		                           newpath);

	return 0;
}


/**
 * mm_check_access() - verify access to a file
 * @path:       path of file
 * @amode:      access mode to check (OR-combination of _OK flags)
 *
 * This function verify the calling process can access the file located at
 * @path according to the bits pattern specified in @amode which can be a
 * OR-combination of the %R_OK, %W_OK, %X_OK to indicate respectively the read,
 * write or execution access to a file. If @amode is F_OK, only the existence
 * of the file is checked.
 *
 * Return:
 * - 0 if the file can be accessed
 * - %ENOENT if a component of @path does not name an existing file
 * - %EACCESS if the file cannot be access with the mode specified in @amode
 * - -1 in case of error (error state is then set accordingly)
 */
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


/**
 * mm_isatty() - test whether a file descriptor refers to a terminal
 * @fd:         File descriptor to test
 *
 * Return: 1 if @fd refers to a terminal, 0 if not. If @fd is not a valid
 * file descriptor, -1 is returned and error state is set accordingly.
 */
API_EXPORTED
int mm_isatty(int fd)
{
	int rv;
	int prev_err = errno;

	rv = isatty(fd);

	if (rv == 0) {
		if (errno != EINVAL && errno != ENOTTY)
			return mm_raise_from_errno("isatty(%i) failed", fd);

		// If errno is EINVAL or ENOTTY, fd is actually not a tty,
		// but this is not an error, thus we restore errno as it was
		errno = prev_err;
	}

	return rv;
}


struct mm_dirstream {
	DIR * dir;
	struct mm_dirent * dirent;
};


/**
 * mm_chdir() - change working directory
 * @path:       path to new working directory
 *
 * The mm_chdir() function causes the directory named by the pathname pointed
 * to by the @path argument to become the current working directory; that is,
 * the starting point for path searches for pathnames that are not absolute.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_chdir(const char* path)
{
	if (chdir(path) != 0)
		return mm_raise_from_errno("chdir(%s) failed", path);

	return 0;
}


/**
 * mm_getcwd() - get the pathname of the current working directory
 * @buf:        pointer to buffer where to write pathname or NULL
 * @size:       size of buffer pointed to by @buf if not NULL
 *
 * The mm_getcwd() function places an absolute pathname of the current
 * working directory in the array pointed to by @buf, and return @buf. The
 * pathname copied to the array contains no components that are symbolic
 * links. The @size argument is size of the buffer pointed to by @buf.
 *
 * If @buf is NULL, space is allocated as necessary to store the pathname.
 * In such a case, @size argument is ignored. This space may later be freed
 * with free().
 *
 * Return: a pointer to a string containing the pathname of the current
 * working directory in case of success. Otherwise NULL is returned and
 * error state is set accordingly.
 */
API_EXPORTED
char* mm_getcwd(char* buf, size_t size)
{
	char* path;

	// Needed for glibc's getcwd() to allocate the needed size
	if (!buf)
		size = 0;

	path = getcwd(buf, size);
	if (!path) {
		if (errno == ERANGE)
			mm_raise_error(ERANGE, "buffer too short for "
			               "holding current directory path");
		else
			mm_raise_from_errno("can't get current directory");
	}

	return path;
}


/**
 * mm_rmdir() - remove a directory
 * @path:       path to the directory to remove
 *
 * The mm_rmdir() function removes the directory named by the pathname pointed
 * to by the @path argument. It only works on empty directories.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_rmdir(const char* path)
{
	if (rmdir(path) != 0)
		return mm_raise_from_errno("rmdir(%s) failed", path);

	return 0;
}


static
void conv_native_to_mm_stat(struct mm_stat* buf,
                            const struct stat* native_stat)
{
	*buf = (struct mm_stat) {
		.dev = native_stat->st_dev,
		.ino = native_stat->st_ino,
		.uid = native_stat->st_uid,
		.gid = native_stat->st_gid,
		.mode = native_stat->st_mode,
		.nlink = native_stat->st_nlink,
		.size = native_stat->st_size,
		.blksize = native_stat->st_blksize,
		.nblocks = native_stat->st_blocks,
		.ctime = native_stat->st_ctime,
		.mtime = native_stat->st_mtime,
	};

	// Accommodate for end of string to be consistent with mm_readlink()
	if (S_ISLNK(buf->mode))
		buf->size += 1;
}


/**
 * mm_fstat() - get file status from file descriptor
 * @fd:         file descriptor
 * @buf:        pointer to mm_stat structure to fill
 *
 * This function obtains information about an open file associated with the
 * file descriptor @fd, and writes it to the area pointed to by @buf.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_fstat(int fd, struct mm_stat* buf)
{
	struct stat native_stat;

	if (fstat(fd, &native_stat) < 0)
		return mm_raise_from_errno("fstat(%i) failed", fd);

	conv_native_to_mm_stat(buf, &native_stat);
	return 0;
}


/**
 * mm_stat() - get file status from file path
 * @path:       path of file
 * @buf:        pointer to mm_stat structure to fill
 * @flags:      0 or MM_NOFOLLOW
 *
 * This function obtains information about an file located by @path, and writes
 * it to the area pointed to by @buf. If @path refers to a symbolic link, the
 * information depents on the value of @flags. If @flags is 0, the information
 * returned will be the one of the target of symbol link. Otherwise, if
 * MM_NOFOLLOW is set in @flags, the information will be the one of the
 * symbolic link itself.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_stat(const char* path, struct mm_stat* buf, int flags)
{
	struct stat native_stat;

	if (flags & MM_NOFOLLOW) {
		if (lstat(path, &native_stat) < 0)
			return mm_raise_from_errno("lstat(%s) failed", path);
	} else {
		if (stat(path, &native_stat) < 0)
			return mm_raise_from_errno("stat(%s) failed", path);
	}

	conv_native_to_mm_stat(buf, &native_stat);
	return 0;
}


/**
 * mm_readlink() - read value of a symbolic link
 * @path:       pathname of symbolic link
 * @buf:        buffer receiving the value
 * @bufsize:    length of @buf
 *
 * mm_readlink() places the contents of the symbolic link @path in the buffer
 * @buf, which has size @bufsize. It does append a null byte to @buf. If @buf
 * is too small to hold the contents, error will be returned. The required size
 * for the buffer can be obtained from &struct stat.filesize value returned by
 * a call to mm_stat() on the link.
 *
 * Return: 0 in case of success, -1 otherwise with error state set.
 */
API_EXPORTED
int mm_readlink(const char* path, char* buf, size_t bufsize)
{
	ssize_t rsz;

	rsz = readlink(path, buf, bufsize);
	if (rsz < 0)
		return mm_raise_from_errno("readlink(%s) failed", path);

	if (rsz == (ssize_t)bufsize)
		return mm_raise_error(EOVERFLOW, "target too large");

	buf[rsz] = '\0';
	return 0;
}


static
int get_file_type(int dirfd, const char* path)
{
	int rv;
	struct stat stat;

	rv = fstatat(dirfd, path, &stat, AT_SYMLINK_NOFOLLOW);
	if (rv != 0)
		return rv;

	switch (stat.st_mode & S_IFMT) {
	case S_IFIFO: return MM_DT_FIFO;
	case S_IFCHR:  return MM_DT_CHR;
	case S_IFBLK:  return MM_DT_BLK;
	case S_IFDIR:  return MM_DT_DIR;
	case S_IFREG:  return MM_DT_REG;
	case S_IFLNK:  return MM_DT_LNK;
	case S_IFSOCK: return MM_DT_SOCK;
	default: return MM_DT_UNKNOWN;
	}
}

#define RECURSION_MAX 100

/**
 * mm_remove_rec() - internal helper to recursively clean given folder
 * @dirfd:           directory file descriptor to clean
 * @flags:           option flag to return on error
 * @rec_lvl:         maximum recursion level
 *
 * Many error return values are *explicitly* skipped.
 * Since this is a recursive removal, we should not stop when we encounter
 * a forbidden file or folder. This except if the @flag contains MM_FAILONERROR.
 *
 * Return: 0 on success, -1 on error
 */
static
int mm_remove_rec(int dirfd, int flags, int rec_lvl)
{
	int rv, exit_value;
	int newdirfd;
	int type;
	DIR * dir = NULL;
	const struct dirent * dp;
	int unlink_flag;

	exit_value = -1;
	if (UNLIKELY(rec_lvl < 0))
		return mm_raise_error(EOVERFLOW, "Too many levels of recurion");

	if ((dir = fdopendir(dirfd)) == NULL) {
		close(dirfd);
		if (flags & MM_FAILONERROR)
			return exit_value;
		else
			return 0;
	}

	while ((dp = readdir(dir)) != NULL) {
		type = get_file_type(dirfd, dp->d_name);
		if (type > 0 && (flags & type) == 0)
			continue;  // only consider filtered files

		/* skip "." and ".." directories */
		if (is_wildcard_directory(dp->d_name))
			continue;

		if (type == MM_DT_DIR) {
			/* remove the inside of the folder */
			newdirfd = openat(dirfd, dp->d_name, O_CLOEXEC, 0);
			if (newdirfd == -1) {
				if (flags & MM_FAILONERROR)
					goto exit;
				else
					continue;
			}

			rv = mm_remove_rec(newdirfd, flags, rec_lvl - 1);
			if (rv != 0 && (flags & MM_FAILONERROR))
				goto exit;

			/* try to remove the folder again
			 * it MAY have been cleansed by the recursive remove
			 * call */
			(void) unlinkat(dirfd, dp->d_name, AT_REMOVEDIR);
		}

		unlink_flag = (type == MM_DT_DIR) ? AT_REMOVEDIR : 0;
		rv = unlinkat(dirfd, dp->d_name, unlink_flag);
		if (rv != 0 && (flags & MM_FAILONERROR))
			goto exit;
	}

	exit_value = 0;

exit:
	closedir(dir);
	return exit_value;;
}


/**
 * mm_remove() - remove a file if of type authorized in flag list
 * @path:       path to the directory to remove
 * @flags:      bitflag of authorized filetypes that can be removed
 *              and removal option
 *
 * The mm_remove() function removes a file if its type is authorized in given
 * type flag argument. It also can remove files recursively.
 *
 * The @flag express whether the call is recursive and the recursivity behavior.
 * If the MM_RECURSIVE flag is set, then the call will be recursive.
 * Additionally, if MM_FAILONERROR is set, the removal operation will stop on
 * the first failure it will encounter. Otherwise, it will ignore all the errors
 * on any file or folder, and only return whether the call could be completed
 * with full success, or any number of possible error.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_remove(const char* path, int flags)
{
	int dirfd, rv, error_flags;
	int type = get_file_type(AT_FDCWD, path);

	if (type < 0)
		return mm_raise_from_errno("unable to get %s filetype", path);

	if ((flags & type) == 0)
		return mm_raise_error(EPERM, "failed to remove %s: "
		                      "invalid type", path);

	/* recusivly try to empty the folder */
	if (flags & MM_RECURSIVE && type == MM_DT_DIR) {
		if ((dirfd = mm_open(path, O_DIRECTORY, 0)) == -1) {
			return mm_raise_from_errno("recursive mm_remove(%s) failed: "
			                           "cannot open directory",
			                           path);
		}

		error_flags = mm_error_set_flags(MM_ERROR_SET, MM_ERROR_NOLOG);
		rv = mm_remove_rec(dirfd, flags, RECURSION_MAX);
		mm_error_set_flags(error_flags, MM_ERROR_NOLOG);
		if (rv != 0 && !(flags & MM_FAILONERROR)) {
			return mm_raise_from_errno(
				"recursive mm_remove(%s) failed",
				path);
		}

		/* allow rmdir(".") when called with the recursive flag only */
		if (is_wildcard_directory(path))
			return rv;
	}

	if (type == MM_DT_DIR)
		return mm_rmdir(path);
	else
		return mm_unlink(path);
}


/**
 * mm_opendir() - open a directory stream
 * @path:       path to directory
 *
 * The mm_opendir() function opens a directory stream corresponding to the
 * directory named by the @path argument. The directory stream is positioned
 * at the first entry.
 *
 * Return: A pointer usable with mm_readdir() on success, to be closed using
 * mm_closedir(). In case of error, NULL is returned and an error state is
 * set accordingly.
 */
API_EXPORTED
MM_DIR* mm_opendir(const char* path)
{
	DIR * dir;
	MM_DIR * d;
	d = malloc(sizeof(*d));
	if (d == NULL) {
		mm_raise_from_errno("opendir(%s) failed", path);
		return NULL;
	}

	dir = opendir(path);
	if (dir == NULL) {
		free(d);
		mm_raise_from_errno("opendir(%s) failed", path);
		return NULL;
	}

	d->dir = dir;
	d->dirent = NULL;

	return d;
}


/**
 * mm_closedir() - close a directory stream
 * @dir:        directory stream to close
 *
 * The mm_closedir() function closes the directory stream referred to by the
 * argument @dir. Upon return, the value of @dir may no longer point to an
 * accessible object of the type MM_DIR.
 */
API_EXPORTED
void mm_closedir(MM_DIR* dir)
{
	if (dir == NULL)
		return;

	closedir(dir->dir);
	free(dir->dirent);
	free(dir);
}


/**
 * mm_rewinddir() - reset a directory stream to its beginning
 * @dir:        directory stream to rewind
 *
 * The mm_rewinddir() function resets the position of the directory stream to
 * which @dir refers to the beginning of the directory. It causes the directory
 * stream to refer to the current state of the corresponding directory, as a
 * call to mm_opendir() would have done.
 */
API_EXPORTED
void mm_rewinddir(MM_DIR* dir)
{
	if (dir == NULL) {
		mm_raise_error(EINVAL,
		               "mm_rewinddir() does not accept NULL pointers");
		return;
	}

	rewinddir(dir->dir);
}


/**
 * mm_readdir() - read current entry from directory stream and advance it
 * @d:          directory stream to read
 * @status:     if not NULL, will contain whether readdir returned on error or
 *              end of dir
 *
 * The type MM_DIR represents a directory stream, which is an ordered sequence
 * of all the directory entries in a particular directory. Directory entries
 * present the files they contain, which may be added or removed from it
 * asynchronously to the operation of mm_readdir().
 *
 * The mm_readdir() function returns a pointer to a structure representing the
 * directory entry at the current position in the directory stream specified by
 * the argument @dir, and position the directory stream at the next entry which
 * will be valid until the next call to mm_readdir() with the same @dir
 * argument. It returns a NULL pointer upon reaching the end of the directory
 * stream.
 *
 * The @status argument is optional. It can be provided to gather information on
 * why the call to mm_readdir() returned NULL. Most of the time, this will
 * happen on end-of-dir, in which case status will be 0. However this is not
 * always the case - eg. if a required internal allocation fails - and then
 * status is filled with a negative value.
 *
 * Return: pointer to the file entry if directory stream has not reached the
 * end. NULL otherwise. In such a case and if an error has occurred and error
 * state is set accordingly and if @status is not NULL, pointed variable
 * will be set to -1.
 */
API_EXPORTED
const struct mm_dirent* mm_readdir(MM_DIR* d, int * status)
{
	size_t reclen, namelen;
	struct dirent* rd;

	if (status != NULL)
		*status = -1;

	if (d == NULL) {
		mm_raise_error(EINVAL,
		               "mm_readdir() does not accept NULL pointers");
		return NULL;
	}

	rd = readdir(d->dir);
	if (rd == NULL) {
		if (status != NULL)
			*status = 0;

		return NULL;
	}

	/* expand mm_dirent structure if needed */
	namelen = strlen(rd->d_name) + 1;
	reclen = sizeof(*d->dirent) + namelen;
	if (UNLIKELY(d->dirent == NULL || d->dirent->reclen < reclen)) {
		void * tmp = realloc(d->dirent, reclen);
		if (tmp == NULL) {
			mm_raise_from_errno("failed to alloc required memory");
			return NULL;
		}

		d->dirent = tmp;
		d->dirent->reclen = reclen;
	}

	switch (rd->d_type) {
	case DT_FIFO: d->dirent->type = MM_DT_FIFO; break;
	case DT_CHR:  d->dirent->type = MM_DT_CHR; break;
	case DT_DIR:  d->dirent->type = MM_DT_DIR; break;
	case DT_BLK:  d->dirent->type = MM_DT_BLK; break;
	case DT_REG:  d->dirent->type = MM_DT_REG; break;
	case DT_LNK:  d->dirent->type = MM_DT_LNK; break;
	case DT_SOCK: d->dirent->type = MM_DT_SOCK; break;
	default:      d->dirent->type = MM_DT_UNKNOWN; break;
	}

	strncpy(d->dirent->name, rd->d_name, namelen);

	if (status != NULL)
		*status = 0;

	return d->dirent;
}


#define COPYBUFFER_SIZE (1024*1024) // 1MiB

static
int clone_fd_fallback(int fd_in, int fd_out)
{
	size_t wbuf_sz;
	char * buffer, * wbuf;
	ssize_t rsz, wsz;
	int rv = -1;

	buffer = malloc(COPYBUFFER_SIZE);
	if (!buffer)
		return mm_raise_from_errno("unable to alloc transfer buffer");

	do {
		// Perform read operation
		rsz = mm_read(fd_in, buffer, COPYBUFFER_SIZE);
		if (rsz < 0)
			goto exit;

		// Do write of what has been read, possibly chunked if transfer
		// got interrupted
		wbuf = buffer;
		wbuf_sz = rsz;
		while (wbuf_sz) {
			wsz = mm_write(fd_out, wbuf, wbuf_sz);
			if (wsz < 0)
				goto exit;

			wbuf += wsz;
			wbuf_sz -= wsz;
		}
	} while (rsz != 0);

	rv = 0;

exit:
	free(buffer);
	return rv;
}


static
int clone_fd_try_cow(int fd_in, int fd_out)
{
#if HAVE_COPY_FILE_RANGE
	ssize_t rsz;
	ssize_t written = 0;
	int err, prev_errno = errno;

	do {
		rsz = copy_file_range(fd_in, NULL, fd_out, NULL, SSIZE_MAX, 0);
		if (rsz > 0)
			written += rsz;
	} while (rsz > 0);

	if (rsz < 0) {
		err = errno;
		if (err == ENOSYS || err == EXDEV) {
			errno = prev_errno;
			return clone_fd_fallback(fd_in, fd_out);
		}
		return mm_raise_from_errno("copy_file_range failed");
	}

	// copy_file_range may return 0 instead of error in case of
	// some filesystem. If no data has been written so far, try
	// copy fallback.
	return written ? 0 : clone_fd_fallback(fd_in, fd_out);
#else
	return clone_fd_fallback(fd_in, fd_out);
#endif
}


static
int copy_symlink(const char* src, const char* dst)
{
	int rv = 0;
	size_t tgt_sz;
	char* target = NULL;
	struct stat buf;

	if (lstat(src, &buf))
		return mm_raise_from_errno("lstat(%s) failed", src);

	tgt_sz = buf.st_size + 1;
	target = mm_malloca(tgt_sz);
	if (!target)
		return -1;

	if (mm_readlink(src, target, tgt_sz)
	    || mm_symlink(target, dst))
		rv = -1;

	mm_freea(target);
	return rv;
}


static
int clone_srcfd(int fd_in, const char* dst, int flags, int mode)
{
	int fd_out = -1;
	int rv = -1;

	fd_out = mm_open(dst, O_WRONLY|O_CREAT|O_EXCL, mode);
	if (fd_out == -1)
		return -1;

	switch (flags & MM_NOCOW) {
	case MM_NOCOW:
		rv = clone_fd_fallback(fd_in, fd_out);
		break;
	default:
		rv = clone_fd_try_cow(fd_in, fd_out);
	}

	mm_close(fd_out);
	return rv;
}


LOCAL_SYMBOL
int copy_internal(const char* src, const char* dst, int flags, int mode)
{
	int fd_in = -1;
	int rv = -1;
	int src_oflags;
	int err, prev_err = errno;

	src_oflags = O_RDONLY;
	if (flags & MM_NOFOLLOW)
		src_oflags |= O_NOFOLLOW;

	fd_in = open(src, src_oflags, 0);
	if (fd_in == -1) {
		err = errno;
		if ((flags & MM_NOFOLLOW) && (err == ELOOP)) {
			errno = prev_err;
			return copy_symlink(src, dst);
		}

		return mm_raise_from_errno("Cannot open %s", src);
	}

	rv = clone_srcfd(fd_in, dst, flags, mode);

	mm_close(fd_in);
	return rv;
}
