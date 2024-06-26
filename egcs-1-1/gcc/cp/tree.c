/* Language-dependent node constructors for parse phase of GNU compiler.
   Copyright (C) 1987, 88, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)

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
#include "obstack.h"
#include "tree.h"
#include "cp-tree.h"
#include "flags.h"
#include "rtl.h"
#include "toplev.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern void compiler_error ();

static tree get_identifier_list PROTO((tree));
static tree bot_manip PROTO((tree));
static tree perm_manip PROTO((tree));
static tree build_cplus_array_type_1 PROTO((tree, tree));
static void list_hash_add PROTO((int, tree));
static int list_hash PROTO((tree, tree, tree));
static tree list_hash_lookup PROTO((int, int, int, int, tree, tree,
				    tree));
static void propagate_binfo_offsets PROTO((tree, tree));
static void unshare_base_binfos PROTO((tree));
static int avoid_overlap PROTO((tree, tree));
static int equal_functions PROTO((tree, tree));

#define CEIL(x,y) (((x) + (y) - 1) / (y))

/* Return nonzero if REF is an lvalue valid for this language.
   Lvalues can be assigned, unless they have TREE_READONLY.
   Lvalues can have their address taken, unless they have DECL_REGISTER.  */

int
real_lvalue_p (ref)
     tree ref;
{
  if (! language_lvalue_valid (ref))
    return 0;
  
  if (TREE_CODE (TREE_TYPE (ref)) == REFERENCE_TYPE)
    return 1;

  if (ref == current_class_ptr && flag_this_is_variable <= 0)
    return 0;

  switch (TREE_CODE (ref))
    {
      /* preincrements and predecrements are valid lvals, provided
	 what they refer to are valid lvals.  */
    case PREINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case COMPONENT_REF:
    case SAVE_EXPR:
    case UNSAVE_EXPR:
    case TRY_CATCH_EXPR:
    case WITH_CLEANUP_EXPR:
      return real_lvalue_p (TREE_OPERAND (ref, 0));

    case STRING_CST:
      return 1;

    case VAR_DECL:
      if (TREE_READONLY (ref) && ! TREE_STATIC (ref)
	  && DECL_LANG_SPECIFIC (ref)
	  && DECL_IN_AGGR_P (ref))
	return 0;
    case INDIRECT_REF:
    case ARRAY_REF:
    case PARM_DECL:
    case RESULT_DECL:
    case ERROR_MARK:
      if (TREE_CODE (TREE_TYPE (ref)) != FUNCTION_TYPE
	  && TREE_CODE (TREE_TYPE (ref)) != METHOD_TYPE)
	return 1;
      break;

      /* A currently unresolved scope ref.  */
    case SCOPE_REF:
      my_friendly_abort (103);
    case OFFSET_REF:
      if (TREE_CODE (TREE_OPERAND (ref, 1)) == FUNCTION_DECL)
	return 1;
      return real_lvalue_p (TREE_OPERAND (ref, 0))
	&& real_lvalue_p (TREE_OPERAND (ref, 1));
      break;

    case COND_EXPR:
      return (real_lvalue_p (TREE_OPERAND (ref, 1))
	      && real_lvalue_p (TREE_OPERAND (ref, 2)));

    case MODIFY_EXPR:
      return 1;

    case COMPOUND_EXPR:
      return real_lvalue_p (TREE_OPERAND (ref, 1));

    case MAX_EXPR:
    case MIN_EXPR:
      return (real_lvalue_p (TREE_OPERAND (ref, 0))
	      && real_lvalue_p (TREE_OPERAND (ref, 1)));

    default:
      break;
    }

  return 0;
}

/* This differs from real_lvalue_p in that class rvalues are considered
   lvalues.  */
int
lvalue_p (ref)
     tree ref;
{
  if (! language_lvalue_valid (ref))
    return 0;
  
  if (TREE_CODE (TREE_TYPE (ref)) == REFERENCE_TYPE)
    return 1;

  if (ref == current_class_ptr && flag_this_is_variable <= 0)
    return 0;

  switch (TREE_CODE (ref))
    {
      /* preincrements and predecrements are valid lvals, provided
	 what they refer to are valid lvals.  */
    case PREINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
    case COMPONENT_REF:
    case SAVE_EXPR:
    case UNSAVE_EXPR:
    case TRY_CATCH_EXPR:
    case WITH_CLEANUP_EXPR:
      return lvalue_p (TREE_OPERAND (ref, 0));

    case STRING_CST:
      return 1;

    case VAR_DECL:
      if (TREE_READONLY (ref) && ! TREE_STATIC (ref)
	  && DECL_LANG_SPECIFIC (ref)
	  && DECL_IN_AGGR_P (ref))
	return 0;
    case INDIRECT_REF:
    case ARRAY_REF:
    case PARM_DECL:
    case RESULT_DECL:
    case ERROR_MARK:
      if (TREE_CODE (TREE_TYPE (ref)) != FUNCTION_TYPE
	  && TREE_CODE (TREE_TYPE (ref)) != METHOD_TYPE)
	return 1;
      break;

    case TARGET_EXPR:
      return 1;

    case CALL_EXPR:
      if (IS_AGGR_TYPE (TREE_TYPE (ref)))
	return 1;
      break;

      /* A currently unresolved scope ref.  */
    case SCOPE_REF:
      my_friendly_abort (103);
    case OFFSET_REF:
      if (TREE_CODE (TREE_OPERAND (ref, 1)) == FUNCTION_DECL)
	return 1;
      return lvalue_p (TREE_OPERAND (ref, 0))
	&& lvalue_p (TREE_OPERAND (ref, 1));
      break;

    case COND_EXPR:
      return (lvalue_p (TREE_OPERAND (ref, 1))
	      && lvalue_p (TREE_OPERAND (ref, 2)));

    case MODIFY_EXPR:
      return 1;

    case COMPOUND_EXPR:
      return lvalue_p (TREE_OPERAND (ref, 1));

    case MAX_EXPR:
    case MIN_EXPR:
      return (lvalue_p (TREE_OPERAND (ref, 0))
	      && lvalue_p (TREE_OPERAND (ref, 1)));

    default:
      break;
    }

  return 0;
}

/* Return nonzero if REF is an lvalue valid for this language;
   otherwise, print an error message and return zero.  */

int
lvalue_or_else (ref, string)
     tree ref;
     char *string;
{
  int win = lvalue_p (ref);
  if (! win)
    error ("non-lvalue in %s", string);
  return win;
}

/* INIT is a CALL_EXPR which needs info about its target.
   TYPE is the type that this initialization should appear to have.

   Build an encapsulation of the initialization to perform
   and return it so that it can be processed by language-independent
   and language-specific expression expanders.  */

tree
build_cplus_new (type, init)
     tree type;
     tree init;
{
  tree slot;
  tree rval;

  if (TREE_CODE (init) != CALL_EXPR && TREE_CODE (init) != AGGR_INIT_EXPR)
    return init;

  slot = build (VAR_DECL, type);
  DECL_ARTIFICIAL (slot) = 1;
  layout_decl (slot, 0);
  rval = build (AGGR_INIT_EXPR, type,
		TREE_OPERAND (init, 0), TREE_OPERAND (init, 1), slot);
  TREE_SIDE_EFFECTS (rval) = 1;
  rval = build (TARGET_EXPR, type, slot, rval, NULL_TREE, NULL_TREE);
  TREE_SIDE_EFFECTS (rval) = 1;

  return rval;
}

/* Encapsulate the expression INIT in a TARGET_EXPR.  */

tree
get_target_expr (init)
     tree init;
{
  tree slot;
  tree rval;

  slot = build (VAR_DECL, TREE_TYPE (init));
  DECL_ARTIFICIAL (slot) = 1;
  layout_decl (slot, 0);
  rval = build (TARGET_EXPR, TREE_TYPE (init), slot, init,
		NULL_TREE, NULL_TREE);
  TREE_SIDE_EFFECTS (rval) = 1;

  return rval;
}

/* Recursively search EXP for CALL_EXPRs that need cleanups and replace
   these CALL_EXPRs with tree nodes that will perform the cleanups.  */

tree
break_out_cleanups (exp)
     tree exp;
{
  tree tmp = exp;

  if (TREE_CODE (tmp) == CALL_EXPR
      && TYPE_NEEDS_DESTRUCTOR (TREE_TYPE (tmp)))
    return build_cplus_new (TREE_TYPE (tmp), tmp);

  while (TREE_CODE (tmp) == NOP_EXPR
	 || TREE_CODE (tmp) == CONVERT_EXPR
	 || TREE_CODE (tmp) == NON_LVALUE_EXPR)
    {
      if (TREE_CODE (TREE_OPERAND (tmp, 0)) == CALL_EXPR
	  && TYPE_NEEDS_DESTRUCTOR (TREE_TYPE (TREE_OPERAND (tmp, 0))))
	{
	  TREE_OPERAND (tmp, 0)
	    = build_cplus_new (TREE_TYPE (TREE_OPERAND (tmp, 0)),
			       TREE_OPERAND (tmp, 0));
	  break;
	}
      else
	tmp = TREE_OPERAND (tmp, 0);
    }
  return exp;
}

/* Recursively perform a preorder search EXP for CALL_EXPRs, making
   copies where they are found.  Returns a deep copy all nodes transitively
   containing CALL_EXPRs.  */

