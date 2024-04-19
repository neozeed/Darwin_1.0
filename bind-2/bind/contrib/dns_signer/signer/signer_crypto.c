/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_crypto.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
    
/* Portions Copyright (c) 1995,1996,1997,1998 by Trusted Information Systems, In
c.  
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
    
#include "signer_crypto.h"
#include "signer_sign.h"
#include "signer_key_monitor.h"
#include "signer_tree.h"
#include "signer_debug.h"

void crypto_free_rr_recs (struct rr_rec **rr)
{
	if (rr==NULL) return;

	if (*rr)
	{
		if ((*rr)->rr_rdata) FREE ((*rr)->rr_rdata);
		if ((*rr)->rr_next) crypto_free_rr_recs(&((*rr)->rr_next));
		FREE (*rr);
		*rr = NULL;
	}
}

void crypto_free_rrset_recs (struct rrset_rec **set)
{
	if (set==NULL) return;

	if (*set)
	{
		if ((*set)->rrs_data) crypto_free_rr_recs (&((*set)->rrs_data));
		if ((*set)->rrs_sig) crypto_free_rr_recs (&((*set)->rrs_sig));
		if ((*set)->rrs_next) crypto_free_rrset_recs (&((*set)->rrs_next));
		FREE (*set);
		*set = NULL;
	}
}

size_t crypto_predict_length (	struct name_rec		*nr,
								struct rrset_rec	*rr_set)
{
	/*
		Calculate the size of the field over which the verification
		is done.  This is the sum of
			the number of bytes through the signer name in the SIG RDATA
			256 = the maximum length of the signer name
			the sum of the lengths of the RRs in the set

		Predict is a slight misnomer - "calculate" is a better term, but
		that implies a higher degree of accuracy.  The case in which
		this result is not accurate is for sets with duplicated records,
		which isn't caught until the sorting of the set.
	*/

	size_t			field_length;
	struct rr_rec	*rr;
	int				owner_length;

	owner_length = wire_name_length (nr->nr_name_n);

	field_length = SIG_RDATA_BY_NAME + MAXDNAME;

	for (rr = rr_set->rrs_data; rr; rr = rr->rr_next)
		field_length += owner_length+DNS_ENVELOPE_LENGTH+rr->rr_rdata_length_h;

	return field_length;
}

void crypto_lower_name (u_int8_t rdata[], int *index)
{

	/* Convert the upper case characters in a domain name to lower case */

	int length = wire_name_length(&rdata[(*index)]);

	while (length-- > 0)
	{
		rdata[(*index)] = tolower(rdata[(*index)]);
		(*index)++;
	}
}

void crypto_lower (u_int16_t type_h, u_int8_t *rdata, int len)
{
	/* Convert the case of any domain name to lower in the RDATA section */

	int	index = 0;

	switch (type_h)
	{
		/* These RR's have no domain name in them */

		case ns_t_nsap:
		case ns_t_eid:
		case ns_t_nimloc:
		case ns_t_key:
		case ns_t_aaaa:
		case ns_t_loc:
		case ns_t_atma:
		case ns_t_a:
		case ns_t_wks:
		case ns_t_hinfo:
		case ns_t_txt:
		case ns_t_x25:
		case ns_t_isdn:
		case ns_t_cert:
		default:
			return;

		/* These RR's have two domain names at the start */

		case ns_t_soa:	case ns_t_minfo:	case ns_t_rp:

			crypto_lower_name (rdata, &index);
			/* fall through */


		/* These have one name (and are joined by the code above) */

		case ns_t_ns:	case ns_t_cname:	case ns_t_mb:		case ns_t_mg:
		case ns_t_mr:	case ns_t_ptr:		case ns_t_nxt:

			crypto_lower_name (rdata, &index);

			return;

		/* These RR's end in one or two domain names */

		case ns_t_srv:

			index = 4; /* SRV has three preceeding 16 bit quantities */

		case ns_t_rt: case ns_t_mx: case ns_t_afsdb: case ns_t_px:

			index += 2; /* Pass the 16 bit quatity prior to the name */

			crypto_lower_name (rdata, &index);

			/* Get the second tail name (only in PX records) */
			if (type_h == ns_t_px) crypto_lower_name (rdata, &index);

			return;

		case ns_t_naptr: /* 16 int, 16 int, 3 strings, then a name */

			index = 4;					/* Skip the two 16 bit values */
			index += rdata[index] + 1;	/* Skip the first string */
			index += rdata[index] + 1;	/* Skip the second string */
			index += rdata[index] + 1;	/* Skip the third string */

			crypto_lower_name (rdata, &index);

			return;

		/* The last case is RR's with names in the middle. */
		/*
			Note: this code is never used as SIG's are the only record in
			this case.  SIG's are not signed, so they never are run through
			this code.  This is left here in case other RR's are defined in
			this unfortunate (for them) manner.
		*/
		case ns_t_sig:

			index = SIG_RDATA_BY_NAME;

			crypto_lower_name (rdata, &index);

			return;
	}
}

