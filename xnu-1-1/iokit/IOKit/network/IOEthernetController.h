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
 * IOEthernetController.h
 *
 * HISTORY
 *
 * Dec 3, 1998  jliu - C++ conversion.
 */

#ifndef _IOETHERNETCONTROLLER_H
#define _IOETHERNETCONTROLLER_H

#include <IOKit/network/IONetworkController.h>

extern "C" {
#include <sys/socket.h>
#include <net/if.h>
#include <net/etherdefs.h>
}

// Property keys.
//
#define kIOMACAddress           "IOMACAddress"

/*! @enum IOEnetPromiscuousMode
    @discussion A promiscuous mode setting passed to setPromiscuousMode() 
    method.
    @constant kIOEnetPromiscuousModeOff Turn off promiscuous mode.
    @constant kIOEnetPromiscuousModeOn  Receive all good frames.
              All good frames on the wire are to be received and submitted
              to the attached interface.
    @constant kIOEnetPromiscuousModeAll Receive all frames, including bad
              frames. More work is needed to communicate the error bits
              to the upper layers. */

enum IOEnetPromiscuousMode {
     kIOEnetPromiscuousModeOff,
     kIOEnetPromiscuousModeOn,
     kIOEnetPromiscuousModeAll
};

/*! @enum IOEnetMulticastMode
    @discussion A multicast mode setting passed to setMulticastMode() 
    method.
    @constant kIOEnetMulticastModeOff Turn off multicast filter.
    @constant kIOEnetMulticastModeFilter Enable multicast filter.
              All frames with a destination address that matches an
              entry in the controller's multicast address list
              should be received.
    @constant kIOEnetMulticastModeAll Receive all multicast frames. All
              frames with a multicast destination address should be
              received. */

enum IOEnetMulticastMode {
     kIOEnetMulticastModeOff,
     kIOEnetMulticastModeFilter,
     kIOEnetMulticastModeAll
};

/*! @class IOEthernetController
    @abstract An abstract superclass for Ethernet controllers.  
    Ethernet drivers should subclass IOEthernetController, and implement
    or override the hardware specific methods to create a working driver.
    An interface object (normally an IOEthernetInterface instance) must
    be instantiated to connect the driver to the network layer. */
    
class IOEthernetController : public IONetworkController
{
    OSDeclareAbstractStructors(IOEthernetController)

private:
    IOEnetMulticastMode     _mcMode;  /* current effective MC mode */
    IOEnetPromiscuousMode   _prMode;  /* current effective PR mode */

public:

/*! @function init
    @abstract Initialize an IOEthernetController instance.
    @param properties A dictionary containing a property table.
    @result true if initialized successfully, false otherwise. */

    virtual bool init(OSDictionary * properties);

/*! @function getFamilyFeatureSet
    @discussion Get the Ethernet features supported by the controller.
    Ethernet controller drivers may override this method and return a mask
    containing all supported feature bits.
    @result IOEthernetController will always return 0. */

    virtual UInt32 getFamilyFeatureSet() const;

/*! @function doGetHardwareAddress
    @discussion Call getHardwareAddress() through syncRequest().
    See getHardwareAddress(). */

    virtual IOReturn doGetHardwareAddress(IOService *   client,
                                          enet_addr_t * addr);

/*! @function doSetHardwareAddress
    @discussion Call setHardwareAddress() through syncRequest().
    See setHardwareAddress(). */

    virtual IOReturn doSetHardwareAddress(IOService *         client,
                                          const enet_addr_t * addr);

/*! @function doSetMulticastList
    @discussion Call setMulticastList() through syncRequest().
    See setMulticastList(). */

    virtual IOReturn doSetMulticastList(IOService *   client,
                                        enet_addr_t * addrs,
                                        UInt          count);

protected:

/*! @function createInterface
    @abstract Create an IONetworkInterface object.
    @discussion Allocate and return a new IOEthernetInterface instance.
    The controller class from each network family must implement this
    method inherited from IONetworkController to return the corresponding
    interface object when attachInterface() is called.
    Subclasses of IOEthernetController (Ethernet drivers) have
    little reason to override this implementation.
    @result The allocated and initialized IOEthernetInterface instance. */

    virtual IONetworkInterface * createInterface();

/*! @function free
    @abstract Frees the IOEthernetController instance. */

    virtual void free();

/*! @function getHardwareAddress
    @abstract Get the controller's hardware address (MAC address).
    @discussion. Ethernet drivers must implement this method,
    by reading the address from hardware and writing it to the buffer provided.
    @param addrP Pointer to an enet_addr_t where the hardware address should be
    written to.
    @result kIOReturnSuccess on success, or an error otherwise. */

