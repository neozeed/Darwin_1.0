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
 * Revision 1.1.1.1  1998/09/22 21:05:30  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:45  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.1.6.1  1995/02/23  17:57:02  alanl
 * 	Take from DIPC2_SHARED
 * 	[1995/01/03  21:55:30  alanl]
 *
 * Revision 1.1.4.2  1994/10/05  22:09:13  alanl
 * 	Extend KKT_HANDLE_INFO to return the code with which a
 * 	handle was aborted.
 * 	[94/10/05            alanl]
 * 
 * Revision 1.1.4.1  1994/08/04  02:27:48  mmp
 * 	Brought forward from dipc_shared.
 * 	[1994/08/03  20:38:53  mmp]
 * 
 * Revision 1.1.2.1  1994/06/14  13:23:33  rwd
 * 	Created
 * 	[94/06/10            rwd]
 * 
 * $EndLog$
 */
/*
 *	The node_map_t is an opaque pointer to a kkt node map.
 */

#ifndef	_MACH_KKT_TYPES_H_
#define	_MACH_KKT_TYPES_H_

typedef unsigned int	node_map_t;	/* at least 32 bits */
#define NODE_MAP_NULL	((node_map_t) 0)

/*
 *	The node_name is an opaque type that uniquely
 *	identifies a node within a NORMA domain.  The
 *	transport must be able to route a message to
 *	the node specified by a node_name.
 */
typedef unsigned int	node_name;	/* at least 32 bits */
#define	NODE_NAME_NULL	((node_name) -1)

/*
 *	The local node name is useful for having a destination node
 *	that is not node_self() but in reality will be routed to
 *	node_self().
 *
 *	N.B.  This node name is ONLY available on versions of KKT
 *	compiled with KERNEL_TEST=1.  Production kernels do NOT
 *	support this node name.
 */
#define	NODE_NAME_LOCAL	((node_name) -2)

/*
 * Handle info
 */
typedef struct handle_info {
	node_name	node;		/* remote node */
	kern_return_t	abort_code;	/* error code if handle aborted */
} *handle_info_t;

/*
 *	The handle_t uniquely identifies a transport endpoint
 *	on the local node.  A handle_t is only valid relative
 *	to the local node and may not be assumed to be valid
 *	on a remote node.
 */
typedef unsigned int	handle_t;	/* at least 32 bits */
#define	HANDLE_NULL	((handle_t) 0)

/*
 * An endpoint is defined to be two pointers quantity which is
 * opaque to the underlying transport.
 */
typedef struct endpoint {
	void	*ep[2];
} endpoint_t;

#endif	/*_MACH_KKT_TYPES_H_*/
