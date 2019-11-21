MMLIB = $(top_builddir)/src/libmmlib.la

# shared library needed for the tests of rename and unlink
lib_LTLIBRARIES = libhandmaid.la
libhandmaid_la_SOURCES = \
	lib_handmaid.c \
	lib_handmaid.h \
	$(eol)
libhandmaid_la_LDFLAGS = \
	-version-info 1:0:0 \
	-no-undefined \
	$(AM_LDFLAGS) \
	$(eol)

# executable needed for the tests of rename and unlink
handmaid_SOURCES = handmaid.c
handmaid_LDADD = \
	libhandmaid.la \
	$(MMLIB) \
	$(eol)
bin_PROGRAMS = handmaid
