/* String search routines for GNU Emacs.
   Copyright (C) 1985, 86, 87, 93, 94, 97, 1998 Free Software Foundation, Inc.

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


#include <config.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#include "lisp.h"
#include "syntax.h"
#include "category.h"
#include "buffer.h"
#include "charset.h"
#include "region-cache.h"
#include "commands.h"
#include "blockinput.h"
#include "intervals.h"

#include <sys/types.h>
#include "regex.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define REGEXP_CACHE_SIZE 20

/* If the regexp is non-nil, then the buffer contains the compiled form
   of that regexp, suitable for searching.  */
struct regexp_cache
{
  struct regexp_cache *next;
  Lisp_Object regexp;
  struct re_pattern_buffer buf;
  char fastmap[0400];
  /* Nonzero means regexp was compiled to do full POSIX backtracking.  */
  char posix;
};

/* The instances of that struct.  */
struct regexp_cache searchbufs[REGEXP_CACHE_SIZE];

/* The head of the linked list; points to the most recently used buffer.  */
struct regexp_cache *searchbuf_head;


/* Every call to re_match, etc., must pass &search_regs as the regs
   argument unless you can show it is unnecessary (i.e., if re_match
   is certainly going to be called again before region-around-match
   can be called).

   Since the registers are now dynamically allocated, we need to make
   sure not to refer to the Nth register before checking that it has
   been allocated by checking search_regs.num_regs.

   The regex code keeps track of whether it has allocated the search
   buffer using bits in the re_pattern_buffer.  This means that whenever
   you compile a new pattern, it completely forgets whether it has
   allocated any registers, and will allocate new registers the next
   time you call a searching or matching function.  Therefore, we need
   to call re_set_registers after compiling a new pattern or after
   setting the match registers, so that the regex functions will be
   able to free or re-allocate it properly.  */
static struct re_registers search_regs;

/* The buffer in which the last search was performed, or
   Qt if the last search was done in a string;
   Qnil if no searching has been done yet.  */
static Lisp_Object last_thing_searched;

/* error condition signaled when regexp compile_pattern fails */

Lisp_Object Qinvalid_regexp;

static void set_search_regs ();
static void save_search_regs ();
static int simple_search ();
static int boyer_moore ();
static int search_buffer ();

static void
matcher_overflow ()
{
  error ("Stack overflow in regexp matcher");
}

#ifdef __STDC__
#define CONST const
#else
#define CONST
#endif

/* Compile a regexp and signal a Lisp error if anything goes wrong.
   PATTERN is the pattern to compile.
   CP is the place to put the result.
   TRANSLATE is a translation table for ignoring case, or nil for none.
   REGP is the structure that says where to store the "register"
   values that will result from matching this pattern.
   If it is 0, we should compile the pattern not to record any
   subexpression bounds.
   POSIX is nonzero if we want full backtracking (POSIX style)
   for this pattern.  0 means backtrack only enough to get a valid match.
   MULTIBYTE is nonzero if we want to handle multibyte characters in
   PATTERN.  0 means all multibyte characters are recognized just as
   sequences of binary data.  */

static void
compile_pattern_1 (cp, pattern, translate, regp, posix, multibyte)
     struct regexp_cache *cp;
     Lisp_Object pattern;
     Lisp_Object translate;
     struct re_registers *regp;
     int posix;
     int multibyte;
{
  unsigned char *raw_pattern;
  int raw_pattern_size;
  char *val;
  reg_syntax_t old;

  /* MULTIBYTE says whether the text to be searched is multibyte.
     We must convert PATTERN to match that, or we will not really
     find things right.  */

  if (multibyte == STRING_MULTIBYTE (pattern))
    {
      raw_pattern = (unsigned char *) XSTRING (pattern)->data;
      raw_pattern_size = STRING_BYTES (XSTRING (pattern));
    }
  else if (multibyte)
    {
      raw_pattern_size = count_size_as_multibyte (XSTRING (pattern)->data,
						  XSTRING (pattern)->size);
      raw_pattern = (unsigned char *) alloca (raw_pattern_size + 1);
      copy_text (XSTRING (pattern)->data, raw_pattern,
		 XSTRING (pattern)->size, 0, 1);
    }
  else
    {
      /* Converting multibyte to single-byte.

	 ??? Perhaps this conversion should be done in a special way
	 by subtracting nonascii-insert-offset from each non-ASCII char,
	 so that only the multibyte chars which really correspond to
	 the chosen single-byte character set can possibly match.  */
      raw_pattern_size = XSTRING (pattern)->size;
      raw_pattern = (unsigned char *) alloca (raw_pattern_size + 1);
      copy_text (XSTRING (pattern)->data, raw_pattern,
		 STRING_BYTES (XSTRING (pattern)), 1, 0);
    }

  cp->regexp = Qnil;
  cp->buf.translate = (! NILP (translate) ? translate : make_number (0));
  cp->posix = posix;
  cp->buf.multibyte = multibyte;
  BLOCK_INPUT;
  old = re_set_syntax (RE_SYNTAX_EMACS
		       | (posix ? 0 : RE_NO_POSIX_BACKTRACKING));
  val = (char *) re_compile_pattern ((char *)raw_pattern,
				     raw_pattern_size, &cp->buf);
  re_set_syntax (old);
  UNBLOCK_INPUT;
  if (val)
    Fsignal (Qinvalid_regexp, Fcons (build_string (val), Qnil));

  cp->regexp = Fcopy_sequence (pattern);
}

/* Shrink each compiled regexp buffer in the cache
   to the size actually used right now.
   This is called from garbage collection.  */

void
shrink_regexp_cache ()
{
  struct regexp_cache *cp, **cpp;

  for (cp = searchbuf_head; cp != 0; cp = cp->next)
    {
      cp->buf.allocated = cp->buf.used;
      cp->buf.buffer
	= (unsigned char *) realloc (cp->buf.buffer, cp->buf.used);
    }
}

/* Compile a regexp if necessary, but first check to see if there's one in
   the cache.
   PATTERN is the pattern to compile.
   TRANSLATE is a translation table for ignoring case, or nil for none.
   REGP is the structure that says where to store the "register"
   values that will result from matching this pattern.
   If it is 0, we should compile the pattern not to record any
   subexpression bounds.
   POSIX is nonzero if we want full backtracking (POSIX style)
   for this pattern.  0 means backtrack only enough to get a valid match.  */

struct re_pattern_buffer *
compile_pattern (pattern, regp, translate, posix, multibyte)
     Lisp_Object pattern;
     struct re_registers *regp;
     Lisp_Object translate;
     int posix, multibyte;
{
  struct regexp_cache *cp, **cpp;

  for (cpp = &searchbuf_head; ; cpp = &cp->next)
    {
      cp = *cpp;
      if (XSTRING (cp->regexp)->size == XSTRING (pattern)->size
	  && !NILP (Fstring_equal (cp->regexp, pattern))
	  && EQ (cp->buf.translate, (! NILP (translate) ? translate : make_number (0)))
	  && cp->posix == posix
	  && cp->buf.multibyte == multibyte)
	break;

      /* If we're at the end of the cache, compile into the last cell.  */
      if (cp->next == 0)
	{
	  compile_pattern_1 (cp, pattern, translate, regp, posix, multibyte);
	  break;
	}
    }

  /* When we get here, cp (aka *cpp) contains the compiled pattern,
     either because we found it in the cache or because we just compiled it.
     Move it to the front of the queue to mark it as most recently used.  */
  *cpp = cp->next;
  cp->next = searchbuf_head;
  searchbuf_head = cp;

  /* Advise the searching functions about the space we have allocated
     for register data.  */
  if (regp)
    re_set_registers (&cp->buf, regp, regp->num_regs, regp->start, regp->end);

  return &cp->buf;
}

/* Error condition used for failing searches */
Lisp_Object Qsearch_failed;

Lisp_Object
signal_failure (arg)
     Lisp_Object arg;
{
  Fsignal (Qsearch_failed, Fcons (arg, Qnil));
  return Qnil;
}

static Lisp_Object
looking_at_1 (string, posix)
     Lisp_Object string;
     int posix;
{
  Lisp_Object val;
  unsigned char *p1, *p2;
  int s1, s2;
  register int i;
  struct re_pattern_buffer *bufp;

  if (running_asynch_code)
    save_search_regs ();

  CHECK_STRING (string, 0);
  bufp = compile_pattern (string, &search_regs,
			  (!NILP (current_buffer->case_fold_search)
			   ? DOWNCASE_TABLE : Qnil),
			  posix,
			  !NILP (current_buffer->enable_multibyte_characters));

  immediate_quit = 1;
  QUIT;			/* Do a pending quit right away, to avoid paradoxical behavior */

  /* Get pointers and sizes of the two strings
     that make up the visible portion of the buffer. */

  p1 = BEGV_ADDR;
  s1 = GPT_BYTE - BEGV_BYTE;
  p2 = GAP_END_ADDR;
  s2 = ZV_BYTE - GPT_BYTE;
  if (s1 < 0)
    {
      p2 = p1;
      s2 = ZV_BYTE - BEGV_BYTE;
      s1 = 0;
    }
  if (s2 < 0)
    {
      s1 = ZV_BYTE - BEGV_BYTE;
      s2 = 0;
    }

  re_match_object = Qnil;
  
  i = re_match_2 (bufp, (char *) p1, s1, (char *) p2, s2,
		  PT_BYTE - BEGV_BYTE, &search_regs,
		  ZV_BYTE - BEGV_BYTE);
  if (i == -2)
    matcher_overflow ();

  val = (0 <= i ? Qt : Qnil);
  if (i >= 0)
    for (i = 0; i < search_regs.num_regs; i++)
      if (search_regs.start[i] >= 0)
	{
	  search_regs.start[i]
	    = BYTE_TO_CHAR (search_regs.start[i] + BEGV_BYTE);
	  search_regs.end[i]
	    = BYTE_TO_CHAR (search_regs.end[i] + BEGV_BYTE);
	}
  XSETBUFFER (last_thing_searched, current_buffer);
  immediate_quit = 0;
  return val;
}

DEFUN ("looking-at", Flooking_at, Slooking_at, 1, 1, 0,
  "Return t if text after point matches regular expression REGEXP.\n\
This function modifies the match data that `match-beginning',\n\
`match-end' and `match-data' access; save and restore the match\n\
data if you want to preserve them.")
  (regexp)
     Lisp_Object regexp;
{
  return looking_at_1 (regexp, 0);
}

DEFUN ("posix-looking-at", Fposix_looking_at, Sposix_looking_at, 1, 1, 0,
  "Return t if text after point matches regular expression REGEXP.\n\
Find the longest match, in accord with Posix regular expression rules.\n\
This function modifies the match data that `match-beginning',\n\
`match-end' and `match-data' access; save and restore the match\n\
data if you want to preserve them.")
  (regexp)
     Lisp_Object regexp;
{
  return looking_at_1 (regexp, 1);
}

