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
 * Revision 1.2  1998/04/29 17:36:39  mburg
 * MK7.3 merger
 *
 * Revision 1.1.37.1  1998/02/03  09:32:57  gdt
 * 	Merge up to MK7.3
 * 	[1998/02/03  09:17:05  gdt]
 *
 * Revision 1.1.34.1  1997/02/17  11:24:51  ferranti
 * 	Removed handling of physical buffers for DIPC introduced during
 * 	the port on HP_PA platform.
 * 	[1997/02/17  11:16:56  ferranti]
 * 
 * Revision 1.1.13.5  1996/07/31  09:57:12  paire
 * 	Merged with nmk20b7_shared (1.1.27.2 -> 1.1.27.1)
 * 	[96/06/10            paire]
 * 
 * Revision 1.1.27.2  1996/07/30  15:15:27  ferranti
 * 	Use virtual memory buffers (io_scatter) for SCSI NORMA
 * 	clusters. Changed ios_phys_addr field of kkt_sglist
 * 	struct into ios_address.
 * 	[1996/07/30  15:12:57  ferranti]
 * 
 * Revision 1.1.27.1  1996/05/10  16:38:38  ferranti
 * 	Modified kkt_sglist_t structure in order to record
 * 	in the ios_phys_address field both physical memory
 * 	or virtual memory buffers. The ios_is_phys field
 * 	making the difference.
 * 	[1996/05/10  16:24:33  ferranti]
 * 
 * Revision 1.1.35.1  1997/06/17  03:00:59  devrcs
 * 	Activate uid node incarnation.
 * 	[1996/09/11  20:08:24  watkins]
 * 
 * 	Merge with rt3_merge.
 * 	[1996/03/25  17:34:47  watkins]
 * 
 * Revision 1.1.22.2  1996/03/21  19:16:33  watkins
 * 	Add kkt channel for rt rpc service.
 * 
 * Revision 1.1.13.4  1995/06/13  18:19:47  sjs
 * 	Merge from flipc_shared.
 * 	[95/05/22            sjs]
 * 
 * Revision 1.1.13.2  1995/05/15  18:08:36  rwd
 * 	Add CHAN_DEVICE_KT.
 * 	[95/04/10            rwd]
 * 
 * Revision 1.1.10.4  1995/01/06  15:39:19  rwd
 * 	Merged up to dipc_b2
 * 	[1995/01/05  20:16:59  rwd]
 * 
 * Revision 1.1.10.3  1994/12/12  19:04:43  rwd
 * 	Merged with changes from 1.1.10.2
 * 	[1994/12/12  19:03:29  rwd]
 * 
 * 	Define KKT_MIN_ERRNO and KKT_MAX_ERRNO.
 * 	[94/12/12            rwd]
 * 
 * Revision 1.1.10.2  1994/12/12  17:46:32  randys
 * 	Added channel for use by FLIPC
 * 	[1994/12/12  05:14:58  randys]
 * 
 * Revision 1.1.10.1  1994/12/09  15:46:33  rwd
 * 	Pulled from dipc2_shared
 * 	[1994/12/08  18:52:39  rwd]
 * 
 * Revision 1.1.6.9  1994/12/06  20:12:58  alanl
 * 	Add KKT_NODE_SELF and KKT_NODE_IS_VALID prototypes.
 * 	[94/12/05            rwd]
 * 
 * Revision 1.1.6.10  1994/12/27  21:10:02  mmp
 * 	Add thread_context arg to kkt callback typedef.
 * 	[1994/12/24  03:25:04  mmp]
 * 
 * Revision 1.1.6.9  1994/12/06  20:12:58  alanl
 * 	Add KKT_NODE_SELF and KKT_NODE_IS_VALID prototypes.
 * 	[94/12/05            rwd]
 * 
 * Revision 1.1.6.8  1994/11/09  03:42:14  alanl
 * 	DIPC and KKT restructuring for transports that provide
 * 	their own threads.  (w/travos)
 * 	[94/11/02            alanl]
 * 
 * Revision 1.1.6.7  1994/10/28  18:59:42  rwd
 * 	Add kkt_bootstrap_init prototype.
 * 	[94/10/28            rwd]
 * 
 * Revision 1.1.6.6  1994/10/05  22:09:09  alanl
 * 	Revamped implementation error code and specification until they
 * 	are largely in agreement.
 * 	[94/10/05            alanl]
 * 
 * 	Rationalized KKT_HANDLE_ABORT error passing conventions.
 * 	Callbacks always receive KKT_SUCCESS, KKT_ABORT or a KKT
 * 	transport error.  Handles have an opaque error argument
 * 	recording the "reason" supplied by the caller of
 * 	KKT_HANDLE_ABORT.  The caller's error space thus does not
 * 	conflict with the KKT error space.
 * 	[94/10/03            alanl]
 * 
 * Revision 1.1.6.5  1994/09/29  13:34:40  rwd
 * 	Remove unncessary receiver argument from KKT_HANDLE_ABORT.
 * 	[94/09/29            rwd]
 * 
 * Revision 1.1.6.4  1994/09/28  15:55:05  alanl
 * 	Defined new error codes, KKT_ENDPOINT_DEAD and
 * 	KKT_MKMSG_DESTROYED, to use in conjunction with
 * 	KKT_HANDLE_ABORT.  Because ABORT invokes callbacks and
 * 	supplies them with a "reason" argument, callbacks must
 * 	use the same error code space that contains KKT_SUCCESS.
 * 	[94/09/23            alanl]
 * 
 * Revision 1.1.6.3  1994/08/11  14:43:23  rwd
 * 	Prototype pointers to functions.
 * 	[94/08/09            rwd]
 * 
 * Revision 1.1.6.2  1994/08/05  19:42:32  mmp
 * 	Added new scatter/gather bit to interface
 * 	[94/07/22            rwd]
 * 
 * Revision 1.1.6.1  1994/08/04  02:27:42  mmp
 * 	Brought forward from dipc_shared.
 * 	[1994/08/03  20:38:49  mmp]
 * 
 * Revision 1.1.2.14  1994/07/18  19:25:31  rwd
 * 	Merged with changes from 1.1.2.13
 * 	[1994/07/18  19:24:35  rwd]
 * 
 * 	Fix malloc in KKT_CHANNEL_INIT and KKT_RPC_INIT.  Fix KKT_RPC_REPLY.
 * 	[94/07/08            rwd]
 * 
 * Revision 1.1.2.13  1994/07/13  20:19:59  sjs
 * 	Added new channel for raw testing of transport interfaces.
 * 	[94/07/13            sjs]
 * 
 * Revision 1.1.2.12  1994/07/08  01:58:49  alanl
 * 	Add prototypes and some comments for
 * 	KKT interfaces.
 * 	[1994/07/08  01:56:46  alanl]
 * 
 * Revision 1.1.2.11  1994/06/14  13:23:28  rwd
 * 	Remove typedefs to kkt_types.h
 * 	[94/06/10            rwd]
 * 
 * Revision 1.1.2.10  1994/06/09  14:40:12  rwd
 * 	Merged with changes from 1.1.2.9
 * 	[1994/06/09  14:37:51  rwd]
 * 
 * 	Add comment in #endif
 * 	[94/06/06            rwd]
 * 
 * Revision 1.1.2.9  1994/06/08  15:56:00  sjs
 * 	Fixed #endif to be ANSII compliant.
 * 	[94/06/08            sjs]
 * 
 * Revision 1.1.2.8  1994/05/20  18:33:38  alanl
 * 	Added CHAN_DIPC_MIGRATION
 * 	[94/05/17            rwd]
 * 
 * Revision 1.1.2.7  1994/05/17  16:08:08  mmp
 * 	Added CHAN_DIPC_RPC_INTR.
 * 	Changed the buf field in struct rpc_buf to buff to match both
 * 	the spec and the similar field in struct transmit.
 * 	[1994/05/17  16:07:17  mmp]
 * 
 * Revision 1.1.2.6  1994/05/16  20:11:31  sjs
 * 	Added error codes, new channel designator.
 * 	[94/05/16            sjs]
 * 
 * Revision 1.1.2.5  1994/05/13  16:23:52  sjs
 * 	Added rpc_buf definitions.
 * 	[94/05/13            sjs]
 * 
 * Revision 1.1.2.4  1994/05/11  18:49:10  mmp
 * 	added channel_t typedef and channel declarations (moved here from kkt.h).
 * 	[1994/05/11  18:48:32  mmp]
 * 
 * Revision 1.1.2.3  1994/04/29  19:59:26  sjs
 * 	Merged with changes from 1.1.2.2
 * 	[1994/04/29  19:59:05  sjs]
 * 
 * 	Added transmit and endpoint structures, added correct
 * 	platform specific include file.  More error codes.
 * 	[94/04/13            sjs]
 * 
 * Revision 1.1.2.2  1994/04/20  18:44:10  alanl
 * 	Replace temporary kkt dipc.h with permanent one.
 * 	File resides in mach/, not dipc/, so the ifdef
 * 	should refer to MACH instead of DIPC.
 * 	[1994/04/20  18:31:19  alanl]
 * 
 * Revision 1.1.2.1  1994/03/25  14:54:38  sjs
 * 	Initial revision.
 * 	[1994/03/25  14:53:43  sjs]
 * 
 * $EndLog$
 */

