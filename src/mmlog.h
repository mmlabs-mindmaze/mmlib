/*
 * @mindmaze_header@
 */
#ifndef MMLOG_H
#define MMLOG_H

#include "mmpredefs.h"
#include <stdio.h>
#include <stdlib.h>

#define MM_LOG_NONE -1
#define MM_LOG_FATAL 0
#define MM_LOG_ERROR 1
#define MM_LOG_WARN 2
#define MM_LOG_INFO 3
#define MM_LOG_DEBUG 4

#ifndef MM_LOG_MAXLEVEL
#  define MML_OG_MAXLEVEL MM_LOG_DEBUG
#endif


#if defined __cplusplus
#define MM_LOG_VOID_CAST static_cast < void >
#else
#define MM_LOG_VOID_CAST (void)
#endif


#if MM_LOG_MAXLEVEL >= MM_LOG_FATAL
#define mm_log_fatal(...) \
	mm_log(MM_LOG_FATAL, MM_LOG_MODULE_NAME, __VA_ARGS__)
#else
#define mm_log_fatal(...) MM_LOG_VOID_CAST(0)
#endif

#if MM_LOG_MAXLEVEL >= MM_LOG_ERROR
#define mm_log_error(...) \
	mm_log(MM_LOG_ERROR, MM_LOG_MODULE_NAME, __VA_ARGS__)
#else
#define mm_log_error(...) MM_LOG_VOID_CAST(0)
#endif

#if MM_LOG_MAXLEVEL >= MM_LOG_WARN
#define mm_log_warn(...) \
	mm_log(MM_LOG_WARN, MM_LOG_MODULE_NAME, __VA_ARGS__)
#else
#define mm_log_warn(...) MM_LOG_VOID_CAST(0)
#endif

#if MM_LOG_MAXLEVEL >= MM_LOG_INFO
#define mm_log_info(...) \
	mm_log(MM_LOG_INFO, MM_LOG_MODULE_NAME, __VA_ARGS__)
#else
#define mm_log_info(...) MM_LOG_VOID_CAST(0)
#endif

#if MM_LOG_MAXLEVEL >= MM_LOG_DEBUG
#define mm_log_debug(...) \
	mm_log(MM_LOG_DEBUG, MM_LOG_MODULE_NAME, __VA_ARGS__)
#else
#define mm_log_debug(...) MM_LOG_VOID_CAST(0)
#endif

#define mm_crash(...) \
	do { \
		char msg[256]; /* size of max length of log line */ \
		snprintf(msg, sizeof(msg), __VA_ARGS__); \
		mm_log(MM_LOG_FATAL, MM_LOG_MODULE_NAME, \
		       "%s (%s() in %s:%i)", \
		       msg, __func__, __FILE__, __LINE__); \
		abort(); \
	} while (0)

#define mm_check(expr) \
	do { \
		if (UNLIKELY(!(expr))) { \
			mm_crash("mm_check(" #expr ") failed. "); \
		} \
	} while (0)

#ifdef __cplusplus
extern "C" {
#endif

MMLIB_API void mm_log(int lvl, const char* location, const char* msg, ...);

MMLIB_API int mm_log_set_maxlvl(int lvl);

#ifdef __cplusplus
}
#endif

#endif /*MMLOG_H*/
