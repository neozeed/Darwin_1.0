#
# apple-generic.make
#
# Generic versioning scheme
#
# This versioning scheme is based on the use of the CURRENT_PROJECT_VERSION
# make variable. This variable is kept in a project's Makefile.preamble and
# its updating can be automated. CURRENT_PROJECT_VERSION must be set to a
# numeric value, either integer or floating point. When a project is built,
# a version file is created which contains the definitions for two constant
# variables:
#	$(VERSION_INFO_PREFIX)$(NAME)VersionString$(VERSION_INFO_SUFFIX)
#	$(VERSION_INFO_PREFIX)$(NAME)VersionNumber$(VERSION_INFO_SUFFIX)
# where $(NAME) is the name of the project as determined by the standard
# makefiles and set in ProjectBuilder. The meaning of $(VERSION_INFO_PREFIX)
# and $(VERSION_INFO_SUFFIX) is explained below. The first contains a C
# string of unsigned characters, the second is a C double. These version
# variables are global by default, but this can be changed by setting
# VERSION_INFO_EXPORT_DECL as explained below. The version string has a
# format that is understood by the `what` command. This version file is
# recreated whenever the binary would otherwise be relinked.
#
# There are three make targets added by the versioning scheme: 'what-version',
# 'new-version', and 'next-version'. 'what-version' simply reports the current
# version to the standard output. 'new-version' modifies the current version
# specified in the CURRENT_PROJECT_VERSION in the Makefile.preamble to that
# specified in the variable NEW_VERSION, which should be set when make is
# invoked with the 'new-version' target. If VERSION_INFO_CVS is YES,
# 'new-version' will automatically commit the modified Makefile.preamble to
# the repository. The 'next-version' target invokes the new-version target
# with floor(CURRENT_PROJECT_VERSION)+1.
#
# If you use CVS (ie if $(VERSION_INFO_CVS) is YES), then a fourth new make 
# target called 'integrate' can be used to tag the current versions in the 
# repository with the tag $(NAME)-$(CURRENT_PROJECT_VERSION) with any '.' 
# in the version number replaced with '~'.
#
# To use this, set these variables in the Makefile.preamble at the top
# level of the project in which you want to include the version globals:
#	CURRENT_PROJECT_VERSION = 100
#	COMPATIBILITY_PROJECT_VERSION = 1
#	VERSIONING_SYSTEM = apple-generic
#
#    Note: Set CURRENT_PROJECT_VERSION to whatever numeric value you want.
#    Note: The CURRENT_PROJECT_VERSION assignment must be at the left
#    margin or only preceeded by spaces and tabs.
#    Note: The COMPATIBILITY_PROJECT_VERSION assignment is not necessary
#    as it will default to 1, but it is a good idea to set this for
#    frameworks and libraries to an interesting value.
#
# You can optionally set these variables in your Makefile.preamble and
# they will be used:
#    ENABLE_VERSION_INFO:	If set to NO, disables creation of the file
#				with the version variables, and its inclusion
#				in the binary of the build product. The
#				CURRENT_PROJECT_VERSION and this versioning
#				scheme are still useful to provide a version
#				number for framework and library binaries;
#				defaults to empty string
#    VERSION_INFO_EXPORT_DECL:	Can be used to change the default storage
#				class specifier of the version variables,
#				for example, to make them "static"; defaults
#				to "__declspec(dllexport)" on Windows so
#				that the variables are exported from a DLL,
#				defaults to empty string on all other
#				platforms (the variables will be global)
#    VERSION_INFO_PREFIX:	The prefix of the version variables in the
#				version file, useful when the variables are
#				global; defaults to empty string
#    VERSION_INFO_SUFFIX:	The suffix of the version variables in the
#				version file, useful when the variables are
#				global; defaults to empty string
#    VERSION_INFO_BUILDER:	Name of the user building the project;
#				defaults to $(USER)
#    VERSION_INFO_CVS:		YES does some extra convenience things for
#				CVS-based projects (assumes cvs is in your
#				path or $(CVS) variable is set to path to
#				cvs; defaults to empty
#    VERSION_INFO_CVS_MODULE:	Name of the CVS module; defaults to $(NAME),
#				the name of the build product
#

ifeq "$(VERSION_INFO_CVS)" "YES"
ifeq "" "$(CVS)"
CVS = cvs
endif
endif

ifneq "$(ENABLE_VERSION_INFO)" "NO"

ifeq "" "$(VERSION_INFO_EXPORT_DECL)"
ifeq "WINDOWS" "$(OS)"
VERSION_INFO_EXPORT_DECL = "__declspec(dllexport)"
endif
endif