tree
break_out_calls (exp)
     tree exp;
{
  register tree t1, t2 = NULL_TREE;
  register enum tree_code code;
  register int changed = 0;
  register int i;

  if (exp == NULL_TREE)
    return exp;

  code = TREE_CODE (exp);

  if (code == CALL_EXPR)
    return copy_node (exp);

  /* Don't try and defeat a save_expr, as it should only be done once.  */
    if (code == SAVE_EXPR)
       return exp;

  switch (TREE_CODE_CLASS (code))
    {
    default:
      abort ();

    case 'c':  /* a constant */
    case 't':  /* a type node */
    case 'x':  /* something random, like an identifier or an ERROR_MARK.  */
      return exp;

    case 'd':  /* A decl node */
#if 0                               /* This is bogus.  jason 9/21/94 */

      t1 = break_out_calls (DECL_INITIAL (exp));
      if (t1 != DECL_INITIAL (exp))
	{
	  exp = copy_node (exp);
	  DECL_INITIAL (exp) = t1;
	}
#endif
      return exp;

    case 'b':  /* A block node */
      {
	/* Don't know how to handle these correctly yet.   Must do a
	   break_out_calls on all DECL_INITIAL values for local variables,
	   and also break_out_calls on all sub-blocks and sub-statements.  */
	abort ();
      }
      return exp;

    case 'e':  /* an expression */
    case 'r':  /* a reference */
    case 's':  /* an expression with side effects */
      for (i = tree_code_length[(int) code] - 1; i >= 0; i--)
	{
	  t1 = break_out_calls (TREE_OPERAND (exp, i));
	  if (t1 != TREE_OPERAND (exp, i))
	    {
	      exp = copy_node (exp);
	      TREE_OPERAND (exp, i) = t1;
	    }
	}
      return exp;

    case '<':  /* a comparison expression */
    case '2':  /* a binary arithmetic expression */
      t2 = break_out_calls (TREE_OPERAND (exp, 1));
      if (t2 != TREE_OPERAND (exp, 1))
	changed = 1;
    case '1':  /* a unary arithmetic expression */
      t1 = break_out_calls (TREE_OPERAND (exp, 0));
      if (t1 != TREE_OPERAND (exp, 0))
	changed = 1;
      if (changed)
	{
	  if (tree_code_length[(int) code] == 1)
	    return build1 (code, TREE_TYPE (exp), t1);
	  else
	    return build (code, TREE_TYPE (exp), t1, t2);
	}
      return exp;
    }

}

extern struct obstack *current_obstack;
extern struct obstack permanent_obstack, class_obstack;
extern struct obstack *saveable_obstack;
extern struct obstack *expression_obstack;

/* Here is how primitive or already-canonicalized types' hash
   codes are made.  MUST BE CONSISTENT WITH tree.c !!! */
#define TYPE_HASH(TYPE) ((HOST_WIDE_INT) (TYPE) & 0777777)

/* Construct, lay out and return the type of methods belonging to class
   BASETYPE and whose arguments are described by ARGTYPES and whose values
   are described by RETTYPE.  If each type exists already, reuse it.  */

tree
build_cplus_method_type (basetype, rettype, argtypes)
     tree basetype, rettype, argtypes;
{
  register tree t;
  tree ptype;
  int hashcode;

  /* Make a node of the sort we want.  */
  t = make_node (METHOD_TYPE);

  TYPE_METHOD_BASETYPE (t) = TYPE_MAIN_VARIANT (basetype);
  TREE_TYPE (t) = rettype;
  if (IS_SIGNATURE (basetype))
    ptype = build_signature_pointer_type (TYPE_MAIN_VARIANT (basetype),
					  TYPE_READONLY (basetype),
					  TYPE_VOLATILE (basetype));
  else
    ptype = build_pointer_type (basetype);

  /* The actual arglist for this function includes a "hidden" argument
     which is "this".  Put it into the list of argument types.  */

  argtypes = tree_cons (NULL_TREE, ptype, argtypes);
  TYPE_ARG_TYPES (t) = argtypes;
  TREE_SIDE_EFFECTS (argtypes) = 1;  /* Mark first argtype as "artificial".  */

  /* If we already have such a type, use the old one and free this one.
     Note that it also frees up the above cons cell if found.  */
  hashcode = TYPE_HASH (basetype) + TYPE_HASH (rettype) + type_hash_list (argtypes);
  t = type_hash_canon (hashcode, t);

  if (TYPE_SIZE (t) == 0)
    layout_type (t);

  return t;
}

static tree
build_cplus_array_type_1 (elt_type, index_type)
     tree elt_type;
     tree index_type;
{
  register struct obstack *ambient_obstack = current_obstack;
  register struct obstack *ambient_saveable_obstack = saveable_obstack;
  tree t;

  /* We need a new one.  If both ELT_TYPE and INDEX_TYPE are permanent,
     make this permanent too.  */
  if (TREE_PERMANENT (elt_type)
      && (index_type == 0 || TREE_PERMANENT (index_type)))
    {
      current_obstack = &permanent_obstack;
      saveable_obstack = &permanent_obstack;
    }

  if (processing_template_decl
      || uses_template_parms (index_type))
    {
      t = make_node (ARRAY_TYPE);
      TREE_TYPE (t) = elt_type;
      TYPE_DOMAIN (t) = index_type;
    }
  else
    t = build_array_type (elt_type, index_type);

  /* Push these needs up so that initialization takes place
     more easily.  */
  TYPE_NEEDS_CONSTRUCTING (t) = TYPE_NEEDS_CONSTRUCTING (TYPE_MAIN_VARIANT (elt_type));
  TYPE_NEEDS_DESTRUCTOR (t) = TYPE_NEEDS_DESTRUCTOR (TYPE_MAIN_VARIANT (elt_type));
  current_obstack = ambient_obstack;
  saveable_obstack = ambient_saveable_obstack;
  return t;
}

tree
build_cplus_array_type (elt_type, index_type)
     tree elt_type;
     tree index_type;
{
  tree t;
  int constp = TYPE_READONLY (elt_type);
  int volatilep = TYPE_VOLATILE (elt_type);
  elt_type = TYPE_MAIN_VARIANT (elt_type);

  t = build_cplus_array_type_1 (elt_type, index_type);

  if (constp || volatilep)
    t = cp_build_type_variant (t, constp, volatilep);

  return t;
}

/* Make a variant type in the proper way for C/C++, propagating qualifiers
   down to the element type of an array.  */

tree
cp_build_type_variant (type, constp, volatilep)
     tree type;
     int constp, volatilep;
{
  if (type == error_mark_node)
    return type;
  
  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree real_main_variant = TYPE_MAIN_VARIANT (type);

      push_obstacks (TYPE_OBSTACK (real_main_variant),
		     TYPE_OBSTACK (real_main_variant));
      type = build_cplus_array_type_1 (cp_build_type_variant
				       (TREE_TYPE (type), constp, volatilep),
				       TYPE_DOMAIN (type));

      /* TYPE must be on same obstack as REAL_MAIN_VARIANT.  If not,
	 make a copy.  (TYPE might have come from the hash table and
	 REAL_MAIN_VARIANT might be in some function's obstack.)  */

      if (TYPE_OBSTACK (type) != TYPE_OBSTACK (real_main_variant))
	{
	  type = copy_node (type);
	  TYPE_POINTER_TO (type) = TYPE_REFERENCE_TO (type) = 0;
	}

      TYPE_MAIN_VARIANT (type) = real_main_variant;
      pop_obstacks ();
      return type;
    }
  return build_type_variant (type, constp, volatilep);
}

/* Returns the canonical version of TYPE.  In other words, if TYPE is
   a typedef, returns the underlying type.  The cv-qualification of
   the type returned matches the type input; they will always be
   compatible types.  */

tree
canonical_type_variant (t)
     tree t;
{
  int constp, volatilep;
  if (TREE_CODE (t) == ARRAY_TYPE)
    {
      constp = TYPE_READONLY (TREE_TYPE (t));
      volatilep = TYPE_VOLATILE (TREE_TYPE (t));
    }
  else
    {
      constp = TYPE_READONLY (t);
      volatilep = TYPE_VOLATILE (t);
    }
  return cp_build_type_variant (TYPE_MAIN_VARIANT (t), constp, volatilep);
}

/* Add OFFSET to all base types of T.

   OFFSET, which is a type offset, is number of bytes.

   Note that we don't have to worry about having two paths to the
   same base type, since this type owns its association list.  */

static void
propagate_binfo_offsets (binfo, offset)
     tree binfo;
     tree offset;
{
  tree binfos = BINFO_BASETYPES (binfo);
  int i, n_baselinks = binfos ? TREE_VEC_LENGTH (binfos) : 0;

  for (i = 0; i < n_baselinks; /* note increment is done in the loop.  */)
    {
      tree base_binfo = TREE_VEC_ELT (binfos, i);

      if (TREE_VIA_VIRTUAL (base_binfo))
	i += 1;
      else
	{
	  int j;
	  tree delta = NULL_TREE;

	  for (j = i+1; j < n_baselinks; j++)
	    if (! TREE_VIA_VIRTUAL (TREE_VEC_ELT (binfos, j)))
	      {
		/* The next basetype offset must take into account the space
		   between the classes, not just the size of each class.  */
		delta = size_binop (MINUS_EXPR,
				    BINFO_OFFSET (TREE_VEC_ELT (binfos, j)),
				    BINFO_OFFSET (base_binfo));
		break;
	      }

#if 0
	  if (BINFO_OFFSET_ZEROP (base_binfo))
	    BINFO_OFFSET (base_binfo) = offset;
	  else
	    BINFO_OFFSET (base_binfo)
	      = size_binop (PLUS_EXPR, BINFO_OFFSET (base_binfo), offset);
#else
	  BINFO_OFFSET (base_binfo) = offset;
#endif

	  unshare_base_binfos (base_binfo);

	  /* Go to our next class that counts for offset propagation.  */
	  i = j;
	  if (i < n_baselinks)
	    offset = size_binop (PLUS_EXPR, offset, delta);
	}
    }
}

