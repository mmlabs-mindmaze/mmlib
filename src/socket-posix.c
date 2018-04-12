/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <fcntl.h>

#include "mmsysio.h"
#include "mmerrno.h"

#include <sys/socket.h>

union posix_sockopt {
	int ival;
	struct timeval timeout;
};

static
int translate_eai_to_errnum(int eai_errcode)
{
	switch(eai_errcode) {
	case EAI_AGAIN:
		return EAGAIN;

	case EAI_ADDRFAMILY:
	case EAI_NONAME:
		return MM_ENOTFOUND;

	case EAI_NODATA:
		return EADDRNOTAVAIL;

	case EAI_BADFLAGS:
	case EAI_SERVICE:
		return EINVAL;

	case EAI_SOCKTYPE:
		return EPROTOTYPE;

	case EAI_MEMORY:
		return ENOMEM;

	case EAI_FAMILY:
		return EAFNOSUPPORT;

	case EAI_SYSTEM:
		return errno;

	default:
		return EIO;
	}
}


API_EXPORTED
int mm_socket(int domain, int type)
{
	int fd;

	fd = socket(domain, type, 0);
	if (fd < 0)
		return mm_raise_from_errno("socket() failed");

	return fd;
}


API_EXPORTED
int mm_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	if (bind(sockfd, addr, addrlen) < 0)
		return mm_raise_from_errno("bind() failed");

	return 0;
}


API_EXPORTED
int mm_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	if (getsockname(sockfd, addr, addrlen) < 0)
		return mm_raise_from_errno("getsockname() failed");

	return 0;
}


API_EXPORTED
int mm_listen(int sockfd, int backlog)
{
	if (listen(sockfd, backlog) < 0)
		return mm_raise_from_errno("listen() failed");

	return 0;
}


API_EXPORTED
int mm_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
	int fd;

	fd = accept(sockfd, addr, addrlen);
	if (fd < 0)
		return mm_raise_from_errno("accept() failed");

	return fd;
}


API_EXPORTED
int mm_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	if (connect(sockfd, addr, addrlen) < 0)
		return mm_raise_from_errno("connect() failed");

	return 0;
}


API_EXPORTED
int mm_setsockopt(int sockfd, int level, int optname,
                  const void *optval, socklen_t optlen)
{
	struct timeval timeout;
	int delay_ms;

	// If SO_RCVTIMEO/SO_SNDTIMEO, Posix mandates timeval structure.
	// Since we accept delay in ms  mapped to int, we do the conversion
	// now.
	if ( (level == SOL_SOCKET)
	  && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) ) {
		if (optlen != sizeof(int))
			return mm_raise_error(EINVAL, "bad option length,"
			                      " SO_RCVTIMEO/SO_SNDTIMEO"
					      " accepts int (timeout in ms)");

		delay_ms = *((int*)optval);
		timeout.tv_sec = delay_ms / 1000;
		timeout.tv_usec = (delay_ms % 1000) * 1000;

		optval = &timeout;
		optlen = sizeof(timeout);
	}

	if (setsockopt(sockfd, level, optname, optval, optlen) < 0)
		return mm_raise_from_errno("setsockopt() failed");

	return 0;
}


API_EXPORTED
int mm_getsockopt(int sockfd, int level, int optname,
                  void *optval, socklen_t* optlen)
{
	int ret, delay_ms;
	union posix_sockopt posix_opt;
	socklen_t posix_optlen;

	posix_optlen = sizeof(posix_opt);
	ret = getsockopt(sockfd, level, optname, &posix_opt, &posix_optlen);
	if (ret)
		return mm_raise_from_errno("getsockopt() failed");

	// If SO_RCVTIMEO/SO_SNDTIMEO, Posix mandates timeval structure.
	// Since we accept delay in ms mapped to int, we do the conversion
	// now.
	if ( (level == SOL_SOCKET)
	  && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) ) {
		delay_ms = posix_opt.timeout.tv_sec * 1000;
		delay_ms += posix_opt.timeout.tv_usec / 1000;

		posix_optlen = sizeof(int);
		posix_opt.ival = delay_ms;
	}

	if (*optlen > posix_optlen)
		*optlen = posix_optlen;

	memcpy(optval, &posix_opt, *optlen);
	return ret;
}


