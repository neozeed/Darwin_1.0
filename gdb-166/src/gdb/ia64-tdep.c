/* Target-dependent code for the IA-64 for GDB, the GNU debugger.
   Copyright 1999, 2000
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

#include "defs.h"
#include "inferior.h"
#include "symfile.h"		/* for entry_point_address */
#include "gdbcore.h"
#include "floatformat.h"

#include "objfiles.h"
#include "elf/common.h"		/* for DT_PLTGOT value */

typedef enum instruction_type
{
  A,			/* Integer ALU ;    I-unit or M-unit */
  I,			/* Non-ALU integer; I-unit */
  M,			/* Memory ;         M-unit */
  F,			/* Floating-point ; F-unit */
  B,			/* Branch ;         B-unit */
  L,			/* Extended (L+X) ; I-unit */
  X,			/* Extended (L+X) ; I-unit */
  undefined		/* undefined or reserved */
} instruction_type;

/* We represent IA-64 PC addresses as the value of the instruction
   pointer or'd with some bit combination in the low nibble which
   represents the slot number in the bundle addressed by the
   instruction pointer.  The problem is that the Linux kernel
   multiplies its slot numbers (for exceptions) by one while the
   disassembler multiplies its slot numbers by 6.  In addition, I've
   heard it said that the simulator uses 1 as the multiplier.
   
   I've fixed the disassembler so that the bytes_per_line field will
   be the slot multiplier.  If bytes_per_line comes in as zero, it
   is set to six (which is how it was set up initially). -- objdump
   displays pretty disassembly dumps with this value.  For our purposes,
   we'll set bytes_per_line to SLOT_MULTIPLIER. This is okay since we
   never want to also display the raw bytes the way objdump does. */

#define SLOT_MULTIPLIER 1

/* Length in bytes of an instruction bundle */

#define BUNDLE_LEN 16

extern void _initialize_ia64_tdep (void);

static gdbarch_init_ftype ia64_gdbarch_init;

static gdbarch_register_name_ftype ia64_register_name;
static gdbarch_register_raw_size_ftype ia64_register_raw_size;
static gdbarch_register_virtual_size_ftype ia64_register_virtual_size;
static gdbarch_register_virtual_type_ftype ia64_register_virtual_type;
static gdbarch_register_byte_ftype ia64_register_byte;
static gdbarch_breakpoint_from_pc_ftype ia64_breakpoint_from_pc;
static gdbarch_frame_chain_ftype ia64_frame_chain;
static gdbarch_frame_saved_pc_ftype ia64_frame_saved_pc;
static gdbarch_skip_prologue_ftype ia64_skip_prologue;
static gdbarch_frame_init_saved_regs_ftype ia64_frame_init_saved_regs;
static gdbarch_get_saved_register_ftype ia64_get_saved_register;
static gdbarch_extract_return_value_ftype ia64_extract_return_value;
static gdbarch_extract_struct_value_address_ftype ia64_extract_struct_value_address;
static gdbarch_use_struct_convention_ftype ia64_use_struct_convention;
static gdbarch_frameless_function_invocation_ftype ia64_frameless_function_invocation;
static gdbarch_init_extra_frame_info_ftype ia64_init_extra_frame_info;
static gdbarch_store_return_value_ftype ia64_store_return_value;
static gdbarch_store_struct_return_ftype ia64_store_struct_return;
static gdbarch_push_arguments_ftype ia64_push_arguments;
static gdbarch_push_return_address_ftype ia64_push_return_address;
static gdbarch_pop_frame_ftype ia64_pop_frame;
static gdbarch_saved_pc_after_call_ftype ia64_saved_pc_after_call;

static void ia64_pop_frame_regular (struct frame_info *frame);

static int ia64_num_regs = 590;

static int pc_regnum = IA64_IP_REGNUM;
static int sp_regnum = IA64_GR12_REGNUM;
static int fp_regnum = IA64_VFP_REGNUM;
static int lr_regnum = IA64_VRAP_REGNUM;

static LONGEST ia64_call_dummy_words[] = {0};

/* Array of register names; There should be ia64_num_regs strings in
   the initializer.  */

static char *ia64_register_names[] = 
{ "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
  "r16",  "r17",  "r18",  "r19",  "r20",  "r21",  "r22",  "r23",
  "r24",  "r25",  "r26",  "r27",  "r28",  "r29",  "r30",  "r31",
  "r32",  "r33",  "r34",  "r35",  "r36",  "r37",  "r38",  "r39",
  "r40",  "r41",  "r42",  "r43",  "r44",  "r45",  "r46",  "r47",
  "r48",  "r49",  "r50",  "r51",  "r52",  "r53",  "r54",  "r55",
  "r56",  "r57",  "r58",  "r59",  "r60",  "r61",  "r62",  "r63",
  "r64",  "r65",  "r66",  "r67",  "r68",  "r69",  "r70",  "r71",
  "r72",  "r73",  "r74",  "r75",  "r76",  "r77",  "r78",  "r79",
  "r80",  "r81",  "r82",  "r83",  "r84",  "r85",  "r86",  "r87",
  "r88",  "r89",  "r90",  "r91",  "r92",  "r93",  "r94",  "r95",
  "r96",  "r97",  "r98",  "r99",  "r100", "r101", "r102", "r103",
  "r104", "r105", "r106", "r107", "r108", "r109", "r110", "r111",
  "r112", "r113", "r114", "r115", "r116", "r117", "r118", "r119",
  "r120", "r121", "r122", "r123", "r124", "r125", "r126", "r127",

  "f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7",
  "f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15",
  "f16",  "f17",  "f18",  "f19",  "f20",  "f21",  "f22",  "f23",
  "f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31",
  "f32",  "f33",  "f34",  "f35",  "f36",  "f37",  "f38",  "f39",
  "f40",  "f41",  "f42",  "f43",  "f44",  "f45",  "f46",  "f47",
  "f48",  "f49",  "f50",  "f51",  "f52",  "f53",  "f54",  "f55",
  "f56",  "f57",  "f58",  "f59",  "f60",  "f61",  "f62",  "f63",
  "f64",  "f65",  "f66",  "f67",  "f68",  "f69",  "f70",  "f71",
  "f72",  "f73",  "f74",  "f75",  "f76",  "f77",  "f78",  "f79",
  "f80",  "f81",  "f82",  "f83",  "f84",  "f85",  "f86",  "f87",
  "f88",  "f89",  "f90",  "f91",  "f92",  "f93",  "f94",  "f95",
  "f96",  "f97",  "f98",  "f99",  "f100", "f101", "f102", "f103",
  "f104", "f105", "f106", "f107", "f108", "f109", "f110", "f111",
  "f112", "f113", "f114", "f115", "f116", "f117", "f118", "f119",
  "f120", "f121", "f122", "f123", "f124", "f125", "f126", "f127",

  "p0",   "p1",   "p2",   "p3",   "p4",   "p5",   "p6",   "p7",
  "p8",   "p9",   "p10",  "p11",  "p12",  "p13",  "p14",  "p15",
  "p16",  "p17",  "p18",  "p19",  "p20",  "p21",  "p22",  "p23",
  "p24",  "p25",  "p26",  "p27",  "p28",  "p29",  "p30",  "p31",
  "p32",  "p33",  "p34",  "p35",  "p36",  "p37",  "p38",  "p39",
  "p40",  "p41",  "p42",  "p43",  "p44",  "p45",  "p46",  "p47",
  "p48",  "p49",  "p50",  "p51",  "p52",  "p53",  "p54",  "p55",
  "p56",  "p57",  "p58",  "p59",  "p60",  "p61",  "p62",  "p63",

  "b0",   "b1",   "b2",   "b3",   "b4",   "b5",   "b6",   "b7",

  "vfp", "vrap",

  "pr", "ip", "psr", "cfm",

  "kr0",   "kr1",   "kr2",   "kr3",   "kr4",   "kr5",   "kr6",   "kr7",
  "", "", "", "", "", "", "", "",
  "rsc", "bsp", "bspstore", "rnat",
  "", "fcr", "", "",
  "eflag", "csd", "ssd", "cflg", "fsr", "fir", "fdr",  "",
  "ccv", "", "", "", "unat", "", "", "",
  "fpsr", "", "", "", "itc",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "",
  "pfs", "lc", "ec",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "",
  "nat0",  "nat1",  "nat2",  "nat3",  "nat4",  "nat5",  "nat6",  "nat7",
  "nat8",  "nat9",  "nat10", "nat11", "nat12", "nat13", "nat14", "nat15",
  "nat16", "nat17", "nat18", "nat19", "nat20", "nat21", "nat22", "nat23",
  "nat24", "nat25", "nat26", "nat27", "nat28", "nat29", "nat30", "nat31",
  "nat32", "nat33", "nat34", "nat35", "nat36", "nat37", "nat38", "nat39",
  "nat40", "nat41", "nat42", "nat43", "nat44", "nat45", "nat46", "nat47",
  "nat48", "nat49", "nat50", "nat51", "nat52", "nat53", "nat54", "nat55",
  "nat56", "nat57", "nat58", "nat59", "nat60", "nat61", "nat62", "nat63",
  "nat64", "nat65", "nat66", "nat67", "nat68", "nat69", "nat70", "nat71",
  "nat72", "nat73", "nat74", "nat75", "nat76", "nat77", "nat78", "nat79",
  "nat80", "nat81", "nat82", "nat83", "nat84", "nat85", "nat86", "nat87",
  "nat88", "nat89", "nat90", "nat91", "nat92", "nat93", "nat94", "nat95",
  "nat96", "nat97", "nat98", "nat99", "nat100","nat101","nat102","nat103",
  "nat104","nat105","nat106","nat107","nat108","nat109","nat110","nat111",
  "nat112","nat113","nat114","nat115","nat116","nat117","nat118","nat119",
  "nat120","nat121","nat122","nat123","nat124","nat125","nat126","nat127",
};

