/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/parse_rdata.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
    
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
#include "WKS_n_RFC1700.h"

#define LOC_RDATA_LENGTH	16

int parse_wks_list (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int8_t		bitmap[USHRT_MAX/sizeof(u_int8_t)];
	int				max_bit = -1;
	int				bitmap_length;
	u_int16_t		port;
	int				ret_val;
	char			token[MAXTOKENLENGTH];
	char			temptoken[MAXTOKENLENGTH];
	char			protoname[MAXTOKENLENGTH];
	struct protoent	*protoptr;

	/* Need to get the protocol first */

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting protocol designation", RETURN_NEG1);

	if ((protoptr = getprotobyname(token))==NULL)
	{
		SKIP_REST_OF_LINE (temptoken);
		SET_PARSE_ERROR (errors,"Expected a protocol, got '%s'",token,*non_rr);
		return -1;
	}
	strcpy (protoname, token);

	/* Add the one byte protocol field to the RR */

	rr->s_field[rr->s_index++] = (u_int8_t) protoptr->p_proto;

	SETCLEAR(bitmap);
	do
	{
		/* Get the next type off the line */
		GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting service designation", RETURN_NEG1);

		if (ret_val == GW_EOLN) break;

		if ((ret_val=getservicenumber(token, protoname, &port))==-1)
			/* See if token is a given in numeric form */
			ret_val = to_u_int16_t (token, &port);

		/* Add the type to the bit map, keep track of the maximum one */
		if (ret_val != -1)
		{
			/* The token resolved to a type */
			SETON (bitmap, port);
			max_bit = (max_bit < (int) port) ? (int) port : max_bit;
		}
		else
		{
			/* The token is not a valid type */
			SKIP_REST_OF_LINE (temptoken);
			SET_PARSE_ERROR(errors,"Expected service, got '%s'",token,*non_rr);
			return -1;
		}
	} while (TRUE);

	/* Put the bit map into the scratch area */
	bitmap_length = (max_bit > -1) ? (max_bit / CHAR_BIT)+1 : 0;

	if (rr->s_index+bitmap_length > rr->s_length)
	{
		SKIP_REST_OF_LINE (temptoken);
		SET_PARSE_ERROR(errors,"Scratch overflow processing WKS","",*non_rr);
		return -1;
	}

	memcpy (&rr->s_field[rr->s_index], bitmap, bitmap_length);
	rr->s_index += bitmap_length;

	return 0;
}

int parse_ipv4address(struct scratch *rr, char *token,
		struct error_data *errors, struct name_context *context, char **non_rr)
{
	/*
		Uses libbind.a's inet_aton to perform the ascii to binary conversion.
		All that is needed is to explain the error, make sure there is room
		in the scratch area, and place it there.
	*/

	struct in_addr	address;
	char			temptoken[MAXTOKENLENGTH];

	if (inet_aton (token, &address)==0)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Ill-formatted IPv4 address '%s'",token,*non_rr);
		return -1;
	}

	if (rr->s_index+sizeof(struct in_addr) > rr->s_length)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Scratch overflow - IPv4 address","",*non_rr);
		return -1;
	}

	memcpy (&rr->s_field[rr->s_index], &address, sizeof(struct in_addr));
	rr->s_index += sizeof(struct in_addr);

	return 0;
}

u_int8_t xdigit2value (char	c)
{
	if (isdigit (c))
		return (c - '0');
	else if (islower (c))
		return (c - 'a' + 10);
	else
		return (c - 'A' + 10);
}


int parse_ia5_string(struct scratch *rr, char *token,
		struct error_data *errors, struct name_context *context, char **non_rr)
{
	int			t_index = 0;
	char		temptoken[MAXTOKENLENGTH];

	while (t_index < strlen (token))
	{
		if (token[t_index]=='.')
		{
			t_index++;
		}
		else if (isdigit (token[t_index]))
		{
			if (rr->s_index == rr->s_length)
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors,"Scratch overflow - hex string",
								"",*non_rr);
				return -1;
			}
			rr->s_field[rr->s_index++] = token[t_index++];
		}
		else
		{
			/* Not an acceptable character */
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors,"Invalid digit in %s",token,*non_rr);
			return -1;
		}
	}

	return 0;
}

int parse_hex_string(struct scratch *rr, char *token,
		struct error_data *errors, struct name_context *context, char **non_rr)
{
	int			t_index = 0;
	int			nybble_number = 0;
	char		first_nybble;
	char		temptoken[MAXTOKENLENGTH];

	if (token[0] == '0' && tolower(token[1]) == 'x') t_index = 2;

	while (t_index < strlen (token))
	{
		if (token[t_index]=='.')
		{
			t_index++;
		}
		else if (isxdigit (token[t_index]))
		{
			if (nybble_number == 0)
			{
				first_nybble = token[t_index];
			}
			else
			{
				if (rr->s_index+1 > rr->s_length)
				{
					SKIP_REST_OF_LINE(temptoken);
					SET_PARSE_ERROR(errors,"Scratch overflow - hex string",
									"",*non_rr);
					return -1;
				}
				rr->s_field[rr->s_index]  = xdigit2value(first_nybble) << 4;
				rr->s_field[rr->s_index] += xdigit2value(token[t_index]);
				rr->s_index++;
			}

			t_index++;
			nybble_number = 1 - nybble_number;
		}
		else
		{
			/* Not an acceptable character */
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors,"Invalid hex character in %s",token,*non_rr);
			return -1;
		}
	}

	if (nybble_number == 1)
	{
		/* Odd number of hex digits */
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Odd number of hex digits","",*non_rr);
		return -1;
	}

	return 0;
}

#define	FIELD_MANDATORY	0
#define FIELD_OPTIONAL	1

int parse_base64 (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, int field_flag)
{
	int			ret_val;
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	char		scratch_base64[65536];
	int			binary_length;

	scratch_base64[0] = '\0';

	do
	{
		/* Get the next token off the line */

		GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting base 64 value", RETURN_NEG1);

		if (ret_val == GW_EOLN) break;

		/* Watch for overflows */
		if (strlen(scratch_base64)+strlen(token) > 65534)
		{
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors, "Scratch overflow - B64 tokens","",*non_rr);
			return -1;
		}

		strcat (scratch_base64, token);
	} while (TRUE);

	/* The entire base64 encoded string is ready for conversion */
	binary_length = b64_pton (scratch_base64, &rr->s_field[rr->s_index], 
									rr->s_length - rr->s_index);

	if (binary_length == -1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Error decoding base64 string","",*non_rr);
		return -1;
	}

	if (binary_length == 0 && field_flag == FIELD_MANDATORY)
	{
		SET_PARSE_ERROR(errors, "Base64 string missing","",*non_rr);
		return -1;
	}

	rr->s_index += binary_length;
	return 0;
}

