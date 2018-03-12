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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "mmpredefs.h"

/**
 * struct iovec - structure for scatter/gather I/O.
 * @iov_base:   Base address of a memory region for input or output
 * @iov_len:    The size of the memory pointed to by @iov_base
 */

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
 * Note that the two file decriptors point to the same file. They will share
 * the same file pointer.
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
 * in the first element can be overridden in the next elements. If an element
 * in @fd_map has a &mm_remap_fd.parent_fd field set to -1, it means that
 * the corresponding @fd_map has a &mm_remap_fd.child_fd must not opened in
 * the child process.
 *
 * For convenience, the standard input, output and error are inherited by
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
 * have been returned by a successful call to mm_mapfile(). If @addr is NULL,
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


/**************************************************************************
 *                        Interprocess communication                      *
 **************************************************************************/

/**
 * struct mmipc_msg - structure for IPC message
 * @iov:        scatter/gather array
 * @fds:        array of file descriptor to pass/receive
 * @num_iov:    number of element in @iov
 * @num_fds:    number of file descriptors in @fds
 * @flags:      flags on received message
 * @num_fds_max: maximum number of file descriptors in @fds
 */
struct mmipc_msg {
	struct iovec* iov;
	int* fds;
	int num_iov;
	int num_fds;
	int flags;
	int num_fds_max;
};

struct mmipc_srv;

/**
 * mmipc_srv_create() - Create a IPC server
 * @addr:       path to which the server must listen
 *
 * This creates a server instance that will listen to the path specified by
 * argument @path. This path does not have necessarily a connection with the
 * filesystem pathnames. However it obey to the same syntax.
 *
 * Only one IPC server instance can listen to same address. If there is
 * already another server, this function will fail.
 *
 * Return: pointer to IPC server in case of success. NULL otherwise with
 * error state set accordingly
 */
MMLIB_API struct mmipc_srv* mmipc_srv_create(const char* addr);


/**
 * mmipc_srv_destroy() - Destroy IPC server
 * @srv:        server to destroy
 *
 * This function destroy the server referenced to by @srv. The path to which
 * server was listening become available for new call to mmipc_srv_create().
 * However, if there were accepted connection still opened, it is
 * unspecified whether the name will be available before all connection are
 * closed or not. If there were client connection pending that had not been
 * accepted by the server yet, those will be dropped.
 *
 * Destroying a server does not affect accepted connections which will
 * survive until they are specifically closed with mm_close().
 */
MMLIB_API void mmipc_srv_destroy(struct mmipc_srv* srv);


/**
 * mmipc_srv_accept() - accept a incoming connection
 * @srv:        IPC server
 *
 * This function extracts the first connection on the queue of pending
 * connections, and allocate a new file descriptor for that connection (the
 * lowest number available). If there are no connection pending, the
 * function will block until one arrives.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mmipc_srv_accept(struct mmipc_srv* srv);


/**
 * mmipc_connect() - connect a client to an IPC server
 * @addr:       path to which the client must connect
 *
 * Client-side counterpart of mmipc_srv_accept(), this functions attempts to
 * connect to a server listening to @addr if there are any. If one is found,
 * it allocates a new file descriptor for that connection (the lowest number
 * available). If there are no server listening to @addr, the function will
 * block until one server does.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mmipc_connect(const char* addr);


/**
 * mmipc_connected_pair() - create a pair of connected IPC endpoints
 * @fds:        array receiving the file descriptor of the 2 endpoints
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mmipc_connected_pair(int fds[2]);


/**
 * mmipc_sendmsg() - send message to ICP endpoint
 * @fd:         file descriptor of an IPC connection endpoint
 * @msg:        IPC message
 *
 * If space is not available at the sending endpoint to hold the message to be
 * transmitted, the function will block until space is available. The
 * message sent in @msg is a datagram: either all could have been
 * transmitted, either none and the function would fail.
 *
 * File descriptors can also be transmitted to the receiving endpoint along a
 * message with the @msg->fds and @msg->num_fds fields. The file descriptors
 * listed here are duplicated for the process holding the other endpoint.
 *
 * Return: the number of bytes sent in case of success, -1 otherwise with
 * error state set accordingly.
 */
