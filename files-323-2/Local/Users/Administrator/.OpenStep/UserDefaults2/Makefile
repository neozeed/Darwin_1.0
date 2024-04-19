Destination = $(DSTROOT)/Local/Users/Administrator/.OpenStep/UserDefaults2

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Directory_Mode = 700
Install_File_Mode      = 600

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do                                              \
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");      \
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c NSGlobalDomain.defaults "$(Destination)/NSGlobalDomain.defaults"
