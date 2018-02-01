/*
   @mindmaze_header@
*/
#ifndef PSHARED_COMMON_H
#define PSHARED_COMMON_H

#include <mmthread.h>

#define SHM_CHILD_FD    3

struct pshared_data {
	mmthr_mtx_t mutex;
	int len;
	char text[1024];
	mmthr_mtx_t notif_mtx;
	mmthr_cond_t notif_cond;
	int start;
};

#endif