/* Makes new binfos for the indirect bases under BASE_BINFO, and updates
   BINFO_OFFSET for them and their bases.  */

static void
unshare_base_binfos (base_binfo)
     tree base_binfo;
{
  if (BINFO_BASETYPES (base_binfo))
    {
      tree base_binfos = BINFO_BASETYPES (base_binfo);
      tree chain = NULL_TREE;
      int j;

      /* Now unshare the structure beneath BASE_BINFO.  */
      for (j = TREE_VEC_LENGTH (base_binfos)-1;
	   j >= 0; j--)
	{
	  tree base_base_binfo = TREE_VEC_ELT (base_binfos, j);
	  if (! TREE_VIA_VIRTUAL (base_base_binfo))
	    TREE_VEC_ELT (base_binfos, j)
	      = make_binfo (BINFO_OFFSET (base_base_binfo),
			    base_base_binfo,
			    BINFO_VTABLE (base_base_binfo),
			    BINFO_VIRTUALS (base_base_binfo),
			    chain);
	  chain = TREE_VEC_ELT (base_binfos, j);
	  TREE_VIA_PUBLIC (chain) = TREE_VIA_PUBLIC (base_base_binfo);
	  TREE_VIA_PROTECTED (chain) = TREE_VIA_PROTECTED (base_base_binfo);
	  BINFO_INHERITANCE_CHAIN (chain) = base_binfo;
	}

      /* Completely unshare potentially shared data, and
	 update what is ours.  */
      propagate_binfo_offsets (base_binfo, BINFO_OFFSET (base_binfo));
    }
}

/* Finish the work of layout_record, now taking virtual bases into account.
   Also compute the actual offsets that our base classes will have.
   This must be performed after the fields are laid out, since virtual
   baseclasses must lay down at the end of the record.

   Returns the maximum number of virtual functions any of the
   baseclasses provide.  */

int
layout_basetypes (rec, max)
     tree rec;
     int max;
{
  tree binfos = TYPE_BINFO_BASETYPES (rec);
  int i, n_baseclasses = binfos ? TREE_VEC_LENGTH (binfos) : 0;

  /* Get all the virtual base types that this type uses.
     The TREE_VALUE slot holds the virtual baseclass type.  */
  tree vbase_types = get_vbase_types (rec);

  unsigned int record_align = MAX (BITS_PER_UNIT, TYPE_ALIGN (rec));
  unsigned int desired_align;

  /* Record size so far is CONST_SIZE bits, where CONST_SIZE is an integer.  */
  register unsigned int const_size = 0;
  unsigned int nonvirtual_const_size;

#ifdef STRUCTURE_SIZE_BOUNDARY
  /* Packed structures don't need to have minimum size.  */
  if (! TYPE_PACKED (rec))
    record_align = MAX (record_align, STRUCTURE_SIZE_BOUNDARY);
#endif

  CLASSTYPE_VBASECLASSES (rec) = vbase_types;

  my_friendly_assert (TREE_CODE (TYPE_SIZE (rec)) == INTEGER_CST, 19970302);
  const_size = TREE_INT_CST_LOW (TYPE_SIZE (rec));

  nonvirtual_const_size = const_size;

  while (vbase_types)
    {
      tree basetype = BINFO_TYPE (vbase_types);
      tree offset;

      desired_align = TYPE_ALIGN (basetype);
      record_align = MAX (record_align, desired_align);

      if (const_size == 0)
	offset = integer_zero_node;
      else
	{
	  /* Give each virtual base type the alignment it wants.  */
	  const_size = CEIL (const_size, desired_align) * desired_align;
	  offset = size_int (CEIL (const_size, BITS_PER_UNIT));
	}

      if (CLASSTYPE_VSIZE (basetype) > max)
	max = CLASSTYPE_VSIZE (basetype);
      BINFO_OFFSET (vbase_types) = offset;

      /* Every virtual baseclass takes a least a UNIT, so that we can
	 take it's address and get something different for each base.  */
      const_size += MAX (BITS_PER_UNIT,
			 TREE_INT_CST_LOW (CLASSTYPE_SIZE (basetype)));

      vbase_types = TREE_CHAIN (vbase_types);
    }

  if (const_size)
    {
      /* Because a virtual base might take a single byte above,
	 we have to re-adjust the total size to make sure it is
	 a multiple of the alignment.  */
      /* Give the whole object the alignment it wants.  */
      const_size = CEIL (const_size, record_align) * record_align;
    }

  /* Set the alignment in the complete type.  We don't set CLASSTYPE_ALIGN
   here, as that is for this class, without any virtual base classes.  */
  TYPE_ALIGN (rec) = record_align;
  if (const_size != nonvirtual_const_size)
    {
      TYPE_SIZE (rec) = size_int (const_size);
      TYPE_SIZE_UNIT (rec) = size_binop (FLOOR_DIV_EXPR, TYPE_SIZE (rec),
                                         size_int (BITS_PER_UNIT));
    }

  /* Now propagate offset information throughout the lattice.  */
  for (i = 0; i < n_baseclasses; i++)
    {
      register tree base_binfo = TREE_VEC_ELT (binfos, i);
      register tree basetype = BINFO_TYPE (base_binfo);
      tree field = TYPE_FIELDS (rec);

      if (TREE_VIA_VIRTUAL (base_binfo))
	continue;

      my_friendly_assert (TREE_TYPE (field) == basetype, 23897);
      BINFO_OFFSET (base_binfo)
	= size_int (CEIL (TREE_INT_CST_LOW (DECL_FIELD_BITPOS (field)),
			  BITS_PER_UNIT));
      unshare_base_binfos (base_binfo);
      TYPE_FIELDS (rec) = TREE_CHAIN (field);
    }

  for (vbase_types = CLASSTYPE_VBASECLASSES (rec); vbase_types;
       vbase_types = TREE_CHAIN (vbase_types))
    {
      BINFO_INHERITANCE_CHAIN (vbase_types) = TYPE_BINFO (rec);
      unshare_base_binfos (vbase_types);
    }

  return max;
}

/* If the empty base field in DECL overlaps with a base of the same type in
   NEWDECL, which is either another base field or the first data field of
   the class, pad the base just before NEWDECL and return 1.  Otherwise,
   return 0.  */

static int
avoid_overlap (decl, newdecl)
     tree decl, newdecl;
{
  tree field;

  if (newdecl == NULL_TREE
      || ! types_overlap_p (TREE_TYPE (decl), TREE_TYPE (newdecl)))
    return 0;

  for (field = decl; TREE_CHAIN (field) && TREE_CHAIN (field) != newdecl;
       field = TREE_CHAIN (field))
    ;

  DECL_SIZE (field) = integer_one_node;

  return 1;
}

/* Returns a list of fields to stand in for the base class subobjects
   of REC.  These fields are later removed by layout_basetypes.  */

tree
build_base_fields (rec)
     tree rec;
{
  /* Chain to hold all the new FIELD_DECLs which stand in for base class
     subobjects.  */
  tree base_decls = NULL_TREE;
  tree binfos = TYPE_BINFO_BASETYPES (rec);
  int n_baseclasses = binfos ? TREE_VEC_LENGTH (binfos) : 0;
  tree decl, nextdecl;
  int i, saw_empty = 0;
  unsigned int base_align = 0;

  for (i = 0; i < n_baseclasses; ++i)
    {
      register tree base_binfo = TREE_VEC_ELT (binfos, i);
      register tree basetype = BINFO_TYPE (base_binfo);

      if (TYPE_SIZE (basetype) == 0)
	/* This error is now reported in xref_tag, thus giving better
	   location information.  */
	continue;

      if (TREE_VIA_VIRTUAL (base_binfo))
	continue;

      decl = build_lang_field_decl (FIELD_DECL, NULL_TREE, basetype);
      DECL_ARTIFICIAL (decl) = 1;
      DECL_FIELD_CONTEXT (decl) = DECL_CLASS_CONTEXT (decl) = rec;
      DECL_SIZE (decl) = CLASSTYPE_SIZE (basetype);
      DECL_ALIGN (decl) = CLASSTYPE_ALIGN (basetype);
      TREE_CHAIN (decl) = base_decls;
      base_decls = decl;

      if (! flag_new_abi)
	{
	  /* Brain damage for backwards compatibility.  For no good reason,
	     the old layout_basetypes made every base at least as large as
	     the alignment for the bases up to that point, gratuitously
	     wasting space.  So we do the same thing here.  */
	  base_align = MAX (base_align, DECL_ALIGN (decl));
	  DECL_SIZE (decl)
	    = size_int (MAX (TREE_INT_CST_LOW (DECL_SIZE (decl)),
			     base_align));
	}
      else if (DECL_SIZE (decl) == integer_zero_node)
	saw_empty = 1;
    }

  /* Reverse the list of fields so we allocate the bases in the proper
     order.  */
  base_decls = nreverse (base_decls);

  /* In the presence of empty base classes, we run the risk of allocating
     two objects of the same class on top of one another.  Avoid that.  */
  if (flag_new_abi && saw_empty)
    for (decl = base_decls; decl; decl = TREE_CHAIN (decl))
      {
	if (DECL_SIZE (decl) == integer_zero_node)
	  {
	    /* First step through the following bases until we find
	       an overlap or a non-empty base.  */
	    for (nextdecl = TREE_CHAIN (decl); nextdecl;
		 nextdecl = TREE_CHAIN (nextdecl))
	      {
		if (avoid_overlap (decl, nextdecl)
		    || DECL_SIZE (nextdecl) != integer_zero_node)
		  goto nextbase;
	      }

	    /* If we're still looking, also check against the first
	       field.  */
	    for (nextdecl = TYPE_FIELDS (rec);
		 nextdecl && TREE_CODE (nextdecl) != FIELD_DECL;
		 nextdecl = TREE_CHAIN (nextdecl))
	      /* keep looking */;
	    avoid_overlap (decl, nextdecl);
	  }
      nextbase:;
      }

  return base_decls;
}

