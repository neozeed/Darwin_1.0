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
 * Revision 1.3.17.6  1996/01/09  19:15:57  devrcs
 * 	In db_more(), if using kdebug on Alpha, don't get input.
 * 	[1995/12/01  21:42:17  jfraser]
 *
 * 	Merged '64-bit safe' changes from DEC alpha port.
 * 	[1995/11/21  18:03:19  jfraser]
 *
 * Revision 1.3.17.5  1995/08/21  20:29:18  devrcs
 * 	Move the include out of db_output_prompt
 * 	[1995/07/20  19:15:52  travos]
 * 
 * Revision 1.3.17.4  1995/02/23  21:43:30  alanl
 * 	Merge with DIPC2_SHARED.
 * 	[95/01/04            alanl]
 * 
 * Revision 1.3.22.1  1994/11/04  09:52:55  dwm
 * 	mk6 CR668 - 1.3b26 merge
 * 	* Revision 1.3.4.6  1994/05/06  18:39:31  tmt
 * 	Merged osc1.3dec/shared with osc1.3b19
 * 	Merge Alpha changes into osc1.312b source code.
 * 	removed db_radix declaration.
 * 	* End1.3merge
 * 	[1994/11/04  08:49:42  dwm]
 * 
 * Revision 1.3.17.2  1994/09/23  01:20:36  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:10:32  ezf]
 * 
 * Revision 1.3.17.1  1994/06/11  21:11:56  bolinger
 * 	Merge up to NMK17.2.
 * 	[1994/06/11  20:02:01  bolinger]
 * 
 * Revision 1.3.19.3  1994/12/06  19:42:52  alanl
 * 	Change _node_self ref to KKT_NODE_SELF().
 * 	[94/12/05            rwd]
 * 
 * Revision 1.3.19.2  1994/09/01  12:48:39  alanl
 * 	Remove dead NORMA_IPC conditional.
 * 	[94/08/29            alanl]
 * 
 * Revision 1.3.19.1  1994/08/04  01:42:27  mmp
 * 	Added DIPC support for debugger prompt.
 * 	[1994/08/03  13:37:21  mmp]
 * 
 * Revision 1.3.15.1  1994/02/08  10:58:09  bernadat
 * 	Added db_output_prompt().
 * 	[93/08/12            paire]
 * 
 * 	Fixed db_end_line() for generating a pseudo end of line.
 * 	[93/09/29            paire]
 * 
 * 	Added db_reset_more() to restart more function.
 * 	Added db_last_gen_return variable to determine if a '\n' has to be
 * 	output (no if precedeed by a generated '\n').
 * 	Added db_auto_wrap variable to define if the terminal automaticallly
 * 	wraps up to the beginning of next line when the last character of the
 * 	current line is output.
 * 	[93/08/12            paire]
 * 
 * 	Fixed db_advance_output_position() to go to the last character of
 * 	each line (the last one generated a buggy '\n').
 * 	Added db_reserve_output_position() to force a new_line if there is not
 * 	enough place before end of line.
 * 	[93/08/11            paire]
 * 	[94/02/07            bernadat]
 * 
 * Revision 1.3.4.4  1993/08/11  20:38:03  elliston
 * 	Add ANSI Prototypes.  CR #9523.
 * 	[1993/08/11  03:33:38  elliston]
 * 
 * Revision 1.3.4.3  1993/07/27  18:27:50  elliston
 * 	Add ANSI prototypes.  CR #9523.
 * 	[1993/07/27  18:12:31  elliston]
 * 
 * Revision 1.3.4.2  1993/06/09  02:20:24  gm
 * 	CR9176 - ANSI C violations: trailing tokens on CPP
 * 	directives, extra semicolons after decl_ ..., asm keywords
 * 	[1993/06/07  18:57:18  jeffc]
 * 
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  20:56:45  jeffc]
 * 
 * Revision 1.3  1993/04/19  16:02:33  devrcs
 * 	Added forward declaration for db_printf.
 * 	[93/04/05            bernadat]
 * 
 * 	Changes from mk78:
 * 	Added void type to functions that needed it.
 * 	[92/05/16            jfriedl]
 * 	[93/02/02            bruel]
 * 
 * 	Fixed tabs management for long lines. [barbou@gr.osf.org]
 * 	[92/12/03            bernadat]
 * 
 * Revision 1.2  1992/11/25  01:04:34  robert
 * 	integrate changes below for norma_14
 * 
 * 	Philippe Bernadat (bernadat) at gr.osf.org
 * 	Moved iprintf to here, result was horible for MPs (cpu number in
 * 	front of each iprintf.)
 * 	[1992/11/13  19:21:47  robert]
 * 
 * Revision 1.1  1992/09/30  02:01:14  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.6  91/10/09  16:01:26  af
 * 	 Revision 2.5.2.1  91/10/05  13:06:46  jeffreyh
 * 	 	Added "more" like function.
 * 	 	[91/08/29            tak]
 * 
 * Revision 2.5.2.1  91/10/05  13:06:46  jeffreyh
 * 	Added "more" like function.
 * 	[91/08/29            tak]
 * 
 * Revision 2.5  91/07/09  23:15:54  danner
 * 	Include machine/db_machdep.c.
 * 	When db_printf is invoked, call db_printing on luna. This is used
 * 	 to trigger a screen clear. Under luna88k conditional.
 * 	[91/07/08            danner]
 *
 * Revision 2.4  91/05/14  15:34:51  mrt
 * 	Correcting copyright
 * 
 * Revision 2.3  91/02/05  17:06:45  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:18:41  mrt]
 * 
 * Revision 2.2  90/08/27  21:51:25  dbg
 * 	Put extra features of db_doprnt in _doprnt.
 * 	[90/08/20            dbg]
 * 	Reduce lint.
 * 	[90/08/07            dbg]
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

