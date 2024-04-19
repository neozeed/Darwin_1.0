/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/signer/tree.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#ifndef LINT
static char RCSid[] = "$Id: tree.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $";
#endif

/*
 * tree - balanced binary tree library
 *
 * vix 05apr94 [removed vixie.h dependencies; cleaned up formatting, names]
 * vix 22jan93 [revisited; uses RCS, ANSI, POSIX; has bug fixes]
 * vix 23jun86 [added delete uar to add for replaced nodes]
 * vix 20jun86 [added tree_delete per wirth a+ds (mod2 v.) p. 224]
 * vix 06feb86 [added tree_mung()]
 * vix 02feb86 [added tree balancing from wirth "a+ds=p" p. 220-221]
 * vix 14dec85 [written]
 */

/*
 * This program text was created by Paul Vixie using examples from the book:
 * "Algorithms & Data Structures," Niklaus Wirth, Prentice-Hall, 1986, ISBN
 * 0-13-022005-1.  Any errors in the conversion from Modula-2 to C are Paul
 * Vixie's.
 *
 * This code and associated documentation is hereby placed in the public
 * domain, with the wish that my name and Prof. Wirth's not be removed
 * from the source or documentation.
 */

/*
 * Date: Fri, 8 Jan 1999 13:40:37 -0500 (EST)
 * From: Edward Lewis <lewis@tis.com>
 * X-Sender: lewis@clipper
 * To: Brian Wellington <bwelling@tis.com>
 * cc: Paul A Vixie <paul@vix.com>
 * Subject: Re: what's the difference between lib/isc/tree.c and contrib/dnssigner/tree.c
 * In-Reply-To: <Pine.LNX.4.05.9901081316240.1212-100000@spiral.hq.tis.com>
 * Message-ID: <Pine.SOL.3.95.990108133429.17467A-100000@clipper>
 * MIME-Version: 1.0
 * Content-Type: TEXT/PLAIN; charset=US-ASCII
 * 
 * 
 * In case you want details (otherwise ignore this):
 * 
 * The only change I made (18 mos ago?) was in tree_add.  If an add found
 * that this (the key) was a duplicate, the in-tree value was deleted and
 * the new value inserted.  For my purposes, I wanted to merge the new with
 * the in-tre value (as in a RR set).
 * 
 * I added an extra parmeter to the call back function which did the delete,
 * so that it was now up to the programmer to delete one (and ignore the
 * other parameter) or merge the two entries.
 * 
 * This is a bit sketchy - it's been a while since I looked at it.
 * 
 * On Fri, 8 Jan 1999, Brian Wellington wrote:
 * 
 * > Ed can correct me if I'm wrong, but I think tree_add acts a bit
 * > differently.  contrib/dns_signer/signer/tree.c appears to be based on
 * > lib/isc/tree.c plus the additions I made for sbind (using trees instead of
 * > hashtables) plus changes Ed made.
 * > 
 * > Brian
 */

/*
 * Portions Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*#define		DEBUG	"tree"*/

/*#include "port_before.h"*/

#include <stdio.h>
#include <stdlib.h>

/*#include "port_after.h"*/

#include "tree.h"

#ifdef DEBUG
static int	debugDepth = 0;
static char	*debugFuncs[256];
# define ENTER(proc) { \
			debugFuncs[debugDepth] = proc; \
			fprintf(stderr, "ENTER(%d:%s.%s)\n", \
				debugDepth, DEBUG,
				debugFuncs[debugDepth]); \
			debugDepth++; \
		}
# define RET(value) { \
			debugDepth--; \
			fprintf(stderr, "RET(%d:%s.%s)\n", \
				debugDepth, DEBUG, \
				debugFuncs[debugDepth]); \
			return (value); \
		}
# define RETV { \
			debugDepth--; \
			fprintf(stderr, "RETV(%d:%s.%s)\n", \
				debugDepth, DEBUG, \
				debugFuncs[debugDepth]); \
			return; \
		}
# define MSG(msg)	fprintf(stderr, "MSG(%s)\n", msg);
#else
# define ENTER(proc)	;
# define RET(value)	return (value);
# define RETV		return;
# define MSG(msg)	;
#endif

#ifndef TRUE
# define TRUE		1
# define FALSE		0
#endif

