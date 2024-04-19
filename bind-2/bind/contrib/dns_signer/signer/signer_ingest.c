/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_ingest.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
    
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
    
#include "signer_ingest.h"
#include "signer_sign.h"
#include "signer_tree.h"
#include "signer_debug.h"
#include "signer_key_monitor.h"
#include "signer_directives.h"

char local_time_string[STRING_64];
char *local_time()
{
	time_t seconds = time(0);
	strftime(local_time_string, STRING_64, "%a, %D %r", localtime(&seconds));
	return local_time_string; 
}

static struct open_file	*files = NULL;

int already_opened (char* filename)
{
	struct open_file	*searcher;

	for (searcher = files; searcher != NULL; searcher = searcher->of_next)
	{
		if (strcmp (filename, searcher->of_filename)==0)
			return TRUE;
	}

	searcher = (struct open_file*) MALLOC (sizeof (struct open_file));

	searcher->of_filename = filename;
	searcher->of_next = files;
	files = searcher;

	return FALSE;
}

void close_file ()
{
	struct open_file	*file_to_free = files;

	if (files==NULL) return;
	files = files->of_next;
	FREE (file_to_free);
}

void mark_soa_signing_keys (struct key_list *the_keys,
										struct signing_options *opt)
{
	/*
		Mark only keys that are autohorized to sign - can't tell if the
		flags are set though - may not have the key available or it may
		be an "any" designation.
	*/
	struct key_list *candidate;

	for (candidate = the_keys; candidate; candidate = candidate->kl_next)
		if (candidate->kl_key != NULL)
			if (opt->so_zone_name_n != NULL && 
				namecmp (candidate->kl_key->km_name_n,opt->so_zone_name_n)==0)
			{
				candidate->kl_key->km_signed_soa = TRUE;
			}
}

u_int32_t grab_minimum_ttl_n (u_int8_t *the_rdata_n)
{
	int			index = 0;
	u_int32_t	long_n;

	index += wire_name_length (&the_rdata_n[index]);
	index += wire_name_length (&the_rdata_n[index]);
	index += sizeof(u_int32_t) * 4;

	memcpy (&long_n, &the_rdata_n[index], sizeof(u_int32_t));

	return long_n;
}

int illegal_set_for_name (struct name_rec *name_rec, u_int16_t the_set_type_h,
							u_int16_t the_type_h, u_int8_t *zone_name_n)
{
	int	has_cname = FALSE;
	int has_soa = FALSE;
	int has_ns = FALSE;
	int has_key = FALSE;
	int has_other = FALSE;

	struct rrset_rec	*a_set;

	if (name_rec == NULL) return TRUE;

	if (zone_name_n != NULL && the_type_h == ns_t_soa) return TRUE;

	for (a_set = name_rec->nr_rrsets; a_set != NULL; a_set = a_set->rrs_next)
	{
		if (a_set->rrs_type_h == ns_t_cname) has_cname = TRUE;
		else if (a_set->rrs_type_h == ns_t_soa) has_soa = TRUE;
		else if (a_set->rrs_type_h == ns_t_ns) has_ns = TRUE;
		else if (a_set->rrs_type_h == ns_t_key) has_key = TRUE;
		else if (the_set_type_h != ns_t_nxt) has_other = TRUE;
	}

	if (the_set_type_h == ns_t_cname) has_cname = TRUE;
	else if (the_set_type_h == ns_t_soa) has_soa = TRUE;
	else if (the_set_type_h == ns_t_ns) has_ns = TRUE;
	else if (the_set_type_h == ns_t_key) has_key = TRUE;
	else if (the_set_type_h != ns_t_nxt) has_other = TRUE;

	if (has_cname && (has_other || has_ns || has_soa)) return TRUE;

	return FALSE;
}

