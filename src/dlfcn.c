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
		               "in dynamic library (h=%p): %s", symbol, handle);
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
		               "in dynamic library (h=%p): %s", symbol,
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

API_EXPORTED
const char* mm_dl_fileext(void)
{
	static const char dynlib_ext[] = LT_MODULE_EXT;

	return dynlib_ext;
}


API_EXPORTED
mmdynlib_t* mm_dlopen(const char* path, int flags)
{
	size_t len;
	char* path_ext;
	mmdynlib_t* hnd;

	if (!path) {
		mm_raise_error(EINVAL, "path cannot be NULL");
		return NULL;
	}

	if ((flags & MMLD_NOW) && (flags & MMLD_LAZY)) {
		mm_raise_error(EINVAL, "MMLD_NOW and MMLD_LAZY flags cannot"
		                       " be set at the same time.");
		return NULL;
	}

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


API_EXPORTED
void mm_dlclose(mmdynlib_t* handle)
{
	if (!handle)
		return;

	arch_dlclose(handle);
}


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