struct frame_extra_info
{
  CORE_ADDR bsp;	/* points at r32 for the current frame */
  CORE_ADDR cfm;	/* cfm value for current frame */
  int       sof;	/* Size of frame  (decoded from cfm value) */
  int	    sol;	/* Size of locals (decoded from cfm value) */
  CORE_ADDR after_prologue;
  			/* Address of first instruction after the last
			   prologue instruction;  Note that there may
			   be instructions from the function's body
			   intermingled with the prologue. */
  int       mem_stack_frame_size;
  			/* Size of the memory stack frame (may be zero),
			   or -1 if it has not been determined yet. */
  int	    fp_reg;	/* Register number (if any) used a frame pointer
                           for this frame.  0 if no register is being used
			   as the frame pointer. */
};

static char *
ia64_register_name (int reg)
{
  return ia64_register_names[reg];
}

int
ia64_register_raw_size (int reg)
{
  return (IA64_FR0_REGNUM <= reg && reg <= IA64_FR127_REGNUM) ? 16 : 8;
}

int
ia64_register_virtual_size (int reg)
{
  return (IA64_FR0_REGNUM <= reg && reg <= IA64_FR127_REGNUM) ? 16 : 8;
}

/* Return true iff register N's virtual format is different from
   its raw format. */
int
ia64_register_convertible (int nr)
{
  return (IA64_FR0_REGNUM <= nr && nr <= IA64_FR127_REGNUM);
}

const struct floatformat floatformat_ia64_ext =
{
  floatformat_little, 82, 0, 1, 17, 65535, 0x1ffff, 18, 64,
  floatformat_intbit_yes
};

void
ia64_register_convert_to_virtual (int regnum, struct type *type,
                                  char *from, char *to)
{
  if (regnum >= IA64_FR0_REGNUM && regnum <= IA64_FR127_REGNUM)
    {
      DOUBLEST val;
      floatformat_to_doublest (&floatformat_ia64_ext, from, &val);
      store_floating(to, TYPE_LENGTH(type), val);
    }
  else
    error("ia64_register_convert_to_virtual called with non floating point register number");
}

void
ia64_register_convert_to_raw (struct type *type, int regnum,
                              char *from, char *to)
{
  if (regnum >= IA64_FR0_REGNUM && regnum <= IA64_FR127_REGNUM)
    {
      DOUBLEST val = extract_floating (from, TYPE_LENGTH(type));
      floatformat_from_doublest (&floatformat_ia64_ext, &val, to);
    }
  else
    error("ia64_register_convert_to_raw called with non floating point register number");
}

struct type *
ia64_register_virtual_type (int reg)
{
  if (reg >= IA64_FR0_REGNUM && reg <= IA64_FR127_REGNUM)
    return builtin_type_long_double;
  else
    return builtin_type_long;
}

int
ia64_register_byte (int reg)
{
  return (8 * reg) +
   (reg <= IA64_FR0_REGNUM ? 0 : 8 * ((reg > IA64_FR127_REGNUM) ? 128 : reg - IA64_FR0_REGNUM));
}

/* Extract ``len'' bits from an instruction bundle starting at
   bit ``from''.  */

long long
extract_bit_field (char *bundle, int from, int len)
{
  long long result = 0LL;
  int to = from + len;
  int from_byte = from / 8;
  int to_byte = to / 8;
  unsigned char *b = (unsigned char *) bundle;
  unsigned char c;
  int lshift;
  int i;

  c = b[from_byte];
  if (from_byte == to_byte)
    c = ((unsigned char) (c << (8 - to % 8))) >> (8 - to % 8);
  result = c >> (from % 8);
  lshift = 8 - (from % 8);

  for (i = from_byte+1; i < to_byte; i++)
    {
      result |= ((long long) b[i]) << lshift;
      lshift += 8;
    }

  if (from_byte < to_byte && (to % 8 != 0))
    {
      c = b[to_byte];
      c = ((unsigned char) (c << (8 - to % 8))) >> (8 - to % 8);
      result |= ((long long) c) << lshift;
    }

  return result;
}

/* Replace the specified bits in an instruction bundle */

void
replace_bit_field (char *bundle, long long val, int from, int len)
{
  int to = from + len;
  int from_byte = from / 8;
  int to_byte = to / 8;
  unsigned char *b = (unsigned char *) bundle;
  unsigned char c;

  if (from_byte == to_byte)
    {
      unsigned char left, right;
      c = b[from_byte];
      left = (c >> (to % 8)) << (to % 8);
      right = ((unsigned char) (c << (8 - from % 8))) >> (8 - from % 8);
      c = (unsigned char) (val & 0xff);
      c = (unsigned char) (c << (from % 8 + 8 - to % 8)) >> (8 - to % 8);
      c |= right | left;
      b[from_byte] = c;
    }
  else
    {
      int i;
      c = b[from_byte];
      c = ((unsigned char) (c << (8 - from % 8))) >> (8 - from % 8);
      c = c | (val << (from % 8));
      b[from_byte] = c;
      val >>= 8 - from % 8;

      for (i = from_byte+1; i < to_byte; i++)
	{
	  c = val & 0xff;
	  val >>= 8;
	  b[i] = c;
	}

      if (to % 8 != 0)
	{
	  unsigned char cv = (unsigned char) val;
	  c = b[to_byte];
	  c = c >> (to % 8) << (to % 8);
	  c |= ((unsigned char) (cv << (8 - to % 8))) >> (8 - to % 8);
	  b[to_byte] = c;
	}
    }
}

