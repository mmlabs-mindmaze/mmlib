AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" \
              -DSYSCONFDIR=\"$(sysconfdir)\" \
              $(GNUTLS_CPPFLAGS)

include_HEADERS = mmlog.h mmerrno.h mmtype.h mmgeometry.h mmskeleton.h
lib_LTLIBRARIES = libmmlib.la
noinst_LTLIBRARIES = liblicense.la
bin_PROGRAMS = mmlicense

BUILT_SOURCES = ca-file.inl
CLEANFILES = ca-file.inl
ca_file = /etc/ssl/certs/Mindmaze_CA.pem

ca-file.inl: $(ca_file)
	$(AM_V_GEN)certtool -i --outder < $^ \
	| od -t x1 -An \
	| sed -r 's/([[:alnum:]]+)/0x\1,/g' > $@

liblicense_la_SOURCES = license.c license.h
liblicense_la_LIBADD = $(GNUTLS_LIBS)

libmmlib_la_SOURCES = mmlog.h log.c \
                      nls-internals.h    \
		      mmerrno.h error.c  \
		      mmtype.h type.c    \
		      license-check.c license.h \
		      mmskeleton.h skeleton.c	\
		      mmgeometry.h geometry.c   \
		      mmprofile.h profile.c
libmmlib_la_LIBADD = $(PTHREAD_LIB) $(MATH_LIB) $(CLOCK_LIB) \
                     liblicense.la
libmmlib_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined \
                     -version-info $(CURRENT):$(REVISION):$(AGE) 


mmlicense_SOURCES = mmlicense.c
mmlicense_LDADD = $(GNUTLS_LIBS) liblicense.la


