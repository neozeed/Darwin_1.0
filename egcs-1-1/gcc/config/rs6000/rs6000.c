/* Subroutines used for code generation on IBM RS/6000.
   Copyright (C) 1991, 93-7, 1998 Free Software Foundation, Inc.
   Contributed by Richard Kenner (kenner@vlsi1.ultra.nyu.edu)

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
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-flags.h"
#include "insn-attr.h"
#include "flags.h"
#include "recog.h"
#include "tree.h"
#include "expr.h"
#include "obstack.h"
#include "except.h"
#include "function.h"
#include "output.h"
#include "toplev.h"

#ifdef MACHO_PIC
#include "apple/machopic.h"
#endif

#ifndef TARGET_NO_PROTOTYPE
#define TARGET_NO_PROTOTYPE 0
#endif

extern char *language_string;
extern int profile_block_flag;

#define min(A,B)	((A) < (B) ? (A) : (B))
#define max(A,B)	((A) > (B) ? (A) : (B))

/* Target cpu type */

enum processor_type rs6000_cpu;
struct rs6000_cpu_select rs6000_select[3] =
{
  /* switch	name,			tune	arch */
  { (char *)0,	"--with-cpu=",		1,	1 },
  { (char *)0,	"-mcpu=",		1,	1 },
  { (char *)0,	"-mtune=",		1,	0 },
};

/* Set to non-zero by "fix" operation to indicate that itrunc and
   uitrunc must be defined.  */

int rs6000_trunc_used;

/* Set to non-zero once they have been defined.  */

static int trunc_defined;

/* Set to non-zero once AIX common-mode calls have been defined.  */
static int common_mode_defined;

/* Save information from a "cmpxx" operation until the branch or scc is
   emitted.  */
rtx rs6000_compare_op0, rs6000_compare_op1;
int rs6000_compare_fp_p;

#ifdef USING_SVR4_H
/* Label number of label created for -mrelocatable, to call to so we can
   get the address of the GOT section */
int rs6000_pic_labelno;
int rs6000_pic_func_labelno;

/* Which abi to adhere to */
char *rs6000_abi_name = RS6000_ABI_NAME;

/* Semantics of the small data area */
enum rs6000_sdata_type rs6000_sdata = SDATA_DATA;

/* Which small data model to use */
char *rs6000_sdata_name = (char *)0;
#endif

/* Whether a System V.4 varargs area was created.  */
int rs6000_sysv_varargs_p;

/* ABI enumeration available for subtarget to use.  */
enum rs6000_abi rs6000_current_abi;

/* Offset & size for fpmem stack locations used for converting between
   float and integral types.  */
int rs6000_fpmem_offset;
int rs6000_fpmem_size;

/* Debug flags */
char *rs6000_debug_name;
int rs6000_debug_stack;		/* debug stack applications */
int rs6000_debug_arg;		/* debug argument handling */

/* Flag to say the TOC is initialized */
int toc_initialized;

/* Flag to say whether a function is being inlined */
int rs6000_inlining;

/* AltiVec Programming Model.  */

struct builtin {
  /* A pointer to the type of the parameters for the builtin function.
     There are at most three parameters.  */
  tree *args[3];
  /* A string giving the constraint letters for each parameter.  */
  char *constraints;
  /* A pointer to the type of the result of the builtin function.  */
  tree *result;
  /* The number of parameters for the builtin function.  */
  int n_args : 8;
  /* 1 if any pointer parameter may point to a const qualified version
     of the same type.  */
  unsigned const_ptr_ok : 1;
  /* 1 if any pointer parameter may point to a volatile qualified version
     of the same type.  */
  unsigned volatile_ptr_ok : 1;
  /* A nonzero value listed in enum builtin_optimize indicating the
     equivalences for the given builtin function.  */
  unsigned optimize : 4;
  /* A unique mangled name for the builtin function.  (The mangling is
     of the form <function>:<index>.)  */
  char *name;
  /* The spelling of the instruction corresponding to the builtin function.  */
  char *insn_name;
  /* The insn code for the builtin function.  */
  int icode;
  /* The assigned built-in function code for the builtin function.  */
  enum built_in_function fcode;
  /* The FUNCTION_DECL created for the builtin function.  */
  tree decl;
};

#define BUILTIN_arg(b,i)		(b)->args[i]
#define BUILTIN_constraint(b,i)		(b)->constraints[i]
#define BUILTIN_result(b)		(b)->result
#define BUILTIN_name(b)			(b)->name
#define BUILTIN_insn_name(b)		(b)->insn_name
#define BUILTIN_n_args(b)		(b)->n_args
#define BUILTIN_const_ptr_ok(b)		(b)->const_ptr_ok
#define BUILTIN_volatile_ptr_ok(b)	(b)->volatile_ptr_ok
#define BUILTIN_optimize(b)		((enum builtin_optimize)(b)->optimize)
#define BUILTIN_icode(b)		(b)->icode
#define BUILTIN_fcode(b)		(b)->fcode
#define BUILTIN_to_DECL(b)		(b)->decl

/* These values are encoded by ops-to-gp into the vec.h table.  */
enum builtin_optimize {
  BUILTIN_zero_if_same = 1,
  BUILTIN_copy_if_same = 2,
  BUILTIN_vsldoi = 3,
  BUILTIN_vspltisb = 4,
  BUILTIN_vspltish = 5,
  BUILTIN_vspltisw = 6,
  BUILTIN_ones_if_same = 7,
  BUILTIN_lvsl = 8,
  BUILTIN_lvsr = 9
};

extern struct builtin *Builtin[];
#define DECL_to_BUILTIN(d)		 \
  Builtin[DECL_FUNCTION_CODE(d) - BUILT_IN_FIRST_TARGET_INTRINSIC]

struct overloadx {
  /* The name of the overloaded builtin function.  */
  char *name;
  /* The number of overloads for this name.  1 if the function has a
     unique signature.  */
  int n_fcns;
  /* The number of arguments to each overload.  ops-to-gp validates that
     this is the same for each overload.  */
  int n_args;
  /* An array of builtin function descriptors.  */
  struct builtin **functions;
  /* The assigned built-in function code for the overloaded builtin
     function.  */
  enum built_in_function fcode;
  /* The FUNCTION_DECL created for the builtin function.  */
  tree decl;
};

#define OVERLOAD_name(o)		(o)->name
#define OVERLOAD_n_fcns(o)		(o)->n_fcns
#define OVERLOAD_n_args(o)		(o)->n_args
#define OVERLOAD_functions(o)		(o)->functions
#define OVERLOAD_fcode(o)		(o)->fcode
#define OVERLOAD_to_DECL(o)		(o)->decl

extern struct overloadx Overload[];
#define DECL_to_TARGET_OVERLOADED_INTRINSIC(d)		 \
  Overload[DECL_FUNCTION_CODE(d) - BUILT_IN_FIRST_TARGET_OVERLOADED_INTRINSIC]

/* Regard two pointer types T1 and T2 as the same by ignoring const
   and volatile qualifiers as specified by SELF.  */

static int
funny_pointer_check (self, t1, t2)
     struct builtin *self;
     tree t1;
     tree t2;
{
  if (!BUILTIN_const_ptr_ok(self)
      && TYPE_READONLY (t2))
    return 0;
  if (!BUILTIN_volatile_ptr_ok(self)
      && TYPE_VOLATILE (t2))
    return 0;
  return lang_comptypes (t1, lang_build_type_variant (t2, 0, 0));
}

/* A call to the FUNCTION_DECL FUNCTION with actual arguments PARAMS
   is known to be a target-specific overloaded intrinsic.  Validate
   the call and return the tree representing the selected overload
   or NULL if the call is invalid.  */

tree
select_target_overloaded_intrinsic (function, params)
     tree function;
     tree params;
{
  extern tree default_conversion (tree);
  tree parm;
  struct overloadx *o = &DECL_to_TARGET_OVERLOADED_INTRINSIC (function);
  int idx, i, match = -1;

  if (list_length (params) == OVERLOAD_n_args (o))
    {
      /* Search for a parameter list that matches.  */
      for (idx = 0; idx < OVERLOAD_n_fcns (o); idx++)
	{
	  struct builtin *self = OVERLOAD_functions (o)[idx];
	  for (parm = params, i = 0; parm; parm = TREE_CHAIN (parm), i++)
	    {
	      tree t1 = *BUILTIN_arg (self, i);
	      tree t2;
	      tree val = TREE_VALUE (parm);
	      if (TREE_CODE (val) == NON_LVALUE_EXPR)
		val = TREE_OPERAND (val, 0);
	      if (TREE_CODE (TREE_TYPE (val)) == ARRAY_TYPE
		  || TREE_CODE (TREE_TYPE (val)) == FUNCTION_TYPE)
		val = default_conversion (val);
	      t2 = TREE_TYPE (val);
	      if (TREE_CODE (val) == ERROR_MARK)
		return NULL_TREE;
              /* Parameters match if they are identical or if they are both
		 integral, or if both are similar enough pointers for the
		 specific overloaded function (i.e. some allow pointers to
		 const and/or volatile qualified types.  */
	      if (!(lang_comptypes (t1, t2)
		    || (POINTER_TYPE_P (t1)
			&& POINTER_TYPE_P (t2)
			&& funny_pointer_check(self,
					       TREE_TYPE (t1),
					       TREE_TYPE (t2)))
		    || (INTEGRAL_TYPE_P (t1)
			&& INTEGRAL_TYPE_P (t2))))
		goto fail;
	    }
	  /* If there is more than one match, the tables are incorrect.  There
	     shouldn't be any ambiguous overloaded builtin functions.  */
	  if (match >= 0)
	    abort();
	  match = idx;
	fail:;
	}
    }
  if (match < 0) {
    error("no instance of overloaded builtin function `%s' matches the parameter list",
	  OVERLOAD_name(o));
    return NULL_TREE;
  }
  /* Substitute the specific overloaded builtin function that we selected.  */
  return BUILTIN_to_DECL (OVERLOAD_functions (o)[match]);
}

/* Expand the call to the FUNCTION_DECL FNDECL, is known to be a
   target-specific intrinsic, with arguments ARGLIST.  Put the result
   in TARGET if that's convenient (and in mode MODE if that's
   convenient).  */

rtx
expand_target_intrinsic (fndecl, target, mode, arglist)
     tree fndecl;
     rtx target;
     enum machine_mode mode;
     tree arglist;
{
  tree name = DECL_ASSEMBLER_NAME (fndecl);
  struct builtin *b = DECL_to_BUILTIN (fndecl);
  rtx ops[5], insns;
  tree t;
  int i, n, c;

  /* Check for various constant folding with the AltiVec builtin's.  */
  switch (BUILTIN_optimize (b))
    {
    case BUILTIN_zero_if_same:
      /* If the two operands are the same, the result is the zero vector.  */
      if (list_length (arglist) == 2
	  && operand_equal_p (TREE_VALUE (arglist),
			      TREE_VALUE (TREE_CHAIN (arglist)), 0))
	{
	  t = build_vector (0, integer_zero_node, integer_zero_node,
			    integer_zero_node, integer_zero_node);
	  return immed_vector_const (t);
	}
      break;
    case BUILTIN_copy_if_same:
      /* If the two operands are the same, the result is the same as either
	 operand.  */
      if (list_length (arglist) == 2
	  && operand_equal_p (TREE_VALUE (arglist),
			      TREE_VALUE (TREE_CHAIN (arglist)), 0))
	{
	  return expand_expr (TREE_VALUE (arglist), NULL_RTX, VOIDmode, 0);
	}
      break;
    case BUILTIN_ones_if_same:
      /* If the two operands are the same, the result is the vector with
	 all bits set.  */
      if (list_length (arglist) == 2
	  && operand_equal_p (TREE_VALUE (arglist),
			      TREE_VALUE (TREE_CHAIN (arglist)), 0))
	{
	  t = build_int_2 (-1, 0);
	  return immed_vector_const (build_vector (0, t, t, t, t));
	}
      break;
    case BUILTIN_vsldoi:
      /* vec_vsldoi(a,a,0) is the same as a.  */
      if (list_length (arglist) == 3
	  && operand_equal_p (TREE_VALUE (arglist),
			      TREE_VALUE (TREE_CHAIN (arglist)), 0))
	{
	  t = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
	  if (TREE_CODE (t) == INTEGER_CST
	      && TREE_INT_CST_LOW (t) == 0
	      && TREE_INT_CST_HIGH (t) == 0)
	    return expand_expr (TREE_VALUE (arglist), NULL_RTX, VOIDmode, 0);
	}
      break;
    case BUILTIN_vspltisb:
    case BUILTIN_vspltish:
    case BUILTIN_vspltisw:
      /* Change each of these into the appropriate vector constant.  */
      if (list_length (arglist) == 1
	  && TREE_CODE (TREE_VALUE (arglist)) == INTEGER_CST)
	{
	  i = TREE_INT_CST_LOW (TREE_VALUE (arglist));
	  switch (BUILTIN_optimize (b))
	    {
	    case BUILTIN_vspltisb:
	      i = (i & 0xff) | ((i & 0xff) << 8);
	    case BUILTIN_vspltish:
	      i = (i & 0xffff) | ((i & 0xffff) << 16);
	    }
	  t = build_int_2 (i, 0);
	  return immed_vector_const (build_vector (0, t, t, t, t));
	}
      break;
    case BUILTIN_lvsl:
    case BUILTIN_lvsr:
      /* Change each of these into the appropriate vector constant.  */
      if (list_length (arglist) == 2
	  && TREE_CODE (TREE_VALUE (arglist)) == INTEGER_CST
	  && TREE_CODE (TREE_VALUE (TREE_CHAIN (arglist))) == INTEGER_CST)
	{
	  i = (TREE_INT_CST_LOW (TREE_VALUE (arglist))
	       + TREE_INT_CST_LOW (TREE_VALUE (TREE_CHAIN (arglist)))) & 0xf;
	  if (BUILTIN_optimize (b) == BUILTIN_lvsr)
	    i = 0x10 - i;
	  i = (i << 8) + i + 0x01;
	  i = (i << 16) + i + 0x0202;
	  t = build_vector (0,
			    build_int_2 (i, 0),
			    build_int_2 (i + 0x04040404, 0),
			    build_int_2 (i + 0x04040404 * 2, 0),
			    build_int_2 (i + 0x04040404 * 3, 0));
	  return immed_vector_const (t);
	}
      break;
    }

  /* Expand each operand and check the constraints should the builtin function
     use them.  */
  for (t = arglist, n = 0; t; t = TREE_CHAIN (t), n++)
    {
      ops[n] = expand_expr (TREE_VALUE (t), NULL_RTX, VOIDmode, 0);
      switch (c = BUILTIN_constraint(b, n))
	{
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	  if (!(GET_CODE (ops[n]) == CONST_INT
		&& CONST_OK_FOR_LETTER_P (INTVAL (ops[n]), c)))
	    {
	      error ("argument %d to built-in function `%s' does not satisfy constraint %c",
		     n, BUILTIN_name(b), c);
	      ops[n] = const0_rtx;
	    }
	  break;
	case 'x':
	  if (TREE_CODE (TREE_VALUE (t)) == ERROR_MARK)
	    ops[n] = immed_vector_const (build_vector (0, 
						       integer_zero_node,
						       integer_zero_node,
						       integer_zero_node,
						       integer_zero_node));
	  break;
	}
    }
  /* Add an additional operand that gives the spelling of the actual
     instruction to be used.  */
  ops[n++] = gen_rtx (SYMBOL_REF, Pmode, BUILTIN_insn_name (b));

  emit_queue ();
  start_sequence ();
  {
    rtx pat;
    int icode = BUILTIN_icode (b);
    enum machine_mode *modes = &insn_operand_mode[icode][0];
    int has_result = (TREE_CODE(*BUILTIN_result(b)) != VOID_TYPE);

    /* Check the predicates for the insn associated with the builtin.  */
    for (i = 0; i < n; i++)
      if (! (*insn_operand_predicate[icode][has_result+i]) (ops[i], modes[i]))
	ops[i] = copy_to_mode_reg (modes[has_result+i], ops[i]);

    /* Generate the insn that computes the builtin.  */
    if (has_result)
      {
	if (!target
	    || ! (*insn_operand_predicate[icode][0]) (target, mode))
	  target = gen_reg_rtx (mode);
	if (n == 1)
	  pat = GEN_FCN (icode) (target, ops[0]);
	else if (n == 2)
	  pat = GEN_FCN (icode) (target, ops[0], ops[1]);
	else if (n == 3)
	  pat = GEN_FCN (icode) (target, ops[0], ops[1], ops[2]);
	else if (n == 4)
	  pat = GEN_FCN (icode) (target, ops[0], ops[1], ops[2], ops[3]);
	else
	  abort ();
      }
    else
      {
	target = NULL_RTX;
	if (n == 1)
	  pat = GEN_FCN (icode) (ops[0]);
	else if (n == 2)
	  pat = GEN_FCN (icode) (ops[0], ops[1]);
	else if (n == 3)
	  pat = GEN_FCN (icode) (ops[0], ops[1], ops[2]);
	else if (n == 4)
	  pat = GEN_FCN (icode) (ops[0], ops[1], ops[2], ops[3]);
	else
	  abort ();
      }
    emit_insn (pat);
  }
  insns = get_insns ();
  end_sequence ();
  emit_insns (insns);

  return target;
}

/* From c-decl.c or cp-decl.c  */
extern tree float_type_node;
extern tree long_integer_type_node;
extern tree short_integer_type_node;
extern tree signed_char_type_node;
extern tree short_unsigned_type_node;
extern tree long_unsigned_type_node;
extern tree unsigned_char_type_node;

extern tree vector_unsigned_char_type_node;
extern tree vector_signed_char_type_node;
extern tree vector_boolean_char_type_node;
extern tree vector_unsigned_short_type_node;
extern tree vector_signed_short_type_node;
extern tree vector_boolean_short_type_node;
extern tree vector_unsigned_long_type_node;
extern tree vector_signed_long_type_node;
extern tree vector_boolean_long_type_node;
extern tree vector_float_type_node;
extern tree vector_pixel_type_node;

/* Type nodes specific to the set of AltiVec intrinsics.  */
tree float_ptr_type_node;
tree integer_ptr_type_node;
tree long_integer_ptr_type_node;
tree short_integer_ptr_type_node;
tree signed_char_ptr_type_node;
tree short_unsigned_ptr_type_node;
tree long_unsigned_ptr_type_node;
tree unsigned_char_ptr_type_node;
tree unsigned_ptr_type_node;
tree vector_boolean_char_ptr_type_node;
tree vector_boolean_long_ptr_type_node;
tree vector_boolean_short_ptr_type_node;
tree vector_float_ptr_type_node;
tree vector_pixel_ptr_type_node;
tree vector_signed_char_ptr_type_node;
tree vector_signed_long_ptr_type_node;
tree vector_signed_short_ptr_type_node;
tree vector_unsigned_char_ptr_type_node;
tree vector_unsigned_long_ptr_type_node;
tree vector_unsigned_short_ptr_type_node;

/* Macros to map the names used in the intrinsic table.  */
#define B_UID(X) \
  ((enum built_in_function)(BUILT_IN_FIRST_TARGET_INTRINSIC+(X)))
#define O_UID(X) \
  ((enum built_in_function)(BUILT_IN_FIRST_TARGET_OVERLOADED_INTRINSIC+(X)))

#define T_char_ptr		char_ptr_type_node
#define T_float_ptr		float_ptr_type_node
#define T_int			integer_type_node
#define T_int_ptr		integer_ptr_type_node
#define T_long_ptr		long_integer_ptr_type_node
#define T_short_ptr		short_integer_ptr_type_node
#define T_signed_char_ptr	signed_char_ptr_type_node
#define T_unsigned_char_ptr	unsigned_char_ptr_type_node
#define T_unsigned_int_ptr	unsigned_ptr_type_node
#define T_unsigned_long_ptr	long_unsigned_ptr_type_node
#define T_unsigned_short_ptr	short_unsigned_ptr_type_node
#define T_vec_b16		vector_boolean_short_type_node
#define T_vec_b16_ptr		vector_boolean_short_ptr_type_node
#define T_vec_b32		vector_boolean_long_type_node
#define T_vec_b32_ptr		vector_boolean_long_ptr_type_node
#define T_vec_b8		vector_boolean_char_type_node
#define T_vec_b8_ptr		vector_boolean_char_ptr_type_node
#define T_vec_f32		vector_float_type_node
#define T_vec_f32_ptr		vector_float_ptr_type_node
#define T_vec_p16		vector_pixel_type_node
#define T_vec_p16_ptr		vector_pixel_ptr_type_node
#define T_vec_s16		vector_signed_short_type_node
#define T_vec_s16_ptr		vector_signed_short_ptr_type_node
#define T_vec_s32		vector_signed_long_type_node
#define T_vec_s32_ptr		vector_signed_long_ptr_type_node
#define T_vec_s8		vector_signed_char_type_node
#define T_vec_s8_ptr		vector_signed_char_ptr_type_node
#define T_vec_u16		vector_unsigned_short_type_node
#define T_vec_u16_ptr		vector_unsigned_short_ptr_type_node
#define T_vec_u32		vector_unsigned_long_type_node
#define T_vec_u32_ptr		vector_unsigned_long_ptr_type_node
#define T_vec_u8		vector_unsigned_char_type_node
#define T_vec_u8_ptr		vector_unsigned_char_ptr_type_node
#define T_void			void_type_node
#define T_volatile_vec_u16	T_vec_u16
#define T_cc24f			T_int
#define T_cc24fd		T_int
#define T_cc24fr		T_int
#define T_cc24t			T_int
#define T_cc24td		T_int
#define T_cc24tr		T_int
#define T_cc26f			T_int
#define T_cc26fd		T_int
#define T_cc26fr		T_int
#define T_cc26t			T_int
#define T_cc26td		T_int
#define T_cc26tr		T_int
#define T_immed_s5		T_int
#define T_immed_u2		T_int
#define T_immed_u4		T_int
#define T_immed_u5		T_int
#define T_volatile_void		T_void

#include "vec.h"

/* Return the parameter list for builtin function B using ENDLINK as the end
   of the prototype.  */

static tree
altivec_ftype (b, endlink)
     struct builtin *b;
     tree endlink;
{
  tree parms = endlink;
  int i;

  for (i = BUILTIN_n_args(b) - 1; i >= 0; i--)
    parms = tree_cons (NULL_TREE, *BUILTIN_arg(b, i), parms);

  return build_function_type (*BUILTIN_result(b), parms);
}

/* Initialize the AltiVec builtin functions using ENDLINK as the end of the
   prototypes created.  */

void
init_target_intrinsic (endlink, flag_vec)
     tree endlink;
     int flag_vec;
{
  tree decl;
  struct overloadx *o;
  struct builtin *b;
  int i;
  tree void_ftype_any;

  if ((int)LAST_B_UID > (int)BUILT_IN_LAST_TARGET_INTRINSIC)
    /* Increase BUILT_IN_LAST_TARGET_INTRINSIC.  */
    abort ();

  if ((int)LAST_O_UID > (int)BUILT_IN_LAST_TARGET_OVERLOADED_INTRINSIC)
    /* Increase BUILT_IN_LAST_TARGET_OVERLOADED_INTRINSIC.  */
    abort();

  /* These only apply if -fvec is specified.  */
  if (!flag_vec)
    return;

  /* Additional types needed for the set of intrinsics.  */
  void_ftype_any = build_function_type (void_type_node, NULL_TREE);
  float_ptr_type_node = build_pointer_type (float_type_node);
  integer_ptr_type_node = build_pointer_type (integer_type_node);
  long_integer_ptr_type_node = build_pointer_type (long_integer_type_node);
  short_integer_ptr_type_node = build_pointer_type (short_integer_type_node);
  signed_char_ptr_type_node = build_pointer_type (signed_char_type_node);
  short_unsigned_ptr_type_node = build_pointer_type (short_unsigned_type_node);
  long_unsigned_ptr_type_node = build_pointer_type (long_unsigned_type_node);
  unsigned_char_ptr_type_node = build_pointer_type (unsigned_char_type_node);
  unsigned_ptr_type_node = build_pointer_type (unsigned_type_node);
  vector_boolean_char_ptr_type_node = build_pointer_type (vector_boolean_char_type_node);
  vector_boolean_long_ptr_type_node = build_pointer_type (vector_boolean_long_type_node);
  vector_boolean_short_ptr_type_node = build_pointer_type (vector_boolean_short_type_node);
  vector_float_ptr_type_node = build_pointer_type (vector_float_type_node);
  vector_pixel_ptr_type_node = build_pointer_type (vector_pixel_type_node);
  vector_signed_char_ptr_type_node = build_pointer_type (vector_signed_char_type_node);
  vector_signed_long_ptr_type_node = build_pointer_type (vector_signed_long_type_node);
  vector_signed_short_ptr_type_node = build_pointer_type (vector_signed_short_type_node);
  vector_unsigned_char_ptr_type_node = build_pointer_type (vector_unsigned_char_type_node);
  vector_unsigned_long_ptr_type_node = build_pointer_type (vector_unsigned_long_type_node);
  vector_unsigned_short_ptr_type_node = build_pointer_type (vector_unsigned_short_type_node);

  /* Walk the Overload table.  */
  for (o = Overload; OVERLOAD_name (o); o++)
    {
      /* Walk each builtin for the overload.  */
      for (i = 0; i < OVERLOAD_n_fcns(o); i++)
	{
	  /* Some overloads map to the same builtin.  Only declare the
	     function once.  */
	  b = OVERLOAD_functions (o)[i];
	  if ((decl = BUILTIN_to_DECL (b)) == NULL_TREE)
	    {
	      /* Declare the function and record it's declaration.  */
	      decl = lang_builtin_function (BUILTIN_name (b),
					    altivec_ftype (b, endlink),
					    BUILTIN_fcode (b),
					    BUILTIN_insn_name (b));
	      BUILTIN_to_DECL (b) = decl;
	    }
	}
      /* If the function is overloaded or has a single overload with a
	 different name, record the overload name as an overloaded
	 builtin.  Give it a prototype that will match any set of
	 parameters.  */
      if (OVERLOAD_n_fcns (o) > 1
	  || strcmp(BUILTIN_name (b), OVERLOAD_name (o)) != 0)
	decl = lang_builtin_function (OVERLOAD_name (o),
				      void_ftype_any,
				      OVERLOAD_fcode (o),
				      NULL_PTR);
      /* Record the declaration of the overload (or non-overloaded
	 and unique name declared as a non-overloaded builtin).  */
      OVERLOAD_to_DECL (o) = decl;
    }
}

/* Default register names.  */
char rs6000_reg_names[][8] =
{
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "8",  "9", "10", "11", "12", "13", "14", "15",
     "16", "17", "18", "19", "20", "21", "22", "23",
     "24", "25", "26", "27", "28", "29", "30", "31",
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "8",  "9", "10", "11", "12", "13", "14", "15",
     "16", "17", "18", "19", "20", "21", "22", "23",
     "24", "25", "26", "27", "28", "29", "30", "31",
     "mq", "lr", "ctr","ap",
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
  "fpmem", "vrsave",
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "8",  "9", "10", "11", "12", "13", "14", "15",
     "16", "17", "18", "19", "20", "21", "22", "23",
     "24", "25", "26", "27", "28", "29", "30", "31"
};

