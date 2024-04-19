/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 * Nov 1999 - Christophe Allie - created.
 * 
 */

#define INET 1
//#define PPP_FILTER 0
#define NPPP 1
#define ENOIOCTL EINVAL



extern int	pppsoft_net_wakeup;
extern int	pppnetisr;

#define	setpppsoftnet()	(wakeup((caddr_t)&pppsoft_net_wakeup))
#define	schedpppnetisr()	{ pppnetisr = 1; setpppsoftnet(); }

