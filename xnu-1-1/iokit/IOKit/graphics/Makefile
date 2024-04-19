export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

export INCDIR = $(FRAMEDIR)/IOKit.framework/Versions/A/Headers
export LCLDIR = $(FRAMEDIR)/System.framework/Versions/B/Headers/iokit

include $(MakeInc_cmd)
include $(MakeInc_def)

MI_DIR = graphics
EXCLUDE_HEADERS = IOFramebufferPrivate.h

INSTINC_SUBDIRS =
INSTINC_SUBDIRS_PPC =
INSTINC_SUBDIRS_I386 =

EXPINC_SUBDIRS = ${INSTINC_SUBDIRS}
EXPINC_SUBDIRS_PPC = ${INSTINC_SUBDIRS_PPC}
EXPINC_SUBDIRS_I386 = ${INSTINC_SUBDIRS_I386}

ALL_HEADERS = $(shell (cd $(SOURCE); echo *.h))
DATAFILES = $(filter-out $(EXCLUDE_HEADERS), $(ALL_HEADERS))

INSTALL_MI_LIST = IOGraphicsEngine.h IOFramebufferShared.h IOGraphicsTypes.h 	\
        IOAccelTypes.h IOAccelClientConnect.h IOAccelSurfaceConnect.h		\
        IOGLWindowConnect.h IOGLTypes.h

INSTALL_MI_LCL_LIST	= ${DATAFILES}

INSTALL_MI_DIR = $(MI_DIR)

EXPORT_MI_LIST	= ${DATAFILES}

EXPORT_MI_DIR = IOKit/$(MI_DIR)

include $(MakeInc_rule)
include $(MakeInc_dir)

RED_INST_SCAFFOLD = $(filter $(INSTALL_MI_LIST), $(INSTALL_MI_LCL_LIST))
PRE_INST_SCAFFOLD = $(filter-out $(INSTALL_MI_LIST), $(INSTALL_MI_LCL_LIST))

build_installhdrs_mi::
	@echo "[ $(SOURCE) ] Removing old IOKit headers";		\
	cd $(DSTROOT)/$(LCLDIR)/$(INSTALL_MI_DIR);			\
	IOKitdir=$(EXPORT_MI_DIR);					\
	iokitdir=iok`expr "$$IOKitdir" : 'IOK\(.*\)'`;			\
	chmod +w $(RED_INST_SCAFFOLD) $(PRE_INST_SCAFFOLD);		\
	rm $(RED_INST_SCAFFOLD) $(PRE_INST_SCAFFOLD);			\
	echo "[ $(SOURCE) ] Constructing redirected scoffold headers";	\
	for targetfile in $(RED_INST_SCAFFOLD);	 			\
	do								\
	    headerName=$$IOKitdir/$$targetfile;				\
	    target=$$iokitdir/$$targetfile;				\
	    echo "#warning $$target deprecated: use $$headerName"	\
		> $$targetfile;						\
	    echo "#include <$$headerName>" >> $$targetfile;		\
	done;								\
	echo "[ $(SOURCE) ] Constructing deprecated scoffold headers";	\
	for targetfile in $(PRE_INST_SCAFFOLD);	 			\
	do								\
	    target=$$iokitdir/$$targetfile;				\
	    echo "#warning $$target deprecated: about to be deleted"	\
		> $$targetfile;						\
	    cat $(SOURCE)/$$targetfile >> $$targetfile;			\
	done;								\
	chmod a-w $(RED_INST_SCAFFOLD) $(PRE_INST_SCAFFOLD)
