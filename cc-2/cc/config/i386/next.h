/* Target definitions for GNU compiler for Intel x86 CPU running NeXTSTEP
   Copyright (C) 1993 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* The macro NEXT_SEMANTICS is supposed to indicate that the *host*
   is OPENSTEP on Mach or Rhapsody, though currently this macro
   is commonly used to simply flag changes we have made to FSF's code.  */
#define NEXT_SEMANTICS

#include "i386/gas.h"
#include "next/nextstep.h"

#define MACHO_PIC 1

#ifndef MACHOPIC_PURE
#define MACHOPIC_PURE          (flag_pic == 2)
#define MACHOPIC_INDIRECT      (flag_pic)
#define MACHOPIC_JUST_INDIRECT (flag_pic == 1)
#endif

#define DEFAULT_TARGET_ARCH "i386"

/* By default, target has a 80387, with IEEE FP, and no mem->mem instructions.
   (The reason why the latter is required is not clear.)  */

#undef	TARGET_DEFAULT
#define TARGET_DEFAULT  (MASK_80387|MASK_IEEE_FP|MASK_NO_MOVE)

/* Copied from egcs 1.1.2.  */
#define MASK_SCHEDULE_PROLOGUE  000040000000    /* Emit prologue as rtl */

/* The -mschedule-prologue and -mno-schedule-prologue switches
   are unimplemented, but are listed here so the egcs specs file
   can be used with this version of the compiler.  */

#undef SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
  { "schedule-prologue",	 MASK_SCHEDULE_PROLOGUE },		\
  { "no-schedule-prologue",	-MASK_SCHEDULE_PROLOGUE },		\
  { "unaligned-text",		 0400 },				\
  { "no-unaligned-text",	-0400 },

#define TARGET_NO_LOOP_ALIGNMENT (target_flags & 0400)

/* According to the NEWS file, -fhandle-exceptions is known to be buggy in
   conjunction with -O.  One can certainly avoid a lot of problems by disabling
   inlining when -fhandle-exceptions is present on the command line.  */
#define CC1PLUS_SPEC "%{fhandle-exceptions:-fno-inline -fkeep-inline-functions}"

/* The NeXT configuration aligns everything at 4 byte boundary.
   Even though this is not optimal with respect to cache lines, this
   saves us sigificant space, which is a precious ressource on a
   NeXTSTEP machine. */

#undef ASM_OUTPUT_ALIGN_CODE
#define ASM_OUTPUT_ALIGN_CODE(FILE)			\
   if (!TARGET_NO_LOOP_ALIGNMENT)			\
     fprintf ((FILE), "\t.align 2,0x90\n")

/* Align start of loop at 4-byte boundary.  */

#undef ASM_OUTPUT_LOOP_ALIGN
#define ASM_OUTPUT_LOOP_ALIGN(FILE) \
   if (!TARGET_NO_LOOP_ALIGNMENT)			\
     fprintf ((FILE), "\t.align 2,0x90\n");  /* Use log of 4 as arg.  */

#undef ASM_OUTPUT_ALIGN
#undef	ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
 do { if ((LOG) != 0)			\
      if (in_text_section ()) 		\
	fprintf (FILE, "\t%s %d,0x90\n", ALIGN_ASM_OP, (LOG)); \
      else \
	fprintf (FILE, "\t%s %d\n", ALIGN_ASM_OP, (LOG)); \
    } while (0)
	

#undef FUNCTION_BOUNDARY
#define FUNCTION_BOUNDARY 32

#define TARGET_ARCHITECTURE \
  { { "i386", 2 },   /* Treat i386 like i486.  */	      	\
    { "i486", 2 },   /* Turn on -m486.  */			\
    { "i486SX", 2 }, /* Turn on -m486.  */			\
 /* { "i586", 4 },   */ /* Turn on -m486.  */			\
 /* { "i586SX", 4 }, */ /* Turn on -m586.  */                   \
 }

/* Implicit library calls should use memcpy, not bcopy, etc.  */

#define TARGET_MEM_FUNCTIONS

/* Machines that use the AT&T assembler syntax
   also return floating point values in an FP register.
   Define how to find the value returned by a function.
   VALTYPE is the data type of the value (as a tree).
   If the precise function being called is known, FUNC is its FUNCTION_DECL;
   otherwise, FUNC is 0.  */

