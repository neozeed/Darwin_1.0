/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_parameters.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
    
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
    
#include "signer_parameters.h"
#include "signer_key_monitor.h"

void set_defaults (struct signing_options *options)
{
	int		index;
	time_t	current_time = time (0);

	debug_level = L_USER;

	options->so_do_nxts = TRUE;
	options->so_generate_parents = FALSE;
	options->so_generate_one_parent = TRUE;
	options->so_up_policy = FALSE;
	options->so_show_stats = FALSE;
	options->so_enforce_self_signing = FALSE;
	options->so_apex_record = NULL;
	for (index = 0; index < 256; index++)
		options->so_signing_modes[index] = SO_MODE_NO_KEY;
	options->so_origin_set = FALSE;
	memset (options->so_origin_n, '\0', MAXDNAME);
	memset (options->so_previous_n, '\0', MAXDNAME);
	options->so_zone_name_n = NULL;
	options->so_parent_name_n = NULL;
	options->so_output_zone_file = "./FINISH-ZONE";
	options->so_output_glue_file = "./FINISH-GLUE";
	options->so_output_parent_file = "./FINISH-PARENT";
	options->so_output_directory = ".";
	options->so_input_parent_file = NULL;
	options->so_input_glue_file = NULL;
	options->so_input_zone_file = "START-ZONE";
	options->so_class_n = htons(ns_c_in);
	options->so_ttl_set = FALSE;
	options->so_soa_min_ttl_n = 0;
	options->so_time_purged_duration_h = 7*24*3600; /* A week */
	options->so_time_expired_duration_h = 31*24*3600; /* A 31-day month */
	options->so_time_purged_n = htonl(current_time +
										options->so_time_purged_duration_h);
	options->so_zone_output = NULL;
	options->so_glue_output = NULL;
	options->so_parent_output = NULL;
	options->so_key_list = NULL;
	options->so_current_directive_keys = NULL;
	options->so_soa_keys = NULL;
	options->so_previous_name = NULL;
	options->so_return_code = SIGNER_OK;
	options->so_bind_extensions = TRUE;
}

void zero_stats (struct signing_statistics *ss)
{
	ss->ss_names_read = 0;
	ss->ss_names_in_zone = 0;
	ss->ss_sets_read = 0; 

	ss->ss_records_read = 0;
	ss->ss_duplicate_records = 0;
	ss->ss_sets_dropped_for_parent = 0;
	ss->ss_records_written = 0;

	ss->ss_null_keys_added = 0;

	ss->ss_nxts_added = 0;
	ss->ss_nxts_dropped = 0;

	ss->ss_signatures_read = 0;
	ss->ss_signatures_expired = 0;
	ss->ss_signatures_retired = 0;
	ss->ss_signatures_cant_confirm = 0;
	ss->ss_signatures_unchecked = 0;
	ss->ss_signatures_parent = 0;
	ss->ss_verifications_successful = 0;
	ss->ss_verifications_failed = 0;
	ss->ss_signings_done = 0;
	ss->ss_signatures_written = 0;
}

