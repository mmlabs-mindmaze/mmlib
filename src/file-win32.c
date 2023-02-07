/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmsysio.h"
#include "mmerrno.h"
#include "mmlib.h"
#include "file-internal.h"
#include "local-ipc-win32.h"
#include "socket-win32.h"
#include "utils-win32.h"
#include "volume-win32.h"
#include "mmlog.h"

#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdbool.h>
#include <uchar.h>
#include <winnt.h>
#include <ntdef.h>
#include <winioctl.h>

#define NUM_ATTEMPT     256

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x02
#endif

struct mm_dirstream {
	HANDLE hdir;
	int find_first_done;       // has folder been though FindFirstFile()?
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
 * @hnd:        handle on which to read
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
ssize_t mmlib_read(HANDLE hnd, void* buf, size_t nbyte)
{
	DWORD read_sz;

	if (!ReadFile(hnd, buf, nbyte, &read_sz, NULL)) {
		// If write end is closed and pipe is empty, this is not an
		// error. A success must be returned with 0 byte read.
		if (GetLastError() == ERROR_BROKEN_PIPE)
			return 0;

		return -1;
	}

	return read_sz;
}


/**
 * mmlib_write() - perform a write operation using local implementation
 * @hnd:        handle on which to write
 * @buf:        buffer to hold the data to write
 * @nbyte:      number of byte to write
 *
 * Perform write assuming file type on which WriteFile() is possible without
 * passing special flags or not requiring transforming the data.
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned
 * and error state is set accordingly.
 */
static
ssize_t mmlib_write(HANDLE hnd, const void* buf, size_t nbyte)
{
	DWORD written_sz;

	if (!WriteFile(hnd, buf, nbyte, &written_sz, NULL))
		return -1;

	return written_sz;
}


/**
 * console_read() - read UTF-8 from console
 * @hnd:        console handle
 * @buf:	buffer that should hold the console input
 * @nbyte:      size of @buf
 *
 * Return: In case of success, a non negative number corresponding to the
 * number of the byte read. -1 in case of failure with error state set
 */
static
ssize_t console_read(HANDLE hnd, char* buf, size_t nbyte)
{
	DWORD nchar16_read;
	size_t nchar16;
	char16_t* buf16;
	int rsz = -1;

	// Allocate supplementary buffer to hold UTF-16 data from console
	nchar16 = nbyte / sizeof(char16_t);
	buf16 = mm_malloca(nchar16*sizeof(char16_t));

	// Read console input in UTF-16
	if (!ReadConsoleW(hnd, buf16, nchar16, &nchar16_read, NULL))
		goto exit;

	// Convert in console data in UTF-8
	rsz = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
	                          buf16, nchar16_read, buf, nbyte,
	                          NULL, NULL);
exit:
	mm_freea(buf16);
	return rsz;
}


/**
 * console_write() - write UTF-8 to console
 * @hnd:        handle on which to write
 * @buf:        UTF-8 string to be written on console
 * @nbyte:      size of @buf
 *
 * Return: In case of success, a non negative number corresponding to the
 * number of the byte written. -1 in case of failure with error state set
 */
static
ssize_t console_write(HANDLE hnd, const char* buf, size_t nbyte)
{
	DWORD nchar16_written;
	int nchar16;
	char16_t* buf16 = NULL;
	ssize_t rsz = -1;

	// Allocate temporary buffers
	buf16 = mm_malloca(2*nbyte*sizeof(*buf16));

	// Convert UTF-8 into UTF-16
	nchar16 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                              buf, nbyte, buf16, 2*nbyte);
	if (nchar16 < 0)
		goto exit;

	// Write the UTF-16 sequence to console
	if (!WriteConsoleW(hnd, buf16, nchar16, &nchar16_written, NULL))
		goto exit;

	// Convert the number of UTF-16 code unit written into number of bytes
	// in the original UTF-8 sequence
	rsz = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
	                          buf16, nchar16_written, NULL, -1,
	                          NULL, NULL);

exit:
	mm_freea(buf16);
	return rsz;
}


/**
 * hnd_write_binary() - perform a binary write operation on handle
 * @hnd:        handle on which to write
 * @fd_info:    file desriptor mmlib info
 * @buf:        buffer to hold the data to write
 * @nbyte:      number of byte to write
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned
 * and error state is set accordingly.
 */
static
ssize_t hnd_write_binary(HANDLE hnd, int fd_info, const void* buf, size_t nbyte)
{
	switch (fd_info & FD_TYPE_MASK) {

	case FD_TYPE_NORMAL:
	case FD_TYPE_PIPE:
		return mmlib_write(hnd, buf, nbyte);

	case FD_TYPE_CONSOLE:
		return console_write(hnd, buf, nbyte);

	case FD_TYPE_SOCKET:
		return sock_hnd_write(hnd, buf, nbyte);

	case FD_TYPE_IPCDGRAM:
		return ipc_hnd_write(hnd, buf, nbyte);

	default:
		abort();
	}
}


