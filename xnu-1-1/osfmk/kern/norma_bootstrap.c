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
 * @OSF_FREE_COPYRIGHT@
 * 
 */
/*
 * HISTORY
 * 
 * Revision 1.1.1.1  1998/09/22 21:05:34  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:55  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.1.9.2  1995/06/13  18:23:01  sjs
 * 	Merge with flipc_shared.
 * 	[95/05/22            sjs]
 *
 * Revision 1.1.7.1  1994/12/12  17:46:03  randys
 * 	Putting initial flipc implementation under flipc_shared
 * 	[1994/12/12  16:27:41  randys]
 * 
 * Revision 1.1.5.2  1994/12/11  23:19:19  randys
 * 	Initial flipc code checkin
 * 
 * Revision 1.1.9.1  1995/02/23  17:31:41  alanl
 * 	DIPC:  Merge from nmk17b2 to nmk18b8.
 * 	[95/01/05            alanl]
 * 
 * Revision 1.1.4.3  1994/12/06  20:11:18  alanl
 * 	Fix up definition of norma_bootstrap.  Include xmm_obj.h for
 * 	prototype of norma_vm_init.  Include misc_protos.h for
 * 	prototype of norma_bootstrap.
 * 	[94/12/05            mmp]
 * 
 * Revision 1.1.4.2  1994/09/01  12:52:14  alanl
 * 	Eliminate dead NORMA_IPC conditional.
 * 	[94/08/29            alanl]
 * 
 * Revision 1.1.4.1  1994/08/04  02:25:11  mmp
 * 	Brought up from dipc_shared.
 * 	[1994/08/03  20:28:32  mmp]
 * 
 * Revision 1.1.2.3  1994/06/09  14:40:01  rwd
 * 	Strip rpc.h and rdma.h includes
 * 	[94/06/06            rwd]
 * 
 * Revision 1.1.2.2  1994/04/28  00:27:20  alanl
 * 	Special ports initialization should happen from
 * 	dipc_bootstrap, not from norma_bootstrap.
 * 	[1994/04/27  23:55:49  alanl]
 * 
 * 	Initialization of special ports must happen
 * 	after initialization of DIPC proper.  In fact,
 * 	this work should be done out of dipc_bootstrap,
 * 	but we'll fix that later.
 * 	[1994/04/27  23:52:45  alanl]
 * 
 * Revision 1.1.2.1  1994/04/20  18:43:48  alanl
 * 	Created.
 * 	[1994/04/20  18:30:57  alanl]
 * 
 * $EndLog$
 */

/*
 *	File:	kern/norma_bootstrap.c
 *	Author:	Alan Langerman
 *	Date:	1994
 *
 *	Bootstrap distributed kernel services.
 */

#include <dipc.h>
#include <norma_device.h>
#include <norma_task.h>
#include <norma_vm.h>
#include <kern/misc_protos.h>
#include <flipc.h>

#if	DIPC
#include <dipc/dipc_funcs.h>
#endif

#if	NORMA_VM
#include <xmm/xmm_obj.h>
#endif

#if	FLIPC
#include <flipc/flipc_usermsg.h>
#endif

void
norma_bootstrap(void)
{
#if	DIPC
	dipc_bootstrap();
#endif	/* DIPC */

#if	NORMA_VM
	norma_vm_init();
#endif	/* NORMA_VM */

#if	FLIPC
	flipc_system_init();
#endif	/* FLIPC */	
}
