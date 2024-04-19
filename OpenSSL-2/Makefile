##
# Makefile for OpenSSL
##

# Project info
Project         = openssl
ProjectName     = OpenSSL
UserType        = Developer
ToolType        = Libraries
Configure       = $(Sources)/config
GnuAfterInstall = Frameworkify

# config is kinda like configure
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Version = 0.9.4

FrameworkName        = openssl
Framework            = $(NSFRAMEWORKDIR)/$(FrameworkName).framework
FrameworkVersion     = 0
FrameworkVersioned   = $(NSFRAMEWORKDIR)/$(FrameworkName).framework/Versions/$(FrameworkVersion)
FrameworkAddress     = 0x64500000
FrameworkVersionInfo = -compatibility_version 0.9.4 -current_version $(Version)

# config is not really like configure
Configure_Flags = --prefix=$(Install_Prefix)					\
		  --openssldir="$(NSLOCALDIR)$(NSLIBRARYSUBDIR)/OpenSSL"	\
		  --install_prefix="$(DSTROOT)"

Environment     = CFLAG="$(CFLAGS)"				\
		  AR="$(SRCROOT)/ar.sh r"			\
		  PERL='/usr/bin/perl'				\
		  BINDIR="$(FrameworkVersioned)/Executables"	\
		  LIBDIR="$(FrameworkVersioned)/Libraries"	\
		  INCLUDEDIR="$(FrameworkVersioned)/Headers"
Install_Target  = install

# Shadow the source tree
lazy_install_source:: shadow_source
	$(_v) if [ -L $(BuildDirectory)/Makefile.ssl ]; then						\
		 $(RM) "$(BuildDirectory)/Makefile.ssl";						\
		 $(CP) "$(Sources)/Makefile.ssl" "$(BuildDirectory)/Makefile.ssl";			\
		 $(RM) "$(BuildDirectory)/crypto/opensslconf.h";					\
		 $(CP) "$(Sources)/crypto/opensslconf.h" "$(BuildDirectory)/crypto/opensslconf.h";	\
	      fi

Frameworkify:
	$(_v) $(CC) $(CC_Archs) -dynamiclib $(FrameworkVersionInfo)			\
		    -all_load -seg1addr $(FrameworkAddress)				\
		    -install_name "$(FrameworkVersioned)/$(FrameworkName)"		\
		    "$(DSTROOT)$(FrameworkVersioned)/Libraries"/*.a			\
		    -o "$(DSTROOT)$(FrameworkVersioned)/$(FrameworkName)"
	$(_v) $(RMDIR) "$(DSTROOT)$(FrameworkVersioned)/Libraries"
	$(_v) ( cd "$(DSTROOT)$(Framework)" && ln -s "Versions/$(FrameworkVersion)"/* . )
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRBINDIR)
	$(_v) for program in `ls "$(DSTROOT)$(Framework)/Executables"`; do			\
		ln -s "$(Framework)/Executables/$${program}" "$(DSTROOT)$(USRBINDIR)/$${program}";	\
	      done
