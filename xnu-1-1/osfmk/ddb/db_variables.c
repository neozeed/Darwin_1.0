/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/*
 * HISTORY
 * 
 * Revision 1.1.1.1  1998/09/22 21:05:48  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:26:09  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.2.18.5  1996/01/09  19:16:34  devrcs
 * 	Search the alternate register names if configured
 * 	Changed declarations of 'register foo' to 'register int foo'.
 * 	[1995/12/01  21:42:42  jfraser]
 *
 * 	Merged '64-bit safe' changes from DEC alpha port.
 * 	[1995/11/21  18:03:56  jfraser]
 *
 * Revision 1.2.18.4  1995/02/23  21:43:56  alanl
 * 	Merged with DIPC2_SHARED.
 * 	[1995/01/05  13:35:55  alanl]
 * 
 * Revision 1.2.21.1  1994/11/04  09:53:26  dwm
 * 	mk6 CR668 - 1.3b26 merge
 * 	* Revision 1.2.4.6  1994/05/06  18:40:13  tmt
 * 	Merged osc1.3dec/shared with osc1.3b19
 * 	Merge Alpha changes into osc1.312b source code.
 * 	64bit cleanup.
 * 	* End1.3merge
 * 	[1994/11/04  08:50:12  dwm]
 * 
 * Revision 1.2.18.2  1994/09/23  01:22:35  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:11:24  ezf]
 * 
 * Revision 1.2.18.1  1994/06/11  21:12:37  bolinger
 * 	Merge up to NMK17.2.
 * 	[1994/06/11  20:03:04  bolinger]
 * 
 * Revision 1.2.23.1  1994/12/06  19:43:18  alanl
 * 	Intel merge, Oct 94 code drop.
 * 	Added db_find_reg_name (came from db_print.c).
 * 	[94/11/28            mmp]
 * 
 * Revision 1.2.16.1  1994/02/08  10:59:08  bernadat
 * 	Added completion variable.
 * 	[93/08/17            paire]
 * 
 * 	Set up new fields (hidden_xxx) of db_vars[] array that are supposed
 * 	to be helpful to display variables depending on an internal value
 * 	like db_macro_level for macro arguments.
 * 	Added db_auto_wrap as new variable.
 * 	Added "set help" for listing all available variables.
 * 	Added db_show_variable() and db_show_one_variable()
 * 	to print variable values.
 * 	[93/08/12            paire]
 * 	[94/02/08            bernadat]
 * 
 * Revision 1.2.4.4  1993/08/11  20:38:20  elliston
 * 	Add ANSI Prototypes.  CR #9523.
 * 	[1993/08/11  03:34:13  elliston]
 * 
 * Revision 1.2.4.3  1993/07/27  18:28:27  elliston
 * 	Add ANSI prototypes.  CR #9523.
 * 	[1993/07/27  18:13:22  elliston]
 * 
 * Revision 1.2.4.2  1993/06/09  02:21:02  gm
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  20:57:43  jeffc]
 * 
 * Revision 1.2  1993/04/19  16:03:25  devrcs
 * 	Changes from mk78:
 * 	Added void to db_read_write_variable().
 * 	Removed unused variable 'func' from db_set_cmd().
 * 	[92/05/16            jfriedl]
 * 	[93/02/02            bruel]
 * 
 * 	Print old value when changing register values.
 * 	[barbou@gr.osf.org]
 * 	[92/12/03            bernadat]
 * 
 * Revision 1.1  1992/09/30  02:01:31  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.5  91/10/09  16:03:59  af
 * 	 Revision 2.4.3.1  91/10/05  13:08:27  jeffreyh
 * 	 	Added suffix handling and thread handling of variables.
 * 		Added new variables: lines, task, thread, work and arg.
 * 		Moved db_read_variable and db_write_variable to db_variables.h
 * 	 	  as macros, and added db_read_write_variable instead.
 * 	 	Changed some error messages.
 * 	 	[91/08/29            tak]
 * 
 * Revision 2.4.3.1  91/10/05  13:08:27  jeffreyh
 * 	Added suffix handling and thread handling of variables.
 * 	Added new variables: lines, task, thread, work and arg.
 * 	Moved db_read_variable and db_write_variable to db_variables.h
 * 	  as macros, and added db_read_write_variable instead.
 * 	Changed some error messages.
 * 	[91/08/29            tak]
 * 
 * Revision 2.4  91/05/14  15:36:57  mrt
 * 	Correcting copyright
 * 
 * Revision 2.3  91/02/05  17:07:19  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:19:46  mrt]
 * 
 * Revision 2.2  90/08/27  21:53:24  dbg
 * 	New db_read/write_variable functions.  Should be used instead
 * 	of dereferencing valuep directly, which might not be a true
 * 	pointer if there is an fcn() access function.
 * 	[90/08/20            af]
 * 
 * 	Fix declarations.
 * 	Check for trailing garbage after last expression on command line.
 * 	[90/08/10  14:34:54  dbg]
 * 
 * 	Created.
 * 	[90/07/25            dbg]
 * 
 */
