/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "file-internal.h"
#include "mmsysio.h"
#include "mmerrno.h"
#include "mmlog.h"
#include "mmlib.h"
#include "utils-win32.h"

#include <windows.h>
#include <process.h>
#include <processthreadsapi.h>
#include <io.h>
#include <synchapi.h>
#include <signal.h>
#include <stdbool.h>
#include <uchar.h>

#define DEFAULT_PATHSEARCH "C:\\Windows\\system32;C:\\Windows;"

/* __osfile flag values for DOS file handles */

#define FOPEN           0x01    /* file handle open */
#define FEOFLAG         0x02    /* end of file has been encountered */
#define FCRLF           0x04    /* CR-LF across read buffer (in text mode) */
#define FPIPE           0x08    /* file handle refers to a pipe */
#define FNOINHERIT      0x10    /* file handle opened _O_NOINHERIT */
#define FAPPEND         0x20    /* file handle opened O_APPEND */
#define FDEV            0x40    /* file handle refers to device */
#define FTEXT           0x80    /* file handle is in text mode */

#define CRT_BUFFER_SIZE(nh)   ( sizeof(int)                       \
	                        + (nh) * sizeof(unsigned char)    \
	                        + (nh) * sizeof(HANDLE)           \
	                        + (nh) * sizeof(unsigned char))
static const struct mm_remap_fd std_fd_mappings[] = {{0, 0}, {1, 1}, {2, 2}};


/**
 * startup_config - configuration of child startup info
 * @num_hnd:        number of WIN32 handle inherited in child process
 * @num_fd:         maximum number of inherited file descriptor
 * @num_crt_fd:     length of @crt_fd_hnds and @crt_fd_flags
 * @inherited_hnds: array of @num_hnd WIN32 handle that child must inherit
 * @crt_buff:       buffer holding the data to pass to &STARTUPINFO.cbReserved2
 * @crt_fd_hnds:    array of handle of each child fd (each element may
 *                  INVALID_HANDLE_VALUE if not inherited). Points in @crt_buff
 * @crt_fd_flags:   array of flags indicating the type of CRT fd inherited
 *                  in the child process. Points in @crt_buff
 * @crt_fd_infos:   mmlib fd type array passed to child, points in @crt_buff
 * @info:           WIN32 structure configured for call of CreateProcess()
 * @attr_list:      helper data holder for call of CreateProcess(). This is
 *                  internally used for setting @info up.
 * @is_attr_init:   true if @attr_list is initialized, false otherwise
 *
 * This structure is meant to keep track and generate the WIN32 structures
 * to create the child process inheriting the right file descriptors and
 * WIN32 handles.
 */
struct startup_config {
	int num_hnd;
	int num_fd;
	int num_crt_fd;
	HANDLE* inherited_hnds;
	BYTE* crt_buff;
	unsigned char* crt_fd_flags;
	HANDLE* crt_fd_hnds;
	unsigned char* crt_fd_infos;
	STARTUPINFOEXW info;
	LPPROC_THREAD_ATTRIBUTE_LIST attr_list;
	bool is_attr_init;
};


/**************************************************************************
 *                                                                        *
 *                     child processes tracking                           *
 *                                                                        *
 **************************************************************************/

/**
 * DOC: Rationale of child process tracking
 *
 * When designing the Win32 size of the API of process creation and
 * termination wait, there were 2 possibilities regarding how a child
 * process must be referenced (what mm_pid_t should represent) :
 *
 *  - either expose a win32 handle
 *  - either expose the process ID (as POSIX part does)
 *
 * Since a process can only be manipulated through handle, for the sake of
 * simplicity the initial versions of the API used to expose an handle.
 * However it quickly become obvious that the handle which is meaningful
 * only in the context of the parent process, was not lacking interesting
 * property of identification (either for reporting/logging or IPC). Then
 * rised the need for a Win32 specific API of handle->pid. This was the
 * proof that exposing handle was not the right approach and show the need
 * PID based manipulation.
 *
 * However if we only expose PID (ie, we forget the handle that are granted
 * at process creation), we run into other issues: the PID can be reused if
 * the process finishes, and it is not sure that can obtain an handle (with
 * sufficient rights) from the PID when we will ask later. To solve those
 * issues, we can simply keep the handle that the process creation provides
 * as internal data (along with PID) and expose the PID. As long as the
 * handle is not closed, we are ensured that the PID is not reused, and
 * whenever we need to wait for the process to finish, we find the handle of
 * process and perform the wait with it.
 *
 * This means that we can wait only for process that are direct child, but
 * the good news is that it is exactly the behavior of POSIX (wait*() can be
 * used only on direct children).
 */

/**
 * struct child_entry - entry for the children list
 * @pid:        Process ID of the child
 * @hnd:        WIN32 handle to the child process
 */
struct child_entry {
	mm_pid_t pid;
	HANDLE hnd;
};


/**
 * struct children_list - children list
 * @num_child:  number of direct child process
 * @num_max:    length of allocated @entries array
 * @lock:       lock protecting the list update
 * @entries:    array of @num_child child entries
 */
struct children_list {
	int num_child;
	int num_max;
	SRWLOCK lock;
	struct child_entry* entries;
};

// Global list of children that have been created with mmlib
static struct children_list children_list = {.lock = SRWLOCK_INIT};


/**
 * add_to_children_list() - add a new child in the children list
 * @pid:        process ID of the child to add
 * @hnd:        handle to process to add to list
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly
 */
