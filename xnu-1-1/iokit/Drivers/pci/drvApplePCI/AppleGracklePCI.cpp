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
 * 23 Nov 98 sdouglas created from objc version.
 */
 
#include <IOKit/system.h>

#include <libkern/c++/OSContainers.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <libkern/OSByteOrder.h>

#include "AppleGracklePCI.h"

#include <IOKit/assert.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOPCIBridge

OSDefineMetaClassAndStructors(AppleGracklePCI, IOPCIBridge)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleGracklePCI::start( IOService * provider )
{
    IOPCIPhysicalAddress 	ioAddrCell;
    IOPhysicalAddress		ioPhys;
    IOPhysicalAddress		ioPhysLen;
    OSArray *			array;
    IODeviceMemory::InitElement	rangeList[ 3 ];
    IORegistryEntry *		bridge;
    OSData *			busProp;
    IOPCIAddressSpace           grackleSpace;
    UInt32                      picr1;
		
    if( 0 == (lock = IOSimpleLockAlloc()))
	return( false );

    ioAddrCell.physHi.bits 	= 0;
    ioAddrCell.physHi.s.space 	= kIOPCIIOSpace;
    ioAddrCell.physMid 		= 0;
    ioAddrCell.physLo 		= 0;
    ioAddrCell.lengthHi 	= 0;
    ioAddrCell.lengthLo 	= 0x10000;

    bridge = provider;

    if( ! IODTResolveAddressCell( bridge, (UInt32 *) &ioAddrCell,
		&ioPhys, &ioPhysLen) ) {

	IOLog("%s: couldn't find my base\n", getName());
	return( false);
    }

    /* define more explicit ranges */

    rangeList[0].start	= ioPhys;
    rangeList[0].length = ioPhysLen;
    rangeList[1].start	= ioPhys + 0x00c00000;
    rangeList[1].length = 4;
    rangeList[2].start	= ioPhys + 0x00e00000;
    rangeList[2].length	= 4;

    array = IODeviceMemory::arrayFromList( rangeList, 3 );
    if( !array)
	return( false);

    provider->setDeviceMemory( array );
    array->release();
    ioMemory = (IODeviceMemory *) array->getObject( 0 );

    if( (configAddrMap = provider->mapDeviceMemoryWithIndex( 1 )))
        configAddr = (volatile UInt32 *) configAddrMap->getVirtualAddress();
    if( (configDataMap = provider->mapDeviceMemoryWithIndex( 2 )))
        configData = (volatile UInt32 *) configDataMap->getVirtualAddress();

    if( !configAddr || !configData)
	return( false);

    busProp = (OSData *) bridge->getProperty("bus-range");
    if( busProp)
	primaryBus = *((UInt32 *) busProp->getBytesNoCopy());

    // Check to see if there is a set loop snoop property.
    if( provider->getProperty("set-loop-snoop")) {
        // Turn on the Loop Snoop bit in PICR1.
        // See: MPC106 User's Manual p. 3-55.
        grackleSpace.bits = 0x80000000;
        picr1 = configRead32(grackleSpace, 0xA8);
        picr1 |= (1 << 4);
        configWrite32(grackleSpace, 0xA8, picr1);
    }

    return( super::start( provider));
}

bool AppleGracklePCI::configure( IOService * provider )
{
    bool ok;

    ok = addBridgeMemoryRange( 0x80000000, 0x7f000000, true );
    ok = addBridgeIORange( 0, 0x10000 );

    return( super::configure( provider ));
}

void AppleGracklePCI::free()
{
    if( configAddrMap)
	configAddrMap->release();
    if( configDataMap)
	configDataMap->release();
    if( lock)
	IOSimpleLockFree( lock);

    super::free();
}

IODeviceMemory * AppleGracklePCI::ioDeviceMemory( void )
{
    return( ioMemory);
}

UInt8 AppleGracklePCI::firstBusNum( void )
{
    return( primaryBus );
}

UInt8 AppleGracklePCI::lastBusNum( void )
{
    return( firstBusNum() );
}

IOPCIAddressSpace AppleGracklePCI::getBridgeSpace( void )
{
    IOPCIAddressSpace	space;

    space.bits = 0;
    space.s.deviceNum = kBridgeSelfDevice;

    return( space );
}

inline void AppleGracklePCI::setConfigSpace( IOPCIAddressSpace space,
					UInt8 offset )
{
    IOPCIAddressSpace	addrCycle;

    addrCycle = space;
    addrCycle.s.reloc = 1;
    addrCycle.s.registerNum = offset;

    OSWriteSwapInt32( configAddr, 0, addrCycle.bits);
    eieio();
    OSReadSwapInt32( configAddr, 0 );
    eieio();
}


UInt32 AppleGracklePCI::configRead32( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt32		data;
    IOInterruptState	ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    setConfigSpace( space, offset );

    data = OSReadSwapInt32( configData, 0 );
    eieio();

    IOSimpleLockUnlockEnableInterrupt( lock, ints );
    return( data );
}

void AppleGracklePCI::configWrite32( IOPCIAddressSpace space, 
					UInt8 offset, UInt32 data )
{
    IOInterruptState ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    setConfigSpace( space, offset );

    OSWriteSwapInt32( configData, 0, data );
    eieio();
    /* read to sync (?) */
    (void) OSReadSwapInt32( configData, 0 );
    eieio();

    IOSimpleLockUnlockEnableInterrupt( lock, ints );
}