/* CMU_ENDHIST */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include <machine/db_machdep.h>
#include <string.h>			/* For strcpy() */

#include <ddb/db_lex.h>
#include <ddb/db_variables.h>
#include <ddb/db_task_thread.h>
#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_expr.h>
#include <ddb/db_macro.h>
#include <ddb/db_output.h>		/* For db_printf() */

extern db_expr_t	db_radix;
extern db_expr_t	db_max_width;
extern db_expr_t	db_tab_stop_width;
extern db_expr_t	db_max_line;
extern db_expr_t	db_auto_wrap;
extern db_expr_t	db_macro_level;
extern db_expr_t	db_auto_completion;

#define DB_NWORK	32		/* number of work variable */

db_expr_t	db_work[DB_NWORK];	/* work variable */

struct db_variable db_vars[] = {
	{ "maxoff",	(db_expr_t*)&db_maxoff,	FCN_NULL },
	{ "autowrap",	&db_auto_wrap,		FCN_NULL },
	{ "completion",	&db_auto_completion,	FCN_NULL },
	{ "maxwidth",	&db_max_width,		FCN_NULL },
	{ "radix",	&db_radix,		FCN_NULL },
	{ "tabstops",	&db_tab_stop_width,	FCN_NULL },
	{ "lines",	&db_max_line,		FCN_NULL },
	{ "thr_act",	0,			db_set_default_act	},
	{ "task",	0,			db_get_task_act,
	  1,		2,			-1,	-1		},
	{ "work",	&db_work[0],		FCN_NULL,
	  1,		1,			0,	DB_NWORK-1	},
	{ "arg",	0,			db_arg_variable,
	  1,		1,			1,	DB_MACRO_NARGS,
	  1,		0,	DB_MACRO_LEVEL-1,	(int *)&db_macro_level	},
};
struct db_variable *db_evars = db_vars + sizeof(db_vars)/sizeof(db_vars[0]);



/* Prototypes for functions local to this file.
 */

static char *db_get_suffix(
	register char	*suffix,
	short		*suffix_value);

static boolean_t db_cmp_variable_name(
	struct db_variable		*vp,
	char				*name,
	register db_var_aux_param_t	ap);

static int db_find_variable(
	struct db_variable	**varp,
	db_var_aux_param_t	ap);

static int db_set_variable(db_expr_t value);

void db_list_variable(void);

static char *
db_get_suffix(
	register char	*suffix,
	short		*suffix_value)
{
	register int value;

	for (value = 0; *suffix && *suffix != '.' && *suffix != ':'; suffix++) {
	    if (*suffix < '0' || *suffix > '9')
		return(0);
	    value = value*10 + *suffix - '0';
	}
	*suffix_value = value;
	if (*suffix == '.')
	    suffix++;
	return(suffix);
}

