/*
   @mindmaze_header@
*/
#ifndef MMPROFILE_H
#define MMPROFILE_H

#include "mmpredefs.h"

#define PROF_CURR	0x01
#define PROF_MIN	0x02
#define PROF_MAX	0x04
#define PROF_MEAN	0x08
#define PROF_DEFAULT	(PROF_MIN|PROF_MAX|PROF_MEAN)
#define PROF_FORCE_NSEC	0x10
#define PROF_FORCE_USEC	0x20
#define PROF_FORCE_MSEC	0x30
#define PROF_FORCE_SEC	0x40

#define PROF_RESET_CPUCLOCK     0x01
#define PROF_RESET_KEEPLABEL    0x02

#ifdef __cplusplus
extern "C" {
#endif

MMLIB_API void mmtic(void);
MMLIB_API void mmtoc(void);
MMLIB_API void mmtoc_label(const char* label);
MMLIB_API int mmprofile_print(int mask, int fd);
MMLIB_API void mmprofile_reset(int reset_flags);

#ifdef __cplusplus
}
#endif

#endif
