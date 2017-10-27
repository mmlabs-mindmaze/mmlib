# MM_OUTPUT_DEF()
# -------------
# Test if linker support flags to output def file. Define conditional
# CAN_OUTPUT_DEF if so
AC_DEFUN([MM_OUTPUT_DEF],
[
  AC_CACHE_CHECK([if compiler/linker supports -Wl,--output-def],
    [mm_cv_output_def],
    [if test "$enable_shared" = no; then
       mm_cv_output_def="not needed, shared libraries are disabled"
     else
       ldflags_save=$LDFLAGS
       LDFLAGS="-Wl,--output-def,conftest.def"
       AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
                   [mm_cv_output_def=yes],
                   [mm_cv_output_def=no])
       rm -f conftest.def
       LDFLAGS="$ldflags_save"
     fi])
  AM_CONDITIONAL([CAN_OUTPUT_DEF], test "x$mm_cv_output_def" = "xyes")
])
