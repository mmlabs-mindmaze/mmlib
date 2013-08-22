
LDADD = $(top_builddir)/src/libmmlib.la
AM_CPPFLAGS = -I$(top_srcdir)/src $(CHECK_CPPFLAGS)
AM_CFLAGS = $(CHECK_CFLAGS)

testgeometry_LDADD = $(CHECK_LIBS) $(top_builddir)/src/libmmlib.la
testtype_LDADD = $(CHECK_LIBS) $(top_builddir)/src/libmmlib.la

check_PROGRAMS = testlog testerrno testgeometry testtype
TESTS = testlog testerrno testgeometry testtype
