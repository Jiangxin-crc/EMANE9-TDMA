lib_LTLIBRARIES = libtdmamaclayer.la

libtdmamaclayer_la_CPPFLAGS=   \
 -I@top_srcdir@/include          \
 $(AM_CFLAGS)                    \
 $(libuuid_CFLAGS)               \
 $(libxml2_CFLAGS)

libtdmamaclayer_la_SOURCES =   	\
 pcrmanager.cc                  \
 tdmamacheadermessage.cc       	\
 tdmamacheader.pb.cc		\
 maclayer.cc			\
 downstreamqueue.cc		\
 fragmentmgr.cc			\
 downstreammgr.cc

EXTRA_DIST=                     \
 pcrmanager.h                   \
 tdmamacheadermessage.h        	\
 tdmanem.xml.in                	\
 tdmamac.xml.in                	\
 tdmapcr.xml.in                	\
 tdmamacheader.proto           	\
 maclayer.h			\
 downstreamqueue.h		\
 fragmentmgr.h			\
 downstreammgr.h

BUILT_SOURCES =              	\
 tdmanem.xml                   	\
 tdmamac.xml                   	\
 tdmapcr.xml                   	\
 tdmamacheader.pb.cc           	\
 tdmamacheader.pb.h		

libtdmamaclayer_la_LIBADD=     \
 $(WIN32_EXTRA_LIBADD)        

libtdmamaclayer_la_LDFLAGS=    \
 $(WIN32_EXTRA_LDFLAGS)        \
 $(libxml2_LIBS)               \
 -avoid-version

edit = sed                                \
       -e 's|@datadir[@]|$(pkgdatadir)|g'  

tdmanem.xml: tdmanem.xml.in
	$(edit) $< > $@

tdmamac.xml: tdmamac.xml.in
	$(edit) $< > $@

tdmapcr.xml: tdmapcr.xml.in
	$(edit) $< > $@

tdmamacheader.pb.cc tdmamacheader.pb.h: tdmamacheader.proto 
	protoc -I=. --cpp_out=. $<

clean-local:
	rm -f $(BUILT_SOURCES)

install-exec-hook:	
	$(mkinstalldirs) $(DESTDIR)$(datadir)/$(PACKAGE)/xml/models/mac/tdma
	cp -f tdmanem.xml $(SCHEMAS) $(DESTDIR)$(datadir)/$(PACKAGE)/xml/models/mac/tdma
	cp -f tdmamac.xml $(SCHEMAS) $(DESTDIR)$(datadir)/$(PACKAGE)/xml/models/mac/tdma
	cp -f tdmapcr.xml $(SCHEMAS) $(DESTDIR)$(datadir)/$(PACKAGE)/xml/models/mac/tdma

uninstall-local:
	rm -f $(DESTDIR)$(datadir)/$(PACKAGE)/xml/models/mac/tdma/tdmanem.xml
	rm -f $(DESTDIR)$(datadir)/$(PACKAGE)/xml/models/mac/tdma/tdmamac.xml
	rm -f $(DESTDIR)$(datadir)/$(PACKAGE)/xml/models/mac/tdma/tdmapcr.xml
