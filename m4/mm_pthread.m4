
# MM_CHECK_PTHREAD([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# --------------------------------------------------------------
AC_DEFUN([MM_CHECK_PTHREAD],
[save_LIBS="$LIBS"
LIBS=""
AC_SEARCH_LIBS([pthread_create], [pthread], [AC_SUBST(PTHREAD_LIB,"$LIBS")
                                             $1], [$2])
LIBS="$save_LIBS"])