static Lisp_Object
string_match_1 (regexp, string, start, posix)
     Lisp_Object regexp, string, start;
     int posix;
{
  int val;
  struct re_pattern_buffer *bufp;
  int pos, pos_byte;
  int i;

  if (running_asynch_code)
    save_search_regs ();

  CHECK_STRING (regexp, 0);
  CHECK_STRING (string, 1);

  if (NILP (start))
    pos = 0, pos_byte = 0;
  else
    {
      int len = XSTRING (string)->size;

      CHECK_NUMBER (start, 2);
      pos = XINT (start);
      if (pos < 0 && -pos <= len)
	pos = len + pos;
      else if (0 > pos || pos > len)
	args_out_of_range (string, start);
      pos_byte = string_char_to_byte (string, pos);
    }

  bufp = compile_pattern (regexp, &search_regs,
			  (!NILP (current_buffer->case_fold_search)
			   ? DOWNCASE_TABLE : Qnil),
			  posix,
			  STRING_MULTIBYTE (string));
  immediate_quit = 1;
  re_match_object = string;
  
  val = re_search (bufp, (char *) XSTRING (string)->data,
		   STRING_BYTES (XSTRING (string)), pos_byte,
		   STRING_BYTES (XSTRING (string)) - pos_byte,
		   &search_regs);
  immediate_quit = 0;
  last_thing_searched = Qt;
  if (val == -2)
    matcher_overflow ();
  if (val < 0) return Qnil;

  for (i = 0; i < search_regs.num_regs; i++)
    if (search_regs.start[i] >= 0)
      {
	search_regs.start[i]
	  = string_byte_to_char (string, search_regs.start[i]);
	search_regs.end[i]
	  = string_byte_to_char (string, search_regs.end[i]);
      }

  return make_number (string_byte_to_char (string, val));
}

DEFUN ("string-match", Fstring_match, Sstring_match, 2, 3, 0,
  "Return index of start of first match for REGEXP in STRING, or nil.\n\
If third arg START is non-nil, start search at that index in STRING.\n\
For index of first char beyond the match, do (match-end 0).\n\
`match-end' and `match-beginning' also give indices of substrings\n\
matched by parenthesis constructs in the pattern.")
  (regexp, string, start)
     Lisp_Object regexp, string, start;
{
  return string_match_1 (regexp, string, start, 0);
}

DEFUN ("posix-string-match", Fposix_string_match, Sposix_string_match, 2, 3, 0,
  "Return index of start of first match for REGEXP in STRING, or nil.\n\
Find the longest match, in accord with Posix regular expression rules.\n\
If third arg START is non-nil, start search at that index in STRING.\n\
For index of first char beyond the match, do (match-end 0).\n\
`match-end' and `match-beginning' also give indices of substrings\n\
matched by parenthesis constructs in the pattern.")
  (regexp, string, start)
     Lisp_Object regexp, string, start;
{
  return string_match_1 (regexp, string, start, 1);
}

/* Match REGEXP against STRING, searching all of STRING,
   and return the index of the match, or negative on failure.
   This does not clobber the match data.  */

int
fast_string_match (regexp, string)
     Lisp_Object regexp, string;
{
  int val;
  struct re_pattern_buffer *bufp;

  bufp = compile_pattern (regexp, 0, Qnil,
			  0, STRING_MULTIBYTE (string));
  immediate_quit = 1;
  re_match_object = string;
  
  val = re_search (bufp, (char *) XSTRING (string)->data,
		   STRING_BYTES (XSTRING (string)), 0,
		   STRING_BYTES (XSTRING (string)), 0);
  immediate_quit = 0;
  return val;
}

/* Match REGEXP against STRING, searching all of STRING ignoring case,
   and return the index of the match, or negative on failure.
   This does not clobber the match data.
   We assume that STRING contains single-byte characters.  */

extern Lisp_Object Vascii_downcase_table;

int
fast_c_string_match_ignore_case (regexp, string)
     Lisp_Object regexp;
     char *string;
{
  int val;
  struct re_pattern_buffer *bufp;
  int len = strlen (string);

  regexp = string_make_unibyte (regexp);
  re_match_object = Qt;
  bufp = compile_pattern (regexp, 0,
			  Vascii_downcase_table, 0,
			  0);
  immediate_quit = 1;
  val = re_search (bufp, string, len, 0, len, 0);
  immediate_quit = 0;
  return val;
}

/* The newline cache: remembering which sections of text have no newlines.  */

/* If the user has requested newline caching, make sure it's on.
   Otherwise, make sure it's off.
   This is our cheezy way of associating an action with the change of
   state of a buffer-local variable.  */
static void
newline_cache_on_off (buf)
     struct buffer *buf;
{
  if (NILP (buf->cache_long_line_scans))
    {
      /* It should be off.  */
      if (buf->newline_cache)
        {
          free_region_cache (buf->newline_cache);
          buf->newline_cache = 0;
        }
    }
  else
    {
      /* It should be on.  */
      if (buf->newline_cache == 0)
        buf->newline_cache = new_region_cache ();
    }
}


/* Search for COUNT instances of the character TARGET between START and END.

   If COUNT is positive, search forwards; END must be >= START.
   If COUNT is negative, search backwards for the -COUNTth instance;
      END must be <= START.
   If COUNT is zero, do anything you please; run rogue, for all I care.

   If END is zero, use BEGV or ZV instead, as appropriate for the
   direction indicated by COUNT.

   If we find COUNT instances, set *SHORTAGE to zero, and return the
   position after the COUNTth match.  Note that for reverse motion
   this is not the same as the usual convention for Emacs motion commands.

   If we don't find COUNT instances before reaching END, set *SHORTAGE
   to the number of TARGETs left unfound, and return END.

   If ALLOW_QUIT is non-zero, set immediate_quit.  That's good to do
   except when inside redisplay.  */

int
scan_buffer (target, start, end, count, shortage, allow_quit)
     register int target;
     int start, end;
     int count;
     int *shortage;
     int allow_quit;
{
  struct region_cache *newline_cache;
  int direction; 

  if (count > 0)
    {
      direction = 1;
      if (! end) end = ZV;
    }
  else
    {
      direction = -1;
      if (! end) end = BEGV;
    }

  newline_cache_on_off (current_buffer);
  newline_cache = current_buffer->newline_cache;

  if (shortage != 0)
    *shortage = 0;

  immediate_quit = allow_quit;

  if (count > 0)
    while (start != end)
      {
        /* Our innermost scanning loop is very simple; it doesn't know
           about gaps, buffer ends, or the newline cache.  ceiling is
           the position of the last character before the next such
           obstacle --- the last character the dumb search loop should
           examine.  */
	int ceiling_byte = CHAR_TO_BYTE (end) - 1;
	int start_byte = CHAR_TO_BYTE (start);
	int tem;

        /* If we're looking for a newline, consult the newline cache
           to see where we can avoid some scanning.  */
        if (target == '\n' && newline_cache)
          {
            int next_change;
            immediate_quit = 0;
            while (region_cache_forward
                   (current_buffer, newline_cache, start_byte, &next_change))
              start_byte = next_change;
            immediate_quit = allow_quit;

            /* START should never be after END.  */
            if (start_byte > ceiling_byte)
              start_byte = ceiling_byte;

            /* Now the text after start is an unknown region, and
               next_change is the position of the next known region. */
            ceiling_byte = min (next_change - 1, ceiling_byte);
          }

        /* The dumb loop can only scan text stored in contiguous
           bytes. BUFFER_CEILING_OF returns the last character
           position that is contiguous, so the ceiling is the
           position after that.  */
	tem = BUFFER_CEILING_OF (start_byte);
	ceiling_byte = min (tem, ceiling_byte);

        {
          /* The termination address of the dumb loop.  */ 
          register unsigned char *ceiling_addr
	    = BYTE_POS_ADDR (ceiling_byte) + 1;
          register unsigned char *cursor
	    = BYTE_POS_ADDR (start_byte);
          unsigned char *base = cursor;

          while (cursor < ceiling_addr)
            {
              unsigned char *scan_start = cursor;

              /* The dumb loop.  */
              while (*cursor != target && ++cursor < ceiling_addr)
                ;

              /* If we're looking for newlines, cache the fact that
                 the region from start to cursor is free of them. */
              if (target == '\n' && newline_cache)
                know_region_cache (current_buffer, newline_cache,
                                   start_byte + scan_start - base,
                                   start_byte + cursor - base);

              /* Did we find the target character?  */
              if (cursor < ceiling_addr)
                {
                  if (--count == 0)
                    {
                      immediate_quit = 0;
                      return BYTE_TO_CHAR (start_byte + cursor - base + 1);
                    }
                  cursor++;
                }
            }

          start = BYTE_TO_CHAR (start_byte + cursor - base);
        }
      }
  else
    while (start > end)
      {
        /* The last character to check before the next obstacle.  */
	int ceiling_byte = CHAR_TO_BYTE (end);
	int start_byte = CHAR_TO_BYTE (start);
	int tem;

        /* Consult the newline cache, if appropriate.  */
        if (target == '\n' && newline_cache)
          {
            int next_change;
            immediate_quit = 0;
            while (region_cache_backward
                   (current_buffer, newline_cache, start_byte, &next_change))
              start_byte = next_change;
            immediate_quit = allow_quit;

            /* Start should never be at or before end.  */
            if (start_byte <= ceiling_byte)
              start_byte = ceiling_byte + 1;

            /* Now the text before start is an unknown region, and
               next_change is the position of the next known region. */
            ceiling_byte = max (next_change, ceiling_byte);
          }

        /* Stop scanning before the gap.  */
	tem = BUFFER_FLOOR_OF (start_byte - 1);
	ceiling_byte = max (tem, ceiling_byte);

        {
          /* The termination address of the dumb loop.  */
          register unsigned char *ceiling_addr = BYTE_POS_ADDR (ceiling_byte);
          register unsigned char *cursor = BYTE_POS_ADDR (start_byte - 1);
          unsigned char *base = cursor;

          while (cursor >= ceiling_addr)
            {
              unsigned char *scan_start = cursor;

              while (*cursor != target && --cursor >= ceiling_addr)
                ;

              /* If we're looking for newlines, cache the fact that
                 the region from after the cursor to start is free of them.  */
              if (target == '\n' && newline_cache)
                know_region_cache (current_buffer, newline_cache,
                                   start_byte + cursor - base,
                                   start_byte + scan_start - base);

              /* Did we find the target character?  */
              if (cursor >= ceiling_addr)
                {
                  if (++count >= 0)
                    {
                      immediate_quit = 0;
                      return BYTE_TO_CHAR (start_byte + cursor - base);
                    }
                  cursor--;
                }
            }

	  start = BYTE_TO_CHAR (start_byte + cursor - base);
        }
      }

  immediate_quit = 0;
  if (shortage != 0)
    *shortage = count * direction;
  return start;
}

