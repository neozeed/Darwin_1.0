#
# Generated by the NeXT Project Builder.
#
# NOTE: Do NOT change this file -- Project Builder maintains it.
#
# Put all of your customizations in files called Makefile.preamble
# and Makefile.postamble (both optional), and Makefile will include them.
#

NAME = bootstrap_cmds

PROJECTVERSION = 2.8
PROJECT_TYPE = Aggregate

TOOLS = migcom.tproj config.tproj relpath.tproj decomment.tproj

OTHERSRCS = Makefile.preamble Makefile Makefile.postamble\
            vers_string.csh

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
CODE_GEN_STYLE = DYNAMIC
MAKEFILE = aggregate.make
LIBS = 
DEBUG_LIBS = $(LIBS)
PROF_LIBS = $(LIBS)


NEXTSTEP_PB_CFLAGS = -DNeXT


NEXTSTEP_BUILD_OUTPUT_DIR = /tmp/bootstrap_cmds

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