API_EXPORTED
int mm_shutdown(int sockfd, int how)
{
	int ret;

	ret = shutdown(sockfd, how);
	if (ret)
		mm_raise_from_errno("shutdown() failed");

	return ret;
}


API_EXPORTED
ssize_t mm_send(int sockfd, const void *buffer, size_t length, int flags)
{
	ssize_t ret_sz;

	ret_sz = send(sockfd, buffer, length, flags);
	if (ret_sz < 0)
		return mm_raise_from_errno("send() failed");

	return ret_sz;
}


API_EXPORTED
ssize_t mm_recv(int sockfd, void *buffer, size_t length, int flags)
{
	ssize_t ret_sz;

	ret_sz = recv(sockfd, buffer, length, flags);
	if (ret_sz <  0)
		return mm_raise_from_errno("recv() failed");

	return ret_sz;
}


API_EXPORTED
ssize_t mm_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	ssize_t ret_sz;

	ret_sz = sendmsg(sockfd, msg, flags);
	if (ret_sz < 0)
		return mm_raise_from_errno("sendmsg() failed");

	return ret_sz;
}


API_EXPORTED
ssize_t mm_recvmsg(int sockfd, struct msghdr* msg, int flags)
{
	ssize_t ret_sz;

	ret_sz = recvmsg(sockfd, msg, flags);
	if (ret_sz < 0)
		return mm_raise_from_errno("recvmsg failed");

	return ret_sz;
}


API_EXPORTED
int mm_send_multimsg(int sockfd, int vlen, struct mmsock_multimsg *msgvec,
                     int flags)
{
	int ret;
	struct mmsghdr* hdrvec = (struct mmsghdr*)msgvec;

	ret = sendmmsg(sockfd, hdrvec, vlen, flags);
	if (ret < 0)
		return mm_raise_from_errno("sendmmsg failed");

	return ret;
}


API_EXPORTED
int mm_recv_multimsg(int sockfd, int vlen, struct mmsock_multimsg *msgvec,
                     int flags, struct timespec *timeout)
{
	int ret;
	struct mmsghdr* hdrvec = (struct mmsghdr*)msgvec;

	ret = recvmmsg(sockfd, hdrvec, vlen, flags, timeout);
	if (ret < 0)
		return mm_raise_from_errno("recvmmsg failed");

	return ret;
}


API_EXPORTED
int mm_getaddrinfo(const char *node, const char *service,
                   const struct addrinfo *hints,
		   struct addrinfo **res)
{
	int retcode, errnum;
	const char* errmsg;

	retcode = getaddrinfo(node, service, hints, res);
	if (retcode != 0) {
		errnum = translate_eai_to_errnum(retcode);
		errmsg = gai_strerror(retcode);
		mm_raise_error(errnum, "getaddrinfo() failed: %s", errmsg);
		return -1;
	}

	return 0;
}


API_EXPORTED
int mm_getnamedinfo(const struct sockaddr *addr, socklen_t addrlen,
                    char *host, socklen_t hostlen,
                    char *serv, socklen_t servlen, int flags)
{
	int retcode, errnum;
	const char* errmsg;

	retcode = getnameinfo(addr, addrlen, host, hostlen, serv, servlen, flags);
	if (retcode != 0) {
		errnum = translate_eai_to_errnum(retcode);
		errmsg = gai_strerror(retcode);
		mm_raise_error(errnum, "getnamedinfo() failed: %s", errmsg);
		return -1;
	}

	return 0;
}


API_EXPORTED
void mm_freeaddrinfo(struct addrinfo *res)
{
	freeaddrinfo(res);
}

static
int is_valid_fd(int fd)
{
    return (fcntl(fd, F_GETFL) != -1) || (errno != EBADF);
}

API_EXPORTED
int mm_poll(struct mm_pollfd *fds, int nfds, int timeout_ms)
{
	int i, rv;

	for (i = 0 ; i < nfds ; i++)
		if (!is_valid_fd(fds[i].fd))
			return -1;

	rv = poll(fds, nfds, timeout_ms);
	if (rv < 0)
		return mm_raise_from_errno("poll() failed");

	for (i = 0 ; i < nfds ; i++) {
		/* if an error occurrent within poll() processing the socket
		 * return it instead of flagging it */
		if (fds[i].events & (POLLNVAL | POLLERR))
			return -1;

		/* only return POLLIN and POLLOUT flags */
		fds[i].events &= (POLLIN | POLLOUT);
	}

	return rv;
}