void print_long_usage_note (int argc,char *argv[])
{
	printf ("Usage: %s [any number of the following switches]\n", argv[0]);
	printf ("	Name                Switch  Arguments\n");
	printf ("Inputing files\n");
	printf ("    Input files         -fzpg   <file-1> <file-2>|'-' <file-3>\n");
	printf ("\n");
	printf ("                                Deprecated, ignore this flag.\n");
	printf ("\n");
	printf ("                                Specifies the zone, parent, and glue file in\n");
	printf ("                                one switch, shorthand for -zi, -pi, -gi.  If\n");
	printf ("                                a dash is substituted for the parent file, it\n");
	printf ("                                is assumed to not exist.\n");
	printf ("\n");
	printf ("                                No default value.\n");
	printf ("\n");
	printf ("    Input zone file     -zi     <file>\n");
	printf ("\n");
	printf ("                                Specifies the zone data file.  The first\n");
	printf ("                                RR must be an SOA, the first record may be\n");
	printf ("                                found in a $INCLUDED file.\n");
	printf ("\n");
	printf ("                                Default: START-ZONE\n");
	printf ("\n");
	printf ("    Input parent file   -pi     <file>\n");
	printf ("\n");
	printf ("                                Specifies the parent file received from the\n");
	printf ("                                parent zone to be used as input to this zone.\n");
	printf ("                                If specified, all records that would conflict\n");
	printf ("                                with it - apex upper nxt, keys, and sigs for\n");
	printf ("                                these are dropped.  If the UP policy is\n");
	printf ("                                specified, then the parent's KEY, NS, and glue\n");
	printf ("                                is also dropped.\n");
	printf ("\n");
	printf ("                                Default: no parent file\n");
	printf ("\n");
	printf ("    Input glue file     -gi     <file>\n");
	printf ("\n");
	printf ("                                Deprecated, ignore this flag.\n");
	printf ("\n");
	printf ("                                Default: no glue file\n");
	printf ("\n");
	printf ("Output files\n");
	printf ("    Zone out file       -zo     <file>\n");
	printf ("\n");
	printf ("                                Specifies the file where signed zone data is\n");
	printf ("                                left.\n");
	printf ("\n");
	printf ("                                Default: FINISH-ZONE\n");
	printf ("\n");
	printf ("    Glue out file       -go     <file>\n");
	printf ("\n");
	printf ("                                Deprecated, ignore this flag.\n");
	printf ("\n");
	printf ("                                Default: set to a tmpnam()\n");
	printf ("\n");
	printf ("Parent File Generation\n");
	printf ("Note: there are two ways in which parent files are made, individual and bulk.\n");
	printf ("The two methods use independent switches - both can be used, neither can be\n");
	printf ("used, or just one.  By default, the bulk approach is used.\n");
	printf ("\n");
	printf ("    Generate a Parent   -p1     none\n");
	printf ("\n");
	printf ("                                Place all of the generated parent data for\n");
	printf ("                                the zone's delegation points into one file.\n");
	printf ("                                Separating lines are added to identify the\n");
	printf ("                                start and end of the information destined for\n");
	printf ("                                individual zones.\n");
	printf ("\n");
	printf ("                                Default: this is done during the run.\n");
	printf ("\n");
	printf ("    Parent out file     -po     <file>\n");
	printf ("\n");
	printf ("                                Name of the file to hold all of the data.\n");
	printf ("\n");
	printf ("                                Default: FINISH-PARENT\n");
	printf ("\n");
	printf ("    Generate no Parent  -no-p1  none\n");
	printf ("\n");
	printf ("                                Do not generate a single parent file during the\n");
	printf ("                                run.\n");
	printf ("\n");
	printf ("                                Default: the file is generated.  Use this\n");
	printf ("                                switch to turn the feature off.\n");
	printf ("\n");
	printf ("    Generate Parents    -ps     none\n");
	printf ("\n");
	printf ("                                Place the generated parent data into\n");
	printf ("                                individual files, named by def-directory/\n");
	printf ("                                zone.PARENT.\n");
	printf ("\n");
	printf ("                                For large delegating zones, there will be many\n");
	printf ("                                files.  COM. would generate about 1-1/2 million\n");
	printf ("                                files (based on winter '98 data).\n");
	printf ("\n");
	printf ("                                Default: this is not done, use the switch to\n");
	printf ("                                start this feature.\n");
	printf ("\n");
	printf ("    Output dir          -pd     <directory>\n");
	printf ("\n");
	printf ("                                Specifies directory for the individual\n");
	printf ("                                parent files.\n");
	printf ("\n");
	printf ("                                Default: . (i.e., current working directory)\n");
	printf ("\n");
	printf ("    Generate no Parents -no-ps  none\n");
	printf ("\n");
	printf ("                                Suppress generation of individual parent files\n");
	printf ("                                for each delegation point.\n");
	printf ("\n");
	printf ("                                Default: Generation is suppressed (this switch\n");
	printf ("                                is not needed unless -ps is specified earlier\n");
	printf ("                                in the line and the parents are now not wanted).\n");
	printf ("\n");
	printf ("Signing Policy\n");
	printf ("    Self-signing of keys\n");
	printf ("    Enforce it          -ess    none\n");
	printf ("\n");
	printf ("                                Makes sure that each key set is self-\n");
	printf ("                                signed.  Each key in the set is to generate\n");
	printf ("                                a signature for the set.\n");
	printf ("\n");
	printf ("                                Default: Don't enforce it\n");
	printf ("\n");
	printf ("    Don't enforce       -no-ess none\n");
	printf ("\n");
	printf ("                                Turns enforcement off, the default.\n");
	printf ("                                Self-signing is allowed.\n");
	printf ("\n");
	printf ("                                Default: Don't enforce it\n");
	printf ("\n");
	printf ("    Down policy         -dn     none\n");
	printf ("\n");
	printf ("                                Signs according to the DOWN policy - i.e.,\n");
	printf ("                                the apex does not sign the parent's keys,\n");
	printf ("                                the parent's keys and glue data are not\n");
	printf ("                                expected from nor written to parent files.\n");
	printf ("\n");
	printf ("                                Default: DOWN policy\n");
	printf ("\n");
	printf ("    Up policy           -up     none\n");
	printf ("\n");
	printf ("                                Signs according to the UP policy - i.e.,\n");
	printf ("                                the apex signs the parent's keys,\n");
	printf ("                                the parent's keys and glue data are\n");
	printf ("                                expected from and written to parent files.\n");
	printf ("\n");
	printf ("                                Default: DOWN policy\n");
	printf ("\n");
	printf ("    Parent name         -pa     <domain>\n");
	printf ("\n");
	printf ("                                Specified the apex's parent zone.  If the keys\n");
	printf ("                                for this zone are known and the UP policy\n");
	printf ("                                is used, the apex zone keys sign the key.\n");
	printf ("                                If UP is used and this is not specified, then\n");
	printf ("                                the signer acts as if it does not otherwise\n");
	printf ("                                know the parent's identity.  This is equivalent\n");
	printf ("                                to the $PARENT directive (except that relative\n");
	printf ("                                domain names are treated as absolute names).\n");
	printf ("\n");
	printf ("                                Default: unspecified\n");
	printf ("NXT management\n");
	printf ("    Do NXT processing   -n      none\n");
	printf ("\n");
	printf ("                                Generate NXT's for the zone, signing them\n");
	printf ("                                with the keys that sign the SOA record.  (If\n");
	printf ("                                none sign the SOA, no NXT's are signed.)\n");
	printf ("\n");
	printf ("                                Default: NXT's are generated.  (This switch is\n");
	printf ("                                unnecessary unless the -no-n switch was\n");
	printf ("                                previously in the run command and now NXT's\n");
	printf ("                                are desired.)\n");
	printf ("\n");
	printf ("    No NXT processing   -no-n   none\n");
	printf ("\n");
	printf ("                                Suppress the generation of the NXT record.\n");
	printf ("\n");
	printf ("                                Default: NXT's are generated, this switch\n");
	printf ("                                stops this.\n");
	printf ("SIG handling\n");
	printf ("    SIG duration        -dur    <ttl>\n");
	printf ("\n");
	printf ("                                All SIG record generated are set to expire\n");
	printf ("                                at a the current time + duration.\n");
	printf ("\n");
	printf ("                                <ttl> format is taken from the BIND definition\n");
	printf ("                                of TTL.  Numeric seconds is fine, as well as:\n");
	printf ("                                         <number> W - weeks\n");
	printf ("                                         <number> D - days\n");
	printf ("                                         <number> H - hours\n");
	printf ("                                         <number> M - minutes (not months!)\n");
	printf ("                                         <number> S - seconds\n");
	printf ("\n");
	printf ("                                Default: 31 days\n");
	printf ("\n");
	printf ("    Purge period        -pt     <ttl>\n");
	printf ("\n");
	printf ("                                Specifies that all SIG records with expiration\n");
	printf ("                                times between the beginning of past up\n");
	printf ("                                through current time + the purge period are\n");
	printf ("                                treated as expired.  SIG records with\n");
	printf ("                                expiration times from current + purge period to\n");
	printf ("                                the end of the future are retained if they\n");
	printf ("                                are not proved invalid.\n");
	printf ("\n");
	printf ("                                <ttl> format is taken from the BIND definition\n");
	printf ("                                of TTL.  Numeric seconds is fine, as well as:\n");
	printf ("                                         <number> W - weeks\n");
	printf ("                                         <number> D - days\n");
	printf ("                                         <number> H - hours\n");
	printf ("                                         <number> M - minutes (not months!)\n");
	printf ("                                         <number> S - seconds\n");
	printf ("\n");
	printf ("                                The end of the future and beginning of the past\n");
	printf ("                                are points in time which have the same time\n");
	printf ("                                representation (one second apart) in a 32-bit\n");
	printf ("                                roll-over specification of time.  The end of\n");
	printf ("                                the future is 2 to the 31st power seconds from\n");
	printf ("                                the current time.\n");
	printf ("\n");
	printf ("                                Default: 1 week\n");
	printf ("\n");
	printf ("    Init key            -k1     <domain> <algorithm> <id>\n");
	printf ("\n");
	printf ("                                This adds the specified key (key owner and\n");
	printf ("                                id) to the list of keys with which\n");
	printf ("                                to sign.  Equivalent to $SIGNER ADD <><>.\n");
	printf ("                                This switch may appear anywhere in the run\n");
	printf ("                                command, it adds just one key.\n");
	printf ("                                The <algorithm> parameter has been added, it\n");
	printf ("                                specifies the algorithm that generated the key.\n");
	printf ("                                Allowable values are 0 to 255, and the\n");
	printf ("                                following strings: \"dh\" \"rssaref\" \"dss\"\n");
	printf ("                                and \"private\".\n");
	printf ("\n");
	printf ("                                Default: keys are specified by $SIGNER\n");
	printf ("                                directives in the data files.  A zone\n");
	printf ("                                may elect not to use any keys.\n");
	printf ("\n");
	printf ("    Init key list       -ks     <domain> <algorithm> <id> [...] \\eoln\n");
	printf ("\n");
	printf ("                                This adds the specified keys (key owner and\n");
	printf ("                                id) to the list of keys with which\n");
	printf ("                                to sign.  Equivalent to $SIGNER ADD <><>.\n");
	printf ("                                This switch is interpreted as the last\n");
	printf ("                                switch of the command line.  Any number\n");
	printf ("                                of keys can be specified.\n");
	printf ("                                The <algorithm> parameter has been added, it\n");
	printf ("                                specifies the algorithm that generated the key.\n");
	printf ("                                Allowable values are 0 to 255, and the\n");
	printf ("                                following strings: \"dh\" \"rssaref\" \"dss\"\n");
	printf ("                                and \"private\".\n");
	printf ("\n");
	printf ("                                Default: keys are specified by $SIGNER\n");
	printf ("                                directives in the data files.  A zone\n");
	printf ("                                may elect not to use any keys.\n");
	printf ("\n");
	printf ("Output Controls\n");
	printf ("    Output Level        -l      <level>\n");
	printf ("\n");
	printf ("                                level can be one of the following values:\n");
	printf ("                                Num or Mnemonic   Meaning\n");
	printf ("                                1      M(inimal)  Just errors\n");
	printf ("                                4      U(ser)     Errors and warnings\n");
	printf ("                                7      DEB(ugger) Source code loc, E & W\n");
	printf ("                                10     DEV(eloper)Source code loc and crypto\n");
	printf ("\n");
	printf ("                                Default: user (aka 4)\n");
	printf ("\n");
	printf ("    Hide Stats          -no-st  none\n");
	printf ("\n");
	printf ("                                When specified, stifles the printing of\n");
	printf ("                                summary statistics at the end of the run.\n");
	printf ("\n");
	printf ("                                Default: stats are not printed.\n");
	printf ("\n");
	printf ("    Show Stats          -st     none\n");
	printf ("\n");
	printf ("                                When specified, causes the printing of\n");
	printf ("                                summary statistics at the end of the run.\n");
	printf ("\n");
	printf ("                                Default: stats are not printed.\n");
	printf ("\n");
	printf ("Miscellaneous\n");
	printf ("    ORIGINal value      -or     <domain>\n");
	printf ("\n");
	printf ("                                This is equivalent to $ORIGIN <domain>, except\n");
	printf ("                                that the terminating period is not needed in\n");
	printf ("                                the domain name.  Specifying an ORIGIN is only\n");
	printf ("                                mandatory for the root zones and other zones\n");
	printf ("                                using relative names in the zone files.  It\n");
	printf ("                                is recommended that the $ORIGIN <domain> be\n");
	printf ("                                put in the data file.\n");
	printf ("\n");
	printf ("                                Default: unspecified\n");
	printf ("\n");
	printf ("    Use BIND extensions -bind   The BIND extended TTL format is used\n");
	printf ("                                when writing files.\n");
	printf ("    Use RFC format      -no-bind TTLs are written as numeric values.\n");
	printf ("    Print HELP          -help   Prints a long version of help and EXITS.\n");
	printf ("\n");
	printf ("                                Default: switch is not specified.\n");
	printf ("\n");
}