/*
 * Printf and character output for debugger.
 */

#include <dipc.h>

#include <mach/boolean.h>
#include <kern/misc_protos.h>
#include <stdarg.h>
#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#include <ddb/db_lex.h>
#include <ddb/db_input.h>
#include <ddb/db_output.h>
#include <ddb/db_task_thread.h>

/*
 *	Character output - tracks position in line.
 *	To do this correctly, we should know how wide
 *	the output device is - then we could zero
 *	the line position when the output device wraps
 *	around to the start of the next line.
 *
 *	Instead, we count the number of spaces printed
 *	since the last printing character so that we
 *	don't print trailing spaces.  This avoids most
 *	of the wraparounds.
 */

#ifndef	DB_MAX_LINE
#define	DB_MAX_LINE		24	/* maximum line */
#define DB_MAX_WIDTH		80	/* maximum width */
#endif	/* DB_MAX_LINE */

#define DB_MIN_MAX_WIDTH	20	/* minimum max width */
#define DB_MIN_MAX_LINE		3	/* minimum max line */
#define CTRL(c)			((c) & 0xff)

int	db_output_position = 0;		/* output column */
int	db_output_line = 0;		/* output line number */
int	db_last_non_space = 0;		/* last non-space character */
int	db_last_gen_return = 0;		/* last character generated return */
int	db_auto_wrap = 1;		/* auto wrap at end of line ? */
int	db_tab_stop_width = 8;		/* how wide are tab stops? */
#define	NEXT_TAB(i) \
	((((i) + db_tab_stop_width) / db_tab_stop_width) * db_tab_stop_width)
int	db_max_line = DB_MAX_LINE;	/* output max lines */
int	db_max_width = DB_MAX_WIDTH;	/* output line width */


/* Prototypes for functions local to this file.  XXX -- should be static!
 */
static void db_more(void);
void db_advance_output_position(int new_output_position,
				int blank);


/*
 * Force pending whitespace.
 */
void
db_force_whitespace(void)
{
	register int last_print, next_tab;

	last_print = db_last_non_space;
	while (last_print < db_output_position) {
	    next_tab = NEXT_TAB(last_print);
	    if (next_tab <= db_output_position) {
		cnputc('\t');
		last_print = next_tab;
	    }
	    else {
		cnputc(' ');
		last_print++;
	    }
	}
	db_last_non_space = db_output_position;
}

void
db_reset_more()
{
	db_output_line = 0;
}

static void
db_more(void)
{
	register  char *p;
	boolean_t quit_output = FALSE;

#if defined(__alpha)
	extern boolean_t kdebug_mode;
	if (kdebug_mode) return;
#endif /* defined(__alpha) */
	for (p = "--db_more--"; *p; p++)
	    cnputc(*p);
	switch(cngetc()) {
	case ' ':
	    db_output_line = 0;
	    break;
	case 'q':
	case CTRL('c'):
	    db_output_line = 0;
	    quit_output = TRUE;
	    break;
	default:
	    db_output_line--;
	    break;
	}
	p = "\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b";
	while (*p)
	    cnputc(*p++);
	if (quit_output) {
	    db_error((char *) 0);
	    /* NOTREACHED */
	}
}

