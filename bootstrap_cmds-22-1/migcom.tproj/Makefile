#
# Generated by the NeXT Project Builder.
#
# NOTE: Do NOT change this file -- Project Builder maintains it.
#
# Put all of your customizations in files called Makefile.preamble
# and Makefile.postamble (both optional), and Makefile will include them.
#

NAME = migcom

PROJECTVERSION = 2.8
PROJECT_TYPE = Tool

HFILES = routine.h type.h utils.h lexxer.h global.h statement.h\
         write.h error.h alloc.h strdefs.h mig_machine.h

OTHERLINKED = lexxer.l parser.y

CFILES = error.c global.c header.c mig.c routine.c server.c\
         statement.c string.c type.c user.c utils.c

OTHERSRCS = i386 Makefile Makefile.postamble Makefile.preamble mig.sh\
            ppc

OTHERLINKEDOFILES = lexxer.o parser.o

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
CODE_GEN_STYLE = DYNAMIC
MAKEFILE = tool.make
NEXTSTEP_INSTALLDIR = /usr/libexec
LIBS = 
DEBUG_LIBS = $(LIBS)
PROF_LIBS = $(LIBS)




NEXTSTEP_BUILD_OUTPUT_DIR = /tmp

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