ifeq "" "$(VERSION_INFO_BUILDER)"
VERSION_INFO_BUILDER = $(USER)
endif
ifeq "" "$(VERSION_INFO_BUILDER)"
VERSION_INFO_BUILDER = unknown
endif

ifeq "" "$(VERSION_INFO_CVS_MODULE)"
VERSION_INFO_CVS_MODULE = "$(NAME)"
endif

LOCAL_CURRENT_PROJECT_VERSION = $(CURRENT_PROJECT_VERSION)
ifeq "" "$(LOCAL_CURRENT_PROJECT_VERSION)"
LOCAL_CURRENT_PROJECT_VERSION = 1
endif

VERS_FILE = $(SFILE_DIR)/$(NAME)_vers.c
OTHER_GENERATED_SRCFILES += $(NAME)_vers.c
OTHER_GENERATED_OFILES += $(NAME)_vers.o
BEFORE_PREBUILD += generate-version-file

generate-version-file: $(VERS_FILE)
$(VERS_FILE): $(SFILE_DIR)
	$(SILENT) $(ECHO) "Creating $@"
ifeq "" "$(CURRENT_PROJECT_VERSION)"
	$(SILENT) $(ECHO) "*** Warning: the CURRENT_PROJECT_VERSION variable is not set."
endif
	$(SILENT) $(ECHO) $(VERSION_INFO_EXPORT_DECL) " const unsigned char $(VERSION_INFO_PREFIX)$(NAME)VersionString$(VERSION_INFO_SUFFIX)[] = \"@(#)PROGRAM:$(NAME)  PROJECT:$(NAME)-$(LOCAL_CURRENT_PROJECT_VERSION)  DEVELOPER:$(VERSION_INFO_BUILDER)  BUILT:\" __DATE__  \" \" __TIME__ \"\n\";" > $(VERS_FILE)
	$(SILENT) $(ECHO) $(VERSION_INFO_EXPORT_DECL) " const double $(VERSION_INFO_PREFIX)$(NAME)VersionNumber$(VERSION_INFO_SUFFIX) = (double)$(LOCAL_CURRENT_PROJECT_VERSION);" >> $(VERS_FILE)

endif

what-version:
ifeq "" "$(CURRENT_PROJECT_VERSION)"
	$(SILENT) $(ECHO) "*** The CURRENT_PROJECT_VERSION variable is not set."
else
	$(SILENT) $(ECHO) "*** The current $(NAME) version is: $(CURRENT_PROJECT_VERSION)"
endif

new-version:
ifeq "" "$(NEW_VERSION)"
	$(SILENT) $(ECHO) "*** The NEW_VERSION make variable must be set."
	$(SILENT) $(ECHO) "*** The current version is: $(CURRENT_PROJECT_VERSION)"
	$(SILENT) $(ECHO) ""
	$(SILENT) exit 1
endif
	$(SILENT) $(SED) "s%^\([ 	]*\)CURRENT_PROJECT_VERSION[ 	]*=.*$$%\1CURRENT_PROJECT_VERSION = $(NEW_VERSION)%" Makefile.preamble > Makefile.preamble.new
	$(SILENT) $(MV) Makefile.preamble.new Makefile.preamble
ifeq "$(VERSION_INFO_CVS)" "YES"
	$(SILENT) $(CVS) commit -m "Change version to $(NEW_VERSION)" Makefile.preamble
endif

next-version:
	$(SILENT) $(MAKE) new-version NEW_VERSION=`$(ECHO) $(CURRENT_PROJECT_VERSION) | $(SED) 's%\..*$$%%' | $(AWK) '{print $$1 + 1}'`

integrate:
ifeq "$(VERSION_INFO_CVS)" "YES"
	$(SILENT) if cvs -q update -d -P | $(AWK) '$$1 == "C" || $$1 == "M" {exit 1}' ; then \
		$(CVS) tag $(NAME)-`$(ECHO) $(CURRENT_PROJECT_VERSION) | $(SED) 's%\.%~%g'` ; \
		$(ECHO) ; \
		$(ECHO) "Integrated $(NAME)-$(CURRENT_PROJECT_VERSION)" ; \
	else									\
		$(ECHO) "*** Conflicts or uncommitted files during update." ;	\
		$(ECHO) "*** Commit all files or update by hand and try again." ; \
		exit 1 ;							\
	fi
else
	$(SILENT) $(ECHO) "Do not know how to integrate this project without CVS."
endif