static
int add_to_children_list(mm_pid_t pid, HANDLE hnd)
{
	struct children_list* list = &children_list;
	struct child_entry * entries, * entry;
	int nmax, retval;

	AcquireSRWLockExclusive(&list->lock);

	// Check that allocated array of entries is large enough to
	// accommodate a new element. Resize it if necessary.
	retval = 0;
	if (list->num_child >= list->num_max) {
		nmax = (list->num_max != 0) ? (list->num_max * 2) : 16;
		entries = realloc(list->entries, nmax*sizeof(*entries));
		if (!entries) {
			retval = mm_raise_from_errno("Cannot alloc child");
			goto exit;
		}

		list->entries = entries;
		list->num_max = nmax;
	}

	// Register the new child (ie keep link between pid and handle)
	entry = &list->entries[list->num_child++];
	entry->pid = pid;
	entry->hnd = hnd;

exit:
	ReleaseSRWLockExclusive(&children_list.lock);
	return retval;
}


/**
 * get_handle_from_children_list() - get handle of a child
 * @pid:        process ID of a direct child
 *
 * Return: handle of the child if found in the children list, NULL
 * if not found.
 */
static
HANDLE get_handle_from_children_list(mm_pid_t pid)
{
	struct children_list* list = &children_list;
	int i;
	HANDLE hnd;

	AcquireSRWLockExclusive(&list->lock);

	// Search for index of child with matching pid
	hnd = NULL;
	for (i = 0; i < list->num_child; i++) {
		if (list->entries[i].pid == pid) {
			hnd = list->entries[i].hnd;
			break;
		}
	}

	ReleaseSRWLockExclusive(&children_list.lock);

	return hnd;
}


/**
 * drop_child_from_children_list() - remove child for children list
 * @pid:        process ID of child to drop
 */
static
void drop_child_from_children_list(mm_pid_t pid)
{
	struct child_entry* entries;
	int i, num_child;

	AcquireSRWLockExclusive(&children_list.lock);

	num_child = children_list.num_child;
	entries = children_list.entries;

	// Search for index of child with matching pid
	for (i = 0; i < num_child; i++) {
		if (entries[i].pid == pid) {
			// Remove matching entry
			memmove(entries + i, entries + i+1,
			        (num_child-i-1)*sizeof(*entries));
			children_list.num_child--;
			break;
		}
	}

	ReleaseSRWLockExclusive(&children_list.lock);

	// if i is equal or bigger to num_child, no entries has been found.
	// This should never happen
	mm_check(i < num_child);
}


/**************************************************************************
 *                                                                        *
 *                     Startup info manipulation                          *
 *                                                                        *
 **************************************************************************/

/**
 * DOC: file descriptors inherited in a new Win32 process
 *
 * FD passing on Windows
 * ---------------------
 *
 * Like in POSIX, file descriptor on Windows can be inherited from parent
 * into a child at its creation. However file descriptor on Win32 are not a
 * object of the OS, but a construct provided by the CRT (msvcrt of each
 * compiler version or more recently the ucrt). Those are simply a
 * combination of a Win32 handle combined to some metadata (file open,
 * append mode, is it a device...) store in a global list indexed by the
 * file descriptor.
 *
 * While the handle backing the file descriptor inheritance is done by the
 * OS, the actual file descriptor can be inherited in child process only
 * with the CRT and OS cooperating. Enter the &cbReserved2 and &lpReserved2
 * fields of the &STARTUPINFO structure used in CreateProcess() function.
 * In MSDN those members are noted as "Reserved for use by the C Run-time",
 * and lack of documentation. But this is completely documented by the
 * source code of the CRT (available either in Visual Studio or in Windows
 * SDK, see UCRT source code in lowio/ioinit.cpp and exec/spawnv.cpp). The
 * &STARTUPINFO.lpReserved2 field has the follow layout :
 *
 *  +--------+---------------------+---------------------------+
 *  | num_fd |  Array of CRT flags |   Array of win32 handle   |
 *  +--------+---------------------+---------------------------+
 *  |  int   |    num_fd * uchar   |      num_fd * HANDLE      |
 *  +--------+---------------------+---------------------------+
 *
 * The layout is packed without any padding. The arrays correspond to the
 * length whole file descriptor array from 0 to the highest fd inherited (or
 * beyond if the tail of arrays correspond to fd not meant to be opened in
 * child). For each pair of flag and handle indexed at %i in both arrays, if
 * flags has FOPEN (0x01) set and not FNOINHERIT (0x10) and the handle is
 * valid and inheritable in the calling process, a file descriptor %i will
 * be available at startup in the child. &STARTUPINFO.cbReserved2 must
 * correspond to the size of this buffer, ie
 * sizeof(int)+num_fd*(sizeof(char)+sizeof(HANDLE))
 *
 * Please note that this mechanism, although it is not documented in MSDN,
 * cannot be changed by Microsoft without breaking compatibility with
 * existing software: the inheritance of file descriptor is documented in
 * MSDN and must coop with the different CRT used in different compiler
 * version. Consequently this mechanism has not changed since at least
 * Windows 95. (MS has just added layer of validation of the data passed
 * over the versions of Windows).
 *
 * FD passing with mmlib
 * ---------------------
 *
 * Even if a software cannot access to internal FD flags of a CRT, if it knows
 * the type of fd it has to pass, it can simply setup &STARTUPINFO.lpReserved2
 * and call CreateProcess(), the CRT (whichever it is) of the child will accept
 * the data and setup the file descriptors the same way the parent would have
 * called spawnv().
 *
 * mmlib uses this to keep metadata regarding fd opened by itself while keeping
 * use of CRT to handle fd allocation and closing (with _open_osfhandle(),
 * _get_osfhandle(), _close()...). It is done by adding an extra array in the
 * data passed in &STARTUPINFO.lpReserved2. This extra array will be
 * interpreted by mmlib at startup. This mechanism also allows one to cooperate
 * well with piece of code in the same process that do not use mmlib to create
 * its file descriptors. The number of FD to be passed is indeed specified by
 * the first field of &STARTUPINFO.lpReserved2: any data after the array of
 * win32 handle will be ignored by process not using mmlib since the offset
 * and length of this array is solely dependent on number of FD passed.
 */


