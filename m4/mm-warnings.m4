
# MM_CC_WARNFLAGS()
#-------------------------------------------
AC_DEFUN([MM_CC_WARNFLAGS],
[AC_ARG_ENABLE([warn-all],
              [AS_HELP_STRING([--enable-warn-all], [turn on all warnings (default: yes)])],
	      [case $enableval in
                 yes|no|error) ;;
                 *) AC_MSG_ERROR([bad value $enableval for enable-warn-all option]) ;;
               esac
               mm_warnings=$enableval], [mm_warnings=yes])

case $mm_warnings in
	yes) MM_WARNFLAGS="-Wall -Wextra" ;;
	error) MM_WARNFLAGS="-Wall -Wextra -Werror" ;;
	no) MM_WARNFLAGS="" ;;
esac

AC_SUBST([MM_WARNFLAGS])
]) #MM_CC_WARNINGS
