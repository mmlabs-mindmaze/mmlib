
# MM_CHECK_LIB(FUNC, LIBRARIES, VARIABLE-PREFIX, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# --------------------------------------------------------------
AC_DEFUN([MM_CHECK_LIB],
[save_LIBS="$LIBS"
LIBS=""
AC_SEARCH_LIBS([$1], [$2], [AC_SUBST($3[]_LIB,"$LIBS")
                                             $4], [$5])
LIBS="$save_LIBS"])

