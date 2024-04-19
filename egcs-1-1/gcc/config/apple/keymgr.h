/* Copyright (C) 1989, 92-97, 1998, Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/*
 * This file added by Apple Computer Inc. for its OS X 
 * environment.
 */

#ifdef NEXT_SEMANTICS

#ifndef PRIVATE_EXTERN
#define PRIVATE_EXTERN 
#endif


/*
 * keymgr - Create and maintain process-wide global data known to 
 *	    all threads across all dynamic libraries. 
 *
 */

PRIVATE_EXTERN void *_keymgr_get_and_lock_key(unsigned int key) ;
PRIVATE_EXTERN void _keymgr_unlock_keys(void) ;
PRIVATE_EXTERN void *_keymgr_set_and_unlock_key(unsigned int key, void *ptr) ;
PRIVATE_EXTERN void * _keymgr_get_per_thread_data(unsigned int key) ;
PRIVATE_EXTERN void _keymgr_set_per_thread_data(unsigned int key, void *keydata) ;


#ifndef NULL
#define NULL (0)
#endif

/*
 * Keys currently in use:
 */

#define KEYMGR_EH_CONTEXT_KEY		1	/*stores handle for root pointer of exception context node.*/



#endif /*NEXT_SYMANTICS*/


