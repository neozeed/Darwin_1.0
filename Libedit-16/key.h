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
 *
 *	@(#)key.h	8.1 (Berkeley) 6/4/93
 */

/*
 * el.key.h: Key macro header
 */
#ifndef _h_el_key
#define _h_el_key

typedef union key_value_t {
    el_action_t  cmd;	/* If it is a command the #	*/
    char        *str;	/* If it is a string...		*/
} key_value_t;

typedef struct key_node_t key_node_t;

typedef struct el_key_t {
    char        *buf;	/* Key print buffer		*/
    key_node_t  *map;	/* Key map			*/
    key_value_t  val;	/* Local conversion buffer	*/
} el_key_t;

#define XK_CMD	0
#define XK_STR	1
#define XK_NOD	2
#define XK_EXE	3

protected int 		key_init	__P((EditLine *));
protected void 		key_end		__P((EditLine *));
protected key_value_t *	key_map_cmd	__P((EditLine *, int));
protected key_value_t *	key_map_str	__P((EditLine *, char *));
protected void		key_reset	__P((EditLine *));
protected int 		key_get		__P((EditLine *, char *, 
					     key_value_t *));
protected void		key_add		__P((EditLine *, char *, key_value_t *,
					     int));
protected void		key_clear	__P((EditLine *, el_action_t *,
					     char *));
protected int		key_delete	__P((EditLine *, char *));
protected void		key_print	__P((EditLine *, char *));
protected void	        key_kprint	__P((EditLine *, char *, 
					     key_value_t *, int));
protected char 	       *key__decode_str	__P((char *, char *, char *));

#endif /* _h_el_key */
