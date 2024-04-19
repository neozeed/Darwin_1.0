/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_key_monitor.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __signer_key_monitor_h__
#define __signer_key_monitor_h__

#include "dns_signer.h"

struct key_matter* key_add (	u_int8_t		*name_n,
				u_int8_t		algorithm,
				int			algorithm_any,
				u_int16_t		footprint_h,
				int			footprint_any,
				int			signing_requested,
				u_int16_t		rdata_length_h,
				u_int8_t		*rdata_n,
				struct signing_options *options);

void key_print_key_matter ();

void key_print_key_matter_pointers ();

int key_list_copy (	struct key_list		**the_copy,
			struct key_list		*the_original,
			struct signing_options	*options);

int key_list_merge (	struct key_list		**old_list,
			struct key_list		*new_list,
			struct signing_options	*options);

void key_list_delete (	struct key_list		**the_keys);

int key_already_in_key_list (
			struct key_list		*the_keys,
			u_int8_t		*name_n,
			int			algorithm_any,
			u_int8_t		algorithm,
			int			foot_any,
			u_int16_t		foot_h);

int key_add_to_list (	u_int8_t		*name_n,
			int			algorithm_any,
			u_int8_t		algorithm,
			int			foot_any,
			u_int16_t		foot_h,
			struct key_list		**key_list,
			struct signing_options	*options);

void key_remove_from_list(
			u_int8_t		*name_n,
			int			algorithm_any,
			u_int8_t		algorithm,
			int			foot_any,
			u_int16_t		foot_h,
			struct key_list		**key_list);

int key_open(		struct signing_options *options);

int key_is_a_zone_key (
			u_int8_t *rdata_n);
int key_is_a_signing_key (
			u_int8_t *rdata_n);
int key_is_a_NULL_key (
			u_int8_t *rdata_n);

void key_cleanup();

int key_add_from_parent_file (char *filename);

int key_find_soa_signers (struct signing_options *options);

int key_footprint_conflict (
				struct rrset_rec	*set_rec,
				u_int16_t		the_rdata_length_h,
				u_int8_t		*the_rdata_n);

struct key_matter *key_find_public (
				u_int8_t		*name_n,
				u_int8_t		algorithm,
				u_int16_t		footprint_h);

struct key_matter *key_add_public (
				u_int8_t		*name_n,
				u_int8_t		algorithm,
				u_int16_t		foot_h,
				struct rrset_rec	*key_set,
				struct rr_rec		*key_rr,
				struct signing_options	*options);

struct rr_rec *key_find_key (	struct rrset_rec	*key_set,
				u_int8_t		algorithm,
				u_int16_t		footprint_h);

void key_list_filter (		struct key_list		**the_copy,
				u_int8_t		*name_n);

DST_KEY *key_open_public (	u_int8_t		*name_n,
				u_int8_t		algorithm,
				u_int16_t		footprint_h);

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




