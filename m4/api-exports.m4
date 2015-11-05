# NOTE: If in future we need to support other compiler that don't understand
# visibility attribute (might be the case on non POSIX embedded platform),
# we will need to add the test for visibility attribute. In such a case, see
# visibilility.m4 from gnulib to see how this could be implemented.

AC_DEFUN([AC_DEF_API_EXPORT_ATTRS],
[AC_REQUIRE([AC_CANONICAL_HOST])
case $host in
	*win32* | *mingw32* | *cygwin* | *windows*)
		os_support=win32
		;;
	*)
		os_support=other
		;;
esac
if test $os_support !=  "win32"; then
     AC_DEFINE(LOCAL_SYMBOL, [__attribute__ ((visibility ("hidden")))],
     	[attribute of the non-exported symbols])
     AC_DEFINE(API_EXPORTED, [__attribute__ ((visibility ("protected")))],
     	[attribute of the symbols exported in the API])
     AC_DEFINE(API_EXPORTED_RELOCATABLE, [__attribute__ ((visibility ("default")))],
        [attribute of the relocatable symbols exported in the API])
else
     AC_DEFINE(LOCAL_SYMBOL, [], [attribute of the non-exported symbols])
     AC_DEFINE(API_EXPORTED, [__declspec(dllexport)], [attribute of the symbols exported in the API])
     AC_DEFINE(API_EXPORTED_RELOCATABLE, [__declspec(dllexport)], [attribute of the relocatable symbols exported in the API])
fi
])