/**
 * hnd_write_binary() - perform a binary write operation on handle
 * @hnd:        handle on which to write
 * @fd_info:    file desriptor mmlib info
 * @buf:        buffer to hold the data to write
 * @nbyte:      number of byte to write
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned
 * and error state is set accordingly.
 */
static
ssize_t hnd_write_text(HANDLE hnd, int fd_info, const char* buf, size_t sz)
{
	char* buf_crlf = NULL;
	int i, len_crlf;
	ssize_t rsz;

	buf_crlf = mm_malloca(2*sz);
	if (!buf_crlf)
		return -1;

	// Copy buf to buf_crlf with LF -> CRLF expansion
	for (i = 0, len_crlf = 0; i < (int)sz; i++) {
		if (buf[i] == '\n')
			buf_crlf[len_crlf++] = '\r';

		buf_crlf[len_crlf++] = buf[i];
	}

	rsz = hnd_write_binary(hnd, fd_info, buf_crlf, len_crlf);

	// Remove from count the inserted CR in each CRLF. This way, we really
	// have the size of the part of @buf which has been written
	for (i = 1; i < len_crlf; i++) {
		if (buf_crlf[i-1] == '\r' && buf_crlf[i] == '\n')
			rsz--;
	}

	mm_freea(buf_crlf);
	return rsz;
}


/**
 * hnd_write() - perform a generic write operation on handle
 * @hnd:        handle on which to write
 * @fd_info:    file desriptor info
 * @buf:        buffer to hold the data to write
 * @sz:         number of byte to write
 *
 * This write operation switch between text or binary mode depending on content pointed to by @pinfo.
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned
 * and error state is set accordingly.
 */
static
ssize_t hnd_write(HANDLE hnd, int fd_info, const void* buf, size_t sz)
{
	if ((fd_info & FD_FLAG_TEXT))
		return hnd_write_text(hnd, fd_info, buf, sz);

	return hnd_write_binary(hnd, fd_info, buf, sz);
}


/**
 * hnd_read_binary() - perform a binary read operation on handle
 * @hnd:        handle on which to read
 * @fd_info:    file desriptor mmlib info
 * @buf:        buffer to hold the data to read
 * @nbyte:      number of byte to read
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned
 * and error state is set accordingly.
 */
static
ssize_t hnd_read_binary(HANDLE hnd, int fd_info, void* buf, size_t nbyte)
{
	switch (fd_info & FD_TYPE_MASK) {

	case FD_TYPE_NORMAL:
	case FD_TYPE_PIPE:
		return mmlib_read(hnd, buf, nbyte);

	case FD_TYPE_CONSOLE:
		return console_read(hnd, buf, nbyte);

	case FD_TYPE_SOCKET:
		return sock_hnd_read(hnd, buf, nbyte);

	case FD_TYPE_IPCDGRAM:
		return ipc_hnd_read(hnd, buf, nbyte);

	default:
		abort();
	}
}


static
ssize_t conv_crlf_to_lf(int* pinfo, char* buf, ssize_t rsz, size_t maxsz)
{
	int fd_info = *pinfo;
	char prev_ch, curr_ch;
	char* dst = buf;
	const char* src = buf;
	const char* src_end = src + rsz;

	// Beginning of CRLF -> LF depends whether a CR is carried.
	if (fd_info & FD_FLAG_CARRY) {
		prev_ch = fd_info_get_carry_char(fd_info);
		*pinfo = fd_info = fd_info_drop_carry(fd_info);
	} else {
		prev_ch = *src++;
	}

	// Perform CRLF -> LF conversion
	for (; src < src_end; prev_ch = curr_ch, src++) {
		curr_ch = *src;
		if ((prev_ch == '\r') && (curr_ch == '\n'))
			continue;

		*dst++ = prev_ch;
	}

	if ((prev_ch == '\r') || (dst == buf + maxsz))
		*pinfo = fd_info_set_carry_char(fd_info, prev_ch);
	else
		*dst++ = prev_ch;

	return dst - buf;
}


/**
 * hnd_read() - perform a generic read operation on handle
 * @hnd:        handle on which to read
 * @fd_info:    file desriptor mmlib info
 * @buf:        buffer to hold the data to read
 * @nbyte:      number of byte to read
 *
 * Return: number of byte written in case of success. Otherwise -1 is returned
 * and error state is set accordingly.
 */
