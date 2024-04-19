/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/dns_signer.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

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

#ifndef __dns_signer_h__
#define __dns_signer_h__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include <isc/dst.h>
#include "iip_support.h"
#include "dns_support.h"
#include "bind_support.h"
#include "dns_parse.h"

#ifdef MAXDNAME
#undef MAXDNAME
#endif
#define MAXDNAME		256

struct rr_rec
{
	u_int16_t		rr_rdata_length_h;	/* RDATA length */
	u_int8_t		*rr_rdata;		/* Raw RDATA */
	struct rr_rec		*rr_next;
};

struct rrset_rec
{
	u_int16_t		rrs_type_h;	/* ns_t_... */
	u_int8_t		rrs_parental_glue;
	u_int32_t		rrs_ttl_n;	/* ttl */
	struct rr_rec		*rrs_data;	/* All data RR's */
	struct rr_rec		*rrs_sig;	/* All signatures */
	struct key_list		*rrs_signers;	/* Copy of list */
	struct rr_rec		*rrs_recyclers; /* Signatures to just verify */
	struct rrset_rec	*rrs_next;
};

struct name_rec
{
	u_int8_t		*nr_name_n;	/* Owner */
	u_int8_t		nr_location;
	u_int8_t		nr_status;
	union
	{
		struct
		{
			char			*nru_filename;
			off_t			nru_offset;
		} nr_disk;
		struct
		{
			u_int16_t		nru_class_n;	/* ns_c_... */
			struct rrset_rec	*nru_rrsets;
		} nr_mem;
	} nr_u;
};

#define nr_filename		nr_u.nr_disk.nru_filename
#define nr_offset		nr_u.nr_disk.nru_offset
#define nr_class_n		nr_u.nr_mem.nru_class_n
#define nr_rrsets		nr_u.nr_mem.nru_rrsets

/* nr_status values */

#define NR_STATUS_UNSET		0x00
#define NR_STATUS_IN		0x01
#define NR_STATUS_SUBZONE	0x02
#define NR_STATUS_GLUE		0x03
#define NR_STATUS_DELEGATION	0x04
#define NR_STATUS_PROTO_DELEG	0x05
#define NR_STATUS_APEX		0x06
#define NR_STATUS_PROTO_APEX	0x07
#define NR_STATUS_ERROR		0x08
#define NR_STATUS_PARENT	0x09

#define NR_LOCATION_UNSET	0x00
#define NR_LOCATION_ONDISK	0x01
#define NR_LOCATION_INMEMORY	0x02

struct open_file
{
	char			*of_filename;
	struct open_file	*of_next;
};

struct key_matter
{
	u_int8_t		*km_name_n;
	u_int8_t		km_any_key;
	u_int8_t		km_any_algorithm;
	u_int8_t		km_signed_soa;
	u_int8_t		km_signing_requested;
	u_int16_t		km_flags_h;
	u_int8_t		km_protocol;
	u_int8_t		km_algorithm;
	u_int16_t		km_footprint_h;
	DST_KEY			*km_key; /* DST_KEY or whatever */
	u_int8_t		km_has_private;
	struct key_matter	*km_next;
};

struct key_list
{
	struct key_matter	*kl_key;
	struct key_list		*kl_next;
};

struct signing_statistics
{
	time_t			ss_run_begun;
	time_t			ss_run_ended;
	time_t			ss_tree_finished;
	time_t			ss_signing_begun;

	int			ss_names_read;
	int			ss_names_in_zone;
	int			ss_sets_read;

	int			ss_records_read;
	int			ss_duplicate_records;
	int			ss_sets_dropped_for_parent;
	int			ss_records_written;

	int			ss_null_keys_added;

	int			ss_nxts_added;
	int			ss_nxts_dropped;

	int			ss_signatures_read;
	int			ss_signatures_expired;
	int			ss_signatures_retired; /* Key was retired */
	int			ss_signatures_parent;
	int			ss_signatures_cant_confirm;
	int			ss_signatures_unchecked;
	int			ss_verifications_successful; /* Sig recycled */
	int			ss_verifications_failed; /* Data changed */
	int			ss_signings_done;
	int			ss_signatures_written;
};

struct signing_options
{
	/* Options set at run time */

