/*
   @mindmaze_header@
*/
#ifndef CLOCK_WIN32_H
#define CLOCK_WIN32_H

#include "mmtime.h"

void gettimespec_wallclock_w32(struct timespec* ts);
void gettimespec_monotonic_w32(struct timespec* ts);
void gettimespec_thread_w32(struct timespec* ts);
void gettimespec_process_w32(struct timespec* ts);

void getres_wallclock_w32(struct timespec* res);
void getres_monotonic_w32(struct timespec* res);
void getres_thread_w32(struct timespec* res);
void getres_process_w32(struct timespec* res);

#endif
