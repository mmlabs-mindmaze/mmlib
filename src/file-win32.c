/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN8
#endif

#include "mmsysio.h"
#include "mmerrno.h"
#include "mmlib.h"
#include "error-internal.h"
#include "file-internal.h"
#include "utils-win32.h"

#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdbool.h>
#include <uchar.h>
#include <winnt.h>
#include <ntdef.h>
#include <winioctl.h>

#define NUM_ATTEMPT	256

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x02
#endif

struct mm_dirstream {
	HANDLE hdir;
	int find_first_done;       /* flag specifying the folder has been though FindFirstFile() */
	struct mm_dirent * dirent;
	char dirname[];
};

static int win32_temp_path_len;
static char win32_temp_path[(MAX_PATH + 1)];

MM_CONSTRUCTOR(win32_temp_path)
{
	int rv;
	rv = GetTempPath(MAX_PATH, win32_temp_path);
	if (rv != 0 && rv < MAX_PATH) {
		win32_temp_path_len = strlen(win32_temp_path);
	} else {
		win32_temp_path[0] = 0;
		win32_temp_path_len = 0;
	}
}

/**
 * mmlib_read() - perform a read operation using local implementation
 * @fd:         file descriptor to read
 * @buf:        buffer to hold the data to read
 * @nbyte:      number of byte to read
 *
 * Perform read assuming file type on which ReadFile() is possible without
 * passing special flags or not requiring transforming the data.
 *
 * Return: number of byte read in case of success. Otherwise -1 is returned and
 * error state is set accordingly
 */
static
ssize_t mmlib_read(int fd, void* buf, size_t nbyte)
{
	DWORD read_sz;
	HANDLE hnd;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	if (!ReadFile(hnd, buf, nbyte, &read_sz, NULL))
		return mm_raise_from_w32err("ReadFile() for fd=%i failed", fd);

	return read_sz;
}


/**
 * mmlib_write() - perform a write operation using local implementation
 * @fd:         file descriptor to write
 * @buf:        buffer to hold the data to write
 * @nbyte:      number of byte to write
 *
 * Perform write assuming file type on which WriteFile() is possible without
 * passing special flags or not requiring transforming the data.
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned and
 * error state is set accordingly
 */
static
ssize_t mmlib_write(int fd, const void* buf, size_t nbyte)
{
	DWORD written_sz;
	HANDLE hnd;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	// If file is opened in append mode, we must reset file pointer to
	// the end.
	if (get_fd_info(fd) & FD_FLAG_APPEND) {
		if (!SetFilePointer(hnd, 0, NULL, FILE_END)) {
			mm_raise_from_w32err("File (fd=%i) is opend in append mode"
			                     " but can't seek file end", fd);
			return -1;
		}
	}

	if (!WriteFile(hnd, buf, nbyte, &written_sz, NULL))
		return mm_raise_from_w32err("WriteFile() for fd=%i failed", fd);

	return written_sz;
}


/**
 * console_read() - read UTF-8 from console
 * @fd:         console file descriptor
 * @buf:	buffer that should hold the console input
 * @nbyte:      size of @buf
 *
 * Return: In case of success, a non negative number corresponding to the
 * number of the byte read. -1 in case of failure with error state set
 */
static
ssize_t console_read(int fd, char* buf, size_t nbyte)
{
	DWORD nchar16_read;
	HANDLE hnd;
	size_t nchar16;
	char16_t* buf16;
	int i, len, rsz = -1;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	// Allocate supplementary buffer to hold UTF-16 data from console
	nchar16 = nbyte / sizeof(char16_t);
	buf16 = mm_malloca(nchar16*sizeof(char16_t));

	// Read console input in UTF-16
	if (!ReadConsoleW(hnd, buf16, nchar16, &nchar16_read, NULL)) {
		mm_raise_from_w32err("Cannot read from console");
		goto exit;
	}

	// Convert in console data in UTF-8
	len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
	                          buf16, nchar16_read, buf, nbyte,
	                          NULL, NULL);
	if (len < 0) {
		mm_raise_from_w32err("Invalid UTF-8 buffer");
		goto exit;
	}

	// Convert CRLF into LF
	rsz = 0;
	for (i = 0; i < len; i++) {
		if ((i != 0) && (buf[i-1] == '\r') && (buf[i] == '\n'))
			continue;

		buf[rsz++] = buf[i];
	}

exit:
	mm_freea(buf16);
	return rsz;
}


/**
 * console_write() - write UTF-8 to console
 * @fd:         console file descriptor
 * @buf:        UTF-8 string to be written on console
 * @nbyte:      size of @buf
 *
 * Return: In case of success, a non negative number corresponding to the
 * number of the byte written. -1 in case of failure with error state set
 */
