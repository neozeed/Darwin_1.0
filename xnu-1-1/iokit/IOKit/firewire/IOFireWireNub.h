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
 *
 *	IOFireWireNub.h
 *
 *
 * 	Note: IOFWCommand(s) are allocated by methods in this class. 
 *            The remaining methods to setup and submit IOFWCommands are defined in
 *            IOFWCommand.h
 */
#ifndef _IOKIT_IOFIREWIRENUB_H
#define _IOKIT_IOFIREWIRENUB_H

#include <IOKit/IOService.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
class IOFireWireController;

class IOFireWireNub : public IOService
{
    OSDeclareDefaultStructors(IOFireWireNub)

/*------------------Useful info about device (also available in the registry)--------*/
public:
    int			fDeviceSpeed;	// Max supported by device
    int			fCommsSpeed;	// Max speed this node can communicate with device
    UInt16		fNodeID;	// Current node ID (could change after bus reset!)
    UInt32		fLocalNodeID;	// ID of the local node (could change after bus reset!)
    UInt32		fGeneration;	// ID Of bus topology that fNodeID is valid for.
    CSRNodeUniqueID	fUniqueID;	// Device's globally unique ID (never changes)
    mach_timespec_t	fAsyncTimeout;	// Guesstimate of how long to wait for response
					// from device when making async requests
					// Different values for quad/block transfers?
					// OS8 FW has 40/100 mSec.

    const UInt32 *	fROM;
    UInt32		fROMSize;

    IOFireWireController *fControl;

// More info to come - device type, CSR ROM etc.

protected:
    // Create an IOUserClient object to handle communication with User task
    virtual IOReturn newUserClient( task_t		owningTask,
                                    void * 		security_id,
                                    UInt32  		type,
                                    IOUserClient **	handler );

/*------------------Methods provided to FireWire device clients-----------------------*/
public:
    // How fast can this system talk to the node?
    virtual IOFWSpeed FWSpeed() const;

    // How fast can this node talk to another node?
    virtual IOFWSpeed FWSpeed(const IOFireWireNub *dst) const;

    // How big (as a power of two) can packets sent to/received from the node be?
    virtual int maxPackLog(bool forSend) const;

    // How big (as a power of two) can packets sent from this node to dst node/received from dst be?
    virtual int maxPackLog(bool forSend, const IOFireWireNub *dst) const;

    /*
     * Create various FireWire commands to send to the device
     */
    virtual IOFWReadCommand 	*createReadCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
 				bool failOnReset=false);
    virtual IOFWReadQuadCommand *createReadQuadCommand(FWAddress devAddress, UInt32 *quads, int numQuads,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
 				bool failOnReset=false);

    virtual IOFWWriteCommand 	*createWriteCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
 				bool failOnReset=false);
    virtual IOFWWriteQuadCommand *createWriteQuadCommand(FWAddress devAddress, UInt32 *quads, int numQuads,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
 				bool failOnReset=false);

    // size is 1 for 32 bit compare, 2 for 64 bit.
    virtual IOFWCompareAndSwapCommand 	*createCompareAndSwapCommand(FWAddress devAddress,
				const UInt32 *cmpVal, const UInt32 *newVal, int size,
				FWDeviceCallback completion=NULL, void *refcon=NULL,
 				bool failOnReset=false);
    /*
     * Create local FireWire address spaces for the device to access
     */
    virtual IOFWPhysicalAddressSpace *createPhysicalAddressSpace(IOMemoryDescriptor *mem);
    virtual IOFWPseudoAddressSpace *createPseudoAddressSpace(FWAddress addr, UInt32 len, 
				FWReadCallback reader, FWWriteCallback writer, void *refcon);

    /*
     * Create commands to activate/deactivate local FireWire address spaces
     */
    virtual IOFWAllocAddressCommand *createAllocAddrCommand(IOFWAddressSpace *space,
				FWBusCallback completion = 0, void *refcon = 0);
    virtual IOFWDeallocAddressCommand *createDeallocAddrCommand(IOFWAddressSpace *space,
				FWBusCallback completion = 0, void *refcon = 0);

    /*
     * Create an iterator for the device ROM
     * Device nub iterates over whle ROM, Unit nub over Unit directory
     */
    virtual IOReturn CSRROMCreateIterator (CSRROMEntryIterator *pCSRROMIterator) = 0;

    /*
     * Standard nub initialization
     */
    virtual bool init(OSDictionary * propTable);

};

#endif /* ! _IOKIT_IOFIREWIRENUB_H */
