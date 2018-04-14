/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include "utils-win32.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>

#include "mmerrno.h"
#include "mmlog.h"
#include "mmlib.h"

static
int set_access_mode(struct w32_create_file_options* opts, int oflags)
{
	switch(oflags & (_O_RDONLY|_O_WRONLY| _O_RDWR)) {
	case _O_RDONLY:
		opts->access_mode = GENERIC_READ;
		break;

	case _O_WRONLY:
		opts->access_mode = GENERIC_WRITE;
		break;

	case _O_RDWR:
		opts->access_mode = (GENERIC_READ | GENERIC_WRITE);
		break;

	default:
		mm_raise_error(EINVAL, "Invalid combination of file access mode");
		return -1;
	}

	return 0;
}


static
int set_creation_mode(struct w32_create_file_options* opts, int oflags)
{
	switch (oflags & (_O_TRUNC|_O_CREAT|_O_EXCL)) {
	case 0:
	case _O_EXCL:
		opts->creation_mode = OPEN_EXISTING;
		break;

	case _O_CREAT:
		opts->creation_mode = OPEN_ALWAYS;
		break;

	case _O_TRUNC:
	case _O_TRUNC|_O_EXCL:
		opts->creation_mode = TRUNCATE_EXISTING;
		break;

	case _O_CREAT|_O_EXCL:
	case _O_CREAT|_O_TRUNC|_O_EXCL:
		opts->creation_mode = CREATE_NEW;
		break;

	case _O_CREAT|_O_TRUNC:
		opts->creation_mode = CREATE_ALWAYS;
		break;

	default:
		mm_crash("Previous cases should have covered all possibilities");
	}

	return 0;
}


LOCAL_SYMBOL
int set_w32_create_file_options(struct w32_create_file_options* opts, int oflags)
{
	if ( set_access_mode(opts, oflags)
	  || set_creation_mode(opts, oflags) )
		return -1;

	opts->file_attribute = FILE_ATTRIBUTE_NORMAL;
	return 0;
}


/**
 * open_handle() - Helper to open file from UTF-8 path
 * @path:       UTF-8 path fo the file to open
 * @access:     desired access to file (dwDesiredAccess in CreateFile())
 * @creat:      action to take on a file or device that exists or does not
 *              exist. (dwCreationDisposition argument in CreateFile())
 * @sec:        Security descriptor to use if the file is created. If NULL,
 *              the file associated with the returned handle is assigned a
 *              default security descriptor.
 * @flags:      Flags and attributes. (dwFlagsAndAttributes in CreateFile())
 *
 * Return: in case of success, the handle of the file opened or created.
 * Otherwise INVALID_HANDLE_VALUE. Please note that this function is meant to
 * be helper for implement public operation and as such does not set error
 * state for the sake of more informative error reporting (The caller has
 * indeed the context of the call. It may retrieve error with GetLastError())
 */
LOCAL_SYMBOL
HANDLE open_handle(const char* path, DWORD access, DWORD creat,
                   SECURITY_DESCRIPTOR* sec, DWORD flags)
{
	HANDLE hnd = INVALID_HANDLE_VALUE;
	char16_t* path_u16 = NULL;
	int path_u16_len;
	DWORD share = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;
	SECURITY_ATTRIBUTES sec_attrs = {
		.nLength = sizeof(SECURITY_ATTRIBUTES),
		.lpSecurityDescriptor = sec,
		.bInheritHandle = FALSE,
	};

	// Get size for converted path into UTF-16
	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0)
		return INVALID_HANDLE_VALUE;

	// Allocate temporary UTF-16 path
	path_u16 = mm_malloca(path_u16_len*sizeof(*path_u16));
	if (!path_u16)
		return INVALID_HANDLE_VALUE;

	// Convert to UTF-16 and open/create file
	conv_utf8_to_utf16(path_u16, path_u16_len, path);
	hnd = CreateFileW(path_u16, access, share, &sec_attrs,
	                  creat, flags, NULL);

	mm_freea(path_u16);

	return hnd;
}


