export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir


include $(MakeInc_cmd)
include $(MakeInc_def)

SETUP_SUBDIRS = \
	tools

COMP_SUBDIRS = 

INST_SUBDIRS = 

ifndef IOKIT_KERNEL_CONFIG
export IOKIT_KERNEL_CONFIG = $(KERNEL_CONFIG)
endif

COMPOBJROOT=$(OBJROOT)/$(KERNEL_CONFIG)_$(ARCH_CONFIG)/$(COMPONENT)

$(OBJROOT)/$(KERNEL_CONFIG)_$(ARCH_CONFIG)/$(COMPONENT)/doconf:
	make build_setup 

$(COMPOBJROOT)/$(IOKIT_KERNEL_CONFIG)/Makefile :  $(SOURCE)/MASTER  \
	$(SOURCE)/MASTER.$(arch_config) \
	$(SOURCE)/Makefile.template  \
	$(SOURCE)/Makefile.$(arch_config)  \
	$(SOURCE)/files \
	$(SOURCE)/files.$(arch_config)
	@echo "Running doconf for $(IOKIT_KERNEL_CONFIG)";
	(doconf_target=$(addsuffix /conf, $(TARGET)); \
	echo $${doconf_target};\
	$(MKDIR) $${doconf_target}; \
	cd $${doconf_target}; \
	rm -f $(notdir $?); \
	cp  $? $${doconf_target}; \
	$(OBJROOT)/$(KERNEL_CONFIG)_$(ARCH_CONFIG)/$(COMPONENT)/doconf -c -cpu $(arch_config) -d  $(TARGET)/$(IOKIT_KERNEL_CONFIG) $(IOKIT_KERNEL_CONFIG); \
	);

.ORDER: $(COMPOBJROOT)/$(IOKIT_KERNEL_CONFIG)/Makefile 

do_setup_conf: $(OBJROOT)/$(KERNEL_CONFIG)_$(ARCH_CONFIG)/$(COMPONENT)/doconf \
		$(COMPOBJROOT)/$(IOKIT_KERNEL_CONFIG)/Makefile 

do_all: 
	@echo "[ $(SOURCE) ] Starting do_all  $(COMPONENT) $(IOKIT_KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)";	\
	(cd $(COMPOBJROOT)/$(IOKIT_KERNEL_CONFIG);				\
	next_source=$(subst conf/,,$(SOURCE));			\
	${MAKE} MAKEFILES=$(TARGET)/$(IOKIT_KERNEL_CONFIG)/Makefile	\
		SOURCE=$${next_source}			\
		TARGET=$(TARGET)					\
		build_all	\
	);		\
	echo "[ $(SOURCE) ] Returning do_all $(COMPONENT) $(IOKIT_KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)";

do_build_all: do_setup_conf do_all 

include $(MakeInc_rule)
include $(MakeInc_dir)

