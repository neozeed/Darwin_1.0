/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/parse_number.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

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

u_int8_t to_8number (char ch)
{
	if (isdigit(ch)) return (0x0f & ch);
	return (0x0f & ((0x07 & tolower(ch)) + 9));
}

u_int16_t to_16number (char ch)
{
	if (isdigit(ch)) return (0x0f & ch);
	return (0x0f & ((0x07 & tolower(ch)) + 9));
}

u_int32_t to_32number (char ch)
{
	if (isdigit(ch)) return (0x0f & ch);
	return (0x0f & ((0x07 & tolower(ch)) + 9));
}

int to_u_int8_t (char	*token, u_int8_t *value)
{
	int			index = 0;
	u_int8_t	base = 10;

	*value = 0;

	if (strlen(token) >= 2 && token[0]=='0' && token[1]=='x')
	{
		base = 16;
		index = 2;
	}

	/*
		Error condition:
			value x base + digit	> MAX
			value					> (MAX-digit)/base
	*/

	while (token[index])
	{
		if (*value > (UCHAR_MAX-to_8number(token[index]))/base) return -1;
		*value = *value * base + to_8number(token[index]);
		index++;
	}

	return 0;
}

int to_u_int16_t (char	*token, u_int16_t *value)
{
	int			index = 0;
	u_int16_t	base = 10;
	u_int16_t	factor;

	*value = 0;

	if (strlen(token) >= 2 && token[0]=='0' && token[1]=='x')
	{
		base = 16;
		index = 2;
	}

	/*
		Error condition:
			value x base + digit	> MAX
			value					> (MAX-digit)/base
	*/

	while (token[index])
	{
		if ((index==strlen(token)-1) && strchr("kmg",tolower(token[index])))
		{
			factor = tolower(token[index])=='k'?1024:
						tolower(token[index])=='m'?1024*1024:
													1024*1024*1024;
			if (*value > (USHRT_MAX/factor)) return -1;
			*value *= factor;
			index++;
		}
		else
		{
			if (*value > (USHRT_MAX-to_16number(token[index]))/base) return -1;
			*value = *value * base + to_16number(token[index]);
			index++;
		}
	}

	return 0;
}

int to_u_int32_t (char	*token, u_int32_t *value)
{
	int			index = 0;
	u_int32_t	base = 10;
	u_int32_t	factor;

	*value = 0;

	if (strlen(token) >= 2 && token[0]=='0' && token[1]=='x')
	{
		base = 16;
		index = 2;
	}

	/*
		Error condition:
			value x base + digit	> MAX
			value					> (MAX-digit)/base
	*/

	while (token[index])
	{
		if ((index==strlen(token)-1) && strchr("kmg",tolower(token[index])))
		{
			factor = tolower(token[index])=='k'?1024:
						tolower(token[index])=='m'?1024*1024:
													1024*1024*1024;
			if (*value > (UINT_MAX/factor)) return -1;
			*value *= factor;
			index++;
		}
		else
		{
			if (*value > (UINT_MAX-to_32number(token[index]))/base) return -1;
			*value = *value * base + to_32number(token[index]);
			index++;
		}
	}

	return 0;
}

int check_numeric_syntax (struct scratch *rr, char *token,
	struct error_data *errors, struct name_context *context, char **non_rr)
{
	int		i;
	char	temptoken[MAXTOKENLENGTH];

	if (strlen(token) >= 2 && token[0]=='0' && token[1]=='x')
	{
		/*
			The following are legal:
				0x12	0x123	0xabcdef
		*/
		for (i = 2; i < strlen(token); i++)
			if (!isxdigit(token[i]))
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors, "Non-hex character in '%s'",
									token, *non_rr);
				return -1;
			}
	}
	else
	{
		/*
			The following are legal:
				1	123	123k	123m	123g
			But not:
				k	m	g
		*/

		for (i = 0; i < strlen(token)-1; i++)
			if (!isdigit(token[i]))
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors,"Non-numeric charcter in '%s'",
									token, *non_rr);
				return -1;
			} 
		/* Test last character for digit or k, m, or g */
		i = strlen(token)-1;
		if (i > 0)
		{
			if (strchr("0123456789KkMmGg", token[i])==NULL)
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors,"Non-numeric charcter in '%s'",
									token, *non_rr);
				return -1;
			} 
		}
		else if (i == 0)
		{
			if (strchr("0123456789", token[i])==NULL)
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors,"Non-numeric charcter in '%s'",
									token, *non_rr);
				return -1;
			} 
		}

		/*
			If i == -1 then the token was empty. Not sure if that's an error,
			the code isn't designed to ever let this happen. GETWORD() will
			only return a blank token when reading the first of a record,
			which is a domain name, never a number.
		*/
	}
	return 0;
}

int parse_8u (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int8_t	value;
	char		temptoken[MAXTOKENLENGTH];

	if (check_numeric_syntax (rr,token,errors,context,non_rr)==-1) return -1;

	if ((rr->s_index + sizeof(u_int8_t)) > rr->s_length)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Scratch area overflow adding '%s'", token,
						*non_rr);
		return -1;
	}

	/* Convert number to binary and insert */
	if (to_u_int8_t (token, &value)==-1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Illegal value for 8 bits '%s'", token,
						*non_rr);
		return -1;
	}

	rr->s_field[rr->s_index++]=value;
	return 0;
}