/* Search for COUNT instances of a line boundary, which means either a
   newline or (if selective display enabled) a carriage return.
   Start at START.  If COUNT is negative, search backwards.

   We report the resulting position by calling TEMP_SET_PT_BOTH.

   If we find COUNT instances. we position after (always after,
   even if scanning backwards) the COUNTth match, and return 0.

   If we don't find COUNT instances before reaching the end of the
   buffer (or the beginning, if scanning backwards), we return
   the number of line boundaries left unfound, and position at
   the limit we bumped up against.

   If ALLOW_QUIT is non-zero, set immediate_quit.  That's good to do
   except in special cases.  */

int
scan_newline (start, start_byte, limit, limit_byte, count, allow_quit)
     int start, start_byte;
     int limit, limit_byte;
     register int count;
     int allow_quit;
{
  int direction = ((count > 0) ? 1 : -1);

  register unsigned char *cursor;
  unsigned char *base;

  register int ceiling;
  register unsigned char *ceiling_addr;

  int old_immediate_quit = immediate_quit;

  /* If we are not in selective display mode,
     check only for newlines.  */
  int selective_display = (!NILP (current_buffer->selective_display)
			   && !INTEGERP (current_buffer->selective_display));

  /* The code that follows is like scan_buffer
     but checks for either newline or carriage return.  */

  if (allow_quit)
    immediate_quit++;

  start_byte = CHAR_TO_BYTE (start);

  if (count > 0)
    {
      while (start_byte < limit_byte)
	{
	  ceiling =  BUFFER_CEILING_OF (start_byte);
	  ceiling = min (limit_byte - 1, ceiling);
	  ceiling_addr = BYTE_POS_ADDR (ceiling) + 1;
	  base = (cursor = BYTE_POS_ADDR (start_byte));
	  while (1)
	    {
	      while (*cursor != '\n' && ++cursor != ceiling_addr)
		;

	      if (cursor != ceiling_addr)
		{
		  if (--count == 0)
		    {
		      immediate_quit = old_immediate_quit;
		      start_byte = start_byte + cursor - base + 1;
		      start = BYTE_TO_CHAR (start_byte);
		      TEMP_SET_PT_BOTH (start, start_byte);
		      return 0;
		    }
		  else
		    if (++cursor == ceiling_addr)
		      break;
		}
	      else
		break;
	    }
	  start_byte += cursor - base;
	}
    }
  else
    {
      while (start_byte > limit_byte)
	{
	  ceiling = BUFFER_FLOOR_OF (start_byte - 1);
	  ceiling = max (limit_byte, ceiling);
	  ceiling_addr = BYTE_POS_ADDR (ceiling) - 1;
	  base = (cursor = BYTE_POS_ADDR (start_byte - 1) + 1);
	  while (1)
	    {
	      while (--cursor != ceiling_addr && *cursor != '\n')
		;

	      if (cursor != ceiling_addr)
		{
		  if (++count == 0)
		    {
		      immediate_quit = old_immediate_quit;
		      /* Return the position AFTER the match we found.  */
		      start_byte = start_byte + cursor - base + 1;
		      start = BYTE_TO_CHAR (start_byte);
		      TEMP_SET_PT_BOTH (start, start_byte);
		      return 0;
		    }
		}
	      else
		break;
	    }
	  /* Here we add 1 to compensate for the last decrement
	     of CURSOR, which took it past the valid range.  */
	  start_byte += cursor - base + 1;
	}
    }

  TEMP_SET_PT_BOTH (limit, limit_byte);
  immediate_quit = old_immediate_quit;

  return count * direction;
}

int
find_next_newline_no_quit (from, cnt)
     register int from, cnt;
{
  return scan_buffer ('\n', from, 0, cnt, (int *) 0, 0);
}

/* Like find_next_newline, but returns position before the newline,
   not after, and only search up to TO.  This isn't just
   find_next_newline (...)-1, because you might hit TO.  */

int
find_before_next_newline (from, to, cnt)
     int from, to, cnt;
{
  int shortage;
  int pos = scan_buffer ('\n', from, to, cnt, &shortage, 1);

  if (shortage == 0)
    pos--;
  
  return pos;
}

/* Subroutines of Lisp buffer search functions. */

static Lisp_Object
search_command (string, bound, noerror, count, direction, RE, posix)
     Lisp_Object string, bound, noerror, count;
     int direction;
     int RE;
     int posix;
{
  register int np;
  int lim, lim_byte;
  int n = direction;

  if (!NILP (count))
    {
      CHECK_NUMBER (count, 3);
      n *= XINT (count);
    }

  CHECK_STRING (string, 0);
  if (NILP (bound))
    {
      if (n > 0)
	lim = ZV, lim_byte = ZV_BYTE;
      else
	lim = BEGV, lim_byte = BEGV_BYTE;
    }
  else
    {
      CHECK_NUMBER_COERCE_MARKER (bound, 1);
      lim = XINT (bound);
      if (n > 0 ? lim < PT : lim > PT)
	error ("Invalid search bound (wrong side of point)");
      if (lim > ZV)
	lim = ZV, lim_byte = ZV_BYTE;
      else if (lim < BEGV)
	lim = BEGV, lim_byte = BEGV_BYTE;
      else
	lim_byte = CHAR_TO_BYTE (lim);
    }

  np = search_buffer (string, PT, PT_BYTE, lim, lim_byte, n, RE,
		      (!NILP (current_buffer->case_fold_search)
		       ? current_buffer->case_canon_table
		       : Qnil),
		      (!NILP (current_buffer->case_fold_search)
		       ? current_buffer->case_eqv_table
		       : Qnil),
		      posix);
  if (np <= 0)
    {
      if (NILP (noerror))
	return signal_failure (string);
      if (!EQ (noerror, Qt))
	{
	  if (lim < BEGV || lim > ZV)
	    abort ();
	  SET_PT_BOTH (lim, lim_byte);
	  return Qnil;
#if 0 /* This would be clean, but maybe programs depend on
	 a value of nil here.  */
	  np = lim;
#endif
	}
      else
	return Qnil;
    }

  if (np < BEGV || np > ZV)
    abort ();

  SET_PT (np);

  return make_number (np);
}

/* Return 1 if REGEXP it matches just one constant string.  */

static int
trivial_regexp_p (regexp)
     Lisp_Object regexp;
{
  int len = STRING_BYTES (XSTRING (regexp));
  unsigned char *s = XSTRING (regexp)->data;
  unsigned char c;
  while (--len >= 0)
    {
      switch (*s++)
	{
	case '.': case '*': case '+': case '?': case '[': case '^': case '$':
	  return 0;
	case '\\':
	  if (--len < 0)
	    return 0;
	  switch (*s++)
	    {
	    case '|': case '(': case ')': case '`': case '\'': case 'b':
	    case 'B': case '<': case '>': case 'w': case 'W': case 's':
	    case 'S': case '=':
	    case 'c': case 'C':	/* for categoryspec and notcategoryspec */
	    case '1': case '2': case '3': case '4': case '5':
	    case '6': case '7': case '8': case '9':
	      return 0;
	    }
	}
    }
  return 1;
}

/* Search for the n'th occurrence of STRING in the current buffer,
   starting at position POS and stopping at position LIM,
   treating STRING as a literal string if RE is false or as
   a regular expression if RE is true.

   If N is positive, searching is forward and LIM must be greater than POS.
   If N is negative, searching is backward and LIM must be less than POS.

   Returns -x if x occurrences remain to be found (x > 0),
   or else the position at the beginning of the Nth occurrence
   (if searching backward) or the end (if searching forward).

   POSIX is nonzero if we want full backtracking (POSIX style)
   for this pattern.  0 means backtrack only enough to get a valid match.  */

#define TRANSLATE(out, trt, d)			\
do						\
  {						\
    if (! NILP (trt))				\
      {						\
	Lisp_Object temp;			\
	temp = Faref (trt, make_number (d));	\
	if (INTEGERP (temp))			\
	  out = XINT (temp);			\
	else					\
	  out = d;				\
      }						\
    else					\
      out = d;					\
  }						\
while (0)

