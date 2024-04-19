##
# Makefile for ntp
##

# Project info
Project           = ntp
UserType          = Administration
ToolType          = Services
Extra_Environment = LIBMATH=""

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Install the HTML docs.
install::
	$(INSTALL_DIRECTORY) $(DSTROOT)$(Install_HTML)
	( cd $(Sources)/html && $(PAX) -rw -s '/.*CVS.*//' . $(DSTROOT)$(Install_HTML) )