static tree *	sprout(tree **, tree_t, int *, int (*)(), void (*)());
static int	delete(tree **, int (*)(), tree_t, void (*)(), int *, int *);
static void	del(tree **, int *, tree **, void (*)(), int *);
static void	bal_L(tree **, int *);
static void	bal_R(tree **, int *);
static tree_t	tree_pred_recurse(tree **, int (*)(), tree_t, tree_t);

void
tree_init(tree **ppr_tree) {
	ENTER("tree_init")
	*ppr_tree = NULL;
	RETV
}
	
tree_t
tree_srch(tree **ppr_tree, int (*pfi_compare)(), tree_t	p_user) {
	ENTER("tree_srch")

	if (*ppr_tree) {
		int i_comp = (*pfi_compare)(p_user, (**ppr_tree).data);

		if (i_comp > 0)
			RET(tree_srch(&(**ppr_tree).right,
				      pfi_compare,
				      p_user))

		if (i_comp < 0)
			RET(tree_srch(&(**ppr_tree).left,
				      pfi_compare,
				      p_user))

		/* not higher, not lower... this must be the one.
		 */
		RET((**ppr_tree).data)
	}

	/* grounded. NOT found.
	 */
	RET(NULL)
}

tree_t
tree_add(tree **ppr_tree, int (*pfi_compare)(),
	 tree_t p_user, void (*pfv_uar)())
{
	tree *last_visit;
	int i_balance = FALSE;

	ENTER("tree_add")
	if (!(last_visit = sprout(ppr_tree, p_user, &i_balance,
						pfi_compare, pfv_uar)))
		RET(NULL)
#ifdef OLD_WAY
	RET(p_user)
#else
	RET(last_visit->data);
#endif
}

int
tree_delete(tree **ppr_p, int (*pfi_compare)(),
	    tree_t p_user, void	(*pfv_uar)())
{
	int i_balance = FALSE, i_uar_called = FALSE;

	ENTER("tree_delete");
	RET(delete(ppr_p, pfi_compare, p_user, pfv_uar,
		   &i_balance, &i_uar_called))
}

int
tree_trav(tree **ppr_tree, int (*pfi_uar)(), void *data) {
	ENTER("tree_trav")

	if (!*ppr_tree)
		RET(TRUE)

	if (!tree_trav(&(**ppr_tree).left, pfi_uar, data))
		RET(FALSE)
	if (!(*pfi_uar)((**ppr_tree).data, data))
		RET(FALSE)
	if (!tree_trav(&(**ppr_tree).right, pfi_uar, data))
		RET(FALSE)
	RET(TRUE)
}

int
tree_seek_trav(tree **ppr_tree, int (*pfi_compare)(), tree_t p_user, int (*pfi_uar)(), void *data) {
	ENTER("tree_seek_trav")

	if (*ppr_tree) {

		int i_comp = (*pfi_compare)(p_user, (**ppr_tree).data);

                if (i_comp < 0) {
                        if (!tree_seek_trav(&(**ppr_tree).left,
					     pfi_compare, p_user,
					     pfi_uar, data))
				RET(FALSE)
		}
                if (i_comp <= 0) {
			if (!(*pfi_uar)((**ppr_tree).data, data))
				RET(FALSE)
		}
		RET (tree_seek_trav(&(**ppr_tree).right,
				    pfi_compare, p_user, pfi_uar, data))
	}
	else 
		RET(TRUE)
}

int
tree_pre_trav(tree **ppr_tree, int (*pfi_uar)(), void *data) {
	ENTER("tree_trav")

	if (!*ppr_tree)
		RET(TRUE)

	if (!(*pfi_uar)((**ppr_tree).data, data))
		RET(FALSE)
	if (!tree_pre_trav(&(**ppr_tree).left, pfi_uar, data))
		RET(FALSE)
	if (!tree_pre_trav(&(**ppr_tree).right, pfi_uar, data))
		RET(FALSE)
	RET(TRUE)
}

int
tree_post_trav(tree **ppr_tree, int (*pfi_uar)(), void *data) {
	ENTER("tree_trav")

	if (!*ppr_tree)
		RET(TRUE)

	if (!tree_post_trav(&(**ppr_tree).left, pfi_uar, data))
		RET(FALSE)
	if (!tree_post_trav(&(**ppr_tree).right, pfi_uar, data))
		RET(FALSE)
	if (!(*pfi_uar)((**ppr_tree).data, data))
		RET(FALSE)
	RET(TRUE)
}

