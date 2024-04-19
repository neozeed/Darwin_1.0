/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/signer_directives.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#include "signer_directives.h"
#include "signer_sign.h"
#include "signer_tree.h"
#include "signer_key_monitor.h"
#include "signer_ingest.h"

int odd_number_of_escapes (char *str, int index)
{
	/*
		Counts the number of consecutive escapes, backwards from the
		**preceeding** character.
	*/

	int count = 0;

	if (index == 0) return FALSE;

	do
	{
		index--;

		if (str[index] == '\\')
			count ++;
		else
			break;

	} while (index > 0);

	return ((count % 2) == 1);
}

void get_name (char *args, int *index, char* new_filename)
{
	int	file_index = 0;

	while (IS_BLANK(args, *index)) (*index)++;

	while ((!IS_BLANK(args, *index) && !IS_EOLN(args, *index))
							|| odd_number_of_escapes(args, *index))
		new_filename[file_index++] = args[(*index)++];

	new_filename[file_index] = '\0';
}

int get_name_n (char* args, int *index, u_int8_t *old_origin_n,
				u_int8_t *new_name_n, struct signing_options *options)
{
	char		*new_name_h;
	int			new_length = strlen (args) + 1;
	int			ret_val;
	int			o_index;

	if ((new_name_h = (char*) MALLOC (new_length)) == NULL)
	{
		ERROR ("Out of memory - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	while (IS_BLANK(args, *index)) (*index)++;

	get_name (args, index, new_name_h);

	if ((ret_val=ns_name_pton(new_name_h, new_name_n, MAXDNAME))== -1)
	{
		sprintf (debug_message, "Converting %s to DNS name failed", new_name_h);
		FREE (new_name_h);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	if (ret_val== 0)
	{
		/* Need to append old origin to this (not fully qualified) */

		o_index = wire_name_length(new_name_n) - 1;

		if (wire_name_length (old_origin_n) + o_index > MAXDNAME)
		{
			wire_to_ascii_name (debug_name1_h, old_origin_n, MAXPNAME);
			sprintf (debug_message, "%s.%s is too long for DNS", new_name_h,
																debug_name1_h);
			FREE (new_name_h);
			return SIGNER_ERROR;
		}

		memcpy (&new_name_n[o_index], old_origin_n, MAXDNAME-o_index);
	}

	FREE (new_name_h);

	return SIGNER_OK;
}

int handle_INCLUDE (char *args, struct name_context *context,
					struct key_list *key_list, struct signing_options *options,
					struct error_data *err)
{
	int						filename_length = strlen (args) + 1;
	int						ret_val;
	u_int8_t				origin_n[MAXDNAME];
	int						index = 0;
	struct signing_options	new_options;
	char					*this_file;

	memcpy (&new_options, options, sizeof (struct signing_options));

	new_options.so_key_list = key_list;

	if ((this_file=(char*)MALLOC(filename_length))==NULL)
	{
		ERROR ("Out of memory - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	/* Get filename from args (should be first) */

	get_name (args, &index, this_file);

	/* Set the correct file (phase of moon stuff */

	switch (options->so_phase_of_moon)
	{
		case SO_PHASE_READING_ZONE:
			new_options.so_input_zone_file = this_file;
			break;
		case SO_PHASE_READING_GLUE:
			new_options.so_input_glue_file = this_file;
			break;
		case SO_PHASE_READING_PARENT:
			new_options.so_input_parent_file = this_file;
			break;
		default:
			/* Error: wrong phase of moon */
			ERROR ("In parsing routine at wrong stage of execution");
			return SIGNER_ERROR;
	}

	/* If there is the optional new origin for the file */

	while (IS_BLANK(args, index)) index++;

	if (IS_EOLN(args, index))
	{
		/* Copy context's origin to origin_n */
		memcpy (origin_n, context->nc_origin_n,
					wire_name_length(context->nc_origin_n));
	}
	else
	{
		/* Convert the origin_h to origin_n */

		if (get_name_n (args, &index, context->nc_origin_n, origin_n, options)
				== SIGNER_ERROR)
			return SIGNER_ERROR;
		new_options.so_origin_set = TRUE;
	}

	memcpy (new_options.so_origin_n, origin_n, wire_name_length (origin_n));

	/* Make sure there's no other garbage in the line */

	while (IS_BLANK(args, index)) index++;

	if (!IS_EOLN(args, index))
	{
		sprintf (debug_message, "%s[%d]: Garbage at end of $INCLUDE : %s",
			err->ed_filename, err->ed_curr_line, &args[index]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	/* Make the recursive call */

	ret_val = parse_from_file (&new_options);

	/* Update the origin_set, zone_name_n, class_n, ttl_n */

	if (options->so_zone_name_n==NULL && new_options.so_zone_name_n != NULL)
	{
		options->so_zone_name_n = new_options.so_zone_name_n;
		options->so_class_n = new_options.so_class_n;
		options->so_ttl_n = new_options.so_ttl_n;
		context->nc_class_n = new_options.so_class_n;
		context->nc_min_ttl_n = new_options.so_ttl_n;
	}

	/* Update the global statistics */

	memcpy (&options->so_stats, &new_options.so_stats,
									sizeof (struct signing_statistics));

	FREE (this_file);
	return ret_val;
}

int handle_ORIGIN (char *args, struct name_context *context,
					struct signing_options *options, struct error_data *err)
{
	int			index = 0;
	u_int8_t	old_origin_n[MAXDNAME];
	u_int8_t	new_origin_n[MAXDNAME];

	while (IS_BLANK(args, index)) index++;

	if (IS_EOLN(args,index))
	{
		sprintf (debug_message, "%s[%d]: Missing name for $ORIGIN",
			err->ed_filename, err->ed_curr_line);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	memcpy (old_origin_n, context->nc_origin_n, MAXDNAME);

	if (get_name_n (args, &index, old_origin_n, new_origin_n, options)
														== SIGNER_ERROR)
		return SIGNER_ERROR;

	/*
		If there is no garbage in the line, copy the new origin to the
		context, then check to see if it was fully qualified.
	*/

	while (IS_BLANK(args, index)) index++;
	if (IS_EOLN(args, index))
		memcpy (context->nc_origin_n, new_origin_n,
								wire_name_length(new_origin_n));
	else
	{
		sprintf (debug_message, "%s[%d]: Garbage at end of $ORIGIN : %s",
			err->ed_filename, err->ed_curr_line, &args[index]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	options->so_origin_set = TRUE;

	return SIGNER_OK;
}

int handle_PARENT (char *args, struct name_context *context,
					struct signing_options *options)
{
	int			index = 0;
	u_int8_t	new_parent_n[MAXDNAME];

	memset (new_parent_n, 0, MAXDNAME);

	while (IS_BLANK(args, index)) index++;

	if (args[index]=='\n')
	{
		ERROR ("Missing name for $PARENT");
		return SIGNER_ERROR;
	}

	if (get_name_n (args, &index, options->so_origin_n, new_parent_n, options)
																== SIGNER_ERROR)
		return SIGNER_ERROR;

	/*
		If there is no garbage in the line, copy the new origin to the
		context, then check to see if it was fully qualified.
	*/

	while (IS_BLANK(args, index)) index++;
	if (!IS_EOLN(args, index))
	{
		sprintf (debug_message, "Garbage at end of $PARENT : %s", &args[index]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	if (options->so_parent_name_n != NULL)
	{
		if (namecmp(options->so_parent_name_n, new_parent_n) == 0)
		{
			WARN ("$PARENT found again - but has same name value");
			return SIGNER_OK;
		}
		else
		{
			wire_to_ascii_name (debug_name1_h, new_parent_n, MAXPNAME);
			wire_to_ascii_name (debug_name2_h, options->so_parent_name_n,
																	MAXPNAME);
			sprintf ("Resetting $PARENT name to %s, originally %s",
											debug_name1_h, debug_name2_h);
			return SIGNER_ERROR;
		}
	}

	index = wire_name_length(new_parent_n);

	options->so_parent_name_n = (u_int8_t *) MALLOC (index);

	if (options->so_parent_name_n==NULL)
	{
		ERROR ("Out of memory - exiting");
		tree_delete_parent_files (options);
		sign_cleanup (options);
		exit (1);
	}

	memcpy (options->so_parent_name_n, new_parent_n, index);

	return SIGNER_OK;
}

int handle_TTL (char *args, struct name_context *context,
					struct signing_options *options, struct error_data *err)
{
	int			index = 0;
	u_long	new_ttl;

	while (IS_BLANK(args, index)) index++;

	if (IS_EOLN(args,index))
	{
		sprintf (debug_message, "%s[%d]: Missing value for $TTL",
				err->ed_filename, err->ed_curr_line);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	if (ns_parse_ttl(args, &new_ttl) != 0)
	{
		sprintf (debug_message, "%s[%d]: Bad value for $TTL: %s",
			err->ed_filename, err->ed_curr_line, args);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	/* Skip the TTL string */
	while (!IS_BLANK(args, index) && !IS_EOLN(args, index)) index++;

	/* If there is no garbage in the line, save the new ttl */
	while (IS_BLANK(args, index)) index++;

	if (IS_EOLN(args, index))
	{
		options->so_ttl_n = htonl(new_ttl);
		context->nc_min_ttl_n = htonl(new_ttl);
		options->so_ttl_set = TRUE;
	}
	else
	{
		sprintf (debug_message, "%s[%d]: Garbage at end of $TTL : %s",
			err->ed_filename, err->ed_curr_line, &args[index]);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}

	return SIGNER_OK;
}

int get_key (char *args, int *index, u_int8_t *name_n, u_int8_t *origin_n,
			int *algorithm_any, u_int8_t *algorithm, int *foot_any,
			u_int16_t *foot_h, struct signing_options *options)
{
	int	number;

	while (IS_BLANK(args, *index)) (*index)++;

	if (IS_EOLN (args, *index)) return FALSE;

	if (get_name_n (args, index, origin_n, name_n, options) == SIGNER_ERROR)
		return SIGNER_ERROR;

	while (IS_BLANK(args, *index)) (*index)++;

	/* Read in the algorithm */

	if (IS_EOLN (args, *index))
	{
		ERROR ("Missing algorithm for key");
		return SIGNER_ERROR;
	}

	if (isdigit (args[*index]))
	{
		/* Assume this is a algorithm */

		number = atoi (&args[*index]);
		while (isdigit(args[*index])) (*index)++;

		if (number>=VALUE_ALGORITHM_MINIMUM && number<=VALUE_ALGORITHM_MAXIMUM)
		{
			*algorithm = (u_int8_t) number;
			*algorithm_any = FALSE;
		}
		else
		{
			sprintf(debug_message,"Illegal algorithm value (%d)for key",number);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
	}
	else
	{
		/* This better be a "ANY" */
		int any_len = strlen ("ANY");

		if (strncasecmp (&args[*index], "ANY", any_len) == 0 &&
			(IS_BLANK (args,*index+any_len) || IS_EOLN (args, *index+any_len)))
		{
			*algorithm_any = TRUE;
			*algorithm = 0;
		}
		else if (strncasecmp (&args[*index],"dss", 2) == 0)
		{
			*algorithm_any = FALSE;
			*algorithm = 3;
		}
		else if (strncasecmp (&args[*index],"rsaref", 3) == 0)
		{
			*algorithm_any = FALSE;
			*algorithm = 1;
		}
		else if (strncasecmp (&args[*index],"dh", 2) == 0)
		{
			*algorithm_any = FALSE;
			*algorithm = 2;
		}
		else if (strncasecmp (&args[*index],"private", 7) == 0)
		{
			*algorithm_any = FALSE;
			*algorithm = 254;
		}
		else
		{
			sprintf(debug_message,"Illegal algorithm value (%s)for key",
																&args[*index]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}

		while (!IS_BLANK(args, *index)) (*index)++;
	}

	while (IS_BLANK(args, *index)) (*index)++;

	/* Read in the footprint */

	if (IS_EOLN (args, *index))
	{
		ERROR ("Missing footprint for key");
		return SIGNER_ERROR;
	}

	if (isdigit (args[*index]))
	{
		/* Assume this is a footprint */

		number = atoi (&args[*index]);
		while (isdigit(args[*index])) (*index)++;

		if (number>=VALUE_FOOTPRINT_MINIMUM && number<=VALUE_FOOTPRINT_MAXIMUM)
		{
			*foot_h = (u_int16_t) number;
			*foot_any = FALSE;
			return TRUE;
		}
		else
		{
			sprintf(debug_message,"Illegal footprint value (%d)for key",number);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
	}
	else
	{
		/* This better be a "ANY" */
		int any_len = strlen ("ANY");

		if (strncasecmp (&args[*index], "ANY", any_len) == 0 &&
			(IS_BLANK (args,*index+any_len) || IS_EOLN (args, *index+any_len)))
		{
			*index += any_len;
			*foot_any = TRUE;
			return TRUE;
		}
		else
		{
			sprintf(debug_message,"Illegal footprint value (%s)for key",
																&args[*index]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
	}
}

int handle_SIGNER_ADD(char *args,u_int8_t *origin_n,struct key_list **key_list,
						struct signing_options *options)
{
	u_int8_t		name_n[MAXDNAME];
	int				foot_any;
	u_int16_t		foot_h;
	int				algorithm_any;
	u_int8_t		algorithm;
	int				ret_val;

	int index = 0;

	while ((ret_val = get_key (args, &index, name_n, origin_n,
			&algorithm_any, &algorithm, &foot_any, &foot_h, options))==TRUE)
	{
		if (key_add_to_list (name_n, algorithm_any, algorithm, foot_any,
								foot_h, key_list, options) == SIGNER_ERROR)
			return SIGNER_ERROR;
	}

	if (ret_val == SIGNER_ERROR)
		return SIGNER_ERROR;

	return SIGNER_OK;
}

int handle_SIGNER_DEL(char *args,u_int8_t *origin_n,struct key_list **key_list,
						struct signing_options *options)
{
	u_int8_t		name_n[MAXDNAME];
	int				algorithm_any;
	u_int8_t		algorithm;
	int				foot_any;
	u_int16_t		foot_h;
	int				ret_val;

	int index = 0;

	while ((ret_val = get_key (args, &index, name_n, origin_n,
									&algorithm_any, &algorithm,
									&foot_any, &foot_h, options)) == TRUE)
	{
		key_remove_from_list (name_n, algorithm_any, algorithm, foot_any,
															foot_h, key_list);
	}

	if (ret_val == SIGNER_ERROR) return SIGNER_ERROR;

	return SIGNER_OK;
}

int handle_SIGNER_SET(char *args,u_int8_t *origin_n,struct key_list **key_list,
						struct signing_options *options)
{
	key_list_delete (key_list);

	return handle_SIGNER_ADD (args, origin_n, key_list, options);
}

int handle_SIGNER (char *args, u_int8_t *origin_n, struct key_list **key_list,
						struct signing_options *options, struct error_data *err)
{
	int index = 0;

	while (IS_BLANK(args, index)) index++;

	if (strncasecmp(&args[index], "SET", 3) == 0 &&
				(IS_EOLN(args,index + 3) || IS_BLANK(args,index + 3)))
		return handle_SIGNER_SET (&args[index + 3],origin_n,key_list,options);
	else if (strncasecmp(&args[index], "ADD", 3) == 0 &&
				(IS_EOLN(args,index + 3) || IS_BLANK(args,index + 3)))
		return handle_SIGNER_ADD (&args[index + 3], origin_n,key_list,options);
	else if (strncasecmp(&args[index], "DEL", 3) == 0 &&
				(IS_EOLN(args,index + 3) || IS_BLANK(args,index + 3)))
		return handle_SIGNER_DEL (&args[index + 3], origin_n,key_list,options);
	else
	{
		if (!IS_EOLN(args, index))
		sprintf (debug_message, "%s[&d]: Unknown SIGNER directive modifier %s",
			err->ed_filename, err->ed_curr_line, &args[index]);
		else
		sprintf (debug_message, "%s[&d]: Missing SIGNER directive modifier",
			err->ed_filename, err->ed_curr_line);

		ERROR (debug_message);
		return SIGNER_ERROR;
	}
}

int handle_directive (	char					*non_rr,
						struct name_context		*context,
						struct error_data		*err,
						struct key_list			**key_list,
						struct signing_options	*options
					)
{
	int index = 1; /* non_rr[0] must be a '$' */
	int signer_len = strlen ("SIGNER");
	int origin_len = strlen ("ORIGIN");
	int include_len = strlen ("INCLUDE");
	int parent_len = strlen ("PARENT");
	int ttl_len = strlen ("TTL");

	while (IS_BLANK(non_rr, index)) index++;

	if (strncmp (&non_rr[index], "SIGNER", signer_len) == 0)
	{
		index += signer_len;
		if (!IS_EOLN(non_rr,index) && !IS_BLANK(non_rr, index))
		{
			sprintf (debug_message, "%s[%d]: Unknown directive %s",
					err->ed_filename, err->ed_curr_line, &non_rr[index]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
		index++;

		return handle_SIGNER (&non_rr[index], context->nc_origin_n, key_list,
								options, err);
	}
	else if (strncmp (&non_rr[index], "ORIGIN", origin_len) == 0)
	{
		index += origin_len;
		if (!IS_EOLN(non_rr,index) && !IS_BLANK(non_rr, index))
		{
			sprintf (debug_message, "%s[%d]: Unknown directive %s",
					err->ed_filename, err->ed_curr_line, &non_rr[index]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
		index++;

		return handle_ORIGIN (&non_rr[index], context, options, err);
	}
	else if (strncmp (&non_rr[index], "PARENT", parent_len) == 0)
	{
		index += parent_len;
		if (!IS_EOLN(non_rr,index) && !IS_BLANK(non_rr, index))
		{
			sprintf (debug_message, "%s[%d]: Unknown directive %s",
					err->ed_filename, err->ed_curr_line, &non_rr[index]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
		index++;

		return handle_PARENT (&non_rr[index], context, options);
	}
	else if (strncmp (&non_rr[index], "INCLUDE", include_len) == 0)
	{
		index += include_len;
		if (!IS_EOLN(non_rr,index) && !IS_BLANK(non_rr, index))
		{
			sprintf (debug_message, "%s[%d]: Unknown directive %s",
					err->ed_filename, err->ed_curr_line, &non_rr[index]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
		index++;

		return handle_INCLUDE(&non_rr[index], context, *key_list, options, err);
	}
	else if (strncmp (&non_rr[index], "TTL", ttl_len) == 0)
	{
		index += ttl_len;
		if (!IS_EOLN(non_rr,index) && !IS_BLANK(non_rr, index))
		{
			sprintf (debug_message, "%s[%d]: Unknown directive %s",
					err->ed_filename, err->ed_curr_line, &non_rr[index]);
			ERROR (debug_message);
			return SIGNER_ERROR;
		}
		index++;

		return handle_TTL (&non_rr[index], context, options, err);
	}
	else
	{
		sprintf (debug_message, "%s[%d]: Directive not understood '%s'\n",
					err->ed_filename, err->ed_curr_line, non_rr);
		ERROR (debug_message);
		return SIGNER_ERROR;
	}
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




