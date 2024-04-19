/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/master_parse.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

#include "pkg_internal_dns_parse.h"

char	special_chars[256];
int		special_count=0;
int		detailed_debugging = FALSE;

#define SCRATCH_SIZE		256

char *collect_words(char *first)
{
	/*
		Get all of the remaining characters from the current input line,
		and append them to the characters in first, and return a pointer
		to the result.
	*/
	char	ch;
	char	*scratch = (char*) MALLOC(SCRATCH_SIZE);
	char	*temp;
	int		scratch_index = 0;
	int		scratch_length = SCRATCH_SIZE;
	char	*ret_field=0;
	int		ret_length = strlen (first)+1;

	if (scratch == NULL) return NULL;

	do
	{
		ch = get_next_char();
		if (ch=='\n' || ch=='\0') break;

		scratch[scratch_index++] = ch;
		if (scratch_index > scratch_length)
		{
			temp = (char *) MALLOC (scratch_length+SCRATCH_SIZE);
			if (temp==NULL) return NULL;
			memset (temp, 0, scratch_length+SCRATCH_SIZE);
			memcpy (temp, scratch, scratch_length);
			scratch_length += SCRATCH_SIZE;
			FREE (scratch);
		}
	} while (TRUE);

	ret_length += scratch_index + 1; /* +1 for the trailing NULL */

	ret_field = (char*) MALLOC (ret_length);

	if (ret_field == NULL) return NULL;

	strcpy (ret_field, first);
	
	if (scratch_index > 0)
	{
		strcat (ret_field, " ");
		memcpy (&ret_field[strlen(ret_field)], scratch, scratch_index);
	}

	ret_field[strlen(first)+1+scratch_index] = '\0';

	FREE (scratch);

	set_new_line();

	return ret_field;
}

int	work_rr (
		struct error_data *errors,
		struct name_context *context,
		u_int8_t **rr,
		char **non_rr)
{
	char			first_token[MAXTOKENLENGTH];
	int				ret_code;
	int				i;
	struct scratch	scratch;

	/* Memory management clean up */
	if (*rr)
	{
		FREE (*rr);
		*rr = NULL;
	}

	if (*non_rr)
	{
		FREE (*non_rr);
		*non_rr = NULL;
	}

	if ((scratch.s_field = (u_int8_t *) MALLOC(RR_SCRATCH_LENGTH)) == NULL)
	{
		errors->ed_error_value = ERR_OUTOFMEMORY;
		return -1;
	}

	scratch.s_length = RR_SCRATCH_LENGTH;

	/* Source of data is hidden, just use GETWORD() to work my way through it */

	/* Initialize errors to 'nothing set' */
	errors->ed_error_value = ERR_UNSET;

	do
	{
		GETWORDorEOLN (ret_code, errors, first_token, *non_rr, "First word",
						FREE_SCRATCH_UPDATE_ERRORS_AND_RETURN_NEG1);

		if (ret_code == GW_EOLN)
		{
			errors->ed_curr_line++;
			decrement_current_line();
		}
	} while (ret_code == GW_EOLN);

	/* ret_code must be GW_WORD and first_token is the first word in line */
	/*
		If the beginning of the first word is in special_chars,
		then
			get words until GW_EOLN is returned
			problem - storing the words until we MALLOC non_rr
			assemble them into *non_rr
			return ERR_OKBUTNOTRR in error structure, other ancillarys
		else
			take the first word as the name and parse the envelope
			depending on the type value, parse the remainder of the record
			no matter what happens, read tokens up through GW_EOLN
	*/

	/*
		This takes care of all the specially registered characters,
		processing and returning anything that is not an RR.
	*/
	for (i =0; i < special_count; i++)
		if (first_token[0]==special_chars[i])
		{
			/* return with
				all of the upcoming line in *non_rr,
				*rr = NULL (already is),
				errors stating OKBUTNOTRR and update its line count.
			*/
			*non_rr = collect_words (first_token);
			errors->ed_curr_line=errors->ed_curr_line+current_line();
			if (*non_rr)
				errors->ed_error_value = ERR_OKBUTNOTRR;
			else
				errors->ed_error_value = ERR_OUTOFMEMORY;
			FREE (scratch.s_field);
			return -1;
		}

	/* Now we can set about the business of parsing RR's */
	/* Two parts, the envelope, the rdata */

	ret_code = parse_record (&scratch, first_token, errors, context, non_rr);

	/* Final clean up */

	if (ret_code==0)
	{
		if ((*rr = (u_int8_t*) MALLOC (scratch.s_index))==NULL)
		{
			errors->ed_error_value = ERR_OUTOFMEMORY;
			return -1;
		}
		memcpy (*rr, scratch.s_field, scratch.s_index);
	}

	errors->ed_curr_line=errors->ed_curr_line+current_line();
	FREE (scratch.s_field);
	return ret_code;
}

int read_rr (	FILE *master,
		struct error_data *errors,
		struct name_context *context,
		u_int8_t **rr,
		char **non_rr)
{
	setsourcefile (master);
	return work_rr (errors, context, rr, non_rr);
}

int parse_rr (	char *master,
		struct error_data *errors,
		struct name_context *context,
		u_int8_t **rr,
		char **non_rr)
{
	setsourceline (master);
	return work_rr (errors, context, rr, non_rr);
}

void register_special_char (char ch)
{
	special_chars[special_count++] = ch;
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




