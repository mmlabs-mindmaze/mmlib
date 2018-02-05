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
#include <uchar.h>


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
	FD_TYPE_SOCKET,
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


/**************************************************************************
 *                                                                        *
 *                         UTF-8/UTF-16 conversion                        *
 *                                                                        *
 **************************************************************************/

int get_utf16_buffer_len_from_utf8(const char* utf8_str);
int conv_utf8_to_utf16(char16_t* utf16_str, int utf16_len, const char* utf8_str);
int get_utf8_buffer_len_from_utf16(const char16_t* utf16_str);
int conv_utf16_to_utf8(char* utf8_str, int utf8_len, const char16_t* utf16_str);


/**************************************************************************
 *                                                                        *
 *                         thread related utils                           *
 *                                                                        *
 **************************************************************************/

/**
 * get_tid() - fast inline replacement for GetCurrentThreadId()
 *
 * This function is a replacement of GetCurrentThreadId() without involving any
 * library call. This is just a lookup in the thread information block provided
 * by the Windows NT kernel which is held in the GS segment register (offset
 * 0x48) in case of 64 bits or FS segment register (offset 0x24) in case of 32
 * bits.
 *
 * Return: the ID of the current thread
 */
static inline
DWORD get_tid(void)
{
	DWORD tid;
#if defined(__x86_64) || defined(_M_X64)
	tid = __readgsdword(0x48);
#elif defined(__i386__) || defined(_M_IX86)
	tid = __readfsdword(0x24);
#else
	tid = GetCurrentThreadId();
#endif
	return tid;
}


static inline
void* read_teb_address(unsigned long offset)
{
#if defined(__x86_64) || defined(_M_X64)
	return (void*)__readgsqword(offset);
#elif defined(__i386__) || defined(_M_IX86)
	return (void*)__readfsdword(offset);
#else
	return (((char*)NtCurrentTeb())+offset);
#endif
}

#define TEB_OFFSET(member)	(offsetof(struct _TEB, member))
#define NUM_TLS_SLOTS		(MM_NELEM(((struct _TEB*)0)->TlsSlots))


/**
 * tls_get_value() - fast inline replacement for TlsGetValue()
 * @index:	The TLS index allocated by the TlsAlloc() function
 *
 * This function is a replacement of TlsGetValue() without involving any
 * library call. It does the same but by directly interrogating the thread
 * information block.
 *
 * Return: If the function succeeds, the return value is the value stored in
 * the calling thread's TLS slot associated with the specified index. NULL
 * is returned if no data has been previously associated for this thread.
 *
 * NOTE: The function does not perform any parameter validation, it is
 * important that @index argument is a really a value returned by
 * TlsAlloc().
 */
static inline
void* tls_get_value(DWORD index)
{
	unsigned long offset;
	void** tls_exp_slots;

	if (index < NUM_TLS_SLOTS) {
		offset = TEB_OFFSET(TlsSlots) + index*sizeof(PVOID);
		return read_teb_address(offset);
	}

	tls_exp_slots = read_teb_address(TEB_OFFSET(TlsExpansionSlots));
	if (!tls_exp_slots)
		return NULL;

	return tls_exp_slots[index - NUM_TLS_SLOTS];
}

#endif

