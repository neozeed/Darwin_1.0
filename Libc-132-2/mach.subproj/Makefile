#
# Generated by the Apple Project Builder.
#
# NOTE: Do NOT change this file -- Project Builder maintains it.
#
# Put all of your customizations in files called Makefile.preamble
# and Makefile.postamble (both optional), and Makefile will include them.
#

NAME = mach

PROJECTVERSION = 2.8
PROJECT_TYPE = Component

HFILES = externs.h

OTHERLINKED = mach_traps.s

CFILES = clock_sleep.c error_codes.c exc_catcher.c exc_catcher_state.c\
         exc_catcher_state_identity.c fprintf_stderr.c mach_error.c\
         mach_error_string.c mach_init.c mach_init_ports.c mach_msg.c\
         mach_msg_destroy.c mach_msg_receive.c mach_msg_send.c\
         mach_msg_server.c mach_subsystem_join.c mig_allocate.c\
         mig_deallocate.c mig_reply_setup.c mig_strncpy.c\
         ms_thread_switch.c panic.c port_obj.c sbrk.c slot_name.c

SUBPROJECTS = headers.subproj servers.subproj

OTHERSRCS = Makefile.preamble Makefile Makefile.postamble\
            bootstrap.defs clock.defs clock_reply.defs\
            default_pager_object.defs default_pager_types.defs\
            err_ipc.sub err_kern.sub err_mach_ipc.sub err_server.sub\
            err_us.sub exc.defs ledger.defs mach_host.defs\
            mach_port.defs memory_object.defs memory_object_default.defs\
            notify.defs prof.defs clock_priv.defs host_priv.defs\
            host_security.defs lock_set.defs memory_object_control.defs\
            memory_object_name.defs processor.defs processor_set.defs\
            semaphore.defs task.defs thread_act.defs vm_task.defs

OTHERLINKEDOFILES = mach_traps.o

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
CODE_GEN_STYLE = DYNAMIC
MAKEFILE = subproj.make
LIBS = 
DEBUG_LIBS = $(LIBS)
PROF_LIBS = $(LIBS)




NEXTSTEP_OBJCPLUS_COMPILER = /usr/bin/cc
WINDOWS_OBJCPLUS_COMPILER = $(DEVDIR)/gcc
PDO_UNIX_OBJCPLUS_COMPILER = $(NEXTDEV_BIN)/gcc
NEXTSTEP_JAVA_COMPILER = /usr/bin/javac
WINDOWS_JAVA_COMPILER = $(JDKBINDIR)/javac.exe
PDO_UNIX_JAVA_COMPILER = $(JDKBINDIR)/javac

include $(MAKEFILEDIR)/platform.make

-include Makefile.preamble

include $(MAKEFILEDIR)/$(MAKEFILE)

-include Makefile.postamble

-include Makefile.dependencies
