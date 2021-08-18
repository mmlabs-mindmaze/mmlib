/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmsysio.h"
#include "mmlib.h"
#include "mmerrno.h"

#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <direct.h>
#include <fcntl.h>
#include "utils-win32.h"


/**
 * get_win32_page_protect() - Get page protection and access mask
 * @mflags:             flags passed to mm_mapfile
 * @protect_mask:       pointer to mask specifying the page protect for file
 *                      mapping creation (output variable)
 * @access_mask:        location of mask that will specify the access during
 *                      the creation of the map view (output variable)
 *
 * This function translate the flags passed to mm_mapfile() into page
 * protection mask to be used by CreateFileMapping() and access mask to be
 * used by MapViewOfFile().
 */
static
void get_win32_page_protect(int mflags,
                            DWORD* protect_mask, DWORD* access_mask)
{
	DWORD protect = 0;
	DWORD access = 0;

	if (mflags & MM_MAP_READ) {
		protect = PAGE_READONLY;
		access = FILE_MAP_READ;
	}

	if (mflags & MM_MAP_WRITE) {
		protect = PAGE_READWRITE;
		access = FILE_MAP_ALL_ACCESS;
	}

	if (!(mflags & MM_MAP_SHARED))
		access = FILE_MAP_COPY;

	if (mflags & MM_MAP_EXEC) {
		protect <<= 4;
		access |= FILE_MAP_EXECUTE;
	}

	*protect_mask = protect;
	*access_mask = access;
}


/* doc in posix implementation */
API_EXPORTED
void* mm_mapfile(int fd, mm_off_t offset, size_t len, int mflags)
{
	HANDLE hfile, hmap;
	DWORD protect, access, off_h, off_l;
	struct mm_stat stat;
	void* ptr;

	if (unwrap_handle_from_fd(&hfile, fd))
		return NULL;

	get_win32_page_protect(mflags, &protect, &access);

	// Create the Filemapping object
	hmap = CreateFileMapping(hfile, NULL, protect, 0, 0, NULL);
	if (hmap == NULL) {
		mm_raise_from_w32err("CreateFileMapping failed (fd=%i)", fd);
		return NULL;
	}

	// Map into memory
	off_h = offset >> 32;
	off_l = offset & 0xffffffff;
	ptr = MapViewOfFile(hmap, access, off_h, off_l, len);
	if (!ptr) {
		/* when trying to map a file, if the request length is larger
		 * than the size of the file itself, windows returns a generic
		 * permission error. Transform it into EINVAL instead */
		if (GetLastError() == ERROR_ACCESS_DENIED
		    && get_stat_from_handle(hfile, &stat) == 0
		    && S_ISREG(stat.mode)
		    && stat.size < (mm_off_t) len) {
			mm_raise_error(EOVERFLOW,
			               "CreateFileMapping failed (fd=%i). "
			               "requested len (%d) > file size (%d)",
			               fd, stat.size, len);
		} else {
			mm_raise_from_w32err("MapViewOfFile failed (fd=%i)",
			                     fd);
		}
	}

	CloseHandle(hmap);
	return ptr;
}


/* doc in posix implementation */
API_EXPORTED
int mm_unmap(void* addr)
{
	if (!addr)
		return 0;

	if (!UnmapViewOfFile(addr))
		return mm_raise_from_w32err("UnmapViewOfFile failed");

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                         SHM access per se                              *
 *                                                                        *
 **************************************************************************/
#define MAX_ATTEMPT     32
#define MAX_NAME        256
#define SHMROOT_PATH_MAXLEN     512


/**
 * get_shm_path() - return the path of directory to be stored SHM objects
 *
 * This function returns the path where the files backing the shared memory
 * objects are located. If this folder do not exist yet, it will create it.
 * Multiple executions of the functions will lead to the same return value.
 *
 * Return: path where to store SHM object.
 */
static
const char* get_shm_path(void)
{
	static char shm_root[SHMROOT_PATH_MAXLEN] = "";
	const char* tmp_root;

	// Check the shm root path has not already be determined
	if (shm_root[0] != '\0')
		return shm_root;

	// Form shm root path
	tmp_root = mm_getenv("TMP", "C:\\WINDOWS\\TEMP");
	snprintf(shm_root, sizeof(shm_root), "%s/shm", tmp_root);

	// Create folder
	_mkdir(shm_root);

	return shm_root;
}


/* doc in posix implementation */
API_EXPORTED
int mm_anon_shm(void)
{
	HANDLE hnd;
	int i, fd;
	LARGE_INTEGER count;
	unsigned int randval;
	char filename[SHMROOT_PATH_MAXLEN + MAX_NAME + 8];

	for (i = 0; i < MAX_ATTEMPT; i++) {
		// Generate random data filename
		QueryPerformanceCounter(&count);
		randval = (unsigned int)count.LowPart;
		sprintf(filename, "%s/mmlib-shm-%u", get_shm_path(), randval);
		// Try open the file
		hnd = CreateFile(filename, GENERIC_READ|GENERIC_WRITE,
		                 0, NULL, CREATE_ALWAYS,
		                 FILE_FLAG_DELETE_ON_CLOSE, NULL);
		if (hnd != INVALID_HANDLE_VALUE)
			break;
	}

	if (hnd == INVALID_HANDLE_VALUE) {
		mm_raise_from_w32err("CreateFile(%s) failed", filename);
		return -1;
	}

	if (wrap_handle_into_fd(hnd, &fd, FD_TYPE_NORMAL)) {
		CloseHandle(hnd);
		return -1;
	}

	return fd;
}


/* doc in posix implementation */
API_EXPORTED
int mm_shm_open(const char* name, int oflag, int mode)
{
	char filename[SHMROOT_PATH_MAXLEN + MAX_NAME + 8];

	sprintf(filename, "%s/%s", get_shm_path(), name);

	return mm_open(filename, oflag, mode);
}


/* doc in posix implementation */
API_EXPORTED
int mm_shm_unlink(const char* name)
{
	char filename[SHMROOT_PATH_MAXLEN + MAX_NAME + 8];

	sprintf(filename, "%s/%s", get_shm_path(), name);

	return mm_unlink(filename);
}