MMLIB_API ssize_t mmipc_sendmsg(int fd, const struct mmipc_msg* msg);


/**
 * mmipc_recvmsg() - recv message from IPC endpoint
 * @fd:         file descriptor of an IPC connection endpoint
 * @msg:        IPC message
 *
 * This function receives a message. The message received is a datagram: if
 * the data received is smaller that requested, the function will return a
 * smaller message and its size will be reported by the return value.
 * Controversy if a message is too long to fit in the supplied buffers in
 * @msg->iov, the excess bytes will be discarded and the flag %MSG_TRUNC
 * will be set in @msg->flags.
 *
 * You can receive file descriptors along with the message in @msg->fds and
 * @msg->num_fds fields. Similarly to the data message, if the buffer
 * holding the file descriptor is too small, the files descriptor in excess
 * will be discarded (implicitly closing them, ensuring no descriptor leak to
 * occur) and the flag %MSG_CTRUNC will be set in @msg->flags.
 *
 * Return: the number of bytes received in case of success, -1 otherwise with
 * error state set accordingly.
 */
MMLIB_API ssize_t mmipc_recvmsg(int fd, struct mmipc_msg* msg);


/**************************************************************************
 *                          Network communication                         *
 **************************************************************************/



/**
 * struct msghdr - structure for socket message
 * @msg_name:       optional address
 * @msg_namelen:    size of address
 * @msg_iov:        scatter/gather array
 * @msg_iovlen:     number of element in @msg_iov (beware of type, see NOTE)
 * @msg_control:    Ancillary data
 * @msg_controllen: length of ancillary data
 * @msg_flags:      flags on received message
 *
 * NOTE:
 * Although Posix mandates that @msg_iovlen is int, many platform do not
 * respect this: it is size_t on Linux, unsigned int on Darwin, int on
 * freebsd. It is safer to consider @msg_iovlen is defined as size_t in
 * struct (avoid one hole due to alignment on 64bits platform) and always
 * manipulate as int (forcing the cast).
 */

#if _WIN32
struct msghdr {
	void*         msg_name;
	socklen_t     msg_namelen;
	struct iovec* msg_iov;
	size_t        msg_iovlen;
	void*         msg_control;
	socklen_t     msg_controllen;
	int           msg_flags;
};
#endif


/**
 * struct mmsock_multimsg - structure for transmitting multiple messages
 * @msg:        message
 * @datalen:    number of received byte for @msg
 *
 * This should alias with struct mmsghdr on system supporting recvmmsg()
 */
struct mmsock_multimsg {
	struct msghdr msg;
	unsigned int  datalen;
};


