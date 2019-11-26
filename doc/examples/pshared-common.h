/*
 * @mindmaze_header@
 */
#ifndef PSHARED_COMMON_H
#define PSHARED_COMMON_H

#include <mmthread.h>

#define SHM_CHILD_FD 3

struct pshared_data {
	mm_thr_mutex_t mutex;
	int len;
	char text[1024];
	mm_thr_mutex_t notif_mtx;
	mm_thr_cond_t notif_cond;
	int start;
};

#endif /* ifndef PSHARED_COMMON_H */
