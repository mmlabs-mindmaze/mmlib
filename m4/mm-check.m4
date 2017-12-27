
# MM_CHECK_LIB(FUNC, LIBRARIES, VARIABLE-PREFIX, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# --------------------------------------------------------------
AC_DEFUN([MM_CHECK_LIB],
[save_LIBS="$LIBS"
LIBS=""
AC_SEARCH_LIBS([$1], [$2], [AC_SUBST($3[]_LIB,"$LIBS")
                                             $4], [$5])
LIBS="$save_LIBS"])


# MM_CHECK_FUNC(FUNC, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND], [OTHER_LIBS])
# --------------------------------------------------------------
AC_DEFUN([MM_CHECK_FUNC],
[save_LIBS="$LIBS"
LIBS="$LIBS $4"
AC_CHECK_FUNC([$1], [$2], [$3])
LIBS="$save_LIBS"])


# MM_CHECK_FUNCS(FUNCS, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND], [OTHER_LIBS])
# --------------------------------------------------------------
AC_DEFUN([MM_CHECK_FUNCS],
[save_LIBS="$LIBS"
LIBS="$LIBS $4"
AC_CHECK_FUNCS([$1], [$2], [$3])
LIBS="$save_LIBS"])