struct rr_rec *crypto_copy (u_int16_t type_h, struct rr_rec *r)
{
	/*
		Make a copy of an RR, lowering the case of any contained
		domain name in the RR section.
	*/
	struct rr_rec *the_copy;

	the_copy = (struct rr_rec *) MALLOC (sizeof(struct rr_rec));

	if (the_copy==NULL) return NULL;

	the_copy->rr_rdata_length_h = r->rr_rdata_length_h;
	the_copy->rr_rdata = (u_int8_t *) MALLOC (the_copy->rr_rdata_length_h);

	if (the_copy->rr_rdata==NULL) return NULL;

	memcpy (the_copy->rr_rdata, r->rr_rdata, r->rr_rdata_length_h);

	crypto_lower (type_h, the_copy->rr_rdata, the_copy->rr_rdata_length_h);

	the_copy->rr_next = NULL;
	return the_copy;
}

#define INSERTED	1
#define DUPLICATE	-1
int crypto_insert (struct rrset_rec *cs, struct rr_rec *cr)
{
	/*
		Insert a copied RR into the set being prepared for signing.  This
		is an implementation of an insertion sort.
	*/
	int				ret_val;
	int				length;
	struct rr_rec	*temp_rr;

	if (cs->rrs_data == NULL) /* Degenerate case - first/only one in set */
	{
		cs->rrs_data = cr;
		return INSERTED;
	}
	else
	{
		length =cs->rrs_data->rr_rdata_length_h<cr->rr_rdata_length_h?
				cs->rrs_data->rr_rdata_length_h:cr->rr_rdata_length_h;

		ret_val = memcmp (cs->rrs_data->rr_rdata, cr->rr_rdata, length);

		if (ret_val==0&&cs->rrs_data->rr_rdata_length_h==cr->rr_rdata_length_h)
		{
			/* cr is a copy of an existing record, forget it... */
			FREE (cr->rr_rdata);
			FREE (cr);
			return DUPLICATE;
		}
		else if (ret_val > 0 || (ret_val==0 && length==cr->rr_rdata_length_h))
		{
			/*
				cr belongs prior to all other records in the set, insert
				it at the head of the set
			*/
			cr->rr_next = cs->rrs_data;
			cs->rrs_data = cr;
			return INSERTED;
		}
		else /* got to go find a place for cr to sit */
		{
			temp_rr = cs->rrs_data;

			while (temp_rr->rr_next)
			{
				length = temp_rr->rr_next->rr_rdata_length_h <
												cr->rr_rdata_length_h ?
						 temp_rr->rr_next->rr_rdata_length_h :
												cr->rr_rdata_length_h;
				
				ret_val = memcmp (temp_rr->rr_next->rr_rdata, cr->rr_rdata,
									length);
				if (ret_val==0 &&
					temp_rr->rr_next->rr_rdata_length_h==cr->rr_rdata_length_h)
				{
					/* cr is a copy of an existing record, forget it... */
					FREE (cr->rr_rdata);
					FREE (cr);
					return DUPLICATE;
				}
				else if (ret_val>0||(ret_val==0&&length==cr->rr_rdata_length_h))
				{
					/* We've found a home for the record, before the next */
					cr->rr_next = temp_rr->rr_next;
					temp_rr->rr_next = cr;
					return INSERTED;
				}
				temp_rr = temp_rr->rr_next;
			}

			/* If we've gone this far, add the record to the end of the list */

			temp_rr->rr_next = cr;
			cr->rr_next = NULL;
			return INSERTED;
		}
	}
}	

