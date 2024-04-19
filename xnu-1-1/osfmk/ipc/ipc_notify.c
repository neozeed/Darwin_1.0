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
 * @OSF_COPYRIGHT@
 */
/*
 * HISTORY
 * 
 * Revision 1.1.1.1  1998/09/22 21:05:29  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:26:16  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.2.24.4  1995/02/23  17:31:21  alanl
 * 	Promoted MACH_RT to a full-fledged build option.  (w/mmp)
 * 	[95/02/21            alanl]
 *
 * 	DIPC:  Merge from nmk17b2 to nmk18b8.
 * 	Removed rt boolean argument from ipc_kmsg_* functions in favor of
 * 	a new bit in the msg: MACH_MSGH_BITS_RTALLOC.  Determine rt
 * 	status of dest port and call either ikm_alloc or ikm_rtalloc, then
 * 	 mark the new kmsg with the RTALLOC bit if appropriate.
 * 	[95/01/05            mmp]
 *
 * Revision 1.2.28.3  1994/10/11  18:24:57  rwd
 * 	Comment out the tr for each entry into ipc_notify_send_once.  This
 * 	can dominate the tr log quite easily.
 * 	[94/10/07            rwd]
 * 
 * Revision 1.2.28.2  1994/09/30  21:26:15  alanl
 * 	Convert code in ipc_notify_send_once to detect dead
 * 	send-once rights from NORMA_IPC to DIPC.
 * 	[94/09/30            alanl]
 * 
 * Revision 1.2.28.1  1994/09/01  12:51:19  alanl
 * 	DIPC:  Eliminate dead NORMA_IPC code.
 * 	[94/09/01            alanl]
 * 
 * Revision 1.2.24.3  1994/09/23  02:09:05  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:29:36  ezf]
 * 
 * Revision 1.2.24.2  1994/08/18  23:11:09  widyono
 * 	Notify routines, integrate RT: ipc_notify_init_port_deleted(),
 * 	    ipc_notify_init_port_destroyed(), ipc_notify_init_no_senders(),
 * 	    ipc_notify_init_send_once(), ipc_notify_init_dead_name()
 * 	[1994/07/28  22:24:14  widyono]
 * 
 * Revision 1.2.24.1  1994/06/13  19:57:31  dlb
 * 	Merge MK6 and NMK17
 * 	[1994/06/13  16:22:14  dlb]
 * 
 * Revision 1.2.22.1  1994/02/10  16:25:18  bernadat
 * 	Removed call to norma_ipc_notify_no_sender in ipc_notify_no_senders,
 * 	since it must be done with ip_lock held in the same critical section
 * 	as the "port->ip_srights--" operation.
 * 	[93/11/08            paire]
 * 	[94/02/10            bernadat]
 * 
 * Revision 1.2.8.7  1993/09/09  16:06:59  jeffc
 * 	CR9745 - delete message accepted notifications
 * 	[1993/09/03  22:14:56  jeffc]
 * 
 * Revision 1.2.8.6  1993/08/05  19:07:33  jeffc
 * 	CR9508 -- delete dead Mach3 code. Remove MACH_IPC_TYPED
 * 	[1993/08/04  16:51:31  jeffc]
 * 
 * 	CR9508 - Delete dead code. Remove MACH_IPC_COMPAT
 * 	[1993/08/03  17:07:31  jeffc]
 * 
 * Revision 1.2.8.5  1993/08/02  17:24:55  rod
 * 	ANSI prototypes:  include kern/misc_protos.h.  CR #9523.
 * 	[1993/07/31  15:03:12  rod]
 * 
 * Revision 1.2.8.4  1993/07/22  16:16:38  rod
 * 	Add ANSI prototypes.  CR #9523.
 * 	[1993/07/22  13:30:46  rod]
 * 
 * Revision 1.2.8.3  1993/07/19  18:46:51  travos
 * 	Changes to the trailer logic, both for correctness (CR8991)
 * 	and performances (CR9487): trailers have a default size set
 * 	to the minimum.
 * 	[1993/07/13  19:13:23  travos]
 * 
 * Revision 1.2.8.2  1993/06/09  02:32:23  gm
 * 	In ipc_notify_init_port_destroyed() initialize disposition in template
 * 	to MACH_MSG_TYPE_PORT_RECEIVE.  CR#8969.
 * 	[1993/04/28  11:17:32  rod]
 * 
 * 	Fix untyped notifications.  CR#8969.
 * 	[1993/04/27  11:28:21  rod]
 * 
 * Revision 1.2  1993/04/19  16:21:19  devrcs
 * 	Set ikm_version in compatability interfaces.
 * 	[1993/04/07  21:03:34  rod]
 * 
 * 	Added trailer support to untyped ipc.		[travos@osf.org]
 * 	[1993/04/06  19:39:03  travos]
 * 
 * 	Untyped ipc merge:
 * 	Remove the NDR format label from messages with no untyped data
 * 	[1993/03/12  22:49:45  travos]
 * 	added support for untyped notifications
 * 	[1993/02/25  22:03:08  fdr]
 * 
 * Revision 1.1  1992/09/30  02:07:42  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.5.2.2  92/03/28  10:09:23  jeffreyh
 * 	NORMA_IPC: Don't send send_once notification if port is dead.
 * 	[92/03/25            dlb]
 * 
 * Revision 2.5.2.1  92/01/03  16:35:29  jsb
 * 	Did I say ndproxy? I meant to say nsproxy.
 * 	[91/12/31  21:40:41  jsb]
 * 
 * 	Changes for IP_NORMA_REQUEST macros being renamed to ip_ndproxy{,m,p}.
 * 	[91/12/30  07:57:26  jsb]
 * 
 * 	Use IP_IS_NORMA_NSREQUEST macro.
 * 	[91/12/28  17:05:21  jsb]
 * 
 * 	Added norma_ipc_notify_no_senders hook in ipc_notify_no_senders.
 * 	[91/12/24  14:37:56  jsb]
 * 
 * Revision 2.5  91/08/28  11:13:41  jsb
 * 	Changed msgh_kind to msgh_seqno.
 * 	[91/08/09            rpd]
 * 
 * Revision 2.4  91/05/14  16:34:24  mrt
 * 	Correcting copyright
 * 
 * Revision 2.3  91/02/05  17:22:33  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  15:46:58  mrt]
 * 
 * Revision 2.2  90/06/02  14:50:50  rpd
 * 	Created for new IPC.
 * 	[90/03/26  20:57:58  rpd]
 * 
 */
