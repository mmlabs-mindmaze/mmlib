/*
   @mindmaze_header@
*/
#ifndef MMPREDEFS_H
#define MMPREDEFS_H

/*
 Attributes of imported symbols from mmlib
*/
#ifndef MMLIB_API
#  ifdef _WIN32
#    define MMLIB_API __declspec(dllimport)
#  else
#    define MMLIB_API
#  endif
#endif

/*
 Double expansion is the usual trick to expand a preprocessor macro argument
 into a string
*/
#define MM_XSTRINGIFY(arg)	#arg
#define MM_STRINGIFY(arg)	MM_XSTRINGIFY(arg)

/*
 Setup the module name (MM_LOG_MODULE_NAME) as it appears in the log.
 If it is unset, it set it to a reasonable default.
 It is allowed to reset MM_LOG_MODULE_NAME in the source code: this will
 change the module name used for the next invocation of any  mm_log_* macro.
*/
#ifndef MM_LOG_MODULE_NAME
# ifdef PACKAGE_NAME
#  define MM_LOG_MODULE_NAME PACKAGE_NAME
# else
#  define MM_LOG_MODULE_NAME "unknown"
# endif
#endif


/*
 Macros MM_CONSTRUCTOR() / MM_DESTRUCTOR() to declare constructors and
 destructors. Those function are typically called before and after main in
 case of normal executable, or at time of load/unload in case of dynamic
 module (ie in the case of dlopen()/LoadLibrary())
*/
#if defined(__GNUC__)

#  define MM_CONSTRUCTOR(name) static void __attribute__((constructor)) name ## _ctor(void)
#  define MM_DESTRUCTOR(name) static void __attribute__((destructor)) name ## _dtor(void)

#elif defined(_MSC_VER)

#include <stdlib.h>

#  define MM_CONSTRUCTOR(name) \
  static void name ## _ctor(void); \
  static int name ## _ctor_wrapper(void) { name ## _ctor(); return 0; } \
  __pragma(section(".CRT$XCU",read)) \
  __declspec(allocate(".CRT$XCU")) static int (* array_ctor_ ## name)(void) = name ## _ctor_wrapper; \
  static void name ## _ctor(void)

#  define MM_DESTRUCTOR(name) \
  static void name ## _dtor(void); \
  static int name ## _reg_dtor_wrapper(void) { atexit(name ## _dtor); return 0; } \
  __pragma(section(".CRT$XCU",read)) \
  __declspec(allocate(".CRT$XCU")) static int (* array_dtor_ ## name)(void) = name ## _reg_dtor_wrapper; \
  static void name ## _dtor(void)

#else

#  warning MM_CONSTRUCTOR() cannot be defined

#endif


/*
 Define LIKELY() and UNLIKELY() macros to help compiler to optimize the
 right conditional branch. DO NOT ABUSE OF THEM. If you use it, you need to be
 sure that is the correct branching to optimize. In doubt, let the compiler
 do its guess.
 */
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)

#  ifndef LIKELY
#    define LIKELY(x)   __builtin_expect(!!(x), 1)
#  endif

#  ifndef UNLIKELY
#    define UNLIKELY(x) __builtin_expect(!!(x), 0)
#  endif

#else // most likely msvc

#  ifndef LIKELY
#    define LIKELY(x)   (x)
#  endif

#  ifndef UNLIKELY
#    define UNLIKELY(x) (x)
#  endif

#endif


/*
 Define NOINLINE attribute
 */
#ifndef NOINLINE
#  if defined(__GNUC__)
#    define NOINLINE    __attribute__ ((noinline))
#  elif defined (_MSC_VER)
#    define NOINLINE    __declspec(noinline)
#  else
#    define NOINLINE
#  endif
#endif


/*
 * Define nonnull attribute for function parameters
 */

/* for all attributes, or as parameter attribute with clang */
#ifndef NONNULL
#  if defined(__GNUC__)
#    define NONNULL __attribute__((nonnull))
#  else
#    define NONNULL
#  endif
#endif

/* for specifying a single attribute. function attribute only */
#ifndef NONNULL_ARGS
#  if defined(__GNUC__)
#    define NONNULL_ARGS(...) __attribute__((nonnull(__VA_ARGS__)))
#  else
#    define NONNULL_ARGS
#  endif
#endif


/*
 * Define macro to deprecate symbol
 * Usage:
 *   // Use of struct old_data is warned
 *   struct MM_DEPRECATED old_data {
 *     ...
 *   };
 *
 *   // call to old_func() is warned
 *   MM_DEPRECATED int old_func(int a, void* b);
 *
 *   // use of old_variable is warned
 *   MM_DEPRECATED int old_variable;
 */
#if defined(__GNUC__)
#  define MM_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#  define MM_DEPRECATED __declspec(deprecated)
#else
#  define MM_DEPRECATED
#endif



/*
 Macros to get the number of element in a C array.
 */
#define MM_NELEM(arr)	((int)(sizeof(arr)/sizeof(arr[0])))

/*
 * Return true if ival is a power of 2 OR null
 */
#define MM_IS_POW2(ival) (!((ival) & ((ival)-1)))

/*
 * Macros to get the page size
 * Overload by compiling with -DMM_PAGESZ or by defining it before.
 */
#ifndef MM_PAGESZ
#  define MM_PAGESZ 0x1000
#endif

#endif
