/* Evaluate expressions for GDB.
   Copyright 1986, 87, 89, 91, 92, 93, 94, 95, 96, 97, 1998
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
#include "gdb_string.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "value.h"
#include "expression.h"
#include "target.h"
#include "frame.h"
#include "demangle.h"
#include "language.h"		/* For CAST_IS_CONVERSION */
#include "f-lang.h"		/* for array bound stuff */

/* Defined in symtab.c */
extern int hp_som_som_object_present;

/* This is defined in valops.c */
extern int overload_resolution;


/* Prototypes for local functions. */

static value_ptr evaluate_subexp_for_sizeof PARAMS ((struct expression *,
						     int *));

static value_ptr evaluate_subexp_for_address PARAMS ((struct expression *,
						      int *, enum noside));

static value_ptr evaluate_subexp PARAMS ((struct type *, struct expression *,
					  int *, enum noside));

static char *get_label PARAMS ((struct expression *, int *));

static value_ptr
  evaluate_struct_tuple PARAMS ((value_ptr, struct expression *, int *,
				 enum noside, int));

static LONGEST
  init_array_element PARAMS ((value_ptr, value_ptr, struct expression *,
			      int *, enum noside, LONGEST, LONGEST));

#if defined (__GNUC__) && !__STDC__
inline
#endif
static value_ptr
evaluate_subexp (expect_type, exp, pos, noside)
     struct type *expect_type;
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  return (*exp->language_defn->evaluate_exp) (expect_type, exp, pos, noside);
}

/* Parse the string EXP as a C expression, evaluate it,
   and return the result as a number.  */

CORE_ADDR
parse_and_eval_address (exp)
     char *exp;
{
  struct expression *expr = parse_expression (exp);
  register CORE_ADDR addr;
  register struct cleanup *old_chain =
  make_cleanup ((make_cleanup_func) free_current_contents, &expr);

  addr = value_as_pointer (evaluate_expression (expr));
  do_cleanups (old_chain);
  return addr;
}

/* Like parse_and_eval_address but takes a pointer to a char * variable
   and advanced that variable across the characters parsed.  */

CORE_ADDR
parse_and_eval_address_1 (expptr)
     char **expptr;
{
  struct expression *expr = parse_exp_1 (expptr, (struct block *) 0, 0);
  register CORE_ADDR addr;
  register struct cleanup *old_chain =
  make_cleanup ((make_cleanup_func) free_current_contents, &expr);

  addr = value_as_pointer (evaluate_expression (expr));
  do_cleanups (old_chain);
  return addr;
}

value_ptr
parse_and_eval (exp)
     char *exp;
{
  struct expression *expr = parse_expression (exp);
  register value_ptr val;
  register struct cleanup *old_chain
  = make_cleanup ((make_cleanup_func) free_current_contents, &expr);

  val = evaluate_expression (expr);
  do_cleanups (old_chain);
  return val;
}

/* Parse up to a comma (or to a closeparen)
   in the string EXPP as an expression, evaluate it, and return the value.
   EXPP is advanced to point to the comma.  */

value_ptr
parse_to_comma_and_eval (expp)
     char **expp;
{
  struct expression *expr = parse_exp_1 (expp, (struct block *) 0, 1);
  register value_ptr val;
  register struct cleanup *old_chain
  = make_cleanup ((make_cleanup_func) free_current_contents, &expr);

  val = evaluate_expression (expr);
  do_cleanups (old_chain);
  return val;
}

/* Evaluate an expression in internal prefix form
   such as is constructed by parse.y.

   See expression.h for info on the format of an expression.  */

value_ptr
evaluate_expression (exp)
     struct expression *exp;
{
  int pc = 0;
  return evaluate_subexp (NULL_TYPE, exp, &pc, EVAL_NORMAL);
}

/* Evaluate an expression, avoiding all memory references
   and getting a value whose type alone is correct.  */

value_ptr
evaluate_type (exp)
     struct expression *exp;
{
  int pc = 0;
  return evaluate_subexp (NULL_TYPE, exp, &pc, EVAL_AVOID_SIDE_EFFECTS);
}

/* If the next expression is an OP_LABELED, skips past it,
   returning the label.  Otherwise, does nothing and returns NULL. */

static char *
get_label (exp, pos)
     register struct expression *exp;
     int *pos;
{
  if (exp->elts[*pos].opcode == OP_LABELED)
    {
      int pc = (*pos)++;
      char *name = &exp->elts[pc + 2].string;
      int tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      return name;
    }
  else
    return NULL;
}

/* This function evaluates tupes (in Chill) or brace-initializers
   (in C/C++) for structure types.  */

static value_ptr
evaluate_struct_tuple (struct_val, exp, pos, noside, nargs)
     value_ptr struct_val;
     register struct expression *exp;
     register int *pos;
     enum noside noside;
     int nargs;
{
  struct type *struct_type = check_typedef (VALUE_TYPE (struct_val));
  struct type *substruct_type = struct_type;
  struct type *field_type;
  int fieldno = -1;
  int variantno = -1;
  int subfieldno = -1;
  while (--nargs >= 0)
    {
      int pc = *pos;
      value_ptr val = NULL;
      int nlabels = 0;
      int bitpos, bitsize;
      char *addr;

      /* Skip past the labels, and count them. */
      while (get_label (exp, pos) != NULL)
	nlabels++;

      do
	{
	  char *label = get_label (exp, &pc);
	  if (label)
	    {
	      for (fieldno = 0; fieldno < TYPE_NFIELDS (struct_type);
		   fieldno++)
		{
		  char *field_name = TYPE_FIELD_NAME (struct_type, fieldno);
		  if (field_name != NULL && STREQ (field_name, label))
		    {
		      variantno = -1;
		      subfieldno = fieldno;
		      substruct_type = struct_type;
		      goto found;
		    }
		}
	      for (fieldno = 0; fieldno < TYPE_NFIELDS (struct_type);
		   fieldno++)
		{
		  char *field_name = TYPE_FIELD_NAME (struct_type, fieldno);
		  field_type = TYPE_FIELD_TYPE (struct_type, fieldno);
		  if ((field_name == 0 || *field_name == '\0')
		      && TYPE_CODE (field_type) == TYPE_CODE_UNION)
		    {
		      variantno = 0;
		      for (; variantno < TYPE_NFIELDS (field_type);
			   variantno++)
			{
			  substruct_type
			    = TYPE_FIELD_TYPE (field_type, variantno);
			  if (TYPE_CODE (substruct_type) == TYPE_CODE_STRUCT)
			    {
			      for (subfieldno = 0;
				 subfieldno < TYPE_NFIELDS (substruct_type);
				   subfieldno++)
				{
				  if (STREQ (TYPE_FIELD_NAME (substruct_type,
							      subfieldno),
					     label))
				    {
				      goto found;
				    }
				}
			    }
			}
		    }
		}
	      error ("there is no field named %s", label);
	    found:
	      ;
	    }
	  else
	    {
	      /* Unlabelled tuple element - go to next field. */
	      if (variantno >= 0)
		{
		  subfieldno++;
		  if (subfieldno >= TYPE_NFIELDS (substruct_type))
		    {
		      variantno = -1;
		      substruct_type = struct_type;
		    }
		}
	      if (variantno < 0)
		{
		  fieldno++;
		  subfieldno = fieldno;
		  if (fieldno >= TYPE_NFIELDS (struct_type))
		    error ("too many initializers");
		  field_type = TYPE_FIELD_TYPE (struct_type, fieldno);
		  if (TYPE_CODE (field_type) == TYPE_CODE_UNION
		      && TYPE_FIELD_NAME (struct_type, fieldno)[0] == '0')
		    error ("don't know which variant you want to set");
		}
	    }

	  /* Here, struct_type is the type of the inner struct,
	     while substruct_type is the type of the inner struct.
	     These are the same for normal structures, but a variant struct
	     contains anonymous union fields that contain substruct fields.
	     The value fieldno is the index of the top-level (normal or
	     anonymous union) field in struct_field, while the value
	     subfieldno is the index of the actual real (named inner) field
	     in substruct_type. */

	  field_type = TYPE_FIELD_TYPE (substruct_type, subfieldno);
	  if (val == 0)
	    val = evaluate_subexp (field_type, exp, pos, noside);

	  /* Now actually set the field in struct_val. */

	  /* Assign val to field fieldno. */
	  if (VALUE_TYPE (val) != field_type)
	    val = value_cast (field_type, val);

	  bitsize = TYPE_FIELD_BITSIZE (substruct_type, subfieldno);
	  bitpos = TYPE_FIELD_BITPOS (struct_type, fieldno);
	  if (variantno >= 0)
	    bitpos += TYPE_FIELD_BITPOS (substruct_type, subfieldno);
	  addr = VALUE_CONTENTS (struct_val) + bitpos / 8;
	  if (bitsize)
	    modify_field (addr, value_as_long (val),
			  bitpos % 8, bitsize);
	  else
	    memcpy (addr, VALUE_CONTENTS (val),
		    TYPE_LENGTH (VALUE_TYPE (val)));
	}
      while (--nlabels > 0);
    }
  return struct_val;
}

