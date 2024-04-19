/* Objective C language support routines for GDB, the GNU debugger.
   Copyright 1996 NeXT Software, Inc.

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
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "parser-defs.h"
#include "language.h"
#include "objc-lang.h"
#include "complaints.h"
#include "value.h"
#include "symfile.h"
#include "objfiles.h"
#include "string.h"		/* for strchr */
#include "target.h"		/* for target_has_execution */
#include "gdbcore.h"
#include "gdbcmd.h"

#include <assert.h>

extern value_ptr find_function_in_inferior PARAMS((char *));

struct objc_object {
  CORE_ADDR isa;
};

struct objc_class {
  CORE_ADDR isa; 
  CORE_ADDR super_class; 
  CORE_ADDR name;               
  long version;
  long info;
  long instance_size;
  CORE_ADDR ivars;
  CORE_ADDR methods;
  CORE_ADDR cache;
  CORE_ADDR protocols;
};

struct objc_super {
  CORE_ADDR receiver;
  CORE_ADDR class;
};

struct objc_method {
  CORE_ADDR name;
  CORE_ADDR types;
  CORE_ADDR imp;
};

/* Complaints about ObjC classes, selectors, etc.  */

#if 0
static struct complaint noclass_complaint = 
  {"%s: class not found", 0, 0};
#endif

static struct complaint noclass_lookup_complaint = 
  {"no way to lookup ObjC classes", 0, 0};

#if 0
static struct complaint noselector_complaint = 
  {"%s: selector not found", 0, 0};
#endif

static struct complaint nosel_lookup_complaint = 
  {"no way to lookup ObjC selectors", 0, 0};


/* test for a string of the form "+/-[...]" with balanced brackets.
   If successful, return pointer to closing right bracket, else NULL */

char *
is_objc_demangled(name)
     char *name;
{
  int nesting = 0;
  char *temp;

  if ((temp = strchr(name, '-')) ||	/* skip prefix (might be file name) */
      (temp = strchr(name, '+')))
    name = temp;

  if ((*name == '-' || *name == '+') &&		/* must start with +/- */
      *++name == '[')				/* followed by [       */
    {
      nesting = 1;
      while (nesting && *++name)		/* scan for balanced [] */
	if (*name == '[')
	  nesting++;
	else if (*name == ']')
	  nesting--;
      if (nesting == 0)
	return name;			/* success, matching ] found */
    }
  return NULL;				/* nope, doesn't fit pattern */
}



/* Lookup a structure type named "struct NAME",
   visible in lexical block BLOCK.  
   If NOERR is nonzero, return zero if NAME is not suitably defined.  */

