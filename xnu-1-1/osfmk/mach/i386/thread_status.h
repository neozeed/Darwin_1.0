/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/*
 * HISTORY
 * 
 * Revision 1.2  1998/09/30 21:21:01  wsanchez
 * Merged in IntelMerge1 (mburg: Intel support)
 *
 * Revision 1.1.1.1.6.1  1998/09/30 18:13:59  mburg
 * Changes for Intel port
 *
 * Revision 1.1.1.1  1998/09/22 21:05:31  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:47  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.2.8.3  1995/01/06  19:50:41  devrcs
 * 	Fix ri-osc CR880:  Make kdb_trap() check exception frame type before
 * 	trying to copy its contents.
 * 	[1994/12/22  20:39:01  bolinger]
 *
 * 	mk6 CR668 - 1.3b26 merge
 * 	Add MD file to define size of arrays
 * 	[1994/10/14  03:42:39  dwm]
 *
 * Revision 1.2.8.2  1994/09/23  02:37:48  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:40:21  ezf]
 * 
 * Revision 1.2.8.1  1994/08/07  20:48:59  bolinger
 * 	Merge up to colo_b7.
 * 	[1994/08/01  21:01:28  bolinger]
 * 
 * Revision 1.2.6.3  1994/06/25  03:47:11  dwm
 * 	mk6 CR98 - include mach/machine/thread_state.h to
 * 	get new MD THREAD_STATE_MAX
 * 	[1994/06/24  21:54:50  dwm]
 * 
 * Revision 1.2.6.2  1994/03/01  01:51:57  condict
 * 	Add fields to i386_saved_state for copying in the user's syscall args.
 * 	This is part of a performance improvement (see exception()).
 * 	[1994/02/28  04:07:12  condict]
 * 
 * Revision 1.2.6.1  1994/02/09  22:37:25  condict
 * 	Exported i386_saved_state as a new flavor of state; this is the same
 * 	struct used by kernel to save user state.  Allows avoidance of state
 * 	copying when doing short-circuited RPC to kernel-loaded servers.
 * 	[1994/02/09  22:36:46  condict]
 * 
 * Revision 1.2.2.2  1993/06/09  02:40:51  gm
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  21:16:28  jeffc]
 * 
 * Revision 1.2  1993/04/19  16:34:21  devrcs
 * 	ansi C conformance changes
 * 	[1993/02/02  18:56:17  david]
 * 
 * 	Add syscall exception state.				[sp@gr.osf.org]
 * 	[1992/12/23  13:06:03  david]
 * 
 * Revision 1.1  1992/09/30  02:30:52  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.6.3.1  92/03/03  16:21:46  jeffreyh
 * 	Changes from TRUNK
 * 	[92/02/26  11:59:48  jeffreyh]
 * 
 * Revision 2.7  92/01/03  20:20:19  dbg
 * 	Add REGS_SEGS flavor to get and set the segment registers.
 * 	Move fp_reg.h to mach/i386.
 * 	[91/10/18            dbg]
 * 
 * Revision 2.6  91/07/31  17:52:29  dbg
 * 	Add V86 mode interrupt assist.
 * 	[91/07/30  17:09:37  dbg]
 * 
 * Revision 2.5  91/05/14  16:52:33  mrt
 * 	Correcting copyright
 * 
 * Revision 2.4  91/02/05  17:32:23  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:10:12  mrt]
 * 
 * Revision 2.3  91/01/08  17:33:44  rpd
 * 	Two new flavors (from 2.5):
 * 	  #define i386_FLOAT_STATE	2
 * 		Hacked in not presently used, but someday...
 * 	 #define i386_ISA_PORT_MAP_STATE	3
 * 		Used 
 * 	[90/12/20  10:23:34  rvb]
 * 
 * Revision 2.2  90/05/03  15:48:05  dbg
 * 	Remove kernel-only definitions.
 * 	[90/02/05            dbg]
 * 
 * Revision 1.3  89/03/09  20:19:59  rpd
 * 	More cleanup.
 * 
 * Revision 1.2  89/02/26  13:01:07  gm0w
 * 	Changes for cleanup.
 * 
 * 24-Feb-89  Robert Baron (rvb) at Carnegie-Mellon University
 *	May need some work.
 */
/* CMU_ENDHIST */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	thread_status.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	This file contains the structure definitions for the thread
 *	state as applied to I386 processors.
 */

#ifndef	_MACH_I386_THREAD_STATUS_H_
#define _MACH_I386_THREAD_STATUS_H_

