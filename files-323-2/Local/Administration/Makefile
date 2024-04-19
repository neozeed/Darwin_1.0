Destination = $(DSTROOT)/Local/Administration

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
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c .dir.tiff "$(Destination)/.dir.tiff"
	$(_v) $(INSTALL_FILE) -c .opendir.tiff "$(Destination)/.opendir.tiff"