static boolean_t
db_cmp_variable_name(
	struct db_variable		*vp,
	char				*name,
	register db_var_aux_param_t	ap)
{
	register char *var_np, *np;
	register int level;
	
	for (np = name, var_np = vp->name; *var_np; ) {
	    if (*np++ != *var_np++)
		return(FALSE);
	}
	for (level = 0; *np && *np != ':' && level < vp->max_level; level++){
	    if ((np = db_get_suffix(np, &ap->suffix[level])) == 0)
		return(FALSE);
	}
	if ((*np && *np != ':') || level < vp->min_level
	    || (level > 0 && (ap->suffix[0] < vp->low 
		  	      || (vp->high >= 0 && ap->suffix[0] > vp->high))))
	    return(FALSE);
	strcpy(ap->modif, (*np)? np+1: "");
	ap->thr_act = (db_option(ap->modif, 't')?db_default_act: THR_ACT_NULL);
	ap->level = level;
	ap->hidden_level = -1;
	return(TRUE);
}

static int
db_find_variable(
	struct db_variable	**varp,
	db_var_aux_param_t	ap)
{
	int	t;
	struct db_variable *vp;

	t = db_read_token();
	if (t == tIDENT) {
	    for (vp = db_vars; vp < db_evars; vp++) {
		if (db_cmp_variable_name(vp, db_tok_string, ap)) {
		    *varp = vp;
		    return (1);
		}
	    }
	    for (vp = db_regs; vp < db_eregs; vp++) {
		if (db_cmp_variable_name(vp, db_tok_string, ap)) {
		    *varp = vp;
		    return (1);
		}
	    }
#if defined(ALTERNATE_REGISTER_DEFS)
	    for (vp = db_altregs; vp < db_ealtregs; vp++) {
		if (db_cmp_variable_name(vp, db_tok_string, ap)) {
		    *varp = vp;
		    return (1);
		}
	    }
#endif /* defined(ALTERNATE_REGISTER_DEFS) */
	}
	db_printf("Unknown variable \"$%s\"\n", db_tok_string);
	db_error(0);
	return (0);
}

int
db_get_variable(db_expr_t *valuep)
{
	struct db_variable *vp;
	struct db_var_aux_param aux_param;
	char		modif[TOK_STRING_SIZE];

	aux_param.modif = modif;
	if (!db_find_variable(&vp, &aux_param))
	    return (0);

	db_read_write_variable(vp, valuep, DB_VAR_GET, &aux_param);

	return (1);
}

static int
db_set_variable(db_expr_t value)
{
	struct db_variable *vp;
	struct db_var_aux_param aux_param;
	char		modif[TOK_STRING_SIZE];

	aux_param.modif = modif;
	if (!db_find_variable(&vp, &aux_param))
	    return (0);

	db_read_write_variable(vp, &value, DB_VAR_SET, &aux_param);

	return (1);
}

void
db_read_write_variable(
	struct db_variable	*vp,
	db_expr_t		*valuep,
	int 			rw_flag,
	db_var_aux_param_t	ap)
{
	int	(*func)(struct db_variable*, db_expr_t*,int, db_var_aux_param_t)
			= vp->fcn;
	struct  db_var_aux_param aux_param;
	db_expr_t old_value;

	if (ap == 0) {
	    ap = &aux_param;
	    ap->modif = "";
	    ap->level = 0;
	    ap->thr_act = THR_ACT_NULL;
	}
	if (rw_flag == DB_VAR_SET && vp->precious)
		db_read_write_variable(vp, &old_value, DB_VAR_GET, ap);
	if (func == FCN_NULL) {
	    if (rw_flag == DB_VAR_SET)
	        vp->valuep[(ap->level)? (ap->suffix[0] - vp->low): 0] = *valuep;
	    else
	        *valuep = vp->valuep[(ap->level)? (ap->suffix[0] - vp->low): 0];
	} else
	    (*func)(vp, valuep, rw_flag, ap);
	if (rw_flag == DB_VAR_SET && vp->precious)
		db_printf("\t$%s:%s<%#x>\t%#8n\t=\t%#8n\n", vp->name,
			  ap->modif, ap->thr_act, old_value, *valuep);
}

