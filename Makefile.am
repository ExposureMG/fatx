include m4/aminclude.am
AM_CXXFLAGS = -Wall -Wextra -pedantic
AM_CXXFLAGS += -Wno-long-long

sbin_PROGRAMS = fatx
dist_man_MANS = man/fatx.8 man/fsck.fatx.8 man/fusefatx.8 man/label.fatx.8 man/mkfs.fatx.8 man/unrm.fatx.8
dist_doc_DATA = README BUGS
EXTRA_DIST = test.sh doxygen.cfg xbox.cpp

if ENABLE_XBE
noinst_DATA = fsck.xbe unrm.xbe
noinst_PROGRAMS = fsck unrm
MINGW32CXX = i686-pc-mingw32-g++
endif

fatx_SOURCES = fatx.cpp fatx.hpp config.h Makefile.am
fatx_LDADD = $(BOOST_PROGRAM_OPTIONS_LIB)
#fatx_LDADD = $(BOOST_PROGRAM_OPTIONS_A)
fatx_CXXFLAGS = $(AM_CXXFLAGS) -D_FILE_OFFSET_BITS=64
if !USE_LIBFUSE
fatx_CXXFLAGS += -D NO_FUSE_CALL
endif
fatx_CXXFLAGS += -D NO_SPLICE
fatx_CXXFLAGS += -D NO_HANDLR
#fatx_CXXFLAGS += -D NO_IO
if ENABLE_DBG
fatx_CXXFLAGS += -D DBGCOLOR=0
#fatx_CXXFLAGS += -D DBG_INIT
fatx_CXXFLAGS += -D DBG_READ
fatx_CXXFLAGS += -D DBG_WRITE
#fatx_CXXFLAGS += -D DBG_DIFF
#fatx_CXXFLAGS += -D DBG_SEM
#fatx_CXXFLAGS += -D DBGSEM=\"FAT\"
fatx_CXXFLAGS += -D DBG_BUFFER
#fatx_CXXFLAGS += -D DBGBUFDMP=256
#fatx_CXXFLAGS += -D DBG_CACHE
#fatx_CXXFLAGS += -D DBG_CACHDMP
#fatx_CXXFLAGS += -D DBG_AREAS
#fatx_CXXFLAGS += -D DBG_GUESS
#fatx_CXXFLAGS += -D DBGCR=32
fatx_CXXFLAGS += -D DBGLIMIT=256
fatx_CXXFLAGS += -D DBG_FAT
fatx_CXXFLAGS += -D DBG_GAPS
#fatx_CXXFLAGS += -D NO_WRITE
#fatx_CXXFLAGS += -D NO_FUSE
#fatx_CXXFLAGS += -D NO_LOCK
#fatx_CXXFLAGS += -D NO_CACHE
#fatx_CXXFLAGS += -D NO_OPTION
fatx_LDFLAGS = -L/usr/local/lib -Wl,-rpath -Wl,/usr/local/lib
endif

if ENABLE_XBE
XBOXPREFIX = /usr/local/openxdk
XBOXCXXFLAGS = -march=i386
XBOXCXXFLAGS += -nostdlib
XBOXCXXFLAGS += -fno-builtin
XBOXCXXFLAGS += -fno-exceptions
XBOXCXXFLAGS += -I$(XBOXPREFIX)/i386-pc-xbox/include
XBOXCXXFLAGS += -I$(XBOXPREFIX)/include
#XBOXCXXFLAGS += -I$(XBOXPREFIX)/include/SDL
XBOXCXXFLAGS += -D ENABLE_XBOX
XBOXCXXFLAGS += -D DISABLE_CDROM
XBOXLDFLAGS = -shared
XBOXLDFLAGS += -nostdlib
XBOXLDFLAGS += -Wl,--file-alignment,0x20
XBOXLDFLAGS += -Wl,--section-alignment,0x20
XBOXLDFLAGS += -Wl,--entry,_WinMainCRTStartup
XBOXLDFLAGS += -Wl,--strip-all
XBOXLDFLAGS += -L$(XBOXPREFIX)/lib
XBOXLDFLAGS += -L$(XBOXPREFIX)/i386-pc-xbox/lib
XBOXLDADD = -lopenxdk -lhal -lc -lusb -lhal -lc -lxboxkrnl -lstdc++

fsck_SOURCES = fsck.cpp fatx.cpp fatx.hpp config.h Makefile.am
fsck_CXXFLAGS = $(XBOXCPPFLAGS) -D XBOX_FSCK
fsck_LDFLAGS = $(XBOXLDFLAGS)
fsck_LDADD = $(XBOXLDADD)
fsck_LINK = $(MINGW32CXX) -o $@ $(XBOXLDFLAGS)

unrm_SOURCES = unrm.cpp fatx.cpp fatx.hpp config.h Makefile.am
unrm_CXXFLAGS = $(XBOXCPPFLAGS) -D XBOX_UNRM
unrm_LDFLAGS = $(XBOXLDFLAGS)
unrm_LDADD = $(XBOXLDADD)
unrm_LINK = $(MINGW32CXX) -o $@ $(XBOXLDFLAGS)

CXBE = cxbe
SUFFIXES = .xbe
.cpp.xbe:
	test -x $*$(EXEEXT) || make $*$(EXEEXT)
	$(CXBE) -MODE:retail -TITLE:$@ -OUT:$@ $*$(EXEEXT) >/dev/null

fsck.xbe: fsck$(EXEEXT)
unrm.xbe: unrm$(EXEEXT)
endif

fsck.cpp: xbox.cpp
	$(LN_S) $< $@
unrm.cpp: xbox.cpp
	$(LN_S) $< $@

TESTS = test0 test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 test15 test16 test17 test18 test19 test20 test21

test0 test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 test15 test16 test17 test18 test19 test20 test21 : test.sh
	$(LN_S) -f $< $@

doc:
	make doxygen-doc

clean-local:
	-$(RM) -r mnt.*
maintainer-clean-local:
	-$(RM) -r debian/fatx debian/patches doc autom4te.cache .deps .pc $(PACKAGE_NAME)-$(PACKAGE_VERSION)

CLEANFILES =
CLEANFILES += *~ *.log logs/*.log debian/*.log debian/fatx.substvars test[0-9]* unrm.cpp fsck.cpp $(EXTRA_PROGRAMS) disk.*.fat disk.*.dif disk.*.ref *fatx name.txt fuse[0-9]* tbff* FILE* fsck$(EXEEXT) unrm$(EXEEXT)
DISTCLEANFILES =
DISTCLEANFILES += config.h config.cache config.log config.status stamp-h1 compile missing
MAINTAINERCLEANFILES =
MAINTAINERCLEANFILES +=  $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz
MAINTAINERCLEANFILES += analyse.sh profile.sh fsck.xbe unrm.xbe
MAINTAINERCLEANFILES += aclocal.m4 configure depcomp config.h.in install-sh test-driver Makefile.in
MAINTAINERCLEANFILES += config.guess config.sub libtool ltmain.sh

install-exec-hook:
	cd $(DESTDIR)$(sbindir) && \
	$(LN_S) -f fatx$(EXEEXT) fusefatx && \
	$(LN_S) -f fatx$(EXEEXT) mkfs.fatx && \
	$(LN_S) -f fatx$(EXEEXT) fsck.fatx && \
	$(LN_S) -f fatx$(EXEEXT) unrm.fatx && \
	$(LN_S) -f fatx$(EXEEXT) label.fatx

uninstall-hook:
	cd $(DESTDIR)$(sbindir) && \
	$(RM) fusefatx mkfs.fatx fsck.fatx unrm.fatx label.fatx