/* Returns list of virtual base class pointers in a FIELD_DECL chain.  */

tree
build_vbase_pointer_fields (rec)
     tree rec;
{
  /* Chain to hold all the new FIELD_DECLs which point at virtual
     base classes.  */
  tree vbase_decls = NULL_TREE;
  tree binfos = TYPE_BINFO_BASETYPES (rec);
  int n_baseclasses = binfos ? TREE_VEC_LENGTH (binfos) : 0;
  tree decl;
  int i;

  /* Handle basetypes almost like fields, but record their
     offsets differently.  */

  for (i = 0; i < n_baseclasses; i++)
    {
      register tree base_binfo = TREE_VEC_ELT (binfos, i);
      register tree basetype = BINFO_TYPE (base_binfo);

      if (TYPE_SIZE (basetype) == 0)
	/* This error is now reported in xref_tag, thus giving better
	   location information.  */
	continue;

      /* All basetypes are recorded in the association list of the
	 derived type.  */

      if (TREE_VIA_VIRTUAL (base_binfo))
	{
	  int j;
	  char *name = (char *)alloca (TYPE_NAME_LENGTH (basetype)
				       + sizeof (VBASE_NAME) + 1);

	  /* The offset for a virtual base class is only used in computing
	     virtual function tables and for initializing virtual base
	     pointers.  It is built once `get_vbase_types' is called.  */

	  /* If this basetype can come from another vbase pointer
	     without an additional indirection, we will share
	     that pointer.  If an indirection is involved, we
	     make our own pointer.  */
	  for (j = 0; j < n_baseclasses; j++)
	    {
	      tree other_base_binfo = TREE_VEC_ELT (binfos, j);
	      if (! TREE_VIA_VIRTUAL (other_base_binfo)
		  && binfo_member (basetype,
				   CLASSTYPE_VBASECLASSES (BINFO_TYPE
							   (other_base_binfo))
				   ))
		goto got_it;
	    }
	  sprintf (name, VBASE_NAME_FORMAT, TYPE_NAME_STRING (basetype));
	  decl = build_lang_field_decl (FIELD_DECL, get_identifier (name),
					build_pointer_type (basetype));
	  /* If you change any of the below, take a look at all the
	     other VFIELD_BASEs and VTABLE_BASEs in the code, and change
	     them too.  */
	  DECL_ASSEMBLER_NAME (decl) = get_identifier (VTABLE_BASE);
	  DECL_VIRTUAL_P (decl) = 1;
	  DECL_ARTIFICIAL (decl) = 1;
	  DECL_FIELD_CONTEXT (decl) = rec;
	  DECL_CLASS_CONTEXT (decl) = rec;
	  DECL_FCONTEXT (decl) = basetype;
	  DECL_SAVED_INSNS (decl) = NULL_RTX;
	  DECL_FIELD_SIZE (decl) = 0;
	  DECL_ALIGN (decl) = TYPE_ALIGN (ptr_type_node);
	  TREE_CHAIN (decl) = vbase_decls;
	  BINFO_VPTR_FIELD (base_binfo) = decl;
	  vbase_decls = decl;

	got_it:
	  /* The space this decl occupies has already been accounted for.  */
	  ;
	}
    }

  return vbase_decls;
}

/* Hashing of lists so that we don't make duplicates.
   The entry point is `list_hash_canon'.  */

/* Each hash table slot is a bucket containing a chain
   of these structures.  */

struct list_hash
{
  struct list_hash *next;	/* Next structure in the bucket.  */
  int hashcode;			/* Hash code of this list.  */
  tree list;			/* The list recorded here.  */
};

/* Now here is the hash table.  When recording a list, it is added
   to the slot whose index is the hash code mod the table size.
   Note that the hash table is used for several kinds of lists.
   While all these live in the same table, they are completely independent,
   and the hash code is computed differently for each of these.  */

#define TYPE_HASH_SIZE 59
static struct list_hash *list_hash_table[TYPE_HASH_SIZE];

/* Compute a hash code for a list (chain of TREE_LIST nodes
   with goodies in the TREE_PURPOSE, TREE_VALUE, and bits of the
   TREE_COMMON slots), by adding the hash codes of the individual entries.  */

static int
list_hash (purpose, value, chain)
     tree purpose, value, chain;
{
  register int hashcode = 0;

  if (chain)
    hashcode += TYPE_HASH (chain);

  if (value)
    hashcode += TYPE_HASH (value);
  else
    hashcode += 1007;
  if (purpose)
    hashcode += TYPE_HASH (purpose);
  else
    hashcode += 1009;
  return hashcode;
}

/* Look in the type hash table for a type isomorphic to TYPE.
   If one is found, return it.  Otherwise return 0.  */

static tree
list_hash_lookup (hashcode, via_public, via_protected, via_virtual,
		  purpose, value, chain)
     int hashcode, via_public, via_virtual, via_protected;
     tree purpose, value, chain;
{
  register struct list_hash *h;

  for (h = list_hash_table[hashcode % TYPE_HASH_SIZE]; h; h = h->next)
    if (h->hashcode == hashcode
	&& TREE_VIA_VIRTUAL (h->list) == via_virtual
	&& TREE_VIA_PUBLIC (h->list) == via_public
	&& TREE_VIA_PROTECTED (h->list) == via_protected
	&& TREE_PURPOSE (h->list) == purpose
	&& TREE_VALUE (h->list) == value
	&& TREE_CHAIN (h->list) == chain)
      return h->list;
  return 0;
}

/* Add an entry to the list-hash-table
   for a list TYPE whose hash code is HASHCODE.  */

static void
list_hash_add (hashcode, list)
     int hashcode;
     tree list;
{
  register struct list_hash *h;

  h = (struct list_hash *) obstack_alloc (&class_obstack, sizeof (struct list_hash));
  h->hashcode = hashcode;
  h->list = list;
  h->next = list_hash_table[hashcode % TYPE_HASH_SIZE];
  list_hash_table[hashcode % TYPE_HASH_SIZE] = h;
}

/* Given TYPE, and HASHCODE its hash code, return the canonical
   object for an identical list if one already exists.
   Otherwise, return TYPE, and record it as the canonical object
   if it is a permanent object.

   To use this function, first create a list of the sort you want.
   Then compute its hash code from the fields of the list that
   make it different from other similar lists.
   Then call this function and use the value.
   This function frees the list you pass in if it is a duplicate.  */

/* Set to 1 to debug without canonicalization.  Never set by program.  */

static int debug_no_list_hash = 0;

tree
hash_tree_cons (via_public, via_virtual, via_protected, purpose, value, chain)
     int via_public, via_virtual, via_protected;
     tree purpose, value, chain;
{
  struct obstack *ambient_obstack = current_obstack;
  tree t;
  int hashcode = 0;

  if (! debug_no_list_hash)
    {
      hashcode = list_hash (purpose, value, chain);
      t = list_hash_lookup (hashcode, via_public, via_protected, via_virtual,
			    purpose, value, chain);
      if (t)
	return t;
    }

  current_obstack = &class_obstack;

  t = tree_cons (purpose, value, chain);
  TREE_VIA_PUBLIC (t) = via_public;
  TREE_VIA_PROTECTED (t) = via_protected;
  TREE_VIA_VIRTUAL (t) = via_virtual;

  /* If this is a new list, record it for later reuse.  */
  if (! debug_no_list_hash)
    list_hash_add (hashcode, t);

  current_obstack = ambient_obstack;
  return t;
}

/* Constructor for hashed lists.  */

tree
hash_tree_chain (value, chain)
     tree value, chain;
{
  return hash_tree_cons (0, 0, 0, NULL_TREE, value, chain);
}

/* Similar, but used for concatenating two lists.  */

tree
hash_chainon (list1, list2)
     tree list1, list2;
{
  if (list2 == 0)
    return list1;
  if (list1 == 0)
    return list2;
  if (TREE_CHAIN (list1) == NULL_TREE)
    return hash_tree_chain (TREE_VALUE (list1), list2);
  return hash_tree_chain (TREE_VALUE (list1),
			  hash_chainon (TREE_CHAIN (list1), list2));
}

static tree
get_identifier_list (value)
     tree value;
{
  tree list = IDENTIFIER_AS_LIST (value);
  if (list != NULL_TREE
      && (TREE_CODE (list) != TREE_LIST
	  || TREE_VALUE (list) != value))
    list = NULL_TREE;
  else if (IDENTIFIER_HAS_TYPE_VALUE (value)
	   && TREE_CODE (IDENTIFIER_TYPE_VALUE (value)) == RECORD_TYPE
	   && IDENTIFIER_TYPE_VALUE (value)
	      == TYPE_MAIN_VARIANT (IDENTIFIER_TYPE_VALUE (value)))
    {
      tree type = IDENTIFIER_TYPE_VALUE (value);

      if (TYPE_PTRMEMFUNC_P (type))
	list = NULL_TREE;
      else if (type == current_class_type)
	/* Don't mess up the constructor name.  */
	list = tree_cons (NULL_TREE, value, NULL_TREE);
      else
	{
	  if (! CLASSTYPE_ID_AS_LIST (type))
	    CLASSTYPE_ID_AS_LIST (type)
	      = perm_tree_cons (NULL_TREE, TYPE_IDENTIFIER (type), NULL_TREE);
	  list = CLASSTYPE_ID_AS_LIST (type);
	}
    }
  return list;
}