void extract_from_rr (	u_int8_t	*rr,
						u_int8_t	**the_name_n,
						u_int16_t	*the_type_h,
						u_int16_t	*the_set_type_h,
						u_int16_t	*the_class_n,
						u_int32_t	*the_ttl_n,
						u_int16_t	*the_rdata_length_h,
						u_int8_t	**the_rdata_n,
						u_int32_t	*sig_expiry_n)
{
	u_int16_t	short_n;
	int			index = 0;

	if (rr==NULL) return;

	(*the_name_n) = &rr[index];
	index += wire_name_length ((*the_name_n));

	memcpy (&short_n, &rr[index], sizeof(u_int16_t));
	(*the_type_h) = ntohs(short_n);
	index +=sizeof(u_int16_t);

	memcpy (the_class_n, &rr[index], sizeof(u_int16_t));
	index +=sizeof(u_int16_t);

	memcpy (the_ttl_n, &rr[index], sizeof(u_int32_t));
	index +=sizeof(u_int32_t);

	memcpy (&short_n, &rr[index], sizeof(u_int16_t));
	*the_rdata_length_h = ntohs(short_n);
	index +=sizeof(u_int16_t);

	(*the_rdata_n) = &rr[index];

	if ((*the_type_h) != ns_t_sig)
	{
		*the_set_type_h = *the_type_h;
		*sig_expiry_n = 0;
	}
	else
	{
		memcpy (&short_n, &rr[index], sizeof(u_int16_t));
		*the_set_type_h = ntohs(short_n);

		memcpy (sig_expiry_n, &rr[index+8], sizeof(u_int32_t));
	}
}

int expired (u_int32_t expiry_n, u_int32_t sig_expiry_n)
{
	time_t		now_h = time(0);
	u_int32_t	then_h = ntohl(expiry_n);
	u_int32_t	sig_expires_h = ntohl(sig_expiry_n);
	u_int32_t	inv_now_h = (0x80000000) + ((u_int32_t) now_h);

	if (then_h <= inv_now_h)
	{
		if (then_h <= sig_expires_h && sig_expires_h <= inv_now_h)
			return FALSE;
	}
	else
	{
		if (then_h <= sig_expires_h || sig_expires_h <= inv_now_h)
			return FALSE;
	}

	return TRUE;
}

int found_ns (u_int8_t *zone_name_n, u_int8_t *the_name_n)
{
	int					index;
	int					z_len = wire_name_length (zone_name_n);
	int					t_len = wire_name_length (the_name_n);
	struct name_rec		*name_rec;

	if (z_len > t_len) return FALSE;

	for (index = 0; index <= (t_len - z_len); index += the_name_n[index] + 1)
	{
		name_rec = tree_find_name (&the_name_n[index]);

		if (name_rec == NULL) continue;

		if (name_rec->nr_status == NR_STATUS_SUBZONE) return TRUE;

		if (name_rec->nr_status == NR_STATUS_DELEGATION) return TRUE;
	}

	return FALSE;
}

int in_domain (u_int8_t *zone_name_n, u_int8_t *the_name_n)
{
	int	index;
	int	z_len = wire_name_length (zone_name_n);
	int t_len = wire_name_length (the_name_n);

	if (z_len > t_len) return FALSE;

	for (index = 0; index <= (t_len - z_len); index += the_name_n[index] + 1)
		if (namecmp (&the_name_n[index], zone_name_n) == 0)
			return TRUE;

	return FALSE;
}

void clean_up(struct key_list **the_keys, FILE *the_file, char *filename)
{
	/* Erase key list copy */

	key_list_delete (the_keys);

	/* Close file */

	fclose (the_file);

	close_file ();
}

int probably_will_sign (struct signing_options	*options,
						u_int8_t				status,
						u_int16_t				type_h,
						u_int8_t				*the_rdata_n)
{
	switch (status)
	{
		case NR_STATUS_PARENT:
			if (options->so_up_policy)
				return type_h==ns_t_key;
			else
				return FALSE;

		case NR_STATUS_PROTO_APEX:
		case NR_STATUS_APEX:
			if (type_h == ns_t_nxt)
			{
				if (ISSET((&the_rdata_n[wire_name_length(the_rdata_n)]),
																ns_t_soa))
					return TRUE;
				else
					return FALSE;
			}

			if (type_h == ns_t_key) return FALSE;

			return TRUE;

		case NR_STATUS_IN:
			return TRUE;

		case NR_STATUS_PROTO_DELEG:
		case NR_STATUS_DELEGATION:
			if (type_h==ns_t_key) return TRUE;
			if (type_h==ns_t_nxt)
			{
				if (!ISSET((&the_rdata_n[wire_name_length(the_rdata_n)]),
																ns_t_soa))
					return FALSE;
				else
					return TRUE;
			}
			return FALSE;
		default:
			return FALSE;
	}
}