void
db_list_variable(void)
{
	register struct db_variable *new;
	register struct db_variable *old;
	register struct db_variable *cur;
	unsigned int l;
	unsigned int len;
	short i;
	unsigned int j;

	len = 1;
	for (cur = db_vars; cur < db_evars; cur++) {
	    if (cur->min_level > 0 || cur->max_level > 0) {
		j = 3 * (cur->max_level - cur->min_level + 1) - 1;
		if (cur->max_level > cur->min_level)
		    j += 2;
	    } else
		j = 0;
	    if ((l = strlen(cur->name) + j) >= len)
		len = l + 1;
	}

	old = (struct db_variable *)0;
	for (;;) {
	    new = (struct db_variable *)0;
	    for (cur = db_vars; cur < db_evars; cur++)
		if ((new == (struct db_variable *)0 ||
		     strcmp(cur->name, new->name) < 0) &&
		    (old == (struct db_variable *)0 ||
		     strcmp(cur->name, old->name) > 0))
		    new = cur;
	    if (new == (struct db_variable *)0)
		    return;
	    db_reserve_output_position(len);
	    db_printf(new->name);
	    j = strlen(new->name);
	    if (new->min_level > 0) {
		db_putchar('?');
		db_putchar('?');
		j += 2;
		for (i = new->min_level - 1; i > 0; i--) {
		    db_putchar('.');
		    db_putchar('?');
		    db_putchar('?');
		    j += 3;
		}
		if (new->max_level > new->min_level) {
		    db_putchar('[');
		    db_putchar('.');
		    db_putchar('?');
		    db_putchar('?');
		    j += 4;
		}
		i = new->min_level + 1;
	    } else {
		if (new->max_level > new->min_level) {
		    db_putchar('[');
		    j++;
		}
		i = new->min_level;
	    }
	    while (i++ < new->max_level) {
		 db_putchar('.');
		 db_putchar('?');
		 db_putchar('?');
		 j += 3;
	    }
	    if (new->max_level > new->min_level) {
		 db_putchar(']');
		 j++;
	    }
	    while (j++ < len)
		    db_putchar(' ');
	    old = new;
	}
}

void
db_set_cmd(void)
{
	db_expr_t	value;
	int		t;
	struct db_variable *vp;
	struct db_var_aux_param aux_param;
	char		modif[TOK_STRING_SIZE];

	aux_param.modif = modif;
	t = db_read_token();
	if (t == tIDENT && strcmp("help", db_tok_string) == 0) {
		db_list_variable();
		return;
	}
	if (t != tDOLLAR) {
	    db_error("Variable name should be prefixed with $\n");
	    return;
	}
	if (!db_find_variable(&vp, &aux_param)) {
	    db_error("Unknown variable\n");
	    return;
	}

	t = db_read_token();
	if (t != tEQ)
	    db_unread_token(t);

	if (!db_expression(&value)) {
	    db_error("No value\n");
	    return;
	}
	if ((t = db_read_token()) == tSEMI_COLON)
	    db_unread_token(t);
	else if (t != tEOL)
	    db_error("?\n");

	db_read_write_variable(vp, &value, DB_VAR_SET, &aux_param);
}

