/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/parse_get_token.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __GETWORD_H_
#define __GETWORD_H_
#include <stdio.h>

/*
	This package performs low level parsing of a DNS RR in file
	format.  The routine parse_get_token() retrieves the next token
	regardless of comments and line continuations.  The word
	returned still contains the escpae sequences as in RFC 1035,
	except that any escaped carriage returns are natively in the
	word.

	The normal pattern of return values from parse_get_token is (barring
	errors):
		GW_WORD GW_WORD ... GW_WORD GW_EOLN

	The end of line (and end of file) are treated as separate tokens.

	There are three error conditions.

	GW_ERRCONTINUATION refers to running into an EOF with a
	dangling open parenthesis, or encountering a closing
	parenthesis without a corresponding open parenthesis.

	GW_ERRDANGLING_QUOTE refers to an unescaped carriage return
	or an eof within quotes.

	GW_ILLEGALCHAR refers to having a control character (0x01-0x1F
	and 0x7F - minus carriage return) in the input buffer.

	GW_TRUNC refers to having to little space in the passed buffer to
	hold the whole token.  The truncated token is returned and the
	remainder skipped in the input stream.

	The helper routines perform the following functions:

	setsourcefile - sets the source of the data to be the given file ptr.
	setsourceline - sets the source of the data to be the given char array.
	current_line - gives the current line number of the input stream.

	setsourcefile can be called repeatedly for the same file.  If
	this is done, the current_line is constantly reset to line 1. 
	For debugging support, the calling package will need to keep
	track of the file line number (for reporting errors). 
	current_line() will give the offset from that line of
	the current token.  At the end of the parsing procedure,
	current_line() will give the number of lines the input consumed.
	(If there was no line continuation, the number is one.)
*/

/*	setsourcefile

	Sets parse_get_token to read from the given file.

	FILE *master -	an open file descriptor pointing to a suspected RR.
			This descriptor is moved alone just as any other
			when reading from a file.

	Side effects
	Each call to this routine resets the current_line to 1.
	No management of the file pointer is atttempted.  At EOF, the
	file is not closed by this.
*/

void setsourcefile (FILE *master);

/*	setsourceline

	Sets parse_get_token to read from an in memory character array.

	char *master -	the start of a character array holding the RR.
			This line may have '\n' in it.  The address passed
			is not updated as the RR is parsed.

	Side effects
	Each call to this routine resets the current_line to 1.
	No management of the array pointer is atttempted.
*/

void setsourceline (char *master);

#define GW_WORD			0
#define GW_EOLN			1
#define GW_EOF			2
#define GW_ERRCONTINUATION	3
#define GW_ERRILLEGALCHAR	4
#define GW_ERRDANGLINGQUOTE	5
#define GW_TRUNC		6

/*	parse_get_token

	Gets the next token from the input source.

	char *buf -	an empty character array of limit bytes. The returned
			token is placed here and null terminated.
	int limit -	the capacity of buf

	Side effects
	If the source is a file, the file pointer is advanced.
	The current line is bumped when an '\n' is passed.

	Error conditions
	See description above.

	Other notes
	The parse table will be published.
*/

int parse_get_token (char *buf, int limit);

/*	current_line

	Returns the number of '\n' passed since the source was last set.
*/
int current_line();

/*	decrement_current_line

	Lowers the current line number by one - has one unusual application...
	when a continued line follows blanks/comments, the count by parse_get_token
	is decremented and the error context is incremented
*/
void decrement_current_line();

/*	get_next_char

	Reads the next input character.  Only publically available to allow
	the reading of the remainder of a non-RR line.
*/
char get_next_char();

/*	set_new_line

	Informs get token that a new line was encountered in "other" processing
	of the input stream (e.g., after collocting words in a non-rr).
*/

void set_new_line();

#endif

/* Portions Copyright (c) 1995,1996,1997,1998 by Trusted Information Systems, Inc.
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