int will_PARENT_overwrite_this (struct name_rec			*nr,
								u_int16_t				the_set_type_h,
								u_int16_t				the_type_h,
								u_int8_t				*the_rdata_n,
								struct signing_options *options)
{
	/*
		If there is data due from a .PARENT and we aren't reading it now
		and this is one of the following sets, forget about it:
			Any STATUS_PARENT
			STATUS_APEX/PROTO_APEX upper NXT and KEY
		This goes for their SIG(x)'s too.
	*/

	if (nr->nr_status == NR_STATUS_PARENT)
		if (options->so_phase_of_moon == SO_PHASE_READING_PARENT)
			if (the_type_h == ns_t_sig && the_set_type_h == ns_t_key)
				return FALSE;
			else
				return TRUE;
		else
			return FALSE;

	if (nr->nr_status ==NR_STATUS_PROTO_APEX || nr->nr_status == NR_STATUS_APEX)
	{
		/* Are we reading the parent now? */
		if (options->so_phase_of_moon == SO_PHASE_READING_PARENT)
			return FALSE;

		if (the_set_type_h == ns_t_key) return TRUE;

		if (the_set_type_h == ns_t_nxt)
		{
			if (the_type_h == ns_t_nxt)
			{
				int index = wire_name_length (the_rdata_n);
				if (!ISSET ((&the_rdata_n[index]), ns_t_soa))
					/* Upper NXT */
					return TRUE;
				else
					/* Lower NXT */
					return FALSE;
			}
			else
			{
				if (namecmp(nr->nr_name_n, &the_rdata_n[SIG_RDATA_BY_NAME])!=0)
					/* Upper SIG(NXT) */
					return TRUE;
				else
					/* Lower SIG(NXT) */
					return FALSE;
			}
		}
	}

	return FALSE;
}

int ingest_rr_record (u_int8_t *rr, struct signing_options *options,
						struct key_list *the_keys)
{
	u_int8_t	*the_name_n;
	u_int16_t	the_type_h;
	u_int16_t	the_set_type_h;
	u_int16_t	the_class_n;
	u_int32_t	the_ttl_n;
	u_int16_t	the_rdata_length_h;
	u_int8_t	*the_rdata_n;
	u_int32_t	sig_expiry_n;

	struct name_rec		*name_rec;
	struct rrset_rec	*set_rec;
	struct key_matter	*km;

	options->so_stats.ss_records_read++;

	/* Have a legal RR to ingest */
	extract_from_rr (rr,&the_name_n,&the_type_h,&the_set_type_h,&the_class_n,
					&the_ttl_n,&the_rdata_length_h,&the_rdata_n,&sig_expiry_n);

	/* Forget all expired SIGs */
	if (the_type_h==ns_t_sig &&expired(options->so_time_purged_n,sig_expiry_n))
	{
		options->so_stats.ss_signatures_read++;
		options->so_stats.ss_signatures_expired++;
		return SIGNER_OK;
	}

	/* Should I admit this NXT? */

	if (the_type_h == ns_t_nxt &&
			!((options->so_zone_name_n &&
				namecmp (options->so_zone_name_n, the_name_n)==0 &&
				!ISSET ((&the_rdata_n[wire_name_length(the_rdata_n)]),ns_t_soa))
					||
			(options->so_parent_name_n &&
				namecmp (options->so_parent_name_n, the_name_n)==0)))
	{
		options->so_stats.ss_nxts_dropped++;
		return SIGNER_OK;
	}

	/* If the name isn't in the tree yet, add it */

	if ((name_rec = tree_add_name (the_name_n, the_class_n, options)) == NULL)
		return SIGNER_ERROR;

	/* Check the value of the CLASS */