void
tree_mung(tree **ppr_tree, void	(*pfv_uar)()) {
	ENTER("tree_mung")
	if (*ppr_tree) {
		tree_mung(&(**ppr_tree).left, pfv_uar);
		tree_mung(&(**ppr_tree).right, pfv_uar);
		if (pfv_uar)
			(*pfv_uar)((**ppr_tree).data);
		free(*ppr_tree);
		*ppr_tree = NULL;
	}
	RETV
}

#ifndef OLD_WAY
/*  
 * Note!  pfv_delete is a misnomer if SIGNER_APP is defined.
 * The function passed as that argument needs to take two arguments,
 * two tree_t typed structures.  The data in p_data is added to the
 * data in (*ppr)->data.  E.g., linked lists would be joined, or
 * bitfields OR'd.
 */
#endif

static tree *
sprout(tree **ppr, tree_t p_data, int *pi_balance,
       int (*pfi_compare)(), void (*pfv_delete)())
{
	tree *p1, *p2, *sub;
	int cmp;

	ENTER("sprout")

	/* are we grounded?  if so, add the node "here" and set the rebalance
	 * flag, then exit.
	 */
	if (!*ppr) {
		MSG("grounded. adding new node, setting h=true")
		*ppr = (tree *) malloc(sizeof(tree));
		if (*ppr) {
			(*ppr)->left = NULL;
			(*ppr)->right = NULL;
			(*ppr)->bal = 0;
			(*ppr)->data = p_data;
			*pi_balance = TRUE;
		}
		RET(*ppr);
	}

	/* compare the data using routine passed by caller.
	 */
	cmp = (*pfi_compare)(p_data, (*ppr)->data);

	/* if LESS, prepare to move to the left.
	 */
	if (cmp < 0) {
		MSG("LESS. sprouting left.")
		sub = sprout(&(*ppr)->left, p_data, pi_balance,
			     pfi_compare, pfv_delete);
		if (sub && *pi_balance) {	/* left branch has grown */
			MSG("LESS: left branch has grown")
			switch ((*ppr)->bal) {
			case 1:
				/* right branch WAS longer; bal is ok now */
				MSG("LESS: case 1.. bal restored implicitly")
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
				break;
			case 0:
				/* balance WAS okay; now left branch longer */
				MSG("LESS: case 0.. balnce bad but still ok")
				(*ppr)->bal = -1;
				break;
			case -1:
				/* left branch was already too long. rebal */
				MSG("LESS: case -1: rebalancing")
				p1 = (*ppr)->left;
				if (p1->bal == -1) {		/* LL */
					MSG("LESS: single LL")
					(*ppr)->left = p1->right;
					p1->right = *ppr;
					(*ppr)->bal = 0;
					*ppr = p1;
				} else {			/* double LR */
					MSG("LESS: double LR")

					p2 = p1->right;
					p1->right = p2->left;
					p2->left = p1;

					(*ppr)->left = p2->right;
					p2->right = *ppr;

					if (p2->bal == -1)
						(*ppr)->bal = 1;
					else
						(*ppr)->bal = 0;

					if (p2->bal == 1)
						p1->bal = -1;
					else
						p1->bal = 0;
					*ppr = p2;
				} /*else*/
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
			} /*switch*/
		} /*if*/
		RET(sub)
	} /*if*/

	/* if MORE, prepare to move to the right.
	 */
	if (cmp > 0) {
		MSG("MORE: sprouting to the right")
		sub = sprout(&(*ppr)->right, p_data, pi_balance,
			     pfi_compare, pfv_delete);
		if (sub && *pi_balance) {
			MSG("MORE: right branch has grown")

			switch ((*ppr)->bal) {
			case -1:
				MSG("MORE: balance was off, fixed implicitly")
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
				break;
			case 0:
				MSG("MORE: balance was okay, now off but ok")
				(*ppr)->bal = 1;
				break;
			case 1:
				MSG("MORE: balance was off, need to rebalance")
				p1 = (*ppr)->right;
				if (p1->bal == 1) {		/* RR */
					MSG("MORE: single RR")
					(*ppr)->right = p1->left;
					p1->left = *ppr;
					(*ppr)->bal = 0;
					*ppr = p1;
				} else {			/* double RL */
					MSG("MORE: double RL")

					p2 = p1->left;
					p1->left = p2->right;
					p2->right = p1;

					(*ppr)->right = p2->left;
					p2->left = *ppr;

					if (p2->bal == 1)
						(*ppr)->bal = -1;
					else
						(*ppr)->bal = 0;

					if (p2->bal == -1)
						p1->bal = 1;
					else
						p1->bal = 0;

					*ppr = p2;
				} /*else*/
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
			} /*switch*/
		} /*if*/
		RET(sub)
	} /*if*/

	/* not less, not more: this is the same key!  replace...
	 */
	*pi_balance = FALSE;
#ifdef OLD_WAY
	MSG("FOUND: Replacing data value")
	if (pfv_delete)
		(*pfv_delete)((*ppr)->data);
	(*ppr)->data = p_data;
#else
    MSG("FOUND: Appending data value"); 
    if (pfv_delete)
        (*pfv_delete) ((*ppr)->data, p_data);
#endif
	RET(*ppr)
}