static int
search_buffer (string, pos, pos_byte, lim, lim_byte, n,
	       RE, trt, inverse_trt, posix)
     Lisp_Object string;
     int pos;
     int pos_byte;
     int lim;
     int lim_byte;
     int n;
     int RE;
     Lisp_Object trt;
     Lisp_Object inverse_trt;
     int posix;
{
  int len = XSTRING (string)->size;
  int len_byte = STRING_BYTES (XSTRING (string));
  register int i;

  if (running_asynch_code)
    save_search_regs ();

  /* Searching 0 times means don't move.  */
  /* Null string is found at starting position.  */
  if (len == 0 || n == 0)
    {
      set_search_regs (pos, 0);
      return pos;
    }

  if (RE && !trivial_regexp_p (string))
    {
      unsigned char *p1, *p2;
      int s1, s2;
      struct re_pattern_buffer *bufp;

      bufp = compile_pattern (string, &search_regs, trt, posix,
			      !NILP (current_buffer->enable_multibyte_characters));

      immediate_quit = 1;	/* Quit immediately if user types ^G,
				   because letting this function finish
				   can take too long. */
      QUIT;			/* Do a pending quit right away,
				   to avoid paradoxical behavior */
      /* Get pointers and sizes of the two strings
	 that make up the visible portion of the buffer. */

      p1 = BEGV_ADDR;
      s1 = GPT_BYTE - BEGV_BYTE;
      p2 = GAP_END_ADDR;
      s2 = ZV_BYTE - GPT_BYTE;
      if (s1 < 0)
	{
	  p2 = p1;
	  s2 = ZV_BYTE - BEGV_BYTE;
	  s1 = 0;
	}
      if (s2 < 0)
	{
	  s1 = ZV_BYTE - BEGV_BYTE;
	  s2 = 0;
	}
      re_match_object = Qnil;
  
      while (n < 0)
	{
	  int val;
	  val = re_search_2 (bufp, (char *) p1, s1, (char *) p2, s2,
			     pos_byte - BEGV_BYTE, lim_byte - pos_byte,
			     &search_regs,
			     /* Don't allow match past current point */
			     pos_byte - BEGV_BYTE);
	  if (val == -2)
	    {
	      matcher_overflow ();
	    }
	  if (val >= 0)
	    {
	      pos_byte = search_regs.start[0] + BEGV_BYTE;
	      for (i = 0; i < search_regs.num_regs; i++)
		if (search_regs.start[i] >= 0)
		  {
		    search_regs.start[i]
		      = BYTE_TO_CHAR (search_regs.start[i] + BEGV_BYTE);
		    search_regs.end[i]
		      = BYTE_TO_CHAR (search_regs.end[i] + BEGV_BYTE);
		  }
	      XSETBUFFER (last_thing_searched, current_buffer);
	      /* Set pos to the new position. */
	      pos = search_regs.start[0];
	    }
	  else
	    {
	      immediate_quit = 0;
	      return (n);
	    }
	  n++;
	}
      while (n > 0)
	{
	  int val;
	  val = re_search_2 (bufp, (char *) p1, s1, (char *) p2, s2,
			     pos_byte - BEGV_BYTE, lim_byte - pos_byte,
			     &search_regs,
			     lim_byte - BEGV_BYTE);
	  if (val == -2)
	    {
	      matcher_overflow ();
	    }
	  if (val >= 0)
	    {
	      pos_byte = search_regs.end[0] + BEGV_BYTE;
	      for (i = 0; i < search_regs.num_regs; i++)
		if (search_regs.start[i] >= 0)
		  {
		    search_regs.start[i]
		      = BYTE_TO_CHAR (search_regs.start[i] + BEGV_BYTE);
		    search_regs.end[i]
		      = BYTE_TO_CHAR (search_regs.end[i] + BEGV_BYTE);
		  }
	      XSETBUFFER (last_thing_searched, current_buffer);
	      pos = search_regs.end[0];
	    }
	  else
	    {
	      immediate_quit = 0;
	      return (0 - n);
	    }
	  n--;
	}
      immediate_quit = 0;
      return (pos);
    }
  else				/* non-RE case */
    {
      unsigned char *raw_pattern, *pat;
      int raw_pattern_size;
      int raw_pattern_size_byte;
      unsigned char *patbuf;
      int multibyte = !NILP (current_buffer->enable_multibyte_characters);
      unsigned char *base_pat = XSTRING (string)->data;
      int charset_base = -1;
      int simple = 1;

      /* MULTIBYTE says whether the text to be searched is multibyte.
	 We must convert PATTERN to match that, or we will not really
	 find things right.  */

      if (multibyte == STRING_MULTIBYTE (string))
	{
	  raw_pattern = (unsigned char *) XSTRING (string)->data;
	  raw_pattern_size = XSTRING (string)->size;
	  raw_pattern_size_byte = STRING_BYTES (XSTRING (string));
	}
      else if (multibyte)
	{
	  raw_pattern_size = XSTRING (string)->size;
	  raw_pattern_size_byte
	    = count_size_as_multibyte (XSTRING (string)->data,
				       raw_pattern_size);
	  raw_pattern = (unsigned char *) alloca (raw_pattern_size_byte + 1);
	  copy_text (XSTRING (string)->data, raw_pattern,
		     XSTRING (string)->size, 0, 1);
	}
      else
	{
	  /* Converting multibyte to single-byte.

	     ??? Perhaps this conversion should be done in a special way
	     by subtracting nonascii-insert-offset from each non-ASCII char,
	     so that only the multibyte chars which really correspond to
	     the chosen single-byte character set can possibly match.  */
	  raw_pattern_size = XSTRING (string)->size;
	  raw_pattern_size_byte = XSTRING (string)->size;
	  raw_pattern = (unsigned char *) alloca (raw_pattern_size + 1);
	  copy_text (XSTRING (string)->data, raw_pattern,
		     STRING_BYTES (XSTRING (string)), 1, 0);
	}

      /* Copy and optionally translate the pattern.  */
      len = raw_pattern_size;
      len_byte = raw_pattern_size_byte;
      patbuf = (unsigned char *) alloca (len_byte);
      pat = patbuf;
      base_pat = raw_pattern;
      if (multibyte)
	{
	  while (--len >= 0)
	    {
	      unsigned char workbuf[4], *str;
	      int c, translated, inverse;
	      int in_charlen, charlen;

	      /* If we got here and the RE flag is set, it's because we're
		 dealing with a regexp known to be trivial, so the backslash
		 just quotes the next character.  */
	      if (RE && *base_pat == '\\')
		{
		  len--;
		  len_byte--;
		  base_pat++;
		}

	      c = STRING_CHAR_AND_LENGTH (base_pat, len_byte, in_charlen);
	      /* Translate the character, if requested.  */
	      TRANSLATE (translated, trt, c);
	      /* If translation changed the byte-length, go back
		 to the original character.  */
	      charlen = CHAR_STRING (translated, workbuf, str);
	      if (in_charlen != charlen)
		{
		  translated = c;
		  charlen = CHAR_STRING (c, workbuf, str);
		}

	      TRANSLATE (inverse, inverse_trt, c);

	      /* Did this char actually get translated?
		 Would any other char get translated into it?  */
	      if (translated != c || inverse != c)
		{
		  /* Keep track of which character set row
		     contains the characters that need translation.  */
		  int charset_base_code = c & ~0xff;
		  if (charset_base == -1)
		    charset_base = charset_base_code;
		  else if (charset_base != charset_base_code)
		    /* If two different rows appear, needing translation,
		       then we cannot use boyer_moore search.  */
		    simple = 0;
		    /* ??? Handa: this must do simple = 0
		       if c is a composite character.  */
		}

	      /* Store this character into the translated pattern.  */
	      bcopy (str, pat, charlen);
	      pat += charlen;
	      base_pat += in_charlen;
	      len_byte -= in_charlen;
	    }
	}
      else
	{
	  while (--len >= 0)
	    {
	      int c, translated, inverse;

	      /* If we got here and the RE flag is set, it's because we're
		 dealing with a regexp known to be trivial, so the backslash
		 just quotes the next character.  */
	      if (RE && *base_pat == '\\')
		{
		  len--;
		  base_pat++;
		}
	      c = *base_pat++;
	      TRANSLATE (translated, trt, c);
	      TRANSLATE (inverse, inverse_trt, c);

	      /* Did this char actually get translated?
		 Would any other char get translated into it?  */
	      if (translated != c || inverse != c)
		{
		  /* Keep track of which character set row
		     contains the characters that need translation.  */
		  int charset_base_code = c & ~0xff;
		  if (charset_base == -1)
		    charset_base = charset_base_code;
		  else if (charset_base != charset_base_code)
		    /* If two different rows appear, needing translation,
		       then we cannot use boyer_moore search.  */
		    simple = 0;
		}
	      *pat++ = translated;
	    }
	}

      len_byte = pat - patbuf;
      len = raw_pattern_size;
      pat = base_pat = patbuf;

      if (simple)
	return boyer_moore (n, pat, len, len_byte, trt, inverse_trt,
			    pos, pos_byte, lim, lim_byte,
			    charset_base);
      else
	return simple_search (n, pat, len, len_byte, trt,
			      pos, pos_byte, lim, lim_byte);
    }
}

/* Do a simple string search N times for the string PAT,
   whose length is LEN/LEN_BYTE,
   from buffer position POS/POS_BYTE until LIM/LIM_BYTE.
   TRT is the translation table.

   Return the character position where the match is found.
   Otherwise, if M matches remained to be found, return -M.

   This kind of search works regardless of what is in PAT and
   regardless of what is in TRT.  It is used in cases where
   boyer_moore cannot work.  */

static int
simple_search (n, pat, len, len_byte, trt, pos, pos_byte, lim, lim_byte)
     int n;
     unsigned char *pat;
     int len, len_byte;
     Lisp_Object trt;
     int pos, pos_byte;
     int lim, lim_byte;
{
  int multibyte = ! NILP (current_buffer->enable_multibyte_characters);
  int forward = n > 0;

  if (lim > pos && multibyte)
    while (n > 0)
      {
	while (1)
	  {
	    /* Try matching at position POS.  */
	    int this_pos = pos;
	    int this_pos_byte = pos_byte;
	    int this_len = len;
	    int this_len_byte = len_byte;
	    unsigned char *p = pat;
	    if (pos + len > lim)
	      goto stop;

	    while (this_len > 0)
	      {
		int charlen, buf_charlen;
		int pat_ch, buf_ch;

		pat_ch = STRING_CHAR_AND_LENGTH (p, this_len_byte, charlen);
		buf_ch = STRING_CHAR_AND_LENGTH (BYTE_POS_ADDR (this_pos_byte),
						 ZV_BYTE - this_pos_byte,
						 buf_charlen);
		TRANSLATE (buf_ch, trt, buf_ch);

		if (buf_ch != pat_ch)
		  break;

		this_len_byte -= charlen;
		this_len--;
		p += charlen;

		this_pos_byte += buf_charlen;
		this_pos++;
	      }

	    if (this_len == 0)
	      {
		pos += len;
		pos_byte += len_byte;
		break;
	      }

	    INC_BOTH (pos, pos_byte);
	  }

	n--;
      }
  else if (lim > pos)
    while (n > 0)
      {
	while (1)
	  {
	    /* Try matching at position POS.  */
	    int this_pos = pos;
	    int this_len = len;
	    unsigned char *p = pat;

	    if (pos + len > lim)
	      goto stop;

	    while (this_len > 0)
	      {
		int pat_ch = *p++;
		int buf_ch = FETCH_BYTE (this_pos);
		TRANSLATE (buf_ch, trt, buf_ch);

		if (buf_ch != pat_ch)
		  break;

		this_len--;
		this_pos++;
	      }

	    if (this_len == 0)
	      {
		pos += len;
		break;
	      }

	    pos++;
	  }

	n--;
      }
  /* Backwards search.  */
  else if (lim < pos && multibyte)
    while (n < 0)
      {
	while (1)
	  {
	    /* Try matching at position POS.  */
	    int this_pos = pos - len;
	    int this_pos_byte = pos_byte - len_byte;
	    int this_len = len;
	    int this_len_byte = len_byte;
	    unsigned char *p = pat;

	    if (pos - len < lim)
	      goto stop;

	    while (this_len > 0)
	      {
		int charlen, buf_charlen;
		int pat_ch, buf_ch;

		pat_ch = STRING_CHAR_AND_LENGTH (p, this_len_byte, charlen);
		buf_ch = STRING_CHAR_AND_LENGTH (BYTE_POS_ADDR (this_pos_byte),
						 ZV_BYTE - this_pos_byte,
						 buf_charlen);
		TRANSLATE (buf_ch, trt, buf_ch);

		if (buf_ch != pat_ch)
		  break;

		this_len_byte -= charlen;
		this_len--;
		p += charlen;
		this_pos_byte += buf_charlen;
		this_pos++;
	      }

	    if (this_len == 0)
	      {
		pos -= len;
		pos_byte -= len_byte;
		break;
	      }

	    DEC_BOTH (pos, pos_byte);
	  }

	n++;
      }
  else if (lim < pos)
    while (n < 0)
      {
	while (1)
	  {
	    /* Try matching at position POS.  */
	    int this_pos = pos - len;
	    int this_len = len;
	    unsigned char *p = pat;

	    if (pos - len < lim)
	      goto stop;

	    while (this_len > 0)
	      {
		int pat_ch = *p++;
		int buf_ch = FETCH_BYTE (this_pos);
		TRANSLATE (buf_ch, trt, buf_ch);

		if (buf_ch != pat_ch)
		  break;
		this_len--;
		this_pos++;
	      }

	    if (this_len == 0)
	      {
		pos -= len;
		break;
	      }

	    pos--;
	  }

	n++;
      }

 stop:
  if (n == 0)
    {
      if (forward)
	set_search_regs ((multibyte ? pos_byte : pos) - len_byte, len_byte);
      else
	set_search_regs (multibyte ? pos_byte : pos, len_byte);

      return pos;
    }
  else if (n > 0)
    return -n;
  else
    return n;
}

