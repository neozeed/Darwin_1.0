#
# apple-generic.make
#
# Apple-internal component of generic versioning scheme
#
# [See [/System]/Developer/Makefiles/VersioningSystems/apple-generic.make
# for more information.]
#
# This local version adds the ability to automatically submit projects to RC
#
# The variable RC_SUBMIT_TOOL can be used to customize the tool used to submit.
# It defaults to "/Network/Servers/seaport/release/bin/submitproject"
#
# The variable RC_PROJECT_NAME can be used to set the name of the RC project 
# to submit to.  It defaults to $(NAME).
#
# The variable RC_SUBMIT_TARGETS is a space-separated list of the RC targets
# to which to submit.

ifeq "" "$(RC_SUBMIT_TOOL)"
RC_SUBMIT_TOOL = /Network/Servers/seaport/release/bin/submitproject
endif

ifeq "" "$(RC_PROJECT_NAME)"
RC_PROJECT_NAME = $(NAME)
endif

submit:
ifeq "" "$(RC_SUBMIT_TARGETS)"
	$(SILENT) $(ECHO) "*** The RC_SUBMIT_TARGETS make variable must be set"
	$(SILENT) $(ECHO) "*** to the list of RC targets to which to submit."
	$(SILENT) exit 1
endif
	$(RC_SUBMIT_TOOL) -version $(RC_PROJECT_NAME)-$(CURRENT_PROJECT_VERSION) . $(RC_SUBMIT_TARGETS)

