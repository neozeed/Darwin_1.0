/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/pkg_internal_dns_parse.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef __pkg_internal_dns_parse_h__
#define __pkg_internal_dns_parse_h__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <arpa/nameser.h>
#ifndef _NETINET_IN_H_
#include <netinet/in.h>
#ifdef __FreeBSD__
/* The following should (have) be(en) defined in /usr/include/netinet/in.h */
struct in6_addr
{
    u_int8_t s6_addr[16];
};
#endif
#endif
#include <resolv.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "iip_support.h"
#include "dns_support.h"
#include "bind_support.h"
#include "dns_parse.h"
#include "parse_get_token.h"

#define RR_SCRATCH_LENGTH	65000
#define MAXTOKENLENGTH		1024

#ifndef TRUE
#define TRUE    1
#endif  

#ifndef FALSE
#define FALSE   0
#endif 

#define NS_PARSE_CLASS(word,success)	sym_ston(__p_class_syms,word,success)
#define NS_PARSE_TYPE(word,success)		sym_ston(__p_type_syms,word,success)

extern int	detailed_debugging;

#define SKIP_REST_OF_LINE(sl_buff) \
{ \
	int		r; \
	do \
	{\
		r=parse_get_token(sl_buff, MAXTOKENLENGTH); \
		if (detailed_debugging) \
			switch(r)\
			{\
				case GW_WORD: printf ("WORD: %s\n", sl_buff);  break; \
				case GW_EOLN: printf ("END OF LINE\n");  break; \
				case GW_EOF: printf ("END OF FILE\n");  break; \
				case GW_ERRCONTINUATION: printf ("CONTINUATION\n");  break; \
				case GW_ERRILLEGALCHAR: printf ("ILLEGAL CHAR\n"); break; \
				case GW_ERRDANGLINGQUOTE:printf("UNTERMINATED QUOTE\n");break; \
				case GW_TRUNC: printf ("TRUNCATED TOKEN\n"); break; \
			}\
	}\
	while(r==GW_WORD || r==GW_ERRILLEGALCHAR || r==GW_TRUNC); \
}

#define SET_PARSE_ERROR(spe_errors,spe_errstring,spe_sub,spe_non_rr) \
	spe_errors->ed_error_value = ERR_PARSEERROR; \
	spe_non_rr = err_msg_sn(spe_errors, spe_errstring, spe_sub);

#define RETURN_NEG1						return -1
#define FREE_SCRATCH_UPDATE_ERRORS_AND_RETURN_NEG1	\
	FREE (scratch.s_field); \
	errors->ed_curr_line=errors->ed_curr_line+current_line(); \
	return -1

/*
	ret_val -> g_rv
	errors	-> g_err
	token	-> g_tkn
	non_rr	-> g_nrr
	"..."	-> g_rsn
	RETURN_NEG1 or FREE...	-> g_return
*/

/*
	This macro is the standard wrapper around a parse_get_token function call.
	It tests for all conditions except for GW_WORD (i.e., success) and
	GW_EOLN (i.e., end of line).  If a bad condition happens, then the
	error recording is done and the return sequence is performed.
*/

#define GETWORDorEOLN(g_rv,g_errs,g_tkn, g_nrr,g_rsn,g_return) \
if (detailed_debugging) printf ("parse_get_token : "); \
g_rv = parse_get_token (g_tkn, MAXTOKENLENGTH);\
if (g_rv != GW_WORD && g_rv != GW_EOLN)\
{\
	switch (g_rv)\
	{\
		case GW_EOF: \
			if (detailed_debugging) printf ("END OF FILE\n"); \
			SET_PARSE_ERROR(g_errs,"%s - end of input",g_rsn,g_nrr); \
			g_errs->ed_error_value=ERR_NOMOREDATA; \
			break; \
		case GW_ERRCONTINUATION: \
			if (detailed_debugging) printf ("CONTINUATION\n"); \
			SET_PARSE_ERROR(g_errs,"%s - line cont'n error",g_rsn,g_nrr); \
			break; \
		case GW_ERRILLEGALCHAR: \
			if (detailed_debugging) printf ("ILLEGAL CHAR\n"); \
			SKIP_REST_OF_LINE(g_tkn); \
			SET_PARSE_ERROR(g_errs,"%s - illegal char",g_rsn,g_nrr); \
			break; \
		case GW_ERRDANGLINGQUOTE: \
			if (detailed_debugging) printf ("UNTERMINATED QUOTE\n"); \
			SET_PARSE_ERROR(g_errs,"%s - unterminated \"",g_rsn,g_nrr); \
			break; \
		case GW_TRUNC: \
			if (detailed_debugging) printf ("TRUNCATED TOKEN\n"); \
			SKIP_REST_OF_LINE(g_tkn); \
			SET_PARSE_ERROR (g_errs,"%s - truncated token",g_rsn,g_nrr); \
			break; \
	}\
	g_return;\
} \
if (detailed_debugging) \
	if(g_rv==GW_WORD) printf ("%s\n", g_tkn); else printf ("END OF LINE\n");

/*
	Macro to handle the case in which a EOLN just won't do.  It calls the
	previous one and then tests for an EOLN.
 */

#define GETWORD(g_rv, g_errs, g_tkn, g_nrr, g_rsn, g_return) \
GETWORDorEOLN(g_rv,g_errs,g_tkn, g_nrr,g_rsn,g_return) \
if (g_rv == GW_EOLN) \
{ \
	SET_PARSE_ERROR(g_errs,"%s - end of line",g_rsn,g_nrr); \
	g_return; \
}

struct scratch
{
	u_int8_t	*s_field; /* A dynamically allocated area for work */
	int		s_index;  /* Current position in it */
	int		s_length; /* Number of bytes in s_field */
};

int parse_record (struct scratch *rr,
		char *first_token,
		struct error_data *errors,
		struct name_context *context,
		char **non_rr);

int parse_rdata (struct scratch *rr,
		struct error_data *errors,
		struct name_context *context,
		char **non_rr);

int parse_name (struct scratch *rr,
		char *name,
		struct error_data *errors,
		struct name_context *context,
		char **non_rr);

char *err_msg_n (struct error_data *err, char *fmt);

char *err_msg_sn (struct error_data *err, char *fmt, const char *one);

/* Number parsing routines */

int parse_8u (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int parse_16u (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int parse_32u (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int parse_datetime (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int parse_ttl (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int parse_string (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int parse_type (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int calc_labels (struct scratch *rr, char *token, struct error_data *errors,
				struct name_context *context, char **non_rr);

int to_u_int8_t (char   *token, u_int8_t *value);

int to_u_int16_t (char  *token, u_int16_t *value);

int to_u_int32_t (char  *token, u_int32_t *value);

#endif

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




