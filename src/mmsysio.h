/*
   @mindmaze_header@
*/
#ifndef MMSYSIO_H
#define MMSYSIO_H

#include <stddef.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "mmpredefs.h"

#ifdef _WIN32
// structure for scatter-gather RW operation
// on win32 this is guaranteed to alias to WSABUF
struct iovec {
	unsigned long iov_len;
	void* iov_base;
};

#ifdef _MSC_VER
#  ifndef _SSIZE_T_DEFINED
#    define _SSIZE_T_DEFINED
#    undef ssize_t
#    ifdef _WIN64
  typedef __int64 ssize_t;
#    else
  typedef int ssize_t;
#    endif /* _WIN64 */
#  endif /* _SSIZE_T_DEFINED */
#endif

#endif

typedef long long mm_off_t;


#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 *                          File manipulation                             *
 **************************************************************************/

#ifdef _WIN32

#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY _O_WRONLY
#endif
#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_APPEND
#define O_APPEND _O_APPEND
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC _O_TRUNC
#endif
#ifndef O_EXCL
#define O_EXCL _O_EXCL
#endif

#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif
#ifndef S_IFREG
#define S_IFREG _S_IFREG
#endif

#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IXUSR
#define S_IXUSR _S_IEXEC
#endif
#ifndef S_IRWXU
#define S_IRWXU (S_IRUSR|S_IWUSR|S_IXUSR)
#endif

#ifndef F_OK
#define F_OK 0x00
#endif
#ifndef X_OK
#define X_OK 0x01
#endif
#ifndef W_OK
#define W_OK 0x02
#endif
#ifndef R_OK
#define R_OK 0x04
#endif

#endif

struct mm_stat {
	int mode;
	int nlink;
	mm_off_t filesize;
	time_t ctime;
	time_t mtime;
};


/**
 * mm_open() - Open file
 * @path:       path to file to open
 * @oflag:      control flags how to open the file
 * @mode:       access permission bits is file is created
 *
 * This function creates an open file description that refers to a file and
 * a file descriptor that refers to that open file description. The file
 * descriptor is used by other I/O functions to refer to that file. The
 * @path argument points to a pathname naming the file.
 *
 * The file status flags and file access modes of the open file description
 * are set according to the value of @oflag, which is constructed by a
 * bitwise-inclusive OR of flags from the following list. It must specify
 * exactly one of the first 3 values.
 *
 * %O_RDONLY
 *   Open for reading only.
 * %O_WRONLY
 *   Open for writing only.
 * %O_RDWR
 *   Open for reading and writing. The result is undefined if this flag is
 *   applied to a FIFO.
 * 
 * Any combination of the following may be used
 *
 * %O_APPEND
 *   If set, the file offset shall be set to the end of the file prior to
 *   each write.
 * %O_CREAT
 *   If the file exists, this flag has no effect except as noted under
 *   %O_EXCL below. Otherwise, the file is created as a regular file; the
 *   user ID of the file shall be set to the effective user ID of the
 *   process; the group ID of the file shall be set to the group ID of the
 *   file's parent directory or to the effective group ID of the process;
 *   and the access permission bits of the file mode are set by the @mode
 *   argument modified by a bitwise AND with the umask of the process. This
 *   @mode argument does not affect whether the file is open for reading,
 *   writing, or for both.
 * %O_TRUNC
 *   If the file exists and is a regular file, and the file is successfully
 *   opened %O_RDWR or %O_WRONLY, its length is truncated to 0, and the
 *   mode and owner are unchanged. The result of using O_TRUNC without
 *   either %O_RDWR or %O_WRONLY is undefined.
 * %O_EXCL
 *   If %O_CREAT and %O_EXCL are set, mm_open() fails if the file exists.
 *   The check for the existence of the file and the creation of the file if
 *   it does not exist is atomic with respect to other threads executing
 *   mm_open() naming the same filename in the same directory with %O_EXCL
 *   and %O_CREAT set.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_open(const char* path, int oflag, int mode);


/**
 * mm_close() - Close a file descriptor
 * @fd:         file descriptor to close
 *
 * This function deallocates the file descriptor indicated by @fd, ie it
 * makes the file descriptor available for return by subsequent calls to
 * mm_open() or other system functions that allocate file descriptors.
 *
 * If a memory mapped file or a shared memory object remains referenced at
 * the last close (that is, a process has it mapped), then the entire
 * contents of the memory object persists until the memory object becomes
 * unreferenced. If this is the last close of a memory mapped file or a
 * shared memory object and the close results in the memory object becoming
 * unreferenced, and the memory object has been unlinked, then the memory
 * object will be removed.
 *
 * If @fd refers to a socket, mm_close() causes the socket to be destroyed.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_close(int fd);


/**
 * mm_fsync() - synchronize changes to a file
 * @fd:         file description to synchronize
 *
 * This requests that all data for the open file descriptor named by @fd is
 * to be transferred to the storage device associated with the file
 * described by @fd. The mm_fsync() function does not return until the
 * system has completed that action or until an error is detected.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_fsync(int fd);


/**
 * mm_read() - Reads data from a file descriptor
 * @fd:         file descriptor to read from
 * @buf:        storage location for data
 * @nbyte:      maximum size to read
 *
 * mm_read() attempts to read @nbyte bytes from the file associated with the
 * open file descriptor, @fd, into the buffer pointed to by @buf.
 *
 * On files that support seeking (for example, a regular file), the
 * mm_read() starts at a position in the file given by the file offset
 * associated with @fd. The file offset will incremented by the number of
 * bytes actually read.  * No data transfer will occur past the current
 * end-of-file. If the starting position is at or after the end-of-file, 0
 * is returned.
 *
 * If @fd refers to a socket, mm_read() shall be equivalent to mm_recv()
 * with no flags set.
 *
 * Return: Upon successful completion, a non-negative integer is returned
 * indicating the number of bytes actually read. Otherwise, -1 is returned
 * and error state is set accordingly
 */