static int
delete(tree **ppr_p, int (*pfi_compare)(), tree_t p_user,
       void (*pfv_uar)(), int *pi_balance, int *pi_uar_called)
{
	tree *pr_q;
	int i_comp, i_ret;

	ENTER("delete")

	if (*ppr_p == NULL) {
		MSG("key not in tree")
		RET(FALSE)
	}

	i_comp = (*pfi_compare)((*ppr_p)->data, p_user);
	if (i_comp > 0) {
		MSG("too high - scan left")
		i_ret = delete(&(*ppr_p)->left, pfi_compare, p_user, pfv_uar,
			       pi_balance, pi_uar_called);
		if (*pi_balance)
			bal_L(ppr_p, pi_balance);
	} else if (i_comp < 0) {
		MSG("too low - scan right")
		i_ret = delete(&(*ppr_p)->right, pfi_compare, p_user, pfv_uar,
			       pi_balance, pi_uar_called);
		if (*pi_balance)
			bal_R(ppr_p, pi_balance);
	} else {
		MSG("equal")
		pr_q = *ppr_p;
		if (pr_q->right == NULL) {
			MSG("right subtree null")
			*ppr_p = pr_q->left;
			*pi_balance = TRUE;
		} else if (pr_q->left == NULL) {
			MSG("right subtree non-null, left subtree null")
			*ppr_p = pr_q->right;
			*pi_balance = TRUE;
		} else {
			MSG("neither subtree null")
			del(&pr_q->left, pi_balance, &pr_q,
			    pfv_uar, pi_uar_called);
			if (*pi_balance)
				bal_L(ppr_p, pi_balance);
		}
		if (!*pi_uar_called && pfv_uar)
			(*pfv_uar)(pr_q->data);
		free(pr_q);	/* thanks to wuth@castrov.cuc.ab.ca */
		i_ret = TRUE;
	}
	RET(i_ret)
}

static void
del(tree **ppr_r, int *pi_balance, tree **ppr_q,
    void (*pfv_uar)(), int *pi_uar_called)
{
	ENTER("del")

	if ((*ppr_r)->right != NULL) {
		del(&(*ppr_r)->right, pi_balance, ppr_q,
		    pfv_uar, pi_uar_called);
		if (*pi_balance)
			bal_R(ppr_r, pi_balance);
	} else {
		if (pfv_uar)
			(*pfv_uar)((*ppr_q)->data);
		*pi_uar_called = TRUE;
		(*ppr_q)->data = (*ppr_r)->data;
		*ppr_q = *ppr_r;
		*ppr_r = (*ppr_r)->left;
		*pi_balance = TRUE;
	}

	RETV
}

