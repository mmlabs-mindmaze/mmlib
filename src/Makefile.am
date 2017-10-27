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
	$(MATH_LIB) \
	$(CLOCK_LIB) \
	$(eol)

libmmlib_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-no-undefined \
        -version-info $(CURRENT):$(REVISION):$(AGE) \
	$(eol)


if OS_TYPE_POSIX

libmmlib_la_SOURCES += \
	$(eol)

libmmlib_la_LIBADD += \
	$(eol)

endif

if OS_TYPE_WIN32

defexecdir = $(bindir)
defexec_DATA =
CLEANFILES = $(defexec_DATA)

# Generate def file if possible
if CAN_OUTPUT_DEF
defexec_DATA += libmmlib-@ABI_VER@.def
libmmlib_la_LDFLAGS += -Wl,--output-def,libmmlib-@ABI_VER@.def

$(defexec_DATA): libmmlib.la

endif

libmmlib_la_SOURCES += \
	$(eol)

libmmlib_la_LIBADD += \
	$(eol)

endif