#ifdef TARGET_REGNAMES
static char alt_reg_names[][8] =
{
   "%r0",   "%r1",  "%r2",  "%r3",  "%r4",  "%r5",  "%r6",  "%r7",
   "%r8",   "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
  "%r16",  "%r17", "%r18", "%r19", "%r20", "%r21", "%r22", "%r23",
  "%r24",  "%r25", "%r26", "%r27", "%r28", "%r29", "%r30", "%r31",
   "%f0",   "%f1",  "%f2",  "%f3",  "%f4",  "%f5",  "%f6",  "%f7",
   "%f8",   "%f9", "%f10", "%f11", "%f12", "%f13", "%f14", "%f15",
  "%f16",  "%f17", "%f18", "%f19", "%f20", "%f21", "%f22", "%f23",
  "%f24",  "%f25", "%f26", "%f27", "%f28", "%f29", "%f30", "%f31",
    "mq",    "lr",  "ctr",   "ap",
  "%cr0",  "%cr1", "%cr2", "%cr3", "%cr4", "%cr5", "%cr6", "%cr7",
 "fpmem", "vrsave",
   "%v0",   "%v1",  "%v2",  "%v3",  "%v4",  "%v5",  "%v6",  "%v7",
   "%v8",   "%v9", "%v10", "%v11", "%v12", "%v13", "%v14", "%v15",
  "%v16",  "%v17", "%v18", "%v19", "%v20", "%v21", "%v22", "%v23",
  "%v24",  "%v25", "%v26", "%v27", "%v28", "%v29", "%v30", "%v31"
};
#endif

#ifndef MASK_STRICT_ALIGN
#define MASK_STRICT_ALIGN 0
#endif

/* Override command line options.  Mostly we process the processor
   type and sometimes adjust other TARGET_ options.  */

void
rs6000_override_options (default_cpu)
     char *default_cpu;
{
  size_t i, j;
  struct rs6000_cpu_select *ptr;

  /* Simplify the entries below by making a mask for any POWER
     variant and any PowerPC variant.  */

#define POWER_MASKS (MASK_POWER | MASK_POWER2 | MASK_MULTIPLE | MASK_STRING)
#define POWERPC_MASKS (MASK_POWERPC | MASK_PPC_GPOPT \
		       | MASK_PPC_GFXOPT | MASK_POWERPC64)
#define POWERPC_OPT_MASKS (MASK_PPC_GPOPT | MASK_PPC_GFXOPT)

  static struct ptt
    {
      char *name;		/* Canonical processor name.  */
      enum processor_type processor; /* Processor type enum value.  */
      int target_enable;	/* Target flags to enable.  */
      int target_disable;	/* Target flags to disable.  */
    } processor_target_table[]
      = {{"common", PROCESSOR_COMMON, MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_MASKS},
	 {"power", PROCESSOR_POWER,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"power2", PROCESSOR_POWER,
	    MASK_POWER | MASK_POWER2 | MASK_MULTIPLE | MASK_STRING,
	    POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"powerpc", PROCESSOR_POWERPC,
	    MASK_POWERPC | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"rios", PROCESSOR_RIOS1,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rios1", PROCESSOR_RIOS1,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rsc", PROCESSOR_PPC601,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rsc1", PROCESSOR_PPC601,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rios2", PROCESSOR_RIOS2,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING | MASK_POWER2,
	    POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"403", PROCESSOR_PPC403,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS | MASK_STRICT_ALIGN,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"505", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"601", PROCESSOR_PPC601,
	    MASK_POWER | MASK_POWERPC | MASK_NEW_MNEMONICS | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"602", PROCESSOR_PPC603,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"603", PROCESSOR_PPC603,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"603e", PROCESSOR_PPC603,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"604", PROCESSOR_PPC604,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"604e", PROCESSOR_PPC604,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"620", PROCESSOR_PPC620,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT},
	 {"801", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"821", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"823", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"860", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64}};

  size_t ptt_size = sizeof (processor_target_table) / sizeof (struct ptt);

  int multiple = TARGET_MULTIPLE;	/* save current -mmultiple/-mno-multiple status */
  int string   = TARGET_STRING;		/* save current -mstring/-mno-string status */

  profile_block_flag = 0;

  /* Identify the processor type */
  rs6000_select[0].string = default_cpu;
  rs6000_cpu = PROCESSOR_DEFAULT;

  for (i = 0; i < sizeof (rs6000_select) / sizeof (rs6000_select[0]); i++)
    {
      ptr = &rs6000_select[i];
      if (ptr->string != (char *)0 && ptr->string[0] != '\0')
	{
	  for (j = 0; j < ptt_size; j++)
	    if (! strcmp (ptr->string, processor_target_table[j].name))
	      {
		if (ptr->set_tune_p)
		  rs6000_cpu = processor_target_table[j].processor;

		if (ptr->set_arch_p)
		  {
		    target_flags |= processor_target_table[j].target_enable;
		    target_flags &= ~processor_target_table[j].target_disable;
		  }
		break;
	      }

	  if (i == ptt_size)
	    error ("bad value (%s) for %s switch", ptr->string, ptr->name);
	}
    }

  /* If we are optimizing big endian systems for space, use the
     store multiple instructions.  */
  if (BYTES_BIG_ENDIAN && optimize_size)
    target_flags |= MASK_MULTIPLE;

  /* If -mmultiple or -mno-multiple was explicitly used, don't
     override with the processor default */
  if (TARGET_MULTIPLE_SET)
    target_flags = (target_flags & ~MASK_MULTIPLE) | multiple;

  /* If -mstring or -mno-string was explicitly used, don't
     override with the processor default */
  if (TARGET_STRING_SET)
    target_flags = (target_flags & ~MASK_STRING) | string;

  /* Don't allow -mmultiple or -mstring on little endian systems, because the
     hardware doesn't support the instructions used in little endian mode */
  if (!BYTES_BIG_ENDIAN)
    {
      if (TARGET_MULTIPLE)
	{
	  target_flags &= ~MASK_MULTIPLE;
	  if (TARGET_MULTIPLE_SET)
	    warning ("-mmultiple is not supported on little endian systems");
	}

      if (TARGET_STRING)
	{
	  target_flags &= ~MASK_STRING;
	  if (TARGET_STRING_SET)
	    warning ("-mstring is not supported on little endian systems");
	}
    }

  if (flag_pic && (DEFAULT_ABI == ABI_AIX))
    {
      warning ("-f%s ignored for AIX (all code is position independent)",
	       (flag_pic > 1) ? "PIC" : "pic");
      flag_pic = 0;
    }

  /* Set debug flags */
  if (rs6000_debug_name)
    {
      if (!strcmp (rs6000_debug_name, "all"))
	rs6000_debug_stack = rs6000_debug_arg = 1;
      else if (!strcmp (rs6000_debug_name, "stack"))
	rs6000_debug_stack = 1;
      else if (!strcmp (rs6000_debug_name, "arg"))
	rs6000_debug_arg = 1;
      else
	error ("Unknown -mdebug-%s switch", rs6000_debug_name);
    }

#ifdef TARGET_REGNAMES
  /* If the user desires alternate register names, copy in the alternate names
     now.  */
  if (TARGET_REGNAMES)
    bcopy ((char *)alt_reg_names, (char *)rs6000_reg_names, sizeof (rs6000_reg_names));
#endif

#ifdef SUBTARGET_OVERRIDE_OPTIONS
  SUBTARGET_OVERRIDE_OPTIONS;
#endif
}

/* Do anything needed at the start of the asm file.  */

void
rs6000_file_start (file, default_cpu)
     FILE *file;
     char *default_cpu;
{
  size_t i;
  char buffer[80];
  char *start = buffer;
  struct rs6000_cpu_select *ptr;

  if (flag_verbose_asm)
    {
      sprintf (buffer, "\n%s rs6000/powerpc options:", ASM_COMMENT_START);
      rs6000_select[0].string = default_cpu;

      for (i = 0; i < sizeof (rs6000_select) / sizeof (rs6000_select[0]); i++)
	{
	  ptr = &rs6000_select[i];
	  if (ptr->string != (char *)0 && ptr->string[0] != '\0')
	    {
	      fprintf (file, "%s %s%s", start, ptr->name, ptr->string);
	      start = "";
	    }
	}

#ifdef USING_SVR4_H
      switch (rs6000_sdata)
	{
	case SDATA_NONE: fprintf (file, "%s -msdata=none", start); start = ""; break;
	case SDATA_DATA: fprintf (file, "%s -msdata=data", start); start = ""; break;
	case SDATA_SYSV: fprintf (file, "%s -msdata=sysv", start); start = ""; break;
	case SDATA_EABI: fprintf (file, "%s -msdata=eabi", start); start = ""; break;
	}

      if (rs6000_sdata && g_switch_value)
	{
	  fprintf (file, "%s -G %d", start, g_switch_value);
	  start = "";
	}
#endif

      if (*start == '\0')
	fputs ("\n", file);
    }
}


/* Create a CONST_DOUBLE from a string.  */

struct rtx_def *
rs6000_float_const (string, mode)
     char *string;
     enum machine_mode mode;
{
  REAL_VALUE_TYPE value = REAL_VALUE_ATOF (string, mode);
  return immed_real_const_1 (value, mode);
}

/* Create a CONST_DOUBLE like immed_double_const, except reverse the
   two parts of the constant if the target is little endian.  */

struct rtx_def *
rs6000_immed_double_const (i0, i1, mode)
     HOST_WIDE_INT i0, i1;
     enum machine_mode mode;
{
  if (! WORDS_BIG_ENDIAN)
    return immed_double_const (i1, i0, mode);

  return immed_double_const (i0, i1, mode);
}


/* Return either PERMUTE or SIMPLE based on the best scheduling of INSN.  */

char *
choose_vec_easy (insn, permute, simple)
     rtx insn;
     char *permute;
     char *simple;
{
  /* Remember our last choice.  */
  static enum attr_type last_easy = TYPE_INTEGER;
  /* First look at the previous instruction.  */
  rtx prev = prev_active_insn (insn);
  enum attr_type type = prev ? get_attr_type (prev) : TYPE_INTEGER;

  /* If the previous instruction was VEC_EASY, its chosen type is in last_easy.  */
  if (type == TYPE_VEC_EASY)
    type = last_easy;

  /* Attempt to alternate VEC_PERM and VEC_SIMPLE.  */
  if (type == TYPE_VEC_PERM)
    last_easy = TYPE_VEC_SIMPLE;
  else if (type == TYPE_VEC_SIMPLE || type == TYPE_VEC_COMPLEX || type == TYPE_VEC_FP)
    last_easy = TYPE_VEC_PERM;
  else
    {
      /* Look at the next instruction to decide.  */
      rtx next = next_active_insn (insn);
      type = next ? get_attr_type (next) : TYPE_INTEGER;
      /* Prefer VEC_PERM unless the next instruction conflicts.  */
      last_easy = (type == TYPE_VEC_PERM ? TYPE_VEC_SIMPLE : TYPE_VEC_PERM);
    }
  insn_extract (insn);
  return (last_easy == TYPE_VEC_SIMPLE ? simple : permute);
}

/* Return non-zero if this function is known to have a null epilogue.  */

int
direct_return ()
{
  if (reload_completed)
    {
      rs6000_stack_t *info = rs6000_stack_info ();

      if (info->first_gp_reg_save == 32
	  && info->first_fp_reg_save == 64
	  && info->first_vector_reg_save == 110
	  && !info->vrsave_save_p
	  && !info->lr_save_p
	  && !info->cr_save_p
	  && !info->push_p)
	return 1;
    }

  return 0;
}

/* Returns 1 always.  */

int
any_operand (op, mode)
     register rtx op ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return 1;
}

/* Returns 1 if op is the count register */
int
count_register_operand(op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  if (GET_CODE (op) != REG)
    return 0;

  if (REGNO (op) == COUNT_REGISTER_REGNUM)
    return 1;

  if (REGNO (op) > FIRST_PSEUDO_REGISTER)
    return 1;

  return 0;
}

/* Returns 1 if op is a vector register */
int
vector_register_operand(op, mode)
     register rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) != REG)
    return 0;

  if (VECTOR_REGNO_P (REGNO (op)))
    return 1;

  if (REGNO (op) > FIRST_PSEUDO_REGISTER)
    return 1;

  return 0;
}

/* Returns 1 if op is memory location for float/int conversions that masquerades
   as a register.  */
int
fpmem_operand(op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  if (GET_CODE (op) != REG)
    return 0;

  if (FPMEM_REGNO_P (REGNO (op)))
    return 1;

#if 0
  if (REGNO (op) > FIRST_PSEUDO_REGISTER)
    return 1;
#endif

  return 0;
}

/* Return 1 if OP is a constant that can fit in a D field.  */

int
short_cint_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return ((GET_CODE (op) == CONST_INT
	   && (unsigned HOST_WIDE_INT) (INTVAL (op) + 0x8000) < 0x10000)
	  || GET_CODE (op) == CONSTANT_P_RTX);
}

/* Similar for a unsigned D field.  */

int
u_short_cint_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return ((GET_CODE (op) == CONST_INT
	   && (INTVAL (op) & (~ (HOST_WIDE_INT) 0xffff)) == 0)
	  || GET_CODE (op) == CONSTANT_P_RTX);
}

/* Return 1 if OP is a CONST_INT that cannot fit in a signed D field.  */

int
non_short_cint_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && (unsigned HOST_WIDE_INT) (INTVAL (op) + 0x8000) >= 0x10000);
}

/* Returns 1 if OP is a register that is not special (i.e., not MQ,
   ctr, or lr).  */

int
gpc_reg_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || (REGNO (op) >= 67
		  && !FPMEM_REGNO_P (REGNO (op))
		  && !VECTOR_REGNO_P (REGNO (op)))
	      || REGNO (op) < 64));
}

/* Returns 1 if OP is either a pseudo-register or a register denoting a
   CR field.  */

int
cc_reg_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || REGNO (op) >= FIRST_PSEUDO_REGISTER
	      || CR_REGNO_P (REGNO (op))));
}

/* Returns 1 if OP is either a constant integer valid for a D-field or a
   non-special register.  If a register, it must be in the proper mode unless
   MODE is VOIDmode.  */

int
reg_or_short_operand (op, mode)
      register rtx op;
      enum machine_mode mode;
{
  return short_cint_operand (op, mode) || gpc_reg_operand (op, mode);
}

/* Similar, except check if the negation of the constant would be valid for
   a D-field.  */

int
reg_or_neg_short_operand (op, mode)
      register rtx op;
      enum machine_mode mode;
{
  if (GET_CODE (op) == CONST_INT)
    return CONST_OK_FOR_LETTER_P (INTVAL (op), 'P');

  return gpc_reg_operand (op, mode);
}

/* Return 1 if the operand is either a register or an integer whose high-order
   16 bits are zero.  */

int
reg_or_u_short_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return u_short_cint_operand (op, mode) || gpc_reg_operand (op, mode);
}

/* Return 1 is the operand is either a non-special register or ANY
   constant integer.  */

int
reg_or_cint_operand (op, mode)
    register rtx op;
    enum machine_mode mode;
{
     return (GET_CODE (op) == CONST_INT
	     || GET_CODE (op) == CONSTANT_P_RTX
	     || gpc_reg_operand (op, mode));
}

/* Return 1 if the operand is an operand that can be loaded via the GOT */

int
got_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == SYMBOL_REF
	  || GET_CODE (op) == CONST
	  || GET_CODE (op) == LABEL_REF);
}

/* Return 1 if the operand is a simple references that can be loaded via
   the GOT (labels involving addition aren't allowed).  */

int
got_no_const_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == LABEL_REF);
}

/* Return the number of instructions it takes to form a constant in an
   integer register.  */

static int
num_insns_constant_wide (value)
     HOST_WIDE_INT value;
{
  /* signed constant loadable with {cal|addi} */
  if (((unsigned HOST_WIDE_INT)value + 0x8000) < 0x10000)
    return 1;

#if HOST_BITS_PER_WIDE_INT == 32
  /* constant loadable with {cau|addis} */
  else if ((value & 0xffff) == 0)
    return 1;

#else
  /* constant loadable with {cau|addis} */
  else if ((value & 0xffff) == 0 && (value & ~0xffffffff) == 0)
    return 1;

  else if (TARGET_64BIT)
    {
      HOST_WIDE_INT low  = value & 0xffffffff;
      HOST_WIDE_INT high = value >> 32;

      if (high == 0 && (low & 0x80000000) == 0)
	return 2;

      else if (high == 0xffffffff && (low & 0x80000000) != 0)
	return 2;

      else if (!low)
	return num_insns_constant_wide (high) + 1;

      else
	return (num_insns_constant_wide (high)
		+ num_insns_constant_wide (low) + 1);
    }
#endif

  else
    return 2;
}

int
num_insns_constant (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) == CONST_INT)
    return num_insns_constant_wide (INTVAL (op));

  else if (GET_CODE (op) == CONST_DOUBLE && mode == SFmode)
    {
      long l;
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);
      return num_insns_constant_wide ((HOST_WIDE_INT)l);
    }

  else if (GET_CODE (op) == CONST_DOUBLE)
    {
      HOST_WIDE_INT low;
      HOST_WIDE_INT high;
      long l[2];
      REAL_VALUE_TYPE rv;
      int endian = (WORDS_BIG_ENDIAN == 0);

      if (mode == VOIDmode || mode == DImode)
	{
	  high = CONST_DOUBLE_HIGH (op);
	  low  = CONST_DOUBLE_LOW (op);
	}
      else
	{
	  REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
	  REAL_VALUE_TO_TARGET_DOUBLE (rv, l);
	  high = l[endian];
	  low  = l[1 - endian];
	}

      if (TARGET_32BIT)
	return (num_insns_constant_wide (low)
		+ num_insns_constant_wide (high));

      else
	{
	  if (high == 0 && (low & 0x80000000) == 0)
	    return num_insns_constant_wide (low);

	  else if (((high & 0xffffffff) == 0xffffffff)
		   && ((low & 0x80000000) != 0))
	    return num_insns_constant_wide (low);

	  else if (mask64_operand (op, mode))
	    return 2;

	  else if (low == 0)
	    return num_insns_constant_wide (high) + 1;

	  else
	    return (num_insns_constant_wide (high)
		    + num_insns_constant_wide (low) + 1);
	}
    }

  else
    abort ();
}

/* Return 1 if the operand is a CONST_DOUBLE and it can be put into a register
   with one instruction per word.  We only do this if we can safely read
   CONST_DOUBLE_{LOW,HIGH}.  */

int
easy_fp_constant (op, mode)
     register rtx op;
     register enum machine_mode mode;
{
  if (GET_CODE (op) != CONST_DOUBLE
      || GET_MODE (op) != mode
      || (GET_MODE_CLASS (mode) != MODE_FLOAT && mode != DImode))
    return 0;

  /* Consider all constants with -msoft-float to be easy */
  if (TARGET_SOFT_FLOAT && mode != DImode)
    return 1;

  /* If we are using V.4 style PIC, consider all constants to be hard */
  if (flag_pic && (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS))
    return 0;

#ifdef TARGET_RELOCATABLE
  /* Similarly if we are using -mrelocatable, consider all constants to be hard */
  if (TARGET_RELOCATABLE)
    return 0;
#endif

  if (mode == DFmode)
    {
      long k[2];
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, k);

      return (num_insns_constant_wide ((HOST_WIDE_INT)k[0]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT)k[1]) == 1);
    }

  else if (mode == SFmode)
    {
      long l;
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);

      return num_insns_constant_wide (l) == 1;
    }

  else if (mode == DImode)
    return ((TARGET_64BIT
	     && GET_CODE (op) == CONST_DOUBLE && CONST_DOUBLE_LOW (op) == 0)
	    || (num_insns_constant (op, DImode) <= 2));

  else
    abort ();
}

/* Return 1 if the operand is a CONST_VECTOR and it can be put into a register
   with one instruction.  */

int
easy_vector_constant (op)
     register rtx op;

{
  unsigned HOST_WIDE_INT immed;
  if (GET_CODE (op) != CONST_VECTOR
      || GET_MODE (op) != SVmode)
    return 0;

  immed = CONST_VECTOR_0 (op);

  /* If the four 32-bit words aren't the same, it can't be done.  */
  if (immed != CONST_VECTOR_1 (op)
      || immed != CONST_VECTOR_2 (op)
      || immed != CONST_VECTOR_3 (op))
    {
      if (immed + 0x04040404 == CONST_VECTOR_1 (op)
	  && CONST_VECTOR_1 (op) + 0x04040404 == CONST_VECTOR_2 (op)
	  && CONST_VECTOR_2 (op) + 0x04040404 == CONST_VECTOR_3 (op)
	  && (immed >> 16) + 0x0202 == (immed & 0xffff)
	  && (immed >> 24) + 1 == ((immed >> 16) & 0xff)
	  && (immed >>= 24) <= 0x10)
	{
	  if (immed == 0x10)
	    /* Use lvsr 0,0.  */
	    return 6;
	  else
	    /* Use lvsl 0,immed. */
	    return 7;
	}
      else
    return 0;
    }

  /* vxor v,v,v and vspltisw v,0 will work.  */
  else if (immed == 0)
    return 1;

  /* vcmpequw v,v,v and vspltisw v,-1 will work.  */
  else if (immed + 1 == 0)
    return 2;

  /* vspltisw will work.  */
  else if (immed + 16 < 32)
    return 3;

  /* The two 16-bit halves aren't the same.  */
  else if (immed >> 16 != (immed & 0xffff))
    return 0;

  /* vspltish will work.  */
  else if (((immed + 16) & 0xffff) < 32)
    return 4;

  /* The two 8-bit halves aren't the same.  */
  else if (immed >> 24 != (immed & 0xff))
    return 0;

  /* vspltisb will work.  */
  else if (((immed + 16) & 0xff) < 32)
    return 5;

  return 0;
}

/* Return 1 if the operand is in volatile memory.  Note that during the
   RTL generation phase, memory_operand does not return TRUE for
   volatile memory references.  So this function allows us to
   recognize volatile references where its safe.  */

int
volatile_mem_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) != MEM)
    return 0;

  if (!MEM_VOLATILE_P (op))
    return 0;

  if (mode != GET_MODE (op))
    return 0;

  if (reload_completed)
    return memory_operand (op, mode);

  if (reload_in_progress)
    return strict_memory_address_p (mode, XEXP (op, 0));

  return memory_address_p (mode, XEXP (op, 0));
}

/* Return 1 if the operand is an offsettable memory address.  */

int
offsettable_addr_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return offsettable_address_p (reload_completed | reload_in_progress,
				mode, op);
}

/* Return 1 if the operand is either an easy FP constant (see above) or
   memory.  */

int
mem_or_easy_const_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return memory_operand (op, mode) || easy_fp_constant (op, mode);
}

/* Return 1 if the operand is either a non-special register or an item
   that can be used as the operand of an SI add insn.  */

int
add_operand (op, mode)
    register rtx op;
    enum machine_mode mode;
{
  return (reg_or_short_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT && (INTVAL (op) & 0xffff) == 0));
}

/* Return 1 if the operand is either 0 or -1.  */

int
zero_m1_operand (op, mode)
    register rtx op;
    enum machine_mode mode;
{
  return (GET_CODE (op) == CONST_INT
	  && (unsigned HOST_WIDE_INT) (INTVAL (op) + 1) < 2);
}

/* Return 1 if OP is a constant but not a valid add_operand.  */

int
non_add_cint_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && (unsigned HOST_WIDE_INT) (INTVAL (op) + 0x8000) >= 0x10000
	  && (INTVAL (op) & 0xffff) != 0);
}

/* Return 1 if the operand is a non-special register or a constant that
   can be used as the operand of an OR or XOR insn on the RS/6000.  */

int
logical_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
	      && ((INTVAL (op) & (~ (HOST_WIDE_INT) 0xffff)) == 0
		  || (INTVAL (op) & 0xffff) == 0))
	  || GET_CODE (op) == CONSTANT_P_RTX);
}

/* Return 1 if C is a constant that is not a logical operand (as
   above).  */

int
non_logical_cint_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && (INTVAL (op) & (~ (HOST_WIDE_INT) 0xffff)) != 0
	  && (INTVAL (op) & 0xffff) != 0);
}

/* Return 1 if C is a constant that can be encoded in a mask on the
   RS/6000.  It is if there are no more than two 1->0 or 0->1 transitions.
   Reject all ones and all zeros, since these should have been optimized
   away and confuse the making of MB and ME.  */

int
mask_constant (c)
     register HOST_WIDE_INT c;
{
  int i;
  int last_bit_value;
  int transitions = 0;

  if (c == 0 || c == ~0)
    return 0;

  last_bit_value = c & 1;

  for (i = 1; i < 32; i++)
    if (((c >>= 1) & 1) != last_bit_value)
      last_bit_value ^= 1, transitions++;

  return transitions <= 2;
}

/* Return 1 if the operand is a constant that is a mask on the RS/6000. */

int
mask_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return GET_CODE (op) == CONST_INT && mask_constant (INTVAL (op));
}

/* Return 1 if the operand is a constant that is a PowerPC64 mask.
   It is if there are no more than one 1->0 or 0->1 transitions.
   Reject all ones and all zeros, since these should have been optimized
   away and confuse the making of MB and ME.  */

int
mask64_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) == CONST_INT)
    {
      HOST_WIDE_INT c = INTVAL (op);
      int i;
      int last_bit_value;
      int transitions = 0;

      if (c == 0 || c == ~0)
	return 0;

      last_bit_value = c & 1;

      for (i = 1; i < HOST_BITS_PER_WIDE_INT; i++)
	if (((c >>= 1) & 1) != last_bit_value)
	  last_bit_value ^= 1, transitions++;

#if HOST_BITS_PER_WIDE_INT == 32
      /* Consider CONST_INT sign-extended.  */
      transitions += (last_bit_value != 1);
#endif

      return transitions <= 1;
    }
  else if (GET_CODE (op) == CONST_DOUBLE
	   && (mode == VOIDmode || mode == DImode))
    {
      HOST_WIDE_INT low = CONST_DOUBLE_LOW (op);
#if HOST_BITS_PER_WIDE_INT == 32
      HOST_WIDE_INT high = CONST_DOUBLE_HIGH (op);
#endif
      int i;
      int last_bit_value;
      int transitions = 0;

      if ((low == 0
#if HOST_BITS_PER_WIDE_INT == 32
	  && high == 0
#endif
	   )
	  || (low == ~0
#if HOST_BITS_PER_WIDE_INT == 32
	      && high == ~0
#endif
	      ))
	return 0;

      last_bit_value = low & 1;

      for (i = 1; i < HOST_BITS_PER_WIDE_INT; i++)
	if (((low >>= 1) & 1) != last_bit_value)
	  last_bit_value ^= 1, transitions++;

#if HOST_BITS_PER_WIDE_INT == 32
      if ((high & 1) != last_bit_value)
	last_bit_value ^= 1, transitions++;

      for (i = 1; i < HOST_BITS_PER_WIDE_INT; i++)
	if (((high >>= 1) & 1) != last_bit_value)
	  last_bit_value ^= 1, transitions++;
#endif

      return transitions <= 1;
    }
  else
    return 0;
}