/* Return the contents of slot N (for N = 0, 1, or 2) in
   and instruction bundle */

long long
slotN_contents (unsigned char *bundle, int slotnum)
{
  return extract_bit_field (bundle, 5+41*slotnum, 41);
}

/* Store an instruction in an instruction bundle */

void
replace_slotN_contents (unsigned char *bundle, long long instr, int slotnum)
{
  replace_bit_field (bundle, instr, 5+41*slotnum, 41);
}

static template_encoding_table[32][3] =
{
  { M, I, I },				/* 00 */
  { M, I, I },				/* 01 */
  { M, I, I },				/* 02 */
  { M, I, I },				/* 03 */
  { M, L, X },				/* 04 */
  { M, L, X },				/* 05 */
  { undefined, undefined, undefined },  /* 06 */
  { undefined, undefined, undefined },  /* 07 */
  { M, M, I },				/* 08 */
  { M, M, I },				/* 09 */
  { M, M, I },				/* 0A */
  { M, M, I },				/* 0B */
  { M, F, I },				/* 0C */
  { M, F, I },				/* 0D */
  { M, M, F },				/* 0E */
  { M, M, F },				/* 0F */
  { M, I, B },				/* 10 */
  { M, I, B },				/* 11 */
  { M, B, B },				/* 12 */
  { M, B, B },				/* 13 */
  { undefined, undefined, undefined },  /* 14 */
  { undefined, undefined, undefined },  /* 15 */
  { B, B, B },				/* 16 */
  { B, B, B },				/* 17 */
  { M, M, B },				/* 18 */
  { M, M, B },				/* 19 */
  { undefined, undefined, undefined },  /* 1A */
  { undefined, undefined, undefined },  /* 1B */
  { M, F, B },				/* 1C */
  { M, F, B },				/* 1D */
  { undefined, undefined, undefined },  /* 1E */
  { undefined, undefined, undefined },  /* 1F */
};

/* Fetch and (partially) decode an instruction at ADDR and return the
   address of the next instruction to fetch.  */

static CORE_ADDR
fetch_instruction (CORE_ADDR addr, instruction_type *it, long long *instr)
{
  char bundle[BUNDLE_LEN];
  int slotnum = (int) (addr & 0x0f) / SLOT_MULTIPLIER;
  long long template;
  int val;

  if (slotnum > 2)
    error("Can't fetch instructions for slot numbers greater than 2.");

  addr &= ~0x0f;

  val = target_read_memory (addr, bundle, BUNDLE_LEN);

  if (val != 0)
    return 0;

  *instr = slotN_contents (bundle, slotnum);
  template = extract_bit_field (bundle, 0, 5);
  *it = template_encoding_table[(int)template][slotnum];

  if (slotnum == 2 || slotnum == 1 && *it == L)
    addr += 16;
  else
    addr += (slotnum + 1) * SLOT_MULTIPLIER;

  return addr;
}

/* There are 5 different break instructions (break.i, break.b,
   break.m, break.f, and break.x), but they all have the same
   encoding.  (The five bit template in the low five bits of the
   instruction bundle distinguishes one from another.)
   
   The runtime architecture manual specifies that break instructions
   used for debugging purposes must have the upper two bits of the 21
   bit immediate set to a 0 and a 1 respectively.  A breakpoint
   instruction encodes the most significant bit of its 21 bit
   immediate at bit 36 of the 41 bit instruction.  The penultimate msb
   is at bit 25 which leads to the pattern below.  
   
   Originally, I had this set up to do, e.g, a "break.i 0x80000"  But
   it turns out that 0x80000 was used as the syscall break in the early
   simulators.  So I changed the pattern slightly to do "break.i 0x080001"
   instead.  But that didn't work either (I later found out that this
   pattern was used by the simulator that I was using.)  So I ended up
   using the pattern seen below. */

#if 0
#define BREAKPOINT 0x00002000040LL
#endif
#define BREAKPOINT 0x00003333300LL

static int
ia64_memory_insert_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  char bundle[BUNDLE_LEN];
  int slotnum = (int) (addr & 0x0f) / SLOT_MULTIPLIER;
  long long instr;
  int val;

  if (slotnum > 2)
    error("Can't insert breakpoint for slot numbers greater than 2.");

  addr &= ~0x0f;

  val = target_read_memory (addr, bundle, BUNDLE_LEN);
  instr = slotN_contents (bundle, slotnum);
  memcpy(contents_cache, &instr, sizeof(instr));
  replace_slotN_contents (bundle, BREAKPOINT, slotnum);
  if (val == 0)
    target_write_memory (addr, bundle, BUNDLE_LEN);

  return val;
}

static int
ia64_memory_remove_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  char bundle[BUNDLE_LEN];
  int slotnum = (addr & 0x0f) / SLOT_MULTIPLIER;
  long long instr;
  int val;

  addr &= ~0x0f;

  val = target_read_memory (addr, bundle, BUNDLE_LEN);
  memcpy (&instr, contents_cache, sizeof instr);
  replace_slotN_contents (bundle, instr, slotnum);
  if (val == 0)
    target_write_memory (addr, bundle, BUNDLE_LEN);

  return val;
}

/* We don't really want to use this, but remote.c needs to call it in order
   to figure out if Z-packets are supported or not.  Oh, well. */
unsigned char *
ia64_breakpoint_from_pc (pcptr, lenptr)
     CORE_ADDR *pcptr;
     int *lenptr;
{
  static unsigned char breakpoint[] =
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  *lenptr = sizeof (breakpoint);
#if 0
  *pcptr &= ~0x0f;
#endif
  return breakpoint;
}

CORE_ADDR
ia64_read_pc (int pid)
{
  CORE_ADDR psr_value = read_register_pid (IA64_PSR_REGNUM, pid);
  CORE_ADDR pc_value   = read_register_pid (IA64_IP_REGNUM, pid);
  int slot_num = (psr_value >> 41) & 3;

  return pc_value | (slot_num * SLOT_MULTIPLIER);
}

void
ia64_write_pc (CORE_ADDR new_pc, int pid)
{
  int slot_num = (int) (new_pc & 0xf) / SLOT_MULTIPLIER;
  CORE_ADDR psr_value = read_register_pid (IA64_PSR_REGNUM, pid);
  psr_value &= ~(3LL << 41);
  psr_value |= (CORE_ADDR)(slot_num & 0x3) << 41;

  new_pc &= ~0xfLL;

  write_register_pid (IA64_PSR_REGNUM, psr_value, pid);
  write_register_pid (IA64_IP_REGNUM, new_pc, pid);
}

#define IS_NaT_COLLECTION_ADDR(addr) ((((addr) >> 3) & 0x3f) == 0x3f)

/* Returns the address of the slot that's NSLOTS slots away from
   the address ADDR. NSLOTS may be positive or negative. */