int parse_nxt_list (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int8_t	bitmap[(ns_t_max+1)/sizeof(u_int8_t)];
	int			max_bit = -1;
	int			bitmap_length;
	u_int16_t	nxt_type;
	int			ret_val;
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];

	SETCLEAR(bitmap);

	do
	{
		/* Get the next type off the line */

		GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting RR type designation", RETURN_NEG1);

		if (ret_val == GW_EOLN) break;

		/* Add the type to the bit map, keep track of the maximum one */

		nxt_type = NS_PARSE_TYPE (token, &ret_val);

		if (ret_val)
		{
			/* The token resolved to a type */
			SETON (bitmap, nxt_type);
			max_bit = max_bit<((int)nxt_type)?(int)nxt_type:max_bit;
		}
		else
		{
			/* The token is not a valid type */
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors,"Expected a type, got '%s'",token,*non_rr);
			return -1;
		}
	} while (TRUE);

	/* Put the bit map into the scratch area */
	bitmap_length = (max_bit > -1) ? (max_bit / CHAR_BIT)+1 : 0;

	if (rr->s_index+bitmap_length > rr->s_length)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Scratch overflow - NXT","",*non_rr);
		return -1;
	}

	memcpy (&rr->s_field[rr->s_index], bitmap, bitmap_length);
	rr->s_index += bitmap_length;

	return 0;
}

int parse_soa_fields (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	int			ret_val;
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting serial number", RETURN_NEG1);

	if (parse_datetime(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting refresh interval", RETURN_NEG1);

	if (parse_ttl(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting retry interval", RETURN_NEG1);

	if (parse_ttl(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting expiration interval", RETURN_NEG1);

	if (parse_ttl(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting miniumum ttl", RETURN_NEG1);

	if (parse_ttl(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting end of line", RETURN_NEG1);

	if (ret_val==GW_WORD)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Found '%s' instead of eoln",token,*non_rr);
		return -1;
	}

	/*
		Since we now have the zone minimum TTL and this record abides
		by it, we need to go backwards now and insert it into the
		envelope of this record and into the context structure.
	*/

	if (context->nc_min_ttl_n == 0)
	memcpy (&context->nc_min_ttl_n, &rr->s_field[rr->s_index-sizeof(u_int32_t)],
					sizeof(u_int32_t));

	memcpy (&rr->s_field[wire_name_length(rr->s_field)+2*sizeof(u_int16_t)],
					&context->nc_min_ttl_n, sizeof(u_int32_t));

	/*
		We can also now set the context class (if it hasn't been set yet)
		value.  If either the class value in the envelope and/or in
		context are unset (0), then both are set to ns_c_in.
	*/

	{
		u_int16_t zero = 0;
		int class_in_envelope = wire_name_length(rr->s_field)+sizeof(u_int16_t);
		u_int16_t class_n = htons (ns_c_in);

		if (memcmp(&rr->s_field[class_in_envelope],&zero,sizeof(u_int16_t))==0)
			memcpy (&rr->s_field[class_in_envelope],&class_n,sizeof(u_int16_t));

		if (context->nc_class_n == 0)
			context->nc_class_n = class_n;
	}

	return 0;
}

int work_type1rr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		Type 1 RR format
			domain name
			domain name again, 4 numbers and a ttl - if SOA
			rr type list - if NXT
	*/
	int			ret_val;
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting domain name", RETURN_NEG1);

	if (parse_name(rr,token,errors,context,non_rr)==-1) return -1;

	if (type_h==ns_t_soa || type_h==ns_t_minfo || type_h==ns_t_rp)
	{
		GETWORD (ret_val, errors, token, *non_rr,
				"Expecting second domain name", RETURN_NEG1);

		if (parse_name(rr,token,errors,context,non_rr)==-1) return -1;
	}

	if (type_h==ns_t_soa)
		return parse_soa_fields(rr,errors,context,non_rr);
	else if (type_h==ns_t_nxt)
		return parse_nxt_list (rr,errors,context,non_rr);

	GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting end of line", RETURN_NEG1);

	if (ret_val==GW_WORD)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Found '%s' instead of eoln", token, *non_rr);
		return -1;
	}
	return 0;
}

int work_type2rr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		Type 2 formats:
				<16u><16u> --> SRV only
				<16u><dname>
				<dname>    --> PX only (ie: 16u 16u 16u dname dname)
	*/
	int			ret_val;
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];

	if (type_h==ns_t_srv)
	{
		GETWORD (ret_val, errors, token, *non_rr,
				"Expecting first of three 16 bit integers", RETURN_NEG1);

		if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;

		GETWORD (ret_val, errors, token, *non_rr,
				"Expecting second of three 16 bit integers", RETURN_NEG1);

		if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;

		GETWORD (ret_val, errors, token, *non_rr,
				"Expecting third of three 16 bit integers", RETURN_NEG1);

		if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;
	}
	else
	{
		GETWORD (ret_val, errors, token, *non_rr,
				"Expecting a 16 bit integer", RETURN_NEG1);

		if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;
	}

	/* Time for a domain name */

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting a domain name", RETURN_NEG1);

	if (parse_name(rr,token,errors,context,non_rr)==-1) return -1;

	/* The PX record has two domain names */
	if (type_h==ns_t_px)
	{
		GETWORD (ret_val, errors, token, *non_rr,
				"Expecting a second domain name", RETURN_NEG1);

		if (parse_name(rr,token,errors,context,non_rr)==-1) return -1;
	}

	/* We should be at the end of the line by now. */

	GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting end of line", RETURN_NEG1);

	if (ret_val==GW_WORD)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Found '%s' instead of eoln",token,*non_rr);
		return -1;
	}
	
	return 0;
}

int work_type3rr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		Legal formats:
			hinfo :	<str><str>
			txt :	<str>* (Zero or more)
			x25 :	<str>
			isdn :	<str>[<str>]
	*/
	char	token[MAXTOKENLENGTH];
	char	temptoken[MAXTOKENLENGTH];
	char	*errmsg1 = "";
	char	*errmsg2 = "";
	int		ret_val;

	/* initialize the error messages */
	switch (type_h)
	{
		case ns_t_isdn:
			errmsg1 = "Expecting an ISDN address";
			errmsg2 = "Expecting a ISDN subaddress";
			break;
		case ns_t_x25:
			errmsg1 = "Expecting an X.25 address";
			break;
		case ns_t_hinfo:
		case ns_t_txt:
			errmsg1 = "Expecting a character string";
			errmsg2 = "Expecting a character string";
			break;
	}

	/* get the first field {HINFO, ISDN, TXT, X25} */
	GETWORD(ret_val, errors, token, *non_rr, errmsg1, RETURN_NEG1);

	if (parse_string(rr, token, errors, context, non_rr) == -1) return -1;

	/* get additional field {HINFO} */

	if (type_h == ns_t_hinfo)
	{
		GETWORD(ret_val, errors, token, *non_rr, errmsg2, RETURN_NEG1);

		if (parse_string(rr, token, errors, context, non_rr) == -1) return -1;
	}

	/* get additional field(s) {ISDN, TXT} */

	if (type_h == ns_t_isdn || type_h == ns_t_txt)
	{
		do
		{
			GETWORDorEOLN(ret_val,errors,token,*non_rr,errmsg2,RETURN_NEG1);

			if (ret_val == GW_EOLN) return 0;

			if (parse_string(rr,token,errors,context,non_rr) == -1) return -1;
		} while (type_h != ns_t_isdn); /* Only once around for ISDN */
	}

	/* Should be at the end of the line by now */

	GETWORDorEOLN(ret_val,errors,token,*non_rr,"Expected eoln", RETURN_NEG1);

	if (ret_val == GW_WORD)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Found '%s' instead of end of line",
						token, *non_rr);
		return -1;
	}
	return 0;
}

int work_type4rr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		Legal formats
			<ipv4address>, if wks, followed by a <wks service list>
	*/

	int			ret_val;
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting an IPv4 address", RETURN_NEG1);

	if (parse_ipv4address(rr,token,errors,context,non_rr)==-1) return -1;

	if (type_h==ns_t_wks)
	{
		if (parse_wks_list (rr,errors,context,non_rr)==-1) return -1;
	}
	else
	{
		/* Make sure we ended up at an EOLN. */
		GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting end of line", RETURN_NEG1);

		if (ret_val==GW_WORD)
		{
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors,"Found '%s' instead of eoln",token,*non_rr);
			return -1;
		}
	}
	return 0;
}