#undef	VALUE_REGNO
#define VALUE_REGNO(MODE) \
  ((MODE) == SFmode || (MODE) == DFmode || (MODE) == XFmode	\
   ? FIRST_FLOAT_REG : 0)

/* 1 if N is a possible register number for a function value. */

#undef	FUNCTION_VALUE_REGNO_P
#define FUNCTION_VALUE_REGNO_P(N) ((N) == 0 || (N)== FIRST_FLOAT_REG)

#ifdef REAL_VALUE_TO_TARGET_LONG_DOUBLE
#undef	ASM_OUTPUT_LONG_DOUBLE
#define ASM_OUTPUT_LONG_DOUBLE(FILE,VALUE)				\
  do {									\
    long hex[3];							\
    REAL_VALUE_TO_TARGET_LONG_DOUBLE (VALUE, hex);			\
    if (sizeof (int) == sizeof (long))					\
      fprintf (FILE, "\t.long 0x%x\n\t.long 0x%x\n\t.long 0x%x\n",	\
		hex[0], hex[1], hex[2]);				\
    else								\
      fprintf (FILE, "\t.long 0x%lx\n\t.long 0x%lx\n\t.long 0x%lx\n",	\
		hex[0], hex[1], hex[2]);				\
  } while (0)
#endif

#ifdef REAL_VALUE_TO_TARGET_DOUBLE
#undef	ASM_OUTPUT_DOUBLE
#define ASM_OUTPUT_DOUBLE(FILE,VALUE)					\
  do {									\
    long hex[2];							\
    REAL_VALUE_TO_TARGET_DOUBLE (VALUE, hex);				\
    if (sizeof (int) == sizeof (long))					\
      fprintf (FILE, "\t.long 0x%x\n\t.long 0x%x\n", hex[0], hex[1]);	\
    else								\
      fprintf (FILE, "\t.long 0x%lx\n\t.long 0x%lx\n", hex[0], hex[1]);	\
  } while (0)
#endif

/* This is how to output an assembler line defining a `float' constant.  */

#ifdef REAL_VALUE_TO_TARGET_SINGLE
#undef	ASM_OUTPUT_FLOAT
#define ASM_OUTPUT_FLOAT(FILE,VALUE)					\
  do {									\
    long hex;								\
    REAL_VALUE_TO_TARGET_SINGLE (VALUE, hex);				\
    if (sizeof (int) == sizeof (long))					\
      fprintf (FILE, "\t.long 0x%x\n", hex);				\
    else								\
      fprintf (FILE, "\t.long 0x%lx\n", hex);				\
  } while (0)
#endif

/* A C statement or statements which output an assembler instruction
   opcode to the stdio stream STREAM.  The macro-operand PTR is a
   variable of type `char *' which points to the opcode name in its
   "internal" form--the form that is written in the machine description.

   GAS version 1.38.1 doesn't understand the `repz' opcode mnemonic.
   So use `repe' instead.  */

#undef	ASM_OUTPUT_OPCODE
#define ASM_OUTPUT_OPCODE(STREAM, PTR)	\
{							\
  if ((PTR)[0] == 'r'					\
      && (PTR)[1] == 'e'				\
      && (PTR)[2] == 'p')				\
    {							\
      if ((PTR)[3] == 'z')				\
	{						\
	  fprintf (STREAM, "repe");			\
	  (PTR) += 4;					\
	}						\
      else if ((PTR)[3] == 'n' && (PTR)[4] == 'z')	\
	{						\
	  fprintf (STREAM, "repne");			\
	  (PTR) += 5;					\
	}						\
    }							\
}

/* Define macro used to output shift-double opcodes when the shift
   count is in %cl.  Some assemblers require %cl as an argument;
   some don't.

   GAS requires the %cl argument, so override unx386.h. */

#undef	AS3_SHIFT_DOUBLE
#define AS3_SHIFT_DOUBLE(a,b,c,d) AS3 (a,b,c,d)

/* Print opcodes the way that GAS expects them. */
#define GAS_MNEMONICS 1

/* Names to predefine in the preprocessor for this target machine.  */