/**
 * mm_fd_init() - Initializer of fd info array
 *
 * This function is called at startup (before main or during dllmain if
 * dynamically loaded) and initialize the fd information array according to
 * what the parent may have pass to the current process
 */
MM_CONSTRUCTOR(mm_fd_init)
{
	STARTUPINFOW si;
	int fd, num_fd;
	const BYTE* crtbuff;
	const unsigned char* fd_infos;

	// Get startup parameters
	GetStartupInfoW(&si);

	crtbuff = si.lpReserved2;
	if (!crtbuff)
		return;

	// Get num_fd and handle from startup info. memcpy is
	// used because crtbuff is not guaranteed to be aligned
	memcpy(&num_fd, crtbuff, sizeof(num_fd));
	fd_infos = crtbuff + sizeof(int)
	           + num_fd * sizeof(unsigned char)
	           + num_fd * sizeof(HANDLE);

	// Check from buffer size that lpReserved2 has been set by mmlib
	if (si.cbReserved2 != CRT_BUFFER_SIZE(num_fd))
		return;

	// Initialize the fd info array with the data passed in startup info
	for (fd = 0; fd < num_fd; fd++)
		set_fd_info(fd, fd_infos[fd]);
}


/**
 * convert_fdinfo_to_crtflags() - convert mmlib fd info into CRT flags
 * @fdinfo:     mmlib file descriptor info
 *
 * Return: a flag value to be used in the field &STARTUPINFO.cbReserved2 (to
 * be consumed by msvcrt or ucrt)
 */
static
unsigned char convert_fdinfo_to_crtflags(int fdinfo)
{
	unsigned char crtflags;
	int fd_type = fdinfo & FD_TYPE_MASK;

	crtflags = FOPEN;

	switch (fd_type) {
	case FD_TYPE_PIPE:      crtflags |= FPIPE; break;
	case FD_TYPE_CONSOLE:   crtflags |= FDEV; break;
	default: break;
	}

	if (fdinfo & FD_FLAG_APPEND)
		crtflags |= FAPPEND;

	if (fdinfo & FD_FLAG_TEXT)
		crtflags |= FTEXT;

	return crtflags;
}


/**
 * get_highest_child_fd() - get the child fd which the highest index
 * @num_map:     number of mapping element in @fd_map
 * @fd_map:      array of remapping file descriptor between child and parent
 *
 * Return: the value of the highest &mm_remap_fd.child_fd member in the
 * array @fd_map.
 */
static
int get_highest_child_fd(int num_map, const struct mm_remap_fd* fd_map)
{
	int i, fd_max, fd;

	fd_max = -1;
	for (i = 0; i < num_map; i++) {
		fd = fd_map[i].child_fd;
		if (fd > fd_max)
			fd_max = fd;
	}

	return fd_max;
}


/**
 * round_up() - round up a value to the next multiple of a divider
 * @value:	value to round up
 * @divider:    multiple of which @value must be round up
 *
 * Return: the first number bigger or equal to @value that is dividable by
 * @divider.
 */
static
int round_up(int value, int divider)
{
	return ((value + divider-1)/divider)*divider;
}


/**
 * startup_config_get_startup_info() - return a configured STARTUPINFO pointer
 * @cfg:        initialized startup config
 *
 * Use this function after having called startup_config_init() to return a
 * pointer to a configured STARTUPINFO structure, meant to be used in
 * Win32 CreateProcess() API. The pointer returns internal data of @cfg.
 * Also it actually returns a STARTUPINFOEX structure, so it can be used
 * with the EXTENDED_STARTUPINFO_PRESENT flag passed to CreateProcess().
 *
 * Return: pointer to STARTUPINFO configured according the configured
 * startup config.
 */
static
STARTUPINFOW* startup_config_get_startup_info(struct startup_config* cfg)
{
	cfg->info = (STARTUPINFOEXW) {
		.StartupInfo = {
			.cb = sizeof(cfg->info),
			.cbReserved2 = CRT_BUFFER_SIZE(cfg->num_crt_fd),
			.lpReserved2 = cfg->crt_buff,
			.dwFlags = STARTF_USESTDHANDLES,
			.hStdInput = cfg->crt_fd_hnds[0],
			.hStdOutput = cfg->crt_fd_hnds[1],
			.hStdError = cfg->crt_fd_hnds[2],
		},
		.lpAttributeList = cfg->attr_list,
	};

	return &cfg->info.StartupInfo;
}


