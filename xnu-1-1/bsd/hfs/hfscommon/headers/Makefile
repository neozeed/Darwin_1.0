export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir


include $(MakeInc_cmd)
include $(MakeInc_def)

INSTINC_SUBDIRS = \
	system

INSTINC_SUBDIRS_PPC = \

INSTINC_SUBDIRS_I386 = \

EXPINC_SUBDIRS = \
	system

EXPINC_SUBDIRS_PPC = \

EXPINC_SUBDIRS_I386 = \

DATAFILES = \
	BTreesInternal.h BTreesPrivate.h CatalogPrivate.h \
	FileMgrInternal.h HFSVolumes.h

INSTALL_MI_LIST	= ${DATAFILES}

INSTALL_MI_DIR = bsd/hfs/hfscommon/headers

EXPORT_MI_LIST	= ${DATAFILES}

EXPORT_MI_DIR = hfs/hfscommon/headers


include $(MakeInc_rule)
include $(MakeInc_dir)