int work_type5rr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	int			ret_val;
	char		token[MAXTOKENLENGTH];

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting a hexadecimal string", RETURN_NEG1);

	if (parse_hex_string(rr,token,errors,context,non_rr)==-1) return -1;

	while (ret_val==GW_WORD)
	{
		GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting continuation of a hex string", RETURN_NEG1);

		if (ret_val != GW_WORD) break;

		if (parse_hex_string(rr,token,errors,context,non_rr)==-1)
			return -1;
	}

	return 0;
}

void replace_cert_menomic (char *token)
{
	if (strcasecmp (token, "unknown")==0)
		strcpy (token, "0");
	else if (strcasecmp (token, "pkix")==0)
		strcpy (token, "1");
	else if (strcasecmp (token, "spki")==0)
		strcpy (token, "2");
	else if (strcasecmp (token, "pgp")==0)
		strcpy (token, "3");
	else if (strcasecmp (token, "url")==0)
		strcpy (token, "253");
	else if (strcasecmp (token, "oid")==0)
		strcpy (token, "254");
}

void replace_alg_mnemonic (char *token)
{
	if (strncasecmp (token, "rsaref", 3)==0)
		strcpy (token, "1");
	else if (strncasecmp (token, "dh", 2)==0)
		strcpy (token, "2");
	else if (strncasecmp (token, "dss", 2)==0)
		strcpy (token, "3");
	else if (strcasecmp (token, "private")==0)
		strcpy (token, "254");
}

void replace_protocol_menomic (char *token)
{
	if (strcasecmp (token, "none")==0)
		strcpy (token, "0");
	else if (strcasecmp (token, "tls")==0)
		strcpy (token, "1");
	else if (strcasecmp (token, "email")==0)
		strcpy (token, "2");
	else if (strcasecmp (token, "dnssec")==0)
		strcpy (token, "3");
	else if (strcasecmp (token, "ipsec")==0)
		strcpy (token, "4");
	else if (strcasecmp (token, "all")==0)
		strcpy (token, "255");
	else if (strcasecmp (token, "any")==0)
		strcpy (token, "255");
}

int work_sigrr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		The binary format of this is:
			<16 bit> - type covered
			< 8 bit> - algorithm
			< 8 bit> - labels (not seen in ASCII version)
			<32 bit> - signature expiration
			<32 bit> - time signed
			<16 bit> - key footprint
			<base64> - signature material 
	*/

	char		token[MAXTOKENLENGTH];
	int  ret_val;

	GETWORD(ret_val,errors,token,*non_rr,"Expected type covered",RETURN_NEG1);
	if (parse_type(rr, token, errors, context, non_rr) == -1) return -1;

	GETWORD(ret_val,errors,token,*non_rr,"Expected algorithm",RETURN_NEG1);
	replace_alg_mnemonic (token);
	if (parse_8u(rr, token, errors, context, non_rr) == -1) return -1;

	/* calculate label field */
	if (calc_labels(rr, token, errors, context, non_rr) == -1) return -1;

	GETWORD(ret_val,errors,token,*non_rr,"Expected TTL",RETURN_NEG1);
	if (parse_ttl(rr, token, errors, context, non_rr) == -1) return -1;

	GETWORD(ret_val,errors,token,*non_rr,"Expected sig expiration",RETURN_NEG1);
	if (parse_datetime(rr, token, errors, context, non_rr) == -1) return -1;

	GETWORD(ret_val,errors,token,*non_rr,"Expected time signed",RETURN_NEG1);
	if (parse_datetime(rr, token, errors, context, non_rr) == -1) return -1;

	GETWORD(ret_val,errors,token,*non_rr,"Expected key footprint",RETURN_NEG1);
	if (parse_16u(rr, token, errors, context, non_rr) == -1) return -1;

	GETWORD(ret_val,errors,token,*non_rr,"Expected signer's name",RETURN_NEG1);
	if (parse_name(rr, token, errors, context, non_rr) == -1) return -1;

	/* the following will take us to the end of the line */
	return parse_base64(rr,errors,context,non_rr, FIELD_MANDATORY);	
}