	if (options->so_zone_name_n != NULL && the_class_n != options->so_class_n)
	{
		wire_to_ascii_name (debug_name1_h, the_name_n, MAXPNAME);
		sprintf (debug_message,
			"Class %s for %s/%s is not the same as SOA's (%s)",
			local_sym_ntop(__p_class_syms, ntohs(the_class_n), &debug_success),
			debug_name1_h,
			local_sym_ntop(__p_type_syms, the_type_h, &debug_success),
			local_sym_ntop(__p_class_syms, ntohs(options->so_class_n),
															&debug_success));
		ERROR (debug_message);
		name_rec->nr_status = NR_STATUS_ERROR;
		return SIGNER_ERROR;
	}

	if (options->so_zone_name_n && name_rec->nr_status == NR_STATUS_UNSET)
	{
		if (options->so_parent_name_n && 
					namecmp (options->so_parent_name_n, the_name_n)==0)
			name_rec->nr_status = NR_STATUS_PARENT;
		else if (in_domain (options->so_zone_name_n, the_name_n) == FALSE)
			name_rec->nr_status = NR_STATUS_GLUE;
		else if (found_ns (options->so_zone_name_n, the_name_n))
			name_rec->nr_status = NR_STATUS_SUBZONE;
		else
			name_rec->nr_status = NR_STATUS_IN;

		options->so_stats.ss_names_read++;

		READ_PROGRESS(options->so_stats.ss_names_read, the_name_n);
	}

	/*
		If there is data due from a .PARENT and we aren't reading it now
		and this is one of the following sets, forget about it:
			Any STATUS_PARENT
			STATUS_APEX/PROTO_APEX upper NXT and KEY
		This goes for their SIG(x)'s too.
	*/

	if (options->so_input_parent_file != NULL &&
			will_PARENT_overwrite_this (name_rec, the_set_type_h, the_type_h,
										the_rdata_n, options))
	{
		if (the_type_h==ns_t_sig)
		{
			options->so_stats.ss_signatures_read++;
			options->so_stats.ss_signatures_parent++;
		}
		else
		{
			options->so_stats.ss_sets_dropped_for_parent++;
		}

		return SIGNER_OK;
	}

	if (name_rec->nr_status == NR_STATUS_ERROR)
		return SIGNER_ERROR;

	/* Now, work on the RR */