/* Recursive helper function for setting elements of array tuples for Chill.
   The target is ARRAY (which has bounds LOW_BOUND to HIGH_BOUND);
   the element value is ELEMENT;
   EXP, POS and NOSIDE are as usual.
   Evaluates index expresions and sets the specified element(s) of
   ARRAY to ELEMENT.
   Returns last index value.  */

static LONGEST
init_array_element (array, element, exp, pos, noside, low_bound, high_bound)
     value_ptr array, element;
     register struct expression *exp;
     register int *pos;
     enum noside noside;
     LONGEST low_bound, high_bound;
{
  LONGEST index;
  int element_size = TYPE_LENGTH (VALUE_TYPE (element));
  if (exp->elts[*pos].opcode == BINOP_COMMA)
    {
      (*pos)++;
      init_array_element (array, element, exp, pos, noside,
			  low_bound, high_bound);
      return init_array_element (array, element,
				 exp, pos, noside, low_bound, high_bound);
    }
  else if (exp->elts[*pos].opcode == BINOP_RANGE)
    {
      LONGEST low, high;
      (*pos)++;
      low = value_as_long (evaluate_subexp (NULL_TYPE, exp, pos, noside));
      high = value_as_long (evaluate_subexp (NULL_TYPE, exp, pos, noside));
      if (low < low_bound || high > high_bound)
	error ("tuple range index out of range");
      for (index = low; index <= high; index++)
	{
	  memcpy (VALUE_CONTENTS_RAW (array)
		  + (index - low_bound) * element_size,
		  VALUE_CONTENTS (element), element_size);
	}
    }
  else
    {
      index = value_as_long (evaluate_subexp (NULL_TYPE, exp, pos, noside));
      if (index < low_bound || index > high_bound)
	error ("tuple index out of range");
      memcpy (VALUE_CONTENTS_RAW (array) + (index - low_bound) * element_size,
	      VALUE_CONTENTS (element), element_size);
    }
  return index;
}