static
ssize_t hnd_read(HANDLE hnd, int* restrict pinfo, void* buf, size_t nbyte)
{
	int fd_info = *pinfo;
	ssize_t rsz;

	rsz = hnd_read_binary(hnd, fd_info, buf, nbyte);
	if (rsz <= 0)
		return rsz;

	if (fd_info & FD_FLAG_TEXT)
		rsz = conv_crlf_to_lf(pinfo, buf, rsz, nbyte);

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
	                  lsd.sd, opts.file_attribute);
	if (hnd == INVALID_HANDLE_VALUE) {
		mm_raise_from_w32err("Can't get handle for %s", path);
		goto exit;
	}

	fdinfo = FD_TYPE_NORMAL;
	if (oflag & O_APPEND)
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
	HANDLE hnd;
	int fd_info;
	ssize_t rsz;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	fd_info = get_fd_info_checked(fd);
	if (fd_info < 0)
		return mm_raise_error(EBADF, "Invalid file descriptor: %i", fd);

	rsz = hnd_read(hnd, &fd_info, buf, nbyte);
	if (rsz < 0)
		mm_raise_from_w32err("reading from fd=%i failed", fd);

	return rsz;
}


/* doc in posix implementation */
API_EXPORTED
ssize_t mm_write(int fd, const void* buf, size_t nbyte)
{
	HANDLE hnd;
	int fd_info;
	ssize_t rsz;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	fd_info = get_fd_info_checked(fd);
	if (fd_info < 0)
		return mm_raise_error(EBADF, "Invalid file descriptor: %i", fd);

	// If file is opened in append mode, we must reset file pointer to
	// the end.
	if (fd_info & FD_FLAG_APPEND) {
		if (!SetFilePointer(hnd, 0, NULL, FILE_END)) {
			mm_raise_from_w32err("handle is opened in append mode "
			                     "but can't seek file end");
			return -1;
		}
	}

	rsz = hnd_write(hnd, fd_info, buf, nbyte);
	if (rsz < 0)
		mm_raise_from_w32err("writing to fd=%i failed", fd);

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
		mm_raise_error(EBADF, "Invalid file descriptor (%i)", fd);
		return -1;
	}

	return (info & FD_FLAG_ISATTY) == FD_FLAG_ISATTY;
}


/**
 * scrub_user_trash_path() - attempt to clean remainings of deleted files
 * @trash_buffer:       Buffer to use to prepare full path of file to be
 *                      deleted. It must be initialized with the trash prefix
 *                      to use to search for file delete by mmlib. Allocated
 *                      size must be at least MAX_PATH+1 long.
 * @prefix_len:         length of prefix in @trash_buffer (excluding '\0')
 *
 * NOTE: This function assumes this is legal to write on @trash_prefix between
 * index @prefix_len and MAX_PATH+1
 */
static
void scrub_user_trash_path(char16_t* trash_buffer, int prefix_len)
{
	WIN32_FIND_DATAW find_data;
	HANDLE find_hnd;

	// Start search file matching .mmlib*.deleted in prefix
	wcscpy(trash_buffer + prefix_len, L".mmlib-*.deleted");
	find_hnd = FindFirstFileW(trash_buffer, &find_data);
	if (find_hnd == INVALID_HANDLE_VALUE)
		return;

	// Loop over the file matching the pattern and try delete them. We do
	// not bother it fails.
	do {
		wcscpy(trash_buffer + prefix_len, find_data.cFileName);
		DeleteFileW(trash_buffer);
	} while (FindNextFileW(find_hnd, &find_data));

	FindClose(find_hnd);
}


/**
 * rename_file_to_trash_from_handle() - rename a file to trash volume folder
 * @hnd:        handle of the file opened for deletion
 *
 * This renames the file opened through @hnd to be renamed to the recycle bin
 * folder of its volume: the recycle bin is a folder present in each mounted
 * volume and a writeable folder is dedicated in it for each logged user. Hence
 * the file renaming should never failed as long as the filename used is
 * unique. To this end, the file to removed is renamed following a pattern
 * based on its unique file id on the volume.
 *
 * Return: 0 in case of success, -1 otherwise. Please note that this
 * function does not set error state. Use GetLastError() to retrieve the
 * origin of error.
 */
