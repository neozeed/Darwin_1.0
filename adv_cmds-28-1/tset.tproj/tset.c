/* 
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1980, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)tset.c	8.1 (Berkeley) 6/9/93
 */


#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "extern.h"

void	obsolete __P((char *[]));
void	report __P((char *, int, u_int));
void	usage __P((void));

struct termios mode, oldmode;

int	erasechar;		/* new erase character */
int	intrchar;		/* new interrupt character */
int	isreset;		/* invoked as reset */
int	killchar;		/* new kill character */
int	lines, columns;		/* window size */

int
main(argc, argv)
	int argc;
	char *argv[];
{
#ifdef TIOCGWINSZ
	struct winsize win;
#endif
	int ch, noinit, noset, quiet, Sflag, sflag, showterm, usingupper;
	char savech, *p, *t, *tcapbuf, *ttype;

	if (tcgetattr(STDERR_FILENO, &mode) < 0)
		err("standard error: %s", strerror(errno));

	oldmode = mode;
	ospeed = cfgetospeed(&mode);

	if (p = strrchr(*argv, '/'))
		++p;
	else
		p = *argv;
	usingupper = isupper(*p);
	if (!strcasecmp(p, "reset")) {
		isreset = 1;
		reset_mode();
	}

	obsolete(argv);
	noinit = noset = quiet = Sflag = sflag = showterm = 0;
	while ((ch = getopt(argc, argv, "-a:d:e:Ii:k:m:np:QSrs")) != EOF) {
		switch (ch) {
		case '-':		/* display term only */
			noset = 1;
			break;
		case 'a':		/* OBSOLETE: map identifier to type */
			add_mapping("arpanet", optarg);
			break;
		case 'd':		/* OBSOLETE: map identifier to type */
			add_mapping("dialup", optarg);
			break;
		case 'e':		/* erase character */
			erasechar = optarg[0] == '^' && optarg[1] != '\0' ?
			    optarg[1] == '?' ? '\177' : CTRL(optarg[1]) :
			    optarg[0];
			break;
		case 'I':		/* no initialization strings */
			noinit = 1;
			break;
		case 'i':		/* interrupt character */
			intrchar = optarg[0] == '^' && optarg[1] != '\0' ?
			    optarg[1] == '?' ? '\177' : CTRL(optarg[1]) :
			    optarg[0];
			break;
		case 'k':		/* kill character */
			killchar = optarg[0] == '^' && optarg[1] != '\0' ?
			    optarg[1] == '?' ? '\177' : CTRL(optarg[1]) :
			    optarg[0];
			break;
		case 'm':		/* map identifier to type */
			add_mapping(NULL, optarg);
			break;
		case 'n':		/* OBSOLETE: set new tty driver */
			break;
		case 'p':		/* OBSOLETE: map identifier to type */
			add_mapping("plugboard", optarg);
			break;
		case 'Q':		/* don't output control key settings */
			quiet = 1;
			break;
		case 'S':		/* output TERM/TERMCAP strings */
			Sflag = 1;
			break;
		case 'r':		/* display term on stderr */
			showterm = 1;
			break;
		case 's':		/* output TERM/TERMCAP strings */
			sflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	ttype = get_termcap_entry(*argv, &tcapbuf);

	if (!noset) {
		columns = tgetnum("co");
		lines = tgetnum("li");

#ifdef TIOCGWINSZ
		/* Set window size */
		(void)ioctl(STDERR_FILENO, TIOCGWINSZ, &win);
		if (win.ws_row == 0 && win.ws_col == 0 &&
		    lines > 0 && columns > 0) {
			win.ws_row = lines;
			win.ws_col = columns;
			(void)ioctl(STDERR_FILENO, TIOCSWINSZ, &win);
		}
#endif
		set_control_chars();
		set_conversions(usingupper);

		if (!noinit)
			set_init();

		/* Set the modes if they've changed. */
		if (memcmp(&mode, &oldmode, sizeof(mode)))
			tcsetattr(STDERR_FILENO, TCSADRAIN, &mode);
	}

	/* Get the terminal name from the entry. */
	p = tcapbuf;
	if (p != NULL && *p != ':') {
		t = p;
		if (p = strpbrk(p, "|:")) {
			savech = *p;
			*p = '\0';
			if ((ttype = strdup(t)) == NULL)
				err("%s", strerror(errno));
			*p = savech;
		}
	}

	if (noset)
		(void)printf("%s\n", ttype);
	else {
		if (showterm)
			(void)fprintf(stderr, "Terminal type is %s.\n", ttype);
		/*
		 * If erase, kill and interrupt characters could have been
		 * modified and not -Q, display the changes.
		 */
		if (!quiet) {
			report("Erase", VERASE, CERASE);
			report("Kill", VKILL, CKILL);
			report("Interrupt", VINTR, CINTR);
		}
	}

	if (Sflag) {
		(void)printf("%s ", ttype);
		wrtermcap(tcapbuf);
	}

	if (sflag) {
		/*
		 * Figure out what shell we're using.  A hack, we look for an
		 * environmental variable SHELL ending in "csh".
		 */
		if ((p = getenv("SHELL")) &&
		    !strcmp(p + strlen(p) - 3, "csh")) {
			p = "set noglob;\nsetenv TERM %s;\nsetenv TERMCAP '";
			t = "';\nunset noglob;\n";
		} else {
			p = "TERM=%s;\nTERMCAP='";
			t = "';\nexport TERMCAP TERM;\n";
		}
		(void)printf(p, ttype);
		wrtermcap(tcapbuf);
		(void)printf(t);
	}

	exit(0);
}

/*
 * Tell the user if a control key has been changed from the default value.
 */
void
report(name, which, def)
	char *name;
	int which;
	u_int def;
{
	u_int old, new;
	char *bp, buf[1024];

	new = mode.c_cc[which];
	old = oldmode.c_cc[which];

	if (old == new && old == def)
		return;

	(void)fprintf(stderr, "%s %s ", name, old == new ? "is" : "set to");

	bp = buf;
	if (tgetstr("kb", &bp) && new == buf[0] && buf[1] == '\0')
		(void)fprintf(stderr, "backspace.\n");
	else if (new == 0177)
		(void)fprintf(stderr, "delete.\n");
	else if (new < 040) {
		new ^= 0100;
		(void)fprintf(stderr, "control-%c (^%c).\n", new, new);
	} else
		(void)fprintf(stderr, "%c.\n", new);
}

/*
 * Convert the obsolete argument form into something that getopt can handle.
 * This means that -e, -i and -k get default arguments supplied for them.
 */
void
obsolete(argv)
	char *argv[];
{
	for (; *argv; ++argv) {
		if (argv[0][0] != '-' || argv[1] && argv[1][0] != '-' ||
		    argv[0][1] != 'e' && argv[0][1] != 'i' &&
		    argv[0][1] != 'k' || argv[0][2] != '\0')
			continue;
		switch(argv[0][1]) {
		case 'e':
			argv[0] = "-e^H";
			break;
		case 'i':
			argv[0] = "-i^C";
			break;
		case 'k':
			argv[0] = "-k^U";
			break;
		}
	}
}

void
usage()
{
	(void)fprintf(stderr,
"usage: tset [-IQrSs] [-] [-e ch] [-i ch] [-k ch] [-m mapping] [terminal]\n");
	exit(1);
}
