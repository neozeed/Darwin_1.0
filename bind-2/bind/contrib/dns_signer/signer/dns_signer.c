/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/dns_signer.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
    
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

#include "dns_signer.h"
#include "signer_ingest.h"
#include "signer_tree.h"
#include "signer_key_monitor.h"
#include "signer_parameters.h"

char	software_version[] = "dns_signer dated April 8, 1999";

#define MAX_MESSAGE		4096 /* Bigger than 2 * MAXPNAME + 80 characters */
int		debug_level;
int		debug_success;
char	debug_name1_h[MAXPNAME];
char	debug_name2_h[MAXPNAME];
char	debug_message[MAX_MESSAGE];

void print_stats (struct signing_statistics *ss)
{
	int		sig_delta;
	int		nxt_delta;
	char	time_string[MAX_MESSAGE];

	nxt_delta = ss->ss_nxts_added - ss->ss_nxts_dropped;
	sig_delta = ss->ss_signatures_written - ss->ss_signatures_read;

	printf ("Time statistics\n");
	strftime(time_string,MAX_MESSAGE,"%a, %D %r",
										localtime(&ss->ss_run_begun));
	printf ("Run begun:                 %s\n",time_string);
	strftime(time_string,MAX_MESSAGE,"%a, %D %r",
										localtime(&ss->ss_tree_finished));
	printf ("Tree finished:             %s\n",time_string);
	strftime(time_string,MAX_MESSAGE,"%a, %D %r",
										localtime(&ss->ss_signing_begun));
	printf ("Signing started:           %s\n",time_string);
	strftime(time_string,MAX_MESSAGE,"%a, %D %r",
										localtime(&ss->ss_run_ended));
	printf ("Run ended:                 %s\n",time_string);

	printf ("\nSearch space statistics\n");
	printf ("Names read:                %9d\n", ss->ss_names_read);
	printf ("Names in zone:             %9d\n", ss->ss_names_in_zone);
	printf ("Sets read:                 %9d\n", ss->ss_sets_read);

	printf ("\nRecords read/written\n");
	printf ("Records read:              %9d\n", ss->ss_records_read);
	if (ss->ss_duplicate_records)
		printf ("Duplicates:                    %9d\n",
											-ss->ss_duplicate_records);
	if (ss->ss_sets_dropped_for_parent)
		printf ("PARENT dropped (non-SIG):      %9d\n",
											-ss->ss_sets_dropped_for_parent);
	if (sig_delta)
		printf ("Signatures gain or loss        %9d\n",
											sig_delta);
	if (nxt_delta)
		printf ("NXTs gain or loss              %9d\n",
											nxt_delta);
	if (ss->ss_null_keys_added)
		printf ("NULL Keys added                %9d\n",
											ss->ss_null_keys_added);
	printf ("Records written:           %9d\n", ss->ss_records_written);

	if (nxt_delta)
	{
		printf ("\nNXT statistics\n");
		printf ("NXTs dropped:              %9d\n", ss->ss_nxts_dropped);
		printf ("NXTs added:                %9d\n", ss->ss_nxts_added);
	}

	if (	ss->ss_signatures_expired || ss->ss_signatures_retired ||
			ss->ss_verifications_failed || ss->ss_signatures_parent ||
			ss->ss_signatures_cant_confirm || ss->ss_signatures_unchecked ||
			ss->ss_verifications_successful || ss->ss_signings_done
		)
		printf ("\nSignature statistics\n");
	else
		return;

	if (ss->ss_signatures_read)
		printf ("Sigs read:                 %9d (= removed + retained)\n",
											ss->ss_signatures_read);
	if (ss->ss_signatures_expired)
		printf ("Sigs expired:              %9d (removed)\n",
											ss->ss_signatures_expired);
	if (ss->ss_signatures_retired)
		printf ("Sigs retired:              %9d (removed)\n",
											ss->ss_signatures_retired);
	if (ss->ss_verifications_failed)
		printf ("Sigs invalidated:          %9d (removed)\n",
											ss->ss_verifications_failed);
	if (ss->ss_signatures_parent)
		printf ("Sigs dropped (PARENT):     %9d (removed)\n",
											ss->ss_signatures_parent);
	if (ss->ss_signatures_cant_confirm)
		printf ("Sigs can't confirm:        %9d (retained)\n",
											ss->ss_signatures_cant_confirm);
	if (ss->ss_signatures_unchecked)
		printf ("Sigs unchecked:            %9d (retained)\n",
											ss->ss_signatures_unchecked);
	if (ss->ss_verifications_successful)
		printf ("Sigs recycled:             %9d (retained)\n",
											ss->ss_verifications_successful);
	if (ss->ss_signings_done)
		printf ("Sigs added:                %9d (inserted)\n",
											ss->ss_signings_done);
	if (ss->ss_signatures_written)
		printf ("Sigs written:              %9d (= retained + inserted)\n",
											ss->ss_signatures_written);
}

