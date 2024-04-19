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

#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/pci/IOAGPDevice.h>
#include <IOKit/IOPlatformExpert.h>

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include <libkern/c++/OSContainers.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClassAndStructors(IOPCIDevice, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// stub driver has two power states, off and on
#define number_of_power_states 2

static IOPMPowerState ourPowerStates[number_of_power_states] = {
{1,IOPMNotAttainable,0,0,0,0,0,0,0,0,0,0},
{1,IOPMNotAttainable,0,IOPMPowerOn,0,0,0,0,0,0,0,0}
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// attach
//
// If the device is an expansion slot device,
// we volunteer to do power managment for the device,
// and all we do is request power stay on.  The effect is
// to prevent system sleep.  If a driver is loaded which can
// power manage the device, it will contact us and we
// will relinquish this control.  This prevents the system
// from sleeping when there are non-power-managed
// PCI cards installed.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
bool IOPCIDevice::attach( IOService * provider )
{
    if ( getProperty("AAPL,slot-name") != NULL ) {
        PMinit();								// initialize superclass variables
        pm_vars->thePlatform->PMRegisterDevice((IOService *)this,(IOService *)this);	// register as policy-maker
        registerControllingDriver(this,ourPowerStates,number_of_power_states);	// register as controlling driver
        changeStateToPriv( number_of_power_states-1);				// clamp power on
    }
    return super::attach(provider);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// If the power domain is supplying power, the device
// can be on.  If there is no power it can only be off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long  IOPCIDevice::maxCapabilityForDomainState(
                                        IOPMPowerFlags domainState )
{
   if( domainState &  IOPMPowerOn )
       return number_of_power_states-1;
   else
       return 0;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// This is our first information about the power domain state.
// If power is on in the new state, the device is on.
// If domain power is off, the device is also off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long IOPCIDevice::initialPowerStateForDomainState(
                                         IOPMPowerFlags domainState )
{
   if( domainState &  IOPMPowerOn )
       return number_of_power_states-1;
   else
       return 0;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// powerStateForDomainState
//
// The power domain may be changing state.
// If power is on in the new state, the device will be on.
// If domain power is off, the device will be off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long  IOPCIDevice::powerStateForDomainState(
                                        IOPMPowerFlags domainState )
{
   if( domainState &  IOPMPowerOn )
       return pm_vars->myCurrentState;
   else
       return 0;
}


//*********************************************************************************
// joinPMtree
//
// A policy-maker for our PCI device calls here when initializing,
// to be attached into the power management hierarchy.
// We are currently the (stub) policy-maker for the device, so we
// attach this driver and then pull ourselves out of the picture.
//
// This overrides the default function of the IOService joinPMtree.
//*********************************************************************************
void IOPCIDevice::joinPMtree ( IOService * driver )
{
    if ( getProperty("AAPL,slot-name") != NULL ) {
        pm_vars->myParent->addChild(driver);	// attach new driver to our parent
        changeStateToPriv(0);			// release our clamp
        pm_vars->myParent->removeChild(this);	// detach ourselves
        PMstop();				// free structures, etc.
    }
    else {
        super::joinPMtree(driver);		// oops, they shouldn't have called here
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIDevice::matchPropertyTable( OSDictionary * table )
{
  return( parent->matchNubWithPropertyTable( this, table ));
}

bool IOPCIDevice::compareName( OSString * name, OSString ** matched = 0 ) const
{
    return( parent->compareNubName( this, name, matched ));
}

IOReturn IOPCIDevice::getResources( void )
{
    return( parent->getNubResources( this ));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UInt32 IOPCIDevice::configRead32( IOPCIAddressSpace _space,
					UInt8 offset )
{
    return( parent->configRead32( _space, offset & 0xfc ));
}

void IOPCIDevice::configWrite32( IOPCIAddressSpace _space, 
					UInt8 offset, UInt32 data )
{
    parent->configWrite32( _space, offset & 0xfc, data );
}

UInt32 IOPCIDevice::configRead32( UInt8 offset )
{
    return( parent->configRead32( space, offset & 0xfc ));
}

void IOPCIDevice::configWrite32( UInt8 offset, UInt32 data )
{
    parent->configWrite32( space, offset & 0xfc, data );
}

UInt32 IOPCIDevice::findPCICapability( UInt8 capabilityID )
{
    return( parent->configRead32( space, capabilityID ));
}

UInt32 IOPCIDevice::setConfigBits( UInt8 reg, UInt32 mask, UInt32 value )
{
    UInt32	was;
    UInt32	bits;

    bits = configRead32( reg );
    was = (bits & mask);
    bits &= ~mask;
    bits |= (value & mask);
    configWrite32( reg, bits );

    return( was );
}

bool IOPCIDevice::setBusMasterEnable( bool enable )
{
    return( 0 != setConfigBits( kIOPCIConfigCommand, kIOPCICommandBusMaster,
				enable ? kIOPCICommandBusMaster : 0));
}

bool IOPCIDevice::setMemoryEnable( bool enable )
{
    return( 0 != setConfigBits( kIOPCIConfigCommand, kIOPCICommandMemorySpace,
				enable ? kIOPCICommandMemorySpace : 0));
}

bool IOPCIDevice::setIOEnable( bool enable, bool /* exclusive = false */ )
{
    // exclusive is TODO.
    return( 0 != setConfigBits( kIOPCIConfigCommand, kIOPCICommandIOSpace,
				enable ? kIOPCICommandIOSpace : 0));
}

UInt8 IOPCIDevice::getBusNumber( void )
{
    return( space.s.busNum );
}

UInt8 IOPCIDevice::getDeviceNumber( void )
{
    return( space.s.deviceNum );
}

UInt8 IOPCIDevice::getFunctionNumber( void )
{
    return( space.s.functionNum );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IODeviceMemory * IOPCIDevice::getDeviceMemoryWithRegister( UInt8 reg )
{
    OSArray *		array;
    IODeviceMemory *	range;
    unsigned int	i = 0;

    array = (OSArray *) getProperty( gIODeviceMemoryKey);
    if( 0 == array)
	return( 0);

    while( (range = (IODeviceMemory *) array->getObject( i++ ))) {
	if( reg == (range->getTag() & 0xff))
	    break;
    }

    return( range);
}

IOMemoryMap * IOPCIDevice:: mapDeviceMemoryWithRegister( UInt8 reg,
						IOOptionBits options = 0 )
{
    IODeviceMemory *	range;
    IOMemoryMap *	map;

    range = getDeviceMemoryWithRegister( reg );
    if( range)
	map = range->map( options );
    else
	map = 0;

    return( map );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IODeviceMemory * IOPCIDevice::ioDeviceMemory( void )
{
    return( parent->ioDeviceMemory() );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService * IOPCIDevice::matchLocation( IOService * /* client */ )
{
      return( this );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOPCIDevice

OSDefineMetaClassAndStructors(IOAGPDevice, IOPCIDevice)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOAGPDevice::createAGPSpace( IOOptionBits options,
				IOPhysicalAddress * address, 
				IOPhysicalLength * length )
{
    return( parent->createAGPSpace( space, options, address, length ));
}

IOReturn IOAGPDevice::destroyAGPSpace( void )
{
    return( parent->destroyAGPSpace( space ));
}

IORangeAllocator * IOAGPDevice::getAGPRangeAllocator( void )
{
    return( parent->getAGPRangeAllocator( space ));
}

IOOptionBits IOAGPDevice::getAGPStatus( IOOptionBits options = 0 )
{
    return( parent->getAGPStatus( space, options ));
}

IOReturn IOAGPDevice::commitAGPMemory(  IOMemoryDescriptor * memory,
					IOByteCount agpOffset,
					IOOptionBits options = 0 )
{
    return( parent->commitAGPMemory ( space, memory, agpOffset, options ));
}

IOReturn IOAGPDevice::releaseAGPMemory( IOMemoryDescriptor * memory,
					IOByteCount agpOffset )
{
    return( parent->releaseAGPMemory ( space, memory, agpOffset ));
}

