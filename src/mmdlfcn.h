/*
 * @mindmaze_header@
 */
#ifndef MMDLFCN_H
#define MMDLFCN_H

#include "mmpredefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mm_dynlib mm_dynlib_t;

#define MM_LD_LAZY       (1 << 0)
#define MM_LD_NOW        (1 << 1)
#define MM_LD_APPEND_EXT (1 << 2)

MMLIB_API mm_dynlib_t* mm_dlopen(const char* path, int flags);
MMLIB_API void mm_dlclose(mm_dynlib_t* handle);
MMLIB_API void* mm_dlsym(mm_dynlib_t* handle, const char* symbol);
MMLIB_API const char* mm_dl_fileext(void);

#ifdef __cplusplus
}
#endif

#endif /* ifndef MMDLFCN_H */
