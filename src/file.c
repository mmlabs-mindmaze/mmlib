/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmlib.h"
#include "mmlog.h"
#include "mmerrno.h"
#include "mmsysio.h"
#include "file-internal.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <libgen.h>
#endif

#ifdef _WIN32
#  ifdef lseek
#    undef lseek
#  endif
#  define lseek _lseeki64
#  define fsync _commit

static
int ftruncate(int fd, mm_off_t size)
{
	errno_t ret;

	ret = _chsize_s(fd, size);
	if (ret != 0) {
		errno = ret;
		return -1;
	}

	return 0;
}

#endif //_WIN32


/**
 * mm_fsync() - synchronize changes to a file
 * @fd:         file description to synchronize
 *
 * This requests that all data for the open file descriptor named by @fd is
 * to be transferred to the storage device associated with the file
 * described by @fd. The mm_fsync() function does not return until the
 * system has completed that action or until an error is detected.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_fsync(int fd)
{
	if (fsync(fd) < 0)
		return mm_raise_from_errno("fsync(%i) failed", fd);

	return 0;
}


/**
 * mm_seek() - change file offset
 * @fd:          file descriptor
 * @offset:      delta
 * @whence:      how the @offset affect the file offset
 *
 * This function sets the file offset for the open file description associated
 * with the file descriptor @fd, as follows depending on the value in @whence
 *
 * %SEEK_SET
 *   the file offset shall be set to @offset bytes.
 * %SEEK_CUR
 *   the file offset shall be set to its current location plus @offset.
 * %SEEK_END
 *   the file offset shall be set to the size of the file plus @offset.
 *
 * Return: in case of success, the resulting offset location as measured in
 * bytes from the beginning of the file. Otherwise -1 with error state set
 * accordingly.
 */
API_EXPORTED
mm_off_t mm_seek(int fd, mm_off_t offset, int whence)
{
	mm_off_t loc;

	loc = lseek(fd, offset, whence);
	if (loc < 0)
		return mm_raise_from_errno("lseek(%i, %lli, %i) failed", fd,
		                           offset, whence);

	return loc;
}


/**
 * mm_ftruncate() -  truncate/resize a file to a specified length
 * @fd:         file descriptor of the file to resize
 * @length:     new length of the file
 *
 * If @fd refers to a regular file, mm_ftruncate() cause the size of the file
 * to be truncated to @length. If the size of the file previously exceeded
 * @length, the extra data shall no longer be available to reads on the file.
 * If the file previously was smaller than this size, mm_ftruncate() increases
 * the size of the file. If the file size is increased, the extended area will
 * appear as if it were zero-filled. The value of the seek pointer shall not be
 * modified by a call to mm_ftruncate().
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_ftruncate(int fd, mm_off_t length)
{
	if (ftruncate(fd, length) < 0)
		return mm_raise_from_errno("ftruncate(%i, %lli) failed", fd,
		                           length);

	return 0;
}


/**
 * internal_dirname() -  quick implementation of dirname()
 * @path:         the path to get the dir of
 *
 * This MAY OR MAY NOT modify in-place the given path so as to transform path
 * to contain its dirname.
 *
 * Return: a pointer to path.
 * If no dirname can be extracted from the path, it will return "." instead.
 *
 * Example:
 * dirname("/usr/lib/") -> "/usr\0lib/"
 * dirname("usr") -> "."
 *
 * Note: windows does not provide dirname, nor memrchr
 */
static
char* internal_dirname(char * path)
{
	char * c = path + strlen(path) - 1;

	/* skip the last chars if they're not a path */
	while (c > path && is_path_separator(*c))
		c--;

	while (--c > path) {
		if (is_path_separator(*c)) {
			/* remove consecutive separators (if any) */
			while (c > path && is_path_separator(*c)) {
				*c = '\0';
				c--;
			}

			return path;
		}
	}

	return ".";
}


