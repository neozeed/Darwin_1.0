/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_debug.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#include "signer_debug.h"

const char *local_sym_ntop(const struct res_sym *syms, int number, int *success)
{
	static char unname[STRING_64];

	for ((void)NULL; syms->name != 0; syms++)
	{
		if (number == syms->number)
		{
			if (success)
			*success = 1;
			return (syms->name);
		}
	}
	sprintf(unname, "%d", number);
	if (success)
	*success = 0;
	return (unname);
}

char status_print[STRING_64];
char *print_name_status (u_int8_t s)
{
	switch (s)
	{
		case NR_STATUS_UNSET:
			strcpy (status_print, "Unset");
			break;
		case NR_STATUS_IN:
			strcpy (status_print, "In");
			break;
		case NR_STATUS_SUBZONE:
			strcpy (status_print, "Subzone");
			break;
		case NR_STATUS_GLUE:
			strcpy (status_print, "Glue");
			break;
		case NR_STATUS_DELEGATION:
			strcpy (status_print, "Delegation");
			break;
		case NR_STATUS_PROTO_DELEG:
			strcpy (status_print, "ProtoDelegation");
			break;
		case NR_STATUS_APEX:
			strcpy (status_print, "Apex");
			break;
		case NR_STATUS_PROTO_APEX:
			strcpy (status_print, "ProtoApex");
			break;
		case NR_STATUS_ERROR:
			strcpy (status_print, "Error");
			break;
		case NR_STATUS_PARENT:
			strcpy (status_print, "Parent");
			break;
		default:
			strcpy (status_print, "Programming Error");
			break;
	}
	return status_print;
}

int print_name_rec (struct name_rec *nr, int *flag)
{
	char				name_h[MAXPNAME];
	int					success;
	struct rrset_rec	*rrs;
	struct key_list		*k;
	struct rr_rec		*rr;
 
	wire_to_ascii_name (name_h, nr->nr_name_n, MAXPNAME);

	if (nr->nr_location != NR_LOCATION_INMEMORY)
	{
		printf ("Name: %s, location %s, status %s\n",
				name_h,
				"On Disk",
				print_name_status (nr->nr_status)
				);
		return TRUE;
	}

	printf ("Name: %s, class %s, location %s, status %s\n",
			name_h,
			local_sym_ntop(__p_class_syms,ntohs(nr->nr_class_n),&success),
			"In Memory",
			print_name_status (nr->nr_status)
			);

	for (rrs = nr->nr_rrsets; rrs; rrs = rrs->rrs_next)
	{
		printf (" Type: %s, TTL: %d\n",
				local_sym_ntop(__p_type_syms,rrs->rrs_type_h,&success),
				(int)ntohl (rrs->rrs_ttl_n));

		if (flag) continue;
		for (k =  rrs->rrs_signers; k; k = k->kl_next)
		{
			printf ("  Signer: ");
			print_key_matter (k->kl_key, (void*)!NULL);
		}

		for (rr = rrs->rrs_data; rr; rr = rr->rr_next)
		{
			printf ("  Data: ");
			print_hex (rr->rr_rdata,
					20 < rr->rr_rdata_length_h ? 20 : rr->rr_rdata_length_h);
			printf ("%s\n", 20 < rr->rr_rdata_length_h? "...":"");
		}

		for (rr = rrs->rrs_sig; rr; rr = rr->rr_next)
		{
			char		name_h[MAXPNAME];
			u_int16_t	short_n;
			wire_to_ascii_name (name_h, &rr->rr_rdata[SIG_RDATA_BY_NAME],
																MAXPNAME);
			memcpy (&short_n, &rr->rr_rdata[SIG_RDATA_BY_FOOTPRINT], 2);

			printf ("  Signature by %s/%d\n", name_h, ntohs(short_n));
		}

		for (rr = rrs->rrs_recyclers; rr; rr = rr->rr_next)
		{
			char		name_h[MAXPNAME];
			u_int16_t	short_n;
			wire_to_ascii_name (name_h, &rr->rr_rdata[SIG_RDATA_BY_NAME],
																MAXPNAME);
			memcpy (&short_n, &rr->rr_rdata[SIG_RDATA_BY_FOOTPRINT], 2);

			printf ("  Recycler by %s/%d\n", name_h, ntohs(short_n));
		}
	}

	return TRUE;
}

