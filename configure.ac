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
m4_define([lib_revision],0)
m4_define([lib_age],1)

# Setup autoconf
AC_INIT(mmlib,m4_normalize(m4_include([VERSION])),[nicolas.bourdaud@gmail.com])
AC_CONFIG_SRCDIR([src/mmlog.h])
#AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config/config.h])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

# Setup automake
AM_INIT_AUTOMAKE([no-dist-gzip dist-xz color-tests subdir-objects parallel-tests foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# check for and set c11 standard
AX_CHECK_COMPILE_FLAG([-std=c11], [AX_APPEND_FLAG([-std=c11])])

# use POSIX definitions up to POSIX.1-2008 standard
AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Strive to comply to POSIX-2008])

# ensure that the "default" definitions are provided
# Note: when defined with _POSIX_C_SOURCE, POSIX sources are always preferred
# in case of conflict
AC_DEFINE([_DEFAULT_SOURCE], [1], [ensure that the default definitions are provided])

# for compatibility with old OS
AC_DEFINE([_BSD_SOURCE], [1], [ensure that the default definitions are provided])

# Check for programs
AM_PROG_CC_C_O
LT_INIT([win32-dll disable-static])
LT_SYS_MODULE_EXT
AC_SUBST(LIBTOOL_DEPS)
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.19)

# Optional build of 'check' based tests
PKG_CHECK_MODULES_EXT(CHECK, [check >= 0.9.12],
                      [has_check=yes; tap_in_check=yes],
                      [PKG_CHECK_MODULES_EXT(CHECK, [check], [has_check=yes], [has_check=no])])
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
AM_CONDITIONAL(TAP_SUPPORT_IN_CHECK, [test "x$tap_in_check" = xyes])

# Option to build win32 lockserver in mmlib dll (not as separated process)
AC_ARG_ENABLE([lockserverprocess], AC_HELP_STRING([--disable-lockserverprocess], [Do not run win32 lockserver in separated process. @<:@default=enabled@:>@]),
	    [], [enable_lockserverprocess=yes])
AM_CONDITIONAL([LOCKSERVER_IN_MMLIB_DLL], [test "x$enable_lockserverprocess" = xno])
AS_IF([test "x$enable_lockserverprocess" = xno],
      AC_DEFINE([LOCKSERVER_IN_MMLIB_DLL], [1], [Define to 1 if lockserver must be built in mmlib dll]))

# Check for libraries
AC_CHECK_FUNCS([posix_memalign aligned_alloc _aligned_malloc], [break])
AC_CHECK_FUNCS([copy_file_range])
MM_CHECK_LIB([pthread_create], [pthread], PTHREAD)
MM_CHECK_FUNCS([pthread_mutex_consistent], [], [], [$PTHREAD_LIB])
MM_CHECK_LIB([clock_gettime], [rt], CLOCK)
MM_CHECK_LIB([localtime64_s], [], LOCALTIME_S, [AC_DEFINE([HAS_LOCALTIME_S], [1], [Define if localtime_s is present])])
MM_CHECK_LIB([shm_open], [rt], SHM)
MM_CHECK_LIB([dlopen], [dl], DL, [AC_DEFINE([HAVE_DLOPEN], [1], [define if dlopen() is available])])

AC_CHECK_HEADERS([alloca.h linux/fs.h])

AC_DEF_API_EXPORT_ATTRS
AC_SET_HOSTSYSTEM
MM_CC_WARNFLAGS

# Test for sphinx doc with linuxdoc
AC_ARG_ENABLE([sphinxdoc], AC_HELP_STRING([--enable-sphinxdoc], [Build sphinx documention. @<:@default=detect@:>@]),
	    [], [enable_sphinxdoc=detect])
AM_PATH_PYTHON([3])
MM_PYTHON_MODULE([sphinx])
MM_PYTHON_MODULE([sphinx_rtd_theme])
MM_PYTHON_MODULE([linuxdoc])

AC_CHECK_PROG(HAVE_SPHINXBUILD, [sphinx-build], [$HAVE_PYMOD_LINUXDOC], [no])
AS_CASE([$enable_sphinxdoc],
        [yes], [AS_IF([test "x$HAVE_SPHINXBUILD" != xyes],
                      [AC_MSG_ERROR(Cannot find sphinx or its plugin linuxdoc)])],
        [detect], [AS_IF([test "x$HAVE_SPHINXBUILD" = xyes],
                         [enable_sphinxdoc=yes], [enable_sphinxdoc=no])])
AM_CONDITIONAL(BUILD_SPHINXDOC, [test "x$enable_sphinxdoc" = xyes])


AC_SUBST([optional_examples])
AC_SUBST([CURRENT],[lib_current])
AC_SUBST([REVISION],[lib_revision])
AC_SUBST([AGE],[lib_age])
AS_VAR_ARITH([lib_abi], [lib_current - lib_age])
AC_SUBST([ABI_VER], [$lib_abi])

# Add ifndef in config.h to fix pthread-win32 header mess
AH_TOP([#ifndef CONFIG_H
#define CONFIG_H])
AH_BOTTOM([#endif /*CONFIG_H*/])

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile
                 tests/Makefile tests/handmaid/Makefile po/Makefile.in])
AC_OUTPUT



