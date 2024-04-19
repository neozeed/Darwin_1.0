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
 * IOEthernetInterface.cpp
 *
 * HISTORY
 * 8-Jan-1999       Joe Liu (jliu) created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IONetworkUserClient.h>

extern "C" {
#include <sys/param.h>
#include <sys/errno.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/sockio.h>
#include <netinet/in_var.h>
#include <sys/malloc.h>
void arpwhohas(struct arpcom * ac, struct in_addr * addr);
}

//---------------------------------------------------------------------------

#define super IONetworkInterface

OSDefineMetaClassAndStructors( IOEthernetInterface, IONetworkInterface )

// The name prefix for all BSD Ethernet interfaces.
// 
#define kIOEthernetInterfaceNamePrefix      "en"

//---------------------------------------------------------------------------
// Macros

#define CTLR_SYNC_REQ(ctlr, action, ret, args...)   \
        ctlr->syncRequest(this, this, (IONetworkAction) action, \
                          (UInt32 *) ret, ## args)

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG
#endif

//---------------------------------------------------------------------------
// Initialize an IOEthernetInterface instance. Instance variables are
// initialized, and an arpcom structure is allocated.

bool IOEthernetInterface::init(OSDictionary * properties = 0)
{
    // Initialize instance variables.
    //
    _arpcom             = 0;     // arpcom structure.
    _mcAddrCount        = 0;     // number of multicast addresses.
    _features           = 0;     // controller's family specific features.
    _activeFilters      = 0;     // active packet filters.
    _availableFilters   = 0;     // available packet filters.
    _controllerEnabled  = false;

    // Allocate an arpcom structure, clear it, then call super::init().
    // We expect our superclass to call getIfnet() during its init()
    // method. So we have to create arpcom before calling super::init().

    if ((_arpcom = (struct arpcom *) IOMalloc(sizeof(*_arpcom))) == 0)
    {
        DLOG("IOEthernetInterface: arpcom allocation failed\n");
        return false;
    }

    bzero(_arpcom, sizeof(*_arpcom));

    // Pass the init() call to our superclass.
    //
    if (!super::init(properties))
        return false;

    // Add an IONetworkData with room to hold an IOEthernetStats structure.
    // This class does not reference the data object created, and no harm
    // is done if the data object is released.

    IONetworkData * data = IONetworkData::withName(kIOEthernetStatsKey,
                                                   sizeof(IOEthernetStats));
    if (data) {
        addNetworkData(data);
        data->release();
    }

    return true;
}

//---------------------------------------------------------------------------
// Initialize the ifnet structure. The argument specified is
// a pointer to an ifnet structure obtained through getIfnet().
// IOEthernetInterface will initialize this structure in a manner that
// is appropriate for Ethernet interfaces.
//
// ifp: Pointer to the ifnet structure to be initialized.
//
// Returns true if successful, false otherwise.

bool IOEthernetInterface::initIfnet(struct ifnet * ifp)
{
    struct arpcom * ac = (struct arpcom *) ifp;
    
    assert(ac);

    lock();    // lock interface state

    bzero(ac, sizeof(*ac));

    // Set defaults suitable for Ethernet interfaces.

    setInterfaceType(IFT_ETHER);
    setMaxTransferUnit(ETHERMTU);
    setMediaAddressLength(NUM_EN_ADDR_BYTES);
    setMediaHeaderLength(ETHERHDRSIZE);
    setFlags(IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS);

    unlock();  // unlock interface state

    return super::initIfnet(ifp);
}

//---------------------------------------------------------------------------
// Free the IOEthernetInterface instance. The memory allocated
// for the arpcom structure is released.

void IOEthernetInterface::free()
{
    if (_arpcom) IOFree(_arpcom, sizeof(*_arpcom));

    super::free();
}

//---------------------------------------------------------------------------
// This method returns a pointer to an ifnet structure
// maintained by the family specific interface. IOEthernetInterface
// allocates an arpcom structure during initialization, and returns
// a pointer to this structure when this method is called.
//
// Returns a pointer to an ifnet structure.

struct ifnet * IOEthernetInterface::getIfnet() const
{
    return (&(_arpcom->ac_if));
}

//---------------------------------------------------------------------------
// The name of the interface advertised to the network layer
// is generated by concatenating the string returned by this method,
// and an unit number.
//
// Returns a pointer to a constant string "en". Thus Ethernet interfaces
// will be registered as en0, en1, etc.

const char * IOEthernetInterface::getNamePrefix() const
{
    return kIOEthernetInterfaceNamePrefix;
}

//---------------------------------------------------------------------------
// Update the packet filter property. This property contains the set of
// packet filters needed by the interface. It may not represent the set
// of packet filters that are actually in use and enabled.
//
// Returns true if the newFilter provided is different from the previously 
// cached value.

bool IOEthernetInterface::_setActiveFilters(UInt32 newFilters)
{
    if (newFilters == _activeFilters)
        return false;

    _activeFilters = newFilters;
    setProperty(kIOPacketFilters, _activeFilters, 32);
    return true;
}

//---------------------------------------------------------------------------
// Prepare the controller after it has been opened.
// This method will be called by our superclass after a
// network controller has accepted an open from this interface.
// IOEthernetInterface uses this method to inspect the controller
// and to cache certain controller properties, such as its hardware
// address, and its set of supported packet filters.
//
// controller: The controller object that was opened.
//
// Returns true if the controller was accepted, false otherwise
// (which will cause the controller to be closed).

bool IOEthernetInterface::controllerDidOpen(IONetworkController * inController)
{
    bool ret = false;
    IOEthernetController * controller = OSDynamicCast(IOEthernetController, 
                                                      inController);

    do {
        IOReturn  r;

        if (!controller)
            break;

        // Call the superclass' controllerDidOpen().
        //
        if (!super::controllerDidOpen(controller))
            break;

        // Cache the controller's family feature set.
        //
        _features = controller->getFamilyFeatureSet();

        // Get the controller's (supported) packet filters.
        //
        r = controller->doGetPacketFilters(this, &_availableFilters);

        if (r != kIOReturnSuccess)
        {
            DLOG("%s: doGetPacketFilters error %x\n", getName(), r);
            break;
        }

        // Controller must support Unicast and Broadcast filtering.
        //
        if ((_availableFilters &
            (kIOPacketFilterUnicast | kIOPacketFilterBroadcast)) !=
            (kIOPacketFilterUnicast | kIOPacketFilterBroadcast))
        {
            DLOG("%s: No Unicast/Broadcast packet filters %lx\n",
                getName(), _availableFilters);
            break;
        }

        // If controller reports multicast or multicast-all capability,
        // then update if_flags to include multicast support.
        //
        if (_availableFilters &
            (kIOPacketFilterMulticast | kIOPacketFilterMulticastAll))
        {
            setFlagsInt(IFF_MULTICAST);
        }

        // Even though the network stack will not explicitly request the
        // interface to enable Unicast and Broadcast filtering. It is
        // implicit. Therefore, we will always activate both of those
        // filter types.

        _setActiveFilters(kIOPacketFilterUnicast | kIOPacketFilterBroadcast);

        r = controller->doEnablePacketFilters(this, _activeFilters);

        if (r != kIOReturnSuccess)
        {
            DLOG("%s: Failed to enable Unicast/Broadcast filters %x\n",
                 getName(), r);
            break;
        }

        // Read and save the controller's MAC address.
        //
        r = ((IOEthernetController *) 
            controller)->doGetHardwareAddress(this, &_macAddr);
        if (r != kIOReturnSuccess)
        {
            DLOG("%s: kRequestGetHardwareAddress error %x\n", getName(), r);
            break;
        }

#if 1   // Print the MAC address
        IOLog("%s: Ethernet address %02x:%02x:%02x:%02x:%02x:%02x\n",
              getName(),
              _macAddr.ether_addr_octet[0],
              _macAddr.ether_addr_octet[1],
              _macAddr.ether_addr_octet[2],
              _macAddr.ether_addr_octet[3],
              _macAddr.ether_addr_octet[4],
              _macAddr.ether_addr_octet[5]);
#endif

        // Copy the hardware address we obtained from the controller
        // to the arpcom structure.
        //
        bcopy(&_macAddr, _arpcom->ac_enaddr, NUM_EN_ADDR_BYTES);

        // Store MAC address in interface's propertyTable.
        //
        setProperty(kIOMACAddress, (void *) &_macAddr, NUM_EN_ADDR_BYTES);

        ret = true;
    }
    while (0);

    return ret;
}

//---------------------------------------------------------------------------
// When the last close from our client is received, the
// interface object will close its controller. But before the controller
// is closed, this method will be called by our superclass to perform any 
// final cleanup. IOEthernetInterface will ensure that the controller
// is disabled before it is closed.
//
// controller: The currently opened controller object.

void IOEthernetInterface::controllerWillClose(IONetworkController * controller)
{
    if (_controllerEnabled)
    {
        // Make sure the controller is disabled when we have lost all
        // our clients, and is about to close the controller. It
        // should be safe to update the _controllerEnabled variable
        // shared with the ioctl handlers, which shouldn't run without
        // an open client.

        controller->doDisable(this);
        _controllerEnabled = false;
    }

    super::controllerWillClose(controller);
}

//---------------------------------------------------------------------------
// The handler for ioctl commands sent from the network layer.
// Commands not handled by this method are passed to our superclass.
//
// Argument convention is:
//
//    arg0 - (struct ifnet *)
//    arg1 - (void *)
//
// The commands handled by IOEthernetInterface are:
//
//    SIOCSIFADDR
//    SIOCSIFFLAGS
//    SIOCADDMULTI
//    SIOCDELMULTI
//
// Returns an error code defined in errno.h (BSD).

SInt IOEthernetInterface::performCommand(IONetworkController * inController,
                                         UInt32                cmd,
                                         void *                arg0,
                                         void *                arg1)
{
    struct arpcom *        ac  = (struct arpcom *) _arpcom;
    struct ifaddr *        ifa = (struct ifaddr *) arg1;
    SInt                   ret = EBUSY;
    IOEthernetController * controller = (IOEthernetController *) inController;

    assert(arg0 == _arpcom);

    if (!controller) return EINVAL;

    switch (cmd)
    {
        case SIOCSIFADDR:
            CTLR_SYNC_REQ(controller, &IOEthernetInterface::syncSIOCSIFADDR, 
                          &ret, controller);
            if (ret)
            {
                IOLog("IOEthernetInterface: SIOCSIFADDR returned %d\n", ret);
                break;
            }

            switch (ifa->ifa_addr->sa_family)
            {
                case AF_INET:
                    //
                    // See if another station has *our* IP address.
                    // i.e.: There is an address conflict! If a
                    // conflict exists, a message is sent to the
                    // console.
                    //
                    if (IA_SIN(ifa)->sin_addr.s_addr != 0)
                    {
                        /* don't bother for 0.0.0.0 */
                        ac->ac_ipaddr = IA_SIN(ifa)->sin_addr;
                        arpwhohas(ac, &IA_SIN(ifa)->sin_addr);
                    }
                    break;

                default:
                    break;
            }
            break;

        case SIOCSIFFLAGS:
            CTLR_SYNC_REQ(controller, &IOEthernetInterface::syncSIOCSIFFLAGS, 
                          &ret, controller);
            break;

        case SIOCADDMULTI:
            CTLR_SYNC_REQ(controller, &IOEthernetInterface::syncSIOCADDMULTI, 
                          &ret, controller);    
            break;

        case SIOCDELMULTI:
            CTLR_SYNC_REQ(controller, &IOEthernetInterface::syncSIOCDELMULTI, 
                          &ret, controller);
            break;  

        default:            
            // Don't know what to do with this ioctl command, forward it
            // to our superclass.
            //
            ret = super::performCommand(controller, cmd, arg0, arg1);
            break;
    }

    return ret;
}

//---------------------------------------------------------------------------
// _enableController() is reponsible for calling the controller's enable()
// method and restoring the state of the controller. We assume that
// controllers can completely reset its state upon receiving a disable()
// method call. And when it is brought back up, the interface should
// assist in restoring the previous state of the Ethernet controller.

IOReturn IOEthernetInterface::_enableController(IONetworkController * ctlr)
{
    IOReturn ret;

    assert(ctlr);

    // Send the controller an enable request.
    //
    ret = ctlr->doEnable(this);
    if (ret != kIOReturnSuccess)
        return ret;     // unable to bring up the controller.

    // Restore current filter settings.
    //
    ret = ctlr->doEnablePacketFilters(this, _activeFilters);
    if (ret != kIOReturnSuccess)
        goto error;

    // Update multicast address list.
    //
    if (_availableFilters & kIOPacketFilterMulticast)
    {
        ret = _loadMulticastList((IOEthernetController *) ctlr);
        if (ret != kIOReturnSuccess)
            goto error;
    }

    _controllerEnabled = true;

    return kIOReturnSuccess;

error:
    // If the controller was enabled, make sure we disable it if an
    // error occurred.
    //
    DLOG("%s: _enableController error %x\n", getName(), ret);

    ret = ctlr->doDisable(this);

    return ret;
}

//---------------------------------------------------------------------------
// Handles SIOCSIFFLAGS ioctl command for Ethernet interfaces. The network
// stack has changed the if_flags field in ifnet. Our job is to go
// through if_flags and see what has changed, and act accordingly.
//
// The fact that if_flags contains both generic and Ethernet specific bits
// means that we cannot move some of the default flag processing to the
// superclass. Sigh...

int IOEthernetInterface::syncSIOCSIFFLAGS(IOEthernetController * ctlr)
{
    SInt      r = 0;
    UInt16    flags = getFlags();
    IOReturn  ret;
    UInt32    newFilters = _activeFilters;

    if ( !(flags & IFF_UP) && (flags & IFF_RUNNING) )
    {
        // If interface is marked down and it is running,
        // then stop it.

        ctlr->doDisable(this);
        flags &= ~IFF_RUNNING;
        _controllerEnabled = false;
    }
    else if ( (flags & IFF_UP) && !(flags & IFF_RUNNING) )
    {
        // If interface is marked up and it is stopped, then
        // start it.

        if ((ret = _enableController(ctlr)) == kIOReturnSuccess)
            flags |= IFF_RUNNING;
        else
            r = errnoFromReturn(ret);
    }

    if (flags & IFF_RUNNING)
    {
        // Set/Clear promiscuous mode.
        //
        if (_availableFilters & kIOPacketFilterPromiscuous)
        {
            if (flags & IFF_PROMISC)
                newFilters |= kIOPacketFilterPromiscuous;
            else
                newFilters &= ~kIOPacketFilterPromiscuous;
        }

        // Set/Clear Multicast-All mode.
        //
        if (_availableFilters & kIOPacketFilterMulticastAll)
        {
            if (flags & IFF_ALLMULTI)
                newFilters |= kIOPacketFilterMulticastAll;
            else
                newFilters &= ~kIOPacketFilterMulticastAll;
        }
    }

    if (_setActiveFilters(newFilters))
    {
        ret = ctlr->doEnablePacketFilters(this, newFilters);    
        if (ret != kIOReturnSuccess)
            r = errnoFromReturn(ret);
    }

    // Update the flags field to pick up any modifications. Also update the
    // property table to reflect any flag changes.
    //
    setFlagsInt(flags, ~flags);

    return r;
}

//---------------------------------------------------------------------------
// Handles SIOCSIFADDR ioctl command for Ethernet interfaces.

SInt IOEthernetInterface::syncSIOCSIFADDR(IOEthernetController * ctlr)
{
    SInt    r = 0;
    UInt16  flags = getFlags();

    flags |= IFF_UP;
    
    if (!(flags & IFF_RUNNING))
    {
        r = errnoFromReturn(_enableController(ctlr));
        if (r == 0)
            flags |= IFF_RUNNING;
    }

    setFlagsInt(flags, ~flags);
    
    return r;
}

//---------------------------------------------------------------------------
// This method is called by syncSIOCADDMULTI() and syncSIOCDELMULTI() to
// reload the hardware's multicast filter whenever the multicast address
// list is changed. A OSData is published in the property table containing
// the multicast addresses.

IOReturn
IOEthernetInterface::_loadMulticastList(IOEthernetController * ctlr)
{
    enet_addr_t *        multiAddrs = 0;
    UInt                 mcount;
    OSData *             mcData = 0;
    struct ifnet *       ifp = (struct ifnet *) _arpcom;
    struct ifmultiaddr * ifma;
    IOReturn             ret;
    bool                 ok;

    assert(ifp);

    // Update the multicast addresses count ivar.
    //
    mcount = 0;
    for (ifma = ifp->if_multiaddrs.lh_first;
         ifma != NULL;
         ifma = ifma->ifma_link.le_next)
    {
        if ((ifma->ifma_addr->sa_family == AF_UNSPEC) ||
            (ifma->ifma_addr->sa_family == AF_LINK))
            mcount++;
    }
    _mcAddrCount = mcount;

    if (mcount)
    {
        char * addrp;
            
        mcData = OSData::withCapacity(mcount * NUM_EN_ADDR_BYTES);
        if (!mcData)
        {
            DLOG("%s: multicast memory allocation failed\n", getName());
            return kIOReturnNoMemory;
        }
        
        // Loop through the linked multicast structures and write the
        // address to the OSData.
        //
        for (ifma = ifp->if_multiaddrs.lh_first;
             ifma != NULL;
             ifma = ifma->ifma_link.le_next)
        {
            if (ifma->ifma_addr->sa_family == AF_UNSPEC) 
                addrp = &ifma->ifma_addr->sa_data[0];
            else
                if (ifma->ifma_addr->sa_family == AF_LINK)
                addrp = LLADDR((struct sockaddr_dl *) ifma->ifma_addr);
                else
                continue;

            ok = mcData->appendBytes((const void *) addrp, NUM_EN_ADDR_BYTES);
            assert(ok);
        }

        multiAddrs = (enet_addr_t *) mcData->getBytesNoCopy();
        assert(multiAddrs);
    }

    // Call the controller's setMulticastList() method.
    //
    ret = ctlr->doSetMulticastList(this, multiAddrs, mcount);

    if (mcData)
    {
        if (ret == kIOReturnSuccess)
            setProperty(kIOMulticastAddresses, mcData);
        mcData->release();
    }
    else {
        removeProperty(kIOMulticastAddresses);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Handles SIOCADDMULTI ioctl command.

SInt IOEthernetInterface::syncSIOCADDMULTI(IOEthernetController * ctlr)
{
    IOReturn  ret;
    UInt32    activatedFilters;
    SInt      r = 0;

    if (_availableFilters & kIOPacketFilterMulticast)
    {
        _setActiveFilters(_activeFilters | kIOPacketFilterMulticast);

        // Make sure the multicast filter is active.
        //
        ret = ctlr->doEnablePacketFilters(this, _activeFilters, 
                                          &activatedFilters);

        // If the controller did not activate the multicast filter,
        // report an error.
        //
        if ((activatedFilters & kIOPacketFilterMulticast) == 0)
        {
            assert(ret != kIOReturnSuccess);
            r = errnoFromReturn(ret);
        }

        // Do not load multicast list if the multicast filter
        // did not become active.
        //
        if (r == 0)
            r = errnoFromReturn(_loadMulticastList(ctlr));
    }
    else
        r = EOPNOTSUPP;   // no multicast support.

    return r;
}

//---------------------------------------------------------------------------
// Handles SIOCDELMULTI ioctl command.

SInt IOEthernetInterface::syncSIOCDELMULTI(IOEthernetController * ctlr)
{
    IOReturn  ret;
    UInt32    activatedFilters;
    SInt      r = 0;

    if (_availableFilters & kIOPacketFilterMulticast)
    {
        r = errnoFromReturn(_loadMulticastList(ctlr));

        if (r == 0)
        {
            // If the multicast list is now empty, instruct the controller
            // to turn off its multicast filter.

            if (_mcAddrCount == 0)
            {
                _setActiveFilters(_activeFilters & ~kIOPacketFilterMulticast);

                ret = ctlr->doEnablePacketFilters(this, _activeFilters,
                                                  &activatedFilters);

                if (activatedFilters & kIOPacketFilterMulticast)
                {
                    assert(ret != kIOReturnSuccess);
                    r = errnoFromReturn(ret);
                }
            }
        }
    }
    else
        r = EOPNOTSUPP;   // no multicast support.

    return r;
}
