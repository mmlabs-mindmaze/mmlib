AM_CPPFLAGS=-DLOCALEDIR=\"$(localedir)\"

include_HEADERS = mmlog.h mmerrno.h mmtype.h mmgeometry.h
lib_LTLIBRARIES = libmmlib.la

libmmlib_la_SOURCES = mmlog.h log.c \
                      nls-internals.h    \
		      mmerrno.h error.c  \
		      mmtype.h           \
		      mmgeometry.h geometry.c
libmmlib_la_LIBADD = $(PTHREAD_LIB) $(MATH_LIB)