int print_usage_note (int argc,char *argv[])
{
	printf("%s [any number of the following switches]\n", argv[0]);
	printf("\tName                Switch    Arguments\n");
	printf("\tInput files         -fzpg     <file-1> <file-2>|'-' <file-3> (deprecated)\n");
	printf("\tInput zone file     -zi       <file>\n");
	printf("\tInput parent file   -pi       <file>\n");
	printf("\tInput glue file     -gi       <file> (deprecated)\n");
	printf("\tZone out file       -zo       <file>\n");
	printf("\tGlue out file       -go       <file> (deprecated)\n");
	printf("\tGenerate a Parent   -p1       none\n");
	printf("\tParent out file     -po       <file>\n");
	printf("\tGenerate no Parent  -no-p1    none\n");
	printf("\tGenerate Parents    -ps       none\n");
	printf("\tOutput dir          -pd       <directory>\n");
	printf("\tGenerate no Parents -no-ps    none\n");
	printf("\tEnforce self-sign   -ess      none\n");
	printf("\tVolunatry self-sign -no-ess   none\n");
	printf("\tDown policy         -dn       none\n");
	printf("\tUp policy           -up       none\n");
	printf("\tParent name         -pa       <domain>\n");
	printf("\tDo NXT processing   -n        none\n");
	printf("\tNo NXT processing   -no-n     none\n");
	printf("\tSIG duration        -dur      <ttl>\n");
	printf("\tPurge period        -pt       <ttl>\n");
	printf("\tInit key            -k1       <domain> <algorithm> <id>\n");
	printf("\tInit key list       -ks       <domain> <algorithm> <id> [...] \\eoln\n");
	printf("\tOutput Level        -l        <level>\n");
	printf("\tHide Stats          -no-st    none\n");
	printf("\tShow Stats          -st       none\n");
	printf("\tORIGINal value      -or       <domain>\n");
	printf("\tUse BIND extensions -bind     none\n");
	printf("\tUse RFC format      -no-bind  none\n");
	printf("\tPrint HELP          -help     none\n");

	return SIGNER_ERROR;
}

