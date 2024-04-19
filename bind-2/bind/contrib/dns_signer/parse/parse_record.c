/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/parse_record.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

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

int parse_envelope (struct scratch *rr, struct error_data *errors,
						struct name_context *context, char **non_rr)
{
	/*
		The envelope may be of one of the following forms:
			class ttl type
			class type
			ttl class type
			ttl type
			type

	BIND routines to rely upon:

	ns_parse_ttl	- converts the "new" TTL format to an unsigned long (why?
					i dunno), not a u_int32_t

	sym_ston		- looks up a symbol in a symbol table, returns a match
					if applicable and a success flag
					See the above macros for sanity.

	Simplified approach:
		do
			grab word
			see if it is a ttl & we need one -> if so keep it
			see if it is a class & we need one -> if so keep it
			see if it is a type & we need one -> if so keep it
		until we have a type

		put type into scratch area
		if we found a class, convert it to network and put into scratch
		if not, put in the default one (already in network order)
		if we found a ttl, convert it to network and put into scratch
		if not, put in the default one (already in network order)

		done
	*/

	char		buffer[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			ret_val;
	int			found_ttl = FALSE;
	u_long		long_ttl_h; /* Solely for the benefit of ns_parse_ttl */
	u_int32_t	ttl_h;
	u_int32_t	ttl_n = context->nc_min_ttl_n;
	int			found_class = FALSE;
	u_int16_t	class_h;
	u_int16_t	class_n = context->nc_class_n;
	int			found_type = FALSE;
	u_int16_t	type_h;
	u_int16_t	type_n;
	int			unidentified_parsing_object;

	do
	{
		GETWORD (ret_val, errors, buffer, *non_rr,
						"Parsing {ttl,class,type}", RETURN_NEG1);

		unidentified_parsing_object = TRUE;

		if (ns_parse_ttl (buffer, &long_ttl_h)!=-1)
		{
			ttl_h = (u_int32_t) long_ttl_h;
			if (found_ttl)
			{
				/* Error - second TTL? */
				SKIP_REST_OF_LINE (temptoken);
				SET_PARSE_ERROR (errors, "A second ttl '%s' found",
										buffer, *non_rr);
				return -1;
			}

			found_ttl = TRUE;
			unidentified_parsing_object = FALSE;
			continue;
		}

		class_h = NS_PARSE_CLASS (buffer, &ret_val);

		if (ret_val)
		{
			if (found_class)
			{
				/* Error - second class? */
				SKIP_REST_OF_LINE (temptoken);
				SET_PARSE_ERROR (errors, "A second class '%s' found",
										buffer, *non_rr);
				return -1;
			}
			if (class_h == ns_c_any)
			{
				/* Error - ns_c_any not allowed here. */
				SKIP_REST_OF_LINE (temptoken);
				SET_PARSE_ERROR (errors, "Class '%s' not allowed here",
										buffer, *non_rr);
				return -1;
			}
			found_class = TRUE;
			unidentified_parsing_object = FALSE;
			continue;
		}

		type_h = NS_PARSE_TYPE (buffer, &ret_val);

		if (ret_val)
		{
			if (type_h == ns_t_any)
			{
				/* Error - ns_t_any not allowed here. */
				SKIP_REST_OF_LINE (temptoken);
				SET_PARSE_ERROR (errors, "Type '%s' not allowed here",
									buffer, *non_rr);
				return -1;
			}
			found_type = TRUE;
			unidentified_parsing_object = FALSE;
		}

		if (unidentified_parsing_object)
		{
			/* Error - not a type, class, or ttl */
			SKIP_REST_OF_LINE (temptoken);
			SET_PARSE_ERROR (errors, "'%s' is not a ttl, class, or type",
								buffer, *non_rr);
			return -1;
		}
	} while (!found_type);

	/* The envelope format is <16 bit type><16 bit class><32 bit ttl> */

	/*
		We have to have found the type, or else we'd have exited prior to
		this point.
	*/

	type_n = htons (type_h);
	memcpy (&rr->s_field[rr->s_index],&type_n,sizeof (u_int16_t));
	rr->s_index += sizeof (u_int16_t);

	/* Now we need to prepare the class */

	if (found_class) class_n = htons (class_h);
	memcpy (&rr->s_field[rr->s_index],&class_n,sizeof (u_int16_t));
	rr->s_index += sizeof (u_int16_t);

	/* Now we do the ttl */
	
	if (found_ttl) ttl_n = htonl (ttl_h);
	memcpy (&rr->s_field[rr->s_index],&ttl_n,sizeof (u_int32_t));
	rr->s_index += sizeof (u_int32_t);

	return 0;
}

int parse_name (struct scratch *rr, char *name, struct error_data *errors,
						struct name_context *context, char **non_rr)
{
	/*
		This routine uses ns_name_pton (from libbind.a) to convert a
		printed name to a DNS protocol name.  If the name is not
		fully qualified, the origin from context is appended to the
		result.  If the name is "@" then the origin is substituted.

		The returned value is -1 for any error, the protocol length of
		the fully qualified name otherwise
	*/
	int		ret_val;
	int		length;
	char	temptoken[MAXTOKENLENGTH];

