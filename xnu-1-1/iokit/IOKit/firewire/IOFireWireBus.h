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


#ifndef _IOKIT_IOFIREWIREBUS_H
#define _IOKIT_IOFIREWIREBUS_H

#include <IOKit/IOService.h>
#include <IOKit/firewire/IOFWIsochChannel.h>

class IOFWIsochPort;
struct DCLCommandStruct;

class IOFireWireBus : public IOService
{
    OSDeclareAbstractStructors(IOFireWireBus)

public:

    typedef void (CallUserProc)(void *refcon, void * userProc, void * dclCommand);
    typedef struct _DCLTaskInfo {
        task_t fTask;	// Task DCL addresses are valid in
        vm_address_t fDCLBaseAddr;
        UInt32 fDCLSize;	// In bytes
        vm_address_t fDataBaseAddr;
        UInt32 fDataSize;
        CallUserProc *fCallUser; // Routine to handle DCLCallCommandProcs
        void *fCallRefCon;	// Refcon for user call
    } DCLTaskInfo;

    // Create an Isochronous Channel object
    virtual IOFWIsochChannel *createIsochChannel(
	bool doIRM, UInt32 bandwidth, IOFWSpeed prefSpeed,
	FWIsochChannelForceStopNotificationProc stopProc=NULL,
	void *stopRefCon=NULL) = 0;
 
   // Create a local isochronous port to run the given DCL program
    virtual IOFWIsochPort *createLocalIsochPort(bool talking,
        DCLCommandStruct *opcodes, DCLTaskInfo *info = 0,
	UInt32 startEvent = 0, UInt32 startState = 0, UInt32 startMask = 0) = 0;
};

#endif /* ! _IOKIT_IOFIREWIREBUS_H */

