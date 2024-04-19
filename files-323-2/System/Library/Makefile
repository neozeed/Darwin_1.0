Destination = $(DSTROOT)/System/Library

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
	$(_v) $(INSTALL_FILE) -c ../../Local/Library/.dir.tiff     "$(Destination)/.dir.tiff"
	$(_v) $(INSTALL_FILE) -c ../../Local/Library/.opendir.tiff "$(Destination)/.opendir.tiff"
	$(_v) $(INSTALL_FILE) -c .hidden                           "$(Destination)/.hidden"
	$(_v) $(INSTALL_DIRECTORY) "$(SysHeaderDir)"