struct symbol *
lookup_struct_typedef (name, block, noerr)
     char *name;
     struct block *block;
     int noerr;
{
  register struct symbol *sym;

  sym = lookup_symbol (name, block, STRUCT_NAMESPACE, 0, (struct symtab **) NULL);

  if (sym == NULL)
    {
      if (noerr)
	return 0;
      else 
	error ("No struct type named %s.", name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    {
      if (noerr)
	return 0;
      else
	error ("This context has class, union or enum %s, not a struct.", name);
    }
  return sym;
}

#if 0
struct symbol *
lookup_objc_class(char *classname)
{
  struct symbol * sym = NULL;

  if (current_language->la_language != language_objc &&
      current_language->la_language != language_unknown)
    return NULL;	/* no sense looking if this isn't ObjC */

  if (!(sym = lookup_struct_typedef (classname, 
				     expression_context_block, 1))) {
    complain (&noclass_complaint, classname);
    return NULL;
  }
  if (SYMBOL_LANGUAGE (sym) != language_objc &&
      SYMBOL_LANGUAGE (sym) != language_unknown)
    return NULL;

  /* FIXME: don't know how to distinguish a class from a struct! */
  return sym;
}
#else
CORE_ADDR 
lookup_objc_class(classname)
     char *classname;
{
  value_ptr function, classval;

  if (!target_has_execution)
    return 0;		/* can't call into inferior to lookup class */

  if (lookup_minimal_symbol("objc_lookUpClass", 0, 0))
    function = find_function_in_inferior("objc_lookUpClass");
  else if (lookup_minimal_symbol("objc_lookup_class", 0, 0))
    function = find_function_in_inferior("objc_lookup_class");
  else
    {
      complain (&noclass_lookup_complaint, 0);
      return 0;
    }

  classval = value_string(classname, strlen(classname) + 1);
  classval = value_coerce_array(classval);
  return (CORE_ADDR) value_as_long (call_function_by_hand (function, 1, &classval));
}
#endif

int
lookup_child_selector (selname)
     char *selname;
{
  value_ptr function, selstring;

  if (!target_has_execution)
    return 0;		/* can't call into inferior to lookup selector */

  if (lookup_minimal_symbol("sel_getUid", 0, 0))
    function = find_function_in_inferior("sel_getUid");
  else if (lookup_minimal_symbol("sel_get_any_uid", 0, 0))
    function = find_function_in_inferior("sel_get_any_uid");
  else
    {
      complain (&nosel_lookup_complaint, 0);
      return 0;
    }

  selstring = value_coerce_array(value_string(selname, strlen(selname) + 1));
  return value_as_long(call_function_by_hand(function, 1, &selstring));
}

value_ptr 
value_nsstring (ptr, len)
     char *ptr;
     int len;
{
  value_ptr stringValue[3];
  value_ptr function, nsstringValue;
  struct symbol *sym;
  struct type *type;

  if (!target_has_execution)
    return 0;		/* can't call into inferior to create NSString */

  if (!(sym = lookup_struct_typedef("NSString", 0, 1)) &&
      !(sym = lookup_struct_typedef("NXString", 0, 1)))
    type = lookup_pointer_type(builtin_type_void);
  else
    type = lookup_pointer_type(SYMBOL_TYPE (sym));

  stringValue[2] = value_string(ptr, len);
  stringValue[2] = value_coerce_array(stringValue[2]);
  /* _NSNewStringFromCString replaces "istr" after Lantern2A */
  if (lookup_minimal_symbol("_NSNewStringFromCString", 0, 0))
    {
      function = find_function_in_inferior("_NSNewStringFromCString");
      nsstringValue = call_function_by_hand(function, 1, &stringValue[2]);
    }
  else if (lookup_minimal_symbol("istr", 0, 0))
    {
      function = find_function_in_inferior("istr");
      nsstringValue = call_function_by_hand(function, 1, &stringValue[2]);
    }
  else if (lookup_minimal_symbol("+[NSString stringWithCString:]", 0, 0))
    {
      function = find_function_in_inferior("+[NSString stringWithCString:]");
      stringValue[0] = value_from_longest 
	(builtin_type_long, lookup_objc_class ("NSString"));
      stringValue[1] = value_from_longest 
	(builtin_type_long, lookup_child_selector ("stringWithCString:"));
      nsstringValue = call_function_by_hand(function, 3, &stringValue[0]);
    }
  else
    error ("NSString: internal error -- no way to create new NSString");

  VALUE_TYPE(nsstringValue) = type;
  return nsstringValue;
}
    


/* Objective C name demangling */

char *
objc_demangle (mangled)
     const char *mangled;
{
  char *demangled, *cp;

  if (mangled[0] == '_' &&
     (mangled[1] == 'i' || mangled[1] == 'c') &&
      mangled[2] == '_')
    {
      cp = demangled = xmalloc(strlen(mangled) + 2);

      if (mangled[1] == 'i')
	*cp++ = '-';		/* for instance method */
      else
	*cp++ = '+';		/* for class    method */

      *cp++ = '[';		/* opening left brace  */
      strcpy(cp, mangled+3);	/* tack on the rest of the mangled name */

      while (*cp && *cp == '_')
	cp++;			/* skip any initial underbars in class name */

      if (!(cp = strchr(cp, '_')))	/* find first non-initial underbar */
	{
	  free(demangled);	/* not mangled name */
	  return NULL;
	}
      if (cp[1] == '_') {	/* easy case: no category name     */
	*cp++ = ' ';		/* replace two '_' with one ' '    */
	strcpy(cp, mangled + (cp - demangled) + 2);
      }
      else {
	*cp++ = '(';		/* less easy case: category name */
	if (!(cp = strchr(cp, '_')))
	  {
	    free(demangled);	/* not mangled name */
	    return NULL;
	  }
	*cp++ = ')';
	*cp++ = ' ';		/* overwriting 1st char of method name... */
	strcpy(cp, mangled + (cp - demangled));	/* get it back */
      }

      while (*cp && *cp == '_')
	cp++;			/* skip any initial underbars in method name */

      for (; *cp; cp++)
	if (*cp == '_')
	  *cp = ':';		/* replace remaining '_' with ':' */

      *cp++ = ']';		/* closing right brace */
      *cp++ = 0;		/* string terminator */
      return demangled;
    }
  else
    return NULL;	/* not an objc mangled name */
}

/* Print the character C on STREAM as part of the contents of a literal
   string whose delimiter is QUOTER.  Note that that format for printing
   characters and strings is language specific. */

static void
objc_emit_char (c, stream, quoter)
     register int c;
     struct ui_file *stream;
     int quoter;
{

  c &= 0xFF;			/* Avoid sign bit follies */

  if (PRINT_LITERAL_FORM (c))
    {
      if (c == '\\' || c == quoter)
	{
	  fputs_filtered ("\\", stream);
	}
      fprintf_filtered (stream, "%c", c);
    }
  else
    {
      switch (c)
	{
	case '\n':
	  fputs_filtered ("\\n", stream);
	  break;
	case '\b':
	  fputs_filtered ("\\b", stream);
	  break;
	case '\t':
	  fputs_filtered ("\\t", stream);
	  break;
	case '\f':
	  fputs_filtered ("\\f", stream);
	  break;
	case '\r':
	  fputs_filtered ("\\r", stream);
	  break;
	case '\033':
	  fputs_filtered ("\\e", stream);
	  break;
	case '\007':
	  fputs_filtered ("\\a", stream);
	  break;
	default:
	  fprintf_filtered (stream, "\\%.3o", (unsigned int) c);
	  break;
	}
    }
}

static void
objc_printchar (c, stream)
     int c;
     struct ui_file *stream;
{
  fputs_filtered ("'", stream);
  objc_emit_char (c, stream, '\'');
  fputs_filtered ("'", stream);
}

/* Print the character string STRING, printing at most LENGTH characters.
   Printing stops early if the number hits print_max; repeat counts
   are printed as appropriate.  Print ellipses at the end if we
   had to stop before printing LENGTH characters, or if FORCE_ELLIPSES.  */

static void
objc_printstr (stream, string, length, force_ellipses)
     struct ui_file *stream;
     char *string;
     unsigned int length;
     int force_ellipses;
{
  register unsigned int i;
  unsigned int things_printed = 0;
  int in_quotes = 0;
  int need_comma = 0;
  extern int inspect_it;
  extern int repeat_count_threshold;
  extern int print_max;

  /* If the string was not truncated due to `set print elements', and
     the last byte of it is a null, we don't print that, in traditional C
     style.  */
  if ((!force_ellipses) && length > 0 && string[length-1] == '\0')
    length--;

  if (length == 0)
    {
      fputs_filtered ("\"\"", stream);
      return;
    }

  for (i = 0; i < length && things_printed < print_max; ++i)
    {
      /* Position of the character we are examining
	 to see whether it is repeated.  */
      unsigned int rep1;
      /* Number of repetitions we have detected so far.  */
      unsigned int reps;

      QUIT;

      if (need_comma)
	{
	  fputs_filtered (", ", stream);
	  need_comma = 0;
	}

      rep1 = i + 1;
      reps = 1;
      while (rep1 < length && string[rep1] == string[i])
	{
	  ++rep1;
	  ++reps;
	}

      if (reps > repeat_count_threshold)
	{
	  if (in_quotes)
	    {
	      if (inspect_it)
		fputs_filtered ("\\\", ", stream);
	      else
		fputs_filtered ("\", ", stream);
	      in_quotes = 0;
	    }
	  objc_printchar (string[i], stream);
	  fprintf_filtered (stream, " <repeats %u times>", reps);
	  i = rep1 - 1;
	  things_printed += repeat_count_threshold;
	  need_comma = 1;
	}
      else
	{
	  if (!in_quotes)
	    {
	      if (inspect_it)
		fputs_filtered ("\\\"", stream);
	      else
		fputs_filtered ("\"", stream);
	      in_quotes = 1;
	    }
	  objc_emit_char (string[i], stream, '"');
	  ++things_printed;
	}
    }

  /* Terminate the quotes if necessary.  */
  if (in_quotes)
    {
      if (inspect_it)
	fputs_filtered ("\\\"", stream);
      else
	fputs_filtered ("\"", stream);
    }

  if (force_ellipses || i < length)
    fputs_filtered ("...", stream);
}

/* Create a fundamental C type using default reasonable for the current
   target machine.

   Some object/debugging file formats (DWARF version 1, COFF, etc) do not
   define fundamental types such as "int" or "double".  Others (stabs or
   DWARF version 2, etc) do define fundamental types.  For the formats which
   don't provide fundamental types, gdb can create such types using this
   function.

   FIXME:  Some compilers distinguish explicitly signed integral types
   (signed short, signed int, signed long) from "regular" integral types
   (short, int, long) in the debugging information.  There is some dis-
   agreement as to how useful this feature is.  In particular, gcc does
   not support this.  Also, only some debugging formats allow the
   distinction to be passed on to a debugger.  For now, we always just
   use "short", "int", or "long" as the type name, for both the implicit
   and explicitly signed types.  This also makes life easier for the
   gdb test suite since we don't have to account for the differences
   in output depending upon what the compiler and debugging format
   support.  We will probably have to re-examine the issue when gdb
   starts taking it's fundamental type information directly from the
   debugging information supplied by the compiler.  fnf@cygnus.com */

static struct type *
objc_create_fundamental_type (objfile, typeid)
     struct objfile *objfile;
     int typeid;
{
  register struct type *type = NULL;

  switch (typeid)
    {
      default:
	/* FIXME:  For now, if we are asked to produce a type not in this
	   language, create the equivalent of a C integer type with the
	   name "<?type?>".  When all the dust settles from the type
	   reconstruction work, this should probably become an error. */
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  0, "<?type?>", objfile);
        warning ("internal error: no C/C++ fundamental type %d", typeid);
	break;
      case FT_VOID:
	type = init_type (TYPE_CODE_VOID,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  0, "void", objfile);
	break;
      case FT_CHAR:
	type = init_type (TYPE_CODE_INT,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  0, "char", objfile);
	break;
      case FT_SIGNED_CHAR:
	type = init_type (TYPE_CODE_INT,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  0, "signed char", objfile);
	break;
      case FT_UNSIGNED_CHAR:
	type = init_type (TYPE_CODE_INT,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned char", objfile);
	break;
      case FT_SHORT:
	type = init_type (TYPE_CODE_INT,
			  TARGET_SHORT_BIT / TARGET_CHAR_BIT,
			  0, "short", objfile);
	break;
      case FT_SIGNED_SHORT:
	type = init_type (TYPE_CODE_INT,
			  TARGET_SHORT_BIT / TARGET_CHAR_BIT,
			  0, "short", objfile);	/* FIXME-fnf */
	break;
      case FT_UNSIGNED_SHORT:
	type = init_type (TYPE_CODE_INT,
			  TARGET_SHORT_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned short", objfile);
	break;
      case FT_INTEGER:
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  0, "int", objfile);
	break;
      case FT_SIGNED_INTEGER:
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  0, "int", objfile); /* FIXME -fnf */
	break;
      case FT_UNSIGNED_INTEGER:
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned int", objfile);
	break;
      case FT_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_BIT / TARGET_CHAR_BIT,
			  0, "long", objfile);
	break;
      case FT_SIGNED_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_BIT / TARGET_CHAR_BIT,
			  0, "long", objfile); /* FIXME -fnf */
	break;
      case FT_UNSIGNED_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned long", objfile);
	break;
      case FT_LONG_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
			  0, "long long", objfile);
	break;
      case FT_SIGNED_LONG_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
			  0, "signed long long", objfile);
	break;
      case FT_UNSIGNED_LONG_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned long long", objfile);
	break;
      case FT_FLOAT:
	type = init_type (TYPE_CODE_FLT,
			  TARGET_FLOAT_BIT / TARGET_CHAR_BIT,
			  0, "float", objfile);
	break;
      case FT_DBL_PREC_FLOAT:
	type = init_type (TYPE_CODE_FLT,
			  TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
			  0, "double", objfile);
	break;
      case FT_EXT_PREC_FLOAT:
	type = init_type (TYPE_CODE_FLT,
			  TARGET_LONG_DOUBLE_BIT / TARGET_CHAR_BIT,
			  0, "long double", objfile);
	break;
      }
  return (type);
}