/**
 * mm_mkdir_rec() - create directory recursively
 * @path:       path of directory to create
 * @mode:       permission to use to create the folder
 *
 * NOTE: although @path will be restored to its value, it needs to be writable.
 *
 * Return: 0 in case of success, -1 otherwise with error state set accordingly.
 */
static
int mm_mkdir_rec(char* path, int mode)
{
	int rv;
	int len, len_orig;

	rv = internal_mkdir(path, mode, 1);
	switch (rv) {
	case 0:
	case EEXIST:    return 0;
	case ENOENT:    break;
	default:        return -1;
	}

	/* prevent recursion: dirname(".") == "." */
	if (is_wildcard_directory(path))
		return 0;

	len_orig = strlen(path);
	rv = mm_mkdir_rec(internal_dirname(path), mode);

	/* restore the dir separators removed by internal_dirname() */
	len = strlen(path);
	while (len < len_orig && path[len] == '\0')
		path[len++] = '/';

	if (rv != 0) {
		mm_log_error("Failed to make dir %s", path);
		return -1;
	}

	return internal_mkdir(path, mode, 0);
}


/**
 * mm_mkdir() - creates a directory
 * @path:       path of the directory to create
 * @mode:       permission to use for directory creation
 * @flags:      creation flags
 *
 * The mm_mkdir() function creates a new directory with name @path. The file
 * permission bits of the new directory shall be initialized from @mode. These
 * file permission bits of the @mode argument are modified by the process' file
 * creation mask.
 *
 * The function will fail if the parent directory does not exist unless @flags
 * contains MM_RECURSIVE which is this case, the function will try to
 * recursively create the missing parent directories (using the file
 * permission).
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_mkdir(const char* path, int mode, int flags)
{
	int rv, len;
	char * tmp_path;

	if (flags & MM_RECURSIVE) {
		len = strlen(path);
		tmp_path = mm_malloca(len + 1);
		strcpy(tmp_path, path);
		rv = mm_mkdir_rec(tmp_path, mode);
		mm_freea(tmp_path);
	} else {
		rv = internal_mkdir(path, mode, 0);
	}

	return rv;
}


/**
 * mm_copy() - copy source to destination
 * @src:        path to file to copy from
 * @dst:        path to destination
 * @flags:      combination of flags controlling the copy
 * @mode:       access permission bits of created file
 *
 * Copies the content of @src into @dst. If the @dst already exists, the
 * function will fail. @mode will be used as permission mask for the created
 * file.
 *
 * @flags must be constructed by a bitwise-inclusive OR of flags from the
 * following list (maybe none):
 *
 * %MM_NOFOLLOW
 *   If @src is a symbolic link the copy is done on the symbolic link itself,
 *   not the target, thus producing a symbolic link in @dst.
 * %MM_NOCOW
 *   Prevents to perform a copy-on-write clone of the source content (reflink),
 *   even if the filesystem allows it.
 * %MM_FORCECOW
 *   Ensures the copy is performed through a copy-on-write clone of the source
 *   content (reflink), leading to failure if the filesystem or the conditions
 *   do not allow it.
 *
 * Note that %MM_NOCOW and %MM_FORCECOW affect only the copy of a regular
 * file. They will be ignored in the case of symlink used as source with
 * %MM_NOFOLLOW flag.
 *
 * If @src is neither a regular file or symbolic link, the function will fail.
 *
 * Return: 0 in case of success, -1 otherwise with error state set accordingly
 */
API_EXPORTED
int mm_copy(const char* src, const char* dst, int flags, int mode)
{
	if (flags & ~(MM_NOFOLLOW|MM_NOCOW|MM_FORCECOW))
		return mm_raise_error(EINVAL, "invalid flags (0x%08x)", flags);


	if ((flags & MM_NOCOW) && (flags & MM_FORCECOW))
		return mm_raise_error(EINVAL, "MM_NOCOW and MM_FORCECOW "
		                      "cannot be set together");

	return copy_internal(src, dst, flags, mode);
}