/**
 * mm_socket() - create an endpoint for communication
 * @domain:     communications domain in which a socket is to be created
 * @type:       type of socket to be created
 *
 * The mm_socket() function creates an unbound socket in a communications
 * domain, and return a file descriptor that can be used in later function
 * calls that operate on sockets. The allocated file descriptor is the
 * lowest numbered file descriptor not currently open for that process.
 *
 * The @domain argument specifies the address family used in the
 * communications domain. The address families supported by the system are
 * system-defined. However you can expect that the following to be
 * available:
 *
 * %AF_UNSPEC
 *   The address family is unspecified.
 * %AF_INET
 *   The Internet Protocol version 4 (IPv4) address family.
 * %AF_INET6
 *   The Internet Protocol version 6 (IPv6) address family.
 *
 * The @type argument specifies the socket type, which determines the
 * semantics of communication over the socket. The following socket types
 * are defined; Some systems may specify additional types:
 *
 * SOCK_STREAM
 *   Provides sequenced, reliable, bidirectional, connection-mode byte
 *   streams
 * SOCK_DGRAM
 *   Provides datagrams, which are connectionless-mode, unreliable messages
 *   of fixed maximum length.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_socket(int domain, int type);


/**
 * mm_bind() - bind a name to a socket
 * @sockfd:     file descriptor of the socket to be bound
 * @addr:       points to a &struct sockaddr containing the address bind
 * @addrlen:    length of the &struct sockaddr pointed to by @addr
 *
 * This assigns a local socket address @addr to a socket identified by
 * descriptor @sockfd that has no local socket address assigned. Socket
 * created with mm_socket() are initially unnamed; they are identified only
 * by their address family.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);


/**
 * mm_listen() - listen for socket connections
 * @sockfd:     file descriptor of the socket that must listen
 * @backlog:    hint for the queue limit
 *
 * This mark a connection-mode socket, specified by the @sockfd argument, as
 * accepting connections. The @backlog argument provides a hint to the
 * implementation which the implementation shall use to limit the number of
 * outstanding connections in the socket's listen queue.
 *
 * return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_listen(int sockfd, int backlog);


/**
 * mm_accept() - accept a new connection on a socket
 * @sockfd:     file descriptor of a listening socket
 * @addr:       NULL or pointer  to &struct sockaddr containing the address
 *              of accepted socket
 * @addrlen:    a pointer to a &typedef socklen_t object which on input
 *              specifies the length of the supplied &struct sockaddr
 *              structure, and on output specifies the length of the stored
 *              address. It can be NULL if @addr is NULL.
 *
 * This extracts the first connection on the queue of pending connections,
 * create a new socket with the same socket type protocol and address family
 * as the specified socket, and allocate a new file descriptor for that
 * socket. The allocated file descriptor is the lowest numbered file
 * descriptor not currently open for that process.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);


/**
 * mm_connect() - connect a socket to a peer
 * @sockfd:     file descriptor of the socket
 * @addr:       pointer  to &struct sockaddr containing the peer address
 * @addrlen:    length of the supplied &struct sockaddr pointed by @addr
 *
 * This attempt to make a connection on a connection-mode socket or to set
 * or reset the peer address of a connectionless-mode socket.  If the socket
 * has not already been bound to a local address, mm_connect() shall bind it
 * to an address which is an unused local address.
 *
 * If the initiating socket is not connection-mode, then mm_connect() sets
 * the socket's peer address, and no connection is made. For %SOCK_DGRAM
 * sockets, the peer address identifies where all datagrams are sent on
 * subsequent mm_send() functions, and limits the remote sender for
 * subsequent mm_recv() functions.
 *
 * If the initiating socket is connection-mode, then mm_connect() attempts
 * to establish a connection to the address specified by the @addr argument.
 * If the connection cannot be established immediately and the call will
 * block for up to an unspecified timeout interval until the connection is
 * established.
 *
 * return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);


/**
 * mm_setsockopt() - set the socket options
 * @sockfd:     file descriptor of the socket
 * @level:      protocol level at which the option resides
 * @optname:    option name
 * @optval:     pointer to option value
 * @optlen:     size of the option value
 *
 * This sets the option specified by the @optname argument, at the protocol
 * level specified by the @level argument, to the value pointed to by the
 * @optval argument for the socket associated with the file descriptor
 * specified by the @sockfd argument.
 *
 * The level argument specifies the protocol level at which the option
 * resides. To set options at the socket level, specify the level argument
 * as %SOL_SOCKET. To set options at other levels, supply the appropriate
 * level identifier for the protocol controlling the option. For example, to
 * indicate that an option is interpreted by the TCP (Transport Control
 * Protocol), set level to %IPPROTO_TCP.
 *
 * return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_setsockopt(int sockfd, int level, int optname,
                            const void *optval, socklen_t optlen);


/**
 * mm_getsockopt() - get the socket options
 * @sockfd:     file descriptor of the socket
 * @level:      protocol level at which the option resides
 * @optname:    option name
 * @optval:     pointer to option value
 * @optlen:     pointer to size of the option value on input, actual length
 *              of option value on output
 *
 * This function retrieves the value for the option specified by the
 * @optname argument for the socket specified by the socket argument. If
 * the size of the option value is greater than @optlen, the value stored
 * in the object pointed to by the @optval argument shall be silently
 * truncated. Otherwise, the object pointed to by the @optlen argument
 * shall be modified to indicate the actual length of the value.
 *
 * return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_getsockopt(int sockfd, int level, int optname,
                            void *optval, socklen_t* optlen);


/**
 * mm_shutdown() - shut down socket send and receive operations
 * @sockfd:     file descriptor of the socket
 * @how:        type of shutdown
 *
 * This causes all or part of a full-duplex connection on the socket associated
 * with the file descriptor socket to be shut down. The type of shutdown is
 * controlled by @how which can be one of the following values:
 *
 * SHUT_RD
 *   Disables further receive operations.
 * SHUT_WR
 *   Disables further send operations.
 * SHUT_RDWR
 *   Disables further send and receive operations.
 *
 * return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_shutdown(int sockfd, int how);


/**
 * mm_send() - send a message on a connected socket
 * @sockfd:     socket file descriptor.
 * @buffer:     buffer containing the message to send.
 * @length:     the length of the message in bytes
 * @flags:      type of message transmission
 *
 * This initiates transmission of a message from the specified socket to its
 * peer. The mm_send() function sends a message only when the socket is
 * connected (including when the peer of a connectionless socket has been
 * set via mm_connect()).
 *
 * @flags specifies the type of message transmission. If flags contains
 * MSG_OOB, the call send out-of-band data on sockets that support
 * out-of-band communications. The significance and semantics of out-of-band
 * data are protocol-specific.
 *
 * The length of the message to be sent is specified by the length argument.
 * If the message is too long to pass through the underlying protocol,
 * mm_send() will fail and no data shall be transmitted (This is typically
 * the case of datagram protocol, like UDP). If space is not
 * available at the sending socket to hold the message to be transmitted,
 * mm_send() will block until space is available. In the case of a stream
 * protocol (like TCP), there are possibility that the sent data is actually
 * smaller than requested (for example due to early interruption because of
 * signal delivery).
 *
 * Return: the number of bytes actually sent in case of success, -1
 * otherwise with error state set accordingly.
 */