	if (illegal_set_for_name (name_rec, the_set_type_h,
									the_type_h, options->so_zone_name_n)==TRUE)
	{
		wire_to_ascii_name (debug_name1_h, the_name_n, MAXPNAME);
		sprintf (debug_message, "Set of type %s is illegal for name %s",
				local_sym_ntop(__p_type_syms, the_set_type_h, &debug_success),
				debug_name1_h);
		name_rec->nr_status = NR_STATUS_ERROR;
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	if ((set_rec = tree_add_set (name_rec, the_set_type_h, the_ttl_n,
										the_rdata_n, options)) != NULL)
	{
		if (set_rec->rrs_sig == NULL && set_rec->rrs_data == NULL)
			options->so_stats.ss_sets_read++;

		if (set_rec->rrs_data != NULL)
		{
			/* This is not a new addition */

			/*
				If this is glue data from the parent file, erase the
				glue data for the same set in case of a conflict.
			*/
			if (name_rec->nr_status == NR_STATUS_GLUE &&
				options->so_phase_of_moon == SO_PHASE_READING_PARENT &&
				set_rec->rrs_parental_glue == FALSE)
			{
				/* Glue data should be unsigned, but, just to make sure... */

				struct rr_rec	*rr;

				/* Count the records deleted in advance of the parent file */

				/* First data */
				for (rr = set_rec->rrs_data; rr; rr=rr->rr_next)
					options->so_stats.ss_sets_dropped_for_parent++;

				/* Next sig(data) */
				for (rr = set_rec->rrs_sig; rr; rr=rr->rr_next)
					options->so_stats.ss_sets_dropped_for_parent++;

				/* Delete the records */

				free_rr_recs (&set_rec->rrs_data);
				free_rr_recs (&set_rec->rrs_sig);

				/* Set the rrs_parental_glue flag to true */
				set_rec->rrs_parental_glue = TRUE;
			}

			/* Only one RR may be present in the sets for types
				SOA (RFC1035, sect. 5.2)
				CNAME (RFC2181, sect. 10.1)
				EID
			*/

			if (the_type_h == ns_t_cname || the_type_h == ns_t_eid ||
					the_type_h == ns_t_soa)
			{
				wire_to_ascii_name (debug_name1_h, the_name_n, MAXPNAME);
				sprintf (debug_message, "There can be only one RR for %s/%s",
					debug_name1_h,
					local_sym_ntop(__p_type_syms, the_set_type_h,
														&debug_success));
				ERROR (debug_message);
				name_rec->nr_status = NR_STATUS_ERROR;
				return SIGNER_ERROR;
			}

			if (the_ttl_n != set_rec->rrs_ttl_n)
			{
				wire_to_ascii_name (debug_name1_h, the_name_n, MAXPNAME);
				sprintf (debug_message,
						"Inconsistent TTLs for %s/%s : %d S differs from %d S",
						debug_name1_h,
						local_sym_ntop(__p_type_syms, the_set_type_h,
														&debug_success),
						(u_int32_t) ntohl(the_ttl_n),
						(u_int32_t) ntohl(set_rec->rrs_ttl_n));
				ERROR (debug_message);
				name_rec->nr_status = NR_STATUS_ERROR;
				return SIGNER_ERROR;
			}
		}
		else
		{
			/* This is a new addition */

			if (the_type_h == ns_t_soa)
			{
				if (options->so_origin_set==FALSE&&namecmp(the_name_n,(u_int8_t*)"\0")==0)
				{
					ERROR ("Won't assume $ORIGIN root and assign it to an SOA");
					return SIGNER_ERROR;
				}
				
				/* Set the zone_name_n */

				options->so_zone_name_n = (u_int8_t*)
								MALLOC(wire_name_length(the_name_n));

				if (options->so_zone_name_n == NULL)
				{
					ERROR ("Out of memory - exiting");
					tree_delete_parent_files (options);
					sign_cleanup (options);
					exit (1);
				}
				memcpy (options->so_zone_name_n, the_name_n,
										wire_name_length(the_name_n));

				/* Set the ttl and class */

				options->so_class_n = the_class_n;

				options->so_soa_min_ttl_n = grab_minimum_ttl_n (the_rdata_n);
				if (options->so_ttl_set == FALSE)
				{
					WARN ("No default TTL set, using SOA minimum instead");
					options->so_ttl_n = options->so_soa_min_ttl_n;
				}

				/* Mark the SOA-signing key(s) */

				mark_soa_signing_keys (the_keys, options);

				/* And make a list of them in a convenient place */

				if (key_find_soa_signers (options)==SIGNER_ERROR)
					return SIGNER_ERROR;

				/* Mark name as a proto-apex */

				name_rec->nr_status = NR_STATUS_PROTO_APEX;
			}
			else if (the_type_h == ns_t_ns)
			{
				if (name_rec->nr_status == NR_STATUS_IN ||
					name_rec->nr_status == NR_STATUS_PROTO_DELEG)
				{
					/* Mark name as delegation point */

					name_rec->nr_status = NR_STATUS_DELEGATION;

					/* Mark names below this point as subzoned */

					tree_move_to_subzone (name_rec->nr_name_n);
				}
				else if (name_rec->nr_status == NR_STATUS_PROTO_APEX)
				{
					/* Mark name as apex */
					name_rec->nr_status = NR_STATUS_APEX;

					options->so_stats.ss_names_read++;

					READ_PROGRESS(options->so_stats.ss_names_read, the_name_n);
				}
			}

			/* The set was introduced by the PARENT file */

			if (options->so_phase_of_moon == SO_PHASE_READING_PARENT)
				set_rec->rrs_parental_glue = TRUE;
		}
	}
	else
		return SIGNER_ERROR;

	if (the_type_h == ns_t_key)
	{
		/*
			See if this is a key which isn't a duplicate but has the
			same footprint as the others
		*/

		if (key_footprint_conflict (set_rec, the_rdata_length_h, the_rdata_n))
		{
			wire_to_ascii_name (debug_name1_h, the_name_n, MAXPNAME);
			sprintf (debug_message, "Conflicting KEY footprints for %s",
				debug_name1_h);
			ERROR (debug_message);
			name_rec->nr_status = NR_STATUS_ERROR;
			return SIGNER_ERROR;
		}

		/* If this is a signing key, make sure the zone keys sign it */

		if (key_is_a_signing_key (the_rdata_n))
		{
			if (name_rec->nr_status == NR_STATUS_IN)
			{
				/* If the new key is a zone key */
				if (key_is_a_zone_key (the_rdata_n))
				{
					/* Mark the name as proto-delegation */
					name_rec->nr_status = NR_STATUS_PROTO_DELEG;
				}
			}

		} 
/* Below was moved out of the if clause */
		/* Add this to the key monitor's pile of keys */

		if ((km = key_add (the_name_n, 0, FALSE,
						0, FALSE, FALSE, the_rdata_length_h,
						the_rdata_n, options)) == NULL)
			return SIGNER_ERROR;
/*		} */
	}

	if (the_type_h != ns_t_sig)
	{
		if (tree_add_as_data (set_rec, the_rdata_length_h, the_rdata_n, options)
																== SIGNER_ERROR)
			return SIGNER_ERROR;

		/* Adjust the signers of the data */

		if (probably_will_sign (options, name_rec->nr_status, the_set_type_h,
									the_rdata_n))
		{
			if (key_list_merge (&set_rec->rrs_signers, the_keys, options) 
															== SIGNER_ERROR)
				return SIGNER_ERROR;

			/* Add zone keys for signing KEYs & and (null-)zone KEYs */

			if (the_type_h == ns_t_key)
			{
				/* No longer filter out self-signing situations */

				if (key_is_a_signing_key (the_rdata_n) ||
						key_is_a_zone_key (the_rdata_n))
				{
					if (key_list_merge (&set_rec->rrs_signers,
								options->so_soa_keys, options) == SIGNER_ERROR)
						return SIGNER_ERROR;
				}

				if (options->so_enforce_self_signing
								&& !key_is_a_NULL_key (the_rdata_n))
				{
					/* Make sure that a kl pointing to km is in list */

					if (key_add_to_list (the_name_n, km->km_any_algorithm,
							km->km_algorithm, km->km_any_key,
							km->km_footprint_h, &set_rec->rrs_signers,
													options)==SIGNER_ERROR)
					{
						return SIGNER_ERROR;
					}
				}
			} /* End of speccial treatment of key set signatures */
		}
		else /* If this is the apex's key set, need special treatment */
		{
			if (the_type_h == ns_t_key &&
				(name_rec->nr_status == NR_STATUS_APEX ||
				name_rec->nr_status == NR_STATUS_PROTO_APEX)
			)
			{
				if (options->so_enforce_self_signing
								&& !key_is_a_NULL_key (the_rdata_n))
				{
					/* Make sure that a kl pointing to km is in list */

					if (key_add_to_list (the_name_n, km->km_any_algorithm,
							km->km_algorithm, km->km_any_key,
							km->km_footprint_h, &set_rec->rrs_signers,
													options)==SIGNER_ERROR)
					{
						return SIGNER_ERROR;
					}
				}
			}
		}
	}
	else /* We have a signature to add */
	{
		if (tree_add_as_sig (set_rec, the_rdata_length_h, the_rdata_n, options)
																== SIGNER_ERROR)
			return SIGNER_ERROR;

		options->so_stats.ss_signatures_read++;
	}

	return SIGNER_OK;
}

int parse_from_file (struct signing_options *options)
{
	FILE				*the_file;
	struct key_list		*the_keys;
	struct error_data	err;
	struct name_context	context;
	char				*non_rr = NULL;
	u_int8_t			*rr = NULL;
	int					dont_do_signing = FALSE;
	char				*this_file;

	/* Which file is to be opened? */

	switch (options->so_phase_of_moon)
	{
		case SO_PHASE_READING_ZONE:
			this_file = options->so_input_zone_file;
			if (key_list_copy (&the_keys, options->so_key_list, options)
																!= SIGNER_OK)
				return SIGNER_ERROR;
			break;
		case SO_PHASE_READING_GLUE:
			this_file = options->so_input_glue_file;
			if (key_list_copy (&the_keys, options->so_key_list, options)
																!= SIGNER_OK)
				return SIGNER_ERROR;
			break;
		case SO_PHASE_READING_PARENT:
			this_file = options->so_input_parent_file;
			if (key_list_copy (&the_keys, options->so_soa_keys, options)
																!= SIGNER_OK)
				return SIGNER_ERROR;
			break;
		default:
			ERROR ("According to the plan, I shouldn't be here now");
			return SIGNER_ERROR;
	}

	/* See if file is already open */

	if (already_opened (this_file)==TRUE)
	{
		sprintf (debug_message, "Attempting to open %s as %s",
					"a result of circular INCLUDE directives", this_file);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	/* Open file if I can reach it */

	if (access (this_file, R_OK)==-1)
	{
		sprintf (debug_message, "Access to %s denied", this_file);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	if ((the_file = fopen (this_file, "r"))==NULL)
	{
		sprintf (debug_message, "Can't open %s (file not found?)", this_file);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	/* Set up context structure */

	memcpy (context.nc_origin_n, options->so_origin_n,
					wire_name_length (options->so_origin_n));
	memcpy (context.nc_previous_n,options->so_previous_n,
					wire_name_length(options->so_previous_n));
	if (options->so_ttl_set==FALSE)
		context.nc_min_ttl_n = 0;
	else
		context.nc_min_ttl_n = options->so_ttl_n;
	context.nc_class_n = options->so_class_n;

	/* Set up error structure */

	err.ed_filename = this_file;
	err.ed_curr_line = 0;
	err.ed_error_value = 0;

	/* Initialize the tree if the SOA hasn't yet been found */

	if (options->so_zone_name_n == NULL) tree_start ();

	do
	{
		/* Retrieve a DNS RR or line */
		if (read_rr (the_file, &err, &context, &rr, &non_rr)==0)
		{
			int zone_name_was_null = (options->so_zone_name_n == NULL);

			/* Have a legal RR to ingest */
			if (ingest_rr_record(rr, options, the_keys)==SIGNER_ERROR)
				dont_do_signing = TRUE;

			if (zone_name_was_null && options->so_zone_name_n != NULL)
			{
				context.nc_min_ttl_n = options->so_ttl_n;
				context.nc_class_n = options->so_class_n;
			}
		}
		else
		{
			/* Either a directive, eof, or error */

			if (err.ed_error_value==ERR_OKBUTNOTRR)
			{
				/* Probably a directive */
				if (non_rr[0]=='$')
				{
					if (handle_directive(non_rr,&context,&err,&the_keys,
							options)!=SIGNER_OK)
						dont_do_signing=TRUE;
				}
				else
					dont_do_signing = TRUE;
			}
			else if (err.ed_error_value==ERR_NOMOREDATA)
			{
				/* End of file hit */
				if (non_rr!=NULL)
				{
					FREE(non_rr);
					non_rr = NULL;
				}
				break;
			}
			else if (err.ed_error_value==ERR_PARSEERROR)
			{
				/* Parsing error */
				sprintf (debug_message, "Parsing error: %s", non_rr);
				ERROR (debug_message);
				dont_do_signing = TRUE;
			}
			else if (err.ed_error_value==ERR_OUTOFMEMORY)
			{
				/* Parse ran out of memory */
				ERROR ("Out of memory, exiting");
				exit (0);
			}
			else
			{
				/* Coding error */
				ERROR ("Internal error, exiting");
				exit (0);
			}
		}
	} while (TRUE);

	clean_up (&the_keys, the_file, this_file);

	memcpy (options->so_previous_n, context.nc_previous_n,
				wire_name_length(context.nc_previous_n));

	return dont_do_signing ? SIGNER_ERROR : SIGNER_OK;
}