static CORE_ADDR
rse_address_add(CORE_ADDR addr, int nslots)
{
  CORE_ADDR new_addr;
  int mandatory_nat_slots = nslots / 63;
  int direction = nslots < 0 ? -1 : 1;

  new_addr = addr + 8 * (nslots + mandatory_nat_slots);

  if ((new_addr >> 9)  != ((addr + 8 * 64 * mandatory_nat_slots) >> 9))
    new_addr += 8 * direction;

  if (IS_NaT_COLLECTION_ADDR(new_addr))
    new_addr += 8 * direction;

  return new_addr;
}

/* The IA-64 frame chain is a bit odd.  We won't always have a frame
   pointer, so we use the SP value as the FP for the purpose of
   creating a frame.  There is sometimes a register (not fixed) which
   is used as a frame pointer.  When this register exists, it is not
   especially hard to determine which one is being used.  It isn't
   even really hard to compute the frame chain, but it can be
   computationally expensive.  So, instead of making life difficult
   (and slow), we pick a more convenient representation of the frame
   chain, knowing that we'll have to make some small adjustments
   in other places.  (E.g, note that read_fp() and write_fp() are
   actually read_sp() and write_sp() below in ia64_gdbarch_init()
   below.) 

   Okay, so what is the frame chain exactly?  It'll be the SP value
   at the time that the function in question was entered.

   Note that this *should* actually the frame pointer for the current
   function!  But as I note above, if we were to attempt to find the
   address of the beginning of the previous frame, we'd waste a lot
   of cycles for no good reason.  So instead, we simply choose to
   represent the frame chain as the end of the previous frame instead
   of the beginning.  */

CORE_ADDR
ia64_frame_chain (struct frame_info *frame)
{
  FRAME_INIT_SAVED_REGS (frame);

  if (frame->saved_regs[IA64_VFP_REGNUM])
    return read_memory_integer (frame->saved_regs[IA64_VFP_REGNUM], 8);
  else
    return frame->frame + frame->extra_info->mem_stack_frame_size;
}

CORE_ADDR
ia64_frame_saved_pc (struct frame_info *frame)
{
  FRAME_INIT_SAVED_REGS (frame);

  if (frame->saved_regs[IA64_VRAP_REGNUM])
    return read_memory_integer (frame->saved_regs[IA64_VRAP_REGNUM], 8);
  else	/* either frameless, or not far enough along in the prologue... */
    return ia64_saved_pc_after_call (frame);
}

#define isScratch(_regnum_) ((_regnum_) == 2 || (_regnum_) == 3 \
  || (8 <= (_regnum_) && (_regnum_) <= 11) \
  || (14 <= (_regnum_) && (_regnum_) <= 31))
#define imm9(_instr_) \
  ( ((((_instr_) & 0x01000000000LL) ? -1 : 0) << 8) \
   | (((_instr_) & 0x00008000000LL) >> 20) \
   | (((_instr_) & 0x00000001fc0LL) >> 6))

