##
# Makefile for bind
##

# Project info
Project           = bind
UserType          = Administration
ToolType          = Services
Extra_Environment = DESTBIN=$(USRBINDIR)		\
		    DESTSBIN=$(USRSBINDIR)		\
		    DESTEXEC=$(LIBEXECDIR)		\
		    DESTHELP=$(SHAREDIR)/misc		\
		    DESTLIB=/usr/local/bind/lib		\
		    DESTINC=/usr/local/bind/include	\
		    LD='$(CC) $(LDFLAGS) -nostdlib'	\
		    LD_LIBFLAGS='-Wl,-x -r'		\
		    AR='$(SRCROOT)/ar.sh cru'

# It's a Common Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

##
# Targets
##

Install_Flags = DESTDIR="$(DSTROOT)"

install:: build
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) install

build:: setup
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment)

setup:: $(BuildDirectory)/Makefile

$(BuildDirectory)/Makefile:
	$(_v) mkdir -p $(BuildDirectory)
	$(_v) $(MAKE) -C $(Sources)/$(Project)/src $(Environment)		\
		DST="$(BuildDirectory)" SRC="$(Sources)/$(Project)/src" links
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment) depend
