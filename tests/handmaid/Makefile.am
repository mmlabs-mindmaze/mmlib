MMLIB = $(top_builddir)/src/libmmlib.la

# shared library needed for the tests of rename and unlink
check_LTLIBRARIES = libhandmaid.la
libhandmaid_la_SOURCES = \
	lib_handmaid.c \
	lib_handmaid.h \
	$(eol)

# -rpath /nowhere permits to create the .so
libhandmaid_la_LDFLAGS = \
	-version-info 1:0:0 \
	-rpath /nowhere \
	-no-undefined \
	$(AM_LDFLAGS) \
	$(eol)

# executable needed for the tests of rename and unlink
handmaid_SOURCES = handmaid.c
handmaid_LDADD = \
	libhandmaid.la \
	$(MMLIB) \
	$(eol)
check_PROGRAMS = handmaid
