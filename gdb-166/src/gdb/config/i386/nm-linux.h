/* Native support for GNU/Linux, for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1989, 1992, 1996, 1998
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef NM_LINUX_H
#define NM_LINUX_H

#include "i386/nm-i386v.h"
#include "nm-linux.h"

#include <sys/ptrace.h>
#include <asm/ptrace.h>

/* Return sizeof user struct to callers in less machine dependent routines */

#define KERNEL_U_SIZE kernel_u_size()
extern int kernel_u_size PARAMS ((void));

#define U_REGS_OFFSET 0

/* GNU/Linux supports the 386 hardware debugging registers.  */

#define TARGET_HAS_HARDWARE_WATCHPOINTS

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) 1

/* After a watchpoint trap, the PC points to the instruction after
   the one that caused the trap.  Therefore we don't need to step over it.
   But we do need to reset the status register to avoid another trap.  */
#define HAVE_CONTINUABLE_WATCHPOINT

#define STOPPED_BY_WATCHPOINT(W)  \
  i386_stopped_by_watchpoint (inferior_pid)

/* Use these macros for watchpoint insertion/removal.  */

#define target_insert_watchpoint(addr, len, type)  \
  i386_insert_watchpoint (inferior_pid, addr, len, type)

#define target_remove_watchpoint(addr, len, type)  \
  i386_remove_watchpoint (inferior_pid, addr, len)

/* We define this if link.h is available, because with ELF we use SVR4 style
   shared libraries. */

#ifdef HAVE_LINK_H
#define SVR4_SHARED_LIBS
#include "solib.h"		/* Support for shared libraries. */
#endif

/* Override copies of {fetch,store}_inferior_registers in infptrace.c.  */
#define FETCH_INFERIOR_REGISTERS

extern CORE_ADDR
  i386_stopped_by_watchpoint PARAMS ((int));
extern int
i386_insert_watchpoint PARAMS ((int pid, CORE_ADDR addr, int len, int rw));
extern int
i386_remove_watchpoint PARAMS ((int pid, CORE_ADDR addr, int len));

#endif /* #ifndef NM_LINUX_H */