static
int rename_file_to_trash_from_handle(HANDLE hnd)
{
	FILE_ID_INFO id_info = {0};
	const struct volume* vol;
	union {
		char buffer[2*(MAX_PATH+1) + sizeof(FILE_RENAME_INFO)];
		FILE_RENAME_INFO info;
	} rename_buffer;
	FILE_RENAME_INFO* info = &rename_buffer.info;
	size_t info_sz;
	mm_ino_t ino;
	int rlen;

	// Get file id and deduce volume of the handle and volume trash prefix
	if (get_file_id_info_from_handle(hnd, &id_info)
	    || !(vol = get_volume_from_dev(id_info.VolumeSerialNumber))
	    || (rlen = volume_get_trash_prefix_u16(vol, info->FileName)) < 0) {
		return -1;
	}

	scrub_user_trash_path(info->FileName, rlen);

	// Some version of mingw64-crt defines FILE_ID_128 as 2 ULONGLONG
	// fields, some as BYTE[16]. Copy explicitly to mm_ino_t avoid
	// us any trouble
	memcpy(&ino, &id_info.FileId, sizeof(ino));

	// append path of file renamed in trash (named after its file id)
	swprintf(info->FileName + rlen, MAX_PATH - rlen,
	         L".mmlib-%016llx%016llx.deleted", ino.id_high, ino.id_low);
	info->FileName[MAX_PATH] = L'\0';  // ensure filename is terminated

	// Finalize the structure of rename operation and perform it
	info->RootDirectory = NULL;
	info->ReplaceIfExists = FALSE;
	info->FileNameLength = wcslen(info->FileName);
	info_sz = sizeof(*info);
	info_sz += sizeof(info->FileName[0]) * info->FileNameLength;
	if (!SetFileInformationByHandle(hnd, FileRenameInfo, info, info_sz))
		return -1;

	return 0;
}

/* doc in posix implementation */
API_EXPORTED
int mm_rename(const char* oldpath, const char* newpath)
{

	/* TODO: make the verifications for the directories (returns an error
	 *       in case the directory is not empty).
	 */

	int newpath_u16_len;
	FILE_RENAME_INFO * info;
	size_t sz;
	HANDLE hndold = INVALID_HANDLE_VALUE;
	HANDLE hndnew = INVALID_HANDLE_VALUE;
	int rv = -1;
	DWORD flags, access;

	newpath_u16_len = get_utf16_buffer_len_from_utf8(newpath);
	if (newpath_u16_len < 0)
		return mm_raise_from_w32err("invalid UTF-8 sequence");

	sz = newpath_u16_len * sizeof(char16_t) + sizeof(*info);
	info = mm_malloca(sz);
	if (!info)
		return -1;

	// FILE_FLAG_BACKUP_SEMANTICS is needed to open directories
	flags = FILE_FLAG_BACKUP_SEMANTICS;
	access = DELETE | FILE_WRITE_ATTRIBUTES;
	hndold = open_handle(oldpath, access, OPEN_EXISTING, NULL, flags);
	if (hndold == INVALID_HANDLE_VALUE)
		goto exit;

	conv_utf8_to_utf16(info->FileName, newpath_u16_len, newpath);
	info->RootDirectory = NULL;
	info->ReplaceIfExists = FALSE;
	info->FileNameLength = newpath_u16_len;

	// Try to rename the file oldpath to newpath. In case the renaming did
	// not work because a file named newpath was already used, then put it
	// in the trash and retry
	if (!SetFileInformationByHandle(hndold, FileRenameInfo, info, sz)) {
		hndnew = CreateFileW(info->FileName, access, FILE_SHARE_ALL,
		                     NULL, OPEN_EXISTING, flags, NULL);

		if ((hndnew == INVALID_HANDLE_VALUE)
		    || (rename_file_to_trash_from_handle(hndnew) == -1)
		    || !SetFileInformationByHandle(hndold, FileRenameInfo,
		                                   info, sz)) {
			goto exit;
		}
	}

	rv = 0;

exit:
	mm_freea(info);
	safe_closehandle(hndold);
	safe_closehandle(hndnew);
	return rv;
}


/**
 * delete_file_from_handle() - delete file opened by handle
 * @hnd:         handle of the file to be deleted opened with the appropriate
 *               access DELETE|FILE_WRITE_ATTRIBUTES
 *
 * Return: 0 in case of success, -1 otherwise. Please note that this
 * function does not set error state. Use GetLastError() to retrieve the
 * origin of error (this is meant to be done by the caller which has more
 * context, in particular the filename).
 */
static
int delete_file_from_handle(HANDLE* hnd)
{
	FILE_DISPOSITION_INFO dispose = {.DeleteFile = TRUE};

	// Make file path available immediately even if file object is not
	// removed yet
	if (rename_file_to_trash_from_handle(hnd))
		return -1;

	// Indicate kernel that file object should deleted. This will be
	// defered to when all handle to file object will be closed. we don't
	// mind to fail file has already been renamed: the file should be
	// eventually deleted afterwards anyway
	SetFileInformationByHandle(hnd, FileDispositionInfo,
	                           &dispose, sizeof(dispose));
	return 0;
}