#undef	CPP_PREDEFINES
#ifdef  OPENSTEP
#define CPP_PREDEFINES "-Di386 -DNeXT -Dunix -D__MACH__ -D__LITTLE_ENDIAN__ -D__ARCHITECTURE__=\"i386\" "
#elif defined (MAC_OS_X_SERVER_1_0)
#define CPP_PREDEFINES "-Di386 -DNeXT -Dunix -D__MACH__ -D__LITTLE_ENDIAN__ -D__ARCHITECTURE__=\"i386\" -D__APPLE__"
#elif defined (MACOSX)
#define CPP_PREDEFINES "-D__i386__ -D__MACH__ -D__LITTLE_ENDIAN__ -D__APPLE__"
#else /* predefined macros in Mac OS X Server 1.1 */
#define CPP_PREDEFINES "-Di386 -D__MACH__ -D__LITTLE_ENDIAN__ -D__ARCHITECTURE__=\"i386\" -D__APPLE__"
#endif

/* This accounts for the return pc and saved fp on the i386. */

#define OBJC_FORWARDING_STACK_OFFSET 8
#define OBJC_FORWARDING_MIN_OFFSET 8

/* We do not want a dot in internal labels.  */

#undef LPREFIX
#define LPREFIX "L"

#undef	ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(BUF,PREFIX,NUMBER)	\
    sprintf ((BUF), "*%s%d", (PREFIX), (NUMBER))

#undef ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)	\
  fprintf (FILE, "%s%d:\n", PREFIX, NUM)

/* Output to assembler file text saying following lines
   may contain character constants, extra white space, comments, etc.  */

#undef	ASM_APP_ON
#define ASM_APP_ON "#APP\n"

/* Output to assembler file text saying following lines
   no longer contain unusual constructs.  */

#undef	ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

#undef	ASM_OUTPUT_REG_PUSH
#define ASM_OUTPUT_REG_PUSH(FILE,REGNO)  \
  fprintf (FILE, "\tpushl %se%s\n", "%", reg_names[REGNO])

#undef	ASM_OUTPUT_REG_POP
#define ASM_OUTPUT_REG_POP(FILE,REGNO)  \
  fprintf (FILE, "\tpopl %se%s\n", "%", reg_names[REGNO])

/* This is being overridden because the default i386 configuration
   generates calls to "_mcount".  NeXT system libraries all use
   "mcount".  */

#undef	FUNCTION_PROFILER
/* mcount needs to be called before establishing the PIC base, since the
   latter could otherwise be clobbered if held in a volatile register.  */
#if 0
#define FUNCTION_PROFILER(FILE, LABELNO)		\
{							\
  if (flag_pic)						\
    fprintf (FILE, "\tcall Lmcount$stub\n");		\
  else							\
    fprintf (FILE, "\tcall mcount\n");			\
}
#endif
#define FUNCTION_PROFILER(FILE, LABELNO)

/* BEGIN Calling Convention CHANGES */

/* These changes violate the Intel/Unix ABI.  Specifically, they
   change the way that space for a block return value is passed to a
   function.  The ABI says that the pointer is passed on the stack.
   We change to pass the pointer in %ebx.  This makes the NeXT
   Objective-C forwarding mechanism possible to implement on an i386.  */

/* Do NOT pass address of structure values on the stack.  */

#undef	STRUCT_VALUE_INCOMING
#undef	STRUCT_VALUE

/* Pass them in %ebx.  */

#undef	STRUCT_VALUE_REGNUM
#define STRUCT_VALUE_REGNUM 3

#undef PIC_OFFSET_TABLE_REGNUM 
#define PIC_OFFSET_TABLE_REGNUM  \
   ( pic_offset_table_rtx && GET_CODE (pic_offset_table_rtx) == REG \
     ? REGNO(pic_offset_table_rtx) \
     : -1)

/* The next four macros were copied from gcc 2.5.8.  */
#define GO_IF_INDEXABLE_BASE(X, ADDR)	\
 if (GET_CODE (X) == REG && REG_OK_FOR_BASE_P (X)) goto ADDR

#define LEGITIMATE_INDEX_REG_P(X)   \
  (GET_CODE (X) == REG && REG_OK_FOR_INDEX_P (X))

/* Return 1 if X is an index or an index times a scale.  */

