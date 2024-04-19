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
 * 05 Nov 99 sdouglas added UniNorth AGP based on UniNorthAGPDriver.c *					by Fernando Urbina, Kent Miller.
 *
 */
 
#include <IOKit/system.h>
#include <ppc/proc_reg.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/OSByteOrder.h>

#include <IOKit/IODeviceMemory.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include "AppleMacRiscPCI.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOPCIBridge

OSDefineMetaClassAndStructors(AppleMacRiscPCI, IOPCIBridge)

OSDefineMetaClassAndStructors(AppleMacRiscVCI, AppleMacRiscPCI)

OSDefineMetaClassAndStructors(AppleMacRiscAGP, AppleMacRiscPCI)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleMacRiscPCI::start( IOService * provider )
{
    IOPCIPhysicalAddress 	ioAddrCell;
    IOPhysicalAddress		ioPhys;
    IOPhysicalAddress		ioPhysLen;
    OSArray *			array;
    IODeviceMemory::InitElement	rangeList[ 3 ];
    IORegistryEntry *		bridge;
    OSData *			busProp;
    
    if( !IODTMatchNubWithKeys(provider, "('pci', 'vci')"))
	return( false);

    if( IODTMatchNubWithKeys(provider, "'uni-north'"))
	configDataOffsetMask = 0x7;
    else
	configDataOffsetMask = 0x3;      

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
    rangeList[1].start	= ioPhys + 0x00800000;
    rangeList[1].length = 4;
    rangeList[2].start	= ioPhys + 0x00c00000;
    rangeList[2].length	= 4;

    IORangeAllocator * platformRanges;
    platformRanges = IOService::getPlatform()->getPhysicalRangeAllocator();
    assert( platformRanges );
    platformRanges->allocateRange( ioPhys, 0x01000000 );

    array = IODeviceMemory::arrayFromList( rangeList, 3 );
    if( !array)
	return( false);

    provider->setDeviceMemory( array );
    array->release();
    ioMemory = (IODeviceMemory *) array->getObject( 0 );

    /* map registers */

    if( (configAddrMap = provider->mapDeviceMemoryWithIndex( 1 )))
        configAddr = (volatile UInt32 *) configAddrMap->getVirtualAddress();
    if( (configDataMap = provider->mapDeviceMemoryWithIndex( 2 )))
        configData = (volatile UInt32 *) configDataMap->getVirtualAddress();

    if( !configAddr || !configData)
	return( false);

    busProp = (OSData *) bridge->getProperty("bus-range");
    if( busProp)
	primaryBus = *((UInt32 *) busProp->getBytesNoCopy());

    return( super::start( provider));
}

bool AppleMacRiscPCI::configure( IOService * provider )
{
    UInt32	addressSelects;
    UInt32	index;
    bool	ok;

    addressSelects = configRead32( getBridgeSpace(), kMacRISCAddressSelect );

    coarseAddressMask	= addressSelects >> 16;
    fineAddressMask	= addressSelects & 0xffff;

    for( index = 0; index < 15; index++ ) {
	if( coarseAddressMask & (1 << index)) {
	    ok = addBridgeMemoryRange( index << 28, 0x10000000, true );
	}
    }

//    if( coarseAddressMask & (1 << 15))	// F segment
	for( index = 0; index < 15; index++ ) {
	    if( fineAddressMask & (1 << index)) {
		ok = addBridgeMemoryRange( (0xf0 | index) << 24,
						0x01000000, true );
	    }
	}

    ok = addBridgeIORange( 0, 0x10000 );

    return( super::configure( provider));
}

void AppleMacRiscPCI::free()
{
    if( configAddrMap)
	configAddrMap->release();
    if( configDataMap)
	configDataMap->release();
    if( lock)
	IOSimpleLockFree( lock);

    super::free();
}

IODeviceMemory * AppleMacRiscPCI::ioDeviceMemory( void )
{
    return( ioMemory);
}

IODeviceMemory * AppleMacRiscVCI::ioDeviceMemory( void )
{
    return( 0 );
}

bool AppleMacRiscVCI::configure( IOService * provider )
{
    addBridgeMemoryRange( 0x90000000, 0x10000000, true );

    return( AppleMacRiscPCI::configure( provider));
}

UInt8 AppleMacRiscPCI::firstBusNum( void )
{
    return( primaryBus );
}

UInt8 AppleMacRiscPCI::lastBusNum( void )
{
    return( firstBusNum() );
}

IOPCIAddressSpace AppleMacRiscPCI::getBridgeSpace( void )
{
    IOPCIAddressSpace	space;

    space.bits = 0;
    space.s.busNum = primaryBus;
    space.s.deviceNum = kBridgeSelfDevice;

    return( space );
}