/*
 * KKT machine independent request parameters
 */

#ifndef	_DIPC_KKT_REQUEST_H_
#define	_DIPC_KKT_REQUEST_H_

#include <mach/mach_types.h>
#include <mach/error.h>
#include <mach/boolean.h>
#include <mach/vm_prot.h>
#include <kern/assert.h>
#include <mach_kdb.h>
#include <mach/kkt_types.h>
#include <machine/kkt.h>
#include <device/io_scatter.h>


/*
 *	KKT exports an SPL level at which callbacks will happen
 *	from KKT to a client.  The client needs to know this
 *	level for synchronizing access to data structures shared
 *	between callbacks and thread context operations.
 *
 *	This spl level is referred to as splkkt, and may be
 *	used in the traditional fashion, e.g.,
 *		s = splkkt();
 *		...
 *		splx(s);
 *
 *	The definition comes from machine/kkt.h.
 */


/*
 * A transport request comes in two pieces: half of it is a
 * type designator, and half of it is the status of the
 * operation (and may be unnecessary, let's see).
 */
typedef struct transport_request {
	short		type;
	short		status;
} tran_req;

/*
 * Valid transport request types
 */
#define	REQUEST_SEND	0x001
#define	REQUEST_RECV	0x002
#define REQUEST_COMMANDS (REQUEST_SEND|REQUEST_RECV)
#define	REQUEST_SG	0x100