int time_arg (int argc, char *argv[], u_int32_t *target, int *argn)
{
	u_long time_spec_u_long; 

	if (*argn == argc)
	{
		sprintf (debug_message, "Missing time argument for %s", argv[*argn-1]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	if (ns_parse_ttl (argv[*argn], &time_spec_u_long) == SIGNER_ERROR)
	{
		/* Not a legal time specification */
		sprintf (debug_message, "Malformed time string: %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	(*target) = (u_int32_t) time_spec_u_long;

	(*argn)++;

	return SIGNER_OK;
}

int string_arg (int argc, char *argv[], char **target, int *argn)
{
	if (*argn == argc)
	{
		sprintf (debug_message,"Missing string argument for %s",argv[*argn-1]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	(*target) = argv[*argn];

	(*argn)++;

	return SIGNER_OK;
}

int string_spec_arg (int argc, char *argv[], char **target, int *argn)
{
	if (string_arg (argc,argv,target,argn) == SIGNER_ERROR)
		return SIGNER_ERROR;

	if (strcmp ((*target), "-") == 0) *target = NULL;

	return SIGNER_OK;
}

int one_key_arg (int argc, char *argv[], struct key_list **target, int *argn,
					struct signing_options *options)
{
	u_int8_t	name_n[MAXDNAME];
	int			foot_int;
	u_int16_t	foot_h;
	int			foot_any;
	int			algorithm_int;
	u_int8_t	algorithm;
	int			algorithm_any;

	if (*argn > argc - 3)
	{
		sprintf (debug_message,
					"Missing key (name, algorithm, id) argument for %s",
					argv[*argn-1]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	/* Assuming name is fully qualified for now */

	if (ns_name_pton (argv[(*argn)], name_n, MAXDNAME) == SIGNER_ERROR)
	{
		sprintf (debug_message,"Illegal key name %s", argv[*argn]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	/* Read the algorithm spec. */

	(*argn)++;

	if (strcasecmp(argv[(*argn)], "any") == 0)
	{
		algorithm_any = TRUE;
		algorithm = 0;
	}
	else if (isdigit (argv[(*argn)][0]))
	{
		algorithm_int = atoi (argv[(*argn)]);
		if (0 > algorithm_int || algorithm_int > 255)
		{
			sprintf (debug_message,"Key algorithm %s out of range",argv[*argn]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}

		algorithm_any = FALSE;
		algorithm = (u_int8_t) algorithm_int;
	}
	else if (strncasecmp (argv[(*argn)], "dss", 2) == 0)
	{
		/* DSA or DSS */
		algorithm_any = FALSE;
		algorithm = (u_int8_t) 3;
	}
	else if (strncasecmp (argv[(*argn)], "rsaref", 3) == 0)
	{
		/* RSAREF */
		algorithm_any = FALSE;
		algorithm = (u_int8_t) 1;
	}
	else if (strncasecmp (argv[(*argn)], "dh", 2) == 0)
	{
		/* Diffie-Hellman */
		algorithm_any = FALSE;
		algorithm = (u_int8_t) 2;
	}
	else if (strcasecmp (argv[(*argn)], "private") == 0)
	{
		/* Private */
		algorithm_any = FALSE;
		algorithm = (u_int8_t) 254;
	}
	else
	{
		sprintf (debug_message,"Bad value for key algorithm %s", argv[*argn]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	/* Read the id spec. */

	(*argn)++;

	if (strcasecmp(argv[(*argn)], "any") == 0)
	{
		foot_any = TRUE;
		foot_h = 0;
	}
	else if (isdigit (argv[(*argn)][0]))
	{
		foot_int = atoi (argv[(*argn)]);
		if (0 > foot_int || foot_int > 65535)
		{
			sprintf (debug_message,"Key id %s out of range",argv[*argn]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}

		foot_any = FALSE;
		foot_h = (u_int16_t) foot_int;
	}
	else
	{
		sprintf (debug_message,"Bad value for key id %s", argv[*argn]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	(*argn)++;

	return key_add_to_list (name_n, algorithm_any, algorithm, foot_any,
							foot_h, target, options);
}

int many_key_arg (int argc, char *argv[], struct key_list **target, int *argn,
					struct signing_options *options)
{
	do
	{
		if (one_key_arg (argc, argv, target, argn, options) == SIGNER_ERROR)
			return SIGNER_ERROR;
	} while (*argn < argc);

	return SIGNER_OK;
}

int name_n_array_arg (int argc, char *argv[], u_int8_t *target, int *argn)
{
	if (*argn == argc)
	{
		sprintf (debug_message,"Missing name argument for %s", argv[*argn-1]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	/* Assuming name is fully qualified for now */

	if (ns_name_pton (argv[(*argn)], target, MAXDNAME) == SIGNER_ERROR)
	{
		sprintf (debug_message,"Illegally formatted name %s", argv[*argn]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	(*argn)++;

	return SIGNER_OK;
}

int name_n_ptr_arg (int argc, char *argv[], u_int8_t **target, int *argn)
{
	u_int8_t	temp_name_n[MAXDNAME];
	size_t		temp_name_length;

	if (name_n_array_arg (argc, argv, temp_name_n, argn) == SIGNER_ERROR)
		return SIGNER_ERROR;

	temp_name_length = wire_name_length (temp_name_n);

	(*target) = (u_int8_t *) MALLOC (temp_name_length);

	memcpy ((*target), temp_name_n, temp_name_length);

	return SIGNER_OK;
}

int level_arg (int argc, char *argv[], int *argn)
{
	if (*argn == argc)
	{
		sprintf (debug_message,"Missing level argument for %s", argv[*argn-1]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	if (isdigit (argv[*argn][0]))
	{
		debug_level = atoi (argv[*argn]);
	
		if (debug_level != L_DEVELOPER && debug_level != L_DEBUGGER &&
			debug_level != L_USER && debug_level != L_MINIMAL)
		{
			sprintf (debug_message,"Debug level must be one of {%d,%d,%d,%d}",
									L_MINIMAL, L_USER, L_DEBUGGER, L_DEVELOPER);
			ERROR (debug_message);
			return print_usage_note (argc, argv);
		}
	}
	else
	{
		if (strncasecmp (argv[*argn], "dev", 3) == 0)
			debug_level =  L_DEVELOPER;
		else if (strncasecmp (argv[*argn], "deb", 3) == 0)
			debug_level =  L_DEBUGGER;
		else if (tolower(argv[*argn][0]) == 'u')
			debug_level =  L_USER;
		else if (tolower(argv[*argn][0]) == 'm')
			debug_level =  L_MINIMAL;
		else
		{
			sprintf (debug_message,"Debug level must be one of {%s,%s,%s,%s}",
						"M(inimal)", "U(ser)", "DEB(ugger)", "DEV(eloper)");
			ERROR (debug_message);
			return print_usage_note (argc, argv);
		}
	}

	(*argn)++;

	return SIGNER_OK;
}

int handle_b_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (strcmp (&argv[*argn][argv_index], "bind") == 0)
	{
		(*argn)++;

		options->so_bind_extensions = TRUE;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_d_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='n' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		options->so_up_policy = FALSE;
	}
	else if (strcmp(&argv[*argn][argv_index], "dur") == 0)
	{
		(*argn)++;

		if (time_arg (argc,argv,&options->so_time_expired_duration_h,argn)
															== SIGNER_ERROR)
			return SIGNER_ERROR;
	}
	else
	{
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_e_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (strcmp(&argv[*argn][argv_index], "ess") == 0)
	{
		(*argn)++;

		options->so_enforce_self_signing = TRUE;
	}
	else
	{
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_f_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='\0' ||
			strcmp(&argv[*argn][argv_index], "fzpg") == 0)
	{
		(*argn)++;

		if (string_arg (argc,argv,&options->so_input_zone_file,argn)
														== SIGNER_ERROR)
			return SIGNER_ERROR;

		if (string_spec_arg (argc,argv,&options->so_input_parent_file,argn)
														== SIGNER_ERROR)
			return SIGNER_ERROR;

		if (string_arg (argc,argv,&options->so_input_glue_file,argn)
														== SIGNER_ERROR)
			return SIGNER_ERROR;

		WARN ("-fzpg is deprecated");
	}
	else
	{
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_g_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='i' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (string_arg (argc,argv,&options->so_input_glue_file,argn)
														== SIGNER_ERROR)
			return SIGNER_ERROR;

		WARN ("-gi is deprecated");
	}
	else if (argv[*argn][argv_index+1]=='o' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (string_arg (argc,argv,&options->so_output_glue_file,argn)
														== SIGNER_ERROR)
			return SIGNER_ERROR;

		WARN ("-go is deprecated");
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_h_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (strcasecmp(&argv[*argn][argv_index],"help")==0)
	{
		print_long_usage_note (argc, argv);
		exit (1);
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_k_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='1' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (one_key_arg (argc, argv, &options->so_key_list, argn, options)
														== SIGNER_ERROR)
			return SIGNER_ERROR;
	}
	else if (argv[*argn][argv_index+1]=='s' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (many_key_arg (argc, argv, &options->so_key_list, argn, options)
														== SIGNER_ERROR)
			return SIGNER_ERROR;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_l_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='\0')
	{
		(*argn)++;

		if (level_arg (argc,argv,argn) == SIGNER_ERROR)
			return SIGNER_ERROR;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_n_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='\0')
	{
		(*argn)++;

		options->so_do_nxts = TRUE;
	}
	else if (strcmp (&argv[*argn][argv_index], "no-n") == 0)
	{
		(*argn)++;

		options->so_do_nxts = FALSE;
	}
	else if (strcmp (&argv[*argn][argv_index], "no-p1") == 0)
	{
		(*argn)++;

		options->so_generate_one_parent = FALSE;
	}
	else if (strcmp (&argv[*argn][argv_index], "no-ps") == 0)
	{
		(*argn)++;

		options->so_generate_parents = FALSE;
	}
	else if (strcmp (&argv[*argn][argv_index], "no-st") == 0)
	{
		(*argn)++;

		options->so_show_stats = FALSE;
	}
	else if (strcmp (&argv[*argn][argv_index], "no-ess") == 0)
	{
		(*argn)++;

		options->so_enforce_self_signing = FALSE;
	}
	else if (strcmp (&argv[*argn][argv_index], "no-bind") == 0)
	{
		(*argn)++;

		options->so_bind_extensions = FALSE;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_o_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='r' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (name_n_array_arg (argc, argv, options->so_origin_n, argn)
														== SIGNER_ERROR)
			return SIGNER_ERROR;

		options->so_origin_set = TRUE;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_p_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='1' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		options->so_generate_one_parent = TRUE;
	}
	else if (argv[*argn][argv_index+1]=='a' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (name_n_ptr_arg (argc, argv, &options->so_parent_name_n, argn)
															== SIGNER_ERROR)
			return SIGNER_ERROR;
	}
	else if (argv[*argn][argv_index+1]=='d' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (string_arg (argc,argv,&options->so_output_directory,argn)
															== SIGNER_ERROR)
			return SIGNER_ERROR;

		options->so_generate_parents = TRUE;
	}
	else if (argv[*argn][argv_index+1]=='i' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (string_arg (argc, argv, &options->so_input_parent_file, argn)
															== SIGNER_ERROR)
			return SIGNER_ERROR;
	}
	else if (argv[*argn][argv_index+1]=='o' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (string_arg (argc, argv, &options->so_output_parent_file, argn)
															== SIGNER_ERROR)
			return SIGNER_ERROR;

		options->so_generate_one_parent = TRUE;
	}
	else if (argv[*argn][argv_index+1]=='s' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		options->so_generate_parents = TRUE;
	}
	else if (argv[*argn][argv_index+1]=='t' && argv[*argn][argv_index+2]=='\0')
	{
		time_t	current_time = time (0);

		(*argn)++;

		if (time_arg (argc,argv,&options->so_time_purged_duration_h,argn)
															== SIGNER_ERROR)
			return SIGNER_ERROR;

		options->so_time_purged_n = htonl(current_time +
										options->so_time_purged_duration_h);
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_s_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='t' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		options->so_show_stats = TRUE;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_u_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='p' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		options->so_up_policy = TRUE;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}

int handle_z_switches (	int argc, char *argv[],
						struct signing_options *options,
						int *argn, int argv_index)
{
	if (argv[*argn][argv_index+1]=='i' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (string_arg (argc, argv, &options->so_input_zone_file, argn)
															== SIGNER_ERROR)
			return SIGNER_ERROR;

		options->so_generate_one_parent = TRUE;
	}
	else if (argv[*argn][argv_index+1]=='o' && argv[*argn][argv_index+2]=='\0')
	{
		(*argn)++;

		if (string_arg (argc,argv,&options->so_output_zone_file,argn)
														== SIGNER_ERROR)
			return SIGNER_ERROR;
	}
	else
	{
		/* Unknown flag */
		sprintf (debug_message,"Unknown flag %s", argv[*argn]);
		ERROR (debug_message);
		return print_usage_note (argc, argv);
	}

	return SIGNER_OK;
}


int get_run_time_parameters (	int						argc,
								char					*argv[],
								struct signing_options	*options)
{
	int	argn = 0;
	int argv_index;

	zero_stats (&options->so_stats);
	set_defaults (options);

	argn++; /* Skip the command name */

	while (argn < argc)
	{
		if (argv[argn][0] == '-')
			argv_index = 1;
		else
			argv_index = 0;

		switch (argv[argn][argv_index])
		{
			case 'b':
				if (handle_b_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'd':
				if (handle_d_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'e':
				if (handle_e_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'f':
				if (handle_f_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'g':
				if (handle_g_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'h':
				if (handle_h_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'k':
				if (handle_k_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'l':
				if (handle_l_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'n':
				if (handle_n_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'o':
				if (handle_o_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'p':
				if (handle_p_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 's':
				if (handle_s_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'u':
				if (handle_u_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			case 'z':
				if (handle_z_switches (argc,argv,options,&argn,argv_index)
															== SIGNER_ERROR)
					return SIGNER_ERROR;
				break;
			default:
				sprintf (debug_message, "Unknown flag %s", argv[argn]);
				ERROR (debug_message);
				return print_usage_note (argc,argv);
		}
	}

	return SIGNER_OK;
}