tree
get_decl_list (value)
     tree value;
{
  tree list = NULL_TREE;

  if (TREE_CODE (value) == IDENTIFIER_NODE)
    list = get_identifier_list (value);
  else if (TREE_CODE (value) == RECORD_TYPE
	   && TYPE_LANG_SPECIFIC (value)
	   && value == TYPE_MAIN_VARIANT (value))
    list = CLASSTYPE_AS_LIST (value);

  if (list != NULL_TREE)
    {
      my_friendly_assert (TREE_CHAIN (list) == NULL_TREE, 301);
      return list;
    }

#ifdef OBJCPLUS
  return build_tree_list (NULL_TREE, value);
#else
  return build_decl_list (NULL_TREE, value);
#endif
}

/* Build an association between TYPE and some parameters:

   OFFSET is the offset added to `this' to convert it to a pointer
   of type `TYPE *'

   BINFO is the base binfo to use, if we are deriving from one.  This
   is necessary, as we want specialized parent binfos from base
   classes, so that the VTABLE_NAMEs of bases are for the most derived
   type, instead of the simple type.

   VTABLE is the virtual function table with which to initialize
   sub-objects of type TYPE.

   VIRTUALS are the virtual functions sitting in VTABLE.

   CHAIN are more associations we must retain.  */

tree
make_binfo (offset, binfo, vtable, virtuals, chain)
     tree offset, binfo;
     tree vtable, virtuals;
     tree chain;
{
  tree new_binfo = make_tree_vec (7);
  tree type;

  if (TREE_CODE (binfo) == TREE_VEC)
    type = BINFO_TYPE (binfo);
  else
    {
      type = binfo;
      binfo = TYPE_BINFO (binfo);
    }

  TREE_CHAIN (new_binfo) = chain;
  if (chain)
    TREE_USED (new_binfo) = TREE_USED (chain);

  TREE_TYPE (new_binfo) = TYPE_MAIN_VARIANT (type);
  BINFO_OFFSET (new_binfo) = offset;
  BINFO_VTABLE (new_binfo) = vtable;
  BINFO_VIRTUALS (new_binfo) = virtuals;
  BINFO_VPTR_FIELD (new_binfo) = NULL_TREE;

  if (binfo && BINFO_BASETYPES (binfo) != NULL_TREE)
    BINFO_BASETYPES (new_binfo) = copy_node (BINFO_BASETYPES (binfo));      
  return new_binfo;
}

/* Return the binfo value for ELEM in TYPE.  */

tree
binfo_value (elem, type)
     tree elem;
     tree type;
{
  if (get_base_distance (elem, type, 0, (tree *)0) == -2)
    compiler_error ("base class `%s' ambiguous in binfo_value",
		    TYPE_NAME_STRING (elem));
  if (elem == type)
    return TYPE_BINFO (type);
  if (TREE_CODE (elem) == RECORD_TYPE && TYPE_BINFO (elem) == type)
    return type;
  return get_binfo (elem, type, 0);
}

tree
reverse_path (path)
     tree path;
{
  register tree prev = 0, tmp, next;
  for (tmp = path; tmp; tmp = next)
    {
      next = BINFO_INHERITANCE_CHAIN (tmp);
      BINFO_INHERITANCE_CHAIN (tmp) = prev;
      prev = tmp;
    }
  return prev;
}

void
debug_binfo (elem)
     tree elem;
{
  unsigned HOST_WIDE_INT n;
  tree virtuals;

  fprintf (stderr, "type \"%s\"; offset = %ld\n",
	   TYPE_NAME_STRING (BINFO_TYPE (elem)),
	   (long) TREE_INT_CST_LOW (BINFO_OFFSET (elem)));
  fprintf (stderr, "vtable type:\n");
  debug_tree (BINFO_TYPE (elem));
  if (BINFO_VTABLE (elem))
    fprintf (stderr, "vtable decl \"%s\"\n", IDENTIFIER_POINTER (DECL_NAME (BINFO_VTABLE (elem))));
  else
    fprintf (stderr, "no vtable decl yet\n");
  fprintf (stderr, "virtuals:\n");
  virtuals = BINFO_VIRTUALS (elem);

  n = skip_rtti_stuff (&virtuals);

  while (virtuals)
    {
      tree fndecl = TREE_OPERAND (FNADDR_FROM_VTABLE_ENTRY (TREE_VALUE (virtuals)), 0);
      fprintf (stderr, "%s [%ld =? %ld]\n",
	       IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (fndecl)),
	       (long) n, (long) TREE_INT_CST_LOW (DECL_VINDEX (fndecl)));
      ++n;
      virtuals = TREE_CHAIN (virtuals);
    }
}

/* Initialize an CPLUS_BINDING node that does not live on an obstack. */

tree
binding_init (node)
     struct tree_binding* node;
{
  static struct tree_binding* source;
  if (!source)
    {
      extern struct obstack permanent_obstack;
      push_obstacks (&permanent_obstack, &permanent_obstack);
      source = (struct tree_binding*)make_node (CPLUS_BINDING);
      pop_obstacks ();
    }
  *node = *source;
  TREE_PERMANENT ((tree)node) = 0;
  return (tree)node;
}

int
count_functions (t)
     tree t;
{
  int i;
  if (TREE_CODE (t) == FUNCTION_DECL)
    return 1;
  else if (TREE_CODE (t) == OVERLOAD)
    {
      for (i=0; t; t = OVL_CHAIN (t))
	i++;
      return i;
    }

  my_friendly_abort (359);
  return 0;
}

int
is_overloaded_fn (x)
     tree x;
{
  /* XXX A baselink is also considered an overloaded function.
     As is a placeholder from push_class_decls.  */
  if (TREE_CODE (x) == TREE_LIST)
    {
      my_friendly_assert (TREE_CODE (TREE_PURPOSE (x)) == TREE_VEC
			  || TREE_CODE (TREE_PURPOSE (x)) == IDENTIFIER_NODE,
			  388);
      x = TREE_VALUE (x);
    }
  return (TREE_CODE (x) == FUNCTION_DECL
	  || TREE_CODE (x) == TEMPLATE_ID_EXPR
	  || DECL_FUNCTION_TEMPLATE_P (x)
	  || TREE_CODE (x) == OVERLOAD);
}

int
really_overloaded_fn (x)
     tree x;
{     
  /* A baselink is also considered an overloaded function.
     This might also be an ambiguous class member. */
  if (TREE_CODE (x) == TREE_LIST)
    x = TREE_VALUE (x);
  return (TREE_CODE (x) == OVERLOAD 
	  && (TREE_CHAIN (x) != NULL_TREE
	      || DECL_FUNCTION_TEMPLATE_P (OVL_FUNCTION (x))));
}

tree
get_first_fn (from)
     tree from;
{
  my_friendly_assert (is_overloaded_fn (from), 9);
  /* A baselink is also considered an overloaded function. */
  if (TREE_CODE (from) == TREE_LIST)
    from = TREE_VALUE (from);
  return OVL_CURRENT (from);
}

/* Return a new OVL node, concatenating it with the old one. */

tree
ovl_cons (decl, chain)
     tree decl;
     tree chain;
{
  tree result = make_node (OVERLOAD);
  TREE_TYPE (result) = unknown_type_node;
  OVL_FUNCTION (result) = decl;
  TREE_CHAIN (result) = chain;
  
  return result;
}

/* Same as ovl_cons, but on the scratch_obstack. */

tree
scratch_ovl_cons (value, chain)
     tree value, chain;
{
  register tree node;
  register struct obstack *ambient_obstack = current_obstack;
  extern struct obstack *expression_obstack;
  current_obstack = expression_obstack;
  node = ovl_cons (value, chain);
  current_obstack = ambient_obstack;
  return node;
}

/* Build a new overloaded function. If this is the first one,
   just return it; otherwise, ovl_cons the _DECLs */

tree
build_overload (decl, chain)
     tree decl;
     tree chain;
{
  if (!chain)
    return decl;
  if (TREE_CODE (chain) != OVERLOAD)
    chain = ovl_cons (chain, NULL_TREE);
  return ovl_cons (decl, chain);
}

/* Returns true iff functions are equivalent. Equivalent functions are
   not identical only if one is a function-local extern function.
   This assumes that function-locals don't have TREE_PERMANENT.  */

static int
equal_functions (fn1, fn2)
     tree fn1;
     tree fn2;
{
  if (!TREE_PERMANENT (fn1) || !TREE_PERMANENT (fn2))
    return decls_match (fn1, fn2);
  return fn1 == fn2;
}

/* True if fn is in ovl. */

int
ovl_member (fn, ovl)
     tree fn;
     tree ovl;
{
  if (ovl == NULL_TREE)
    return 0;
  if (TREE_CODE (ovl) != OVERLOAD)
    return equal_functions (ovl, fn);
  for (; ovl; ovl = OVL_CHAIN (ovl))
    if (equal_functions (OVL_FUNCTION (ovl), fn))
      return 1;
  return 0;
}

int
is_aggr_type_2 (t1, t2)
     tree t1, t2;
{
  if (TREE_CODE (t1) != TREE_CODE (t2))
    return 0;
  return IS_AGGR_TYPE (t1) && IS_AGGR_TYPE (t2);
}

#define PRINT_RING_SIZE 4

