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

/**
 * mm_dlopen() - Load library dynamically
 * @path:       path of the library to load
 * @flags:      flags controlling how the library is loaded
 *
 * This function makes the symbols (function identifiers and data object
 * identifiers) in the shared library specified by @path available to the
 * calling process. A successful mm_dlopen() returns an handle which the
 * caller may use on subsequent calls to mm_dlsym() and mm_dlclose().
 *
 * The behavior of the function can be controlled by @flags which must be a
 * OR-combination of the following flags:
 *
 * MMLD_LAZY
 *   Relocations shall be performed at an implementation-defined time,
 *   ranging from the time of the imm_dlopen() call until the first reference to
 *   a given symbol occurs. Currently, this has no effect on Windows
 *   platform.
 *
 * MMLD_NOW
 *   All necessary relocations shall be performed when shared library is
 *   first loaded. This may waste some processing if relocations are
 *   performed for symbols that are never referenced. This behavior may be
 *   useful for applications that need to know that all symbols referenced
 *   during execution will be available before mm_dlopen() returns.
 *   Currently this is the only possible behavior for Windows platform.
 *
 * MMLD_APPEND_EXT
 *   If set, mm_dlopen() append automatically the usual file extension
 *   of a shared library (OS dependent) to @path and load this file instead.
 *   This flag allows to write code that is fully platform independent.
 *
 * Return: In case of success, mm_dlopen() return a non-NULL handle.
 * Otherwise NULL is returned and error state is set accordingly.
 */
MMLIB_API mmdynlib_t* mm_dlopen(const char* path, int flags);


/**
 * mm_dlclose() - Close an handle of an dynamic library
 * @handle:     handle of library to close
 *
 * this informs the system that the library specified by handle is no longer
 * needed by the application.  Once the handle has been closed, an
 * application should assume that any symbols (function identifiers and data
 * object identifiers) made visible using @handle, are no longer available
 * to the process.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly
 */
MMLIB_API void mm_dlclose(mmdynlib_t* handle);

/**
 * mm_dlsym() - get the address of a symbol from library handle
 * @handle:     handle of library
 * @symbol:     function or a data object identifier
 *
 * This obtain the address of a symbol (a function identifier or a data
 * object identifier) defined in the symbol table identified by the handle
 * argument.
 *
 * Return: pointer to the symbol if found, NULL otherwise with error state
 * set acccordingly
 */
MMLIB_API void* mm_dlsym(mmdynlib_t* handle, const char* symbol);

/**
 * mm_dl_fileext() - get usual shared library extension
 *
 * Return: the usual shared library extension of the platform.
 */
MMLIB_API const char* mm_dl_fileext(void);

#ifdef __cplusplus
}
#endif

#endif