typedef io_scatter_t kkt_sglist_t;

/*
 * Primary request block.  Allocated by the user and handed to 
 * KKT_REQUEST.
 */
typedef	struct request_block	*request_block_t;
typedef	unsigned int		request_t;
typedef	kern_return_t		kkt_return_t;
typedef unsigned int		channel_t;


typedef void (*kkt_callback_t)(
			kkt_return_t,
			handle_t,
			request_block_t,
			boolean_t);


struct request_block {
	unsigned int	request_type;
	union {
		vm_offset_t	address;
		kkt_sglist_t	list;
	} data;		
	vm_size_t	data_length;
	kkt_callback_t	callback;
	unsigned int	args[2];
	request_block_t	next;
	transport_t	transport;
};

#define	NULL_REQUEST	(request_block_t)0

/*
 * Error codes
 */
#define	KKTSUB		9
#define	KKTERR(errno)	(err_kern | err_sub(KKTSUB) | errno)

#define KKT_MIN_ERRNO		KKTERR(0)    /* MIN ERRNO */
#define	KKT_SUCCESS		KKTERR(0)    /* operation succeeded */
#define	KKT_DATA_RECEIVED	KKTERR(1)    /* data received immediately,
						...no callback forthcoming */
#define	KKT_DATA_SENT		KKTERR(2)    /* data sent immediately, no
					        ...callback forthcoming */