/* doc in posix implementation */
API_EXPORTED
int mm_unlink(const char* path)
{
	DWORD flags, access;
	HANDLE hnd;
	int rv = 0;

	// Do not follow symlink (Hence FILE_FLAG_OPEN_REPARSE_POINT). Also
	// FILE_FLAG_BACKUP_SEMANTICS is added to handle symlink to folder.
	flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
	access = DELETE | FILE_WRITE_ATTRIBUTES;
	hnd = open_handle(path, access, OPEN_EXISTING, NULL, flags);
	if (hnd == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("Can't get handle for %s", path);

	if (delete_file_from_handle(hnd))
		rv = mm_raise_from_w32err("Can't delete %s", path);

	CloseHandle(hnd);
	return rv;
}


#define TOKEN_ACCESS    \
	(  TOKEN_IMPERSONATE | TOKEN_QUERY        \
	   | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ)
/*
 * from http://blog.aaronballman.com/2011/08/how-to-check-access-rights/
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
	if (!OpenProcessToken(hproc, TOKEN_ACCESS, &proc_htoken)
	    || !DuplicateToken(proc_htoken, SecurityImpersonation, &imp_htoken)
	    || !AccessCheck(sd, imp_htoken, access_rights, &mapping,
	                    &privileges, &priv_len, &granted, &result)) {
		goto exit;
	}

	rv = (result == TRUE) ? 0 : EACCES;

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
		if (w32err == ERROR_PATH_NOT_FOUND
		    || w32err == ERROR_FILE_NOT_FOUND)
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
	char16_t * oldpath_u16, * newpath_u16;
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
		mm_raise_from_w32err("CreateHardLinkW(%s, %s) failed",
		                     newpath,
		                     oldpath);
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
	char16_t * oldpath_u16, * newpath_u16;
	int retval = 0;
	int err;
	DWORD flags, file_attrs;

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

	flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;

	// Make directory symlink if target is detected as such. If target
	// (specified by oldpath) does not exist, the symlink will be
	// assumed to be symlink to a file. This is not a major problem to
	// do this assumption because having a file symlink pointing to a
	// directory is still functional to a certain extent (only opening
	// the target folder handle seems not functional)
	file_attrs = GetFileAttributesW(oldpath_u16);
	if ((file_attrs != INVALID_FILE_ATTRIBUTES)
	    && (file_attrs & FILE_ATTRIBUTE_DIRECTORY))
		flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

	err = CreateSymbolicLinkW(newpath_u16, oldpath_u16, flags);
	if (!err && GetLastError() == ERROR_INVALID_PARAMETER) {
		/* SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE was introduced
		 * in 2017-03. If it is not recognized, try again without it */
		flags &= ~SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
		err = CreateSymbolicLinkW(newpath_u16, oldpath_u16, flags);
	}

	if (!err) {
		mm_raise_from_w32err("CreateSymbolicLinkW(%s, %s) failed",
		                     newpath,
		                     oldpath);
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

		// Try to get the copy the currdir to buffer (if buffer is NULL
		// or too short, the return value will be larger than provided
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
		mm_raise_error(ERANGE, "Buffer too short for holding "
		               "current directory path");
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
	if (type & FILE_ATTRIBUTE_REPARSE_POINT) {
		return MM_DT_LNK;
	} else if (type & FILE_ATTRIBUTE_DIRECTORY) {
		return MM_DT_DIR;
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
 * @d:             pointer to the current MM_DIR structure to clean
 * @flags:         option flag to return on error
 * @rec_lvl:       maximum recursion level
 *
 * Many error return values are *explicitly* skipped.
 * Since this is a recursive removal, we should not stop when we encounter
 * a forbidden file or folder. This except if the @flag contains MM_FAILONERROR.
 *
 * Return: 0 on success, -1 on error
 */
static
int mm_remove_rec(const char * prefix, MM_DIR * d, int flags, int rec_lvl)
{
	int rv, status;
	unsigned int type;
	MM_DIR * newdir;
	const struct mm_dirent * dp;

	if (UNLIKELY(rec_lvl < 0))
		return mm_raise_error(EOVERFLOW, "Too many levels of recurion");

	while ((dp = mm_readdir(d, &status)) != NULL) {
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

			rv = mm_remove_rec(newdir_path,
			                   newdir,
			                   flags,
			                   rec_lvl - 1);
			mm_freea(newdir_path);
			mm_closedir(newdir);
			if (rv != 0 && (flags & MM_FAILONERROR))
				return -1;
		}

		rv = win32_unlinkat(prefix, d->dirent->name, type);
		if (rv != 0 && (flags & MM_FAILONERROR))
			return -1;
	}

	if (GetLastError() == ERROR_NO_MORE_FILES)
		return 0;

	return -1;
}

/* doc in posix implementation */
API_EXPORTED
int mm_remove(const char* path, int flags)
{
	int rv, error_flags;
	MM_DIR * dir;
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

			error_flags = mm_error_set_flags(MM_ERROR_SET,
			                                 MM_ERROR_NOLOG);
			rv = mm_remove_rec(path, dir, flags, RECURSION_MAX);
			mm_error_set_flags(error_flags, MM_ERROR_NOLOG);
			mm_closedir(dir);
			if (rv != 0 && !(flags & MM_FAILONERROR)) {
				return mm_raise_from_errno(
					"recursive mm_remove(%s) failed",
					path);
			}

			/* allow rmdir(".") when called with the recursive flag
			 * only */
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
 * @dir:    pointer to a MM_DIR structure
 *
 * NOTE: windows Prototype are:
 *   HANDLE FindFirstFileW(path, ...)
 *   bool FindFirstFileW(HANDLE, ...)
 *
 * This function hides the difference by always returning a HANDLE like
 * FindFirstFile() and storing the HANDLE and path within the MM_DIR
 * structure.
 *
 * Return: 0 on success, -1 on error
 * The caller should call GetLastError() and investigate the issue itself
 *
 */
static
int win32_find_file(MM_DIR * dir)
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
		if (!FindNextFileW(dir->hdir, &find_dataw))
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
		if (tmp == NULL)
			return mm_raise_from_errno("cannot alloc dirent");

		dir->dirent = tmp;
		dir->dirent->reclen = reclen;
	}

	dir->dirent->type = translate_filetype(find_dataw.dwFileAttributes);
	conv_utf16_to_utf8(dir->dirent->name, namelen, find_dataw.cFileName);

	return 0;
}