void free_name_rec  (struct name_rec *nr)
{
	if (nr)
	{
		if (nr->nr_name_n) FREE (nr->nr_name_n);
		if (nr->nr_location == NR_LOCATION_INMEMORY)
			free_rrset_recs (&(nr->nr_rrsets));
		FREE (nr);
	}
}

void free_rrset_recs (struct rrset_rec **rrs)
{
	if (rrs && *rrs)
	{
		if ((*rrs)->rrs_data) free_rr_recs (&((*rrs)->rrs_data));
		if ((*rrs)->rrs_sig) free_rr_recs (&((*rrs)->rrs_sig));
		if ((*rrs)->rrs_signers) key_list_delete (&((*rrs)->rrs_signers));
		if ((*rrs)->rrs_recyclers) free_rr_recs (&((*rrs)->rrs_recyclers));
		if ((*rrs)->rrs_next) free_rrset_recs (&((*rrs)->rrs_next));
		FREE (*rrs);
		*rrs = NULL;
	}
}

void free_rr_recs (struct rr_rec **rr)
{
	if (rr && *rr)
	{
		if ((*rr)->rr_rdata) FREE ((*rr)->rr_rdata);
		if ((*rr)->rr_next) free_rr_recs (&((*rr)->rr_next));
		FREE (*rr);
		*rr = NULL;
	}
}

void free_key_matters (struct key_matter **km)
{
	if (km && *km)
	{
		if ((*km)->km_name_n) FREE ((*km)->km_name_n);
		if ((*km)->km_any_key==FALSE && 
			(*km)->km_key) dst_free_key ((*km)->km_key);
		if ((*km)->km_next) free_key_matters (&((*km)->km_next));
		FREE (*km);
		*km = NULL;
	}
}

int main (int argc, char *argv[])
{
	int						ret_val;
	struct signing_options	options;

	options.so_stats.ss_run_begun = time(0);

	options.so_phase_of_moon = SO_PHASE_INITIALIZATION;

	register_special_char ('$');

	/* Interpret run time paramters */

	if ((ret_val = get_run_time_parameters(argc,argv,&options)) == SIGNER_ERROR)
		exit (1);

	/* Ingest zone data */

	options.so_phase_of_moon = SO_PHASE_READING_ZONE;

	dst_init();

	ret_val = parse_from_file (&options);

	/* Ingest glue data */

	if ((ret_val == SIGNER_OK) && options.so_input_glue_file != NULL)
	{
		options.so_phase_of_moon = SO_PHASE_READING_GLUE;
		ret_val = parse_from_file (&options);
	}

	/* Ingest parent data */

	if ((ret_val == SIGNER_OK) && options.so_input_parent_file != NULL)
	{
		options.so_phase_of_moon = SO_PHASE_READING_PARENT;
		ret_val = parse_from_file (&options);
	}

	options.so_stats.ss_tree_finished = time(0);

	if (ret_val == SIGNER_OK) 
	{
		/* Process the keys */

		options.so_phase_of_moon = SO_PHASE_OPENING_KEYS;

		ret_val = key_open (&options);

		options.so_stats.ss_signing_begun = time(0);

		if (ret_val == SIGNER_OK)
		{
			/* Make NXT's, make sure no Proto-x status names exist, and sign */

			options.so_phase_of_moon = SO_PHASE_SIGNING;

			tree_sign (&options);
			ret_val = options.so_return_code;
		}
	}

	if (ret_val == SIGNER_ERROR)
		tree_delete_parent_files (&options);

	options.so_stats.ss_run_ended = time(0);

#ifdef MEMORY_DEBUGGING

	tree_cleanup ();
	tree_print_pointers ();

	key_list_delete (&options.so_soa_keys);
	key_list_delete (&options.so_key_list);
	key_cleanup ();
	key_print_key_matter_pointers ();

	if (options.so_zone_name_n) FREE (options.so_zone_name_n);
	if (options.so_parent_name_n) FREE (options.so_parent_name_n);

#endif

	if (options.so_show_stats && ret_val != SIGNER_ERROR)
		print_stats (&options.so_stats);

	return SIGNER_OK;
}
