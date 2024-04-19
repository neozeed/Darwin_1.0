/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/tree.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
/* tree.h - declare structures used by tree library
 *
 * vix 22jan93 [revisited; uses RCS, ANSI, POSIX; has bug fixes]
 * vix 27jun86 [broken out of tree.c]
 *
 * $Id: tree.h,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $
 */


#ifndef	_TREE_H_INCLUDED
#define	_TREE_H_INCLUDED


/*
 * tree_t is our package-specific anonymous pointer.
 */
typedef	void *tree_t;


typedef	struct tree_s {
		tree_t		data;
		struct tree_s	*left, *right;
		short		bal;
} tree;


void	tree_init(tree **);
tree_t	tree_srch(tree **, int (*)(), tree_t);
tree_t	tree_add (tree **, int (*)(), tree_t, void (*)());
int	tree_delete(tree **, int (*)(), tree_t, void (*)());
int	tree_trav(tree **, int (*)(), void *);
int	tree_seek_trav(tree **, int (*)(), tree_t, int (*)(), void *);
int	tree_pre_trav(tree **, int (*)(), void *);
int	tree_post_trav(tree **, int (*)(), void *);
void	tree_mung(tree **, void (*)());
tree_t	tree_pred(tree **, int (*)(), tree_t);
tree_t	tree_first(tree **ppr_tree);
tree_t	tree_last(tree **ppr_tree);

#endif	/* _TREE_H_INCLUDED */

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




