/* longjmp-free interface between gdb and gdbtk.
   Copyright (C) 1999-2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "value.h"
#include "gdbtk-wrapper.h"

/*
 * Wrapper functions exported to the world
 */

gdb_result GDB_value_fetch_lazy PARAMS ((value_ptr));

gdb_result GDB_evaluate_expression PARAMS ((struct expression *, value_ptr *));

gdb_result GDB_type_print PARAMS ((value_ptr, char *, struct ui_file *, int));

gdb_result GDB_val_print PARAMS ((struct type * type, char *valaddr,
				  CORE_ADDR address, struct ui_file * stream,
				  int format, int deref_ref, int recurse,
				  enum val_prettyprint pretty));

gdb_result GDB_select_frame PARAMS ((struct frame_info *, int));

gdb_result GDB_value_equal PARAMS ((value_ptr, value_ptr, int *));

gdb_result GDB_parse_exp_1 PARAMS ((char **stringptr, struct block * block, int comma,
				    struct expression ** result));

gdb_result GDB_evaluate_type PARAMS ((struct expression * exp, value_ptr * result));

gdb_result GDB_block_for_pc PARAMS ((CORE_ADDR pc, struct block ** result));

gdb_result GDB_block_innermost_frame PARAMS ((struct block * block,
					      struct frame_info ** result));

gdb_result GDB_reinit_frame_cache PARAMS ((void));

gdb_result GDB_find_frame_addr_in_frame_chain PARAMS ((CORE_ADDR addr,
					      struct frame_info ** result));

gdb_result GDB_value_ind PARAMS ((value_ptr val, value_ptr * rval));

gdb_result GDB_value_slice PARAMS ((value_ptr val, int low, int num,
				    value_ptr * rval));

gdb_result GDB_value_coerce_array PARAMS ((value_ptr val, value_ptr * rval));

gdb_result GDB_value_struct_elt PARAMS ((value_ptr * argp, value_ptr * args,
					 char *name, int *static_memfunc,
					 char *err, value_ptr * rval));

gdb_result GDB_value_cast PARAMS ((struct type * type, value_ptr val, value_ptr * rval));

gdb_result GDB_get_frame_block PARAMS ((struct frame_info * fi, struct block ** rval));

gdb_result GDB_get_prev_frame PARAMS ((struct frame_info *fi,
				       struct frame_info **result));

gdb_result GDB_get_next_frame PARAMS ((struct frame_info *fi,
				       struct frame_info **result));

gdb_result GDB_find_relative_frame PARAMS ((struct frame_info *fi,
					    int *start,
					    struct frame_info **result));

gdb_result GDB_get_current_frame PARAMS ((struct frame_info **result));

/*
 * Private functions for this file
 */
static gdb_result call_wrapped_function PARAMS ((catch_errors_ftype *,
					   struct gdb_wrapper_arguments *));

static int wrap_type_print PARAMS ((char *));

static int wrap_evaluate_expression PARAMS ((char *));

static int wrap_value_fetch_lazy PARAMS ((char *));

static int wrap_val_print PARAMS ((char *));

static int wrap_select_frame PARAMS ((char *));

static int wrap_value_equal PARAMS ((char *));

static int wrap_parse_exp_1 PARAMS ((char *opaque_arg));

static int wrap_evaluate_type PARAMS ((char *opaque_arg));

static int wrap_block_for_pc PARAMS ((char *opaque_arg));

static int wrap_block_innermost_frame PARAMS ((char *opaque_arg));

static int wrap_reinit_frame_cache PARAMS ((char *opaque_arg));

static int wrap_find_frame_addr_in_frame_chain PARAMS ((char *opaque_arg));

static int wrap_value_ind PARAMS ((char *opaque_arg));

static int wrap_value_slice PARAMS ((char *opaque_arg));

static int wrap_value_coerce_array PARAMS ((char *opaque_arg));

static int wrap_value_struct_elt PARAMS ((char *opaque_arg));

static int wrap_value_cast PARAMS ((char *opaque_arg));

static int wrap_get_frame_block PARAMS ((char *opaque_arg));

static int wrap_get_prev_frame PARAMS ((char *opaque_arg));

static int wrap_get_next_frame PARAMS ((char *opaque_arg));

static int wrap_find_relative_frame PARAMS ((char *opaque_arg));

static int wrap_get_current_frame PARAMS ((char *opaque_arg));

static gdb_result
call_wrapped_function (fn, arg)
     catch_errors_ftype *fn;
     struct gdb_wrapper_arguments *arg;
{
  if (!catch_errors (fn, (char *) &arg, "", RETURN_MASK_ERROR))
    {
      /* An error occurred */
      return GDB_ERROR;
    }

  return GDB_OK;
}