MMLIB_API ssize_t mm_read(int fd, void* buf, size_t nbyte);


/**
 * mm_read() - Write data to a file descriptor
 * @fd:         file descriptor to write to
 * @buf:        storage location for data
 * @nbyte:      amount of data to write
 *
 * The mm_write() function attempts to write @nbyte bytes from the buffer
 * pointed to by @buf to the file associated with the open file descriptor,
 * @fd.
 *
 * On a regular file or other file capable of seeking, the actual writing of
 * data shall proceed from the position in the file indicated by the file
 * offset associated with @fd. Before successful return from mm_write(), the
 * file offset is incremented by the number of bytes actually written.
 *
 * If the %O_APPEND flag of the file status flags is set, the file offset is set
 * to the end of the file prior to each write and no intervening file
 * modification operation will occur between changing the file offset and the
 * write operation.
 *
 * Write requests to a pipe or FIFO shall be handled in the same way as a
 * regular file with the following exceptions
 *
 * - there is no file offset associated with a pipe, hence each write request
 * shall append to the end of the pipe.
 * - write requests of pipe buffer size bytes or less will not be interleaved
 * with data from other processes doing writes on the same pipe.
 * - a write request may cause the thread to block, but on normal completion it
 * shall return @nbyte.
 *
 * Return: Upon successful completion, a non-negative integer is returned
 * indicating the number of bytes actually written. Otherwise, -1 is returned
 * and error state is set accordingly
 */
MMLIB_API ssize_t mm_write(int fd, const void* buf, size_t nbyte);


