/*
   @mindmaze_header@
*/
#ifndef MMLOG_H
#define MMLOG_H

#define MMLOG_NONE	-1
#define MMLOG_FATAL	0
#define MMLOG_ERROR	1
#define MMLOG_WARN	2
#define MMLOG_INFO	3
#define MMLOG_DEBUG	4

#ifndef MMLOG_MAXLEVEL
#  define MMLOG_MAXLEVEL	MMLOG_DEBUG
#endif


/*
 Double expansion is the usual trick to expand a preprocessor macro argument
 into a string
*/
#define mm_xstringify(arg)	#arg
#define mm_stringify(arg)	mm_xstringify(arg)

/*
 Define MMLOG_VERBOSE_LOCATION to display location with file and line
*/
#if MMLOG_VERBOSE_LOCATION
# define mm_location(module)	 module " " __FILE__ "(" mm_stringify( __LINE__ ) ")"
#else
# define mm_location(module)	 module
#endif

/*
 Setup the module name (MMLOG_MODULE_NAME) as it appears in the log.
 If it is unset, it set it to a reasonable default.
 It is allowed to reset MMLOG_MODULE_NAME in the source code: this will
 change the module name used for the next invocation of any  mmlog_* macro.
*/
#ifndef MMLOG_MODULE_NAME
# ifdef PACKAGE_NAME
#  define MMLOG_MODULE_NAME PACKAGE_NAME
# else
#  define MMLOG_MODULE_NAME "unknown"
# endif
#endif


#if defined __cplusplus
# define MMLOG_VOID_CAST static_cast<void>
#else
# define MMLOG_VOID_CAST (void)
#endif



#if MMLOG_MAXLEVEL >= MMLOG_FATAL
#  define mmlog_fatal(msg, ...)	mmlog_log(MMLOG_FATAL, mm_location(MMLOG_MODULE_NAME), msg,  ## __VA_ARGS__)
#else
#  define mmlog_fatal(msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_ERROR
#  define mmlog_error(msg, ...)	mmlog_log(MMLOG_ERROR, mm_location(MMLOG_MODULE_NAME), msg,  ## __VA_ARGS__)
#else
#  define mmlog_error(msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_WARN
#  define mmlog_warn(msg, ...)	mmlog_log(MMLOG_WARN, mm_location(MMLOG_MODULE_NAME), msg,  ## __VA_ARGS__)
#else
#  define mmlog_warn(msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_INFO
#  define mmlog_info(msg, ...)	mmlog_log(MMLOG_INFO, mm_location(MMLOG_MODULE_NAME), msg,  ## __VA_ARGS__)
#else
#  define mmlog_info(msg, ...) MMLOG_VOID_CAST(0)
#endif

#if MMLOG_MAXLEVEL >= MMLOG_DEBUG
#  define mmlog_debug(msg, ...)	mmlog_log(MMLOG_DEBUG, mm_location(MMLOG_MODULE_NAME), msg,  ## __VA_ARGS__)
#else
#  define mmlog_debug(msg, ...) MMLOG_VOID_CAST(0)
#endif

/* Use mmlog_debug instead, provided only for backward compatibility */
#define mmlog_trace	mmlog_debug

#ifdef __cplusplus
extern "C" {
#endif

void mmlog_log(int lvl, const char* location, const char* msg, ...);


#ifdef __cplusplus
}
#endif

#endif /*MMLOG_H*/
