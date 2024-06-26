OFLAG = -O
ifeq "mwcc" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) -g \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR)
else
  CFLAGS = $(OFLAG) -g -Wall -Wno-precomp \
	 -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR)
endif
LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

MIG = $(NEXT_ROOT)/usr/bin/mig

SRCROOT = .
OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)
SYMROOT = .

BINDIR = /bin
USRBINDIR = /usr/bin
LOCBINDIR = /usr/local/bin

CFILES1 = libtool.c
CFILES2 = main.c lipo.c size.c strings.c nm.c checksyms.c inout.c \
	 indr.c strip.c atom.c segedit.c kern_tool.c cmpdylib.c \
	 dylib_pcsampler.c pagestuff.c redo_prebinding.c
ifeq "nextstep" "$(RC_OS)"
  CFILES3 = file.c ar.c
endif
DEFS = make.defs make_defs.h
INSTALL_FILES = $(CFILES1) $(CFILES2) $(CFILES3) $(DEFS) Makefile \
		notes


PROGS = lipo.NEW size.NEW strings.NEW nm.NEW \
	libtool.NEW checksyms.NEW indr.NEW strip.NEW nmedit.NEW \
	segedit.NEW kern_tool.NEW cmpdylib.NEW \
	dylib_pcsampler.NEW pagestuff.NEW redo_prebinding.NEW

teflon_all macos_all: $(PROGS)

nextstep_all: $(PROGS) atom.NEW ar.NEW file.NEW

all:	$(OFILE_DIR) $(SYMROOT) $(RC_OS)_all

vers.c:
	vers_string -c $(VERS_STRING_FLAGS) cctools_misc > $(OFILE_DIR)/$@

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d

checksyms.NEW: checksyms.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/checksyms.private.o \
		$(OFILE_DIR)/checksyms.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/checksyms.NEW \
		$(OFILE_DIR)/checksyms.private.o

cmpdylib.NEW: cmpdylib.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/cmpdylib.private.o \
		$(OFILE_DIR)/cmpdylib.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/cmpdylib.NEW \
		$(OFILE_DIR)/cmpdylib.private.o

main.NEW: main.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/main.private.o \
		$(OFILE_DIR)/main.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/main.NEW \
		$(OFILE_DIR)/lipo.private.o

lipo.NEW: lipo.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/lipo.private.o \
		$(OFILE_DIR)/lipo.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/lipo.NEW \
		$(OFILE_DIR)/lipo.private.o

size.NEW: size.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/size.private.o \
		$(OFILE_DIR)/size.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/size.NEW \
		$(OFILE_DIR)/size.private.o

strings.NEW: strings.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/strings.private.o \
		$(OFILE_DIR)/strings.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/strings.NEW \
		$(OFILE_DIR)/strings.private.o

nm.NEW: nm.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/nm.private.o \
		$(OFILE_DIR)/nm.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/nm.NEW \
		$(OFILE_DIR)/nm.private.o

kern_tool.NEW: kern_tool.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/kern_tool.private.o \
		$(OFILE_DIR)/kern_tool.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/kern_tool.NEW \
		$(OFILE_DIR)/kern_tool.private.o

libtool.NEW: makeUser.o libtool.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/libtool.private.o \
		$(OFILE_DIR)/libtool.o $(OFILE_DIR)/vers.o \
		$(OFILE_DIR)/makeUser.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/libtool.NEW \
		$(OFILE_DIR)/libtool.private.o

redo_prebinding.NEW: redo_prebinding.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/redo_prebinding.private.o \
		$(OFILE_DIR)/redo_prebinding.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/redo_prebinding.NEW \
		$(OFILE_DIR)/redo_prebinding.private.o

makeUser.c libtool.o: make.h

make.h makeUser.c: make.defs
	@if [ $(SRCROOT) = . ];						\
	then								\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/make.defs";		\
	    $(MIG) $(MIG_FLAGS) $(SRCROOT)/make.defs;			\
	    rm -f makeServer.c;						\
	else								\
	    echo -n "cd $(OBJROOT); "					\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/make.defs";		\
	    cd $(OBJROOT); $(MIG) $(MIG_FLAGS) $(SRCROOT)/make.defs; 	\
	    cd $(OBJROOT); rm -f makeServer.c;				\
	fi

makeUser.o: makeUser.c
	cd $(OBJROOT); $(CC) $(CFLAGS) $(RC_CFLAGS) -c makeUser.c

inout.NEW: inout.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/inout.private.o \
		$(OFILE_DIR)/inout.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/inout.NEW \
		$(OFILE_DIR)/inout.private.o

core.NEW: core.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/core.private.o \
		$(OFILE_DIR)/core.o $(OFILE_DIR)/vers.o $(LIBSTUFF) \
		../libdyld/dtmp_obj/dyld_debug.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/core.NEW \
		$(OFILE_DIR)/core.private.o

indr.NEW: indr.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/indr.private.o \
		$(OFILE_DIR)/indr.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/indr.NEW \
		$(OFILE_DIR)/indr.private.o