value_ptr
evaluate_subexp_standard (expect_type, exp, pos, noside)
     struct type *expect_type;
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  enum exp_opcode op;
  int tem, tem2, tem3;
  register int pc, pc2 = 0, oldpos;
  register value_ptr arg1 = NULL, arg2 = NULL, arg3;
  struct type *type;
  int nargs;
  value_ptr *argvec;
  int upper, lower, retcode;
  int code;
  int ix;
  long mem_offset;
  struct type **arg_types;
  int save_pos1;

  pc = (*pos)++;
  op = exp->elts[pc].opcode;

  switch (op)
    {
    case OP_SCOPE:
      tem = longest_to_int (exp->elts[pc + 2].longconst);
      (*pos) += 4 + BYTES_TO_EXP_ELEM (tem + 1);
      arg1 = value_struct_elt_for_reference (exp->elts[pc + 1].type,
					     0,
					     exp->elts[pc + 1].type,
					     &exp->elts[pc + 3].string,
					     NULL_TYPE);
      if (arg1 == NULL)
	error ("There is no field named %s", &exp->elts[pc + 3].string);
      return arg1;

    case OP_LONG:
      (*pos) += 3;
      return value_from_longest (exp->elts[pc + 1].type,
				 exp->elts[pc + 2].longconst);

    case OP_DOUBLE:
      (*pos) += 3;
      return value_from_double (exp->elts[pc + 1].type,
				exp->elts[pc + 2].doubleconst);

    case OP_VAR_VALUE:
      (*pos) += 3;
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  struct symbol *sym = exp->elts[pc + 2].symbol;
	  enum lval_type lv;

	  switch (SYMBOL_CLASS (sym))
	    {
	    case LOC_CONST:
	    case LOC_LABEL:
	    case LOC_CONST_BYTES:
	      lv = not_lval;
	      break;

	    case LOC_REGISTER:
	    case LOC_REGPARM:
	      lv = lval_register;
	      break;

	    default:
	      lv = lval_memory;
	      break;
	    }

	  return value_zero (SYMBOL_TYPE (sym), lv);
	}
      else
	return value_of_variable (exp->elts[pc + 2].symbol,
				  exp->elts[pc + 1].block);

    case OP_LAST:
      (*pos) += 2;
      return
	access_value_history (longest_to_int (exp->elts[pc + 1].longconst));

    case OP_REGISTER:
      {
	int regno = longest_to_int (exp->elts[pc + 1].longconst);
	value_ptr val = value_of_register (regno);

	(*pos) += 2;
	if (val == NULL)
	  error ("Value of register %s not available.", REGISTER_NAME (regno));
	else
	  return val;
      }
    case OP_BOOL:
      (*pos) += 2;
      return value_from_longest (LA_BOOL_TYPE,
				 exp->elts[pc + 1].longconst);

    case OP_INTERNALVAR:
      (*pos) += 2;
      return value_of_internalvar (exp->elts[pc + 1].internalvar);

    case OP_STRING:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      if (noside == EVAL_SKIP)
	goto nosideret;
      return value_string (&exp->elts[pc + 2].string, tem);

      /* FIXME: I think above call is likely wrong, and should be 
         value_string (..., tem + 1);  I can't easily test it, since 
         while Fortran and Chill still use OP_STRING, C and C++ do not. */

    case OP_NSSTRING:		/* Objective C Foundation Class NSString constant */
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      if (noside == EVAL_SKIP)
	{
	  goto nosideret;
	}
      return (value_ptr) value_nsstring (&exp->elts[pc + 2].string, tem + 1);

    case OP_BITSTRING:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos)
	+= 3 + BYTES_TO_EXP_ELEM ((tem + HOST_CHAR_BIT - 1) / HOST_CHAR_BIT);
      if (noside == EVAL_SKIP)
	goto nosideret;
      return value_bitstring (&exp->elts[pc + 2].string, tem);
      break;

    case OP_ARRAY:
      (*pos) += 3;
      tem2 = longest_to_int (exp->elts[pc + 1].longconst);
      tem3 = longest_to_int (exp->elts[pc + 2].longconst);
      nargs = tem3 - tem2 + 1;
      type = expect_type ? check_typedef (expect_type) : NULL_TYPE;

      if (expect_type != NULL_TYPE && noside != EVAL_SKIP
	  && TYPE_CODE (type) == TYPE_CODE_STRUCT)
	{
	  value_ptr rec = allocate_value (expect_type);
	  memset (VALUE_CONTENTS_RAW (rec), '\0', TYPE_LENGTH (type));
	  return evaluate_struct_tuple (rec, exp, pos, noside, nargs);
	}

      if (expect_type != NULL_TYPE && noside != EVAL_SKIP
	  && TYPE_CODE (type) == TYPE_CODE_ARRAY)
	{
	  struct type *range_type = TYPE_FIELD_TYPE (type, 0);
	  struct type *element_type = TYPE_TARGET_TYPE (type);
	  value_ptr array = allocate_value (expect_type);
	  int element_size = TYPE_LENGTH (check_typedef (element_type));
	  LONGEST low_bound, high_bound, index;
	  if (get_discrete_bounds (range_type, &low_bound, &high_bound) < 0)
	    {
	      low_bound = 0;
	      high_bound = (TYPE_LENGTH (type) / element_size) - 1;
	    }
	  index = low_bound;
	  memset (VALUE_CONTENTS_RAW (array), 0, TYPE_LENGTH (expect_type));
	  for (tem = nargs; --nargs >= 0;)
	    {
	      value_ptr element;
	      int index_pc = 0;
	      if (exp->elts[*pos].opcode == BINOP_RANGE)
		{
		  index_pc = ++(*pos);
		  evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
		}
	      element = evaluate_subexp (element_type, exp, pos, noside);
	      if (VALUE_TYPE (element) != element_type)
		element = value_cast (element_type, element);
	      if (index_pc)
		{
		  int continue_pc = *pos;
		  *pos = index_pc;
		  index = init_array_element (array, element, exp, pos, noside,
					      low_bound, high_bound);
		  *pos = continue_pc;
		}
	      else
		{
		  if (index > high_bound)
		    /* to avoid memory corruption */
		    error ("Too many array elements");
		  memcpy (VALUE_CONTENTS_RAW (array)
			  + (index - low_bound) * element_size,
			  VALUE_CONTENTS (element),
			  element_size);
		}
	      index++;
	    }
	  return array;
	}

      if (expect_type != NULL_TYPE && noside != EVAL_SKIP
	  && TYPE_CODE (type) == TYPE_CODE_SET)
	{
	  value_ptr set = allocate_value (expect_type);
	  char *valaddr = VALUE_CONTENTS_RAW (set);
	  struct type *element_type = TYPE_INDEX_TYPE (type);
	  struct type *check_type = element_type;
	  LONGEST low_bound, high_bound;

	  /* get targettype of elementtype */
	  while (TYPE_CODE (check_type) == TYPE_CODE_RANGE ||
		 TYPE_CODE (check_type) == TYPE_CODE_TYPEDEF)
	    check_type = TYPE_TARGET_TYPE (check_type);

	  if (get_discrete_bounds (element_type, &low_bound, &high_bound) < 0)
	    error ("(power)set type with unknown size");
	  memset (valaddr, '\0', TYPE_LENGTH (type));
	  for (tem = 0; tem < nargs; tem++)
	    {
	      LONGEST range_low, range_high;
	      struct type *range_low_type, *range_high_type;
	      value_ptr elem_val;
	      if (exp->elts[*pos].opcode == BINOP_RANGE)
		{
		  (*pos)++;
		  elem_val = evaluate_subexp (element_type, exp, pos, noside);
		  range_low_type = VALUE_TYPE (elem_val);
		  range_low = value_as_long (elem_val);
		  elem_val = evaluate_subexp (element_type, exp, pos, noside);
		  range_high_type = VALUE_TYPE (elem_val);
		  range_high = value_as_long (elem_val);
		}
	      else
		{
		  elem_val = evaluate_subexp (element_type, exp, pos, noside);
		  range_low_type = range_high_type = VALUE_TYPE (elem_val);
		  range_low = range_high = value_as_long (elem_val);
		}
	      /* check types of elements to avoid mixture of elements from
	         different types. Also check if type of element is "compatible"
	         with element type of powerset */
	      if (TYPE_CODE (range_low_type) == TYPE_CODE_RANGE)
		range_low_type = TYPE_TARGET_TYPE (range_low_type);
	      if (TYPE_CODE (range_high_type) == TYPE_CODE_RANGE)
		range_high_type = TYPE_TARGET_TYPE (range_high_type);
	      if ((TYPE_CODE (range_low_type) != TYPE_CODE (range_high_type)) ||
		  (TYPE_CODE (range_low_type) == TYPE_CODE_ENUM &&
		   (range_low_type != range_high_type)))
		/* different element modes */
		error ("POWERSET tuple elements of different mode");
	      if ((TYPE_CODE (check_type) != TYPE_CODE (range_low_type)) ||
		  (TYPE_CODE (check_type) == TYPE_CODE_ENUM &&
		   range_low_type != check_type))
		error ("incompatible POWERSET tuple elements");
	      if (range_low > range_high)
		{
		  warning ("empty POWERSET tuple range");
		  continue;
		}
	      if (range_low < low_bound || range_high > high_bound)
		error ("POWERSET tuple element out of range");
	      range_low -= low_bound;
	      range_high -= low_bound;
	      for (; range_low <= range_high; range_low++)
		{
		  int bit_index = (unsigned) range_low % TARGET_CHAR_BIT;
		  if (BITS_BIG_ENDIAN)
		    bit_index = TARGET_CHAR_BIT - 1 - bit_index;
		  valaddr[(unsigned) range_low / TARGET_CHAR_BIT]
		    |= 1 << bit_index;
		}
	    }
	  return set;
	}

      argvec = (value_ptr *) alloca (sizeof (value_ptr) * nargs);
      for (tem = 0; tem < nargs; tem++)
	{
	  /* Ensure that array expressions are coerced into pointer objects. */
	  argvec[tem] = evaluate_subexp_with_coercion (exp, pos, noside);
	}
      if (noside == EVAL_SKIP)
	goto nosideret;
      return value_array (tem2, tem3, argvec);

    case TERNOP_SLICE:
      {
	value_ptr array = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	int lowbound
	= value_as_long (evaluate_subexp (NULL_TYPE, exp, pos, noside));
	int upper
	= value_as_long (evaluate_subexp (NULL_TYPE, exp, pos, noside));
	if (noside == EVAL_SKIP)
	  goto nosideret;
	return value_slice (array, lowbound, upper - lowbound + 1);
      }

    case TERNOP_SLICE_COUNT:
      {
	value_ptr array = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	int lowbound
	= value_as_long (evaluate_subexp (NULL_TYPE, exp, pos, noside));
	int length
	= value_as_long (evaluate_subexp (NULL_TYPE, exp, pos, noside));
	return value_slice (array, lowbound, length);
      }

    case TERNOP_COND:
      /* Skip third and second args to evaluate the first one.  */
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (value_logical_not (arg1))
	{
	  evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
	  return evaluate_subexp (NULL_TYPE, exp, pos, noside);
	}
      else
	{
	  arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	  evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
	  return arg2;
	}

    case OP_SELECTOR:
      {				/* Objective C @selector operator */
	char *sel = &exp->elts[pc + 2].string;
	int len = longest_to_int (exp->elts[pc + 1].longconst);

	(*pos) += 3 + BYTES_TO_EXP_ELEM (len + 1);
	if (noside == EVAL_SKIP)
	  goto nosideret;

	if (sel[len] != 0)
	  sel[len] = 0;		/* make sure terminated */
	return value_from_longest (lookup_pointer_type (builtin_type_void),
				   lookup_child_selector (sel));
      }

    case OP_MSGCALL:
      {				/* Objective C message (method) call */
	/* User wants to call an objc method */
	value_ptr find_function_in_inferior PARAMS ((char *));
	value_ptr ret;
	struct symbol *sym;
	value_ptr gnu_runtime = 0;
	CORE_ADDR addr;

	/* First evaluate the target (class or object) of the message */
	(*pos) += 3;
	nargs = exp->elts[pc + 2].longconst;
	argvec = (value_ptr *) alloca (sizeof (value_ptr) * (nargs + 5));
	argvec[1] = evaluate_subexp (0, exp, pos, noside);	/* target */

	if (!value_as_long (argvec[1]))		/* null target */
	  return value_from_longest (builtin_type_long, 0);

	/* Now lookup the ObjC runtime message dispatcher */
	if (lookup_minimal_symbol ("objc_msgSend", 0, 0))
	  argvec[0] = find_function_in_inferior ("objc_msgSend");
	else
	  /* NeXT version not found, try GNU variant */
	  {
	    argvec[0] = find_function_in_inferior ("objc_msg_lookup");
	    gnu_runtime = argvec[0];
	  }

	/* Verify target responds to method selector. This logic needs
	 * work: not sure of GNU variant's name.  Must also account for
	 * new (NSObject) and old (Object) worlds
	 */

	tem = lookup_child_selector ("respondsToSelector:");
	if (tem == 0)
	  {
	    tem = lookup_child_selector ("respondsTo:");
	  }
	if (tem == 0)
	  {
	    error ("no 'respondsTo:' or 'respondsToSelector:' method");
	  }

	/* call "respondsToSelector:" method, to make sure that 
	 * the target class implements the user's desired method selector
	 */
	argvec[2] = value_from_longest (builtin_type_long, tem);
	argvec[3] = value_from_longest (builtin_type_long,	/* user's selector */
					exp->elts[pc + 1].longconst);
	argvec[4] = 0;		/* see if target responds to selector */
	ret = call_function_by_hand (argvec[0], 3, argvec + 1);
	if (gnu_runtime)
	  {
	    /* Whereas the NeXT objc runtime function objc_msgSend actually
	     * calls the method, the GNU runtime function objc_msg_lookup
	     * only returns a pointer to it.  We still have to call it.
	     */
	    argvec[0] = ret;	/* prepare to call the method */
	    ret = call_function_by_hand (argvec[0], 3, argvec + 1);
	    argvec[0] = gnu_runtime;	/* prepare to call objc_msg_lookup */
	  }

	if (!value_as_long (ret))	/* no, it doesn't! */
	  error ("Target does not respond to this message selector.");

	/* Try to lookup the method implementation; do we have a symbol? */
	/* FIXME: which one of these should I try first!?! */
	if (gnu_runtime)	/* look for methodFor: first */
	  {
	    if ((tem = lookup_child_selector ("methodFor:")) == 0)
	      tem = lookup_child_selector ("methodForSelector:");
	  }
	else
	  /* look for methodForSelector: first */
	  {
	    if ((tem = lookup_child_selector ("methodForSelector:")) == 0)
	      tem = lookup_child_selector ("methodFor:");
	  }
	if (!tem)
	  error ("no 'methodFor:' or 'methodForSelector:' method");

	/* call "methodForSelector:" method, to get the address of a 
	 * function method that implements this selector for this class.
	 * If we can find a symbol at that address, then we know the 
	 * return type, parameter types etc.  (that's a good thing).
	 */
	argvec[2] = value_from_longest (builtin_type_long, tem);
	ret = call_function_by_hand (argvec[0], 3, argvec + 1);
	if (gnu_runtime)
	  {
	    /* Whereas the NeXT objc runtime function objc_msgSend actually
	     * calls the method, the GNU runtime function objc_msg_lookup
	     * only returns a pointer to it.  We still have to call it.
	     */
	    argvec[0] = ret;	/* prepare to call the method */
	    ret = call_function_by_hand (argvec[0], 3, argvec + 1);
	    argvec[0] = gnu_runtime;	/* prepare to call objc_msg_lookup */
	  }

	if (addr = value_as_long (ret))		/* got an address */
	  {			/* is it a high_level symbol? */
#ifdef GDB_TARGET_IS_HPPA
	    CORE_ADDR tmp;
	    /* code and comment lifted from hppa-tdep.c -- unfortunately 
	       there is no builtin function to do this for me. */
	    /* If bit 30 (counting from the left) is on, then addr is the 
	       address of the PLT entry for this function, not the address 
	       of the function itself.  Bit 31 has meaning too, but only 
	       for MPE.  */
	    if (addr & 0x2)
	      addr = (CORE_ADDR) read_memory_unsigned_integer (addr & ~0x3, 4);
	    if (tmp = skip_trampoline_code (addr, 0))
	      addr = tmp;	/* in case of trampoline code */
#endif
	    sym = find_pc_function (addr);
	    /* 
	     * Found a function symbol.  Now we will substitute its
	     * value in place of the message dispatcher (obj_msgSend),
	     * so that we call the method directly instead of thru
	     * the dispatcher.  The main reason for doing this is that
	     * we can now evaluate the return value and parameter values
	     * according to their known data types, in case we need to
	     * do things like promotion, dereferencing, special handling
	     * of structs and doubles, etc.
	     */
	    if (sym)		/* yes; we will call it directly */
	      argvec[0] = value_of_variable (sym, 0);
	  }

	/* So now we actually call the selector.  Move it's value into [2] */
	argvec[2] = argvec[3];	/* selector */
	/* Evaluate the method's user-supplied arguments */
	for (tem = 0; tem < nargs; tem++)	/* args, if any */
	  argvec[tem + 3] = evaluate_subexp_with_coercion (exp, pos, noside);
	argvec[tem + 3] = 0;
	/* Now depending on whether we found a symbol for the method, 
	 * we will either call the runtime dispatcher or the method directly.
	 */
	if (noside == EVAL_SKIP)
	  goto nosideret;
	if (noside == EVAL_AVOID_SIDE_EFFECTS)
	  {
	    /* If the return type doesn't look like a function type, call an
	       error.  This can happen if somebody tries to turn a variable into
	       a function call. This is here because people often want to
	       call, eg, strcmp, which gdb doesn't know is a function.  If
	       gdb isn't asked for it's opinion (ie. through "whatis"),
	       it won't offer it. */

	    struct type *ftype = TYPE_TARGET_TYPE (VALUE_TYPE (argvec[0]));

	    if (ftype)
	      return allocate_value (TYPE_TARGET_TYPE (VALUE_TYPE (argvec[0])));
	    else
	      error ("Expression of type other than \"Method returning ...\" used as a method");
	  }
	ret = call_function_by_hand (argvec[0], nargs + 2, argvec + 1);
	if (gnu_runtime && argvec[0] == gnu_runtime)
	  {
	    /* Whereas the NeXT objc runtime function objc_msgSend actually
	     * calls the method, the GNU runtime function objc_msg_lookup
	     * only returns a pointer to it.  We still have to call it.
	     */
	    argvec[0] = ret;	/* prepare to call the method */
	    return call_function_by_hand (argvec[0], nargs + 2, argvec + 1);
	  }
	else
	  return ret;
      }
      break;

    case OP_FUNCALL:
      (*pos) += 2;
      op = exp->elts[*pos].opcode;
      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      /* Allocate arg vector, including space for the function to be
         called in argvec[0] and a terminating NULL */
      argvec = (value_ptr *) alloca (sizeof (value_ptr) * (nargs + 3));
      if (op == STRUCTOP_MEMBER || op == STRUCTOP_MPTR)
	{
	  LONGEST fnptr;

	  /* 1997-08-01 Currently we do not support function invocation
	     via pointers-to-methods with HP aCC. Pointer does not point
	     to the function, but possibly to some thunk. */
	  if (hp_som_som_object_present)
	    {
	      error ("Not implemented: function invocation through pointer to method with HP aCC");
	    }

	  nargs++;
	  /* First, evaluate the structure into arg2 */
	  pc2 = (*pos)++;

	  if (noside == EVAL_SKIP)
	    goto nosideret;

	  if (op == STRUCTOP_MEMBER)
	    {
	      arg2 = evaluate_subexp_for_address (exp, pos, noside);
	    }
	  else
	    {
	      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	    }

	  /* If the function is a virtual function, then the
	     aggregate value (providing the structure) plays
	     its part by providing the vtable.  Otherwise,
	     it is just along for the ride: call the function
	     directly.  */

	  arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);

	  fnptr = value_as_long (arg1);

	  if (METHOD_PTR_IS_VIRTUAL (fnptr))
	    {
	      int fnoffset = METHOD_PTR_TO_VOFFSET (fnptr);
	      struct type *basetype;
	      struct type *domain_type =
	      TYPE_DOMAIN_TYPE (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)));
	      int i, j;
	      basetype = TYPE_TARGET_TYPE (VALUE_TYPE (arg2));
	      if (domain_type != basetype)
		arg2 = value_cast (lookup_pointer_type (domain_type), arg2);
	      basetype = TYPE_VPTR_BASETYPE (domain_type);
	      for (i = TYPE_NFN_FIELDS (basetype) - 1; i >= 0; i--)
		{
		  struct fn_field *f = TYPE_FN_FIELDLIST1 (basetype, i);
		  /* If one is virtual, then all are virtual.  */
		  if (TYPE_FN_FIELD_VIRTUAL_P (f, 0))
		    for (j = TYPE_FN_FIELDLIST_LENGTH (basetype, i) - 1; j >= 0; --j)
		      if ((int) TYPE_FN_FIELD_VOFFSET (f, j) == fnoffset)
			{
			  value_ptr temp = value_ind (arg2);
			  arg1 = value_virtual_fn_field (&temp, f, j, domain_type, 0);
			  arg2 = value_addr (temp);
			  goto got_it;
			}
		}
	      if (i < 0)
		error ("virtual function at index %d not found", fnoffset);
	    }
	  else
	    {
	      VALUE_TYPE (arg1) = lookup_pointer_type (TYPE_TARGET_TYPE (VALUE_TYPE (arg1)));
	    }
	got_it:

	  /* Now, say which argument to start evaluating from */
	  tem = 2;
	}
      else if (op == STRUCTOP_STRUCT || op == STRUCTOP_PTR)
	{
	  /* Hair for method invocations */
	  int tem2;

	  nargs++;
	  /* First, evaluate the structure into arg2 */
	  pc2 = (*pos)++;
	  tem2 = longest_to_int (exp->elts[pc2 + 1].longconst);
	  *pos += 3 + BYTES_TO_EXP_ELEM (tem2 + 1);
	  if (noside == EVAL_SKIP)
	    goto nosideret;

	  if (op == STRUCTOP_STRUCT)
	    {
	      /* If v is a variable in a register, and the user types
	         v.method (), this will produce an error, because v has
	         no address.

	         A possible way around this would be to allocate a
	         copy of the variable on the stack, copy in the
	         contents, call the function, and copy out the
	         contents.  I.e. convert this from call by reference
	         to call by copy-return (or whatever it's called).
	         However, this does not work because it is not the
	         same: the method being called could stash a copy of
	         the address, and then future uses through that address
	         (after the method returns) would be expected to
	         use the variable itself, not some copy of it.  */
	      arg2 = evaluate_subexp_for_address (exp, pos, noside);
	    }
	  else
	    {
	      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	    }
	  /* Now, say which argument to start evaluating from */
	  tem = 2;
	}
      else
	{
	  /* Non-method function call */
	  save_pos1 = *pos;
	  argvec[0] = evaluate_subexp_with_coercion (exp, pos, noside);
	  tem = 1;
	  type = VALUE_TYPE (argvec[0]);
	  if (type && TYPE_CODE (type) == TYPE_CODE_PTR)
	    type = TYPE_TARGET_TYPE (type);
	  if (type && TYPE_CODE (type) == TYPE_CODE_FUNC)
	    {
	      for (; tem <= nargs && tem <= TYPE_NFIELDS (type); tem++)
		{
		  /* pai: FIXME This seems to be coercing arguments before
		   * overload resolution has been done! */
		  argvec[tem] = evaluate_subexp (TYPE_FIELD_TYPE (type, tem - 1),
						 exp, pos, noside);
		}
	    }
	}

      /* Evaluate arguments */
      for (; tem <= nargs; tem++)
	{
	  /* Ensure that array expressions are coerced into pointer objects. */
	  argvec[tem] = evaluate_subexp_with_coercion (exp, pos, noside);
	}

      /* signal end of arglist */
      argvec[tem] = 0;

      if (op == STRUCTOP_STRUCT || op == STRUCTOP_PTR)
	{
	  int static_memfuncp;
	  value_ptr temp = arg2;
	  char tstr[256];

	  /* Method invocation : stuff "this" as first parameter */
	  /* pai: this used to have lookup_pointer_type for some reason,
	   * but temp is already a pointer to the object */
	  argvec[1] = value_from_longest (VALUE_TYPE (temp),
				VALUE_ADDRESS (temp) + VALUE_OFFSET (temp));
	  /* Name of method from expression */
	  strcpy (tstr, &exp->elts[pc2 + 2].string);

	  if (overload_resolution && (exp->language_defn->la_language == language_cplus))
	    {
	      /* Language is C++, do some overload resolution before evaluation */
	      value_ptr valp = NULL;

	      /* Prepare list of argument types for overload resolution */
	      arg_types = (struct type **) xmalloc (nargs * (sizeof (struct type *)));
	      for (ix = 1; ix <= nargs; ix++)
		arg_types[ix - 1] = VALUE_TYPE (argvec[ix]);

	      (void) find_overload_match (arg_types, nargs, tstr,
				     1 /* method */ , 0 /* strict match */ ,
					  arg2 /* the object */ , NULL,
					  &valp, NULL, &static_memfuncp);


	      argvec[1] = arg2;	/* the ``this'' pointer */
	      argvec[0] = valp;	/* use the method found after overload resolution */
	    }
	  else
	    /* Non-C++ case -- or no overload resolution */
	    {
	      temp = arg2;
	      argvec[0] = value_struct_elt (&temp, argvec + 1, tstr,
					    &static_memfuncp,
					    op == STRUCTOP_STRUCT
				       ? "structure" : "structure pointer");
	      argvec[1] = arg2;	/* the ``this'' pointer */
	    }

	  if (static_memfuncp)
	    {
	      argvec[1] = argvec[0];
	      nargs--;
	      argvec++;
	    }
	}
      else if (op == STRUCTOP_MEMBER || op == STRUCTOP_MPTR)
	{
	  argvec[1] = arg2;
	  argvec[0] = arg1;
	}
      else if (op == OP_VAR_VALUE)
	{
	  /* Non-member function being called */
          /* fn: This can only be done for C++ functions.  A C-style function
             in a C++ program, for instance, does not have the fields that 
             are expected here */

	  if (overload_resolution && (exp->language_defn->la_language == language_cplus))
	    {
	      /* Language is C++, do some overload resolution before evaluation */
	      struct symbol *symp;

	      /* Prepare list of argument types for overload resolution */
	      arg_types = (struct type **) xmalloc (nargs * (sizeof (struct type *)));
	      for (ix = 1; ix <= nargs; ix++)
		arg_types[ix - 1] = VALUE_TYPE (argvec[ix]);

	      (void) find_overload_match (arg_types, nargs, NULL /* no need for name */ ,
				 0 /* not method */ , 0 /* strict match */ ,
		      NULL, exp->elts[save_pos1+2].symbol /* the function */ ,
					  NULL, &symp, NULL);

	      /* Now fix the expression being evaluated */
	      exp->elts[save_pos1+2].symbol = symp;
	      argvec[0] = evaluate_subexp_with_coercion (exp, &save_pos1, noside);
	    }
	  else
	    {
	      /* Not C++, or no overload resolution allowed */
	      /* nothing to be done; argvec already correctly set up */
	    }
	}
      else
	{
	  /* It is probably a C-style function */
	  /* nothing to be done; argvec already correctly set up */
	}

    do_call_it:

      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  /* If the return type doesn't look like a function type, call an
	     error.  This can happen if somebody tries to turn a variable into
	     a function call. This is here because people often want to
	     call, eg, strcmp, which gdb doesn't know is a function.  If
	     gdb isn't asked for it's opinion (ie. through "whatis"),
	     it won't offer it. */

	  struct type *ftype =
	  TYPE_TARGET_TYPE (VALUE_TYPE (argvec[0]));

	  if (ftype)
	    return allocate_value (TYPE_TARGET_TYPE (VALUE_TYPE (argvec[0])));
	  else
	    error ("Expression of type other than \"Function returning ...\" used as function");
	}
      if (argvec[0] == NULL)
	error ("Cannot evaluate function -- may be inlined");
      return call_function_by_hand (argvec[0], nargs, argvec + 1);
      /* pai: FIXME save value from call_function_by_hand, then adjust pc by adjust_fn_pc if +ve  */

    case OP_F77_UNDETERMINED_ARGLIST:

      /* Remember that in F77, functions, substring ops and 
         array subscript operations cannot be disambiguated 
         at parse time.  We have made all array subscript operations, 
         substring operations as well as function calls  come here 
         and we now have to discover what the heck this thing actually was.  
         If it is a function, we process just as if we got an OP_FUNCALL. */

      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 2;

      /* First determine the type code we are dealing with.  */
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      type = check_typedef (VALUE_TYPE (arg1));
      code = TYPE_CODE (type);

      switch (code)
	{
	case TYPE_CODE_ARRAY:
	  goto multi_f77_subscript;

	case TYPE_CODE_STRING:
	  goto op_f77_substr;

	case TYPE_CODE_PTR:
	case TYPE_CODE_FUNC:
	  /* It's a function call. */
	  /* Allocate arg vector, including space for the function to be
	     called in argvec[0] and a terminating NULL */
	  argvec = (value_ptr *) alloca (sizeof (value_ptr) * (nargs + 2));
	  argvec[0] = arg1;
	  tem = 1;
	  for (; tem <= nargs; tem++)
	    argvec[tem] = evaluate_subexp_with_coercion (exp, pos, noside);
	  argvec[tem] = 0;	/* signal end of arglist */
	  goto do_call_it;

	default:
	  error ("Cannot perform substring on this type");
	}

    op_f77_substr:
      /* We have a substring operation on our hands here, 
         let us get the string we will be dealing with */

      /* Now evaluate the 'from' and 'to' */

      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);

      if (nargs < 2)
	return value_subscript (arg1, arg2);

      arg3 = evaluate_subexp_with_coercion (exp, pos, noside);

      if (noside == EVAL_SKIP)
	goto nosideret;

      tem2 = value_as_long (arg2);
      tem3 = value_as_long (arg3);

      return value_slice (arg1, tem2, tem3 - tem2 + 1);

    case OP_COMPLEX:
      /* We have a complex number, There should be 2 floating 
         point numbers that compose it */
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);

      return value_literal_complex (arg1, arg2, builtin_type_f_complex_s16);

    case STRUCTOP_STRUCT:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	return value_zero (lookup_struct_elt_type (VALUE_TYPE (arg1),
						   &exp->elts[pc + 2].string,
						   0),
			   lval_memory);
      else
	{
	  value_ptr temp = arg1;
	  return value_struct_elt (&temp, NULL, &exp->elts[pc + 2].string,
				   NULL, "structure");
	}

    case STRUCTOP_PTR:
      tem = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 3 + BYTES_TO_EXP_ELEM (tem + 1);
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	return value_zero (lookup_struct_elt_type (VALUE_TYPE (arg1),
						   &exp->elts[pc + 2].string,
						   0),
			   lval_memory);
      else
	{
	  value_ptr temp = arg1;
	  return value_struct_elt (&temp, NULL, &exp->elts[pc + 2].string,
				   NULL, "structure pointer");
	}

    case STRUCTOP_MEMBER:
      arg1 = evaluate_subexp_for_address (exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);

      /* With HP aCC, pointers to methods do not point to the function code */
      if (hp_som_som_object_present &&
	  (TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_PTR) &&
      (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2))) == TYPE_CODE_METHOD))
	error ("Pointers to methods not supported with HP aCC");	/* 1997-08-19 */

      mem_offset = value_as_long (arg2);
      goto handle_pointer_to_member;

    case STRUCTOP_MPTR:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);

      /* With HP aCC, pointers to methods do not point to the function code */
      if (hp_som_som_object_present &&
	  (TYPE_CODE (VALUE_TYPE (arg2)) == TYPE_CODE_PTR) &&
      (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg2))) == TYPE_CODE_METHOD))
	error ("Pointers to methods not supported with HP aCC");	/* 1997-08-19 */

      mem_offset = value_as_long (arg2);

    handle_pointer_to_member:
      /* HP aCC generates offsets that have bit #29 set; turn it off to get
         a real offset to the member. */
      if (hp_som_som_object_present)
	{
	  if (!mem_offset)	/* no bias -> really null */
	    error ("Attempted dereference of null pointer-to-member");
	  mem_offset &= ~0x20000000;
	}
      if (noside == EVAL_SKIP)
	goto nosideret;
      type = check_typedef (VALUE_TYPE (arg2));
      if (TYPE_CODE (type) != TYPE_CODE_PTR)
	goto bad_pointer_to_member;
      type = check_typedef (TYPE_TARGET_TYPE (type));
      if (TYPE_CODE (type) == TYPE_CODE_METHOD)
	error ("not implemented: pointer-to-method in pointer-to-member construct");
      if (TYPE_CODE (type) != TYPE_CODE_MEMBER)
	goto bad_pointer_to_member;
      /* Now, convert these values to an address.  */
      arg1 = value_cast (lookup_pointer_type (TYPE_DOMAIN_TYPE (type)),
			 arg1);
      arg3 = value_from_longest (lookup_pointer_type (TYPE_TARGET_TYPE (type)),
				 value_as_long (arg1) + mem_offset);
      return value_ind (arg3);
    bad_pointer_to_member:
      error ("non-pointer-to-member value used in pointer-to-member construct");

    case BINOP_CONCAT:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, OP_NULL, noside);
      else
	return value_concat (arg1, arg2);

    case BINOP_ASSIGN:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);

      /* Do special stuff for HP aCC pointers to members */
      if (hp_som_som_object_present)
	{
	  /* 1997-08-19 Can't assign HP aCC pointers to methods. No details of
	     the implementation yet; but the pointer appears to point to a code
	     sequence (thunk) in memory -- in any case it is *not* the address
	     of the function as it would be in a naive implementation. */
	  if ((TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_PTR) &&
	      (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg1))) == TYPE_CODE_METHOD))
	    error ("Assignment to pointers to methods not implemented with HP aCC");

	  /* HP aCC pointers to data members require a constant bias */
	  if ((TYPE_CODE (VALUE_TYPE (arg1)) == TYPE_CODE_PTR) &&
	      (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg1))) == TYPE_CODE_MEMBER))
	    {
	      unsigned int *ptr = (unsigned int *) VALUE_CONTENTS (arg2);	/* forces evaluation */
	      *ptr |= 0x20000000;	/* set 29th bit */
	    }
	}

      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, OP_NULL, noside);
      else
	return value_assign (arg1, arg2);

    case BINOP_ASSIGN_MODIFY:
      (*pos) += 2;
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      op = exp->elts[pc + 1].opcode;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, BINOP_ASSIGN_MODIFY, op, noside);
      else if (op == BINOP_ADD)
	arg2 = value_add (arg1, arg2);
      else if (op == BINOP_SUB)
	arg2 = value_sub (arg1, arg2);
      else
	arg2 = value_binop (arg1, arg2, op);
      return value_assign (arg1, arg2);

    case BINOP_ADD:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, OP_NULL, noside);
      else
	return value_add (arg1, arg2);

    case BINOP_SUB:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, OP_NULL, noside);
      else
	return value_sub (arg1, arg2);

    case BINOP_MUL:
    case BINOP_DIV:
    case BINOP_REM:
    case BINOP_MOD:
    case BINOP_LSH:
    case BINOP_RSH:
    case BINOP_BITWISE_AND:
    case BINOP_BITWISE_IOR:
    case BINOP_BITWISE_XOR:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, OP_NULL, noside);
      else if (noside == EVAL_AVOID_SIDE_EFFECTS
	       && (op == BINOP_DIV || op == BINOP_REM || op == BINOP_MOD))
	return value_zero (VALUE_TYPE (arg1), not_lval);
      else
	return value_binop (arg1, arg2, op);

    case BINOP_RANGE:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      error ("':' operator used in invalid context");

    case BINOP_SUBSCRIPT:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	return value_x_binop (arg1, arg2, op, OP_NULL, noside);
      else
	{
	  /* If the user attempts to subscript something that is not an
	     array or pointer type (like a plain int variable for example),
	     then report this as an error. */

	  COERCE_REF (arg1);
	  type = check_typedef (VALUE_TYPE (arg1));
	  if (TYPE_CODE (type) != TYPE_CODE_ARRAY
	      && TYPE_CODE (type) != TYPE_CODE_PTR)
	    {
	      if (TYPE_NAME (type))
		error ("cannot subscript something of type `%s'",
		       TYPE_NAME (type));
	      else
		error ("cannot subscript requested type");
	    }

	  if (noside == EVAL_AVOID_SIDE_EFFECTS)
	    return value_zero (TYPE_TARGET_TYPE (type), VALUE_LVAL (arg1));
	  else
	    return value_subscript (arg1, arg2);
	}

    case BINOP_IN:
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      return value_in (arg1, arg2);

    case MULTI_SUBSCRIPT:
      (*pos) += 2;
      nargs = longest_to_int (exp->elts[pc + 1].longconst);
      arg1 = evaluate_subexp_with_coercion (exp, pos, noside);
      while (nargs-- > 0)
	{
	  arg2 = evaluate_subexp_with_coercion (exp, pos, noside);
	  /* FIXME:  EVAL_SKIP handling may not be correct. */
	  if (noside == EVAL_SKIP)
	    {
	      if (nargs > 0)
		{
		  continue;
		}
	      else
		{
		  goto nosideret;
		}
	    }
	  /* FIXME:  EVAL_AVOID_SIDE_EFFECTS handling may not be correct. */
	  if (noside == EVAL_AVOID_SIDE_EFFECTS)
	    {
	      /* If the user attempts to subscript something that has no target
	         type (like a plain int variable for example), then report this
	         as an error. */

	      type = TYPE_TARGET_TYPE (check_typedef (VALUE_TYPE (arg1)));
	      if (type != NULL)
		{
		  arg1 = value_zero (type, VALUE_LVAL (arg1));
		  noside = EVAL_SKIP;
		  continue;
		}
	      else
		{
		  error ("cannot subscript something of type `%s'",
			 TYPE_NAME (VALUE_TYPE (arg1)));
		}
	    }

	  if (binop_user_defined_p (op, arg1, arg2))
	    {
	      arg1 = value_x_binop (arg1, arg2, op, OP_NULL, noside);
	    }
	  else
	    {
	      arg1 = value_subscript (arg1, arg2);
	    }
	}
      return (arg1);

    multi_f77_subscript:
      {
	int subscript_array[MAX_FORTRAN_DIMS + 1];	/* 1-based array of 
							   subscripts, max == 7 */
	int array_size_array[MAX_FORTRAN_DIMS + 1];
	int ndimensions = 1, i;
	struct type *tmp_type;
	int offset_item;	/* The array offset where the item lives */

	if (nargs > MAX_FORTRAN_DIMS)
	  error ("Too many subscripts for F77 (%d Max)", MAX_FORTRAN_DIMS);

	tmp_type = check_typedef (VALUE_TYPE (arg1));
	ndimensions = calc_f77_array_dims (type);

	if (nargs != ndimensions)
	  error ("Wrong number of subscripts");

	/* Now that we know we have a legal array subscript expression 
	   let us actually find out where this element exists in the array. */

	offset_item = 0;
	for (i = 1; i <= nargs; i++)
	  {
	    /* Evaluate each subscript, It must be a legal integer in F77 */
	    arg2 = evaluate_subexp_with_coercion (exp, pos, noside);

	    /* Fill in the subscript and array size arrays */

	    subscript_array[i] = value_as_long (arg2);

	    retcode = f77_get_dynamic_upperbound (tmp_type, &upper);
	    if (retcode == BOUND_FETCH_ERROR)
	      error ("Cannot obtain dynamic upper bound");

	    retcode = f77_get_dynamic_lowerbound (tmp_type, &lower);
	    if (retcode == BOUND_FETCH_ERROR)
	      error ("Cannot obtain dynamic lower bound");

	    array_size_array[i] = upper - lower + 1;

	    /* Zero-normalize subscripts so that offsetting will work. */

	    subscript_array[i] -= lower;

	    /* If we are at the bottom of a multidimensional 
	       array type then keep a ptr to the last ARRAY
	       type around for use when calling value_subscript()
	       below. This is done because we pretend to value_subscript
	       that we actually have a one-dimensional array 
	       of base element type that we apply a simple 
	       offset to. */

	    if (i < nargs)
	      tmp_type = check_typedef (TYPE_TARGET_TYPE (tmp_type));
	  }

	/* Now let us calculate the offset for this item */

	offset_item = subscript_array[ndimensions];

	for (i = ndimensions - 1; i >= 1; i--)
	  offset_item =
	    array_size_array[i] * offset_item + subscript_array[i];

	/* Construct a value node with the value of the offset */

	arg2 = value_from_longest (builtin_type_f_integer, offset_item);

	/* Let us now play a dirty trick: we will take arg1 
	   which is a value node pointing to the topmost level
	   of the multidimensional array-set and pretend
	   that it is actually a array of the final element 
	   type, this will ensure that value_subscript()
	   returns the correct type value */

	VALUE_TYPE (arg1) = tmp_type;
	return value_ind (value_add (value_coerce_array (arg1), arg2));
      }

    case BINOP_LOGICAL_AND:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	{
	  arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	  goto nosideret;
	}

      oldpos = *pos;
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      *pos = oldpos;

      if (binop_user_defined_p (op, arg1, arg2))
	{
	  arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_logical_not (arg1);
	  arg2 = evaluate_subexp (NULL_TYPE, exp, pos,
				  (tem ? EVAL_SKIP : noside));
	  return value_from_longest (LA_BOOL_TYPE,
			     (LONGEST) (!tem && !value_logical_not (arg2)));
	}

    case BINOP_LOGICAL_OR:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	{
	  arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	  goto nosideret;
	}

      oldpos = *pos;
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      *pos = oldpos;

      if (binop_user_defined_p (op, arg1, arg2))
	{
	  arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_logical_not (arg1);
	  arg2 = evaluate_subexp (NULL_TYPE, exp, pos,
				  (!tem ? EVAL_SKIP : noside));
	  return value_from_longest (LA_BOOL_TYPE,
			     (LONGEST) (!tem || !value_logical_not (arg2)));
	}

    case BINOP_EQUAL:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_equal (arg1, arg2);
	  return value_from_longest (LA_BOOL_TYPE, (LONGEST) tem);
	}

    case BINOP_NOTEQUAL:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_equal (arg1, arg2);
	  return value_from_longest (LA_BOOL_TYPE, (LONGEST) ! tem);
	}

    case BINOP_LESS:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_less (arg1, arg2);
	  return value_from_longest (LA_BOOL_TYPE, (LONGEST) tem);
	}

    case BINOP_GTR:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_less (arg2, arg1);
	  return value_from_longest (LA_BOOL_TYPE, (LONGEST) tem);
	}

    case BINOP_GEQ:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_less (arg2, arg1) || value_equal (arg1, arg2);
	  return value_from_longest (LA_BOOL_TYPE, (LONGEST) tem);
	}

    case BINOP_LEQ:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (VALUE_TYPE (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (binop_user_defined_p (op, arg1, arg2))
	{
	  return value_x_binop (arg1, arg2, op, OP_NULL, noside);
	}
      else
	{
	  tem = value_less (arg1, arg2) || value_equal (arg1, arg2);
	  return value_from_longest (LA_BOOL_TYPE, (LONGEST) tem);
	}

    case BINOP_REPEAT:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      arg2 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      type = check_typedef (VALUE_TYPE (arg2));
      if (TYPE_CODE (type) != TYPE_CODE_INT)
	error ("Non-integral right operand for \"@\" operator.");
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  return allocate_repeat_value (VALUE_TYPE (arg1),
				     longest_to_int (value_as_long (arg2)));
	}
      else
	return value_repeat (arg1, longest_to_int (value_as_long (arg2)));

    case BINOP_COMMA:
      evaluate_subexp (NULL_TYPE, exp, pos, noside);
      return evaluate_subexp (NULL_TYPE, exp, pos, noside);

    case UNOP_NEG:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (unop_user_defined_p (op, arg1))
	return value_x_unop (arg1, op, noside);
      else
	return value_neg (arg1);

    case UNOP_COMPLEMENT:
      /* C++: check for and handle destructor names.  */
      op = exp->elts[*pos].opcode;

      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (unop_user_defined_p (UNOP_COMPLEMENT, arg1))
	return value_x_unop (arg1, UNOP_COMPLEMENT, noside);
      else
	return value_complement (arg1);

    case UNOP_LOGICAL_NOT:
      arg1 = evaluate_subexp (NULL_TYPE, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (unop_user_defined_p (op, arg1))
	return value_x_unop (arg1, op, noside);
      else
	return value_from_longest (LA_BOOL_TYPE,
				   (LONGEST) value_logical_not (arg1));

    case UNOP_IND:
      if (expect_type && TYPE_CODE (expect_type) == TYPE_CODE_PTR)
	expect_type = TYPE_TARGET_TYPE (check_typedef (expect_type));
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if ((TYPE_TARGET_TYPE (VALUE_TYPE (arg1))) &&
	  ((TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg1))) == TYPE_CODE_METHOD) ||
	   (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (arg1))) == TYPE_CODE_MEMBER)))
	error ("Attempt to dereference pointer to member without an object");
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (unop_user_defined_p (op, arg1))
	return value_x_unop (arg1, op, noside);
      else if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  type = check_typedef (VALUE_TYPE (arg1));
	  if (TYPE_CODE (type) == TYPE_CODE_PTR
	      || TYPE_CODE (type) == TYPE_CODE_REF
	  /* In C you can dereference an array to get the 1st elt.  */
	      || TYPE_CODE (type) == TYPE_CODE_ARRAY
	    )
	    return value_zero (TYPE_TARGET_TYPE (type),
			       lval_memory);
	  else if (TYPE_CODE (type) == TYPE_CODE_INT)
	    /* GDB allows dereferencing an int.  */
	    return value_zero (builtin_type_int, lval_memory);
	  else
	    error ("Attempt to take contents of a non-pointer value.");
	}
      return value_ind (arg1);

    case UNOP_ADDR:
      /* C++: check for and handle pointer to members.  */

      op = exp->elts[*pos].opcode;

      if (noside == EVAL_SKIP)
	{
	  if (op == OP_SCOPE)
	    {
	      int temm = longest_to_int (exp->elts[pc + 3].longconst);
	      (*pos) += 3 + BYTES_TO_EXP_ELEM (temm + 1);
	    }
	  else
	    evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
	  goto nosideret;
	}
      else
	{
	  value_ptr retvalp = evaluate_subexp_for_address (exp, pos, noside);
	  /* If HP aCC object, use bias for pointers to members */
	  if (hp_som_som_object_present &&
	      (TYPE_CODE (VALUE_TYPE (retvalp)) == TYPE_CODE_PTR) &&
	      (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (retvalp))) == TYPE_CODE_MEMBER))
	    {
	      unsigned int *ptr = (unsigned int *) VALUE_CONTENTS (retvalp);	/* forces evaluation */
	      *ptr |= 0x20000000;	/* set 29th bit */
	    }
	  return retvalp;
	}

    case UNOP_SIZEOF:
      if (noside == EVAL_SKIP)
	{
	  evaluate_subexp (NULL_TYPE, exp, pos, EVAL_SKIP);
	  goto nosideret;
	}
      return evaluate_subexp_for_sizeof (exp, pos);

    case UNOP_CAST:
      (*pos) += 2;
      type = exp->elts[pc + 1].type;
      arg1 = evaluate_subexp (type, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (type != VALUE_TYPE (arg1))
	arg1 = value_cast (type, arg1);
      return arg1;

    case UNOP_MEMVAL:
      (*pos) += 2;
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP)
	goto nosideret;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	return value_zero (exp->elts[pc + 1].type, lval_memory);
      else
	return value_at_lazy (exp->elts[pc + 1].type,
			      value_as_pointer (arg1),
			      NULL);

    case UNOP_PREINCREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op, noside);
	}
      else
	{
	  arg2 = value_add (arg1, value_from_longest (builtin_type_char,
						      (LONGEST) 1));
	  return value_assign (arg1, arg2);
	}

    case UNOP_PREDECREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op, noside);
	}
      else
	{
	  arg2 = value_sub (arg1, value_from_longest (builtin_type_char,
						      (LONGEST) 1));
	  return value_assign (arg1, arg2);
	}

    case UNOP_POSTINCREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op, noside);
	}
      else
	{
	  arg2 = value_add (arg1, value_from_longest (builtin_type_char,
						      (LONGEST) 1));
	  value_assign (arg1, arg2);
	  return arg1;
	}

    case UNOP_POSTDECREMENT:
      arg1 = evaluate_subexp (expect_type, exp, pos, noside);
      if (noside == EVAL_SKIP || noside == EVAL_AVOID_SIDE_EFFECTS)
	return arg1;
      else if (unop_user_defined_p (op, arg1))
	{
	  return value_x_unop (arg1, op, noside);
	}
      else
	{
	  arg2 = value_sub (arg1, value_from_longest (builtin_type_char,
						      (LONGEST) 1));
	  value_assign (arg1, arg2);
	  return arg1;
	}

    case OP_THIS:
      (*pos) += 1;
      return value_of_local ("this", 1);

    case OP_SELF:
      (*pos) += 1;
      return value_of_local ("self", 1);

    case OP_TYPE:
      error ("Attempt to use a type name as an expression");

    default:
      /* Removing this case and compiling with gcc -Wall reveals that
         a lot of cases are hitting this case.  Some of these should
         probably be removed from expression.h; others are legitimate
         expressions which are (apparently) not fully implemented.

         If there are any cases landing here which mean a user error,
         then they should be separate cases, with more descriptive
         error messages.  */

      error ("\
GDB does not (yet) know how to evaluate that kind of expression");
    }