char *
lang_printable_name (decl, v)
     tree decl;
     int v;
{
  static tree decl_ring[PRINT_RING_SIZE];
  static char *print_ring[PRINT_RING_SIZE];
  static int ring_counter;
  int i;

  /* Only cache functions.  */
  if (v < 2
      || TREE_CODE (decl) != FUNCTION_DECL
      || DECL_LANG_SPECIFIC (decl) == 0)
    return lang_decl_name (decl, v);

  /* See if this print name is lying around.  */
  for (i = 0; i < PRINT_RING_SIZE; i++)
    if (decl_ring[i] == decl)
      /* yes, so return it.  */
      return print_ring[i];

  if (++ring_counter == PRINT_RING_SIZE)
    ring_counter = 0;

  if (current_function_decl != NULL_TREE)
    {
      if (decl_ring[ring_counter] == current_function_decl)
	ring_counter += 1;
      if (ring_counter == PRINT_RING_SIZE)
	ring_counter = 0;
      if (decl_ring[ring_counter] == current_function_decl)
	my_friendly_abort (106);
    }

  if (print_ring[ring_counter])
    free (print_ring[ring_counter]);

  print_ring[ring_counter] = xstrdup (lang_decl_name (decl, v));
  decl_ring[ring_counter] = decl;
  return print_ring[ring_counter];
}

/* Build the FUNCTION_TYPE or METHOD_TYPE which may throw exceptions
   listed in RAISES.  */

tree
build_exception_variant (type, raises)
     tree type;
     tree raises;
{
  tree v = TYPE_MAIN_VARIANT (type);
  int constp = TYPE_READONLY (type);
  int volatilep = TYPE_VOLATILE (type);

  for (; v; v = TYPE_NEXT_VARIANT (v))
    {
      if (TYPE_READONLY (v) != constp
	  || TYPE_VOLATILE (v) != volatilep)
	continue;

      /* @@ This should do set equality, not exact match.  */
      if (simple_cst_list_equal (TYPE_RAISES_EXCEPTIONS (v), raises))
	/* List of exceptions raised matches previously found list.

	   @@ Nice to free up storage used in consing up the
	   @@ list of exceptions raised.  */
	return v;
    }

  /* Need to build a new variant.  */
  v = build_type_copy (type);

  if (raises && ! TREE_PERMANENT (raises))
    {
      push_obstacks_nochange ();
      end_temporary_allocation ();
      raises = copy_list (raises);
      pop_obstacks ();
    }

  TYPE_RAISES_EXCEPTIONS (v) = raises;
  return v;
}

/* Given a TEMPLATE_TEMPLATE_PARM node T, create a new one together with its 
   lang_specific field and its corresponding TEMPLATE_DECL node */

tree
copy_template_template_parm (t)
     tree t;
{
  tree template = TYPE_NAME (t);
  tree t2 = make_lang_type (TEMPLATE_TEMPLATE_PARM);
  template = copy_node (template);
  copy_lang_decl (template);
  TREE_TYPE (template) = t2;
  TYPE_NAME (t2) = template;
  TYPE_STUB_DECL (t2) = template;

  /* No need to copy these */
  TYPE_FIELDS (t2) = TYPE_FIELDS (t);
  CLASSTYPE_TEMPLATE_INFO (t2) = CLASSTYPE_TEMPLATE_INFO (t);
  return t2;
}

/* Subroutine of copy_to_permanent

   Assuming T is a node build bottom-up, make it all exist on
   permanent obstack, if it is not permanent already.  */

tree
mapcar (t, func)
     tree t;
     tree (*func) PROTO((tree));
{
  tree tmp;

  if (t == NULL_TREE)
    return t;

  if (tmp = func (t), tmp != NULL_TREE)
    return tmp;

  switch (TREE_CODE (t))
    {
    case ERROR_MARK:
      return error_mark_node;

    case VAR_DECL:
    case FUNCTION_DECL:
    case CONST_DECL:
      /* Rather than aborting, return error_mark_node.  This allows us
	 to report a sensible error message on code like this:

	 void g() { int i; f<i>(7); } 

         In a case like:

           void g() { const int i = 7; f<i>(7); }

	 however, we must actually return the constant initializer.  */
      tmp = decl_constant_value (t);
      if (tmp != t)
	return mapcar (tmp, func);
      else
	return error_mark_node;

    case PARM_DECL:
      {
	tree chain = TREE_CHAIN (t);
	t = copy_node (t);
	TREE_CHAIN (t) = mapcar (chain, func);
	TREE_TYPE (t) = mapcar (TREE_TYPE (t), func);
	DECL_INITIAL (t) = mapcar (DECL_INITIAL (t), func);
	DECL_SIZE (t) = mapcar (DECL_SIZE (t), func);
	return t;
      }

    case TREE_LIST:
      {
	tree chain = TREE_CHAIN (t);
	t = copy_node (t);
	TREE_PURPOSE (t) = mapcar (TREE_PURPOSE (t), func);
	TREE_VALUE (t) = mapcar (TREE_VALUE (t), func);
	TREE_CHAIN (t) = mapcar (chain, func);
	return t;
      }

    case OVERLOAD:
      {
	tree chain = OVL_CHAIN (t);
	t = copy_node (t);
	OVL_FUNCTION (t) = mapcar (OVL_FUNCTION (t), func);
	OVL_CHAIN (t) = mapcar (chain, func);
	return t;
      }

    case TREE_VEC:
      {
	int len = TREE_VEC_LENGTH (t);

	t = copy_node (t);
	while (len--)
	  TREE_VEC_ELT (t, len) = mapcar (TREE_VEC_ELT (t, len), func);
	return t;
      }

    case INTEGER_CST:
    case REAL_CST:
    case STRING_CST:
      return copy_node (t);

    case COND_EXPR:
    case TARGET_EXPR:
    case AGGR_INIT_EXPR:
      t = copy_node (t);
      TREE_OPERAND (t, 0) = mapcar (TREE_OPERAND (t, 0), func);
      TREE_OPERAND (t, 1) = mapcar (TREE_OPERAND (t, 1), func);
      TREE_OPERAND (t, 2) = mapcar (TREE_OPERAND (t, 2), func);
      return t;

    case SAVE_EXPR:
      t = copy_node (t);
      TREE_OPERAND (t, 0) = mapcar (TREE_OPERAND (t, 0), func);
      return t;

    case MODIFY_EXPR:
    case PLUS_EXPR:
    case MINUS_EXPR:
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case TRUNC_MOD_EXPR:
    case MIN_EXPR:
    case MAX_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
    case BIT_AND_EXPR:
    case BIT_ANDTC_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
    case COMPOUND_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case ARRAY_REF:
    case SCOPE_REF:
    case TRY_CATCH_EXPR:
    case WITH_CLEANUP_EXPR:
      t = copy_node (t);
      TREE_OPERAND (t, 0) = mapcar (TREE_OPERAND (t, 0), func);
      TREE_OPERAND (t, 1) = mapcar (TREE_OPERAND (t, 1), func);
      return t;

    case CALL_EXPR:
      t = copy_node (t);
      TREE_TYPE (t) = mapcar (TREE_TYPE (t), func);
      TREE_OPERAND (t, 0) = mapcar (TREE_OPERAND (t, 0), func);
      TREE_OPERAND (t, 1) = mapcar (TREE_OPERAND (t, 1), func);

      /* tree.def says that operand two is RTL, but
	 build_call_declarator puts trees in there.  */
      if (TREE_OPERAND (t, 2)
	  && TREE_CODE (TREE_OPERAND (t, 2)) == TREE_LIST)
	TREE_OPERAND (t, 2) = mapcar (TREE_OPERAND (t, 2), func);
      else
	TREE_OPERAND (t, 2) = NULL_TREE;
      return t;

    case CONVERT_EXPR:
    case ADDR_EXPR:
    case INDIRECT_REF:
    case NEGATE_EXPR:
    case BIT_NOT_EXPR:
    case TRUTH_NOT_EXPR:
    case NOP_EXPR:
    case COMPONENT_REF:
    case CLEANUP_POINT_EXPR:
      t = copy_node (t);
      TREE_OPERAND (t, 0) = mapcar (TREE_OPERAND (t, 0), func);
      return t;

    case POINTER_TYPE:
      tmp = build_pointer_type (mapcar (TREE_TYPE (t), func));
      return cp_build_type_variant (tmp, TYPE_READONLY (t), TYPE_VOLATILE (t));
    case REFERENCE_TYPE:
      tmp = build_reference_type (mapcar (TREE_TYPE (t), func));
      return cp_build_type_variant (tmp, TYPE_READONLY (t), TYPE_VOLATILE (t));
    case FUNCTION_TYPE:
      tmp = build_function_type (mapcar (TREE_TYPE (t), func),
				 mapcar (TYPE_ARG_TYPES (t), func));
      return cp_build_type_variant (tmp, TYPE_READONLY (t), TYPE_VOLATILE (t));
    case ARRAY_TYPE:
      tmp = build_cplus_array_type (mapcar (TREE_TYPE (t), func),
				    mapcar (TYPE_DOMAIN (t), func));
      return cp_build_type_variant (tmp, TYPE_READONLY (t), TYPE_VOLATILE (t));
    case INTEGER_TYPE:
      tmp = build_index_type (mapcar (TYPE_MAX_VALUE (t), func));
      return cp_build_type_variant (tmp, TYPE_READONLY (t), TYPE_VOLATILE (t));
    case OFFSET_TYPE:
      tmp = build_offset_type (mapcar (TYPE_OFFSET_BASETYPE (t), func),
			       mapcar (TREE_TYPE (t), func));
      return cp_build_type_variant (tmp, TYPE_READONLY (t), TYPE_VOLATILE (t));
    case METHOD_TYPE:
      tmp = build_cplus_method_type
	(mapcar (TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (t))), func),
	 mapcar (TREE_TYPE (t), func),
	 mapcar (TREE_CHAIN (TYPE_ARG_TYPES (t)), func));
      return cp_build_type_variant (tmp, TYPE_READONLY (t), TYPE_VOLATILE (t));

    case COMPLEX_CST:
      t = copy_node (t);
      TREE_REALPART (t) = mapcar (TREE_REALPART (t), func);
      /* This line seems to be incorrect.  */
      TREE_IMAGPART (t) = mapcar (TREE_REALPART (t), func);
      return t;

    case CONSTRUCTOR:
      t = copy_node (t);
      CONSTRUCTOR_ELTS (t) = mapcar (CONSTRUCTOR_ELTS (t), func);
      return t;

    case TEMPLATE_TEMPLATE_PARM:
      return copy_template_template_parm (t);

    case BIND_EXPR:
      t = copy_node (t);
      TREE_OPERAND (t, 0) = mapcar (TREE_OPERAND (t, 0), func);
      TREE_OPERAND (t, 1) = mapcar (TREE_OPERAND (t, 1), func);
      TREE_OPERAND (t, 2) = NULL_TREE;
      return t;

    case NEW_EXPR:
      t = copy_node (t);
      TREE_OPERAND (t, 0) = mapcar (TREE_OPERAND (t, 0), func);
      TREE_OPERAND (t, 1) = mapcar (TREE_OPERAND (t, 1), func);
      TREE_OPERAND (t, 2) = mapcar (TREE_OPERAND (t, 2), func);
      return t;

    case VECTOR_CST:
      t = copy_node (t);
      TREE_VECTOR_CST_LOW (t) = mapcar (TREE_VECTOR_CST_LOW (t), func);
      TREE_VECTOR_CST_HIGH (t) = mapcar (TREE_VECTOR_CST_HIGH (t), func);
      return t;

    case RECORD_TYPE:
      if (TYPE_PTRMEMFUNC_P (t))
	return build_ptrmemfunc_type
	  (mapcar (TYPE_PTRMEMFUNC_FN_TYPE (t), func));
      /* else fall through */
      
      /*  This list is incomplete, but should suffice for now.
	  It is very important that `sorry' not call
	  `report_error_function'.  That could cause an infinite loop.  */
    default:
      sorry ("initializer contains unrecognized tree code");
      return error_mark_node;

    }
  my_friendly_abort (107);
  /* NOTREACHED */
  return NULL_TREE;
}

