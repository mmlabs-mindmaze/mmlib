
AC_DEFUN([AC_SET_HOSTSYSTEM],
[AC_REQUIRE([AC_CANONICAL_HOST])
case $host in
	*win32* | *mingw* | *windows*)
		os_system=win32
		;;
	*)
		os_system=posix
		;;
esac
AM_CONDITIONAL([OS_TYPE_POSIX], [test "$os_system" = posix])
AM_CONDITIONAL([OS_TYPE_WIN32], [test "$os_system" = win32])
])
