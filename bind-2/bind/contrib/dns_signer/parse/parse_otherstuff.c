/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/parse_otherstuff.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

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

#include "pkg_internal_dns_parse.h"

int parse_string (struct scratch *rr, char *token, struct error_data *errors,
		struct name_context *context, char **non_rr) {
	/*
		The returned value is -1 for any error, the length of
		the parsed string otherwise.
	*/
	int		token_index = 0;
	int		length_index;
	int		escape_num;
	char	temptoken[MAXTOKENLENGTH];

	if (rr->s_index==rr->s_length)
	{
		SKIP_REST_OF_LINE (temptoken);
		SET_PARSE_ERROR(errors,
			"Scratch overflow adding %s", token, *non_rr);
		return -1;
	}

	length_index = rr->s_index++;

	while (token[token_index])
	{
		/* If escaped, better be getting 3 digits next */
		if (token[token_index]=='\\')
		{
			token_index++;
			if (isdigit(token[token_index]))
			{
				/* First digit */
				escape_num = (0x0F & token[token_index++])*100;

				/* Second digit */
				if (!isdigit(token[token_index]))
				{
					SKIP_REST_OF_LINE (temptoken);
					SET_PARSE_ERROR(errors,
						"Illegally formed escape sequence in %s",token,*non_rr);
					return -1;
				}

				escape_num += (0x0F & token[token_index++])*10;

				/* Third & final digit */
				if (!isdigit(token[token_index]))
				{
					SKIP_REST_OF_LINE (temptoken);
					SET_PARSE_ERROR(errors,
						"Illegally formed escape sequence in %s",token,*non_rr);
					return -1;
				}

				escape_num += (0x0F & token[token_index++]);

				if (escape_num < 0 || escape_num > 255)
				{
					SKIP_REST_OF_LINE (temptoken);
					SET_PARSE_ERROR(errors,
						"Illegal value numeric in %s",token,*non_rr);
					return -1;
				}

				if (rr->s_index==rr->s_length)
				{
					SKIP_REST_OF_LINE (temptoken);
					SET_PARSE_ERROR(errors,
						"Scratch overflow adding %s",token,*non_rr);
					return -1;
				}
				rr->s_field[rr->s_index++] = (u_int8_t) escape_num;
			}
			else /* The escaped character is not 'DDD' */
			{
				if (rr->s_index==rr->s_length)
				{
					SKIP_REST_OF_LINE (temptoken);
					SET_PARSE_ERROR(errors,
						"Scratch overflow adding %s",token,*non_rr);
					return -1;
				}
				rr->s_field[rr->s_index++] = (u_int8_t) token[token_index++];
			}
		}
		else
		{
			if (rr->s_index==rr->s_length)
		{
				SKIP_REST_OF_LINE (temptoken);
				SET_PARSE_ERROR(errors,
					"Scratch overflow adding %s",token,*non_rr);
				return -1;
			}
			rr->s_field[rr->s_index++] = (u_int8_t) token[token_index++];
		}
	}

	/* Check string length (to see if it is too long for DNS */

	if (rr->s_index-length_index+1 > 255)
	{
		SKIP_REST_OF_LINE (temptoken);
		SET_PARSE_ERROR(errors,"String %s is too long for DNS",token,*non_rr);
		return -1;
	}

	/* Fill in the length of this string */

	if (length_index < rr->s_index-1) /* We found characters! */
	{
		rr->s_field[length_index] = (u_int8_t) (rr->s_index-length_index-1);
	}
	return rr->s_index-length_index+1;
}

int parse_type (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int16_t	type_n;
	int			type_h = 0;
	int			success;
	char		temptoken[MAXTOKENLENGTH];

	type_h = NS_PARSE_TYPE(token, &success);

	if (success==FALSE)
	{
		SKIP_REST_OF_LINE (temptoken);
		SET_PARSE_ERROR (errors,"Illegal value for a type '%s'",token,*non_rr);
		return -1;
	}

	if (type_h == ns_t_any)
	{
		SKIP_REST_OF_LINE (temptoken);
		SET_PARSE_ERROR (errors,"Type '%s' not permitted here",token,*non_rr);
		return -1;
	}

	/* otherwise we got the type */
	type_n = htons (type_h);

	memcpy (&rr->s_field[rr->s_index],&type_n,sizeof(u_int16_t));
	rr->s_index += sizeof(u_int16_t);
	return 0;
}

int calc_labels (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	int			field_index = 0;
	u_int8_t	label_count = 0;
	char		temptoken[MAXTOKENLENGTH];

	/*	A label consists of a one byte length and the bytes in it.  Count
		one for each non-zero length, and skip that number of bytes + 1 to
		find the next label.

		This count gives the correct number of labels for the purposes of
		the DNS SIG record.  The count it wants does not include the root
		label that terminates a fully qualified name.
	*/

	while (rr->s_field[field_index])
	{
		if (field_index >= rr->s_index)
		{
			SKIP_REST_OF_LINE (temptoken);
			SET_PARSE_ERROR (errors,"Labels could not be counted","",*non_rr);
			return -1;
		}
		label_count++;
		field_index += rr->s_field[field_index] + 1;
	}

	memcpy (&rr->s_field[rr->s_index],&label_count,sizeof(u_int8_t));
	rr->s_index += sizeof(u_int8_t);
	return label_count;
}