/* doc in posix implementation */
API_EXPORTED
MM_DIR* mm_opendir(const char* path)
{
	int len;
	MM_DIR * d;

	len = strlen(path) + 3;  // concat with "/*\0"
	d = malloc(sizeof(*d) + len);
	if (d == NULL)
		goto error;

	*d = (MM_DIR) {.hdir = INVALID_HANDLE_VALUE};
	strcpy(d->dirname, path);
	strcat(d->dirname, "/*");

	/* call FindFirstFile() to ensure that the given path
	 * is meaningful, and to keep ithe folder opened */
	if (win32_find_file(d))
		goto error;

	return d;

error:
	mm_raise_from_errno("opendir(%s) failed", path);
	mm_closedir(d);
	return NULL;
}

/* doc in posix implementation */
API_EXPORTED
void mm_closedir(MM_DIR* dir)
{
	if (dir == NULL)
		return;

	if (dir->hdir != INVALID_HANDLE_VALUE)
		FindClose(dir->hdir);

	free(dir->dirent);
	free(dir);
}

/* doc in posix implementation */
API_EXPORTED
void mm_rewinddir(MM_DIR* dir)
{
	if (dir == NULL) {
		mm_raise_error(EINVAL, "Does not accept NULL arguments");
		return;
	}

	FindClose(dir->hdir);
	win32_find_file(dir);
}

