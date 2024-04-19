export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir


include $(MakeInc_cmd)
include $(MakeInc_def)

INSTINC_SUBDIRS = \

EXPINC_SUBDIRS = \

COMP_SUBDIRS = \

INST_SUBDIRS = \


setup_installhdrs_mi:
	@echo "[ $(SOURCE) ] make setup_installhdrs_mi $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

do_installhdrs_mi:
	@echo "[ $(SOURCE) ] make local_installhdrs_mi $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

setup_installhdrs_md:
	@echo "[ $(SOURCE) ] make setup_installhdrs_md $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

do_installhdrs_md:
	@echo "[ $(SOURCE) ] make local_installhdrs_md $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

setup_exporthdrs_mi:
	@echo "[ $(SOURCE) ] make setup_exporthdrs_mi $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

do_exporthdrs_mi:
	@echo "[ $(SOURCE) ] make local_exporthdrs_mi $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

setup_exporthdrs_md:
	@echo "[ $(SOURCE) ] make setup_exporthdrs_md $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

do_exporthdrs_md:
	@echo "[ $(SOURCE) ] make local_exporthdrs_md $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

setup_build_all:
	@echo "[ $(SOURCE) ] make setup_build_all $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

do_build_all:
	@echo "[ $(SOURCE) ] make do_build_all $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

setup_build_install:
	@echo "[ $(SOURCE) ] make setup_build_all $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

do_build_install:
	@echo "[ $(SOURCE) ] make do_build_all $(KERNEL_CONFIG) $(ARCH_CONFIG) $(TARGET)"

include $(MakeInc_rule)
include $(MakeInc_dir)