/* Table mapping opcodes into strings for printing operators
   and precedences of the operators.  */

static const struct op_print objc_op_print_tab[] =
  {
    {",",  BINOP_COMMA, PREC_COMMA, 0},
    {"=",  BINOP_ASSIGN, PREC_ASSIGN, 1},
    {"||", BINOP_LOGICAL_OR, PREC_LOGICAL_OR, 0},
    {"&&", BINOP_LOGICAL_AND, PREC_LOGICAL_AND, 0},
    {"|",  BINOP_BITWISE_IOR, PREC_BITWISE_IOR, 0},
    {"^",  BINOP_BITWISE_XOR, PREC_BITWISE_XOR, 0},
    {"&",  BINOP_BITWISE_AND, PREC_BITWISE_AND, 0},
    {"==", BINOP_EQUAL, PREC_EQUAL, 0},
    {"!=", BINOP_NOTEQUAL, PREC_EQUAL, 0},
    {"<=", BINOP_LEQ, PREC_ORDER, 0},
    {">=", BINOP_GEQ, PREC_ORDER, 0},
    {">",  BINOP_GTR, PREC_ORDER, 0},
    {"<",  BINOP_LESS, PREC_ORDER, 0},
    {">>", BINOP_RSH, PREC_SHIFT, 0},
    {"<<", BINOP_LSH, PREC_SHIFT, 0},
    {"+",  BINOP_ADD, PREC_ADD, 0},
    {"-",  BINOP_SUB, PREC_ADD, 0},
    {"*",  BINOP_MUL, PREC_MUL, 0},
    {"/",  BINOP_DIV, PREC_MUL, 0},
    {"%",  BINOP_REM, PREC_MUL, 0},
    {"@",  BINOP_REPEAT, PREC_REPEAT, 0},
    {"-",  UNOP_NEG, PREC_PREFIX, 0},
    {"!",  UNOP_LOGICAL_NOT, PREC_PREFIX, 0},
    {"~",  UNOP_COMPLEMENT, PREC_PREFIX, 0},
    {"*",  UNOP_IND, PREC_PREFIX, 0},
    {"&",  UNOP_ADDR, PREC_PREFIX, 0},
    {"sizeof ", UNOP_SIZEOF, PREC_PREFIX, 0},
    {"++", UNOP_PREINCREMENT, PREC_PREFIX, 0},
    {"--", UNOP_PREDECREMENT, PREC_PREFIX, 0},
    {NULL, 0, 0, 0}
};

struct type ** const (objc_builtin_types[]) = 
{
  &builtin_type_int,
  &builtin_type_long,
  &builtin_type_short,
  &builtin_type_char,
  &builtin_type_float,
  &builtin_type_double,
  &builtin_type_void,
  &builtin_type_long_long,
  &builtin_type_signed_char,
  &builtin_type_unsigned_char,
  &builtin_type_unsigned_short,
  &builtin_type_unsigned_int,
  &builtin_type_unsigned_long,
  &builtin_type_unsigned_long_long,
  &builtin_type_long_double,
  &builtin_type_complex,
  &builtin_type_double_complex,
  0
};