/* CMU_ENDHIST */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	ipc/ipc_notify.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Notification-sending functions.
 */

#include <dipc.h>
#include <mach_rt.h>

#include <mach/port.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <kern/assert.h>
#include <kern/misc_protos.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_mqueue.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_port.h>
#if	DIPC
#include <ddb/tr.h>
#endif	/* DIPC */

/*
 * Forward declarations
 */
void ipc_notify_init_port_deleted(
	mach_port_deleted_notification_t	*n);

void ipc_notify_init_port_destroyed(
	mach_port_destroyed_notification_t	*n);

void ipc_notify_init_no_senders(
	mach_no_senders_notification_t		*n);

void ipc_notify_init_send_once(
	mach_send_once_notification_t		*n);

void ipc_notify_init_dead_name(
	mach_dead_name_notification_t		*n);

mach_port_deleted_notification_t	ipc_notify_port_deleted_template;
mach_port_destroyed_notification_t	ipc_notify_port_destroyed_template;
mach_no_senders_notification_t		ipc_notify_no_senders_template;
mach_send_once_notification_t		ipc_notify_send_once_template;
mach_dead_name_notification_t		ipc_notify_dead_name_template;

/*
 *	Routine:	ipc_notify_init_port_deleted
 *	Purpose:
 *		Initialize a template for port-deleted notifications.
 */

void
ipc_notify_init_port_deleted(
	mach_port_deleted_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_PORT_DELETED;
	m->msgh_size = ((int)sizeof *n) - sizeof(mach_msg_format_0_trailer_t);

	n->not_port = MACH_PORT_NULL;
	n->NDR = NDR_record;
	n->trailer.msgh_seqno = 0;
	n->trailer.msgh_sender = KERNEL_SECURITY_TOKEN;
	n->trailer.msgh_trailer_type = MACH_MSG_TRAILER_FORMAT_0;
	n->trailer.msgh_trailer_size = MACH_MSG_TRAILER_MINIMUM_SIZE;
}

/*
 *	Routine:	ipc_notify_init_port_destroyed
 *	Purpose:
 *		Initialize a template for port-destroyed notifications.
 */