/**
 * startup_config_alloc_crt_buffs() - allocated and setup CRT buffers
 * @cfg:        being initialized startup config
 *
 * This internal function is meant to allocate the CRT buffer (to be used in
 * &STARTUPINFO.cbReserved2) and setup properly the fields
 * @cfg->crt_fd_flags and @cfg->crt_fd_hnds which will point into the
 * allocated @cfg->crt_buf. It will find an appropriate value of
 * @cfg->num_crt_fd so that @cfg->crt_fd_hnds has an alignment suitable for
 * its data type (HANDLE).
 *
 * Return: 0 in case of success, -1 otherwise with error state set.
 */
static
int startup_config_alloc_crt_buffs(struct startup_config* cfg)
{
	int num_crt_fd;
	int offset;

	// Adjust num_fd so that cfg->crt_fd_hnds is aligned. This is the
	// case if sizeof(int)+num_fd is a multiple of sizeof(HANDLE)
	num_crt_fd = round_up(sizeof(int)+cfg->num_fd, sizeof(HANDLE))
	             - sizeof(int);

	// Allocate a CRT buffer with the adjusted number of file descriptor
	cfg->num_crt_fd = num_crt_fd;
	cfg->crt_buff = calloc(1, CRT_BUFFER_SIZE(cfg->num_crt_fd));
	if (!cfg->crt_buff)
		return mm_raise_error(ENOMEM, "Failed to CRT buffers");

	// By adjustment of num_crt_fd, we are ensured that
	// cfg->crt_fd_hnds pointer is properly aligned on HANDLE.
	offset = 0;
	*(int*)cfg->crt_buff = num_crt_fd;
	offset += sizeof(int);

	cfg->crt_fd_flags = cfg->crt_buff + offset;
	offset += num_crt_fd * sizeof(unsigned char);

	cfg->crt_fd_hnds = (HANDLE*)(cfg->crt_buff + offset);
	offset += num_crt_fd * sizeof(HANDLE);

	cfg->crt_fd_infos = cfg->crt_buff + offset;

	return 0;
}


/**
 * startup_config_allocate_internals() - alloc internal data of startup config
 * @cfg:        being initialized startup config
 *
 * Allocate the internal buffers and create the extra handle and memory
 * mapping. This function is meant to be called in startup_config_init().
 *
 * Return: 0 in case of success, -1 otherwise with error state set.
 */
static
int startup_config_allocate_internals(struct startup_config* cfg)
{
	int max_num_hnd;
	SIZE_T attrlist_bufsize;
	SECURITY_ATTRIBUTES sa = {
		.nLength = sizeof(sa),
		.lpSecurityDescriptor = NULL,
		.bInheritHandle = TRUE,
	};

	max_num_hnd = cfg->num_fd + 1;
	InitializeProcThreadAttributeList(NULL, 1, 0, &attrlist_bufsize);

	if (startup_config_alloc_crt_buffs(cfg))
		return -1;

	// Allocate auxiliary buffers
	cfg->inherited_hnds =
		malloc(max_num_hnd * sizeof(*cfg->inherited_hnds));
	cfg->attr_list = malloc(attrlist_bufsize);
	if (!cfg->inherited_hnds || !cfg->attr_list)
		return mm_raise_error(ENOMEM, "Cannot alloc info buffers");

	// Allocate a process attribute list
	if (!InitializeProcThreadAttributeList(cfg->attr_list, 1, 0,
	                                       &attrlist_bufsize))
		return mm_raise_from_w32err("cannot init attribute list");

	cfg->is_attr_init = true;
	return 0;
}


/**
 * startup_config_allocate_internals() - setup CRT buffer according to remapping
 * @cfg:        being initialized startup config
 * @num_map:    number of element in @fd_map
 * @fd_map:     array of file descriptor mapping elements
 *
 * Configures @cfg->crt_fd_flags, @cfg->crt_fd_hnds and cfg->fd_infos
 * according to the remapping defined in @fd_map
 *
 * Return: 0 in case of success, -1 otherwise with error set accordingly.
 */
static
int startup_config_setup_mappings(struct startup_config* cfg,
                                  int num_map, const struct mm_remap_fd* fd_map)
{
	int i, fd_info, child_fd, parent_fd;
	HANDLE hnd;

	for (i = 0; i < num_map; i++) {
		child_fd = fd_map[i].child_fd;
		parent_fd = fd_map[i].parent_fd;

		if (child_fd < 0) {
			mm_raise_error(EBADF,
			               "fd_map[%i].child_fd=%i is invalid",
			               i,
			               child_fd);
			return -1;
		}

		// If parent_fd is -1, this means that the child_fd must not
		// be inherited in child
		if (parent_fd == -1) {
			cfg->crt_fd_hnds[child_fd] = INVALID_HANDLE_VALUE;
			cfg->crt_fd_flags[child_fd] = 0;
			continue;
		}

		// Get handle of fd in parent
		hnd = (HANDLE)_get_osfhandle(parent_fd);
		if (hnd == INVALID_HANDLE_VALUE) {
			mm_raise_error(EBADF,
			               "fd_map[%i].parent_fd=%i is invalid",
			               i,
			               parent_fd);
			return -1;
		}

		// setup child_fd mapping
		fd_info = get_fd_info(parent_fd);
		cfg->crt_fd_hnds[child_fd] = hnd;
		cfg->crt_fd_flags[child_fd] =
			convert_fdinfo_to_crtflags(fd_info);
		cfg->crt_fd_infos[child_fd] = fd_info;
	}

	return 0;
}


/**
 * startup_config_dup_inherited_hnds() - duplicate inherited handles
 * @cfg:        being initialized startup config
 *
 * This function, meant to be called in startup_config_init(), will populate
 * the array of inherited handle. Since this list must contains only
 * inheritable handle and we cannot know (for sure) if it is the case, we
 * duplicate them unconditionally.
 *
 * Return: 0 in case of success, -1 otherwise with error set accordingly.
 */
