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
 * IONetworkStack.cpp - An IOKit proxy for the BSD network stack.
 *
 * HISTORY
 *
 * IONetworkStack abstracts certain essential network stack services.
 * Those services include attaching/detaching network interfaces, and
 * interface naming. Note that although BSD network stack does not assign
 * interface names, it is the responsibility of this object to manage the
 * interface name space.
 *
 * IONetworkStack is a client of IONetworkInterface. This object uses the
 * standard IOKit matching mechanism to discover and attach to interface
 * objects. Each interface object is expected to have only a single
 * IONetworkStack client. Under IOKit, the stack object initiates the
 * action to attach to an interface. And subsequently, detaches from an
 * interface when signaled to do so.
 *
 * The packet flow bypasses this object for efficiency sake. The interface
 * object interact directly with the 'real' network stack to send and
 * receive packets.
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include "IONetworkStack.h"

extern "C" {
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/bpf.h>
#include <net/if.h>
#include <netinet/if_ether.h>
}

#define super IOService

OSDefineMetaClassAndStructorsWithInit( IONetworkStack,
                                       IOService,
                                       IONetworkStack::initialize())

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG
#endif

// Maintain a linked list of network interfaces.
// Each time a new interface is added, an unit number is assigned,
// and an interface entry is added to the list. When the interface
// goes away, the entry is removed from the list and deallocated.
//
static queue_head_t         netifTable;
static IOLock *             netifTableLock;

typedef struct {
    IONetworkInterface *    netif;
    const char *            name;
    short                   unit;
    queue_chain_t           link;
} netifEntry;

#define BSD_IFNET_ENTRY     ((IONetworkInterface *) 0)
#define NETIF_LOCK          IOTakeLock(netifTableLock)
#define NETIF_UNLOCK        IOUnlock(netifTableLock)

#define LOCK                IOTakeLock(_lock);
#define UNLOCK              IOUnlock(_lock);

// --------------------------------------------------------------------------
//
// Add a new entry to the netifTable for the interface 'netif'.
// Return true if the entry was added successfully, otherwise
// returns false.
//
// The table lock must be held by the caller of this function.
//
static bool addNetworkInterfaceEntry(IONetworkInterface * netif,
                                     const char *         name,
                                     short                unit)
{
    if (!netif || !name || (unit < 0)) return false;

    netifEntry * entry = (netifEntry *) IOMalloc(sizeof(netifEntry));
    if (!entry)
        return false;
    bzero(entry, sizeof(*entry));

    entry->netif = netif;
    entry->unit  = unit;
    entry->name  = name;

    queue_enter(&netifTable, entry, netifEntry *, link);

    return true;
}

// --------------------------------------------------------------------------
//
// Release the table entry occupied by the 'netif' instance.
// Returns true if an entry was found and removed, otherwise
// return false.
//
static bool releaseNetworkInterfaceEntry(IONetworkInterface * netif)
{
    netifEntry * foundEntry = 0;

    if (!netif) return false;

    NETIF_LOCK;
    if (!queue_empty(&netifTable)) {
        netifEntry * entry;
        queue_iterate(&netifTable, entry, netifEntry *, link) {
            if (entry->netif == netif) {
                foundEntry = entry;
                break;
            }
        }
    }
    if (foundEntry) {
        queue_remove(&netifTable, foundEntry, netifEntry *, link);
        IOFree(foundEntry, sizeof(*foundEntry));
    }
    NETIF_UNLOCK;

    return (foundEntry ? true : false);
}