void set_flags_value (int *flags, char *mnemonic, int length)
{
	if (strncasecmp (mnemonic, "NOCONF", length)==0)
		*flags |= 0x4000;
	else if (strncasecmp (mnemonic, "NOAUTH", length)==0)
		*flags |= 0x8000;
	else if (strncasecmp (mnemonic, "NOKEY", length)==0)
		*flags |= 0xC000;
	else if (strncasecmp (mnemonic, "EXTEND", length)==0)
		*flags |= 0x1000;
	else if (strncasecmp (mnemonic, "USER", length)==0)
		*flags &= 0xFCFF;
	else if (strncasecmp (mnemonic, "ZONE", length)==0)
	{
		*flags &= 0xFCFF;
		*flags |= 0x0100;
	}
	else if (strncasecmp (mnemonic, "HOST", length)==0)
	{
		*flags &= 0xFCFF;
		*flags |= 0x0200;
	}
	else if (strncasecmp (mnemonic, "NTYP3", length)==0)
		*flags |= 0x0300;
	else if (strncasecmp (mnemonic, "FLAG2", length)==0)
		*flags |= 0x2000;
	else if (strncasecmp (mnemonic, "FLAG4", length)==0)
		*flags |= 0x0800;
	else if (strncasecmp (mnemonic, "FLAG5", length)==0)
		*flags |= 0x0400;
	else if (strncasecmp (mnemonic, "FLAG8", length)==0)
		*flags |= 0x0080;
	else if (strncasecmp (mnemonic, "FLAG9", length)==0)
		*flags |= 0x0040;
	else if (strncasecmp (mnemonic, "FLAG10", length)==0)
		*flags |= 0x0020;
	else if (strncasecmp (mnemonic, "FLAG11", length)==0)
		*flags |= 0x0010;
	else if (strncasecmp (mnemonic, "SIG0", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0000;
	}
	else if (strncasecmp (mnemonic, "SIG1", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0001;
	}
	else if (strncasecmp (mnemonic, "SIG2", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0002;
	}
	else if (strncasecmp (mnemonic, "SIG3", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0003;
	}
	else if (strncasecmp (mnemonic, "SIG4", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0004;
	}
	else if (strncasecmp (mnemonic, "SIG5", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0005;
	}
	else if (strncasecmp (mnemonic, "SIG6", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0006;
	}
	else if (strncasecmp (mnemonic, "SIG7", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0007;
	}
	else if (strncasecmp (mnemonic, "SIG8", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0008;
	}
	else if (strncasecmp (mnemonic, "SIG9", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x0009;
	}
	else if (strncasecmp (mnemonic, "SIG10", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x000A;
	}
	else if (strncasecmp (mnemonic, "SIG11", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x000B;
	}
	else if (strncasecmp (mnemonic, "SIG12", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x000C;
	}
	else if (strncasecmp (mnemonic, "SIG13", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x000D;
	}
	else if (strncasecmp (mnemonic, "SIG14", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x000E;
	}
	else if (strncasecmp (mnemonic, "SIG15", length)==0)
	{
		*flags &= 0xFFF0; *flags |= 0x000F;
	}
}

void replace_key_flag_mnemonic (char *token)
{
	int the_null = strlen(token);
	int begin = 0;
	int end = 0;
	int flags_value = 0;

	while (end < the_null)
	{
		while (token[end] != '|' && end < the_null) end++;
	
		/* mnemonic is in token [begin..end-1] */

		if (end > begin)
			set_flags_value (&flags_value, &token[begin], end-begin);

		begin = end+1;
		end++;
	}

	if (flags_value != 0)
		sprintf (token, "0x%x", flags_value);
	
}

int work_keyrr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		The ascii format of this is:
			<16 bit> - flags        --> to be changed in future
			< 8 bit> - protocol     --> protocols for this key
			< 8 bit> - algorithm    --> algorithm for key
			<base64> - key material --> key itself (optional field for NULL)

			<base64> is a concatenated sequence of tokens until the end of line
	*/
	int			ret_val;
	char		token[MAXTOKENLENGTH];

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting key flags", RETURN_NEG1);

	replace_key_flag_mnemonic (token);
	if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting protocols", RETURN_NEG1);

	replace_protocol_menomic (token);
	if (parse_8u(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting algorithm", RETURN_NEG1);

	replace_alg_mnemonic (token);
	if (parse_8u(rr,token,errors,context,non_rr)==-1) return -1;

	/* The following will take us to the end of the line */
	return parse_base64(rr,errors,context,non_rr, FIELD_OPTIONAL);
}

int count_ipv8_parts (char *token, int *specified_parts, int *ipv4tail)
{
	int			ipv4dots = 0;
	int			t_index;
	int			variable_part = FALSE;

	*ipv4tail = FALSE;
	*specified_parts = 0;

	for (t_index = 0; t_index < strlen (token); t_index++)
	{
		if (token[t_index] == ':')
		{
			if (*ipv4tail == TRUE)
			{
				/* Can't have ipv6 style address after ipv4 */
				return -1;
			}
			else if (t_index == 0 || token[t_index-1] == ':')
			{
				/* This is signifies a variable length 0 part */
				if (variable_part == TRUE)
				{
					/* Already found another one */
					return -1;
				}
				if (t_index != 0)
				{
					variable_part = TRUE;
				}
			}
			else
				(*specified_parts)++;
		}
		else if (token[t_index] == '.')
		{
			if (t_index == 0)
			{
				/* Can't begin address with IPv4 or simply a dot */
				return -1;
			}
			else if (t_index == strlen(token)-1)
			{
				/* Can't end address with a dot */
				return -1;
			}
			else if (token[t_index-1] == '.' || token[t_index-1] == ':')
			{
				/* Can't have a dot follow another separator */
				return -1;
			}
			else
			{
				*ipv4tail = TRUE;
				ipv4dots++;
			}
		}
	}

	if (*ipv4tail == TRUE)
	{
		if (ipv4dots != 3)
		{
			/* Not enough parts of the ipv4 address */
			return -1;
		}
		else
			(*specified_parts) += 2;
	}
	else
		(*specified_parts)++;

	if (*specified_parts > 8)
	{
		/* Address is too long */
		return -1;
	}

	if (variable_part == FALSE && *specified_parts < 8)
	{
		/* Address is too short */
		return -1;
	}

	return 0;
}

int convert_ipv8_address (	struct scratch *rr, char *token,
							int specified_parts, int ipv4tail)
{
	int			zeroed_parts = 8 - specified_parts;
	int			parts_converted = 0;
	int			t_index = 0;

	if (rr->s_index+16 > rr->s_length) return -1;

	if (token[0]==':')
	{
		if (token[1]!=':') return -1;

		memset (&rr->s_field[rr->s_index], 0, zeroed_parts * 2);
		rr->s_index += zeroed_parts * 2;
		parts_converted = zeroed_parts;
		t_index = 2;
	}

	while (parts_converted < 8)
	{
		if (ipv4tail && parts_converted == 6)
		{
			/* do ipv4 conversion */
			struct in_addr address;

			if (inet_aton (&token[t_index], &address)==0) return -1;

			memcpy (&rr->s_field[rr->s_index],&address,sizeof(struct in_addr));

			parts_converted = 8;
			rr->s_index += sizeof (struct in_addr);
		}
		else
		{
			/* another ipv6 portion */
			if (token[t_index] == ':')
			{
				/* Variable length zero part */
				memset (&rr->s_field[rr->s_index], 0, zeroed_parts * 2);
				rr->s_index += zeroed_parts * 2;
				parts_converted += zeroed_parts;
				t_index++;
			}
			else
			{
				/* Convert upto 4 hex chars */
				u_int16_t	sh_h = 0;
				u_int16_t	sh_n;

				if (token[t_index+1]=='\0' || token[t_index+1]==':' ||
					token[t_index+2]=='\0' || token[t_index+2]==':' ||
					token[t_index+3]=='\0' || token[t_index+3]==':' ||
					token[t_index+4]=='\0' || token[t_index+4]==':')
				{

					while (token[t_index] != '\0' && token[t_index] != ':')
					{
						sh_h = sh_h << 4;

						if (isxdigit (token[t_index]))
						{
							sh_h += xdigit2value(token[t_index]);
							t_index++;
						}
						else
							return -1;
					}

					sh_n = htons (sh_h);
					memcpy(&rr->s_field[rr->s_index],&sh_n,sizeof(u_int16_t));
					rr->s_index += sizeof(u_int16_t);

					parts_converted++;

					/* Skip separator, increment count */
					if (token[t_index] == '\0')
					{
						if (parts_converted != 8) return -1;
					}
					else
					{
						t_index++;
					}
				}
				else
					return -1;
			}
		}
	}

	return 0;
}

int work_aaaarr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			specified_parts;
	int			ipv4tail;
	int			ret_val;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting an IPv6 address", RETURN_NEG1);

	if (count_ipv8_parts (token, &specified_parts, &ipv4tail) == -1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Malformed address", token, *non_rr);
		return -1;
	}

	if (convert_ipv8_address (rr, token, specified_parts, ipv4tail) == -1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Malformed address", token, *non_rr);
		return -1;
	}

	/* Make sure we ended up at an EOLN. */
	GETWORDorEOLN (ret_val, errors, token, *non_rr,
			"Expecting end of line", RETURN_NEG1);

	if (ret_val==GW_WORD)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Found '%s' instead of eoln",token,*non_rr);
		return -1;
	}

	return 0;
}

char to_int (int *result, char *token)
{
	int t_index = 0;

	*result = atoi (token);

	while (isdigit(token[t_index])) t_index++;

	return token[t_index];
}

char to_loc_char (u_int8_t *size_cm, char *token)
{
	/*
		token is dddddd[m] or dddd.dd[m]
		max number of d's is 10, counting 2 after the decimal point
	*/

	int	t_index = 0;

	(*size_cm) = 0;

	if (!isdigit(token[t_index]) && token[t_index] != '.') /* Then non number */
		return token[t_index];

	while (token[t_index]=='0') t_index++; /* Skip leading zeros. */

	if (isdigit(token[t_index])) /* Found a non-zero number of meters */
	{
		(*size_cm) = (token[t_index++] - '0') << 4; /* Save first digit > 0 */

		while (isdigit(token[t_index]))
		{
			(*size_cm)++; /* Count up powers of 10 */
			t_index++;
		}
	}

	/* At the decimal point; size_cm still may be zero */

	/* No fractional meters if token ends in d\0, d.\0, d.m\0 */

	if (token[t_index] == '.' &&
			tolower(token[t_index+1]) != 'm' && token[t_index+1] != '\0')
	{
		/* The number has fractional meters - which are still whole cm's */

		t_index++;

		/* Tenths of a meter (decimeters, 10's of cm's) */

		if (!isdigit(token[t_index])) return token[t_index]; /* Error by now */

		if ((*size_cm) == 0) /* If we haven't seen a non-zero yet */
		{
			if (token[t_index] != '0')
				(*size_cm) = (token[t_index] - '0') << 4;
		}
		else
			(*size_cm)++;

		/* Hundredths of a meter (cm's) */

		t_index++;

		if (tolower(token[t_index]) == 'm') t_index++;

		if (token[t_index] == '\0')
		{
			if (*size_cm != 0) (*size_cm)++;
			return (token[t_index]);
		}

		if (!isdigit(token[t_index]))
			return token[t_index]; /* Error by now */

		if ((*size_cm) == 0) /* If we haven't seen a non-zero yet */
		{
			if (token[t_index] != '0')
				(*size_cm) = (token[t_index] - '0') << 4;
		}
		else
			(*size_cm)++;

		t_index++;
		if (tolower(token[t_index]=='m')) t_index++;
	}
	else
	{
		if (*size_cm != 0) (*size_cm) += 2;
		if (token[t_index]=='.') t_index++;
		if (tolower(token[t_index]=='m')) t_index++;
	}

	return token[t_index];
}

int parse_min_sec (int *minutes, int *seconds, int *thous_seconds,
					char *direction, struct error_data *errors,
					struct name_context *context, char **non_rr)
{
	/*
		Parse errors to check -
			(if present) minutes 0..59 and followed by null
			(if present) seconds 0..59 and followed by null or period
			(if present) thous_seconds 0..99 and followed by null
			one-char direction value present
	*/
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			ret_val;
	char		ret_char;

	GETWORD (ret_val, errors, token, *non_rr, "Expecting min/sec/direction",
				RETURN_NEG1);

	if (!isdigit(token[0]))
	{
		/* Assume this is supposed to be the direction */
		if (token[1]=='\0')
			*direction = token[0];
		else
		{
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors,"Illegal direction value(%s)",token,*non_rr);
			return -1;
		}
	}

	/* The token must be a minutes of arc value */

	if (to_int (minutes, token) != '\0')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) minutes of arc",token,*non_rr);
		return -1;
	}

	/* Check the range of values (0..59) */

	if (0 > *minutes || *minutes > 59)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Minutes of arc (%s) out of range",
																token,*non_rr);
		return -1;
	}

	/* Now, either ss.ttt or direction is due */

	GETWORD (ret_val, errors, token, *non_rr, "Expecting sec/direction",
				RETURN_NEG1);

	if (!isdigit(token[0]) && token[0]!='.')
	{
		/* Assume this is supposed to be the direction */
		if (token[1]=='\0')
			*direction = token[0];
		else
		{
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors,"Illegal direction value(%s)",token,*non_rr);
			return -1;
		}
	}

	/* The token must be a seconds(.thousandths_of_a_sec) of arc value */

	ret_char = to_int (seconds, token);

	/* Check the range of values (0..59) */

	if (0 > *seconds || *seconds > 59)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Seconds of arc (%s) out of range",
																token,*non_rr);
		return -1;
	}

	if (ret_char != '\0' && ret_char != '.')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) seconds of arc",token,*non_rr);
		return -1;
	}

	if (ret_char == '.')
	{
		/* Then there's a thous_seconds value remaining */

		char *thousandths = (char *) ((int)strchr (token, '.') +  1);

		if (thousandths[0] != '\0')
		{
			if (to_int (thous_seconds, thousandths) != '\0')
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors,"Non-#(%s) thousandths of sec's of arc",
							token,*non_rr);
				return -1;
			}

			/* Check the range of values (0..99) */

			if (0 > *thous_seconds || *thous_seconds > 999)
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors,
							"Thousandths of seconds of arc (%s) out of range",
																token,*non_rr);
				return -1;
			}
		}
	}

	/* Now, the next token has got to be the direction value */

	GETWORD (ret_val, errors, token, *non_rr, "Expecting sec/direction",
				RETURN_NEG1);

	if (!isdigit(token[0]))
	{
		/* Assume this is supposed to be the direction */
		if (token[1]=='\0')
			*direction = token[0];
		else
		{
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors,"Illegal direction value(%s)",token,*non_rr);
			return -1;
		}
	}

	return 0;
}

