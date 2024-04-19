/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/WKS_n_RFC1700.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __wks_N_rfc1700_h__
#define __wks_N_rfc1700_h__

/*
	This maps the {service, protocol} into the port number assigned it
	in the well known numbers RFC.  The latest RFC for this is RFC 1700.
	A -1 return means the service is not found, 0 means it was and port 
	is set to it.
*/

int	getservicenumber (char* service, char* protocol, u_int16_t *port);

/* This frees the dynamically linked list built for the look ups */

void free_port_numbers();
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




