/* Output generating routines for GDB CLI.
   Copyright 1999-2000 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.
   Written by Fernando Nasser for Cygnus.

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
#include "ui-out.h"
#include "cli-out.h"

/* Convenience macro for allocting typesafe memory. */

#ifndef XMALLOC
#define XMALLOC(TYPE) (TYPE*) xmalloc (sizeof (TYPE))
#endif

struct ui_out_data
  {
    struct ui_file *stream;
  };

/* These are the CLI output functions */

static void cli_table_begin (struct ui_out *uiout, int nbrofcols, char *tblid);
static void cli_table_body (struct ui_out *uiout);
static void cli_table_end (struct ui_out *uiout);
static void cli_table_header (struct ui_out *uiout, int width,
			      enum ui_align alig, char *colhdr);
static void cli_list_begin (struct ui_out *uiout, int list_flag, char *lstid);
static void cli_list_end (struct ui_out *uiout, int list_flag);
static void cli_field_int (struct ui_out *uiout, int fldno, int width,
			   enum ui_align alig, char *fldname, int value);
static void cli_field_skip (struct ui_out *uiout, int fldno, int width,
			    enum ui_align alig, char *fldname);
static void cli_field_string (struct ui_out *uiout, int fldno, int width,
			   enum ui_align alig, char *fldname,
			      const char *string);
static void cli_field_fmt (struct ui_out *uiout, int fldno,
			   int width, enum ui_align align,
			   char *fldname, char *format, va_list args);
static void cli_spaces (struct ui_out *uiout, int numspaces);
static void cli_text (struct ui_out *uiout, char *string);
static void cli_text_fmt (struct ui_out *uiout, char *format, va_list args);
static void cli_message (struct ui_out *uiout, int verbosity, char *format,
			 va_list args);
static void cli_wrap_hint (struct ui_out *uiout, char *identstring);
static void cli_flush (struct ui_out *uiout);

/* This is the CLI ui-out implementation functions vector */

/* FIXME: This can be initialized dynamically after default is set to
   handle initial output in main.c */

static struct ui_out_impl cli_ui_out_impl =
{
  cli_table_begin,
  cli_table_body,
  cli_table_end,
  cli_table_header,
  cli_list_begin,
  cli_list_end,
  cli_field_int,
  cli_field_skip,
  cli_field_string,
  cli_field_fmt,
  cli_spaces,
  cli_text,
  cli_text_fmt,
  cli_message,
  cli_wrap_hint,
  cli_flush
};

/* Prototypes for local functions */

extern void _initialize_cli_out PARAMS ((void));

static void field_separator (void);

static void out_field_fmt (struct ui_out *uiout, int fldno, char *fldname,
			   char *format,...);

/* local variables */

/* (none yet) */

/* Mark beginning of a table */

void
cli_table_begin (uiout, nbrofcols, tblid)
     struct ui_out *uiout;
     int nbrofcols;
     char *tblid;
{
}

/* Mark beginning of a table body */

void
cli_table_body (uiout)
     struct ui_out *uiout;
{
  /* first, close the table header line */
  cli_text (uiout, "\n");
}

/* Mark end of a table */

void
cli_table_end (uiout)
     struct ui_out *uiout;
{
}

/* Specify table header */

void
cli_table_header (uiout, width, alignment, colhdr)
     struct ui_out *uiout;
     int width;
     enum ui_align alignment;
     char *colhdr;
{
  cli_field_string (uiout, 0, width, alignment, 0, colhdr);
}

/* Mark beginning of a list */

void
cli_list_begin (uiout, list_flag, lstid)
     struct ui_out *uiout;
     int list_flag;
     char *lstid;
{
}

/* Mark end of a list */

void
cli_list_end (uiout, list_flag)
     struct ui_out *uiout;
     int list_flag;
{
}

/* output an int field */

void
cli_field_int (uiout, fldno, width, alignment, fldname, value)
     struct ui_out *uiout;
     int fldno;
     int width;
     enum ui_align alignment;
     char *fldname;
     int value;
{
  char buffer[20];		/* FIXME: how many chars long a %d can become? */

  sprintf (buffer, "%d", value);
  cli_field_string (uiout, fldno, width, alignment, fldname, buffer);
}

