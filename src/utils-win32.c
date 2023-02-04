/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "utils-win32.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <aclapi.h>
#include <sddl.h>

#include "mmerrno.h"
#include "mmlog.h"
#include "mmlib.h"
#include "mmthread.h"

// https://docs.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-shell-setup-offlineuseraccounts-offlinedomainaccounts-offlinedomainaccount-sid
#define SID_STRING_MAXLEN 256

static
int set_access_mode(struct w32_create_file_options* opts, int oflags)
{
	switch (oflags & (_O_RDONLY|_O_WRONLY| _O_RDWR)) {
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
		mm_raise_error(EINVAL,
		               "Invalid combination of file access mode");
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
int set_w32_create_file_options(struct w32_create_file_options* opts,
                                int oflags)
{
	if (set_access_mode(opts, oflags)
	    || set_creation_mode(opts, oflags))
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
	hnd = CreateFileW(path_u16, access, FILE_SHARE_ALL, &sec_attrs,
	                  creat, flags, NULL);

	mm_freea(path_u16);

	return hnd;
}


/**
 * get_file_id_info_from_handle() - get file and volume ids
 * @hnd:        handle of an opened file
 * @info:       pointer to FILE_ID_INFO struct to fill
 *
 * This function is similar to GetFileInformationByHandleEx() called with
 * FileIdInfo with fallback in case of buggy volume type:
 * GetFileInformationByHandleEx() returns error when called on file on FAT32.
 *
 * Returns: 0 in case of success, -1 otherwise.
 */
LOCAL_SYMBOL
int get_file_id_info_from_handle(HANDLE hnd, FILE_ID_INFO* id_info)
{
	BY_HANDLE_FILE_INFORMATION file_info;
	DWORD id[4];

	// First try with normal call
	if (GetFileInformationByHandleEx(hnd, FileIdInfo, id_info,
	                                 sizeof(*id_info)))
		return 0;

	// GetFileInformationByHandleEx() usually fail with file handle in
	// FAT32. Let's fall back on GetFileInformationByHandle()
	if (!GetFileInformationByHandle(hnd, &file_info))
		return mm_raise_from_w32err("failed to get file info");

	// Convert 2 DWORDs file ID into FILE_ID_128
	id[0] = file_info.nFileIndexLow;
	id[1] = file_info.nFileIndexHigh;
	id[2] = 0;
	id[3] = 0;
	memcpy(&id_info->FileId, id, sizeof(id));

	id_info->VolumeSerialNumber = file_info.dwVolumeSerialNumber;
	return 0;
}


/**************************************************************************
 *                                                                        *
 *                       Access control setup                             *
 *                                                                        *
 **************************************************************************/

#define MIN_ACCESS_FLAGS \
	(READ_CONTROL | SYNCHRONIZE | FILE_READ_ATTRIBUTES | FILE_READ_EA)
#define MIN_OWNER_ACCESS_FLAGS \
	(DELETE | WRITE_DAC | WRITE_OWNER | FILE_WRITE_EA | \
	 FILE_WRITE_ATTRIBUTES)

#define IS_DACL_EMPTY(acl, dacl_present) \
	(((acl) == NULL) && ((dacl_present) == TRUE))

#define IS_DACL_NULL(acl, dacl_present) \
	(((acl) == NULL) && ((dacl_present) == FALSE))

/**
 * union local_sid - type to preallocate buffer for SID on stack
 * @sid:         SID structure (not complete to hold a SID)
 * @buffer:      buffer large enough to hold any SID
 */
union local_sid {
	SID sid;
	char buffer[SECURITY_MAX_SID_SIZE];
};

static union local_sid everyone;

MM_CONSTRUCTOR(wellknown_sid)
{
	DWORD len = sizeof(everyone);

	CreateWellKnownSid(WinWorldSid, NULL, &everyone.sid, &len);
}


static
mode_t access_to_mode(DWORD access)
{
	mode_t mode = 0;

	mode |= (access & FILE_READ_DATA) ? S_IREAD : 0;
	mode |= (access & FILE_WRITE_DATA) ? S_IWRITE : 0;
	mode |= (access & FILE_EXECUTE) ? S_IEXEC : 0;

	return mode;
}


static
DWORD get_access_from_perm_bits(mode_t mode)
{
	DWORD access = MIN_ACCESS_FLAGS;

	access |= (mode & S_IREAD) ? FILE_GENERIC_READ : 0;
	access |= (mode & S_IWRITE) ? FILE_GENERIC_WRITE : 0;
	access |= (mode & S_IEXEC) ? FILE_GENERIC_EXECUTE : 0;

	return access;
}


/**
 * get_caller_sids() - retrieve owner and primary group SIDs of caller
 * @owner:       pointer to buffer that will receive the owner SID.
 *               Ignored if NULL.
 * @primary_group: pointer to buffer that will receive the primary group SID.
 *                 Ignored if NULL
 *
 * Return: 0 in case of success, -1 otherwise. Please note that this
 * function does not set error state. Use GetLastError() to retrieve the
 * origin of error.
 */
static
int get_caller_sids(SID* owner, SID* primary_group)
{
	HANDLE htoken = INVALID_HANDLE_VALUE;
	char tmp[128];
	TOKEN_OWNER* owner_info;
	TOKEN_PRIMARY_GROUP* group_info;
	DWORD len;
	int rv = -1;

	// Open access token of the caller
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &htoken))
		goto exit;

	// Get Owner SID
	if (owner) {
		len = sizeof(tmp);
		owner_info = (TOKEN_OWNER*)tmp;
		if (!GetTokenInformation(htoken, TokenOwner,
		                         owner_info, len, &len))
			goto exit;

		CopySid(SECURITY_MAX_SID_SIZE, owner,
		        owner_info->Owner);
	}

	// Get Primary group SID
	if (primary_group) {
		len = sizeof(tmp);
		group_info = (TOKEN_PRIMARY_GROUP*)tmp;
		if (!GetTokenInformation(htoken, TokenPrimaryGroup,
		                         group_info, len, &len))
			goto exit;

		CopySid(SECURITY_MAX_SID_SIZE, primary_group,
		        group_info->PrimaryGroup);
	}

	rv = 0;

exit:
	safe_closehandle(htoken);
	return rv;
}


