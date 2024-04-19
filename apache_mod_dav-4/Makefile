##
# Makefile for mod_dav
##

APXS = /usr/sbin/apxs

# Project info
Project               = mod_dav
ProjectName	      = apache_mod_dav
UserType              = Administrator
ToolType              = Services
Extra_Configure_Flags = --with-apxs
Extra_Environment     = APXS="$(APXS) -S LIBEXECDIR=\"$(DSTROOT)$(shell $(APXS) -q LIBEXECDIR)\""

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
