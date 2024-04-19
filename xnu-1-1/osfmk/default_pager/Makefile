export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir


include $(MakeInc_cmd)
include $(MakeInc_def)

MIG_DEFS =	\
	default_pager_object.defs default_pager_types.defs \
	default_pager_alerts.defs

MIG_HDRS = \
	default_pager_object_server.h default_pager_alerts.h 

MIG_EXPORT_HDRS = \
	default_pager_object.h \
	default_pager_types.h

DATAFILES = \
	default_pager_internal.h diag.h default_pager_types.h \
	${MIG_DEFS}

MIGINCLUDES = \
	default_pager_object_server.h default_pager_alerts.h \
	default_pager_object.h

INSTALL_MI_LIST	= ${DATAFILES} 

INSTALL_MI_GEN_LIST = ${MIGINCLUDES}

INSTALL_MI_DIR = default_pager

EXPORT_MI_LIST	= 

EXPORT_MI_GEN_LIST = ${MIG_EXPORT_HDRS} ${MIG_DEFS}

EXPORT_MI_DIR = default_pager

.ORDER: ${MIG_HDRS} ${MIGINCLUDES}

#
# Build path
#       
INCFLAGS_MAKEFILE= -I..

MIGKSFLAGS	= -DMACH_KERNEL_PRIVATE -DKERNEL_SERVER=1
MIGKUFLAGS	= -DMACH_KERNEL_PRIVATE -DKERNEL_USER=1

EXTERNAL_DEFAULT_PAGER_FILES = default_pager_object.c \
				default_pager_object.h

DEFAULT_PAGER_FILES    = default_pager_object_server.h \
				default_pager_object_server.c

DEFAULT_PAGER_ALERTS    = default_pager_alerts.h \
				default_pager_alerts.c 

COMP_FILES	= ${DEFAULT_PAGER_FILES} ${DEFAULT_PAGER_ALERTS} \
	${EXTERNAL_DEFAULT_PAGER_FILES}

.ORDER: ${DEFAULT_PAGER_FILES} ${DEFAULT_PAGER_ALERTS}

${EXTERNAL_DEFAULT_PAGER_FILES}\
	${DEFAULT_PAGER_FILES}: default_pager_object.defs
	${MIG} ${MIGFLAGS} ${MIGKSFLAGS}	\
	-header default_pager_object.h		\
	-user default_pager_object.c		\
	-sheader default_pager_object_server.h	\
	-server default_pager_object_server.c	\
	$<
	${MIG} ${MIGFLAGS} -DNO_DIRECT_RPC	\
	-header default_pager_object.h		\
	-user default_pager_object.c		\
	-sheader /dev/null			\
	-server /dev/null			\
	$<

${DEFAULT_PAGER_FILES}: default_pager_object.defs   
	${MIG} ${MIGFLAGS} ${MIGKSFLAGS}	\
	-header /dev/null			\
	-user /dev/null				\
	-sheader default_pager_object_server.h	\
	-server default_pager_object_server.c	\
	$<

${DEFAULT_PAGER_ALERTS}: default_pager_alerts.defs
	${MIG} ${MIGFLAGS} ${MIGKUFLAGS}	\
	-header default_pager_alerts.h		\
	-user default_pager_alerts.c		\
	-sheader /dev/null			\
	-server /dev/null			\
	$<   




include $(MakeInc_rule)
include $(MakeInc_dir)


