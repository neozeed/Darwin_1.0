/* Longjump free calls to gdb internal routines.
   Copyright 1999 Free Software Foundation, Inc.

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

#ifndef WRAPPER_H
#define WRAPPER_H 1

/* Use this struct used to pass arguments to wrapper routines. */
struct gdb_wrapper_arguments;

extern int gdb_parse_exp_1 PARAMS ((char **, struct block *, 
			     int, struct expression **));
extern int wrap_parse_exp_1 PARAMS ((char *));

extern int gdb_evaluate_expression PARAMS ((struct expression *, value_ptr *));
extern int wrap_evaluate_expression PARAMS ((char *));

extern int gdb_value_fetch_lazy PARAMS ((value_ptr));
extern int wrap_value_fetch_lazy PARAMS ((char *));

extern int gdb_value_equal PARAMS ((value_ptr, value_ptr, int *));
extern int wrap_value_equal PARAMS ((char *));

extern int gdb_value_ind PARAMS ((value_ptr val, value_ptr * rval));
extern int wrap_value_ind PARAMS ((char *opaque_arg));

#endif /* WRAPPER_H */