const struct language_defn objc_language_defn = {
  "objective-c",				/* Language name */
  language_objc,
  objc_builtin_types,
  range_check_off,
  type_check_off,
  objc_parse,
  objc_error,
  evaluate_subexp_standard,
  objc_printchar,		/* Print a character constant */
  objc_printstr,		/* Function to print string constant */
  objc_emit_char,
  objc_create_fundamental_type,	/* Create fundamental type in this language */
  c_print_type,			/* Print a type using appropriate syntax */
  c_val_print,			/* Print a value using appropriate syntax */
  c_value_print,		/* Print a top-level value */
  {"",     "",    "",  ""},	/* Binary format info */
  {"0%lo",  "0",   "o", ""},	/* Octal format info */
  {"%ld",   "",    "d", ""},	/* Decimal format info */
  {"0x%lx", "0x",  "x", ""},	/* Hex format info */
  objc_op_print_tab,		/* expression operators for printing */
  1,				/* c-style arrays */
  0,				/* String lower bound */
  &builtin_type_char,		/* Type of string elements */
  LANG_MAGIC
};

/*
 * ObjC:
 * Following functions help construct Objective C message calls 
 */

struct selname		/* for parsing Objective C */
  {
    struct selname *next;
    char *msglist_sel;
    int msglist_len;
  };

static int msglist_len;
static struct selname *selname_chain;
static char *msglist_sel;

void
start_msglist()
{
  register struct selname *new = 
    (struct selname *) xmalloc (sizeof (struct selname));

  new->next = selname_chain;
  new->msglist_len = msglist_len;
  new->msglist_sel = msglist_sel;
  msglist_len = 0;
  msglist_sel = (char *)xmalloc(1);
  *msglist_sel = 0;
  selname_chain = new;
}

void
add_msglist(str, addcolon)
     struct stoken *str;
     int addcolon;
{
  char *s, *p;
  int len, plen;

  if (str == 0) {		/* unnamed arg, or... */
    if (addcolon == 0) {	/* variable number of args */
      msglist_len++;
      return;
    }
    p = "";
    plen = 0;
  } else {
    p = str->ptr;
    plen = str->length;
  }
  len = plen + strlen(msglist_sel) + 2;
  s = (char *)xmalloc(len);
  strcpy(s, msglist_sel);
  strncat(s, p, plen);
  free(msglist_sel);
  msglist_sel = s;
  if (addcolon) {
    s[len-2] = ':';
    s[len-1] = 0;
    msglist_len++;
  } else
    s[len-2] = '\0';
}

int
end_msglist()
{
  register int val = msglist_len;
  register struct selname *sel = selname_chain;
  register char *p = msglist_sel;
  int selid;

  selname_chain = sel->next;
  msglist_len = sel->msglist_len;
  msglist_sel = sel->msglist_sel;
  selid = lookup_child_selector(p);
  if (!selid)
    error("Can't find selector \"%s\"", p);
  write_exp_elt_longcst (selid);
  free(p);
  write_exp_elt_longcst (val);/* Number of args */
  free(sel);

  return val;
}

/*
 * Function: specialcmp (char *a, char *b)
 *
 * Special strcmp: treats ']' and ' ' as end-of-string.
 * Used for qsorting lists of objc methods (either by class or selector) 
 */

int specialcmp(a, b)
     char *a;
     char *b;
{
  while (*a && *a != ' ' && *a != ']' && *b && *b != ' ' && *b != ']')
    {
      if (*a != *b)
	return *a - *b;
      a++, b++;
    }
  if (*a && *a != ' ' && *a != ']')
    return  1;		/* a is longer therefore greater */
  if (*b && *b != ' ' && *b != ']')
    return -1;		/* a is shorter therefore lesser */
  return    0;		/* a and b are identical */
}

/*
 * Function: compare_selectors (void *, void *)
 *
 * Comparison function for use with qsort.  Arguments are symbols or msymbols
 * Compares selector part of objc method name alphabetically.
 */

static int
compare_selectors (a, b)
     void *a;
     void *b;
{
  char *aname, *bname;

  if ((aname = SYMBOL_SOURCE_NAME (*(struct symbol **) a)) == NULL ||
      (bname = SYMBOL_SOURCE_NAME (*(struct symbol **) b)) == NULL)
    error ("internal: compare_selectors(1)");

  if ((aname = strchr(aname, ' ')) == NULL ||
      (bname = strchr(bname, ' ')) == NULL)
    error ("internal: compare_selectors(2)");

  return specialcmp (aname+1, bname+1);
}

/*
 * Function: selectors_info (regexp, from_tty)
 *
 * Implements the "Info selectors" command.  Takes an optional regexp arg.
 * Lists all objective c selectors that match the regexp.  Works by 
 * grepping thru all symbols for objective c methods.  Output list is 
 * sorted and uniqued. 
 */