static int owner_sid_strlen = -1;
static char16_t owner_sid_str[SID_STRING_MAXLEN];


static
void init_owner_sid_str(void)
{
	union local_sid owner;
	char16_t* sidstr = NULL;

	if (get_caller_sids(&owner.sid, NULL)
	    || !ConvertSidToStringSidW(&owner.sid, &sidstr)) {
		mm_raise_from_w32err("could not initialize owner SID string");
		return;
	}

	owner_sid_strlen = wcslen(sidstr);
	memcpy(owner_sid_str, sidstr, (owner_sid_strlen+1)*sizeof(*sidstr));

	LocalFree(sidstr);
}


LOCAL_SYMBOL
const char16_t* get_caller_string_sid_u16(void)
{
	static mm_thr_once_t sid_init_once = MM_THR_ONCE_INIT;

	// Initialize owner SID string only once: the owner SID of a process
	// cannot be changed
	mm_thr_once(&sid_init_once, init_owner_sid_str);

	if (owner_sid_strlen < 0)
		return NULL;

	return owner_sid_str;
}


/**
 * local_secdesc_resize() - accommodate local secdesc for a new size
 * @lsd:        pointer to local_secdesc structure to initialize
 * @size:       needed size for SECURITY_DESCRIPTOR
 *
 * Return: 0 in case of success, -1 otherwise. Please note that this
 * function does not set error state. Use GetLastError() to retrieve the
 * origin of error.
 */
static
int local_secdesc_resize(struct local_secdesc* lsd, size_t size)
{
	local_secdesc_deinit(lsd);

	if (size <= sizeof(lsd->buffer)) {
		lsd->sd = (SECURITY_DESCRIPTOR*)lsd->buffer;
		return 0;
	}

	lsd->sd = malloc(size);
	return lsd->sd ? 0 : -1;
}


/**
 * local_secdesc_init_from_mode() - init from permission bits
 * @lsd:        pointer to local_secdesc structure to initialize
 * @mode:       permission bits
 *
 * This function initialize a security descriptor in self-relative format in
 * @lsd->sd from the @mode argument and the access token of the caller. This
 * function is mostly called directly or indirectly when creating a new
 * object on filesystem (file, directory, named pipe, ...).
 *
 * Return: 0 in case of success, -1 otherwise. Please note that this
 * function does not set error state. Use GetLastError() to retrieve the
 * origin of error.
 */
