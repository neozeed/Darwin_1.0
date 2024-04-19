/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/dns_parse.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __DNS_PARSE_H__
#define __DNS_PARSE_H__

#include <stdio.h>
#include <arpa/nameser.h>
#include "parse_get_token.h"

#define ERR_UNSET	0 /* RR parsed successfully */
#define ERR_OKBUTNOTRR	1 /* Line is not recognizable as an RR */
#define	ERR_PARSEERROR	2 /* Line failed to parse */
#define ERR_NOMOREDATA	3 /* Called when staring at EOF(or End Of Data) */
#define ERR_OUTOFMEMORY	4 /* A call to malloc failed along the line */

struct error_data
{
	char		*ed_filename;	/* Input file name */
	int		ed_curr_line;   /* Current line of file */
	int		ed_error_value;	/* One of three ERR_ values above */
};

struct name_context
{
	u_int8_t	nc_origin_n[MAXDNAME];   /* The current $ORIGIN value */
	u_int8_t	nc_previous_n[MAXDNAME]; /* Previous record owner */
	u_int32_t	nc_min_ttl_n;	/* Zone ttl, SOA ignores then sets */
	u_int16_t	nc_class_n;	/* Default class */
};

/*	This routine expects the file pointer in master to be where an RR
	is expected.  If the routine finds an RR, it converts it to the 
	uncompressed RR format  and leaves it in 'rr'.  If not, the line
	is left in its original form in non_rr.

	If something other than an RR is found, this is not necessarily
	an error condition.  The line may be a directive or a comment.

	An upcoming record is determined to be an RR if it begins with a
	character that has not been registered as having a special meaning.
	Registration of characters is done with "register_special_char()",
	which appears later in this file.

	If the return value is 0, then the errors->ed_error_value will be
	ERR_UNSET, *non_rr will be NULL, and *rr will point to malloc'd
	memory with the uncompressed RR.

	If the return value is -1, then either
		errors->ed_error_value will be ERR_NOTRRBUTOK
		and *non_rr will point to the malloc'd memory holding the line
	or
		errors->ed_error_value will be ERR_PARSEERROR
		and *non_rr will point to the malloc'd memory holding an error
			message
--what about ERR_NOMOREDATA
	or
		error->ed_error_value will be ERR_OUTOFMEMORY
		and *non_rr

	In addition, *rr will be NULL (whenever the return value is -1).

	Parameter notes:
	master -	File must be open and ready for reading prior to call.

	errors -	Structure must be initialized prior to first call.
			ed_filename should be specified (if reading from file),
			ed_curr_line is updated when the parsing is complete,
			its initial value should be 0.
			ed_error_value does not matter on input.
	context -	Structure must be initialized prior to first call.
			nc_origin_n is the network representation of the
			current origin name.  The initial value is a pointer
			to a null byte (0x00) representing the root name.  This
			value is updated by what ever handles the directives
			kicked away from the parsing routines.
			nc_previous_n is the network representation of the
			previous RR owner name.  Initially it is also a pointer
			to NULL, and when the origin is set, this is also
			to be updated by the directive handler.
			nc_min_ttl_n and nc_class_n are both network reps of
			the respective information.  They are set upon the
			parsing of the SOA record.
	*rr - 		This must be set to NULL for the first call.  To assist
			in memory management, if this is not NULL, an attempt
			is made to free it.  This will later either point to a
			parsed RR or to NULL.  If the previous RR is not to
			be freed, then save a pointer to it elsewhere and
			set the actual parameter to NULL prior to the call.
	*non_rr -	See the note for *rr, with the exception that if
			*non_rr is set to something other than NULL, it is
			either an error message or the original line.
*/

int read_rr (	FILE *master,
		struct error_data *errors,
		struct name_context *context,
		u_int8_t **rr,
		char **non_rr);

/*	The following routine is the same, except that the data is pulled from
	the beginning of the character array passed (instead of a file).
*/

int parse_rr (	char *master,
		struct error_data *errors,
		struct name_context *context,
		u_int8_t **rr,
		char **non_rr);

/*	This routine registers certain characters as having special meaning
	in the parsing of a RR.  For example, the dollar sign '$' is used
	by BIND to signify a directive, so a call to register_special_char('$')
	is made.  When the parsing routines encounter on of these characters
	in the first position of the first token, the incoming data is
	treated as a non-RR.
*/
void register_special_char (char ch);
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