static void
selectors_info (regexp, from_tty)
     char *regexp;
     int   from_tty;
{
  struct objfile	*objfile;
  struct minimal_symbol *msymbol;
  char                  *name;
  char                  *val;
  int                    matches = 0;
  int                    maxlen  = 0;
  int                    ix;
  char                   myregexp[2048];
  char                   asel[256];
  struct symbol        **sym_arr;
  int                    plusminus = 0;

  if (regexp == NULL)
    strcpy(myregexp, ".*]");	/* null input, match all objc methods */
  else
    {
      if (*regexp == '+' || *regexp == '-')	/* if regexp starts with +/- */
	{ 	   /* user wants only class methods or only instance methods */
	  plusminus = *regexp++;
	  while (*regexp == ' ' || *regexp == '\t')
	    regexp++;
	}
      if (*regexp == '\0')
	strcpy(myregexp, ".*]");
      else
	{
	  strcpy(myregexp, regexp);
	  if (myregexp[strlen(myregexp) - 1] == '$') /* end of selector */
	    myregexp[strlen(myregexp) - 1] = ']';    /* end of method name */
	  else
	    strcat(myregexp, ".*]");
	}
    }

  if (regexp != NULL)
    if (0 != (val = re_comp (myregexp)))
      error ("Invalid regexp (%s): %s", val, regexp);

  /* first time thru is JUST to get max length and count */
  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;
      if ((name = SYMBOL_DEMANGLED_NAME (msymbol)) == NULL)
	name = SYMBOL_NAME (msymbol);
      if (name &&
	 (name[0] == '-' || name[0] == '+') &&
	  name[1] == '[')		/* got a method name */
	{
	  if (plusminus && name[0] != plusminus)
	    continue;			/* filter for class/instance methods */
	  name = (char *) strchr(name+2, ' ');	/* find selector part */
	  if (regexp == NULL || re_exec(++name) != 0)
	    { 
	      char *mystart = name;
	      char *myend   = (char *) strchr(mystart, ']');
	      
	      if (myend && (myend - mystart > maxlen))
		maxlen = myend - mystart;	/* get longest selector */
	      matches++;
	    }
	}
    }
  if (matches)
    {
      printf_filtered ("Selectors matching \"%s\":\n\n", 
		       regexp ? regexp : "*");

      sym_arr = alloca (matches * sizeof (struct symbol *));
      matches = 0;
      ALL_MSYMBOLS (objfile, msymbol)
	{
	  QUIT;
	  if ((name = SYMBOL_DEMANGLED_NAME (msymbol)) == NULL)
	    name = SYMBOL_NAME (msymbol);
	  if (name &&
	     (name[0] == '-' || name[0] == '+') &&
	      name[1] == '[')		/* got a method name */
	    {
	      if (plusminus && name[0] != plusminus)
		continue;		/* filter for class/instance methods */
	      name = (char *) strchr(name+2, ' ');     /* find selector part */
	      if (regexp == NULL || re_exec(++name) != 0)
		sym_arr[matches++] = (struct symbol *) msymbol;
	    }
	}

      qsort (sym_arr, matches, sizeof (struct minimal_symbol *), 
	     compare_selectors);
      asel[0] = 0;		/* to prevent compare on first iteration */
      for (ix = 0; ix < matches; ix++)	/* now do the output */
	{
	  char *p = asel;

	  QUIT;
	  if ((name = SYMBOL_DEMANGLED_NAME (sym_arr[ix])) == NULL)
	    name = SYMBOL_NAME (sym_arr[ix]);
	  name = strchr (name, ' ') + 1;
	  if (p[0] && specialcmp(name, p) == 0)
	    continue;		/* seen this one already (not unique) */

	  while (*name && *name != ']')		/* copy selector part */
	    *p++ = *name++;
	  *p++ = '\0';
	  puts_filtered_tabular(asel, maxlen + 1, 0);	/* print in columns */
	}
      begin_line();
    }
  else
    printf_filtered ("No selectors matching \"%s\"\n", regexp ? regexp : "*");
}

/*
 * Function: compare_classes (void *, void *)
 *
 * Comparison function for use with qsort.  Arguments are symbols or msymbols
 * Compares class part of objc method name alphabetically.
 */

static int
compare_classes (a, b)
     void *a;
     void *b;
{
  char *aname, *bname;

  if ((aname = SYMBOL_SOURCE_NAME (*(struct symbol **) a)) == NULL ||
      (bname = SYMBOL_SOURCE_NAME (*(struct symbol **) b)) == NULL)
    error ("internal: compare_classes(1)");

  return specialcmp (aname+1, bname+1);
}

/*
 * Function: classes_info(regexp, from_tty)
 *
 * Implements the "info classes" command for objective c classes.
 * Lists all objective c classes that match the optional regexp.
 * Works by grepping thru the list of objective c methods.
 * List will be sorted and uniqued (since one class may have many methods).
 * BUGS: will not list a class that has no methods.
 */

static void
classes_info (regexp, from_tty)
     char *regexp;
     int   from_tty;
{
  struct objfile	*objfile;
  struct minimal_symbol *msymbol;
  char                  *name;
  char                  *val;
  int                    matches = 0;
  int                    maxlen  = 0;
  int                    ix;
  char                   myregexp[2048];
  char                   aclass[256];
  struct symbol        **sym_arr;

  if (regexp == NULL)
    strcpy(myregexp, ".* ");	/* null input: match all objc classes */
  else
    {
      strcpy(myregexp, regexp);
      if (myregexp[strlen(myregexp) - 1] == '$')
	/* in the method name, the end of the class name is marked by ' ' */
	myregexp[strlen(myregexp) - 1] = ' ';
      else
	strcat(myregexp, ".* ");
    }

  if (regexp != NULL)
    if (0 != (val = re_comp (myregexp)))
      error ("Invalid regexp (%s): %s", val, regexp);

  /* first time thru is JUST to get max length and count */
  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;
      if ((name = SYMBOL_DEMANGLED_NAME (msymbol)) == NULL)
	name = SYMBOL_NAME (msymbol);
      if (name &&
	 (name[0] == '-' || name[0] == '+') &&
	  name[1] == '[')				/* got a method name */
	if (regexp == NULL || re_exec(name+2) != 0)
	  { 
	    char *mystart = name + 2;	/* compute length of classname part */
	    char *myend   = (char *) strchr(mystart, ' ');
	    
	    if (myend && (myend - mystart > maxlen))
	      maxlen = myend - mystart;
	    matches++;
	  }
    }
  if (matches)
    {
      printf_filtered ("Classes matching \"%s\":\n\n", 
		       regexp ? regexp : "*");
      sym_arr = alloca (matches * sizeof (struct symbol *));
      matches = 0;
      ALL_MSYMBOLS (objfile, msymbol)
	{
	  QUIT;
	  if ((name = SYMBOL_DEMANGLED_NAME (msymbol)) == NULL)
	    name = SYMBOL_NAME (msymbol);
	  if (name &&
	     (name[0] == '-' || name[0] == '+') &&
	      name[1] == '[')				/* got a method name */
	    if (regexp == NULL || re_exec(name+2) != 0)
		sym_arr[matches++] = (struct symbol *) msymbol;
	}

      qsort (sym_arr, matches, sizeof (struct minimal_symbol *), 
	     compare_classes);
      aclass[0] = 0;		/* to prevent compare on first iteration */
      for (ix = 0; ix < matches; ix++)	/* now do the output */
	{
	  char *p = aclass;

	  QUIT;
	  if ((name = SYMBOL_DEMANGLED_NAME (sym_arr[ix])) == NULL)
	    name = SYMBOL_NAME (sym_arr[ix]);
	  name += 2;
	  if (p[0] && specialcmp(name, p) == 0)
	    continue;	/* seen this one already (not unique) */

	  while (*name && *name != ' ')	/* copy class part of method name */
	    *p++ = *name++;
	  *p++ = '\0';
	  puts_filtered_tabular(aclass, maxlen + 1, 0);	/* print in columns */
	}
      begin_line();
    }
  else
    printf_filtered ("No classes matching \"%s\"\n", regexp ? regexp : "*");
}

/*
 * Function: total_number_of_imps (char *selector);
 *
 * Input:  a string representing a selector 
 * Output: number of methods that implement that selector.
 *
 * By analogy with function "total_number_of_methods", this allows 
 * decode_line_1 (symtab.c) to detect if there are objective c methods
 * matching the input, and to allocate an array of pointers to them 
 * which can be manipulated by "decode_line_2" (also in symtab.c)
 */