void
ipc_notify_init_port_destroyed(
	mach_port_destroyed_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;

	m->msgh_bits = MACH_MSGH_BITS_COMPLEX |
		MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_PORT_DESTROYED;
	m->msgh_size = ((int)sizeof *n) - sizeof(mach_msg_format_0_trailer_t);

	n->not_body.msgh_descriptor_count = 1;
	n->not_port.disposition = MACH_MSG_TYPE_PORT_RECEIVE;
	n->not_port.name = MACH_PORT_NULL;
	n->not_port.type = MACH_MSG_PORT_DESCRIPTOR;
	n->trailer.msgh_seqno = 0;
	n->trailer.msgh_sender = KERNEL_SECURITY_TOKEN;
	n->trailer.msgh_trailer_type = MACH_MSG_TRAILER_FORMAT_0;
	n->trailer.msgh_trailer_size = MACH_MSG_TRAILER_MINIMUM_SIZE;
}

/*
 *	Routine:	ipc_notify_init_no_senders
 *	Purpose:
 *		Initialize a template for no-senders notifications.
 */

void
ipc_notify_init_no_senders(
	mach_no_senders_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_NO_SENDERS;
	m->msgh_size = ((int)sizeof *n) - sizeof(mach_msg_format_0_trailer_t);

	n->NDR = NDR_record;
	n->trailer.msgh_seqno = 0;
	n->trailer.msgh_sender = KERNEL_SECURITY_TOKEN;
	n->trailer.msgh_trailer_type = MACH_MSG_TRAILER_FORMAT_0;
	n->trailer.msgh_trailer_size = MACH_MSG_TRAILER_MINIMUM_SIZE;
	n->not_count = 0;
}

/*
 *	Routine:	ipc_notify_init_send_once
 *	Purpose:
 *		Initialize a template for send-once notifications.
 */

void
ipc_notify_init_send_once(
	mach_send_once_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_SEND_ONCE;
	m->msgh_size = ((int)sizeof *n) - sizeof(mach_msg_format_0_trailer_t);
	n->trailer.msgh_seqno = 0;
	n->trailer.msgh_sender = KERNEL_SECURITY_TOKEN;
	n->trailer.msgh_trailer_type = MACH_MSG_TRAILER_FORMAT_0;
	n->trailer.msgh_trailer_size = MACH_MSG_TRAILER_MINIMUM_SIZE;
}

/*
 *	Routine:	ipc_notify_init_dead_name
 *	Purpose:
 *		Initialize a template for dead-name notifications.
 */

void
ipc_notify_init_dead_name(
	mach_dead_name_notification_t	*n)
{
	mach_msg_header_t *m = &n->not_header;

	m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0);
	m->msgh_local_port = MACH_PORT_NULL;
	m->msgh_remote_port = MACH_PORT_NULL;
	m->msgh_id = MACH_NOTIFY_DEAD_NAME;
	m->msgh_size = ((int)sizeof *n) - sizeof(mach_msg_format_0_trailer_t);

	n->not_port = MACH_PORT_NULL;
	n->NDR = NDR_record;
	n->trailer.msgh_seqno = 0;
	n->trailer.msgh_sender = KERNEL_SECURITY_TOKEN;
	n->trailer.msgh_trailer_type = MACH_MSG_TRAILER_FORMAT_0;
	n->trailer.msgh_trailer_size = MACH_MSG_TRAILER_MINIMUM_SIZE;
}

/*
 *	Routine:	ipc_notify_init
 *	Purpose:
 *		Initialize the notification subsystem.
 */

void
ipc_notify_init(void)
{
	ipc_notify_init_port_deleted(&ipc_notify_port_deleted_template);
	ipc_notify_init_port_destroyed(&ipc_notify_port_destroyed_template);
	ipc_notify_init_no_senders(&ipc_notify_no_senders_template);
	ipc_notify_init_send_once(&ipc_notify_send_once_template);
	ipc_notify_init_dead_name(&ipc_notify_dead_name_template);
}

/*
 *	Routine:	ipc_notify_port_deleted
 *	Purpose:
 *		Send a port-deleted notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_port_deleted(
	ipc_port_t		port,
	mach_port_name_t	name)
{
	ipc_kmsg_t kmsg;
	mach_port_deleted_notification_t *n;

#if	MACH_RT
	if (IP_RT(port))
		kmsg = ikm_rtalloc(sizeof *n);
	else
#endif	/* MACH_RT */
		kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped port-deleted (0x%08x, 0x%x)\n", port, name);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_port_deleted_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_port_deleted_template;
#if	MACH_RT
	if (IP_RT(port))
		KMSG_MARK_RT(kmsg);