#define	KKT_ERROR		KKTERR(3)    /* vague:  probably a lower-level
						...transport error */
#define	KKT_ABORT		KKTERR(4)    /* operation was aborted, handle
						...has aborted or is aborting */
#define	KKT_INVALID_ARGUMENT	KKTERR(5)    /* bad argument or range error */
#define	KKT_INVALID_HANDLE	KKTERR(6)    /* illegal handle value */
#define	KKT_NODE_DOWN		KKTERR(7)    /* can't reach node */
#define	KKT_RESOURCE_SHORTAGE	KKTERR(8)    /* requested or required resource
						...can't be allocated */
#define	KKT_NODE_PRESENT	KKTERR(9)    /* node already in map */
#define	KKT_MAP_EMPTY		KKTERR(10)   /* no nodes left in map */
#define	KKT_INVALID_REQUEST	KKTERR(11)   /* illegal operation requested */
#define	KKT_RELEASE		KKTERR(12)   /* resources no longer in use,
						...caller can free at will */
#define KKT_INVALID_CHANNEL	KKTERR(13)   /* illegal channel specifier */
#define	KKT_HANDLE_BUSY		KKTERR(14)   /* connection is active */
#define	KKT_HANDLE_UNUSED	KKTERR(15)   /* handle is unconnected */
#define	KKT_HANDLE_MIGRATED	KKTERR(16)   /* handle already migrated */
#define	KKT_CHANNEL_IN_USE	KKTERR(17)   /* channel already exists */
#define	KKT_HANDLE_TYPE		KKTERR(18)   /* handle is of wrong type,
					        ...possibly aborted */
#define	KKT_INVALID_NODE	KKTERR(19)   /* no such node */
#define KKT_MAX_ERRNO		KKTERR(19)   /* MAX ERRNO */


struct rpc_buf {
	unsigned int		*ulink;		/* user available field */
	handle_t		handle;		/* RPC handle */
	unsigned int		buff[1];	/* variable sized buffer */
};

typedef struct rpc_buf	*rpc_buf_t;

/* channel declarations */
#define CHAN_KKT		0	/* KKT internal channel */
#define CHAN_USER_KMSG		1	/* user kmsg channel */
#define CHAN_DIPC_RPC		2	/* dipc rpc, thread delivery */
#define CHAN_DIPC_RPC_INTR	3	/* dipc rpc, interrupt delivery */
#define CHAN_TEST_KMSG		4	/* kkt test channel */
#define CHAN_TEST_RPC		5	/* kkt rpc test channel */
#define CHAN_DIPC_MIGRATION	6	/* dipc migration threads */
#define CHAN_RESERVE_TEST	7	/* Raw RPC/RDMA testing */
#define CHAN_FLIPC		8	/* FLIPC channel */
#define CHAN_DEVICE_KT		9	/* KKT "kt" ethernet device */
#define CHAN_MACH_RPC		10	/* mach rpc service */

/*
 *	KKT properties.  This is configuration information
 *	exported from KKT at run-time.
 */
typedef enum kkt_upcall {
	KKT_THREAD_UPCALLS,		/* all upcalls happen in thread context */
	KKT_INTERRUPT_UPCALLS,		/* all upcalls happen in interrupt context */
	KKT_MIXED_UPCALLS		/* upcalls can happen in EITHER context */
} kkt_upcall_t;

typedef struct kkt_properties {
	/*
	 *	How does KKT invoke client services during upcalls?
	 */
	kkt_upcall_t	kkt_upcall_methods;
	/*
	 *	Total number of *usable* bytes that a client can
	 *	fit into an inline KKT buffer during an RPC.
	 *	When sending data via KKT_SEND_CONNECT, KKT may
	 *	be able to optimize the send by cramming the
	 *	inline data into an RPC buffer (but specifying
	 *	larger amounts of inline data still works).
	 */
	vm_size_t	kkt_buffer_size;
} kkt_properties_t;


/*
 *	KKT interfaces.
 *
 *	Refer to the document, "Kernel to Kernel Transport Interface
 *	for the Mach Kernel", Open Software Foundation Research
 *	Institute, 1994.
 */