/* Return 1 if the operand is either a non-special register or a constant
   that can be used as the operand of a PowerPC64 logical AND insn.  */

int
and64_operand (op, mode)
    register rtx op;
    enum machine_mode mode;
{
  return (logical_operand (op, mode)
	  || mask64_operand (op, mode));
}

/* Return 1 if the operand is either a non-special register or a
   constant that can be used as the operand of an RS/6000 logical AND insn.  */

int
and_operand (op, mode)
    register rtx op;
    enum machine_mode mode;
{
  return (logical_operand (op, mode)
	  || mask_operand (op, mode));
}

/* Return 1 if the operand is a general register or memory operand.  */

int
reg_or_mem_operand (op, mode)
     register rtx op;
     register enum machine_mode mode;
{
  return (gpc_reg_operand (op, mode)
	  || memory_operand (op, mode)
	  || volatile_mem_operand (op, mode));
}

/* Return 1 if the operand is a general register or memory operand without
   pre-inc or pre_dec which produces invalid form of PowerPC lwa
   instruction.  */

int
lwa_operand (op, mode)
     register rtx op;
     register enum machine_mode mode;
{
  rtx inner = op;

  if (reload_completed && GET_CODE (inner) == SUBREG)
    inner = SUBREG_REG (inner);
    
  return gpc_reg_operand (inner, mode)
    || (memory_operand (inner, mode)
	&& GET_CODE (XEXP (inner, 0)) != PRE_INC
	&& GET_CODE (XEXP (inner, 0)) != PRE_DEC);
}

/* Return 1 if the operand, used inside a MEM, is a valid first argument
   to CALL.  This is a SYMBOL_REF or a pseudo-register, which will be
   forced to lr.  */

int
call_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  if (mode != VOIDmode && GET_MODE (op) != mode)
    return 0;

  return (GET_CODE (op) == SYMBOL_REF
	  || (GET_CODE (op) == REG && REGNO (op) >= FIRST_PSEUDO_REGISTER));
}


/* Return 1 if the operand is a SYMBOL_REF for a function known to be in
   this file, or if the operand is a LABEL_REF.  Used as an operand
   match constraint for function call instructions.  We need to permit
   the LABEL_REF in order for reload_ppc_pic_register to work.  */

int
current_file_function_operand (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
#ifdef MACHO_PIC
  if (GET_CODE (op) == LABEL_REF)
    return 1;
#endif
  return (GET_CODE (op) == SYMBOL_REF
	  && (SYMBOL_REF_FLAG (op)
	      || op == XEXP (DECL_RTL (current_function_decl), 0)));
}


/* Return 1 if this operand is a valid input for a move insn.  */

int
input_operand (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  /* Memory is always valid.  */
  if (memory_operand (op, mode))
    return 1;

  /* For floating-point, easy constants are valid.  */
  if (GET_MODE_CLASS (mode) == MODE_FLOAT
      && CONSTANT_P (op)
      && easy_fp_constant (op, mode))
    return 1;

  /* Allow any integer constant.  */
  if (GET_MODE_CLASS (mode) == MODE_INT
      && (GET_CODE (op) == CONST_INT
	  || GET_CODE (op) == CONSTANT_P_RTX
	  || GET_CODE (op) == CONST_DOUBLE))
    return 1;

  /* For floating-point or multi-word mode, the only remaining valid type
     is a register.  */
  if (GET_MODE_CLASS (mode) == MODE_FLOAT
      || GET_MODE_SIZE (mode) > UNITS_PER_WORD)
    return register_operand (op, mode);

  /* The only cases left are integral modes one word or smaller (we
     do not get called for MODE_CC values).  These can be in any
     register.  */
  if (register_operand (op, mode))
    return 1;

  /* A SYMBOL_REF referring to the TOC is valid.  */
  if (LEGITIMATE_CONSTANT_POOL_ADDRESS_P (op))
    return 1;

  /* Windows NT allows SYMBOL_REFs and LABEL_REFs against the TOC
     directly in the instruction stream */
  if (DEFAULT_ABI == ABI_NT
      && (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == LABEL_REF))
    return 1;

  /* V.4 allows SYMBOL_REFs and CONSTs that are in the small data region
     to be valid.  */
  if ((DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
      && (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == CONST)
      && small_data_operand (op, Pmode))
    return 1;

  return 0;
}

/* Return 1 for an operand in small memory on V.4/eabi */

int
small_data_operand (op, mode)
#if TARGET_ELF
     rtx op;
     enum machine_mode mode;
#else
     rtx op ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
#endif
{
#if TARGET_ELF
  rtx sym_ref, const_part;

  if (rs6000_sdata == SDATA_NONE || rs6000_sdata == SDATA_DATA)
    return 0;

  if (DEFAULT_ABI != ABI_V4 && DEFAULT_ABI != ABI_SOLARIS)
    return 0;

  if (GET_CODE (op) == SYMBOL_REF)
    sym_ref = op;

  else if (GET_CODE (op) != CONST
	   || GET_CODE (XEXP (op, 0)) != PLUS
	   || GET_CODE (XEXP (XEXP (op, 0), 0)) != SYMBOL_REF
	   || GET_CODE (XEXP (XEXP (op, 0), 1)) != CONST_INT)
    return 0;

  else
    {
      rtx sum = XEXP (op, 0);
      HOST_WIDE_INT summand;

      /* We have to be careful here, because it is the referenced address
        that must be 32k from _SDA_BASE_, not just the symbol.  */
      summand = INTVAL (XEXP (sum, 1));
      if (summand < 0 || summand > g_switch_value)
       return 0;

      sym_ref = XEXP (sum, 0);
    }

  if (*XSTR (sym_ref, 0) != '@')
    return 0;

  return 1;

#else
  return 0;
#endif
}


/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.

   For incoming args we set the number of arguments in the prototype large
   so we never return a PARALLEL.  */

void
init_cumulative_args (cum, fntype, libname, incoming)
     CUMULATIVE_ARGS *cum;
     tree fntype;
     rtx libname ATTRIBUTE_UNUSED;
     int incoming;
{
  static CUMULATIVE_ARGS zero_cumulative;
  enum rs6000_abi abi = DEFAULT_ABI;

  *cum = zero_cumulative;
  cum->words = cum->gwords = 0;
  cum->fregno = FP_ARG_MIN_REG;
  cum->vregno = VECTOR_ARG_MIN_REG;
  cum->num_vector = 0;
  cum->prototype = (fntype && TYPE_ARG_TYPES (fntype));
  cum->call_cookie = CALL_NORMAL;
  cum->is_incoming = incoming;

  if (incoming)
    {
      cum->is_varargs = (current_function_stdarg || current_function_varargs);
      cum->nargs_prototype = 1000;		/* don't return a PARALLEL */
      if (abi == ABI_V4 || abi == ABI_SOLARIS)
	cum->varargs_offset = RS6000_VARARGS_OFFSET;
    }

  else if (cum->prototype)
    {
      tree last = tree_last (TYPE_ARG_TYPES (fntype));
      cum->is_varargs = !(last && TREE_VALUE (last) == void_type_node);
      cum->nargs_prototype = (list_length (TYPE_ARG_TYPES (fntype)) - 1
			      + (TYPE_MODE (TREE_TYPE (fntype)) == BLKmode
				 || RETURN_IN_MEMORY (TREE_TYPE (fntype))));
    }

  else
    cum->nargs_prototype = 0;

  cum->orig_nargs = cum->nargs_prototype;

  /* Check for DLL import functions */
  if (abi == ABI_NT
      && fntype
      && lookup_attribute ("dllimport", TYPE_ATTRIBUTES (fntype)))
    cum->call_cookie = CALL_NT_DLLIMPORT;

  /* Also check for longcall's */
  else if (fntype && lookup_attribute ("longcall", TYPE_ATTRIBUTES (fntype)))
    cum->call_cookie = CALL_LONG;

  if (TARGET_DEBUG_ARG)
    {
      fprintf (stderr, "\ninit_cumulative_args:");
      if (fntype)
	{
	  tree ret_type = TREE_TYPE (fntype);
	  fprintf (stderr, " ret code = %s,",
		   tree_code_name[ (int)TREE_CODE (ret_type) ]);
	}

      if ((abi == ABI_V4 || abi == ABI_SOLARIS) && incoming)
	fprintf (stderr, " varargs = %d, ", cum->varargs_offset);

      if (cum->call_cookie & CALL_NT_DLLIMPORT)
	fprintf (stderr, " dllimport,");

      if (cum->call_cookie & CALL_LONG)
	fprintf (stderr, " longcall,");

      fprintf (stderr, " proto = %d, nargs = %d\n",
	       cum->prototype, cum->nargs_prototype);
    }
}

/* Rearrange the argument or parameter list just before scanning it to
   place locate the arguments.

   For the Apple and AIX AltiVec Programming Model, non-vector
   parameters are passed in the same registers and stack locations as
   they would be if the vector parameters were not present.
   Accomplish this by moving the vector parameters to the end of the
   argument list.  */

tree
rearrange_arg_list (cum, args)
     CUMULATIVE_ARGS *cum;
     tree args;
{
  tree arg, value, next;
  tree prev = NULL_TREE;
  tree vector_args = NULL_TREE;
  tree last_vector_arg = NULL_TREE;
  tree vector_reg_args = NULL_TREE;
  int i = 0;

  /* Don't rearrange for SVR4 or varargs and stdarg.  */
  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS || cum->is_varargs)
    return args;

  /* Remove all vector args.  */
  for (arg = args; arg; arg = next)
    {
      next = TREE_CHAIN (arg);
      value = (TREE_CODE (arg) == TREE_LIST ? TREE_VALUE (arg) : arg);
      if (TREE_CODE (TREE_TYPE (value)) == VECTOR_TYPE)
	{
	  if (prev)
	    TREE_CHAIN (prev) = next;
	  else
	    args = next;

	  if (i++ < VECTOR_ARG_NUM_REG)
	    {
	      TREE_CHAIN (arg) = vector_reg_args;
	      vector_reg_args = arg;
	    }
	  else
	    {
	      TREE_CHAIN (arg) = vector_args;
	      last_vector_arg = vector_args = arg;
	    }
	}
      else
	prev = arg;
    }
  vector_reg_args = nreverse (vector_reg_args);
  vector_args = nreverse (vector_args);

  cum->num_vector = i;

  if (last_vector_arg)
    TREE_CHAIN (last_vector_arg) = vector_reg_args;
  else
    vector_args = vector_reg_args;

  if (prev)
    TREE_CHAIN (prev) = vector_args;
  else
    args = vector_args;

  return args;
}

/* If defined, a C expression which determines whether, and in which
   direction, to pad out an argument with extra space.  The value
   should be of type `enum direction': either `upward' to pad above
   the argument, `downward' to pad below, or `none' to inhibit
   padding.

   For the AIX ABI structs are always stored left shifted in their
   argument slot.  */

int
function_arg_padding (mode, type)
     enum machine_mode mode;
     tree type;
{
  if (type != 0 && AGGREGATE_TYPE_P (type))
    return (int)upward;

  /* This is the default definition.  */
  return (! BYTES_BIG_ENDIAN
          ? (int)upward
          : ((mode == BLKmode
              ? (type && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST
                 && int_size_in_bytes (type) < (PARM_BOUNDARY / BITS_PER_UNIT))
              : GET_MODE_BITSIZE (mode) < PARM_BOUNDARY)
             ? (int)downward : (int)upward));
}

/* If defined, a C expression that gives the alignment boundary, in bits,
   of an argument with the specified mode and type.  If it is not defined, 
   PARM_BOUNDARY is used for all arguments.
   
   Windows NT wants anything >= 8 bytes to be double word aligned.

   V.4 wants long longs to be double word aligned.  */

int
function_arg_boundary (mode, type)
     enum machine_mode mode;
     tree type;
{
  if (mode == SVmode
      || (mode == BLKmode
	  && DEFAULT_ABI != ABI_V4
	  && DEFAULT_ABI != ABI_SOLARIS
	  && TYPE_ALIGN (type) == 128))
    /* Vector parameters must be 16-byte aligned.  This places them at
       64 mod 128 from the arg pointer.  */
    return 128;

  if ((DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS) && mode == DImode)
    return 64;

  if (DEFAULT_ABI != ABI_NT || TARGET_64BIT)
    return PARM_BOUNDARY;

  if (mode != BLKmode)
    return (GET_MODE_SIZE (mode)) >= 8 ? 64 : 32;

  return (int_size_in_bytes (type) >= 8) ? 64 : 32;
}

int
function_arg_mod_boundary (mode, type)
     enum machine_mode mode;
     tree type;
{
  if (mode == SVmode
      || (mode == BLKmode 
	  && DEFAULT_ABI != ABI_V4
	  && DEFAULT_ABI != ABI_SOLARIS
	  && TYPE_ALIGN (type) == 128))
    /* Vector parameters must be 16-byte aligned.  This places them at 2 mod 4
       in terms of words.  */
    return 64;
  return 0;
}

static int
function_arg_skip (mode, type, words)
     enum machine_mode mode;
     tree type;
     int words;
{
  if (mode == SVmode
      || (mode == BLKmode 
	  && DEFAULT_ABI != ABI_V4
	  && DEFAULT_ABI != ABI_SOLARIS
	  && TYPE_ALIGN (type) == 128))
    /* Vector parameters must be 16-byte aligned.  This places them at 2 mod 4
       in terms of words.  */
    return ((6 - (words & 3)) & 3);
  if (function_arg_boundary (mode, type) == 64)
    return (words & 1);
  return 0;
}

int
no_reg_parm_stack_space (cum, entry)
     CUMULATIVE_ARGS *cum;
     rtx entry;
{
  return (!cum->is_varargs
	  && entry
	  && GET_CODE (entry) == REG
	  && GET_MODE (entry) == SVmode);
}

/* Update the data in CUM to advance over an argument
   of mode MODE and data type TYPE.
   (TYPE is null for libcalls where that information may not be available.)  */

void
function_arg_advance (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named;
{
  int align = function_arg_skip (mode, type, cum->words);
  int words = cum->words + align;
  int galign = function_arg_skip (mode, type, cum->gwords);
  int gwords = cum->gwords + galign;
  cum->nargs_prototype--;

  if (cum->is_incoming && GET_MODE_CLASS (mode) == MODE_VECTOR)
    named = 1;

  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
    {
      /* Long longs must not be split between registers and stack */
      if ((GET_MODE_CLASS (mode) != MODE_FLOAT || TARGET_SOFT_FLOAT)
	  && GET_MODE_CLASS (mode) != MODE_VECTOR
	  && type && !AGGREGATE_TYPE_P (type)
	  && gwords < GP_ARG_NUM_REG
	  && gwords + RS6000_ARG_SIZE (mode, type, named) > GP_ARG_NUM_REG)
	{
	  cum->words = GP_ARG_NUM_REG; /* FIXME */
	  cum->gwords = GP_ARG_NUM_REG;
	}

      /* Aggregates get passed as pointers */
      if (type && AGGREGATE_TYPE_P (type))
	{
	  cum->words = words + 1;
	  if (gwords < GP_ARG_NUM_REG)
	    cum->gwords = gwords + 1;
	}

      /* Floats go in registers, & don't occupy space in the GP registers
	 like they do for AIX unless software floating point.  */
      else if (GET_MODE_CLASS (mode) == MODE_FLOAT
	       && TARGET_HARD_FLOAT
	       && cum->fregno <= FP_ARG_V4_MAX_REG)
	cum->fregno++;

      /* Vectors go in registers and don't occupy space in the GPRs.  */
      else if (GET_MODE_CLASS (mode) == MODE_VECTOR
	       && cum->vregno <= VECTOR_ARG_MAX_REG
	       && cum->nargs_prototype >= -1)
	cum->vregno++;

      else if (GET_MODE_CLASS (mode) == MODE_VECTOR)
	cum->words = words + RS6000_ARG_SIZE (mode, type, 1);

      else
	{
	  cum->words = words + RS6000_ARG_SIZE (mode, type, 1);
	  if (gwords < GP_ARG_NUM_REG)
	    cum->gwords = gwords + RS6000_ARG_SIZE (mode, type, 1);
	}
    }
  else
    if (named)
      {
	/* Vectors go in registers and don't occupy space in the GPRs.  */
	if (GET_MODE_CLASS (mode) == MODE_VECTOR
	    && cum->nargs_prototype >= -1)
	  cum->vregno++;

	/* Unless it's varargs or stdarg.  */
	if (GET_MODE_CLASS (mode) != MODE_VECTOR || cum->is_varargs)
	  {
	    cum->words = words + RS6000_ARG_SIZE (mode, type, named);
	    if (gwords < GP_ARG_NUM_REG)
	      cum->gwords = gwords + RS6000_ARG_SIZE (mode, type, named);
	  }

	if (GET_MODE_CLASS (mode) == MODE_FLOAT && TARGET_HARD_FLOAT)
	  cum->fregno++;
      }

  if (TARGET_DEBUG_ARG)
    fprintf (stderr,
	     "function_adv: words = %2d, gwords = %2d, fregno = %2d, vregno = %2d, num_vector = %2d, nargs = %4d, proto = %d, mode = %4s, named = %d, align = %d\n",
	     cum->words, cum->gwords, cum->fregno, cum->vregno, cum->num_vector, cum->nargs_prototype, cum->prototype, GET_MODE_NAME (mode), named, align);
}

/* Determine where to put an argument to a function.
   Value is zero to push the argument on the stack,
   or a hard register in which to store the argument.

   MODE is the argument's machine mode.
   TYPE is the data type of the argument (as a tree).
    This is null for libcalls where that information may
    not be available.
   CUM is a variable of type CUMULATIVE_ARGS which gives info about
    the preceding args and about the function being called.
   NAMED is nonzero if this argument is a named parameter
    (otherwise it is an extra parameter matching an ellipsis).

   On RS/6000 the first eight words of non-FP are normally in registers
   and the rest are pushed.  Under AIX, the first 13 FP args are in registers.
   Under V.4, the first 8 FP args are in registers.

   If this is floating-point and no prototype is specified, we use
   both an FP and integer register (or possibly FP reg and stack).  Library
   functions (when TYPE is zero) always have the proper types for args,
   so we can pass the FP value just in one register.  emit_library_function
   doesn't support PARALLEL anyway.  */

struct rtx_def *
function_arg (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named;
{
  int align = function_arg_skip (mode, type, cum->words);
  int words = cum->words + align;
  int galign = function_arg_skip (mode, type, cum->gwords);
  int gwords = cum->gwords + galign;

  if (cum->is_incoming && GET_MODE_CLASS (mode) == MODE_VECTOR)
    named = 1;

  if (TARGET_DEBUG_ARG)
    fprintf (stderr,
	     "function_arg: words = %2d, gwords = %2d, fregno = %2d, vregno = %2d, num_vector = %2d, nargs = %4d, proto = %d, mode = %4s, named = %d, align = %d\n",
	     cum->words, cum->gwords, cum->fregno, cum->vregno, cum->num_vector, cum->nargs_prototype, cum->prototype, GET_MODE_NAME (mode), named, align);

  /* Return a marker to indicate whether CR1 needs to set or clear the bit that V.4
     uses to say fp args were passed in registers.  Assume that we don't need the
     marker for software floating point, or compiler generated library calls.  */
  if (mode == VOIDmode)
    {
      enum rs6000_abi abi = DEFAULT_ABI;

      if ((abi == ABI_V4 || abi == ABI_SOLARIS)
	  && TARGET_HARD_FLOAT
	  && cum->nargs_prototype < 0
	  && type && (cum->prototype || TARGET_NO_PROTOTYPE))
	{
	  return GEN_INT (cum->call_cookie
			  | ((cum->fregno == FP_ARG_MIN_REG)
			     ? CALL_V4_SET_FP_ARGS
			     : CALL_V4_CLEAR_FP_ARGS));
	}

      return GEN_INT (cum->call_cookie);
    }

  if (!named)
    {
      if (DEFAULT_ABI != ABI_V4 && DEFAULT_ABI != ABI_SOLARIS)
	return NULL_RTX;
    }

  if (type && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST)
    return NULL_RTX;

  if (USE_FP_FOR_ARG_P (*cum, mode, type))
    {
      if (DEFAULT_ABI == ABI_V4 /* V.4 never passes FP values in GP registers */
	  || DEFAULT_ABI == ABI_SOLARIS
	  || ! type
	  || ((cum->nargs_prototype > 0)
	      /* IBM AIX extended its linkage convention definition always to
		 require FP args after register save area hole on the stack.  */
	      && (DEFAULT_ABI != ABI_AIX
		  || ! TARGET_XL_CALL
		  || (words < GP_ARG_NUM_REG))))
	return gen_rtx_REG (mode, cum->fregno);

      return gen_rtx_PARALLEL (mode,
		      gen_rtvec
		      (2,
		       gen_rtx_EXPR_LIST (VOIDmode,
				((words >= GP_ARG_NUM_REG)
				 ? NULL_RTX
				 : (words
				    + RS6000_ARG_SIZE (mode, type, named)
				    > GP_ARG_NUM_REG
				    /* If this is partially on the stack, then
				       we only include the portion actually
				       in registers here.  */
				    ? gen_rtx_REG (SImode,
					       GP_ARG_MIN_REG + words)
				    : gen_rtx_REG (mode,
					       GP_ARG_MIN_REG + words))),
				const0_rtx),
		       gen_rtx_EXPR_LIST (VOIDmode,
				gen_rtx_REG (mode, cum->fregno),
				const0_rtx)));
    }

  else if (GET_MODE_CLASS (mode) == MODE_VECTOR)
    {
      if (cum->nargs_prototype >= 0)
	{
	  int vregno = cum->vregno;
	  if (cum->num_vector > VECTOR_ARG_NUM_REG)
	    vregno -= cum->num_vector - VECTOR_ARG_NUM_REG;
	  if ((unsigned)vregno - VECTOR_ARG_MIN_REG < (unsigned)VECTOR_ARG_NUM_REG)
	    return gen_rtx (REG, mode, vregno);
	  return NULL_RTX;
	}
      else if (DEFAULT_ABI != ABI_V4 && DEFAULT_ABI != ABI_SOLARIS
	       && words < GP_ARG_NUM_REG)
	{
	  /* Claim that the vector value goes in both memory and GPRs.  See
	     gen_movsv for how the GPR copy gets interpreted.  */
	  rtx reg = gen_rtx (REG, SVmode, GP_ARG_MIN_REG + words);
	  return gen_rtx (PARALLEL, mode,
			  gen_rtvec (2,
				     gen_rtx (EXPR_LIST, VOIDmode,
					      NULL_RTX, const0_rtx),
				     gen_rtx (EXPR_LIST, VOIDmode,
					      reg, const0_rtx)));
	}
    }

  /* Long longs won't be split between register and stack;
     FP arguments get passed on the stack if they didn't get a register.  */
  else if ((DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS) &&
	   (gwords + RS6000_ARG_SIZE (mode, type, named) > GP_ARG_NUM_REG
	    || (GET_MODE_CLASS (mode) == MODE_FLOAT && TARGET_HARD_FLOAT)))
    {
      return NULL_RTX;
    }

  else if (gwords < GP_ARG_NUM_REG)
    return gen_rtx_REG (mode, gwords + GP_ARG_MIN_REG);

  return NULL_RTX;
}

/* For an arg passed partly in registers and partly in memory,
   this is the number of registers used.
   For args passed entirely in registers or entirely in memory, zero.  */

int
function_arg_partial_nregs (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named;
{
  int words;

  if (! named)
    return 0;

  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
    return 0;

  if (USE_FP_FOR_ARG_P (*cum, mode, type))
    {
      if (cum->nargs_prototype >= 0)
	return 0;
    }

  if (type && TREE_CODE (type) == VECTOR_TYPE)
    return 0;

  words = cum->gwords;
  words += function_arg_skip (mode, type, words);
  if (words < GP_ARG_NUM_REG
      && GP_ARG_NUM_REG < (words + RS6000_ARG_SIZE (mode, type, named)))
    {
      int ret = GP_ARG_NUM_REG - words;
      if (ret && TARGET_DEBUG_ARG)
	fprintf (stderr, "function_arg_partial_nregs: %d\n", ret);

      return ret;
    }

  return 0;
}

/* A C expression that indicates when an argument must be passed by
   reference.  If nonzero for an argument, a copy of that argument is
   made in memory and a pointer to the argument is passed instead of
   the argument itself.  The pointer is passed in whatever way is
   appropriate for passing a pointer to that type.

   Under V.4, structures and unions are passed by reference.  */

int
function_arg_pass_by_reference (cum, mode, type, named)
     CUMULATIVE_ARGS *cum ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
     tree type;
     int named ATTRIBUTE_UNUSED;
{
  if ((DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
      && type && AGGREGATE_TYPE_P (type))
    {
      if (TARGET_DEBUG_ARG)
	fprintf (stderr, "function_arg_pass_by_reference: aggregate\n");

      return 1;
    }

  return 0;
}


/* Perform any needed actions needed for a function that is receiving a
   variable number of arguments. 

   CUM is as above.

   MODE and TYPE are the mode and type of the current parameter.

   PRETEND_SIZE is a variable that should be set to the amount of stack
   that must be pushed by the prolog to pretend that our caller pushed
   it.

   Normally, this macro will push all remaining incoming registers on the
   stack and set PRETEND_SIZE to the length of the registers pushed.  */

void
setup_incoming_varargs (cum, mode, type, pretend_size, no_rtl)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int *pretend_size;
     int no_rtl;

{
  rtx save_area = virtual_incoming_args_rtx;
  int reg_size	= TARGET_32BIT ? 4 : 8;

  if (TARGET_DEBUG_ARG)
    fprintf (stderr,
	     "setup_vararg: words = %2d, gwords = %2d, fregno = %2d, vregno = %2d, num_vector = %2d, nargs = %4d, proto = %d, mode = %4s, no_rtl= %d\n",
	     cum->words, cum->gwords, cum->fregno, cum->vregno, cum->num_vector, cum->nargs_prototype, cum->prototype, GET_MODE_NAME (mode), no_rtl);

  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
    {
      rs6000_sysv_varargs_p = 1;
      if (! no_rtl)
	save_area = plus_constant (frame_pointer_rtx, RS6000_VARARGS_OFFSET);
    }
  else
    rs6000_sysv_varargs_p = 0;

  if (cum->gwords < GP_ARG_NUM_REG)
    {
      int first_reg_offset = cum->gwords;

      /* For varargs, vector values occupy memory locations, so count them.  */
      if (DEFAULT_ABI != ABI_V4 && DEFAULT_ABI != ABI_SOLARIS
	  && GET_MODE_CLASS (mode) == MODE_VECTOR)
	{
	  first_reg_offset += function_arg_skip (mode, type, cum->words);
	  first_reg_offset += RS6000_ARG_SIZE (mode, type, 1);
	}

      if (MUST_PASS_IN_STACK (mode, type))
	first_reg_offset += RS6000_ARG_SIZE (TYPE_MODE (type), type, 1);

      if (first_reg_offset > GP_ARG_NUM_REG)
	first_reg_offset = GP_ARG_NUM_REG;

      if (!no_rtl && first_reg_offset != GP_ARG_NUM_REG)
	move_block_from_reg
	  (GP_ARG_MIN_REG + first_reg_offset,
	   gen_rtx_MEM (BLKmode,
		    plus_constant (save_area, first_reg_offset * reg_size)),
	   GP_ARG_NUM_REG - first_reg_offset,
	   (GP_ARG_NUM_REG - first_reg_offset) * UNITS_PER_WORD);

      *pretend_size = (GP_ARG_NUM_REG - first_reg_offset) * UNITS_PER_WORD;
    }

  /* Save FP registers if needed.  */
  if ((DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS) && TARGET_HARD_FLOAT && !no_rtl)
    {
      int fregno     = cum->fregno;
      int num_fp_reg = FP_ARG_V4_MAX_REG + 1 - fregno;

      if (num_fp_reg >= 0)
	{
	  rtx cr1 = gen_rtx_REG (CCmode, 69);
	  rtx lab = gen_label_rtx ();
	  int off = (GP_ARG_NUM_REG * reg_size) + ((fregno - FP_ARG_MIN_REG) * 8);

	  emit_jump_insn (gen_rtx_SET (VOIDmode,
				   pc_rtx,
				   gen_rtx_IF_THEN_ELSE (VOIDmode,
					    gen_rtx_NE (VOIDmode, cr1, const0_rtx),
					    gen_rtx_LABEL_REF (VOIDmode, lab),
					    pc_rtx)));

	  while ( num_fp_reg-- >= 0)
	    {
	      emit_move_insn (gen_rtx_MEM (DFmode, plus_constant (save_area, off)),
			      gen_rtx_REG (DFmode, fregno++));
	      off += 8;
	    }

	  emit_label (lab);
	}
    }
}

/* If defined, is a C expression that produces the machine-specific
   code for a call to `__builtin_saveregs'.  This code will be moved
   to the very beginning of the function, before any parameter access
   are made.  The return value of this function should be an RTX that
   contains the value to use as the return of `__builtin_saveregs'.

   The argument ARGS is a `tree_list' containing the arguments that
   were passed to `__builtin_saveregs'.

   If this macro is not defined, the compiler will output an ordinary
   call to the library function `__builtin_saveregs'.
   
   On the Power/PowerPC return the address of the area on the stack
   used to hold arguments.  Under AIX, this includes the 8 word register
   save area.  Under V.4 this does not.  */

struct rtx_def *
expand_builtin_saveregs (args)
     tree args ATTRIBUTE_UNUSED;
{
  return virtual_incoming_args_rtx;
}


/* Generate a memory reference for expand_block_move, copying volatile,
   and other bits from an original memory reference.  */

static rtx
expand_block_move_mem (mode, addr, orig_mem)
     enum machine_mode mode;
     rtx addr;
     rtx orig_mem;
{
  rtx mem = gen_rtx_MEM (mode, addr);

  RTX_UNCHANGING_P (mem) = RTX_UNCHANGING_P (orig_mem);
  MEM_VOLATILE_P (mem) = MEM_VOLATILE_P (orig_mem);
  MEM_IN_STRUCT_P (mem) = MEM_IN_STRUCT_P (orig_mem);
#ifdef MEM_UNALIGNED_P
  MEM_UNALIGNED_P (mem) = MEM_UNALIGNED_P (orig_mem);
#endif
  return mem;
}

/* Expand a block move operation, and return 1 if successful.  Return 0
   if we should let the compiler generate normal code.

   operands[0] is the destination
   operands[1] is the source
   operands[2] is the length
   operands[3] is the alignment */

#define MAX_MOVE_REG 4

int
expand_block_move (operands)
     rtx operands[];
{
  rtx orig_dest = operands[0];
  rtx orig_src	= operands[1];
  rtx bytes_rtx	= operands[2];
  rtx align_rtx = operands[3];
  int constp	= (GET_CODE (bytes_rtx) == CONST_INT);
  int align	= XINT (align_rtx, 0);
  int bytes;
  int offset;
  int num_reg;
  int i;
  rtx src_reg;
  rtx dest_reg;
  rtx src_addr;
  rtx dest_addr;
  rtx tmp_reg;
  rtx stores[MAX_MOVE_REG];
  int move_bytes;

  /* If this is not a fixed size move, just call memcpy */
  if (!constp)
    return 0;

  /* Anything to move? */
  bytes = INTVAL (bytes_rtx);
  if (bytes <= 0)
    return 1;

  /* Don't support real large moves.  If string instructions are not used,
     then don't generate more than 8 loads.  */
  if (TARGET_STRING)
    {
      if (bytes > 4*8)
	return 0;
    }
  else if (!STRICT_ALIGNMENT)
    {
      if (bytes > 4*8)
	return 0;
    }
  else if (bytes > 8*align)
    return 0;

  /* Move the address into scratch registers.  */
  dest_reg = copy_addr_to_reg (XEXP (orig_dest, 0));
  src_reg  = copy_addr_to_reg (XEXP (orig_src,  0));

  if (TARGET_STRING)	/* string instructions are available */
    {
      for ( ; bytes > 0; bytes -= move_bytes)
	{
	  if (bytes > 24		/* move up to 32 bytes at a time */
	      && !fixed_regs[5]
	      && !fixed_regs[6]
	      && !fixed_regs[7]
	      && !fixed_regs[8]
	      && !fixed_regs[9]
	      && !fixed_regs[10]
	      && !fixed_regs[11]
	      && !fixed_regs[12])
	    {
	      move_bytes = (bytes > 32) ? 32 : bytes;
	      emit_insn (gen_movstrsi_8reg (expand_block_move_mem (BLKmode, dest_reg, orig_dest),
					    expand_block_move_mem (BLKmode, src_reg, orig_src),
					    GEN_INT ((move_bytes == 32) ? 0 : move_bytes),
					    align_rtx));
	    }
	  else if (bytes > 16	/* move up to 24 bytes at a time */
		   && !fixed_regs[7]
		   && !fixed_regs[8]
		   && !fixed_regs[9]
		   && !fixed_regs[10]
		   && !fixed_regs[11]
		   && !fixed_regs[12])
	    {
	      move_bytes = (bytes > 24) ? 24 : bytes;
	      emit_insn (gen_movstrsi_6reg (expand_block_move_mem (BLKmode, dest_reg, orig_dest),
					    expand_block_move_mem (BLKmode, src_reg, orig_src),
					    GEN_INT (move_bytes),
					    align_rtx));
	    }
	  else if (bytes > 8	/* move up to 16 bytes at a time */
		   && !fixed_regs[9]
		   && !fixed_regs[10]
		   && !fixed_regs[11]
		   && !fixed_regs[12])
	    {
	      move_bytes = (bytes > 16) ? 16 : bytes;
	      emit_insn (gen_movstrsi_4reg (expand_block_move_mem (BLKmode, dest_reg, orig_dest),
					    expand_block_move_mem (BLKmode, src_reg, orig_src),
					    GEN_INT (move_bytes),
					    align_rtx));
	    }
	  else if (bytes > 4 && !TARGET_64BIT)
	    {			/* move up to 8 bytes at a time */
	      move_bytes = (bytes > 8) ? 8 : bytes;
	      emit_insn (gen_movstrsi_2reg (expand_block_move_mem (BLKmode, dest_reg, orig_dest),
					    expand_block_move_mem (BLKmode, src_reg, orig_src),
					    GEN_INT (move_bytes),
					    align_rtx));
	    }
	  else if (bytes >= 4 && (align >= 4 || !STRICT_ALIGNMENT))
	    {			/* move 4 bytes */
	      move_bytes = 4;
	      tmp_reg = gen_reg_rtx (SImode);
	      emit_move_insn (tmp_reg, expand_block_move_mem (SImode, src_reg, orig_src));
	      emit_move_insn (expand_block_move_mem (SImode, dest_reg, orig_dest), tmp_reg);
	    }
	  else if (bytes == 2 && (align >= 2 || !STRICT_ALIGNMENT))
	    {			/* move 2 bytes */
	      move_bytes = 2;
	      tmp_reg = gen_reg_rtx (HImode);
	      emit_move_insn (tmp_reg, expand_block_move_mem (HImode, src_reg, orig_src));
	      emit_move_insn (expand_block_move_mem (HImode, dest_reg, orig_dest), tmp_reg);
	    }
	  else if (bytes == 1)	/* move 1 byte */
	    {
	      move_bytes = 1;
	      tmp_reg = gen_reg_rtx (QImode);
	      emit_move_insn (tmp_reg, expand_block_move_mem (QImode, src_reg, orig_src));
	      emit_move_insn (expand_block_move_mem (QImode, dest_reg, orig_dest), tmp_reg);
	    }
	  else
	    {			/* move up to 4 bytes at a time */
	      move_bytes = (bytes > 4) ? 4 : bytes;
	      emit_insn (gen_movstrsi_1reg (expand_block_move_mem (BLKmode, dest_reg, orig_dest),
					    expand_block_move_mem (BLKmode, src_reg, orig_src),
					    GEN_INT (move_bytes),
					    align_rtx));
	    }

	  if (bytes > move_bytes)
	    {
	      emit_insn (gen_addsi3 (src_reg, src_reg, GEN_INT (move_bytes)));
	      emit_insn (gen_addsi3 (dest_reg, dest_reg, GEN_INT (move_bytes)));
	    }
	}
    }

  else			/* string instructions not available */
    {
      num_reg = offset = 0;
      for ( ; bytes > 0; (bytes -= move_bytes), (offset += move_bytes))
	{
	  /* Calculate the correct offset for src/dest */
	  if (offset == 0)
	    {
	      src_addr  = src_reg;
	      dest_addr = dest_reg;
	    }
	  else
	    {
	      src_addr  = gen_rtx_PLUS (Pmode, src_reg,  GEN_INT (offset));
	      dest_addr = gen_rtx_PLUS (Pmode, dest_reg, GEN_INT (offset));
	    }

	  /* Generate the appropriate load and store, saving the stores for later */
	  if (bytes >= 8 && TARGET_64BIT && (align >= 8 || !STRICT_ALIGNMENT))
	    {
	      move_bytes = 8;
	      tmp_reg = gen_reg_rtx (DImode);
	      emit_insn (gen_movdi (tmp_reg, expand_block_move_mem (DImode, src_addr, orig_src)));
	      stores[ num_reg++ ] = gen_movdi (expand_block_move_mem (DImode, dest_addr, orig_dest), tmp_reg);
	    }
	  else if (bytes >= 4 && (align >= 4 || !STRICT_ALIGNMENT))
	    {
	      move_bytes = 4;
	      tmp_reg = gen_reg_rtx (SImode);
	      emit_insn (gen_movsi (tmp_reg, expand_block_move_mem (SImode, src_addr, orig_src)));
	      stores[ num_reg++ ] = gen_movsi (expand_block_move_mem (SImode, dest_addr, orig_dest), tmp_reg);
	    }
	  else if (bytes >= 2 && (align >= 2 || !STRICT_ALIGNMENT))
	    {
	      move_bytes = 2;
	      tmp_reg = gen_reg_rtx (HImode);
	      emit_insn (gen_movsi (tmp_reg, expand_block_move_mem (HImode, src_addr, orig_src)));
	      stores[ num_reg++ ] = gen_movhi (expand_block_move_mem (HImode, dest_addr, orig_dest), tmp_reg);
	    }
	  else
	    {
	      move_bytes = 1;
	      tmp_reg = gen_reg_rtx (QImode);
	      emit_insn (gen_movsi (tmp_reg, expand_block_move_mem (QImode, src_addr, orig_src)));
	      stores[ num_reg++ ] = gen_movqi (expand_block_move_mem (QImode, dest_addr, orig_dest), tmp_reg);
	    }

	  if (num_reg >= MAX_MOVE_REG)
	    {
	      for (i = 0; i < num_reg; i++)
		emit_insn (stores[i]);
	      num_reg = 0;
	    }
	}

      for (i = 0; i < num_reg; i++)
	emit_insn (stores[i]);
    }

  return 1;
}


/* Return 1 if OP is a load multiple operation.  It is known to be a
   PARALLEL and the first section will be tested.  */

int
load_multiple_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  int dest_regno;
  rtx src_addr;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != REG
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != MEM)
    return 0;

  dest_regno = REGNO (SET_DEST (XVECEXP (op, 0, 0)));
  src_addr = XEXP (SET_SRC (XVECEXP (op, 0, 0)), 0);

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_DEST (elt)) != REG
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || REGNO (SET_DEST (elt)) != dest_regno + i
	  || GET_CODE (SET_SRC (elt)) != MEM
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || GET_CODE (XEXP (SET_SRC (elt), 0)) != PLUS
	  || ! rtx_equal_p (XEXP (XEXP (SET_SRC (elt), 0), 0), src_addr)
	  || GET_CODE (XEXP (XEXP (SET_SRC (elt), 0), 1)) != CONST_INT
	  || INTVAL (XEXP (XEXP (SET_SRC (elt), 0), 1)) != i * 4)
	return 0;
    }

  return 1;
}

