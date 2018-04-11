/*
   @mindmaze_header@
*/
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "mmlib.h"
#include "mmerrno.h"
#include <stdlib.h>
#include "mmpredefs.h"

#ifdef HAVE__ALIGNED_MALLOC
#include <malloc.h>
#endif

static
void* internal_aligned_alloc(size_t alignment, size_t size)
{
	void * ptr = NULL;

#if   defined(HAVE_POSIX_MEMALIGN)

	int ret = posix_memalign(&ptr, alignment, size);
	if (ret) {
		errno = ret;
		ptr = NULL;
	}

#elif defined(HAVE_ALIGNED_ALLOC)

	ptr = aligned_alloc(alignment, size);

#elif defined(HAVE__ALIGNED_MALLOC)

	if (!MM_IS_POW2(alignment) || (alignment < sizeof(void*)))  {
		ptr = NULL;
		errno = EINVAL;
	} else {
		ptr = _aligned_malloc(size, alignment);
	}

#else
#  error Cannot find aligned allocation primitive
#endif

	return ptr;
}

API_EXPORTED
void* mm_aligned_alloc(size_t alignment, size_t size)
{
	void * ptr = internal_aligned_alloc(alignment, size);

	if (!ptr) {
		mm_raise_from_errno("Cannot allocate buffer (alignment=%zu, size=%zu)",
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


/**
 * _mm_malloca_on_heap() - heap memory allocation version of mm_malloca()
 * @size:       size of memory to be allocated
 *
 * Function called when mm_malloca() cannot allocate on stack because @size is
 * too big. The allocation will be attempted on heap.
 *
 * NOTE: although this is function is exported, this should not be used
 * anywhere excepting by the mm_malloca() macro.
 *
 * Return: the pointer on allocated memory in case of success. The return value
 * is then ensured to be of value MM_STK_ALIGN modulo (2*MM_STK_ALIGN). This
 * particularity will be used to recognize when a memory block has been
 * allocated on stack or on heap. In case of failure (likely due to @size too
 * large), NULL is returned.
 */
API_EXPORTED
void* _mm_malloca_on_heap(size_t size)
{
	char * ptr;
	size_t alloc_size;

	// Increase allocated size to guarantee alignment requirement
	alloc_size = size + 2*MM_STK_ALIGN;
	if (alloc_size < size) {
		mm_raise_error(ENOMEM, "size=%zu is too big", size);
		return NULL;
	}

	// Allocate memory block
	ptr = internal_aligned_alloc(2*MM_STK_ALIGN, alloc_size);
	if (ptr == NULL) {
		mm_raise_from_errno("malloca_on_heap(%zu) failed", alloc_size);
		return NULL;
	}

	// Get pointer aligned on MM_STK_ALIGN modulo (2*MM_STK_ALIGN)
	ptr += MM_STK_ALIGN;

	return ptr;
}


/**
 * _mm_freea_on_heap() - deallocate memory when mm_malloca() has used heap
 * @ptr:        memory block to deallocate
 *
 * Function called when mm_freea() has detected that @ptr has been allocated on
 * heap.
 *
 * NOTE: although this is function is exported, this should not be used
 * anywhere excepting by the mm_freea() macro.
 */
API_EXPORTED
void _mm_freea_on_heap(void* ptr)
{
	char* base = ptr;

	mm_aligned_free(base - MM_STK_ALIGN);
}
