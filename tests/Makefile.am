
LDADD = $(top_builddir)/src/libmmlib.la
AM_CPPFLAGS = -I$(top_srcdir)/src $(CHECK_CPPFLAGS)
AM_CFLAGS = $(CHECK_CFLAGS)

testgeometry_LDADD = $(CHECK_LIBS) $(top_builddir)/src/libmmlib.la

check_PROGRAMS = testlog testerrno testgeometry
TESTS = testlog testerrno testgeometry