static
int get_errcode_from_w32err(DWORD w32err)
{
	switch(w32err) {

	case ERROR_TOO_MANY_OPEN_FILES:
	case WSAEMFILE:                 return EMFILE;
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:      return EEXIST;
	case ERROR_PATH_NOT_FOUND:
	case ERROR_FILE_NOT_FOUND:      return ENOENT;
	case ERROR_INVALID_HANDLE:      return EBADF;
	case ERROR_OUTOFMEMORY:         return ENOMEM;
	case ERROR_ACCESS_DENIED:
	case WSAEACCES:                 return EACCES;
	case ERROR_INVALID_PARAMETER:
	case ERROR_INVALID_ACCESS:
	case ERROR_INVALID_DATA:
	case WSAEINVAL:                 return EINVAL;
	case WSAESHUTDOWN:
	case ERROR_NO_DATA:
	case ERROR_BROKEN_PIPE:         return EPIPE;
	case WSASYSNOTREADY:
	case WSAENETDOWN:               return ENETDOWN;
	case WSAENETUNREACH:            return ENETUNREACH;
	case WSAVERNOTSUPPORTED:        return ENOSYS;
	case WSAEINPROGRESS:            return EINPROGRESS;
	case WSAECONNRESET:             return ECONNRESET;
	case WSAEFAULT:                 return EFAULT;
	case WSAEMSGSIZE:               return EMSGSIZE;
	case WSAENOBUFS:                return ENOBUFS;
	case WSAEISCONN:                return EISCONN;
	case WSAENOTCONN:               return ENOTCONN;
	case WSAENOTSOCK:               return ENOTSOCK;
	case WSAECONNREFUSED:           return ECONNREFUSED;
	case WSAEDESTADDRREQ:           return EDESTADDRREQ;
	case WSAEADDRINUSE:             return EADDRINUSE;
	case WSAEADDRNOTAVAIL:          return EADDRNOTAVAIL;
	case WSAEOPNOTSUPP:             return EOPNOTSUPP;
	case WSAEINTR:                  return EINTR;
	case WSAENOPROTOOPT:            return ENOPROTOOPT;
	case WSAEPFNOSUPPORT:
	case WSAEAFNOSUPPORT:           return EAFNOSUPPORT;
	case WSAEPROTOTYPE:             return EPROTOTYPE;
	case WAIT_TIMEOUT:
	case WSAETIMEDOUT:              return ETIMEDOUT;
	case ERROR_NO_UNICODE_TRANSLATION: return EILSEQ;

	default:
		return EIO;
	}
}


LOCAL_SYMBOL
int mm_raise_from_w32err_full(const char* module, const char* func,
                              const char* srcfile, int srcline,
                              const char* desc, ...)
{
	DWORD w32err = GetLastError();
	int errcode, ret;
	size_t len;
	va_list args;
	char errmsg[512];

	len = snprintf(errmsg, sizeof(errmsg), "%s : ", desc);

	// Append Win32 error message if space is remaining on string
	if (len < sizeof(errmsg)-1) {
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
		              | FORMAT_MESSAGE_IGNORE_INSERTS
		              | FORMAT_MESSAGE_MAX_WIDTH_MASK,
		              NULL, w32err, 0,
		              errmsg+len, sizeof(errmsg)-len, NULL);
	}

	// Translate win32 error into mmlib error code
	errcode = get_errcode_from_w32err(w32err);

	va_start(args, desc);
	ret = mm_raise_error_vfull(errcode, module, func,
	                           srcfile, srcline, NULL, errmsg, args);
	va_end(args);

	return ret;
}


/**************************************************************************
 *                                                                        *
 *                  File descriptor mmlib metadata                        *
 *                                                                        *
 **************************************************************************/

/**
 * guess_fd_info() - inspect fd and associated handle and guess type info
 * @fd:         file descriptor to inspect
 *
 * Once the type is guessed, it is stored in mmlib fd metadata so that it will
 * not be guessed the next time @fd is encountered... Of course, until
 * mm_close() is called on @fd.
 *
 * Return: a FD_TYPE_* constant different from FD_TYPE_UNKNOWN in case of
 * success, -1 in case of failure.
 */
static
int guess_fd_info(int fd)
{
	int info;
	HANDLE hnd;
	DWORD mode;

	hnd = (HANDLE)_get_osfhandle(fd);
	if (hnd == INVALID_HANDLE_VALUE)
		return -1;

	info = FD_TYPE_MSVCRT;
	if (_isatty(fd) && GetConsoleMode(hnd, &mode))
		info = FD_TYPE_CONSOLE;

	set_fd_info(fd, info);
	return info;
}

// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setmaxstdio
#define MAX_FD	2048


static unsigned char fd_infos[MAX_FD] = {0};


