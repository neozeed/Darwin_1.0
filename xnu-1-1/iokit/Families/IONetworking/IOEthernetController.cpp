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
 * IOEthernetController.cpp
 *
 * Abstract Ethernet controller superclass.
 *
 * HISTORY
 *
 * Dec 3, 1998  jliu - C++ conversion.
 */

#include <IOKit/assert.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>

//---------------------------------------------------------------------------

#define super IONetworkController

OSDefineMetaClass( IOEthernetController, IONetworkController )

OSDefineAbstractStructors( IOEthernetController, IONetworkController )

//-------------------------------------------------------------------------
// Macros

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG
#endif

// Define SYNC_REQ macro to simplify syncRequest() calls.
//
#define SYNC_REQ(sender, action, ret, args...)  \
        syncRequest(sender, this, (IONetworkAction) action, (UInt32 *) ret, \
                    ## args)

#define DO_SYNC_REQ(action, args...)                                 \
{                                                                    \
    IOReturn  ret = kIOReturnNotReady;                               \
    syncRequest(client, this,                                        \
                (RequestAction) &IOEthernetController:: ## action,   \
                (UInt32 *) &ret, ## args);                           \
    return ret;                                                      \
}

//---------------------------------------------------------------------------
// Initialize an IOEthernetController instance.

bool IOEthernetController::init(OSDictionary * properties)
{
    if (!super::init(properties))
    {
        DLOG("IOEthernetController: super::init() failed\n");
        return false;
    }

    _mcMode = kIOEnetMulticastModeOff;
    _prMode = kIOEnetPromiscuousModeOff;

    return true;
}

//---------------------------------------------------------------------------
// Frees the IOEthernetController instance.

void IOEthernetController::free()
{
    super::free();
}

//---------------------------------------------------------------------------
// Publish Ethernet specific properties to the property
// table. For instance, getHardwareAddress() is called to fetch the
// hardware address. This method is called by the superclass and
// should never be called by drivers.
//
// Returns true if all capabilities were discovered and published
// successfully, false otherwise. Returning false will prevent client
// objects from attaching to the Ethernet controller since a property
// that a client depends on may be missing

bool IOEthernetController::publishCapabilities()
{
    bool        ret = false;
    enet_addr_t addr;

    if (!super::publishCapabilities())
        return false;

    // Publish the controller's MAC address. The Ethernet interface
    // object will also publish this attribute, but the idea is that
    // the controller will always publish the initial address read
    // from the hardware, and it will remain static. While the
    // address recorded by the interface may change if for instance
    // a setHardwareAddress() is accepted by the controller.

    if (getHardwareAddress(&addr) == kIOReturnSuccess)  
        ret = setProperty(kIOMACAddress, (void *) &addr, NUM_EN_ADDR_BYTES);

    return ret;
}

//---------------------------------------------------------------------------
// Called when a client wishes to change the unicast address used
// by the  controller's hardware filter. Implementation of this method is 
// optional.
//
// addr: Pointer to an enet_addr_t containing the new Ethernet address.
//
// Returns kIOReturnUnsupported. Drivers that implement this method
// must return an appropriate return code.

IOReturn IOEthernetController::setHardwareAddress(const enet_addr_t * /*addr*/)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Called by enablePacketFilters() when the controller's multicast filter
// mode needs to be changed. See IOEnetMulticastMode for the list of defined 
// modes.
//
// mode: The new multicast filter mode.
//
// Returns kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
// indicate success, or an error otherwise.

IOReturn IOEthernetController::setMulticastMode(IOEnetMulticastMode /*mode*/)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Called by an interface client when its multicast group membership changes.
// Drivers that support multicasting should override this method and update
// the hardware filter using the address list provided.
//
// Perfect multicast filtering is preferred if supported by the hardware,
// in order to reduce the number of unwanted packets that are received.
// If the number of multicast addresses in the list exceed the limit
// set by the hardware, or if perfect filtering is not supported,
// then ideally the hardware should be programmed to perform imperfect 
// filtering, perhaps through a hash table built by the driver.
//
// This method may be called only if getPacketFilters()
// reports that kIOPacketFilterMulticast is supported.
//
// addrs: Pointer to a list of multicast addresses. Not valid if
//        the count argument is 0.
// count: The number of multicast addresses in the list. May be zero
//        if the list is empty.
//
// Returns kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
// indicate success, or an error otherwise.

IOReturn IOEthernetController::setMulticastList(enet_addr_t * /*addrs*/,
                                                UInt          /*count*/)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Called by enablePacketFilters() when the controller's promiscuous filter
// mode needs to be changed. See IOEnetPromiscuousMode for the list of defined
// modes.
//
// mode: The new promiscuous filter mode.
//
// Returns kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
// indicate success, or an error otherwise.

IOReturn IOEthernetController::setPromiscuousMode(IOEnetPromiscuousMode /*m*/)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Report Ethernet family specific feature set. 

UInt32 IOEthernetController::getFamilyFeatureSet() const
{   
    return 0;
}

//---------------------------------------------------------------------------
// Allocate and return a new IOEthernetInterface instance. The controller
// class from each network family must implement this method inherited from
// IONetworkController to return the corresponding interface object when 
// attachInterface() is called. Subclasses of IOEthernetController
// (Ethernet drivers) have little reason to override this implementation.
//
// Returns the allocated and initialized IOEthernetInterface instance.

IONetworkInterface * IOEthernetController::createInterface()
{
    IOEthernetInterface * enetIf = new IOEthernetInterface;

    if (enetIf && !enetIf->init())
    {
        enetIf->release();
        enetIf = 0;
    }
    return enetIf;
}

//---------------------------------------------------------------------------
// Returns all the packet filters supported by the Ethernet controller.
// See IONetworkController for the list of packet filters.
// This method will perform a bitwise OR of:
//
//    kIOPacketFilterUnicast
//    kIOPacketFilterBroadcast
//    kIOPacketFilterMulticast
//    kIOPacketFilterPromiscuous
//
// and write the result to the integer pointed by 'filtersP'. Drivers that
// support a different set of filters should override this method.
//
// Returns kIOReturnSuccess. Drivers that override this method must return
// kIOReturnSuccess to indicate success, or an error otherwise.

IOReturn IOEthernetController::getPacketFilters(UInt32 * filters)
{
    *filters = (kIOPacketFilterUnicast   |
                kIOPacketFilterBroadcast |
                kIOPacketFilterMulticast |
                kIOPacketFilterPromiscuous);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Called by an interface object to change the set of packet filters that
// are enabled by the controller. The implementation in IOEthernetController
// will trap any changes to multicast or promiscuous filters and translate
// the changes into a filter mode for setMulticastMode() and 
// setPromiscuousMode() methods. This is done strictly as a convenience for
// drivers, which may choose to override this method to handle the filter
// changes directly. Unicast and Broadcast filters are assumed to be always
// enabled, no driver intervention is required.
//
// filters: Each bit set indicates a particular filter that should be 
//          enabled. Filter bits that are not turned on must be disabled.
// activeFiltersP: Updated by this method to reflect the real set
//                 of active filters. Ideally, it should be the same as
//                 the set specified by the first argument.
//
// Returns kIOReturnSuccess for success, otherwise an error code is returned.

IOReturn IOEthernetController::enablePacketFilters(UInt32   filters,
                                                   UInt32 * activeFiltersP)
{
    IOEnetMulticastMode    mcMode;
    IOEnetPromiscuousMode  prMode;
    IOReturn               ret = kIOReturnSuccess;
    IOReturn               err;

#define ACTIVATE_FILTERS(x)     (*activeFiltersP |= (filters & (x)))

    *activeFiltersP = 0;

    // Assume Unicast and Broadcast filters are always active.
    //
    if (filters & (kIOPacketFilterUnicast | kIOPacketFilterBroadcast))
        ACTIVATE_FILTERS(kIOPacketFilterUnicast | kIOPacketFilterBroadcast);

    // Update multicast mode. Avoid redundant mode transitions by
    // caching the last successful multicast mode.
    //
    if (filters & kIOPacketFilterMulticastAll)
        mcMode = kIOEnetMulticastModeAll;
    else if (filters & kIOPacketFilterMulticast)
        mcMode = kIOEnetMulticastModeFilter;
    else
        mcMode = kIOEnetMulticastModeOff;

    if (mcMode != _mcMode)
    {
        err = setMulticastMode(mcMode);
        if (err == kIOReturnSuccess)
        {
            _mcMode = mcMode;
            ACTIVATE_FILTERS(kIOPacketFilterMulticastAll | 
                             kIOPacketFilterMulticast);
        }
        else {
            _mcMode = kIOEnetMulticastModeOff;
            ret = err;
        }
    }
    else {
        ACTIVATE_FILTERS(kIOPacketFilterMulticastAll |
                         kIOPacketFilterMulticast);
    }

    // Update promiscuous mode. Avoid redundant mode transitions by
    // caching the last successful promiscuous mode.
    //
    if (filters & kIOPacketFilterPromiscuousAll)
        prMode = kIOEnetPromiscuousModeAll;
    else if (filters & kIOPacketFilterPromiscuous)
        prMode = kIOEnetPromiscuousModeOn;
    else
        prMode = kIOEnetPromiscuousModeOff;

    if (prMode != _prMode)
    {
        err = setPromiscuousMode(prMode);
        if (err == kIOReturnSuccess)
        {
            _prMode = prMode;
            ACTIVATE_FILTERS(kIOPacketFilterPromiscuousAll |
                             kIOPacketFilterPromiscuous);
        }
        else {
            _prMode = kIOEnetPromiscuousModeOff;
            ret = err;
        }
    }
    else {
        ACTIVATE_FILTERS(kIOPacketFilterPromiscuousAll |
                         kIOPacketFilterPromiscuous);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Call getHardwareAddress() through syncRequest(). See getHardwareAddress().

IOReturn
IOEthernetController::doGetHardwareAddress(IOService *   client,
                                           enet_addr_t * addr)
{
    DO_SYNC_REQ(getHardwareAddress, (void *) addr)
}

//---------------------------------------------------------------------------
// Call setHardwareAddress() through syncRequest(). See setHardwareAddress().

IOReturn
IOEthernetController::doSetHardwareAddress(IOService *         client,
                                           const enet_addr_t * addr)
{
    DO_SYNC_REQ(setHardwareAddress, (void *) addr)
}

//---------------------------------------------------------------------------
// Call setMulticastList() through syncRequest(). See setMulticastList().

IOReturn
IOEthernetController::doSetMulticastList(IOService *   client,
                                         enet_addr_t * addrs,
                                         UInt          count)
{
    DO_SYNC_REQ(setMulticastList, (void *) addrs, (void *) count)
}