/**
 * mm_seek() - change file offset
 * @fd:          file descriptor
 * @offset:      delta
 * @whence:      how the @offset affect the file offset
 *
 * This function sets the file offset for the open file description associated
 * with the file descriptor @fd, as follows depending on the value in @whence
 *
 * %SEEK_SET
 *   the file offset shall be set to @offset bytes.
 * %SEEK_CUR
 *   the file offset shall be set to its current location plus @offset.
 * %SEEK_END
 *   the file offset shall be set to the size of the file plus @offset.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API mm_off_t mm_seek(int fd, mm_off_t offset, int whence);


/**
 * mm_ftruncate() -  truncate/resize a file to a specified length
 * @fd:         file descriptor of the file to resize
 * @length:     new length of the file
 *
 * If @fd refers to a regular file, mm_ftruncate() cause the size of the file
 * to be truncated to @length. If the size of the file previously exceeded
 * @length, the extra data shall no longer be available to reads on the file.
 * If the file previously was smaller than this size, mm_ftruncate() increases
 * the size of the file. If the file size is increased, the extended area will
 * appear as if it were zero-filled. The value of the seek pointer shall not be
 * modified by a call to mm_ftruncate().
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_ftruncate(int fd, mm_off_t length);

/**
 * mm_fstat() - get file status from file descriptor
 * @fd:         file descriptor
 * @buf:        pointer to mm_stat structure to fill
 *
 * This function obtains information about an open file associated with the
 * file descriptor @fd, and writes it to the area pointed to by @buf.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_fstat(int fd, struct mm_stat* buf);


/**
 * mm_stat() - get file status from file path
 * @path:       path of file
 * @buf:        pointer to mm_stat structure to fill
 *
 * This function obtains information about an file located by @path, and writes
 * it to the area pointed to by @buf.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_stat(const char* path, struct mm_stat* buf);

/**
 * mm_check_access() - verify access to a file
 * @path:       path of file
 * @amode:      access mode to check (OR-combination of *_OK flags)
 *
 * This function verify the calling process can access the file located at
 * @path according to the bits pattern specified in @amode which can be a
 * OR-combination of the %R_OK, %W_OK, %X_OK to indicate respectively the read,
 * write or execution access to a file. If @amode is F_OK, only the existence
 * of the file is checked.
 *
 * Return:
 * - 0 if the file can be accessed
 * - %ENOENT if a component of @path does not name an existing file
 * - %EACCESS if the file cannot be access with the mode specified in @amode
 * - -1 in case of error (error state is then set accordingly)
 */
MMLIB_API int mm_check_access(const char* path, int amode);


/**
 * mm_dup() - duplicate an open file descriptor
 * @fd:         file descriptor to duplicate
 *
 * This function creates a new file descriptor referencing the same file
 * description as the one referenced by @fd.
 *
 * Return: a non-negative integer representing the new file descriptor in case
 * of success. The return file descriptor value is then guaranteed to be the
 * lowest available at the time of the call. In case of error, -1 is returned
 * with error state set accordingly.
 */
MMLIB_API int mm_dup(int fd);


/**
 * mm_dup2() - duplicate an open file descriptor to a determined file descriptor
 * @fd:         file descriptor to duplicate
 * @newfd:      file descriptor number that will become the duplicate
 *
 * This function duplicates an open file descriptor @fd and assign it to the
 * file descriptor @newfd. In other word, this function is similar to mm_dup()
 * but in case of success, the returned value is ensured to be @newfd.
 *
 * Return: a non-negative integer representing the new file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_dup2(int fd, int newfd);


/**
 * mm_pipe() - creates an interprocess channel
 * @pipefd:     array of two in receiving the read and write endpoints
 *
 * The mm_pipe() function creates a pipe and place two file descriptors, one
 * each into the arguments @pipefd[0] and @pipefd[1], that refer to the open
 * file descriptions for the read and write ends of the pipe. Their integer
 * values will be the two lowest available at the time of the mm_pipe() call.
 *
 * Data can be written to the file descriptor @pipefd[1] and read from the file
 * descriptor @pipefd[0]. A read on the file descriptor @pipefd[0] shall access
 * data written to the file descriptor @pipefd[1] on a first-in-first-out
 * basis.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_pipe(int pipefd[2]);


/**
 * mm_unlink() -  remove a directory entry
 * @path:       location to remove from file system
 *
 * The mm_unlink() function removes a link to a file. If @path names a symbolic
 * link, it removes the symbolic link named by @path and does not affect any
 * file or directory named by the contents of the symbolic link. Otherwise,
 * mm_unlink() remove the link named by the pathname pointed to by @path and
 * decrements the link count of the file referenced by the link.
 *
 * When the file's link count becomes 0 and no process has the file open, the
 * space occupied by the file will be freed and the file will no longer be
 * accessible. If one or more processes have the file open when the last link
 * is removed, the link will be removed before mm_unlink() returns, but the
 * removal of the file contents is postponed until all references to the
 * file are closed (ie when all file descriptors referencing it are closed).
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 *
 * NOTE: On Windows platform, it is usually believed that an opened file is not
 * permitted to be deleted. This is not true. This is only due to the fact that
 * many libraries/application open file missing the right share mode
 * (FILE_SHARE_DELETE). If you access the file through mmlib APIs, you will be
 * able to unlink your file before it is closed (even if memory mapped...).
 */
MMLIB_API int mm_unlink(const char* path);