inline bool AppleMacRiscPCI::setConfigSpace( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt32	addrCycle;

    if( space.s.busNum == primaryBus) {

	if( space.s.deviceNum < kBridgeSelfDevice)
	    return( false);

        // primary config cycle
        addrCycle = (	  (1 << space.s.deviceNum)
			| (space.s.functionNum << 8)
			| offset );

    } else {
        // pass thru config cycle
        addrCycle = (	  (space.bits)
			| offset
			| 1 );
    }

    do {
        OSWriteSwapInt32( configAddr, 0, addrCycle);
        eieio();
    } while( addrCycle != OSReadSwapInt32( configAddr, 0 ));
    eieio();

    return( true );
}


UInt32 AppleMacRiscPCI::configRead32( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt32		data;
    IOInterruptState	ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        data = OSReadSwapInt32( configData, offset & configDataOffsetMask );
        eieio();

    } else
	data = 0xffffffff;

    IOSimpleLockUnlockEnableInterrupt( lock, ints );

    return( data );
}

void AppleMacRiscPCI::configWrite32( IOPCIAddressSpace space, 
					UInt8 offset, UInt32 data )
{
    IOInterruptState ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        OSWriteSwapInt32( configData, offset & configDataOffsetMask, data );
        eieio();
	/* read to sync (?) */
        (void) OSReadSwapInt32( configData, offset & configDataOffsetMask );
        eieio();
	sync();
	isync();
    }

    IOSimpleLockUnlockEnableInterrupt( lock, ints );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super AppleMacRiscPCI

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleMacRiscAGP::configure( IOService * provider )
{
    if( !findPCICapability( getBridgeSpace(), kIOPCIAGPCapability))
	return( false );

    return( super::configure( provider));
}

IOPCIDevice * AppleMacRiscAGP::createNub( OSDictionary * from )
{
    IOPCIDevice *	nub;
    IOPCIAddressSpace	space;
    bool		isAGP;

    spaceFromProperties( from, &space);

    isAGP = (  (space.s.deviceNum != getBridgeSpace().s.deviceNum)
	    && findPCICapability( space, kIOPCIAGPCapability));

    if( isAGP)
	nub = new IOAGPDevice;
    else
        nub = super::createNub( from );

    return( nub );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn AppleMacRiscAGP::createAGPSpace( IOPCIAddressSpace master, 
				      IOOptionBits options,
				      IOPhysicalAddress * address, 
				      IOPhysicalLength * length )
{
    IOReturn		err;
    IORangeAllocator *	platformRanges;
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOPhysicalLength	agpLength;
    IOPhysicalAddress	gartPhys;

    enum { agpSpacePerPage = 4 * 1024 * 1024 };
    enum { agpBytesPerGartByte = 1024 };
    enum { alignLen = 4 * 1024 * 1024 - 1 };

    destroyAGPSpace( master );

    agpLength = *length;
    if( !agpLength)
	agpLength = 32 * 1024 * 1024;

    agpLength = (agpLength + alignLen) & ~alignLen;

    err = kIOReturnVMError;
    do {

	gartLength = agpLength / agpBytesPerGartByte;
	gartArray = (volatile UInt32 *) IOMallocContiguous( 
				gartLength, 4096, &gartPhys );
	if( !gartArray)
	    continue;
//IOMapPages( kernel_map, gartArray, gartPhys, gartLength, kIOMapInhibitCache );
        bzero( (void *) gartArray, gartLength);

	platformRanges = getPlatform()->getPhysicalRangeAllocator();

	for( agpBaseIndex = 0xf; agpBaseIndex > 0; agpBaseIndex--) {
	    systemBase = agpBaseIndex * 0x10000000;
	    if( platformRanges->allocateRange( systemBase, agpLength )) {
		systemLength = agpLength;
		break;
	    }
	}
	if( !systemLength)
	    continue;

	agpRange = IORangeAllocator::withRange( agpLength, 4096 );
	if( !agpRange)
	    continue;

        *address = systemBase;
        *length = systemLength;
#if 0
        coarseAddressMask |= (1 << agpBaseIndex);
        configWrite32( target, kMacRISCAddressSelect,
				(coarseAddressMask << 16) | fineAddressMask );
#endif
        configWrite32( target, kUniNAGP_BASE, agpBaseIndex << 28 );

        assert( 0 == (gartPhys & 0xfff));
        configWrite32( target, kUniNGART_BASE,
     			gartPhys | (agpLength / agpSpacePerPage));

        err = kIOReturnSuccess;

    } while( false );

    if( kIOReturnSuccess == err)
        setAGPEnable( master, true, 1 );
    else
	destroyAGPSpace( master );

    return( err );
}

IOReturn AppleMacRiscAGP::destroyAGPSpace( IOPCIAddressSpace master )
{
    IORangeAllocator * platformRanges;

    setAGPEnable( master, false, 0 );

    if( gartArray) {
	IOFreeContiguous( (void *) gartArray, gartLength);
	gartArray = 0;
    }
    if( agpRange) {
	agpRange->release();
	agpRange = 0;
    }
    if( systemLength) {
	platformRanges = getPlatform()->getPhysicalRangeAllocator();
	platformRanges->deallocate( systemBase, systemLength);
	systemLength = 0;
    }

    return( kIOReturnSuccess );
}

IORangeAllocator * AppleMacRiscAGP::getAGPRangeAllocator(
					IOPCIAddressSpace master )
{
//    if( agpRange)	agpRange->retain();
    return( agpRange );
}

IOOptionBits AppleMacRiscAGP::getAGPStatus( IOPCIAddressSpace master,
					    IOOptionBits options = 0 )
{
    return( configRead32( getBridgeSpace(), kUniNINTERNAL_STATUS ) );
}

IOReturn AppleMacRiscAGP::commitAGPMemory( IOPCIAddressSpace master, 
				      IOMemoryDescriptor * memory,
				      IOByteCount agpOffset,
				      IOOptionBits options = 0 )
{
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOReturn		err = kIOReturnSuccess;
    UInt32		offset = 0;
    IOPhysicalAddress	physAddr;
    IOByteCount		len;

//    ok = agpRange->allocate( memory->getLength(), &agpOffset );

    assert( agpOffset < systemLength );
    agpOffset /= (page_size / 4);
    while( (physAddr = memory->getPhysicalSegment( offset, &len ))) {

	offset += len;
	len = (len + 0xfff) & ~0xfff;
	while( len > 0) {
	    OSWriteLittleInt32( gartArray, agpOffset,
				((physAddr & ~0xfff) | 1));
	    agpOffset += 4;
	    physAddr += page_size;
	    len -= page_size;
	}
    }
    flush_dcache( (vm_offset_t) gartArray, gartLength, false);
    sync();
    isync();
#if 1
    configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_INV );
    configWrite32( target, kUniNGART_CTRL, kGART_EN );
    configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_2xRESET);
    configWrite32( target, kUniNGART_CTRL, kGART_EN );
