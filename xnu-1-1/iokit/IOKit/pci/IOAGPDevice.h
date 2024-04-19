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


#ifndef _IOKIT_IOAGPDEVICE_H
#define _IOKIT_IOAGPDEVICE_H

#include <IOKit/IORangeAllocator.h>
#include <IOKit/pci/IOPCIDevice.h>

/* Definitions of AGP Master config registers */
enum {
    kIOPCIConfigAGPMasterStatus		= 0x54,
    kIOPCIConfigAGPMasterCommand	= 0x58
};

/* Definitions of AGP Target config registers */
enum {
    kIOPCIConfigAGPTargetStatus		= 0x84,
    kIOPCIConfigAGPTargetCommand	= 0x88
};

/* Definitions of AGP Command & Status registers */
enum {
    kIOAGPRequestQueueMask	= 0xff000000,
    kIOAGPSideBandAddresssing	= 0x00000200,
    kIOAGPEnable		= 0x00000100,
    kIOAGP4GbAddressing		= 0x00000020,
    kIOAGP4xDataRate		= 0x00000004,
    kIOAGP2xDataRate		= 0x00000002,
    kIOAGP1xDataRate		= 0x00000001
};

enum {
    kIOAGPDefaultStatus		= 0
};
enum {
    kIOAGPIdle			= 0x00000001,
    kIOAGPInvalidGARTEntry	= 0x00000002,
    kIOAGPAccessOutOfRange	= 0x00000004
};

/*! @class IOAGPDevice : public IOPCIDevice
    @abstract An IOService class representing an AGP master device.
    @discussion The discovery of an AGP master device by the PCI bus family results in an instance of the IOAGPDevice being created and published. It provides services specific to AGP, in addition to the PCI services supplied by its superclass IOPCIDevice. */

class IOAGPDevice : public IOPCIDevice
{
    OSDeclareDefaultStructors(IOAGPDevice)

private:

public:

/*! @function createAGPSpace
    @abstract Allocates the AGP space, and enables AGP transactions on the master and slave.
    @discussion This method should be called by the driver for the AGP master device  to set the size of the space and enable AGP transactions. It will destroy any AGP space currently allocated.
    @param options No options are currently defined, pass zero.
    @param address The physical range allocated for the AGP space is passed back to the caller.
    @param length An in/out parameter - the caller sets the devices maximum AGP addressing and the actual size created is passed back.
    @result An IOReturn code indicating success or failure. */

    virtual IOReturn createAGPSpace( IOOptionBits options,
				    IOPhysicalAddress * address, 
				    IOPhysicalLength * length );

/*! @function destroyAGPSpace
    @abstract Destroys the AGP space, and disables AGP transactions on the master and slave.
    @discussion This method should be called by the driver to shutdown AGP transactions and release resources. */

    virtual IOReturn destroyAGPSpace( void );

/*! @function getAGPRangeAllocator
    @abstract Accessor to obtain the AGP range allocator.
    @discussion To allocate ranges in AGP space, obtain a range allocator for the space with this method. It is retained while the space is created (until destroyAGPSpace is called) and should not be released by the caller.
    @result A pointer to the range allocator for the AGP space. */

    virtual IORangeAllocator * getAGPRangeAllocator( void );

/*! @function getAGPStatus
    @abstract Returns the current state of the AGP bus.
    @discussion Returns state bits for the AGP bus. Only one type of status is currently defined. 
    @param which Type of status - only kIOAGPDefaultStatus is currently valid.
    @result Mask of status bits for the AGP bus. */

    virtual IOOptionBits getAGPStatus( IOOptionBits which = kIOAGPDefaultStatus );

/*! @function commitAGPMemory
    @abstract Makes memory addressable by AGP transactions.
    @discussion Makes the memory described by the IOMemoryDescriptor object addressable by AGP by entering its pages into the GART array, given an offset into AGP space supplied by the caller (usually allocated by the AGP range allocator). It  is the callers responsibility to prepare non-kernel pageable memory before calling this method, with IOMemoryDescriptor::prepare.
    @param memory A IOMemoryDescriptor object describing the memory to add to the GART.
    @param agpOffset An offset into AGP space that the caller has allocated - usually allocated by the AGP range allocator.
    @param options None currently defined - pass zero.
    @result An IOReturn code indicating success or failure. */

    virtual IOReturn commitAGPMemory( IOMemoryDescriptor * memory,
					IOByteCount agpOffset,
					IOOptionBits options = 0 );

/*! @function releaseAGPMemory
    @abstract Releases memory addressable by AGP transactions.
    @discussion Makes the memory described by the IOMemoryDescriptor object unaddressable by AGP by removing its pages from the GART array, given an offset into AGP space supplied by the caller (usually allocated by the AGP range allocator). It  is the callers responsibility to complete non-kernel pageable memory before calling this method, with IOMemoryDescriptor::complete.
    @param memory A IOMemoryDescriptor object describing the memory to remove from the GART.
    @param agpOffset An offset into AGP space that the caller has allocated - usually allocated by the AGP range allocator.
    @result An IOReturn code indicating success or failure. */

    virtual IOReturn releaseAGPMemory( IOMemoryDescriptor * memory, 
					IOByteCount agpOffset );

};

#endif /* ! _IOKIT_IOAGPDEVICE_H */
