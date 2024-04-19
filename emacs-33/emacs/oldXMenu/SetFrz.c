#include "copyright.h"

/* $Header: /cvs/Darwin/Commands/GNU/emacs/emacs/oldXMenu/SetFrz.c,v 1.1.1.1 1999/05/02 02:43:46 wsanchez Exp $ */
/* Copyright    Massachusetts Institute of Technology    1985	*/

/*
 * XMenu:	MIT Project Athena, X Window system menu package
 *
 *	XMenuSetFreeze - Forcibly set the menu freeze mode switch
 *			 overriding the Xdefaults setting.
 *			 This is necessary in some situations.
 *
 *	Author:		Tony Della Fera, DEC
 *			January 29, 1986
 *
 */

#include "XMenuInt.h"

XMenuSetFreeze(menu, freeze)
    register XMenu *menu;	/* Menu object to be modified. */
    register int freeze;	/* Freeze mode? */
{
    /*
     * Set the freeze mode switch.
     */
    menu->freeze = freeze;
}