static CORE_ADDR
examine_prologue (CORE_ADDR pc, CORE_ADDR lim_pc, struct frame_info *frame)
{
  CORE_ADDR next_pc;
  CORE_ADDR last_prologue_pc = pc;
  int done = 0;
  instruction_type it;
  long long instr;
  int do_fsr_stuff = 0;

  int cfm_reg  = 0;
  int ret_reg  = 0;
  int fp_reg   = 0;
  int unat_save_reg = 0;
  int pr_save_reg = 0;
  int mem_stack_frame_size = 0;
  int spill_reg   = 0;
  CORE_ADDR spill_addr = 0;

  if (frame && !frame->saved_regs)
    {
      frame_saved_regs_zalloc (frame);
      do_fsr_stuff = 1;
    }

  if (frame 
      && !do_fsr_stuff
      && frame->extra_info->after_prologue != 0
      && frame->extra_info->after_prologue <= lim_pc)
    return frame->extra_info->after_prologue;

  /* Must start with an alloc instruction */
  next_pc = fetch_instruction (pc, &it, &instr);
  if (pc < lim_pc && next_pc 
      && it == M && ((instr & 0x1ee0000003fLL) == 0x02c00000000LL))
    {
      /* alloc */
      int sor = (int) ((instr & 0x00078000000LL) >> 27);
      int sol = (int) ((instr & 0x00007f00000LL) >> 20);
      int sof = (int) ((instr & 0x000000fe000LL) >> 13);
      /* Okay, so sor, sol, and sof aren't used right now; but perhaps
         we could compare against the size given to us via the cfm as
	 either a sanity check or possibly to see if the frame has been
	 changed by a later alloc instruction... */
      int rN = (int) ((instr & 0x00000001fc0LL) >> 6);
      cfm_reg = rN;
      last_prologue_pc = next_pc;
      pc = next_pc;
    }
  else
    pc = lim_pc;	/* We're done early */

  /* Loop, looking for prologue instructions, keeping track of
     where preserved registers were spilled. */
  while (pc < lim_pc)
    {
      next_pc = fetch_instruction (pc, &it, &instr);
      if (next_pc == 0)
	break;

      if (it == I && ((instr & 0x1eff8000000LL) == 0x00188000000LL))
        {
	  /* Move from BR */
	  int b2 = (int) ((instr & 0x0000000e000LL) >> 13);
	  int rN = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp = (int) (instr & 0x0000000003f);

	  if (qp == 0 && b2 == 0 && rN >= 32 && ret_reg == 0)
	    {
	      ret_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if ((it == I || it == M) 
          && ((instr & 0x1ee00000000LL) == 0x10800000000LL))
	{
	  /* adds rN = imm14, rM   (or mov rN, rM  when imm14 is 0) */
	  int imm = (int) ((((instr & 0x01000000000LL) ? -1 : 0) << 13) 
	                   | ((instr & 0x001f8000000LL) >> 20)
		           | ((instr & 0x000000fe000LL) >> 13));
	  int rM = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rN = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp = (int) (instr & 0x0000000003fLL);

	  if (qp == 0 && rN >= 32 && imm == 0 && rM == 12 && fp_reg == 0)
	    {
	      /* mov rN, r12 */
	      fp_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	  else if (qp == 0 && rN == 12 && rM == 12)
	    {
	      /* adds r12, -mem_stack_frame_size, r12 */
	      mem_stack_frame_size -= imm;
	      last_prologue_pc = next_pc;
	    }
	  else if (qp == 0 && rN == 2 
	        && ((rM == fp_reg && fp_reg != 0) || rM == 12))
	    {
	      /* adds r2, spilloffset, rFramePointer 
	           or
		 adds r2, spilloffset, r12

	         Get ready for stf.spill or st8.spill instructions.
		 The address to start spilling at is loaded into r2. 
		 FIXME:  Why r2?  That's what gcc currently uses; it
		 could well be different for other compilers.  */

	      /* Hmm... whether or not this will work will depend on
	         where the pc is.  If it's still early in the prologue
		 this'll be wrong.  FIXME */
	      spill_addr  = (frame ? frame->frame : 0)
	                  + (rM == 12 ? 0 : mem_stack_frame_size) 
			  + imm;
	      spill_reg   = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M 
            && (   ((instr & 0x1efc0000000LL) == 0x0eec0000000LL)
                || ((instr & 0x1ffc8000000LL) == 0x0cec0000000LL) ))
	{
	  /* stf.spill [rN] = fM, imm9
	     or
	     stf.spill [rN] = fM  */

	  int imm = imm9(instr);
	  int rN = (int) ((instr & 0x00007f00000LL) >> 20);
	  int fM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && rN == spill_reg && spill_addr != 0
	      && ((2 <= fM && fM <= 5) || (16 <= fM && fM <= 31)))
	    {
	      if (do_fsr_stuff)
	        frame->saved_regs[IA64_FR0_REGNUM + fM] = spill_addr;

              if ((instr & 0x1efc0000000) == 0x0eec0000000)
		spill_addr += imm;
	      else
		spill_addr = 0;		/* last one; must be done */
	      last_prologue_pc = next_pc;
	    }
	}
      else if ((it == M && ((instr & 0x1eff8000000LL) == 0x02110000000LL))
            || (it == I && ((instr & 0x1eff8000000LL) == 0x00050000000LL)) )
	{
	  /* mov.m rN = arM   
	       or 
	     mov.i rN = arM */

	  int arM = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rN  = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp  = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && isScratch (rN) && arM == 36 /* ar.unat */)
	    {
	      /* We have something like "mov.m r3 = ar.unat".  Remember the
		 r3 (or whatever) and watch for a store of this register... */
	      unat_save_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == I && ((instr & 0x1eff8000000LL) == 0x00198000000LL))
	{
	  /* mov rN = pr */
	  int rN  = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp  = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && isScratch (rN))
	    {
	      pr_save_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M 
            && (   ((instr & 0x1ffc8000000LL) == 0x08cc0000000LL)
	        || ((instr & 0x1efc0000000LL) == 0x0acc0000000LL)))
	{
	  /* st8 [rN] = rM 
	      or
	     st8 [rN] = rM, imm9 */
	  int rN = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && rN == spill_reg && spill_addr != 0
	      && (rM == unat_save_reg || rM == pr_save_reg))
	    {
	      /* We've found a spill of either the UNAT register or the PR
	         register.  (Well, not exactly; what we've actually found is
		 a spill of the register that UNAT or PR was moved to).
		 Record that fact and move on... */
	      if (rM == unat_save_reg)
		{
		  /* Track UNAT register */
		  if (do_fsr_stuff)
		    frame->saved_regs[IA64_UNAT_REGNUM] = spill_addr;
		  unat_save_reg = 0;
		}
	      else
	        {
		  /* Track PR register */
		  if (do_fsr_stuff)
		    frame->saved_regs[IA64_PR_REGNUM] = spill_addr;
		  pr_save_reg = 0;
		}
	      if ((instr & 0x1efc0000000LL) == 0x0acc0000000LL)
		/* st8 [rN] = rM, imm9 */
		spill_addr += imm9(instr);
	      else
		spill_addr = 0;		/* must be done spilling */
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M
            && (   ((instr & 0x1ffc8000000LL) == 0x08ec0000000LL)
	        || ((instr & 0x1efc0000000LL) == 0x0aec0000000LL)))
	{
	  /* st8.spill [rN] = rM
	       or
	     st8.spill [rN] = rM, imm9 */
	  int rN = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && rN == spill_reg && 4 <= rM && rM <= 7)
	    {
	      /* We've found a spill of one of the preserved general purpose
	         regs.  Record the spill address and advance the spill
		 register if appropriate. */
	      if (do_fsr_stuff)
		frame->saved_regs[IA64_GR0_REGNUM + rM] = spill_addr;
	      if ((instr & 0x1efc0000000LL) == 0x0aec0000000LL)
	        /* st8.spill [rN] = rM, imm9 */
		spill_addr += imm9(instr);
	      else
		spill_addr = 0;		/* Done spilling */
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == B || ((instr & 0x3fLL) != 0LL))
	break;

      pc = next_pc;
    }

  if (do_fsr_stuff) {
    int i;
    CORE_ADDR addr;

    for (i = 0, addr = frame->extra_info->bsp;
	 i < frame->extra_info->sof;
	 i++, addr += 8)
      {
	if (IS_NaT_COLLECTION_ADDR (addr))
	  {
	    addr += 8;
	  }
	frame->saved_regs[IA64_GR32_REGNUM + i] = addr;

	if (i+32 == cfm_reg)
	  frame->saved_regs[IA64_CFM_REGNUM] = addr;
	if (i+32 == ret_reg)
	  frame->saved_regs[IA64_VRAP_REGNUM] = addr;
	if (i+32 == fp_reg)
	  frame->saved_regs[IA64_VFP_REGNUM] = addr;
      }
  }

  if (frame && frame->extra_info) {
    frame->extra_info->after_prologue = last_prologue_pc;
    frame->extra_info->mem_stack_frame_size = mem_stack_frame_size;
    frame->extra_info->fp_reg = fp_reg;
  }

  return last_prologue_pc;
}

CORE_ADDR
ia64_skip_prologue (CORE_ADDR pc)
{
  return examine_prologue (pc, pc+1024, 0);
}

void
ia64_frame_init_saved_regs (struct frame_info *frame)
{
  CORE_ADDR func_start;

  if (frame->saved_regs)
    return;

  func_start = get_pc_function_start (frame->pc);
  examine_prologue (func_start, frame->pc, frame);
}

static CORE_ADDR
ia64_find_saved_register (frame, regnum)
     struct frame_info *frame;
     int regnum;
{
  register CORE_ADDR addr = 0;

  if ((IA64_GR32_REGNUM <= regnum && regnum <= IA64_GR127_REGNUM)
      || regnum == IA64_VFP_REGNUM
      || regnum == IA64_VRAP_REGNUM)
    {
      FRAME_INIT_SAVED_REGS (frame);
      return frame->saved_regs[regnum];
    }
  else if (regnum == IA64_IP_REGNUM && frame->next)
    {
      FRAME_INIT_SAVED_REGS (frame->next);
      return frame->next->saved_regs[IA64_VRAP_REGNUM];
    }
  else
    {
      struct frame_info *frame1 = NULL;
      while (1)
	{
	  QUIT;
	  frame1 = get_prev_frame (frame1);
	  if (frame1 == 0 || frame1 == frame)
	    break;
	  FRAME_INIT_SAVED_REGS (frame1);
	  if (frame1->saved_regs[regnum])
	    addr = frame1->saved_regs[regnum];
	}
    }

  return addr;
}

void
ia64_get_saved_register (char *raw_buffer, 
                         int *optimized, 
			 CORE_ADDR *addrp,
			 struct frame_info *frame,
			 int regnum,
			 enum lval_type *lval)
{
  CORE_ADDR addr;

  if (!target_has_registers)
    error ("No registers.");

  if (optimized != NULL)
    *optimized = 0;
  addr = ia64_find_saved_register (frame, regnum);
  if (addr != 0)
    {
      if (lval != NULL)
	*lval = lval_memory;
      if (regnum == SP_REGNUM)
	{
	  if (raw_buffer != NULL)
	    {
	      /* Put it back in target format.  */
	      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), (LONGEST) addr);
	    }
	  if (addrp != NULL)
	    *addrp = 0;
	  return;
	}
      if (raw_buffer != NULL)
	read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
    }
  else if (IA64_GR32_REGNUM <= regnum && regnum <= IA64_GR127_REGNUM)
    {
      /* r32 - r127 must be fetchable via memory.  If they aren't,
         then the register is unavailable */
      addr = 0;
      if (lval != NULL)
	*lval = not_lval;
      memset (raw_buffer, 0, REGISTER_RAW_SIZE (regnum));
    }
  else if (regnum == IA64_IP_REGNUM)
    {
      CORE_ADDR pc;
      if (frame->next)
        {
	  /* This case will normally be handled above, except when it's
	     frameless or we haven't advanced far enough into the prologue
	     of the top frame to save the register. */
	  addr = REGISTER_BYTE (regnum);
	  if (lval != NULL)
	    *lval = lval_register;
	  pc = ia64_saved_pc_after_call (frame);
        }
      else
        {
	  addr = 0;
	  if (lval != NULL)
	    *lval = not_lval;
	  pc = read_pc ();
	}
      store_address (raw_buffer, REGISTER_RAW_SIZE (IA64_IP_REGNUM), pc);
    }
  else if (regnum == SP_REGNUM && frame->next)
    {
      /* Handle SP values for all frames but the topmost. */
      addr = 0;
      if (lval != NULL)
        *lval = not_lval;
      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), frame->frame);
    }
  else if (regnum == IA64_BSP_REGNUM)
    {
      addr = 0;
      if (lval != NULL)
        *lval = not_lval;
      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), 
                     frame->extra_info->bsp);
    }
  else if (regnum == IA64_VFP_REGNUM)
    {
      /* If the function in question uses an automatic register (r32-r127)
         for the frame pointer, it'll be found by ia64_find_saved_register()
	 above.  If the function lacks one of these frame pointers, we can
	 still provide a value since we know the size of the frame */
      CORE_ADDR vfp = frame->frame + frame->extra_info->mem_stack_frame_size;
      addr = 0;
      if (lval != NULL)
	*lval = not_lval;
      store_address (raw_buffer, REGISTER_RAW_SIZE (IA64_VFP_REGNUM), vfp);
    }
  else if (IA64_PR0_REGNUM <= regnum && regnum <= IA64_PR63_REGNUM)
    {
      char pr_raw_buffer[MAX_REGISTER_RAW_SIZE];
      int  pr_optim;
      enum lval_type pr_lval;
      CORE_ADDR pr_addr;
      int prN_val;
      ia64_get_saved_register (pr_raw_buffer, &pr_optim, &pr_addr,
                               frame, IA64_PR_REGNUM, &pr_lval);
      prN_val = extract_bit_field ((unsigned char *) pr_raw_buffer,
                                   regnum - IA64_PR0_REGNUM, 1);
      store_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum), prN_val);
      addr = 0;
      if (lval != NULL)
	*lval = not_lval;
    }
  else if (IA64_NAT0_REGNUM <= regnum && regnum <= IA64_NAT31_REGNUM)
    {
      char unat_raw_buffer[MAX_REGISTER_RAW_SIZE];
      int  unat_optim;
      enum lval_type unat_lval;
      CORE_ADDR unat_addr;
      int unatN_val;
      ia64_get_saved_register (unat_raw_buffer, &unat_optim, &unat_addr,
                               frame, IA64_UNAT_REGNUM, &unat_lval);
      unatN_val = extract_bit_field ((unsigned char *) unat_raw_buffer,
                                   regnum - IA64_NAT0_REGNUM, 1);
      store_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum), 
                              unatN_val);
      addr = 0;
      if (lval != NULL)
	*lval = not_lval;
    }
  else if (IA64_NAT32_REGNUM <= regnum && regnum <= IA64_NAT127_REGNUM)
    {
      int natval = 0;
      /* Find address of general register corresponding to nat bit we're
         interested in. */
      CORE_ADDR gr_addr = 
	ia64_find_saved_register (frame, 
	                          regnum - IA64_NAT0_REGNUM + IA64_GR0_REGNUM);
      if (gr_addr)
	{
	  /* Compute address of nat collection bits */
	  CORE_ADDR nat_addr = gr_addr | 0x1f8;
	  CORE_ADDR bsp = read_register (IA64_BSP_REGNUM);
	  CORE_ADDR nat_collection;
	  int nat_bit;
	  /* If our nat collection address is bigger than bsp, we have to get
	     the nat collection from rnat.  Otherwise, we fetch the nat
	     collection from the computed address. */
	  if (nat_addr >= bsp)
	    nat_collection = read_register (IA64_RNAT_REGNUM);
	  else
	    nat_collection = read_memory_integer (nat_addr, 8);
	  nat_bit = (gr_addr >> 3) & 0x3f;
	  natval = (nat_collection >> nat_bit) & 1;
	}
      store_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum), natval);
      addr = 0;
      if (lval != NULL)
	*lval = not_lval;
    }
  else
    {
      if (lval != NULL)
	*lval = lval_register;
      addr = REGISTER_BYTE (regnum);
      if (raw_buffer != NULL)
	read_register_gen (regnum, raw_buffer);
    }
  if (addrp != NULL)
    *addrp = addr;
}