// --------------------------------------------------------------------------
//
// Initialize the table, and preload it with any existing BSD network
// interfaces.
//
static void initNetifTable()
{
    netifTableLock = IOLockAlloc();
    assert(netifTableLock);

    IOLockInitWithState(netifTableLock, kIOLockStateUnlocked);

    queue_init(&netifTable);

    // Insert entries for all existing 'BSD' network interfaces into the 
    // netifTable. Hopefully only lo0 will be found.
    //
#if 1   // FreeBSD 3.2
    for (struct ifnet * ifp = ifnet.tqh_first;
         ifp;
         ifp = ifp->if_link.tqe_next)
        addNetworkInterfaceEntry(BSD_IFNET_ENTRY, ifp->if_name, ifp->if_unit);
#else
    for (struct ifnet * ifp = ifnet; ifp; ifp = ifp->if_next)
        addNetworkInterfaceEntry(BSD_IFNET_ENTRY, ifp->if_name, ifp->if_unit);
#endif
}

// --------------------------------------------------------------------------
//
// Add a entry for the interface object 'netif', with name prefix 'name'.
// The chosen unit number is returned in 'unit'. Returns true on success.
//
static bool reserveNetworkInterfaceEntry(IONetworkInterface * netif,
                                         const char *         name,
                                         short *              unit)
{
    bool ret = true;
    bool addEntry = true;   // add new entry to list.
    bool error = false;

    if (!netif || !name || !unit || (*unit < 0)) return false;

    NETIF_LOCK;

    if (!queue_empty(&netifTable)) {
        bool rescan;
        netifEntry * entry;

        do {
            rescan = false;
            
            // Scan through the interface list and search for duplicates
            // and conflicts. If a conflict is found, increment the unit
            // number and rescan.
            //
            queue_iterate(&netifTable, entry, netifEntry *, link) {
                if (entry->netif == netif) {    // can't have duplicates
                    addEntry = false;           // re-use existing entry
                    *unit = entry->unit;
                    break;
                }
                if ((entry->unit == *unit) && !strcmp(entry->name, name)) {
                    // name conflict detected.
                    if (++(*unit) > 0)
                        rescan = true;
                    else
                        error = true;
                    break;
                }
            }
        } while (rescan);
    }

    if (error)
        ret = false;
    else if (addEntry)
        ret = addNetworkInterfaceEntry(netif, name, *unit);

    NETIF_UNLOCK;

    return ret;
}

// --------------------------------------------------------------------------
//
// IONetworkStack class initializer.

void IONetworkStack::initialize()
{
    initNetifTable();
}

// --------------------------------------------------------------------------
//
// init method.

bool IONetworkStack::init(OSDictionary * properties)
{
    _netif = 0;     // IONetworkInterface instance (provider).
    _ifp   = 0;     // ifnet struct for the interface.
    _lock  = 0;     // big serialization lock.
    _state = kIONetworkStackStateInit;  // default state.

    if (!super::init())
        return false;

    // Allocate a lock which will protect all accesses to this object.
    //
    _lock = IOLockAlloc();
    if (!_lock)
        return false;

    return true;
}

// --------------------------------------------------------------------------
//
// probe. the score forces the probe and start in IONetworkStack to occur
// before the notification from IONetworkInterface is sent. We need to delay
// that until the network stack object has assigned a BSD name to the
// interface.

IOService * IONetworkStack::probe(IOService * provider,
                                  SInt32 *    score)
{
    if (!super::probe(provider, score))
        return 0;

    // We are not picky about our provider, but it must be an
    // IONetworkInterface instance.
    //
    IONetworkInterface * netif = OSDynamicCast(IONetworkInterface, provider);
    if (!netif)
        return 0;

    *score = 32;    // must be larger than notification score.
    
    // Query interface for any requirements before qualifying the probe.
    // For now, there is nothing that needs to be done. Return 'this' to
    // indicate successful probe.

    return this;
}

// --------------------------------------------------------------------------
//
// start method. We have attached to our provider, now start ourselve up.