/* used to ommit a field */

void
cli_field_skip (uiout, fldno, width, alignment, fldname)
     struct ui_out *uiout;
     int fldno;
     int width;
     enum ui_align alignment;
     char *fldname;
{
  cli_field_string (uiout, fldno, width, alignment, fldname, "");
}

/* other specific cli_field_* end up here so alignment and field
   separators are both handled by cli_field_string */

void
cli_field_string (struct ui_out *uiout,
		  int fldno,
		  int width,
		  enum ui_align align,
		  char *fldname,
		  const char *string)
{
  int before = 0;
  int after = 0;

  if ((align != ui_noalign) && string)
    {
      before = width - strlen (string);
      if (before <= 0)
	before = 0;
      else
	{
	  if (align == ui_right)
	    after = 0;
	  else if (align == ui_left)
	    {
	      after = before;
	      before = 0;
	    }
	  else
	    /* ui_center */
	    {
	      after = before / 2;
	      before -= after;
	    }
	}
    }

  if (before)
    ui_out_spaces (uiout, before);
  if (string)
    out_field_fmt (uiout, fldno, fldname, "%s", string);
  if (after)
    ui_out_spaces (uiout, after);

  if (align != ui_noalign)
    field_separator ();
}

/* This is the only field function that does not align */

void
cli_field_fmt (struct ui_out *uiout, int fldno,
	       int width, enum ui_align align,
	       char *fldname, char *format, va_list args)
{
  struct ui_out_data *data = ui_out_data (uiout);
  vfprintf_filtered (data->stream, format, args);

  if (align != ui_noalign)
    field_separator ();
}

void
cli_spaces (uiout, numspaces)
     struct ui_out *uiout;
     int numspaces;
{
  struct ui_out_data *data = ui_out_data (uiout);
  print_spaces_filtered (numspaces, data->stream);
}

void
cli_text (uiout, string)
     struct ui_out *uiout;
     char *string;
{
  struct ui_out_data *data = ui_out_data (uiout);
  fputs_filtered (string, data->stream);
}

/* VARARGS */
void
cli_text_fmt (struct ui_out *uiout, char *format, va_list args)
{
  struct ui_out_data *data = ui_out_data (uiout);
  vfprintf_filtered (data->stream, format, args);
}

void
cli_message (struct ui_out *uiout, int verbosity, char *format, va_list args)
{
  struct ui_out_data *data = ui_out_data (uiout);
  if (ui_out_get_verblvl (uiout) >= verbosity)
    vfprintf_unfiltered (data->stream, format, args);
}

void
cli_wrap_hint (uiout, identstring)
     struct ui_out *uiout;
     char *identstring;
{
  wrap_here (identstring);
}

void
cli_flush (uiout)
     struct ui_out *uiout;
{
  struct ui_out_data *data = ui_out_data (uiout);
  gdb_flush (data->stream);
}

/* local functions */

/* Like cli_field_fmt, but takes a variable number of args
   and makes a va_list and does not insert a separator */

/* VARARGS */
static void
out_field_fmt (struct ui_out *uiout, int fldno, char *fldname,
	       char *format,...)
{
  struct ui_out_data *data = ui_out_data (uiout);
  va_list args;

  va_start (args, format);
  vfprintf_filtered (data->stream, format, args);

  va_end (args);
}

/* access to ui_out format private members */

static void
field_separator ()
{
  struct ui_out_data *data = ui_out_data (uiout);
  fputc_filtered (' ', data->stream);
}

/* initalize private members at startup */

struct ui_out *
cli_out_new (struct ui_file *stream)
{
  int flags = ui_source_list;

  struct ui_out_data *data = XMALLOC (struct ui_out_data);
  data->stream = stream;
  return ui_out_new (&cli_ui_out_impl, data, flags);
}

/* standard gdb initialization hook */
void
_initialize_cli_out ()
{
  /* nothing needs to be done */
}
