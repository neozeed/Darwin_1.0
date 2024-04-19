/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_key_monitor.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#include "signer_key_monitor.h"
#include "signer_sign.h"
#include "signer_debug.h"
#include "signer_ingest.h"
#include "signer_tree.h"

/*
	n  name_n, a = algorithm, aa= algorithm's any flag, f & fa are for the
	footprint and its any, and c is the candidate
*/
#define KEY_MATCH(n,a,aa,f,fa,c)	\
((fa && c->km_any_key) || (!fa && !c->km_any_key && f==c->km_footprint_h)) && \
((aa && c->km_any_algorithm) || (!aa && !c->km_any_key && a==c->km_algorithm)) \
&& namecmp (c->km_name_n, n) == 0

static struct key_matter *all_keys = NULL;

struct key_matter *key_already_in_matter_list (	u_int8_t		*name_n,
												int				alg_any,
												u_int8_t		alg,
												int				foot_any,
												u_int16_t		foot_h)
{
	struct key_matter *candidate;

	for (candidate = all_keys; candidate; candidate = candidate->km_next)
	{
		if (KEY_MATCH(name_n,alg,alg_any,foot_h,foot_any,candidate))
			return candidate;
	}

	return NULL;
}

struct key_matter *key_add (	u_int8_t				*name_n,
								u_int8_t				algorithm,
								int						algorithm_any,
								u_int16_t				footprint_h,
								int						footprint_any,
								int						signing_requested,
								u_int16_t				rdata_length_h,
								u_int8_t				*rdata_n,
								struct signing_options	*options)
{
	struct key_matter	*km;
	int					name_length = wire_name_length (name_n);

	/* If the rdata is specified, figure out the footprint */
	if (rdata_n != NULL)
	{
		algorithm = rdata_n[KEY_RDATA_ALGORITHM];
		algorithm_any = FALSE;
		footprint_h = dst_s_dns_key_id (rdata_n, rdata_length_h);
		footprint_any = FALSE;
	}

	/* If this key is already in the list, return the pointer to it */

	if ((km = key_already_in_matter_list (name_n, algorithm_any, algorithm,
										footprint_any, footprint_h)) != NULL)
	{
		/* Update the key matter in case we've learned something */

		if (signing_requested == TRUE) km->km_signing_requested = TRUE;

		if (rdata_length_h > 0)
		{
			u_int16_t	short_n;

			memcpy (&short_n, rdata_n, sizeof(u_int16_t));

			if (km->km_flags_h != 0 &&
				km->km_flags_h != ntohs (short_n) &&
				km->km_protocol != 0 &&
				km->km_protocol == rdata_n[KEY_RDATA_PROTOCOL] &&
				km->km_algorithm != 0 &&
				km->km_algorithm == rdata_n[KEY_RDATA_ALGORITHM])
			{
				wire_to_ascii_name (debug_name1_h, name_n, MAXPNAME);
				sprintf (debug_message,
					"Conflicting information for a key for %s", debug_name1_h);
				ERROR (debug_message);
				return NULL;
			}

			if (km->km_flags_h == 0) km->km_flags_h = ntohs (short_n);

			/* Not worth testing for equality */

			km->km_protocol = rdata_n[KEY_RDATA_PROTOCOL];
			km->km_algorithm = rdata_n[KEY_RDATA_ALGORITHM];
		}

		return km;
	}

	/* Add this key to the list and return the pointer to it */

	km = (struct key_matter *) MALLOC (sizeof (struct key_matter));