/* Similar, but tests for store multiple.  Here, the second vector element
   is a CLOBBER.  It will be tested later.  */

int
store_multiple_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0) - 1;
  int src_regno;
  rtx dest_addr;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != MEM
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != REG)
    return 0;

  src_regno = REGNO (SET_SRC (XVECEXP (op, 0, 0)));
  dest_addr = XEXP (SET_DEST (XVECEXP (op, 0, 0)), 0);

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i + 1);

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_SRC (elt)) != REG
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || REGNO (SET_SRC (elt)) != src_regno + i
	  || GET_CODE (SET_DEST (elt)) != MEM
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || GET_CODE (XEXP (SET_DEST (elt), 0)) != PLUS
	  || ! rtx_equal_p (XEXP (XEXP (SET_DEST (elt), 0), 0), dest_addr)
	  || GET_CODE (XEXP (XEXP (SET_DEST (elt), 0), 1)) != CONST_INT
	  || INTVAL (XEXP (XEXP (SET_DEST (elt), 0), 1)) != i * 4)
	return 0;
    }

  return 1;
}

/* Return 1 if OP is an equality operator.  */

int
equality_operator (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);
  if (mode == VOIDmode && (code == EQ || code == NE))
    return 1;
  return 0;
}

/* Return 1 if OP is a vector comparison operator.  */

int
vector_comparison_operator (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);
  if (mode == SImode && (code == EQ || code == LT))
    return 1;
  return 0;
}

/* Return 1 if OP is a comparison operation that is valid for a branch insn.
   We only check the opcode against the mode of the CC value here.  */

int
branch_comparison_operator (op, mode)
     register rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;

  if (GET_RTX_CLASS (code) != '<')
    return 0;

  cc_mode = GET_MODE (XEXP (op, 0));
  if (GET_MODE_CLASS (cc_mode) != MODE_CC)
    return 0;

  if ((code == GT || code == LT || code == GE || code == LE)
      && cc_mode == CCUNSmode)
    return 0;

  if ((code == GTU || code == LTU || code == GEU || code == LEU)
      && (cc_mode != CCUNSmode))
    return 0;

  return 1;
}

/* Return 1 if OP is a comparison operation that is valid for an scc insn.
   We check the opcode against the mode of the CC value and disallow EQ or
   NE comparisons for integers.  */

int
scc_comparison_operator (op, mode)
     register rtx op;
     enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;

  if (GET_MODE (op) != mode && mode != VOIDmode)
    return 0;

  if (GET_RTX_CLASS (code) != '<')
    return 0;

  cc_mode = GET_MODE (XEXP (op, 0));
  if (GET_MODE_CLASS (cc_mode) != MODE_CC)
    return 0;

  if (code == NE && cc_mode != CCFPmode)
    return 0;

  if ((code == GT || code == LT || code == GE || code == LE)
      && cc_mode == CCUNSmode)
    return 0;

  if ((code == GTU || code == LTU || code == GEU || code == LEU)
      && (cc_mode != CCUNSmode))
    return 0;

  if (cc_mode == CCEQmode && code != EQ && code != NE)
    return 0;

  return 1;
}

int
trap_comparison_operator (op, mode)
    rtx op;
    enum machine_mode mode;
{
  if (mode != VOIDmode && mode != GET_MODE (op))
    return 0;
  return (GET_RTX_CLASS (GET_CODE (op)) == '<'
          || GET_CODE (op) == EQ || GET_CODE (op) == NE);
}

/* Return 1 if ANDOP is a mask that has no bits on that are not in the
   mask required to convert the result of a rotate insn into a shift
   left insn of SHIFTOP bits.  Both are known to be CONST_INT.  */

int
includes_lshift_p (shiftop, andop)
     register rtx shiftop;
     register rtx andop;
{
  int shift_mask = (~0 << INTVAL (shiftop));

  return (INTVAL (andop) & ~shift_mask) == 0;
}

/* Similar, but for right shift.  */

int
includes_rshift_p (shiftop, andop)
     register rtx shiftop;
     register rtx andop;
{
  unsigned HOST_WIDE_INT shift_mask = ~(unsigned HOST_WIDE_INT) 0;

  shift_mask >>= INTVAL (shiftop);

  return (INTVAL (andop) & ~ shift_mask) == 0;
}

/* Return 1 if REGNO (reg1) == REGNO (reg2) - 1 making them candidates
   for lfq and stfq insns.

   Note reg1 and reg2 *must* be hard registers.  To be sure we will
   abort if we are passed pseudo registers.  */

int
registers_ok_for_quad_peep (reg1, reg2)
     rtx reg1, reg2;
{
  /* We might have been passed a SUBREG.  */
  if (GET_CODE (reg1) != REG || GET_CODE (reg2) != REG) 
    return 0;

  return (REGNO (reg1) == REGNO (reg2) - 1);
}

/* Return 1 if addr1 and addr2 are suitable for lfq or stfq insn.  addr1 and
   addr2 must be in consecutive memory locations (addr2 == addr1 + 8).  */

int
addrs_ok_for_quad_peep (addr1, addr2)
     register rtx addr1;
     register rtx addr2;
{
  int reg1;
  int offset1;

  /* Extract an offset (if used) from the first addr.  */
  if (GET_CODE (addr1) == PLUS)
    {
      /* If not a REG, return zero.  */
      if (GET_CODE (XEXP (addr1, 0)) != REG)
	return 0;
      else
	{
          reg1 = REGNO (XEXP (addr1, 0));
	  /* The offset must be constant!  */
	  if (GET_CODE (XEXP (addr1, 1)) != CONST_INT)
            return 0;
          offset1 = INTVAL (XEXP (addr1, 1));
	}
    }
  else if (GET_CODE (addr1) != REG)
    return 0;
  else
    {
      reg1 = REGNO (addr1);
      /* This was a simple (mem (reg)) expression.  Offset is 0.  */
      offset1 = 0;
    }

  /* Make sure the second address is a (mem (plus (reg) (const_int).  */
  if (GET_CODE (addr2) != PLUS)
    return 0;

  if (GET_CODE (XEXP (addr2, 0)) != REG
      || GET_CODE (XEXP (addr2, 1)) != CONST_INT)
    return 0;

  if (reg1 != REGNO (XEXP (addr2, 0)))
    return 0;

  /* The offset for the second addr must be 8 more than the first addr.  */
  if (INTVAL (XEXP (addr2, 1)) != offset1 + 8)
    return 0;

  /* All the tests passed.  addr1 and addr2 are valid for lfq or stfq
     instructions.  */
  return 1;
}

/* Return the register class of a scratch register needed to copy IN into
   or out of a register in CLASS in MODE.  If it can be done directly,
   NO_REGS is returned.  */

enum reg_class
secondary_reload_class (class, mode, in)
     enum reg_class class;
     enum machine_mode mode ATTRIBUTE_UNUSED;
     rtx in;
{
  int regno = true_regnum (in);

  if (regno >= FIRST_PSEUDO_REGISTER)
    regno = -1;

  /* We can not copy a symbolic operand directly into anything other than
     BASE_REGS for TARGET_ELF.  So indicate that a register from BASE_REGS
     is needed as an intermediate register.  */
  if (TARGET_ELF
      && class != BASE_REGS
      && (GET_CODE (in) == SYMBOL_REF
	  || GET_CODE (in) == LABEL_REF
	  || GET_CODE (in) == CONST))
    return BASE_REGS;

  /* We can place anything into GENERAL_REGS and can put GENERAL_REGS
     into anything.  */
  if (class == GENERAL_REGS || class == BASE_REGS
      || (regno >= 0 && INT_REGNO_P (regno)))
    return NO_REGS;

  /* Constants, memory, and FP registers can go into FP registers.  */
  if ((regno == -1 || FP_REGNO_P (regno))
      && (class == FLOAT_REGS || class == NON_SPECIAL_REGS))
    return NO_REGS;

  /* Easy constants, memory, and Vector registers can go into Vector registers.  */
  if (class == VECTOR_REGS
      && (VECTOR_REGNO_P (regno)
	  || (regno == -1
	      && (GET_CODE (in) != CONST_VECTOR
		  || easy_vector_constant (in)))))
    return NO_REGS;

  /* We can copy among the CR registers.  */
  if ((class == CR_REGS || class == CR0_REGS)
      && regno >= 0 && CR_REGNO_P (regno))
    return NO_REGS;

  /* Otherwise, we need GENERAL_REGS.  */
  return GENERAL_REGS;
}

/* Given a comparison operation, return the bit number in CCR to test.  We
   know this is a valid comparison.  

   SCC_P is 1 if this is for an scc.  That means that %D will have been
   used instead of %C, so the bits will be in different places.

   Return -1 if OP isn't a valid comparison for some reason.  */

int
ccr_bit (op, scc_p)
     register rtx op;
     int scc_p;
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;
  int cc_regnum;
  int base_bit;

  if (GET_RTX_CLASS (code) != '<')
    return -1;

  cc_mode = GET_MODE (XEXP (op, 0));
  cc_regnum = REGNO (XEXP (op, 0));
  base_bit = 4 * (cc_regnum - 68);

  /* In CCEQmode cases we have made sure that the result is always in the
     third bit of the CR field.  */

  if (cc_mode == CCEQmode)
    return base_bit + 3;

  switch (code)
    {
    case NE:
      return scc_p ? base_bit + 3 : base_bit + 2;
    case EQ:
      return base_bit + 2;
    case GT:  case GTU:
      return base_bit + 1;
    case LT:  case LTU:
      return base_bit;

    case GE:  case GEU:
      /* If floating-point, we will have done a cror to put the bit in the
	 unordered position.  So test that bit.  For integer, this is ! LT
	 unless this is an scc insn.  */
      return cc_mode == CCFPmode || scc_p ? base_bit + 3 : base_bit;

    case LE:  case LEU:
      return cc_mode == CCFPmode || scc_p ? base_bit + 3 : base_bit + 1;

    default:
      abort ();
    }
}

/* Return the GOT register, creating it if needed.  */

struct rtx_def *
rs6000_got_register (value)
     rtx value;
{
  if (!current_function_uses_pic_offset_table || !pic_offset_table_rtx)
    {
      if (reload_in_progress || reload_completed)
	fatal_insn ("internal error -- needed new GOT register during reload phase to load:", value);

      current_function_uses_pic_offset_table = 1;
      pic_offset_table_rtx = gen_rtx_REG (Pmode, GOT_TOC_REGNUM);
    }

  return pic_offset_table_rtx;
}


/* Replace all occurrences of register FROM with an new pseudo register in an insn X.
   Store the pseudo register used in REG.
   This is only safe during FINALIZE_PIC, since the registers haven't been setup
   yet.  */

static rtx
rs6000_replace_regno (x, from, reg)
     rtx x;
     int from;
     rtx *reg;
{
  register int i, j;
  register char *fmt;

  /* Allow this function to make replacements in EXPR_LISTs.  */
  if (!x)
    return x;

  switch (GET_CODE (x))
    {
    case SCRATCH:
    case PC:
    case CC0:
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case LABEL_REF:
      return x;

    case REG:
      if (REGNO (x) == from)
	{
	  if (! *reg)
	    *reg = pic_offset_table_rtx = gen_reg_rtx (Pmode);

	  return *reg;
	}

      return x;

    default:
      break;
    }

  fmt = GET_RTX_FORMAT (GET_CODE (x));
  for (i = GET_RTX_LENGTH (GET_CODE (x)) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	XEXP (x, i) = rs6000_replace_regno (XEXP (x, i), from, reg);
      else if (fmt[i] == 'E')
	for (j = XVECLEN (x, i) - 1; j >= 0; j--)
	  XVECEXP (x, i, j) = rs6000_replace_regno (XVECEXP (x, i, j), from, reg);
    }

  return x;
}  


/* By generating position-independent code, when two different
   programs (A and B) share a common library (libC.a), the text of
   the library can be shared whether or not the library is linked at
   the same address for both programs.  In some of these
   environments, position-independent code requires not only the use
   of different addressing modes, but also special code to enable the
   use of these addressing modes.

   The `FINALIZE_PIC' macro serves as a hook to emit these special
   codes once the function is being compiled into assembly code, but
   not before.  (It is not done before, because in the case of
   compiling an inline function, it would lead to multiple PIC
   prologues being included in functions which used inline functions
   and were compiled to assembly language.)  */

void
rs6000_finalize_pic ()
{
  /* Loop through all of the insns, replacing the special GOT_TOC_REGNUM
     with an appropriate pseudo register.  If we find we need GOT/TOC,
     add the appropriate init code.  */
  if (flag_pic && (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS))
    {
      rtx insn = get_insns ();
      rtx reg = NULL_RTX;
      rtx first_insn;
      rtx last_insn = NULL_RTX;

      if (GET_CODE (insn) == NOTE)
	insn = next_nonnote_insn (insn);

      first_insn = insn;
      for ( ; insn != NULL_RTX; insn = NEXT_INSN (insn))
	{
	  if (GET_RTX_CLASS (GET_CODE (insn)) == 'i')
	    {
	      PATTERN (insn) = rs6000_replace_regno (PATTERN (insn),
						     GOT_TOC_REGNUM,
						     &reg);

	      if (REG_NOTES (insn))
		REG_NOTES (insn) = rs6000_replace_regno (REG_NOTES (insn),
							 GOT_TOC_REGNUM,
							 &reg);
	    }

	  if (GET_CODE (insn) != NOTE)
	    last_insn = insn;
	}

      if (reg)
	{
	  rtx init = gen_init_v4_pic (reg);
	  emit_insn_before (init, first_insn);
	  if (!optimize && last_insn)
	    emit_insn_after (gen_rtx_USE (VOIDmode, reg), last_insn);
	}
    }
}


/* Search for any occurrence of the GOT_TOC register marker that should
   have been eliminated, but may have crept back in.  */

void
rs6000_reorg (insn)
     rtx insn;
{
  if (flag_pic && (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS))
    {
      rtx got_reg = gen_rtx_REG (Pmode, GOT_TOC_REGNUM);
      for ( ; insn != NULL_RTX; insn = NEXT_INSN (insn))
	if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
	    && reg_mentioned_p (got_reg, PATTERN (insn)))
	  fatal_insn ("GOT/TOC register marker not removed:", PATTERN (insn));
    }
}


