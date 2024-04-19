Destination = $(DSTROOT)/Local

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
	$(_v) (cd ../Local/Administration && $(MAKE) $@ Destination="$(Destination)/Administration"  );
	$(_v) (cd ../Local/Applications   && $(MAKE) $@ Destination="$(Destination)/Applications"    );
	$(_v) (cd ../Local/Developer      && $(MAKE) $@ Destination="$(Destination)/Developer"       );
	$(_v) (cd ../Local/Documentation  && $(MAKE) $@ Destination="$(Destination)/Documentation"   );
	$(_v) (cd ../Local/Library        && $(MAKE) $@ Destination="$(Destination)/Library"         );
	$(_v) (cd ../Local/Public         && $(MAKE) $@ Destination="$(Destination)/Public"          );
	$(_v) (cd ../Local/Users          && $(MAKE) $@ Destination="$(Destination)/Users" SubDirs="");
