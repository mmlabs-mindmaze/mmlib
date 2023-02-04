/*
 * @mindmaze_header@
 */
#ifndef MMSYSIO_H
#define MMSYSIO_H

#include <stddef.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#ifdef _WIN32
/*
 * WARNING
 * Windows enforces include of <windows.h> after <winsock2.h>
 * However, what gets included when you do the actual include of <windows.h>
 * depends on some macros of your project (eg. WIN32_LEAN_AND_MEAN), so
 * mmsysio.h cannot do this for you.
 * Should you stumble on warning such as this one:
 *   #warning Please include winsock2.h before windows.h
 * The easiest way around is probably for you to re-order your includes.
 * Other platforms NEVER enforce include order, so this will not have an
 * impact there.
 *
 * More info here:
 * https://docs.microsoft.com/en-us/windows/win32/winsock/creating-a-basic-winsock-application
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#endif /* ifdef _WIN32 */

#include "mmpredefs.h"
#include "mmtime.h"

#ifdef _WIN32
/**
 * struct iovec - structure for scatter/gather I/O.
 * @iov_base:   Base address of a memory region for input or output
 * @iov_len:    The size of the memory pointed to by @iov_base
 *
 * Note: on win32 this is guaranteed to alias to WSABUF
 */
struct iovec {
	unsigned long iov_len;
	void* iov_base;
};

// Not defined on windows platform, so we can keep the standard name
// without mm_ prefix
typedef unsigned long long uid_t;
typedef unsigned long long gid_t;

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

typedef long long mm_off_t;
typedef unsigned long long mm_dev_t;
typedef struct {
	unsigned long long id_low;
	unsigned long long id_high;
} mm_ino_t;

#else /* _WIN32 */

typedef off_t mm_off_t;
typedef dev_t mm_dev_t;
typedef ino_t mm_ino_t;

#endif /* _WIN32 */

/**
 * mm_ino_equal() - test equality between two mm_ino_t
 * @a:  first mm_ino_t operand
 * @b:  second mm_ino_t operand
 *
 * Return: 1 if equal, 0 otherwise
 */
static inline
int mm_ino_equal(mm_ino_t a, mm_ino_t b)
{
#ifdef _WIN32
	return ((a.id_low == b.id_low) && (a.id_high == b.id_high));
#else
	return (a == b);
#endif
}


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

#ifndef S_IFMT
#define S_IFMT _S_IFMT
#endif
#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif
#ifndef S_IFREG
#define S_IFREG _S_IFREG
#endif
#ifndef S_IFLNK
#define S_IFLNK (_S_IFREG|_S_IFCHR)
#endif

#define S_ISTYPE(mode, mask)  (((mode) & S_IFMT) == (mask))
#ifndef S_ISDIR
#define S_ISDIR(mode)   S_ISTYPE((mode), S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode)   S_ISTYPE((mode), S_IFREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(mode)   S_ISTYPE((mode), S_IFLNK)
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

#ifndef S_IRGRP
#define S_IRGRP 040
#endif
#ifndef S_IWGRP
#define S_IWGRP 020
#endif
#ifndef S_IXGRP
#define S_IXGRP 010
#endif
#ifndef S_IRWXG
#define S_IRWXG (S_IRGRP|S_IWGRP|S_IXGRP)
#endif

#ifndef S_IROTH
#define S_IROTH 04
#endif
#ifndef S_IWOTH
#define S_IWOTH 02
#endif
#ifndef S_IXOTH
#define S_IXOTH 01
#endif
#ifndef S_IRWXO
#define S_IRWXO (S_IROTH|S_IWOTH|S_IXOTH)
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

#define UTIME_NOW       (-1L)
#define UTIME_OMIT      (-2L)

#endif /* _WIN32 */

#define MODE_DEF        (1 << 16)
#define MODE_EXEC       (1 << 17)
#define MODE_XDEF       (MODE_DEF | MODE_EXEC)

/* file types returned when scanning a directory */
#define MM_DT_UNKNOWN 0
#define MM_DT_FIFO (1 << 1)
#define MM_DT_CHR (1 << 2)
#define MM_DT_BLK (1 << 3)
#define MM_DT_DIR (1 << 4)
#define MM_DT_REG (1 << 5)
#define MM_DT_LNK (1 << 6)
#define MM_DT_SOCK (1 << 7)
#define MM_DT_ANY (0XFF)

#define MM_RECURSIVE (1 << 31)
#define MM_FAILONERROR (1 << 30)
#define MM_NOFOLLOW (1 << 29)
#define MM_NOCOW (1 << 28)
#define MM_FORCECOW (1 << 27)

/**
 * struct mm_stat - file status data
 * @dev:        Device ID of device containing file
 * @ino:        File serial number
 * @mode:       Mode of file (Indicate file type and permission)
 * @nlink:      Number of hard links to the file
 * @uid:        Currently unused
 * @gid:        Currently unused
 * @size:       For regular files, the file size in bytes.
 *              For symbolic links, the length in bytes of the UTF-8
 *              pathname contained in the symbolic link (including null
 *              termination).
 * @atime:      time of last access
 * @nblocks:    Currently unused
 * @mtime:      time of last modification
 * @ctime:      time of last status change
 */