gdb_result
GDB_type_print (val, varstring, stream, show)
     value_ptr val;
     char *varstring;
     struct ui_file *stream;
     int show;
{
  struct gdb_wrapper_arguments args;

  args.args[0] = (char *) val;
  args.args[1] = varstring;
  args.args[2] = (char *) stream;
  args.args[3] = (char *) show;
  return call_wrapped_function ((catch_errors_ftype *) wrap_type_print, &args);
}

static int
wrap_type_print (a)
     char *a;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) a;
  value_ptr val = (value_ptr) (*args)->args[0];
  char *varstring = (*args)->args[1];
  struct ui_file *stream = (struct ui_file *) (*args)->args[2];
  int show = (int) (*args)->args[3];
  type_print (VALUE_TYPE (val), varstring, stream, show);
  return 1;
}

gdb_result
GDB_val_print (type, valaddr, address, stream, format, deref_ref,
	       recurse, pretty)
     struct type *type;
     char *valaddr;
     CORE_ADDR address;
     struct ui_file *stream;
     int format;
     int deref_ref;
     int recurse;
     enum val_prettyprint pretty;
{
  struct gdb_wrapper_arguments args;

  args.args[0] = (char *) type;
  args.args[1] = (char *) valaddr;
  args.args[2] = (char *) &address;
  args.args[3] = (char *) stream;
  args.args[4] = (char *) format;
  args.args[5] = (char *) deref_ref;
  args.args[6] = (char *) recurse;
  args.args[7] = (char *) pretty;

  return call_wrapped_function ((catch_errors_ftype *) wrap_val_print, &args);
}

static int
wrap_val_print (a)
     char *a;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) a;
  struct type *type;
  char *valaddr;
  CORE_ADDR address;
  struct ui_file *stream;
  int format;
  int deref_ref;
  int recurse;
  enum val_prettyprint pretty;

  type = (struct type *) (*args)->args[0];
  valaddr = (char *) (*args)->args[1];
  address = *(CORE_ADDR *) (*args)->args[2];
  stream = (struct ui_file *) (*args)->args[3];
  format = (int) (*args)->args[4];
  deref_ref = (int) (*args)->args[5];
  recurse = (int) (*args)->args[6];
  pretty = (enum val_prettyprint) (*args)->args[7];

  val_print (type, valaddr, 0, address, stream, format, deref_ref,
	     recurse, pretty);
  return 1;
}

gdb_result
GDB_value_fetch_lazy (value)
     value_ptr value;
{
  struct gdb_wrapper_arguments args;

  args.args[0] = (char *) value;
  return call_wrapped_function ((catch_errors_ftype *) wrap_value_fetch_lazy, &args);
}

static int
wrap_value_fetch_lazy (a)
     char *a;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) a;

  value_fetch_lazy ((value_ptr) (*args)->args[0]);
  return 1;
}

gdb_result
GDB_evaluate_expression (exp, value)
     struct expression *exp;
     value_ptr *value;
{
  struct gdb_wrapper_arguments args;
  gdb_result result;
  args.args[0] = (char *) exp;

  result = call_wrapped_function ((catch_errors_ftype *) wrap_evaluate_expression, &args);
  if (result != GDB_OK)
    return result;

  *value = (value_ptr) args.result;
  return GDB_OK;
}

static int
wrap_evaluate_expression (a)
     char *a;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) a;

  (*args)->result =
    (char *) evaluate_expression ((struct expression *) (*args)->args[0]);
  return 1;
}

gdb_result
GDB_select_frame (fi, level)
     struct frame_info *fi;
     int level;
{
  struct gdb_wrapper_arguments args;

  args.args[0] = (char *) fi;
  args.args[1] = (char *) &level;

  return call_wrapped_function ((catch_errors_ftype *) wrap_select_frame, &args);
}

static int
wrap_select_frame (a)
     char *a;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) a;
  int level = *(int *) (*args)->args[1];
  struct frame_info *fi = (struct frame_info *) (*args)->args[0];

  select_frame (fi, level);
  return 1;
}

gdb_result
GDB_value_equal (val1, val2, result)
     value_ptr val1;
     value_ptr val2;
     int *result;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) val1;
  args.args[1] = (char *) val2;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_value_equal, &args);
  if (r != GDB_OK)
    return r;

  *result = (int) args.result;
  return GDB_OK;
}

static int
wrap_value_equal (a)
     char *a;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) a;
  value_ptr val1, val2;

  val1 = (value_ptr) (*args)->args[0];
  val2 = (value_ptr) (*args)->args[1];

  (*args)->result = (char *) value_equal (val1, val2);
  return 1;
}

