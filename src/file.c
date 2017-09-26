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
#include <stdarg.h>

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

#ifdef _WIN32
#  ifdef lseek
#    undef lseek
#  endif
#  define lseek         _lseeki64
#  define fsync         _commit

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


API_EXPORTED
int mm_fsync(int fd)
{
	if (fsync(fd) < 0)
		return mm_raise_from_errno("fsync(%i) failed", fd);

	return 0;
}


API_EXPORTED
mm_off_t mm_seek(int fd, mm_off_t offset, int whence)
{
	mm_off_t loc;

	loc = lseek(fd, offset, whence);
	if (loc < 0)
		return mm_raise_from_errno("lseek(%i, %lli, %i) failed", fd, offset, whence);

	return loc;
}


API_EXPORTED
int mm_ftruncate(int fd, mm_off_t length)
{
	if (ftruncate(fd, length) < 0)
		return mm_raise_from_errno("ftruncate(%i, %lli) failed", fd, length);

	return 0;
}


static
void conv_native_to_mm_stat(struct mm_stat* buf,
                            const struct stat* native_stat)
{
	buf->mode = native_stat->st_mode;
	buf->nlink = native_stat->st_nlink;
	buf->filesize = native_stat->st_size;
	buf->ctime = native_stat->st_ctime;
	buf->mtime = native_stat->st_mtime;
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
int mm_stat(const char* path, struct mm_stat* buf)
{
	struct stat native_stat;

	if (stat(path, &native_stat) < 0)
		return mm_raise_from_errno("stat(%s) failed", path);

	conv_native_to_mm_stat(buf, &native_stat);
	return 0;
}