	if (km == NULL)
	{
		wire_to_ascii_name (debug_name1_h, name_n, MAXPNAME);
		sprintf (debug_message,
			"Out of memory while adding a key for %s - exiting", debug_name1_h);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	if ((km->km_name_n = (u_int8_t *) MALLOC (name_length)) == NULL)
	{
		wire_to_ascii_name (debug_name1_h, name_n, MAXPNAME);
		sprintf (debug_message,
			"Out of memory while adding a key for %s - exiting", debug_name1_h);
		ERROR (debug_message);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	memcpy (km->km_name_n, name_n, name_length);
	km->km_any_algorithm = algorithm_any;
	km->km_any_key = footprint_any;
	km->km_signed_soa = FALSE;
	km->km_signing_requested = signing_requested;
	km->km_has_private = FALSE;
	km->km_key = NULL;

	if (km->km_any_algorithm == TRUE)
		km->km_algorithm = 0;
	else
		km->km_algorithm = algorithm;

	if (km->km_any_key == TRUE)
		km->km_footprint_h = 0;
	else
		km->km_footprint_h = footprint_h;

	if (rdata_n != NULL)
	{
		u_int16_t	short_n;

		memcpy (&short_n, rdata_n, sizeof(u_int16_t));
		km->km_flags_h = ntohs (short_n);
		km->km_protocol = rdata_n[KEY_RDATA_PROTOCOL];
	}
	else
	{
		km->km_flags_h = 0;
		km->km_protocol = '\0';
	}

	km->km_next = all_keys;
	all_keys = km;

	return km;
}

void key_print_key_matter ()
{
	struct key_matter *candidate;

	for (candidate = all_keys; candidate; candidate = candidate->km_next)
		print_key_matter (candidate, NULL);
}

void key_print_key_matter_pointers ()
{
	struct key_matter *candidate;

	for (candidate = all_keys; candidate; candidate = candidate->km_next)
		print_key_matter_pointers (candidate, NULL);
}

void key_list_delete (struct key_list **the_keys)
{
	struct key_list *one_kl = *the_keys?*the_keys:NULL;
	struct key_list *two_kl;

	while (one_kl)
	{
		two_kl = one_kl;
		one_kl = one_kl->kl_next;
		FREE (two_kl);
	}

	*the_keys = NULL;
}

int key_list_copy (struct key_list **the_copy, struct key_list *the_original,
					struct signing_options *options)
{
	struct key_list *to_kl;
	struct key_list *from_kl;
	struct key_list *prev_kl = NULL;

	if (the_original == NULL)
	{
		*the_copy = NULL;
		return SIGNER_OK;
	}

	for (from_kl = the_original; from_kl; from_kl = from_kl->kl_next)
	{
		to_kl = (struct key_list*) MALLOC (sizeof (struct key_list));
		if (to_kl == NULL)
		{ 
			ERROR ("Out of memory copying a key list - exiting");
			tree_delete_parent_files (options);
			sign_cleanup (options);
			exit (1);
		}

		to_kl->kl_key = from_kl->kl_key;
		to_kl->kl_next = NULL;

		if (prev_kl == NULL)
			*the_copy = to_kl;
		else
			prev_kl->kl_next = to_kl;

		prev_kl = to_kl;
	}

	return SIGNER_OK;
}

int key_list_merge (struct key_list **old_list, struct key_list *new_list,
						struct signing_options *options)
{
	struct key_list *candidate;
	struct key_list *looker;

	for (candidate = new_list; candidate; candidate = candidate->kl_next)
	{
		looker = *old_list;
		while (looker != NULL)
		{
			if (looker->kl_key == candidate->kl_key)
				break;
			looker = looker->kl_next;
		}

		if (looker == NULL)
		{
			looker = (struct key_list*) MALLOC (sizeof (struct key_list));
			if (looker == NULL)
			{
				ERROR ("Out of memory merging a key list - exiting");
				tree_delete_parent_files (options);
				sign_cleanup (options);
				exit (1);
			}

			looker->kl_key = candidate->kl_key;
			looker->kl_next = *old_list;
			*old_list = looker;
		}
	}

	return SIGNER_OK;
}

int key_already_in_key_list (struct key_list *the_keys, u_int8_t *name_n,
								int alg_any, u_int8_t alg,
								int foot_any, u_int16_t foot_h)
{
	struct key_list		*candidate;
	struct key_matter	*km;

	for (candidate = the_keys; candidate; candidate = candidate->kl_next)
	{
		if ((km = candidate->kl_key) == NULL)
		{
			/* ERROR: keys should all be known before this check is done */
			WARN ("Dangling key reference found");
			continue;
		}

		if (KEY_MATCH(name_n,alg,alg_any,foot_h,foot_any,km))
			return TRUE;
	}
	return FALSE;
}

int key_add_to_list (u_int8_t *name_n, int alg_any, u_int8_t alg,
						int foot_any, u_int16_t foot_h,
						struct key_list **key_list,
						struct signing_options *options)
{
	struct key_list	*new_key;

	if (key_already_in_key_list (*key_list, name_n, alg_any, alg, foot_any,
															foot_h) == TRUE)
		return SIGNER_OK;

	if ((new_key = (struct key_list*)MALLOC (sizeof(struct key_list))) == NULL)
	{
		wire_to_ascii_name (debug_name1_h, name_n, MAXPNAME);
		sprintf (debug_message, "Out of memory adding %s/key - exiting",
															debug_name1_h);
		ERROR (debug_message);
		key_list_delete (key_list);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	if ((new_key->kl_key = key_add (name_n, alg, alg_any, foot_h, foot_any,
											TRUE, 0, NULL, options)) ==NULL)
	{
		key_list_delete (key_list);
		return SIGNER_ERROR;
	}

	new_key->kl_next = *key_list;
	*key_list = new_key;

	return SIGNER_OK;
}

void key_remove_from_list(u_int8_t *name_n, int alg_any, u_int8_t alg,
				int foot_any, u_int16_t foot_h, struct key_list **key_list)
{
	struct key_list	*trailer = NULL;
	struct key_list *delete = *key_list;

	while (delete)
	{
		if (KEY_MATCH(name_n,alg,alg_any,foot_h,foot_any,delete->kl_key))
		{
			if (trailer)
				trailer->kl_next = delete->kl_next;
			else
				*key_list = delete->kl_next;

			FREE (delete);

			return;
		}

		trailer = delete;
		delete = delete->kl_next;
	}
}

int key_has_zone_flags (u_int16_t flags_h, u_int8_t protocol)
{
	return (VALUE_FLAGS_ZONE_KEY & flags_h &&
				(protocol == PROTOCOL_DNSSEC || protocol == PROTOCOL_ALL));
}

int key_has_signing_flags (u_int16_t flags_h, u_int8_t protocol)
{
	return (!(VALUE_FLAGS_CANT_AUTHENTICATE & flags_h) &&
				(protocol == PROTOCOL_DNSSEC || protocol == PROTOCOL_ALL));
}

int key_is_a_zone_key (u_int8_t *rdata_n)
{
	u_int16_t	short_n;
	u_int16_t	flags_h;
	u_int8_t	protocol;

	memcpy (&short_n, rdata_n, sizeof(u_int16_t));
	flags_h = ntohs (short_n);
	protocol = rdata_n[KEY_RDATA_PROTOCOL];

	return key_has_zone_flags (flags_h, protocol);
}

int key_is_a_signing_key (u_int8_t *rdata_n)
{
	u_int16_t	short_n;
	u_int16_t	flags_h;
	u_int8_t	protocol;

	memcpy (&short_n, rdata_n, sizeof(u_int16_t));
	flags_h = ntohs (short_n);
	protocol = rdata_n[KEY_RDATA_PROTOCOL];

	return key_has_signing_flags (flags_h, protocol);
}

int key_is_a_NULL_key (u_int8_t *rdata_n)
{
	u_int16_t	short_n;
	u_int16_t	flags_h;
	u_int8_t	protocol;

	memcpy (&short_n, rdata_n, sizeof(u_int16_t));
	flags_h = ntohs (short_n);
	protocol = rdata_n[KEY_RDATA_PROTOCOL];

	return ((flags_h & VALUE_FLAGS_CANT_AUTHENTICATE) &&
			(flags_h & VALUE_FLAGS_CANT_CONFIDE) &&
			(protocol == PROTOCOL_DNSSEC || protocol == PROTOCOL_ALL));
}

struct key_matter *key_resolve_any (struct key_matter *key)
{
	/* If there is a key w/same name && w/soa signing on, use it */
	/* If there is a key w/same name && w/signing requested, use it */
	/* If there is a key w/same name && w/zone key, use it */
	/* If there is a key w/same name && w/signing ability, use it */
	/* If there is a key w/same name use it */
	/* Else, return false */

	struct key_matter *signing_requested = NULL;
	struct key_matter *zone_key = NULL;
	struct key_matter *signing_ability = NULL;
	struct key_matter *same_name = NULL;
	struct key_matter *the_winner = NULL;
	
	struct key_matter *candidate;

	/* For each in list of keys (all_keys) */

	for (candidate = all_keys; candidate; candidate = candidate->km_next)
	{
		if (candidate->km_any_key == FALSE &&
			(key->km_any_key ||
				key->km_footprint_h==candidate->km_footprint_h) &&
			candidate->km_any_algorithm == FALSE &&
			(key->km_any_algorithm ||
				key->km_algorithm==candidate->km_algorithm) &&
			namecmp(candidate->km_name_n, key->km_name_n) == 0)
		{
			if (candidate->km_signed_soa)
			{
				the_winner = candidate;
				break; /* We can't beat this one */
			}
			else if (signing_requested == NULL &&
				candidate->km_signing_requested)
			{
				signing_requested = candidate;
			}
			else if (zone_key == NULL &&
				key_has_zone_flags (candidate->km_flags_h,
													candidate->km_protocol))
			{
				zone_key = candidate;
			}
			else if (signing_ability == NULL &&
				key_has_signing_flags (candidate->km_flags_h,
													candidate->km_protocol))
			{
				signing_ability = candidate;
			}
			else if (same_name == NULL)
			{
				same_name = candidate;
			}
		}
	}

	if (the_winner == NULL)
	{
		if (signing_requested != NULL)
			the_winner = signing_requested;
		else if (zone_key != NULL)
			the_winner = zone_key;
		else if (signing_ability != NULL)
			the_winner = signing_ability;
		else
			the_winner = same_name;
	}

	/* If there is at least one non-null */
	if (the_winner != NULL)
	{
		/* Copy the footprint from the best */
		key->km_footprint_h = the_winner->km_footprint_h;
		key->km_algorithm = the_winner->km_algorithm;
	}

	return the_winner;
}

int key_determine_modes (u_int8_t *name_n, u_int8_t modes[], int *max_algorithm)
{
	struct name_rec		*apex;
	struct rrset_rec	*key_set;
	struct rr_rec		*a_key;
	int					algorithm;

	*max_algorithm = 0;

	memset (modes, SO_MODE_UNSET, VALUE_ALGORITHM_MAXIMUM+1);

	apex = tree_find_name (name_n);

	if (apex == NULL)
	{
		ERROR ("There is no zone apex");
		return SIGNER_ERROR;
	}

	key_set = tree_find_set (apex, ns_t_key);

	if (key_set == NULL)
	{
		memset (modes, SO_MODE_NO_KEY, VALUE_ALGORITHM_MAXIMUM+1);
		return SIGNER_OK;
	}

	for (a_key = key_set->rrs_data; a_key; a_key = a_key->rr_next)
	{
		if (key_is_a_zone_key (a_key->rr_rdata))
		{
			algorithm = (int) a_key->rr_rdata[KEY_RDATA_ALGORITHM];

			if (algorithm > *max_algorithm) *max_algorithm = algorithm;

			if (key_is_a_NULL_key (a_key->rr_rdata))
			{
				if (modes[algorithm] == SO_MODE_ZONE_KEY)
					modes[algorithm] =  SO_MODE_BOTH_KEY;
				else
					modes[algorithm] =  SO_MODE_NULL_KEY;
			}
			else
			{
				if (modes[algorithm] == SO_MODE_NULL_KEY)
					modes[algorithm] =  SO_MODE_BOTH_KEY;
				else
					modes[algorithm] =  SO_MODE_ZONE_KEY;
			}
		}
	}
	return SIGNER_OK;
}

int key_find_soa_signers (struct signing_options *options)
{
	struct key_matter	*current;

	/* For each in list of keys (all_keys) */

	for (current = all_keys; current; current = current->km_next)
	{
		if (current->km_signed_soa)
		{
			struct key_list *new_list;

			new_list = (struct key_list *) MALLOC (sizeof(struct key_list));
			if (new_list == NULL)
			{
				ERROR ("Out of memory - exiting");
				tree_delete_parent_files (options);
				sign_cleanup (options);
				exit (1);
			}

			new_list->kl_key = current;
			new_list->kl_next = options->so_soa_keys;
			options->so_soa_keys = new_list;
		}
	}

	return SIGNER_OK;
}

int key_open (struct signing_options	*options)
{
	struct key_matter	*current;

	/* For each in list of keys (all_keys) */

	for (current = all_keys; current != NULL; current = current->km_next)
	{
		struct key_matter	*copy_key = NULL;

		/* If the key is marked as signing_requested */

		if (current->km_key == NULL && current->km_signing_requested == TRUE)
		{
			/* If the key is "any", try to resolve the key */
			if ((current->km_any_key || current->km_any_algorithm) &&
					(copy_key = key_resolve_any (current)) == NULL)
			{
				current->km_key = NULL;
				continue;
			}

			/* Attempt to get the key from dst */

			if (copy_key != NULL && copy_key->km_key != NULL)
			{
				current->km_key = copy_key->km_key;
				current->km_flags_h = copy_key->km_flags_h;
				current->km_algorithm = copy_key->km_algorithm;
				current->km_protocol = copy_key->km_protocol;
				current->km_has_private = copy_key->km_has_private;
			}
			else
			{
				/* Call DST to get the key */
				char	keyname[MAXPNAME];

				wire_to_ascii_name (keyname, current->km_name_n, MAXPNAME);

				current->km_key = dst_read_key (keyname,
							current->km_footprint_h, current->km_algorithm, 
												DST_PRIVATE);

				if (current->km_key != NULL)
				{
					/*
						Warn user if key is not a valid key for
						signing based upon flags and protocol alone
					*/

					if (!key_has_signing_flags (current->km_key->dk_flags,
												current->km_key->dk_proto))
					{
						/* Key is not flagged or protocoled for signing */
						wire_to_ascii_name (debug_name1_h, current->km_name_n,
												MAXPNAME);
						sprintf (debug_message,
							"Key %s/%d is not a DNSSEC-approved signing key",
							debug_name1_h, current->km_footprint_h);
						WARN (debug_message);
					}

					current->km_flags_h = current->km_key->dk_flags;
					current->km_algorithm = current->km_key->dk_alg;
					current->km_protocol = current->km_key->dk_proto;
					current->km_has_private = TRUE;
				}

				if (copy_key != NULL)
				{
					copy_key->km_key = current->km_key;
					if (current->km_key != NULL)
					{
						copy_key->km_flags_h = current->km_flags_h;
						copy_key->km_algorithm = current->km_algorithm;
						copy_key->km_protocol = current->km_protocol;
						copy_key->km_has_private = TRUE;
					}
				} /* if copy_key was not NULL */
			}/* else of if (copy_key != NULL && copy_key->km_key != NULL) */
		} /* if (current->km_key == NULL && current->km_signing_requested */
		print_key_status(current);
	} /* for (current; ... */

	if (debug_level == L_DEBUGGER)
	{
		printf ("Status of keys:\n");
		key_print_key_matter();
		printf ("'#' indicates an any key request, the number its match.\n");
		printf ("Keys with zeroes in () are keys whose private keys are not found\n");
	}

	/* Determine if zone is experimental, signed, or unsigned */

	return key_determine_modes (options->so_zone_name_n,
								options->so_signing_modes,
								&options->so_max_algorithm);
}

void key_cleanup()
{
	free_key_matters (&all_keys);
}

int key_footprint_conflict (struct rrset_rec *set_rec,
							u_int16_t the_rdata_length_h, u_int8_t *the_rdata_n)
{
	struct rr_rec	*e_key;
	u_int16_t		the_footprint_h;

	if (key_is_a_NULL_key (the_rdata_n)) return FALSE;

	the_footprint_h = dst_s_dns_key_id (the_rdata_n, the_rdata_length_h);

	for (e_key = set_rec->rrs_data; e_key; e_key = e_key->rr_next)
	{
		if (!key_is_a_NULL_key (e_key->rr_rdata))
		{
			if (dst_s_dns_key_id (e_key->rr_rdata,
						e_key->rr_rdata_length_h) == the_footprint_h)
			{
				if (the_rdata_length_h == e_key->rr_rdata_length_h &&
				memcmp (e_key->rr_rdata, the_rdata_n, the_rdata_length_h) == 0)
					return FALSE;
				else
					return TRUE;
			}
		}
	}

	return FALSE;
}

struct key_matter *key_find_public (u_int8_t *name_n,
									u_int8_t algorithm,
									u_int16_t footprint_h)
{
	struct key_matter *candidate;

	for (candidate = all_keys; candidate; candidate = candidate->km_next)
	{
		if (KEY_MATCH(name_n,algorithm,FALSE,footprint_h,FALSE,candidate))
			return candidate;
	}

	return NULL;
}

struct key_matter *key_add_public (u_int8_t *name_n,
									u_int8_t alg,
									u_int16_t foot_h,
									struct rrset_rec *key_set,
									struct rr_rec *key_rr,
									struct signing_options *options)
{
	/* Assume this key has already been checked for absence from list */
	struct key_matter	*new_one;
	size_t				name_len = wire_name_length (name_n);
	u_int16_t			short_n;
	char				name_h[MAXPNAME];

	if ((new_one = (struct key_matter *) MALLOC (sizeof (struct key_matter)))
			== NULL)
	{
		wire_to_ascii_name (debug_name1_h, name_n, MAXPNAME);
		sprintf (debug_message,
					"Out of memory while adding a public key for %s - exiting",
					debug_name1_h);
		ERROR (debug_message);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	/* km_name_n = name_n*/

	if  ((new_one->km_name_n = (u_int8_t *) MALLOC (name_len)) == NULL)
	{
		wire_to_ascii_name (debug_name1_h, name_n, MAXPNAME);
		sprintf (debug_message,
					"Out of memory while adding a public key for %s - exiting",
					debug_name1_h);
		ERROR (debug_message);
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	memcpy (new_one->km_name_n, name_n, name_len);

	/* km_any_key = FALSE */
	/* km_signed_soa = FALSE */
	/* km_signing_requested = FALSE */

	new_one->km_any_key = FALSE;
	new_one->km_any_algorithm = FALSE;
	new_one->km_signed_soa = FALSE;
	new_one->km_signing_requested = FALSE;

	/* km_flags_h from key_rr */

	memcpy (&short_n, key_rr->rr_rdata, sizeof (u_int16_t));
	new_one->km_flags_h = ntohs (short_n);

	/* km_protocol from key_rr */
	/* km_algorithm from key_rr */
	/* km_footprint_h from foot_h */

	new_one->km_protocol = key_rr->rr_rdata[KEY_RDATA_PROTOCOL];
	new_one->km_protocol = key_rr->rr_rdata[KEY_RDATA_ALGORITHM];
	new_one->km_footprint_h = foot_h;
	new_one->km_algorithm = alg;

	/* km_key = dst fromdnskey to ... */

	wire_to_ascii_name (name_h, name_n, MAXPNAME);
	new_one->km_key = dst_dnskey_to_key (name_h, key_rr->rr_rdata, 
										 (int) key_rr->rr_rdata_length_h);

	/* km_next = all_keys/all_keys = new_one */

	new_one->km_next = all_keys;
	all_keys = new_one;

	return new_one;
}

struct rr_rec *key_find_key (struct rrset_rec *key_set, u_int8_t algorithm,
														u_int16_t footprint_h)
{
	struct rr_rec *candidate;

	for (candidate=key_set->rrs_data; candidate; candidate=candidate->rr_next)
	{
		if (algorithm == candidate->rr_rdata[KEY_RDATA_ALGORITHM] &&
			footprint_h == dst_s_dns_key_id (candidate->rr_rdata,
											 candidate->rr_rdata_length_h))
		{
			break;
		}
	}
	return candidate;
}

void key_list_filter (	struct key_list **the_list,
						u_int8_t	*name_n)
{
	struct key_list *trail = NULL;
	struct key_list *examine = *the_list;

	while (examine)
	{
		if (namecmp (examine->kl_key->km_name_n, name_n) == 0)
		{
			if (trail != NULL)
			{
				trail->kl_next = examine->kl_next;
				FREE (examine);
				examine = trail->kl_next;
			}
			else
			{
				*the_list = (*the_list)->kl_next;
				FREE (examine);
				examine = *the_list;
			}
		}
		else
		{
			trail = examine;
			examine = examine->kl_next;
		}
	}
}

DST_KEY *key_open_public (u_int8_t *name_n, u_int8_t algorithm,
											u_int16_t footprint_h)
{
	char				name_h[MAXPNAME];
	struct name_rec		*nr;
	struct rrset_rec	*rrs;
	struct rr_rec		*rr;

	wire_to_ascii_name (name_h, name_n, MAXPNAME);

	if ((nr = tree_find_name (name_n)) == NULL) return NULL;
	if ((rrs = tree_find_set (nr, ns_t_key)) == NULL) return NULL;
	if ((rr = key_find_key (rrs, algorithm, footprint_h)) == NULL) return NULL;

	return dst_dnskey_to_key(name_h, rr->rr_rdata, rr->rr_rdata_length_h);
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




