export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

include $(MakeInc_cmd)
include $(MakeInc_def)

INSTINC_SUBDIRS = machine

INSTINC_SUBDIRS_PPC = ppc

INSTINC_SUBDIRS_I386 = i386

EXPINC_SUBDIRS = ${INSTINC_SUBDIRS}

EXPINC_SUBDIRS_PPC = ${INSTINC_SUBDIRS_PPC}

EXPINC_SUBDIRS_I386 = ${INSTINC_SUBDIRS_I386}

DATAFILES = \
	    EventShmemLock.h	\
            ev_types.h		\
            event.h		\
            evio.h		\
	    ev_keymap.h		\
            evsio.h

INSTALL_MI_LIST	= ${DATAFILES}

INSTALL_MI_DIR = bsd/dev

EXPORT_MI_LIST	= \

EXPORT_MI_DIR = \


include $(MakeInc_rule)
include $(MakeInc_dir)


