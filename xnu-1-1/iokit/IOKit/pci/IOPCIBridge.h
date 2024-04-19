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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_IOPCIBRIDGE_H
#define _IOKIT_IOPCIBRIDGE_H

#include <IOKit/IOService.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/pci/IOAGPDevice.h>


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOPCIBridge : public IOService
{
    friend IOPCIDevice;

    OSDeclareAbstractStructors(IOPCIBridge)

private:
    IORegistryEntry * findMatching( OSIterator * in, IOPCIAddressSpace space );
    void publishNubs( OSIterator * kids, UInt32 index );
    virtual bool isDTNub( IOPCIDevice * nub );
    static void nvLocation( IORegistryEntry * entry,
	UInt8 * busNum, UInt8 * deviceNum, UInt8 * functionNum );

protected:
    IORangeAllocator *	bridgeMemoryRanges;
    IORangeAllocator *	bridgeIORanges;

protected:
    virtual void probeBus( IOService * provider, UInt8 busNum );

    virtual UInt8 firstBusNum( void );
    virtual UInt8 lastBusNum( void );

    virtual void spaceFromProperties( OSDictionary * propTable,
					IOPCIAddressSpace * space );
    virtual OSDictionary * constructProperties( IOPCIAddressSpace space );

    virtual IOPCIDevice * createNub( OSDictionary * from );

    virtual bool initializeNub( IOPCIDevice * nub, OSDictionary * from );

    virtual bool publishNub( IOPCIDevice * nub, UInt32 index );

    virtual bool addBridgeMemoryRange( IOPhysicalAddress start,
				 	IOPhysicalLength length, bool host );

    virtual bool addBridgeIORange( IOByteCount start, IOByteCount length );

    virtual bool constructRange( IOPCIAddressSpace * flags,
                                 IOPhysicalAddress phys, IOPhysicalLength len,
                                 OSArray * array );

    virtual bool matchNubWithPropertyTable( IOService * nub,
					    OSDictionary * propertyTable );

    virtual bool compareNubName( const IOService * nub, OSString * name,
				 OSString ** matched = 0 ) const;

    virtual bool pciMatchNub( IOPCIDevice * nub, OSDictionary * table );

    virtual bool matchKeys( IOPCIDevice * nub, const char * keys,
				UInt32 defaultMask, UInt8 regNum );

    virtual IOReturn getNubResources( IOService * nub );

    virtual IOReturn getNubAddressing( IOPCIDevice * nub );

    virtual IOReturn getDTNubAddressing( IOPCIDevice * nub );

public:

    virtual bool start( IOService * provider );

    virtual bool configure( IOService * provider );

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    virtual IODeviceMemory * ioDeviceMemory( void ) = 0;

    virtual UInt32 configRead32( IOPCIAddressSpace space, UInt8 offset ) = 0;

    virtual void configWrite32( IOPCIAddressSpace space,
					UInt8 offset, UInt32 data ) = 0;

    virtual IOPCIAddressSpace getBridgeSpace( void ) = 0;

    virtual UInt32 findPCICapability( IOPCIAddressSpace space,
					UInt8 capabilityID );

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    virtual IOReturn createAGPSpace( IOPCIAddressSpace master, 
				     IOOptionBits options,
				     IOPhysicalAddress * address, 
				     IOPhysicalLength * length );

    virtual IOReturn destroyAGPSpace( IOPCIAddressSpace master );

    virtual IORangeAllocator * getAGPRangeAllocator( IOPCIAddressSpace master );

    virtual IOOptionBits getAGPStatus( IOPCIAddressSpace master,
				      IOOptionBits options = 0 );

    virtual IOReturn commitAGPMemory( IOPCIAddressSpace master, 
				      IOMemoryDescriptor * memory,
				      IOByteCount agpOffset,
				      IOOptionBits options = 0 );

    virtual IOReturn releaseAGPMemory( IOPCIAddressSpace master, 
				      	IOMemoryDescriptor * memory, 
					IOByteCount agpOffset );

};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOPCI2PCIBridge : public IOPCIBridge
{
    OSDeclareDefaultStructors(IOPCI2PCIBridge)

private:

    IOPCIDevice * bridgeDevice;

protected:
    virtual UInt8 firstBusNum( void );
    virtual UInt8 lastBusNum( void );

public:
    virtual IOService * probe(	IOService * 	provider,
                                SInt32 *	score );

    virtual bool configure( IOService * provider );

    virtual bool publishNub( IOPCIDevice * nub, UInt32 index );

    virtual IODeviceMemory * ioDeviceMemory( void );

    virtual IOPCIAddressSpace getBridgeSpace( void );

    virtual UInt32 configRead32( IOPCIAddressSpace space, UInt8 offset );

    virtual void configWrite32( IOPCIAddressSpace space,
					UInt8 offset, UInt32 data );

};

#endif /* ! _IOKIT_IOPCIBRIDGE_H */

