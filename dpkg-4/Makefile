##
# Makefile for dpkg
##

# Project info
Project               = dpkg.src
ProjectName           = dpkg
Extra_Environment     = OPTCFLAGS="$(CFLAGS)" OPTCXXFLAGS="$(CXXFLAGS)"
Extra_Configure_Flags = --localstatedir=/var --sharedstatedir=/etc --libdir=/usr/share
Extra_Install_Flags   = localstatedir=/private/var sharedstatedir=/private/etc libdir=/usr/share
UserType              = Administration
ToolType              = Commands

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install::
	mkdir -p $(DSTROOT)$(dir $(Install_HTML))
	mv $(DSTROOT)/usr/doc/dpkg $(DSTROOT)$(dir $(Install_HTML))
	mv $(DSTROOT)$(Install_HTML)/packaging.html \
	   $(DSTROOT)$(Install_HTML)/packaging
	mv $(DSTROOT)$(Install_HTML)/internals.html \
	   $(DSTROOT)$(Install_HTML)/internals
	rm $(DSTROOT)$(Install_HTML)/developer-keys.pgp
	rm $(DSTROOT)/usr/sbin/install-info
	rm $(DSTROOT)/usr/sbin/update-rc.d
	rm $(DSTROOT)/usr/sbin/start-stop-daemon
	rm $(DSTROOT)/usr/share/man/man8/install-info.8
	rm $(DSTROOT)/usr/share/man/man8/update-rc.d.8
	rm $(DSTROOT)/usr/share/man/man8/start-stop-daemon.8
	rmdir $(DSTROOT)/usr/doc
	rm -rf $(DSTROOT)/usr/etc
	mkdir -p $(DSTROOT)/System/Library/Frameworks/dpkg.framework
	mv $(DSTROOT)/usr/include/dpkg \
	   $(DSTROOT)/System/Library/Frameworks/dpkg.framework/Headers
	rmdir $(DSTROOT)/usr/include