static tree
perm_manip (t)
     tree t;
{
  if (TREE_PERMANENT (t))
    return t;

  /* Support `void f () { extern int i; A<&i> a; }' */
  if ((TREE_CODE (t) == VAR_DECL || TREE_CODE (t) == FUNCTION_DECL)
      && TREE_PUBLIC (t))
    {
      t = copy_node (t);

      /* copy_rtx won't make a new SYMBOL_REF, so call make_decl_rtl again.  */
      DECL_RTL (t) = 0;
      make_decl_rtl (t, NULL_PTR, 1);

      return t;
    }
  return NULL_TREE;
}

/* Assuming T is a node built bottom-up, make it all exist on
   permanent obstack, if it is not permanent already.  */

tree
copy_to_permanent (t)
     tree t;
{
  if (t == NULL_TREE || TREE_PERMANENT (t))
    return t;

  push_obstacks_nochange ();
  end_temporary_allocation ();

  t = mapcar (t, perm_manip);

  pop_obstacks ();

  return t;
}

#ifdef GATHER_STATISTICS
extern int depth_reached;
#endif

void
print_lang_statistics ()
{
  extern struct obstack decl_obstack;
  print_obstack_statistics ("class_obstack", &class_obstack);
  print_obstack_statistics ("decl_obstack", &decl_obstack);
  print_search_statistics ();
  print_class_statistics ();
#ifdef GATHER_STATISTICS
  fprintf (stderr, "maximum template instantiation depth reached: %d\n",
	   depth_reached);
#endif
}

/* This is used by the `assert' macro.  It is provided in libgcc.a,
   which `cc' doesn't know how to link.  Note that the C++ front-end
   no longer actually uses the `assert' macro (instead, it calls
   my_friendly_assert).  But all of the back-end files still need this.  */

void
__eprintf (string, expression, line, filename)
#ifdef __STDC__
     const char *string;
     const char *expression;
     unsigned line;
     const char *filename;
#else
     char *string;
     char *expression;
     unsigned line;
     char *filename;
#endif
{
  fprintf (stderr, string, expression, line, filename);
  fflush (stderr);
  abort ();
}

/* Return, as an INTEGER_CST node, the number of elements for TYPE
   (which is an ARRAY_TYPE).  This counts only elements of the top
   array.  */

tree
array_type_nelts_top (type)
     tree type;
{
  return fold (build (PLUS_EXPR, sizetype,
		      array_type_nelts (type),
		      integer_one_node));
}

/* Return, as an INTEGER_CST node, the number of elements for TYPE
   (which is an ARRAY_TYPE).  This one is a recursive count of all
   ARRAY_TYPEs that are clumped together.  */

tree
array_type_nelts_total (type)
     tree type;
{
  tree sz = array_type_nelts_top (type);
  type = TREE_TYPE (type);
  while (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree n = array_type_nelts_top (type);
      sz = fold (build (MULT_EXPR, sizetype, sz, n));
      type = TREE_TYPE (type);
    }
  return sz;
}

static
tree
bot_manip (t)
     tree t;
{
  if (TREE_CODE (t) != TREE_LIST && ! TREE_SIDE_EFFECTS (t))
    return t;
  else if (TREE_CODE (t) == TARGET_EXPR)
    {
      if (TREE_CODE (TREE_OPERAND (t, 1)) == AGGR_INIT_EXPR)
	{
	  mark_used (TREE_OPERAND (TREE_OPERAND (TREE_OPERAND (t, 1), 0), 0));
	  return build_cplus_new
	    (TREE_TYPE (t), break_out_target_exprs (TREE_OPERAND (t, 1)));
	}
      t = copy_node (t);
      TREE_OPERAND (t, 0) = build (VAR_DECL, TREE_TYPE (t));
      layout_decl (TREE_OPERAND (t, 0), 0);
      return t;
    }
  else if (TREE_CODE (t) == CALL_EXPR)
    mark_used (TREE_OPERAND (TREE_OPERAND (t, 0), 0));

  return NULL_TREE;
}
  
/* Actually, we'll just clean out the target exprs for the moment.  */

tree
break_out_target_exprs (t)
     tree t;
{
  return mapcar (t, bot_manip);
}

/* Obstack used for allocating nodes in template function and variable
   definitions.  */

/* Similar to `build_nt', except we build
   on the permanent_obstack, regardless.  */

tree
build_min_nt VPROTO((enum tree_code code, ...))
{
#ifndef __STDC__
  enum tree_code code;
#endif
  register struct obstack *ambient_obstack = expression_obstack;
  va_list p;
  register tree t;
  register int length;
  register int i;

  VA_START (p, code);

#ifndef __STDC__
  code = va_arg (p, enum tree_code);
#endif

  expression_obstack = &permanent_obstack;

  t = make_node (code);
  length = tree_code_length[(int) code];
  TREE_COMPLEXITY (t) = lineno;

  for (i = 0; i < length; i++)
    {
      tree x = va_arg (p, tree);
      TREE_OPERAND (t, i) = copy_to_permanent (x);
    }

  va_end (p);
  expression_obstack = ambient_obstack;
  return t;
}

/* Similar to `build', except we build
   on the permanent_obstack, regardless.  */

tree
build_min VPROTO((enum tree_code code, tree tt, ...))
{
#ifndef __STDC__
  enum tree_code code;
  tree tt;
#endif
  register struct obstack *ambient_obstack = expression_obstack;
  va_list p;
  register tree t;
  register int length;
  register int i;

  VA_START (p, tt);

#ifndef __STDC__
  code = va_arg (p, enum tree_code);
  tt = va_arg (p, tree);
#endif

  expression_obstack = &permanent_obstack;

  t = make_node (code);
  length = tree_code_length[(int) code];
  TREE_TYPE (t) = tt;
  TREE_COMPLEXITY (t) = lineno;

  for (i = 0; i < length; i++)
    {
      tree x = va_arg (p, tree);
      TREE_OPERAND (t, i) = copy_to_permanent (x);
    }

  va_end (p);
  expression_obstack = ambient_obstack;
  return t;
}

/* Same as `tree_cons' but make a permanent object.  */

tree
min_tree_cons (purpose, value, chain)
     tree purpose, value, chain;
{
  register tree node;
  register struct obstack *ambient_obstack = current_obstack;
  current_obstack = &permanent_obstack;

  node = tree_cons (copy_to_permanent (purpose),
		    copy_to_permanent (value), chain);
  current_obstack = ambient_obstack;
  return node;
}

tree
get_type_decl (t)
     tree t;
{
  if (TREE_CODE (t) == TYPE_DECL)
    return t;
  if (TREE_CODE_CLASS (TREE_CODE (t)) == 't')
    return TYPE_STUB_DECL (t);
  
  my_friendly_abort (42);

  /* Stop compiler from complaining control reaches end of non-void function.  */
  return 0;
}

int
can_free (obstack, t)
     struct obstack *obstack;
     tree t;
{
  int size = 0;

  if (TREE_CODE (t) == TREE_VEC)
    size = (TREE_VEC_LENGTH (t)-1) * sizeof (tree) + sizeof (struct tree_vec);
  else
    my_friendly_abort (42);

#define ROUND(x) ((x + obstack_alignment_mask (obstack)) \
		  & ~ obstack_alignment_mask (obstack))
  if ((char *)t + ROUND (size) == obstack_next_free (obstack))
    return 1;
#undef ROUND

  return 0;
}

/* Return first vector element whose BINFO_TYPE is ELEM.
   Return 0 if ELEM is not in VEC.  VEC may be NULL_TREE.  */

tree
vec_binfo_member (elem, vec)
     tree elem, vec;
{
  int i;

  if (vec)
    for (i = 0; i < TREE_VEC_LENGTH (vec); ++i)
      if (comptypes (elem, BINFO_TYPE (TREE_VEC_ELT (vec, i)), 1))
	return TREE_VEC_ELT (vec, i);

  return NULL_TREE;
}

