#
# Generated by the Apple Project Builder.
#
# NOTE: Do NOT change this file -- Project Builder maintains it.
#
# Put all of your customizations in files called Makefile.preamble
# and Makefile.postamble (both optional), and Makefile will include them.
#

NAME = kext

PROJECTVERSION = 2.8
PROJECT_TYPE = Component

HFILES = CFAdditions.h KEXTDefs.h KEXTPrivate.h KEXTManager.h

CFILES = CFAdditions.c KEXTBundle.c KEXTLib.c KEXTManager.c\
         KEXTModule.c KEXTPersonality.c

OTHERSRCS = Makefile.preamble Makefile


MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
CODE_GEN_STYLE = DYNAMIC
MAKEFILE = subproj.make
NEXTSTEP_INSTALLDIR = $(SYSTEM_LIBRARY_DIR)/Frameworks
LIBS = 
DEBUG_LIBS = $(LIBS)
PROF_LIBS = $(LIBS)


PUBLIC_HEADERS = KEXTManager.h

PROJECT_HEADERS = KEXTDefs.h CFAdditions.h KEXTPrivate.h KEXTManager.h



NEXTSTEP_BUILD_OUTPUT_DIR = /$(USER)/build/$(NAME)

NEXTSTEP_OBJCPLUS_COMPILER = /usr/bin/cc
WINDOWS_OBJCPLUS_COMPILER = $(DEVDIR)/gcc
PDO_UNIX_OBJCPLUS_COMPILER = $(NEXTDEV_BIN)/gcc
NEXTSTEP_JAVA_COMPILER = /usr/bin/javac
WINDOWS_JAVA_COMPILER = $(JDKBINDIR)/javac.exe
PDO_UNIX_JAVA_COMPILER = $(JDKBINDIR)/javac

include $(MAKEFILEDIR)/platform.make

-include Makefile.preamble

include $(MAKEFILEDIR)/$(MAKEFILE)

-include Makefile.postamble

-include Makefile.dependencies
