##
# Makefile for Bash
##

# Project info
Project               = bash
UserType              = Administration
ToolType              = Commands
Extra_Configure_Flags = --bindir=/bin
Extra_Install_Flags   = bindir=$(DSTROOT)/bin
Extra_CC_Flags        = -traditional-cpp

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Bash makefiles are a bit screwy...
# Setting CCFLAGS upsets bash, so we override Environment
# so that it doesn't.
Environment =   CFLAGS="$(CFLAGS)"	\
	       LDFLAGS="$(LDFLAGS)"	\
	      $(Extra_Environment)
