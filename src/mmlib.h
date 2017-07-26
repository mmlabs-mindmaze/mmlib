/*
   @mindmaze_header@
*/
#ifndef MMLIB_H
#define MMLIB_H

#include <stddef.h>
#include "mmpredefs.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * mm_aligned_alloc() - Allocate memory on a specified alignment boundary.
 * @alignment:  alignment value, must be a power of 2
 * @size:       size of the requested memory allocation
 *
 * This allocates a block of @size bytes whose address is a multiple of
 * @alignment.
 *
 * Use mm_aligned_free() to deallocate data returned by mm_aligned_alloc().
 *
 * Returns: A pointer to the memory block that was allocated in case of
 * success. Otherwise NULL is returned and error state set accordingly
 */
MMLIB_API void* mm_aligned_alloc(size_t alignment, size_t size);


/**
 * mm_aligned_free() - Free memory allocated with mm_aligned_alloc()
 * @ptr:        data to dellocate
 *
 * This function cause the space pointed to by @ptr to be deallocated. If
 * ptr is a NULL pointer, no action occur (this is not an error). Otherwise
 * the behavior is undefined if the space has not been allocated with
 * mm_aligned_alloc().
 */
MMLIB_API void mm_aligned_free(void* ptr);


#ifdef __cplusplus
}
#endif

#endif
