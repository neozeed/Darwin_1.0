#include "copyright.h"

/* $Header: /cvs/Darwin/Commands/GNU/emacs/emacs/oldXMenu/Error.c,v 1.1.1.1 1999/05/02 02:43:45 wsanchez Exp $ */
/* Copyright    Massachusetts Institute of Technology    1985	*/

/*
 * XMenu:	MIT Project Athena, X Window system menu package
 *
 * 	XMenuError -	Returns a string description of the current
 *			XMenu error status flag.
 *
 *	Author:		Tony Della Fera, DEC
 *			August, 1985
 *
 */

#include "XMenuInt.h"

char *
XMenuError()
{
    static char message[128];		/* Error message buffer. */

    if ((_XMErrorCode < XME_CODE_COUNT) && (_XMErrorCode >= 0)) {
	return(_XMErrorList[_XMErrorCode]);
    }
    sprintf(message, "Unknown _XMErrorCode: %d", _XMErrorCode);
    return(message);
}