struct rrset_rec *crypto_prepare_for_field (struct rrset_rec *rr_set,
											size_t	*length, int o_length)
{
	struct rrset_rec	*copy_set;
	struct rr_rec		*orig_rr;
	struct rr_rec		*copy_rr;
	int					rdata_len;

	copy_set = (struct rrset_rec *) MALLOC (sizeof(struct rrset_rec));

	if (copy_set == NULL) return NULL;

	memcpy (copy_set, rr_set, sizeof(struct rrset_rec));

	copy_set->rrs_data = NULL;
	copy_set->rrs_next = NULL;
	copy_set->rrs_sig = NULL;

	/*
		Do an insertion sort of the records in rr_set.  As records are
		copied, convert the domain names to lower case.
	*/

	for (orig_rr = rr_set->rrs_data; orig_rr; orig_rr = orig_rr->rr_next)
	{
		/* Copy it into the right form for verification */
		copy_rr = crypto_copy (rr_set->rrs_type_h, orig_rr);

		if (copy_rr==NULL) return NULL;

		rdata_len = copy_rr->rr_rdata_length_h;

		/* Now, find a place for it */

		if (crypto_insert (copy_set, copy_rr)==DUPLICATE)
			/* Reduce the field_length value from the prediction */
			*length -= (o_length + DNS_ENVELOPE_LENGTH + rdata_len);
	}
	return copy_set;
}

int crypto_make_field (	u_int8_t				**field,
						size_t					*field_length,
						struct name_rec			*nr,
						struct rrset_rec		*rr_set,
						struct signing_options	*options)
{
	struct rr_rec		*curr_rr;
	int					index;
	u_int16_t			type_n;
	int					owner_length;
	u_int16_t			rdata_length_n;
	struct rrset_rec	*copy_set;
	u_int8_t			lowered_owner_n[MAXDNAME];
	int					l_index;

	/*
		This routine will prepare the properly ordered and formatted
		data field containing the RR set to be signed or verified.
	*/

	*field_length = crypto_predict_length (nr, rr_set);