int
total_number_of_imps (selector, symtab)
     char *selector;
     struct symtab *symtab;
{
  struct objfile	*objfile;
  struct minimal_symbol *msymbol;
  char                  *name;
  int                    matches = 0;
  int                    plusminus = 0;
  struct block          *block = 0;

  if (selector != NULL)		/* null input, match all selectors */
    {				/* (should never really happen)    */
      if (*selector == '-' || *selector == '+')
	plusminus = *selector++;	/* user selects class/inst methods */
      while (*selector == ' ' || *selector == '\t')
	selector++;			/* strip spaces/tabs */
    }

  if (symtab)
    block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (symtab), STATIC_BLOCK);

  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;
      if (symtab && 		/* was a symtab (source file) specified? */
	 (SYMBOL_VALUE_ADDRESS (msymbol) <  block->startaddr ||
	  SYMBOL_VALUE_ADDRESS (msymbol) >= block->endaddr))
	continue;		/* not in the specified symtab */

      if ((name = SYMBOL_DEMANGLED_NAME (msymbol)) == NULL)
	name = SYMBOL_NAME (msymbol);
      if (name && (name[0] == '-' || name[0] == '+') && name[1] == '[')
	{
	  /* got a method name */
	  if (plusminus && name[0] != plusminus)
	    continue;		/* filter for class/instance methods */
	  else 
	    {
	      if (!(name = strchr (name, ' ')))
		continue;		/* defensive coding: shouldn't happen */
	      if (!specialcmp (name+1, selector))
		matches++;	/* got a match */
	    }
	}
    }
  return matches;
}

/* 
 * Function: find_imps (char *selector, struct symbol **sym_arr)
 *
 * Input:  a string representing a selector
 *         a pointer to an array of symbol pointers
 *         possibly a pointer to a symbol found by the caller.
 *
 * Output: number of methods that implement that selector.
 * Side effects: The array of symbol pointers is filled with matching syms.
 *
 * By analogy with function "find_methods" (symtab.c), builds a list of
 * symbols matching the ambiguous input, so that "decode_line_2" (symtab.c)
 * can list them and ask the user to choose one or more.  In this case the
 * matches are objective c methods ("implementations") matching an objective
 * c selector.
 *
 * Note that it is possible for a normal (c-style) function to have the
 * same name as an objective c selector.  To prevent the selector from
 * eclipsing the function, we allow the caller (decode_line_1) to search
 * for such a function first, and if it finds one, pass it in to us.  We
 * will then integrate it into the list.  We also search for one here, 
 * among the minsyms.
 *
 * NOTE: if NUM_DEBUGGABLE is non-zero, the sym_arr will be 
 *       divided into two parts: debuggable (struct symbol) syms, 
 *       and non_debuggable (struct minimal_symbol) syms.  
 *       The debuggable ones will come first, before NUM_DEBUGGABLE
 *       (which will thus be the index of the first non-debuggable one).
 */

int 
find_imps (selector, sym_arr, num_debuggable, fn_match, symtab)
     char           *selector;		/* the string to match */
     struct symbol **sym_arr;		/* where to put matches */
     int            *num_debuggable;	/* -g (source) matches */
     struct symbol  *fn_match;		/* function with the same name */
     struct symtab  *symtab;
{
  struct objfile	*objfile;
  struct minimal_symbol *msymbol;
  struct symbol         *sym;
  char                  *name;
  int                    matches = 0;
  int                    plusminus = 0;
  struct block          *block = 0;

  if (selector != NULL)		/* Null would mean match all */
    {				/* (shouldn't happen) */
      if (*selector == '-' || *selector == '+')
	plusminus = *selector++;	/* user selects class/inst methods */
      while (*selector == ' ' || *selector == '\t')
	selector++;
    }

  if (num_debuggable)
    *num_debuggable = 0;

  if (symtab)
    block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (symtab), STATIC_BLOCK);

  if (fn_match)
    { /* match found by caller -- assume this one is debuggable (i.e. "-g") */
      sym_arr[matches++] = fn_match;
      if (num_debuggable) (*num_debuggable)++;
    }

  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;

      if (msymbol->type != mst_text &&
	  msymbol->type != mst_file_text)
	continue;		/* can't be a function or method */

      if (symtab &&		/* was a symtab (source file) specified? */
	 (SYMBOL_VALUE_ADDRESS (msymbol) <  block->startaddr ||
	  SYMBOL_VALUE_ADDRESS (msymbol) >= block->endaddr))
	continue;		/* not in the specified symtab */

      if ((name = SYMBOL_DEMANGLED_NAME (msymbol)) != NULL ||
	  (name = SYMBOL_NAME (msymbol))           != NULL) {
	if ((name[0] == '-' || name[0] == '+') && name[1] == '[')
	  {				/* found a method */
	    if (plusminus && name[0] != plusminus)
	      continue;			/* filter for class/instance methods */
	    if (!(name = strchr (name, ' ')))
	      continue;		/* defensive coding; shouldn't happen */
	    if (!specialcmp(name+1, selector)) {
	      if (num_debuggable &&
		  (sym = find_pc_function (SYMBOL_VALUE_ADDRESS (msymbol)))) {
		if (strcmp (SYMBOL_NAME (msymbol), SYMBOL_NAME (sym)) == 0) {
		  /* found a high-level method sym: swap it into the 
		     lower part of sym_arr (below num_debuggable) */
		  sym_arr[matches++] = sym_arr[*num_debuggable];
		  sym_arr[(*num_debuggable)++] = sym;
		} else {
		  warning ("debugging symbol \"%s\" does not match selector; ignoring", SYMBOL_NAME (sym));
		  sym_arr[matches++] = (struct symbol *) msymbol;
		}
	      } else {
		/* found a non-debuggable method symbol */
		sym_arr[matches++] = (struct symbol *) msymbol;
	      }
	    }
	  }
	else if (!fn_match && strcmp(SYMBOL_NAME (msymbol), selector) == 0)
	  { /* found a matching function (we only want one) */
	    sym_arr[matches++] = fn_match = (struct symbol *) msymbol;
	  }
      }
    }

  if (matches > 1)
    {
      if (num_debuggable)	/* sort two separate sections of sym_arr */
	{
	  if (*num_debuggable > 1)		/* sort debuggable symbols */
	    qsort (sym_arr, *num_debuggable, 
		   sizeof (struct minimal_symbol *), compare_classes);

	  if (matches - *num_debuggable > 1)	/* sort minimal_symbols */
	    qsort (&sym_arr[*num_debuggable], matches - *num_debuggable, 
		   sizeof (struct minimal_symbol *), compare_classes);
	}
      else
	qsort (sym_arr, matches, 		/* sort everybody */
	       sizeof (struct minimal_symbol *), compare_classes);
    }

  sym_arr[matches] = 0;		/* terminate the sym_arr list */
  return  matches;
}