/* Do Boyer-Moore search N times for the string PAT,
   whose length is LEN/LEN_BYTE,
   from buffer position POS/POS_BYTE until LIM/LIM_BYTE.
   DIRECTION says which direction we search in.
   TRT and INVERSE_TRT are translation tables.

   This kind of search works if all the characters in PAT that have
   nontrivial translation are the same aside from the last byte.  This
   makes it possible to translate just the last byte of a character,
   and do so after just a simple test of the context.

   If that criterion is not satisfied, do not call this function.  */

static int
boyer_moore (n, base_pat, len, len_byte, trt, inverse_trt,
	     pos, pos_byte, lim, lim_byte, charset_base)
     int n;
     unsigned char *base_pat;
     int len, len_byte;
     Lisp_Object trt;
     Lisp_Object inverse_trt;
     int pos, pos_byte;
     int lim, lim_byte;
     int charset_base;
{
  int direction = ((n > 0) ? 1 : -1);
  register int dirlen;
  int infinity, limit, k, stride_for_teases;
  register int *BM_tab;
  int *BM_tab_base;
  register unsigned char *cursor, *p_limit;  
  register int i, j;
  unsigned char *pat, *pat_end;
  int multibyte = ! NILP (current_buffer->enable_multibyte_characters);

  unsigned char simple_translate[0400];
  int translate_prev_byte;
  int translate_anteprev_byte;

#ifdef C_ALLOCA
  int BM_tab_space[0400];
  BM_tab = &BM_tab_space[0];
#else
  BM_tab = (int *) alloca (0400 * sizeof (int));
#endif
  /* The general approach is that we are going to maintain that we know */
  /* the first (closest to the present position, in whatever direction */
  /* we're searching) character that could possibly be the last */
  /* (furthest from present position) character of a valid match.  We */
  /* advance the state of our knowledge by looking at that character */
  /* and seeing whether it indeed matches the last character of the */
  /* pattern.  If it does, we take a closer look.  If it does not, we */
  /* move our pointer (to putative last characters) as far as is */
  /* logically possible.  This amount of movement, which I call a */
  /* stride, will be the length of the pattern if the actual character */
  /* appears nowhere in the pattern, otherwise it will be the distance */
  /* from the last occurrence of that character to the end of the */
  /* pattern. */
  /* As a coding trick, an enormous stride is coded into the table for */
  /* characters that match the last character.  This allows use of only */
  /* a single test, a test for having gone past the end of the */
  /* permissible match region, to test for both possible matches (when */
  /* the stride goes past the end immediately) and failure to */
  /* match (where you get nudged past the end one stride at a time). */ 

  /* Here we make a "mickey mouse" BM table.  The stride of the search */
  /* is determined only by the last character of the putative match. */
  /* If that character does not match, we will stride the proper */
  /* distance to propose a match that superimposes it on the last */
  /* instance of a character that matches it (per trt), or misses */
  /* it entirely if there is none. */  

  dirlen = len_byte * direction;
  infinity = dirlen - (lim_byte + pos_byte + len_byte + len_byte) * direction;

  /* Record position after the end of the pattern.  */
  pat_end = base_pat + len_byte;
  /* BASE_PAT points to a character that we start scanning from.
     It is the first character in a forward search,
     the last character in a backward search.  */
  if (direction < 0)
    base_pat = pat_end - 1;

  BM_tab_base = BM_tab;
  BM_tab += 0400;
  j = dirlen;		/* to get it in a register */
  /* A character that does not appear in the pattern induces a */
  /* stride equal to the pattern length. */
  while (BM_tab_base != BM_tab)
    {
      *--BM_tab = j;
      *--BM_tab = j;
      *--BM_tab = j;
      *--BM_tab = j;
    }

  /* We use this for translation, instead of TRT itself.
     We fill this in to handle the characters that actually
     occur in the pattern.  Others don't matter anyway!  */
  bzero (simple_translate, sizeof simple_translate);
  for (i = 0; i < 0400; i++)
    simple_translate[i] = i;

  i = 0;
  while (i != infinity)
    {
      unsigned char *ptr = base_pat + i;
      i += direction;
      if (i == dirlen)
	i = infinity;
      if (! NILP (trt))
	{
	  int ch;
	  int untranslated;
	  int this_translated = 1;

	  if (multibyte
	      /* Is *PTR the last byte of a character?  */
	      && (pat_end - ptr == 1 || CHAR_HEAD_P (ptr[1])))
	    {
	      unsigned char *charstart = ptr;
	      while (! CHAR_HEAD_P (*charstart))
		charstart--;
	      untranslated = STRING_CHAR (charstart, ptr - charstart + 1);
	      if (charset_base == (untranslated & ~0xff))
		{
		  TRANSLATE (ch, trt, untranslated);
		  if (! CHAR_HEAD_P (*ptr))
		    {
		      translate_prev_byte = ptr[-1];
		      if (! CHAR_HEAD_P (translate_prev_byte))
			translate_anteprev_byte = ptr[-2];
		    }
		}
	      else
		{
		  this_translated = 0;
		  ch = *ptr;
		}
	    }
	  else if (!multibyte)
	    TRANSLATE (ch, trt, *ptr);
	  else
	    {
	      ch = *ptr;
	      this_translated = 0;
	    }

	  if (ch > 0400)
	    j = ((unsigned char) ch) | 0200;
	  else
	    j = (unsigned char) ch;

	  if (i == infinity)
	    stride_for_teases = BM_tab[j];

	  BM_tab[j] = dirlen - i;
	  /* A translation table is accompanied by its inverse -- see */
	  /* comment following downcase_table for details */ 
	  if (this_translated)
	    {
	      int starting_ch = ch;
	      int starting_j = j;
	      while (1)
		{
		  TRANSLATE (ch, inverse_trt, ch);
		  if (ch > 0400)
		    j = ((unsigned char) ch) | 0200;
		  else
		    j = (unsigned char) ch;

		  /* For all the characters that map into CH,
		     set up simple_translate to map the last byte
		     into STARTING_J.  */
		  simple_translate[j] = starting_j;
		  if (ch == starting_ch)
		    break;
		  BM_tab[j] = dirlen - i;
		}
	    }
	}
      else
	{
	  j = *ptr;

	  if (i == infinity)
	    stride_for_teases = BM_tab[j];
	  BM_tab[j] = dirlen - i;
	}
      /* stride_for_teases tells how much to stride if we get a */
      /* match on the far character but are subsequently */
      /* disappointed, by recording what the stride would have been */
      /* for that character if the last character had been */
      /* different. */
    }
  infinity = dirlen - infinity;
  pos_byte += dirlen - ((direction > 0) ? direction : 0);
  /* loop invariant - POS_BYTE points at where last char (first
     char if reverse) of pattern would align in a possible match.  */
  while (n != 0)
    {
      int tail_end;
      unsigned char *tail_end_ptr;

      /* It's been reported that some (broken) compiler thinks that
	 Boolean expressions in an arithmetic context are unsigned.
	 Using an explicit ?1:0 prevents this.  */
      if ((lim_byte - pos_byte - ((direction > 0) ? 1 : 0)) * direction
	  < 0)
	return (n * (0 - direction));
      /* First we do the part we can by pointers (maybe nothing) */
      QUIT;
      pat = base_pat;
      limit = pos_byte - dirlen + direction;
      if (direction > 0)
	{
	  limit = BUFFER_CEILING_OF (limit);
	  /* LIMIT is now the last (not beyond-last!) value POS_BYTE
	     can take on without hitting edge of buffer or the gap.  */
	  limit = min (limit, pos_byte + 20000);
	  limit = min (limit, lim_byte - 1);
	}
      else
	{
	  limit = BUFFER_FLOOR_OF (limit);
	  /* LIMIT is now the last (not beyond-last!) value POS_BYTE
	     can take on without hitting edge of buffer or the gap.  */
	  limit = max (limit, pos_byte - 20000);
	  limit = max (limit, lim_byte);
	}
      tail_end = BUFFER_CEILING_OF (pos_byte) + 1;
      tail_end_ptr = BYTE_POS_ADDR (tail_end);

      if ((limit - pos_byte) * direction > 20)
	{
	  unsigned char *p2;

	  p_limit = BYTE_POS_ADDR (limit);
	  p2 = (cursor = BYTE_POS_ADDR (pos_byte));
	  /* In this loop, pos + cursor - p2 is the surrogate for pos */
	  while (1)		/* use one cursor setting as long as i can */
	    {
	      if (direction > 0) /* worth duplicating */
		{
		  /* Use signed comparison if appropriate
		     to make cursor+infinity sure to be > p_limit.
		     Assuming that the buffer lies in a range of addresses
		     that are all "positive" (as ints) or all "negative",
		     either kind of comparison will work as long
		     as we don't step by infinity.  So pick the kind
		     that works when we do step by infinity.  */
		  if ((EMACS_INT) (p_limit + infinity) > (EMACS_INT) p_limit)
		    while ((EMACS_INT) cursor <= (EMACS_INT) p_limit)
		      cursor += BM_tab[*cursor];
		  else
		    while ((EMACS_UINT) cursor <= (EMACS_UINT) p_limit)
		      cursor += BM_tab[*cursor];
		}
	      else
		{
		  if ((EMACS_INT) (p_limit + infinity) < (EMACS_INT) p_limit)
		    while ((EMACS_INT) cursor >= (EMACS_INT) p_limit)
		      cursor += BM_tab[*cursor];
		  else
		    while ((EMACS_UINT) cursor >= (EMACS_UINT) p_limit)
		      cursor += BM_tab[*cursor];
		}
/* If you are here, cursor is beyond the end of the searched region. */
/* This can happen if you match on the far character of the pattern, */
/* because the "stride" of that character is infinity, a number able */
/* to throw you well beyond the end of the search.  It can also */
/* happen if you fail to match within the permitted region and would */
/* otherwise try a character beyond that region */
	      if ((cursor - p_limit) * direction <= len_byte)
		break;	/* a small overrun is genuine */
	      cursor -= infinity; /* large overrun = hit */
	      i = dirlen - direction;
	      if (! NILP (trt))
		{
		  while ((i -= direction) + direction != 0)
		    {
		      int ch;
		      cursor -= direction;
		      /* Translate only the last byte of a character.  */
		      if (! multibyte
			  || ((cursor == tail_end_ptr
			       || CHAR_HEAD_P (cursor[1]))
			      && (CHAR_HEAD_P (cursor[0])
				  || (translate_prev_byte == cursor[-1]
				      && (CHAR_HEAD_P (translate_prev_byte)
					  || translate_anteprev_byte == cursor[-2])))))
			ch = simple_translate[*cursor];
		      else
			ch = *cursor;
		      if (pat[i] != ch)
			break;
		    }
		}
	      else
		{
		  while ((i -= direction) + direction != 0)
		    {
		      cursor -= direction;
		      if (pat[i] != *cursor)
			break;
		    }
		}
	      cursor += dirlen - i - direction;	/* fix cursor */
	      if (i + direction == 0)
		{
		  int position;

		  cursor -= direction;

		  position = pos_byte + cursor - p2 + ((direction > 0)
						       ? 1 - len_byte : 0);
		  set_search_regs (position, len_byte);

		  if ((n -= direction) != 0)
		    cursor += dirlen; /* to resume search */
		  else
		    return ((direction > 0)
			    ? search_regs.end[0] : search_regs.start[0]);
		}
	      else
		cursor += stride_for_teases; /* <sigh> we lose -  */
	    }
	  pos_byte += cursor - p2;
	}
      else
	/* Now we'll pick up a clump that has to be done the hard */
	/* way because it covers a discontinuity */
	{
	  limit = ((direction > 0)
		   ? BUFFER_CEILING_OF (pos_byte - dirlen + 1)
		   : BUFFER_FLOOR_OF (pos_byte - dirlen - 1));
	  limit = ((direction > 0)
		   ? min (limit + len_byte, lim_byte - 1)
		   : max (limit - len_byte, lim_byte));
	  /* LIMIT is now the last value POS_BYTE can have
	     and still be valid for a possible match.  */
	  while (1)
	    {
	      /* This loop can be coded for space rather than */
	      /* speed because it will usually run only once. */
	      /* (the reach is at most len + 21, and typically */
	      /* does not exceed len) */    
	      while ((limit - pos_byte) * direction >= 0)
		pos_byte += BM_tab[FETCH_BYTE (pos_byte)];
	      /* now run the same tests to distinguish going off the */
	      /* end, a match or a phony match. */
	      if ((pos_byte - limit) * direction <= len_byte)
		break;	/* ran off the end */
	      /* Found what might be a match.
		 Set POS_BYTE back to last (first if reverse) pos.  */
	      pos_byte -= infinity;
	      i = dirlen - direction;
	      while ((i -= direction) + direction != 0)
		{
		  int ch;
		  unsigned char *ptr;
		  pos_byte -= direction;
		  ptr = BYTE_POS_ADDR (pos_byte);
		  /* Translate only the last byte of a character.  */
		  if (! multibyte
		      || ((ptr == tail_end_ptr
			   || CHAR_HEAD_P (ptr[1]))
			  && (CHAR_HEAD_P (ptr[0])
			      || (translate_prev_byte == ptr[-1]
				  && (CHAR_HEAD_P (translate_prev_byte)
				      || translate_anteprev_byte == ptr[-2])))))
		    ch = simple_translate[*ptr];
		  else
		    ch = *ptr;
		  if (pat[i] != ch)
		    break;
		}
	      /* Above loop has moved POS_BYTE part or all the way
		 back to the first pos (last pos if reverse).
		 Set it once again at the last (first if reverse) char.  */
	      pos_byte += dirlen - i- direction;
	      if (i + direction == 0)
		{
		  int position;
		  pos_byte -= direction;

		  position = pos_byte + ((direction > 0) ? 1 - len_byte : 0);

		  set_search_regs (position, len_byte);

		  if ((n -= direction) != 0)
		    pos_byte += dirlen; /* to resume search */
		  else
		    return ((direction > 0)
			    ? search_regs.end[0] : search_regs.start[0]);
		}
	      else
		pos_byte += stride_for_teases;
	    }
	  }
      /* We have done one clump.  Can we continue? */
      if ((lim_byte - pos_byte) * direction < 0)
	return ((0 - n) * direction);
    }
  return BYTE_TO_CHAR (pos_byte);
}