int parse_loc_latitude (u_int32_t *latitude_001sec, struct error_data *errors,
						struct name_context *context, char **non_rr)
{
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			ret_val;

	int			degrees; /* Must be between 0 and 90 */
	int			minutes = 0; /* Must be between 0 and 59 if degrees != 90 */
	int			seconds = 0; /* Ditto */
	int			thous_seconds = 0; /* Must be between 0 and 99 if deg != 90 */
	char		direction; /* Either 'N' or 'S' */

	GETWORD (ret_val, errors, token, *non_rr, "Expecting degrees", RETURN_NEG1);

	if (to_int (&degrees, token) != '\0')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) degrees latitude",token,*non_rr);
		return -1;
	}

	if (degrees < 0 || 90 < degrees)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Degrees latitude (%s) out of range",
																token,*non_rr);
		return -1;
	}

	/*
		Read the min/sec/thous_sec/direction
		Errors: no direction, multiple-char in direction, negative value
				non-digit in numeric field
	*/

	if (parse_min_sec (&minutes, &seconds, &thous_seconds, &direction, errors,
														context, non_rr) == -1)
		return -1;

	/* Need to test for:
			if degrees = 90 -> min,sec,thous_sec = 0
			direction must be only N or S
	*/

	if (degrees == 90 && (minutes != 0 || seconds != 0 || thous_seconds != 0))
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Illegal degrees latitude value", "", *non_rr);
		return -1;
	}

	if (direction != 'N' && direction != 'S')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Illegal latitude direction value", "", *non_rr);
		return -1;
	}

	thous_seconds += 1000 * (degrees * 3600 + minutes * 60 + seconds);

	if (direction == 'N')
		*latitude_001sec = 2147483648u + thous_seconds;
	else
		*latitude_001sec = 2147483648u - thous_seconds;

	return 0;
}

