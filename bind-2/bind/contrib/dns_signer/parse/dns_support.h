/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/dns_support.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __dns_support_h__
#define __dns_support_h__

#include <sys/types.h>

/*	DNS protocol specific, but not BIND dependent */

/*	wire_name_length

	Calculates the number of bytes in a DNS wire format name

	Return values:
		0 = Either the parameter is NULL, or the name is too long (> 255)
		1-255 = length of name
*/

u_int16_t wire_name_length (const u_int8_t *field);

/*	wire_name_labels

	Calculates the number of labels in a DNS wire format name

	Return values:
		0 = Either the parameter is NULL, or the name is too long (> 255)
		1-128 = number of labels

	The root name, 0x00, has one label.  "COM." has two labels 0x03 C O M 0x00.
	Caution: this is not the same as the label count in the SIG record.
*/

u_int16_t wire_name_labels (const u_int8_t *field);

/*	wire_length

	Calculates the number of bytes in a DNS wire format record */

u_int16_t wire_length (const u_int8_t *field);

/*	temporally_challenged_signature

	Returns true if the signature has expired or the time signed is in the
	future.  Also returns true if the RR is not a signature.
*/

int temporally_challeneged_signature (const u_int8_t *rr);

/*	dump_response

	Performs a somewhat formatted raw dump of the response.  What this
	means is that the header (including rcode) is printed in hex on one
	line, the query on the next, and then each response RR is dumped on
	a line by itself.  No attempt is made to interpret the hex values
	beyond this.
*/

void dump_response (const u_int8_t *ans, int resplen);

/*	skip_questions

	Returns the index of the first byte of the answer section
	(authority if the answer is empty) of a DNS response
	(starting at memory address buf).
*/

int skip_questions(const u_int8_t *buf);

/*	namecmp

	Compare the DNS wire format names in name1 and name2 based upon
	DNSSEC sorting rules.  Returns -1 if name1 comes before name2, 
	0 if the names are equivalent, and +1 otherwise.
*/

int	namecmp (const u_int8_t *name1, const u_int8_t *name2);

/*	retrieve_type

	Gets the host ordered type value from an RR.
*/

u_int16_t retrieve_type (const u_int8_t *rr);

int rr_length (const u_int8_t *buf, int buf_index);

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