bool IONetworkStack::start(IOService * provider)
{
    IONetworkInterface * netif = OSDynamicCast(IONetworkInterface, provider);
    bool                 ret    = false;
    bool                 opened = false;

    if (!netif) return false;

    LOCK;

    do {
        // Catch illegal state transitions.
        //
        if (_state != kIONetworkStackStateInit)
            break;

        // Our provider must reveal its ifnet structure.
        //
        _netif = netif;
        _ifp   = _netif->getIfnet();
        if (!_ifp || (_netif != (IONetworkInterface *) _ifp->if_private))
            break;

        // Pass start() to our superclass.
        //
        if (!super::start(provider))
            break;

        // Open our provider.
        //
        if (!_netif->open(this))
            break;
        opened = true;

        // Assign a name for the interface. Must do this before the
        // if_attach() call. Remember to release the OSString object
        // obtained through this call.
        //
        OSString * ifname = _assignInterfaceName(_netif);
        if (!ifname) {
            releaseNetworkInterfaceEntry(_netif);
            break;
        }
        ifname->release();

        // When IONetworkStack gets probed, we assume that BSD is already
        // up and running. So it is safe to call BSD to attach the network 
        // interface.
        //
        // FIXME: This will only work with Ethernet interfaces. Switch to
        // new DLIL API once that is available.
        //
        _netif->lock();

        // Set the if_free() function pointer in the ifnet to point to our
        // detachCallBack() static function.
        //
        // _ifp->if_free  = detachCallBackHandler;
        // _netif->_stack = this;

#if 1   // FreeBSD 3.2
        bpfattach(_ifp, DLT_EN10MB,sizeof(struct ether_header));
#else
        bpfattach(&_ifp->if_bpf, _ifp, DLT_EN10MB,sizeof(struct ether_header));
#endif  

        ether_ifattach(_ifp); 
        
        _netif->unlock();

        // Network stack is now attached to the interface.
        //
        _state = kIONetworkStackStateAttached;

        ret = true;
    }
    while (0);

    if (!ret)
    {
        // start failed, undo any actions performed.
        //
        if (opened)
            _netif->close(this);
        
        _netif = 0;
        _ifp   = 0;
    }

    UNLOCK;

    return ret;
}

// --------------------------------------------------------------------------
//
// stop method.

void IONetworkStack::stop(IOService * provider)
{
    LOCK;

    // Catch an illegal stop.
    //
    assert((_state == kIONetworkStackStateInit) ||
           (_state == kIONetworkStackStateDetached));

    // Remove the interface from linked list.
    //
    if (_netif) {
        assert(_netif == (IONetworkInterface *) provider);
        releaseNetworkInterfaceEntry(_netif);
    }

    _netif = 0;
    _ifp   = 0;
    _state = kIONetworkStackStateInit;

    UNLOCK;
    
    super::stop(provider);
}

// --------------------------------------------------------------------------
//
// sendIfDetachRequest
//
// Handle a detach request from our provider and signal to the network stack 
// that the interface wishes to detach. The network stack will callback when 
// the interface is allowed to proceed with the detach. There is no hard
// limit on how soon the callback will occur.
//
// Returns true if the request was handled. Otherwise, returns false.

bool IONetworkStack::sendIfDetachRequest()
{
    bool ret = false;

    DLOG("IONetworkStack::sendIfDetachRequest() called state = %d\n", _state);

    LOCK;

    if (_state == kIONetworkStackStateAttached) {
    
        // Send a detach request to the network stack and wait for
        // a callback indicating detach operation complete.
        //
        // if_detach() is a new proposed DLIL call and does not exist yet.

        _state = kIONetworkStackStateDetaching;

        ret = true; // detach request handled.
    }
    
    UNLOCK;
    
    return ret;
}

// --------------------------------------------------------------------------
//
// ifDetachCallback method.

void IONetworkStack::ifDetachCallback()
{
    LOCK;
    
    if (_state == kIONetworkStackStateDetaching) {
        
        // We previously sent a detach request, the network stack is now
        // calling back to indicate detach completion.
        //
        assert(_netif && _ifp);

        // Close our provider.
        //
        _netif->close(this);
 
        // Detach is now complete.
        //
        _state = kIONetworkStackStateDetached;
    }
    else {
        DLOG("%s: Unexpected detach callback\n", getName());
    }

    UNLOCK;
}

