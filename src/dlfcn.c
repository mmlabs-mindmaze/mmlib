/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmdlfcn.h"
#include "mmerrno.h"
#include "mmlib.h"
#include <string.h>

static mmdynlib_t* arch_dlopen(const char *path, int flags);
static void arch_dlclose(mmdynlib_t* handle);
static void* arch_dlsym(mmdynlib_t* handle, const char* symbol);

/**************************************************************************
 *                                                                        *
 *                         Architecture specific                          *
 *                                                                        *
 **************************************************************************/


#ifdef _WIN32

/**************************************************************************
 *                            Windows version                             *
 **************************************************************************/

#include <windows.h>


static
mmdynlib_t* arch_dlopen(const char *path, int flags)
{
	HMODULE handle;
	(void)flags;

	if (path == NULL)
		handle = GetModuleHandle(NULL);
	else
		handle = LoadLibrary(path);

	if (!handle) {
		mm_raise_error(EIO, "Can't open dynamic library %s", path);
		return NULL;
	}

	return (mmdynlib_t*)handle;
}


static
void arch_dlclose(mmdynlib_t* handle)
{
	FreeLibrary((HMODULE)handle);
}


static
void* arch_dlsym(mmdynlib_t* handle, const char* symbol)
{
	void* ptr = (void*)GetProcAddress((HMODULE)handle, symbol);
	if (!ptr) {
		mm_raise_error(MM_ENOTFOUND, "symbol (%s) could not be found"
		               " in dynamic library (h=%p): %s", symbol, handle);
		return NULL;
	}

	return ptr;
}


#elif HAVE_DLOPEN

/**************************************************************************
 *                              POSIX version                             *
 **************************************************************************/

#include <dlfcn.h>

static
mmdynlib_t* arch_dlopen(const char *path, int flags)
{
	void* handle;
	int dlflags = 0;

	// Default is lazy binding
	if (flags & MMLD_NOW)
		dlflags |= RTLD_NOW;
	else
		dlflags |= RTLD_LAZY;

	handle = dlopen(path, dlflags);
	if (!handle) {
		mm_raise_error(ELIBEXEC, "Can't open dynamic library %s"
		               " (mode %08x): %s", path, dlflags, dlerror());
		return NULL;
	}

	return handle;
}


static
void arch_dlclose(mmdynlib_t* handle)
{
	dlclose(handle);
}


static
void* arch_dlsym(mmdynlib_t* handle, const char* symbol)
{
	void* ptr = dlsym(handle, symbol);
	if (!ptr) {
		mm_raise_error(MM_ENOTFOUND, "symbol (%s) could not be found"
		               " in dynamic library (h=%p): %s", symbol,
			       handle, dlerror());
		return NULL;
	}

	return ptr;
}

#else

/**************************************************************************
 *                              Other version                             *
 **************************************************************************/
#error "dynamic loading facility unknown"

#endif


/**************************************************************************
 *                                                                        *
 *                            API implementation                          *
 *                                                                        *
 **************************************************************************/


/**
 * mm_dl_fileext() - get usual shared library extension
 *
 * Return: the usual shared library extension of the platform.
 */
API_EXPORTED
const char* mm_dl_fileext(void)
{
	static const char dynlib_ext[] = LT_MODULE_EXT;

	return dynlib_ext;
}


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
 * If path is NULL, this will return a handle to the main program.
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
API_EXPORTED
mmdynlib_t* mm_dlopen(const char* path, int flags)
{
	size_t len;
	char* path_ext;
	mmdynlib_t* hnd;

	if ((flags & MMLD_NOW) && (flags & MMLD_LAZY)) {
		mm_raise_error(EINVAL, "MMLD_NOW and MMLD_LAZY flags cannot"
		                       " be set at the same time.");
		return NULL;
	}
	if (path == NULL)
		return arch_dlopen(NULL, flags);

	len = strlen(path);
	path_ext = mm_malloca(len+sizeof(LT_MODULE_EXT));
	if (!path_ext)
		return NULL;

	// Form dynamic libray filename
	strcpy(path_ext, path);
	if (flags & MMLD_APPEND_EXT)
		strcat(path_ext, LT_MODULE_EXT);

	hnd = arch_dlopen(path_ext, flags);

	mm_freea(path_ext);
	return hnd;
}


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
API_EXPORTED
void mm_dlclose(mmdynlib_t* handle)
{
	if (!handle)
		return;

	arch_dlclose(handle);
}


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
API_EXPORTED
void* mm_dlsym(mmdynlib_t* handle, const char* symbol)
{
	if (!handle || !symbol) {
		mm_raise_error(EINVAL, "invalid handle (%p) or symbol (%s) "
		                       " arguments", handle, symbol);
		return NULL;
	}

	return arch_dlsym(handle, symbol);
}


