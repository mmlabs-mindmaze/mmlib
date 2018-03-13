eol=


AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
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

check_PROGRAMS = \
	testlog \
	testerrno \
	testprofile \
	$(eol)

if BUILD_CHECK_TESTS
check_PROGRAMS += \
	testinternals \
	testapi.tap \
	$(eol)
endif

TESTS = $(check_PROGRAMS)

testlog_SOURCES = testlog.c
testlog_LDADD = $(MMLIB)

testerrno_SOURCES = testerrno.c
testerrno_LDADD = $(MMLIB)

testprofile_SOURCES = testprofile.c
testprofile_LDADD = $(MMLIB)

testinternals_SOURCES = testinternals.c
testinternals_LDADD = $(CHECK_LIBS)

testapi_tap_SOURCES = \
	testapi.c \
	api-testcases.h \
	type-api-tests.c \
	geometry-api-tests.c \
	alloc-api-tests.c \
	$(eol)

testapi_tap_LDADD = \
	$(CHECK_LIBS) \
	$(MMLIB) \
	$(eol)

if OS_TYPE_WIN32
testinternals_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DMMLIB_API=API_EXPORTED \
	$(eol)
endif