/* Record beginning BEG_BYTE and end BEG_BYTE + NBYTES
   for the overall match just found in the current buffer.
   Also clear out the match data for registers 1 and up.  */

static void
set_search_regs (beg_byte, nbytes)
     int beg_byte, nbytes;
{
  int i;

  /* Make sure we have registers in which to store
     the match position.  */
  if (search_regs.num_regs == 0)
    {
      search_regs.start = (regoff_t *) xmalloc (2 * sizeof (regoff_t));
      search_regs.end = (regoff_t *) xmalloc (2 * sizeof (regoff_t));
      search_regs.num_regs = 2;
    }

  /* Clear out the other registers.  */
  for (i = 1; i < search_regs.num_regs; i++)
    {
      search_regs.start[i] = -1;
      search_regs.end[i] = -1;
    }

  search_regs.start[0] = BYTE_TO_CHAR (beg_byte);
  search_regs.end[0] = BYTE_TO_CHAR (beg_byte + nbytes);
  XSETBUFFER (last_thing_searched, current_buffer);
}

/* Given a string of words separated by word delimiters,
  compute a regexp that matches those exact words
  separated by arbitrary punctuation.  */

static Lisp_Object
wordify (string)
     Lisp_Object string;
{
  register unsigned char *p, *o;
  register int i, i_byte, len, punct_count = 0, word_count = 0;
  Lisp_Object val;
  int prev_c = 0;
  int adjust;

  CHECK_STRING (string, 0);
  p = XSTRING (string)->data;
  len = XSTRING (string)->size;

  for (i = 0, i_byte = 0; i < len; )
    {
      int c;
      
      if (STRING_MULTIBYTE (string))
	FETCH_STRING_CHAR_ADVANCE (c, string, i, i_byte);
      else
	c = XSTRING (string)->data[i++];

      if (SYNTAX (c) != Sword)
	{
	  punct_count++;
	  if (i > 0 && SYNTAX (prev_c) == Sword)
	    word_count++;
	}

      prev_c = c;
    }

  if (SYNTAX (prev_c) == Sword)
    word_count++;
  if (!word_count)
    return build_string ("");

  adjust = - punct_count + 5 * (word_count - 1) + 4;
  if (STRING_MULTIBYTE (string))
    val = make_uninit_multibyte_string (len + adjust,
					STRING_BYTES (XSTRING (string))
					+ adjust);
  else
    val = make_uninit_string (len + adjust);

  o = XSTRING (val)->data;
  *o++ = '\\';
  *o++ = 'b';
  prev_c = 0;

  for (i = 0, i_byte = 0; i < len; )
    {
      int c;
      int i_byte_orig = i_byte;
      
      if (STRING_MULTIBYTE (string))
	FETCH_STRING_CHAR_ADVANCE (c, string, i, i_byte);
      else
	{
	  c = XSTRING (string)->data[i++];
	  i_byte++;
	}

      if (SYNTAX (c) == Sword)
	{
	  bcopy (&XSTRING (string)->data[i_byte_orig], o,
		 i_byte - i_byte_orig);
	  o += i_byte - i_byte_orig;
	}
      else if (i > 0 && SYNTAX (prev_c) == Sword && --word_count)
	{
	  *o++ = '\\';
	  *o++ = 'W';
	  *o++ = '\\';
	  *o++ = 'W';
	  *o++ = '*';
	}

      prev_c = c;
    }

  *o++ = '\\';
  *o++ = 'b';

  return val;
}

