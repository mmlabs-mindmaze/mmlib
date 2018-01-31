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

	opts->share_flags = FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE;
	opts->file_attribute = FILE_ATTRIBUTE_NORMAL;
	return 0;
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
	if ((fd < 0) && (fd >= MAX_FD))
		return -1;

	return fd_infos[fd];
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
