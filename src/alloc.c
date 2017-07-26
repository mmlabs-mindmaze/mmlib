/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmlib.h"
#include "mmerrno.h"
#include <stdlib.h>

#ifdef HAVE__ALIGNED_MALLOC
#include <malloc.h>
#endif


API_EXPORTED
void* mm_aligned_alloc(size_t alignment, size_t size)
{
	void* ptr;

#if   defined(HAVE_POSIX_MEMALIGN)

	int ret = posix_memalign(&ptr, alignment, size);
	if (ret) {
		errno = ret;
		ptr = NULL;
	}

#elif defined(HAVE_ALIGNED_ALLOC)

	ptr = aligned_alloc(alignment, size);

#elif defined(HAVE__ALIGNED_MALLOC)

	ptr = _aligned_malloc(size, alignment);

#else
#  error Cannot find aligned allocation primitive
#endif

	if (!ptr) {
		mm_raise_from_errno("Cannot allocate buffer (alignment=%zi, size=%zi)",
		                     alignment, size);
		return NULL;
	}

	return ptr;
}


API_EXPORTED
void mm_aligned_free(void* ptr)
{
#ifdef HAVE__ALIGNED_MALLOC
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

