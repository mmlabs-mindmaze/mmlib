eol=

AM_CPPFLAGS = -I$(top_srcdir)/src $(CHECK_CPPFLAGS)
AM_CFLAGS = $(CHECK_CFLAGS) $(MM_WARNFLAGS)
MMLIB = $(top_builddir)/src/libmmlib.la

check_PROGRAMS = \
	testlog \
	testerrno \
	testgeometry \
	testtype \
	testprofile \
	testinternals \
	$(eol)

TESTS = $(check_PROGRAMS)

testlog_SOURCES = testlog.c
testlog_LDADD = $(MMLIB)

testerrno_SOURCES = testerrno.c
testerrno_LDADD = $(MMLIB)

testgeometry_SOURCES = testgeometry.c
testgeometry_LDADD = $(CHECK_LIBS) $(MMLIB)

testtype_SOURCES = testtype.c
testtype_LDADD = $(CHECK_LIBS) $(MMLIB)

testprofile_SOURCES = testprofile.c
testprofile_LDADD = $(MMLIB)

testinternals_SOURCES = testinternals.c
testinternals_LDADD = $(CHECK_LIBS)
