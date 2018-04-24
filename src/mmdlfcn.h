/*
   @mindmaze_header@
*/
#ifndef MMDLFCN_H
#define MMDLFCN_H

#include "mmpredefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mmdynlib mmdynlib_t;

#define MMLD_LAZY       0x00000001
#define MMLD_NOW        0x00000002
#define MMLD_APPEND_EXT 0x00000004

MMLIB_API mmdynlib_t* mm_dlopen(const char* path, int flags);
MMLIB_API void mm_dlclose(mmdynlib_t* handle);
MMLIB_API void* mm_dlsym(mmdynlib_t* handle, const char* symbol);
MMLIB_API const char* mm_dl_fileext(void);

#ifdef __cplusplus
}
#endif

#endif
