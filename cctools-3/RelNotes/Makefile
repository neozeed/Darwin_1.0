OTHER_SRCS = Makefile
PRIVATE_NOTES = Private_CompilerTools.html FatFiles.rtf
NOTES = CompilerTools.html
INSTALL_FILES = $(OTHER_SRCS) $(NOTES) $(PRIVATE_NOTES)

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

NOTESDIR = /System/Documentation/Developer/ReleaseNotes
PRIVATE_NOTESDIR = /usr/local/RelNotes

install: ${DSTROOT}${NOTESDIR} ${DSTROOT}${PRIVATE_NOTESDIR} ${NOTES} ${PRIVATE_NOTES}
	install -c -m 444 ${IFLAGS} ${NOTES} ${DSTROOT}${NOTESDIR}
	install -c -m 444 ${IFLAGS} ${PRIVATE_NOTES} ${DSTROOT}${PRIVATE_NOTESDIR}

installGNUsrc installMWsrc:

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

${DSTROOT}${NOTESDIR}:; $(MKDIRS) $@

${DSTROOT}${PRIVATE_NOTESDIR}:; $(MKDIRS) $@

${DSTROOT}${PPC_ASSEMBLER_DIR}:; $(MKDIRS) $@

depend:

clean:
shlib_clean:

all:

local_install:
	cp Private_CompilerTools.html /Local/Library/WebServer/Documents
	cp CompilerTools.html /Local/Library/WebServer/Documents
