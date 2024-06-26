/* Functions specific to running gdb native on IA64 running Linux.
   Copyright 1999 Free Software Foundation, Inc.

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

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif
#include <sys/user.h>

#include <asm/ptrace_offsets.h>
#include <sys/procfs.h>

/* These must match the order of the register names.

   Some sort of lookup table is needed because the offsets associated
   with the registers are all over the board.  */

static int u_offsets[] =
  {
    /* general registers */
    -1,		/* gr0 not available; i.e, it's always zero */
    PT_R1,
    PT_R2,
    PT_R3,
    PT_R4,
    PT_R5,
    PT_R6,
    PT_R7,
    PT_R8,
    PT_R9,
    PT_R10,
    PT_R11,
    PT_R12,
    PT_R13,
    PT_R14,
    PT_R15,
    PT_R16,
    PT_R17,
    PT_R18,
    PT_R19,
    PT_R20,
    PT_R21,
    PT_R22,
    PT_R23,
    PT_R24,
    PT_R25,
    PT_R26,
    PT_R27,
    PT_R28,
    PT_R29,
    PT_R30,
    PT_R31,
    /* gr32 through gr127 not directly available via the ptrace interface */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* Floating point registers */
    -1, -1,	/* f0 and f1 not available (f0 is +0.0 and f1 is +1.0) */
    PT_F2,
    PT_F3,
    PT_F4,
    PT_F5,
    PT_F6,
    PT_F7,
    PT_F8,
    PT_F9,
    PT_F10,
    PT_F11,
    PT_F12,
    PT_F13,
    PT_F14,
    PT_F15,
    PT_F16,
    PT_F17,
    PT_F18,
    PT_F19,
    PT_F20,
    PT_F21,
    PT_F22,
    PT_F23,
    PT_F24,
    PT_F25,
    PT_F26,
    PT_F27,
    PT_F28,
    PT_F29,
    PT_F30,
    PT_F31,
    PT_F32,
    PT_F33,
    PT_F34,
    PT_F35,
    PT_F36,
    PT_F37,
    PT_F38,
    PT_F39,
    PT_F40,
    PT_F41,
    PT_F42,
    PT_F43,
    PT_F44,
    PT_F45,
    PT_F46,
    PT_F47,
    PT_F48,
    PT_F49,
    PT_F50,
    PT_F51,
    PT_F52,
    PT_F53,
    PT_F54,
    PT_F55,
    PT_F56,
    PT_F57,
    PT_F58,
    PT_F59,
    PT_F60,
    PT_F61,
    PT_F62,
    PT_F63,
    PT_F64,
    PT_F65,
    PT_F66,
    PT_F67,
    PT_F68,
    PT_F69,
    PT_F70,
    PT_F71,
    PT_F72,
    PT_F73,
    PT_F74,
    PT_F75,
    PT_F76,
    PT_F77,
    PT_F78,
    PT_F79,
    PT_F80,
    PT_F81,
    PT_F82,
    PT_F83,
    PT_F84,
    PT_F85,
    PT_F86,
    PT_F87,
    PT_F88,
    PT_F89,
    PT_F90,
    PT_F91,
    PT_F92,
    PT_F93,
    PT_F94,
    PT_F95,
    PT_F96,
    PT_F97,
    PT_F98,
    PT_F99,
    PT_F100,
    PT_F101,
    PT_F102,
    PT_F103,
    PT_F104,
    PT_F105,
    PT_F106,
    PT_F107,
    PT_F108,
    PT_F109,
    PT_F110,
    PT_F111,
    PT_F112,
    PT_F113,
    PT_F114,
    PT_F115,
    PT_F116,
    PT_F117,
    PT_F118,
    PT_F119,
    PT_F120,
    PT_F121,
    PT_F122,
    PT_F123,
    PT_F124,
    PT_F125,
    PT_F126,
    PT_F127,
    /* predicate registers - we don't fetch these individually */
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    /* branch registers */
    PT_B0,
    PT_B1,
    PT_B2,
    PT_B3,
    PT_B4,
    PT_B5,
    PT_B6,
    PT_B7,
    /* virtual frame pointer and virtual return address pointer */
    -1, -1,
    /* other registers */
    PT_PR,
    PT_CR_IIP,	/* ip */
    PT_CR_IPSR, /* psr */
    PT_CR_IFS,	/* cfm */
    /* kernel registers not visible via ptrace interface (?) */
    -1, -1, -1, -1, -1, -1, -1, -1,
    /* hole */
    -1, -1, -1, -1, -1, -1, -1, -1,
    PT_AR_RSC,
    PT_AR_BSP,
    PT_AR_BSPSTORE,
    PT_AR_RNAT,
    -1,
    -1,		/* Not available: FCR, IA32 floating control register */
    -1, -1,
    -1,		/* Not available: EFLAG */
    -1,		/* Not available: CSD */
    -1,		/* Not available: SSD */
    -1,		/* Not available: CFLG */
    -1,		/* Not available: FSR */
    -1,		/* Not available: FIR */
    -1,		/* Not available: FDR */
    -1,
    PT_AR_CCV,
    -1, -1, -1,
    PT_AR_UNAT,
    -1, -1, -1,
    PT_AR_FPSR,
    -1, -1, -1,
    -1,		/* Not available: ITC */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1,
    PT_AR_PFS,
    PT_AR_LC,
    -1,		/* Not available: EC, the Epilog Count register */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1,
    /* nat bits - not fetched directly; instead we obtain these bits from
       either rnat or unat or from memory. */
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
  };

