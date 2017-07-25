eol=

AM_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	$(eol)

AM_CFLAGS = $(MM_WARNFLAGS)

include_HEADERS = \
	mmlog.h \
	mmerrno.h \
	mmtype.h \
	mmgeometry.h \
	mmskeleton.h \
	mmprofile.h \
	mmpredefs.h \
	$(eol)

lib_LTLIBRARIES = libmmlib.la

libmmlib_la_SOURCES = \
	mmlog.h log.c \
	nls-internals.h    \
	mmerrno.h error.c  \
	mmtype.h type.c    \
	mmskeleton.h skeleton.c	\
	mmgeometry.h geometry.c   \
	mmprofile.h profile.c \
	$(eol)

libmmlib_la_LIBADD = \
	$(PTHREAD_LIB) \
	$(MATH_LIB) \
	$(CLOCK_LIB) \
	$(eol)

libmmlib_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-no-undefined \
        -version-info $(CURRENT):$(REVISION):$(AGE) \
	$(eol)