/* Should we use EXTRACT_STRUCT_VALUE_ADDRESS instead of
   EXTRACT_RETURN_VALUE?  GCC_P is true if compiled with gcc
   and TYPE is the type (which is known to be struct, union or array).  */
int
ia64_use_struct_convention (int gcc_p, struct type *type)
{
  /* FIXME: Need to check for HFAs; structures containing (only) up to 8
     floating point values of the same size are returned in floating point
     registers. */
  return TYPE_LENGTH (type) > 32;
}

void
ia64_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    ia64_register_convert_to_virtual (IA64_FR8_REGNUM, type,
      &regbuf[REGISTER_BYTE (IA64_FR8_REGNUM)], valbuf);
  else
    memcpy (valbuf, &regbuf[REGISTER_BYTE (IA64_GR8_REGNUM)], TYPE_LENGTH (type));
}

/* FIXME: Turn this into a stack of some sort.  Unfortunately, something
   like this is necessary though since the IA-64 calling conventions specify
   that r8 is not preserved. */
static CORE_ADDR struct_return_address;

CORE_ADDR
ia64_extract_struct_value_address (char *regbuf)
{
  /* FIXME: See above. */
  return struct_return_address;
}

void
ia64_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  /* FIXME: See above. */
  /* Note that most of the work was done in ia64_push_arguments() */
  struct_return_address = addr;
}

int
ia64_frameless_function_invocation (struct frame_info *frame)
{
  /* FIXME: Implement */
  return 0;
}

CORE_ADDR
ia64_saved_pc_after_call (struct frame_info *frame)
{
  return read_register (IA64_BR0_REGNUM);
}

CORE_ADDR
ia64_frame_args_address (struct frame_info *frame)
{
  /* frame->frame points at the SP for this frame; But we want the start
     of the frame, not the end.  Calling frame chain will get his for us. */
  return ia64_frame_chain (frame);
}

CORE_ADDR
ia64_frame_locals_address (struct frame_info *frame)
{
  /* frame->frame points at the SP for this frame; But we want the start
     of the frame, not the end.  Calling frame chain will get his for us. */
  return ia64_frame_chain (frame);
}

void
ia64_init_extra_frame_info (int fromleaf, struct frame_info *frame)
{
  CORE_ADDR bsp, cfm;

  frame->extra_info = (struct frame_extra_info *)
    frame_obstack_alloc (sizeof (struct frame_extra_info));

  if (frame->next == 0)
    {
      bsp = read_register (IA64_BSP_REGNUM);
      cfm = read_register (IA64_CFM_REGNUM);

    }
  else
    {
      struct frame_info *frn = frame->next;
      CORE_ADDR cfm_addr;

      FRAME_INIT_SAVED_REGS (frn);

      if (frn->saved_regs[IA64_CFM_REGNUM] != 0)
	cfm = read_memory_integer (frn->saved_regs[IA64_CFM_REGNUM], 8);
      else
	cfm = read_register (IA64_CFM_REGNUM);

      bsp = frn->extra_info->bsp;
    }
  frame->extra_info->cfm = cfm;
  frame->extra_info->sof = cfm & 0x7f;
  frame->extra_info->sol = (cfm >> 7) & 0x7f;
  if (frame->next == 0)
    frame->extra_info->bsp = rse_address_add (bsp, -frame->extra_info->sof);
  else
    frame->extra_info->bsp = rse_address_add (bsp, -frame->extra_info->sol);

  frame->extra_info->after_prologue = 0;
  frame->extra_info->mem_stack_frame_size = -1;		/* Not yet determined */
  frame->extra_info->fp_reg = 0;
}