struct mm_stat {
	mm_dev_t dev;
	mm_ino_t ino;
	mode_t mode;
	int nlink;
	uid_t uid;
	gid_t gid;
	mm_off_t size;
	time_t atime;
	size_t nblocks;
	time_t mtime;
	time_t ctime;
};

MMLIB_API int mm_open(const char* path, int oflag, int mode);
MMLIB_API int mm_rename(const char* oldpath, const char * newpath);
MMLIB_API int mm_close(int fd);
MMLIB_API int mm_fsync(int fd);
MMLIB_API ssize_t mm_read(int fd, void* buf, size_t nbyte);
MMLIB_API ssize_t mm_write(int fd, const void* buf, size_t nbyte);
MMLIB_API mm_off_t mm_seek(int fd, mm_off_t offset, int whence);
MMLIB_API int mm_ftruncate(int fd, mm_off_t length);
MMLIB_API int mm_fstat(int fd, struct mm_stat* buf);
MMLIB_API int mm_stat(const char* path, struct mm_stat* buf, int flags);
MMLIB_API int mm_futimens(int fd, const struct mm_timespec ts[2]);
MMLIB_API int mm_utimens(const char* path,
                         const struct mm_timespec ts[2], int flags);
MMLIB_API int mm_check_access(const char* path, int amode);
MMLIB_API int mm_isatty(int fd);

MMLIB_API int mm_dup(int fd);
MMLIB_API int mm_dup2(int fd, int newfd);
MMLIB_API int mm_pipe(int pipefd[2]);

MMLIB_API int mm_unlink(const char* path);
MMLIB_API int mm_link(const char* oldpath, const char* newpath);
MMLIB_API int mm_symlink(const char* oldpath, const char* newpath);
MMLIB_API int mm_readlink(const char* path, char* buf, size_t bufsize);
MMLIB_API int mm_copy(const char* src, const char* dst, int flags, int mode);

MMLIB_API int mm_mkdir(const char* path, int mode, int flags);
MMLIB_API int mm_chdir(const char* path);
MMLIB_API char* mm_getcwd(char* buffer, size_t size);
MMLIB_API int mm_rmdir(const char* path);
MMLIB_API int mm_remove(const char* path, int flags);


/**************************************************************************
 *                      Directory navigation                              *
 **************************************************************************/
typedef struct mm_dirstream MM_DIR;

struct mm_dirent {
	size_t reclen;  /* this record length */
	int type;       /* file type (see above) */
	int id;         /* reserved for later use */
#ifndef __cplusplus
	char name[];    /* Null-terminated filename (C flexible array)*/
#else
	char name[1];   /* Null-terminated filename (C++ compatibility)*/
#endif
};

MMLIB_API MM_DIR* mm_opendir(const char* path);
MMLIB_API void mm_closedir(MM_DIR* dir);
MMLIB_API void mm_rewinddir(MM_DIR* dir);
MMLIB_API const struct mm_dirent* mm_readdir(MM_DIR* dir, int * status);


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
#define MM_SPAWN_KEEP_FDS 0x00000002   // Keep all inheritable fd in child

MMLIB_API int mm_spawn(mm_pid_t* child_pid, const char* path,
                       int num_map, const struct mm_remap_fd* fd_map,
                       int flags, char* const* argv, char* const* envp);
MMLIB_API int mm_execv(const char* path,
                       int num_map, const struct mm_remap_fd* fd_map,
                       int flags, char* const* argv, char* const* envp);

#define MM_WSTATUS_CODEMASK 0x000000FF
#define MM_WSTATUS_EXITED   0x00000100
#define MM_WSTATUS_SIGNALED 0x00000200

MMLIB_API int mm_wait_process(mm_pid_t pid, int* status);


/**************************************************************************
 *                            memory mapping                              *
 **************************************************************************/

#define MM_MAP_READ   0x00000001
#define MM_MAP_WRITE  0x00000002
#define MM_MAP_EXEC   0x00000004
#define MM_MAP_SHARED 0x00000008

#define MM_MAP_RDWR (MM_MAP_READ | MM_MAP_WRITE)
#define MM_MAP_PRIVATE 0x00000000


MMLIB_API void* mm_mapfile(int fd, mm_off_t offset, size_t len, int mflags);
MMLIB_API int mm_unmap(void* addr);

MMLIB_API int mm_shm_open(const char* name, int oflag, int mode);
MMLIB_API int mm_anon_shm(void);
MMLIB_API int mm_shm_unlink(const char* name);


/**************************************************************************
 *                        Interprocess communication                      *
 **************************************************************************/

/**
 * struct mm_ipc_msg - structure for IPC message
 * @iov:        scatter/gather array
 * @fds:        array of file descriptor to pass/receive
 * @num_iov:    number of element in @iov
 * @num_fds:    number of file descriptors in @fds
 * @flags:      flags on received message
 * @num_fds_max: maximum number of file descriptors in @fds
 * @reserved:   reserved for future use (must be NULL)
 */