#endif

    return( err );
}

IOReturn AppleMacRiscAGP::releaseAGPMemory( IOPCIAddressSpace master, 
				  		IOMemoryDescriptor * memory, 
						IOByteCount agpOffset )
{
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOReturn		err = kIOReturnSuccess;
    IOByteCount		length;

    if( !memory)
	return( kIOReturnBadArgument );

    length = memory->getLength();

    if( (agpOffset + length) >= systemLength)
	return( kIOReturnBadArgument );

//    agpRange->deallocate( agpOffset, length );

    length = (length + 0xfff) & ~0xfff;
    agpOffset /= page_size;
    while( length > 0) {
	gartArray[ agpOffset++ ] = 0;
	length -= page_size;
    }
    flush_dcache( (vm_offset_t) gartArray, gartLength, false);
    sync();
    isync();

    configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_INV );
    configWrite32( target, kUniNGART_CTRL, kGART_EN );
    configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_2xRESET);
    configWrite32( target, kUniNGART_CTRL, kGART_EN );

    return( err );
}

IOReturn AppleMacRiscAGP::setAGPEnable( IOPCIAddressSpace master,
					bool enable, IOOptionBits options )
{
    IOReturn		err = kIOReturnSuccess;
    IOPCIAddressSpace 	target = getBridgeSpace();
    UInt32		command;
    UInt32		targetStatus, masterStatus;

    if( enable) {

	targetStatus = configRead32( target, kIOPCIConfigAGPTargetStatus );
	masterStatus = configRead32( master, kIOPCIConfigAGPMasterStatus );

	command = kIOAGPSideBandAddresssing
		| kIOAGP4xDataRate | kIOAGP2xDataRate | kIOAGP1xDataRate;
	command &= targetStatus;
	command &= masterStatus;

	if( command & kIOAGP4xDataRate)
	    command &= ~(kIOAGP2xDataRate | kIOAGP1xDataRate);
	else if( command & kIOAGP2xDataRate)
	    command &= ~(kIOAGP1xDataRate);

	command |= kIOAGPEnable;

	if( targetStatus > masterStatus)
	    targetStatus = masterStatus;
	command |= (targetStatus & kIOAGPRequestQueueMask);

#if 1
        configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, kGART_EN );
        configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_2xRESET);
        configWrite32( target, kUniNGART_CTRL, kGART_EN );
#endif
	do {
	    configWrite32( target, kIOPCIConfigAGPTargetCommand, command );
	} while( (command & kIOAGPEnable) != 
	(kIOAGPEnable & configRead32( target, kIOPCIConfigAGPTargetCommand)));

	do {
	    configWrite32( master, kIOPCIConfigAGPMasterCommand, command );
	} while( (command & kIOAGPEnable) != 
	(kIOAGPEnable & configRead32( master, kIOPCIConfigAGPMasterCommand)));

        configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, kGART_EN );
        configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_2xRESET);
        configWrite32( target, kUniNGART_CTRL, kGART_EN );

    } else {

        configWrite32( master, kIOPCIConfigAGPMasterCommand, 0 );
        configWrite32( target, kIOPCIConfigAGPTargetCommand, 0 );
        configWrite32( target, kUniNGART_CTRL, kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, 0 );
        configWrite32( target, kUniNGART_CTRL, kGART_2xRESET);
        configWrite32( target, kUniNGART_CTRL, 0 );
    }

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
