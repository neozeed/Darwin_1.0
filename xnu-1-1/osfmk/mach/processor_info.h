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
 * Revision 1.1.1.1  1998/09/22 21:05:30  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:46  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.2.15.4  1996/01/09  19:22:17  devrcs
 * 	Changes PROCESSOR_SET_*_COUNT to be in sizeof natural_t  units
 * 	instead of sizeof int .  This agrees with mach_types.defs.
 * 	[1995/12/01  19:49:29  jfraser]
 *
 * 	Merged '64-bit safe' changes from DEC alpha port.
 * 	[1995/11/21  18:09:26  jfraser]
 *
 * Revision 1.2.15.3  1995/01/06  19:51:34  devrcs
 * 	mk6 CR668 - 1.3b26 merge
 * 	64bit changes, typedefs
 * 	[1994/10/14  03:43:07  dwm]
 * 
 * Revision 1.2.15.2  1994/09/23  02:41:50  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:42:27  ezf]
 * 
 * Revision 1.2.15.1  1994/08/07  20:49:52  bolinger
 * 	Merge up to colo_b7.
 * 	[1994/08/01  21:01:59  bolinger]
 * 
 * Revision 1.2.12.3  1994/06/25  03:47:17  dwm
 * 	mk6 CR98 - add flavor interface typedefs (processor(_set)_flavor_t).
 * 	[1994/06/24  21:54:55  dwm]
 * 
 * Revision 1.2.12.2  1994/05/02  21:36:00  dwm
 * 	Remove nmk15_compat support.
 * 	[1994/05/02  21:09:07  dwm]
 * 
 * Revision 1.2.12.1  1994/01/12  17:57:22  dwm
 * 	Fix "ifdef" NMK15_COMPAT to "if"
 * 	[1994/01/12  17:31:11  dwm]
 * 
 * Revision 1.2.3.6  1993/08/03  18:29:48  gm
 * 	CR9596: Change KERNEL to MACH_KERNEL.
 * 	[1993/08/02  18:30:33  gm]
 * 
 * Revision 1.2.3.5  1993/07/08  19:04:50  watkins
 * 	Change contents of struct processor_set_basic_info and add
 * 	struct processor_set_load_info for spec conformance.
 * 	[1993/07/07  21:04:07  watkins]
 * 
 * Revision 1.2.3.4  1993/06/29  21:55:48  watkins
 * 	New defines for scheduling control.
 * 	[1993/06/29  20:50:51  watkins]
 * 
 * Revision 1.2.3.3  1993/06/29  17:54:42  brezak
 * 	Add type for processor_slot_t.
 * 	[1993/06/28  20:59:17  brezak]
 * 
 * Revision 1.2.3.2  1993/06/09  02:43:05  gm
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  21:17:58  jeffc]
 * 
 * Revision 1.2  1993/04/19  16:38:44  devrcs
 * 	ansi C conformance changes
 * 	[1993/02/02  18:54:34  david]
 * 
 * Revision 1.1  1992/09/30  02:32:02  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.4  91/05/14  16:58:46  mrt
 * 	Correcting copyright
 * 
 * Revision 2.3  91/02/05  17:35:31  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:20:39  mrt]
 * 
 * Revision 2.2  90/06/02  14:59:49  rpd
 * 	Created for new host/processor technology.
 * 	[90/03/26  23:51:38  rpd]
 * 
 * 	Merge to X96
 * 	[89/08/02  23:12:21  dlb]
 * 
 * 	Add scheduling flavor of information.
 * 	[89/07/25  18:52:18  dlb]
 * 
 * 	Add load average and mach factor to processor set basic info.
 * 	[89/02/09            dlb]
 * 
 * Revision 2.3  89/10/15  02:05:54  rpd
 * 	Minor cleanups.
 * 
 * Revision 2.2  89/10/11  14:41:03  dlb
 * 	Add scheduling flavor of information.
 * 
 * 	Add load average and mach factor to processor set basic info.
 * 	[89/02/09            dlb]
 * 
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
 *	File:	mach/processor_info.h
 *	Author:	David L. Black
 *	Date:	1988
 *
 *	Data structure definitions for processor_info, processor_set_info
 */