/* Define the structure for the machine field in struct function.  */
struct machine_function
{
  int sysv_varargs_p;
  int save_toc_p;
  int fpmem_size;
  int fpmem_offset;
  rtx pic_offset_table_rtx;
};

/* Functions to save and restore rs6000_fpmem_size.
   These will be called, via pointer variables,
   from push_function_context and pop_function_context.  */

void
rs6000_save_machine_status (p)
     struct function *p;
{
  struct machine_function *machine =
    (struct machine_function *) xmalloc (sizeof (struct machine_function));

  p->machine = machine;
  machine->sysv_varargs_p = rs6000_sysv_varargs_p;
  machine->fpmem_size     = rs6000_fpmem_size;
  machine->fpmem_offset   = rs6000_fpmem_offset;
  machine->pic_offset_table_rtx = pic_offset_table_rtx;
}

void
rs6000_restore_machine_status (p)
     struct function *p;
{
  struct machine_function *machine = p->machine;

  rs6000_sysv_varargs_p = machine->sysv_varargs_p;
  rs6000_fpmem_size     = machine->fpmem_size;
  rs6000_fpmem_offset   = machine->fpmem_offset;
  pic_offset_table_rtx  = machine->pic_offset_table_rtx;

  free (machine);
  p->machine = (struct machine_function *)0;
}

/* Do anything needed before RTL is emitted for each function.  */

void
rs6000_init_expanders ()
{
  /* Reset varargs and save TOC indicator */
  rs6000_sysv_varargs_p = 0;
  rs6000_fpmem_size = 0;
  rs6000_fpmem_offset = 0;
#ifndef MACHO_PIC
  pic_offset_table_rtx = (rtx)0;
#endif

  /* Arrange to save and restore machine status around nested functions.  */
  save_machine_status = rs6000_save_machine_status;
  restore_machine_status = rs6000_restore_machine_status;
}


/* Print an operand.  Recognize special options, documented below.  */

#if TARGET_ELF
#define SMALL_DATA_RELOC ((rs6000_sdata == SDATA_EABI) ? "sda21" : "sdarel")
#define SMALL_DATA_REG ((rs6000_sdata == SDATA_EABI) ? 0 : 13)
#else
#define SMALL_DATA_RELOC "sda21"
#define SMALL_DATA_REG 0
#endif

void
print_operand (file, x, code)
    FILE *file;
    rtx x;
    char code;
{
  int i;
  HOST_WIDE_INT val;

  /* These macros test for integers and extract the low-order bits.  */
#define INT_P(X)  \
((GET_CODE (X) == CONST_INT || GET_CODE (X) == CONST_DOUBLE)	\
 && GET_MODE (X) == VOIDmode)

#define INT_LOWPART(X) \
  (GET_CODE (X) == CONST_INT ? INTVAL (X) : CONST_DOUBLE_LOW (X))

  switch (code)
    {
    case '.':
      /* Write out an instruction after the call which may be replaced
	 with glue code by the loader.  This depends on the AIX version.  */
      asm_fprintf (file, RS6000_CALL_GLUE);
      return;

    case '*':
      /* Write the register number of the TOC register.  */
      fputs (TARGET_MINIMAL_TOC ? reg_names[30] : reg_names[2], file);
      return;

    case '$':
      /* Write out either a '.' or '$' for the current location, depending
	 on whether this is Solaris or not.  */
      putc ((DEFAULT_ABI == ABI_SOLARIS) ? '.' : '$', file);
      return;

    case 'A':
      /* If X is a constant integer whose low-order 5 bits are zero,
	 write 'l'.  Otherwise, write 'r'.  This is a kludge to fix a bug
	 in the AIX assembler where "sri" with a zero shift count
	 write a trash instruction.  */
      if (GET_CODE (x) == CONST_INT && (INTVAL (x) & 31) == 0)
	putc ('l', file);
      else
	putc ('r', file);
      return;

    case 'b':
      /* Low-order 16 bits of constant, unsigned.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%b value");

      fprintf (file, "%d", INT_LOWPART (x) & 0xffff);
      return;

    case 'B':
      /* If the low-order bit is zero, write 'r'; otherwise, write 'l'
	 for 64-bit mask direction.  */
      putc (((INT_LOWPART(x) & 1) == 0 ? 'r' : 'l'), file);
      return;

    case 'C':
      /* This is an optional cror needed for LE or GE floating-point
	 comparisons.  Otherwise write nothing.  */
      if ((GET_CODE (x) == LE || GET_CODE (x) == GE)
	  && GET_MODE (XEXP (x, 0)) == CCFPmode)
	{
	  int base_bit = 4 * (REGNO (XEXP (x, 0)) - 68);

	  fprintf (file, "cror %d,%d,%d\n\t", base_bit + 3,
		   base_bit + 2, base_bit + (GET_CODE (x) == GE));
	}
      return;

    case 'D':
      /* Similar, except that this is for an scc, so we must be able to
	 encode the test in a single bit that is one.  We do the above
	 for any LE, GE, GEU, or LEU and invert the bit for NE.  */
      if (GET_CODE (x) == LE || GET_CODE (x) == GE
	  || GET_CODE (x) == LEU || GET_CODE (x) == GEU)
	{
	  int base_bit = 4 * (REGNO (XEXP (x, 0)) - 68);

	  fprintf (file, "cror %d,%d,%d\n\t", base_bit + 3,
		   base_bit + 2,
		   base_bit + (GET_CODE (x) == GE || GET_CODE (x) == GEU));
	}

      else if (GET_CODE (x) == NE)
	{
	  int base_bit = 4 * (REGNO (XEXP (x, 0)) - 68);

	  fprintf (file, "crnor %d,%d,%d\n\t", base_bit + 3,
		   base_bit + 2, base_bit + 2);
	}
      return;

    case 'E':
      /* X is a CR register.  Print the number of the third bit of the CR */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%E value");

      fprintf(file, "%d", 4 * (REGNO (x) - 68) + 3);
      return;

    case 'f':
      /* X is a CR register.  Print the shift count needed to move it
	 to the high-order four bits.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%f value");
      else
	fprintf (file, "%d", 4 * (REGNO (x) - 68));
      return;

    case 'F':
      /* Similar, but print the count for the rotate in the opposite
	 direction.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%F value");
      else
	fprintf (file, "%d", 32 - 4 * (REGNO (x) - 68));
      return;

    case 'G':
      /* X is a constant integer.  If it is negative, print "m",
	 otherwise print "z".  This is to make a aze or ame insn.  */
      if (GET_CODE (x) != CONST_INT)
	output_operand_lossage ("invalid %%G value");
      else if (INTVAL (x) >= 0)
	putc ('z', file);
      else
	putc ('m', file);
      return;
	
    case 'h':
      /* If constant, output low-order five bits.  Otherwise,
	 write normally. */
      if (INT_P (x))
	fprintf (file, "%d", INT_LOWPART (x) & 31);
      else
	print_operand (file, x, 0);
      return;

    case 'H':
      /* If constant, output low-order six bits.  Otherwise,
	 write normally. */
      if (INT_P (x))
	fprintf (file, "%d", INT_LOWPART (x) & 63);
      else
	print_operand (file, x, 0);
      return;

    case 'I':
      /* Print `i' if this is a constant, else nothing.  */
      if (INT_P (x))
	putc ('i', file);
      return;

    case 'j':
      /* Write the bit number in CCR for jump.  */
      i = ccr_bit (x, 0);
      if (i == -1)
	output_operand_lossage ("invalid %%j code");
      else
	fprintf (file, "%d", i);
      return;

    case 'J':
      /* Similar, but add one for shift count in rlinm for scc and pass
	 scc flag to `ccr_bit'.  */
      i = ccr_bit (x, 1);
      if (i == -1)
	output_operand_lossage ("invalid %%J code");
      else
	/* If we want bit 31, write a shift count of zero, not 32.  */
	fprintf (file, "%d", i == 31 ? 0 : i + 1);
      return;

    case 'k':
      /* X must be a constant.  Write the 1's complement of the
	 constant.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%k value");

      fprintf (file, "%d", ~ INT_LOWPART (x));
      return;

    case 'L':
      /* Write second word of DImode or DFmode reference.  Works on register
	 or non-indexed memory only.  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 1]);
      else if (GET_CODE (x) == MEM)
	{
	  /* Handle possible auto-increment.  Since it is pre-increment and
	     we have already done it, we can just use an offset of word.  */
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0),
					   UNITS_PER_WORD));
	  else
	    output_address (plus_constant (XEXP (x, 0), UNITS_PER_WORD));
	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;
			    
    case 'm':
      /* MB value for a mask operand.  */
      if (! mask_operand (x, VOIDmode))
	output_operand_lossage ("invalid %%m value");

      val = INT_LOWPART (x);

      /* If the high bit is set and the low bit is not, the value is zero.
	 If the high bit is zero, the value is the first 1 bit we find from
	 the left.  */
      if (val < 0 && (val & 1) == 0)
	{
	  putc ('0', file);
	  return;
	}
      else if (val >= 0)
	{
	  for (i = 1; i < 32; i++)
	    if ((val <<= 1) < 0)
	      break;
	  fprintf (file, "%d", i);
	  return;
	}
	  
      /* Otherwise, look for the first 0 bit from the right.  The result is its
	 number plus 1. We know the low-order bit is one.  */
      for (i = 0; i < 32; i++)
	if (((val >>= 1) & 1) == 0)
	  break;

      /* If we ended in ...01, i would be 0.  The correct value is 31, so
	 we want 31 - i.  */
      fprintf (file, "%d", 31 - i);
      return;

    case 'M':
      /* ME value for a mask operand.  */
      if (! mask_operand (x, VOIDmode))
	output_operand_lossage ("invalid %%M value");

      val = INT_LOWPART (x);

      /* If the low bit is set and the high bit is not, the value is 31.
	 If the low bit is zero, the value is the first 1 bit we find from
	 the right.  */
      if ((val & 1) && val >= 0)
	{
	  fputs ("31", file);
	  return;
	}
      else if ((val & 1) == 0)
	{
	  for (i = 0; i < 32; i++)
	    if ((val >>= 1) & 1)
	      break;

	  /* If we had ....10, i would be 0.  The result should be
	     30, so we need 30 - i.  */
	  fprintf (file, "%d", 30 - i);
	  return;
	}
	  
      /* Otherwise, look for the first 0 bit from the left.  The result is its
	 number minus 1. We know the high-order bit is one.  */
      for (i = 0; i < 32; i++)
	if ((val <<= 1) >= 0)
	  break;

      fprintf (file, "%d", i);
      return;

    case 'N':
      /* Write the number of elements in the vector times 4.  */
      if (GET_CODE (x) != PARALLEL)
	output_operand_lossage ("invalid %%N value");

      fprintf (file, "%d", XVECLEN (x, 0) * 4);
      return;

    case 'O':
      /* Similar, but subtract 1 first.  */
      if (GET_CODE (x) != PARALLEL)
	output_operand_lossage ("invalid %%O value");

      fprintf (file, "%d", (XVECLEN (x, 0) - 1) * 4);
      return;

    case 'p':
      /* X is a CONST_INT that is a power of two.  Output the logarithm.  */
      if (! INT_P (x)
	  || (i = exact_log2 (INT_LOWPART (x))) < 0)
	output_operand_lossage ("invalid %%p value");

      fprintf (file, "%d", i);
      return;

    case 'P':
      /* The operand must be an indirect memory reference.  The result
	 is the register number. */
      if (GET_CODE (x) != MEM || GET_CODE (XEXP (x, 0)) != REG
	  || REGNO (XEXP (x, 0)) >= 32)
	output_operand_lossage ("invalid %%P value");

      fprintf (file, "%s", reg_names[REGNO (XEXP (x, 0))]);
      return;

    case 'R':
      /* X is a CR register.  Print the mask for `mtcrf'.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%R value");
      else
	fprintf (file, "%d", 128 >> (REGNO (x) - 68));
      return;

    case 's':
      /* Low 5 bits of 32 - value */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%s value");

      fprintf (file, "%d", (32 - INT_LOWPART (x)) & 31);
      return;

    case 'S':
      /* PowerPC64 mask position.  All 0's and all 1's are excluded.
	 CONST_INT 32-bit mask is considered sign-extended so any
	 transition must occur within the CONST_INT, not on the boundary.  */
      if (! mask64_operand (x, VOIDmode))
	output_operand_lossage ("invalid %%S value");

      val = INT_LOWPART (x);

      if (val & 1)      /* Clear Left */
	{
	  for (i = 0; i < HOST_BITS_PER_WIDE_INT; i++)
	    if (!((val >>= 1) & 1))
	      break;

#if HOST_BITS_PER_WIDE_INT == 32
	  if (GET_CODE (x) == CONST_DOUBLE && i == 32)
	    {
	      val = CONST_DOUBLE_HIGH (x);

	      if (val == 0)
		--i;
	      else
		for (i = 32; i < 64; i++)
		  if (!((val >>= 1) & 1))
		    break;
	    }
#endif
	/* i = index of last set bit from right
	   mask begins at 63 - i from left */
	  if (i > 63)
	    output_operand_lossage ("%%S computed all 1's mask");
	  fprintf (file, "%d", 63 - i);
	  return;
	}
      else	/* Clear Right */
	{
	  for (i = 0; i < HOST_BITS_PER_WIDE_INT; i++)
	    if ((val >>= 1) & 1)
	      break;

#if HOST_BITS_PER_WIDE_INT == 32
	if (GET_CODE (x) == CONST_DOUBLE && i == 32)
	  {
	    val = CONST_DOUBLE_HIGH (x);

	    if (val == (HOST_WIDE_INT) -1)
	      --i;
	    else
	      for (i = 32; i < 64; i++)
		if ((val >>= 1) & 1)
		  break;
	  }
#endif
	/* i = index of last clear bit from right
	   mask ends at 62 - i from left */
	  if (i > 62)
	    output_operand_lossage ("%%S computed all 0's mask");
	  fprintf (file, "%d", 62 - i);
	  return;
	}

    case 't':
      /* Write 12 if this jump operation will branch if true, 4 otherwise. 
	 All floating-point operations except NE branch true and integer
	 EQ, LT, GT, LTU and GTU also branch true.  */
      if (GET_RTX_CLASS (GET_CODE (x)) != '<')
	output_operand_lossage ("invalid %%t value");

      else if ((GET_MODE (XEXP (x, 0)) == CCFPmode
		&& GET_CODE (x) != NE)
	       || GET_CODE (x) == EQ
	       || GET_CODE (x) == LT || GET_CODE (x) == GT
	       || GET_CODE (x) == LTU || GET_CODE (x) == GTU)
	fputs ("12", file);
      else
	putc ('4', file);
      return;
      
    case 'T':
      /* Opposite of 't': write 4 if this jump operation will branch if true,
	 12 otherwise.   */
      if (GET_RTX_CLASS (GET_CODE (x)) != '<')
	output_operand_lossage ("invalid %%T value");

      else if ((GET_MODE (XEXP (x, 0)) == CCFPmode
		&& GET_CODE (x) != NE)
	       || GET_CODE (x) == EQ
	       || GET_CODE (x) == LT || GET_CODE (x) == GT
	       || GET_CODE (x) == LTU || GET_CODE (x) == GTU)
	putc ('4', file);
      else
	fputs ("12", file);
      return;
      
    case 'u':
      /* High-order 16 bits of constant for use in unsigned operand.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%u value");

      fprintf (file, "0x%x", (INT_LOWPART (x) >> 16) & 0xffff);
      return;

    case 'v':
      /* High-order 16 bits of constant for use in signed operand.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%v value");

      {
	int value = (INT_LOWPART (x) >> 16) & 0xffff;

	/* Solaris assembler doesn't like lis 0,0x80000 */
	if (DEFAULT_ABI == ABI_SOLARIS && (value & 0x8000) != 0)
	  fprintf (file, "%d", value | (~0 << 16));
	else
	  fprintf (file, "0x%x", value);
	return;
      }

    case 'U':
      /* Print `u' if this has an auto-increment or auto-decrement.  */
      if (GET_CODE (x) == MEM
	  && (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC))
	putc ('u', file);
      return;

    case 'V':
      /* Print the trap code for this operand.  */
      switch (GET_CODE (x))
	{
	case EQ:
	  fputs ("eq", file);   /* 4 */
	  break;
	case NE:
	  fputs ("ne", file);   /* 24 */
	  break;
	case LT:
	  fputs ("lt", file);   /* 16 */
	  break;
	case LE:
	  fputs ("le", file);   /* 20 */
	  break;
	case GT:
	  fputs ("gt", file);   /* 8 */
	  break;
	case GE:
	  fputs ("ge", file);   /* 12 */
	  break;
	case LTU:
	  fputs ("llt", file);  /* 2 */
	  break;
	case LEU:
	  fputs ("lle", file);  /* 6 */
	  break;
	case GTU:
	  fputs ("lgt", file);  /* 1 */
	  break;
	case GEU:
	  fputs ("lge", file);  /* 5 */
	  break;
	default:
	  abort ();
	}
      break;

    case 'w':
      /* If constant, low-order 16 bits of constant, signed.  Otherwise, write
	 normally.  */
      if (INT_P (x))
	fprintf (file, "%d",
		 (INT_LOWPART (x) & 0xffff) - 2 * (INT_LOWPART (x) & 0x8000));
      else
	print_operand (file, x, 0);
      return;

    case 'W':
      /* If constant, low-order 16 bits of constant, unsigned.
	 Otherwise, write normally.  */
      if (INT_P (x))
	fprintf (file, "%d", INT_LOWPART (x) & 0xffff);
      else
	print_operand (file, x, 0);
      return;

    case 'X':
      if (GET_CODE (x) == MEM
	  && LEGITIMATE_INDEXED_ADDRESS_P (XEXP (x, 0)))
	putc ('x', file);
      return;

    case 'Y':
      /* Like 'L', for third word of TImode  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 2]);
      else if (GET_CODE (x) == MEM)
	{
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0), 8));
	  else
	    output_address (plus_constant (XEXP (x, 0), 8));
	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;
			    
    case 'z':
      /* X is a SYMBOL_REF.  Write out the name preceded by a
	 period and without any trailing data in brackets.  Used for function
	 names.  If we are configured for System V (or the embedded ABI) on
	 the PowerPC, do not emit the period, since those systems do not use
	 TOCs and the like.  */
      if (GET_CODE (x) != SYMBOL_REF)
	abort ();

      if (XSTR (x, 0)[0] != '.')
	{
	  switch (DEFAULT_ABI)
	    {
	    default:
	      abort ();

	    case ABI_AIX:
	      putc ('.', file);
	      break;

	    case ABI_V4:
	    case ABI_AIX_NODESC:
	    case ABI_SOLARIS:
	    case ABI_RHAPSODY:
	      break;

	    case ABI_NT:
	      fputs ("..", file);
	      break;
	    }
	}
      RS6000_OUTPUT_BASENAME (file, XSTR (x, 0));
      return;

    case 'Z':
      /* Like 'L', for last word of TImode.  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 3]);
      else if (GET_CODE (x) == MEM)
	{
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0), 12));
	  else
	    output_address (plus_constant (XEXP (x, 0), 12));
	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;
			    
    case 0:
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x)]);
      else if (GET_CODE (x) == MEM)
	{
	  /* We need to handle PRE_INC and PRE_DEC here, since we need to
	     know the width from the mode.  */
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC)
	    fprintf (file, "%d(%s)", GET_MODE_SIZE (GET_MODE (x)),
		     reg_names[REGNO (XEXP (XEXP (x, 0), 0))]);
	  else if (GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    fprintf (file, "%d(%s)", - GET_MODE_SIZE (GET_MODE (x)),
		     reg_names[REGNO (XEXP (XEXP (x, 0), 0))]);
	  else
	    output_address (XEXP (x, 0));
	}
      else
	output_addr_const (file, x);
      return;

    default:
      output_operand_lossage ("invalid %%xn code");
    }
}

/* Print the address of an operand.  */

void
print_operand_address (file, x)
     FILE *file;
     register rtx x;
{
  if (GET_CODE (x) == REG)
    fprintf (file, "0(%s)", reg_names[ REGNO (x) ]);
  else if (GET_CODE (x) == SYMBOL_REF || GET_CODE (x) == CONST || GET_CODE (x) == LABEL_REF)
    {
      output_addr_const (file, x);
      if (small_data_operand (x, GET_MODE (x)))
	fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		 reg_names[SMALL_DATA_REG]);

#ifdef TARGET_NO_TOC
      else if (TARGET_NO_TOC)
	;
#endif
      else
	fprintf (file, "(%s)", reg_names[ TARGET_MINIMAL_TOC ? 30 : 2 ]);
    }
  else if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 1)) == REG)
    {
      if (REGNO (XEXP (x, 0)) == 0)
	fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (x, 1)) ],
		 reg_names[ REGNO (XEXP (x, 0)) ]);
      else
	fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (x, 0)) ],
		 reg_names[ REGNO (XEXP (x, 1)) ]);
    }
  else if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 1)) == CONST_INT)
    fprintf (file, "%d(%s)", INTVAL (XEXP (x, 1)), reg_names[ REGNO (XEXP (x, 0)) ]);
  else if (TARGET_ELF && !TARGET_64BIT && GET_CODE (x) == LO_SUM
	   && GET_CODE (XEXP (x, 0)) == REG && CONSTANT_P (XEXP (x, 1)))
    {
      output_addr_const (file, XEXP (x, 1));
      fprintf (file, "@l(%s)", reg_names[ REGNO (XEXP (x, 0)) ]);
    }
  else
    abort ();
}

/* This page contains routines that are used to determine what the function
   prologue and epilogue code will do and write them out.  */

/*  Return the first fixed-point register that is required to be saved. 32 if
    none.  */

int
first_reg_to_save ()
{
  int first_reg, last_parm_reg;

  /* Find lowest numbered live register.  */
  for (first_reg = 13; first_reg <= 31; first_reg++)
    if (regs_ever_live[first_reg])
      break;

  /* Figure out last used parameter register.  The proper thing to do is
     to walk incoming args of the function.  A function might have live
     parameter registers even if it has no incoming args.  */
  for (last_parm_reg = 10;
       last_parm_reg > 2 && ! regs_ever_live [last_parm_reg];
       last_parm_reg--)
    ;

  /* If profiling, then we must save/restore every register that contains
     a parameter before/after the .mcount call.  Use registers from
     RS6000_LAST_REG-(no. of parm regs used+1) to RS6000_LAST_REG to do this.
     Don't use the frame pointer in reg 31.  */
#ifndef RS6000_LAST_REG
#define RS6000_LAST_REG 30
#endif
  if ((DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_RHAPSODY) && profile_flag)
    if (first_reg > RS6000_LAST_REG - (last_parm_reg - 2) + 1)
      first_reg = RS6000_LAST_REG - (last_parm_reg - 2) + 1;

#ifdef MACHO_PIC
  if (flag_pic && current_function_uses_pic_offset_table && 
      (first_reg > PIC_OFFSET_TABLE_REGNUM))
    return PIC_OFFSET_TABLE_REGNUM;
#endif

  return first_reg;
}

/* Similar, for FP regs.  */

int
first_fp_reg_to_save ()
{
  int first_reg;

  /* Find lowest numbered live register.  */
  for (first_reg = 14 + 32; first_reg <= 63; first_reg++)
    if (regs_ever_live[first_reg])
      break;

  return first_reg;
}

/* Similar, for VECTOR regs.  */

int
first_vector_reg_to_save ()
{
  int first_reg;

  /* Find lowest numbered live register.  */
  for (first_reg = 20 + 78; first_reg <= 109; first_reg++)
    if (regs_ever_live[first_reg])
      break;

  return first_reg;
}

/* Return non-zero if this function makes calls.  */

int
rs6000_makes_calls ()
{
  rtx insn;

  /* If we are profiling, we will be making a call to __mcount.
     Under the System V ABI's, we store the LR directly, so
     we don't need to do it here.  */
  if ((DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_RHAPSODY) && profile_flag)
    return 1;

  for (insn = get_insns (); insn; insn = next_insn (insn))
    if (GET_CODE (insn) == CALL_INSN)
      return 1;

  return 0;
}


/* Calculate the stack information for the current function.  This is
   complicated by having two separate calling sequences, the AIX calling
   sequence and the V.4 calling sequence.

   AIX stack frames look like:
							  32-bit  64-bit
	SP---->	+---------------------------------------+
		| back chain to caller			| 0	  0
		+---------------------------------------+
		| saved CR				| 4       8 (8-11)
		+---------------------------------------+
		| saved LR				| 8       16
		+---------------------------------------+
		| reserved for compilers		| 12      24
		+---------------------------------------+
		| reserved for binders			| 16      32
		+---------------------------------------+
		| saved TOC pointer			| 20      40
		+---------------------------------------+
		| Parameter save area (P)		| 24      48
		+---------------------------------------+
		| Alloca space (A)			| 24+P    etc.
		+---------------------------------------+
		| Local variable space (L)		| 24+P+A
		+---------------------------------------+
		| Float/int conversion temporary (X)	| 24+P+A+L
		+---------------------------------------+
		| Save area for Vector registers (Z)	| 24+P+A+L+X
		+---------------------------------------+
		| alignment padding (Y)			| 24+P+A+L+X+Z
		+---------------------------------------+
		| saved VRsave (W)			| 24+P+A+L+X+Z+Y
		+---------------------------------------+
		| Save area for GP registers (G)	| 24+P+A+L+X+Z+Y+W
		+---------------------------------------+
		| Save area for FP registers (F)	| 24+P+A+L+X+Z+Y+W+G
		+---------------------------------------+
	old SP->| back chain to caller's caller		|
		+---------------------------------------+

   V.4 stack frames look like:

	SP---->	+---------------------------------------+
		| back chain to caller			| 0
		+---------------------------------------+
		| caller's saved LR			| 4
		+---------------------------------------+
		| Parameter save area (P)		| 8
		+---------------------------------------+
		| Alloca space (A)			| 8+P
		+---------------------------------------+    
		| Varargs save area (V)			| 8+P+A
		+---------------------------------------+    
		| Local variable space (L)		| 8+P+A+V
		+---------------------------------------+    
		| Float/int conversion temporary (X)	| 8+P+A+V+L
		+---------------------------------------+
		| Save area for Vector registers (Z)	| 8+P+A+V+L+X
		+---------------------------------------+
		| alignment padding (Y)			| 8+P+A+V+L+X+Z
		+---------------------------------------+
		| saved VRsave (W)			| 8+P+A+V+L+X+Z+Y
		+---------------------------------------+
		| saved CR (C)				| 8+P+A+V+L+X+Z+Y+W
		+---------------------------------------+    
		| Save area for GP registers (G)	| 8+P+A+V+L+X+Z+Y+W+C
		+---------------------------------------+    
		| Save area for FP registers (F)	| 8+P+A+V+L+X+Z+Y+W+C+G
		+---------------------------------------+
	old SP->| back chain to caller's caller		|
		+---------------------------------------+


   A PowerPC Windows/NT frame looks like:

	SP---->	+---------------------------------------+
		| back chain to caller			| 0
		+---------------------------------------+
		| reserved				| 4
		+---------------------------------------+
		| reserved				| 8
		+---------------------------------------+
		| reserved				| 12
		+---------------------------------------+
		| reserved				| 16
		+---------------------------------------+
		| reserved				| 20
		+---------------------------------------+
		| Parameter save area (P)		| 24
		+---------------------------------------+
		| Alloca space (A)			| 24+P
		+---------------------------------------+     
		| Local variable space (L)		| 24+P+A
		+---------------------------------------+     
		| Float/int conversion temporary (X)	| 24+P+A+L
		+---------------------------------------+
		| Save area for FP registers (F)	| 24+P+A+L+X
		+---------------------------------------+     
		| Possible alignment area (Y)		| 24+P+A+L+X+F
		+---------------------------------------+     
		| Save area for GP registers (G)	| 24+P+A+L+X+F+Y
		+---------------------------------------+     
		| Save area for CR (C)			| 24+P+A+L+X+F+Y+G
		+---------------------------------------+     
		| Save area for TOC (T)			| 24+P+A+L+X+F+Y+G+C
		+---------------------------------------+     
		| Save area for LR (R)			| 24+P+A+L+X+F+Y+G+C+T
		+---------------------------------------+
	old SP->| back chain to caller's caller		|
		+---------------------------------------+

   For NT, there is no specific order to save the registers, but in
   order to support __builtin_return_address, the save area for the
   link register needs to be in a known place, so we use -4 off of the
   old SP.  To support calls through pointers, we also allocate a
   fixed slot to store the TOC, -8 off the old SP.  */

