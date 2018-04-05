/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include "mmsysio.h"
#include "mmerrno.h"
#include "mmlib.h"
#include "utils-win32.h"
#include <windows.h>
#include <stdio.h>
#include <uchar.h>

#define NUM_ATTEMPT	256

static int win32_temp_path_len;
static char16_t win32_temp_path[(MAX_PATH + 1)];

MM_CONSTRUCTOR(win32_temp_path)
{
	int rv;
	rv = GetTempPathW(MAX_PATH, win32_temp_path);
	if (rv != 0 && rv < MAX_PATH) {
		win32_temp_path_len = wcslen(win32_temp_path);
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


API_EXPORTED
int mm_open(const char* path, int oflag, int mode)
{
	(void)mode;
	HANDLE hnd;
	int fd, fdinfo;
	struct w32_create_file_options opts;
	int path_u16_len;
	char16_t* path_u16;

	if (set_w32_create_file_options(&opts, oflag))
		return -1;

	// Get size for converted path into UTF-16
	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0)
		return mm_raise_from_w32err("Invalid UTF-8 path");

	// Create temporary UTF-16 path and use to create the file handle
	path_u16 = mm_malloca(path_u16_len*sizeof(*path_u16));
	conv_utf8_to_utf16(path_u16, path_u16_len, path);
	hnd = CreateFileW(path_u16, opts.access_mode, opts.share_flags, NULL,
	                  opts.creation_mode, opts.file_attribute, NULL);
	mm_freea(path_u16);

	if (hnd == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("CreateFileW(%s) failed", path);

	fdinfo = FD_TYPE_NORMAL;
	if (mode & O_APPEND)
		fdinfo |= FD_FLAG_APPEND;

	if (wrap_handle_into_fd(hnd, &fd, fdinfo)) {
		CloseHandle(hnd);
		return -1;
	}

	return fd;
}


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


API_EXPORTED
int mm_dup2(int fd, int newfd)
{
	if (_dup2(fd, newfd) < 0)
		return mm_raise_from_errno("_dup2(%i, %i) failed", fd, newfd);

	set_fd_info(newfd, get_fd_info(fd));
	return newfd;
}


API_EXPORTED
int mm_pipe(int pipefd[2])
{
	HANDLE wr_hnd, rd_hnd;

	if (!CreatePipe(&rd_hnd, &wr_hnd, NULL, 64*4096))
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

static inline
const char16_t * win32_basename(const char16_t * path)
{
	const char16_t * c = path + wcslen(path) - 1;

	/* skip the last chars if they're not a path */
	while (c > path && (*c == L'/' || *c == L'\\'))
		c--;

	while (c > path) {
		if (*c == L'/' || *c == L'\\')
			return c + 1;
		c--;
	}
	return path;
}


API_EXPORTED
int mm_unlink(const char* path)
{
	int i, path_u16_len, delpath_maxlen, len, exit_value;
	size_t rename_info_size;
	HANDLE hnd;
	FILE_RENAME_INFO* rename_info;
	char16_t* path_u16, *delpath;

	// Get size for converted path into utf-16
	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0)
		return mm_raise_from_w32err("Invalid UTF-8 path");

	// Create temporary UTF-16 string from UTF-8 path and open the file
	// with the DELETE_ON_CLOSE flag
	path_u16_len += win32_temp_path_len;
	path_u16 = mm_malloca(path_u16_len*sizeof(*path_u16));
	conv_utf8_to_utf16(path_u16, path_u16_len, path);
	hnd = CreateFileW(path_u16, DELETE,
	                  FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
	                  NULL, OPEN_EXISTING,
	                  FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (hnd == INVALID_HANDLE_VALUE) {
		mm_freea(path_u16);
		return mm_raise_from_w32err("Cannot access %s for deletion", path);
	}

	// Allocate (and init) temporary rename info structure in order to rename path
	// into <path>.deleted-####
	delpath_maxlen = path_u16_len+64;
	rename_info_size = sizeof(*rename_info);
	rename_info_size += delpath_maxlen*sizeof(*rename_info->FileName);
	rename_info = mm_malloca(rename_info_size);
	rename_info->RootDirectory = NULL;
	rename_info->ReplaceIfExists = FALSE;
	delpath  = rename_info->FileName;

	// Rename before closing (hence marking file for deleting) to make the
	// filename reusable immediately even if the file object is not deleted
	// immediately (make different rename attempt in order to find a
	// name that is available)

	exit_value = 0;
	// try to move to TempPath if possible
	if (win32_temp_path_len != 0) {
		for (i = 0; i < NUM_ATTEMPT; i++) {
			len = swprintf(delpath, delpath_maxlen, L"%s/%s.deleted-%i",
			               win32_temp_path, win32_basename(path_u16), i);
			rename_info->FileNameLength = len*sizeof(*delpath);

			if (SetFileInformationByHandle(hnd, FileRenameInfo,
						rename_info, rename_info_size)) {
				goto exit;
			}
			if (GetLastError() == ERROR_NOT_SAME_DEVICE)
				break;
		}
	}

	// try to rename the file and postfix is as deleted
	for (i = 0; i < NUM_ATTEMPT; i++) {
		len = swprintf(delpath, delpath_maxlen, L"%s.deleted-%i", path_u16, i);
		rename_info->FileNameLength = len*sizeof(*delpath);

		if (SetFileInformationByHandle(hnd, FileRenameInfo,
		                               rename_info, rename_info_size)) {
			goto exit;
		}
	}

	exit_value = -1;
exit:
	mm_freea(path_u16);
	mm_freea(rename_info);

	CloseHandle(hnd);
	return exit_value;
}


API_EXPORTED
int mm_check_access(const char* path, int amode)
{
	WIN32_FILE_ATTRIBUTE_DATA attrs;
	DWORD bin_type, w32err;
	int path_u16_len;
	char16_t* path_u16;
	BOOL success;

	// Get size for converted path into utf-16
	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0)
		return mm_raise_from_w32err("Invalid UTF-8 path");

	// TODO: Implement using GetFileSecurity()

	// Create temporary UTF-16 string from path get attribute from it
	path_u16 = mm_malloca(path_u16_len*sizeof(*path_u16));
	conv_utf8_to_utf16(path_u16, path_u16_len, path);
	success = GetFileAttributesExW(path_u16, GetFileExInfoStandard, &attrs);
	mm_freea(path_u16);

	if (!success) {
		// If file cannot be found, this not an actual error (hence
		// do not set error state)
		w32err = GetLastError();
		if (  (w32err == ERROR_FILE_NOT_FOUND)
		   || (w32err == ERROR_PATH_NOT_FOUND)  )
			return ENOENT;

		return mm_raise_from_w32err("Failed to get file attributes (%s)", path);
	}

	// Check for write access
	if (amode & W_OK) {
		if (attrs.dwFileAttributes == FILE_ATTRIBUTE_READONLY)
			return EACCES;
	}

	// Check for Execution access
	if (amode & X_OK) {
		if (!GetBinaryType(path, &bin_type))
			return EACCES;
	}

	return 0;
}


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


API_EXPORTED
int mm_symlink(const char* oldpath, const char* newpath)
{
	int oldpath_u16_len, newpath_u16_len;
	char16_t *oldpath_u16, *newpath_u16;
	int retval = 0;

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

	if (!CreateSymbolicLinkW(newpath_u16, oldpath_u16, 0)) {
		mm_raise_from_w32err("CreateSymbolicLinkW(%s, %s) failed", newpath, oldpath);
		retval = -1;
	}

	mm_freea(newpath_u16);
	mm_freea(oldpath_u16);
	return retval;
}