    virtual IOReturn getHardwareAddress(enet_addr_t * addrP) = 0;

/*! @function setHardwareAddress
    @abstract Change the station's unicast address.
    @discussion Called when a client wishes to change the unicast
    address used by the controller's hardware filter.
    Implementation of this method is optional.
    @param addrP Pointer to an enet_addr_t containing the new Ethernet address.
    @result kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
    indicate success, or an error otherwise. */

    virtual IOReturn setHardwareAddress(const enet_addr_t * addrP);

/*! @function setMulticastMode
    @abstract Select a new setting for the controller's multicast filter.
    @discussion Called by enablePacketFilters() when the controller's
    multicast filter mode needs to be changed. See IOEnetMulticastMode
    for the list of defined modes.
    @param mode The new multicast filter mode.
    @result kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
    indicate success, or an error otherwise. */   

    virtual IOReturn setMulticastMode(IOEnetMulticastMode mode);

/*! @function setMulticastList
    @abstract Set the list of multicast addresses to be allowed through by
    the multicast filter.
    @discussion Called by an interface client when its multicast group
    membership changes. Drivers that support multicasting should override
    this method and update the hardware filter using the address list
    provided.
    Perfect multicast filtering is preferred if supported by the hardware,
    in order to reduce the number of unwanted packets that are received.
    If the number of multicast addresses in the list exceed the limit
    set by the hardware, or if perfect filtering is not supported,
    then ideally the hardware should be programmed to perform imperfect 
    filtering, perhaps through a hash table built by the driver.
    This method will be called only if getPacketFilters() reports that 
    kIOPacketFilterMulticast is supported.
    @param addrs Pointer to a list of multicast addresses. Not valid if
           the count argument is 0.
    @param count The number of multicast addresses in the list. May be
           zero if the list is empty.
    @result kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
    indicate success, or an error otherwise. */

    virtual IOReturn setMulticastList(enet_addr_t * addrs, UInt count);

/*! @function setPromiscuousMode
    @abstract Select a new promiscuous mode.
    @discussion Called by enablePacketFilters() when the controller's
    promiscuous filter mode needs to be changed. See IOEnetPromiscuousMode
    for the list of defined modes.
    @param mode The new promiscuous filter mode.
    @result kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
    indicate success, or an error otherwise. */

    virtual IOReturn setPromiscuousMode(IOEnetPromiscuousMode mode);

/*! @function publishCapabilities
    @abstract Publish Ethernet controller's properties and capabilities.
    @discussion Publish Ethernet specific properties to the property
    table. For instance, getHardwareAddress() is called to fetch the
    hardware address. This method is called by the superclass and
    should never be called by drivers.
    @result true if all capabilities were discovered and published
    successfully, false otherwise. Returning false will prevent client
    objects from attaching to the Ethernet controller since a property
    that a client depends on may be missing. */

    virtual bool publishCapabilities();

/*! @function getPacketFilters
    @abstract Get the set of packet filters supported by the Ethernet 
    controller.
    @discussion Returns all the packet filters supported by the Ethernet
    controller. See IONetworkController for the list of packet filters.
    This method will perform a bitwise OR of kIOPacketFilterUnicast, 
    kIOPacketFilterBroadcast, kIOPacketFilterMulticast,
    kIOPacketFilterPromiscuous, and write the result to the integer
    pointed by 'filtersP'. Drivers that support a different set of filters
    should override this method.
    @param filtersP Pointer to an integer that shall be updated by this
    method to contain the set of supported filters.
    @result kIOReturnSuccess. Drivers that override this
    method must return kIOReturnSuccess to indicate success, or an error 
    otherwise. */

    virtual IOReturn getPacketFilters(UInt32 * filtersP);

/*! @function enablePacketFilters
    @abstract Enable a set of packet filters supported by the Ethernet 
    controller.
    @discussion Called by an interface object to change the set of
    packet filters that are enabled by the controller.
    The implementation in IOEthernetController will trap
    any changes to multicast or promiscuous filters and translate
    the changes into a filter mode for setMulticastMode() and
    setPromiscuousMode() methods. This is done strictly as a
    convenience for drivers, which may choose to override this method
    to handle the filter changes directly.
    Unicast and Broadcast filters are assumed to be always enabled,
    no driver intervention is required.
    @param filters Each bit that is set indicates a particular filter that
           should be enabled. Filter bits that cleared should be disabled.
    @param activeFiltersP Must be updated by this method to contain the filter
           set that was activated. Ideally, it should be the same as the filter 
           set specified by the first argument.
    @result kIOReturnSuccess on success, otherwise an error code is
    returned. If (*activeFiltersP != filters), then an error is expected. */

    virtual IOReturn enablePacketFilters(UInt32   filters,
                                         UInt32 * activeFiltersP);
};

#endif /* !_IOETHERNETCONTROLLER_H */
