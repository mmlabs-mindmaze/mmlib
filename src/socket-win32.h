/*
 *  @mindmaze_header@
 */
#ifndef SOCKET_WIN32_H
#define SOCKET_WIN32_H

#include <windows.h>


ssize_t sock_hnd_read(HANDLE hpipe, void* buf, size_t nbyte);
ssize_t sock_hnd_write(HANDLE hpipe, const void* buf, size_t nbyte);


#endif /* SOCKET_WIN32_H */