#endif	/* MACH_RT */

	n->not_header.msgh_remote_port = port;
	n->not_port = name;

	ipc_kmsg_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_port_destroyed
 *	Purpose:
 *		Send a port-destroyed notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 *		Consumes a ref for right, which should be a receive right
 *		prepped for placement into a message.  (In-transit,
 *		or in-limbo if a circularity was detected.)
 */

void
ipc_notify_port_destroyed(
	ipc_port_t	port,
	ipc_port_t	right)
{
	ipc_kmsg_t kmsg;
	mach_port_destroyed_notification_t *n;

#if	MACH_RT
	if (IP_RT(port))
		kmsg = ikm_rtalloc(sizeof *n);
	else
#endif	/* MACH_RT */
		kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped port-destroyed (0x%08x, 0x%08x)\n",
		       port, right);
		ipc_port_release_sonce(port);
		ipc_port_release_receive(right);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_port_destroyed_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_port_destroyed_template;
#if	MACH_RT
	if (IP_RT(port))
		KMSG_MARK_RT(kmsg);
#endif	/* MACH_RT */

	n->not_header.msgh_remote_port = port;
	n->not_port.name = right;

	ipc_kmsg_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_no_senders
 *	Purpose:
 *		Send a no-senders notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_no_senders(
	ipc_port_t		port,
	mach_port_mscount_t	mscount)
{
	ipc_kmsg_t kmsg;
	mach_no_senders_notification_t *n;

#if	MACH_RT
	if (IP_RT(port))
		kmsg = ikm_rtalloc(sizeof *n);
	else
#endif	/* MACH_RT */
		kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped no-senders (0x%08x, %u)\n", port, mscount);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_no_senders_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_no_senders_template;
#if	MACH_RT
	if (IP_RT(port))
		KMSG_MARK_RT(kmsg);
#endif	/* MACH_RT */

	n->not_header.msgh_remote_port = port;
	n->not_count = mscount;

	ipc_kmsg_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_send_once
 *	Purpose:
 *		Send a send-once notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_send_once(
	ipc_port_t	port)
{
	ipc_kmsg_t kmsg;
	mach_send_once_notification_t *n;
#if	DIPC
	boolean_t active;
	TR_DECL("ipc_notify_send_once");
#endif	/* DIPC */

#if	DIPC
/*	tr2("enter:  port 0x%x", port);*/
	/*
	 *	If the port is not active, sending this
	 *	notification causes an infinite loop.
	 *	DIPC gets here because the send
	 *	once right could be on another node.
	 */
	ip_lock(port);
	active = ip_active(port);
	ip_unlock(port);

	if (!active) {
		tr2("...port 0x%x is dead", port);
		ipc_port_release_sonce(port);
		return;
	}
#endif	/* DIPC */

#if	MACH_RT
	if (IP_RT(port))
		kmsg = ikm_rtalloc(sizeof *n);
	else
#endif	/* MACH_RT */
		kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped send-once (0x%08x)\n", port);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_send_once_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_send_once_template;
#if	MACH_RT
	if (IP_RT(port))
		KMSG_MARK_RT(kmsg);
#endif	/* MACH_RT */

        n->not_header.msgh_remote_port = port;

	ipc_kmsg_send_always(kmsg);
}

/*
 *	Routine:	ipc_notify_dead_name
 *	Purpose:
 *		Send a dead-name notification.
 *	Conditions:
 *		Nothing locked.
 *		Consumes a ref/soright for port.
 */

void
ipc_notify_dead_name(
	ipc_port_t		port,
	mach_port_name_t	name)
{
	ipc_kmsg_t kmsg;
	mach_dead_name_notification_t *n;

#if	MACH_RT
	if (IP_RT(port))
		kmsg = ikm_rtalloc(sizeof *n);
	else
#endif	/* MACH_RT */
		kmsg = ikm_alloc(sizeof *n);
	if (kmsg == IKM_NULL) {
		printf("dropped dead-name (0x%08x, 0x%x)\n", port, name);
		ipc_port_release_sonce(port);
		return;
	}

	ikm_init(kmsg, sizeof *n);
	n = (mach_dead_name_notification_t *) &kmsg->ikm_header;
	*n = ipc_notify_dead_name_template;
#if	MACH_RT
	if (IP_RT(port))
		KMSG_MARK_RT(kmsg);
#endif	/* MACH_RT */

	n->not_header.msgh_remote_port = port;
	n->not_port = name;

	ipc_kmsg_send_always(kmsg);
}
