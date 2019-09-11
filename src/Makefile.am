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
	mmprofile.h \
	mmpredefs.h \
	mmlib.h \
	mmtime.h \
	mmsysio.h \
	mmthread.h \
	mmdlfcn.h \
	mmargparse.h \
	$(eol)

lib_LTLIBRARIES = libmmlib.la
pkglibexec_PROGRAMS =

libmmlib_la_SOURCES = \
	mmlog.h log.c \
	nls-internals.h    \
	mmerrno.h error.c \
	mmtype.h type.c    \
	mmprofile.h profile.c \
	mmlib.h \
	alloc.c \
	utils.c \
	mmargparse.h argparse.c \
	mmtime.h time.c \
	mmsysio.h \
	file.c file-internal.h \
	socket-internal.h \
	socket.c \
	mmthread.h \
	mmdlfcn.h dlfcn.c \
	$(eol)

libmmlib_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DLIBEXECDIR='"$(pkglibexecdir)"' \
	$(eol)

libmmlib_la_LIBADD = \
	@LTLIBINTL@ \
	$(MATH_LIB) \
	$(DL_LIB) \
	$(eol)

libmmlib_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-no-undefined \
        -version-info $(CURRENT):$(REVISION):$(AGE) \
	$(eol)


if OS_TYPE_POSIX

libmmlib_la_SOURCES += \
	time-posix.c \
	file-posix.c \
	shm-posix.c \
	process-posix.c \
	local-ipc-posix.c \
	thread-posix.c \
	socket-posix.c \
	$(eol)

libmmlib_la_LIBADD += \
	$(CLOCK_LIB) \
	$(SHM_LIB) \
	$(PTHREAD_LIB) \
	$(eol)

endif

if OS_TYPE_WIN32

AM_CPPFLAGS += \
	-DWIN32_LEAN_AND_MEAN \
	-D_WIN32_WINNT=_WIN32_WINNT_WIN8 \
	$(eol)

libmmlib_la_SOURCES += \
	time-win32.c \
	clock-win32.h clock-win32.c \
	utils-win32.h utils-win32.c \
	file-win32.c \
	shm-win32.c \
	process-win32.c \
	local-ipc-win32.c \
	thread-win32.c \
	atomic-win32.h \
	mutex-lockval.h \
	lock-referee-proto.h \
	pshared-lock.h pshared-lock.c \
	socket-win32.c \
	env-win32.c \
	startup-win32.c \
	$(eol)

libmmlib_la_CPPFLAGS += \
	-DMMLIB_API=API_EXPORTED \
	-D__USE_MINGW_ANSI_STDIO=1 \
	$(eol)

libmmlib_la_LIBADD += \
	-lpowrprof \
	-lws2_32 \
	$(eol)

if LOCKSERVER_IN_MMLIB_DLL

libmmlib_la_SOURCES +=  \
	lock-referee-server.c \
	$(eol)

else #!LOCKSERVER_IN_MMLIB_DLL

pkglibexec_PROGRAMS += lock-referee
lock_referee_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DMMLOG_MODULE_NAME=\"lock-referee\" \
	$(eol)

lock_referee_SOURCES = \
	lock-referee-proto.h \
	lock-referee-server.c \
	clock-win32.h clock-win32.c \
	$(eol)

lock_referee_LDADD = \
	-lpowrprof \
	-ladvapi32 \
	$(eol)

endif #!LOCKSERVER_IN_MMLIB_DLL


endif #OS_TYPE_WIN32

# check all the source files for correct coding style
.PHONY: checkstyle
checkstyle: ../tools/uncrustify.cfg
	cd $(srcdir) && uncrustify -c ../tools/uncrustify.cfg -l C --check \
		$(libmmlib_la_SOURCES)

.PHONY: fixstyle
fixstyle: ../tools/uncrustify.cfg
	cd $(srcdir) && uncrustify -c ../tools/uncrustify.cfg -l C --replace \
		$(libmmlib_la_SOURCES)

.PHONY: spelling
spelling: $(libmmlib_la_SOURCES)
	$(AM_V_at) codespell $^
