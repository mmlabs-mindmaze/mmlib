/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmlib.h"
#include "mmerrno.h"
#include "mmpredefs.h"
#include "mmsysio.h"
#include "error-internal.h"
#include "file-internal.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>


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

struct mm_dirstream {
	DIR * dir;
	struct mm_dirent * dirent;
};

API_EXPORTED
int mm_chdir(const char* path)
{
	if (chdir(path) != 0)
		return mm_raise_from_errno("chdir(%s) failed", path);

	return 0;
}

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
		.mode = native_stat->st_mode,
		.nlink = native_stat->st_nlink,
		.filesize = native_stat->st_size,
		.ctime = native_stat->st_ctime,
		.mtime = native_stat->st_mtime,
	};
}


API_EXPORTED
int mm_fstat(int fd, struct mm_stat* buf)
{
	struct stat native_stat;

	if (fstat(fd, &native_stat) < 0)
		return mm_raise_from_errno("fstat(%i) failed", fd);

	conv_native_to_mm_stat(buf, &native_stat);
	return 0;
}


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
 * @flag:            option flag to return on error
 * @rec_lvl:         maximum recursion level
 *
 * Many error return values are *explicitely* skipped.
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
			goto exit;
		else
			return 0;
	}

	while ((dp = readdir(dir)) != NULL)
	{
		type = get_file_type(dirfd, dp->d_name);
		if (type > 0 && (flags & type) == 0)
			continue;  // only consider filtered files

		/* skip "." and ".." directories */
		if (is_wildcard_directory(dp->d_name))
			continue;

		if (type == MM_DT_DIR) {
			/* remove the inside of the folder */
			if ((newdirfd = openat(dirfd, dp->d_name, O_CLOEXEC, 0)) == -1) {
				if (flags & MM_FAILONERROR)
					goto exit;
				else
					continue;
			}

			rv = mm_remove_rec(newdirfd, flags, rec_lvl - 1);
			if (rv != 0 && (flags & MM_FAILONERROR))
				goto exit;

			/* try to remove the folder again
			 * it MAY have been cleansed by the recursive remove call */
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
			                           "cannot open directory", path);
		}
		error_flags = mm_error_set_flags(MM_ERROR_NOLOG, MM_ERROR_NOLOG);
		rv = mm_remove_rec(dirfd, flags, RECURSION_MAX);
		mm_error_set_flags(error_flags, MM_ERROR_ALL);
		if (rv != 0 && !(flags & MM_FAILONERROR)) {
			return mm_raise_from_errno("recursive mm_remove(%s) failed", path);
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

API_EXPORTED
MMDIR * mm_opendir(const char* path)
{
	DIR * dir;
	MMDIR * d;
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

API_EXPORTED
void mm_closedir(MMDIR* dir)
{
	if (dir == NULL)
		return;

	closedir(dir->dir);
	free(dir->dirent);
	free(dir);
}

API_EXPORTED
void mm_rewinddir(MMDIR* dir)
{
	if (dir == NULL) {
		mm_raise_error(EINVAL, "mm_rewinddir() does not accept NULL pointers");
		return;
	}

	rewinddir(dir->dir);
}

API_EXPORTED
const struct mm_dirent* mm_readdir(MMDIR* d, int * status)
{
	size_t reclen, namelen;
	struct dirent* rd;

	if (status != NULL)
		*status = -1;

	if (d == NULL) {
		mm_raise_error(EINVAL, "mm_readdir() does not accept NULL pointers");
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
			mm_raise_from_errno("readdir() failed to alloc the required memory");
			return NULL;
		}
		d->dirent = tmp;
		d->dirent->reclen = reclen;
	}

	switch(rd->d_type) {
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