struct mm_ipc_msg {
	struct iovec* iov;
	int* fds;
	int num_iov;
	int num_fds;
	int flags;
	int num_fds_max;
	void* reserved;
};

struct mm_ipc_srv;

MMLIB_API struct mm_ipc_srv* mm_ipc_srv_create(const char* addr);
MMLIB_API void mm_ipc_srv_destroy(struct mm_ipc_srv* srv);
MMLIB_API int mm_ipc_srv_accept(struct mm_ipc_srv* srv);
MMLIB_API int mm_ipc_connect(const char* addr);
MMLIB_API int mm_ipc_connected_pair(int fds[2]);
MMLIB_API ssize_t mm_ipc_sendmsg(int fd, const struct mm_ipc_msg* msg);
MMLIB_API ssize_t mm_ipc_recvmsg(int fd, struct mm_ipc_msg* msg);


/**************************************************************************
 *                          Network communication                         *
 **************************************************************************/

struct addrinfo;

#if defined (_WIN32)
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
struct msghdr {
	void*         msg_name;
	socklen_t msg_namelen;
	struct iovec* msg_iov;
	size_t msg_iovlen;
	void*         msg_control;
	socklen_t msg_controllen;
	int msg_flags;
};

#  define SHUT_RD 0
#  define SHUT_WR 1
#  define SHUT_RDWR 2

// The following constants are defined on Windows platform from ws2tcpip.h
// if _WIN32_WINNT is defined higher to 0x0600 (corresponding roughly to
// windows vista release). It is way below the support threshold of mmlib.
// We can then assume their availability. ws2tcpip.h is included at the top
// of this file but we cannot expect _WIN32_WINNT to be defined in the
// project using mmlib. Hence we define the constants to their right values
// if they are not defined yet.
#  ifndef AI_NUMERICSERV
#    define AI_NUMERICSERV 0x00000008
#  endif
#  ifndef AI_ALL
#    define AI_ALL 0x00000100
#  endif
#  ifndef AI_ADDRCONFIG
#    define AI_ADDRCONFIG 0x00000400
#  endif
#  ifndef AI_V4MAPPED
#    define AI_V4MAPPED 0x00000800
#  endif

#endif /* _WIN32 */


/**
 * struct mm_sock_multimsg - structure for transmitting multiple messages
 * @msg:        message
 * @datalen:    number of received byte for @msg
 *
 * This should alias with struct mm_sghdr on system supporting recvmmsg()
 */
struct mm_sock_multimsg {
	struct msghdr msg;
	unsigned int datalen;
};


MMLIB_API int mm_socket(int domain, int type, int protocol);
MMLIB_API int mm_bind(int sockfd, const struct sockaddr * addr, socklen_t
                      addrlen);
MMLIB_API int mm_getsockname(int sockfd, struct sockaddr * addr,
                             socklen_t * addrlen);
MMLIB_API int mm_getpeername(int sockfd, struct sockaddr * addr,
                             socklen_t * addrlen);
MMLIB_API int mm_listen(int sockfd, int backlog);
MMLIB_API int mm_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
MMLIB_API int mm_connect(int sockfd, const struct sockaddr * addr, socklen_t
                         addrlen);
MMLIB_API int mm_setsockopt(int sockfd, int level, int optname,
                            const void * optval, socklen_t optlen);

MMLIB_API int mm_getsockopt(int sockfd, int level, int optname,
                            void * optval, socklen_t* optlen);

MMLIB_API int mm_shutdown(int sockfd, int how);
MMLIB_API ssize_t mm_send(int sockfd, const void * buffer, size_t length, int
                          flags);
MMLIB_API ssize_t mm_recv(int sockfd, void * buffer, size_t length, int flags);
MMLIB_API ssize_t mm_sendmsg(int sockfd, const struct msghdr* msg, int flags);
MMLIB_API ssize_t mm_recvmsg(int sockfd, struct msghdr* msg, int flags);
MMLIB_API int mm_send_multimsg(int sockfd, int vlen,
                               struct mm_sock_multimsg * msgvec, int flags);

MMLIB_API int mm_recv_multimsg(int sockfd, int vlen,
                               struct mm_sock_multimsg * msgvec, int flags,
                               struct mm_timespec * timeout);

MMLIB_API int mm_getaddrinfo(const char * node, const char * service,
                             const struct addrinfo * hints,
                             struct addrinfo ** res);

MMLIB_API int mm_getnameinfo(const struct sockaddr * addr, socklen_t addrlen,
                             char * host, socklen_t hostlen,
                             char * serv, socklen_t servlen, int flags);

MMLIB_API void mm_freeaddrinfo(struct addrinfo * res);
MMLIB_API int mm_create_sockclient(const char* uri);

#if defined (_WIN32)
struct mm_pollfd {
	int fd;         /* file descriptor  */
	short events;   /* requested events */
	short revents;  /* returned events  */
};
#else
#define mm_pollfd pollfd
#endif

MMLIB_API int mm_poll(struct mm_pollfd * fds, int nfds, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ifndef MMSYSIO_H */
