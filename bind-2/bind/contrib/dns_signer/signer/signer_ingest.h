/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_ingest.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __signer_ingest_h__
#define __signer_ingest_h__

#include "dns_signer.h"

int parse_from_file (struct signing_options *options);

int in_domain (u_int8_t *zone_name_n, u_int8_t *the_name_n);
int found_ns (u_int8_t *zone_name_n, u_int8_t *the_name_n);

void extract_from_rr (	u_int8_t	*rr,
			u_int8_t	**the_name_n,
			u_int16_t	*the_type_h,
			u_int16_t	*the_set_type_h,
			u_int16_t	*the_class_n,
			u_int32_t	*the_ttl_n,
			u_int16_t	*the_rdata_length_h,
			u_int8_t	**the_rdata_n,
			u_int32_t	*sig_expiry_n);

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