#define ROUND_UP(n,a) (((n)+(a)-1) & ~((a)-1))

CORE_ADDR
ia64_push_arguments (int nargs, value_ptr *args, CORE_ADDR sp,
		    int struct_return, CORE_ADDR struct_addr)
{
  int argno;
  value_ptr arg;
  struct type *type;
  int len, argoffset;
  int nslots, rseslots, memslots, slotnum;
  int floatreg;
  CORE_ADDR bsp, cfm, pfs, new_bsp;

  nslots = 0;
  /* Count the number of slots needed for the arguments */
  for (argno = 0; argno < nargs; argno++)
    {
      arg = args[argno];
      type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (type);

      /* FIXME: This is crude and it is wrong (IMO), but it matches
         what gcc does, I think. */
      if (len > 8 && (nslots & 1))
	nslots++;

      nslots += (len + 7) / 8;
    }

  rseslots = (nslots > 8) ? 8 : nslots;
  memslots = nslots - rseslots;

  cfm = read_register (IA64_CFM_REGNUM);

  bsp = read_register (IA64_BSP_REGNUM);
  bsp = rse_address_add (bsp, cfm & 0x7f);
  new_bsp = rse_address_add (bsp, rseslots);
  write_register (IA64_BSP_REGNUM, new_bsp);

  pfs = read_register (IA64_PFS_REGNUM);
  pfs &= 0xc000000000000000LL;
  pfs |= (cfm & 0xffffffffffffLL);
  write_register (IA64_PFS_REGNUM, pfs);

  cfm &= 0xc000000000000000LL;
  cfm |= rseslots;
  write_register (IA64_CFM_REGNUM, cfm);
  

  
  sp = sp - 16 - memslots * 8;
  sp &= ~0xfLL;				/* Maintain 16 byte alignment */

  slotnum = 0;
  floatreg = IA64_FR8_REGNUM;
  for (argno = 0; argno < nargs; argno++)
    {
      arg = args[argno];
      type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (type);
      if (len > 8 && (slotnum & 1))
	slotnum++;
      argoffset = 0;
      while (len > 0)
	{
	  char val_buf[8];

	  memset (val_buf, 0, 8);
	  memcpy (val_buf, VALUE_CONTENTS (arg) + argoffset, (len > 8) ? 8 : len);

	  if (slotnum < rseslots)
	    write_memory (rse_address_add (bsp, slotnum), val_buf, 8);
	  else
	    write_memory (sp + 16 + 8 * (slotnum - rseslots), val_buf, 8);

	  argoffset += 8;
	  len -= 8;
	  slotnum++;
	}
      if (TYPE_CODE (type) == TYPE_CODE_FLT && floatreg < IA64_FR16_REGNUM)
        {
	  ia64_register_convert_to_raw (type, floatreg, VALUE_CONTENTS (arg),
	    &registers[REGISTER_BYTE (floatreg)]);
	  floatreg++;
	}
    }

  if (struct_return)
    {
      store_address (&registers[REGISTER_BYTE (IA64_GR8_REGNUM)],
                     REGISTER_RAW_SIZE (IA64_GR8_REGNUM),
		     struct_addr);
    }


  target_store_registers (-1);

  /* FIXME: This doesn't belong here!  Instead, SAVE_DUMMY_FRAME_TOS needs
     to be defined to call generic_save_dummy_frame_tos().  But at the
     time of this writing, SAVE_DUMMY_FRAME_TOS wasn't gdbarch'd, so
     I chose to put this call here instead of using the old mechanisms. 
     Once SAVE_DUMMY_FRAME_TOS is gdbarch'd, all we need to do is add the
     line

	set_gdbarch_save_dummy_frame_tos (gdbarch, generic_save_dummy_frame_tos);

     to ia64_gdbarch_init() and remove the line below. */
  generic_save_dummy_frame_tos (sp);

  return sp;
}

CORE_ADDR
ia64_push_return_address (CORE_ADDR pc, CORE_ADDR sp)
{
  struct partial_symtab *pst;

  /* Attempt to determine and set global pointer (r1) for this pc.
     
     This rather nasty bit of code searchs for the .dynamic section
     in the objfile corresponding to the pc of the function we're
     trying to call.  Once it finds the addresses at which the .dynamic
     section lives in the child process, it scans the Elf64_Dyn entries
     for a DT_PLTGOT tag.  If it finds one of these, the corresponding
     d_un.d_ptr value is the global pointer. */
  pst = find_pc_psymtab (pc);
  if (pst != NULL)
    {
      struct obj_section *osect;

      ALL_OBJFILE_OSECTIONS (pst->objfile, osect)
	{
	  if (strcmp (osect->the_bfd_section->name, ".dynamic") == 0)
	    break;
	}

      if (osect < pst->objfile->sections_end)
	{
	  CORE_ADDR addr;

	  addr = osect->addr;
	  while (addr < osect->endaddr)
	    {
	      int status;
	      LONGEST tag;
	      char buf[8];

	      status = target_read_memory (addr, buf, sizeof (buf));
	      if (status != 0)
		break;
	      tag = extract_signed_integer (buf, sizeof (buf));

	      if (tag == DT_PLTGOT)
		{
		  CORE_ADDR global_pointer;

		  status = target_read_memory (addr + 8, buf, sizeof (buf));
		  if (status != 0)
		    break;
		  global_pointer = extract_address (buf, sizeof (buf));

		  /* The payoff... */
		  write_register (IA64_GR1_REGNUM, global_pointer);
		  break;
		}

	      if (tag == DT_NULL)
		break;

	      addr += 16;
	    }
	}
    }

  write_register (IA64_BR0_REGNUM, CALL_DUMMY_ADDRESS ());
  return sp;
}

void
ia64_store_return_value (struct type *type, char *valbuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      ia64_register_convert_to_raw (type, IA64_FR8_REGNUM, valbuf,
				  &registers[REGISTER_BYTE (IA64_FR8_REGNUM)]);
      target_store_registers (IA64_FR8_REGNUM);
    }
  else
    write_register_bytes (REGISTER_BYTE (IA64_GR8_REGNUM),
			  valbuf, TYPE_LENGTH (type));
}

void
ia64_pop_frame (void)
{
  generic_pop_current_frame (ia64_pop_frame_regular);
}

