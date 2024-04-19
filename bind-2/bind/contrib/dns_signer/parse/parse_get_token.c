/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/parse_get_token.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

/* Portions Copyright (c) 1995,1996,1997,1998 by Trusted Information Systems, In
c.  
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL TRUSTED INFORMATION
 * SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 *  
 * Trusted Information Systems, Inc. has received approval from the
 * United States Government for export and reexport of TIS/DNSSEC
 * software from the United States of America under the provisions of
 * the Export Administration Regulations (EAR) General Software Note
 * (GSN) license exception for mass market software.  Under the
 * provisions of this license, this software may be exported or
 * reexported to all destinations except for the embargoed countries of
 * Cuba, Iran, Iraq, Libya, North Korea, Sudan and Syria.  Any export
 * or reexport of TIS/DNSSEC software to the embargoed countries
 * requires additional, specific licensing approval from the United
 * States Government.
 */

#include "parse_get_token.h"
#include <ctype.h>

#ifndef TRUE
#define TRUE	!!1
#endif

#ifndef FALSE
#define FALSE	!TRUE
#endif

static FILE	*master_file = NULL;
static char	master_lookahead;
static char	*master_line = NULL;
static int	master_index = 0;
static int	line_number = 0;
static int	just_read_eoln = TRUE;

void setsourcefile (FILE *master)
{
	master_file = master;
	master_lookahead = '\0';
	master_line = NULL;
	master_index = 0;
	line_number=0;
	just_read_eoln=TRUE;
}

void setsourceline (char *master)
{
	master_file = NULL;
	master_lookahead = '\0';
	master_line = master;
	master_index = 0;
	line_number=0;
	just_read_eoln=TRUE;
}

char get_next_char ()
{
	char return_ch;

	if (master_file != NULL)
	{
		/* Assume we're reading from the file */
		if (master_lookahead != '\0')
		{
			return_ch = master_lookahead;
			master_lookahead = '\0';

			if (just_read_eoln) line_number++;
			just_read_eoln = (return_ch=='\n');

			return return_ch;
		}
		else
		{
			return_ch = fgetc (master_file);

			if (!feof (master_file))
			{
				if (just_read_eoln) line_number++;
				just_read_eoln = (return_ch=='\n');

				return return_ch;
			}
			else
				return '\0';
		}
	}
	else if (master_line)
	{
		/* Assume we're reading from the line */
		if (master_index <= strlen (master_line))
			return_ch = master_line[master_index++];
		else
			return_ch = master_line[master_index];

		if (just_read_eoln) line_number++;
		just_read_eoln = (return_ch=='\n');

		return return_ch;
	}	

	return '\0';
}

void put_back_char (char	ch)
{
	if (master_file!=NULL)
			master_lookahead = ch;
	else
		if (master_index) master_index--;

	just_read_eoln = FALSE;
}


static int first_word = TRUE;
static int cont_line = 0;

#define ADD_CHAR(a,b)	if (token_index >= limit)\
							token_truncated = TRUE;\
						else\
							a[token_index++] = b

#define GW_MAYBE_WORD	token_truncated?GW_TRUNC:GW_WORD

#define SEPARATOR(a)	a==';'||a=='('||a==')'||a=='"'||a=='\0'||a=='\n'

