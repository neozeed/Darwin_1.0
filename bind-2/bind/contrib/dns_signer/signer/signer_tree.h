/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_tree.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __signer_tree_h__
#define __signer_tree_h__

#include "dns_signer.h"
#include "tree.h"

int tree_start();

struct name_rec *tree_add_name( u_int8_t		*the_name_n,
				u_int16_t		the_class_n,
				struct signing_options	*options);

struct rrset_rec *tree_add_set(	struct name_rec		*name_rec,
				u_int16_t		the_set_type_h,
				u_int32_t		the_ttl_n,
				u_int8_t		*the_rdata_n,
				struct signing_options	*options);

struct name_rec *tree_find_name(u_int8_t*		the_name_n);

struct rrset_rec *tree_find_set(struct name_rec *name_rec,
				u_int16_t		the_type_h);

void tree_move_to_subzone (u_int8_t *name_n);

void tree_print();

void tree_print_pointers();

void tree_cleanup();

int tree_sign (struct signing_options *options);

int tree_add_as_sig (		struct rrset_rec	*set_rec,
				u_int16_t		the_rdata_length_h,
				u_int8_t*		the_rdata_n,
				struct signing_options	*options);

int tree_add_as_data (		struct rrset_rec	*set_rec,
				u_int16_t		the_rdata_length_h,
				u_int8_t*		the_rdata_n,
				struct signing_options	*options);

void tree_delete_parent_files (struct signing_options *options);

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