#ifndef ABI_STACK_BOUNDARY
#define ABI_STACK_BOUNDARY STACK_BOUNDARY
#endif

rs6000_stack_t *
rs6000_stack_info ()
{
  static rs6000_stack_t info, zero_info;
  rs6000_stack_t *info_ptr = &info;
  int reg_size = TARGET_32BIT ? 4 : 8;
  enum rs6000_abi abi;
  int total_raw_size;

  /* Zero all fields portably */
  info = zero_info;

  /* Select which calling sequence */
  info_ptr->abi = abi = DEFAULT_ABI;

  /* Calculate which registers need to be saved & save area size */
  info_ptr->first_gp_reg_save = first_reg_to_save ();
  info_ptr->gp_size = reg_size * (32 - info_ptr->first_gp_reg_save);

  info_ptr->first_fp_reg_save = first_fp_reg_to_save ();
  info_ptr->fp_size = 8 * (64 - info_ptr->first_fp_reg_save);

  info_ptr->first_vector_reg_save = first_vector_reg_to_save ();
  info_ptr->vector_size = 16 * (110 - info_ptr->first_vector_reg_save);

  /* Does this function call anything? */
  info_ptr->calls_p = rs6000_makes_calls ();

  /* Allocate space to save the toc. */
  if (abi == ABI_NT && info_ptr->calls_p)
    {
      info_ptr->toc_save_p = 1;
      info_ptr->toc_size = reg_size;
    }

  /* Does this machine need the float/int conversion area? */
  info_ptr->fpmem_p = regs_ever_live[FPMEM_REGNUM];

  /* If this is main and we need to call a function to set things up,
     save main's arguments around the call.  */
#ifdef TARGET_EABI
  if (TARGET_EABI)
#endif
    {
      if (strcmp (IDENTIFIER_POINTER (DECL_NAME (current_function_decl)), "main") == 0
	  && DECL_CONTEXT (current_function_decl) == NULL_TREE)
	{
	  info_ptr->main_p = 1;

#ifdef NAME__MAIN
	  info_ptr->calls_p = 1;

	  if (DECL_ARGUMENTS (current_function_decl))
	    {
	      int i;
	      tree arg;

	      info_ptr->main_save_p = 1;
	      info_ptr->main_size = 0;

	      for ((i = 0), (arg = DECL_ARGUMENTS (current_function_decl));
		   arg != NULL_TREE && i < 8;
		   (arg = TREE_CHAIN (arg)), i++)
		{
		  info_ptr->main_size += reg_size;
		}
	    }
#endif
	}
    }


  /* Determine if we need to save the link register */
  if (regs_ever_live[65]
      || (DEFAULT_ABI == ABI_AIX && profile_flag)
#ifdef TARGET_RELOCATABLE
      || (TARGET_RELOCATABLE && (get_pool_size () != 0))
#endif
      || (info_ptr->first_fp_reg_save != 64
	  && !FP_SAVE_INLINE (info_ptr->first_fp_reg_save))
      || (info_ptr->first_vector_reg_save != 110
	  && !VECTOR_SAVE_INLINE (info_ptr->first_vector_reg_save))
      || (abi == ABI_V4 && current_function_calls_alloca)
      || (abi == ABI_SOLARIS && current_function_calls_alloca)
#ifdef MACHO_PIC
      || current_function_uses_pic_offset_table
#endif
      || info_ptr->calls_p)
    {
      info_ptr->lr_save_p = 1;
      regs_ever_live[65] = 1;
      if (abi == ABI_NT)
	info_ptr->lr_size = reg_size;
    }

  /* Determine if we need to save the condition code registers */
  if (regs_ever_live[70] || regs_ever_live[71] || regs_ever_live[72])
    {
      info_ptr->cr_save_p = 1;
      if (abi == ABI_V4 || abi == ABI_NT || abi == ABI_SOLARIS)
	info_ptr->cr_size = reg_size;
    }

  /* Ensure that fp_save_offset will be aligned to an 8-byte boundary. */
  if (info_ptr->fpmem_p)
    {
      info_ptr->gp_size = RS6000_ALIGN (info_ptr->gp_size, 8);
      info_ptr->main_size = RS6000_ALIGN (info_ptr->main_size, 8);
    }

  /* Determine various sizes */
  info_ptr->reg_size     = reg_size;
  info_ptr->fixed_size   = RS6000_SAVE_AREA;
  info_ptr->varargs_size = RS6000_VARARGS_AREA;
  /* TODO: Not considering 16-byte alignment here. */
  info_ptr->vars_size    = RS6000_ALIGN (get_frame_size (), 8);
  info_ptr->parm_size    = RS6000_ALIGN (current_function_outgoing_args_size, 8);
  info_ptr->fpmem_size	 = (info_ptr->fpmem_p) ? 8 : 0;
  info_ptr->save_size    = RS6000_ALIGN (info_ptr->fp_size
				  + info_ptr->gp_size
				  + info_ptr->vector_size
				  + info_ptr->vrsave_size
				  + info_ptr->align_size
				  + info_ptr->cr_size
				  + info_ptr->lr_size
				  + info_ptr->toc_size
				  + info_ptr->main_size, 8);

  total_raw_size	 = (info_ptr->vars_size
			    + info_ptr->parm_size
			    + info_ptr->fpmem_size
			    + info_ptr->save_size
			    + info_ptr->varargs_size
			    + info_ptr->fixed_size);

  info_ptr->total_size   = RS6000_ALIGN (total_raw_size, ABI_STACK_BOUNDARY / BITS_PER_UNIT);

  /* Determine if we need to allocate any stack frame:

     For AIX we need to push the stack if a frame pointer is needed (because
     the stack might be dynamically adjusted), if we are debugging, if we
     make calls, or if the sum of fp_save, gp_save, fpmem, and local variables
     are more than the space needed to save all non-volatile registers:
     32-bit: 18*8 + 19*4 = 220 or 64-bit: 18*8 + 19*8 = 296

     For V.4 we don't have the stack cushion that AIX uses, but assume that
     the debugger can handle stackless frames.  */

  if (info_ptr->calls_p)
    info_ptr->push_p = 1;

  else if (abi == ABI_V4 || abi == ABI_NT || abi == ABI_SOLARIS)
    info_ptr->push_p = (total_raw_size > info_ptr->fixed_size
			|| (abi == ABI_NT ? info_ptr->lr_save_p
			    : info_ptr->calls_p));

  else
    info_ptr->push_p = (frame_pointer_needed
			|| (abi != ABI_RHAPSODY && write_symbols != NO_DEBUG)
			|| ((total_raw_size - info_ptr->fixed_size)
			    > (TARGET_32BIT ? 220 : 296)));

  /* Calculate the offsets */
  switch (abi)
    {
    case ABI_NONE:
    default:
      abort ();

    case ABI_AIX:
    case ABI_AIX_NODESC:
    case ABI_RHAPSODY:
      info_ptr->fp_save_offset   = - info_ptr->fp_size;
      info_ptr->gp_save_offset   = info_ptr->fp_save_offset - info_ptr->gp_size;
      info_ptr->vrsave_save_offset = info_ptr->gp_save_offset - info_ptr->vrsave_size;
      info_ptr->align_size = (info_ptr->vector_size ? (16 - ((-info_ptr->vrsave_save_offset) % 16)) % 16 : 0);
      info_ptr->vector_save_offset = info_ptr->vrsave_save_offset - info_ptr->align_size - info_ptr->vector_size;
      if (- info_ptr->vector_save_offset > 220)
	info_ptr->vector_outside_red_zone_p = 1;
      info_ptr->main_save_offset = info_ptr->vector_save_offset - info_ptr->main_size;
      info_ptr->cr_save_offset   = reg_size; /* first word when 64-bit.  */
      info_ptr->lr_save_offset   = 2*reg_size;
      break;

    case ABI_V4:
    case ABI_SOLARIS:
      info_ptr->fp_save_offset   = - info_ptr->fp_size;
      info_ptr->gp_save_offset   = info_ptr->fp_save_offset - info_ptr->gp_size;
      info_ptr->cr_save_offset   = info_ptr->gp_save_offset - info_ptr->cr_size;
      info_ptr->toc_save_offset  = info_ptr->cr_save_offset - info_ptr->toc_size;
      info_ptr->vrsave_save_offset = info_ptr->toc_save_offset - info_ptr->vrsave_size;
      info_ptr->align_size = (info_ptr->vector_size ? (16 - ((-info_ptr->vrsave_save_offset) % 16)) % 16 : 0);
      info_ptr->vector_save_offset = info_ptr->vrsave_save_offset - info_ptr->align_size - info_ptr->vector_size;
      info_ptr->main_save_offset = info_ptr->vector_save_offset - info_ptr->main_size;
      info_ptr->lr_save_offset   = reg_size;
      break;

    case ABI_NT:
      info_ptr->lr_save_offset    = -reg_size;
      info_ptr->toc_save_offset   = info_ptr->lr_save_offset - info_ptr->lr_size;
      info_ptr->cr_save_offset    = info_ptr->toc_save_offset - info_ptr->toc_size;
      info_ptr->gp_save_offset    = info_ptr->cr_save_offset - info_ptr->cr_size - info_ptr->gp_size + reg_size;
      info_ptr->fp_save_offset    = info_ptr->gp_save_offset - info_ptr->fp_size;
      if (info_ptr->fp_size && ((- info_ptr->fp_save_offset) % 8) != 0)
	info_ptr->fp_save_offset -= reg_size;

      info_ptr->main_save_offset = info_ptr->fp_save_offset - info_ptr->main_size;
      break;
    }


  if (info_ptr->fpmem_p)
    {
      info_ptr->fpmem_offset = info_ptr->main_save_offset - info_ptr->fpmem_size;
      rs6000_fpmem_size   = info_ptr->fpmem_size;
      rs6000_fpmem_offset = (info_ptr->push_p
			     ? info_ptr->total_size + info_ptr->fpmem_offset
			     : info_ptr->fpmem_offset);
    }
  else
    info_ptr->fpmem_offset = 0;  

  /* Zero offsets if we're not saving those registers */
  if (!info_ptr->fp_size)
    info_ptr->fp_save_offset = 0;

  if (!info_ptr->gp_size)
    info_ptr->gp_save_offset = 0;

  if (!info_ptr->vector_size)
    info_ptr->vector_save_offset = 0;

  if (!info_ptr->lr_save_p)
    info_ptr->lr_save_offset = 0;

  if (!info_ptr->cr_save_p)
    info_ptr->cr_save_offset = 0;

  if (!info_ptr->vrsave_save_p)
    info_ptr->vrsave_save_offset = 0;

  if (!info_ptr->toc_save_p)
    info_ptr->toc_save_offset = 0;

  if (!info_ptr->main_save_p)
    info_ptr->main_save_offset = 0;

  return info_ptr;
}

void
debug_stack_info (info)
     rs6000_stack_t *info;
{
  char *abi_string;

  if (!info)
    info = rs6000_stack_info ();

  fprintf (stderr, "\nStack information for function %s:\n",
	   ((current_function_decl && DECL_NAME (current_function_decl))
	    ? IDENTIFIER_POINTER (DECL_NAME (current_function_decl))
	    : "<unknown>"));

  switch (info->abi)
    {
    default:		 abi_string = "Unknown";	break;
    case ABI_NONE:	 abi_string = "NONE";		break;
    case ABI_AIX:	 abi_string = "AIX";		break;
    case ABI_AIX_NODESC: abi_string = "AIX";		break;
    case ABI_V4:	 abi_string = "V.4";		break;
    case ABI_RHAPSODY:	 abi_string = "Rhapsody";	break;
    case ABI_SOLARIS:	 abi_string = "Solaris";	break;
    case ABI_NT:	 abi_string = "NT";		break;
    }

  fprintf (stderr, "\tABI                 = %5s\n", abi_string);

  if (info->first_gp_reg_save != 32)
    fprintf (stderr, "\tfirst_gp_reg_save   = %5d\n", info->first_gp_reg_save);

  if (info->first_fp_reg_save != 64)
    fprintf (stderr, "\tfirst_fp_reg_save   = %5d\n", info->first_fp_reg_save);

  if (info->first_vector_reg_save != 110)
    fprintf (stderr, "\tfirst_vector_reg_save = %3d\n", info->first_vector_reg_save);

  if (info->lr_save_p)
    fprintf (stderr, "\tlr_save_p           = %5d\n", info->lr_save_p);

  if (info->cr_save_p)
    fprintf (stderr, "\tcr_save_p           = %5d\n", info->cr_save_p);

  if (info->toc_save_p)
    fprintf (stderr, "\ttoc_save_p          = %5d\n", info->toc_save_p);

  if (info->vrsave_save_p)
    fprintf (stderr, "\tvrsave_save_p       = %5d\n", info->vrsave_save_p);

  if (info->vector_outside_red_zone_p)
    fprintf (stderr, "\tvector_outside_red_zone_p = %d\n", info->vector_outside_red_zone_p);

  if (info->push_p)
    fprintf (stderr, "\tpush_p              = %5d\n", info->push_p);

  if (info->calls_p)
    fprintf (stderr, "\tcalls_p             = %5d\n", info->calls_p);

  if (info->main_p)
    fprintf (stderr, "\tmain_p              = %5d\n", info->main_p);

  if (info->main_save_p)
    fprintf (stderr, "\tmain_save_p         = %5d\n", info->main_save_p);

  if (info->fpmem_p)
    fprintf (stderr, "\tfpmem_p             = %5d\n", info->fpmem_p);

  if (info->gp_save_offset)
    fprintf (stderr, "\tgp_save_offset      = %5d\n", info->gp_save_offset);

  if (info->fp_save_offset)
    fprintf (stderr, "\tfp_save_offset      = %5d\n", info->fp_save_offset);

  if (info->vector_save_offset)
    fprintf (stderr, "\tvector_save_offset  = %5d\n", info->vector_save_offset);

  if (info->lr_save_offset)
    fprintf (stderr, "\tlr_save_offset      = %5d\n", info->lr_save_offset);

  if (info->cr_save_offset)
    fprintf (stderr, "\tcr_save_offset      = %5d\n", info->cr_save_offset);

  if (info->toc_save_offset)
    fprintf (stderr, "\ttoc_save_offset     = %5d\n", info->toc_save_offset);

  if (info->vrsave_save_offset)
    fprintf (stderr, "\tvrsave_save_offset  = %5d\n", info->vrsave_save_offset);

  if (info->varargs_save_offset)
    fprintf (stderr, "\tvarargs_save_offset = %5d\n", info->varargs_save_offset);

  if (info->main_save_offset)
    fprintf (stderr, "\tmain_save_offset    = %5d\n", info->main_save_offset);

  if (info->fpmem_offset)
    fprintf (stderr, "\tfpmem_offset        = %5d\n", info->fpmem_offset);

  if (info->total_size)
    fprintf (stderr, "\ttotal_size          = %5d\n", info->total_size);

  if (info->varargs_size)
    fprintf (stderr, "\tvarargs_size        = %5d\n", info->varargs_size);

  if (info->vars_size)
    fprintf (stderr, "\tvars_size           = %5d\n", info->vars_size);

  if (info->parm_size)
    fprintf (stderr, "\tparm_size           = %5d\n", info->parm_size);

  if (info->fpmem_size)
    fprintf (stderr, "\tfpmem_size          = %5d\n", info->fpmem_size);

  if (info->fixed_size)
    fprintf (stderr, "\tfixed_size          = %5d\n", info->fixed_size);

  if (info->gp_size)
    fprintf (stderr, "\tgp_size             = %5d\n", info->gp_size);

  if (info->fp_size)
    fprintf (stderr, "\tfp_size             = %5d\n", info->fp_size);

  if (info->vector_size)
    fprintf (stderr, "\tvector_size         = %5d\n", info->vector_size);

 if (info->lr_size)
    fprintf (stderr, "\tlr_size             = %5d\n", info->cr_size);

  if (info->cr_size)
    fprintf (stderr, "\tcr_size             = %5d\n", info->cr_size);

 if (info->toc_size)
    fprintf (stderr, "\ttoc_size            = %5d\n", info->toc_size);

  if (info->vrsave_size)
    fprintf (stderr, "\tvrsave_size         = %5d\n", info->vrsave_size);

 if (info->main_size)
    fprintf (stderr, "\tmain_size           = %5d\n", info->main_size);

  if (info->save_size)
    fprintf (stderr, "\tsave_size           = %5d\n", info->save_size);

  if (info->reg_size != 4)
    fprintf (stderr, "\treg_size            = %5d\n", info->reg_size);

  if (info->align_size != 0)
    fprintf (stderr, "\talign_size          = %5d\n", info->align_size);

  fprintf (stderr, "\n");
}

/* Write out an instruction to load the TOC_TABLE address into register 30.
   This is only needed when TARGET_TOC, TARGET_MINIMAL_TOC, and there is
   a constant pool.  */

void
rs6000_output_load_toc_table (file, reg)
     FILE *file;
     int reg;
{
  char buf[256];

#ifdef USING_SVR4_H
  if (TARGET_RELOCATABLE)
    {
      ASM_GENERATE_INTERNAL_LABEL (buf, "LCF", rs6000_pic_labelno);
      fprintf (file, "\tbl ");
      assemble_name (file, buf);
      fprintf (file, "\n");

      /* possibly create the toc section */
      if (!toc_initialized)
	{
	  toc_section ();
	  function_section (current_function_decl);
	}

      /* If not first call in this function, we need to put the
	 different between .LCTOC1 and the address we get to right
	 after the bl.  It will mess up disassembling the instructions
	 but that can't be helped.  We will later need to bias the
	 address before loading.  */
      if (rs6000_pic_func_labelno != rs6000_pic_labelno)
	{
	  char *init_ptr = TARGET_32BIT ? ".long" : ".quad";
	  char *buf_ptr;

	  ASM_OUTPUT_INTERNAL_LABEL (file, "LCL", rs6000_pic_labelno);

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCTOC", 1);
	  STRIP_NAME_ENCODING (buf_ptr, buf);
	  fprintf (file, "\t%s %s-", init_ptr, buf_ptr);

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCF", rs6000_pic_labelno);
	  fprintf (file, "%s\n", buf_ptr);
	}

      ASM_OUTPUT_INTERNAL_LABEL (file, "LCF", rs6000_pic_labelno);
      fprintf (file, "\tmflr %s\n", reg_names[reg]);

      if (rs6000_pic_func_labelno != rs6000_pic_labelno)
	  asm_fprintf(file, "\t{cal|la} %s,%d(%s)\n", reg_names[reg],
		      (TARGET_32BIT ? 4 : 8), reg_names[reg]);

      asm_fprintf (file, (TARGET_32BIT) ? "\t{l|lwz} %s,(" : "\tld %s,(",
		   reg_names[0]);
      ASM_GENERATE_INTERNAL_LABEL (buf, "LCL", rs6000_pic_labelno);
      assemble_name (file, buf);
      fputs ("-", file);
      ASM_GENERATE_INTERNAL_LABEL (buf, "LCF", rs6000_pic_labelno);
      assemble_name (file, buf);
      fprintf (file, ")(%s)\n", reg_names[reg]);
      asm_fprintf (file, "\t{cax|add} %s,%s,%s\n",
		   reg_names[reg], reg_names[0], reg_names[reg]);
      rs6000_pic_labelno++;
    }
  else if (!TARGET_64BIT)
    {
      ASM_GENERATE_INTERNAL_LABEL (buf, "LCTOC", 1);
      asm_fprintf (file, "\t{liu|lis} %s,", reg_names[reg]);
      assemble_name (file, buf);
      fputs ("@ha\n", file);
      asm_fprintf (file, "\t{cal|la} %s,", reg_names[reg]);
      assemble_name (file, buf);
      asm_fprintf (file, "@l(%s)\n", reg_names[reg]);
    }
  else
    abort ();

#else	/* !USING_SVR4_H */
  ASM_GENERATE_INTERNAL_LABEL (buf, "LCTOC", 0);
  asm_fprintf (file, TARGET_32BIT ? "\t{l|lwz} %s," : "\tld %s,",
	       reg_names[reg]);
  assemble_name (file, buf);
  asm_fprintf (file, "(%s)\n", reg_names[2]);
#endif /* USING_SVR4_H */
}


/* Emit the correct code for allocating stack space.  If COPY_R12, make sure a copy
   of the old frame is left in r12.  */

void
rs6000_allocate_stack_space (file, size, copy_r12)
     FILE *file;
     int size;
     int copy_r12;
{
  int neg_size = -size;
  if (TARGET_UPDATE)
    {
      if (size < 32767)
	asm_fprintf (file,
		     (TARGET_32BIT) ? "\t{stu|stwu} %s,%d(%s)\n" : "\tstdu %s,%d(%s)\n",
		     reg_names[1], neg_size, reg_names[1]);
      else
	{
	  if (copy_r12)
	    fprintf (file, "\tmr %s,%s\n", reg_names[12], reg_names[1]);

	  asm_fprintf (file, "\t{liu|lis} %s,%d\n\t{oril|ori} %s,%s,%d\n",
		       reg_names[0], (neg_size >> 16) & 0xffff,
		       reg_names[0], reg_names[0], neg_size & 0xffff);
	  asm_fprintf (file,
		       (TARGET_32BIT) ? "\t{stux|stwux} %s,%s,%s\n" : "\tstdux %s,%s,%s\n",
		       reg_names[1], reg_names[1], reg_names[0]);
	}
    }
  else
    {
      fprintf (file, "\tmr %s,%s\n", reg_names[12], reg_names[1]);
      if (size < 32767)
	asm_fprintf (file, "\t{cal|la} %s,%d(%s)\n",
		 reg_names[1], neg_size, reg_names[1]);
      else
	{
	  asm_fprintf (file, "\t{liu|lis} %s,%d\n\t{oril|ori} %s,%s,%d\n",
		       reg_names[0], (neg_size >> 16) & 0xffff,
		       reg_names[0], reg_names[0], neg_size & 0xffff);
	  asm_fprintf (file, "\t{cax|add} %s,%s,%s\n", reg_names[1],
		       reg_names[0], reg_names[1]);
	}

      asm_fprintf (file,
		   (TARGET_32BIT) ? "\t{st|stw} %s,0(%s)\n" : "\tstd %s,0(%s)\n",
		   reg_names[12], reg_names[1]);
    }
}


#ifndef ORDINARY_REG_NO
/* The number of a register (other than zero) that can always be clobbered
   by a called function without saving it first.  */
#define ORDINARY_REG_NO 12
#endif