static
ssize_t console_write(int fd, const char* buf, size_t nbyte)
{
	DWORD nchar16_written;
	HANDLE hnd;
	int i, len_crlf, nchar16, nchar;
	char *buf_crlf = NULL;
	char16_t* buf16 = NULL;
	ssize_t rsz = -1;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	// Allocate temporary buffers
	buf_crlf = mm_malloca(2*nbyte);
	buf16 = mm_malloca(2*nbyte*sizeof(*buf16));

	// Convert LF into CRLF and get size of crlf transformed string
	nchar = nbyte;
	for (i = 0, len_crlf = 0; i < nchar; i++) {
		if (buf[i] == '\n')
			buf_crlf[len_crlf++] = '\r';

		buf_crlf[len_crlf++] = buf[i];
	}

	// Convert UTF-8 (with crlf) into UTF-16
	nchar16 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                             buf_crlf, len_crlf, buf16, 2*nbyte);
	if (nchar16 < 0) {
		mm_raise_from_w32err("Invalid UTF-8 buffer");
		goto exit;
	}

	// Write the UTF-16 sequence to console
	if (!WriteConsoleW(hnd, buf16, nchar16, &nchar16_written, NULL)) {
		mm_raise_from_w32err("Cannot write to console");
		goto exit;
	}

	// Convert the number of UTF-16 code unit written into number of bytes
	// in the original UTF-8 sequence
	rsz = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
	                          buf16, nchar16_written, NULL, -1,
	                          NULL, NULL);

	// Remove from count the inserted CR in each CRLF. This way, we really
	// have the size of the part of @buf which has been written
	for (i = 1; i < nchar16; i++) {
		if (buf16[i-1] == L'\r' && buf16[i] == L'\n')
			rsz--;
	}

exit:
	mm_freea(buf16);
	mm_freea(buf_crlf);
	return rsz;
}


/**
 * msvcrt_read() - perform a read operation using MSVCRT implementation
 * @fd:         file descriptor to read
 * @buf:        buffer to hold the data to read
 * @nbyte:      number of byte to read
 *
 * Same as _read() from MSVCRT, excepting that error state is set in case of
 * error.
 *
 * Return: number of byte read in case of success. Otherwise -1 is returned and
 * error state is set accordingly
 */
static
ssize_t msvcrt_read(int fd, void* buf, size_t nbyte)
{
	ssize_t rsz;

	rsz = _read(fd, buf, nbyte);
	if (rsz < 0)
		return mm_raise_from_errno("_read(%i, ...) failed", fd);

	return rsz;
}


/**
 * msvcrt_write() - perform a write operation using MSVCRT implementation
 * @fd:         file descriptor to write
 * @buf:        buffer to hold the data to write
 * @nbyte:      number of byte to write
 *
 * Same as _write() from MSVCRT, excepting that error state is set in case of
 * error.
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned and
 * error state is set accordingly
 */
static
ssize_t msvcrt_write(int fd, const void* buf, size_t nbyte)
{
	ssize_t rsz;

	rsz = _write(fd, buf, nbyte);
	if (rsz < 0)
		return mm_raise_from_errno("_write(%i, ...) failed", fd);

	return rsz;
}

/* doc in posix implementation */
API_EXPORTED
int mm_open(const char* path, int oflag, int mode)
{
	HANDLE hnd;
	int fdinfo, fd = -1;
	struct local_secdesc lsd;
	struct w32_create_file_options opts;

	if (set_w32_create_file_options(&opts, oflag))
		return -1;

	if (local_secdesc_init_from_mode(&lsd, mode)) {
		mm_raise_from_w32err("can't create security descriptor");
		goto exit;
	}

	hnd = open_handle(path, opts.access_mode, opts.creation_mode,
	                  &lsd.desc, opts.file_attribute);
	if (hnd == INVALID_HANDLE_VALUE) {
		mm_raise_from_w32err("Can't get handle for %s", path);
		goto exit;
	}

	fdinfo = FD_TYPE_NORMAL;
	if (mode & O_APPEND)
		fdinfo |= FD_FLAG_APPEND;

	if (wrap_handle_into_fd(hnd, &fd, fdinfo)) {
		CloseHandle(hnd);
		goto exit;
	}

exit:
	local_secdesc_deinit(&lsd);
	return fd;
}