int parse_get_token (char *buf, int limit)
{
	int token_index = 0;
	int escape_state = FALSE;
	int quoted_state = FALSE;
	int comment_state = FALSE;
	int token_truncated = FALSE;

	char	current_ch, ch;

	do
	{
		current_ch = get_next_char();
		if (comment_state)
		{
			/*
				A comment is ended by an EOLN or EOF.
				If cont_line is not 0, we keep looking
				for a token (or until a EOF is hit).
				If it is, we return EOF or EOLN.
			*/
			if (current_ch == '\n')
			{
				if (cont_line == 0)
				{
					first_word = TRUE;
					return GW_EOLN;
				}
				comment_state = FALSE;
			}
			else if (current_ch == '\0')
			{
				first_word = TRUE;
				if (cont_line)
					return GW_ERRCONTINUATION;
				else
					return GW_EOF;
			}
		}
		else if (escape_state)
		{
			/*
				An escape character was hit.  The next
				character is copied in (re-escaped) with
				the exception of the end of line - which is
				copied unescaped.

				If a digit follows the escape character there
				are supposed to be two more, but that error
				checking is left to the next level of parsing.
			*/
			if (current_ch == '\n')
			{
				ADD_CHAR (buf, current_ch);
			}
			else if (current_ch == 0x7F || current_ch < 0x20)
			{
				do
					ch = get_next_char();
				while (ch != '\n' && ch != '\0');
				if (ch== '\0') put_back_char (ch);

				first_word = TRUE;
				cont_line = 0;
				return GW_ERRILLEGALCHAR;
			}
			else
			{
				ADD_CHAR (buf, '\\');
				ADD_CHAR (buf, current_ch);
			}
			escape_state = FALSE;
		}
		else if (quoted_state)
		{
			/*
				In the middle of a quote, escapes matter,
				and we don't return the enclosing quote
				marks.
			*/
			if (current_ch == '\\')
				escape_state = TRUE;
			else if (current_ch == '\n' || current_ch == '\0')
			{
				buf[token_index]='\0';
				if (cont_line > 0)
				{
					while (cont_line > 0)
					{
						do
							ch = get_next_char();
						while (ch != '\n' && ch != '\0');
						cont_line--;
					}
					put_back_char (ch);
				}
				first_word = TRUE;
				cont_line = 0;
				return GW_ERRDANGLINGQUOTE;
			}
			else if (current_ch == '"')
			{
				buf[token_index]='\0';
				return GW_MAYBE_WORD;
			}
			else if (current_ch!='\t' && (current_ch<0x20 || current_ch==0x7F))
			{
				do
					ch = get_next_char();
				while (ch != '\n' && ch != '\0');

				first_word = TRUE;
				cont_line = 0;
				return GW_ERRILLEGALCHAR;
			}
			else
			{
				ADD_CHAR (buf, current_ch);
			}
		}
		else if (token_index > 0)
		{
			/*
				In this case, there is a token being
				built.  It is either added to or a
				separating character ends it and itself
				may be left (e.g., a ; beginning a comment
				ends the previous token and is left to
				signal the start of the comment next time
				around.
			*/
			if (isspace(current_ch)&&current_ch!='\n'&&current_ch!='\0')
			{
				first_word = FALSE;
				buf[token_index]='\0';
				if (current_ch== '\0') put_back_char (current_ch);
				return GW_MAYBE_WORD;
			}
			else if (current_ch == '\\')
			{
				escape_state = TRUE;
			}
			else if (SEPARATOR(current_ch))
			{
				first_word = FALSE;
				put_back_char (current_ch);
				buf[token_index]='\0';
				return GW_MAYBE_WORD;
			}
			else if (current_ch < 0x20 || current_ch == 0x7F)
			{
				do
					ch = get_next_char();
				while (ch != '\n' && ch != '\0');
				if (ch== '\0') put_back_char (ch);

				first_word = TRUE;
				cont_line = 0;
				return GW_ERRILLEGALCHAR;
			}
			else
				ADD_CHAR (buf, current_ch);
		}
		else
			/*
				The character is potentially the
				start of a token.  Some of these
				may have been seen by the above code and
				returned to the input stream.
			*/
			switch (current_ch)
			{
				case ';':
					buf[token_index]='\0';
					comment_state = TRUE;
					break;
				case '(':
					cont_line++;
					do
						ch = get_next_char();
					while (ch != '\n' && ch != '\0');
					if (ch== '\0') put_back_char (ch);

					first_word = FALSE;
					break;
				case ')':
					cont_line--;
					while ((ch = get_next_char()) != '\n')
					{
						if (ch == '\0') return GW_EOF;
						if (ch == ')')
						{
							put_back_char (ch);
							break;
						}
					}
					if (cont_line==0)
					{
						first_word = TRUE;
						return GW_EOLN;
					}
					if (cont_line < 0)
					{
						/*
							We've already read to
							the end of the line
						*/
						first_word = TRUE;
						cont_line = 0;
						return GW_ERRCONTINUATION;
					}
					break;
				case ' ': case '\t':
					if (first_word)
					{
						ADD_CHAR (buf, current_ch);
						buf[token_index]='\0';
						first_word = FALSE;
						return GW_MAYBE_WORD;
					}
					break;
				case '\\':
					escape_state = TRUE;
					break;
				case '"':
					quoted_state = TRUE;
					break;
				case '\n':
					if (cont_line==0)
					{
						first_word = TRUE;
						return GW_EOLN;
					}
					break;
				case '\0':
					if (cont_line == 0)
						return GW_EOF;
					else
					{
						cont_line = 0;
						return GW_ERRCONTINUATION;
					}
				default:
					if (current_ch<0x20 || current_ch==0x7F)
					{
						do
							ch = get_next_char();
						while (ch != '\n' && ch != '\0');
						if (ch== '\0') put_back_char (ch);

						first_word = TRUE;
						cont_line = 0;
						return GW_ERRILLEGALCHAR;
					}
					else
					{
						ADD_CHAR (buf, current_ch);
					}
			} /* end switch body */
	} while (TRUE); /* end the do-while loop */
}

void decrement_current_line()
{
	if (line_number) line_number--;
}

int current_line()
{
	return line_number;
}

void set_new_line()
{
	first_word = TRUE;
	cont_line = 0;
}
