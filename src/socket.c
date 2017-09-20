/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmsysio.h"
#include "mmerrno.h"
#include <string.h>
#include <stdio.h>

#ifndef alloca
#  if HAVE_ALLOCA_H
#    include <alloca.h>
#  elif defined __GNUC__
#    define alloca __builtin_alloca
#  elif defined _AIX
#    define alloca __alloca
#  elif defined _MSC_VER
#    include <malloc.h>
#    define alloca _alloca
#  else
#    error Do not know how to call alloca
#  endif
#endif

static
int create_connected_socket(const char* service, const char *host, int port)
{
	struct addrinfo *ai, *res, hints = {.ai_family = AF_UNSPEC};
	int fd, family, socktype;
	struct sockaddr_in6* addrin6;
	struct sockaddr_in* addrin;
	struct mm_error_state errstate;

	// Name resolution
	if (mm_getaddrinfo(host, service, &hints, &res))
		return -1;

	// Create and connect socket (loop over all possible addresses)
	mm_save_errorstate(&errstate);
	for (ai=res; ai != NULL; ai = ai->ai_next) {
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


API_EXPORTED
int mm_create_sockclient(const char* uri)
{
	size_t len;
	char service[16];
	const char* host;
	int port = -1;
	int num_field;

	if (!uri)
		return mm_raise_error(EINVAL, "uri cannot be NULL");

	len = strlen(uri);
	host = alloca(len+1);

	num_field = sscanf("%[a-z]://%[^/:]:%hi", service, host, &port);
	if (num_field < 2) {
		mm_raise_error(EINVAL, "uri \"%s\" does follow"
		               " service://host or service://host:port"
		               " format", uri);
		return -1;
	}

	return create_connected_socket(service, host, port);
}
