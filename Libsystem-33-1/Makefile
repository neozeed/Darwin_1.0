#
# Generated by the Apple Project Builder.
#
# NOTE: Do NOT change this file -- Project Builder maintains it.
#
# Put all of your customizations in files called Makefile.preamble
# and Makefile.postamble (both optional), and Makefile will include them.
#

NAME = System

PROJECTVERSION = 2.8
PROJECT_TYPE = Framework

OTHERSRCS = Makefile.preamble Makefile System.order

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
CURRENTLY_ACTIVE_VERSION = YES
DEPLOY_WITH_VERSION_NAME = B
CODE_GEN_STYLE = DYNAMIC
MAKEFILE = framework.make
NEXTSTEP_INSTALLDIR = $(SYSTEM_LIBRARY_DIR)/Frameworks
WINDOWS_INSTALLDIR = /Library/Frameworks
PDO_UNIX_INSTALLDIR = /Library/Frameworks
LIBS = -lc -lcurses -ldyld -lrld -ledit -linfo -lkvm -lm -lmacho\
       -lstreams
DEBUG_LIBS = $(LIBS)
PROF_LIBS = $(LIBS)


LIBRARY_PATHS = -L$(NEXT_ROOT)/Local/Developer/System
NEXTSTEP_PB_LDFLAGS = -nostdlib -all_load -seg1addr 0x41300000


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
