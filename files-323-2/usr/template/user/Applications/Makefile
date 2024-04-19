Destination = $(DSTROOT)/usr/template/user/Applications

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) (cd ../../../../Local/Applications && $(MAKE) $@ Destination="$(Destination)");
	$(_v) $(LN) -fs /System/Applications/Grab.app		"$(Destination)"
	$(_v) $(LN) -fs /System/Applications/Mail.app		"$(Destination)"
	$(_v) $(LN) -fs /System/Applications/Preferences.app	"$(Destination)"
	$(_v) $(LN) -fs /System/Applications/Preview.app	"$(Destination)"
	$(_v) $(LN) -fs /System/Applications/Sherlock.app	"$(Destination)"
	$(_v) $(LN) -fs /System/Applications/TextEdit.app	"$(Destination)"
	$(_v) $(LN) -fs /System/Demos				"$(Destination)"