typedef void (*kkt_channel_deliver_t)(channel_t, handle_t, endpoint_t *,
				      vm_size_t, boolean_t, boolean_t);
typedef	void (*kkt_rpc_deliver_t)(channel_t, rpc_buf_t, boolean_t);
typedef	void *(*kkt_malloc_t)(channel_t, vm_size_t);
typedef void (*kkt_free_t)(channel_t, vm_offset_t, vm_size_t);
typedef void (*kkt_error_t)(kkt_return_t, handle_t, request_block_t);

void kkt_bootstrap_init(void);

/*
 *	Set up a general communications path, called a
 *	channel, which has a defined set of resources
 *	and operations.  The resources include buffers
 *	for sending and receiving operations.  The
 *	operations include upcalls from KKT to the
 *	client code.
 *
 *	Individual communications endpoints, or handles,
 *	are allocated in the context of a channel.
 *
 *	channel		identifier specified by caller
 *			("channel name")
 *	senders		number of sending buffers
 *	receivers	number of receiving buffers
 *	(*deliver)()	upcall for incoming data
 *	(*malloc)()	upcall when KKT needs memory
 *	(*free)()	upcall to free (*malloc)()'d memory
 */
kkt_return_t	KKT_CHANNEL_INIT(
			channel_t	channel,
			unsigned int	senders,
			unsigned int	receivers,
			kkt_channel_deliver_t deliver,
			kkt_malloc_t	malloc,
			kkt_free_t	free);

/*
 *	Allocate a handle within a channel.  A handle
 *	is a connection endpoint; both receiver and
 *	sender must have a handle in order to communicate.
 *	A handle is not associated with a node at this time.
 *	Requesting a handle may be thought of as requesting
 *	a class of service -- and possibly having to wait
 *	until resources become available.
 *
 *	channel		channel name
 *	uhandle		handle returned from KKT to caller
 *	(*error)()	upcall if transmission error encountered
 *	wait		TRUE=block awaiting handle availability
 *	must_request	TRUE=data flows only on receiver request
 *			(i.e., receiver-pull); FALSE=transport
 *			may optimize (possibly do sender-push)
 */
kkt_return_t	KKT_HANDLE_ALLOC(
			channel_t	channel,
			handle_t	*uhandle,
			kkt_error_t	error,
			boolean_t	wait,
			boolean_t	must_request);

/*
 *	Free up a handle.
 */
kkt_return_t	KKT_HANDLE_FREE(
			handle_t	handle);

/*
 *	Initiate arbitrary-length kernel-to-kernel
 *	data transfer.  The sender blocks until the
 *	receiver responds.  This call causes a (*deliver)()
 *	upcall on the receiver's channel.
 *
 *	handle		KKT-level communications endpoint
 *	node		destination for data
 *	endpoint	client-code's communications endpoint
 *	request		initial data to move
 *	more		hint as to whether more data will follow
 *	*ret		return values from KKT_CONNECT_REPLY
 */
kkt_return_t	KKT_SEND_CONNECT(
			handle_t	handle,
			node_name	node,
			endpoint_t	*endpoint,
			request_block_t request,
			boolean_t	more,
			kern_return_t	*ret);

/*
 *	Respond to KKT_SEND_CONNECT.
 *
 *	handle		KKT handle
 *	status		user-defined success/error indicator
 *	substatus	user-defined success/error indicator
 */
kkt_return_t	KKT_CONNECT_REPLY(
			handle_t	handle,
			kern_return_t	status,
			kern_return_t	substatus);

/*
 *	Enqueue a request block on the handle.  Initiate
 *	transmission if necessary, otherwise, transmit
 *	the data when the opportunity arises.
 *
 *	handle		KKT handle
 *	request		request block describing data to be transferred
 */
kkt_return_t	KKT_REQUEST(
			handle_t	handle,
			request_block_t	request);

/*
 *	Copy the entire RPC buffer contents into the
 *	caller's buffer.  The buffer and size are
 *	inferred from the handle; the caller takes
 *	responsibility for supplying a region large
 *	enough to contain the data to be copied.
 */