nosideret:
  return value_from_longest (builtin_type_long, (LONGEST) 1);
}

/* Evaluate a subexpression of EXP, at index *POS,
   and return the address of that subexpression.
   Advance *POS over the subexpression.
   If the subexpression isn't an lvalue, get an error.
   NOSIDE may be EVAL_AVOID_SIDE_EFFECTS;
   then only the type of the result need be correct.  */

static value_ptr
evaluate_subexp_for_address (exp, pos, noside)
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  enum exp_opcode op;
  register int pc;
  struct symbol *var;

  pc = (*pos);
  op = exp->elts[pc].opcode;

  switch (op)
    {
    case UNOP_IND:
      (*pos)++;
      return evaluate_subexp (NULL_TYPE, exp, pos, noside);

    case UNOP_MEMVAL:
      (*pos) += 3;
      return value_cast (lookup_pointer_type (exp->elts[pc + 1].type),
			 evaluate_subexp (NULL_TYPE, exp, pos, noside));

    case OP_VAR_VALUE:
      var = exp->elts[pc + 2].symbol;

      /* C++: The "address" of a reference should yield the address
       * of the object pointed to. Let value_addr() deal with it. */
      if (TYPE_CODE (SYMBOL_TYPE (var)) == TYPE_CODE_REF)
	goto default_case;

      (*pos) += 4;
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  struct type *type =
	  lookup_pointer_type (SYMBOL_TYPE (var));
	  enum address_class sym_class = SYMBOL_CLASS (var);

	  if (sym_class == LOC_CONST
	      || sym_class == LOC_CONST_BYTES
	      || sym_class == LOC_REGISTER
	      || sym_class == LOC_REGPARM)
	    error ("Attempt to take address of register or constant.");

	  return
	    value_zero (type, not_lval);
	}
      else
	return
	  locate_var_value
	  (var,
	   block_innermost_frame (exp->elts[pc + 1].block));

    default:
    default_case:
      if (noside == EVAL_AVOID_SIDE_EFFECTS)
	{
	  value_ptr x = evaluate_subexp (NULL_TYPE, exp, pos, noside);
	  if (VALUE_LVAL (x) == lval_memory)
	    return value_zero (lookup_pointer_type (VALUE_TYPE (x)),
			       not_lval);
	  else
	    error ("Attempt to take address of non-lval");
	}
      return value_addr (evaluate_subexp (NULL_TYPE, exp, pos, noside));
    }
}