MMLIB_API ssize_t mm_send(int sockfd, const void *buffer, size_t length, int flags);


/**
 * mm_recv() - receive a message from a socket
 * @sockfd:     socket file descriptor.
 * @buffer:     buffer containing the message to receive.
 * @length:     the size of buffer pointed by @buffer
 * @flags:      type of message reception
 *
 * This receives a message from a connection-mode or connectionless-mode
 * socket. It is normally used with connected sockets because it does not
 * permit the application to retrieve the source address of received data.
 *
 * @flags specifies the type of message reception. Values of this argument
 * are formed by logically OR'ing zero or more of the following values:
 *
 * %MSG_PEEK
 *   Peeks at an incoming message. The data is treated as unread and the
 *   next mm_recv() or similar function shall still return this data.
 * %MSG_OOB
 *   Requests out-of-band data. The significance and semantics of
 *   out-of-band data are protocol-specific.
 * %MSG_WAITALL
 *   On SOCK_STREAM sockets this requests that the function block until the
 *   full amount of data can be returned. The function may return the
 *   smaller amount of data if the socket is a message-based socket, if a
 *   signal is caught, if the connection is terminated, if MSG_PEEK was
 *   specified, or if an error is pending for the socket.
 *
 * The mm_recv() function return the length of the message written to the
 * buffer pointed to by the buffer argument. For message-based sockets, such
 * as SOCK_DGRAM and SOCK_SEQPACKET, the entire message will be read in a
 * single operation. If a message is too long to fit in the supplied buffer,
 * and %MSG_PEEK is not set in the @flags argument, the excess bytes will be
 * discarded. For stream-based sockets, such as SOCK_STREAM, message
 * boundaries will be ignored. In this case, data is returned to the
 * user as soon as it becomes available, and no data will be discarded.
 *
 * If the MSG_WAITALL flag is not set, data will be returned only up to the
 * end of the first message.
 *
 * If no messages are available at the socket, mm_recv() will block until a
 * message arrives.
 *
 * Return: the number of bytes actually received in case of success, -1
 * otherwise with error state set accordingly.
 */
MMLIB_API ssize_t mm_recv(int sockfd, void *buffer, size_t length, int flags);