void
db_show_one_variable(void)
{
	struct db_variable *cur;
	unsigned int len;
	unsigned int sl;
	unsigned int slen;
	short h;
	short i;
	short j;
	short k;
	short low;
	int hidden_level;
	struct db_var_aux_param aux_param;
	char *p;
	char *q;
	char *name;
	db_addr_t offset;

	for (cur = db_vars; cur < db_evars; cur++)
	    if (db_cmp_variable_name(cur, db_tok_string, &aux_param))
		break;
	if (cur == db_evars) {
	    for (cur = db_vars; cur < db_evars; cur++) {
		for (q = cur->name, p = db_tok_string; *q && *p == *q; p++,q++)
		    continue;
		if (*q == '\0')
		    break;
	    }
	    if (cur == db_evars) {
		db_error("Unknown variable\n");
		return;
	    }

	    for (i = 0; *p && *p != ':' && i < cur->max_level; i++, p = q)
		if ((q = db_get_suffix(p, &aux_param.suffix[i])) == 0)
		    break;
	    aux_param.level = i;
	    if ((*p && *p != ':') ||
		(i > 0 && (aux_param.suffix[0] < cur->low  ||
			   (cur->high >= 0 &&
			    aux_param.suffix[0] > cur->high)))) {
		db_error("Unknown variable format\n");
		return;
	    }

	    strcpy(aux_param.modif, *p ? p + 1 : "");
	    aux_param.thr_act = (db_option(aux_param.modif, 't') ?
			db_default_act : THR_ACT_NULL);
	}

	if (cur->hidden_level)
	    if (*cur->hidden_levelp >= cur->hidden_low &&
		*cur->hidden_levelp <= cur->hidden_high) {
		hidden_level = 1;
		aux_param.hidden_level = h = *cur->hidden_levelp;
	    } else {
		hidden_level = 0;
		aux_param.hidden_level = h = cur->hidden_low;
		slen = 1;
		for (k = aux_param.level > 0 ? aux_param.suffix[0] : cur->high;
		     k > 9; k /= 10)
		    slen++;
	    }
	else
	    aux_param.hidden_level = -1;

	if ((cur->min_level == 0 && !cur->hidden_level) || cur->high < 0)
	    j = 0;
	else {
	    if (cur->min_level > 0) {
		j = 1;
		for (k = aux_param.level > 0 ?
		     aux_param.suffix[0] : cur->high; k > 9; k /= 10)
		    j++;
	    } else
		j = 0;
	    if (cur->hidden_level && hidden_level == 0) {
		j += 3;
		for (k = aux_param.hidden_level >= 0 ?
		     aux_param.hidden_level : cur->hidden_high; k > 9; k /= 10)
		    j++;
	    }
	}
	len = strlen(cur->name) + j;
	i = low = aux_param.level > 0 ? aux_param.suffix[0] : cur->low;

	for (;;) {
	    db_printf(cur->name);
	    j = strlen(cur->name);
	    if (cur->high >= 0) {
		if (cur->min_level > 0) {
		    db_printf("%d", i);
		    j++;
		    for (k = i; k > 9; k /= 10)
			j++;
		}
		if (cur->hidden_level && hidden_level == 0) {
		    sl = 1;
		    for (k = i; k > 9; k /= 10)
			sl++;
		    while (sl++ < slen) {
			db_putchar(' ');
			j++;
		    }
		    db_printf("[%d]", h);
		    j += 3;
		    for (k = h; k > 9; k /= 10)
			j++;
		}
	    }

	    while (j++ < len)
		db_putchar(' ');
	    db_putchar(':');
	    db_putchar(' ');

	    if (cur->fcn) {
		aux_param.suffix[0] = i;
		(*cur->fcn)(cur, (db_expr_t *)0, DB_VAR_SHOW, &aux_param);
	    } else {
		db_printf("%#n", *(cur->valuep + i));
	        db_find_xtrn_task_sym_and_offset(*(cur->valuep + i), &name,
						 &offset, TASK_NULL);
		if (name != (char *)0 && offset <= db_maxoff &&
		    offset != *(cur->valuep + i)) {
		    db_printf("\t%s", name);
		    if (offset != 0)
			db_printf("+%#r", offset);
		}
	    }
	    db_putchar('\n');
	    if (cur->high < 0)
		break;
	    if (aux_param.level > 0 || i++ == cur->high) {
		if (!cur->hidden_level ||
		    hidden_level == 0 ||
		    h++ == cur->hidden_high)
		    break;
		aux_param.hidden_level = h;
		i = low;
	    }
	}
}