LOCAL_SYMBOL
int local_secdesc_init_from_mode(struct local_secdesc* lsd, int mode)
{
	PACL acl = NULL;
	SECURITY_DESCRIPTOR abs_sd;
	DWORD acl_size, access;
	union local_sid owner, group;
	DWORD len;
	int rv = -1;

	// Initial local secdesc functional field
	lsd->sd = NULL;

	// If default mode flags is specified, only NULL security descriptor
	// will be returned, hence default descriptor will be used.
	if (mode & MODE_DEF)
		return 0;

	// Get SID values
	if (get_caller_sids(&owner.sid, &group.sid))
		goto exit;

	// Initialize tmp_sd in absolute format
	InitializeSecurityDescriptor(&abs_sd, SECURITY_DESCRIPTOR_REVISION);

	acl_size = sizeof(ACL) + 3*sizeof(ACCESS_ALLOWED_ACE);
	acl_size += GetLengthSid(&owner.sid) - sizeof(DWORD);
	acl_size += GetLengthSid(&group.sid) - sizeof(DWORD);
	acl_size += GetLengthSid(&everyone.sid) - sizeof(DWORD);
	acl = mm_malloca(acl_size);
	InitializeAcl(acl, acl_size, ACL_REVISION);

	// Set owner permissions
	access = get_access_from_perm_bits(mode)|MIN_OWNER_ACCESS_FLAGS;
	if (!AddAccessAllowedAce(acl, ACL_REVISION, access, &owner.sid))
		goto exit;

	// Set group member permission
	access = get_access_from_perm_bits(mode << 3);
	if (!AddAccessAllowedAce(acl, ACL_REVISION, access, &group.sid))
		goto exit;

	// Set everyone permission
	access = get_access_from_perm_bits(mode << 6);
	if (!AddAccessAllowedAce(acl, ACL_REVISION, access, &everyone.sid))
		goto exit;

	// Add ACL into security descriptor and return a self-relative version
	if (!SetSecurityDescriptorDacl(&abs_sd, TRUE, acl, FALSE))
		goto exit;

	// Transform the security descriptor in absolute format into a
	// self-relative format and put it in lsd buffer
	len = sizeof(lsd->buffer);
	lsd->sd = (SECURITY_DESCRIPTOR*)lsd->buffer;
	while (!MakeSelfRelativeSD(&abs_sd, lsd->sd, &len)) {
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER
		    || local_secdesc_resize(lsd, len))
			goto exit;
	}

	rv = 0;

exit:
	if (rv)
		local_secdesc_deinit(lsd);

	mm_freea(acl);
	return rv;
}


/**
 * local_secdesc_init_from_handle() - init from securable object handle
 * @lsd:        pointer to local_secdesc structure to initialize
 * @hnd:        handle to the securable object (most of time file handle)
 *
 * This function initializes a copy of security descriptor of @hnd in
 * self-relative format in @lsd->sd.
 *
 * Return: 0 in case of success, -1 otherwise. Please note that this
 * function does not set error state. Use GetLastError() to retrieve the
 * origin of error.
 */
LOCAL_SYMBOL
int local_secdesc_init_from_handle(struct local_secdesc* lsd, HANDLE hnd)
{
	int rv;
	DWORD len;
	SECURITY_INFORMATION requested_info = OWNER_SECURITY_INFORMATION
	                                      | GROUP_SECURITY_INFORMATION
	                                      | DACL_SECURITY_INFORMATION;

	// Start with using lsd->buffer as backing buffer of secuity descriptor
	lsd->sd = (SECURITY_DESCRIPTOR*)lsd->buffer;
	len = sizeof(lsd->buffer);

	rv = 0;
	while (!GetKernelObjectSecurity(hnd, requested_info,
	                                lsd->sd, len, &len)) {

		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER
		    || local_secdesc_resize(lsd, len)) {
			rv = -1;
			local_secdesc_deinit(lsd);
			break;
		}
	}

	return rv;
}


/**
 * local_secdesc_deinit() - Cleanup a initialized local secdesc.
 * @lsd:        pointer to an initialized local secdesc
 */
LOCAL_SYMBOL
void local_secdesc_deinit(struct local_secdesc* lsd)
{
	if ((char*)lsd->sd != lsd->buffer)
		free(lsd->sd);

	lsd->sd = NULL;
}


