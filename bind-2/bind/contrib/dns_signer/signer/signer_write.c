/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_write.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#include "signer_write.h"
#include "signer_tree.h"
#include "signer_debug.h"
#include "signer_ns_print.h"
#include "signer_key_monitor.h"

void write_trim_name (char *name_h, char *zone_name_h)
{
	int index = strlen (name_h) - strlen (zone_name_h) - 1;

	if (index == 0) return;

	if (strcmp (zone_name_h, ".")==0) return;

	if (strcmp (&name_h[index+1], zone_name_h) != 0) return;

	name_h[index] = '\0';
}

void write_remove_duplicates (	struct rrset_rec		*rrs,
								struct signing_options	*options)
{
	/*
		rrs->rrs_data points to a list of rdatas.  There maybe
		duplicates at this point.  They are to be deleted before
		printing.

		This is done by moving all but the first RR to the unchecked
		list.  The first RR in the original list remains in the
		rrs_data, which is the "checked list."

		RR's in the unchecked list are checked against the ones
		in the checked list for equality.  If an equivalent RR is
		found, the RR in the unchecked list is going to be removed. 
		If there is no equality then the RR being checked is moved
		from the unchecked list to the tail of the checked list.

		After checking all RR's, all RR's remaining in the unchecked
		list are deleted.

	*/

	struct rr_rec	*zero; /* The unchecked RR pointing to one */
	struct rr_rec	*one; /* The unchecked RR being checked */
	struct rr_rec	*two; /* The checked RR being matched to one */
	struct rr_rec	*old; /* Head of the unchecked (and to be deleted) RRs */
	struct rr_rec	*tail; /* The last element in the checked RR list */

	if (rrs->rrs_data==NULL) return;
	old = rrs->rrs_data->rr_next;
	if (old==NULL) return;

	/* All but the first RR is in the old list */

	tail = rrs->rrs_data;
	tail->rr_next = NULL;

	/* Tail points to the lone element in the "checked" list */

	/* one will be the current RR under check, zero is the one pointing to it */

	zero = NULL;
	one = old;

	while (one != NULL)
	{
		/*
			two will point to the previously checked and unique RR
			to be compared to one
		*/
		two = rrs->rrs_data;

		while (two != NULL)
		{
			if (one->rr_rdata_length_h == two->rr_rdata_length_h &&
				memcmp (one->rr_rdata, two->rr_rdata,
											one->rr_rdata_length_h)==0)
				break;
			else
				two = two->rr_next;
		}

		/*
			If two is pointing to an RR, then *one is equivalent to *two.

			In this case, leave *one in place, other wise move one to after
			tail.

			In code, the test is reversed...
		*/
		if (two == NULL)
		{
			/* Move one to tail->rr_next */
			tail->rr_next = one;
			if (zero==NULL) old = old->rr_next;
			else			zero->rr_next = one->rr_next;
			one = one->rr_next;
			tail = tail->rr_next;
			tail->rr_next = NULL;
		}
		else
		{
			options->so_stats.ss_duplicate_records++;
			zero = one;
			one = one->rr_next;
		}
	}

	/* Delete whatever remains in the unchecked list */

	if (old != NULL) free_rr_recs (&old);
}

/* Done */
int write_set (	FILE					*file,
				struct name_rec			*nr,
				u_int16_t				type_h,
				struct signing_options	*options,
				int						is_glue)
{
	struct rrset_rec	*the_set = tree_find_set (nr, type_h);

	if (the_set==NULL)
	{
		wire_to_ascii_name (debug_name1_h, nr->nr_name_n, MAXPNAME);
		sprintf (debug_message, "Couldn't locate set %s/%s for writing",
					debug_name1_h,
					local_sym_ntop(__p_type_syms, type_h, &debug_success));
		ERROR (debug_message);
		options->so_return_code = SIGNER_ERROR;
		return SIGNER_ERROR;
	}

	return write_rrset (file, nr, the_set, options, is_glue);
}

