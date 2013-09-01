EXTRA_DIST = \
	tokens/268B634DA2374A43F3E3C177B0AC5D21C2072127.sig \
	tokens/4159918692C24F700C08C6CF7E374C28E074F5A8.sig \
	tokens/58AFC58478D6349E7408956A768A8D9F0C77CB37.sig \
	tokens/EBBC47D1D49D3A4F3405DD72766A29515F93912F.sig

LDADD = $(top_builddir)/src/libmmlib.la
AM_CPPFLAGS = -I$(top_srcdir)/src $(CHECK_CPPFLAGS)
AM_CFLAGS = $(CHECK_CFLAGS)

testgeometry_LDADD = $(CHECK_LIBS) $(top_builddir)/src/libmmlib.la
testtype_LDADD = $(CHECK_LIBS) $(top_builddir)/src/libmmlib.la

check_PROGRAMS = testlog testerrno testgeometry testtype
TESTS = testlog testerrno testgeometry testtype
TESTS_ENVIRONMENT = MM_LIC_ALTDIR=$(srcdir)/tokens