void print_object_command (args, from_tty)
     char *args;
     int from_tty;
{
  value_ptr object, function, description;
  CORE_ADDR string_addr;
  int i = 0;
  char c = -1;

  if (!args || !*args)
    error ("print-object requires an argument (an Objective-C object)");

  if (!(object = parse_and_eval (args)))
    error ("invalid expression: <%s>", args);

  if (!(function = find_function_in_inferior ("_NSPrintForDebugger")))
    error ("can't find _NSPrintForDebugger in child process");

  description = call_function_by_hand (function, 1, &object);

  if ((string_addr = value_as_long (description)) == 0)
    error ("object returns null description");

  read_memory (string_addr + i++, &c, 1);
  if (c != '\0')
    do
      { /* read and print characters up to EOS */
	QUIT;
	printf_filtered ("%c", c);
	read_memory (string_addr + i++, &c, 1);
      } while (c != 0);
  else
    printf_filtered("<object returns empty description>");
  printf_filtered ("\n");
}

/* The data structure 'methcalls' is used to detect method calls
 * (thru ObjC runtime lib functions objc_msgSend, objc_msgSendSuper, etc.),
 * and ultimately find the method being called.
 */

struct objc_methcall {
  char *name;
  CORE_ADDR (*stop_at) (CORE_ADDR); /* should return instance method to be called */
  CORE_ADDR begin;		    /* start of pc range corresponding to method invocation */
  CORE_ADDR end;		    /* end of pc range corresponding to method invocation */
};

static int resolve_msgsend PARAMS ((CORE_ADDR pc, CORE_ADDR *new_pc));
static int resolve_msgsend_stret PARAMS ((CORE_ADDR pc, CORE_ADDR *new_pc));
static int resolve_msgsend_super PARAMS ((CORE_ADDR pc, CORE_ADDR *new_pc));
static int resolve_msgsend_super_stret PARAMS ((CORE_ADDR pc, CORE_ADDR *new_pc));

static struct objc_methcall methcalls[] = {
  { "_objc_msgSend", resolve_msgsend, 0, 0},
  { "_objc_msgSend_stret", resolve_msgsend_stret, 0, 0},
  { "_objc_msgSendSuper", resolve_msgsend_super, 0, 0},
  { "_objc_msgSendSuper_stret", resolve_msgsend_super_stret, 0, 0},
  { "_objc_getClass", NULL, 0, 0},
  { "_objc_getMetaClass", NULL, 0, 0}
};

#define nmethcalls (sizeof (methcalls) / sizeof (methcalls[0]))

/* The following function, "find_objc_msgsend", fills in the data structure
 * "objc_msgs" by finding the addresses of each of the (currently four) 
 * functions that it holds (of which objc_msgSend is the first).  This
 * must be called each time symbols are loaded, in case the functions
 * have moved for some reason.
 */

void find_objc_msgsend ()
{
  unsigned int i;
  for (i = 0; i < nmethcalls; i++) {

    struct minimal_symbol *func;

    /* try both with and without underscore */
    func = lookup_minimal_symbol (methcalls[i].name, NULL, NULL);
    if ((func == NULL) && (methcalls[i].name[0] == '_')) {
      func = lookup_minimal_symbol (methcalls[i].name + 1, NULL, NULL);
    }
    if (func == NULL) { 
      methcalls[i].begin = 0;
      methcalls[i].end = 0;
      continue; 
    }
    
    methcalls[i].begin = SYMBOL_VALUE_ADDRESS (func);
    do {
      methcalls[i].end = SYMBOL_VALUE_ADDRESS (++func);
    } while (methcalls[i].begin == methcalls[i].end);
  }
}

/* find_objc_msgcall (replaces pc_off_limits)
 *
 * ALL that this function now does is to determine whether the input 
 * address ("pc") is the address of one of the Objective C message
 * dispatch functions (mainly objc_msgSend or objc_msgSendSuper), and
 * if so, it returns the address of the method that will be called.
 *
 * The old function "pc_off_limits" used to do a lot of other things
 * in addition, such as detecting shared library jump stubs and 
 * returning the address of the shlib function that would be called.
 * That functionality has been moved into the SKIP_TRAMPOLINE_CODE
 * and IN_SOLIB_TRAMPOLINE macros, which are resolved in the target-
 * dependent modules.
 */

struct objc_submethod_helper_data {
  CORE_ADDR (*f) (CORE_ADDR, CORE_ADDR *);
  CORE_ADDR pc;
  CORE_ADDR *new_pc;
};

int find_objc_msgcall_submethod_helper (arg)
     PTR arg;
{
  struct objc_submethod_helper_data *s = (struct objc_submethod_helper_data *) arg;
  if (s->f (s->pc, s->new_pc) == 0) { 
    return 1;
  } else {
    return 0;
  }
}

int find_objc_msgcall_submethod (f, pc, new_pc)
     CORE_ADDR (*f) (CORE_ADDR, CORE_ADDR *);
     CORE_ADDR pc;
     CORE_ADDR *new_pc;
{
  struct objc_submethod_helper_data s;

  s.f = f;
  s.pc = pc;
  s.new_pc = new_pc;

  if (catch_errors (find_objc_msgcall_submethod_helper,
		   (PTR) &s,
		   "Unable to determine target of Objective-C method call (ignoring):\n",
		   RETURN_MASK_ALL) == 0) {
    return 1;
  } else {
    return 0;
  }
}

int find_objc_msgcall (pc, new_pc)
     CORE_ADDR pc;
     CORE_ADDR *new_pc;
{
  unsigned int i;

  find_objc_msgsend ();
  if (new_pc != NULL) { *new_pc = 0; }

  for (i = 0; i < nmethcalls; i++) {
    if ((pc >= methcalls[i].begin) && (pc < methcalls[i].end)) {
      if (methcalls[i].stop_at != NULL) {
	return find_objc_msgcall_submethod (methcalls[i].stop_at, pc, new_pc);
      } else {
	return 0;
      }
    }
  }
  return 0;
}

void
_initialize_objc_language ()
{
  add_language (&objc_language_defn);
  add_info ("selectors", selectors_info, 	/* INFO SELECTORS command */
	    "All Objective C selectors, or those matching REGEXP.");
  add_info ("classes", classes_info, 		/* INFO CLASSES   command */
	    "All Objective C classes, or those matching REGEXP.");
  add_com ("print-object", class_vars, print_object_command, 
	   "Ask an Objective-C object to print itself.\n");
  add_com_alias ("po", "print-object", class_vars, 1);
}