/* Evaluate like `evaluate_subexp' except coercing arrays to pointers.
   When used in contexts where arrays will be coerced anyway, this is
   equivalent to `evaluate_subexp' but much faster because it avoids
   actually fetching array contents (perhaps obsolete now that we have
   VALUE_LAZY).

   Note that we currently only do the coercion for C expressions, where
   arrays are zero based and the coercion is correct.  For other languages,
   with nonzero based arrays, coercion loses.  Use CAST_IS_CONVERSION
   to decide if coercion is appropriate.

 */

value_ptr
evaluate_subexp_with_coercion (exp, pos, noside)
     register struct expression *exp;
     register int *pos;
     enum noside noside;
{
  register enum exp_opcode op;
  register int pc;
  register value_ptr val;
  struct symbol *var;

  pc = (*pos);
  op = exp->elts[pc].opcode;

  switch (op)
    {
    case OP_VAR_VALUE:
      var = exp->elts[pc + 2].symbol;
      if (TYPE_CODE (check_typedef (SYMBOL_TYPE (var))) == TYPE_CODE_ARRAY
	  && CAST_IS_CONVERSION)
	{
	  (*pos) += 4;
	  val =
	    locate_var_value
	    (var, block_innermost_frame (exp->elts[pc + 1].block));
	  return value_cast (lookup_pointer_type (TYPE_TARGET_TYPE (SYMBOL_TYPE (var))),
			     val);
	}
      /* FALLTHROUGH */

    default:
      return evaluate_subexp (NULL_TYPE, exp, pos, noside);
    }
}

