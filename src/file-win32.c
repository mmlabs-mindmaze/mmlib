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
#include "utils-win32.h"
#include <windows.h>
#include <malloc.h>
#include <stdio.h>

#define NUM_ATTEMPT	256


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

	if (set_w32_create_file_options(&opts, oflag))
		return -1;

	hnd = CreateFile(path, opts.access_mode, opts.share_flags, NULL,
	                 opts.creation_mode, opts.file_attribute, NULL);
	if (hnd == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("CreateFile(%s) failed", path);

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

	set_fd_info(fd, FD_TYPE_MSVCRT);
	return 0;
}


API_EXPORTED
ssize_t mm_read(int fd, void* buf, size_t nbyte)
{
	ssize_t rsz;
	int fd_info;

	fd_info = get_fd_info_checked(fd);
	if (fd_info < 0)
		return mm_raise_error(EBADF, "Invalid file descriptor");

	switch(fd_info & FD_TYPE_MASK) {

	case FD_TYPE_NORMAL:
	case FD_TYPE_PIPE:
		rsz = mmlib_read(fd, buf, nbyte);
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
	int fd_info;

	fd_info = get_fd_info_checked(fd);
	if (fd_info < 0)
		return mm_raise_error(EBADF, "Invalid file descriptor");

	switch(fd_info & FD_TYPE_MASK) {

	case FD_TYPE_NORMAL:
	case FD_TYPE_PIPE:
		rsz = mmlib_write(fd, buf, nbyte);
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


API_EXPORTED
int mm_unlink(const char* path)
{
	int i;
	HANDLE hnd;
	char rename_info_buffer[sizeof(FILE_RENAME_INFO)+sizeof(WCHAR)*MAX_PATH];
	char name_info_buffer[sizeof(FILE_NAME_INFO)+sizeof(WCHAR)*MAX_PATH];
	FILE_RENAME_INFO* rename_info = (FILE_RENAME_INFO*)rename_info_buffer;
	FILE_NAME_INFO* name_info = (FILE_NAME_INFO*)name_info_buffer;

	rename_info->ReplaceIfExists = FALSE;
	rename_info->RootDirectory = NULL;

	// Open file with DELETE_ON_CLOSE flag
	hnd = CreateFile(path, DELETE, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			 NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (hnd == INVALID_HANDLE_VALUE) {
		return mm_raise_from_w32err("Cannot access %s for deletion", path);
	}

	// Get filename in Unicode
	if (!GetFileInformationByHandleEx(hnd, FileNameInfo,
	                                  name_info, sizeof(name_info_buffer))) {
		goto exit;
	}

	// Rename before closing (hence marking file for deleting) to make the
	// filename reusable immediately even if the file object is not deleted
	// immediately
	for (i = 0; i < NUM_ATTEMPT; i++) {
		rename_info->FileNameLength = swprintf(rename_info->FileName, MAX_PATH,
		                                       L"%s.deleted-%i", name_info->FileName, i);

		if (SetFileInformationByHandle(hnd, FileRenameInfo,
		                               rename_info, sizeof(rename_info_buffer))) {
			break;
		}
	}

exit:
	CloseHandle(hnd);
	return 0;
}


API_EXPORTED
int mm_check_access(const char* path, int amode)
{
	WIN32_FILE_ATTRIBUTE_DATA attrs;
	DWORD bin_type, w32err;

	// TODO: Implement using GetFileSecurity()

	if (!GetFileAttributesEx(path, GetFileExInfoStandard, &attrs)) {
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
	if (!CreateHardLink(newpath, oldpath, NULL)) {
		mm_raise_from_w32err("CreateHardLink(%s, %s) failed", newpath, oldpath);
		return -1;
	}

	return 0;
}


API_EXPORTED
int mm_symlink(const char* oldpath, const char* newpath)
{
	if (!CreateSymbolicLink(newpath, oldpath, 0)) {
		mm_raise_from_w32err("CreateSymbolicLink(%s, %s) failed", newpath, oldpath);
		return -1;
	}

	return 0;
}