/**
 * mm_link() - create a hard link to a file
 * @oldpath:    existing path for the file to link
 * @newpath:    new path of the file
 *
 * The mm_link() function creates a new link (directory entry) for the existing
 * file, @oldpath.
 *
 * The @oldpath argument points to a pathname naming an existing file. The
 * @newpath argument points to a pathname naming the new directory entry to be
 * created. The mm_link() function create atomically a new link for the
 * existing file and the link count of the file shall be incremented by one.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_link(const char* oldpath, const char* newpath);


/**
 * mm_symlink() - create a symbolic link to a file
 * @oldpath:    existing path for the file to link
 * @newpath:    new path of the file
 *
 * The mm_link() function creates a new symbolinc link for the existing file,
 * @oldpath. The @oldpath argument do not need to point to a pathname naming an
 * existing file.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_symlink(const char* oldpath, const char* newpath);


/**
 * mm_mkdir() - creates a directory
 * @path:       path of the directory to create
 * @mode:       permission to use for directory creation
 * @flags:      creation flags
 *
 * The mm_mkdir() function creates a new directory with name @path. The file
 * permission bits of the new directory shall be initialized from @mode. These
 * file permission bits of the @mode argument are modified by the process' file
 * creation mask.
 *
 * The function will fail if the parent directory does not exist unless @flags
 * contains MM_CREATE_RECURSIVE which is this case, the function will try to
 * recursively create the missing parent directories (using the file
 * permission).
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_mkdir(const char* path, int mode, int flags);


/**
 * mm_chdir() - change working directory
 * @path:       path to new working directory
 *
 * The mm_chdir() function causes the directory named by the pathname pointed
 * to by the @path argument to become the current working directory; that is,
 * the starting point for path searches for pathnames that are not absolute.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_chdir(const char* path);


/**************************************************************************
 *                      Directory navigation                              *
 **************************************************************************/
typedef struct mm_dirstream MMDIR;

#define MM_DT_UNKNOWN   0x00
#define MM_DT_FIFO      0x01
#define MM_DT_CHR       0x02
#define MM_DT_BLK       0x03
#define MM_DT_DIR       0x04
#define MM_DT_REG       0x05
#define MM_DT_LNK       0x05
#define MM_DT_SOCK      0x06


struct mm_dirent {
	size_t reclen;
	unsigned char type;
	char name[];
};

#define MM_CREATE_RECURSIVE	0x000000001


/**
 * mm_opendir() - open a directory stream
 * @path:       path to directory
 *
 * The mm_opendir() function opens a directory stream corresponding to the
 * directory named by the @path argument. The directory stream is positioned
 * at the first entry.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API MMDIR* mm_opendir(const char* path);



/**
 * mm_closedir() - close a directory stream
 * @dir:        directory stream to close
 *
 * The mm_closedir() function closes the directory stream referred to by the
 * argument @dir. Upon return, the value of @dir may no longer point to an
 * accessible object of the type MMDIR.
 */
MMLIB_API void mm_closedir(MMDIR* dir);


/**
 * mm_rewinddir() - reset a directory stream to its beginning
 * @dir:        directory stream to rewind
 *
 * The mm_rewinddir() function resets the position of the directory stream to
 * which @dir refers to the beginning of the directory. It causes the directory
 * stream to refer to the current state of the corresponding directory, as a
 * call to mm_opendir() would have done.
 */
MMLIB_API void mm_rewinddir(MMDIR* dir);


/**
 * mm_readdir() - read current entry from directory stream and advance it
 * @dir:        directory stream to read
 *
 * The type MMDIR represents a directory stream, which is an ordered sequence
 * of all the directory entries in a particular directory. Directory entries
 * represent files; files may be removed from a directory or added to a
 * directory asynchronously to the operation of mm_readdir().
 *
 * The mm_readdir() function returns a pointer to a structure representing the
 * directory entry at the current position in the directory stream specified by
 * the argument @dir, and position the directory stream at the next entry which
 * will be valid until the next call to mm_readdir() with the same @dir
 * argument. It returns a NULL pointer upon reaching the end of the directory
 * stream.
 *
 * Return: pointer to the file entry if directory stream has not reached the
 * end. NULL otherwise
 */
MMLIB_API const struct mm_dirent* mm_readdir(MMDIR* dir);


/**************************************************************************
 *                             Process spawning                           *
 **************************************************************************/
#ifdef _WIN32
typedef DWORD mm_pid_t;
#else
typedef pid_t mm_pid_t;
#endif