#include <mach/i386/fp_reg.h>
#include <mach/i386/thread_state.h>
#include <architecture/i386/frame.h>	/* FIXME */
#include <architecture/i386/fpu.h>	/* FIXME */
/*
 *	i386_thread_state	this is the structure that is exported
 *				to user threads for use in status/mutate
 *				calls.  This structure should never
 *				change.
 *
 *	i386_float_state	exported to use threads for access to 
 *				floating point registers. Try not to 
 *				change this one, either.
 *
 *	i386_isa_port_map_state	exported to user threads to allow
 *				selective in/out operations
 *
 * 	i386_v86_assist_state 
 *
 *	thread_syscall_state 
 */

/*     THREAD_STATE_FLAVOR_LIST 0 */
#define i386_NEW_THREAD_STATE	1	/* used to be i386_THREAD_STATE */
#define i386_FLOAT_STATE	2
#define i386_ISA_PORT_MAP_STATE	3
#define i386_V86_ASSIST_STATE	4
#define i386_REGS_SEGS_STATE	5
#define THREAD_SYSCALL_STATE	6
#define THREAD_STATE_NONE	7
#define i386_SAVED_STATE	8

/*
 * This structure is used for both
 * i386_THREAD_STATE and i386_REGS_SEGS_STATE.
 */
struct i386_new_thread_state {
	unsigned int	gs;
	unsigned int	fs;
	unsigned int	es;
	unsigned int	ds;
	unsigned int	edi;
	unsigned int	esi;
	unsigned int	ebp;
	unsigned int	esp;
	unsigned int	ebx;
	unsigned int	edx;
	unsigned int	ecx;
	unsigned int	eax;
	unsigned int	eip;
	unsigned int	cs;
	unsigned int	efl;
	unsigned int	uesp;
	unsigned int	ss;
};
#define i386_NEW_THREAD_STATE_COUNT	\
		(sizeof (struct i386_new_thread_state)/sizeof(unsigned int))

/*
 * Subset of saved state stored by processor on kernel-to-kernel
 * trap.  (Used by ddb to examine state guaranteed to be present
 * on all traps into debugger.)
 */
struct i386_saved_state_from_kernel {
	unsigned int	gs;
	unsigned int	fs;
	unsigned int	es;
	unsigned int	ds;
	unsigned int	edi;
	unsigned int	esi;
	unsigned int	ebp;
	unsigned int	esp;		/* kernel esp stored by pusha -
					   we save cr2 here later */
	unsigned int	ebx;
	unsigned int	edx;
	unsigned int	ecx;
	unsigned int	eax;
	unsigned int	trapno;
	unsigned int	err;
	unsigned int	eip;
	unsigned int	cs;
	unsigned int	efl;
};

/*
 * The format in which thread state is saved by Mach on this machine.  This
 * state flavor is most efficient for exception RPC's to kernel-loaded
 * servers, because copying can be avoided:
 */
struct i386_saved_state {
	unsigned int	gs;
	unsigned int	fs;
	unsigned int	es;
	unsigned int	ds;
	unsigned int	edi;
	unsigned int	esi;
	unsigned int	ebp;
	unsigned int	esp;		/* kernel esp stored by pusha -
					   we save cr2 here later */
	unsigned int	ebx;
	unsigned int	edx;
	unsigned int	ecx;
	unsigned int	eax;
	unsigned int	trapno;
	unsigned int	err;
	unsigned int	eip;
	unsigned int	cs;
	unsigned int	efl;
	unsigned int	uesp;
	unsigned int	ss;
	struct v86_segs {
	    unsigned int v86_es;	/* virtual 8086 segment registers */
	    unsigned int v86_ds;
	    unsigned int v86_fs;
	    unsigned int v86_gs;
	} v86_segs;
#define i386_SAVED_ARGV_COUNT	7
	unsigned int	argv_status;	/* Boolean flag indicating whether or
					 * not Mach copied in the args */
	unsigned int	argv[i386_SAVED_ARGV_COUNT];
					/* The return address, and the first several
					 * function call args from the stack, for
					 * efficient syscall exceptions */
};
#define i386_SAVED_STATE_COUNT	(sizeof (struct i386_saved_state)/sizeof(unsigned int))
#define i386_REGS_SEGS_STATE_COUNT	i386_SAVED_STATE_COUNT

/*
 * Machine-independent way for servers and Mach's exception mechanism to
 * choose the most efficient state flavor for exception RPC's:
 */
#define MACHINE_THREAD_STATE		i386_SAVED_STATE
#define MACHINE_THREAD_STATE_COUNT	i386_SAVED_STATE_COUNT

/*
 * Largest state on this machine:
 * (be sure mach/machine/thread_state.h matches!)
 */
#define THREAD_MACHINE_STATE_MAX	i386_SAVED_STATE_COUNT