static
int startup_config_dup_inherited_hnds(struct startup_config* cfg)
{
	int i;
	BOOL res;
	HANDLE hnd, dup_hnd, proc_hnd;

	proc_hnd = GetCurrentProcess();

	for (i = 0; i < cfg->num_fd; i++) {
		hnd = cfg->crt_fd_hnds[i];

		// Skip if not inherited in child
		if ((hnd == INVALID_HANDLE_VALUE)
		    || !(cfg->crt_fd_flags[i] & FOPEN))
			continue;

		// Duplicate handle: all handle in the attr_list must be
		// inheritable. Since we cannot know it the case or not, we
		// duplicate the handle unconditionally and set
		// inheritability in the duplicated handle
		res = DuplicateHandle(proc_hnd, hnd, proc_hnd, &dup_hnd,
		                      0, TRUE, DUPLICATE_SAME_ACCESS);
		if (res == FALSE)
			return mm_raise_from_w32err("cannot duplicate handle");

		// Add the handle in the inherited list and replace the one
		// in CRT handle list by the duplicated one
		cfg->inherited_hnds[cfg->num_hnd++] = dup_hnd;
		cfg->crt_fd_hnds[i] = dup_hnd;
	}

	// Add the inherited handle list in attribute list
	UpdateProcThreadAttribute(cfg->attr_list,
	                          0,
	                          PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
	                          cfg->inherited_hnds,
	                          cfg->num_hnd*sizeof(*cfg->inherited_hnds),
	                          NULL,
	                          NULL);

	return 0;
}


/**
 * startup_config_deinit() - cleanup a startup config
 * @cfg:        startup config
 *
 * This function cleanup and release all resources used to setup the child
 * process startup config.
 */
static
void startup_config_deinit(struct startup_config* cfg)
{
	int i;

	if (cfg->is_attr_init)
		DeleteProcThreadAttributeList(cfg->attr_list);

	for (i = 0; i < cfg->num_hnd; i++)
		CloseHandle(cfg->inherited_hnds[i]);

	free(cfg->crt_buff);
	free(cfg->inherited_hnds);
	free(cfg->attr_list);

	*cfg = (struct startup_config) {.num_fd = 0};
}


/**
 * startup_config_init() - Initial a child process startup config
 * @cfg:        startup config to init
 * @num_map     number of element in the @fd_map array
 * @fd_map:     array of file descriptor remapping to pass into the child
 *
 * This function allocate and configure all data necessary to setup a
 * STARTUPINFOEX structure meant to be used in a call to CreateProcess().
 *
 * Return: 0 in case of success, -1 otherwise with error state set.
 */
static
int startup_config_init(struct startup_config* cfg,
                        int num_map, const struct mm_remap_fd* fd_map)
{
	int num_fd;

	num_fd = get_highest_child_fd(num_map, fd_map)+1;

	// We need to take consider at least std file descriptors
	if (num_fd < 3)
		num_fd = 3;

	*cfg = (struct startup_config) {
		.num_fd = num_fd,
	};

	if (startup_config_allocate_internals(cfg)
	    || startup_config_setup_mappings(cfg, MM_NELEM(std_fd_mappings),
	                                     std_fd_mappings)
	    || startup_config_setup_mappings(cfg, num_map, fd_map)
	    || startup_config_dup_inherited_hnds(cfg)) {
		startup_config_deinit(cfg);
		return -1;
	}

	return 0;
}


/**
 * concat_strv() - Concatenate an array of null-terminated strings
 * @strv:       NULL-terminated array of null-terminated strings (may be NULL)
 * @sep:        separator to include between the strings
 *
 * This function transform into UTF-16 and concatenate the strings found in
 * the @strv arrayThe character @sep will be put between each concatenated
 * strings.
 *
 * Return: NULL if @strv is NULL, otherwise the concatenated string encoded
 * in UTF-16
 */
static
char16_t* concat_strv(char* const* strv, char16_t sep)
{
	int i, len, tot_len, rem_len;
	char16_t * concatstr, * ptr;

	// This is a legit possibility (for example envp can be NULL and this
	// is the value that must then be returned)
	if (!strv)
		return NULL;

	// Compute the total length for allocating the concatanated string
	// (including null termination)
	tot_len = 1;
	for (i = 0; strv[i]; i++) {
		len = get_utf16_buffer_len_from_utf8(strv[i]);
		if (len < 0)
			return NULL;

		tot_len += len;
	}

	concatstr = malloc(tot_len*sizeof(*concatstr));
	if (!concatstr)
		return NULL;

	// Do actual concatenation. Only the first element will not be prefixed
	// by the separator
	ptr = concatstr;
	for (i = 0; strv[i]; i++) {
		if (i != 0)
			*(ptr++) = sep;

		rem_len = tot_len - (ptr - concatstr);
		len = conv_utf8_to_utf16(ptr, rem_len, strv[i]);
		ptr += len-1;
	}

	// Null terminate the concatanated string
	*ptr = L'\0';

	return concatstr;
}


/**
 * set_char() - write char in a buffer offset if buffer is not NULL
 * @str:        string buffer (can be NULL)
 * @offset:     offset in @str at which the character must be written
 * @c:          byte to write
 *
 * If @str is NULL, this function does nothing.  This simple function is
 * actually an helper to escape_cmd_str() to handle the case when the
 * destination string is NULL
 */
