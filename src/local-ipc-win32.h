/*
 *  @mindmaze_header@
 */
#ifndef LOCAL_IPC_WIN32_H
#define LOCAL_IPC_WIN32_H

#include <windows.h>


ssize_t ipc_hnd_read(HANDLE hpipe, void* buf, size_t nbyte);
ssize_t ipc_hnd_write(HANDLE hpipe, const void* buf, size_t nbyte);


#endif /* LOCAL_IPC_WIN32_H */
