/*
   @mindmaze_header@
*/
#ifndef MMERRNO_H
#define MMERRNO_H

#include <stddef.h>
#include <errno.h>

#define MM_EDISCONNECTED	1000
#define MM_EUNKNOWNUSER		1001
#define MM_EWRONGPWD		1002
#define MM_EWRONGSTATE		1003
#define MM_ETOOMANY		1004
#define MM_ENOTFOUND		1005
#define MM_EBADFMT		1006
#define MM_ENOCALIB		1007
#define MM_ENOINERTIAL		1008
#define MM_ECAMERROR		1009


#ifdef __cplusplus
extern "C" {
#endif

const char* mmstrerror(int errnum);
int mmstrerror_r(int errnum, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* MMERRNO */
