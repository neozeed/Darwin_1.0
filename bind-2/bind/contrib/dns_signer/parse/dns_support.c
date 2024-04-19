/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/dns_support.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#include <stdio.h>
#include <arpa/nameser.h>
#include <time.h>
#include <ctype.h>
#include <netinet/in.h>
#include "dns_support.h"

#ifndef NULL
#define NULL	(void*)0
#endif
#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

u_int16_t wire_name_length (const u_int8_t *field)
{
	/* Calculates the number of bytes in a DNS wire format name */
	u_short	j;
	if (field==NULL) return 0;

	for (j = 0; field[j]&&!(0xc0&field[j])&&j<MAXDNAME ; j += field[j]+1);
	if (field[j]) j++;
	j++;

	if (j > MAXDNAME)
		return 0;
	else
		return j;
}

u_int16_t wire_name_labels (const u_int8_t *field)
{
	/* Calculates the number of bytes in a DNS wire format name */
	u_short	j;
	u_short l=0;
	if (field==NULL) return 0;

	for (j = 0; field[j]&&!(0xc0&field[j])&&j<MAXDNAME ; j += field[j]+1)
		l++;
	if (field[j]) j++;
	j++;
	l++;

	if (field[0] == 1 && field[1] == '*')
		l--;

	if (j > MAXDNAME)
		return 0;
	else
		return l;
}

u_int16_t wire_length (const u_int8_t *field)
{
	u_short name, rdata;

	name = wire_name_length(field);
	memcpy (&rdata, &field[name+8],sizeof(u_short));
	return name + 10 + ntohs(rdata);
}

int temporally_challeneged_signature (const u_int8_t *rr)
{
	int			rr_index;
	u_int16_t	check_type;
	time_t		current_time = time (0);
	time_t		net_time;
	time_t		signed_time;
	time_t		expire_time;

	rr_index = wire_name_length (rr);

	/* Just be sure we are dealing with a SIG record */
	memcpy (&check_type, &rr[rr_index], sizeof(u_int16_t));

	/* Return 'true' even though it's not even a SIG record, 'true' should
		trigger the dumpping of the record anyhow. */
	if (ntohs(check_type) != T_SIG) return TRUE;

	/* Get the times we are interested in */
	rr_index += 10;
	
	/* SIG RDATA format: tc=2, alg=1, labels=1, ottl=4 bytes, expire, signed */

	rr_index += 8;
	memcpy (&net_time, &rr[rr_index], sizeof(u_int32_t));
	expire_time = ntohl (net_time);

	rr_index += sizeof(u_int32_t);
	memcpy (&net_time, &rr[rr_index], sizeof(u_int32_t));
	signed_time = ntohl (net_time);

	if(signed_time <= current_time && current_time <= expire_time) return FALSE;

	/* For time wrapping problems */
	if (expire_time < signed_time
			&&
		(expire_time>=current_time || current_time>=signed_time)) return FALSE;

	return TRUE;
} 

void dump_response (const u_int8_t *ans, int resplen)
{ 
	/* Prints the "raw" response from DNS */
	int i,j, k;

	printf ("Message length is %d\n", resplen);

	for (i = 0; i < 12; i++) printf ("%02x ", (u_char) ans[i]);
	printf ("\n");
	k = 12;
	while (ans[k]) k += ans[k] + 1;
	for (i = 12; i < k+1; i++) printf ("%02x ", (u_char) ans[i]);
	printf (": ");
	for (i = k+1; i < k+5; i++) printf ("%02x ", (u_char) ans[i]);
	printf ("\n");
	k += 5;
	if (k < resplen)
	do
	{
		j = wire_name_length(&ans[k]) + 10; /* j = envelope length */
		j += ntohs(*(u_short*)(&ans[k+j-2])); /* adds rdata length to j */
		for (i = k; i < k+j; i++) printf ("%02x ", (u_char) ans[i]);
		printf ("\n");
		k += j;
	} while (k < resplen);
}

int skip_questions(const u_int8_t *buf)
{
	return 12 + wire_name_length (&buf[12]) + 4;
}

/* The following routine is not advertised outside this file. */

int	labelcmp (const u_int8_t *name1, const u_int8_t *name2)
{
	/* Compare two names, assuming same number of labels in each */
	int				index1 = 0;
	int				index2 = 0;
	int				length1 = (int) name1[index1];
	int				length2 = (int) name2[index2];
	int				min_len = length1 < length2 ? length1 : length2;
	int				ret_val;

	u_int8_t		buffer1[MAXDNAME];
	u_int8_t		buffer2[MAXDNAME];
	int				i;

	/* Degenerate case - root versus root */
	if (length1==0 && length2==0) return 0;

	/* Recurse to try more significant label(s) first */
	ret_val=labelcmp(&name1[length1+1],&name2[length2+1]);

	/* If there is a difference, propogate that back up the calling tree */
	if (ret_val!=0) return ret_val;

	/* Compare this label's first min_len bytes */
	/* Convert to lower case first */
	memcpy (buffer1, &name1[index1+1], min_len);
	for (i =0; i < min_len; i++)
		if (isupper(buffer1[i])) buffer1[i]=tolower(buffer1[i]);
		
	memcpy (buffer2, &name2[index2+1], min_len);
	for (i =0; i < min_len; i++)
		if (isupper(buffer2[i])) buffer2[i]=tolower(buffer2[i]);
		
	ret_val=memcmp(buffer1, buffer2, min_len);

	/* If they differ, propgate that */
	if (ret_val!=0) return ret_val;

	/* If the first n bytes are the same, then the length determines
		the difference - if any */
	return length1-length2;
}

int	namecmp (const u_int8_t *name1, const u_int8_t *name2)
{
	/* compare the DNS wire format names in name1 and name2 */
	/* return -1 if name1 is before name2, 0 if equal, +1 otherwise */
	int	labels1 = 1;
	int	labels2 = 1;
	int	index1 = 0;
	int index2 = 0;
	int ret_val;
	int i;

	/* count labels */
	for (;name1[index1];index1 += (int) name1[index1]+1) labels1++;
	for (;name2[index2];index2 += (int) name2[index2]+1) labels2++;

	index1 = 0;
	index2 = 0;

	if (labels1 > labels2)
		for (i = 0; i < labels1-labels2; i++) index1 += (int) name1[index1]+1;
	else
		for (i = 0; i < labels2-labels1; i++) index2 += (int) name2[index2]+1;

	ret_val = labelcmp(&name1[index1], &name2[index2]);

	if (ret_val != 0) return ret_val;

	/* If one dname is a "proper suffix" of the other,
		the shorter comes first */
	return labels1-labels2;
}

u_int16_t retrieve_type (const u_int8_t *rr)
{
	u_int16_t	type_n; 
	int			name_length = wire_name_length (rr);

	memcpy (&type_n, &rr[name_length], sizeof(u_int16_t));
	return ntohs(type_n);
}

int rr_length (const u_int8_t *buf, int buf_index)
{
	u_int16_t len_n;

	int rr_len = wire_name_length (&buf[buf_index]);
	rr_len += sizeof (u_int16_t);
	rr_len += sizeof (u_int16_t);
	rr_len += sizeof (u_int32_t);
	memcpy (&len_n, &buf[buf_index+rr_len], sizeof(u_int16_t));
	rr_len += sizeof (u_int16_t);
	rr_len += ntohs(len_n);

	return rr_len;
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