int parse_16u (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int16_t	value_h;
	u_int16_t	value_n;
	char		temptoken[MAXTOKENLENGTH];

	if (check_numeric_syntax (rr,token,errors,context,non_rr)==-1) return -1;

	if ((rr->s_index + sizeof(u_int16_t)) > rr->s_length)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Scratch area overflow adding '%s'", token,
							*non_rr);
		return -1;
	}

	/* Convert number to binary and insert */
	if (to_u_int16_t (token, &value_h)==-1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Illegal value for 16 bits '%s'", token,
							*non_rr);
		return -1;
	}

	value_n = htons(value_h);
	memcpy (&rr->s_field[rr->s_index],&value_n,sizeof(u_int16_t));
	rr->s_index += sizeof(u_int16_t);
	return 0;
}

int parse_32u (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int32_t	value_h;
	u_int32_t	value_n;
	char		temptoken[MAXTOKENLENGTH];

	if (check_numeric_syntax (rr,token,errors,context,non_rr)==-1) return -1;

	if ((rr->s_index + sizeof(u_int32_t)) > rr->s_length)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Scratch area overflow adding '%s'", token,
							*non_rr);
		return -1;
	}

	/* Convert number to binary and insert */
	if (to_u_int32_t (token, &value_h)==-1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Illegal value for 32 bits '%s'", token,
							*non_rr);
		return -1;
	}

	value_n = htonl(value_h);
	memcpy (&rr->s_field[rr->s_index],&value_n,sizeof(u_int32_t));
	rr->s_index += sizeof(u_int32_t);
	return 0;
}

time_t mytimegm (struct tm *time)
{
	/* Following code is taken from db_load.c, second half of datetosecs */
	
	time_t				result;
	int					mdays, i;
	static const int	days_per_month[12] =
			{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
 
#define SECS_PER_DAY	((u_int32_t)24*60*60)
#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
 
	result  = time->tm_sec;							/* Seconds */
	result += time->tm_min * 60;					/* Minutes */
	result += time->tm_hour * (60*60);				/* Hours */
	result += (time->tm_mday - 1) * SECS_PER_DAY;	/* Days */
		
	/* Months are trickier. Look without leaping, then leap */
	mdays = 0;
	for (i = 0; i < time->tm_mon; i++)
		mdays += days_per_month[i];
	result += mdays * SECS_PER_DAY;	/* Months */
	if (time->tm_mon > 1 && isleap (1900+time->tm_year))
		result += SECS_PER_DAY; /* Add leapday for this year */
		
	/* First figure years without leapdays, then add them in. */
	/* The loop is slow, FIXME, but simple and accurate. */
	result += (time->tm_year - 70) * (SECS_PER_DAY*365); /* Years */
	for (i = 70; i < time->tm_year; i++)
		if (isleap (1900+i))
			result += SECS_PER_DAY; /* Add leapday for prev year */
		return result;
}

time_t convert_to_seconds(char *token)
{
	struct tm	now;
	char		workfield[8];

	/* Assume the date is in YYYYMMDDHHMMSS format */
	memset (&now, '\0', sizeof(struct tm));
	now.tm_isdst = -1;
	memset (workfield, '\0', 8);
	workfield[0] = token[4];	workfield[1] = token[5];
	now.tm_mon = atoi (workfield) - 1;

	workfield[0] = token[6];	workfield[1] = token[7];
	now.tm_mday = atoi (workfield);

	workfield[0] = token[8];	workfield[1] = token[9];
	now.tm_hour = atoi (workfield);

	workfield[0] = token[10];	workfield[1] = token[11];
	now.tm_min = atoi (workfield);

	workfield[0] = token[12];	workfield[1] = token[13];
	now.tm_sec = atoi (workfield);

	workfield[0] = token[0]; workfield[1] = token[1];
	workfield[2] = token[2]; workfield[3] = token[3];
	now.tm_year = atoi (workfield) - 1900;

	return mytimegm(&now);
}

int parse_datetime (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int32_t	value_n;
	int			i;
	char		temptoken[MAXTOKENLENGTH];

	if (strlen(token)== 14)
	{
		/* Assume the date is in YYYYMMDDHHMMSS format */

		for (i=0; i < 14; i++)
			if (!isdigit(token[i]))
			{
				SKIP_REST_OF_LINE(temptoken);
				SET_PARSE_ERROR(errors, "Expected YYYYMMDDHHMMSS, got %s",
								token, *non_rr);
				return -1;
			}

		value_n = htonl((u_int32_t)convert_to_seconds(token));
		memcpy (&rr->s_field[rr->s_index],&value_n,sizeof(u_int32_t));
		rr->s_index += sizeof(u_int32_t);
		return 0;
	}
	else
		return parse_32u(rr,token,errors,context,non_rr);
}

int parse_ttl (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr)
{
	u_int32_t	ttl_h;
	u_int32_t	ttl_n;
	u_long		ttl_h_long;
	char		temptoken[MAXTOKENLENGTH];

	if (ns_parse_ttl (token, &ttl_h_long)==-1)
	{
		SKIP_REST_OF_LINE(temptoken);
		SET_PARSE_ERROR(errors, "Illegal value for a ttl '%s'", token, *non_rr);
		return -1;
	}

	ttl_h = (u_int32_t) ttl_h_long;
	ttl_n = htonl (ttl_h);

	memcpy (&rr->s_field[rr->s_index],&ttl_n,sizeof(u_int32_t));
	rr->s_index += sizeof(u_int32_t);
	return 0;
}
