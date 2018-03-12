/*
   @mindmaze_header@
*/
#ifndef MMLIB_H
#define MMLIB_H

#include <stddef.h>
#include <stdint.h>
#include "mmpredefs.h"

// declare alloca() macro
#ifdef _WIN32
#  include <malloc.h>
#  ifndef alloca
#    define alloca _alloca
#  endif
#else
#  include <alloca.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * mm_getenv() - Return environment variable or default value
 * @name:          name of the environment variable
 * @default_value: default value
 *
 * Return: the value set in the environment if the variable @name is
 * set. Otherwise @default_value is returned.
 */
MMLIB_API char* mm_getenv(const char* name, char* default_value);


/**
 * mm_setenv() - Add or change environment variable
 * @name:       name of the environment variable
 * @value:      value to set the environment variable called @name
 * @overwrite:  set to 0 if only add is permitted
 *
 * This updates or adds a variable in the environment of the calling
 * process. The @name argument points to a string containing the name of an
 * environment variable to be added or altered. The environment variable
 * will be set to the value to which @value points. If the environment
 * variable named by @name already exists and the value of overwrite is
 * non-zero, the function return success and the environment is updated. If
 * the environment variable named by @name already exists and the value of
 * @overwrite is zero, the function shall return success and the environment
 * shall remain unchanged.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_setenv(const char* name, char* value, int overwrite);


/**
 * mm_unsetenv() - remove an environment variable
 * @name:       name of environment variable to remove
 *
 * This function removes an environment variable from the environment of the
 * calling process. The @name argument points to a string, which is the name
 * of the variable to be removed. If the named variable does not exist in
 * the current environment, the environment shall be unchanged and the
 * function is considered to have completed successfully.
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 * accordingly.
 */
MMLIB_API int mm_unsetenv(const char* name);


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


/*************************************************************************
 *                                                                       *
 *                          stack allocation                             *
 *                                                                       *
 *************************************************************************/
#ifdef __BIGGEST_ALIGNMENT__
#  define MM_STK_ALIGN  __BIGGEST_ALIGNMENT__
#else
#  define MM_STK_ALIGN  16
#endif

#define MM_STACK_ALLOC_THRESHOLD        2048

// Do not use those function directly. There are meant ONLY for use in
// mm_malloca() and mm_freea()
MMLIB_API void* _mm_malloca_on_heap(size_t size);
MMLIB_API void _mm_freea_on_heap(void* ptr);


/**
 * mm_aligned_alloca() - allocates memory on the stack with alignment
 * @alignment:  alignment, must be a power of two
 * @size:       size of memory to be allocated
 *
 * This macro allocates @size bytes from the stack and the returned pointer
 * is ensured to be aligned on @alignment boundaries (if @alignment is a
 * power of two). If size is 0, mm_aligned_alloca() allocates a zero-length
 * item and returns a unique pointer to that item.
 *
 * Please note that more than @size byte are consumed from the stack (even in
 * case of if @size is 0). This is due to the overhead necessary for having
 * an aligned allocated memory block.
 *
 * The lifetime of the allocated object ends just before the calling
 * function returns to its caller. This is so even when mm_aligned_alloca()
 * is called within a nested block.
 *
 * WARNING: If is NOT safe to try allocate more than a page size on the
 * stack. In general it is even recommended to limit allocation under half
 * of a page. Ignoring this put the program under the thread of more than
 * simply a stack overflow: there will be a risk that the stack overflow
 * will not be detected and the execution to continue while corrupting both
 * heap and stack... For safe stack allocation, use mm_malloca()/mm_freea().
 *
 * Return: the pointer to the allocated space in case of success. Otherwise,
 * ie, if @alignment is not a power of two, NULL is returned.
*/
#define mm_aligned_alloca(alignment, size)                                 \
  ( (alignment) & (alignment-1)                                            \
    ? NULL                                                                 \
    :(void*)( ((uintptr_t)alloca((size)+(alignment)-1) + (alignment)-1)    \
            & ~(uintptr_t)((alignment)-1) ))

/**
 * mm_malloca() - safely allocates memory on the stack
 * @size:       size of memory to be allocated
 *
 * This macro allocates @size bytes from the stack if not too big (lower or
 * equal to MM_STACK_ALLOC_THRESHOLD) or on the heap. The returned pointer
 * is ensured to be aligned on a boundary suitable for any data type. If
 * @size is 0, mm_malloca() allocates a zero-length item and returns a valid
 * pointer to that item.
 *
 * Use this macro as a safer replacement of alloca() or Variable Length
 * Array (VLA).
 *
 * Return: pointer to the allocated space in case success, NULL otherwise.
 * The allocation might fail if the requested size is larger that the system
 * memory (or the OS do not overcommit and is running out of memory). In
 * case of successfull allocation, the returned pointer must be passed to
 * mm_freea() before calling function returns to its caller.
 */
#define mm_malloca(size)                                                 \
  ( (size) > MM_STACK_ALLOC_THRESHOLD                                    \
     ? _mm_malloca_on_heap(size)                                         \
     : mm_aligned_alloca(2*MM_STK_ALIGN, (size)) )


/**
 * mm_freea() - free memory allocated with mm_malloca()
 * @ptr:        memory object allocated by mm_malloca()
 *
 * This function free memory allocated by mm_malloca(). Please note that
 * when this function returns, the memory might not be reusable yet. If @ptr
 * has been allocated on stack, the memory will be reclaimed (hence
 * reusable) only when the function that has called mm_malloca() for
 * allocating @ptr will return to its caller.
 */
static inline
void mm_freea(void* ptr)
{
	// The type of allocation is recognised from the alignment of the
	// @ptr address. If @ptr is:
	//  - NULL: the allocation failed
	//  - 0 mod (2*MM_STK_ALIGN): allocated on stack
	//  - MM_STK_ALIGN mod (2*MM_STK_ALIGN): allocated on heap
	if ((uintptr_t)ptr & (2*MM_STK_ALIGN-1))
		_mm_freea_on_heap(ptr);
}

#ifdef __cplusplus
}
#endif

#endif