kkt_return_t	KKT_COPYOUT_RECV_BUFFER(
			handle_t	handle,
			char		*dest);

/*
 *	Abort ongoing operations on the specified handle.
 *	This need should rarely arise.
 *
 *	handle		KKT handle
 *	reason		status information to be supplied to
 *			any parties currently involved in a
 *			data transfer or otherwise somehow
 *			using this handle
 */
kkt_return_t	KKT_HANDLE_ABORT(
			handle_t	handle,
			kern_return_t	reason);

/*
 *	Obtain status information on a handle.
 *
 *	handle		KKT handle
 *	info		Whatever information KKT can
 *			supply about a handle
 */
kkt_return_t	KKT_HANDLE_INFO(
			handle_t	handle,
			handle_info_t	info);

/*
 *	Move the state associated with old_handle on
 *	node *node* to new_handle.  The new_handle
 *	must have already been created as a valid handle
 *	prior to invoking this call.
 *
 *	node		node from which handle is migrating
 *	old_handle	identifier of handle on node *node*
 *	new_handle	valid handle on current node
 */
kkt_return_t	KKT_HANDLE_MIGRATE(
			node_name	node,
			int		old_handle,
			handle_t	new_handle);

/*
 *	Allocate a map capable of tracking nodes
 *	within a KKT/NORMA domain.  This map need
 *	not be dense.  For instance, an Ethernet-
 *	based cluster may use sparse node names
 *	based on IP addresses.  A node map may be
 *	dynamic, i.e., adding a node to a map may
 *	require dynamic memory allocation.  For
 *	this reason, we define a "fallback" map;
 *	such a map is allocated with sufficient
 *	resources to track every node in the domain.
 *	(A fallback map is necessary when a node is
 *	very low on memory and has no other way to
 *	remember what is going on.)
 *
 *	map		pointer to a map created by KKT
 *	full		TRUE=caller requests creation of
 *			a fallback map
 */
kkt_return_t	KKT_MAP_ALLOC(
			node_map_t	*map,
			boolean_t	full);

/*
 *	Destroy a map.  The map need not be empty.
 *	There is no concept of a reference-counted
 *	map, so destruction takes place immediately.
 */
kkt_return_t	KKT_MAP_FREE(
			node_map_t	map);

/*
 *	Add a node to a map.  This call may block
 *	for memory allocation unless the map is
 *	known to be a fallback map.
 *
 *	map		KKT node map
 *	node		node to add to map (returns
 *			error if node already in map)
 */
kkt_return_t	KKT_ADD_NODE(
			node_map_t	map,
			node_name	node);

/*
 *	Remove the next node from a map.  The caller
 *	does not specify the node to be removed;
 *	rather, KKT returns the node_name of the
 *	next node in the map.
 *
 *	map		KKT node map
 *	node		name of node removed from map
 */
kkt_return_t	KKT_REMOVE_NODE(
			node_map_t	map,
			node_name	*node);

kkt_return_t	KKT_RPC_INIT(
			channel_t	channel,
			unsigned int	senders,
			unsigned int	receivers,
			kkt_rpc_deliver_t deliver,
			kkt_malloc_t	malloc,
			kkt_free_t	free,
			unsigned int	max_send_size);

kkt_return_t	KKT_RPC_HANDLE_ALLOC(
			channel_t	channel,
			handle_t	*uhandle,
			vm_size_t	size);

kkt_return_t	KKT_RPC_HANDLE_BUF(
			handle_t	handle,
			void		**userbuf);

kkt_return_t	KKT_RPC_HANDLE_FREE(
			handle_t	handle);

kkt_return_t	KKT_RPC(
			node_name	node,
			handle_t	handle);

kkt_return_t	KKT_RPC_REPLY(
			handle_t	handle);

kkt_return_t	KKT_PROPERTIES(
			kkt_properties_t *prop);

node_name	KKT_NODE_SELF(void);

boolean_t	KKT_NODE_IS_VALID(node_name node);

kkt_return_t	KKT_NODE_INCARN(node_name node, unsigned int *incarn);

#endif	/* _DIPC_KKT_REQUEST_H_ */