int parse_loc_longitude (u_int32_t *longitude_001sec, struct error_data *errors,
						struct name_context *context, char **non_rr)
{
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			ret_val;

	int			degrees; /* Must be between 0 and 180 */
	int			minutes = 0; /* Must be between 0 and 59 if degrees != 180 */
	int			seconds = 0; /* Ditto */
	int			thous_seconds = 0; /* Must be between 0 and 99 if deg != 180 */
	char		direction; /* Either 'W' or 'E' */

	GETWORD (ret_val, errors, token, *non_rr, "Expecting degrees", RETURN_NEG1);

	if (to_int (&degrees, token) != '\0')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) degrees longitude",
															token,*non_rr);
		return -1;
	}

	if (degrees < 0 || 180 < degrees)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Degrees longitude (%s) out of range",
																token,*non_rr);
		return -1;
	}

	/*
		Read the min/sec/thous_sec/direction
		Errors: no direction, multiple-char in direction, negative value
				non-digit in numeric field
	*/

	if (parse_min_sec (&minutes, &seconds, &thous_seconds, &direction, errors,
														context, non_rr) == -1)
		return -1;

	/* Need to test for:
			if degrees = 180 -> min,sec,thous_sec = 0
			direction must be only W or E
	*/

	if (degrees == 180 && (minutes != 0 || seconds != 0 || thous_seconds != 0))
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Illegal degrees longitude value", "", *non_rr);
		return -1;
	}

	if (direction != 'W' && direction != 'E')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Illegal longitude direction value", "",*non_rr);
		return -1;
	}

	thous_seconds += 1000 * (degrees * 3600 + minutes * 60 + seconds);

	if (direction == 'E')
		*longitude_001sec = 2147483648u + thous_seconds;
	else
		*longitude_001sec = 2147483648u - thous_seconds;

	return 0;
}

char to_u_int (u_int32_t *value, char *token, int *range_flag)
{
	/*
		Format of token -
			##...##[.[#[#]]]['m']'\0'

			range of legal values:
				42849672.95 m = 4284967295
				 -100000.00 m =  -10000000
	*/

	/*
		Because neither an int nor u_int can handle all of the
		cases of this, the code has to treat the input data in
		two code segments - one for positive and another for
		negative cases.
	*/

	char		*decimal = strchr (token, '.');
	char		*meters = strchr (token, 'm');
	int			t_index = 0;

	*range_flag = 0;

	if (meters == NULL) meters = strchr (token, 'M');

	if (decimal != NULL && meters != NULL && meters > decimal + 3)
	{
		/* The precision isn't right */
		if (!isdigit(decimal[1])) return decimal[1];
		if (!isdigit(decimal[2])) return decimal[2];
		return decimal[3];
	}

	if (decimal != NULL && meters==NULL && strlen (decimal) > 3)
		return decimal[3];

	if (token[0]=='-')
	{
		int			temp_value;

		temp_value =  -(atoi (&token[1]) * 100);

		if (decimal && isdigit(decimal[1]))
		{
			temp_value -= (decimal[1]-'0') * 10;

			if (isdigit(decimal[2]))
				temp_value -= decimal[2]-'0';
		}

		if (-10000000 > temp_value)
		{
			*range_flag = -1;
		}

		*value = temp_value + 10000000; /* Add for GPS-like spheroid */
	}
	else
	{
		u_int32_t	temp_value;

		temp_value = atoi (token) * 100;

		if (decimal && isdigit(decimal[1]))
		{
			temp_value += (decimal[1]-'0') * 10;

			if (isdigit(decimal[2]))
				temp_value += (decimal[2]-'0');
		}

		if (4284967295u < temp_value)
		{
			*range_flag = -1;
		}

		*value = temp_value + 10000000; /* Add for GPS-like spheroid */
	}

	t_index = 0;
	if (token[t_index]=='-') t_index++;
	while (isdigit(token[t_index])) t_index++;
	if (token[t_index]=='.') t_index++;
	while (isdigit(token[t_index])) t_index++;
	if (token[t_index]=='m' || token[t_index]=='M') t_index++;
	return token[t_index];
}

