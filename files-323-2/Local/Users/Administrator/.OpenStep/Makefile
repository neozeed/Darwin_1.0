Destination = $(DSTROOT)/Local/Users/Administrator/.OpenStep

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Directory_Mode = 700
Install_File_Mode      = 600

# Subdirectories with their own makefiles
SubDirs = UserDefaults2

install::
	$(_v) for subdir in $(SubDirs); do                                              \
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");      \
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
