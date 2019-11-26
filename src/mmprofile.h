/*
 * @mindmaze_header@
 */
#ifndef MMPROFILE_H
#define MMPROFILE_H

#include "mmpredefs.h"

#define PROF_CURR       0x01
#define PROF_MIN        0x02
#define PROF_MAX        0x04
#define PROF_MEAN       0x08
#define PROF_MEDIAN     0x10
#define PROF_DEFAULT    (PROF_MIN|PROF_MAX|PROF_MEAN|PROF_MEDIAN)
#define PROF_FORCE_NSEC 0x100
#define PROF_FORCE_USEC 0x200
#define PROF_FORCE_MSEC 0x300
#define PROF_FORCE_SEC  0x400

#define PROF_RESET_CPUCLOCK  0x01
#define PROF_RESET_KEEPLABEL 0x02

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

MMLIB_API void mm_tic(void);
MMLIB_API void mm_toc(void);
MMLIB_API void mm_toc_label(const char* label);
MMLIB_API int mm_profile_print(int mask, int fd);
MMLIB_API void mm_profile_reset(int reset_flags);
MMLIB_API int64_t mm_profile_get_data(int measure_point, int type);

#ifdef __cplusplus
}
#endif

#endif /* ifndef MMPROFILE_H */
