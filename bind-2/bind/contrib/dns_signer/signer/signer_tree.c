/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_tree.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#include "signer_tree.h"
#include "signer_ingest.h"
#include "signer_debug.h"
#include "signer_key_monitor.h"
#include "signer_sign.h"

static tree				*the_tree;
static struct name_rec	*previous_one;

int tree_start()
{
	the_tree = NULL;
	previous_one = NULL;
	return SIGNER_OK;
}

int tree_name_compare (struct name_rec	*first, struct name_rec *second)
{
	return namecmp (first->nr_name_n, second->nr_name_n);
}

void tree_free_second (struct name_rec  *first, struct name_rec *second)
{
	FREE (second->nr_name_n);
	FREE (second);
}

struct name_rec *tree_add_name(	u_int8_t*				the_name_n,
								u_int16_t				the_class_n,
								struct signing_options	*options)
{
	/*
		Search tree for the_name_n
		If it is found, then return that pointer unchanged (and tree unchanged)
		If not, insert name and class, and return that pointer (tree changed)
	*/

	int				the_name_length;
	struct name_rec *new_one;

	if (the_tree==NULL)
	{
		tree_init(&the_tree);
	}

	if (previous_one != NULL && namecmp (previous_one->nr_name_n,the_name_n)==0)
		return previous_one;

	the_name_length = wire_name_length(the_name_n);
	new_one=(struct name_rec *)MALLOC(sizeof(struct name_rec));