static void
bal_L(tree **ppr_p, int *pi_balance) {
	tree *p1, *p2;
	int b1, b2;

	ENTER("bal_L")
	MSG("left branch has shrunk")

	switch ((*ppr_p)->bal) {
	case -1:
		MSG("was imbalanced, fixed implicitly")
		(*ppr_p)->bal = 0;
		break;
	case 0:
		MSG("was okay, is now one off")
		(*ppr_p)->bal = 1;
		*pi_balance = FALSE;
		break;
	case 1:
		MSG("was already off, this is too much")
		p1 = (*ppr_p)->right;
		b1 = p1->bal;
		if (b1 >= 0) {
			MSG("single RR")
			(*ppr_p)->right = p1->left;
			p1->left = *ppr_p;
			if (b1 == 0) {
				MSG("b1 == 0")
				(*ppr_p)->bal = 1;
				p1->bal = -1;
				*pi_balance = FALSE;
			} else {
				MSG("b1 != 0")
				(*ppr_p)->bal = 0;
				p1->bal = 0;
			}
			*ppr_p = p1;
		} else {
			MSG("double RL")
			p2 = p1->left;
			b2 = p2->bal;
			p1->left = p2->right;
			p2->right = p1;
			(*ppr_p)->right = p2->left;
			p2->left = *ppr_p;
			if (b2 == 1)
				(*ppr_p)->bal = -1;
			else
				(*ppr_p)->bal = 0;
			if (b2 == -1)
				p1->bal = 1;
			else
				p1->bal = 0;
			*ppr_p = p2;
			p2->bal = 0;
		}
	}
	RETV
}

static void
bal_R(tree **ppr_p, int *pi_balance) {
	tree *p1, *p2;
	int b1, b2;

	ENTER("bal_R")
	MSG("right branch has shrunk")
	switch ((*ppr_p)->bal) {
	case 1:
		MSG("was imbalanced, fixed implicitly")
		(*ppr_p)->bal = 0;
		break;
	case 0:
		MSG("was okay, is now one off")
		(*ppr_p)->bal = -1;
		*pi_balance = FALSE;
		break;
	case -1:
		MSG("was already off, this is too much")
		p1 = (*ppr_p)->left;
		b1 = p1->bal;
		if (b1 <= 0) {
			MSG("single LL")
			(*ppr_p)->left = p1->right;
			p1->right = *ppr_p;
			if (b1 == 0) {
				MSG("b1 == 0")
				(*ppr_p)->bal = -1;
				p1->bal = 1;
				*pi_balance = FALSE;
			} else {
				MSG("b1 != 0")
				(*ppr_p)->bal = 0;
				p1->bal = 0;
			}
			*ppr_p = p1;
		} else {
			MSG("double LR")
			p2 = p1->right;
			b2 = p2->bal;
			p1->right = p2->left;
			p2->left = p1;
			(*ppr_p)->left = p2->right;
			p2->right = *ppr_p;
			if (b2 == -1)
				(*ppr_p)->bal = 1;
			else
				(*ppr_p)->bal = 0;
			if (b2 == 1)
				p1->bal = -1;
			else
				p1->bal = 0;
			*ppr_p = p2;
			p2->bal = 0;
		}
	}
	RETV
}

static tree_t
tree_pred_recurse(tree **ppr_tree, int (*pfi_compare)(), tree_t	p_user,
		  tree_t best)
{
	ENTER("tree_pred_recurse")
	if (*ppr_tree) {
		int i_comp = (*pfi_compare)(p_user, (**ppr_tree).data);

		if (i_comp < 0)
			RET(tree_pred_recurse(&(**ppr_tree).left,
				      pfi_compare,
				      p_user, best))
		if (i_comp > 0) {
			if (!best || (*pfi_compare)((**ppr_tree).data, best) >0)
				best = (**ppr_tree).data;
			RET(tree_pred_recurse(&(**ppr_tree).right,
				      pfi_compare,
				      p_user, best))
		}

		/* we've found the node, but we still need it's predecessor.
		 */
		RET(tree_pred_recurse(&(**ppr_tree).left,
			      pfi_compare,
			      p_user, best))
	}
	/* grounded. NOT found.
	 */
	RET(best)
}

tree_t
tree_pred(tree **ppr_tree, int (*pfi_compare)(), tree_t p_user) {
	ENTER("tree_pred")
	RET (tree_pred_recurse (ppr_tree, pfi_compare, p_user, NULL))
}

tree_t
tree_first(tree **ppr_tree) {
	if (*ppr_tree == NULL)
		return NULL;
	while ((**ppr_tree).left)
		*ppr_tree = (**ppr_tree).left;
	return (**ppr_tree).data;
}

tree_t
tree_last(tree **ppr_tree) {
	if (*ppr_tree == NULL)
		return NULL;
	while ((**ppr_tree).right)
		*ppr_tree = (**ppr_tree).right;
	return (**ppr_tree).data;
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