	if ((*field = (u_int8_t*) MALLOC (*field_length)) == NULL)
	{
		ERROR ("Out of memory - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	/*
		While we're at it, we'll gather other common info, specifically
		network ordered numbers (type, class) and name length.
	*/

	owner_length = wire_name_length (nr->nr_name_n);

	if (owner_length == 0)
	{
		ERROR ("Illegal owner name found");
		return SIGNER_ERROR;
	}

	memcpy (lowered_owner_n, nr->nr_name_n, owner_length);
	l_index = 0;
	crypto_lower_name (lowered_owner_n, &l_index);

	type_n = htons(rr_set->rrs_type_h);

	/* Leave room for the SIG RDATA */

	index = SIG_RDATA_BY_NAME + MAXDNAME;
	memset (*field, 0, index);

	/* Convert to lower case & sort the records */

	if ((copy_set = crypto_prepare_for_field(rr_set,field_length,owner_length))
			==NULL)
	{
		FREE (*field);
		*field = NULL;
		ERROR ("Out of memory - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	/* For each record of data, copy in the envelope & the lower cased rdata */

	for (curr_rr = copy_set->rrs_data; curr_rr; curr_rr = curr_rr->rr_next)
	{
		/* Copy in the envelope information */

		memcpy (&(*field)[index], lowered_owner_n, owner_length);
		index += owner_length;
		memcpy (&(*field)[index], &type_n, sizeof(u_int16_t));
		index += sizeof(u_int16_t);
		memcpy (&(*field)[index], &nr->nr_class_n, sizeof(u_int16_t));
		index += sizeof(u_int16_t);
		memcpy (&(*field)[index], &rr_set->rrs_ttl_n, sizeof(u_int32_t));
		index += sizeof(u_int32_t);

		/* Now the RR-specific info, the length and the data */

		rdata_length_n = htons (curr_rr->rr_rdata_length_h);
		memcpy (&(*field)[index], &rdata_length_n, sizeof(u_int16_t));
		index += sizeof(u_int16_t);
		memcpy (&(*field)[index],curr_rr->rr_rdata,curr_rr->rr_rdata_length_h);
		index += curr_rr->rr_rdata_length_h;
	}

	crypto_free_rrset_recs (&copy_set);

	return SIGNER_OK;
}

int crypto_find_signature (u_int8_t **field, struct rr_rec *rr_sig)
{
	/* Find the signature in a SIG RR (in the RDATA) */

	int		sig_index;

	sig_index = SIG_RDATA_BY_NAME +
					wire_name_length (&rr_sig->rr_rdata[SIG_RDATA_BY_NAME]);

	*field = &rr_sig->rr_rdata[sig_index];

	return rr_sig->rr_rdata_length_h - sig_index;
}

void crypto_identify_key (struct rr_rec *sig,
							u_int8_t **name_n,
							u_int8_t *algorithm,
							u_int16_t *footprint_n)
{
	/* Set pointers to the identity (name,alg, footprint) of the SIG's key */

	*name_n = &sig->rr_rdata[SIG_RDATA_BY_NAME];

	*algorithm = sig->rr_rdata[SIG_RDATA_ALGORITHM];

	memcpy (footprint_n, &sig->rr_rdata[SIG_RDATA_BY_NAME-sizeof(u_int16_t)],
				sizeof(u_int16_t));
}

int crypto_recycle_signature_given_sig_and_key (
								u_int8_t				*field,
								size_t					field_length,
								struct rr_rec			*the_sig,
								struct key_matter		*the_key,
								struct rrset_rec		*rrs,
								u_int8_t				*sig_maker_name_n,
								struct signing_options	*options)
{
	/*
		Try to recycle signature
			Place the SIG RDATA for the sig into field
			Don't update the TTL's
			Try to verify &field[256-signernamelen] with sig, key
	*/

	int				start_of_rdata;
	size_t			sig_maker_name_length;
	u_int8_t		*signature;
	int				signature_length;
	u_int32_t		temp_length;
	int				ret_val;

	crypto_find_signature (&signature, the_sig);
	temp_length = ((u_int32_t)signature - (u_int32_t)the_sig->rr_rdata);
	signature_length = (int)(the_sig->rr_rdata_length_h-
											(u_int16_t)temp_length);
	
	/* Calc start of rdata (MAXDNAME-length of the_key name */
	
	sig_maker_name_length = wire_name_length (sig_maker_name_n);
	start_of_rdata = MAXDNAME - sig_maker_name_length;
	
	/* Put the sig's rdata portions (18 bytes + name ) into field */
	
	memcpy (&field[start_of_rdata], the_sig->rr_rdata,
							SIG_RDATA_BY_NAME + sig_maker_name_length);
	
	/* Try verification */
	
	ret_val = dst_verify_data (SIG_MODE_ALL, the_key->km_key, NULL, 
							   &field[start_of_rdata], 
								field_length-start_of_rdata,
								signature, signature_length);
	
	/* If verification works return TRUE */

	if (ret_val == 0)
		options->so_stats.ss_verifications_successful++;
	else
		options->so_stats.ss_verifications_failed++;
	
	if (debug_level == L_DEVELOPER)
	{
		printf ("\nVerifying this field:\n");
		print_hex_field (&field[start_of_rdata],field_length-start_of_rdata,
							21,"VER: ");
		printf ("\nThis is the supposed signature:\n");
		print_hex_field (signature, signature_length, 21,"VER: ");
		printf ("Result of verification is %s\n", ret_val==0?"GOOD":"BAD");
	}

	return ret_val == 0;
	
}

int crypto_recycle_signature_given_sig (u_int8_t			*field,
										size_t				field_length,
										struct rr_rec		*the_sig,
										struct rrset_rec	*rrs,
										struct key_matter	**the_key,
										struct signing_options	*options)
{
	u_int8_t			*sig_maker_name_n;
	u_int8_t			sig_maker_algorithm;
	u_int16_t			sig_maker_footprint_n;

	/* Identify the signer, find the key & do the recycling */

	crypto_identify_key(the_sig, &sig_maker_name_n, &sig_maker_algorithm,
													&sig_maker_footprint_n);

	*the_key = key_find_public (sig_maker_name_n, sig_maker_algorithm,
											ntohs(sig_maker_footprint_n));

	if (*the_key == NULL)
	{
		wire_to_ascii_name (debug_name1_h, sig_maker_name_n, MAXPNAME);
		sprintf (debug_message, "Could not get key record for %s/%d",
								debug_name1_h, ntohs(sig_maker_footprint_n));
		ERROR (debug_message);
		return FALSE;
	}

	if ((*the_key)->km_key == NULL)
		(*the_key)->km_key = key_open_public ((*the_key)->km_name_n,
												(*the_key)->km_algorithm,
												(*the_key)->km_footprint_h);

	if ((*the_key)->km_key == NULL)
	{
		wire_to_ascii_name (debug_name1_h, sig_maker_name_n, MAXPNAME);
		sprintf (debug_message, "Could not get public key for %s/%d/%d",
								debug_name1_h, sig_maker_algorithm,
								ntohs(sig_maker_footprint_n));
		ERROR (debug_message);
		return FALSE;
	}

	return crypto_recycle_signature_given_sig_and_key ( field, field_length,
							the_sig, *the_key, rrs, sig_maker_name_n, options);
}

void crypto_remove_signer (struct rrset_rec *rrs, struct key_list *the_key)
{
	/*
		Removes the signer record from a set - either it's been used or
		a recyclable SIG has been found.

		This is just a run-of-the-mill linear list removal.
	*/

	struct key_list	*trail_key = NULL;
	struct key_list	*search_key = rrs->rrs_signers;

	while (search_key != NULL)
	{
		if (search_key->kl_key->km_key == the_key->kl_key->km_key)
		{
			if (trail_key == NULL)
			{
				rrs->rrs_signers = search_key->kl_next;
				FREE (search_key);
				search_key = rrs->rrs_signers;
			}
			else
			{
				trail_key->kl_next = search_key->kl_next;
				FREE (search_key);
				search_key = trail_key->kl_next;
			}
		}
		else
		{
			trail_key = search_key;
			search_key = search_key->kl_next;
		}
	}
}

void crypto_count_unchecked_sigs (	struct key_list			*the_key,
									struct rrset_rec		*rrs,
									struct signing_options	*options)
{
	u_int8_t		*sig_maker_name_n;
	u_int8_t		sig_maker_algorithm;
	u_int16_t		sig_maker_footprint_n;
	struct rr_rec	*rr_sig;

	for (rr_sig = rrs->rrs_sig; rr_sig != NULL; rr_sig=rr_sig->rr_next)
	{
		crypto_identify_key(rr_sig, &sig_maker_name_n, &sig_maker_algorithm,
														&sig_maker_footprint_n);

		if (sig_maker_footprint_n == htons(the_key->kl_key->km_footprint_h)
			&& sig_maker_algorithm ==  the_key->kl_key->km_algorithm
			&& namecmp (sig_maker_name_n, the_key->kl_key->km_name_n)==0)
		{
			options->so_stats.ss_signatures_unchecked++;
		}
	}
}

int crypto_recycle_signature_given_key(	u_int8_t			*field,
										size_t				field_length,
										struct key_list		*the_key,
										struct rrset_rec	*rrs,
										struct signing_options	*options)
{
	struct rr_rec	*rr_prior_sig;
	struct rr_rec	*rr_sig;
	struct rr_rec	*previous_prior_sig;
	struct rr_rec	*previous_sig;
	u_int8_t		*sig_maker_name_n;
	u_int8_t		sig_maker_algorithm;
	u_int16_t		sig_maker_footprint_n;
	int				found_one = FALSE;

	/* Find the sig to recycle.  If none, return FALSE */

	rr_prior_sig = NULL;
	rr_sig = rrs->rrs_sig;
	previous_prior_sig = NULL;
	previous_sig = NULL;

	if (the_key->kl_key->km_key == NULL)
	{
		/* Try opening up key for public value only */
		the_key->kl_key->km_key = key_open_public (the_key->kl_key->km_name_n,
											the_key->kl_key->km_algorithm,
											the_key->kl_key->km_footprint_h);
		the_key->kl_key->km_has_private = FALSE;
	}

	/* Gaack: There may be multiple w/same name&no. (an error, no doubt) */
	do
	{

		/* Find the a signature made by the requested key */

		while (rr_sig)
		{
			/* Determine which key was used for this signature */

			crypto_identify_key(rr_sig, &sig_maker_name_n,
											&sig_maker_algorithm,
											&sig_maker_footprint_n);
	
			/*
				If this signature's signer matches the requested signer
				then break from the while loop, pointing to the signature.

				A match is defined by the same owner name, algorithm, and
				footprint.  Sorry, I don't handle footprint conflicts in
				this version of the signer.
			*/

			if (sig_maker_footprint_n == htons(the_key->kl_key->km_footprint_h)
				&& sig_maker_algorithm == the_key->kl_key->km_algorithm
				&& namecmp (sig_maker_name_n, the_key->kl_key->km_name_n)==0)
			{
				/* A match is found */

				break;
			}
	
			rr_prior_sig = rr_sig;
			rr_sig = rr_sig->rr_next;

		}
	
		/*
			If there is no match, then rr_sig will be NULL, so there is
			no signature to verify for this requested key
		*/

		if (rr_sig==NULL) break;

		/*
			If we don't have the key material OR the signature verifies
				then we have to do some maintenance of it
			else
				then we dispose of the signature
		*/

		if (the_key->kl_key->km_key == NULL ||
			crypto_recycle_signature_given_sig_and_key (field, field_length,
								rr_sig, the_key->kl_key, rrs, sig_maker_name_n,
								options)
			)
		{

			/* The signature is still valid - but is it the best one? */

			if (previous_sig==NULL)
			{
				previous_prior_sig = rr_prior_sig;
				previous_sig = rr_sig;
				rr_prior_sig = rr_sig;
				rr_sig = rr_sig->rr_next;
				found_one = TRUE;
			}

			/* Take sig that will last longest into future */

			else if (memcmp(&previous_sig->rr_rdata[SIG_RDATA_TIME_SIGNED],
								&rr_sig->rr_rdata[SIG_RDATA_TIME_SIGNED],
													sizeof(u_int32_t)) < 0)
			{
				/* Treat rrs_sig as if it failed */
				if (rr_prior_sig==NULL)
				{
					rrs->rrs_sig = rr_sig->rr_next;
					rr_sig->rr_next = NULL;
					crypto_free_rr_recs (&rr_sig);
					rr_sig = rrs->rrs_sig;
				}
				else
				{
					rr_prior_sig->rr_next = rr_sig->rr_next;
					rr_sig->rr_next = NULL;
					crypto_free_rr_recs (&rr_sig);
					rr_sig = rr_prior_sig->rr_next;
				}
			}
			else /* The newest one is not as good as one previously found */
			{
				/* Treat previous as a failure and save this */
				if (previous_prior_sig==NULL)
					rrs->rrs_sig = previous_sig->rr_next;
				else
					previous_prior_sig->rr_next = previous_sig->rr_next;
	
				previous_sig->rr_next = NULL;
				crypto_free_rr_recs (&previous_sig);

				previous_prior_sig = rr_prior_sig;
				previous_sig = rr_sig;
				rr_prior_sig = rr_sig;
				rr_sig = rr_sig->rr_next;
				found_one = TRUE;
			}
		}
		else /* Remove the current signature (it failed) */
		{
			/* If rr_prior_sig is null, delete from head of sig field */
			/* Else delete from prior's next record */
	
			if (rr_prior_sig==NULL)
			{
				rrs->rrs_sig = rr_sig->rr_next;
				rr_sig->rr_next = NULL;
				crypto_free_rr_recs (&rr_sig);
				rr_sig = rrs->rrs_sig;
			}
			else
			{
				rr_prior_sig->rr_next = rr_sig->rr_next;
				rr_sig->rr_next = NULL;
				crypto_free_rr_recs (&rr_sig);
				rr_sig = rr_prior_sig->rr_next;
			}
		}
	} while (rr_sig != NULL);

	/*
		found_one signifies the finding of at least one valid recycled
		signature, obviating the need for resigning this set under this
		key.
	*/

	if (found_one) crypto_remove_signer (rrs, the_key);
	return found_one;
}

void crypto_add_SIG_rdata (	u_int8_t				*field,
							int						index,
							struct name_rec			*nr,
							struct rrset_rec		*rrs,
							struct key_list			*the_key,
							struct signing_options	*options)
{
	u_int16_t	u_int16_t_n;

	/* u_int16_t -> type covered */

	u_int16_t_n = htons (rrs->rrs_type_h);
	memcpy (&field[index], &u_int16_t_n, sizeof (u_int16_t));
	index += sizeof (u_int16_t);

	/* u_int8_t -> algorithm */

	field[index] = the_key->kl_key->km_algorithm;
	index += sizeof (u_int8_t);

	/* u_int8_t -> labels */

	field[index] = wire_name_labels (nr->nr_name_n) - 1;
	index += sizeof (u_int8_t);

	/* u_int32_t -> ttl */

	memcpy (&field[index], &rrs->rrs_ttl_n, sizeof (u_int32_t));
	index += sizeof (u_int32_t);

	/* u_int32_t -> expires */

	memcpy (&field[index], &options->so_time_expired_n, sizeof (u_int32_t));
	index += sizeof (u_int32_t);

	/* u_int32_t -> time signed */

	memcpy (&field[index], &options->so_time_signed_n, sizeof (u_int32_t));
	index += sizeof (u_int32_t);

	/* u_int16_t -> footprint */

	u_int16_t_n = htons (the_key->kl_key->km_footprint_h);
	memcpy (&field[index], &u_int16_t_n, sizeof (u_int16_t));
	index += sizeof (u_int16_t);

	/* u_int8_t* -> name */

	memcpy (&field[index], the_key->kl_key->km_name_n,
							wire_name_length (the_key->kl_key->km_name_n));
}

int crypto_make_signature ( u_int8_t				*field,
							size_t					field_length,
							struct name_rec			*nr,
							struct rrset_rec		*rrs,
							struct key_list			*the_key,
							struct signing_options	*options)
{
	/*
		Make a new signature
			Prepare the SIG RDATA for set
			Prepend this to the field
			Generate the SIG of &field[256-signernamelen] with key
			Add the signature to the set
	*/

	int				start_of_rdata;
	size_t			signer_length=wire_name_length (the_key->kl_key->km_name_n);
	u_int8_t		*sig_rr_rdata;
	size_t			sig_rr_rdata_length;
	int				key_bits;
	int				key_bytes;
	int				sig_bytes;

	/* Prepare the new rdata */

	start_of_rdata = MAXDNAME - signer_length;
	crypto_add_SIG_rdata (field, start_of_rdata, nr, rrs, the_key, options);

	/* Prepare for the result */

	if ((key_bits = the_key->kl_key->km_key->dk_key_size)==0)
	{
		/* NULL key (maybe?) - nothing to sign */
		return SIGNER_OK;
	}

	key_bytes = (key_bits+7)/8;
	sig_rr_rdata_length = SIG_RDATA_BY_NAME + signer_length + key_bytes;
	if ((sig_rr_rdata = (u_int8_t *)MALLOC (sig_rr_rdata_length))==NULL)
	{
		ERROR ("Out of memory - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	crypto_add_SIG_rdata (sig_rr_rdata, 0, nr, rrs, the_key, options);

	/* Generate the signature */

	sig_bytes = dst_sign_data (	SIG_MODE_ALL,
								the_key->kl_key->km_key, NULL,
								&field[start_of_rdata],
								field_length-start_of_rdata,
								&sig_rr_rdata[SIG_RDATA_BY_NAME+signer_length],
								key_bytes);

	if (debug_level == L_DEVELOPER)
	{
		printf ("\nSigning this field:\n");
		print_hex_field (&field[start_of_rdata],field_length-start_of_rdata,
							21,"SIG: ");
		printf ("\nThis is the signature:\n");
		print_hex_field (&sig_rr_rdata[SIG_RDATA_BY_NAME + signer_length],
							key_bytes, 21, "SIG: ");
	}

	if (sig_bytes <= 0)
	{
		/* Signing wasn't completed or done at all */

		options->so_return_code = SIGNER_ERROR;
		wire_to_ascii_name (debug_name1_h,nr->nr_name_n,MAXPNAME);
		wire_to_ascii_name (debug_name2_h,the_key->kl_key->km_name_n,MAXPNAME);
		sprintf (debug_message, "Could not sign %s/%s with %s/%d",
				debug_name1_h,
				local_sym_ntop(__p_type_syms, rrs->rrs_type_h, &debug_success),
				debug_name2_h,
				the_key->kl_key->km_footprint_h);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	/* Add the newly generated signature to the list */

	if (tree_add_as_sig (rrs, sig_rr_rdata_length+(sig_bytes-key_bytes),
														sig_rr_rdata, options))
	{
		options->so_return_code = SIGNER_ERROR;
		return SIGNER_ERROR;
	}

	FREE (sig_rr_rdata);

	/* Remove the signer(s) from the list of signers */

	/*
		This removes both an ANY and a signular footprint in the case there's
		a match.
	*/

	crypto_remove_signer (rrs, the_key);

	return SIGNER_OK;
}

void crypto_key_warning(struct key_list *the_key, struct name_rec *nr,
						struct rrset_rec *rrs, struct signing_options *options)
{
	/*
		If the use of the key deserves a warning, issue it
				- Non-apex & non-zone-key signing a signing key set
	*/
	struct rr_rec	*a_key;

	if (rrs->rrs_type_h == ns_t_key)
	{
		for (a_key = rrs->rrs_data; a_key; a_key = a_key->rr_next)
		{
			if (key_is_a_signing_key (a_key->rr_rdata))
			{
				/* If the_key isn't the apex zone key - trouble */

				if (!(the_key->kl_key->km_flags_h & VALUE_FLAGS_ZONE_KEY) &&
					namecmp (the_key->kl_key->km_name_n,
									options->so_zone_name_n)!=0)
				{
					/* This key signing isn't DNSSEC approved */

					wire_to_ascii_name (debug_name1_h, nr->nr_name_n, MAXPNAME);
					wire_to_ascii_name (debug_name2_h,
										the_key->kl_key->km_name_n, MAXPNAME);
					sprintf (debug_message,"Signing %s/KEY w/ non-zone %s/%d\n",
									debug_name1_h, debug_name2_h,
									the_key->kl_key->km_footprint_h);
					WARN (debug_message);
					return;
				}
			}
		}
	}
}

int crypto_make_signatures (	struct name_rec			*nr,
								struct rrset_rec		*rrs,
								struct signing_options	*options)
{
	u_int8_t			*field;
	size_t				field_length;
	struct key_list		*the_key;
	struct key_list		*the_next_key;
	struct rr_rec		*the_sig;
	struct rr_rec		*the_last;
	struct rr_rec		*the_prior;
	struct key_matter	*key_used;

	/*
		Make verification field:
			Calculate the maximum size (assume signer is 256 bytes long
			Place the non-SIG RDATA at the **end** of the field
	*/

	if (crypto_make_field (&field, &field_length, nr, rrs, options)
														== SIGNER_ERROR)
	{
		options->so_return_code = SIGNER_ERROR;
		return SIGNER_ERROR;
	}

	/*
		For each signer
			If the key is not available continue
			If the use of the key deserves a warning, issue it
			If there is a signature
				Try to recycle signature
				If the sig is recycled, continue
			Make a new signature
			Remove the key from the key_list
	*/

	the_key = rrs->rrs_signers;
	while (the_key)
	{
		the_next_key = the_key->kl_next;

		if (rrs->rrs_type_h != ns_t_soa &&
			crypto_recycle_signature_given_key (field,field_length,the_key,rrs,
												options))
		{
			the_key = the_next_key;
			continue;
		}

		if (the_key->kl_key->km_has_private==FALSE)
		{
			/*
				For accounting purposes, count SIGs slipped through
				unchecked here
			*/

			if (rrs->rrs_type_h == ns_t_soa)
				crypto_count_unchecked_sigs (the_key, rrs, options);

			the_key = the_next_key;
			continue;
		}

		if (rrs->rrs_type_h == ns_t_soa &&
			crypto_recycle_signature_given_key (field,field_length,the_key,rrs,
												options))
		{
			the_key = the_next_key;
			continue;
		}

		crypto_key_warning(the_key, nr, rrs, options);

		if (crypto_make_signature (field,field_length,nr,rrs,the_key,options)
												==SIGNER_ERROR)
		{
			FREE (field);
			return SIGNER_ERROR;
		}

		options->so_stats.ss_signings_done++;

		SIGN_PROGRESS(options->so_stats.ss_signings_done, nr->nr_name_n,
						rrs->rrs_type_h);

		the_key = the_next_key;
	}

	/* For each recycler, verify sig, if fails, don't regenerate it */

	the_prior = NULL;
	the_sig = rrs->rrs_recyclers;
	while (the_sig)
	{
		if (crypto_recycle_signature_given_sig (field,field_length,the_sig,rrs,
							&key_used,options))
		{
			the_prior = the_sig;
			the_sig = the_sig->rr_next;
		}
		else
		{
			/* The signature failed to verify.  */

			if (the_prior == NULL)
			{
				rrs->rrs_recyclers = rrs->rrs_recyclers->rr_next;
				the_sig->rr_next = NULL;
				free_rr_recs (&the_sig);	
				the_sig = rrs->rrs_recyclers;
			}
			else
			{
				the_prior->rr_next = the_prior->rr_next->rr_next;
				the_sig->rr_next = NULL;
				free_rr_recs (&the_sig);	
				the_sig = the_prior->rr_next;
			}
		}
	}

	the_last = rrs->rrs_sig;
	if (the_last == NULL)
	{
		rrs->rrs_sig = rrs->rrs_recyclers;
	}
	else
	{
		while (the_last->rr_next) the_last = the_last->rr_next;
		the_last->rr_next = rrs->rrs_recyclers;
	}
	rrs->rrs_recyclers = NULL;
	
	/*
		Clean up (remove verification field memory)
	*/

	FREE (field);

	return SIGNER_OK;
}
