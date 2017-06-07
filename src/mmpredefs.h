/*
   @mindmaze_header@
*/
#ifndef MM_PREDEFS_H
#define MM_PREDEFS_H


/*
 Double expansion is the usual trick to expand a preprocessor macro argument
 into a string
*/
#define MM_XSTRINGIFY(arg)	#arg
#define MM_STRINGIFY(arg)	MM_XSTRINGIFY(arg)

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

#endif