strip.NEW: strip.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/strip.private.o \
		$(OFILE_DIR)/strip.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/strip.NEW \
		$(OFILE_DIR)/strip.private.o

nmedit.NEW: nmedit.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/nmedit.private.o \
		$(OFILE_DIR)/nmedit.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/nmedit.NEW \
		$(OFILE_DIR)/nmedit.private.o

nmedit.o: strip.c
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/nmedit.o $(SRCROOT)/strip.c -DNMEDIT
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d

file.NEW: file.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/file.private.o \
		$(OFILE_DIR)/file.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/file.NEW \
		$(OFILE_DIR)/file.private.o

ar.NEW: ar.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/ar.private.o \
		$(OFILE_DIR)/ar.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/ar.NEW \
		$(OFILE_DIR)/ar.private.o

atom.NEW: atom.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/atom.private.o \
		$(OFILE_DIR)/atom.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/atom.NEW \
		$(OFILE_DIR)/atom.private.o

segedit.NEW: segedit.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/segedit.private.o \
		$(OFILE_DIR)/segedit.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/segedit.NEW \
		$(OFILE_DIR)/segedit.private.o

dylib_pcsampler.NEW: dylib_pcsampler.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/dylib_pcsampler.private.o \
		$(OFILE_DIR)/dylib_pcsampler.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/dylib_pcsampler.NEW \
		$(OFILE_DIR)/dylib_pcsampler.private.o

pagestuff.NEW: pagestuff.o vers.o
	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/pagestuff.private.o \
		$(OFILE_DIR)/pagestuff.o $(OFILE_DIR)/vers.o $(LIBSTUFF)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/pagestuff.NEW \
		$(OFILE_DIR)/pagestuff.private.o


clean: shlib_clean
	-rm -f \
	$(SYMROOT)/main.NEW \
	$(SYMROOT)/lipo.NEW \
	$(SYMROOT)/size.NEW \
	$(SYMROOT)/strings.NEW \
	$(SYMROOT)/nm.NEW \
	$(SYMROOT)/libtool.NEW \
	$(SYMROOT)/checksyms.NEW \
	$(SYMROOT)/inout.NEW \
	$(SYMROOT)/indr.NEW \
	$(SYMROOT)/strip.NEW \
	$(SYMROOT)/nmedit.NEW \
	$(SYMROOT)/file.NEW \
	$(SYMROOT)/ar.NEW \
	$(SYMROOT)/atom.NEW \
	$(SYMROOT)/segedit.NEW \
	$(SYMROOT)/dylib_pcsampler.NEW \
	$(SYMROOT)/pagestuff.NEW \
	$(SYMROOT)/redo_prebinding.NEW \
	$(SYMROOT)/kern_tool.NEW \
	$(SYMROOT)/cmpdylib.NEW

shlib_clean:
	-rm -f \
	$(OFILE_DIR)/vers.c \
	$(OFILE_DIR)/vers.o \
	$(OFILE_DIR)/main.o \
	$(OFILE_DIR)/lipo.o \
	$(OFILE_DIR)/size.o \
	$(OFILE_DIR)/strings.o \
	$(OFILE_DIR)/nm.o \
	$(OFILE_DIR)/libtool.o \
	$(OFILE_DIR)/make.h \
	$(OFILE_DIR)/makeUser.c \
	$(OFILE_DIR)/checksyms.o \
	$(OFILE_DIR)/inout.o \
	$(OFILE_DIR)/indr.o \
	$(OFILE_DIR)/file.o \
	$(OFILE_DIR)/ar.o \
	$(OFILE_DIR)/atom.o \
	$(OFILE_DIR)/segedit.o \
	$(OFILE_DIR)/dylib_pcsampler.o \
	$(OFILE_DIR)/pagestuff.o \
	$(OFILE_DIR)/redo_prebinding.o \
	$(OFILE_DIR)/kern_tool.o \
	$(OFILE_DIR)/cmpdylib.o \
	$(OFILE_DIR)/nmedit.o \
	$(OFILE_DIR)/strip.o \
	$(OFILE_DIR)/main.private.o \
	$(OFILE_DIR)/lipo.private.o \
	$(OFILE_DIR)/size.private.o \
	$(OFILE_DIR)/strings.private.o \
	$(OFILE_DIR)/nm.private.o \
	$(OFILE_DIR)/libtool.private.o \
	$(OFILE_DIR)/checksyms.private.o \
	$(OFILE_DIR)/inout.private.o \
	$(OFILE_DIR)/indr.private.o \
	$(OFILE_DIR)/file.private.o \
	$(OFILE_DIR)/ar.private.o \
	$(OFILE_DIR)/atom.private.o \
	$(OFILE_DIR)/segedit.private.o \
	$(OFILE_DIR)/dylib_pcsampler.private.o \
	$(OFILE_DIR)/pagestuff.private.o \
	$(OFILE_DIR)/redo_prebinding.private.o \
	$(OFILE_DIR)/kern_tool.private.o \
	$(OFILE_DIR)/cmpdylib.private.o \
	$(OFILE_DIR)/nmedit.private.o \
	$(OFILE_DIR)/strip.private.o \
	$(OFILE_DIR)/makeUser.o
	-rm -f $(OFILE_DIR)/Makedep make.out