CORE_ADDR
register_addr (regno, blockend)
     int regno;
     CORE_ADDR blockend;
{
  CORE_ADDR addr;

  if (regno < 0 || regno >= NUM_REGS)
    error ("Invalid register number %d.", regno);

  if (u_offsets[regno] == -1)
    addr = 0;
  else
    addr = (CORE_ADDR) u_offsets[regno];

  return addr;
}

int ia64_cannot_fetch_register (regno)
     int regno;
{
  return regno < 0 || regno >= NUM_REGS || u_offsets[regno] == -1;
}

int ia64_cannot_store_register (regno)
     int regno;
{
  /* Rationale behind not permitting stores to bspstore...
  
     The IA-64 architecture provides bspstore and bsp which refer
     memory locations in the RSE's backing store.  bspstore is the
     next location which will be written when the RSE needs to write
     to memory.  bsp is the address at which r32 in the current frame
     would be found if it were written to the backing store.

     The IA-64 architecture provides read-only access to bsp and
     read/write access to bspstore (but only when the RSE is in
     the enforced lazy mode).  It should be noted that stores
     to bspstore also affect the value of bsp.  Changing bspstore
     does not affect the number of dirty entries between bspstore
     and bsp, so changing bspstore by N words will also cause bsp
     to be changed by (roughly) N as well.  (It could be N-1 or N+1
     depending upon where the NaT collection bits fall.)

     OTOH, the linux kernel provides read/write access to bsp (and
     currently read/write access to bspstore as well).  But it
     is definitely the case that if you change one, the other
     will change at the same time.  It is more useful to gdb to
     be able to change bsp.  So in order to prevent strange and
     undesirable things from happening when a dummy stack frame
     is popped (after calling an inferior function), we allow
     bspstore to be read, but not written.  (Note that popping
     a (generic) dummy stack frame causes all registers that
     were previously read from the inferior process to be written
     back.)  */

  return regno < 0 || regno >= NUM_REGS || u_offsets[regno] == -1
         || regno == IA64_BSPSTORE_REGNUM;
}

void
supply_gregset (gregsetp)
     gregset_t *gregsetp;
{
  int regi;
  greg_t *regp = (greg_t *) gregsetp;

  for (regi = IA64_GR0_REGNUM; regi <= IA64_GR31_REGNUM; regi++)
    {
      supply_register (regi, (char *) (regp + (regi - IA64_GR0_REGNUM)));
    }

  /* FIXME: NAT collection bits are at index 32; gotta deal with these
     somehow... */

  supply_register (IA64_PR_REGNUM, (char *) (regp + 33));

  for (regi = IA64_BR0_REGNUM; regi <= IA64_BR7_REGNUM; regi++)
    {
      supply_register (regi, (char *) (regp + 34 + (regi - IA64_BR0_REGNUM)));
    }

  supply_register (IA64_IP_REGNUM, (char *) (regp + 42));
  supply_register (IA64_CFM_REGNUM, (char *) (regp + 43));
  supply_register (IA64_PSR_REGNUM, (char *) (regp + 44));
  supply_register (IA64_RSC_REGNUM, (char *) (regp + 45));
  supply_register (IA64_BSP_REGNUM, (char *) (regp + 46));
  supply_register (IA64_BSPSTORE_REGNUM, (char *) (regp + 47));
  supply_register (IA64_RNAT_REGNUM, (char *) (regp + 48));
  supply_register (IA64_CCV_REGNUM, (char *) (regp + 49));
  supply_register (IA64_UNAT_REGNUM, (char *) (regp + 50));
  supply_register (IA64_FPSR_REGNUM, (char *) (regp + 51));
  supply_register (IA64_PFS_REGNUM, (char *) (regp + 52));
  supply_register (IA64_LC_REGNUM, (char *) (regp + 53));
  supply_register (IA64_EC_REGNUM, (char *) (regp + 54));
}

void
fill_gregset (gregsetp, regno)
     gregset_t *gregsetp;
     int regno;
{
  fprintf(stderr, "Warning: fill_gregset not implemented!\n");
  /* FIXME: Implement later */
}
