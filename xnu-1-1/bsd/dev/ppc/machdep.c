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
 * Copyright (c) 1997 Apple Computer, Inc.  All rights reserved.
 * Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved.
 *
 *
 * Machine dependent cruft.
 *
 * 27-Apr-1997  A.Ramesh at Apple 
 *
 *
 */

#import <mach/mach_types.h>
#import <mach/machine.h>
#import <sys/reboot.h>

int reboot_how;
extern struct tty	cons;
extern struct tty	*constty;		/* current console device */

extern int getchar();







#define putchar cnputc

void
gets(buf)
	char *buf;
{
	register char *lp;
	register c;

	lp = buf;
	for (;;) {
		c = getchar() & 0177;
		switch(c) {
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
			if (lp > buf) {
				lp--;
				putchar(' ');
				putchar('\b');
			}
			continue;
		case '#':
		case '\177':
			lp--;
			if (lp < buf)
				lp = buf;
			continue;
		case '@':
		case 'u'&037:
			lp = buf;
			putchar('\n');		/* XXX calls 'cnputc' on mips */
			continue;
		default:
			*lp++ = c;
		}
	}
}

int
getchar()
{
	int c;

	c = cngetc();
#if 0
	if (c == 0x1b)		/* ESC ? */
		call_kdp();
#endif 0

	if (c == '\r')
		c = '\n';
        cnputc(c);
	return c;
}