static
void set_char(char* str, int offset, char c)
{
	if (str)
		str[offset] = c;
}


/**
 * escape_cmd_str() - espace a command line argument string
 * @dst:        buffer where to write escaped string (can be NULL)
 * @src:        NULL terminated string that must be escaped.
 *
 * This function takes the string @src and transform it into an escaped
 * string suitable for consumption in CreateProcess() command line argument
 * such a way that the string will be recognized as a unique argument
 * following the interpretation detailed at
 * https://docs.microsoft.com/en-us/windows/desktop/api/shellapi/nf-shellapi-commandlinetoargvw
 *
 * Please note that the terminal byte will be written in the escaped string
 * and is taken into account in the returned number of bytes written.
 *
 * If @dst is NULL, nothing will be written @dst but the number of bytes
 * required to generate the escaped string will be returned.
 *
 * Return: The number of byte written or needed by @dst.
 */
static
int escape_cmd_str(char* restrict dst, const char* restrict src)
{
	int i = 0, nbackslash = 0;
	char c;

	// Write initial quotation mark
	set_char(dst, i++, '"');

	while (1) {
		c = *src++;
		if (c == '\0')
			break;

		switch (c) {
		case '\\':
			nbackslash++;
			break;

		case '"':
			while (nbackslash) {
				set_char(dst, i++, '\\');
				set_char(dst, i++, '\\');
				nbackslash--;
			}

			set_char(dst, i++, '\\');
			set_char(dst, i++, '"');
			break;

		default:
			while (nbackslash) {
				set_char(dst, i++, '\\');
				nbackslash--;
			}

			set_char(dst, i++, c);
			break;
		}
	}

	// Write final quotation mark
	while (nbackslash) {
		set_char(dst, i++, '\\');
		set_char(dst, i++, '\\');
		nbackslash--;
	}

	set_char(dst, i++, '"');
	set_char(dst, i++, '\0');

	return i;
}


/**
 * escape_argv() - clone argument array into an escaped argument array
 * @argv:       NULL terminated array of NULL terminated argument strings
 *
 * Generate an argument array identical to @argv excepting that the string
 * will be escaped (with quotation mark) so that when concatenated and
 * separated by space, the resulting commandline (after UTF-16
 * transformation), when supplied to CommandLineToArgvW(), the generate
 * argument will be the same as @argv (modulo the UTF-8/UTF-16
 * transformation).
 *
 * This transformation is necessary because without, an argument containing
 * one or more spaces will be parse/split into 2 or more argument in the
 * child process.
 *
 * The resulting array in allocated in one block of memory which contains
 * both the array of string pointer and the different strings.
 *
 * Return: the escaped argument array in case of success. This must be
 * cleanup by a sole call to free() with the returned pointer. NULL in case
 * of failure with error state set accordingly.
 */
static
char** escape_argv(char* const* argv)
{
	char** esc_argv;
	char * ptr, * newptr;
	intptr_t* index_strv;
	int i, len, index, maxlen, argc;

	// Count number of argument in the array
	for (argc = 0; argv[argc]; argc++)
		;

	// Do an initial allocation
	maxlen = 256;
	ptr = malloc(maxlen);
	if (!ptr)
		goto failure;

	index_strv = (intptr_t*)ptr;
	index = (argc+1)*sizeof(char*);
	for (i = 0; i < argc; i++) {
		// Save offset of the i-th argument string
		index_strv[i] = index;

		// Compute the space needed for the escaped string
		len = escape_cmd_str(NULL, argv[i]);

		// Check allocated block suffices and realloc if necessary
		if (index + len > maxlen) {
			// Readjust maxlen to ensure new block fits
			for (; maxlen < index + len; maxlen *= 2)
				;

			// Realloc memory block
			newptr = realloc(ptr, maxlen);
			if (!newptr)
				goto failure;

			ptr = newptr;
			index_strv = (intptr_t*)ptr;
		}

		// Write the actual escaped string
		index += escape_cmd_str(ptr+index, argv[i]);
	}

	// Transform offset of allocated memory wrt base pointer of the
	// allocated memory into actual string pointer array
	esc_argv = (char**)index_strv;
	for (i = 0; i < argc; i++)
		esc_argv[i] = ptr + index_strv[i];

	// Null terminate the array of pointer.
	esc_argv[argc] = NULL;
	return esc_argv;

failure:
	free(ptr);
	mm_raise_from_errno("failed to escape argv");
	return NULL;
}


/**
 * translate_exitcode_in_status() - translate Win32 exit code into mmlib one
 * @exitcode:	Win32 exitcode
 *
 * This function will identify the error case from @exitcode and translate
 * them into a signal value if applicable, or return the reported exitcode
 * in case of normal exit.
 *
 * Return: a status code meant to be returned by mm_wait_process()
 */
static
int translate_exitcode_in_status(DWORD exitcode)
{
	int status = 0;

	// Normal exit case
	if (exitcode < 0xC0000000) {
		status = exitcode & ~MM_WSTATUS_CODEMASK;
		status |= MM_WSTATUS_EXITED;
		return status;
	}

	// Error cases
	switch (exitcode) {
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	case EXCEPTION_GUARD_PAGE:
	case EXCEPTION_STACK_OVERFLOW:
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		status = SIGSEGV;
		break;

	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_STACK_CHECK:
	case EXCEPTION_FLT_UNDERFLOW:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_OVERFLOW:
		status = SIGFPE;
		break;

	case EXCEPTION_ILLEGAL_INSTRUCTION:
		status = SIGILL;
		break;

	case STATUS_CONTROL_C_EXIT:
		status = SIGINT;
		break;

	default:
		status = SIGABRT;
		break;
	}

	status |= MM_WSTATUS_SIGNALED;
	return status;
}