/**
 * local_secdesc_get_mode() - Get permission bits from secdesc
 * @lsd:        pointer to an initialized local secdesc
 *
 * Return: permission bits inferred from the security descriptor.
 */
LOCAL_SYMBOL
mode_t local_secdesc_get_mode(struct local_secdesc* lsd)
{
	PSID owner_sid = NULL, group_sid = NULL, sid = NULL;
	PACL acl = NULL;
	ACCESS_ALLOWED_ACE* ace = NULL;
	BOOL defaulted, dacl_present;
	mode_t mode;
	int i;

	GetSecurityDescriptorOwner(lsd->sd, &owner_sid, &defaulted);
	GetSecurityDescriptorGroup(lsd->sd, &group_sid, &defaulted);
	GetSecurityDescriptorDacl(lsd->sd, &dacl_present, &acl, &defaulted);

	// null DACL means all access while empty dacl means no access. See
	// https://msdn.microsoft.com/en-us/library/windows/desktop/aa446648(v=vs.85).aspx

	if (IS_DACL_NULL(acl, dacl_present))
		return 0777;

	if (IS_DACL_EMPTY(acl, dacl_present))
		return 0;

	// Scan all "Access Allowed" entries and interpret them as owner,
	// group or other permission depending on SID in the ACE
	mode = 0;
	for (i = 0; i < acl->AceCount; i++) {
		GetAce(acl, i, (LPVOID*)&ace);

		if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
			continue;

		sid = (PSID)(&ace->SidStart);

		if (owner_sid && EqualSid(sid, owner_sid)) {
			mode |= access_to_mode(ace->Mask);
		} else if (group_sid && EqualSid(sid, group_sid)) {
			mode |= (access_to_mode(ace->Mask) >> 3);
		} else if (EqualSid(sid, &everyone.sid)) {
			mode |= (access_to_mode(ace->Mask) >> 6);
		}
	}

	return mode;
}

/**************************************************************************
 *                                                                        *
 *                       Win32 Error reporting                            *
 *                                                                        *
 **************************************************************************/
/**
 * get_errcode_from_w32err() - translate Win32 error into error code
 * @w32err:     error value returned by GetLastError()
 *
 * Return: translated error code usable in mm_raise_error()
 */
LOCAL_SYMBOL
int get_errcode_from_w32err(DWORD w32err)
{
	switch (w32err) {

	case ERROR_TOO_MANY_OPEN_FILES:
	case WSAEMFILE:                 return EMFILE;
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:      return EEXIST;
	case ERROR_INVALID_NAME:
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
	case ERROR_BAD_EXE_FORMAT:      return ENOEXEC;
	case WSAESHUTDOWN:
	case ERROR_NO_DATA:
	case ERROR_BROKEN_PIPE:         return EPIPE;
	case ERROR_NOT_SAME_DEVICE:     return EXDEV;
	case WSASYSNOTREADY:
	case ERROR_DIRECTORY_NOT_SUPPORTED: return EISDIR;
	case ERROR_NOT_SUPPORTED:       return ENOTSUP;
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
	case WSAESOCKTNOSUPPORT:
	case WSAEPROTOTYPE:             return EPROTOTYPE;
	case WSAEPROTONOSUPPORT:        return EPROTONOSUPPORT;
	case WAIT_TIMEOUT:
	case WSAETIMEDOUT:              return ETIMEDOUT;
	case ERROR_NO_UNICODE_TRANSLATION: return EILSEQ;

	default:
		return EIO;
	}
}


/**
 * write_w32err_msg() - write error message associated to Win32 error
 * @w32err:     Win32 error code (for example from GetLastError())
 * @len:        length of buffer in @errmsg
 * @buff:       buffer that must receive the error message
 *
 * This writes in @buff the error message associated with Win32 error code
 * specified by @w32err. If @len is too short, the error message will be
 * truncated. The resulting string will be null-terminated (if @len is not 0).
 */
