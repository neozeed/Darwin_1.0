/* 
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)print.c	8.6 (Berkeley) 4/16/94
 */


#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "stty.h"
#include "extern.h"

static void  binit __P((char *));
static void  bput __P((char *));
static char *ccval __P((struct cchar *, int));

void
print(tp, wp, ldisc, fmt)
	struct termios *tp;
	struct winsize *wp;
	int ldisc;
	enum FMT fmt;
{
	struct cchar *p;
	long tmp;
	u_char *cc;
	int cnt, ispeed, ospeed;
	char buf1[100], buf2[100];

	cnt = 0;

	/* Line discipline. */
	if (ldisc != TTYDISC) {
		switch(ldisc) {
		case TABLDISC:	
			cnt += printf("tablet disc; ");
			break;
		case SLIPDISC:	
			cnt += printf("slip disc; ");
			break;
		default:	
			cnt += printf("#%d disc; ", ldisc);
			break;
		}
	}

	/* Line speed. */
	ispeed = cfgetispeed(tp);
	ospeed = cfgetospeed(tp);
	if (ispeed != ospeed)
		cnt +=
		    printf("ispeed %d baud; ospeed %d baud;", ispeed, ospeed);
	else
		cnt += printf("speed %d baud;", ispeed);
	if (fmt >= BSD)
		cnt += printf(" %d rows; %d columns;", wp->ws_row, wp->ws_col);
	if (cnt)
		(void)printf("\n");

#define	on(f)	((tmp&f) != 0)
#define put(n, f, d) \
	if (fmt >= BSD || on(f) != d) \
		bput(n + on(f));

	/* "local" flags */
	tmp = tp->c_lflag;
	binit("lflags");
	put("-icanon", ICANON, 1);
	put("-isig", ISIG, 1);
	put("-iexten", IEXTEN, 1);
	put("-echo", ECHO, 1);
	put("-echoe", ECHOE, 0);
	put("-echok", ECHOK, 0);
	put("-echoke", ECHOKE, 0);
	put("-echonl", ECHONL, 0);
	put("-echoctl", ECHOCTL, 0);
	put("-echoprt", ECHOPRT, 0);
	put("-altwerase", ALTWERASE, 0);
	put("-noflsh", NOFLSH, 0);
	put("-tostop", TOSTOP, 0);
	put("-flusho", FLUSHO, 0);
	put("-pendin", PENDIN, 0);
	put("-nokerninfo", NOKERNINFO, 0);
	put("-extproc", EXTPROC, 0);

	/* input flags */
	tmp = tp->c_iflag;
	binit("iflags");
	put("-istrip", ISTRIP, 0);
	put("-icrnl", ICRNL, 1);
	put("-inlcr", INLCR, 0);
	put("-igncr", IGNCR, 0);
	put("-ixon", IXON, 1);
	put("-ixoff", IXOFF, 0);
	put("-ixany", IXANY, 1);
	put("-imaxbel", IMAXBEL, 1);
	put("-ignbrk", IGNBRK, 0);
	put("-brkint", BRKINT, 1);
	put("-inpck", INPCK, 0);
	put("-ignpar", IGNPAR, 0);
	put("-parmrk", PARMRK, 0);

	/* output flags */
	tmp = tp->c_oflag;
	binit("oflags");
	put("-opost", OPOST, 1);
	put("-onlcr", ONLCR, 1);
	put("-oxtabs", OXTABS, 1);

	/* control flags (hardware state) */
	tmp = tp->c_cflag;
	binit("cflags");
	put("-cread", CREAD, 1);
	switch(tmp&CSIZE) {
	case CS5:
		bput("cs5");
		break;
	case CS6:
		bput("cs6");
		break;
	case CS7:
		bput("cs7");
		break;
	case CS8:
		bput("cs8");
		break;
	}
	bput("-parenb" + on(PARENB));
	put("-parodd", PARODD, 0);
	put("-hupcl", HUPCL, 1);
	put("-clocal", CLOCAL, 0);
	put("-cstopb", CSTOPB, 0);
	put("-crtscts", CRTSCTS, 0);
	put("-mdmbuf", MDMBUF, 0);

	/* special control characters */
	cc = tp->c_cc;
	if (fmt == POSIX) {
		binit("cchars");
		for (p = cchars1; p->name; ++p) {
			(void)snprintf(buf1, sizeof(buf1), "%s = %s;",
			    p->name, ccval(p, cc[p->sub]));
			bput(buf1);
		}
		binit(NULL);
	} else {
		binit(NULL);
		for (p = cchars1, cnt = 0; p->name; ++p) {
			if (fmt != BSD && cc[p->sub] == p->def)
				continue;
#define	WD	"%-8s"
			(void)sprintf(buf1 + cnt * 8, WD, p->name);
			(void)sprintf(buf2 + cnt * 8, WD, ccval(p, cc[p->sub]));
			if (++cnt == LINELENGTH / 8) {
				cnt = 0;
				(void)printf("%s\n", buf1);
				(void)printf("%s\n", buf2);
			}
		}
		if (cnt) {
			(void)printf("%s\n", buf1);
			(void)printf("%s\n", buf2);
		}
	}
}

static int col;
static char *label;

static void
binit(lb)
	char *lb;
{

	if (col) {
		(void)printf("\n");
		col = 0;
	}
	label = lb;
}

static void
bput(s)
	char *s;
{

	if (col == 0) {
		col = printf("%s: %s", label, s);
		return;
	}
	if ((col + strlen(s)) > LINELENGTH) {
		(void)printf("\n\t");
		col = printf("%s", s) + 8;
		return;
	}
	col += printf(" %s", s);
}

static char *
ccval(p, c)
	struct cchar *p;
	int c;
{
	static char buf[5];
	char *bp;

	if (c == _POSIX_VDISABLE)
		return ("<undef>");

	if (p->sub == VMIN || p->sub == VTIME) {
		(void)snprintf(buf, sizeof(buf), "%d", c);
		return (buf);
	}
	bp = buf;
	if (c & 0200) {
		*bp++ = 'M';
		*bp++ = '-';
		c &= 0177;
	}
	if (c == 0177) {
		*bp++ = '^';
		*bp++ = '?';
	}
	else if (c < 040) {
		*bp++ = '^';
		*bp++ = c + '@';
	}
	else
		*bp++ = c;
	*bp = '\0';
	return (buf);
}
