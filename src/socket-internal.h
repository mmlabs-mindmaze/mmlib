/*
 * @mindmaze_header@
 */
#ifndef SOCKET_INTERNAL_H
#define SOCKET_INTERNAL_H

int internal_getaddrinfo(const char *node, const char *service,
                         const struct addrinfo *hints, struct addrinfo **res,
                         char* errmsg);
int internal_getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                         char *host, socklen_t hostlen,
                         char *serv, socklen_t servlen, int flags,
                         char* errmsg);

#endif /* ifndef SOCKET_INTERNAL_H */