	u_int8_t	so_do_nxts; /* Whether to do NXT's or not */
	u_int8_t	so_generate_parents; /* Make multiple .PARENTs? */
	u_int8_t	so_generate_one_parent; /* Make single .PARENT? */
	u_int8_t	so_up_policy; /* Do extra work for the UP policy */
	u_int8_t	so_show_stats; /* Print count stats at end of run */
	u_int8_t	so_enforce_self_signing; /* Force all keys to be self signed */
	u_int8_t	so_bind_extensions; /* Use BIND only master files */
	u_int8_t	*so_parent_name_n; /* Identity of parent */
	char		*so_output_zone_file; /* - in case deletion is needed*/
	char		*so_output_glue_file; /* - in case deletion is needed*/
	char		*so_output_parent_file; /*-in case deletion is needed*/
	char		*so_output_directory; /* For placing new .PARENTS */
	char		*so_input_zone_file; /* Name of input file */
	char		*so_input_parent_file; /* Name of .PARENT file */
	char		*so_input_glue_file; /* Name of GLUE file */
	u_int32_t	so_time_expired_n;
	u_int32_t	so_time_expired_duration_h;
	u_int32_t	so_time_purged_n;
	u_int32_t	so_time_purged_duration_h;
	u_int32_t	so_time_signed_n;
	struct key_list *so_key_list;
	struct signing_statistics	so_stats;

	/* Options set at run time and modified during run */

	u_int8_t	so_origin_set;
	u_int8_t	so_origin_n[MAXDNAME]; /* Might be set */
	u_int8_t	so_previous_n[MAXDNAME]; /* Set to nothing */
	u_int8_t	*so_zone_name_n; /* Might be set */
	u_int8_t	so_phase_of_moon; /* See SO_PHASE_... values */

	/* Options set or modified when the SOA is found and processed */

	u_int16_t	so_class_n;
	u_int16_t	so_ttl_set;
	u_int32_t	so_ttl_n; /* Now modified by the $TTL */
	u_int32_t	so_soa_min_ttl_n; /* Used for NXT's only */

	/* Options set when opening keys */

	struct key_list *so_soa_keys; /* SOA keys for NXT's */
	u_int8_t	so_signing_modes[256]; /* Default: NO KEY per alg'hm */
	int		so_max_algorithm; /* Largest alg number seen */

	/* Options set during signing */

	struct name_rec	*so_apex_record;
	struct key_list *so_current_directive_keys; /* $SIGNER output keys */
	struct name_rec	*so_previous_name; /* For adding to (making) its NXT */
	int		so_return_code;
	FILE		*so_zone_output; /* File for output */
	FILE		*so_glue_output; /* File for output */
	FILE		*so_parent_output; /* File for output */
};

#define SO_MODE_UNSET		0
#define SO_MODE_NO_KEY		1
#define SO_MODE_ZONE_KEY	2
#define SO_MODE_NULL_KEY	3
#define SO_MODE_BOTH_KEY	4

/*
	Phase of the moon values ... only set in main, referenced in other
	places (for starters - just in will_PARENT_overwrite_this)
*/

#define SO_PHASE_UNSET		0
#define SO_PHASE_INITIALIZATION	1
#define SO_PHASE_READING_ZONE	2
#define SO_PHASE_READING_GLUE	3
#define SO_PHASE_READING_PARENT	4
#define SO_PHASE_OPENING_KEYS	5
#define SO_PHASE_SIGNING	6

#define L_DEVELOPER		10
#define L_DEBUGGER		 7
#define L_USER			 4
#define L_MINIMAL		 1

extern int	debug_level;
extern int	debug_success;
extern char	debug_name1_h[];
extern char	debug_name2_h[];
extern char	debug_message[];

#define ERROR(msg) \
if (debug_level == L_DEVELOPER) \
	printf ("ERROR %s:%d\n",__FILE__,__LINE__);\
else if (debug_level == L_DEBUGGER) \
	printf ("ERROR %s:%d %s\n",__FILE__,__LINE__,msg);\
else \
	printf ("ERROR %s\n",msg);\
fflush (stdout)

#define WARN(msg) \
if (debug_level == L_DEVELOPER) \
	printf ("WARN %s:%d\n",__FILE__,__LINE__);\
else if (debug_level == L_DEBUGGER) \
	printf ("WARN %s:%d %s\n",__FILE__,__LINE__,msg);\
else if (debug_level == L_USER) \
	printf ("WARN %s\n",msg);\