void
db_advance_output_position(int new_output_position,
			   int blank)
{
	if (db_max_width >= DB_MIN_MAX_WIDTH 
	    && new_output_position >= db_max_width) {
		/* auto new line */
		if (!db_auto_wrap || blank)
		    cnputc('\n');
		db_output_position = 0;
		db_last_non_space = 0;
		db_last_gen_return = 1;
		db_output_line++;
	} else {
		db_output_position = new_output_position;
	}
}

boolean_t
db_reserve_output_position(int increment)
{
	if (db_max_width >= DB_MIN_MAX_WIDTH
	    && db_output_position + increment >= db_max_width) {
		/* auto new line */
		if (!db_auto_wrap || db_last_non_space != db_output_position)
		    cnputc('\n');
		db_output_position = 0;
		db_last_non_space = 0;
		db_last_gen_return = 1;
		db_output_line++;
		return TRUE;
	}
	return FALSE;
}

/*
 * Output character.  Buffer whitespace.
 */
void
db_putchar(char c)
{
	if (db_max_line >= DB_MIN_MAX_LINE && db_output_line >= db_max_line-1)
	    db_more();
	if (c > ' ' && c <= '~') {
	    /*
	     * Printing character.
	     * If we have spaces to print, print them first.
	     * Use tabs if possible.
	     */
	    db_force_whitespace();
	    cnputc(c);
	    db_last_gen_return = 0;
	    db_advance_output_position(db_output_position+1, 0);
	    db_last_non_space = db_output_position;
	}
	else if (c == '\n') {
	    /* Return */
	    if (db_last_gen_return) {
		db_last_gen_return = 0;
	    } else {
		cnputc(c);
		db_output_position = 0;
		db_last_non_space = 0;
		db_output_line++;
		db_check_interrupt();
	    }
	}
	else if (c == '\t') {
	    /* assume tabs every 8 positions */
	    db_advance_output_position(NEXT_TAB(db_output_position), 1);
	}
	else if (c == ' ') {
	    /* space */
	    db_advance_output_position(db_output_position+1, 1);
	}
	else if (c == '\007') {
	    /* bell */
	    cnputc(c);
	}
	/* other characters are assumed non-printing */
}

/*
 * Return output position
 */
int
db_print_position(void)
{
	return (db_output_position);
}

/*
 * End line if too long.
 */
void
db_end_line(void)
{
	if (db_output_position >= db_max_width-1) {
	    /* auto new line */
	    if (!db_auto_wrap)
		cnputc('\n');
	    db_output_position = 0;
	    db_last_non_space = 0;
	    db_last_gen_return = 1;
	    db_output_line++;
	}
}

/*
 * Printing
 */

void
db_printf(char *fmt, ...)
{
	va_list	listp;

#ifdef	luna88k
	db_printing();
#endif
	va_start(listp, fmt);
	_doprnt(fmt, &listp, db_putchar, db_radix);
	va_end(listp);
}

/* alternate name */

void
kdbprintf(char *fmt, ...)
{
	va_list	listp;

	va_start(listp, fmt);
	_doprnt(fmt, &listp, db_putchar, db_radix);
	va_end(listp);
}

int	db_indent = 0;

/*
 * Printing (to console) with indentation.
 */
void
iprintf(char *fmt, ...)
{
	va_list listp;
	register int i;

	for (i = db_indent; i > 0; ){
	    if (i >= 8) {
		kdbprintf("\t");
		i -= 8;
	    }
	    else {
		kdbprintf(" ");
		i--;
	    }
	}

	va_start(listp, fmt);
	_doprnt(fmt, &listp, db_putchar, db_radix);
	va_end(listp);
}

#if	DIPC
#include <mach/kkt_request.h>
#endif	/* DIPC */

void
db_output_prompt(void)
{
	db_printf("db%s", (db_default_act) ? "t": "");
#if	DIPC
	db_printf("%d", KKT_NODE_SELF());
#endif	/* DIPC */
#if	NCPUS > 1
	db_printf("{%d}", cpu_number());
#endif
	db_printf("> ");
}