int write_signer_directives (	FILE *file,
								struct signing_options *options,
								struct key_list *new_list)
{
	struct key_list *suspect;
	struct key_list *trailer;
	int				begun_print;
	char			name_h[MAXPNAME];
	char			zone_name_h[MAXPNAME];

	wire_to_ascii_name(zone_name_h,options->so_zone_name_n,MAXPNAME);

	/* Look for keys to delete */

	begun_print = FALSE;
	suspect = options->so_current_directive_keys;
	trailer = NULL;

	while (suspect)
	{
		struct key_matter *km = suspect->kl_key;

		if (!key_already_in_key_list (new_list, km->km_name_n,
					km->km_any_algorithm, km->km_algorithm,
					km->km_any_key, km->km_footprint_h))
		{
			if (!begun_print)
			{
				fprintf (file, "$SIGNER DEL");
				begun_print = TRUE;
			}

			wire_to_ascii_name(name_h,km->km_name_n,MAXPNAME);
			if (file == options->so_zone_output)
				write_trim_name (name_h, zone_name_h);

			if (km->km_any_key && km->km_any_algorithm)
				fprintf (file, " %s any any", name_h);
			else if (km->km_any_key)
				fprintf (file, " %s %d any", name_h, km->km_algorithm);
			else if (km->km_any_algorithm)
				fprintf (file, " %s any %d", name_h, km->km_footprint_h);
			else
				fprintf (file, " %s %d %d", name_h, km->km_algorithm,
													km->km_footprint_h);

			/* Trim out the deadbeat */

			if (trailer == NULL)
			{
				options->so_current_directive_keys = suspect->kl_next;
				FREE (suspect);
				suspect = options->so_current_directive_keys;
			}
			else
			{
				trailer->kl_next = suspect->kl_next;
				FREE (suspect);
				suspect = trailer->kl_next;
			}
		}
		else
		{
			trailer = suspect;
			suspect = suspect->kl_next;
		}
	}
	if (begun_print) fprintf (file, "\n");

	/* Look for keys to add */
	begun_print = FALSE;
	suspect = new_list;

	while (suspect)
	{
		struct key_matter *km = suspect->kl_key;

		if (!key_already_in_key_list (options->so_current_directive_keys,
				km->km_name_n,km->km_any_algorithm, km->km_algorithm,
				km->km_any_key, km->km_footprint_h))
		{
			if (!begun_print)
			{
				fprintf (file, "$SIGNER ADD");
				begun_print = TRUE;
			}

			wire_to_ascii_name(name_h,km->km_name_n,MAXPNAME);
			if (file == options->so_zone_output)
				write_trim_name (name_h, zone_name_h);

			if (km->km_any_key && km->km_any_algorithm)
				fprintf (file, " %s any any", name_h);
			else if (km->km_any_key)
				fprintf (file, " %s %d any", name_h, km->km_algorithm);
			else if (km->km_any_algorithm)
				fprintf (file, " %s any %d", name_h, km->km_footprint_h);
			else
				fprintf (file, " %s %d %d", name_h, km->km_algorithm,
													km->km_footprint_h);
		}
		suspect = suspect->kl_next;
	}
	if (begun_print) fprintf (file, "\n");

	/* Update the current list */

	if (key_list_merge (&options->so_current_directive_keys, new_list, options)
															== SIGNER_ERROR)
		return SIGNER_ERROR;

	return SIGNER_OK;
}