/* Evaluate a subexpression of EXP, at index *POS,
   and return a value for the size of that subexpression.
   Advance *POS over the subexpression.  */

static value_ptr
evaluate_subexp_for_sizeof (exp, pos)
     register struct expression *exp;
     register int *pos;
{
  enum exp_opcode op;
  register int pc;
  struct type *type;
  value_ptr val;

  pc = (*pos);
  op = exp->elts[pc].opcode;

  switch (op)
    {
      /* This case is handled specially
         so that we avoid creating a value for the result type.
         If the result type is very big, it's desirable not to
         create a value unnecessarily.  */
    case UNOP_IND:
      (*pos)++;
      val = evaluate_subexp (NULL_TYPE, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      type = check_typedef (VALUE_TYPE (val));
      if (TYPE_CODE (type) != TYPE_CODE_PTR
	  && TYPE_CODE (type) != TYPE_CODE_REF
	  && TYPE_CODE (type) != TYPE_CODE_ARRAY)
	error ("Attempt to take contents of a non-pointer value.");
      type = check_typedef (TYPE_TARGET_TYPE (type));
      return value_from_longest (builtin_type_int, (LONGEST)
				 TYPE_LENGTH (type));

    case UNOP_MEMVAL:
      (*pos) += 3;
      type = check_typedef (exp->elts[pc + 1].type);
      return value_from_longest (builtin_type_int,
				 (LONGEST) TYPE_LENGTH (type));

    case OP_VAR_VALUE:
      (*pos) += 4;
      type = check_typedef (SYMBOL_TYPE (exp->elts[pc + 2].symbol));
      return
	value_from_longest (builtin_type_int, (LONGEST) TYPE_LENGTH (type));

    default:
      val = evaluate_subexp (NULL_TYPE, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      return value_from_longest (builtin_type_int,
				 (LONGEST) TYPE_LENGTH (VALUE_TYPE (val)));
    }
}

/* Parse a type expression in the string [P..P+LENGTH). */

struct type *
parse_and_eval_type (p, length)
     char *p;
     int length;
{
  char *tmp = (char *) alloca (length + 4);
  struct expression *expr;
  tmp[0] = '(';
  memcpy (tmp + 1, p, length);
  tmp[length + 1] = ')';
  tmp[length + 2] = '0';
  tmp[length + 3] = '\0';
  expr = parse_expression (tmp);
  if (expr->elts[0].opcode != UNOP_CAST)
    error ("Internal error in eval_type.");
  return expr->elts[1].type;
}

int
calc_f77_array_dims (array_type)
     struct type *array_type;
{
  int ndimen = 1;
  struct type *tmp_type;

  if ((TYPE_CODE (array_type) != TYPE_CODE_ARRAY))
    error ("Can't get dimensions for a non-array type");

  tmp_type = array_type;

  while ((tmp_type = TYPE_TARGET_TYPE (tmp_type)))
    {
      if (TYPE_CODE (tmp_type) == TYPE_CODE_ARRAY)
	++ndimen;
    }
  return ndimen;
}