#ifndef	_MACH_PROCESSOR_INFO_H_
#define _MACH_PROCESSOR_INFO_H_

#include <mach/machine.h>
#include <mach/machine/processor_info.h>

/*
 *	Generic information structure to allow for expansion.
 */
typedef integer_t	*processor_info_t;	/* varying array of int. */

#define PROCESSOR_INFO_MAX	(1024)	/* max array size */
typedef integer_t	processor_info_data_t[PROCESSOR_INFO_MAX];


typedef integer_t	*processor_set_info_t;	/* varying array of int. */

#define PROCESSOR_SET_INFO_MAX	(1024)	/* max array size */
typedef integer_t	processor_set_info_data_t[PROCESSOR_SET_INFO_MAX];


typedef int	*processor_slot_t;	/* varying array of int. */

/*
 *	Currently defined information.
 */
typedef int	processor_flavor_t;
#define	PROCESSOR_BASIC_INFO	1		/* basic information */
#define	PROCESSOR_CPU_LOAD_INFO	2	/* cpu load information */
#define	PROCESSOR_PM_REGS_INFO	0x10000001	/* performance monitor register info */

struct processor_basic_info {
	cpu_type_t	cpu_type;	/* type of cpu */
	cpu_subtype_t	cpu_subtype;	/* subtype of cpu */
	boolean_t	running;	/* is processor running */
	int		slot_num;	/* slot number */
	boolean_t	is_master;	/* is this the master processor */
};

typedef	struct processor_basic_info	processor_basic_info_data_t;
typedef struct processor_basic_info	*processor_basic_info_t;
#define PROCESSOR_BASIC_INFO_COUNT \
		(sizeof(processor_basic_info_data_t)/sizeof(natural_t))

struct processor_cpu_load_info {             /* number of ticks while running... */
        unsigned long   cpu_ticks[CPU_STATE_MAX]; /* ... in the given mode */
}; 

typedef	struct processor_cpu_load_info	processor_cpu_load_info_data_t;
typedef struct processor_cpu_load_info	*processor_cpu_load_info_t;
#define PROCESSOR_CPU_LOAD_INFO_COUNT \
		(sizeof(processor_cpu_load_info_data_t)/sizeof(natural_t))

/*
 *	Scaling factor for load_average, mach_factor.
 */
#define	LOAD_SCALE	1000		

typedef	int	processor_set_flavor_t;
#define	PROCESSOR_SET_BASIC_INFO	5	/* basic information */

struct processor_set_basic_info {
	int		processor_count;	/* How many processors */
	int		default_policy;		/* When others not enabled */
};

typedef	struct processor_set_basic_info	processor_set_basic_info_data_t;
typedef struct processor_set_basic_info	*processor_set_basic_info_t;
#define PROCESSOR_SET_BASIC_INFO_COUNT \
		(sizeof(processor_set_basic_info_data_t)/sizeof(natural_t))

#define PROCESSOR_SET_LOAD_INFO		4	/* scheduling statistics */

struct processor_set_load_info {
        int             task_count;             /* How many tasks */
        int             thread_count;           /* How many threads */
        integer_t       load_average;           /* Scaled */
        integer_t       mach_factor;            /* Scaled */
};

typedef struct processor_set_load_info processor_set_load_info_data_t;
typedef struct processor_set_load_info *processor_set_load_info_t;
#define PROCESSOR_SET_LOAD_INFO_COUNT	\
                (sizeof(processor_set_load_info_data_t)/sizeof(natural_t))


/*
 *      New scheduling control interface
 */
#define PROCESSOR_SET_ENABLED_POLICIES                   3
#define PROCESSOR_SET_ENABLED_POLICIES_COUNT	\
		(sizeof(policy_t)/sizeof(natural_t))

#define PROCESSOR_SET_TIMESHARE_DEFAULT                 10
#define PROCESSOR_SET_TIMESHARE_LIMITS                  11

#define PROCESSOR_SET_RR_DEFAULT                        20
#define PROCESSOR_SET_RR_LIMITS                         21

#define PROCESSOR_SET_FIFO_DEFAULT                      30
#define PROCESSOR_SET_FIFO_LIMITS                       31

#endif	/* _MACH_PROCESSOR_INFO_H_ */
