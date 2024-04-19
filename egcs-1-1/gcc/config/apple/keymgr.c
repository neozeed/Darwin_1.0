/* Copyright (C) 1989, 92-97, 1998, 1999, Free Software Foundation, Inc.

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

#include "config.h"

#ifdef NEXT_SEMANTICS

/*
 * keymgr - Create and maintain process-wide global data known to 
 *	    all threads across all dynamic libraries. 
 *
 */

#include "keymgr.h"
#include <pthread.h>


extern void *__eh_global_dataptr ; /*from system framework.*/

static pthread_mutex_t _keyArray_lock=PTHREAD_MUTEX_INITIALIZER ;
static pthread_mutex_t *_keyArray_lock_ptr=&_keyArray_lock ;

typedef struct _Skey_data {
	struct _Skey_data *next ;
	unsigned int handle ;
	void *ptr ;
	} _Tkey_Data ;



static _Tkey_Data *_keymgr_get_key_element(unsigned int key) {

	_Tkey_Data  *keyArray ;

	for (keyArray = __eh_global_dataptr  ; keyArray != NULL ; keyArray = keyArray->next) {
	      if (keyArray->handle == key)
		 break ;
	      }

	return(keyArray) ;
	}
    


static _Tkey_Data *_keymgr_create_key_element(unsigned int key, void *ptr) {

	_Tkey_Data  *keyArray ;

	keyArray = (_Tkey_Data *) malloc(sizeof(_Tkey_Data)) ;
	keyArray->handle = key ;
	keyArray->ptr = ptr ;
	keyArray->next = __eh_global_dataptr ;
	__eh_global_dataptr = keyArray ;

	return(keyArray) ;
	}


static _Tkey_Data *_keymgr_set_key_element(unsigned int key, void *ptr) {


	_Tkey_Data  *keyArray ;

	keyArray = _keymgr_get_key_element(key) ; 
	   
	if (keyArray == NULL) {
	   keyArray = _keymgr_create_key_element(key,ptr) ;
	   }

	else {
	   keyArray->ptr = ptr ;
	   }

	return(keyArray) ;

	}


/*
 * External interfaces to keymgr. 
 * The purpose of these routines is to store
 * runtime information in a global place 
 * accessible from all threads and all dlls.
 * Thread safety is guaranteed.
 * _keymgr_get_key must be called first. It locks all keys
 * and returns the value of the specified key.
 * Next either _keymgr_unlock_keys can be called to unlock
 * the keys and allow them to change in value or
 * _keymgr_set_key can be called to set the value
 * of the key and then unlock all keys. The key number itself
 * is determined by the user. To prevent collisions, register
 * your key via a define in keymgr.h. The first use of
 * a key causes a memory location for the key value
 * to be reserved. As long as no key number collision
 * occurs, this interface allows different versions
 * of the runtime to exists together.
 */

PRIVATE_EXTERN
void *_keymgr_get_and_lock_key(unsigned int key) {

    _Tkey_Data *keyArray ;
    void *retptr ;

    if (pthread_mutex_lock(_keyArray_lock_ptr) != 0)
	__terminate() ;

    keyArray = _keymgr_get_key_element(key) ;

    if (keyArray == NULL) {
       keyArray = _keymgr_create_key_element(key, NULL) ;
       }

    retptr = keyArray->ptr ;

    return(retptr) ;
    }

PRIVATE_EXTERN
void _keymgr_unlock_keys(void) {

    if (pthread_mutex_unlock(_keyArray_lock_ptr) != 0)
       __terminate() ;

    }
    



PRIVATE_EXTERN
void *_keymgr_set_and_unlock_key(unsigned int key, void *ptr) {

    _Tkey_Data *keyArray ;
    void * retptr ;


    keyArray = _keymgr_set_key_element(key, ptr) ;

    retptr = keyArray->ptr ;

    if (pthread_mutex_unlock(_keyArray_lock_ptr) != 0)
	 __terminate() ;

    return(retptr) ;
    }

/*
 * The following routines allow the user to store per thread
 * data using a key of the user's choice. 
 *
 * _keymgr_init_per_thread_data creates a pthread's key
 * if it doesn't exist already. The data associated with
 * the pthreads' key is initialized to NULL. The pthread key
 * is associated with the specified keymgr key. The key must have
 * been locked by a _keymgr_get_and_lock_key operation.
 *
 * _keymgr_get_per_thread_data lock's the user's key
 * and maps it to a pthreads' key. The pthreads' data associated
 * with that key is returned. The user key is then unlocked.
 *
 * _keymgr_set_per_thread_data lock's the user's key
 * and maps it to a pthreads' key. The argument data is then
 * used to update the data associated with the pthreads' key.
 * Then the user's key is unlocked.
 *
 */


static
void _keymgr_init_per_thread_data(unsigned int key) {

     void * pthread_data ;
     pthread_key_t pthread_key ;

     pthread_key = (pthread_key_t) _keymgr_get_key_element(key)->ptr ;
     if (pthread_key == NULL) {
	if (pthread_key_create(&pthread_key,NULL) != 0)
	   __terminate() ;

	_keymgr_set_and_unlock_key(key,(void *) pthread_key) ;
	}

     else {
	  _keymgr_unlock_keys() ;
	  if (pthread_setspecific(pthread_key, NULL) != 0)
	     __terminate() ;
	  }

     }

PRIVATE_EXTERN 
void * _keymgr_get_per_thread_data(unsigned int key) {

     pthread_key_t pthread_key ;
     void * pthread_data ;

     pthread_key = (pthread_key_t) _keymgr_get_and_lock_key(key) ;

     if (pthread_key == NULL) {
	_keymgr_init_per_thread_data(key) ; /*unlocks keys.*/
	pthread_data = NULL ;
	}

     else {
	_keymgr_unlock_keys() ;
	pthread_data = pthread_getspecific (pthread_key) ;
	}

     return(pthread_data) ;
     }

PRIVATE_EXTERN
void _keymgr_set_per_thread_data(unsigned int key, void *keydata) {

     pthread_key_t pthread_key ;
     void * pthread_data ;

     pthread_key = (pthread_key_t) _keymgr_get_and_lock_key(key) ;

     if (pthread_key == NULL) {
	_keymgr_init_per_thread_data(key) ;
	pthread_data = NULL ;
	}

     _keymgr_unlock_keys() ;

     if (pthread_setspecific(pthread_key, keydata) != 0)
	__terminate() ;

     }



#endif /*NEXT_SEMANTICS*/