/**
 * get_fd_info_checked() - validate file descriptor and retrieve its info
 * @fd:         file descriptor whose info has to be retrieved
 *
 * Return: If successful, a non-negative file descriptor info. If @fd cannot be
 * a valid file descriptor -1 is returned (please note that error state is NOT
 * set in such a case).
 */
LOCAL_SYMBOL
int get_fd_info_checked(int fd)
{
	int info;

	if ((fd < 0) && (fd >= MAX_FD))
		return -1;

	info = fd_infos[fd];
	if (UNLIKELY(info == FD_TYPE_UNKNOWN))
		info = guess_fd_info(fd);

	return info;
}


/**
 * get_fd_info() - get file descriptor info (no validation check)
 * @fd:         file descriptor whose info has to be retrieved
 *
 * Return: file descriptor info
 */
LOCAL_SYMBOL
int get_fd_info(int fd)
{
	return fd_infos[fd];
}


/**
 * set_fd_info() - set file descriptor info (no validation check)
 * @fd:         file descriptor whose info has to be set
 */
LOCAL_SYMBOL
void set_fd_info(int fd, int info)
{
	fd_infos[fd] = info;
}


/**************************************************************************
 *                                                                        *
 *                         UTF-8/UTF-16 conversion                        *
 *                                                                        *
 **************************************************************************/

/**
 * get_utf16_buffer_len_from_utf8() - get size needed for the UTF-16 string
 * @utf8_str:   null terminated UTF-8 encoded string
 *
 * Return: number of UTF-16 code unit (ie char16_t) needed to hold the UTF-16
 * encoded string that would be equivalent to @utf8_str (this includes the
 * NUL termination).
 */
LOCAL_SYMBOL
int get_utf16_buffer_len_from_utf8(const char* utf8_str)
{
	int len;

	len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                          utf8_str, -1,
	                          NULL, 0);

	return (len == 0) ? -1 : len;
}


/**
 * conv_utf8_to_utf16() - convert UTF-8 string into a UTF-16 string
 * @utf16_str:  buffer receiving the converted UTF-16 string
 * @utf16_len:  length of @utf16_len in code unit (char16_t)
 * @utf8_str:   null terminated UTF-8 encoded string
 *
 * This function convert the string @utf8_str encoded in UTF-8 into UTF-16
 * and store the result in @utf16_str. The length @utf16_len of this buffer
 * must be large enough to hold the whole transformed string including NUL
 * termination. Use get_utf16_buffer_size_from_utf8() to allocate the
 * necessary size.
 *
 * Return: O in case of success, -1 otherwise with error state set
 */
LOCAL_SYMBOL
int conv_utf8_to_utf16(char16_t* utf16_str, int utf16_len, const char* utf8_str)
{
	int len;

	len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                          utf8_str, -1, utf16_str, utf16_len);

	return (len == 0) ? -1 : len;
}


/**
 * get_utf8_buffer_size_from_utf16() - get size needed for the UTF-8 string
 * @utf16_str:   null terminated UTF-16 encoded string
 *
 * Return: number of UTF-8 code unit (ie char) needed to hold the UTF-8
 * encoded string that would be equivalent to @utf16_str (this includes the
 * NUL termination).
 */
LOCAL_SYMBOL
int get_utf8_buffer_len_from_utf16(const char16_t* utf16_str)
{
	int len;
	len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
	                          utf16_str, -1, NULL, 0, NULL, NULL);

	return (len == 0) ? -1 : len;
}


/**
 * conv_utf16_to_utf8() - convert UTF-16 string into a UTF-8 string
 * @utf8_str:  buffer receiving the converted UTF-8 string
 * @utf8_len:  length of @utf8_len in code unit (char16_t)
 * @utf16_str:   null terminated UTF-16 encoded string
 *
 * This function convert the string @utf16_str encoded in UTF-16 into UTF-8
 * and store the result in @utf8_str. The length @utf8_len of this buffer
 * must be large enough to hold the whole transformed string including NUL
 * termination. Use get_utf8_buffer_size_from_utf16() to allocate the
 * necessary size.
 *
 * Return: O in case of success, -1 otherwise with error state set
 */
LOCAL_SYMBOL
int conv_utf16_to_utf8(char* utf8_str, int utf8_len, const char16_t* utf16_str)
{
	int len;
	len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
	                          utf16_str, -1, utf8_str, utf8_len,
	                          NULL, NULL);

	return (len == 0) ? -1 : len;
}


