eol=

dist_man_MANS = mmlog_log.3	\
                mmlog_fatal.3	\
                mmlog_error.3	\
                mmlog_warn.3	\
                mmlog_info.3	\
                mmlog_debug.3	\
                mmtic.3 mmtoc.3 mmtoc_label.3 \
                mmprofile_print.3 \
                mmprofile_reset.3

LDADD = $(top_builddir)/src/libmmlib.la
AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(eol)
AM_CFLAGS = $(CHECK_CFLAGS) $(MM_WARNFLAGS)

check_PROGRAMS = \
	multithread \
	pshared-parent \
	pshared-child \
	parse_args \
	$(eol)

multithread_SOURCES = \
	examples/multithread.c \
	$(eol)

pshared_parent_SOURCES = \
	examples/pshared-common.h \
	examples/pshared-parent.c \
	$(eol)

pshared_child_SOURCES = \
	examples/pshared-child.c \
	$(eol)

parse_args_SOURCES = \
	examples/parse_args.c \
	$(eol)

nobase_dist_doc_DATA = \
	$(multithread_SOURCES) \
	$(pshared_parent_SOURCES) \
	$(pshared_child_SOURCES) \
	$(parse_args_SOURCES) \
	$(eol)