int parse_loc_rest (	u_int32_t *altitude_cm, u_int8_t *size_cm,
						u_int8_t *horiz_pre_cm, u_int8_t *vert_pre_cm,
						struct error_data *errors, struct name_context *context,
						char **non_rr)
{
	/*
		To be parsed:
			<alt> ["m"] [<size> ["m"] [<hp> ["m"] [<vp> ["m"]]]] & eoln
	*/

	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			ret_val;
	int			range_flag;

	/* Get altitude and optionally the "m" */

	GETWORD (ret_val,errors,token,*non_rr,"Expecting altitude",RETURN_NEG1);

	if (to_u_int (altitude_cm, token, &range_flag) != '\0')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) altitude", token,*non_rr);
		return -1;
	}

	if (range_flag == -1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Altitude (%s) out of range", token,*non_rr);
		return -1;
	}

	/* From here on out, everything is optional */
	/* Begin with size, then horizontal precision and vertical precision */

	GETWORDorEOLN (ret_val,errors,token,*non_rr,"Expecting size", RETURN_NEG1);

	if (ret_val == GW_EOLN) return 0;

	if (to_loc_char (size_cm, token) != '\0')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) size", token,*non_rr);
		return -1;
	}

	/* Next is horizontal precision and then vertical precision */

	GETWORDorEOLN (ret_val, errors, token, *non_rr,
					"Expecting horizontal precision", RETURN_NEG1);

	if (ret_val == GW_EOLN) return 0;

	if (to_loc_char (horiz_pre_cm, token) != '\0')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) horizontal precision",
																token,*non_rr);
		return -1;
	}

	/* Finally, the vertical precision */

	GETWORDorEOLN (ret_val, errors, token, *non_rr,
					"Expecting vertical precision", RETURN_NEG1);

	if (ret_val == GW_EOLN) return 0;

	if (to_loc_char (vert_pre_cm, token) != '\0')
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Non-number(%s) vertical precision",
																token,*non_rr);
		return -1;
	}

	return 0;
}

void parse_assemble_loc_rdata (	struct scratch	*rr,
								u_int8_t		version,
								u_int32_t		latitude_001sec,
								u_int32_t		longitude_001sec,
								u_int32_t		altitude_cm,
								u_int8_t		size_cm,
								u_int8_t		horiz_pre_cm,
								u_int8_t		vert_pre_cm)
{
	u_int32_t	long_n;

	rr->s_field[rr->s_index++] = version;
	rr->s_field[rr->s_index++] = size_cm;
	rr->s_field[rr->s_index++] = horiz_pre_cm;
	rr->s_field[rr->s_index++] = vert_pre_cm;

	long_n = htonl(latitude_001sec);
	memcpy (&rr->s_field[rr->s_index], &long_n, sizeof (u_int32_t));
	rr->s_index += sizeof (u_int32_t);

	long_n = htonl(longitude_001sec);
	memcpy (&rr->s_field[rr->s_index], &long_n, sizeof (u_int32_t));
	rr->s_index += sizeof (u_int32_t);

	long_n = htonl(altitude_cm);
	memcpy (&rr->s_field[rr->s_index], &long_n, sizeof (u_int32_t));
	rr->s_index += sizeof (u_int32_t);
}


int work_locrr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		Network RDATA format

			u_int8_t	version		-	Currently must be zero
			u_int8_t	size		cm	Encoded : Mantissa/Exponent in nybbles
			u_int8_t	horiz pre	cm	Encoded : Mantissa/Exponent in nybbles
			u_int8_t	vert pre	cm	Encoded : Mantissa/Exponent in nybbles
			u_int32_t	latitude	thousandths of a second arc - 2to31 = 0deg.
			u_int32_t	longitude	thousandths of a second arc - 2to31 = 0deg.
			u_int32_t	altitude	cm - 100km = sea level

		Presentation RDATA format

			<d_lat> [<m_lat> [<s_lat>]] {"N"|"S"} \
			<d_long> [<m_long> [<s_long>]] {"E"|"W"} \
			<alt> ["m"] [<size> ["m"] [<hp> ["m"] [<vp> ["m"]]]]

			"variable"	range							default
			<d_lat> :	0 .. 90								-
			<d_long> :	0 .. 180							-
			<m_*> :		0 .. 59								0
			<d_*> :		0 .. 59.999							0
			<alt> :		-100000.00 .. 42849672.95 by .01	-
			<siz> :		0 .. 90000000.00					1m
			<hp> :		0 .. 90000000.00					10,000m
			<vp> :		0 .. 90000000.00					10m
	*/

	char		temptoken[MAXTOKENLENGTH];
	u_int8_t	version = 0;
	u_int8_t	size_cm = 0x12; /* Default = 1 meter */
	u_int8_t	horiz_pre_cm = 0x16; /* Defalut = 10,000m = 1x10(6) cm */
	u_int8_t	vert_pre_cm = 0x13; /* Defalut = 10m = 1x10(3) cm */
	u_int32_t	latitude_001sec; /* Values range 2147483648 +/- 324000000 */
	u_int32_t	longitude_001sec; /* Values range 2147483648 +/- 648000000 */
	u_int32_t	altitude_cm; /* Sea level is 10,000.00 m = 1000000 */

	if (rr->s_index+LOC_RDATA_LENGTH > rr->s_length)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Scratch overflow - LOC","",*non_rr);
		return -1;
	}
	if (parse_loc_latitude (&latitude_001sec, errors, context, non_rr) == -1)
		return -1;

	if (parse_loc_longitude (&longitude_001sec, errors, context, non_rr) == -1)
		return -1;

	if (parse_loc_rest (&altitude_cm, &size_cm, &horiz_pre_cm, &vert_pre_cm,
							errors, context, non_rr) == -1)
		return -1;

	parse_assemble_loc_rdata (rr, version, latitude_001sec, longitude_001sec,
							altitude_cm, size_cm, horiz_pre_cm, vert_pre_cm);

	return 0;
}

int work_naptrrr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		NAPTR record syntax:
				<16u>		- Order
				<16u>		- Preference
				<str>		- Flags
				<str>		- Service
				<str>		- Regexp
				<dname>		- Replacement
	*/
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			ret_val;

	GETWORD (ret_val,errors,token,*non_rr,"Expecting order",RETURN_NEG1);

	if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val,errors,token,*non_rr,"Expecting preference",RETURN_NEG1);

	if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val,errors,token,*non_rr,"Expecting flags",RETURN_NEG1);

	if (parse_string(rr,token,errors,context,non_rr) == -1) return -1;

	/* Should check flags - but for what? */

	GETWORD (ret_val,errors,token,*non_rr,"Expecting service",RETURN_NEG1);

	if (parse_string(rr,token,errors,context,non_rr) == -1) return -1;

	/* Should check service - but for what? */

	GETWORD (ret_val,errors,token,*non_rr,"Expecting regexp",RETURN_NEG1);

	if (parse_string(rr,token,errors,context,non_rr) == -1) return -1;

	/* Should check regexp - but for what? */

	GETWORD (ret_val,errors,token,*non_rr,"Expecting domain name",RETURN_NEG1);

	if (parse_name(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting end of line", RETURN_NEG1);

	if (ret_val==GW_WORD)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Found '%s' instead of eoln",token,*non_rr);
		return -1;
	}

	return 0;
}

