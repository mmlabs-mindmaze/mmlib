eol=

AUTOMAKE_OPTIONS = -Wno-portability

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

OVERRIDING_MANPAGES = \
	mmlog_debug.3 \
	mmlog_fatal.3 \
	mmlog_warn.3 \
	mmtoc.3 \
	mmlog_error.3 \
	mmlog_info.3 \
	mmtic.3 \
	mmtoc_label.3 \
	$(eol)

DOC_SRCS = \
	alloc.rst \
	argparse.rst \
	design.rst \
	dlfcn.rst \
	env.rst \
	error.rst \
	filesystem.rst \
	index.rst \
	ipc.rst \
	log.rst \
	process.rst \
	profiling.rst \
	socket.rst \
	thread.rst \
	time.rst \
	type.rst \
	$(eol)

EXTRA_DIST = \
	$(OVERRIDING_MANPAGES) \
	$(DOC_SRCS) \
	$(eol)

export srctree=$(SRCTREE)

#
# Sphinx documentation build targets
# Skipped if --enable-sphinxdoc=no
#
if BUILD_SPHINXDOC
SPHINXBUILD   = sphinx-build
SRCTREE       = $(top_srcdir)

sphinx_verbose = $(sphinx_verbose_@AM_V@)
sphinx_verbose_ = $(sphinx_verbose_@AM_DEFAULT_V@)
sphinx_verbose_0 = -q

ALLSPHINXOPTS = -d $(builddir)/.doctrees $(sphinx_verbose) $(srcdir)

# Due to a dependency bug in sphinx-build, we use the -M option instead of
# the usual -b one. This hidden option sums up to the same as -b with makefile
# dependency support ... except that it does not support -W option which turns
# warnings into errors !
# => do this with a simple grep line on the output
# Since html target depends on man target, this is only needed once
sphinx-build.log: $(wildcard $(SRCTREE)/src/*.[ch]) $(DOC_SRCS)
	@$(RM) -f manpages-buildstamp
	$(AM_V_GEN) $(SPHINXBUILD) -M kernel-doc-man $(ALLSPHINXOPTS) $(builddir)/man &> sphinx-build.log

# sphinx-build.log target does the doc generation
# This target checks that it was done right
manpages-buildstamp: sphinx-build.log
	@if [ "$(shell grep -c WARNING: sphinx-build.log)" != "0" ] ; then \
		grep WARNING: sphinx-build.log; \
		false; \
	fi
	@echo
	@echo "Build finished. The man pages are in $(builddir)/man."
	@touch $@

.PHONY: man-pages
man-pages: manpages-buildstamp
html-local: html-buildstamp

# always generate html after man: parallel sphinx invocation is not upported
html-buildstamp: $(wildcard $(SRCTREE)/src/*.[ch]) $(DOC_SRCS) manpages-buildstamp
	@$(RM) -f $@
	$(AM_V_GEN) $(SPHINXBUILD) -M html $(ALLSPHINXOPTS) $(builddir)/html
	@echo
	@echo "Build finished. The HTML pages are in $(builddir)/html."
	@touch $@

install-html: html-buildstamp
	@mkdir -p $(DESTDIR)$(docdir)/html/
	cd $(builddir)/html && \
		find . -type f -exec $(install_sh_DATA) '{}' $(DESTDIR)$(docdir)/html/'{}' \;

uninstall-html:
	$(RM) -r $(DESTDIR)$(docdir)/html/

install-man3: manpages-buildstamp
	@mkdir -p $(DESTDIR)$(mandir)/man3
	$(foreach manpage, $(notdir $(wildcard $(builddir)/man/*.3)), \
		$(install_sh_DATA) $(builddir)/man/$(manpage) $(DESTDIR)$(mandir)/man3/$(manpage);)
	$(foreach manpage, $(OVERRIDING_MANPAGES), \
		$(install_sh_DATA) $(srcdir)/$(manpage) $(DESTDIR)$(mandir)/man3/$(manpage);)

uninstall-man3: manpages-buildstamp
	$(foreach manpage, $(notdir $(wildcard $(builddir)/man/*.3)), \
		$(RM) $(DESTDIR)$(mandir)/man3/$(manpage);)
	$(foreach manpage, $(OVERRIDING_MANPAGES), \
		$(RM) $(DESTDIR)$(mandir)/man3/$(manpage);)
	@-rmdir $(mandir)/man3

all-local: manpages-buildstamp html-buildstamp
install-data-local: install-man3 install-html
uninstall-local: uninstall-man3 uninstall-html

endif # BUILD_SPHINXDOC

clean-local:
	$(RM) -rf html .doctrees man manpages-buildstamp html-buildstamp sphinx-build.log