#if defined (TARGET_POWERPC)
static unsigned long FETCH_ARGUMENT (int i)
{
  return read_register (3 + i);
}
#elif defined (TARGET_I386)
static unsigned long FETCH_ARGUMENT (int i)
{
  CORE_ADDR stack = read_register (SP_REGNUM);
  return read_memory_unsigned_integer (stack + (4 * (i + 1)), 4);
}
#elif defined (__sparc__)
static unsigned long FETCH_ARGUMENT (int i)
{
  return read_register (O0_REGNUM + i);
}
#elif defined (__hppa__) || defined (__hppa)
static unsigned long FETCH_ARGUMENT (int i)
{
  return read_register (R0_REGNUM + 26 - i);
}
#else
#error unknown architecture
#endif

#if defined (__hppa__) || defined (__hppa)
static CORE_ADDR CONVERT_FUNCPTR (CORE_ADDR pc)
{
  if (pc & 0x2) {
    pc = (CORE_ADDR) read_memory_integer (pc & ~0x3, 4);
  }
  return pc;
}
#else
static CORE_ADDR CONVERT_FUNCPTR (CORE_ADDR pc)
{
  return pc;
}
#endif

static void read_objc_method (addr, method)
     CORE_ADDR addr;
     struct objc_method *method;
{
  method->name = read_memory_unsigned_integer (addr, 4);
  method->types = read_memory_unsigned_integer (addr + 4, 4);
  method->imp = read_memory_unsigned_integer (addr + 8, 4);
}

static unsigned long read_objc_methlist_nmethods (addr)
     CORE_ADDR addr;
{
  return read_memory_unsigned_integer (addr + 4, 4);
}

static void read_objc_methlist_method (addr, num, method)
     CORE_ADDR addr;
     unsigned long num;
     struct objc_method *method;
{
  assert (num < read_objc_methlist_nmethods (addr));
  read_objc_method (addr + 8 + (12 * num), method);
}
  
static void read_objc_object (addr, object)
  CORE_ADDR addr;
  struct objc_object *object;
{
  object->isa = read_memory_unsigned_integer (addr, 4);
}

static void read_objc_super (addr, super)
  CORE_ADDR addr;
  struct objc_super *super;
{
  super->receiver = read_memory_unsigned_integer (addr, 4);
  super->class = read_memory_unsigned_integer (addr + 4, 4);
};

static void read_objc_class (addr, class)
  CORE_ADDR addr;
  struct objc_class *class;
{
  class->isa = read_memory_unsigned_integer (addr, 4);
  class->super_class = read_memory_unsigned_integer (addr + 4, 4);
  class->name = read_memory_unsigned_integer (addr + 8, 4);
  class->version = read_memory_unsigned_integer (addr + 12, 4);
  class->info = read_memory_unsigned_integer (addr + 16, 4);
  class->instance_size = read_memory_unsigned_integer (addr + 18, 4);
  class->ivars = read_memory_unsigned_integer (addr + 24, 4);
  class->methods = read_memory_unsigned_integer (addr + 28, 4);
  class->cache = read_memory_unsigned_integer (addr + 32, 4);
  class->protocols = read_memory_unsigned_integer (addr + 36, 4);
}

CORE_ADDR
find_implementation_from_class (class, sel)
     CORE_ADDR class;
     CORE_ADDR sel;
{
  CORE_ADDR subclass = class;

  while (subclass != 0) {

    struct objc_class class_str;
    unsigned mlistnum = 0;

    read_objc_class (subclass, &class_str);

    for (;;) {
      CORE_ADDR mlist;
      unsigned long nmethods;
      unsigned long i;
      
      mlist = read_memory_unsigned_integer (class_str.methods + (4 * mlistnum), 4);
      if (mlist == 0) { break; }

      nmethods = read_objc_methlist_nmethods (mlist);

      for (i = 0; i < nmethods; i++) {
	struct objc_method meth_str;
	read_objc_methlist_method (mlist, i, &meth_str);

#if 0
	fprintf (stderr, "checking method 0x%lx against selector 0x%lx\n", meth_str.name, sel);
#endif

	if (meth_str.name == sel) {
	  return CONVERT_FUNCPTR (meth_str.imp);
	}
      }

      mlistnum++;
    }
      
    subclass = class_str.super_class;
  }

  return 0;
}

CORE_ADDR
find_implementation (object, sel)
     CORE_ADDR object;
     CORE_ADDR sel;
{
  struct objc_object ostr;

  if (object == 0) { return 0; }
  read_objc_object (object, &ostr);
  if (ostr.isa == 0) { return 0; }

  return find_implementation_from_class (ostr.isa, sel);
}

static int
resolve_msgsend (pc, new_pc)
     CORE_ADDR pc;
     CORE_ADDR *new_pc;
{
  CORE_ADDR object;
  CORE_ADDR sel;
  CORE_ADDR res;

  object = FETCH_ARGUMENT (0);
  sel = FETCH_ARGUMENT (1);

  res = find_implementation (object, sel);
  if (new_pc != 0) { *new_pc = res; }
  if (res == 0) { return 1; }
  return 0;
}

static int
resolve_msgsend_stret (pc, new_pc)
     CORE_ADDR pc;
     CORE_ADDR *new_pc;
{
  CORE_ADDR object;
  CORE_ADDR sel;
  CORE_ADDR res;

  object = FETCH_ARGUMENT (1);
  sel = FETCH_ARGUMENT (2);

  res = find_implementation (object, sel);
  if (new_pc != 0) { *new_pc = res; }
  if (res == 0) { return 1; }
  return 0;
}

static int
resolve_msgsend_super (pc, new_pc)
     CORE_ADDR pc;
     CORE_ADDR *new_pc;
{
  struct objc_super sstr;

  CORE_ADDR super;
  CORE_ADDR sel;
  CORE_ADDR res;

  super = FETCH_ARGUMENT (0);
  sel = FETCH_ARGUMENT (1);

  read_objc_super (super, &sstr);
  if (sstr.class == 0) { return 0; }
  
  res = find_implementation_from_class (sstr.class, sel);
  if (new_pc != 0) { *new_pc = res; }
  if (res == 0) { return 1; }
  return 0;
}

static int
resolve_msgsend_super_stret (pc, new_pc)
     CORE_ADDR pc;
     CORE_ADDR *new_pc;
{
  struct objc_super sstr;

  CORE_ADDR super;
  CORE_ADDR sel;
  CORE_ADDR res;

  super = FETCH_ARGUMENT (1);
  sel = FETCH_ARGUMENT (2);

  read_objc_super (super, &sstr);
  if (sstr.class == 0) { return 0; }
  
  res = find_implementation_from_class (sstr.class, sel);
  if (new_pc != 0) { *new_pc = res; }
  if (res == 0) { return 1; }
  return 0;
}
