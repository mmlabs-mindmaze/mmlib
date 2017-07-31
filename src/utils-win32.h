/*
   @mindmaze_header@
*/
#ifndef UTILS_WIN32_H
#define UTILS_WIN32_H

#include "mmerrno.h"
#include "mmpredefs.h"

#include <windows.h>
#include <winternl.h>
#include <stddef.h>
#include <io.h>
#include <fcntl.h>


struct w32_create_file_options {
	DWORD access_mode;
	DWORD creation_mode;
	DWORD share_flags;
	DWORD file_attribute;
};

int set_w32_create_file_options(struct w32_create_file_options* opts, int oflags);
int mm_raise_from_w32err_full(const char* module, const char* func,
                              const char* srcfile, int srcline,
                              const char* desc, ...);
#define mm_raise_from_w32err(desc, ...) \
	mm_raise_from_w32err_full(MMLOG_MODULE_NAME, __func__, __FILE__, __LINE__, desc,  ## __VA_ARGS__ )


/**************************************************************************
 *                                                                        *
 *                 Win32 handle / file descriptor wrapping                *
 *                                                                        *
 **************************************************************************/

enum {
	FD_TYPE_MSVCRT = 0,
	FD_TYPE_NORMAL,
	FD_TYPE_PIPE,
	FD_TYPE_IPCDGRAM,
	FD_TYPE_MASK = 0x07
};

#define FD_FIRST_FLAG	(FD_TYPE_MASK+1)
#define FD_FLAG_APPEND	(FD_FIRST_FLAG << 0)

int get_fd_info_checked(int fd);
int get_fd_info(int fd);
void set_fd_info(int fd, int info);


static inline
int wrap_handle_into_fd_with_logctx(HANDLE hnd, int* p_fd, int info,
                                    const char* func,
                                    const char* srcfile, int srcline)
{
	int fd, osf_flags, errnum;

	osf_flags = _O_NOINHERIT | _O_BINARY;
	if (info & FD_FLAG_APPEND)
		osf_flags |= _O_APPEND;

	fd = _open_osfhandle((intptr_t)hnd, osf_flags);
	if (UNLIKELY(fd == -1))
		goto error;

	set_fd_info(fd, info);
	*p_fd = fd;
	return 0;

error:
	errnum = errno;
	mm_raise_error_full(errnum, MMLOG_MODULE_NAME,
	                    func, srcfile, srcline, NULL,
	                    "Failed to wrap windows handle into file"
	                    " descriptor: %s", strerror(errnum));
	return -1;
}


static inline
int unwrap_handle_from_fd_with_logctx(HANDLE* p_hnd, int fd, const char* func,
                                      const char* srcfile, int srcline)
{
	HANDLE hnd;
	int errnum;

	hnd = (HANDLE)_get_osfhandle(fd);
	if (UNLIKELY(hnd == INVALID_HANDLE_VALUE))
		goto error;

	*p_hnd = hnd;
	return 0;

error:
	errnum = errno;
	mm_raise_error_full(errnum, MMLOG_MODULE_NAME,
	                    func, srcfile, srcline, NULL,
	                    "Failed to unwrap windows handle from file"
	                    " descriptor: %s", strerror(errnum));
	return -1;
}

#define wrap_handle_into_fd(hnd, p_fd, type) \
	wrap_handle_into_fd_with_logctx(hnd, p_fd, type, __func__, __FILE__, __LINE__)

#define unwrap_handle_from_fd(p_hnd, fd) \
	unwrap_handle_from_fd_with_logctx(p_hnd, fd, __func__, __FILE__, __LINE__)

#endif

