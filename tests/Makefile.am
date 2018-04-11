eol=

clean-local:
	$(RM) *.gcno *.gcda

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-DBUILDDIR=\"$(abs_builddir)\" \
	-DEXEEXT=\"$(EXEEXT)\" \
	-DTOP_BUILDDIR=\"$(abs_top_builddir)\" \
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
	testfile \
	testprocess \
	testdlfcn \
	$(eol)

if BUILD_CHECK_TESTS
TESTS += \
	testinternals \
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

testfile_SOURCES = testfile.c
testfile_LDADD = $(MMLIB)

testprocess_SOURCES = testprocess.c
testprocess_LDADD = $(MMLIB)

child_proc_SOURCES = child-proc.c
child_proc_LDADD = $(MMLIB)

perflock_SOURCES = perflock.c
perflock_LDADD = $(MMLIB)

testdlfcn_SOURCES = \
	dynlib-api.h \
	testdlfcn.c \
	$(eol)
testdlfcn_LDADD = $(MMLIB)

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

testinternals_SOURCES = testinternals.c
testinternals_LDADD = $(CHECK_LIBS)

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
	socket-api-tests.c \
	socket-testlib.h \
	socket-testlib.c \
	ipc-api-tests.c \
	ipc-api-tests-exported.c \
	ipc-api-tests-exported.h \
	shm-api-tests.c \
	dirtests.c \
	$(eol)

testapi_tap_LDADD = \
	$(CHECK_LIBS) \
	$(MMLIB) \
	$(eol)

tests_child_proc_SOURCES = \
	tests-child-proc.c \
	tests-child-proc.h \
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

if OS_TYPE_WIN32
testinternals_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DMMLIB_API=API_EXPORTED \
	$(eol)
endif