/* Write function prologue.  */
void
output_prolog (file, size)
     FILE *file;
     int size ATTRIBUTE_UNUSED;
{
  rs6000_stack_t *info = rs6000_stack_info ();
  int reg_size = info->reg_size;
  char *store_reg;
  char *load_reg;
  int sp_reg = 1;
  int sp_offset = 0;

#ifdef MACHO_PIC
  if (current_function_uses_pic_offset_table && flag_pic)
    {
      char *piclabel_name = machopic_function_base_name();
      
      /* Do this only if this function is a method.
	 Determine objc method from name - ugly!  */
      if (piclabel_name[0] == '*'
	  && piclabel_name[1] == '"' ? (piclabel_name[2] == 'L'
					&& (piclabel_name[3] == '+'
					    || piclabel_name[3] == '-'))
				     : (piclabel_name[1] == 'L'
					&& (piclabel_name[2] == '+'
					    || piclabel_name[2] == '-')))
	/* Output pic base label.  */
	ASM_OUTPUT_LABEL (file, piclabel_name);
    }
#endif

  if (TARGET_32BIT)
    {
      store_reg = "\t{st|stw} %s,%d(%s)\n";
      load_reg = "\t{l|lwz} %s,%d(%s)\n";
    }
  else
    {
      store_reg = "\tstd %s,%d(%s)\n";
      load_reg = "\tlld %s,%d(%s)\n";
    }

  if (TARGET_DEBUG_STACK)
    debug_stack_info (info);

#ifndef APPLE_RHAPSODY
  /* Write .extern for any function we will call to save and restore fp
     values.  */
  if (info->first_fp_reg_save < 64 && !FP_SAVE_INLINE (info->first_fp_reg_save))
    fprintf (file, "\t.extern %s%d%s\n\t.extern %s%d%s\n",
	     SAVE_FP_PREFIX, info->first_fp_reg_save - 32, SAVE_FP_SUFFIX,
	     RESTORE_FP_PREFIX, info->first_fp_reg_save - 32, RESTORE_FP_SUFFIX);

  /* Write .extern for any function we will call to save and restore vector
     values.  */
  if (info->first_vector_reg_save < 110 && !VECTOR_SAVE_INLINE (info->first_vector_reg_save))
    fprintf (file, "\t.extern %s%d%s\n\t.extern %s%d%s\n",
	     SAVE_VECTOR_PREFIX, info->first_vector_reg_save - 78, SAVE_VECTOR_SUFFIX,
	     RESTORE_VECTOR_PREFIX, info->first_vector_reg_save - 78, RESTORE_VECTOR_SUFFIX);

  /* Write .extern for truncation routines, if needed.  */
  if (rs6000_trunc_used && ! trunc_defined)
    {
      fprintf (file, "\t.extern .%s\n\t.extern .%s\n",
	       RS6000_ITRUNC, RS6000_UITRUNC);
      trunc_defined = 1;
    }

  /* Write .extern for AIX common mode routines, if needed.  */
  if (! TARGET_POWER && ! TARGET_POWERPC && ! common_mode_defined)
    {
      fputs ("\t.extern __mulh\n", file);
      fputs ("\t.extern __mull\n", file);
      fputs ("\t.extern __divss\n", file);
      fputs ("\t.extern __divus\n", file);
      fputs ("\t.extern __quoss\n", file);
      fputs ("\t.extern __quous\n", file);
      common_mode_defined = 1;
    }
#endif  /* APPLE_RHAPSODY */

  /* For V.4, update stack before we do any saving and set back pointer.  */
  if (info->push_p && (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS))
    {
      if (info->total_size < 32767)
	sp_offset = info->total_size;
      else
	sp_reg = 12;
      rs6000_allocate_stack_space (file, info->total_size, sp_reg == 12);
    }

  /* If we use the link register, get it into r0.  */
  if (info->lr_save_p)
    asm_fprintf (file, "\tmflr %s\n", reg_names[0]);

  /* If we need to save CR, put it into ORDINARY_REG_NO.  */
  if (info->cr_save_p && sp_reg != ORDINARY_REG_NO && 0)
    asm_fprintf (file, "\tmfcr %s\n", reg_names[ORDINARY_REG_NO]);

  /* Do any required saving of fpr's.  If only one or two to save, do it
     ourself.  Otherwise, call function.  Note that since they are statically
     linked, we do not need a nop following them.  */
  if (FP_SAVE_INLINE (info->first_fp_reg_save))
    {
      int regno = info->first_fp_reg_save;
      int loc   = info->fp_save_offset + sp_offset;

      for ( ; regno < 64; regno++, loc += 8)
	asm_fprintf (file, "\tstfd %s,%d(%s)\n", reg_names[regno], loc, reg_names[sp_reg]);
    }
  else if (info->first_fp_reg_save != 64)
    asm_fprintf (file, "\tbl %s%d%s\n", SAVE_FP_PREFIX,
		 info->first_fp_reg_save - 32, SAVE_FP_SUFFIX);

  /* Now save gpr's.  */
  if (! TARGET_MULTIPLE || info->first_gp_reg_save == 31 || TARGET_64BIT)
    {
      int regno    = info->first_gp_reg_save;
      int loc      = info->gp_save_offset + sp_offset;

      for ( ; regno < 32; regno++, loc += reg_size)
	asm_fprintf (file, store_reg, reg_names[regno], loc, reg_names[sp_reg]);
    }

  else if (info->first_gp_reg_save != 32)
    asm_fprintf (file, "\t{stm|stmw} %s,%d(%s)\n",
		 reg_names[info->first_gp_reg_save],
		 info->gp_save_offset + sp_offset,
		 reg_names[sp_reg]);

  /* Save main's arguments if we need to call a function */
#ifdef NAME__MAIN
  if (info->main_save_p)
    {
      int regno;
      int loc = info->main_save_offset + sp_offset;
      int size = info->main_size;

      for (regno = 3; size > 0; regno++, loc += reg_size, size -= reg_size)
	asm_fprintf (file, store_reg, reg_names[regno], loc, reg_names[sp_reg]);
    }
#endif

  /* Save lr if we used it.  */
  if (info->lr_save_p)
    asm_fprintf (file, store_reg, reg_names[0], info->lr_save_offset + sp_offset,
		 reg_names[sp_reg]);

  /* Save CR if we use any that must be preserved.  */
  if (info->cr_save_p)
    {
      if (sp_reg == ORDINARY_REG_NO || 1)
	{
	  /* If ORDINARY_REG_NO is used to hold original sp, copy cr now.  */
	  asm_fprintf (file, "\tmfcr %s\n", reg_names[0]);
	  asm_fprintf (file, store_reg, reg_names[0],
		       info->cr_save_offset + sp_offset,
		       reg_names[sp_reg]);
	}
      else
	asm_fprintf (file, store_reg, reg_names[ORDINARY_REG_NO],
		     info->cr_save_offset + sp_offset, reg_names[sp_reg]);
    }

  /* NT needs us to probe the stack frame every 4k pages for large frames, so
     do it here.  */
  if (DEFAULT_ABI == ABI_NT && info->total_size > 4096)
    {
      if (info->total_size < 32768)
	{
	  int probe_offset = 4096;
	  while (probe_offset < info->total_size)
	    {
	      asm_fprintf (file, "\t{l|lwz} %s,%d(%s)\n", reg_names[0], -probe_offset, reg_names[1]);
	      probe_offset += 4096;
	    }
	}
      else
	{
	  int probe_iterations = info->total_size / 4096;
	  static int probe_labelno = 0;
	  char buf[256];

	  if (probe_iterations < 32768)
	    asm_fprintf (file, "\tli %s,%d\n", reg_names[12], probe_iterations);
	  else
	    {
	      asm_fprintf (file, "\tlis %s,%d\n", reg_names[12], probe_iterations >> 16);
	      if (probe_iterations & 0xffff)
		asm_fprintf (file, "\tori %s,%s,%d\n", reg_names[12], reg_names[12],
			     probe_iterations & 0xffff);
	    }
	  asm_fprintf (file, "\tmtctr %s\n", reg_names[12]);
	  asm_fprintf (file, "\tmr %s,%s\n", reg_names[12], reg_names[1]);
	  ASM_OUTPUT_INTERNAL_LABEL (file, "LCprobe", probe_labelno);
	  asm_fprintf (file, "\t{lu|lwzu} %s,-4096(%s)\n", reg_names[0], reg_names[12]);
	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCprobe", probe_labelno++);
	  fputs ("\tbdnz ", file);
	  assemble_name (file, buf);
	  fputs ("\n", file);
	}
    }

  /* Update stack and set back pointer unless this is V.4, which was done previously */
  if (info->push_p && DEFAULT_ABI != ABI_V4 && DEFAULT_ABI != ABI_SOLARIS)
    {
      if (info->first_vector_reg_save != 110)
	{
	  if (info->total_size < 32767)
	    sp_offset = info->total_size;
	  else
	    sp_reg = 12;
	}
      rs6000_allocate_stack_space (file, info->total_size, sp_reg == 12);
    }

  /* Do any required saving of vector registers.  If only one or two to save, do it
     ourself.  Otherwise, call function.  Note that since they are statically
     linked, we do not need a nop following them.  */
  if (VECTOR_SAVE_INLINE (info->first_vector_reg_save))
    {
      int regno = info->first_vector_reg_save;
      int loc   = info->vector_save_offset + sp_offset;

      for ( ; regno < 110; regno++, loc += 16)
	{
	  if (TARGET_NEW_MNEMONICS)
	    asm_fprintf (file, "\tli %s,%d\n", reg_names[0], loc);
	  else
	    asm_fprintf (file, "\tcal %s,%d(%s)\n", reg_names[0], loc,
			 reg_names[0]);
	  asm_fprintf (file, "\tstvx %s,%s,%s\n", reg_names[regno],
		       reg_names[sp_reg], reg_names[0]);
	}
    }
  else if (info->first_vector_reg_save != 110)
    {
      int loc = info->vector_save_offset + sp_offset + info->vector_size;
      if (TARGET_NEW_MNEMONICS)
	asm_fprintf (file, "\taddi %s,%s,%d\n", reg_names[0],
		     reg_names[sp_reg], loc);
      else
	asm_fprintf (file, "\tcal %s,%d(%s)\n", reg_names[0], loc,
		     reg_names[sp_reg]);
      asm_fprintf (file, "\tbl %s%d%s\n", SAVE_VECTOR_PREFIX,
		   info->first_vector_reg_save - 78, SAVE_VECTOR_SUFFIX);
    }

  /* Set frame pointer, if needed.  */
  if (frame_pointer_needed)
    asm_fprintf (file, "\tmr %s,%s\n",
		 reg_names[FRAME_POINTER_REGNUM], reg_names[1]);

#ifdef NAME__MAIN
  /* If we need to call a function to set things up for main, do so now
     before dealing with the TOC.  */
  if (info->main_p)
    {
      char *prefix = "";

      switch (DEFAULT_ABI)
	{
	case ABI_AIX:	prefix = ".";	break;
	case ABI_NT:	prefix = "..";	break;
	}

      fprintf (file, "\tbl %s%s\n", prefix, NAME__MAIN);
#ifdef RS6000_CALL_GLUE2
      fprintf (file, "\t%s%s%s\n", RS6000_CALL_GLUE2, prefix, NAME_MAIN);
#else
#ifdef RS6000_CALL_GLUE
      if (DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_NT)
	fprintf (file, "\t%s\n", RS6000_CALL_GLUE);
#endif
#endif

      if (info->main_save_p)
	{
	  int regno;
	  int loc;
	  int size = info->main_size;

	  if (info->total_size < 32767)
	    {
	      loc = info->total_size + info->main_save_offset;
	      for (regno = 3; size > 0; regno++, size -= reg_size, loc += reg_size)
		asm_fprintf (file, load_reg, reg_names[regno], loc, reg_names[1]);
	    }
	  else
	    {
	      int neg_size = info->main_save_offset - info->total_size;
	      loc = 0;
	      asm_fprintf (file, "\t{liu|lis} %s,%d\n\t{oril|ori} %s,%s,%d\n",
			   reg_names[0], (neg_size >> 16) & 0xffff,
			   reg_names[0], reg_names[0], neg_size & 0xffff);

	      asm_fprintf (file, "\t{sf|subf} %s,%s,%s\n", reg_names[0], reg_names[0],
			   reg_names[1]);

	      for (regno = 3; size > 0; regno++, size -= reg_size, loc += reg_size)
		asm_fprintf (file, load_reg, reg_names[regno], loc, reg_names[0]);
	    }
	}
    }
#endif


  /* If TARGET_MINIMAL_TOC, and the constant pool is needed, then load the
     TOC_TABLE address into register 30.  */
  if (TARGET_TOC && TARGET_MINIMAL_TOC && get_pool_size () != 0)
    {
#ifdef USING_SVR4_H
      if (!profile_flag)
	rs6000_pic_func_labelno = rs6000_pic_labelno;
#endif
      rs6000_output_load_toc_table (file, 30);
    }

  if (DEFAULT_ABI == ABI_NT)
    {
      assemble_name (file, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));
      fputs (".b:\n", file);
    }

#ifdef MACHO_PIC
  if (DEFAULT_ABI == ABI_RHAPSODY
      && current_function_uses_pic_offset_table && flag_pic)
    {
      char *picbase = machopic_function_base_name ();
      /* Save link register if profiling is enabled.  */
      if (profile_flag)
	fprintf (file, "\tmflr r0\n");
      /* We optimize when calling a method.  */
      if (picbase[0] == '*' &&
	  picbase[1] == '"' ? (picbase[2] == 'L'
			       && (picbase[3] == '+' || picbase[3] == '-'))
			    : (picbase[1] == 'L'
			       && (picbase[2] == '+' || picbase[2] == '-')))
	/* The prolog already output the picbase label.
	   Set the picbase from r12.  */
	fprintf (file, "\tmr %s,%s\n",
		 reg_names[PIC_OFFSET_TABLE_REGNUM], reg_names[12]);
      else
	{
	  /* Load pic base register.  */
	  fprintf (file, "\tbcl 20,31,");
	  assemble_name (file, picbase);
	  fprintf (file, "\n");
	  ASM_OUTPUT_LABEL (file, picbase);
	  fprintf (file, "\tmflr %s\n", reg_names[PIC_OFFSET_TABLE_REGNUM]);
	}
    }
#endif
}

/* Write function epilogue.  */

void
output_epilog (file, size)
     FILE *file;
     int size ATTRIBUTE_UNUSED;
{
  rs6000_stack_t *info = rs6000_stack_info ();
  char *load_reg = (TARGET_32BIT) ? "\t{l|lwz} %s,%d(%s)\n" : "\tld %s,%d(%s)\n";
  rtx insn = get_last_insn ();
  int sp_reg = 1;
  int sp_offset = 0;
  enum { none, call, branch } vrsave = none;
  int i;

  /* If the last insn was a BARRIER, we don't have to write anything except
     the trace table.  */
  if (GET_CODE (insn) == NOTE)
    insn = prev_nonnote_insn (insn);
  if (insn == 0 ||  GET_CODE (insn) != BARRIER)
    {
      /* If we have a frame pointer, a call to alloca,  or a large stack
	 frame, restore the old stack pointer using the backchain.  Otherwise,
	 we know what size to update it with.  */
      if (frame_pointer_needed || current_function_calls_alloca
	  || info->total_size > 32767)
	{
	  /* Under V.4, don't reset the stack pointer until after we're done
	     loading the saved registers.  */
	  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS
	      || info->vector_outside_red_zone_p)
	    sp_reg = 11;

	  asm_fprintf (file, load_reg, reg_names[sp_reg], 0, reg_names[1]);
	}
      else if (info->push_p)
	{
	  if (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS
	      || info->vector_outside_red_zone_p)
	    sp_offset = info->total_size;
	  else
	    asm_fprintf (file, "\t{cal|la} %s,%d(%s)\n",
			 reg_names[1], info->total_size, reg_names[1]);
	}

      /* Determine if we will call or branch to a vector restore routine.  This
	 effects how and when the link register is restored.  */
      if (info->first_vector_reg_save != 110 && !VECTOR_SAVE_INLINE (info->first_vector_reg_save))
	vrsave = (sp_offset == 0
		  && sp_reg == 1
		  && !(info->first_fp_reg_save != 64
		       && !FP_SAVE_INLINE (info->first_fp_reg_save)) ? branch : call);

      /* Get the old lr if we saved it.  */
      if (info->lr_save_p && vrsave != call)
	asm_fprintf (file, load_reg, reg_names[0], info->lr_save_offset + sp_offset, reg_names[sp_reg]);

      /* Get the old cr if we saved it.  */
      if (info->cr_save_p)
	asm_fprintf (file, load_reg, reg_names[ORDINARY_REG_NO],
		     info->cr_save_offset + sp_offset, reg_names[sp_reg]);

      /* Set LR here to try to overlap restores below.  */
      if (info->lr_save_p && vrsave != call)
	asm_fprintf (file, "\tmtlr %s\n", reg_names[0]);

      /* Restore gpr's.  */
      if (! TARGET_MULTIPLE || info->first_gp_reg_save == 31 || TARGET_64BIT)
	{
	  int regno    = info->first_gp_reg_save;
	  int loc      = info->gp_save_offset + sp_offset;
	  int reg_size = (TARGET_32BIT) ? 4 : 8;

	  for ( ; regno < 32; regno++, loc += reg_size)
	    asm_fprintf (file, load_reg, reg_names[regno], loc, reg_names[sp_reg]);
	}

      else if (info->first_gp_reg_save != 32)
	asm_fprintf (file, "\t{lm|lmw} %s,%d(%s)\n",
		     reg_names[info->first_gp_reg_save],
		     info->gp_save_offset + sp_offset,
		     reg_names[sp_reg]);

      /* Restore fpr's if we can do it without calling a function.  */
      if (FP_SAVE_INLINE (info->first_fp_reg_save))
	{
	  int regno = info->first_fp_reg_save;
	  int loc   = info->fp_save_offset + sp_offset;

	  for ( ; regno < 64; regno++, loc += 8)
	    asm_fprintf (file, "\tlfd %s,%d(%s)\n", reg_names[regno], loc, reg_names[sp_reg]);
	}

      /* Restore vector registers if we can do it without calling a
	 function.  */
      if (VECTOR_SAVE_INLINE (info->first_vector_reg_save))
	{
	  int regno = info->first_vector_reg_save;
	  int loc   = info->vector_save_offset + sp_offset;

	  for ( ; regno < 110; regno++, loc += 16)
	    {
	      if (TARGET_NEW_MNEMONICS)
		asm_fprintf (file, "\tli %s,%d\n", reg_names[0], loc);
	      else
		asm_fprintf (file, "\tcal %s,%d(%s)\n", reg_names[0], loc,
			     reg_names[0]);
	      asm_fprintf (file, "\tlvx %s,%s,%s\n", reg_names[regno],
			   reg_names[sp_reg], reg_names[0]);
	    }
	}

      /* If we saved cr, restore it here.  Just those of cr2, cr3, and cr4
	 that were used.  */
      if (info->cr_save_p)
	asm_fprintf (file, "\tmtcrf %d,%s\n",
		     (regs_ever_live[70] != 0) * 0x20
		     + (regs_ever_live[71] != 0) * 0x10
		     + (regs_ever_live[72] != 0) * 0x8,
		     reg_names[ORDINARY_REG_NO]);

      /* If we have to restore more than two VECTOR registers, branch to the
	 restore function.  */
      if (vrsave != none)
	{
	  int loc = info->vector_save_offset + sp_offset + info->vector_size;
	  if (TARGET_NEW_MNEMONICS)
	    asm_fprintf (file, "\taddi %s,%s,%d\n", reg_names[0],
			 reg_names[sp_reg], loc);
	  else
	    asm_fprintf (file, "\tcal %s,%d(%s)\n", reg_names[0], loc,
			 reg_names[sp_reg]);
	  asm_fprintf (file, "\t%s %s%d%s\n",
		       vrsave == call ? "bl" : "b",
		       RESTORE_VECTOR_PREFIX,
		       info->first_vector_reg_save - 78, RESTORE_VECTOR_SUFFIX);

	  /* Get the old lr if we saved it.  */
	  if (info->lr_save_p && vrsave == call)
	    {
	      asm_fprintf (file, load_reg, reg_names[0], info->lr_save_offset + sp_offset, reg_names[sp_reg]);
	      asm_fprintf (file, "\tmtlr %s\n", reg_names[0]);
	    }
	}

      /* If this is V.4, unwind the stack pointer after all of the loads have been done */
      if (sp_offset)
	asm_fprintf (file, "\t{cal|la} %s,%d(%s)\n",
		     reg_names[1], sp_offset, reg_names[1]);
      else if (sp_reg != 1)
	asm_fprintf (file, "\tmr %s,%s\n", reg_names[1], reg_names[sp_reg]);

      /* If we have to restore more than two FP registers, branch to the
	 restore function.  It will return to our caller.  */
      if (info->first_fp_reg_save != 64 && !FP_SAVE_INLINE (info->first_fp_reg_save))
	asm_fprintf (file, "\tb %s%d%s\n", RESTORE_FP_PREFIX,
		     info->first_fp_reg_save - 32, RESTORE_FP_SUFFIX);
      else if (vrsave != branch)
	asm_fprintf (file, "\t{br|blr}\n");
    }

  /* Output a traceback table here.  See /usr/include/sys/debug.h for info
     on its format.

     We don't output a traceback table if -finhibit-size-directive was
     used.  The documentation for -finhibit-size-directive reads
     ``don't output a @code{.size} assembler directive, or anything
     else that would cause trouble if the function is split in the
     middle, and the two halves are placed at locations far apart in
     memory.''  The traceback table has this property, since it
     includes the offset from the start of the function to the
     traceback table itself.

     System V.4 Powerpc's (and the embedded ABI derived from it) use a
     different traceback table.  */
  if (DEFAULT_ABI == ABI_AIX && ! flag_inhibit_size_directive)
    {
      char *fname = XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0);
      int fixed_parms, float_parms, parm_info;
      int i;

      while (*fname == '.')	/* V.4 encodes . in the name */
	fname++;

      /* Need label immediately before tbtab, so we can compute its offset
	 from the function start.  */
      if (*fname == '*')
	++fname;
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");
      ASM_OUTPUT_LABEL (file, fname);

      /* The .tbtab pseudo-op can only be used for the first eight
	 expressions, since it can't handle the possibly variable
	 length fields that follow.  However, if you omit the optional
	 fields, the assembler outputs zeros for all optional fields
	 anyways, giving each variable length field is minimum length
	 (as defined in sys/debug.h).  Thus we can not use the .tbtab
	 pseudo-op at all.  */

      /* An all-zero word flags the start of the tbtab, for debuggers
	 that have to find it by searching forward from the entry
	 point or from the current pc.  */
      fputs ("\t.long 0\n", file);

      /* Tbtab format type.  Use format type 0.  */
      fputs ("\t.byte 0,", file);

      /* Language type.  Unfortunately, there doesn't seem to be any
	 official way to get this info, so we use language_string.  C
	 is 0.  C++ is 9.  No number defined for Obj-C, so use the
	 value for C for now.  */
      if (! strcmp (language_string, "GNU C")
	  || ! strcmp (language_string, "GNU Obj-C"))
	i = 0;
      else if (! strcmp (language_string, "GNU F77"))
	i = 1;
      else if (! strcmp (language_string, "GNU Ada"))
	i = 3;
      else if (! strcmp (language_string, "GNU Pascal"))
	i = 2;
      else if (! strcmp (language_string, "GNU C++"))
	i = 9;
      else
	abort ();
      fprintf (file, "%d,", i);

      /* 8 single bit fields: global linkage (not set for C extern linkage,
	 apparently a PL/I convention?), out-of-line epilogue/prologue, offset
	 from start of procedure stored in tbtab, internal function, function
	 has controlled storage, function has no toc, function uses fp,
	 function logs/aborts fp operations.  */
      /* Assume that fp operations are used if any fp reg must be saved.  */
      fprintf (file, "%d,", (1 << 5) | ((info->first_fp_reg_save != 64) << 1));

      /* 6 bitfields: function is interrupt handler, name present in
	 proc table, function calls alloca, on condition directives
	 (controls stack walks, 3 bits), saves condition reg, saves
	 link reg.  */
      /* The `function calls alloca' bit seems to be set whenever reg 31 is
	 set up as a frame pointer, even when there is no alloca call.  */
      fprintf (file, "%d,",
	       ((1 << 6) | (frame_pointer_needed << 5)
		| (info->cr_save_p << 1) | (info->lr_save_p)));

      /* 3 bitfields: saves backchain, spare bit, number of fpr saved
	 (6 bits).  */
      fprintf (file, "%d,",
	       (info->push_p << 7) | (64 - info->first_fp_reg_save));

      /* 2 bitfields: spare bits (2 bits), number of gpr saved (6 bits).  */
      fprintf (file, "%d,", (32 - first_reg_to_save ()));

      {
	/* Compute the parameter info from the function decl argument
	   list.  */
	tree decl;
	int next_parm_info_bit;

	next_parm_info_bit = 31;
	parm_info = 0;
	fixed_parms = 0;
	float_parms = 0;

	for (decl = DECL_ARGUMENTS (current_function_decl);
	     decl; decl = TREE_CHAIN (decl))
	  {
	    rtx parameter = DECL_INCOMING_RTL (decl);
	    enum machine_mode mode = GET_MODE (parameter);

	    if (GET_CODE (parameter) == REG)
	      {
		if (GET_MODE_CLASS (mode) == MODE_FLOAT)
		  {
		    int bits;

		    float_parms++;

		    if (mode == SFmode)
		      bits = 0x2;
		    else if (mode == DFmode)
		      bits = 0x3;
		    else
		      abort ();

		    /* If only one bit will fit, don't or in this entry.  */
		    if (next_parm_info_bit > 0)
		      parm_info |= (bits << (next_parm_info_bit - 1));
		    next_parm_info_bit -= 2;
		  }
		else
		  {
		    fixed_parms += ((GET_MODE_SIZE (mode)
				     + (UNITS_PER_WORD - 1))
				    / UNITS_PER_WORD);
		    next_parm_info_bit -= 1;
		  }
	      }
	  }
      }

      /* Number of fixed point parameters.  */
      /* This is actually the number of words of fixed point parameters; thus
	 an 8 byte struct counts as 2; and thus the maximum value is 8.  */
      fprintf (file, "%d,", fixed_parms);

      /* 2 bitfields: number of floating point parameters (7 bits), parameters
	 all on stack.  */
      /* This is actually the number of fp registers that hold parameters;
	 and thus the maximum value is 13.  */
      /* Set parameters on stack bit if parameters are not in their original
	 registers, regardless of whether they are on the stack?  Xlc
	 seems to set the bit when not optimizing.  */
      fprintf (file, "%d\n", ((float_parms << 1) | (! optimize)));

      /* Optional fields follow.  Some are variable length.  */

      /* Parameter types, left adjusted bit fields: 0 fixed, 10 single float,
	 11 double float.  */
      /* There is an entry for each parameter in a register, in the order that
	 they occur in the parameter list.  Any intervening arguments on the
	 stack are ignored.  If the list overflows a long (max possible length
	 34 bits) then completely leave off all elements that don't fit.  */
      /* Only emit this long if there was at least one parameter.  */
      if (fixed_parms || float_parms)
	fprintf (file, "\t.long %d\n", parm_info);

      /* Offset from start of code to tb table.  */
      fputs ("\t.long ", file);
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");
      RS6000_OUTPUT_BASENAME (file, fname);
      fputs ("-.", file);
      RS6000_OUTPUT_BASENAME (file, fname);
      putc ('\n', file);

      /* Interrupt handler mask.  */
      /* Omit this long, since we never set the interrupt handler bit
	 above.  */

      /* Number of CTL (controlled storage) anchors.  */
      /* Omit this long, since the has_ctl bit is never set above.  */

      /* Displacement into stack of each CTL anchor.  */
      /* Omit this list of longs, because there are no CTL anchors.  */

      /* Length of function name.  */
      fprintf (file, "\t.short %d\n", (int) strlen (fname));

      /* Function name.  */
      assemble_string (fname, strlen (fname));

      /* Register for alloca automatic storage; this is always reg 31.
	 Only emit this if the alloca bit was set above.  */
      if (frame_pointer_needed)
	fputs ("\t.byte 31\n", file);
    }

  if (DEFAULT_ABI == ABI_NT)
    {
      RS6000_OUTPUT_BASENAME (file, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));
      fputs (".e:\nFE_MOT_RESVD..", file);
      RS6000_OUTPUT_BASENAME (file, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));
      fputs (":\n", file);
    }
}

/* Output a TOC entry.  We derive the entry name from what is
   being written.  */