gdb_result
GDB_parse_exp_1 (stringptr, block, comma, result)
     char **stringptr;
     struct block *block;
     int comma;
     struct expression **result;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) stringptr;
  args.args[1] = (char *) block;
  args.args[2] = (char *) comma;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_parse_exp_1, &args);
  if (r != GDB_OK)
    return r;

  *result = (struct expression *) args.result;
  return GDB_OK;
}

static int
wrap_parse_exp_1 (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  struct block *block;
  char **stringptr;
  int comma;

  stringptr = (char **) (*args)->args[0];
  block = (struct block *) (*args)->args[1];
  comma = (int) (*args)->args[2];

  (*args)->result = (char *) parse_exp_1 (stringptr, block, comma);
  return 1;
}

gdb_result
GDB_evaluate_type (exp, result)
     struct expression *exp;
     value_ptr *result;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) exp;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_evaluate_type, &args);
  if (r != GDB_OK)
    return r;

  *result = (value_ptr) args.result;
  return GDB_OK;
}

static int
wrap_evaluate_type (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  struct expression *exp;

  exp = (struct expression *) (*args)->args[0];
  (*args)->result = (char *) evaluate_type (exp);
  return 1;
}

gdb_result
GDB_block_for_pc (pc, result)
     CORE_ADDR pc;
     struct block **result;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) &pc;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_block_for_pc, &args);
  if (r != GDB_OK)
    return r;

  *result = (struct block *) args.result;
  return GDB_OK;
}

static int
wrap_block_for_pc (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  CORE_ADDR pc;

  pc = *(CORE_ADDR *) (*args)->args[0];
  (*args)->result = (char *) block_for_pc (pc);
  return 1;
}

gdb_result
GDB_block_innermost_frame (block, result)
     struct block *block;
     struct frame_info **result;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) block;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_block_innermost_frame, &args);
  if (r != GDB_OK)
    return r;

  *result = (struct frame_info *) args.result;
  return GDB_OK;
}

static int
wrap_block_innermost_frame (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  struct block *block;

  block = (struct block *) (*args)->args[0];
  (*args)->result = (char *) block_innermost_frame (block);
  return 1;
}

gdb_result
GDB_reinit_frame_cache ()
{
  gdb_result r;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_reinit_frame_cache, NULL);
  if (r != GDB_OK)
    return r;

  return GDB_OK;
}

static int
wrap_reinit_frame_cache (opaque_arg)
     char *opaque_arg;
{
  reinit_frame_cache ();
  return 1;
}

gdb_result
GDB_find_frame_addr_in_frame_chain (addr, result)
     CORE_ADDR addr;
     struct frame_info **result;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) &addr;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_find_frame_addr_in_frame_chain, &args);
  if (r != GDB_OK)
    return r;

  *result = (struct frame_info *) args.result;
  return GDB_OK;
}

static int
wrap_find_frame_addr_in_frame_chain (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  CORE_ADDR addr;

  addr = *(CORE_ADDR *) (*args)->args[0];
  (*args)->result = (char *) find_frame_addr_in_frame_chain (addr);
  return 1;
}

gdb_result
GDB_value_ind (val, rval)
     value_ptr val;
     value_ptr *rval;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) val;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_value_ind, &args);
  if (r != GDB_OK)
    return r;

  *rval = (value_ptr) args.result;
  return GDB_OK;
}

static int
wrap_value_ind (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  value_ptr val;

  val = (value_ptr) (*args)->args[0];
  (*args)->result = (char *) value_ind (val);
  return 1;
}

gdb_result
GDB_value_slice (val, low, num, rval)
     value_ptr val;
     int low;
     int num;
     value_ptr *rval;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) val;
  args.args[1] = (char *) &low;
  args.args[2] = (char *) &num;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_value_slice, &args);
  if (r != GDB_OK)
    return r;

  *rval = (value_ptr) args.result;
  return GDB_OK;
}

static int
wrap_value_slice (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  value_ptr val;
  int low, num;

  val = (value_ptr) (*args)->args[0];
  low = *(int *) (*args)->args[1];
  num = *(int *) (*args)->args[2];
  (*args)->result = (char *) value_slice (val, low, num);
  return 1;
}

gdb_result
GDB_value_coerce_array (val, rval)
     value_ptr val;
     value_ptr *rval;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) val;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_value_coerce_array,
			     &args);
  if (r != GDB_OK)
    return r;

  *rval = (value_ptr) args.result;
  return GDB_OK;
}

static int
wrap_value_coerce_array (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  value_ptr val;

  val = (value_ptr) (*args)->args[0];
  (*args)->result = (char *) value_coerce_array (val);
  return 1;
}

