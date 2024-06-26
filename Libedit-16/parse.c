/*
 * Copyright 1995-1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/*
 * parse.c: parse an editline extended command
 *
 * commands are:
 *
 *	bind
 *	echotc
 *	settc
 *	gettc
 */
#include "sys.h"
#include "el.h"
#include "tokenizer.h"

private struct {
    char *name;
    int (*func) __P((EditLine *, int, char **));
} cmds[] = {
    {	"bind",		map_bind 	},
    {	"echotc",	term_echotc 	},
    {	"history",	hist_list	},
    {	"telltc",	term_telltc 	},
    {	"settc",	term_settc	},
    {	"setty",	tty_stty	},
    {	NULL,		NULL		}
};


/* parse_line():
 *	Parse a line and dispatch it
 */
protected int
parse_line(el, line)
    EditLine *el;
    const char *line;
{
    char **argv;
    int argc;
    Tokenizer *tok;

    tok = tok_init(NULL);
    tok_line(tok, line, &argc, &argv);
    argc = el_parse(el, argc, argv);
    tok_end(tok);
    return argc;
}

/* el_parse():
 *	Command dispatcher
 */
public int
el_parse(el, argc, argv)
    EditLine *el;
    int argc;
    char *argv[];
{
    char *ptr;
    int i;

    for (ptr = argv[0]; *ptr && *ptr != ':'; ptr++)
	continue;

    if (*ptr == ':') {
	*ptr = '\0';
	if (el_match(el->el_prog, ptr))
	    return 0;
    }
    else
	ptr = argv[0];

    for (i = 0; cmds[i].name != NULL; i++)
	if (strcmp(cmds[i].name, ptr) == 0) {
	    i = (*cmds[i].func)(el, argc, argv);
	    return -i;
	}

    return -1;
}


/* parse__escape():
 *	Parse a string of the form ^<char> \<odigit> \<char> and return
 *	the appropriate character or -1 if the escape is not valid
 */
protected int
parse__escape(ptr)
    const char  ** const ptr;
{
    const char   *p;
    int   c;

    p = *ptr;

    if (p[1] == 0) 
	return -1;

    if (*p == '\\') {
	p++;
	switch (*p) {
	case 'a':
	    c = '\007';		/* Bell */
	    break;
	case 'b':
	    c = '\010';		/* Backspace */
	    break;
	case 't':
	    c = '\011';		/* Horizontal Tab */
	    break;
	case 'n':
	    c = '\012';		/* New Line */
	    break;
	case 'v':
	    c = '\013';		/* Vertical Tab */
	    break;
	case 'f':
	    c = '\014';		/* Form Feed */
	    break;
	case 'r':
	    c = '\015';		/* Carriage Return */
	    break;
	case 'e':
	    c = '\033';		/* Escape */
	    break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	    {
		int cnt, ch;

		for (cnt = 0, c = 0; cnt < 3; cnt++) {
		    ch = *p++;
		    if (ch < '0' || ch > '7') {
			p--;
			break;
		    }
		    c = (c << 3) | (ch - '0');
		}
		if ((c & 0xffffff00) != 0) 
		    return -1;
		--p;
	    }
	    break;
	default:
	    c = *p;
	    break;
	}
    }
    else if (*p == '^' && isalpha((unsigned char) *p)) {
	p++;
	c = (*p == '?') ? '\177' : (*p & 0237);
    }
    else
	c = *p;
    *ptr = ++p;
    return c;
}

/* parse__string():
 *	Parse the escapes from in and put the raw string out
 */
protected char *
parse__string(out, in)
    char *out;
    const char *in;
{
    char *rv = out;
    int n;
    for (;;)
	switch (*in) {
	case '\0':
	    *out = '\0';
	    return rv;

	case '\\':
	case '^':
	    if ((n = parse__escape(&in)) == -1)
		return NULL;
	    *out++ = n;
	    break;

	default:
	    *out++ = *in++;
	    break;
	}
}

/* parse_cmd():
 *	Return the command number for the command string given
 *	or -1 if one is not found
 */
protected int
parse_cmd(el, cmd)
    EditLine *el;
    const char *cmd;
{
    el_bindings_t *b;

    for (b = el->el_map.help; b->name != NULL; b++)
	if (strcmp(b->name, cmd) == 0)
	    return b->func;
    return -1;
}