void
output_toc (file, x, labelno)
     FILE *file;
     rtx x;
     int labelno;
{
  char buf[256];
  char *name = buf;
  char *real_name;
  rtx base = x;
  int offset = 0;

  if (TARGET_NO_TOC)
    abort ();

  /* if we're going to put a double constant in the TOC, make sure it's
     aligned properly when strict alignment is on. */
  if (GET_CODE (x) == CONST_DOUBLE
      && STRICT_ALIGNMENT
      && GET_MODE (x) == DFmode
      && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC)) {
    ASM_OUTPUT_ALIGN (file, 3);
  }


  if (TARGET_ELF && TARGET_MINIMAL_TOC)
    {
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LC");
      fprintf (file, "%d = .-", labelno);
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LCTOC");
      fputs ("1\n", file);
    }
  else
    ASM_OUTPUT_INTERNAL_LABEL (file, "LC", labelno);

  /* Handle FP constants specially.  Note that if we have a minimal
     TOC, things we put here aren't actually in the TOC, so we can allow
     FP constants.  */
  if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == DFmode
      && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC))
    {
      REAL_VALUE_TYPE rv;
      long k[2];

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, k);
      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fprintf (file, "\t.llong 0x%lx%08lx\n", k[0], k[1]);
	  else
	    fprintf (file, "\t.tc FD_%lx_%lx[TC],0x%lx%08lx\n",
		     k[0], k[1], k[0] & 0xffffffff, k[1] & 0xffffffff);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fprintf (file, "\t.long %ld\n\t.long %ld\n", k[0], k[1]);
	  else
	    fprintf (file, "\t.tc FD_%lx_%lx[TC],%ld,%ld\n",
		     k[0], k[1], k[0], k[1]);
	  return;
	}
    }
  else if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == SFmode
	   && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC))
    {
      REAL_VALUE_TYPE rv;
      long l;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);

      if (TARGET_MINIMAL_TOC)
	fprintf (file, TARGET_32BIT ? "\t.long %ld\n" : "\t.llong %ld\n", l);
      else
	fprintf (file, "\t.tc FS_%lx[TC],%ld\n", l, l);
      return;
    }
  else if (GET_MODE (x) == DImode
	   && (GET_CODE (x) == CONST_INT || GET_CODE (x) == CONST_DOUBLE)
	   && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC))
    {
      HOST_WIDE_INT low;
      HOST_WIDE_INT high;

      if (GET_CODE (x) == CONST_DOUBLE)
	{
	  low = CONST_DOUBLE_LOW (x);
	  high = CONST_DOUBLE_HIGH (x);
	}
      else
#if HOST_BITS_PER_WIDE_INT == 32
	{
	  low = INTVAL (x);
	  high = (low < 0) ? ~0 : 0;
	}
#else
	{
          low = INTVAL (x) & 0xffffffff;
          high = (HOST_WIDE_INT) INTVAL (x) >> 32;
	}
#endif

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fprintf (file, "\t.llong 0x%lx%08lx\n", (long)high, (long)low);
	  else
	    fprintf (file, "\t.tc ID_%lx_%lx[TC],0x%lx%08lx\n",
		     (long)high, (long)low, (long)high, (long)low);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fprintf (file, "\t.long %ld\n\t.long %ld\n",
		     (long)high, (long)low);
	  else
	    fprintf (file, "\t.tc ID_%lx_%lx[TC],%ld,%ld\n",
		     (long)high, (long)low, (long)high, (long)low);
	  return;
	}
    }

  if (GET_CODE (x) == CONST)
    {
      base = XEXP (XEXP (x, 0), 0);
      offset = INTVAL (XEXP (XEXP (x, 0), 1));
    }
  
  if (GET_CODE (base) == SYMBOL_REF)
    name = XSTR (base, 0);
  else if (GET_CODE (base) == LABEL_REF)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (XEXP (base, 0)));
  else if (GET_CODE (base) == CODE_LABEL)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (base));
  else
    abort ();

  STRIP_NAME_ENCODING (real_name, name);
  if (TARGET_MINIMAL_TOC)
    fputs (TARGET_32BIT ? "\t.long " : "\t.llong ", file);
  else
    {
      fprintf (file, "\t.tc %s", real_name);

      if (offset < 0)
	fprintf (file, ".N%d", - offset);
      else if (offset)
	fprintf (file, ".P%d", offset);

      fputs ("[TC],", file);
    }

  /* Currently C++ toc references to vtables can be emitted before it
     is decided whether the vtable is public or private.  If this is
     the case, then the linker will eventually complain that there is
     a TOC reference to an unknown section.  Thus, for vtables only,
     we emit the TOC reference to reference the symbol and not the
     section.  */
  if (!strncmp ("_vt.", name, 4))
    {
      RS6000_OUTPUT_BASENAME (file, name);
      if (offset < 0)
	fprintf (file, "%d", offset);
      else if (offset > 0)
	fprintf (file, "+%d", offset);
    }
  else
    output_addr_const (file, x);
  putc ('\n', file);
}

/* Output an assembler pseudo-op to write an ASCII string of N characters
   starting at P to FILE.

   On the RS/6000, we have to do this using the .byte operation and
   write out special characters outside the quoted string.
   Also, the assembler is broken; very long strings are truncated,
   so we must artificially break them up early. */

void
output_ascii (file, p, n)
     FILE *file;
     char *p;
     int n;
{
  char c;
  int i, count_string;
  char *for_string = "\t.byte \"";
  char *for_decimal = "\t.byte ";
  char *to_close = NULL;

  count_string = 0;
  for (i = 0; i < n; i++)
    {
      c = *p++;
      if (c >= ' ' && c < 0177)
	{
	  if (for_string)
	    fputs (for_string, file);
	  putc (c, file);

	  /* Write two quotes to get one.  */
	  if (c == '"')
	    {
	      putc (c, file);
	      ++count_string;
	    }

	  for_string = NULL;
	  for_decimal = "\"\n\t.byte ";
	  to_close = "\"\n";
	  ++count_string;

	  if (count_string >= 512)
	    {
	      fputs (to_close, file);

	      for_string = "\t.byte \"";
	      for_decimal = "\t.byte ";
	      to_close = NULL;
	      count_string = 0;
	    }
	}
      else
	{
	  if (for_decimal)
	    fputs (for_decimal, file);
	  fprintf (file, "%d", c);

	  for_string = "\n\t.byte \"";
	  for_decimal = ", ";
	  to_close = "\n";
	  count_string = 0;
	}
    }

  /* Now close the string if we have written one.  Then end the line.  */
  if (to_close)
    fprintf (file, to_close);
}

/* Generate a unique section name for FILENAME for a section type
   represented by SECTION_DESC.  Output goes into BUF.

   SECTION_DESC can be any string, as long as it is different for each
   possible section type.

   We name the section in the same manner as xlc.  The name begins with an
   underscore followed by the filename (after stripping any leading directory
   names) with the last period replaced by the string SECTION_DESC.  If
   FILENAME does not contain a period, SECTION_DESC is appended to the end of
   the name.  */

void
rs6000_gen_section_name (buf, filename, section_desc)
     char **buf;
     char *filename;
     char *section_desc;
{
  char *q, *after_last_slash, *last_period;
  char *p;
  int len;

  after_last_slash = filename;
  for (q = filename; *q; q++)
    {
      if (*q == '/')
	after_last_slash = q + 1;
      else if (*q == '.')
	last_period = q;
    }

  len = strlen (after_last_slash) + strlen (section_desc) + 2;
  *buf = (char *) permalloc (len);

  p = *buf;
  *p++ = '_';

  for (q = after_last_slash; *q; q++)
    {
      if (q == last_period)
        {
	  strcpy (p, section_desc);
	  p += strlen (section_desc);
        }

      else if (ISALNUM (*q))
        *p++ = *q;
    }

  if (last_period == 0)
    strcpy (p, section_desc);
  else
    *p = '\0';
}

#ifndef APPLE_RHAPSODY
/* Write function profiler code. */

void
output_function_profiler (file, labelno)
  FILE *file;
  int labelno;
{
  /* The last used parameter register.  */
  int last_parm_reg;
  int i, j;
  char buf[100];

  ASM_GENERATE_INTERNAL_LABEL (buf, "LP", labelno);
  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    case ABI_V4:
    case ABI_SOLARIS:
    case ABI_AIX_NODESC:
      fprintf (file, "\tmflr %s\n", reg_names[0]);
      if (flag_pic == 1)
	{
	  fputs ("\tbl _GLOBAL_OFFSET_TABLE_@local-4\n", file);
	  asm_fprintf (file, "\t{st|stw} %s,4(%s)\n",
		       reg_names[0], reg_names[1]);
	  asm_fprintf (file, "\tmflr %s\n", reg_names[11]);
	  asm_fprintf (file, "\t{l|lwz} %s,", reg_names[0]);
	  assemble_name (file, buf);
	  asm_fprintf (file, "@got(%s)\n", reg_names[11]);
	}
#if TARGET_ELF
      else if (flag_pic > 1 || TARGET_RELOCATABLE)
	{
	  asm_fprintf (file, "\t{st|stw} %s,4(%s)\n",
		       reg_names[0], reg_names[1]);
	  rs6000_pic_func_labelno = rs6000_pic_labelno;
	  rs6000_output_load_toc_table (file, 11);
	  asm_fprintf (file, "\t{l|lwz} %s,", reg_names[11]);
	  assemble_name (file, buf);
	  asm_fprintf (file, "X(%s)\n", reg_names[11]);
	  asm_fprintf (file, "%s\n", MINIMAL_TOC_SECTION_ASM_OP);
	  assemble_name (file, buf);
	  fputs ("X = .-.LCTOC1\n", file);
	  fputs ("\t.long ", file);
	  assemble_name (file, buf);
	  fputs ("\n\t.previous\n", file);
	}
#endif
      else
	{
	  asm_fprintf (file, "\t{liu|lis} %s,", reg_names[11]);
	  assemble_name (file, buf);
	  fputs ("@ha\n", file);
	  asm_fprintf (file, "\t{st|stw} %s,4(%s)\n", reg_names[0], reg_names[1]);
	  asm_fprintf (file, "\t{cal|la} %s,", reg_names[0]);
	  assemble_name (file, buf);
	  asm_fprintf (file, "@l(%s)\n", reg_names[11]);
	}

      fprintf (file, "\tbl %s\n", RS6000_MCOUNT);
      break;

    case ABI_AIX:
      /* Set up a TOC entry for the profiler label.  */
      toc_section ();
      ASM_OUTPUT_INTERNAL_LABEL (file, "LPC", labelno);
      if (TARGET_MINIMAL_TOC)
	{
	  fputs (TARGET_32BIT ? "\t.long " : "\t.llong ", file);
	  assemble_name (file, buf);
	  putc ('\n', file);
	}
      else
	{
	  fputs ("\t.tc\t", file);
	  assemble_name (file, buf);
	  fputs ("[TC],", file);
	  assemble_name (file, buf);
	  putc ('\n', file);
	}
      text_section ();

  /* Figure out last used parameter register.  The proper thing to do is
     to walk incoming args of the function.  A function might have live
     parameter registers even if it has no incoming args.  */

      for (last_parm_reg = 10;
	   last_parm_reg > 2 && ! regs_ever_live [last_parm_reg];
	   last_parm_reg--)
	;

  /* Save parameter registers in regs 23-30.  Don't overwrite reg 31, since
     it might be set up as the frame pointer.  */

      for (i = 3, j = 30; i <= last_parm_reg; i++, j--)
	asm_fprintf (file, "\tmr %d,%d\n", j, i);

  /* Load location address into r3, and call mcount.  */

      ASM_GENERATE_INTERNAL_LABEL (buf, "LPC", labelno);
      asm_fprintf (file, TARGET_32BIT ? "\t{l|lwz} %s," : "\tld %s,",
		   reg_names[3]);
      assemble_name (file, buf);
      asm_fprintf (file, "(%s)\n\tbl %s\n\t%s\n",
		   reg_names[2], RS6000_MCOUNT, RS6000_CALL_GLUE);

  /* Restore parameter registers.  */

      for (i = 3, j = 30; i <= last_parm_reg; i++, j--)
	asm_fprintf (file, "\tmr %d,%d\n", i, j);
      break;
    }
}
#endif /* ndef APPLE_RHAPSODY */

/* Adjust the cost of a scheduling dependency.  Return the new cost of
   a dependency LINK or INSN on DEP_INSN.  COST is the current cost.  */

int
rs6000_adjust_cost (insn, link, dep_insn, cost)
     rtx insn;
     rtx link;
     rtx dep_insn ATTRIBUTE_UNUSED;
     int cost;
{
  if (! recog_memoized (insn))
    return 0;

  if (REG_NOTE_KIND (link) != 0)
    return 0;

  if (REG_NOTE_KIND (link) == 0)
    {
      /* Data dependency; DEP_INSN writes a register that INSN reads some
	 cycles later.  */

    switch (get_attr_type (insn))
      {
      case TYPE_JMPREG:
      /* Tell the first scheduling pass about the latency between a mtctr
	 and bctr (and mtlr and br/blr).  The first scheduling pass will not
	 know about this latency since the mtctr instruction, which has the
	 latency associated to it, will be generated by reload.  */
	return TARGET_POWER ? 5 : 4;
      case TYPE_STORE:
      case TYPE_FPSTORE:
	if (SET_SRC (PATTERN (insn)) == SET_DEST (PATTERN (dep_insn)))
	  return cost - 3;
	return cost;
      }
      /* Fall out to return default cost.  */
    }

  return cost;
}

/* Return how many instructions the machine can issue per cycle */
int get_issue_rate()
{
  switch (rs6000_cpu_attr) {
  case CPU_RIOS1:
    return 3;       /* ? */
  case CPU_RIOS2:
    return 4;
  case CPU_PPC601:
    return 3;       /* ? */
  case CPU_PPC603:
    return 2; 
  case CPU_PPC604:
    return 4;
  case CPU_PPC620:
    return 4;
  default:
    return 1;
  }
}



/* Output assembler code for a block containing the constant parts
   of a trampoline, leaving space for the variable parts.

   The trampoline should set the static chain pointer to value placed
   into the trampoline and should branch to the specified routine.  */

void
rs6000_trampoline_template (file)
     FILE *file;
{
  char *sc = reg_names[STATIC_CHAIN_REGNUM];
  char *r0 = reg_names[0];
  char *r2 = reg_names[2];

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    /* Under AIX, this is not code at all, but merely a data area,
       since that is the way all functions are called.  The first word is
       the address of the function, the second word is the TOC pointer (r2),
       and the third word is the static chain value.  */
    case ABI_AIX:
      break;


    /* V.4/eabi function pointers are just a single pointer, so we need to
       do the full gory code to load up the static chain.  */
    case ABI_V4:
    case ABI_SOLARIS:
    case ABI_AIX_NODESC:
      break;

  /* NT function pointers point to a two word area (real address, TOC)
     which unfortunately does not include a static chain field.  So we
     use the function field to point to ..LTRAMP1 and the toc field
     to point to the whole table.  */
    case ABI_NT:
      if (STATIC_CHAIN_REGNUM == 0
	  || STATIC_CHAIN_REGNUM == 2
	  || TARGET_64BIT
	  || !TARGET_NEW_MNEMONICS)
	abort ();

      fprintf (file, "\t.ualong 0\n");			/* offset  0 */
      fprintf (file, "\t.ualong 0\n");			/* offset  4 */
      fprintf (file, "\t.ualong 0\n");			/* offset  8 */
      fprintf (file, "\t.ualong 0\n");			/* offset 12 */
      fprintf (file, "\t.ualong 0\n");			/* offset 16 */
      fprintf (file, "..LTRAMP1..0:\n");		/* offset 20 */
      fprintf (file, "\tlwz %s,8(%s)\n", r0, r2);	/* offset 24 */
      fprintf (file, "\tlwz %s,12(%s)\n", sc, r2);	/* offset 28 */
      fprintf (file, "\tmtctr %s\n", r0);		/* offset 32 */
      fprintf (file, "\tlwz %s,16(%s)\n", r2, r2);	/* offset 36 */
      fprintf (file, "\tbctr\n");			/* offset 40 */
      break;
    }

  return;
}

/* Length in units of the trampoline for entering a nested function.  */

int
rs6000_trampoline_size ()
{
  int ret = 0;

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    case ABI_AIX:
      ret = (TARGET_32BIT) ? 12 : 24;
      break;

    case ABI_V4:
    case ABI_SOLARIS:
    case ABI_AIX_NODESC:
      ret = (TARGET_32BIT) ? 40 : 48;
      break;

    case ABI_NT:
      ret = 20;
      break;
    }

  return ret;
}

/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.  */

void
rs6000_initialize_trampoline (addr, fnaddr, cxt)
     rtx addr;
     rtx fnaddr;
     rtx cxt;
{
  enum machine_mode pmode = Pmode;
  int regsize = (TARGET_32BIT) ? 4 : 8;
  rtx ctx_reg = force_reg (pmode, cxt);

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

/* Macros to shorten the code expansions below.  */
#define MEM_DEREF(addr) gen_rtx_MEM (pmode, memory_address (pmode, addr))
#define MEM_PLUS(addr,offset) gen_rtx_MEM (pmode, memory_address (pmode, plus_constant (addr, offset)))

    /* Under AIX, just build the 3 word function descriptor */
    case ABI_AIX:
      {
	rtx fn_reg = gen_reg_rtx (pmode);
	rtx toc_reg = gen_reg_rtx (pmode);
	emit_move_insn (fn_reg, MEM_DEREF (fnaddr));
	emit_move_insn (toc_reg, MEM_PLUS (fnaddr, 4));
	emit_move_insn (MEM_DEREF (addr), fn_reg);
	emit_move_insn (MEM_PLUS (addr, regsize), toc_reg);
	emit_move_insn (MEM_PLUS (addr, 2*regsize), ctx_reg);
      }
      break;

    /* Under V.4/eabi, call __trampoline_setup to do the real work.  */
    case ABI_V4:
    case ABI_SOLARIS:
    case ABI_AIX_NODESC:
      emit_library_call (gen_rtx_SYMBOL_REF (SImode, "__trampoline_setup"),
			 FALSE, VOIDmode, 4,
			 addr, pmode,
			 GEN_INT (rs6000_trampoline_size ()), SImode,
			 fnaddr, pmode,
			 ctx_reg, pmode);
      break;

    /* Under NT, update the first word to point to the ..LTRAMP1..0 header,
       the second word will point to the whole trampoline, third-fifth words
       will then have the real address, static chain, and toc value.  */
    case ABI_NT:
      {
	rtx tramp_reg = gen_reg_rtx (pmode);
	rtx fn_reg = gen_reg_rtx (pmode);
	rtx toc_reg = gen_reg_rtx (pmode);

	emit_move_insn (tramp_reg, gen_rtx_SYMBOL_REF (pmode, "..LTRAMP1..0"));
	addr = force_reg (pmode, addr);
	emit_move_insn (fn_reg, MEM_DEREF (fnaddr));
	emit_move_insn (toc_reg, MEM_PLUS (fnaddr, regsize));
	emit_move_insn (MEM_DEREF (addr), tramp_reg);
	emit_move_insn (MEM_PLUS (addr, regsize), addr);
	emit_move_insn (MEM_PLUS (addr, 2*regsize), fn_reg);
	emit_move_insn (MEM_PLUS (addr, 3*regsize), ctx_reg);
	emit_move_insn (MEM_PLUS (addr, 4*regsize), gen_rtx_REG (pmode, 2));
      }
      break;
    }

  return;
}


/* If defined, a C expression whose value is nonzero if IDENTIFIER
   with arguments ARGS is a valid machine specific attribute for DECL.
   The attributes in ATTRIBUTES have previously been assigned to DECL.  */

int
rs6000_valid_decl_attribute_p (decl, attributes, identifier, args)
     tree decl ATTRIBUTE_UNUSED;
     tree attributes ATTRIBUTE_UNUSED;
     tree identifier ATTRIBUTE_UNUSED;
     tree args ATTRIBUTE_UNUSED;
{
  return 0;
}

/* If defined, a C expression whose value is nonzero if IDENTIFIER
   with arguments ARGS is a valid machine specific attribute for TYPE.
   The attributes in ATTRIBUTES have previously been assigned to TYPE.  */

int
rs6000_valid_type_attribute_p (type, attributes, identifier, args)
     tree type;
     tree attributes ATTRIBUTE_UNUSED;
     tree identifier;
     tree args;
{
  if (TREE_CODE (type) != FUNCTION_TYPE
      && TREE_CODE (type) != FIELD_DECL
      && TREE_CODE (type) != TYPE_DECL)
    return 0;

  /* Longcall attribute says that the function is not within 2**26 bytes
     of the current function, and to do an indirect call.  */
  if (is_attribute_p ("longcall", identifier))
    return (args == NULL_TREE);

  if (DEFAULT_ABI == ABI_NT)
    {
      /* Stdcall attribute says callee is responsible for popping arguments
	 if they are not variable.  */
      if (is_attribute_p ("stdcall", identifier))
	return (args == NULL_TREE);

      /* Cdecl attribute says the callee is a normal C declaration */
      if (is_attribute_p ("cdecl", identifier))
	return (args == NULL_TREE);

      /* Dllimport attribute says the caller is to call the function
	 indirectly through a __imp_<name> pointer.  */
      if (is_attribute_p ("dllimport", identifier))
	return (args == NULL_TREE);

      /* Dllexport attribute says the callee is to create a __imp_<name>
	 pointer.  */
      if (is_attribute_p ("dllexport", identifier))
	return (args == NULL_TREE);

      /* Exception attribute allows the user to specify 1-2 strings or identifiers
	 that will fill in the 3rd and 4th fields of the structured exception
	 table.  */
      if (is_attribute_p ("exception", identifier))
	{
	  int i;

	  if (args == NULL_TREE)
	    return 0;

	  for (i = 0; i < 2 && args != NULL_TREE; i++)
	    {
	      tree this_arg = TREE_VALUE (args);
	      args = TREE_PURPOSE (args);

	      if (TREE_CODE (this_arg) != STRING_CST
		  && TREE_CODE (this_arg) != IDENTIFIER_NODE)
		return 0;
	    }

	  return (args == NULL_TREE);
	}
    }

  return 0;
}

/* If defined, a C expression whose value is zero if the attributes on
   TYPE1 and TYPE2 are incompatible, one if they are compatible, and
   two if they are nearly compatible (which causes a warning to be
   generated).  */

int
rs6000_comp_type_attributes (type1, type2)
     tree type1 ATTRIBUTE_UNUSED;
     tree type2 ATTRIBUTE_UNUSED;
{
  return 1;
}

/* If defined, a C statement that assigns default attributes to newly
   defined TYPE.  */

void
rs6000_set_default_type_attributes (type)
     tree type ATTRIBUTE_UNUSED;
{
}

/* Return a dll import reference corresponding to a call's SYMBOL_REF */
struct rtx_def *
rs6000_dll_import_ref (call_ref)
     rtx call_ref;
{
  char *call_name;
  int len;
  char *p;
  rtx reg1, reg2;
  tree node;

  if (GET_CODE (call_ref) != SYMBOL_REF)
    abort ();

  call_name = XSTR (call_ref, 0);
  len = sizeof ("__imp_") + strlen (call_name);
  p = alloca (len);
  reg2 = gen_reg_rtx (Pmode);

  strcpy (p, "__imp_");
  strcat (p, call_name);
  node = get_identifier (p);

  reg1 = force_reg (Pmode, gen_rtx_SYMBOL_REF (VOIDmode, IDENTIFIER_POINTER (node)));
  emit_move_insn (reg2, gen_rtx_MEM (Pmode, reg1));

  return reg2;
}

/* Return a reference suitable for calling a function with the longcall attribute.  */
struct rtx_def *
rs6000_longcall_ref (call_ref)
     rtx call_ref;
{
  char *call_name;
  tree node;

  if (GET_CODE (call_ref) != SYMBOL_REF)
    return call_ref;

  /* System V adds '.' to the internal name, so skip them.  */
  call_name = XSTR (call_ref, 0);
  if (*call_name == '.')
    {
      while (*call_name == '.')
	call_name++;

      node = get_identifier (call_name);
      call_ref = gen_rtx_SYMBOL_REF (VOIDmode, IDENTIFIER_POINTER (node));
    }

  return force_reg (Pmode, call_ref);
}


/* A C statement or statements to switch to the appropriate section
   for output of RTX in mode MODE.  You can assume that RTX is some
   kind of constant in RTL.  The argument MODE is redundant except in
   the case of a `const_int' rtx.  Select the section by calling
   `text_section' or one of the alternatives for other sections.

   Do not define this macro if you put all constants in the read-only
   data section.  */

#ifdef USING_SVR4_H

void
rs6000_select_rtx_section (mode, x)
     enum machine_mode mode;
     rtx x;
{
  if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (x))
    toc_section ();
  else
    const_section ();
}

/* A C statement or statements to switch to the appropriate
   section for output of DECL.  DECL is either a `VAR_DECL' node
   or a constant of some sort.  RELOC indicates whether forming
   the initial value of DECL requires link-time relocations.  */

void
rs6000_select_section (decl, reloc)
     tree decl;
     int reloc;
{
  int size = int_size_in_bytes (TREE_TYPE (decl));

  if (TREE_CODE (decl) == STRING_CST)
    {
      if (! flag_writable_strings)
	const_section ();
      else
	data_section ();
    }
  else if (TREE_CODE (decl) == VAR_DECL)
    {
      if ((flag_pic && reloc)
	  || !TREE_READONLY (decl)
	  || TREE_SIDE_EFFECTS (decl)
	  || !DECL_INITIAL (decl)
	  || (DECL_INITIAL (decl) != error_mark_node
	      && !TREE_CONSTANT (DECL_INITIAL (decl))))
	{
	  if (rs6000_sdata != SDATA_NONE && (size > 0) && (size <= g_switch_value))
	    sdata_section ();
	  else
	    data_section ();
	}
      else
	{
	  if (rs6000_sdata != SDATA_NONE && (size > 0) && (size <= g_switch_value))
	    {
	      if (rs6000_sdata == SDATA_EABI)
		sdata2_section ();
	      else
		sdata_section ();	/* System V doesn't have .sdata2/.sbss2 */
	    }
	  else
	    const_section ();
	}
    }
  else
    const_section ();
}



/* If we are referencing a function that is static or is known to be
   in this file, make the SYMBOL_REF special.  We can use this to indicate
   that we can branch to this function without emitting a no-op after the
   call.  For real AIX and NT calling sequences, we also replace the
   function name with the real name (1 or 2 leading .'s), rather than
   the function descriptor name.  This saves a lot of overriding code
   to read the prefixes.  */

void
rs6000_encode_section_info (decl)
     tree decl;
{
  if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      rtx sym_ref = XEXP (DECL_RTL (decl), 0);
      if (TREE_ASM_WRITTEN (decl) || ! TREE_PUBLIC (decl))
	SYMBOL_REF_FLAG (sym_ref) = 1;

      if (DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_NT)
	{
	  char *prefix = (DEFAULT_ABI == ABI_AIX) ? "." : "..";
	  char *str = permalloc (strlen (prefix) + 1
				 + strlen (XSTR (sym_ref, 0)));
	  strcpy (str, prefix);
	  strcat (str, XSTR (sym_ref, 0));
	  XSTR (sym_ref, 0) = str;
	}
    }
  else if (rs6000_sdata != SDATA_NONE
	   && (DEFAULT_ABI == ABI_V4 || DEFAULT_ABI == ABI_SOLARIS)
	   && TREE_CODE (decl) == VAR_DECL)
    {
      int size = int_size_in_bytes (TREE_TYPE (decl));
      tree section_name = DECL_SECTION_NAME (decl);
      char *name = (char *)0;
      int len = 0;

      if (section_name)
	{
	  if (TREE_CODE (section_name) == STRING_CST)
	    {
	      name = TREE_STRING_POINTER (section_name);
	      len = TREE_STRING_LENGTH (section_name);
	    }
	  else
	    abort ();
	}

      if ((size > 0 && size <= g_switch_value)
	  || (name
	      && ((len == sizeof (".sdata")-1 && strcmp (name, ".sdata") == 0)
		  || (len == sizeof (".sdata2")-1 && strcmp (name, ".sdata2") == 0)
		  || (len == sizeof (".sbss")-1 && strcmp (name, ".sbss") == 0)
		  || (len == sizeof (".sbss2")-1 && strcmp (name, ".sbss2") == 0)
		  || (len == sizeof (".PPC.EMB.sdata0")-1 && strcmp (name, ".PPC.EMB.sdata0") == 0)
		  || (len == sizeof (".PPC.EMB.sbss0")-1 && strcmp (name, ".PPC.EMB.sbss0") == 0))))
	{
	  rtx sym_ref = XEXP (DECL_RTL (decl), 0);
	  char *str = permalloc (2 + strlen (XSTR (sym_ref, 0)));
	  strcpy (str, "@");
	  strcat (str, XSTR (sym_ref, 0));
	  XSTR (sym_ref, 0) = str;
	}
    }
}

#endif /* USING_SVR4_H */