gdb_result
GDB_value_struct_elt (argp, args, name, static_memfunc, err, rval)
     value_ptr *argp;
     value_ptr *args;
     char *name;
     int *static_memfunc;
     char *err;
     value_ptr *rval;
{
  struct gdb_wrapper_arguments argss;
  gdb_result r;

  argss.args[0] = (char *) argp;
  argss.args[1] = (char *) args;
  argss.args[2] = name;
  argss.args[3] = (char *) static_memfunc;
  argss.args[4] = err;
  r = call_wrapped_function ((catch_errors_ftype *) wrap_value_struct_elt, &argss);
  if (r != GDB_OK)
    return r;

  *rval = (value_ptr) argss.result;
  return GDB_OK;
}

static int
wrap_value_struct_elt (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **argss = (struct gdb_wrapper_arguments **) opaque_arg;
  value_ptr *argp, *args;
  char *name;
  int *static_memfunc;
  char *err;

  argp = (value_ptr *) (*argss)->args[0];
  args = (value_ptr *) (*argss)->args[1];
  name = (*argss)->args[2];
  static_memfunc = (int *) (*argss)->args[3];
  err = (*argss)->args[4];

  (*argss)->result = (char *) value_struct_elt (argp, args, name, static_memfunc, err);
  return 1;
}

gdb_result
GDB_value_cast (type, val, rval)
     struct type *type;
     value_ptr val;
     value_ptr *rval;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) type;
  args.args[1] = (char *) val;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_value_cast, &args);
  if (r != GDB_OK)
    return r;

  *rval = (value_ptr) args.result;
  return GDB_OK;
}

static int
wrap_value_cast (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  value_ptr val;
  struct type *type;

  type = (struct type *) (*args)->args[0];
  val = (value_ptr) (*args)->args[1];
  (*args)->result = (char *) value_cast (type, val);

  return 1;
}

gdb_result
GDB_get_frame_block (fi, rval)
     struct frame_info *fi;
     struct block **rval;
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) fi;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_get_frame_block, &args);
  if (r != GDB_OK)
    return r;

  *rval = (struct block *) args.result;
  return GDB_OK;
}

static int
wrap_get_frame_block (opaque_arg)
     char *opaque_arg;
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  struct frame_info *fi;

  fi = (struct frame_info *) (*args)->args[0];
  (*args)->result = (char *) get_frame_block (fi);

  return 1;
}

gdb_result
GDB_get_prev_frame (struct frame_info *fi, struct frame_info **result)
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) fi;
  r = call_wrapped_function ((catch_errors_ftype *) wrap_get_prev_frame, &args);
  if (r != GDB_OK)
    return r;

  *result = (struct frame_info *) args.result;
  return GDB_OK;
}

static int
wrap_get_prev_frame (char *opaque_arg)
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  struct frame_info *fi = (struct frame_info *) (*args)->args[0];

  (*args)->result = (char *) get_prev_frame (fi);
  return 1;
}

gdb_result
GDB_get_next_frame (struct frame_info *fi, struct frame_info **result)
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) fi;
  r = call_wrapped_function ((catch_errors_ftype *) wrap_get_next_frame, &args);
  if (r != GDB_OK)
    return r;

  *result = (struct frame_info *) args.result;
  return GDB_OK;
}

static int
wrap_get_next_frame (char *opaque_arg)
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  struct frame_info *fi = (struct frame_info *) (*args)->args[0];

  (*args)->result = (char *) get_next_frame (fi);
  return 1;
}

gdb_result
GDB_find_relative_frame (struct frame_info *fi, int *start,
			 struct frame_info **result)
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  args.args[0] = (char *) fi;
  args.args[1] = (char *) start;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_find_relative_frame, 
			     &args);
  if (r != GDB_OK)
    return r;

  *result = (struct frame_info *) args.result;
  return GDB_OK;
}

static int
wrap_find_relative_frame (char *opaque_arg)
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;
  struct frame_info *fi = (struct frame_info *) (*args)->args[0];
  int *start = (int *) (*args)->args[1];

  (*args)->result = (char *) find_relative_frame (fi, start);
  return 1;
}

gdb_result
GDB_get_current_frame (struct frame_info **result)
{
  struct gdb_wrapper_arguments args;
  gdb_result r;

  r = call_wrapped_function ((catch_errors_ftype *) wrap_get_current_frame, 
			     &args);
  if (r != GDB_OK)
    return r;

  *result = (struct frame_info *) args.result;
  return GDB_OK;
}

static int
wrap_get_current_frame (char *opaque_arg)
{
  struct gdb_wrapper_arguments **args = (struct gdb_wrapper_arguments **) opaque_arg;

  (*args)->result = (char *) get_current_frame ();
  return 1;
}



/* Local variables: */
/* change-log-default-name: "ChangeLog-gdbtk" */
/* End: */
