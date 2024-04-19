##
# Makefile for make
##

# Project info
Project               = make
ProjectName           = gnumake
UserType              = Developer
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="gnu"
GnuAfterInstall       = install-links

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-links:
	@echo "Installing make symlink"
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRBINDIR)
	$(_v) $(LN) -fs gnumake $(DSTROOT)$(USRBINDIR)/make
	@echo "Installing /bin compatibility symlinks"
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(BINDIR)
	$(_v) $(LN) -fs $(USRBINDIR)/gnumake $(DSTROOT)$(BINDIR)/gnumake
	$(_v) $(LN) -fs $(USRBINDIR)/gnumake $(DSTROOT)$(BINDIR)/make
