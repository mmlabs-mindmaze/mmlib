/*
 * @mindmaze_header@
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
 * enum mm_known_dir - identifier of base folder
 * @MM_HOME: home folder of user
 * @MM_DATA_HOME: user-specific data files
 * @MM_CONFIG_HOME: user-specific configuration files
 * @MM_CACHE_HOME: user-specific non-essential (cached) data
 * @MM_RUNTIME_DIR: where user-specific runtime files and other
 *                  objects should be placed.
 * @MM_NUM_DIRTYPE: do not use (for internals of mmlib only)
 */
enum mm_known_dir {
	MM_HOME,
	MM_DATA_HOME,
	MM_CONFIG_HOME,
	MM_CACHE_HOME,
	MM_RUNTIME_DIR,
	MM_NUM_DIRTYPE,
};


/**
 * enum mm_env_action - used for setenv action argument
 * @MM_ENV_PRESERVE: preserve environment if set
 * @MM_ENV_OVERWRITE: overwrite environment
 * @MM_ENV_PREPEND: prepend to environment
 * @MM_ENV_APPEND: append to environment
 *
 * @MM_ENV_MAX: internal
 */
enum mm_env_action {
	MM_ENV_PRESERVE = 0,
	MM_ENV_OVERWRITE = 1,
	MM_ENV_PREPEND = 2,
	MM_ENV_APPEND = 3,

	MM_ENV_MAX
};


MMLIB_API const char* mm_getenv(const char* name, const char* default_value);
MMLIB_API int mm_setenv(const char* name, const char* value, int action);
MMLIB_API int mm_unsetenv(const char* name);
MMLIB_API char const* const* mm_get_environ(void);

MMLIB_API const char* mm_get_basedir(enum mm_known_dir dirtype);
MMLIB_API char* mm_path_from_basedir(enum mm_known_dir dirtype,
                                     const char* suffix);

MMLIB_API int mm_basename(char* dst, const char* path);
MMLIB_API int mm_dirname(char* dst, const char* path);

MMLIB_API void* mm_aligned_alloc(size_t alignment, size_t size);
MMLIB_API void mm_aligned_free(void* ptr);


/*************************************************************************
 *                                                                       *
 *                          stack allocation                             *
 *                                                                       *
 *************************************************************************/
#ifdef __BIGGEST_ALIGNMENT__
#  define MM_STK_ALIGN __BIGGEST_ALIGNMENT__
#else
#  define MM_STK_ALIGN 16
#endif

#define MM_STACK_ALLOC_THRESHOLD 2048

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
#define mm_aligned_alloca(alignment, size) \
	( (alignment) & (alignment-1) \
	  ? NULL \
	  : (void*)( ((uintptr_t)alloca((size)+(alignment)-1) + (alignment)-1) \
	             & ~(uintptr_t)((alignment)-1)))

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
 * case of successful allocation, the returned pointer must be passed to
 * mm_freea() before calling function returns to its caller.
 */
#define mm_malloca(size) \
	( (size) > MM_STACK_ALLOC_THRESHOLD \
	  ? _mm_malloca_on_heap(size) \
	  : mm_aligned_alloca(2*MM_STK_ALIGN, (size)))


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


/*
 * On windows, strcasecmp() is not defined, but _stricmp() is and performs the
 * exact same function. Since strcasecmp() is standard C and _stricmp() isn't,
 * choose to define based on strcasecmp's name.
 */
#ifdef _WIN32
#define mm_strcasecmp _stricmp
#else
#define mm_strcasecmp strcasecmp
#endif

#ifdef __cplusplus
}
#endif

#endif /* ifndef MMLIB_H */