/**
 * struct mm_remap_fd - file descriptor mapping for child creation
 * @child_fd:   file descriptor in the child
 * @parent_fd:  file descriptor in the parent process to @child_fd must be
 *              mapped. If @child_fd must be specifically closed in the
 *              child, @parent_fd can be set to -1;
 *
 * Use in combination of mm_spawn(), this structure is meant to be in an
 * array that define the file descriptor remapping in child.
 */
struct mm_remap_fd {
	int child_fd;
	int parent_fd;
};

#define MM_SPAWN_DAEMONIZE 0x00000001
#define MM_SPAWN_KEEP_FDS  0x00000002  // Keep all inheritable fd in child

/**
 * mm_spawn() - spawn a new process
 * @child_pid:  pointer receiving the child process pid
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
 * This function creates a new process executing the file located at @path
 * and the pid the of created child is set in the variable pointed by
 * @child_pid.
 *
 * The child process will inherit only the open file descriptors specified
 * in the @fd_map array whose length is indicated by @num_map. For each
 * element in @fd_map, a file descriptor numbered as specified in
 * &mm_remap_fd.child_fd is available at child startup referencing the
 * same file description as the corresponding &mm_remap_fd.parent_fd
 * in the calling process. This means that after successful execution of
 * mm_spawn() two reference of the same file will exist and the underlying
 * file will be actually closed when all file descriptor  referencing it
 * will be closed. The @fd_map array is processed sequentially so a mapping
 * in the first element can be overrided in the next elements. If an element
 * in @fd_map has a &mm_remap_fd.parent_fd field set to -1, it means that
 * the corresponding @fd_map has a &mm_remap_fd.child_fd must not opened in
 * the child process.
 *
 * For conveniency, the standard input, output and error are inherited by
 * default in the child process. If any of those file are meant to be closed
 * or redirected in the child, this can simply be done by adding element in
 * @fd_map that redirect a standard file descriptor in the parent, or close
 * them (by setting &mm_remap_fd.parent_fd to -1.
 *
 * @flags must contains a OR-combination or 0 or any number of the following
 * flags:
 *
 * MM_SPAWN_DAEMONIZE
 *   the created process will be detached from calling process and will
 *   survive to its parent death (a daemon in the UNIX terminology).
 * MM_SPAWN_KEEP_FDS
 *   All open file descriptors in the calling process that are inherintable are
 *   kept in the child with the same index. All the other file descriptor are
 *   closed. Unless specified otherwise, all file descriptor created in mmlib
 *   API are not inheritable. If this flag is specified, @num_map and
 *   @fd_map argument are ignored.
 *
 * The argument @argv, if not null, is an array of character pointers to
 * null-terminated strings. The application shall ensure that the last
 * member of this array is a null pointer. These strings constitutes
 * the argument list available to the new process image. The value in
 * argv[0] should point to a filename that is associated with the process
 * being started. If @argv is NULL, the behavior is as if mm_spawn() were
 * called with a two array argument, @argv[0] = @path, @argv[1] = NULL.
 *
 * The argument @envp is an array of character pointers to null-terminated
 * strings. These strings constitutes the environment for the new
 * process image. The @envp array is terminated by a null pointer. If @envp
 * is NULL, the new process use the same environment of the calling process
 * at the time of the mm_spawn() call.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_spawn(mm_pid_t* child_pid, const char* path,
                       int num_map, const struct mm_remap_fd* fd_map,
                       int flags, char* const* argv, char* const* envp);

#define MM_WSTATUS_CODEMASK     0x000000FF
#define MM_WSTATUS_EXITED       0x00000100
#define MM_WSTATUS_SIGNALED     0x00000200

/**
 * mm_wait_process() - wait for a child process to terminate
 * @pid:        PID of child process
 * @status:     location where to put status of the child process
 *
 * This function get the status of a the child process whose PID is @pid. If
 * the child process is not terminated yet, the function will block until
 * the child is terminated.
 *
 * If @status is not NULL, it refers to a location that will receive the
 * status information of the terminated process. The information is a mask
 * of MM_WSTATUS_* indicating wheter the child has terminated because of
 * normal termination or abnormal one and the exit code (or signal number).
 * To be accessible in the status information, the return code of a child
 * program must be between 0 and 255.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_wait_process(mm_pid_t pid, int* status);


/**************************************************************************
 *                            memory mapping                              *
 **************************************************************************/

