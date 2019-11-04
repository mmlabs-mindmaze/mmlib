eol=

clean-local:
	$(RM) *.gcno *.gcda

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-DLOCALEDIR=\"$(localedir)\" \
	-DBUILDDIR=\"$(abs_builddir)\" \
	-DEXEEXT=\"$(EXEEXT)\" \
	-DTOP_BUILDDIR=\"$(abs_top_builddir)\" \
	-DSRCDIR=\"$(abs_srcdir)\" \
	$(CHECK_CPPFLAGS) \
	$(eol)
AM_CFLAGS = $(CHECK_CFLAGS) $(MM_WARNFLAGS)
MMLIB = $(top_builddir)/src/libmmlib.la

TEST_EXTENSIONS = .test .tap

#if check support TAP output, make use of it
if TAP_SUPPORT_IN_CHECK
AM_CPPFLAGS += -DCHECK_SUPPORT_TAP
TAP_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                 $(top_srcdir)/build-aux/tap-driver.sh
else
TAP_LOG_DRIVER = $(LOG_DRIVER)
endif #TAP_SUPPORT_IN_CHECK

TESTS = \
	testlog \
	testerrno \
	testprofile \
	$(eol)

if BUILD_CHECK_TESTS
TESTS += \
	testinternals.tap \
	testapi.tap \
	$(eol)
endif

check_LTLIBRARIES = dynlib-test.la

check_PROGRAMS = \
	$(TESTS) \
	child-proc \
	perflock \
	tests-child-proc \
	$(eol)

testlog_SOURCES = testlog.c
testlog_LDADD = $(MMLIB)

testerrno_SOURCES = testerrno.c
testerrno_LDADD = $(MMLIB)

testprofile_SOURCES = testprofile.c
testprofile_LDADD = $(MMLIB)

child_proc_SOURCES = child-proc.c
child_proc_LDADD = $(MMLIB)

perflock_SOURCES = perflock.c
perflock_LDADD = $(MMLIB)

dynlib_test_la_SOURCES = \
	dynlib-api.h \
	dynlib-test.c \
	$(eol)
dynlib_test_la_LDFLAGS= \
	-module \
	-avoid-version \
	-no-undefined \
	-rpath /nowhere \
	$(eol)

testinternals_tap_SOURCES = \
	internals-testcases.h \
	testinternals.c \
	log-internals.c \
	$(eol)
testinternals_tap_LDADD = \
	$(CHECK_LIBS) \
	@LTLIBINTL@ \
	../src/libmmlib-internal-wrapper.la \
	$(eol)
testinternals_tap_CPPFLAGS = $(AM_CPPFLAGS)

if OS_TYPE_WIN32

testinternals_tap_SOURCES += \
	startup-win32-internals.c \
	$(eol)

testinternals_tap_CPPFLAGS += \
	-DMMLIB_API=API_EXPORTED \
	-DWIN32_LEAN_AND_MEAN \
	-D_WIN32_WINNT=_WIN32_WINNT_WIN8 \
	$(eol)

testinternals_tap_LDFLAGS= \
	-Wl,--allow-multiple-definition \
	$(eol)

endif

testapi_tap_SOURCES = \
	testapi.c \
	api-testcases.h \
	tests-child-proc.h \
	tests-run-func.c \
	type-api-tests.c \
	geometry-api-tests.c \
	alloc-api-tests.c \
	time-api-tests.c \
	thread-api-tests.c \
	threaddata-manipulation.h \
	threaddata-manipulation.c \
	file-api-tests.c \
	socket-api-tests.c \
	socket-testlib.h \
	socket-testlib.c \
	ipc-api-tests.c \
	ipc-api-tests-exported.c \
	ipc-api-tests-exported.h \
	shm-api-tests.c \
	dirtests.c \
	dlfcn-api-tests.c \
	process-api-tests.c \
	process-testlib.c \
	process-testlib.h \
	argparse-api-tests.c \
	utils-api-tests.c \
	$(eol)

testapi_tap_LDADD = \
	$(CHECK_LIBS) \
	$(MMLIB) \
	$(eol)

tests_child_proc_SOURCES = \
	tests-child-proc.c \
	tests-child-proc.h \
	process-testlib.c \
	process-testlib.h \
	threaddata-manipulation.h \
	threaddata-manipulation.c \
	socket-testlib.h \
	socket-testlib.c \
	ipc-api-tests-exported.c \
	ipc-api-tests-exported.h \
	$(eol)
tests_child_proc_LDADD = $(MMLIB)
tests_child_proc_LDFLAGS = \
	-export-dynamic \
	$(eol)

tests_child_proc_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DMMLOG_MODULE_NAME=\"tests_child_proc\" \
	$(eol)