/**
 * contains_dirsep() - indicate whether argument has a directory separator
 * @path:       string to test
 *
 * Return: 1 if @path has at least one \ or / character
 */
static
int contains_dirsep(const char* path)
{
	char c;

	for (; *path != '\0'; path++) {
		c = *path;
		if (is_path_separator(c))
			return 1;
	}

	return 0;
}


/**
 * search_bin_in_path() - search executable in folders listed in PATH
 * @base:       basename of the executable
 *
 * Return: the path of the executable found on an allocated block of memory
 * in case of success. In such case the path returned must be cleanup using
 * free() when no longer needed. In case of failure, NULL is returned with
 * error state set accordingly.
 */
static
char* search_bin_in_path(const char* base)
{
	const char * dir, * eos;
	char * path, * new_path, * ext;
	int baselen, dirlen, len, maxlen, error, rv, i;
	const char* search_exts[] = {"", ".exe"};

	baselen = strlen(base);
	path = NULL;
	maxlen = 0;

	// Get the PATH environment variable.
	dir = mm_getenv("PATH", DEFAULT_PATHSEARCH);

	error = ENOENT;
	while (dir[0] != '\0') {
		// Compute length of the dir component (ie until next ;)
		eos = strchr(dir, ';');
		dirlen = eos ? (eos - dir) : (int)strlen(dir);

		// Realloc path if too small (take into account the extension)
		len = dirlen + baselen + 6;
		if (len > maxlen) {
			new_path = realloc(path, len);
			if (!new_path) {
				mm_raise_from_errno("Cannot allocate path");
				free(path);
				return NULL;
			}

			path = new_path;
			maxlen = len;
		}

		// Form the path base on the dir component
		memcpy(path, dir, dirlen);
		path[dirlen] = '\\';
		memcpy(path + dirlen + 1, base, baselen+1);

		// Get pointer to extension substring in path
		ext = path + dirlen + 1 + baselen;

		// Search a candidate by trying all the possible extension
		for (i = 0; i < MM_NELEM(search_exts); i++) {
			strcpy(ext, search_exts[i]);

			// Check the candidate path exist and can be run
			rv = mm_check_access(path, X_OK);
			if (rv == 0)
				return path;

			if (rv == EACCES)
				error = EACCES;
		}

		// Move to next dir component (maybe end of list)
		dir += dirlen;
		if (dir[0] == ';')
			dir++;
	}

	mm_raise_error(error, "Cannot find %s executable in PATH", base);
	free(path);
	return NULL;
}

/**************************************************************************
 *                                                                        *
 *                     Spawn functions and wait                           *
 *                                                                        *
 **************************************************************************/


/**
 * spawn_process() - spawn a new process
 * @pid:        pointer to variable that will receive PID of new process
 * @path:       path to the executable file
 * @num_map     number of element in the @fd_map array
 * @fd_map:     array of file descriptor remapping to pass into the child
 * @flags:      spawn flags
 * @argv:       null-terminated array of string containing the command
 *              arguments (starting with command). Can be NULL.
 * @envp:       null-terminated array of strings specifying the environment
 *              of the executed program. If it is NULL, it inherit its
 *              environment from the calling process
 *
 * Backend to mm_spawn(), this will have the same behavior as mm_spawn() but
 * return an handle to the created process.
 *
 * Return: handle to the created process in case of success,
 * INVALID_HANDLE_VALUE in case of failure with error state set accordingly.
 */
static
HANDLE spawn_process(DWORD* pid, const char* path,
                     int num_map, const struct mm_remap_fd* fd_map,
                     char* const* argv, char* const* envp)
{
	PROCESS_INFORMATION proc_info;
	struct startup_config cfg;
	BOOL res;
	int path_u16_len;
	char16_t* path_u16;
	char16_t* cmdline = NULL;
	char16_t* concat_envp = NULL;
	HANDLE proc_hnd = INVALID_HANDLE_VALUE;

	path_u16_len = get_utf16_buffer_len_from_utf8(path);
	if (path_u16_len < 0) {
		mm_raise_from_w32err("Invalid UTF-8 path");
		goto exit;
	}

	// Transform the provided argument and environment arrays into their
	// concatanated form: argument must be separated by spaces and
	// environment variables by '\0' (See doc of CreateProcess()).
	// envp is allowed to be null (meaning keep same env for child), and
	// NULL must then be passed to CreateProcees()
	cmdline = concat_strv(argv, L' ');
	concat_envp = concat_strv(envp, L'\0');
	if (!cmdline || (envp && !concat_envp)) {
		mm_raise_from_w32err("Failed to format cmdline or environment");
		goto exit;
	}

	// Fill the STARTUPINFO struct. The list of inherited handle will be
	// written there.
	if (startup_config_init(&cfg, num_map, fd_map))
		goto exit;

	// Create process with a temporary UTF-16 version of path
	path_u16 = mm_malloca(path_u16_len * sizeof(*path_u16));
	conv_utf8_to_utf16(path_u16, path_u16_len, path);
	res = CreateProcessW(path_u16,
	                     cmdline,
	                     NULL,
	                     NULL,
	                     TRUE,
	                     EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
	                     concat_envp,
	                     NULL,
	                     startup_config_get_startup_info(&cfg),
	                     &proc_info);
	mm_freea(path_u16);

	// We no longer need the configured STARTUPINFO
	startup_config_deinit(&cfg);

	if (res == FALSE) {
		mm_raise_from_w32err("Cannot exec \"%s\"", path);
		goto exit;
	}

	// We use only the handle of new process, no its main thread
	CloseHandle(proc_info.hThread);
	proc_hnd = proc_info.hProcess;
	*pid = proc_info.dwProcessId;

exit:
	free(cmdline);
	free(concat_envp);
	return proc_hnd;
}