int print_key_matter (struct key_matter *km, int *flag)
{
	char				name_h[MAXPNAME];
	char				footprint_s[STRING_64];
	u_int16_t			short_n;
 
	wire_to_ascii_name (name_h, km->km_name_n, MAXPNAME);

	if ((km->km_flags_h & 0xC000)==0xC000)
		sprintf (footprint_s, "#%5s", "-----");
	else if (km->km_any_key==TRUE)
		sprintf (footprint_s, "#%5d", km->km_footprint_h);
	else
		sprintf (footprint_s, "%6d", km->km_footprint_h);

	if (flag==NULL)
	{
		short_n = htons (km->km_flags_h);
		printf ("  Key: %-15s/%-7s (%08x) x%02x%02x(f) %3d(p) %s%3d(a)%s%s\n",
				name_h,
				footprint_s,
				(u_int32_t) km->km_key,
				((u_int8_t*)&short_n)[0],
				((u_int8_t*)&short_n)[1],
				km->km_protocol,
				km->km_any_algorithm?"#":" ",
				km->km_algorithm,
				km->km_signing_requested==FALSE?", non-signer":"",
				km->km_signed_soa==FALSE?"":", soa signer"
			);
	}
	else
		printf ("  Name: %s/%s\n", name_h, footprint_s);

	return TRUE;
}

/*
 * km->km_key == NULL && km->km_signing_requested && !km->km_any_key
 * ==> print a warning
 */
void print_key_status (struct key_matter *km)
{
	char				name_h[MAXPNAME];
	char				alg[8], footprint[8];
 
	wire_to_ascii_name (name_h, km->km_name_n, MAXPNAME);

	if (km->km_key == NULL && km->km_signing_requested == TRUE)
	{
		if (km->km_any_algorithm == TRUE)
			strcpy(alg, " any");
		else
			sprintf(alg, "+%03d", km->km_algorithm);
		if (km->km_any_key == TRUE)
			strcpy(footprint, " any");
		else
			sprintf(footprint, "+%05d", km->km_footprint_h);
		sprintf(debug_message, "key K%s%s%s not found",
				name_h, alg, footprint);
		WARN(debug_message);
	}
}

int print_name_rec_pointers (struct name_rec *nr, int *flag)
{
	struct rrset_rec	*rrs;
	struct key_list		*k;
	struct rr_rec		*rr;
 
	printf ("0x000%lx bINUSE nr\n", (u_long) nr);
	printf ("0x000%lx bINUSE nr_name\n", (u_long) nr->nr_name_n);

	if (nr->nr_location != NR_LOCATION_INMEMORY) return TRUE;

	for (rrs = nr->nr_rrsets; rrs; rrs = rrs->rrs_next)
	{
		printf ("0x000%lx bINUSE rrs\n", (u_long) rrs);

		for (k =  rrs->rrs_signers; k; k = k->kl_next)
			printf ("0x000%lx bINUSE k\n", (u_long) k);

		for (rr =  rrs->rrs_recyclers; rr; rr = rr->rr_next)
			printf ("0x000%lx bINUSE k\n", (u_long) k);

		for (rr = rrs->rrs_data; rr; rr = rr->rr_next)
		{
			printf ("0x000%lx bINUSE rr data\n", (u_long) rr);
			printf ("0x000%lx bINUSE rr_rdata data\n", (u_long) rr->rr_rdata);
		}

		for (rr = rrs->rrs_sig; rr; rr = rr->rr_next)
		{
			printf ("0x000%lx bINUSE rr sig\n", (u_long) rr);
			printf ("0x000%lx bINUSE rr_rdata sig\n", (u_long) rr->rr_rdata);
		}
	}

	return TRUE;
}

int print_key_matter_pointers (struct key_matter *km, int *flag)
{
	printf ("0x000%lx bINUSE km\n", (u_long) km);
	printf ("0x000%lx bINUSE km_name\n", (u_long) km->km_name_n);

	return TRUE;
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