static void
ia64_pop_frame_regular (struct frame_info *frame)
{
  int regno;
  CORE_ADDR bsp, cfm, pfs;

  FRAME_INIT_SAVED_REGS (frame);

  for (regno = 0; regno < ia64_num_regs; regno++)
    {
      if (frame->saved_regs[regno]
	  && (!(IA64_GR32_REGNUM <= regno && regno <= IA64_GR127_REGNUM))
	  && regno != pc_regnum
	  && regno != sp_regnum
	  && regno != IA64_PFS_REGNUM
	  && regno != IA64_CFM_REGNUM
	  && regno != IA64_BSP_REGNUM
	  && regno != IA64_BSPSTORE_REGNUM)
	{
	  write_register (regno, 
			  read_memory_integer (frame->saved_regs[regno],
					       REGISTER_RAW_SIZE (regno)));
	}
    }

  write_register (sp_regnum, FRAME_CHAIN (frame));
  write_pc (FRAME_SAVED_PC (frame));

  cfm = read_register (IA64_CFM_REGNUM);

  if (frame->saved_regs[IA64_PFS_REGNUM])
    {
      pfs = read_memory_integer (frame->saved_regs[IA64_PFS_REGNUM],
				 REGISTER_RAW_SIZE (IA64_PFS_REGNUM));
    }
  else
    pfs = read_register (IA64_PFS_REGNUM);

  /* Compute the new bsp by *adding* the difference between the
     size of the frame and the size of the locals (both wrt the
     frame that we're going back to).  This seems kind of strange,
     especially since it seems like we ought to be subtracting the
     size of the locals... and we should; but the linux kernel
     wants bsp to be set at the end of all used registers.  It's
     likely that this code will need to be revised to accomodate
     other operating systems. */
  bsp = rse_address_add (frame->extra_info->bsp,
                         (pfs & 0x7f) - ((pfs >> 7) & 0x7f));
  write_register (IA64_BSP_REGNUM, bsp);

  /* FIXME: What becomes of the epilog count in the PFS? */
  cfm = (cfm & ~0xffffffffffffLL) | (pfs & 0xffffffffffffLL);
  write_register (IA64_CFM_REGNUM, cfm);

  flush_cached_frames ();
}

static void
ia64_remote_translate_xfer_address (CORE_ADDR memaddr, int nr_bytes,
				    CORE_ADDR *targ_addr, int *targ_len)
{
  *targ_addr = memaddr;
  *targ_len  = nr_bytes;
}

static struct gdbarch *
ia64_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  gdbarch = gdbarch_alloc (&info, NULL);

  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 64);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch, 64);

  set_gdbarch_num_regs (gdbarch, ia64_num_regs);
  set_gdbarch_sp_regnum (gdbarch, sp_regnum);
  set_gdbarch_fp_regnum (gdbarch, fp_regnum);
  set_gdbarch_pc_regnum (gdbarch, pc_regnum);

  set_gdbarch_register_name (gdbarch, ia64_register_name);
  set_gdbarch_register_size (gdbarch, 8);
  set_gdbarch_register_bytes (gdbarch, ia64_num_regs * 8 + 128*8);
  set_gdbarch_register_byte (gdbarch, ia64_register_byte);
  set_gdbarch_register_raw_size (gdbarch, ia64_register_raw_size);
  set_gdbarch_max_register_raw_size (gdbarch, 16);
  set_gdbarch_register_virtual_size (gdbarch, ia64_register_virtual_size);
  set_gdbarch_max_register_virtual_size (gdbarch, 16);
  set_gdbarch_register_virtual_type (gdbarch, ia64_register_virtual_type);

  set_gdbarch_skip_prologue (gdbarch, ia64_skip_prologue);

  set_gdbarch_frame_num_args (gdbarch, frame_num_args_unknown);
  set_gdbarch_frameless_function_invocation (gdbarch, ia64_frameless_function_invocation);

  set_gdbarch_saved_pc_after_call (gdbarch, ia64_saved_pc_after_call);

  set_gdbarch_frame_chain (gdbarch, ia64_frame_chain);
  set_gdbarch_frame_chain_valid (gdbarch, func_frame_chain_valid);
  set_gdbarch_frame_saved_pc (gdbarch, ia64_frame_saved_pc);

  set_gdbarch_frame_init_saved_regs (gdbarch, ia64_frame_init_saved_regs);
  set_gdbarch_get_saved_register (gdbarch, ia64_get_saved_register);

  set_gdbarch_register_convertible (gdbarch, ia64_register_convertible);
  set_gdbarch_register_convert_to_virtual (gdbarch, ia64_register_convert_to_virtual);
  set_gdbarch_register_convert_to_raw (gdbarch, ia64_register_convert_to_raw);

  set_gdbarch_use_struct_convention (gdbarch, ia64_use_struct_convention);
  set_gdbarch_extract_return_value (gdbarch, ia64_extract_return_value);

  set_gdbarch_store_struct_return (gdbarch, ia64_store_struct_return);
  set_gdbarch_store_return_value (gdbarch, ia64_store_return_value);
  set_gdbarch_extract_struct_value_address (gdbarch, ia64_extract_struct_value_address);

  set_gdbarch_memory_insert_breakpoint (gdbarch, ia64_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch, ia64_memory_remove_breakpoint);
  set_gdbarch_breakpoint_from_pc (gdbarch, ia64_breakpoint_from_pc);
  set_gdbarch_read_pc (gdbarch, ia64_read_pc);
  set_gdbarch_write_pc (gdbarch, ia64_write_pc);

  /* Settings for calling functions in the inferior.  */
  set_gdbarch_use_generic_dummy_frames (gdbarch, 1);
  set_gdbarch_call_dummy_length (gdbarch, 0);
  set_gdbarch_push_arguments (gdbarch, ia64_push_arguments);
  set_gdbarch_push_return_address (gdbarch, ia64_push_return_address);
  set_gdbarch_pop_frame (gdbarch, ia64_pop_frame);

  set_gdbarch_call_dummy_p (gdbarch, 1);
  set_gdbarch_call_dummy_words (gdbarch, ia64_call_dummy_words);
  set_gdbarch_sizeof_call_dummy_words (gdbarch, sizeof (ia64_call_dummy_words));
  set_gdbarch_call_dummy_breakpoint_offset_p (gdbarch, 1);
  set_gdbarch_init_extra_frame_info (gdbarch, ia64_init_extra_frame_info);
  set_gdbarch_frame_args_address (gdbarch, ia64_frame_args_address);
  set_gdbarch_frame_locals_address (gdbarch, ia64_frame_locals_address);

  /* We won't necessarily have a frame pointer and even if we do,
     it winds up being extraordinarly messy when attempting to find
     the frame chain.  So for the purposes of creating frames (which
     is all read_fp() is used for), simply use the stack pointer value
     instead.  */
  set_gdbarch_read_fp (gdbarch, generic_target_read_sp);
  set_gdbarch_write_fp (gdbarch, generic_target_write_sp);

  /* Settings that should be unnecessary.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_read_sp (gdbarch, generic_target_read_sp);
  set_gdbarch_write_sp (gdbarch, generic_target_write_sp);

  set_gdbarch_call_dummy_location (gdbarch, AT_ENTRY_POINT);
  set_gdbarch_call_dummy_address (gdbarch, entry_point_address);
  set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 0);
  set_gdbarch_call_dummy_start_offset (gdbarch, 0);
  set_gdbarch_pc_in_call_dummy (gdbarch, generic_pc_in_call_dummy);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 0);
  set_gdbarch_push_dummy_frame (gdbarch, generic_push_dummy_frame);
  set_gdbarch_fix_call_dummy (gdbarch, generic_fix_call_dummy);

  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_function_start_offset (gdbarch, 0);

  set_gdbarch_remote_translate_xfer_address (
    gdbarch, ia64_remote_translate_xfer_address);

  return gdbarch;
}

void
_initialize_ia64_tdep (void)
{
  register_gdbarch_init (bfd_arch_ia64, ia64_gdbarch_init);

  tm_print_insn = print_insn_ia64;
  tm_print_insn_info.bytes_per_line = SLOT_MULTIPLIER;
}