int work_atmarr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		ATMA record syntax:
				<8u>		- Address Kind
				<bits>		- Address
		Masterfile
				hex.hex.hex...hex -> Kind = 0, hex encoded.
                +dddd.ddd.dd.dddd -> Kind = 1, ia5 encoded.
	*/
	char		token[MAXTOKENLENGTH];
	char		temptoken[MAXTOKENLENGTH];
	int			ret_val;

	GETWORD (ret_val,errors,token,*non_rr,"Expecting address",RETURN_NEG1);

	if (token[0]=='+')
	{
		if (rr->s_index == rr->s_length)
		{
			SET_PARSE_ERROR(errors, "Scratch area overflow adding ATMA type",
								"",*non_rr);
			return -1;
		}
		rr->s_field[rr->s_index++] = 1;

		if (parse_ia5_string(rr,&token[1],errors,context,non_rr)==-1) return -1;
	}
	else
	{
		if (rr->s_index == rr->s_length)
		{
			SET_PARSE_ERROR(errors, "Scratch area overflow adding ATMA type",
								"",*non_rr);
			return -1;
		}
		rr->s_field[rr->s_index++] = 0;

		if (parse_hex_string(rr,token,errors,context,non_rr)==-1) return -1;
	}

	GETWORDorEOLN (ret_val, errors, token, *non_rr,
				"Expecting end of line", RETURN_NEG1);

	if (ret_val==GW_WORD)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors,"Found '%s' instead of eoln",token,*non_rr);
		return -1;
	}

	return 0;
}

int work_certrr (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		The ascii format of this is:
			<16 bit> - type
					0 = reserved or unknown
					1 = PKIX
					2 = SPKI
					3 = PGP
					253 = URL Private
					254 = OID Private
			< 8 bit> - key tag
			< 8 bit> - algorithm    --> algorithm for key
			<base64> - cert material
	*/
	int			ret_val;
	char		token[MAXTOKENLENGTH];

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting certificate type", RETURN_NEG1);

	/* Need to cover the memonics too */

	replace_cert_menomic (token);

	if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting key tag", RETURN_NEG1);

	if (parse_16u(rr,token,errors,context,non_rr)==-1) return -1;

	GETWORD (ret_val, errors, token, *non_rr,
				"Expecting algorithm", RETURN_NEG1);

	replace_alg_mnemonic (token);
	if (parse_8u(rr,token,errors,context,non_rr)==-1) return -1;

	/* The following will take us to the end of the line */
	return parse_base64(rr,errors,context,non_rr, FIELD_OPTIONAL);
}

int work_rdata (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr, u_int8_t type_h)
{
	/*
		The parsing of an RR from here on out is heavily type dependent.
		Some types share common formats fortunately.

		The core of this routine is the switch statement based on the
		parameter type_h.  The result will either be

			*rr->s_field[rr->s_index] filled in and 0 returned

		or
			*non_rr filled in with message and errors-> ed_error_value
			set to ERR_PARSEERROR and -1 returned
	*/
	char		temptoken[MAXTOKENLENGTH];

	switch (type_h)
	{
		case ns_t_soa:		case ns_t_minfo:		case ns_t_rp:
		case ns_t_ns:		case ns_t_cname:		case ns_t_mb:
		case ns_t_mr:		case ns_t_ptr:			case ns_t_nxt:
		case ns_t_mg:
			/* NS records for zone sources */
			return work_type1rr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_srv:		case ns_t_rt:			case ns_t_mx:
		case ns_t_afsdb:	case ns_t_px:
			return work_type2rr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_hinfo:	case ns_t_txt:
		case ns_t_x25:		case ns_t_isdn:
			return work_type3rr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_a:		case ns_t_wks:
			/* A records for glue */
			return work_type4rr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_nsap:		case ns_t_eid:			case ns_t_nimloc:
			return work_type5rr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_sig:
			return work_sigrr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_key:
			/* KEY records for signature verification */
			return work_keyrr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_aaaa:
			return work_aaaarr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_loc:
			return work_locrr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_naptr:
			return work_naptrrr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_atma:
			return work_atmarr (rr,errors,context,non_rr,type_h);
			break;

			/* Save the following fragment for some upcoming types */

			/* Haven't yet found the formats documented */
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors, "Type %s format is unknown",
						p_type(type_h), *non_rr);
			return -1;
			break;

		case ns_t_md:		case ns_t_mf:
		case ns_t_nsap_ptr:	case ns_t_gpos:
			/* These are deprecated forms */
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors, "Type %s is deprecated, not parsed",
						p_type(type_h), *non_rr);
			return -1;
			break;

		case ns_t_cert:
			return work_certrr (rr,errors,context,non_rr,type_h);
			break;

		case ns_t_null:
			/* Illegal on input */
			SKIP_REST_OF_LINE(temptoken);
			SET_PARSE_ERROR(errors, "Type %s format is illegal on input",
						p_type(type_h), *non_rr);
			return -1;
			break;

		default:
			/* Unrecognized form */
			SKIP_REST_OF_LINE (temptoken);
			SET_PARSE_ERROR(errors, "Type %s format is not recognized",
						p_type(type_h), *non_rr);
			return -1;
			break;
	}
	SKIP_REST_OF_LINE (temptoken);
	SET_PARSE_ERROR(errors, "Type %s format is not implemented",
					p_type(type_h), *non_rr);
	return -1;
}

int parse_rdata (struct scratch *rr, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	/*
		This routine will assume that another (work_rdata) will perform
		the parsing and return the length of the result.  In here all I
		will do is determine the type (and pass it to work_rdata) and
		stuff the rdata length into the appropriate location (ie right
		behind the envelope - where rr->s_index is pointing upon
		the call.
	*/
	u_int16_t	type_n, type_h;
	u_int16_t	length_n, length_h;
	int			rdata_length_index = rr->s_index;

	memcpy (&type_n, &rr->s_field[wire_name_length(rr->s_field)],
												sizeof(u_int16_t));
	type_h = ntohs (type_n);

	if (rr->s_index + sizeof (u_int16_t) > rr->s_length)
	{
		SET_PARSE_ERROR(errors, "Scratch area overflow adding rdata length",
							"",*non_rr);
		return -1;
	}
	rr->s_index += sizeof (u_int16_t);

	if (work_rdata (rr,errors,context,non_rr,type_h)==-1) return -1;

	length_h = rr->s_index - (rdata_length_index + sizeof(u_int16_t));
	length_n = htons(length_h);
	memcpy (&rr->s_field[rdata_length_index],&length_n,sizeof(u_int16_t));
	return 0;
}
