/* Declarations having to do with Emacs category tables.
   Copyright (C) 1995 Electrotechnical Laboratory, JAPAN.
     Licensed to the Free Software Foundation.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/* We introduce here three types of object: category, category set,
   and category table.

   A category is like syntax but differs in the following points:

   o A category is represented by a mnemonic character of the range
   ` '(32)..`~'(126) (printable ASCII characters).

   o A category is not exclusive, i.e. a character has multiple
   categories (category set).  Of course, there's a case that a
   category set is empty, i.e. the character has no category.

   o In addition to the predefined categories, a user can define new
   categories.  Total number of categories is limited to 95.

   A category set is a set of categories represented by Lisp
   bool-vector of length 128 (only elements of 31th through 126th
   are used).

   A category table is like syntax-table, represented by a Lisp
   char-table.  The contents are category sets or nil.  It has two
   extra slots, for a vector of doc string of each category and a
   version number.

   The first extra slot is a vector of doc strings of categories, the
   length is 95.  The Nth element corresponding to the category N+32.

   The second extra slot is a version number of the category table.
   But, for the moment, we are not using this slot.  */

#define CATEGORYP(x) \
  (INTEGERP ((x)) && XFASTINT ((x)) >= 0x20 && XFASTINT ((x)) <= 0x7E)

#define CHECK_CATEGORY(x, i)						\
  do {									\
    if (!CATEGORYP ((x))) x = wrong_type_argument (Qcategoryp, (x));	\
  } while (0)

#define XCATEGORY_SET XBOOL_VECTOR

#define CATEGORY_SET_P(x) \
  (BOOL_VECTOR_P ((x)) && (EMACS_INT) (XBOOL_VECTOR ((x))->size) == 128)

/* Return a new empty category set.  */
#define MAKE_CATEGORY_SET (Fmake_bool_vector (make_number (128), Qnil))

/* Make CATEGORY_SET includes (if VAL is t) or excludes (if VAL is
   nil) CATEGORY.  */
#define SET_CATEGORY_SET(category_set, category, val) \
  (Faset (category_set, category, val))

#define CHECK_CATEGORY_SET(x, i)					   \
  do {									   \
    if (!CATEGORY_SET_P ((x))) x = wrong_type_argument (Qcategorysetp, (x)); \
  } while (0)

/* Return 1 if CATEGORY_SET contains CATEGORY, else return 0.
   The faster version of `!NILP (Faref (category_set, category))'.  */
#define CATEGORY_MEMBER(category, category_set)		 		\
  (XCATEGORY_SET (category_set)->data[(category) / 8]			\
   & (1 << ((category) % 8)))

/* Temporary internal variable used in macro CHAR_HAS_CATEGORY.  */
extern Lisp_Object _temp_category_set;

/* Return 1 if category set of CH contains CATEGORY, elt return 0.  */
#define CHAR_HAS_CATEGORY(ch, category)	\
  (_temp_category_set = CATEGORY_SET (ch),	\
   CATEGORY_MEMBER (category, _temp_category_set))

/* The standard category table is stored where it will automatically
   be used in all new buffers.  */
#define Vstandard_category_table buffer_defaults.category_table

/* Return the category set of character C in the current category table.  */
#ifdef __GNUC__
#define CATEGORY_SET(c)							     \
  ({ Lisp_Object table = current_buffer->category_table;		     \
     Lisp_Object temp;							     \
     if ((c) < CHAR_TABLE_SINGLE_BYTE_SLOTS)				     \
       while (NILP (temp = XCHAR_TABLE (table)->contents[(unsigned char) c]) \
	      && NILP (temp = XCHAR_TABLE (table)->defalt))		     \
	 table = XCHAR_TABLE (table)->parent;				     \
     else								     \
       temp = Faref (table,						     \
		     make_number (COMPOSITE_CHAR_P (c)			     \
				  ? cmpchar_component (c, 0) : (c)));	     \
     temp; })
#else
#define CATEGORY_SET(c)							     \
  ((c) < CHAR_TABLE_SINGLE_BYTE_SLOTS					     \
   ? Faref (current_buffer->category_table, make_number ((unsigned char) c)) \
   : Faref (current_buffer->category_table,				     \
	    make_number (COMPOSITE_CHAR_P (c)				     \
			 ? cmpchar_component ((c), 0) : (c))))
#endif   

/* Return the doc string of CATEGORY in category table TABLE.  */
#define CATEGORY_DOCSTRING(table, category) \
  XVECTOR (Fchar_table_extra_slot (table, make_number (0)))->contents[(category) - ' ']

/* Return the version number of category table TABLE.  Not used for
   the moment.  */
#define CATEGORY_TABLE_VERSION (table) \
  Fchar_table_extra_slot (table, make_number (1))

/* Return 1 if there is a word boundary between two word-constituent
   characters C1 and C2 if they appear in this order, else return 0.
   There is no word boundary between two word-constituent ASCII
   characters.  */
#define WORD_BOUNDARY_P(c1, c2)					\
  (!(SINGLE_BYTE_CHAR_P (c1) && SINGLE_BYTE_CHAR_P (c2))	\
   && word_boundary_p (c1, c2))

extern int word_boundary_p P_ ((int, int));
