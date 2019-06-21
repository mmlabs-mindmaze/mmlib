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
#include <string.h>

#include "mmsysio.h"
#include "mmerrno.h"
#include "socket-internal.h"

#include <sys/socket.h>

union posix_sockopt {
	int ival;
	struct timeval timeout;
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
 * available :
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
 * are defined; Some systems may specify additional types :
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
API_EXPORTED
int mm_socket(int domain, int type)
{
	int fd;

	fd = socket(domain, type, 0);
	if (fd < 0)
		return mm_raise_from_errno("socket() failed");

	return fd;
}


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
API_EXPORTED
int mm_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	if (bind(sockfd, addr, addrlen) < 0)
		return mm_raise_from_errno("bind() failed");

	return 0;
}


/**
 * mm_getsockname() - returns  the  current address to which the socket sockfd is bound
 * @sockfd:     file descriptor to which the socket is bound
 * @addr:       points to a &struct sockaddr containing the bound address
 * @addrlen:    length of the &struct sockaddr pointed to by @addr
 *
 * getsockname() returns the current address to which the socket sockfd is bound,
 * in the buffer pointed to by @addr.
 * The @addrlen argument should be initialized to indicate the amount of space
 * (in bytes) pointed to by addr. On return it contains the actual size of the
 * socket address.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	if (getsockname(sockfd, addr, addrlen) < 0)
		return mm_raise_from_errno("getsockname() failed");

	return 0;
}


/**
 * mm_getpeername() - get name of connected peer socket
 * @sockfd:     file descriptor to which the socket is bound
 * @addr:       points to a &struct sockaddr containing the peer address
 * @addrlen:    length of the &struct sockaddr pointed to by @addr
 *
 * This obtains the address of the peer connected to the socket @sockfd in
 * the buffer pointed to by @addr. The @addrlen argument should be
 * initialized to indicate the amount of space (in bytes) pointed to by
 * addr. On return it contains the actual size of the socket address.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	if (getpeername(sockfd, addr, addrlen) < 0)
		return mm_raise_from_errno("getpeername() failed");

	return 0;
}


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
API_EXPORTED
int mm_listen(int sockfd, int backlog)
{
	if (listen(sockfd, backlog) < 0)
		return mm_raise_from_errno("listen() failed");

	return 0;
}


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
API_EXPORTED
int mm_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
	int fd;

	fd = accept(sockfd, addr, addrlen);
	if (fd < 0)
		return mm_raise_from_errno("accept() failed");

	return fd;
}


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
API_EXPORTED
int mm_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	if (connect(sockfd, addr, addrlen) < 0)
		return mm_raise_from_errno("connect() failed");

	return 0;
}


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


/**
 * mm_shutdown() - shut down socket send and receive operations
 * @sockfd:     file descriptor of the socket
 * @how:        type of shutdown
 *
 * This causes all or part of a full-duplex connection on the socket associated
 * with the file descriptor socket to be shut down. The type of shutdown is
 * controlled by @how which can be one of the following values :
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
API_EXPORTED
int mm_shutdown(int sockfd, int how)
{
	int ret;

	ret = shutdown(sockfd, how);
	if (ret)
		mm_raise_from_errno("shutdown() failed");

	return ret;
}


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
API_EXPORTED
ssize_t mm_send(int sockfd, const void *buffer, size_t length, int flags)
{
	ssize_t ret_sz;

	ret_sz = send(sockfd, buffer, length, flags);
	if (ret_sz < 0)
		return mm_raise_from_errno("send() failed");

	return ret_sz;
}


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
 * are formed by logically OR'ing zero or more of the following values :
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
API_EXPORTED
ssize_t mm_recv(int sockfd, void *buffer, size_t length, int flags)
{
	ssize_t ret_sz;

	ret_sz = recv(sockfd, buffer, length, flags);
	if (ret_sz <  0)
		return mm_raise_from_errno("recv() failed");

	return ret_sz;
}


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
API_EXPORTED
ssize_t mm_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	ssize_t ret_sz;

	ret_sz = sendmsg(sockfd, msg, flags);
	if (ret_sz < 0)
		return mm_raise_from_errno("sendmsg() failed");

	return ret_sz;
}


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
API_EXPORTED
ssize_t mm_recvmsg(int sockfd, struct msghdr* msg, int flags)
{
	ssize_t ret_sz;

	ret_sz = recvmsg(sockfd, msg, flags);
	if (ret_sz < 0)
		return mm_raise_from_errno("recvmsg failed");

	return ret_sz;
}


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


/**
 * mm_recv_multimsg() - receive multiple messages from a socket
 * @sockfd:     socket file descriptor.
 * @vlen:       size of @msgvec array
 * @msgvec:     pointer to an array of &struct mmsock_multimsg structures
 * @flags:      type of message reception
 * @timeout:    timeout for receive operation. If NULL, the operation blocks
 *              indefinitely
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
 *
 * Errors:
 * In case of failure, the error number reported in the error state
 * indicates the origin of failure :
 *
 * %MM_ENONAME
 *   The @node is not known.
 * %MM_ENOTFOUND
 *   The service is not known or not available for the requested socket
 *   type.
 * %EDDRNOTAVAILABLE
 *   Host is found but does not have any network address or in the requested
 *   family.
 * %EAGAIN
 *   The name server returned a temporary failure indication. Try again
 *   later.
 * %EAFNOSUPPORT
 *   Address family is not supported or address length was invalid for
 *   specifed family
 * %EINVAL
 *   Invalid salue in flags. Both @node and @service are NULL. %AI_CANONNAME
 *   set in flags but @node is NULL. %AI_NUMERICSERV set in flags but
 *   @service is not numeric port-number string.
 * %EPROTOTYPE
 *   Requested socket type is not supported or inconsistent with protocol.
 *
 * Other error can be reported by the platform is other case not listed
 * above.
 */