DEFUN ("search-backward", Fsearch_backward, Ssearch_backward, 1, 4,
  "MSearch backward: ",
  "Search backward from point for STRING.\n\
Set point to the beginning of the occurrence found, and return point.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must not extend before that position.\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
 If not nil and not t, position at limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.\n\
See also the functions `match-beginning', `match-end' and `replace-match'.")
  (string, bound, noerror, count)
     Lisp_Object string, bound, noerror, count;
{
  return search_command (string, bound, noerror, count, -1, 0, 0);
}

DEFUN ("search-forward", Fsearch_forward, Ssearch_forward, 1, 4, "MSearch: ",
  "Search forward from point for STRING.\n\
Set point to the end of the occurrence found, and return point.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must not extend after that position.  nil is equivalent\n\
  to (point-max).\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
  If not nil and not t, move to limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.\n\
See also the functions `match-beginning', `match-end' and `replace-match'.")
  (string, bound, noerror, count)
     Lisp_Object string, bound, noerror, count;
{
  return search_command (string, bound, noerror, count, 1, 0, 0);
}

DEFUN ("word-search-backward", Fword_search_backward, Sword_search_backward, 1, 4,
  "sWord search backward: ",
  "Search backward from point for STRING, ignoring differences in punctuation.\n\
Set point to the beginning of the occurrence found, and return point.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must not extend before that position.\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
  If not nil and not t, move to limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.")
  (string, bound, noerror, count)
     Lisp_Object string, bound, noerror, count;
{
  return search_command (wordify (string), bound, noerror, count, -1, 1, 0);
}

DEFUN ("word-search-forward", Fword_search_forward, Sword_search_forward, 1, 4,
  "sWord search: ",
  "Search forward from point for STRING, ignoring differences in punctuation.\n\
Set point to the end of the occurrence found, and return point.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must not extend after that position.\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
  If not nil and not t, move to limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.")
  (string, bound, noerror, count)
     Lisp_Object string, bound, noerror, count;
{
  return search_command (wordify (string), bound, noerror, count, 1, 1, 0);
}

DEFUN ("re-search-backward", Fre_search_backward, Sre_search_backward, 1, 4,
  "sRE search backward: ",
  "Search backward from point for match for regular expression REGEXP.\n\
Set point to the beginning of the match, and return point.\n\
The match found is the one starting last in the buffer\n\
and yet ending before the origin of the search.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must start at or after that position.\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
  If not nil and not t, move to limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.\n\
See also the functions `match-beginning', `match-end' and `replace-match'.")
  (regexp, bound, noerror, count)
     Lisp_Object regexp, bound, noerror, count;
{
  return search_command (regexp, bound, noerror, count, -1, 1, 0);
}

DEFUN ("re-search-forward", Fre_search_forward, Sre_search_forward, 1, 4,
  "sRE search: ",
  "Search forward from point for regular expression REGEXP.\n\
Set point to the end of the occurrence found, and return point.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must not extend after that position.\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
  If not nil and not t, move to limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.\n\
See also the functions `match-beginning', `match-end' and `replace-match'.")
  (regexp, bound, noerror, count)
     Lisp_Object regexp, bound, noerror, count;
{
  return search_command (regexp, bound, noerror, count, 1, 1, 0);
}

DEFUN ("posix-search-backward", Fposix_search_backward, Sposix_search_backward, 1, 4,
  "sPosix search backward: ",
  "Search backward from point for match for regular expression REGEXP.\n\
Find the longest match in accord with Posix regular expression rules.\n\
Set point to the beginning of the match, and return point.\n\
The match found is the one starting last in the buffer\n\
and yet ending before the origin of the search.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must start at or after that position.\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
  If not nil and not t, move to limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.\n\
See also the functions `match-beginning', `match-end' and `replace-match'.")
  (regexp, bound, noerror, count)
     Lisp_Object regexp, bound, noerror, count;
{
  return search_command (regexp, bound, noerror, count, -1, 1, 1);
}

DEFUN ("posix-search-forward", Fposix_search_forward, Sposix_search_forward, 1, 4,
  "sPosix search: ",
  "Search forward from point for regular expression REGEXP.\n\
Find the longest match in accord with Posix regular expression rules.\n\
Set point to the end of the occurrence found, and return point.\n\
An optional second argument bounds the search; it is a buffer position.\n\
The match found must not extend after that position.\n\
Optional third argument, if t, means if fail just return nil (no error).\n\
  If not nil and not t, move to limit of search and return nil.\n\
Optional fourth argument is repeat count--search for successive occurrences.\n\
See also the functions `match-beginning', `match-end' and `replace-match'.")
  (regexp, bound, noerror, count)
     Lisp_Object regexp, bound, noerror, count;
{
  return search_command (regexp, bound, noerror, count, 1, 1, 1);
}

DEFUN ("replace-match", Freplace_match, Sreplace_match, 1, 5, 0,
  "Replace text matched by last search with NEWTEXT.\n\
If second arg FIXEDCASE is non-nil, do not alter case of replacement text.\n\
Otherwise maybe capitalize the whole text, or maybe just word initials,\n\
based on the replaced text.\n\
If the replaced text has only capital letters\n\
and has at least one multiletter word, convert NEWTEXT to all caps.\n\
If the replaced text has at least one word starting with a capital letter,\n\
then capitalize each word in NEWTEXT.\n\n\
If third arg LITERAL is non-nil, insert NEWTEXT literally.\n\
Otherwise treat `\\' as special:\n\
  `\\&' in NEWTEXT means substitute original matched text.\n\
  `\\N' means substitute what matched the Nth `\\(...\\)'.\n\
       If Nth parens didn't match, substitute nothing.\n\
  `\\\\' means insert one `\\'.\n\
FIXEDCASE and LITERAL are optional arguments.\n\
Leaves point at end of replacement text.\n\
\n\
The optional fourth argument STRING can be a string to modify.\n\
In that case, this function creates and returns a new string\n\
which is made by replacing the part of STRING that was matched.\n\
\n\
The optional fifth argument SUBEXP specifies a subexpression of the match.\n\
It says to replace just that subexpression instead of the whole match.\n\
This is useful only after a regular expression search or match\n\
since only regular expressions have distinguished subexpressions.")
  (newtext, fixedcase, literal, string, subexp)
     Lisp_Object newtext, fixedcase, literal, string, subexp;
{
  enum { nochange, all_caps, cap_initial } case_action;
  register int pos, pos_byte;
  int some_multiletter_word;
  int some_lowercase;
  int some_uppercase;
  int some_nonuppercase_initial;
  register int c, prevc;
  int inslen;
  int sub;
  int opoint, newpoint;

  CHECK_STRING (newtext, 0);

  if (! NILP (string))
    CHECK_STRING (string, 4);

  case_action = nochange;	/* We tried an initialization */
				/* but some C compilers blew it */

  if (search_regs.num_regs <= 0)
    error ("replace-match called before any match found");

  if (NILP (subexp))
    sub = 0;
  else
    {
      CHECK_NUMBER (subexp, 3);
      sub = XINT (subexp);
      if (sub < 0 || sub >= search_regs.num_regs)
	args_out_of_range (subexp, make_number (search_regs.num_regs));
    }

  if (NILP (string))
    {
      if (search_regs.start[sub] < BEGV
	  || search_regs.start[sub] > search_regs.end[sub]
	  || search_regs.end[sub] > ZV)
	args_out_of_range (make_number (search_regs.start[sub]),
			   make_number (search_regs.end[sub]));
    }
  else
    {
      if (search_regs.start[sub] < 0
	  || search_regs.start[sub] > search_regs.end[sub]
	  || search_regs.end[sub] > XSTRING (string)->size)
	args_out_of_range (make_number (search_regs.start[sub]),
			   make_number (search_regs.end[sub]));
    }

  if (NILP (fixedcase))
    {
      /* Decide how to casify by examining the matched text. */
      int last;

      pos = search_regs.start[sub];
      last = search_regs.end[sub];

      if (NILP (string))
	pos_byte = CHAR_TO_BYTE (pos);
      else
	pos_byte = string_char_to_byte (string, pos);

      prevc = '\n';
      case_action = all_caps;

      /* some_multiletter_word is set nonzero if any original word
	 is more than one letter long. */
      some_multiletter_word = 0;
      some_lowercase = 0;
      some_nonuppercase_initial = 0;
      some_uppercase = 0;

      while (pos < last)
	{
	  if (NILP (string))
	    {
	      c = FETCH_CHAR (pos_byte);
	      INC_BOTH (pos, pos_byte);
	    }
	  else
	    FETCH_STRING_CHAR_ADVANCE (c, string, pos, pos_byte);

	  if (LOWERCASEP (c))
	    {
	      /* Cannot be all caps if any original char is lower case */

	      some_lowercase = 1;
	      if (SYNTAX (prevc) != Sword)
		some_nonuppercase_initial = 1;
	      else
		some_multiletter_word = 1;
	    }
	  else if (!NOCASEP (c))
	    {
	      some_uppercase = 1;
	      if (SYNTAX (prevc) != Sword)
		;
	      else
		some_multiletter_word = 1;
	    }
	  else
	    {
	      /* If the initial is a caseless word constituent,
		 treat that like a lowercase initial.  */
	      if (SYNTAX (prevc) != Sword)
		some_nonuppercase_initial = 1;
	    }

	  prevc = c;
	}

      /* Convert to all caps if the old text is all caps
	 and has at least one multiletter word.  */
      if (! some_lowercase && some_multiletter_word)
	case_action = all_caps;
      /* Capitalize each word, if the old text has all capitalized words.  */
      else if (!some_nonuppercase_initial && some_multiletter_word)
	case_action = cap_initial;
      else if (!some_nonuppercase_initial && some_uppercase)
	/* Should x -> yz, operating on X, give Yz or YZ?
	   We'll assume the latter.  */
	case_action = all_caps;
      else
	case_action = nochange;
    }

  /* Do replacement in a string.  */
  if (!NILP (string))
    {
      Lisp_Object before, after;

      before = Fsubstring (string, make_number (0),
			   make_number (search_regs.start[sub]));
      after = Fsubstring (string, make_number (search_regs.end[sub]), Qnil);

      /* Substitute parts of the match into NEWTEXT
	 if desired.  */
      if (NILP (literal))
	{
	  int lastpos = 0;
	  int lastpos_byte = 0;
	  /* We build up the substituted string in ACCUM.  */
	  Lisp_Object accum;
	  Lisp_Object middle;
	  int length = STRING_BYTES (XSTRING (newtext));

	  accum = Qnil;

	  for (pos_byte = 0, pos = 0; pos_byte < length;)
	    {
	      int substart = -1;
	      int subend;
	      int delbackslash = 0;

	      FETCH_STRING_CHAR_ADVANCE (c, newtext, pos, pos_byte);

	      if (c == '\\')
		{
		  FETCH_STRING_CHAR_ADVANCE (c, newtext, pos, pos_byte);
		  if (c == '&')
		    {
		      substart = search_regs.start[sub];
		      subend = search_regs.end[sub];
		    }
		  else if (c >= '1' && c <= '9' && c <= search_regs.num_regs + '0')
		    {
		      if (search_regs.start[c - '0'] >= 0)
			{
			  substart = search_regs.start[c - '0'];
			  subend = search_regs.end[c - '0'];
			}
		    }
		  else if (c == '\\')
		    delbackslash = 1;
		  else
		    error ("Invalid use of `\\' in replacement text");
		}
	      if (substart >= 0)
		{
		  if (pos - 2 != lastpos)
		    middle = substring_both (newtext, lastpos,
					     lastpos_byte,
					     pos - 2, pos_byte - 2);
		  else
		    middle = Qnil;
		  accum = concat3 (accum, middle,
				   Fsubstring (string,
					       make_number (substart),
					       make_number (subend)));
		  lastpos = pos;
		  lastpos_byte = pos_byte;
		}
	      else if (delbackslash)
		{
		  middle = substring_both (newtext, lastpos,
					   lastpos_byte,
					   pos - 1, pos_byte - 1);

		  accum = concat2 (accum, middle);
		  lastpos = pos;
		  lastpos_byte = pos_byte;
		}
	    }

	  if (pos != lastpos)
	    middle = substring_both (newtext, lastpos,
				     lastpos_byte,
				     pos, pos_byte);
	  else
	    middle = Qnil;

	  newtext = concat2 (accum, middle);
	}

      /* Do case substitution in NEWTEXT if desired.  */
      if (case_action == all_caps)
	newtext = Fupcase (newtext);
      else if (case_action == cap_initial)
	newtext = Fupcase_initials (newtext);

      return concat3 (before, newtext, after);
    }

  /* Record point, the move (quietly) to the start of the match.  */
  if (PT > search_regs.start[sub])
    opoint = PT - ZV;
  else
    opoint = PT;

  TEMP_SET_PT (search_regs.start[sub]);

  /* We insert the replacement text before the old text, and then
     delete the original text.  This means that markers at the
     beginning or end of the original will float to the corresponding
     position in the replacement.  */
  if (!NILP (literal))
    Finsert_and_inherit (1, &newtext);
  else
    {
      struct gcpro gcpro1;
      int length = STRING_BYTES (XSTRING (newtext));

      GCPRO1 (newtext);

      for (pos_byte = 0, pos = 0; pos_byte < length;)
	{
	  int offset = PT - search_regs.start[sub];

	  FETCH_STRING_CHAR_ADVANCE (c, newtext, pos, pos_byte);

	  if (c == '\\')
	    {
	      FETCH_STRING_CHAR_ADVANCE (c, newtext, pos, pos_byte);
	      if (c == '&')
		Finsert_buffer_substring
		  (Fcurrent_buffer (),
		   make_number (search_regs.start[sub] + offset),
		   make_number (search_regs.end[sub] + offset));
	      else if (c >= '1' && c <= '9' && c <= search_regs.num_regs + '0')
		{
		  if (search_regs.start[c - '0'] >= 1)
		    Finsert_buffer_substring
		      (Fcurrent_buffer (),
		       make_number (search_regs.start[c - '0'] + offset),
		       make_number (search_regs.end[c - '0'] + offset));
		}
	      else if (c == '\\')
		insert_char (c);
	      else
		error ("Invalid use of `\\' in replacement text");
	    }
	  else
	    insert_char (c);
	}
      UNGCPRO;
    }

  inslen = PT - (search_regs.start[sub]);
  del_range (search_regs.start[sub] + inslen, search_regs.end[sub] + inslen);

  if (case_action == all_caps)
    Fupcase_region (make_number (PT - inslen), make_number (PT));
  else if (case_action == cap_initial)
    Fupcase_initials_region (make_number (PT - inslen), make_number (PT));

  newpoint = PT;

  /* Put point back where it was in the text.  */
  if (opoint <= 0)
    TEMP_SET_PT (opoint + ZV);
  else
    TEMP_SET_PT (opoint);

  /* Now move point "officially" to the start of the inserted replacement.  */
  move_if_not_intangible (newpoint);
  
  return Qnil;
}

static Lisp_Object
match_limit (num, beginningp)
     Lisp_Object num;
     int beginningp;
{
  register int n;

  CHECK_NUMBER (num, 0);
  n = XINT (num);
  if (n < 0 || n >= search_regs.num_regs)
    args_out_of_range (num, make_number (search_regs.num_regs));
  if (search_regs.num_regs <= 0
      || search_regs.start[n] < 0)
    return Qnil;
  return (make_number ((beginningp) ? search_regs.start[n]
		                    : search_regs.end[n]));
}

DEFUN ("match-beginning", Fmatch_beginning, Smatch_beginning, 1, 1, 0,
  "Return position of start of text matched by last search.\n\
SUBEXP, a number, specifies which parenthesized expression in the last\n\
  regexp.\n\
Value is nil if SUBEXPth pair didn't match, or there were less than\n\
  SUBEXP pairs.\n\
Zero means the entire text matched by the whole regexp or whole string.")
  (subexp)
     Lisp_Object subexp;
{
  return match_limit (subexp, 1);
}

DEFUN ("match-end", Fmatch_end, Smatch_end, 1, 1, 0,
  "Return position of end of text matched by last search.\n\
SUBEXP, a number, specifies which parenthesized expression in the last\n\
  regexp.\n\
Value is nil if SUBEXPth pair didn't match, or there were less than\n\
  SUBEXP pairs.\n\
Zero means the entire text matched by the whole regexp or whole string.")
  (subexp)
     Lisp_Object subexp;
{
  return match_limit (subexp, 0);
} 

DEFUN ("match-data", Fmatch_data, Smatch_data, 0, 2, 0,
  "Return a list containing all info on what the last search matched.\n\
Element 2N is `(match-beginning N)'; element 2N + 1 is `(match-end N)'.\n\
All the elements are markers or nil (nil if the Nth pair didn't match)\n\
if the last match was on a buffer; integers or nil if a string was matched.\n\
Use `store-match-data' to reinstate the data in this list.\n\
\n\
If INTEGERS (the optional first argument) is non-nil, always use integers\n\
\(rather than markers) to represent buffer positions.\n\
If REUSE is a list, reuse it as part of the value.  If REUSE is long enough\n\
to hold all the values, and if INTEGERS is non-nil, no consing is done.")
  (integers, reuse)
     Lisp_Object integers, reuse;
{
  Lisp_Object tail, prev;
  Lisp_Object *data;
  int i, len;

  if (NILP (last_thing_searched))
    return Qnil;

  data = (Lisp_Object *) alloca ((2 * search_regs.num_regs)
				 * sizeof (Lisp_Object));

  len = -1;
  for (i = 0; i < search_regs.num_regs; i++)
    {
      int start = search_regs.start[i];
      if (start >= 0)
	{
	  if (EQ (last_thing_searched, Qt)
	      || ! NILP (integers))
	    {
	      XSETFASTINT (data[2 * i], start);
	      XSETFASTINT (data[2 * i + 1], search_regs.end[i]);
	    }
	  else if (BUFFERP (last_thing_searched))
	    {
	      data[2 * i] = Fmake_marker ();
	      Fset_marker (data[2 * i],
			   make_number (start),
			   last_thing_searched);
	      data[2 * i + 1] = Fmake_marker ();
	      Fset_marker (data[2 * i + 1],
			   make_number (search_regs.end[i]), 
			   last_thing_searched);
	    }
	  else
	    /* last_thing_searched must always be Qt, a buffer, or Qnil.  */
	    abort ();

	  len = i;
	}
      else
	data[2 * i] = data [2 * i + 1] = Qnil;
    }

  /* If REUSE is not usable, cons up the values and return them.  */
  if (! CONSP (reuse))
    return Flist (2 * len + 2, data);

  /* If REUSE is a list, store as many value elements as will fit
     into the elements of REUSE.  */
  for (i = 0, tail = reuse; CONSP (tail);
       i++, tail = XCONS (tail)->cdr)
    {
      if (i < 2 * len + 2)
	XCONS (tail)->car = data[i];
      else
	XCONS (tail)->car = Qnil;
      prev = tail;
    }

  /* If we couldn't fit all value elements into REUSE,
     cons up the rest of them and add them to the end of REUSE.  */
  if (i < 2 * len + 2)
    XCONS (prev)->cdr = Flist (2 * len + 2 - i, data + i);

  return reuse;
}


DEFUN ("set-match-data", Fset_match_data, Sset_match_data, 1, 1, 0,
  "Set internal data on last search match from elements of LIST.\n\
LIST should have been created by calling `match-data' previously.")
  (list)
     register Lisp_Object list;
{
  register int i;
  register Lisp_Object marker;

  if (running_asynch_code)
    save_search_regs ();

  if (!CONSP (list) && !NILP (list))
    list = wrong_type_argument (Qconsp, list);

  /* Unless we find a marker with a buffer in LIST, assume that this 
     match data came from a string.  */
  last_thing_searched = Qt;

  /* Allocate registers if they don't already exist.  */
  {
    int length = XFASTINT (Flength (list)) / 2;

    if (length > search_regs.num_regs)
      {
	if (search_regs.num_regs == 0)
	  {
	    search_regs.start
	      = (regoff_t *) xmalloc (length * sizeof (regoff_t));
	    search_regs.end
	      = (regoff_t *) xmalloc (length * sizeof (regoff_t));
	  }
	else
	  {
	    search_regs.start
	      = (regoff_t *) xrealloc (search_regs.start,
				       length * sizeof (regoff_t));
	    search_regs.end
	      = (regoff_t *) xrealloc (search_regs.end,
				       length * sizeof (regoff_t));
	  }

	search_regs.num_regs = length;
      }
  }

  for (i = 0; i < search_regs.num_regs; i++)
    {
      marker = Fcar (list);
      if (NILP (marker))
	{
	  search_regs.start[i] = -1;
	  list = Fcdr (list);
	}
      else
	{
	  if (MARKERP (marker))
	    {
	      if (XMARKER (marker)->buffer == 0)
		XSETFASTINT (marker, 0);
	      else
		XSETBUFFER (last_thing_searched, XMARKER (marker)->buffer);
	    }

	  CHECK_NUMBER_COERCE_MARKER (marker, 0);
	  search_regs.start[i] = XINT (marker);
	  list = Fcdr (list);

	  marker = Fcar (list);
	  if (MARKERP (marker) && XMARKER (marker)->buffer == 0)
	    XSETFASTINT (marker, 0);

	  CHECK_NUMBER_COERCE_MARKER (marker, 0);
	  search_regs.end[i] = XINT (marker);
	}
      list = Fcdr (list);
    }

  return Qnil;  
}

/* If non-zero the match data have been saved in saved_search_regs
   during the execution of a sentinel or filter. */
static int search_regs_saved;
static struct re_registers saved_search_regs;

/* Called from Flooking_at, Fstring_match, search_buffer, Fstore_match_data
   if asynchronous code (filter or sentinel) is running. */
static void
save_search_regs ()
{
  if (!search_regs_saved)
    {
      saved_search_regs.num_regs = search_regs.num_regs;
      saved_search_regs.start = search_regs.start;
      saved_search_regs.end = search_regs.end;
      search_regs.num_regs = 0;
      search_regs.start = 0;
      search_regs.end = 0;

      search_regs_saved = 1;
    }
}

/* Called upon exit from filters and sentinels. */
void
restore_match_data ()
{
  if (search_regs_saved)
    {
      if (search_regs.num_regs > 0)
	{
	  xfree (search_regs.start);
	  xfree (search_regs.end);
	}
      search_regs.num_regs = saved_search_regs.num_regs;
      search_regs.start = saved_search_regs.start;
      search_regs.end = saved_search_regs.end;

      search_regs_saved = 0;
    }
}

/* Quote a string to inactivate reg-expr chars */

DEFUN ("regexp-quote", Fregexp_quote, Sregexp_quote, 1, 1, 0,
  "Return a regexp string which matches exactly STRING and nothing else.")
  (string)
     Lisp_Object string;
{
  register unsigned char *in, *out, *end;
  register unsigned char *temp;
  int backslashes_added = 0;

  CHECK_STRING (string, 0);

  temp = (unsigned char *) alloca (STRING_BYTES (XSTRING (string)) * 2);

  /* Now copy the data into the new string, inserting escapes. */

  in = XSTRING (string)->data;
  end = in + STRING_BYTES (XSTRING (string));
  out = temp; 

  for (; in != end; in++)
    {
      if (*in == '[' || *in == ']'
	  || *in == '*' || *in == '.' || *in == '\\'
	  || *in == '?' || *in == '+'
	  || *in == '^' || *in == '$')
	*out++ = '\\', backslashes_added++;
      *out++ = *in;
    }

  return make_specified_string (temp,
				XSTRING (string)->size + backslashes_added,
				out - temp,
				STRING_MULTIBYTE (string));
}
  
void
syms_of_search ()
{
  register int i;

  for (i = 0; i < REGEXP_CACHE_SIZE; ++i)
    {
      searchbufs[i].buf.allocated = 100;
      searchbufs[i].buf.buffer = (unsigned char *) malloc (100);
      searchbufs[i].buf.fastmap = searchbufs[i].fastmap;
      searchbufs[i].regexp = Qnil;
      staticpro (&searchbufs[i].regexp);
      searchbufs[i].next = (i == REGEXP_CACHE_SIZE-1 ? 0 : &searchbufs[i+1]);
    }
  searchbuf_head = &searchbufs[0];

  Qsearch_failed = intern ("search-failed");
  staticpro (&Qsearch_failed);
  Qinvalid_regexp = intern ("invalid-regexp");
  staticpro (&Qinvalid_regexp);

  Fput (Qsearch_failed, Qerror_conditions,
	Fcons (Qsearch_failed, Fcons (Qerror, Qnil)));
  Fput (Qsearch_failed, Qerror_message,
	build_string ("Search failed"));

  Fput (Qinvalid_regexp, Qerror_conditions,
	Fcons (Qinvalid_regexp, Fcons (Qerror, Qnil)));
  Fput (Qinvalid_regexp, Qerror_message,
	build_string ("Invalid regexp"));

  last_thing_searched = Qnil;
  staticpro (&last_thing_searched);

  defsubr (&Slooking_at);
  defsubr (&Sposix_looking_at);
  defsubr (&Sstring_match);
  defsubr (&Sposix_string_match);
  defsubr (&Ssearch_forward);
  defsubr (&Ssearch_backward);
  defsubr (&Sword_search_forward);
  defsubr (&Sword_search_backward);
  defsubr (&Sre_search_forward);
  defsubr (&Sre_search_backward);
  defsubr (&Sposix_search_forward);
  defsubr (&Sposix_search_backward);
  defsubr (&Sreplace_match);
  defsubr (&Smatch_beginning);
  defsubr (&Smatch_end);
  defsubr (&Smatch_data);
  defsubr (&Sset_match_data);
  defsubr (&Sregexp_quote);
}