/**
 * mm_sendmsg() - send a message on a socket using a message structure
 * @sockfd:     socket file descriptor.
 * @msg:        message structure containing both the destination address
 *              (if any) and the buffers for the outgoing message.
 * @flags:      type of message transmission
 *
 * This functions send a message through a connection-mode or
 * connectionless-mode socket. If the socket is connectionless-mode, the
 * message will be sent to the address specified by @msg. If the socket
 * is connection-mode, the destination address in @msg is ignored.
 *
 * The @msg->msg_iov and @msg->msg_iovlen fields of message specify zero or
 * more buffers containing the data to be sent. @msg->msg_iov points to an
 * array of iovec structures; @msg->msg_iovlen is set to the dimension of
 * this array. In each &struct iovec structure, the &iovec.iov_base field
 * specifies a storage area and the &iovec.iov_len field gives its size in
 * bytes. Some of these sizes can be zero.  The data from each storage area
 * indicated by @msg.msg_iov is sent in turn.
 *
 * Excepting for the specification of the message buffers and destination
 * address, the behavior of mm_sendmsg() is the same as mm_send().
 *
 * Return: the number of bytes actually sent in case of success, -1
 * otherwise with error state set accordingly.
 */
MMLIB_API ssize_t mm_sendmsg(int sockfd, const struct msghdr* msg, int flags);


/**
 * mm_recvmsg() - receive a message from a socket using a message structure
 * @sockfd:     socket file descriptor.
 * @msg:        message structure containing both the source address (if
 *              set) and the buffers for the inbound message.
 * @flags:      type of message reception
 *
 * This function receives a message from a connection-mode or connectionless-mode
 * socket. It is normally used with connectionless-mode sockets because it
 * permits the application to retrieve the source address of received data.
 *
 * In the &struct mmsock_msg structure, the &msghdr.msg_name and
 * &msghdr.msg_namelen members specify the source address if the socket is
 * unconnected. If the socket is connected, those members are ignored. The
 * @msg->msg_name may be a null pointer if no names are desired or required.
 * The @msg->msg_iov and @msg->msg_iovlen fields are used to specify where
 * the received data will be stored. @msg->msg_iov points to an array of
 * &struct iovec structures; @msg->msg_iovlen is set to the dimension of
 * this array.  In each &struct iovec structure, the &iovec.iov_base field
 * specifies a storage area and the &iovec.iov_len field gives its size in
 * bytes. Each storage area indicated by @msg.msg_iov is filled with
 * received data in turn until all of the received data is stored or all of
 * the areas have been filled.
 *
 * The recvmsg() function returns the total length of the message. For
 * message-based sockets, such as SOCK_DGRAM and SOCK_SEQPACKET, the entire
 * message is read in a single operation. If a message is too long to fit in
 * the supplied buffers, and %MSG_PEEK is not set in the flags argument, the
 * excess bytes will be discarded, and %MSG_TRUNC will be set in
 * @msg->flags. For stream-based sockets, such as SOCK_STREAM, message
 * boundaries are ignored. In this case, data will be returned to the user
 * as soon as it becomes available, and no data will be discarded.
 *
 * Excepting for the specification of message buffers and source address,
 * the behavior of mm_recvmsg() is the same as mm_rec().
 *
 * Return: the number of bytes actually received in case of success, -1
 * otherwise with error state set accordingly.
 */
MMLIB_API ssize_t mm_recvmsg(int sockfd, struct msghdr* msg, int flags);


/**
 * mm_send_multimsg() - send multiple messages on a socket
 * @sockfd:     socket file descriptor.
 * @vlen:       size of @msgvec array
 * @msgvec:     pointer to an array of &struct mmsock_multimsg structures
 * @flags:      type of message transmission
 *
 * This function is an extension of mm_sendmsg that allows the
 * caller to send multiple messages to a socket using a single
 * call. This is equivalent to call mm_sendmsg() in a loop for each element
 * in @msgvec.
 *
 * On return from mm_sendmmsg(), the &struct mmsock_multimsg.data_len fields
 * of successive elements of @msgvec are updated to contain the number of
 * bytes transmitted from the corresponding &struct mmsock_multimsg.msg.
 *
 * Return: On success, it returns the number of messages sent from @msgvec;
 * if this is less than @vlen, the caller can retry with a further
 * mm_sendmmsg() call to send the remaining messages. On error, -1 is
 * returned and the error state is set accordingly.
 */