fflush (stdout)

#define READ_PROGRESS(count,name_n) \
if (debug_level == L_DEBUGGER && (count % 100000) == 0)\
{ \
	char	currently[MAXPNAME]; \
	char	name_h[MAXPNAME]; \
	time_t	current_time; \
 \
	wire_to_ascii_name (name_h, name_n, MAXPNAME); \
	current_time = time (0); \
	strftime (currently, MAXPNAME, "%D %r", localtime(&current_time)); \
\
	if (count > 1000000) \
		printf ("%s: Read name #%1d.%1d million: %s\n", \
				currently, count / 1000000, \
				(count / 100000) % 10, name_h); \
	else \
		printf ("%s: Read name #%1d00,000 : %s\n", \
				currently, count / 100000, name_h); \
	fflush (stdout); \
}

/*
	Following is defined in signer_debug.c and is needed in the
	following macro.
*/

const char *local_sym_ntop (	const struct res_sym *syms,
				int number,
				int *success);

#define SIGN_PROGRESS(count,name_n,type_h) \
if (debug_level == L_DEBUGGER && (count % 100000) == 0) \
{ \
	char	currently[MAXPNAME]; \
	char	name_h[MAXPNAME]; \
	int	success; \
	time_t	current_time; \
 \
	wire_to_ascii_name (name_h, name_n, MAXPNAME); \
	current_time = time (0); \
	strftime (currently, MAXPNAME, "%D %r", localtime(&current_time)); \
\
	if (count > 1000000) \
		printf ("%s: Signature #%1d.%1d million applied to: %s/%s\n", \
			currently, count / 1000000, \
			(count / 100000) % 10, name_h, \
			local_sym_ntop(__p_type_syms, type_h, &success)); \
	else \
		printf ("%s: Signature #%1d00,000 applied to : %s/%s\n", \
			currently, count / 100000, name_h, \
			local_sym_ntop(__p_type_syms, type_h, &success)); \
	fflush (stdout); \
}

#define SIGNER_OK		0
#define SIGNER_ERROR		-1

void free_name_rec (struct name_rec *nr);
void free_rrset_recs (struct rrset_rec **rrs);
void free_rr_recs (struct rr_rec **rr);
void free_key_matters (struct key_matter **km);

/* Internally used defines */

#define PROTOCOL_DNSSEC	0x03
#define PROTOCOL_ALL	0xFF

#define DNS_ENVELOPE_LENGTH	10

#define KEY_RDATA_FLAGS 	 0
#define KEY_RDATA_PROTOCOL	 2
#define KEY_RDATA_ALGORITHM	 3

#define SIG_RDATA_TYPE_COVERED	 0
#define SIG_RDATA_ALGORITHM	 2
#define SIG_RDATA_LABELS	 3
#define SIG_RDATA_ORIGINAL_TTL	 4
#define SIG_RDATA_TIME_EXPIRES	 8
#define SIG_RDATA_TIME_SIGNED	12
#define SIG_RDATA_BY_FOOTPRINT	16
#define SIG_RDATA_BY_NAME	18

/*
	Maximum length of a printed DNS name in a null terminated string is:

	Starting with network name:
	\063 <63 char> \063 <63 char> \063 <63 char> \062 <62 chars> \00
	Each char can expand to 4 characters.  The five label lengths
	become 4 dots.  Add one for the null byte.  The maximum length is:
	(63+63+63+62)*4 + 4 + 1 = 251 * 4 + 5 = 1009
*/
#define MAXPNAME		1009

#define VALUE_FLAGS_CANT_AUTHENTICATE	0x8000
#define VALUE_FLAGS_CANT_CONFIDE	0x4000
#define VALUE_FLAGS_ZONE_KEY		0x0100

#define VALUE_FOOTPRINT_MINIMUM		0
#define VALUE_FOOTPRINT_MAXIMUM		65535

#define VALUE_ALGORITHM_MINIMUM		0
#define VALUE_ALGORITHM_MAXIMUM		255

#define STRING_64	64

#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    !FALSE
#endif
#ifndef NULL
#define NULL    (void*)0
#endif
#define IS_EOLN(a,b)	(a[b]=='\0' || a[b]=='\n' || a[b]==';')
#define IS_BLANK(a,b)	(a[b]==' ' || a[b]=='\t')

extern char	software_version[];

#endif
