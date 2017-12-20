eol=

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(CHECK_CPPFLAGS) \
	$(eol)
AM_CFLAGS = $(CHECK_CFLAGS) $(MM_WARNFLAGS)
MMLIB = $(top_builddir)/src/libmmlib.la

check_PROGRAMS = \
	testlog \
	testerrno \
	testprofile \
	$(eol)

if BUILD_CHECK_TESTS
check_PROGRAMS += \
	testgeometry \
	testtype \
	testinternals \
	testapi \
	$(eol)
endif

TESTS = $(check_PROGRAMS)

testlog_SOURCES = testlog.c
testlog_LDADD = $(MMLIB)

testerrno_SOURCES = testerrno.c
testerrno_LDADD = $(MMLIB)

testprofile_SOURCES = testprofile.c
testprofile_LDADD = $(MMLIB)

testgeometry_SOURCES = testgeometry.c
testgeometry_LDADD = $(CHECK_LIBS) $(MMLIB)

testtype_SOURCES = testtype.c
testtype_LDADD = $(CHECK_LIBS) $(MMLIB)

testinternals_SOURCES = testinternals.c
testinternals_LDADD = $(CHECK_LIBS)

testapi_SOURCES = \
	testapi.c \
	api-testcases.h \
	$(eol)

testapi_LDADD = \
	$(CHECK_LIBS) \
	$(MMLIB) \
	$(eol)

if OS_TYPE_WIN32
testinternals_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DMMLIB_API=API_EXPORTED \
	$(eol)
endif