MMLIB_API int mm_send_multimsg(int sockfd, int vlen,
                               struct mmsock_multimsg *msgvec, int flags);


/**
 * mm_recv_multimsg() - receive multiple messages from a socket
 * @sockfd:     socket file descriptor.
 * @vlen:       size of @msgvec array
 * @msgvec:     pointer to an array of &struct mmsock_multimsg structures
 * @flags:      type of message reception
 * @timeout:    timeout for receive operation. If NULL, the operation blocks
 *              indefinitively
 *
 * This function is an extension of mm_sendmsg that allows the
 * caller to receive multiple messages from a socket using a single
 * call. This is equivalent to call mm_recvmsg() in a loop for each element
 * in @msgvec with loop break if @timeout has been reached.
 *
 * On return from mm_recvmmsg(), the &struct mmsock_multimsg.data_len fields
 * of successive elements of @msgvec are updated to contain the number of
 * bytes received from the corresponding &struct mmsock_multimsg.msg.
 *
 * Return: On success, it returns the number of messages received from @msgvec;
 * if this is less than @vlen, the caller can retry with a further
 * mm_recvmmsg() call to receive the remaining messages. On error, -1 is
 * returned and the error state is set accordingly.
 */
MMLIB_API int mm_recv_multimsg(int sockfd, int vlen,
                               struct mmsock_multimsg *msgvec, int flags,
                               struct timespec *timeout);

/**
 * mm_getaddrinfo() - get address information
 * @node:       descriptive name or address string (can be NULL)
 * @service:    string identifying the requested service (can be NULL)
 * @hints:      input values that may direct the operation by providing
 *              options and by limiting the returned information (can be
 *              NULL)
 * @res:        return value that will contain the resulting linked list of
 *              &struct addrinfo structures.
 *
 * Same as getaddrinfo() from POSIX excepting that mmlib error state will be
 * set in case of error.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_getaddrinfo(const char *node, const char *service,
                             const struct addrinfo *hints,
                              struct addrinfo **res);

/**
 * mm_getnamedinfo() - get name information
 * @addr:       socket address
 * @addrlen:    size of @addr
 * @host:       buffer receiving the host name
 * @hostlen:    size of buffer in @host
 * @serv:       buffer receiving the service name
 * @servlen:    size of buffer in @serv
 * @flags:      control of processing of mm_getnamedinfo()
 *
 * Same as getnamedinfo() from POSIX excepting that mmlib error state will
 * be set in case of error.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_getnamedinfo(const struct sockaddr *addr, socklen_t addrlen,
                              char *host, socklen_t hostlen,
                              char *serv, socklen_t servlen, int flags);

/**
 * mm_freeaddrinfo() - free linked list of address
 * @res:        linked list of addresses returned by @mm_getaddrinfo()
 *
 * Deallocate linked list of address allocated by a successfull call to
 * mm_getaddrinfo(). If @res is NULL, mm_getnamedinfo() do nothing.
 */
MMLIB_API void mm_freeaddrinfo(struct addrinfo *res);


/**
 * mm_create_sockclient() - Create a client socket and connect it to server
 * @uri:        URI indicating the ressource to connect
 *
 * This functions resolves URI resource, create a socket and try to connect
 * to resource. The service, protocol, port, hostname will be parsed from
 * @uri and the resulting socket will be configured and connected to the
 * resource.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_create_sockclient(const char* uri);


/**
 * mm_create_sockserver() - Create a listeing socket bound the address
 * @uri:        URI indicating the listening server
 *
 * This functions resolves URI resource, create a socket, bind to the
 * address and make it listening.  The service, protocol, port, hostname
 * will be parsed from @uri and the resulting socket will be bound the
 * resulting address.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
MMLIB_API int mm_create_sockserver(const char* uri);


#ifdef __cplusplus
}
#endif

#endif