LOCAL_SYMBOL
void write_w32err_msg(DWORD w32err, size_t len, char* buff)
{
	if (len == 0)
		return;

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
	              | FORMAT_MESSAGE_IGNORE_INSERTS
	              | FORMAT_MESSAGE_MAX_WIDTH_MASK,
	              NULL, w32err, 0, buff, len-1, NULL);

	// Replace '%' character in win32 message to avoid messing with
	// error message formatting in mm_raise_error_vfull()
	for (; *buff != '\0'; buff++) {
		if (*buff == '%')
			*buff = ' ';
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
	write_w32err_msg(w32err, sizeof(errmsg)-len, errmsg+len);

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
 * is_cygpty_pipe() - Test a pipe handle is a cygwin PTY
 * @hnd:        a handle to a pipe
 *
 * cygwin/msys often use mintty to emulate terminal. When this happens, the
 * handle associated with the terminal is not of the type Console but a
 * named pipe connected to a local server which implements the xterm
 * protocol. The name of the pipe is very specific to it and can be
 * recognized easily. Example of read handle of PTY1 when using msys:
 * \msys-dd50a72ab4668b33-pty1-from-master
 *
 * Return: 1 if @hnd has been recognized to be cygwin/msys PTY, 0 otherwise
 */
static
int is_cygpty_pipe(HANDLE hnd)
{
	char buff[256];
	FILE_NAME_INFO* info = (FILE_NAME_INFO*)buff;
	char* name_u8;
	char orig[8] = "";
	char dir[16] = "";
	int r, len;

	// Get name associated with the handle... Advertised buffer size is
	// smaller to ensure we can write null terminator at the end
	// of filename. Also we don't bother to get the name if buffer is
	// not sufficient, because in this case, the name definitively does
	// not fit the PTY name pattern
	if (!GetFileInformationByHandleEx(hnd, FileNameInfo,
	                                  info, sizeof(buff)-sizeof(WCHAR)))
		return 0;

	// Add null terminator to filename
	len = info->FileNameLength/sizeof(*info->FileName);
	info->FileName[len] = L'\0';

	// Convert filename in utf8
	name_u8 = alloca(len + 1);
	if (conv_utf16_to_utf8(name_u8, len+1, info->FileName) < 0)
		return 0;

	// PTY from cygwin/msys have a name of the form:
	// \(cygwin|msys)-[number_in_hexa]-ptyN-(from-master|to-master)
	// (the actual \Device\NamedPipe part is striped by
	// GetFileInformationByHandleEx(FileNameInfo)
	r = sscanf(name_u8,
	           "\\%7[a-z]-%*[0-9a-f]-pty%*u-%15[a-z]-master",
	           orig,
	           dir);
	if ((r == 2)
	    && (!strcmp(orig, "msys") || !strcmp(orig, "cygwin"))
	    && (!strcmp(dir, "from") || !strcmp(dir, "to")))
		return 1;

	return 0;
}


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
static NOINLINE
int guess_fd_info(int fd)
{
	int info, tmode;
	HANDLE hnd;
	DWORD mode, type;

	hnd = (HANDLE)_get_osfhandle(fd);
	if (hnd == INVALID_HANDLE_VALUE)
		return -1;

	type = GetFileType(hnd);

	info = FD_TYPE_NORMAL;
	if ((type == FILE_TYPE_CHAR) && GetConsoleMode(hnd, &mode)) {
		info = FD_TYPE_CONSOLE | FD_FLAG_TEXT | FD_FLAG_ISATTY;
		goto exit;
	} else if (type == FILE_TYPE_PIPE) {
		info = FD_TYPE_PIPE;
		if (is_cygpty_pipe(hnd)) {
			info |= FD_FLAG_ISATTY;
			goto exit;
		}
	}

	// Detect whether the file is in text mode
	tmode = _setmode(fd, _O_BINARY);
	if (tmode != -1) {
		_setmode(fd, tmode);
		if (tmode != _O_BINARY)
			info |= FD_FLAG_TEXT;
	}

exit:
	set_fd_info(fd, info);
	return info;
}

// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setmaxstdio
#define MAX_FD  2048


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

	if ((fd < 0) || (fd >= MAX_FD))
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


/**
 * close_all_known_fds() - forcingly close all open file descriptors
 *
 * Use this function only in case of unusual cleanup process.
 * Beware: this is not thread safe. Use only this function if the other
 * threads cannot oipen new fds or if it does not matter
 */
LOCAL_SYMBOL
void close_all_known_fds(void)
{
	int fd;

	for (fd = 0; fd < MAX_FD; fd++) {
		if (fd_infos[fd] == FD_TYPE_UNKNOWN)
			continue;

		_close(fd);
		fd_infos[fd] = FD_TYPE_UNKNOWN;
	}
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