/* Kludge around the fact that DECL_CONTEXT for virtual functions returns
   the wrong thing for decl_function_context.  Hopefully the uses in the
   backend won't matter, since we don't need a static chain for local class
   methods.  FIXME!  */

tree
hack_decl_function_context (decl)
     tree decl;
{
  if (TREE_CODE (decl) == FUNCTION_DECL && DECL_FUNCTION_MEMBER_P (decl))
    return decl_function_context (TYPE_MAIN_DECL (DECL_CLASS_CONTEXT (decl)));
  return decl_function_context (decl);
}

/* Return truthvalue of whether T1 is the same tree structure as T2.
   Return 1 if they are the same.
   Return 0 if they are understandably different.
   Return -1 if either contains tree structure not understood by
   this function.  */

int
cp_tree_equal (t1, t2)
     tree t1, t2;
{
  register enum tree_code code1, code2;
  int cmp;

  if (t1 == t2)
    return 1;
  if (t1 == 0 || t2 == 0)
    return 0;

  code1 = TREE_CODE (t1);
  code2 = TREE_CODE (t2);

  if (code1 == NOP_EXPR || code1 == CONVERT_EXPR || code1 == NON_LVALUE_EXPR)
    {
      if (code2 == NOP_EXPR || code2 == CONVERT_EXPR || code2 == NON_LVALUE_EXPR)
	return cp_tree_equal (TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0));
      else
	return cp_tree_equal (TREE_OPERAND (t1, 0), t2);
    }
  else if (code2 == NOP_EXPR || code2 == CONVERT_EXPR
	   || code2 == NON_LVALUE_EXPR)
    return cp_tree_equal (t1, TREE_OPERAND (t2, 0));

  if (code1 != code2)
    return 0;

  switch (code1)
    {
    case INTEGER_CST:
      return TREE_INT_CST_LOW (t1) == TREE_INT_CST_LOW (t2)
	&& TREE_INT_CST_HIGH (t1) == TREE_INT_CST_HIGH (t2);

    case REAL_CST:
      return REAL_VALUES_EQUAL (TREE_REAL_CST (t1), TREE_REAL_CST (t2));

    case STRING_CST:
      return TREE_STRING_LENGTH (t1) == TREE_STRING_LENGTH (t2)
	&& !bcmp (TREE_STRING_POINTER (t1), TREE_STRING_POINTER (t2),
		  TREE_STRING_LENGTH (t1));

    case CONSTRUCTOR:
      /* We need to do this when determining whether or not two
	 non-type pointer to member function template arguments
	 are the same.  */
      if (!(comptypes (TREE_TYPE (t1), TREE_TYPE (t2), 1)
	    /* The first operand is RTL.  */
	    && TREE_OPERAND (t1, 0) == TREE_OPERAND (t2, 0)))
	return 0;
      return cp_tree_equal (TREE_OPERAND (t1, 1), TREE_OPERAND (t2, 1));

    case TREE_LIST:
      cmp = cp_tree_equal (TREE_PURPOSE (t1), TREE_PURPOSE (t2));
      if (cmp <= 0)
	return cmp;
      cmp = cp_tree_equal (TREE_VALUE (t1), TREE_VALUE (t2));
      if (cmp <= 0)
	return cmp;
      return cp_tree_equal (TREE_CHAIN (t1), TREE_CHAIN (t2));

    case SAVE_EXPR:
      return cp_tree_equal (TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0));

    case CALL_EXPR:
      cmp = cp_tree_equal (TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0));
      if (cmp <= 0)
	return cmp;
      return simple_cst_list_equal (TREE_OPERAND (t1, 1), TREE_OPERAND (t2, 1));

    case TARGET_EXPR:
      /* Special case: if either target is an unallocated VAR_DECL,
	 it means that it's going to be unified with whatever the
	 TARGET_EXPR is really supposed to initialize, so treat it
	 as being equivalent to anything.  */
      if ((TREE_CODE (TREE_OPERAND (t1, 0)) == VAR_DECL
	   && DECL_NAME (TREE_OPERAND (t1, 0)) == NULL_TREE
	   && DECL_RTL (TREE_OPERAND (t1, 0)) == 0)
	  || (TREE_CODE (TREE_OPERAND (t2, 0)) == VAR_DECL
	      && DECL_NAME (TREE_OPERAND (t2, 0)) == NULL_TREE
	      && DECL_RTL (TREE_OPERAND (t2, 0)) == 0))
	cmp = 1;
      else
	cmp = cp_tree_equal (TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0));
      if (cmp <= 0)
	return cmp;
      return cp_tree_equal (TREE_OPERAND (t1, 1), TREE_OPERAND (t2, 1));

    case WITH_CLEANUP_EXPR:
      cmp = cp_tree_equal (TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0));
      if (cmp <= 0)
	return cmp;
      return cp_tree_equal (TREE_OPERAND (t1, 2), TREE_OPERAND (t1, 2));

    case COMPONENT_REF:
      if (TREE_OPERAND (t1, 1) == TREE_OPERAND (t2, 1))
	return cp_tree_equal (TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0));
      return 0;

    case VAR_DECL:
    case PARM_DECL:
    case CONST_DECL:
    case FUNCTION_DECL:
      return 0;

    case TEMPLATE_PARM_INDEX:
      return TEMPLATE_PARM_IDX (t1) == TEMPLATE_PARM_IDX (t2)
	&& TEMPLATE_PARM_LEVEL (t1) == TEMPLATE_PARM_LEVEL (t2);

    case SIZEOF_EXPR:
    case ALIGNOF_EXPR:
      if (TREE_CODE (TREE_OPERAND (t1, 0)) != TREE_CODE (TREE_OPERAND (t2, 0)))
	return 0;
      if (TREE_CODE_CLASS (TREE_CODE (TREE_OPERAND (t1, 0))) == 't')
	return comptypes (TREE_OPERAND (t1, 0), TREE_OPERAND (t2, 0), 1);
      break;

    default:
      break;
    }

  switch (TREE_CODE_CLASS (code1))
    {
      int i;
    case '1':
    case '2':
    case '<':
    case 'e':
    case 'r':
    case 's':
      cmp = 1;
      for (i=0; i<tree_code_length[(int) code1]; ++i)
	{
	  cmp = cp_tree_equal (TREE_OPERAND (t1, i), TREE_OPERAND (t2, i));
	  if (cmp <= 0)
	    return cmp;
	}
      return cmp;
    }

  return -1;
}

/* Similar to make_tree_vec, but build on a temporary obstack.  */

tree
make_temp_vec (len)
     int len;
{
  register tree node;
  register struct obstack *ambient_obstack = current_obstack;
  current_obstack = expression_obstack;
  node = make_tree_vec (len);
  current_obstack = ambient_obstack;
  return node;
}

/* Build a wrapper around some pointer PTR so we can use it as a tree.  */

tree
build_ptr_wrapper (ptr)
     void *ptr;
{
  tree t = make_node (WRAPPER);
  WRAPPER_PTR (t) = ptr;
  return t;
}

/* Same, but on the expression_obstack.  */

tree
build_expr_ptr_wrapper (ptr)
     void *ptr;
{
  tree t;
  push_expression_obstack ();
  t = build_ptr_wrapper (ptr);
  pop_obstacks ();
  return t;
}

/* Build a wrapper around some integer I so we can use it as a tree.  */

tree
build_int_wrapper (i)
     int i;
{
  tree t = make_node (WRAPPER);
  WRAPPER_INT (t) = i;
  return t;
}

tree
build_srcloc (file, line)
     char *file;
     int line;
{
  tree t;

  /* Make sure that we put these on the permanent obstack; up in
     add_pending_template, we pass this return value into perm_tree_cons,
     which also puts it on the permanent_obstack.  However, this wasn't
     explicitly doing the same.  */
  register struct obstack *ambient_obstack = current_obstack;
  current_obstack = &permanent_obstack;

  t = make_node (SRCLOC);
  SRCLOC_FILE (t) = file;
  SRCLOC_LINE (t) = line;

  current_obstack = ambient_obstack;

  return t;
}

tree
build_srcloc_here ()
{
  return build_srcloc (input_filename, lineno);
}

void
push_expression_obstack ()
{
  push_obstacks_nochange ();
  current_obstack = expression_obstack;
}

/* The type of ARG when used as an lvalue.  */

tree
lvalue_type (arg)
     tree arg;
{
  tree type = TREE_TYPE (arg);
  if (TREE_CODE (arg) == OVERLOAD)
    type = unknown_type_node;
  return cp_build_type_variant
    (type, TREE_READONLY (arg), TREE_THIS_VOLATILE (arg));
}

/* The type of ARG for printing error messages; denote lvalues with
   reference types.  */

tree
error_type (arg)
     tree arg;
{
  tree type = TREE_TYPE (arg);
  if (TREE_CODE (type) == ARRAY_TYPE)
    ;
  else if (real_lvalue_p (arg))
    type = build_reference_type (lvalue_type (arg));
  else if (IS_AGGR_TYPE (type))
    type = lvalue_type (arg);

  return type;
}

/* Does FUNCTION use a variable-length argument list?  */

int
varargs_function_p (function)
     tree function;
{
  tree parm = TYPE_ARG_TYPES (TREE_TYPE (function));
  for (; parm; parm = TREE_CHAIN (parm))
    if (TREE_VALUE (parm) == void_type_node)
      return 0;
  return 1;
}

/* Returns 1 if decl is a member of a class.  */

int
member_p (decl)
     tree decl;
{
  tree ctx = DECL_CONTEXT (decl);
  return (ctx && TREE_CODE_CLASS (TREE_CODE (ctx)) == 't');
}