void
db_show_variable(void)
{
	struct db_variable *cur;
	unsigned int l;
	unsigned int len;
	unsigned int sl;
	unsigned int slen;
	short h;
	short i;
	short j;
	short k;
	int t;
	int t1;
	struct db_var_aux_param aux_param;
	char *name;
	db_addr_t offset;

	switch(t = db_read_token()) {
	case tEOL:
	case tEOF:
	case tSEMI_COLON:
	    break;

	case tDOLLAR:
	    t1 = db_read_token();
	    if (t1 == tIDENT) {
		db_show_one_variable();
		return;
	    }
	    db_error("Not a variable name after $\n");
	    db_unread_token(t);
	    return;

	default:
	    db_error("Variable name should be prefixed with $\n");
	    db_unread_token(t);
	    return;
	}
	db_unread_token(t);

	slen = len = 1;
	for (cur = db_vars; cur < db_evars; cur++) {
	    if ((cur->min_level == 0 && !cur->hidden_level) || cur->high < 0)
		j = 0;
	    else {
		if (cur->min_level > 0) {
		    j = 1;
		    for (k = cur->high; k > 9; k /= 10)
			j++;
		} else
		    j = 0;
		if (cur->hidden_level &&
		    (*cur->hidden_levelp < cur->hidden_low ||
		     *cur->hidden_levelp > cur->hidden_high)) {
		    j += 3;
		    for (k = cur->hidden_high; k > 9; k /= 10)
			j++;
		}
	    }
	    if ((l = strlen(cur->name) + j) >= len)
		len = l + 1;
	}

	aux_param.modif = "";
	aux_param.level = 1;
	aux_param.thr_act = THR_ACT_NULL;

	for (cur = db_vars; cur < db_evars; cur++) {
	    i = cur->low;
	    if (cur->hidden_level) {
		if (*cur->hidden_levelp >= cur->hidden_low &&
		    *cur->hidden_levelp <= cur->hidden_high) {
		    h = cur->hidden_low - 1;
		    aux_param.hidden_level = *cur->hidden_levelp;
		} else {
		    h = cur->hidden_low;
		    aux_param.hidden_level = cur->hidden_low;
		}
		slen = 1;
		for (k = cur->high; k > 9; k /= 10)
		    slen++;
	    } else
		aux_param.hidden_level = -1;

	    if (cur != db_vars && cur->high >= 0 &&
		(cur->min_level > 0 || cur->hidden_level))
		    db_putchar('\n');

	    for (;;) {
		db_printf(cur->name);
		j = strlen(cur->name);
		if (cur->high >= 0) {
		    if (cur->min_level > 0) {
			db_printf("%d", i);
			j++;
			for (k = i; k > 9; k /= 10)
			    j++;
		    }
		    if (cur->hidden_level && h >= cur->hidden_low) {
			sl = 1;
			for (k = i; k > 9; k /= 10)
			    sl++;
			while (sl++ < slen) {
			    db_putchar(' ');
			    j++;
			}
			db_printf("[%d]", h);
			j += 3;
			for (k = h; k > 9; k /= 10)
			    j++;
		    }
		}
		while (j++ < len)
		    db_putchar(' ');
		db_putchar(':');
		db_putchar(' ');

		if (cur->fcn) {
		    aux_param.suffix[0] = i;
		    (*cur->fcn)(cur, (db_expr_t *)0, DB_VAR_SHOW, &aux_param);
		} else {
		    db_printf("%#n", *(cur->valuep + i));
		    db_find_xtrn_task_sym_and_offset(*(cur->valuep + i), &name,
						     &offset, TASK_NULL);
		    if (name != (char *)0 && offset <= db_maxoff &&
			offset != *(cur->valuep + i)) {
			db_printf("\t%s", name);
			if (offset != 0)
			    db_printf("+%#r", offset);
		    }
		}
		db_putchar('\n');
		if (cur->high < 0)
		    break;
		if (i++ == cur->high) {
		    if (!cur->hidden_level || h++ == cur->hidden_high)
			break;
		    aux_param.hidden_level = h;
		    i = cur->low;
		}
	    }
	}
}

/*
 * given a name of a machine register, return a variable pointer to it.
 */
db_variable_t
db_find_reg_name(
	char	*s)
{
	register db_variable_t	regp;

	if ( s == (char *)0 )
		return DB_VAR_NULL;

	for (regp = db_regs; regp < db_eregs; regp++) {
		if ( strcmp( s, regp->name) == 0 )
			return regp;
	}
	return DB_VAR_NULL;
}