API_EXPORTED
int mm_getaddrinfo(const char *node, const char *service,
                   const struct addrinfo *hints,
		   struct addrinfo **res)
{
	int errnum;
	char errmsg[256];

	errnum = internal_getaddrinfo(node, service, hints, res, errmsg);
	if (errnum == 0)
		return 0;

	// Handle platform specific error
	if (errnum == -1) {
		errnum = errno;
		strerror_r(errnum, errmsg, sizeof(errmsg));
	}

	mm_raise_error(errnum, "getaddrinfo(%s, %s) failed: %s",
	               node, service, errmsg);
	return -1;
}


/**
 * mm_getnameinfo() - get name information
 * @addr:       socket address
 * @addrlen:    size of @addr
 * @host:       buffer receiving the host name
 * @hostlen:    size of buffer in @host
 * @serv:       buffer receiving the service name
 * @servlen:    size of buffer in @serv
 * @flags:      control of processing of mm_getnameinfo()
 *
 * Same as getnameinfo() from POSIX excepting that mmlib error state will
 * be set in case of error.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
API_EXPORTED
int mm_getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                   char *host, socklen_t hostlen,
                   char *serv, socklen_t servlen, int flags)
{
	int errnum;
	char errmsg[256];

	errnum = internal_getnameinfo(addr, addrlen, host, hostlen,
	                              serv, servlen, flags, errmsg);
	if (errnum == 0)
		return 0;

	// Handle platform specific error
	if (errnum == -1) {
		errnum = errno;
		strerror_r(errnum, errmsg, sizeof(errmsg));
	}

	mm_raise_error(errnum, "getnameinfo() failed: %s", errmsg);
	return -1;
}


/**
 * mm_freeaddrinfo() - free linked list of address
 * @res:        linked list of addresses returned by @mm_getaddrinfo()
 *
 * Deallocate linked list of address allocated by a successful call to
 * mm_getaddrinfo(). If @res is NULL, mm_getnameinfo() do nothing.
 */
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


/**
 * mm_poll() - waits for one of a set of file descriptors to become ready to perform I/O.
 * @fds:          array of struct pollfd. See below.
 * @nfds:         number of @fds passed in argument
 * @timeout_ms:   number of milliseconds that poll() should block waiting
 *
 * In each element of @fds array, &mm_pollfd.fd should be a *socket* file
 * descriptor.
 *
 * If @timeout_ms is set to 0, the call will return immediately even if no file
 * descriptors are ready. if @timeout_ms is negative, the call will block
 * indefinitely.
 *
 * &mm_pollfd.events contains a mask on revents with the following values :
 *
 * - MM_POLLIN: there is data to read
 * - MM_POLLOUT: writing is now possible
 *
 * &mm_pollfd.revents will contain the output events flags, a combination of
 * MM_POLLIN and MM_POLLOUT, or 0 if unset.
 *
 * Return:
 *   (>0) On success, the number of fds on which an event was raised
 *   (=0) zero if poll() returned because the timeout was reached
 *   (<0) a negative value on error
 */
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
