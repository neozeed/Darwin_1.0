/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_IOFWISOCHCHANNEL_H
#define _IOKIT_IOFWISOCHCHANNEL_H

#include <libkern/c++/OSObject.h>
#include <IOKit/firewire/IOFWRegs.h>

enum
{
    kFWIsochChannelUnknownCondition	= 0,
    kFWIsochChannelNotEnoughBandwidth	= 1,
    kFWIsochChannelChannelNotAvailable	= 2
};

class IOFireWireController;
class IOFWIsochChannel;
class IOFWIsochPort;
class OSSet;

typedef IOReturn	(FWIsochChannelForceStopNotificationProc) (
        void *				refCon,
	IOFWIsochChannel *		isochChannelID,
	UInt32				stopCondition);

typedef FWIsochChannelForceStopNotificationProc *FWIsochChannelForceStopNotificationProcPtr;

class IOFWIsochChannel : public OSObject
{
    OSDeclareDefaultStructors(IOFWIsochChannel)

protected:
    IOFireWireController *fControl;
    FWIsochChannelForceStopNotificationProcPtr fStopProc;
    void *		fStopRefCon;
    IOFWIsochPort *	fTalker;
    OSSet *		fListeners;
    bool		fDoIRM;
    UInt32		fBandwidth;	// Allocation units used
    UInt32		fMBitSec;
    IOFWSpeed		fPrefSpeed;
    IOFWSpeed		fSpeed;		// Actual speed used
    UInt32		fChannel;	// Actual channel used

    static void		threadFunc(void * arg, void *);
    virtual IOReturn	updateBandwidth(bool claim);
    virtual void	reallocBandwidth();	
    virtual void	free();

public:
    // Called from IOFireWireController
    virtual bool init(IOFireWireController *control,
		bool doIRM, UInt32 bandwidth, IOFWSpeed prefSpeed,
		FWIsochChannelForceStopNotificationProcPtr stopProc,
		void *stopRefCon);
    virtual void handleBusReset();

    // Called by clients
    virtual IOReturn setTalker(IOFWIsochPort *talker);
    virtual IOReturn addListener(IOFWIsochPort *listener);

    virtual IOReturn allocateChannel();
    virtual IOReturn releaseChannel();
    virtual IOReturn start();
    virtual IOReturn stop();
};

#endif /* ! _IOKIT_IOFWISOCHCHANNEL_H */