/* doc in posix implementation */
API_EXPORTED
const struct mm_dirent* mm_readdir(MM_DIR* d, int * status)
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
		if (GetLastError() == ERROR_NO_MORE_FILES)
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
 * HANDLE:      handle of an open reparse point
 *
 * Return: initialized REPARSE_DATA_BUFFER in case of success, NULL
 * otherwise. Must be cleanup with free() when you don't need it any longer
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
 * hnd:         handle of an open symlink
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
LOCAL_SYMBOL
int get_stat_from_handle(HANDLE hnd, struct mm_stat* buf)
{
	FILE_ATTRIBUTE_TAG_INFO attr_tag;
	FILE_ID_INFO id_info;
	BY_HANDLE_FILE_INFORMATION info;
	struct local_secdesc lsd;
	int type;

	if (!GetFileInformationByHandle(hnd, &info)
	    || local_secdesc_init_from_handle(&lsd, hnd)) {
		return -1;
	}

	if (!GetFileInformationByHandleEx(hnd, FileAttributeTagInfo,
	                                  &attr_tag, sizeof(attr_tag))
	    || !GetFileInformationByHandleEx(hnd, FileIdInfo,
	                                     &id_info, sizeof(id_info))) {
		DWORD id[4];

		attr_tag = (FILE_ATTRIBUTE_TAG_INFO) {
			.FileAttributes = info.dwFileAttributes,
		};

		// Convert 2 DWORDs file ID into FILE_ID_128
		id[0] = info.nFileIndexLow;
		id[1] = info.nFileIndexHigh;
		id[2] = 0;
		id[3] = 0;
		memcpy(&id_info.FileId, id, sizeof(id));

		id_info.VolumeSerialNumber = info.dwVolumeSerialNumber;
	}

	// translate_filetype() consider all reparse point as symlink. Here
	// we can use dwReserved0 field to distinguish type
	if (attr_tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		switch (attr_tag.ReparseTag) {
		case IO_REPARSE_TAG_SYMLINK:     type = S_IFLNK; break;
		case IO_REPARSE_TAG_MOUNT_POINT: type = S_IFDIR; break;
		default:                         type = S_IFREG; break;
		}
	} else {
		switch (translate_filetype(attr_tag.FileAttributes)) {
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

	buf->atime = filetime_to_time(info.ftLastAccessTime);
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


static
int hnd_utimens(HANDLE hnd, const struct mm_timespec ts[2])
{
	FILETIME aft;
	FILETIME mft;
	FILETIME now;

	if (!ts || ts[0].tv_nsec == UTIME_NOW || ts[1].tv_nsec == UTIME_NOW)
		GetSystemTimePreciseAsFileTime(&now);

	// Set access filetime
	if (!ts || ts[0].tv_nsec == UTIME_NOW)
		aft = now;
	else if (ts[0].tv_nsec == UTIME_OMIT)
		aft = (FILETIME) {0};
	else
		aft = timespec_to_filetime(ts[0]);

	// Set modification filetime
	if (!ts || ts[1].tv_nsec == UTIME_NOW)
		mft = now;
	else if (ts[1].tv_nsec == UTIME_OMIT)
		mft = (FILETIME) {0};
	else
		mft = timespec_to_filetime(ts[1]);

	return SetFileTime(hnd, NULL, &aft, &mft) ? 0 : -1;
}


/* doc in posix implementation */
API_EXPORTED
int mm_futimens(int fd, const struct mm_timespec ts[2])
{
	HANDLE hnd, hnd_new;
	int rv = -1;

	if (unwrap_handle_from_fd(&hnd, fd))
		return -1;

	hnd_new = ReOpenFile(hnd, FILE_WRITE_ATTRIBUTES, FILE_SHARE_ALL, 0);
	if (hnd_new == INVALID_HANDLE_VALUE)
		goto exit;

	rv = hnd_utimens(hnd_new, ts);
	CloseHandle(hnd_new);

exit:
	if (rv)
		mm_raise_from_w32err("Cannot change times of fd %i", fd);
	return rv;
}


/* doc in posix implementation */
API_EXPORTED
int mm_utimens(const char* path, const struct mm_timespec ts[2], int flags)
{
	HANDLE hnd;
	int rv;

	if (flags & ~MM_NOFOLLOW)
		return mm_raise_error(EINVAL, "invalid flags (0x%08x)", flags);

	hnd = open_handle(path, FILE_WRITE_ATTRIBUTES,
	                  OPEN_EXISTING, NULL, flags);
	if (hnd == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("Cannot open %s", path);

	rv = hnd_utimens(hnd, ts);
	if (rv)
		mm_raise_from_w32err("Cannot change times of %s", path);

	CloseHandle(hnd);
	return rv;
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



#define COPYBUFFER_SIZE (1024*1024) // 1MiB

static
int clone_hnd_fallback(HANDLE hnd_src, HANDLE hnd_dst)
{
	size_t wbuf_sz;
	char * buffer, * wbuf;
	ssize_t rsz, wsz;
	int rv = -1;

	buffer = malloc(COPYBUFFER_SIZE);
	if (!buffer)
		return -1;

	do {
		// Perform read operation
		rsz = mmlib_read(hnd_src, buffer, COPYBUFFER_SIZE);
		if (rsz < 0)
			goto exit;

		// Do write of what has been read, possibly chunked if transfer
		// got interrupted
		wbuf = buffer;
		wbuf_sz = rsz;
		while (wbuf_sz) {
			wsz = mmlib_write(hnd_dst, wbuf, wbuf_sz);
			if (wsz < 0)
				goto exit;

			wbuf += wsz;
			wbuf_sz -= wsz;
		}
	} while (rsz != 0);

	rv = 0;

exit:
	free(buffer);
	return rv;
}


static
int clone_hnd_force_cow(HANDLE hnd_src, HANDLE hnd_dst)
{
	(void)hnd_src;
	(void)hnd_dst;
	SetLastError(ERROR_NOT_SUPPORTED);
	return -1;
}


static
int copy_hnd_symlink(HANDLE hnd, const char* dst)
{
	REPARSE_DATA_BUFFER* rep = NULL;
	const char16_t* target;
	char16_t* dst16 = NULL;
	DWORD flags;
	int dst16_len, rv = -1;

	dst16_len = get_utf16_buffer_len_from_utf8(dst);
	if (dst16_len < 0)
		goto exit;

	dst16 = mm_malloca(dst16_len * sizeof(*dst16));
	if (dst16 == NULL)
		goto exit;

	conv_utf8_to_utf16(dst16, dst16_len, dst);
	rep = get_reparse_data(hnd);
	if (!rep)
		goto exit;

	target = get_target_u16_from_reparse_data(rep);

	flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
	if (!CreateSymbolicLinkW(dst16, target, flags)) {
		if (GetLastError() != ERROR_INVALID_PARAMETER
		   || !CreateSymbolicLinkW(dst16, target, 0))
			goto exit;
	}
	rv = 0;

exit:
	mm_freea(dst16);
	free(rep);
	return rv;
}


static
int clone_src_hnd(HANDLE hnd_src, const char* dst, int flags, int mode)
{
	HANDLE hnd_dst;
	struct local_secdesc lsd;
	int rv = -1;
	DWORD w32err;

	if (local_secdesc_init_from_mode(&lsd, mode))
		return -1;

	hnd_dst = open_handle(dst, GENERIC_WRITE|DELETE|FILE_WRITE_ATTRIBUTES,
			      CREATE_NEW, lsd.sd, FILE_ATTRIBUTE_NORMAL);
	local_secdesc_deinit(&lsd);
	if (hnd_dst == INVALID_HANDLE_VALUE)
		return -1;

	switch (flags & (MM_NOCOW & MM_FORCECOW)) {
	case MM_FORCECOW:
		rv = clone_hnd_force_cow(hnd_src, hnd_dst);
		break;

	case MM_NOCOW:
	default:
		rv = clone_hnd_fallback(hnd_src, hnd_dst);
	}

	// Delete incomplete destination file in case of failure while keeping
	// reported system error
	if (rv) {
		w32err = GetLastError();
		delete_file_from_handle(hnd_dst);
		SetLastError(w32err);
	}

	CloseHandle(hnd_dst);
	return rv;
}


LOCAL_SYMBOL
int copy_internal(const char* src, const char* dst, int flags, int mode)
{
	int rv = -1;
	HANDLE hnd_src;
	DWORD attrs;
	FILE_ATTRIBUTE_TAG_INFO tag_info;

	attrs = FILE_ATTRIBUTE_NORMAL;
	attrs |= flags & MM_NOFOLLOW ? FILE_FLAG_OPEN_REPARSE_POINT : 0;
	hnd_src = open_handle(src, GENERIC_READ, OPEN_EXISTING, NULL, attrs);

	// Force to open handle to identify EISDIR error case which is aliased
	// to access denied error if FILE_FLAG_BACKUP_SEMANTICS is not used.
	if ((hnd_src == INVALID_HANDLE_VALUE)
	    && (GetLastError() == ERROR_ACCESS_DENIED)) {
		hnd_src = open_handle_for_metadata(src, flags & MM_NOFOLLOW);
	}

	if (hnd_src == INVALID_HANDLE_VALUE)
		return mm_raise_from_w32err("Cannot open %s", src);

	if (!GetFileInformationByHandleEx(hnd_src, FileAttributeTagInfo,
	                                  &tag_info, sizeof(tag_info)))
		goto exit;

	if ((tag_info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
	    && (tag_info.ReparseTag == IO_REPARSE_TAG_SYMLINK))
		rv = copy_hnd_symlink(hnd_src, dst);
	else if (tag_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		SetLastError(ERROR_DIRECTORY_NOT_SUPPORTED);
	else
		rv = clone_src_hnd(hnd_src, dst, flags, mode);

exit:
	CloseHandle(hnd_src);
	if (rv)
		mm_raise_from_w32err("Fail to copy %s to %s", src, dst);

	return rv;
}


LOCAL_SYMBOL
int internal_mkdir(const char* path, int mode, int report_recursive)
{
	int rv, path_u16_len;
	DWORD err;
	char16_t* path_u16;
	struct local_secdesc lsd;
	SECURITY_ATTRIBUTES sec = {.nLength = sizeof(SECURITY_ATTRIBUTES)};

	if (local_secdesc_init_from_mode(&lsd, mode))
		return mm_raise_from_w32err("can't create security descriptor");

	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0) {
		rv = mm_raise_from_w32err("Invalid UTF-8 path");
		goto exit;
	}

	path_u16 = mm_malloca(path_u16_len * sizeof(*path_u16));
	if (path_u16 == NULL) {
		rv = mm_raise_from_w32err("Failed to alloc required memory!");
		goto exit;
	}

	conv_utf8_to_utf16(path_u16, path_u16_len, path);

	rv = 0;
	sec.lpSecurityDescriptor = lsd.sd;
	if (!CreateDirectoryW(path_u16, &sec)) {
		rv = -1;
		if (!report_recursive) {
			switch (GetLastError()) {
			case ERROR_ALREADY_EXISTS: rv = EEXIST; break;
			case ERROR_PATH_NOT_FOUND: rv = ENOENT; break;
			}
		}
	}
	if (rv < 0)
		mm_raise_from_w32err("Failed to make dir %s", path);

exit:
	mm_freea(path_u16);
	local_secdesc_deinit(&lsd);
	return rv;
}
