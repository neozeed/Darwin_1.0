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
 * Revision 1.1.1.1  1998/09/22 21:05:33  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1  1998/04/29 17:36:12  mburg
 * MK7.3 merger
 *
 * Revision 1.1.7.1  1998/02/03  09:29:37  gdt
 * 	Merge up to MK7.3
 * 	[1998/02/03  09:14:34  gdt]
 *
 * Revision 1.1.4.1  1997/06/17  02:58:23  devrcs
 * 	Handle some errors caused by node failures.
 * 	[1996/09/25  19:00:00  watkins]
 * 
 * 	Added `#include' for DIPC types.  This is needed for non-DIPC
 * 	configurations to build.  Also, defined away trans-node RPC
 * 	routines when DIPC is not configured.  (Trans-node RPC currently
 * 	uses DIPC features.)
 * 	[1996/04/16  19:02:12  rkc]
 * 
 * 	Use scheduling framework to access scheduling parameters.
 * 	[1996/03/29  22:08:50  watkins]
 * 
 * 	Merge with rt3_merge.
 * 	[1996/03/22  22:49:57  watkins]
 * 
 * Revision 1.1.1.2  1996/03/21  19:21:13  watkins
 * 	Initial revision.
 * 
 * $EndLog$
 */


/*
 *	rpc_remote.h
 *
 *	Definitions and functions for the Mach RPC remote.
 */


#include <dipc.h>
#include <kernel_test.h>
#include <mach/kkt_request.h>
#include <dipc/dipc_types.h>
#include <mach/policy.h>
#include <kern/sf.h>
#include <kern/mk_sp.h>


/*
 *	IN and OUT are defined for documenting argument-passing 
 *	conventions and have no meaning to the compiler.
 */
#define	IN
#define	OUT
#define	INOUT


/*
 *	Bound on the highest allocated remote procedure call number.  
 *	This number may be used for allocating arrays.
 */
#define	RPC_KKT_MAX_PROC		10


/*
 *	We staticly size the transfer arrays for now. This will
 *	be dynamic later.
 */
#define RPC_ARRAYS_MAX_SIZE	4
#define RPC_ARRAYS_MAX_ENTRY	32
#define RPC_ARRAYS_MAX_DATA	(RPC_ARRAYS_MAX_SIZE * RPC_ARRAYS_MAX_ENTRY)


/*
 *	rpc_control
 *
 *	Control information sent with a remote RPC request, including
 *	scheduling information for the request on remote node.
 */

typedef struct rpc_control {
	policy_t                			policy;
       	union {
		mk_sp_attribute_struct_t	mk;
	}						attributes;
	union {
		mk_sp_info_struct_t		mk;
	}						information;
} Rpc_Control;

typedef struct rpc_service {
	node_name		src_node;
	node_name		dst_node;
	uid_t			uid;
	int			routine_num;
	int			signature_token;
	void			*instance;
} Rpc_Service;

typedef struct rpc_control *rpc_control_t;
typedef struct rpc_service *rpc_service_t;


/*
 *	rpc invoke
 *	
 *	Request RPC service from a remote node. The initial contact
 *	contact includes subsystem, signature token and scheduling 
 *	information. The invoke request will be acknowledged with a
 *	reply.
 *
 *	Send a list of arrays to a remote node and receive a list of
 *	arrays in return. Count is the number of arrays and the number of 
 *	size values in the Sizes array. Data points to the arrays, one 
 *	after another.
 */

#define	RPC_KKT_INVOKE		2
#define	RPC_KKT_INVOKE_ACK	3

extern kkt_return_t	rpc_kkt_invoke(
			IN	rpc_service_t		servp,
			INOUT	unsigned		*count,
			INOUT	unsigned		*sizes,
			INOUT	unsigned		*datap,
                        OUT     rpc_return_t       	*rr);

extern rpc_return_t	rpc_kkt_invoke_ack( 
                        OUT     rpc_return_t       	rr);

struct rpc_kkt_invoke_args {
	Rpc_Service		service;
	unsigned		count;
	unsigned		sizes[RPC_ARRAYS_MAX_SIZE];
	unsigned		data[RPC_ARRAYS_MAX_DATA];
};


/*
 *      rpc reply
 *
 * 	Reply to an RPC request from a remote node. The reply will
 *	include any result data, as well as signature token and sched-
 *	uling information. This reply must be acknowledged by the client
 *	side of the RPC.
 *
 *	Send a list of arrays to a remote node and receive a list of
 *	arrays in return. Count is the number of arrays and the number of 
 *	size values in the Sizes array. Data points to the arrays, one 
 *	after another.
 */

#define RPC_KKT_REPLY		4
#define RPC_KKT_REPLY_ACK	5

extern kkt_return_t     rpc_kkt_reply(
			IN	rpc_service_t		servp,
			INOUT	unsigned		*count,
			INOUT	unsigned		*sizes,
			INOUT	unsigned		*datap,
                        OUT     rpc_return_t       	*rr);

extern rpc_return_t     rpc_kkt_reply_ack(
			OUT	rpc_return_t		rr);

struct rpc_kkt_reply_args {
	Rpc_Service		service;
	unsigned		count;
	unsigned		sizes[RPC_ARRAYS_MAX_SIZE];
	unsigned		data[RPC_ARRAYS_MAX_DATA];
};



/* 
 *	All possible arguments for thread-level service 
 */
typedef struct rpc_kkt_args {
	int			procnum;
	rpc_return_t		rr;
	Rpc_Control		control;
      	Rpc_Service             service;
	vm_offset_t		kkt_size;
} *rpc_kkt_args_t;

#define alink 	rr
#define alink_t	rpc_return_t

#define RPC_KKT_ARG_NULL                ((rpc_kkt_args_t)0)


/*
 *	Macros for sanity checking.
 */

#define valid_rpc_kkt_proc(p)	(p >= 2 && p <= 5 && p < RPC_KKT_MAX_PROC)



/*
 *	External Declarations
 */

void		rpc_bootstrap(void);

rpc_return_t	rpc_server_reply(void);

rpc_return_t	rpc_client_reply(void);

rpc_return_t	rpc_server_invoke(rpc_kkt_args_t);

rpc_return_t	rpc_client_invoke(
				ipc_port_t, rpc_id_t, rpc_signature_t, 
				rpc_size_t);

/*
 * Definitions for Trans-node RPC routines when DIPC is disabled
 */

#if	!DIPC
#define rpc_client_invoke(a, b, c, d)	KERN_FAILURE
#define rpc_server_reply()		panic("rpc_server_reply() disabled")
#endif	/* DIPC */


/*
 *      Macros for declaring and using statistics variables.
 *      These variables are present whenever statistics
 *      or assertion checking are enabled.
 */

/* #include <rpc_stats.h> */
#define  RPC_STATS 1
#include <mach_assert.h>


#define RPC_DO_STATS   (RPC_STATS || MACH_ASSERT)


#if	RPC_DO_STATS
#define dstat_decl(clause)      clause
#define dstat(clause)           clause
#define dstat_max(m,c)          ((c) > (m) ? (m) = (c) : 0)
extern void	rpc_kkt_stats(void);
#else	/* RPC_DO_STATS */
#define dstat_decl(clause)
#define dstat(clause)
#define dstat_max(m,c)
#endif	/* RPC_DO_STATS */

