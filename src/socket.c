/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

// Necessary for using some non standard error code returned by getaddrinfo()
// on GNU/Linux platforms
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mmsysio.h"
#include "mmerrno.h"
#include "mmlib.h"
#include "mmpredefs.h"
#include "socket-internal.h"
#include <string.h>
#include <stdio.h>


#ifdef _WIN32
#  ifndef EAI_OVERFLOW
#    define EAI_OVERFLOW ERROR_BUFFER_OVERFLOW
#  endif
#  ifndef EAI_NODATA
#    define EAI_NODATA WSANO_DATA
#  endif
#endif


/**
 * is_numeric_string() - test if a string contains unsigned numeric value
 * @str:        string to test (may be NULL)
 *
 * @str pointing to NULL or an empty string will not be considered as a
 * numeric value.
 *
 * Return: 1 if @str contains an unsigned numeric value, 0 otherwise.
 */
static
int is_numeric_string(const char* str)
{
	if (!str || *str == '\0')
		return 0;

	while (*str) {
		if (*str < '0' || *str > '9')
			return 0;

		str++;
	}

	return 1;
}


/**************************************************************************
 *                                                                        *
 *                    Common socket internals                             *
 *                                                                        *
 **************************************************************************/

static
const struct {
	int eai;
	int errnum;
	const char* msg;
} eai_error_cases[] = {
#ifdef EAI_ADDRFAMILY
	{EAI_ADDRFAMILY, EADDRNOTAVAIL,
	 "host does not have network address in requested family"},
#endif
	{EAI_AGAIN, EAGAIN,
	 "The name server returned a temporary failure. Try again later."},
	{EAI_FAMILY, EAFNOSUPPORT,
	 "Address family was not recognized or address length was invalid "
	 "for the specified family"},
	{EAI_SERVICE, MM_ENOTFOUND,
	 "Requested service not available for the requested socket type"},
	{EAI_BADFLAGS, EINVAL, "invalid value in flags"},
	{EAI_FAIL, EIO, "A non recoverable error occurred"},
	{EAI_MEMORY, ENOMEM, "Out of memory"},
#ifdef EAI_NODATA
	{EAI_NODATA, EADDRNOTAVAIL, "host doesn't have any network addresses"},
#endif
	{EAI_NONAME, MM_ENONAME, "Node is not known"},
	{EAI_OVERFLOW, EOVERFLOW, "host or serv buffer is too small"},
	{EAI_SOCKTYPE, EPROTOTYPE,
	 "requested socket type not supported or inconsistent with protocol"},
};


/**
 * translate_eai_to_errnum() - translate EAI_* return code into error code
 * @eai:        return value of getaddrinfo() or getnameinfo()
 * @errmsg:     pointer to buffer that will receive the error message
 *
 * Return: the translated error code if an error case has been matched, -1
 * otherwise.
 */
static
int translate_eai_to_errnum(int eai, char* errmsg)
{
	int i;

	// Try to find an eai return value that match an know error case
	for (i = 0; i < MM_NELEM(eai_error_cases); i++) {
		if (eai_error_cases[i].eai == eai) {
			strcpy(errmsg, eai_error_cases[i].msg);
			return eai_error_cases[i].errnum;
		}
	}

	errmsg[0] = '\0';
	return -1;
}


/**
 * internal_getaddrinfo() - get address information
 * @node:       descriptive name or address string (can be NULL)
 * @service:    string identifying the requested service (can be NULL)
 * @hints:      input values that may direct the operation by providing
 *              options and by limiting the returned information (can be
 *              NULL)
 * @res:        return value that will contain the resulting linked list of
 *              &struct addrinfo structures.
 * @errmsg:     buffer receiving the error message if translated from EAI
 *
 * Same as mm_getaddrinfo() excepting that error code is returned in return
 * value and associated error message is written in @errmsg if the error is
 * not platform specific.
 *
 * Return: 0 in case of success, > 0 if a error translated from EAI_*
 * returned value has been found, -1 if the error must be translated from
 * the system (maybe platform specific).
 */
LOCAL_SYMBOL
int internal_getaddrinfo(const char * node, const char * service,
                         const struct addrinfo * hints, struct addrinfo ** res,
                         char* errmsg)
{
	int rv;

	rv = getaddrinfo(node, service, hints, res);
	if (rv == 0)
		return 0;

	if (!node && !service) {
		strcpy(errmsg, "Both node and service are NULL");
		return EINVAL;
	}

	if ((hints != NULL) && (hints->ai_flags & AI_CANONNAME) && !node) {
		strcpy(errmsg, "FQDN requested but node is NULL");
		return EINVAL;
	}

	if ((hints != NULL)
	    && (hints->ai_flags & AI_NUMERICSERV)
	    && !is_numeric_string(service)) {
		strcpy(errmsg, "while requested, service is not "
		       "numeric port-number string");
		return EINVAL;
	}

	return translate_eai_to_errnum(rv, errmsg);
}


