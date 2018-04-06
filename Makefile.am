ACLOCAL_AMFLAGS = -I m4 --install
SUBDIRS = po src doc tests
EXTRA_DIST = build-aux/config.rpath  autogen.sh tools/coverage.sh

test-coverage:
	$(srcdir)/tools/coverage.sh run

clean-local:
	$(srcdir)/tools/coverage.sh clean
