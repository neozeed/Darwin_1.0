/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/parse_error_message.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

#include "pkg_internal_dns_parse.h"

/*
	These routines build error messages.  The generic error message is of the
	form:

	<filename>[<line number>] : <error message>

	The brackets are included in the message (they are not denoting optional
	items in this case).  For now, assume the line number is %5d.

	In general, assume 10 bytes for each number.  Over guessing string length
	is not harmful here.  Hey, we're talking error messages...

	The base size of the message is the strlen of the filename + 10.  But
	since I'll allow 10 spaces for a number alone, we'll bump this to 15.

	Ohh, and add one for the null at the end.

	A trailing 6 is added in case the error covers multiple lines (because
	line continuation is in use). "<file>[<5d>-<5d>}" prefix...

*/

#define	BASE_SIZE(a)	strlen(a->ed_filename)+15+2+6

char *err_msg_n (struct error_data *err, char *fmt)
{
	char* ret = (char*) MALLOC (BASE_SIZE(err)+strlen(fmt));

	if (ret == NULL)
	{
		err->ed_error_value = ERR_OUTOFMEMORY;
		return NULL;
	}

	if (current_line()==1)
		sprintf (ret, "%s[%5d] : %s", err->ed_filename,
									err->ed_curr_line+1, fmt);
	else
		sprintf (ret, "%s[%5d-%5d] : %s", err->ed_filename,
									err->ed_curr_line+1,
									err->ed_curr_line+current_line(),
									fmt);
	return ret;
}

char *err_msg_sn (struct error_data *err, char *fmt, const char *one)
{
	char* temp = (char*) MALLOC (BASE_SIZE(err)+strlen(fmt));
	char* ret = (char*) MALLOC (BASE_SIZE(err)+strlen(fmt)+strlen(one));

	if (ret == NULL || temp == NULL)
	{
		err->ed_error_value = ERR_OUTOFMEMORY;
		return NULL;
	}

	if (current_line()==1)
		sprintf (temp, "%s[%5d] : %s", err->ed_filename,
									err->ed_curr_line+current_line(), fmt);
	else
		sprintf (temp, "%s[%5d-%5d] : %s", err->ed_filename,
									err->ed_curr_line+1,
									err->ed_curr_line+current_line(),
									fmt);
	sprintf (ret, temp, one);
	FREE (temp);
	return ret;
}

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




