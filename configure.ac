# @mindmaze_header@

# - If the library source code has changed at all since the last update,
# then increment revision.
# - If any interfaces have been added, removed, or changed since the last
# update, increment current, and set revision to 0.
# - If any interfaces have been added since the last public release, then
# increment age.
# - If any interfaces have been removed since the last public release, then
# set age to 0.

m4_define([lib_current],3)
m4_define([lib_revision],4)
m4_define([lib_age],3)

# Setup autoconf
AC_INIT(mmlib,[lib_current.lib_revision],[nicolas.bourdaud@mindmaze.ch])
AC_CONFIG_SRCDIR([src/mmlog.h])
#AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config/config.h])

# Setup automake
AM_INIT_AUTOMAKE([no-dist-gzip dist-xz color-tests subdir-objects parallel-tests])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Check for programs
AC_PROG_CC_C99
AM_PROG_CC_C_O
LT_INIT([win32-dll disable-static])
LT_SYS_MODULE_EXT
AC_SUBST(LIBTOOL_DEPS)
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.19)

# Optional build of 'check' based tests
PKG_CHECK_MODULES_EXT(CHECK, [check], [has_check=yes], [has_check=no])
AC_ARG_WITH([check], AC_HELP_STRING([--with-check], [Build and run check based tests. @<:@default=detect@:>@]),
	    [], [with_check=detect])
AS_CASE([$with_check],
        [yes], [AS_IF([test "$has_check" != no],
	              [build_check_tests=yes],
		      [AC_MSG_FAILURE([check required but not found])])
	       ],
        [no], [build_check_tests=no],
        [detect], [build_check_tests=$has_check],
        [AC_MSG_FAILURE([invalid argument for '--with-check' option])])
AM_CONDITIONAL(BUILD_CHECK_TESTS, [test "$build_check_tests" = yes])

# Check for libraries
MM_CHECK_LIB([pthread_create], [pthread], PTHREAD)
MM_CHECK_LIB([sqrtf], [m], MATH)
MM_CHECK_LIB([clock_gettime], [rt], CLOCK)
MM_CHECK_LIB([localtime64_s], [], LOCALTIME_S, [AC_DEFINE([HAS_LOCALTIME_S], [1], [Define if localtime_s is present])])

AC_DEF_API_EXPORT_ATTRS
AC_SET_HOSTSYSTEM
MM_CC_WARNFLAGS

AC_SUBST([optional_examples])
AC_SUBST([CURRENT],[lib_current])
AC_SUBST([REVISION],[lib_revision])
AC_SUBST([AGE],[lib_age])

# Add ifndef in config.h to fix pthread-win32 header mess
AH_TOP([#ifndef CONFIG_H
#define CONFIG_H])
AH_BOTTOM([#endif /*CONFIG_H*/])

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile
                 tests/Makefile po/Makefile.in]) 
AC_OUTPUT



