/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/iip_support.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __iip_support_h__
#define __iip_support_h__

#include <sys/types.h>

#ifdef SunOS
#include <sys/bitypes.h>
#endif

#define SETON(field,bit)	field[bit/8]|=(1<<(7-(bit%8)))
#define SETOFF(field,bit)	field[bit/8]&=~(1<<(7-(bit%8)))
#define ISSET(field,bit)	(field[bit/8]&(1<<(7-(bit%8))))
#define SETCLEAR(field)		memset(field,'\0',sizeof(field))

#ifdef MEMORY_DEBUGGING
#define MALLOC(s) my_malloc(s, __FILE__, __LINE__)
#define FREE(p) my_free(p,__FILE__,__LINE__)
#define STRDUP(p) my_strdup(p,__FILE__,__LINE__)
#else
#define MALLOC(s) malloc(s)
#define FREE(p) free(p)
#define STRDUP(p) strdup(p)
#endif

/*	Generic routines (i.e. not DNS related) */

/*	print_hex_field

	Prints the hex bytes (and if isprint() the printed interpretation
	below) in field.  'length' is the number of bytes to print, 'width'
	is the number of bytes to have in a line, and 'pref' is a character
	string to prefix a line with.  The latter is useful for indenting.
	This function is very, very to have around in gdb.
*/

void print_hex_field (u_int8_t field[], int length, int width, char *pref);

/*	print_hex

	Prints the hex values in field, length bytes worth.  There is no line
	line wrapping or ASCII conversion performed here.
*/
void print_hex (u_int8_t field[], int length);

/*	complete_read

	Reads from a socket until all data expected is received or an error
	occurs.
*/

int complete_read (int sock, void* field, int length);

/* Memory debugging routines */

void my_free (void *p, char *filename, int lineno); 
void *my_malloc (size_t t, char *filename, int lineno);
char *my_strdup (const char *str, char *filename, int lineno);

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




