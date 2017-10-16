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



struct w32_create_file_options {
	DWORD access_mode;
	DWORD creation_mode;
	DWORD share_flags;
	DWORD file_attribute;
};

int set_w32_create_file_options(struct w32_create_file_options* opts, int oflags);


static inline
int wrap_handle_into_fd_with_logctx(HANDLE hnd, int* p_fd, const char* func,
                                    const char* srcfile, int srcline)
{
	int fd;
	int errnum;

	fd = _open_osfhandle((intptr_t)hnd, 0);

	if (LIKELY(fd != -1)) {
		*p_fd = fd;
		return 0;
	}

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

	if (LIKELY(hnd != INVALID_HANDLE_VALUE)) {
		*p_hnd = hnd;
		return 0;
	}

	errnum = errno;
	mm_raise_error_full(errnum, MMLOG_MODULE_NAME,
	                    func, srcfile, srcline, NULL,
	                    "Failed to unwrap windows handle from file"
	                    " descriptor: %s", strerror(errnum));
	return -1;
}

#define wrap_handle_into_fd(hnd, p_fd) \
	wrap_handle_into_fd_with_logctx(hnd, p_fd, __func__, __FILE__, __LINE__)

#define unwrap_handle_from_fd(p_hnd, fd) \
	unwrap_handle_from_fd_with_logctx(p_hnd, fd, __func__, __FILE__, __LINE__)

#endif