	/* Name may be just an '@' or it may be fully/partially qualified. */

	if (!strcmp (name, "@"))
	{
		/* Substitute the origin for the lone "@" */
		length = wire_name_length (context->nc_origin_n);
		memcpy (&rr->s_field[rr->s_index], context->nc_origin_n, length);
	}
	else
	{
		/* Use BIND library code to convert name */
		ret_val = ns_name_pton (name, &rr->s_field[rr->s_index],
											rr->s_length-rr->s_index);
		length = wire_name_length (&rr->s_field[rr->s_index]);

		/* Check for either: error, partially qualified, fully qualified name */
		if (ret_val == -1)
		{
			/* The name is illegally formed. */
			SKIP_REST_OF_LINE (temptoken);
			SET_PARSE_ERROR (errors,"Name illegally formed '%s'",name,*non_rr);
			return -1;
		}
		else if (ret_val == 0)
		{
			/* The name was not fully qualified, need to append origin */
			memcpy (&rr->s_field[rr->s_index+length-1], context->nc_origin_n,
						wire_name_length (context->nc_origin_n));
		}
		/* else if (ret_val == 1) we are done */
	}
	/* Advance the scratch index */
	ret_val = wire_name_length (&rr->s_field[rr->s_index]);
	rr->s_index += ret_val;

	return ret_val;
}

int parse_owner_name (struct scratch *rr, char *name, struct error_data *errors,
						struct name_context *context, char **non_rr)
{
	int length;

	/*
		If the owner name is 'whitespace' then use the previous owner name
		Otherwise it is just another name.  Make sure that previous
		name is updated before returning (on success).
	*/
	if (name[0]==' ' || name[0]=='\t')
	{
		/* Use the previous name */
		length = wire_name_length (context->nc_previous_n);
		memcpy (&rr->s_field[rr->s_index], context->nc_previous_n, length);
		rr->s_index += length;
	}
	else
	{
		/* This name is specified, it may be "new" */
		if ((length=parse_name (rr,name,errors,context,non_rr))==-1) return -1;

		/* Reset the previous name for the next go round */
		memcpy (context->nc_previous_n,&rr->s_field[rr->s_index-length],length);
	}
	return 0;
}

int parse_record (struct scratch *rr, char *first_token,
	struct error_data *errors, struct name_context *context, char **non_rr)
{
	rr->s_index = 0;

	/* First determine owner name */
	if (parse_owner_name (rr, first_token, errors, context, non_rr)==-1)
		return -1;

	/* Next get the remainder of the envelope */
	if (parse_envelope (rr, errors, context, non_rr)==-1)
		return -1;

	/* Now, time to tackle the rdata length and rdata */
	if (parse_rdata (rr, errors, context, non_rr)==-1)
		return -1;

	return 0;
}