#define RDATA_LENGTH 8192
int write_rrset (FILE					*file,
				struct name_rec			*nr,
				struct rrset_rec		*rrs,
				struct signing_options	*options,
				int						is_glue)
{
	char			name_h[MAXPNAME];
	struct rr_rec	*rr;
	int				success;
	char			zone_name_h[MAXPNAME];
	int				count=0;

	wire_to_ascii_name (zone_name_h, options->so_zone_name_n, MAXPNAME);

	if (write_signer_directives(file,options,rrs->rrs_signers) == SIGNER_ERROR)
		return SIGNER_ERROR;

	wire_to_ascii_name (name_h, nr->nr_name_n, MAXPNAME);
	if (file == options->so_zone_output)
	{
		write_trim_name (name_h, zone_name_h);
	}

	for (rr = rrs->rrs_data; rr; rr = rr->rr_next)
	{
		char	rdata_h[RDATA_LENGTH];

		memset (rdata_h, 0, RDATA_LENGTH);
		count++;

		fprintf (file, "%-12s %8d %-5s %-5s ",
			count==1?name_h:" ",
			(int) ntohl(rrs->rrs_ttl_n),
			local_sym_ntop(__p_class_syms,ntohs(nr->nr_class_n),&success),
			local_sym_ntop(__p_type_syms,rrs->rrs_type_h,&success));

		ns_sprintrrf_rdata (options, rrs->rrs_type_h, rr->rr_rdata,
				rr->rr_rdata_length_h, rdata_h, RDATA_LENGTH);

		fprintf (file, "%s\n", rdata_h);

		options->so_stats.ss_records_written++;
	}

	if (is_glue == FALSE)
	{
		for (rr = rrs->rrs_sig; rr; rr = rr->rr_next)
		{
			char rdata_h[RDATA_LENGTH];
			memset (rdata_h, 0, RDATA_LENGTH);
			fprintf (file, "%-12s %8d %-5s %-5s ", " ",
				(int) ntohl(rrs->rrs_ttl_n),
				local_sym_ntop(__p_class_syms,ntohs(nr->nr_class_n),&success),
				local_sym_ntop(__p_type_syms,ns_t_sig,&success));

			ns_sprintrrf_rdata (options, ns_t_sig, rr->rr_rdata,
				rr->rr_rdata_length_h, rdata_h, RDATA_LENGTH);

			fprintf (file, "%s\n", rdata_h);

			options->so_stats.ss_signatures_written++;
			options->so_stats.ss_records_written++;
		}
	}

	return SIGNER_OK;
}

int write_glue (FILE					*file,
				struct name_rec			*nr,
				struct signing_options	*options)
{
	struct rrset_rec	*nrNS;
	struct rr_rec		*a_nrNS;
	struct name_rec		*a_server;
	struct rrset_rec	*a_server_set;

	/* Get the NS records for the name */
	nrNS = tree_find_set (nr, ns_t_ns);

	if (nrNS==NULL)
	{
		wire_to_ascii_name (debug_name1_h, nr->nr_name_n, MAXPNAME);
		sprintf (debug_message, "Couldn't locate set %s/%s for writing",
					debug_name1_h, "ns");
		ERROR (debug_message);
		options->so_return_code = SIGNER_ERROR;
		return SIGNER_ERROR;
	}

	/* For each NS record rdata */

	for (a_nrNS = nrNS->rrs_data; a_nrNS; a_nrNS = a_nrNS->rr_next)
	{
		/* Get the NS name record */
		a_server = tree_find_name (a_nrNS->rr_rdata);

		if (a_server == NULL)
		{
			wire_to_ascii_name (debug_name1_h, a_nrNS->rr_rdata, MAXPNAME);
			sprintf (debug_message,
						"Couldn't locate entry for NS %s for writing",
						debug_name1_h);
			ERROR (debug_message);
			options->so_return_code = SIGNER_ERROR;
			continue;
		}

		a_server_set = tree_find_set (a_server, ns_t_a);

		if (a_server_set==NULL)
		{
			wire_to_ascii_name (debug_name1_h, nr->nr_name_n, MAXPNAME);
			sprintf (debug_message, "Couldn't find set %s/%s for writing",
						debug_name1_h, "a");
			ERROR (debug_message);
			options->so_return_code = SIGNER_ERROR;
			return SIGNER_ERROR;
		}

		/* Make sure there are no duplicates */

		write_remove_duplicates (a_server_set, options);

		/* Write the set */

		if (write_rrset (file, a_server, a_server_set, options, TRUE)
															== SIGNER_ERROR)
		{
			options->so_return_code = SIGNER_ERROR;
			return SIGNER_ERROR;
		}
	}

	return SIGNER_OK;
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