// --------------------------------------------------------------------------
//
// This static member function is registered as the if_free() handler in
// the ifnet structure of our provider. The IONetworkStack instance is 
// discovered and its ifFreeCallback() method is called.

void IONetworkStack::ifDetachCallbackHandler(struct ifnet * ifp)
{
    IONetworkInterface *    netif;
    IONetworkStack *        stack;
    
    netif = OSDynamicCast(IONetworkInterface,
                          (IONetworkInterface *) ifp->if_private);
    assert(netif);

    netif->lockForArbitration();
    
    stack = (IONetworkStack *) netif->_client;
    if (stack)
        stack->ifDetachCallback();
    else
        DLOG("IONetworkStack: No target for ifDetachCallback\n");

    netif->unlockForArbitration();
}

// --------------------------------------------------------------------------
//
// Release allocated resources.

void IONetworkStack::free()
{
    assert(_state == kIONetworkStackStateInit);

    if (_lock)
        IOLockFree(_lock);
}

// --------------------------------------------------------------------------
//
// Facility provided by IOService for general purpose provider-to-client 
// notification. We catch the kIOMessageServiceIsTerminated message.

IOReturn IONetworkStack::message(UInt32 type, IOService * provider,
                                 void * argument = 0)
{
    // Our provider has gone into an inactive state, we should begin the
    // tear-down process by signalling the network stack to unregister
    // and detach the network interface.
    //
    // We do not look at the argument, which contains the options given
    // to the terminate() method.
    //
    if (type == kIOMessageServiceIsTerminated) {
        sendIfDetachRequest();
        return kIOReturnSuccess;
    }
    
    return kIOReturnUnsupported;
}

// --------------------------------------------------------------------------
//
// Assign a BSD friendly name to the network interface. The interface
// object has to assist in this process by returning its name prefix,
// i.e. "en". This routine will pick an unit number that does not
// conflict with any existing interface.

OSString * IONetworkStack::_assignInterfaceName(IONetworkInterface * netif)
{
    SInt16                  unit = 0;
    UInt32                  index = 0;
    const char *            namePrefix = netif->getNamePrefix();
    char                    nameBuf[40];
    OSString *              nameString;
    IONetworkController *   ctlr = OSDynamicCast(IONetworkController, 
                                                 netif->getProvider());

    if (!ctlr || !namePrefix || !*namePrefix)
        return 0;

    // First, we need to determine the initial interface unit number.
    // The current scheme needs a lot of work. Currently, motherboard
    // devices gets unit 0, while add-on PCI cards are assigned
    // unit 1 and up. Here, we need to determine whether this is an
    // onboard or a PCI network controller.
    //
    if (ctlr->getProvider()) {
            OSObject * propObject = ctlr->getProvider()->getProperty("built-in");
        
        if (!propObject) {
            propObject = ctlr->getProvider()->getProperty("AAPL,slot-name");
            if (propObject) {
                unit = 1;   // PCI add-on card
                
                // If we have an index number, increment
                // the unit by the index number.
                // (for multiport PCI cards).
                ctlr->doGetControllerIndex(this, &index);
                unit += index;
            }       
        }
    }

    // "unit" is now the initial unit number requested by the interface.
    // If this unit number is already taken by an existing interface, we
    // will automatically increment the unit number until there are no 
    // conflicts. We don't want to confuse BSD with identical names.
    //
    // Now try to reserve the chosen name.
    //
    if (!reserveNetworkInterfaceEntry(netif, namePrefix, &unit))
        return 0;

    // Update the interface object with its assigned name.
    //
    sprintf(nameBuf, "%s%d", namePrefix, unit);
    nameString = OSString::withCString(nameBuf);
    if (nameString) {
        //
        // Now fill in the ifnet name fields.
        //      
        netif->setInterfaceNameInt(namePrefix);
        netif->setUnitNumberInt(unit);
        
        // Set the interface's kIOBSDName property.
        //
        netif->setProperty(kIOBSDName, nameString);
    }

    return nameString;
}
