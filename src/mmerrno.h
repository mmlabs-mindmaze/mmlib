/*
    Copyright (C) 2012  MindMaze SA
    All right reserved

    Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
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


#ifdef __cplusplus
extern "C" {
#endif

const char* mmstrerror(int errnum);
int mmstrerror_r(int errnum, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* MMERRNO */
