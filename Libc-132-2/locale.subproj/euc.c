/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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


#include <sys/types.h>

#include <errno.h>
#include <rune.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

rune_t	_EUC_sgetrune __P((const char *, size_t, char const **));
int	_EUC_sputrune __P((rune_t, char *, size_t, char **));

typedef struct {
	int	count[4];
	rune_t	bits[4];
	rune_t	mask;
} _EucInfo;

int
_EUC_init(rl)
	_RuneLocale *rl;
{
	_EucInfo *ei;
	int x;
	char *v, *e;

	rl->sgetrune = _EUC_sgetrune;
	rl->sputrune = _EUC_sputrune;

	if (!rl->variable) {
		free(rl);
		return (EFTYPE);
	}
	v = (char *) rl->variable;

	while (*v == ' ' || *v == '\t')
		++v;

	if ((ei = malloc(sizeof(_EucInfo))) == NULL) {
		free(rl);
		return (ENOMEM);
	}
	for (x = 0; x < 4; ++x) {
		ei->count[x] = (int) strtol(v, &e, 0);
		if (v == e || !(v = e)) {
			free(rl);
			free(ei);
			return (EFTYPE);
		}
		while (*v == ' ' || *v == '\t')
			++v;
		ei->bits[x] = (int) strtol(v, &e, 0);
		if (v == e || !(v = e)) {
			free(rl);
			free(ei);
			return (EFTYPE);
		}
		while (*v == ' ' || *v == '\t')
			++v;
	}
	ei->mask = (int)strtol(v, &e, 0);
	if (v == e || !(v = e)) {
		free(rl);
		free(ei);
		return (EFTYPE);
	}
	if (sizeof(_EucInfo) <= rl->variable_len) {
		memcpy(rl->variable, ei, sizeof(_EucInfo));
		free(ei);
	} else {
		rl->variable = &ei;
	}
	rl->variable_len = sizeof(_EucInfo);
	_CurrentRuneLocale = rl;
	__mb_cur_max = 3;
	return (0);
}

#define	CEI	((_EucInfo *)(_CurrentRuneLocale->variable))

#define	_SS2	0x008e
#define	_SS3	0x008f

static inline int
_euc_set(c)
	u_int c;
{
	c &= 0xff;

	return ((c & 0x80) ? c == _SS3 ? 3 : c == _SS2 ? 2 : 1 : 0);
}
rune_t
_EUC_sgetrune(string, n, result)
	const char *string;
	size_t n;
	char const **result;
{
	rune_t rune = 0;
	int len, set;

	if (n < 1 || (len = CEI->count[set = _euc_set(*string)]) > n) {
		if (result)
			*result = string;
		return (_INVALID_RUNE);
	}
	switch (set) {
	case 3:
	case 2:
		--len;
		++string;
		/* FALLTHROUGH */
	case 1:
	case 0:
		while (len-- > 0)
			rune = (rune << 8) | ((u_int)(*string++) & 0xff);
		break;
	}
	if (result)
		*result = string;
	return ((rune & ~CEI->mask) | CEI->bits[set]);
}

int
_EUC_sputrune(c, string, n, result)
	rune_t c;
	char *string, **result;
	size_t n;
{
	rune_t m = c & CEI->mask;
	rune_t nm = c & ~m;
	int i, len;

	if (m == CEI->bits[1]) {
CodeSet1:
		/* Codeset 1: The first byte must have 0x80 in it. */
		i = len = CEI->count[1];
		if (n >= len) {
			if (result)
				*result = string + len;
			while (i-- > 0)
				*string++ = (nm >> (i << 3)) | 0x80;
		} else
			if (result)
				*result = (char *) 0;
	} else {
		if (m == CEI->bits[0]) {
			i = len = CEI->count[0];
			if (n < len) {
				if (result)
					*result = NULL;
				return (len);
			}
		} else
			if (m == CEI->bits[2]) {
				i = len = CEI->count[2];
				if (n < len) {
					if (result)
						*result = NULL;
					return (len);
				}
				*string++ = _SS2;
				--i;
			} else
				if (m == CEI->bits[3]) {
					i = len = CEI->count[3];
					if (n < len) {
						if (result)
							*result = NULL;
						return (len);
					}
					*string++ = _SS3;
					--i;
				} else
					goto CodeSet1;	/* Bletch */
		while (i-- > 0)
			*string++ = (nm >> (i << 3)) & 0xff;
		if (result)
			*result = string;
	}
	return (len);
}