/**
 * internal_getnameinfo() - get name information
 * @addr:       socket address
 * @addrlen:    size of @addr
 * @host:       buffer receiving the host name
 * @hostlen:    size of buffer in @host
 * @serv:       buffer receiving the service name
 * @servlen:    size of buffer in @serv
 * @flags:      control of processing of mm_getnameinfo()
 * @errmsg:     buffer receiving the error message if translated from EAI
 *
 * Same as mm_getnameinfo() excepting that error code is returned in return
 * value and associated error message is written in @errmsg if the error is
 * not platform specific.
 *
 * Return: 0 in case of success, > 0 if a error translated from EAI_*
 * returned value has been found, -1 if the error must be translated from
 * the system (maybe platform specific).
 */
LOCAL_SYMBOL
int internal_getnameinfo(const struct sockaddr * addr, socklen_t addrlen,
                         char * host, socklen_t hostlen,
                         char * serv, socklen_t servlen, int flags,
                         char* errmsg)
{
	int rv;

	rv = getnameinfo(addr, addrlen, host, hostlen, serv, servlen, flags);
	if (rv == 0)
		return 0;

	return translate_eai_to_errnum(rv, errmsg);
}


/**************************************************************************
 *                                                                        *
 *                    Exported socket helpers                             *
 *                                                                        *
 **************************************************************************/

static struct {
	char name[8];
	int socktype;
} protocol_services[] = {
	{"tcp", SOCK_STREAM},
	{"udp", SOCK_DGRAM},
};


static
int get_socktype_from_protocol_services(const char* service)
{
	int i;

	for (i = 0; i < MM_NELEM(protocol_services); i++) {
		if (!strcmp(protocol_services[i].name, service))
			return protocol_services[i].socktype;
	}

	// No match, so return unspecified socket type
	return 0;
}


static
int create_connected_socket(const char* service, const char * host, int port,
                            const struct addrinfo* hints)
{
	struct addrinfo * ai, * res;
	int fd, family, socktype;
	struct sockaddr_in6* addrin6;
	struct sockaddr_in* addrin;
	struct mm_error_state errstate;

	fd = -1;

	// Name resolution
	if (mm_getaddrinfo(host, service, hints, &res))
		return -1;

	// Create and connect socket (loop over all possible addresses)
	mm_save_errorstate(&errstate);
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		family = ai->ai_family;
		socktype = ai->ai_socktype;

		// Override port is specified
		if (port > 0) {
			if (family == AF_INET6) {
				addrin6 = (struct sockaddr_in6*)ai->ai_addr;
				addrin6->sin6_port = port;
			} else if (family == AF_INET) {
				addrin = (struct sockaddr_in*)ai->ai_addr;
				addrin->sin_port = port;
			}
		}

		// Try create the socket and connect
		if ((fd = mm_socket(family, socktype)) < 0
		    || mm_connect(fd, ai->ai_addr, ai->ai_addrlen)) {
			mm_close(fd);
			fd = -1;
		} else {
			// We succeed, so we need to pop the last error
			mm_set_errorstate(&errstate);
			break;
		}
	}

	mm_freeaddrinfo(res);
	return fd;
}


/**
 * mm_create_sockclient() - Create a client socket and connect it to server
 * @uri:        URI indicating the resource to connect to
 *
 * This functions resolves URI resource, create a socket and try to connect
 * to resource. The service, protocol, port, hostname will be parsed from
 * @uri and the resulting socket will be configured and connected to the
 * resource.
 *
 * In addition to the normal services registered in the system, the function
 * supports tcp and udp as scheme in the URI. In such a case, the port number
 * must be specified in the URI, otherwise the function will fail.
 *
 * Return: a non-negative integer representing the file descriptor in case
 * of success. Otherwise -1 is returned with error state set accordingly.
 */
API_EXPORTED
int mm_create_sockclient(const char* uri)
{
	size_t len;
	char service[16];
	char* host;
	int port = -1;
	int num_field, retval;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
	};

	if (!uri)
		return mm_raise_error(EINVAL, "uri cannot be NULL");

	len = strlen(uri);
	host = mm_malloca(len+1);
	if (!host)
		return -1;

	num_field = sscanf(uri, "%[a-z]://%[^/:]:%i", service, host, &port);
	if (num_field < 2) {
		mm_raise_error(EINVAL, "uri \"%s\" does not follow "
		               "service://host or service://host:port "
		               "format", uri);
		return -1;
	}

	// Force socket type from service name if tcp:// or udp://.
	// Otherwise the socket type will be 0 (ie unspecified) and will be
	// left to mm_getaddrinfo() to propose proper matches.
	hints.ai_socktype = get_socktype_from_protocol_services(service);
	if (hints.ai_socktype != 0) {
		if (port < 0) {
			mm_raise_error(EINVAL, "port must be specified "
			               "with %s", service);
			return -1;
		}

		sprintf(service, "%i", port);
		hints.ai_flags |= AI_NUMERICSERV;
		port = -1;
	}

	retval = create_connected_socket(service, host, port, &hints);

	mm_freea(host);
	return retval;
}
