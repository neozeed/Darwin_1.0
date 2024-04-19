##
# Makefile for emacs
##

# Project info
Project        = emacs
UserType       = Developer
ToolType       = Commands
Extra_LD_Flags = -read_only_relocs warning

# It's a GNU Source project
include /System/Developer/Makefiles/CoreOS/ReleaseControl/GNUSource.make

# Ignore RC_CLAGS; we don't want -DNeXT
Extra_CC_Flags = -traditional-cpp

# Emacs cannot be built fat; it requires a forked native build
CC_Archs = 

.PHONY: install_app

# Delete this line when the App is known to work. This puts the app in /Local instead of /System.
NSDEVELOPERAPPSDIR = $(NSLOCALDIR)$(NSDEVELOPERSUBDIR)$(NSAPPLICATIONSUBDIR)

install_app:
	$(MKDIR) $(DSTROOT)$(NSDEVELOPERAPPSDIR)/Emacs.app
	$(LN) -fs $(Install_Prefix)/bin/emacs $(DSTROOT)$(NSDEVELOPERAPPSDIR)/Emacs.app/Emacs