	if (new_one==NULL)
	{
		wire_to_ascii_name (debug_name1_h, the_name_n, MAXPNAME);
		sprintf (debug_message, "Out of memory - adding name %s to tree",
									debug_name1_h);
		ERROR (debug_message);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	new_one->nr_name_n = (u_int8_t*)MALLOC(the_name_length);

	if (new_one->nr_name_n==NULL)
	{
		wire_to_ascii_name (debug_name1_h, the_name_n, MAXPNAME);
		sprintf (debug_message, "Out of memory - adding name %s to tree",
									debug_name1_h);
		ERROR (debug_message);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	memcpy (new_one->nr_name_n, the_name_n, the_name_length);
	new_one->nr_class_n = the_class_n;
	new_one->nr_location = NR_LOCATION_INMEMORY;
	new_one->nr_status = NR_STATUS_UNSET;
	new_one->nr_rrsets = NULL;

	return (previous_one = (struct name_rec *) tree_add (&the_tree,
							tree_name_compare, new_one, tree_free_second));
}

#define NXT_UPPER	0
#define NXT_LOWER	1

int nxt_match_test (struct name_rec *nr,
					struct rrset_rec *new_set,
					u_int8_t *the_rdata_n)
{
	int	which_one_is_set;
	int	which_one_is_rdata;
	int index;

	/* Can assume we are talkin' apex NXT's here. */

	if (new_set->rrs_data == NULL && new_set->rrs_sig == NULL)
		return TRUE;

	/*
		Are we lookin' to add a NXT or SIG(NXT)?
			If the first two bytes are 0x001E, then
			if the data is part of an NXT, the 00 means root, and 1E
			means it has (among others) a CNAME.  Since this isn't
			possible, 00 1E must be interpreted as a sig's type
			covered for SIG(NXT)
	*/

	if (the_rdata_n[0]==0x00 && the_rdata_n[1]==0x1e)
	{
		/* We have a signature for sure */
		which_one_is_rdata = namecmp (nr->nr_name_n,
					&the_rdata_n[SIG_RDATA_BY_NAME])==0? NXT_LOWER:NXT_UPPER;
	}
	else
	{
		/* We have a nxt for sure */
		index = wire_name_length(the_rdata_n);
		which_one_is_rdata = ISSET((&the_rdata_n[index]), ns_t_soa)?
									 NXT_LOWER:NXT_UPPER;
	}

	if (new_set->rrs_data != NULL)
	{
		index = wire_name_length(new_set->rrs_data->rr_rdata);
		which_one_is_set = ISSET((&new_set->rrs_data->rr_rdata[index]),
								ns_t_soa)? NXT_LOWER:NXT_UPPER;
	}
	else
	{
		which_one_is_set = namecmp (nr->nr_name_n,
			&new_set->rrs_sig->rr_rdata[SIG_RDATA_BY_NAME])==0 ?
													NXT_LOWER:NXT_UPPER;
	}

	return which_one_is_rdata == which_one_is_set;
}

struct rrset_rec *tree_add_set(	struct name_rec			*name_rec,
								u_int16_t				the_set_type_h,
								u_int32_t				the_ttl_n,
								u_int8_t				*the_rdata_n,
								struct signing_options	*options)
{
	struct rrset_rec	*new_set = (name_rec)?name_rec->nr_rrsets:NULL;

	if (name_rec == NULL)
	{
		ERROR ("NULL pointer to name rec passed to tree_add_set");
		return NULL;
	}

	if (new_set == NULL)
	{
		name_rec->nr_rrsets=(struct rrset_rec*)MALLOC(sizeof(struct rrset_rec));
		if (name_rec->nr_rrsets==NULL)
		{
			wire_to_ascii_name (debug_name1_h, name_rec->nr_name_n, MAXPNAME);
			sprintf (debug_message, "Out of memory - adding set %s to %s",
				local_sym_ntop(__p_type_syms, the_set_type_h, &debug_success),
				debug_name1_h);
			ERROR (debug_message);
			tree_delete_parent_files (options);
			sign_cleanup (options);
			exit (1);
		}

		name_rec->nr_rrsets->rrs_type_h = the_set_type_h;
		name_rec->nr_rrsets->rrs_parental_glue = FALSE;
		name_rec->nr_rrsets->rrs_ttl_n = the_ttl_n;
		name_rec->nr_rrsets->rrs_data = NULL;
		name_rec->nr_rrsets->rrs_sig = NULL;
		name_rec->nr_rrsets->rrs_signers = NULL;
		name_rec->nr_rrsets->rrs_recyclers = NULL;
		name_rec->nr_rrsets->rrs_next = NULL;

		return name_rec->nr_rrsets;
	}

	if (new_set->rrs_type_h == the_set_type_h &&
			(new_set->rrs_type_h != ns_t_nxt ||
			name_rec->nr_status != NR_STATUS_APEX ||
			nxt_match_test (name_rec, new_set, the_rdata_n)))
	{
		return new_set;
	}

	while (new_set->rrs_next != NULL)
	{
		if (new_set->rrs_next->rrs_type_h == the_set_type_h &&
			(new_set->rrs_next->rrs_type_h != ns_t_nxt ||
			name_rec->nr_status != NR_STATUS_APEX ||
			nxt_match_test (name_rec, new_set->rrs_next, the_rdata_n)))
		{
			return new_set->rrs_next;
		}
		new_set = new_set->rrs_next;
	}

	new_set->rrs_next=(struct rrset_rec*)MALLOC(sizeof(struct rrset_rec));
	if (new_set->rrs_next==NULL)
	{
		wire_to_ascii_name (debug_name1_h, name_rec->nr_name_n, MAXPNAME);
		sprintf (debug_message, "Out of memory - adding set %s to %s",
			local_sym_ntop(__p_type_syms, the_set_type_h, &debug_success),
			debug_name1_h);
		ERROR (debug_message);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	new_set->rrs_next->rrs_type_h = the_set_type_h;
	new_set->rrs_next->rrs_parental_glue = FALSE;
	new_set->rrs_next->rrs_ttl_n = the_ttl_n;
	new_set->rrs_next->rrs_data = NULL;
	new_set->rrs_next->rrs_sig = NULL;
	new_set->rrs_next->rrs_signers = NULL;
	new_set->rrs_next->rrs_recyclers = NULL;
	new_set->rrs_next->rrs_next = NULL;

	return new_set->rrs_next;
}

struct name_rec *tree_find_name(u_int8_t*			the_name_n)
{
	struct name_rec	searcher;

	if (the_tree==NULL) return NULL;
	searcher.nr_name_n = the_name_n;

	return tree_srch (&the_tree, tree_name_compare, &searcher);
}

struct rrset_rec *tree_find_set(struct name_rec		*name_rec,
								u_int16_t			the_type_h)
{
	struct rrset_rec	*the_one;

	if (name_rec == NULL) return NULL;

	the_one = name_rec->nr_rrsets;

	while (the_one != NULL)
	{
		if (the_one->rrs_type_h == the_type_h) break;
		the_one = the_one->rrs_next;
	}

	return the_one;
}

int tree_marker (struct name_rec *nr, u_int8_t *name_n)
{
	struct rrset_rec	*a_set;

	if (namecmp(nr->nr_name_n, name_n)==0)
	{
		for (a_set = nr->nr_rrsets; a_set!=NULL; a_set = a_set->rrs_next)
			if (a_set->rrs_type_h != ns_t_key)
				key_list_delete (&a_set->rrs_signers);
		return TRUE;
	}

	if (in_domain (name_n, nr->nr_name_n))
	{
		if (nr->nr_status == NR_STATUS_IN)
		{
			nr->nr_status = NR_STATUS_SUBZONE;
			for (a_set = nr->nr_rrsets; a_set!=NULL; a_set = a_set->rrs_next)
				key_list_delete (&a_set->rrs_signers);
		}

		return TRUE;
	}

	return FALSE;
}

void tree_move_to_subzone (u_int8_t *name_n)
{
	struct name_rec	searcher;

	if (the_tree==NULL) return;

	searcher.nr_name_n = name_n;

	tree_seek_trav (&the_tree, tree_name_compare, &searcher,
				tree_marker, name_n);
}

void tree_print_short()
{
	if (the_tree != NULL)
		tree_trav (&the_tree, print_name_rec, (void*)1);
}

void tree_print()
{
	if (the_tree != NULL)
		tree_trav (&the_tree, print_name_rec, NULL);
}

void tree_print_pointers()
{
	if (the_tree != NULL)
		tree_trav (&the_tree, print_name_rec_pointers, NULL);
}

void tree_cleanup ()
{
	if (the_tree==NULL) return;
	tree_mung (&the_tree, free_name_rec);
	the_tree = NULL;
}

int tree_sign (struct signing_options *options)
{
	time_t	current_time = time(0);

	if (the_tree==NULL) return SIGNER_OK;

	options->so_time_signed_n = htonl(current_time);
	options->so_time_expired_n = htonl(current_time +
										options->so_time_expired_duration_h);

	tree_trav (&the_tree, sign_name, options);

	if (options->so_return_code == SIGNER_OK)
		sign_finish (options);

	return options->so_return_code;
}

int tree_add_as_sig (	struct rrset_rec		*set_rec,
						u_int16_t				the_rdata_length_h,
						u_int8_t				*the_rdata_n,
						struct signing_options	*options)
{
	struct rr_rec	*rr;
	struct rr_rec	*last;

	if ((rr=(struct rr_rec *)MALLOC(sizeof(struct rr_rec)))==NULL)
	{
		ERROR ("Out of memory adding data - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	if ((rr->rr_rdata=(u_int8_t *)MALLOC(the_rdata_length_h))==NULL)
	{
		ERROR ("Out of memory adding data - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	rr->rr_rdata_length_h = the_rdata_length_h;
	memcpy (rr->rr_rdata, the_rdata_n, the_rdata_length_h);

	if (set_rec->rrs_sig ==NULL)
		set_rec->rrs_sig = rr;
	else
	{
		last = set_rec->rrs_sig;
		while (last->rr_next) last = last->rr_next;

		last->rr_next = rr;
	}
	rr->rr_next = NULL;

	return SIGNER_OK;
}

int tree_add_as_data (	struct rrset_rec		*set_rec,
						u_int16_t				the_rdata_length_h,
						u_int8_t				*the_rdata_n,
						struct signing_options	*options)
{
	struct rr_rec	*rr;
	struct rr_rec	*last;

	if ((rr=(struct rr_rec *)MALLOC(sizeof(struct rr_rec)))==NULL)
	{
		ERROR ("Out of memory adding data - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	if ((rr->rr_rdata=(u_int8_t *)MALLOC(the_rdata_length_h))==NULL)
	{
		ERROR ("Out of memory adding data - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	rr->rr_rdata_length_h = the_rdata_length_h;
	memcpy (rr->rr_rdata, the_rdata_n, the_rdata_length_h);

	if (set_rec->rrs_data ==NULL)
		set_rec->rrs_data = rr;
	else
	{
		last =  set_rec->rrs_data;
		while (last->rr_next) last = last->rr_next;

		last->rr_next = rr;
	}
	rr->rr_next = NULL;

	return SIGNER_OK;
}

void tree_delete_parent_files (struct signing_options *options)
{
	if (the_tree != NULL && options->so_generate_parents)
		tree_trav (&the_tree, sign_delete_parent_file, options);

	if (options->so_generate_one_parent && options->so_output_parent_file)
		unlink (options->so_output_parent_file);
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