/* 
 * Floating point state.
 *
 * fpkind tells in what way floating point operations are supported.  
 * See the values for fp_kind in <mach/i386/fp_reg.h>.
 * 
 * If the kind is FP_NO, then calls to set the state will fail, and 
 * thread_getstatus will return garbage for the rest of the state.
 * If "initialized" is false, then the rest of the state is garbage.  
 * Clients can set "initialized" to false to force the coprocessor to 
 * be reset.
 * "exc_status" is non-zero if the thread has noticed (but not 
 * proceeded from) a coprocessor exception.  It contains the status 
 * word with the exception bits set.  The status word in "fp_status" 
 * will have the exception bits turned off.  If an exception bit in 
 * "fp_status" is turned on, then "exc_status" should be zero.  This 
 * happens when the coprocessor exception is noticed after the system 
 * has context switched to some other thread.
 * 
 * If kind is FP_387, then "state" is a i387_state.  Other kinds might
 * also use i387_state, but somebody will have to verify it (XXX).
 * Note that the registers are ordered from top-of-stack down, not
 * according to physical register number.
 */

#define FP_STATE_BYTES \
	(sizeof (struct i386_fp_save) + sizeof (struct i386_fp_regs))

struct i386_float_state {
	int		fpkind;			/* FP_NO..FP_387 (readonly) */
	int		initialized;
	unsigned char	hw_state[FP_STATE_BYTES]; /* actual "hardware" state */
	int		exc_status;		/* exception status (readonly) */
};
#define i386_FLOAT_STATE_COUNT \
		(sizeof(struct i386_float_state)/sizeof(unsigned int))


#define PORT_MAP_BITS 0x400
struct i386_isa_port_map_state {
	unsigned char	pm[PORT_MAP_BITS>>3];
};

#define i386_ISA_PORT_MAP_STATE_COUNT \
		(sizeof(struct i386_isa_port_map_state)/sizeof(unsigned int))

/*
 * V8086 assist supplies a pointer to an interrupt
 * descriptor table in task space.
 */
struct i386_v86_assist_state {
	unsigned int	int_table;	/* interrupt table address */
	int		int_count;	/* interrupt table size */
};

struct v86_interrupt_table {
	unsigned int	count;	/* count of pending interrupts */
	unsigned short	mask;	/* ignore this interrupt if true */
	unsigned short	vec;	/* vector to take */
};

#define	i386_V86_ASSIST_STATE_COUNT \
	    (sizeof(struct i386_v86_assist_state)/sizeof(unsigned int))

struct thread_syscall_state {
	unsigned eax;
	unsigned edx;
	unsigned efl;
	unsigned eip;
	unsigned esp;
};

#define i386_THREAD_SYSCALL_STATE_COUNT \
		(sizeof(struct thread_syscall_state) / sizeof(unsigned int))

/*
 * Main thread state consists of
 * general registers, segment registers,
 * eip and eflags.
 */

#define i386_THREAD_STATE	-1

typedef struct {
    unsigned int	eax;
    unsigned int	ebx;
    unsigned int	ecx;
    unsigned int	edx;
    unsigned int	edi;
    unsigned int	esi;
    unsigned int	ebp;
    unsigned int	esp;
    unsigned int	ss;
    unsigned int	eflags;
    unsigned int	eip;
    unsigned int	cs;
    unsigned int	ds;
    unsigned int	es;
    unsigned int	fs;
    unsigned int	gs;
} i386_thread_state_t;

#define i386_THREAD_STATE_COUNT		\
    ( sizeof (i386_thread_state_t) / sizeof (int) )

/*
 * Default segment register values.
 */
    
#define USER_CODE_SELECTOR	0x0017
#define USER_DATA_SELECTOR	0x001f
#define KERN_CODE_SELECTOR	0x0008
#define KERN_DATA_SELECTOR	0x0010

/*
 * Thread floating point state
 * includes FPU environment as
 * well as the register stack.
 */
 
#define i386_THREAD_FPSTATE	-2

typedef struct {
    fp_env_t		environ;
    fp_stack_t		stack;
} i386_thread_fpstate_t;

#define i386_THREAD_FPSTATE_COUNT 	\
    ( sizeof (i386_thread_fpstate_t) / sizeof (int) )

/*
 * Extra state that may be
 * useful to exception handlers.
 */

#define i386_THREAD_EXCEPTSTATE	-3

typedef struct {
    unsigned int	trapno;
    err_code_t		err;
} i386_thread_exceptstate_t;

#define i386_THREAD_EXCEPTSTATE_COUNT	\
    ( sizeof (i386_thread_exceptstate_t) / sizeof (int) )

/*
 * Per-thread variable used
 * to store 'self' id for cthreads.
 */
 
#define i386_THREAD_CTHREADSTATE	-4
 
typedef struct {
    unsigned int	self;
} i386_thread_cthreadstate_t;

#define i386_THREAD_CTHREADSTATE_COUNT	\
    ( sizeof (i386_thread_cthreadstate_t) / sizeof (int) )

#endif	/* _MACH_I386_THREAD_STATUS_H_ */
