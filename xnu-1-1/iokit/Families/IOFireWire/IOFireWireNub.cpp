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
 * 21 May 99 wgulland created.
 *
 */

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf
#include <IOKit/assert.h>

#include <IOKit/IOMessage.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOFireWireController.h>
#include "IOFireWireUserClient.h"

#define super IOService

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOFireWireNub, IOService )
OSDefineAbstractStructors(IOFireWireNub, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFireWireNub::init(OSDictionary * propTable)
{
    OSNumber *offset;
    if( !super::init(propTable))
        return (false);

    offset = OSDynamicCast(OSNumber, propTable->getObject("GUID"));
    if(offset)
        fUniqueID = offset->unsigned64BitValue();

    return true;
}

IOFWSpeed IOFireWireNub::FWSpeed() const
{
    return fControl->FWSpeed(fNodeID);
}

// How fast can this node talk to another node?
IOFWSpeed IOFireWireNub::FWSpeed(const IOFireWireNub *dst) const
{
    return fControl->FWSpeed(fNodeID, dst->fNodeID);
}

// How big (as a power of two) can packets sent to the node be?
int IOFireWireNub::maxPackLog(bool forSend) const
{
    return fControl->maxPackLog(forSend, fNodeID);
}

// How big (as a power of two) can packets sent between nodes be?
int IOFireWireNub::maxPackLog(bool forSend, const IOFireWireNub *dst) const
{
    if(forSend)
        return fControl->maxPackLog(fNodeID, dst->fNodeID);
    else
        return fControl->maxPackLog(dst->fNodeID, fNodeID);
}

IOFWReadCommand *IOFireWireNub::createReadCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWReadCommand * cmd;
    cmd = new IOFWReadCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress, hostMem, completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWReadQuadCommand *IOFireWireNub::createReadQuadCommand(FWAddress devAddress, UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWReadQuadCommand * cmd;
    cmd = new IOFWReadQuadCommand;
    if(cmd) {
        if(!cmd->initAll(fControl, this, devAddress, quads, numQuads, 
		completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWWriteCommand *IOFireWireNub::createWriteCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWWriteCommand * cmd;
    cmd = new IOFWWriteCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress, hostMem, completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWWriteQuadCommand *IOFireWireNub::createWriteQuadCommand(FWAddress devAddress, 
				UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWWriteQuadCommand * cmd;
    cmd = new IOFWWriteQuadCommand;
    if(cmd) {
        if(!cmd->initAll(fControl, this, devAddress, quads, numQuads,
		completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWCompareAndSwapCommand *
IOFireWireNub::createCompareAndSwapCommand(FWAddress devAddress, const UInt32 *cmpVal, const UInt32 *newVal,
		int size, FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    IOFWCompareAndSwapCommand * cmd;
    cmd = new IOFWCompareAndSwapCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress, cmpVal, newVal, size, completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

    /*
     * Create local FireWire address spaces for the device to access
     */
IOFWPhysicalAddressSpace *IOFireWireNub::createPhysicalAddressSpace(IOMemoryDescriptor *mem)
{
    IOFWPhysicalAddressSpace *space;
    space = new IOFWPhysicalAddressSpace;
    if(!space)
        return NULL;
    if(!space->initWithDesc(mem)) {
        space->release();
        space = NULL;
    }
    return space;
}

IOFWPseudoAddressSpace *IOFireWireNub::createPseudoAddressSpace(FWAddress addr, UInt32 len, 
				FWReadCallback reader, FWWriteCallback writer, void *refcon)
{
    IOFWPseudoAddressSpace *space;
    space = new IOFWPseudoAddressSpace;
    if(!space)
        return NULL;
    if(!space->initAll(addr, len, reader, writer, refcon)) {
        space->release();
        space = NULL;
    }
    return space;
}

    /*
     * Create commands to activate/deactivate local FireWire address spaces
     */
IOFWAllocAddressCommand *IOFireWireNub::createAllocAddrCommand(IOFWAddressSpace *space,
				FWBusCallback completion, void *refcon)
{
    IOFWAllocAddressCommand * cmd;
    cmd = new IOFWAllocAddressCommand;
    if(cmd) {
        if(!cmd->initWithSpace(this, space, completion, refcon)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWDeallocAddressCommand *IOFireWireNub::createDeallocAddrCommand(IOFWAddressSpace *space,
				FWBusCallback completion, void *refcon)
{
    IOFWDeallocAddressCommand * cmd;
    cmd = new IOFWDeallocAddressCommand;
    if(cmd) {
        if(!cmd->initWithSpace(this, space, completion, refcon)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}


/*
 * Create an iterator for the device ROM
 */
IOReturn IOFireWireNub::CSRROMCreateIterator (CSRROMEntryIterator *pCSRROMIterator)
{
    CSRROMEntryIteratorRecPtr	pIteratorRec;
    UInt32			hdrSize;

    // Allocate memory for iterator record.
    pIteratorRec = (CSRROMEntryIteratorRecPtr)IOMalloc (sizeof (CSRROMEntryIteratorRec));
    if (pIteratorRec == NULL)
        return kIOReturnNoMemory;

    // Fill in other record fields.
    // Set relationship to descendants.
    pIteratorRec->relationship = kIterateDescendants;
    // Find start of root directory.
    hdrSize = ((fROM[0] & kCSRBusInfoBlockLength) >> kCSRBusInfoBlockLengthPhase) + 1;
    pIteratorRec->data = fROM;
    pIteratorRec->logicalPath[0] = 0;
    pIteratorRec->physicalPath[0] = hdrSize;

    pIteratorRec->logicalPath[1] = 0;
    pIteratorRec->physicalPath[1] = pIteratorRec->physicalPath[0] + 1;

    pIteratorRec->pathSize = 2;
    pIteratorRec->reset = true;

    // Return iterator.
    *pCSRROMIterator = (CSRROMEntryIterator) pIteratorRec;

    return (kIOReturnSuccess);
}


/**
 ** IOUserClient methods
 **/

IOReturn IOFireWireNub::newUserClient(task_t		owningTask,
                                      void * 		/* security_id */,
                                      UInt32  		type,
                                      IOUserClient **	handler )

{
    IOReturn			err = kIOReturnSuccess;
    IOFireWireUserClient *	client;

    if( type != 11)
        return( kIOReturnBadArgument);

    client = IOFireWireUserClient::withTask(owningTask);

    if( !client || (false == client->attach( this )) ||
        (false == client->start( this )) ) {
        if(client) {
            client->detach( this );
            client->release();
        }
        err = kIOReturnNoMemory;
    }

    *handler = client;
    return( err );
}