/* doc in posix implementation */
API_EXPORTED
int mm_spawn(mm_pid_t* child_pid, const char* path,
             int num_map, const struct mm_remap_fd* fd_map,
             int flags, char* const* argv, char* const* envp)
{
	char* default_argv[] = {(char*)path, NULL};
	char** esc_argv;
	HANDLE hnd;
	DWORD pid;
	int retval = -1;
	char* actual_path = NULL;

	if (!path)
		return mm_raise_error(EINVAL, "path must not be NULL");

	if (flags & ~(MM_SPAWN_KEEP_FDS | MM_SPAWN_DAEMONIZE))
		return mm_raise_error(EINVAL, "Invalid flags (%08x)", flags);

	if (flags & MM_SPAWN_KEEP_FDS)
		return mm_raise_error(ENOTSUP, "MM_SPAWN_KEEP_FDS "
		                      "not supported yet");

	if (!argv)
		argv = default_argv;

	esc_argv = escape_argv(argv);
	if (!esc_argv)
		return -1;

	// Search executable in PATH if path does not contains dirsep
	if (!contains_dirsep(path)) {
		path = actual_path = search_bin_in_path(path);
		if (!path)
			goto exit;
	}

	hnd = spawn_process(&pid, path, num_map, fd_map, esc_argv, envp);
	if (hnd == INVALID_HANDLE_VALUE)
		goto exit;

	if (child_pid && !(flags & MM_SPAWN_DAEMONIZE)) {
		add_to_children_list(pid, hnd);
		*child_pid = pid;
	} else {
		CloseHandle(hnd);
	}

	retval = 0;

exit:
	free(actual_path);
	free(esc_argv);
	return retval;
}


/* doc in posix implementation */
API_EXPORTED
int mm_wait_process(mm_pid_t pid, int* status)
{
	DWORD exitcode;
	HANDLE hnd;

	hnd = get_handle_from_children_list(pid);
	if (hnd == NULL) {
		mm_raise_error(ECHILD, "Cannot find process %lli in the list "
		               "of children", pid);
		return -1;
	}

	// Wait for the process to be signaled (ie to stop)
	WaitForSingleObject(hnd, INFINITE);

	GetExitCodeProcess(hnd, &exitcode);

	drop_child_from_children_list(pid);
	CloseHandle(hnd);

	if (status)
		*status = translate_exitcode_in_status(exitcode);

	return 0;
}


/**************************************************************************
 *                                                                        *
 *                          mm_execv() implementation                     *
 *                                                                        *
 **************************************************************************/
static HANDLE child_hnd_to_exit = INVALID_HANDLE_VALUE;

MM_DESTRUCTOR(waited_child)
{
	union {
		LPTHREAD_START_ROUTINE thr_start;
		FARPROC farproc;
	} cast_fn;
	HMODULE hmod;

	if (child_hnd_to_exit == INVALID_HANDLE_VALUE)
		return;

	// Get pointer of ExitProcess in this process. Given how windows load
	// dll, we are ensured that the address will be the same in the child
	// process. union is used to cast function type to avoid compiler
	// warnings (no compiler should warn this).
	hmod = GetModuleHandle("kernel32.dll");
	cast_fn.farproc = GetProcAddress(hmod, "ExitProcess");

	// Create remotely a thread in child process that will call
	// ExitProcess(STATUS_CONTROL_C_EXIT);
	CreateRemoteThread(child_hnd_to_exit, NULL, 0, cast_fn.thr_start,
	                   (LPVOID)STATUS_CONTROL_C_EXIT, 0, NULL);

	CloseHandle(child_hnd_to_exit);
	child_hnd_to_exit = NULL;
}


/* doc in posix implementation */
API_EXPORTED
int mm_execv(const char* path,
             int num_map, const struct mm_remap_fd* fd_map,
             int flags, char* const* argv, char* const* envp)
{
	DWORD exitcode;
	mm_pid_t pid;
	HANDLE hnd;

	if (!path)
		return mm_raise_error(EINVAL, "path must not be NULL");

	if (flags & ~MM_SPAWN_KEEP_FDS)
		return mm_raise_error(EINVAL, "Invalid flags (%08x)", flags);

	if (mm_spawn(&pid, path, num_map, fd_map, flags, argv, envp))
		return -1;

	// Get handle of child process (and remove it from list) and mark
	// it as the process to exit if the current process receive an
	// early exit
	hnd = child_hnd_to_exit = get_handle_from_children_list(pid);
	drop_child_from_children_list(pid);

	close_all_known_fds();

	// Wait for the process to be signaled (ie to stop)
	WaitForSingleObject(hnd, INFINITE);
	child_hnd_to_exit = INVALID_HANDLE_VALUE;

	// Transfer child process exit code untouched
	GetExitCodeProcess(hnd, &exitcode);
	CloseHandle(hnd);
	ExitProcess(exitcode);
}