/* doc in posix implementation */
API_EXPORTED
int mm_close(int fd)
{
	if (fd == -1)
		return 0;

	if (_close(fd) < 0)
		return mm_raise_from_errno("_close(%i) failed", fd);

	set_fd_info(fd, FD_TYPE_UNKNOWN);
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
ssize_t mm_read(int fd, void* buf, size_t nbyte)
{
	ssize_t rsz;
	struct mmipc_msg ipcmsg;
	struct iovec iov;
	int fd_info;

	fd_info = get_fd_info_checked(fd);
	if (fd_info < 0)
		return mm_raise_error(EBADF, "Invalid file descriptor");

	switch(fd_info & FD_TYPE_MASK) {

	case FD_TYPE_NORMAL:
	case FD_TYPE_PIPE:
		rsz = mmlib_read(fd, buf, nbyte);
		break;

	case FD_TYPE_CONSOLE:
		rsz = console_read(fd, buf, nbyte);
		break;

	case FD_TYPE_SOCKET:
		rsz = mm_recv(fd, buf, nbyte, 0);
		break;

	case FD_TYPE_IPCDGRAM:
		iov.iov_base = buf;
		iov.iov_len = nbyte;
		ipcmsg = (struct mmipc_msg) {.iov = &iov, .num_iov = 1};
		rsz = mmipc_recvmsg(fd, &ipcmsg);
		break;

	case FD_TYPE_MSVCRT:
		rsz = msvcrt_read(fd, buf, nbyte);
		break;

	default:
		return mm_raise_error(EBADF, "Invalid file descriptor");

	}

	return rsz;
}


/* doc in posix implementation */
API_EXPORTED
ssize_t mm_write(int fd, const void* buf, size_t nbyte)
{
	ssize_t rsz;
	struct mmipc_msg ipcmsg;
	struct iovec iov;
	int fd_info;

	fd_info = get_fd_info_checked(fd);
	if (fd_info < 0)
		return mm_raise_error(EBADF, "Invalid file descriptor");

	switch(fd_info & FD_TYPE_MASK) {

	case FD_TYPE_NORMAL:
	case FD_TYPE_PIPE:
		rsz = mmlib_write(fd, buf, nbyte);
		break;

	case FD_TYPE_CONSOLE:
		rsz = console_write(fd, buf, nbyte);
		break;

	case FD_TYPE_SOCKET:
		rsz = mm_send(fd, buf, nbyte, 0);
		break;

	case FD_TYPE_IPCDGRAM:
		iov.iov_base = (void*)buf;
		iov.iov_len = nbyte;
		ipcmsg = (struct mmipc_msg) {.iov = &iov, .num_iov = 1};
		rsz = mmipc_sendmsg(fd, &ipcmsg);
		break;

	case FD_TYPE_MSVCRT:
		rsz = msvcrt_write(fd, buf, nbyte);
		break;

	default:
		return mm_raise_error(EBADF, "Invalid file descriptor");

	}

	return rsz;
}


/* doc in posix implementation */
API_EXPORTED
int mm_dup(int fd)
{
	int newfd;

	newfd = _dup(fd);
	if (newfd < 0)
		return mm_raise_from_errno("_dup(%i) failed", fd);

	set_fd_info(newfd, get_fd_info(fd));
	return newfd;
}


/* doc in posix implementation */
API_EXPORTED
int mm_dup2(int fd, int newfd)
{
	if (_dup2(fd, newfd) < 0)
		return mm_raise_from_errno("_dup2(%i, %i) failed", fd, newfd);

	set_fd_info(newfd, get_fd_info(fd));
	return newfd;
}


/* doc in posix implementation */
API_EXPORTED
int mm_pipe(int pipefd[2])
{
	HANDLE wr_hnd, rd_hnd;

	if (!CreatePipe(&rd_hnd, &wr_hnd, NULL, 16*MM_PAGESZ))
		return mm_raise_from_w32err("Cannot create pipe");

	if (wrap_handle_into_fd(rd_hnd, &pipefd[0], FD_TYPE_PIPE)) {
		CloseHandle(rd_hnd);
		CloseHandle(wr_hnd);
		return -1;
	}

	if (wrap_handle_into_fd(wr_hnd, &pipefd[1], FD_TYPE_PIPE)) {
		_close(pipefd[0]);
		CloseHandle(wr_hnd);
		return -1;
	}

	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mm_isatty(int fd)
{
	int info;

	info = get_fd_info_checked(fd);
	if (info == -1) {
		mm_raise_error(EBADF, "Bad file descriptor (%i)", fd);
		return -1;
	}

	return (info == FD_TYPE_CONSOLE);
}


static
const char * win32_basename(const char * path)
{
	const char * c = path + strlen(path) - 1;

	/* skip the last chars if they're not a path */
	while (c > path && (*c == '/' || *c == '\\'))
		c--;

	while (c > path) {
		if (*c == '/' || *c == '\\')
			return c + 1;
		c--;
	}
	return path;
}


/**
 * rename_file_handle() - rename a file though handle
 * @hnd:        file handle opened with DELETE access
 * @path:       UTF-8 path to which the file must be renamed
 *
 * Return: 0 in case of success, -1 otherwise. Please note that this
 * function is meant to be helper for implement public operation and as such
 * does not set error state (retrieve error in caller with GetLastError())
 */
static
int rename_file_handle(HANDLE hnd, const char* path)
{
	FILE_RENAME_INFO* info;
	size_t info_sz;
	int path_u16_len, rv = -1;

	// Get size for converted path into utf-16
	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0)
		return -1;

	// Allocate (and init) temporary rename info structure in order to
	// rename path (UTF-16 in allocated within this structure)
	info_sz = sizeof(*info);
	info_sz += path_u16_len * sizeof(info->FileName[0]);
	info = mm_malloca(info_sz);
	if (!info)
		return -1;

	// Init rename structure (convert path to UTF-16) and try to do it
	info->RootDirectory = NULL;
	info->ReplaceIfExists = FALSE;
	info->FileNameLength = path_u16_len * sizeof(info->FileName[0]);
	conv_utf8_to_utf16(info->FileName, path_u16_len, path);
	if (SetFileInformationByHandle(hnd, FileRenameInfo, info, info_sz))
		rv = 0;

	mm_freea(info);
	return rv;
}


/* doc in posix implementation */
API_EXPORTED
int mm_unlink(const char* path)
{
	size_t delpath_maxlen;
	char* delpath;
	HANDLE hnd;
	int i;

	// Open file with DELETE_ON_CLOSE flag. If this operation has
	// succeed, the file will be deleted when CloseHandle() is called.
	hnd = open_handle(path, DELETE, OPEN_EXISTING, NULL,
	                  FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_OPEN_REPARSE_POINT);
	if (hnd == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("Can't get handle for %s", path);

	// Allocate (and init) temporary rename info structure in order to rename path
	// into TEMP_FILE/<basename>.deleted-#### or <path>.deleted-####
	delpath_maxlen = strlen(path)+win32_temp_path_len+64;
	delpath = mm_malloca(delpath_maxlen);

	// Rename before closing (hence marking file for deleting) to make the
	// filename reusable immediately even if the file object is not deleted
	// immediately (make different rename attempt in order to find a
	// name that is available)

	// try to move to TempPath if possible
	if (win32_temp_path_len != 0) {
		for (i = 0; i < NUM_ATTEMPT; i++) {
			snprintf(delpath, delpath_maxlen, "%s/%s.deleted-%i",
			        win32_temp_path, win32_basename(path), i);
			if (rename_file_handle(hnd, delpath) == 0)
				goto exit;

			if (GetLastError() == ERROR_NOT_SAME_DEVICE)
				break;
		}
	}

	// try to rename the file and postfix is as deleted
	for (i = 0; i < NUM_ATTEMPT; i++) {
		snprintf(delpath, delpath_maxlen, "%s.deleted-%i", path, i);
		if (rename_file_handle(hnd, delpath) == 0)
			break;
	}

exit:
	mm_freea(delpath);
	CloseHandle(hnd);
	return 0;
}


#define TOKEN_ACCESS 	\
  (  TOKEN_IMPERSONATE | TOKEN_QUERY        \
   | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ)
/*
from http://blog.aaronballman.com/2011/08/how-to-check-access-rights/
*/
static
int test_access(SECURITY_DESCRIPTOR* sd, DWORD access_rights)
{
	HANDLE proc_htoken = INVALID_HANDLE_VALUE;
	HANDLE imp_htoken = INVALID_HANDLE_VALUE;
	HANDLE hproc = GetCurrentProcess();
	GENERIC_MAPPING mapping;
	PRIVILEGE_SET privileges = {0};
	DWORD granted = 0, priv_len = sizeof(privileges);
	BOOL result = FALSE;
	int rv = -1;

	// Setup mapping to File generic rights
	mapping.GenericRead = FILE_GENERIC_READ;
	mapping.GenericWrite = FILE_GENERIC_WRITE;
	mapping.GenericExecute = FILE_GENERIC_EXECUTE;
	mapping.GenericAll = FILE_ALL_ACCESS;
	MapGenericMask(&access_rights, &mapping);

	// Get obtained a token suitable for impersonation and check access
	if (  !OpenProcessToken(hproc, TOKEN_ACCESS, &proc_htoken)
	   || !DuplicateToken(proc_htoken, SecurityImpersonation, &imp_htoken)
	   || !AccessCheck(sd, imp_htoken, access_rights, &mapping,
	                   &privileges, &priv_len, &granted, &result)  ) {
		goto exit;
	}

	rv =  (result == TRUE) ? 0 : EACCES;

exit:
	safe_closehandle(imp_htoken);
	safe_closehandle(proc_htoken);
	return rv;
}


/* doc in posix implementation */
API_EXPORTED
int mm_check_access(const char* path, int amode)
{
	struct local_secdesc lsd;
	int rv = -1;
	HANDLE hnd;
	DWORD w32err, access_rights;

	hnd = open_handle_for_metadata(path, false);
	if (hnd == INVALID_HANDLE_VALUE) {
		// If we receive file not found, this is not an error and
		// must be reported as return value
		w32err = GetLastError();
		if (  w32err == ERROR_PATH_NOT_FOUND
		   || w32err == ERROR_FILE_NOT_FOUND  )
			return ENOENT;

		return mm_raise_from_w32err("Can't get handle for %s", path);
	}

	// If only file presence is tested, we skip the access test
	if (amode == F_OK) {
		rv = 0;
		goto exit;
	}

	if (local_secdesc_init_from_handle(&lsd, hnd))
		goto exit;

	access_rights = 0;
	access_rights |= (amode & R_OK) ? GENERIC_READ : 0;
	access_rights |= (amode & W_OK) ? GENERIC_WRITE : 0;
	access_rights |= (amode & X_OK) ? GENERIC_EXECUTE : 0;
	rv = test_access(lsd.sd, access_rights);

	local_secdesc_deinit(&lsd);

exit:
	if (rv < 0)
		mm_raise_from_w32err("Failed to test access of %s", path);

	CloseHandle(hnd);
	return rv;
}


/* doc in posix implementation */
API_EXPORTED
int mm_link(const char* oldpath, const char* newpath)
{
	int oldpath_u16_len, newpath_u16_len;
	char16_t *oldpath_u16, *newpath_u16;
	int retval = 0;

	// Get the length (in byte) of the string when converted in UTF-8
	oldpath_u16_len = get_utf16_buffer_len_from_utf8(oldpath);
	newpath_u16_len = get_utf16_buffer_len_from_utf8(newpath);
	if (oldpath_u16_len < 0 || newpath_u16_len < 0)
		return mm_raise_from_w32err("invalid UTF-8 sequence");

	// temporary alloc of the UTF-16 string
	oldpath_u16 = mm_malloca(oldpath_u16_len*sizeof(*oldpath_u16));
	newpath_u16 = mm_malloca(newpath_u16_len*sizeof(*newpath_u16));

	// Do actual UTF-8 -> UTF-16 conversion
	conv_utf8_to_utf16(oldpath_u16, oldpath_u16_len, oldpath);
	conv_utf8_to_utf16(newpath_u16, newpath_u16_len, newpath);

	if (!CreateHardLinkW(newpath_u16, oldpath_u16, NULL)) {
		mm_raise_from_w32err("CreateHardLinkW(%s, %s) failed", newpath, oldpath);
		retval = -1;
	}

	mm_freea(newpath_u16);
	mm_freea(oldpath_u16);
	return retval;
}


/* doc in posix implementation */
API_EXPORTED
int mm_symlink(const char* oldpath, const char* newpath)
{
	int oldpath_u16_len, newpath_u16_len;
	char16_t *oldpath_u16, *newpath_u16;
	int retval = 0;
	int err;

	// Get the length (in byte) of the string when converted in UTF-8
	oldpath_u16_len = get_utf16_buffer_len_from_utf8(oldpath);
	newpath_u16_len = get_utf16_buffer_len_from_utf8(newpath);
	if (oldpath_u16_len < 0 || newpath_u16_len < 0)
		return mm_raise_from_w32err("invalid UTF-8 sequence");

	// Stack alloc the UTF-16 string
	oldpath_u16 = mm_malloca(oldpath_u16_len*sizeof(*oldpath_u16));
	newpath_u16 = mm_malloca(newpath_u16_len*sizeof(*newpath_u16));

	// Do actual UTF-8 -> UTF-16 conversion
	conv_utf8_to_utf16(oldpath_u16, oldpath_u16_len, oldpath);
	conv_utf8_to_utf16(newpath_u16, newpath_u16_len, newpath);


	err = CreateSymbolicLinkW(newpath_u16, oldpath_u16,
			SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
	if (!err && GetLastError() == ERROR_INVALID_PARAMETER) {
		/* SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE was introduced in 2017-03
		 * if it is not recognized, try again without it */
		err = CreateSymbolicLinkW(newpath_u16, oldpath_u16, 0);
	}
	if (!err) {
		mm_raise_from_w32err("CreateSymbolicLinkW(%s, %s) failed", newpath, oldpath);
		retval = -1;
	}

	mm_freea(newpath_u16);
	mm_freea(oldpath_u16);
	return retval;
}

/* doc in posix implementation */
API_EXPORTED
int mm_chdir(const char* path)
{
	int rv, path_u16_len;
	char16_t* path_u16;

	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0)
		return mm_raise_from_w32err("Invalid UTF-8 path");

	path_u16 = mm_malloca(path_u16_len * sizeof(*path_u16));
	if (path_u16 == NULL)
		return mm_raise_from_w32err("Failed to alloc required memory!");
	conv_utf8_to_utf16(path_u16, path_u16_len, path);

	rv = _wchdir(path_u16);

	mm_freea(path_u16);

	if (rv != 0)
		return mm_raise_from_errno("chdir(%s) failed", path);

	return rv;
}


/* doc in posix implementation */
API_EXPORTED
char* mm_getcwd(char* buf, size_t size)
{
	char* path = NULL;
	char16_t* path_u16;
	int path_u8_len, ret, path_u16_len;

	path_u16_len = 256;
	while (1) {
		path_u16 = malloc(path_u16_len*sizeof(*path_u16));
		if (!path_u16) {
			mm_raise_from_errno("Buffer allocation failed");
			return NULL;
		}

		// Try to get the copy the currdir to buffer (if buffer is NULL or
		// too short, the return value will be larger than provided
		// and will represent the needed size)
		ret = GetCurrentDirectoryW(path_u16_len, path_u16);
		if (ret == 0) {
			mm_raise_from_w32err("Failed to get current dir");
			goto exit;
		}

		if (ret < path_u16_len)
			break;

		path_u16_len = ret;
		free(path_u16);
	}

	path_u8_len = get_utf8_buffer_len_from_utf16(path_u16);

	// If buf argument is supplied, we must check that the size is
	// sufficient
	if (buf && (int)size < path_u8_len) {
		mm_raise_error(ERANGE, "Buffer too short for holding"
		                       " current directory path");
		goto exit;
	}

	// If buf argument is NULL, the buffer of necessary size must be
	// allocatted and returned
	if (!buf) {
		size = path_u8_len;
		buf = malloc(path_u8_len);
		if (!buf) {
			mm_raise_from_errno("Buffer allocation failed");
			goto exit;
		}
	}

	conv_utf16_to_utf8(buf, size, path_u16);
	path = buf;

exit:
	free(path_u16);
	return path;
}


/* doc in posix implementation */
API_EXPORTED
int mm_rmdir(const char* path)
{
	int rv, path_u16_len;
	char16_t* path_u16;

	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0)
		return mm_raise_from_w32err("Invalid UTF-8 path");

	path_u16 = mm_malloca(path_u16_len * sizeof(*path_u16));
	if (path_u16 == NULL)
		return mm_raise_from_w32err("Failed to alloc required memory!");
	conv_utf8_to_utf16(path_u16, path_u16_len, path);

	rv = _wrmdir(path_u16);

	mm_freea(path_u16);

	if (rv != 0)
		return mm_raise_from_errno("rmdir(%s) failed", path);

	return rv;
}

/*
 * Take care: FILE_ATTRIBUTE_ARCHIVE can be added to almost any type of file
 * here we only consider it if left alone, and then consider it a regular file
 */
static
int translate_filetype(DWORD type)
{
	if (type & FILE_ATTRIBUTE_DIRECTORY) {
		return MM_DT_DIR;
	} else if (type & FILE_ATTRIBUTE_REPARSE_POINT) {
		return MM_DT_LNK;
	} else if ((type & FILE_ATTRIBUTE_NORMAL)
			|| (type & FILE_ATTRIBUTE_ARCHIVE)) {
		return MM_DT_REG;
	} else {
		return MM_DT_UNKNOWN;
	}
}

static
int win32_unlinkat(const char * prefix, const char * name, int type)
{
	int rv;
	int len;
	char * path;

	/* path + "/" + name + "\0" */
	len = strlen(prefix) + 1 + strlen(name) + 1;
	path = mm_malloca(len);
	*path = '\0';
	strcat(path, prefix);
	strcat(path, "/");
	strcat(path, name);

	if (type == MM_DT_DIR)
		rv = mm_rmdir(path);
	else
		rv = mm_unlink(path);

	mm_freea(path);
	return rv;
}

#define RECURSION_MAX 100

/**
 * mm_remove_rec() - internal helper to recursively clean given folder
 * @prefix:        tracks the relative prefix path from the original callpoint
 * @d:             pointer to the current MMDIR structure to clean
 * @flags:         option flag to return on error
 * @rec_lvl:       maximum recursion level
 *
 * Many error return values are *explicitely* skipped.
 * Since this is a recursive removal, we should not stop when we encounter
 * a forbidden file or folder. This except if the @flag contains MM_FAILONERROR.
 *
 * Return: 0 on success, -1 on error
 */
static
int mm_remove_rec(const char * prefix, MMDIR * d, int flags, int rec_lvl)
{
	int rv, status;
	unsigned int type;
	MMDIR * newdir;
	const struct mm_dirent * dp;

	if (UNLIKELY(rec_lvl < 0))
		return mm_raise_error(EOVERFLOW, "Too many levels of recurion");

	while ((dp = mm_readdir(d, &status)) != NULL)
	{
		if (status != 0)
			return -1;

		type = d->dirent->type;
		if (type > 0 && (flags & type) == 0)
			continue;  // only consider filtered files

		/* skip "." and ".." directories */
		if (is_wildcard_directory(d->dirent->name))
			continue;

		/* try removing the file or folder */
		if (type == MM_DT_DIR) {
			/* remove the inside of the folder */
			int len;
			char * newdir_path;

			/* newdir_path + "/" + name + "\0" */
			len = strlen(prefix) + 1 + strlen(d->dirent->name) + 1;
			newdir_path = mm_malloca(len);
			if (newdir_path == NULL)
				return -1;
			*newdir_path = '\0';
			strcat(newdir_path, prefix);
			strcat(newdir_path, "/");
			strcat(newdir_path, d->dirent->name);
			newdir = mm_opendir(newdir_path);

			if (newdir == NULL) {
				if (flags & MM_FAILONERROR) {
					mm_freea(newdir_path);
					return -1;
				} else {
					continue;
				}
			}
			rv = mm_remove_rec(newdir_path, newdir, flags, rec_lvl - 1);
			mm_freea(newdir_path);
			mm_closedir(newdir);
			if (rv != 0 && (flags & MM_FAILONERROR))
				return -1;
		}

		rv = win32_unlinkat(prefix, d->dirent->name, type);
		if (rv != 0 && (flags & MM_FAILONERROR))
			return -1;
	}

	if (GetLastError() ==  ERROR_NO_MORE_FILES)
		return 0;

	return -1;
}

/* doc in posix implementation */
API_EXPORTED
int mm_remove(const char* path, int flags)
{
	int rv, error_flags;
	MMDIR * dir;
	int type = -1;
	DWORD attrs;

	attrs = GetFileAttributes(path);
	if (attrs != INVALID_FILE_ATTRIBUTES)
		type = translate_filetype(attrs);

	if (type < 0)
		return mm_raise_from_w32err("unable to get %s filetype", path);

	if (flags & MM_RECURSIVE) {
		flags |= MM_DT_DIR;
		if (type == MM_DT_DIR) {
			dir = mm_opendir(path);

			error_flags = mm_error_set_flags(MM_ERROR_NOLOG, MM_ERROR_NOLOG);
			rv = mm_remove_rec(path, dir, flags, RECURSION_MAX);
			mm_error_set_flags(error_flags, MM_ERROR_ALL);
			mm_closedir(dir);
			if (rv != 0 && !(flags & MM_FAILONERROR)) {
				return mm_raise_from_errno("recursive mm_remove(%s) failed", path);
			}

			/* allow rmdir(".") when called with the recursive flag only */
			if (is_wildcard_directory(path))
				return rv;
		}
	}

	if ((flags & type) == 0)
		return mm_raise_error(EPERM, "failed to remove %s: "
				"invalid type", path);

	if (type == MM_DT_DIR)
		return mm_rmdir(path);
	else
		return mm_unlink(path);
}

/**
 * win32_find_file() - helper to handle the conversion between utf8 and 16
 * @dir:    pointer to a MMDIR structure
 *
 * NOTE: windows Prototype are:
 *   HANDLE FindFirstFileW(path, ...)
 *   bool FindFirstFileW(HANDLE, ...)
 *
 * This function hides the difference by always returning a HANDLE like FindFirstFile()
 * and storing the HANDLE and path within the MMDIR structure.
 *
 * Return: 0 on success, -1 on error
 * The caller should call GetLastError() and investigate the issue itself
 *
 */
static
int win32_find_file(MMDIR * dir)
{
	size_t reclen, namelen;
	int path_u16_len;
	char16_t* path_u16;
	WIN32_FIND_DATAW find_dataw;
	HANDLE hdir;

	if (dir == NULL) {
		mm_raise_error(EINVAL, "Does not accept NULL arguments");
		return -1;
	}

	path_u16_len = get_utf16_buffer_len_from_utf8(dir->dirname);
	if (path_u16_len < 0) {
		 mm_raise_from_w32err("Invalid UTF-8 path");
		return -1;
	}

	path_u16 = mm_malloca(path_u16_len * sizeof(*path_u16));
	if (path_u16 == NULL) {
		mm_raise_from_w32err("Failed to alloc required memory!");
		return -1;
	}
	conv_utf8_to_utf16(path_u16, path_u16_len, dir->dirname);

	if (!dir->find_first_done) {
		hdir = dir->hdir = FindFirstFileW(path_u16, &find_dataw);
		dir->find_first_done = 1;
	} else {
		if(!FindNextFileW(dir->hdir, &find_dataw))
			hdir = INVALID_HANDLE_VALUE;
		else
			hdir = dir->hdir;
	}

	mm_freea(path_u16);

	/* do not copy the result to the dirent structure
	 * let the caller check the errors */
	if (hdir == INVALID_HANDLE_VALUE)
		return -1;

	namelen = get_utf8_buffer_len_from_utf16(find_dataw.cFileName);
	reclen = sizeof(*dir->dirent) + namelen;
	if (dir->dirent == NULL || dir->dirent->reclen != reclen) {
		void * tmp = realloc(dir->dirent, reclen);
		if (tmp == NULL) {
			mm_raise_from_errno("win32_find_file() failed to alloc the required memory");
			return -1;
		}
		dir->dirent = tmp;
		dir->dirent->reclen = reclen;
	}
	dir->dirent->type = translate_filetype(find_dataw.dwFileAttributes);
	conv_utf16_to_utf8(dir->dirent->name, namelen, find_dataw.cFileName);

	return 0;
}

/* doc in posix implementation */
API_EXPORTED
MMDIR * mm_opendir(const char* path)
{
	int len;
	MMDIR * d;

	len = strlen(path) + 3;  // concat with "/*\0"
	d = malloc(sizeof(*d) + len);
	if (d == NULL)
		goto error;
	
	*d = (MMDIR) { .hdir = INVALID_HANDLE_VALUE };
	strncpy(d->dirname, path, len);
	strcat(d->dirname, "/*");

	/* call FindFirstFile() to ensure that the given path
	 * is meaningfull, and to keep ithe folder opened */
	if(win32_find_file(d))
		goto error;

	return d;

error:
	mm_raise_from_errno("opendir(%s) failed", path);
	if (d != NULL) {
		free(d->dirname);
		free(d->dirent);
	}
	free(d);
	return NULL;
}

/* doc in posix implementation */
API_EXPORTED
void mm_closedir(MMDIR* dir)
{
	if (dir == NULL)
		return;

	FindClose(dir->hdir);
	free(dir->dirent);
	free(dir);
}

/* doc in posix implementation */
API_EXPORTED
void mm_rewinddir(MMDIR* dir)
{
	if (dir == NULL) {
		mm_raise_error(EINVAL, "Does not accept NULL arguments");
		return;
	}

	FindClose(dir->hdir);
	win32_find_file (dir);
}

/* doc in posix implementation */
API_EXPORTED
const struct mm_dirent* mm_readdir(MMDIR* d, int * status)
{
	if (d == NULL) {
		if (status != NULL)
			*status = -1;
		mm_raise_error(EINVAL, "Does not accept NULL arguments");
		return NULL;
	}

	if (status != NULL)
		*status = 0;

	if (win32_find_file(d)) {
		if (GetLastError() ==  ERROR_NO_MORE_FILES)
			return NULL;

		if (status != NULL)
			*status = -1;
		mm_raise_from_errno("readdir() failed to find next file");
		return NULL;
	}

	return d->dirent;
}


/*************************************************************************
 *                                                                       *
 *                   stat like function implementation                   *
 *                                                                       *
 *************************************************************************/

/**
 * reparse_data_create() - allocate and get reparse data from handle
 * HANDLE:      handle of a open reparse point
 *
 * Return: initialized REPARSE_DATA_BUFFER in case of success, NULL
 * othewise. Must be cleanup with free() when you don't need it any longer
 */
static
REPARSE_DATA_BUFFER* get_reparse_data(HANDLE hnd)
{
	REPARSE_DATA_BUFFER* rep;
	DWORD io_retsz;

	rep = malloc(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
	if (!rep)
		return NULL;

	// Get reparse point data
	if (!DeviceIoControl(hnd, FSCTL_GET_REPARSE_POINT, NULL, 0,
	                     rep, MAXIMUM_REPARSE_DATA_BUFFER_SIZE,
			     &io_retsz, NULL)) {
		free(rep);
		return NULL;
	}

	return rep;
}


/**
 * get_target_u16_from_reparse_data() - extract symlink target from data
 * rep:         reparse point data read from a symlink
 *
 * Return: UTF-16 string (null terminated) of the target if @rep is the
 * reparse point data of a symlink, NULL otherwise. The return pointer is
 * valid for the lifetime of @rep.
 */
static
char16_t* get_target_u16_from_reparse_data(REPARSE_DATA_BUFFER* rep)
{
	char* buf8;
	char16_t * trgt_u16;
	int trgt_u16_len, buf8_len, buf8_offset;

	if (rep->ReparseTag != IO_REPARSE_TAG_SYMLINK)
		return NULL;

	// Get UTF-16 path of target
	buf8 = (char*)rep->SymbolicLinkReparseBuffer.PathBuffer;
	buf8_offset = rep->SymbolicLinkReparseBuffer.SubstituteNameOffset;
	buf8_len = rep->SymbolicLinkReparseBuffer.SubstituteNameLength;
	trgt_u16 = (char16_t*)(buf8 + buf8_offset);
	trgt_u16_len = buf8_len / sizeof(*trgt_u16);
	trgt_u16[trgt_u16_len] = L'\0';

	return trgt_u16;
}


/**
 * get_symlink_target_strlen() - Get size of target string of symlink
 * hnd:         handle of a open symlink
 *
 * Return: size of the UTF-8 string of the target including null
 * terminator.
 */
static
size_t get_symlink_target_strlen(HANDLE hnd)
{
	REPARSE_DATA_BUFFER* rep;
	char16_t * trgt_u16;
	size_t len = 0;

	rep = get_reparse_data(hnd);
	if (!rep)
		return 0;

	trgt_u16 = get_target_u16_from_reparse_data(rep);
	if (trgt_u16)
		len = get_utf8_buffer_len_from_utf16(trgt_u16);

	free(rep);
	return len;
}


/**
 * get_stat_from_handle() - fill stat info from opened file handle
 * @hnd:        file handle
 * @buf:        stat info to fill
 *
 * Return: 0 in case of success, -1 otherwise. BE CAREFUL, this function
 * does not set error state. Only win32 last error is set. It is expected
 * to raise the corresponding in the caller (which will have more context)
 */
static
int get_stat_from_handle(HANDLE hnd, struct mm_stat* buf)
{
	FILE_ATTRIBUTE_TAG_INFO attr_tag;
	FILE_ID_INFO id_info;
	BY_HANDLE_FILE_INFORMATION info;
	struct local_secdesc lsd;
	int type;

	if (  !GetFileInformationByHandleEx(hnd, FileAttributeTagInfo,
	                                    &attr_tag, sizeof(attr_tag))
	   || !GetFileInformationByHandleEx(hnd, FileIdInfo,
	                                    &id_info, sizeof(id_info))
	   || !GetFileInformationByHandle(hnd, &info)
	   || local_secdesc_init_from_handle(&lsd, hnd)  ) {
		return -1;
	}

	// translate_filetype() consider all reparse point as symlink. Here
	// we can use dwReserved0 field to distinguish type
	if (attr_tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		switch(attr_tag.ReparseTag) {
		case IO_REPARSE_TAG_SYMLINK:     type = S_IFLNK; break;
		case IO_REPARSE_TAG_MOUNT_POINT: type = S_IFDIR; break;
		default:                         type = S_IFREG; break;
		}
	} else {
		switch(translate_filetype(attr_tag.FileAttributes)) {
		case MM_DT_DIR:  type = S_IFDIR; break;
		case MM_DT_LNK:  type = S_IFLNK; break;
		case MM_DT_FIFO: type = S_IFIFO; break;
		case MM_DT_CHR:  type = S_IFCHR; break;
		default:         type = S_IFREG; break;
		}
	}

	buf->mode = type | local_secdesc_get_mode(&lsd);
	buf->nlink = info.nNumberOfLinks;

	if (type == S_IFLNK) {
		buf->size = get_symlink_target_strlen(hnd);
	} else {
		buf->size = ((mm_off_t)info.nFileSizeHigh) * MAXDWORD;
		buf->size += info.nFileSizeLow;
	}

	buf->ctime = filetime_to_time(info.ftCreationTime);
	buf->mtime = filetime_to_time(info.ftLastWriteTime);
	buf->dev = id_info.VolumeSerialNumber;
	memcpy(&buf->ino, &id_info.FileId, sizeof(buf->ino));

	local_secdesc_deinit(&lsd);
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mm_stat(const char* path, struct mm_stat* buf, int flags)
{
	HANDLE hnd;
	int rv = 0;

	hnd = open_handle_for_metadata(path, flags & MM_NOFOLLOW);
	if (hnd == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("Can't open %s", path);

	// Get stat info from open file handle
	if (get_stat_from_handle(hnd, buf))
		rv = mm_raise_from_w32err("Can't get stat of %s", path);

	CloseHandle(hnd);
	return rv;
}


/* doc in posix implementation */
API_EXPORTED
int mm_fstat(int fd, struct mm_stat* buf)
{
	HANDLE hnd;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	if (get_stat_from_handle(hnd, buf))
		return mm_raise_from_w32err("Can't get stat of fd=%i", fd);

	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mm_readlink(const char* path, char* buf, size_t bufsize)
{
	REPARSE_DATA_BUFFER* rep;
	HANDLE hnd = INVALID_HANDLE_VALUE;
	char16_t* trgt_u16;
	int rv = 0;

	hnd = open_handle_for_metadata(path, true);
	if (hnd == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("Can't open %s", path);

	// Get reparse point data
	rep = get_reparse_data(hnd);
	if (!rep) {
		rv = mm_raise_from_w32err("Can't get data of %s", path);
		goto exit;
	}

	// Extract target string from reparse point data
	trgt_u16 = get_target_u16_from_reparse_data(rep);
	if (!trgt_u16) {
		rv = mm_raise_error(EINVAL, "%s is not a symlink", path);
		goto exit;
	}

	// Try convert symlink target UTF-16->UTF-8
	if (conv_utf16_to_utf8(buf, bufsize, trgt_u16) < 0)
		rv = mm_raise_error(EOVERFLOW, "target too large");

exit:
	free(rep);
	CloseHandle(hnd);
	return rv;
}