#define LEGITIMATE_INDEX_P(X)   \
   (LEGITIMATE_INDEX_REG_P (X)				\
    || (GET_CODE (X) == MULT				\
	&& LEGITIMATE_INDEX_REG_P (XEXP (X, 0))		\
	&& GET_CODE (XEXP (X, 1)) == CONST_INT		\
	&& (INTVAL (XEXP (X, 1)) == 2			\
	    || INTVAL (XEXP (X, 1)) == 4		\
	    || INTVAL (XEXP (X, 1)) == 8)))

/* Go to ADDR if X is an index term, a base reg, or a sum of those.  */

#define GO_IF_INDEXING(X, ADDR)	\
{ if (LEGITIMATE_INDEX_P (X)) goto ADDR;				\
  GO_IF_INDEXABLE_BASE (X, ADDR);					\
  if (GET_CODE (X) == PLUS && LEGITIMATE_INDEX_P (XEXP (X, 0)))		\
    { GO_IF_INDEXABLE_BASE (XEXP (X, 1), ADDR); }			\
  if (GET_CODE (X) == PLUS && LEGITIMATE_INDEX_P (XEXP (X, 1)))		\
    { GO_IF_INDEXABLE_BASE (XEXP (X, 0), ADDR); } }

#undef GO_IF_LEGITIMATE_ADDRESS
#define GO_IF_LEGITIMATE_ADDRESS(MODE, X, ADDR)	\
{									\
  if (CONSTANT_ADDRESS_P (X)						\
      && (! flag_pic || LEGITIMATE_PIC_OPERAND_P (X)))			\
    goto ADDR;								\
  GO_IF_INDEXING (X, ADDR);						\
  if (GET_CODE (X) == PLUS && CONSTANT_ADDRESS_P (XEXP (X, 1)))		\
    {									\
      rtx x0 = XEXP (X, 0);						\
      rtx x1 = XEXP (X, 0);						\
      if (! flag_pic || LEGITIMATE_PIC_OPERAND_P (x1))			\
	{ GO_IF_INDEXING (x0, ADDR); }					\
    }									\
}

#undef INIT_EXPANDERS
#define INIT_EXPANDERS \
 do { pic_offset_table_rtx  = gen_reg_rtx (SImode); \
      clear_386_stack_locals (); \
    } while (0)

/* Value is the number of bytes of arguments automatically
   popped when returning from a subroutine call.
   FUNDECL is the declaration node of the function (as a tree),
   FUNTYPE is the data type of the function (as a tree),
   or for a library call it is an identifier node for the subroutine name.
   SIZE is the number of bytes of arguments passed on the stack.

   On the 80386, the RTD insn may be used to pop them if the number
     of args is fixed, but if the number is variable then the caller
     must pop them all.  RTD can't be used for library calls now
     because the library is compiled with the Unix compiler.
   Use of RTD is a selectable option, since it is incompatible with
   standard Unix calling sequences.  If the option is not selected,
   the caller must always pop the args.

   The attribute stdcall is equivalent to RTD on a per module basis.  */

/* Because we are passing the pointer in a register, we don't need to
   rely on the callee to pop it.  */

#undef	RETURN_POPS_ARGS
#define RETURN_POPS_ARGS(FUNDECL,FUNTYPE,SIZE)			\
  (TREE_CODE (FUNTYPE) == IDENTIFIER_NODE			\
   ? 0								\
   : (TARGET_RTD						\
      && (TYPE_ARG_TYPES (FUNTYPE) == 0				\
          || (TREE_VALUE (tree_last (TYPE_ARG_TYPES (FUNTYPE)))	\
              == void_type_node))) ? (SIZE) : 0)

/* END Calling Convention CHANGES */

/* Turn on floating point precision control as default */

/* #define DEFAULT_FPPC 1 */

#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL) \
  fprintf (FILE, "\t.long %s%d-%s%d\n",LPREFIX, VALUE,LPREFIX, REL)


#undef FINALIZE_PIC
#define FINALIZE_PIC i386_finalize_machopic ()

#define PIC_OFFSET_TABLE_RTX \
   (reload_in_progress ? (rtx)assign_386_stack_local (SImode, 2) : pic_offset_table_rtx)

#define RELOAD_PIC_REGISTER reload_i386_pic_register()