#define MM_MAP_READ     0x00000001
#define MM_MAP_WRITE	0x00000002
#define MM_MAP_EXEC     0x00000004
#define MM_MAP_SHARED   0x00000008

#define MM_MAP_RDWR	(MM_MAP_READ | MM_MAP_WRITE)
#define MM_MAP_PRIVATE  0x00000000


/**
 * mm_mapfile() - map pages of memory
 * @fd:         file descriptor of file to map in memory
 * @offset:     offset within the file from which the mapping must start
 * @len:        length of the mapping
 * @mflags:     control how the mapping is done
 *
 * The mm_mapfile() function establishes a mapping between a process'
 * address space and a portion or the entirety of a file or shared memory
 * object represented by @fd. The portion of the object to map can be
 * controlled by the parameters @offset and @len. @offset must be a multiple
 * of page size.
 *
 * The flags in parameters @mflags determines whether read, write, execute,
 * or some combination of accesses are permitted to the data being mapped.
 * The requested access can of course cannot grant more permission than the
 * one associated with @fd.
 *
 * MM_MAP_READ
 *   Data can be read
 * MM_MAP_WRITE
 *   Data can be written
 * MM_MAP_EXEC
 *   Data can be executed
 * MM_MAP_SHARED
 *   Change to mapping are shared
 *
 * If MM_MAP_SHARED is specified, write change the underlying object.
 * Otherwise, modifications to the mapped data by the calling process will
 * be visible only to the calling process and shall not change the
 * underlying object.
 *
 * The mm_mapfile() function adds an extra reference to the file associated
 * with the file descriptor @fd which is not removed by a subsequent
 * mm_close() on that file descriptor. This reference will be removed when
 * there are no more mappings to the file.
 *
 * Return: The starting address of the mapping in case of success.
 * Otherwise NULL is returned and error state is set accordingly.
 */
MMLIB_API void* mm_mapfile(int fd, mm_off_t offset, size_t len, int mflags);


/**
 * mm_unmap() - unmap pages of memory
 * @addr:       starting address of memory block to unmap
 *
 * Remove a memory mapping previously established. @addr must be NULL or must
 * have been returned by a successfull call to mm_mapfile(). If @addr is NULL,
 * mm_unmap() do nothing.
 *
 * Return: 0 in case of success, -1 otherwise with error state set.
 */
MMLIB_API int mm_unmap(void* addr);


/**
 * mm_shm_open() - open a shared memory object
 * @name:       name of the shared memory object
 * @oflag:      flags controlling the open operation
 * @mode:       permission if object is created
 *
 * This function establishes a connection between a shared memory object and
 * a file descriptor. The @name argument points to a string naming a shared
 * memory object. If successful, the file descriptor for the shared memory
 * object is the lowest numbered file descriptor not currently open for that
 * process.
 *
 * The file status flags and file access modes of the open file description
 * are according to the value of @oflag. It must contains the exactly one of
 * the following: %O_RDONLY, %O_RDWR. It can contains any combination of the
 * remaining flags: %O_CREAT, %O_EXCL, %O_TRUNC. The meaning of these
 * constant is exactly the same as for mm_open().
 *
 * When a shared memory object is created, the state of the shared memory
 * object, including all data associated with the shared memory object,
 * persists until the shared memory object is unlinked and all other
 * references are gone. It is unspecified whether the name and shared memory
 * object state remain valid after a system reboot.
 *
 * Once a new shared memory object has been created, it can be removed with
 * a call to mm_shm_unlink().
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_shm_open(const char* name, int oflag, int mode);


/**
 * mm_anon_shm() - Creates an anonymous memory object
 *
 * This function creates an anonymous shared memory object (ie nameless) and
 * establishes a connection between it and a file descriptor. If successful,
 * the file descriptor for the shared memory object is the lowest numbered
 * file descriptor not currently open for that process.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_anon_shm(void);


/**
 * The mm_shm_unlink() removes the name of the shared memory
 * object named by the string pointed to by @name.
 *
 * If one or more references to the shared memory object exist when the object
 * is unlinked, the name is removed before mm_shm_unlink() returns, but the
 * removal of the memory object contents is postponed until all open and
 * map references to the shared memory object have been removed.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_shm_unlink(const char* name);


#ifdef __cplusplus
}
#endif

#endif