install: all $(RC_OS)_install

teflon_install macos_install: common_install
	$(MKDIRS) $(DSTROOT)$(USRBINDIR)
	install -c -s -m 555 $(SYMROOT)/strip.NEW $(DSTROOT)$(USRBINDIR)/strip
	install -c -s -m 555 $(SYMROOT)/strings.NEW \
		$(DSTROOT)$(USRBINDIR)/strings
	install -c -s -m 555 $(SYMROOT)/size.NEW $(DSTROOT)$(USRBINDIR)/size
	install -c -s -m 555 $(SYMROOT)/nm.NEW $(DSTROOT)$(USRBINDIR)/nm
	install -c -s -m 555 $(SYMROOT)/libtool.NEW \
		$(DSTROOT)$(USRBINDIR)/libtool
	(cd $(DSTROOT)$(USRBINDIR); rm -f ranlib; ln -s libtool ranlib)
	install -c -s -m 555 $(SYMROOT)/lipo.NEW $(DSTROOT)$(USRBINDIR)/lipo
	install -c -s -m 555 $(SYMROOT)/segedit.NEW \
		$(DSTROOT)$(USRBINDIR)/segedit
	install -c -s -m 555 $(SYMROOT)/cmpdylib.NEW \
			  $(DSTROOT)$(USRBINDIR)/cmpdylib
	install -c -s -m 555 $(SYMROOT)/pagestuff.NEW \
			  $(DSTROOT)$(USRBINDIR)/pagestuff
	install -c -s -m 555 $(SYMROOT)/redo_prebinding.NEW \
			  $(DSTROOT)$(USRBINDIR)/redo_prebinding
	install -c -s -m 555 $(SYMROOT)/nmedit.NEW $(DSTROOT)$(USRBINDIR)/nmedit
	(cd $(DSTROOT)$(LOCBINDIR); rm -f nmedit; \
	 ln -s $(USRBINDIR)/nmedit nmedit)

nextstep_install: common_install
	$(MKDIRS) $(DSTROOT)$(BINDIR)
	$(MKDIRS) $(DSTROOT)$(LOCBINDIR)
	install -c -s -m 555 $(SYMROOT)/strip.NEW $(DSTROOT)$(BINDIR)/strip
	install -c -s -m 555 $(SYMROOT)/strings.NEW $(DSTROOT)$(BINDIR)/strings
	install -c -s -m 555 $(SYMROOT)/size.NEW $(DSTROOT)$(BINDIR)/size
	install -c -s -m 555 $(SYMROOT)/nm.NEW $(DSTROOT)$(BINDIR)/nm
	install -c -s -m 555 $(SYMROOT)/libtool.NEW $(DSTROOT)$(BINDIR)/libtool
	(cd $(DSTROOT)$(BINDIR); rm -f ranlib; ln -s libtool ranlib)
	install -c -s -m 555 $(SYMROOT)/lipo.NEW $(DSTROOT)$(BINDIR)/lipo
	install -c -s -m 555 $(SYMROOT)/segedit.NEW $(DSTROOT)$(BINDIR)/segedit
	install -c -s -m 555 $(SYMROOT)/cmpdylib.NEW \
		$(DSTROOT)$(BINDIR)/cmpdylib
	install -c -s -m 555 $(SYMROOT)/pagestuff.NEW \
		$(DSTROOT)$(BINDIR)/pagestuff
	install -c -s -m 555 $(SYMROOT)/redo_prebinding.NEW \
			  $(DSTROOT)$(BINDIR)/redo_prebinding
	install -c -s -m 555 $(SYMROOT)/nmedit.NEW $(DSTROOT)$(LOCBINDIR)/nmedit
	install -c -s -m 555 $(SYMROOT)/ar.NEW $(DSTROOT)$(BINDIR)/ar
	install -c -s -m 555 $(SYMROOT)/file.NEW $(DSTROOT)$(BINDIR)/file
	install -c -s -m 555 $(SYMROOT)/atom.NEW $(DSTROOT)$(BINDIR)/atom

common_install:
	$(MKDIRS) $(DSTROOT)$(LOCBINDIR)
	install -c -s -m 555 $(SYMROOT)/checksyms.NEW \
			  $(DSTROOT)$(LOCBINDIR)/checksyms
	install -c -s -m 555 $(SYMROOT)/indr.NEW $(DSTROOT)$(LOCBINDIR)/indr
	install -c -s -m 555 $(SYMROOT)/kern_tool.NEW \
			  $(DSTROOT)$(LOCBINDIR)/kern_tool

installGNUsrc:

installMWsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(CFILES1) $(DEFS) Makefile | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
