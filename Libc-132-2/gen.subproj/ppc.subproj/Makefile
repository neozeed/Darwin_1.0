#
# Generated by the NeXT Project Builder.
#
# NOTE: Do NOT change this file -- Project Builder maintains it.
#
# Put all of your customizations in files called Makefile.preamble
# and Makefile.postamble (both optional), and Makefile will include them.
#

NAME = ppc

PROJECTVERSION = 2.8
PROJECT_TYPE = Component

HFILES = fp.h genassym.h

OTHERLINKED = abs.s bcopy.s bzero.s ffs.s mcount.s memcpy.s\
              memmove.s strlen.s

CFILES = bcmp.c ecvt.c insque.c isinf.c remque.c setjmperr.c\
         strcat.c strcpy.c strncat.c strncmp.c strncpy.c

OTHERSRCS = Makefile.preamble Makefile Makefile.postamble

OTHERLINKEDOFILES = abs.o bcopy.o bzero.o ffs.o mcount.o memcpy.o\
                    memmove.o strlen.o

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
CODE_GEN_STYLE = DYNAMIC
MAKEFILE = subproj.make
LIBS = 
DEBUG_LIBS = $(LIBS)
PROF_LIBS = $(LIBS)




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
