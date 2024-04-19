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
 * IONetworkInterface.cpp
 *
 * HISTORY
 * 8-Jan-1999       Joe Liu (jliu) created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkUserClient.h>

extern "C" {
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/bpf.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <net/if_media.h>
#include <net/dlil.h>
int copyout(void *kaddr, void *udaddr, size_t len);
}

//---------------------------------------------------------------------------

#define super IOService

OSDefineMetaClass( IONetworkInterface, IOService )
OSDefineAbstractStructors( IONetworkInterface, IOService )

//---------------------------------------------------------------------------
// Macros

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG
#endif

//---------------------------------------------------------------------------
// Initialize an IONetworkInterface instance.
//
// properties: A property dictionary.
//
// Returns true if initialized successfully, false otherwise.

bool IONetworkInterface::init(OSDictionary * properties = 0)
{
    _provider         = 0;      // single IONetworkController provider.
    _ifLock           = 0;      // locks interface state and data accesses.
    _clientSet        = 0;      // A set of all client objects.
    _inputFilterFunc  = 0;      // input filter tap handler.
    _outputFilterFunc = 0;      // output filter tap handler.
    _target           = 0;      // output target object.
    _outAction        = 0;      // output packet action.
    _dataDict         = 0;      // IONetworkData dict. 
    _registered       = false;

    inputQHead = inputQTail = 0;

    // Propagate the init() call to our superclass.
    //
    if (!super::init(properties))
        return false;

    // Create interface lock.
    //
    _ifLock = IORecursiveLockAlloc();
    if (!_ifLock)
        return false;

    // Create an OSSet to store client objects. Initial capacity
    // (which can grow) is set at 2 clients.
    //
    _clientSet = OSSet::withCapacity(2);
    if (!_clientSet)
        return false;

    // Get the ifnet structure of the network interface. Subclasses must
    // implement getIfnet() and expect this function to be called when
    // they call IONetworkInterface::init().
    //
    _ifp = getIfnet();
    if (!_ifp)
    {
        DLOG("%s: getIfnet() returned NULL\n", getName());
        return false;
    }

    // Intialize the ifnet structure.
    //
    if (!initIfnet(_ifp))
        return false;

    // Create a data dictionary.
    //
    if ((_dataDict = OSDictionary::withCapacity(5)) == 0)
        return false;

    IONetworkData * data = IONetworkData::withName(
                                    kIONetworkStatsKey,
                                    sizeof(IONetworkStats),
                                    (UInt8 *) &(_ifp->if_data.ifi_ipackets));
    if (data) {
        addNetworkData(data);
        data->release();
    }

    // Set the default filter tap mode for both directions to
    // kIOFilterTapModeInternal. Both taps will automatically receive all
    // packets that flow through the interface in their respective
    // directions.
    //
    _inputFilterTapMode = _outputFilterTapMode = kIOFilterTapModeInternal;

    return true;
}

//---------------------------------------------------------------------------
// Destroy the interface. Release all allocated resources.

void IONetworkInterface::free()
{
    DLOG("IONetworkInterface::free() called\n");

    // Must never free a registered interface.

    assert(_registered == false);

    if (_clientSet)
    {
        // Should not have any clients.
        //
        assert(_clientSet->getCount() == 0);
        _clientSet->release();
    }

    if (_ifLock)
    {
        IORecursiveLockFree(_ifLock);
        _ifLock = 0;
    }

    if (_dataDict)
        _dataDict->release();

    clearInputQueue();

    super::free();
}

//---------------------------------------------------------------------------
// Initialize the ifnet structure. Subclasses must override this
// method and initialize the ifnet structure given in a family specific
// manner. The implementation of this method in the subclass must call
// the version in super before it returns. The argument provided is a
// pointer to an ifnet structure obtained through getIfnet().
// IONetworkInterface uses this method to initialize the function
// pointers in ifnet.
//
// ifp: Pointer to an ifnet structure obtained through an earlier
//      getIfnet() call.
//
// Returns true if the initialization was successful.
    
bool IONetworkInterface::initIfnet(struct ifnet * ifp)
{
    lock();

    // Register our 'shim' functions. These function pointers
    // points to static member functions inside class.

    ifp->if_output   = output_shim;
    ifp->if_ioctl    = ioctl_shim;
    ifp->if_set_bpf_tap = set_bpf_tap_shim;
    ifp->if_private  = this;

    // Enable driver reentrancy. This allows the network stack
    // to call the driver from the same thread that sent a received
    // packet up to the stack. The netisr thread is bypassed. The context
    // switch cost is eliminated, but how long will the driver have to wait
    // before the thread can return to do driver work?
    //
    // Not enabled by default, do this on a per driver basis by calling,
    // netif->setExtraFlags(IFEF_DVR_REENTRY_OK) from the driver's
    // configureNetworkInterface() function.
    //
    // ifp->if_eflags |= IFEF_DVR_REENTRY_OK;

    unlock();

    return true;
}

//---------------------------------------------------------------------------
// Take the interface lock (recursive lock).

void IONetworkInterface::lock()
{
    IORecursiveLockLock(_ifLock);
}

//---------------------------------------------------------------------------
// Release the interface lock (recursive lock).

void IONetworkInterface::unlock()
{
    IORecursiveLockUnlock(_ifLock);
}

//---------------------------------------------------------------------------
// Called by handleOpen() to configure our controller after we have just
// called he controller's open() method. We open the controller only when
// we receive the initial open request from a client. Subclasses should
// override this method and setup the controller before allowing the
// client open, and they must call this method and check the return
// value in their implementation.
//
// controller: Our controller object.
//
// Must return true in order for handleOpen() to accept the client open.
// If the return is false, then the controller will be closed and the client
// open will be rejected.

bool IONetworkInterface::controllerDidOpen(IONetworkController * controller)
{
    return true;   // open approved.
}

//---------------------------------------------------------------------------
// Called by handleClose() after receiving a close from the
// last client, and just before the controller is closed. Subclasses
// can override this method to perform any cleanup action before the 
// controller is closed.
//
// controller: The controller that will be closed.

void IONetworkInterface::controllerWillClose(IONetworkController * controller)
{
}

//---------------------------------------------------------------------------
// Handle a client open on the interface. The open() method in IOService
// calls this method with the arbitration lock held. This method must
// return true to accept the client open. Subclasses should not override
// this method.
//
// client:   See IOService.
// options:  See IOService.
// argument: See IOService.
//
// Returns true to accept the client open.

bool IONetworkInterface::handleOpen(IOService *  client,
                                    IOOptionBits options,
                                    void *       argument)
{
    bool                  accept         = false;
    bool                  controllerOpen = false;
    IONetworkController * controller     = OSDynamicCast(IONetworkController, 
                                           getProvider());

    do {
        // Was this object already registered as our client?
        //
        if (_clientSet->containsObject(client))
        {
            DLOG("%s: multiple opens from client %lx\n",
                getName(), (UInt32) client);
            accept = true;
            break;
        }

        // We can only provide services to a single client.
        // IONetworkUserClient are excluded from this restriction.
        //
        bool isClient = (OSDynamicCast(IONetworkUserClient, client) == 0);
        if (isClient && _client)
        {
            DLOG("%s: extra client %lx\n", getName(), (UInt32) client);
            break;
        }

        // If our provider has not yet been opened, then open it.
        // However if the controller open fails, then we will not allow
        // the new client to open us.
        //
        if (_provider == 0) {
            if ((controller == 0) ||
                ((controllerOpen = controller->open(this)) == false) ||
                (controllerDidOpen(controller) == false))
                break;
        }

        // Qualify the client.
        //
        if (!handleClientOpen(controller, client))
            break;

        // Add the new client object to our client set.
        //
        if (!_clientSet->setObject(client))
        {
            handleClientClose(controller, client);
            break;
        }

        // Remember our client.
        //
        if (isClient)
            _client = client;

        accept = true;
    }
    while (0);

    // If provider was opened above, but an error has caused us to refuse
    // the client open, then close our provider. Otherwise, cache our
    // provider.
    //
    if (controllerOpen)
    {
        if (accept)
        {
            _provider = controller; // cache our provider.
        }
        else {
            controllerWillClose(controller);
            controller->close(this);
        }
    }

    return accept;
}

//---------------------------------------------------------------------------
// Handle a close by one of our clients. We close the controller when
// we receive a close from our last client. The close() method in
// IOService calls this method with the arbitration lock held.
// Subclasses should not override this method.
//
// client:   See IOService.
// options:  See IOService.

void IONetworkInterface::handleClose(IOService * client, IOOptionBits options)
{
    // Remove the object from the client OSSet.
    //
    if (_clientSet->containsObject(client))
    {
        bool isClient = (OSDynamicCast(IONetworkUserClient, client) == 0);

        // Call handleClientClose() to handle the client close.
        //
        assert(_provider);
        handleClientClose(_provider, client);

        if (isClient)
            _client = 0;

        // If this is the last client, then close our provider.
        //
        if (_clientSet->getCount() == 1)
        {
            controllerWillClose(_provider);
            _provider->close(this);
            _provider = 0;
        }

        // Remove the client from our OSSet.
        //
        _clientSet->removeObject(client);
    }
}

//---------------------------------------------------------------------------
// Returns true if the specified client, or any client if none if
// specified, presently has an open on this object. This function
// is called by IOService with the arbitration lock held.
// Subclasses should not override this method.

bool IONetworkInterface::handleIsOpen(const IOService * client) const
{
    if (client)
        return _clientSet->containsObject(client);
    else
        return (_clientSet->getCount() > 0);
}

//---------------------------------------------------------------------------
// This method is called by handleOpen() to qualify a client
// object is trying to open us. This method must return true
// true to accept the client open. The provider of the
// interface object, an IONetworkController, is also provided.

bool IONetworkInterface::handleClientOpen(IONetworkController * /*ctlr*/,
                                          IOService *           client)
{
    bool allowOpen = true;

    // Always allow opens from IOUserClients.
    //
    if (OSDynamicCast(IONetworkUserClient, client))
        return true;

    lock();

    // Packet output handler must be valid.
    //
    if (!_target || !_outAction)
        allowOpen = false;

    // Transition state to registered.
    //
    if (allowOpen)
        _registered = true;

    unlock();

    return allowOpen;
}

//---------------------------------------------------------------------------
// Called by handleClose() to handle a client close. Both the provider
// and the client that performed the close are provided.

void IONetworkInterface::handleClientClose(IONetworkController * /*ctlr*/,
                                           IOService *           client)
{
    if (OSDynamicCast(IONetworkUserClient, client))
        return;

    lock();

    // Transition state to unregistered.
    //
    _registered = false;

    unlock();
}

//---------------------------------------------------------------------------
// Register the output handler. The interface will forward all output packets,
// sent from the network layer, to the output handler registered through
// this method. Until a handler is registered, handleClientOpen()
// will refuse all client opens. The output handler cannot be changed
// when the interface state is kIONetworkInterfaceStateRegistered. 
//
// target: Target object that implements the output action.
// action: The action which handles output packets.

bool IONetworkInterface::registerOutputHandler(OSObject *      target,
                                               IOOutputAction  action)
{
    target = OSDynamicCast(OSObject, target);

    lock();

    // Sanity check on the arguments.
    //
    if (_registered || !target || !action)
    {
        unlock();
        return false;
    }

    _target    = target;
    _outAction = action;

    unlock();

    return true;
}

//---------------------------------------------------------------------------
// Functions to set and inspect the filter tap settings. These settings
// cannot change once the interface becomes registered.

bool IONetworkInterface::_setFilterTapMode(IOFilterTapMode * modePtr,
                                           IOFilterTapMode   mode)
{
    lock();

    if (_registered)
    {
        unlock();
        return false;
    }
    *modePtr = mode;

    unlock();
    
    return true;
}

bool IONetworkInterface::setInputFilterTapMode(IOFilterTapMode mode)
{
    return _setFilterTapMode(&_inputFilterTapMode, mode);
}

bool IONetworkInterface::setOutputFilterTapMode(IOFilterTapMode mode)
{
    return _setFilterTapMode(&_outputFilterTapMode, mode);
}

IOFilterTapMode IONetworkInterface::getInputFilterTapMode() const
{
    return _inputFilterTapMode;
}

IOFilterTapMode IONetworkInterface::getOutputFilterTapMode() const
{
    return _outputFilterTapMode;
}

//---------------------------------------------------------------------------
// Feed packets to the input/output BPF packet filter taps.

static inline void _feedFilterTap(struct ifnet * ifp,
                                  struct mbuf *  m,
                                  BPF_FUNC       func,
                                  int            mode)
{
    if (func)
        func(ifp, m);
}

//---------------------------------------------------------------------------
// Feed a packet to the output filter tap. This method should not be called if
// the output filter tap mode is set to kIOFilterTapModeInternal, since the tap
// would already receive every output packet that flows through the interface.
// A further call to feedOutputFilterTap() would cause the tap to receive
// multiple copies of the same output packet.
//
// pkt: The packet mbuf to pass to the output tap.

void IONetworkInterface::feedOutputFilterTap(struct mbuf * m)
{
    assert(m);
    _feedFilterTap(_ifp, m, _outputFilterFunc, BPF_TAP_OUTPUT);
}

//---------------------------------------------------------------------------
// Feed a packet to the input filter tap. This method should not be called if
// the input filter tap mode is set to kIOFilterTapModeInternal, since the tap 
// would already receive every input packet that flows through the interface.
// A further call to feedInputFilterTap() would cause the tap to receive 
// multiple copies of the same input packet
//
// pkt: The packet mbuf to pass to the input tap. The rcvif
//      field in the mbuf is modified by this method.

void IONetworkInterface::feedInputFilterTap(struct mbuf * m)
{
    assert(m);
    m->m_pkthdr.rcvif = _ifp;
    _feedFilterTap(_ifp, m, _inputFilterFunc, BPF_TAP_INPUT);
}

//---------------------------------------------------------------------------
// Called by a controller to pass a received packet
// to the network layer. Packets received by this method can
// also be placed on a queue local to the interface, that the controller
// can use to delay the packet handoff to the network layer, until all
// received packets have been transferred to the queue. A subsequent call
// of flushInputQueue(), or inputPacket() with the queue argument set to 
// false, will cause all queued packets (may be a single packet) to be 
// delivered to the network layer, by making a single dlil_input() call.
// Additional methods that manipulate the input queue are flushInputQueue() 
// and clearInputQueue(). This queue, which is nothing more than a chain of
// mbufs, is not protected by a lock since the controller is expected to
// manipulate the input queue from a single thread. If the input filter
// tap mode is set to kIOFilterTapModeInternal, then packets sent to
// this method are also fed to the input filter tap.
//
// m:      The packet mbuf containing the received frame.
//
// length: If non zero, the mbuf will be truncated to the
//         given length. If zero, then no truncation will take place.
//
// queue:  If true, the only action performed is to queue the
//         input packet. Otherwise, the dlil_input() function is
//         called to handoff all queued packets (including the packet
//         passed in).
//
// Returns the number of packets submitted to the network layer.
// Returns 0 if the packet was queued.
//
// FIXME - The current implementation does not use the new DLIL API's.
// Thus it will only work for Ethernet devices.

#define IN_Q_RESET  {inputQHead = 0;}

#define IN_Q_ENQUEUE(m)                 \
{                                       \
    if (inputQHead == 0)                \
        inputQHead = inputQTail = (m);  \
    else {                              \
        inputQTail->m_nextpkt = (m);    \
        inputQTail = (m);               \
    }                                   \
}

#define IN_Q_FLUSH(count)                                               \
{                                                                       \
    struct  mbuf * _m;                                                  \
    struct  ether_header * _eh;                                         \
                                                                        \
    count = 0;                                                          \
                                                                        \
    while ((_m = inputQHead)) {                                         \
        inputQHead = inputQHead->m_nextpkt;                             \
        _m->m_nextpkt = 0;                                              \
        _m->m_pkthdr.rcvif = _ifp;                                      \
                                                                        \
        if (_inputFilterTapMode == kIOFilterTapModeInternal)            \
            _feedFilterTap(_ifp, _m, _inputFilterFunc, BPF_TAP_INPUT);  \
                                                                        \
        _eh = (struct ether_header *) _m->m_data;                       \
        _m->m_len -= sizeof(struct ether_header);                       \
        _m->m_data += sizeof(struct ether_header);                      \
        _m->m_pkthdr.len -= sizeof(struct ether_header);                \
                                                                        \
        dlil_input(_ifp, _m, (char *) _eh);                                      \
        count++;                                                        \
    }                                                                   \
}

UInt32 IONetworkInterface::flushInputQueue()
{
    UInt32 count;

    IN_Q_FLUSH(count);
//  IN_Q_RESET;
    return count;
}

UInt32 IONetworkInterface::clearInputQueue()
{
    UInt32 count = 0;
    struct mbuf * m;
    
    while ((m = inputQHead))
    {
        inputQHead = inputQHead->m_nextpkt;
        m_freem(m);
        count++;
    }
//  IN_Q_RESET;
    return count;
}

UInt32 IONetworkInterface::inputPacket(struct mbuf * m,
                                       UInt32        length = 0,
                                       bool          queue  = false)
{
    assert(m);

    // Truncate the tail of the mbuf chain to the specified length.
    // The mbuf chain passed in will usually have the length in
    // each mbuf set to its capacity.
    //
    if (length)
    {
        struct mbuf * mb = m;
        mb->m_pkthdr.len = length;      // set total length
        do {
            if (length < (UInt) mb->m_len)
            {
                mb->m_len = length;     // truncate mbuf to remaining length
            }
            length -= mb->m_len;        // decrement remaining length
        } while ((mb = mb->m_next));
        assert(length == 0);            // why is capacity smaller than length?
    }

    IN_Q_ENQUEUE(m);

    if (queue == false)
    {
        UInt32 count;
        IN_Q_FLUSH(count);
//      IN_Q_RESET;
        return count;
    }
    else
        return 0;
}

//---------------------------------------------------------------------------
// Called by the controller driver to send a network event to the network 
// layer. Possible applications include: media changed events,
// power management events, controller state change events.
//
// eventType: The event type.
// arg:       An argument associated with the event.

void IONetworkInterface::inputEvent(UInt32 eventType, void * arg)
{
    switch (eventType)
    {
        case kIONetworkEventMediumChange:
        case kIONetworkEventLinkChange:
            _handleMediumAndLinkChangeEvent();
            break;

        default:
            IOLog("IONetworkInterface: event (%x, %x) not handled\n",
                (UInt) eventType, (UInt) arg);
            break;
    }
}

//---------------------------------------------------------------------------
// Handles media and link change events. Deliver a notification to
// all IONetworkUserClients.

void IONetworkInterface::_handleMediumAndLinkChangeEvent()
{
    OSCollectionIterator * iter;
    IONetworkUserClient *  uc;

    lockForArbitration();
    iter = OSCollectionIterator::withCollection(_clientSet);
    if (iter) {
        while ((uc = (IONetworkUserClient *) iter->getNextObject())) {
            if ((uc = OSDynamicCast(IONetworkUserClient, uc)) == 0)
                continue;
            uc->deliverNotification(kIONUCNotificationTypeLinkChange);
        }
        iter->release();
    }
    unlockForArbitration();
}

//---------------------------------------------------------------------------
// SIOCSIFMTU (set interface MTU) ioctl handler.

SInt IONetworkInterface::syncSIOCSIFMTU(IONetworkController * ctlr,
                                        struct ifreq *        ifr)
{   
    SInt    error;
    UInt32  newMtu = ifr->ifr_mtu;

    // If change is not necessary, return success without getting the
    // controller involved.

    if (getMaxTransferUnit() == newMtu)
        return 0;

    // Request the controller to switch MTU size.
    //
    error = errnoFromReturn(ctlr->doSetMaxTransferUnit(this, newMtu));

    if (error == 0)
    {
        // Controller reports success. Update the interface MTU size
        // property.
        //
        setMaxTransferUnitInt(newMtu);
    }

    return error;
}

//---------------------------------------------------------------------------
// SIOCSIFMEDIA (SET interface media) ioctl handler.

SInt IONetworkInterface::syncSIOCSIFMEDIA(IONetworkController * ctlr,
                                          struct ifreq *        ifr)
{
    OSDictionary *    mediumDict;
    IONetworkMedium * medium;
    SInt              error;

    mediumDict = ctlr->copyMediumDictionary();  // creates a copy
    if (!mediumDict)
    {
        // unable to allocate memory, or no medium dictionary.
        return EOPNOTSUPP;
    }

    medium = IONetworkMedium::getMediumWithType(mediumDict, ifr->ifr_media);
    if (!medium)
    {
        // Exact type was not found. Try a partial match.
        // ifconfig program sets the media type and media
        // options separately. The client should not send
        // an incomplete type!!!

        medium = IONetworkMedium::getMediumWithType(mediumDict,
                                                    ifr->ifr_media,
                                                    ~kIOMediumTypeMask);
        if (!medium)
        {   
            mediumDict->release();
            return EINVAL;       // requested medium not found.
        }
    }

    // It may be possible for the controller to update the medium
    // dictionary and perhaps delete the medium entry that we have
    // selected from our copy of the stale dictionary. This should be
    // harmless since IONetworkController's doSelectMedium() should 
    // filter invalid selections before calling the driver.

    error = errnoFromReturn(ctlr->doSelectMedium(this, medium->getName()));

    if (error == 0)
    {
        // Remember the last media type sent by BSD which the driver
        // accepted. Note that _bsdMediaType may not be equal to
        // medium->getType() since we may have done a partial match.
        // The only reason for this is to be able to return the
        // _bsdMediaType as the current media type in SICGIFMEDIA.

        _bsdMediaType = ifr->ifr_media;
    }

    mediumDict->release();

    return error;
}

//---------------------------------------------------------------------------
// SIOCGIFMEDIA (GET interface media) ioctl handler.

SInt IONetworkInterface::syncSIOCGIFMEDIA(IONetworkController * ctlr,
                                          struct ifreq *        ifr)
{
    OSDictionary *          mediumDict  = 0;
    UInt                    mediumCount = 0;
    UInt                    maxCount;
    OSCollectionIterator *  iter = 0;
    UInt32 *                typeList;
    UInt                    typeListSize;
    OSSymbol *              keyObject;
    SInt                    error = 0;
    struct ifmediareq *     ifmr = (struct ifmediareq *) ifr;

    // Maximum number of medium types that the ioctl caller will accept.
    //
    maxCount = ifmr->ifm_count;

    do {
        mediumDict = ctlr->copyMediumDictionary();  // creates a copy
        if (!mediumDict)
        {
            error = EOPNOTSUPP;
            break;  // unable to allocate memory, or no medium dictionary.
        }

        if ((mediumCount = mediumDict->getCount()) == 0)
            break;  // no medium in the medium dictionary

        if (maxCount == 0)
            break;  //  caller is only probing for support and media count.

        if (maxCount < mediumCount)
        {
            // user buffer is too small to hold all medium entries.
            error = E2BIG;

            // Proceed with partial copy on E2BIG. This follows the
            // SIOCGIFMEDIA handling practice in bsd/net/if_media.c.
            //
            // break;
        }

        // Create an iterator to loop through the medium entries in the
        // dictionary.
        //
        iter = OSCollectionIterator::withCollection(mediumDict);
        if (!iter)
        {
            error = ENOMEM;
            break;
        }

        // Allocate memory for the copyout buffer.
        //
        typeListSize = maxCount * sizeof(UInt32);
        typeList = (UInt32 *) IOMalloc(typeListSize);
        if (!typeList)
        {
            error = ENOMEM;
            break;
        }
        bzero(typeList, typeListSize);

        // Iterate through the medium dictionary and copy the type of
        // each medium entry to typeList[].
        //
        mediumCount = 0;
        while ( (keyObject = (OSSymbol *) iter->getNextObject()) &&
                (mediumCount < maxCount) )
        {
            IONetworkMedium * medium = OSDynamicCast(IONetworkMedium, 
                                       mediumDict->getObject(keyObject));
            if (!medium)
                continue;   // should not happen!

            typeList[mediumCount++] = medium->getType();
        }

        if (mediumCount)
        {
            error = copyout((caddr_t) typeList,
                            (caddr_t) ifmr->ifm_ulist,
                            typeListSize);
        }

        IOFree(typeList, typeListSize);
    }
    while (0);

    ifmr->ifm_active = ifmr->ifm_current = IFM_NONE;
    ifmr->ifm_status = 0;
    ifmr->ifm_count  = mediumCount;

    // Get a copy of the controller's property table and read the
    // link status, current, and active medium.

    OSDictionary * pTable = ctlr->dictionaryWithProperties();
    if (pTable)
    {
        OSNumber * linkStatus = OSDynamicCast(OSNumber, 
                                pTable->getObject(kIOLinkStatus));
        if (linkStatus)
            ifmr->ifm_status = linkStatus->unsigned32BitValue();
        
        if (mediumDict)
        {
            IONetworkMedium * medium;
            OSSymbol *        mediumName;

            if ((mediumName = OSDynamicCast(OSSymbol, 
                              pTable->getObject(kIOCurrentMedium))) &&
                (medium = OSDynamicCast(IONetworkMedium,
                          mediumDict->getObject(mediumName))))
            {
                ifmr->ifm_current = medium->getType();
                if (IOMediumGetType(_bsdMediaType) ==
                    IOMediumGetType((UInt32) ifmr->ifm_current))
                {
                    ifmr->ifm_current = _bsdMediaType;
                }
            }

            if ((mediumName = OSDynamicCast(OSSymbol, 
                              pTable->getObject(kIOActiveMedium))) &&
                (medium = OSDynamicCast(IONetworkMedium,
                          mediumDict->getObject(mediumName))))
            {
                ifmr->ifm_active = medium->getType();
            }
        }
        pTable->release();
    }
    
    if (iter)
        iter->release();
    if (mediumDict)
        mediumDict->release();

    return error;
}

//---------------------------------------------------------------------------
// Handles generic socket ioctl commands sent to the interface.
// IONetworkInterface handles commands that are common to all network
// families. A subclass of IONetworkInterface may override this method
// in order to handle the same command that is handled here, but in a
// different manner, or (the more like case) to augment the command
// handling to include additional commands, and call super for any
// commands not handled in the subclass.
//
// The commands handled by IONetworkInterface are:
//    SIOCGIFMTU   - Get interface MTU size.
//    SIOCSIFMTU   - Set interface MTU size.
//    SIOCSIFMEDIA - Set media.
//    SIOCGIFMEDIA - Get media and link status.
//
// Returns a BSD return code defined in bsd/sys/errno.h.

SInt IONetworkInterface::performCommand(IONetworkController * ctlr,
                                        UInt32                cmd,
                                        void *                arg0,
                                        void *                arg1)
{
    SInt ret = EBUSY;

    ctlr->syncRequest(this,             /* sender */
                      this,             /* target */
                     (IONetworkAction) &IONetworkInterface::syncPerformCommand,
                      (UInt32 *) &ret,  /* return */
                      (void *) ctlr,    /* arg0 - arg3 */
                      (void *) cmd,
                      (void *) arg0,
                      (void *) arg1);

    return ret;
}

SInt IONetworkInterface::syncPerformCommand(IONetworkController * ctlr,
                                            UInt32                cmd,
                                            void *                arg0,
                                            void *                arg1)
{
    struct ifreq * ifr = (struct ifreq *) arg1;
    SInt           ret = EINVAL;

    if (ifr == 0)
        return EINVAL;

    switch (cmd)
    {
        // Get interface MTU.
        //
        case SIOCGIFMTU:
            ifr->ifr_mtu = getMaxTransferUnit();
            ret = 0;    // no error
            break;

        // Set interface MTU.
        //
        case SIOCSIFMTU:
            ret = syncSIOCSIFMTU(ctlr, ifr);
            break;

        // Set interface media type.
        //
        case SIOCSIFMEDIA:
            ret = syncSIOCSIFMEDIA(ctlr, ifr);
            break;

        // Get interface media type and status.
        //
        case SIOCGIFMEDIA:
            ret = syncSIOCGIFMEDIA(ctlr, ifr);
            break;

        default:
            // DLOG(%s: command not handled (%08lx), getName(), cmd);
            break;
    }

    return ret;
}

//---------------------------------------------------------------------------
// if_ioctl() handler - Calls performCommand() when we receive an ioctl
// command from the network stack.

int
IONetworkInterface::ioctl_shim(struct ifnet * ifp, u_long cmd, caddr_t data)
{
    assert(ifp && ifp->if_private);

    IONetworkInterface * self = (IONetworkInterface *) ifp->if_private;

    assert((ifp == self->_ifp) && self->_provider);

    return self->performCommand(self->_provider,
                                cmd,
                                (void *) ifp,
                                (void *) data);
}

//---------------------------------------------------------------------------
// if_output() handler.
//
// Handle a call from the network stack to transmit all packets in
// the ifnet's output queue. This queue is likely to be removed in
// the future when DLIL gets rolled out.

int IONetworkInterface::output_shim(struct ifnet * ifp, struct mbuf * m)
{
    assert(ifp && ifp->if_private);

    IONetworkInterface * self = (IONetworkInterface *) ifp->if_private;
    
    assert(ifp == self->_ifp);

    if (m == 0)
    {
        DLOG("IONetworkInterface: NULL output mbuf\n");
        return EINVAL;
    }

    if ((m->m_flags & M_PKTHDR) == 0)
    {
        DLOG("IONetworkInterface: M_PKTHDR bit not set\n");
        m_freem(m);
        return EINVAL;
    }

    // Feed the output filter tap.
    //
    if (self->_outputFilterTapMode == kIOFilterTapModeInternal)
        _feedFilterTap(ifp, m, self->_outputFilterFunc, BPF_TAP_OUTPUT);
    
    // Forward the packet to the registered output packet handler.
    //
    return ((self->_target)->*(self->_outAction))(m);
}

//---------------------------------------------------------------------------
// if_set_bpf_tap() handler. Handles request from the DLIL to enable or
// disable the input/output filter taps.
//
// FIXME - locking may be needed.

int IONetworkInterface::set_bpf_tap_shim(struct ifnet * ifp,
                                         int            mode,
                                         BPF_FUNC       func)
{
    assert(ifp && ifp->if_private);

    IONetworkInterface * self = (IONetworkInterface *) ifp->if_private;

    assert(ifp == self->_ifp);

    switch (mode)
    {
        case BPF_TAP_DISABLE:
            self->_inputFilterFunc = self->_outputFilterFunc = 0;
            break;

        case BPF_TAP_INPUT:
            assert(func);
            self->_inputFilterFunc = func;
            break;

        case BPF_TAP_OUTPUT:
            assert(func);
            self->_outputFilterFunc = func;
            break;
        
        case BPF_TAP_INPUT_OUTPUT:
            assert(func);
            self->_inputFilterFunc = self->_outputFilterFunc = func;
            break;

        default:
            DLOG("IONetworkInterface: Unknown BPF tap mode %d\n", mode);
            break;
    }

    return 0;
}

//---------------------------------------------------------------------------
// As the name implies, this function does nothing. This will get called
// if the network stack tries to call the if_watchdog function pointer 
// in ifnet. This should not happen. IOKit does not use this watchdog
// timer facility.

void IONetworkInterface::null_shim(struct ifnet * /*ifp*/)
{
    IOLog("IONetworkInterface::null_shim called!\n");
}

//---------------------------------------------------------------------------
// ifnet field (and property table) getter/setter.
//
// For the setter, if restrict flag is set, that means that the property is
// restricted from being modified once the interface's has been registered.

bool IONetworkInterface::_setInterfaceProperty(
    UInt32   value,
    UInt32   mask,
    UInt32   bytes,
    void *   addr,
    char *   key,
    bool     restrict)
{
    bool   updateOk = false;
    UInt32 newValue;

    lock();

    if (restrict && (_registered == true))
        goto abort;

    // Update the property in ifnet.
    //
    switch (bytes)
    {
        case 1:
            newValue = (*((UInt8 *) addr) & mask) | value;
            *((UInt8 *) addr) = (UInt8) newValue;
            break;
        case 2:
            newValue = (*((UInt16 *) addr) & mask) | value;
            *((UInt16 *) addr) = (UInt16) newValue;
            break;
        case 4:
            newValue = (*((UInt32 *) addr) & mask) | value;
            *((UInt32 *) addr) = (UInt32) newValue;
            break;
        default:
            goto abort;
    }

    // Update the OSNumber in the property table.
    //
    updateOk = key ? setProperty(key, newValue, bytes * 8) : true;

abort:
    unlock();
    return updateOk;
}

#define IO_IFNET_GET(func, type, field)                        \
type IONetworkInterface:: ## func() const                      \
{                                                              \
    type ret;                                                  \
    ((IONetworkInterface *) this)->lock();                     \
    ret = _ifp ? _ifp-> ## field : 0;                          \
    ((IONetworkInterface *) this)->unlock();                   \
    return ret;                                                \
}

#define IO_IFNET_SET(func, type, field, propName)              \
bool IONetworkInterface:: ## func ## Int(type value)           \
{                                                              \
    return _setInterfaceProperty(                              \
        (UInt32) value,                                        \
        0,                                                     \
        sizeof(type),                                          \
        (void *) &_ifp-> ## field,                             \
        propName,                                              \
        false);                                                \
}                                                              \
                                                               \
bool IONetworkInterface:: ## func(type value)                  \
{                                                              \
    return _setInterfaceProperty(                              \
        (UInt32) value,                                        \
        0,                                                     \
        sizeof(type),                                          \
        (void *) &_ifp-> ## field,                             \
        propName,                                              \
        true);                                                 \
}

#define IO_IFNET_RMW(func, type, field, propName)                            \
bool IONetworkInterface:: ## func ## Int(type value, type clear = 0)         \
{                                                                            \
    return _setInterfaceProperty(                                            \
        (UInt32) value,                                                      \
        (UInt32) ~clear,                                                     \
        sizeof(type),                                                        \
        (void *) &_ifp-> ## field,                                           \
        propName,                                                            \
        false);                                                              \
}                                                                            \
                                                                             \
bool IONetworkInterface:: ## func(type value, type clear = 0)                \
{                                                                            \
    return _setInterfaceProperty(                                            \
        (UInt32) value,                                                      \
        (UInt32) ~clear,                                                     \
        sizeof(type),                                                        \
        (void *) &_ifp-> ## field,                                           \
        propName,                                                            \
        true);                                                               \
}

//---------------------------------------------------------------------------
// Interface type accessors (ifp->if_type). The list of interface types is
// defined in <bsd/net/if_types.h>.

IO_IFNET_SET(setInterfaceType, UInt8, if_type, kIOInterfaceType)
IO_IFNET_GET(getInterfaceType, UInt8, if_type)

//---------------------------------------------------------------------------
// Mtu (MaxTransferUnit) accessors (ifp->if_mtu).

IO_IFNET_SET(setMaxTransferUnit, UInt32, if_mtu, kIOMaxTransferUnit)
IO_IFNET_GET(getMaxTransferUnit, UInt32, if_mtu)

//---------------------------------------------------------------------------
// Flags accessors (ifp->if_flags). This is a read-modify-write operation.

IO_IFNET_RMW(setFlags, UInt16, if_flags, kIOInterfaceFlags)
IO_IFNET_GET(getFlags, UInt16, if_flags)

//---------------------------------------------------------------------------
// EFlags accessors (ifp->if_eflags). This is a read-modify-write operation.

IO_IFNET_RMW(setExtraFlags, UInt32, if_eflags, kIOInterfaceExtraFlags)
IO_IFNET_GET(getExtraFlags, UInt32, if_eflags)

//---------------------------------------------------------------------------
// MediaAddressLength accessors (ifp->if_addrlen)

IO_IFNET_SET(setMediaAddressLength, UInt8, if_addrlen, kIOMediaAddressLength)
IO_IFNET_GET(getMediaAddressLength, UInt8, if_addrlen)

//---------------------------------------------------------------------------
// MediaHeaderLength accessors (ifp->if_hdrlen)

IO_IFNET_SET(setMediaHeaderLength, UInt8, if_hdrlen, kIOMediaHeaderLength)
IO_IFNET_GET(getMediaHeaderLength, UInt8, if_hdrlen)

//---------------------------------------------------------------------------
// Interface unit number. The unit number for the interface is assigned
// by our client.

IO_IFNET_SET(setUnitNumber, UInt16, if_unit, 0)
IO_IFNET_GET(getUnitNumber, UInt16, if_unit)

//---------------------------------------------------------------------------
// Interface name.

IO_IFNET_SET(setInterfaceName, const char *, if_name, 0)
IO_IFNET_GET(getInterfaceName, const char *, if_name)

//---------------------------------------------------------------------------
// Return true if the interface has been registered with the network layer,
// false otherwise.

bool IONetworkInterface::isRegistered() const
{
    return _registered;
}

//---------------------------------------------------------------------------
// Perform a lookup of the dictionary kept by the interface,
// and return an entry that matches the specified string key.
//
// key: Search for an IONetworkData entry with this key.
//
// Returns the matching entry, or 0 if no match was found.

IONetworkData * IONetworkInterface::getNetworkData(const OSSymbol * key) const
{
    return OSDynamicCast(IONetworkData, _dataDict->getObject(key));
}

IONetworkData * IONetworkInterface::getNetworkData(const char * key) const
{
    return OSDynamicCast(IONetworkData, _dataDict->getObject(key));
}

//---------------------------------------------------------------------------
// A private function to copy the data dictionary to the property table.

bool IONetworkInterface::_copyNetworkDataDictToPropertyTable()
{
    OSDictionary * aCopy = OSDictionary::withDictionary(_dataDict);
    bool           ret   = false;

    if (aCopy) {
        ret = setProperty(kIONetworkData, _dataDict);
        aCopy->release();
    }

    return ret;
}

//---------------------------------------------------------------------------
// Remove an entry from the dictionary of IONetworkData objects.
//
// aKey: A key for an IONetworkData entry in the dictionary.
//
// Returns true if completed without errors,
// false if the operation was aborted.

bool IONetworkInterface::removeNetworkData(const OSSymbol * aKey)
{
    bool ret = false;

    lockForArbitration();

    do {
        if (_clientSet->getCount())
            break;

        _dataDict->removeObject(aKey);
        ret = _copyNetworkDataDictToPropertyTable();
    }
    while (0);

    unlockForArbitration();

    return ret;
}

bool IONetworkInterface::removeNetworkData(const char * aKey)
{
    bool ret = false;

    lockForArbitration();

    do {
        if (_clientSet->getCount())
            break;

        _dataDict->removeObject(aKey);
        ret = _copyNetworkDataDictToPropertyTable();
    }
    while (0);

    unlockForArbitration();

    return ret;
}

//---------------------------------------------------------------------------
// Add an IONetworkData object to a dictionary kept by
// the interface.
//
// aData: An IONetworkData object.
//
// Returns true if the data object was added successfully, false otherwise.

bool IONetworkInterface::addNetworkData(IONetworkData * aData)
{
    bool ret = false;

    if (OSDynamicCast(IONetworkData, aData) == 0)
        return false;

    lockForArbitration();

    if (_clientSet->getCount() == 0)
    {
        if ((ret = _dataDict->setObject(aData->getKey(), aData)))
            ret = _copyNetworkDataDictToPropertyTable();
    }

    unlockForArbitration();

    return ret;
}

//---------------------------------------------------------------------------
// Create a new IOUserClient to handle client requests. The default
// implementation will create an IONetworkUserClient instance if
// the type is kIONUCType.

IOReturn IONetworkInterface::newUserClient(task_t           owningTask,
                                           void *         /*security_id*/,
                                           UInt32           type,
                                           IOUserClient **  handler)
{
    IOReturn                err = kIOReturnSuccess;
    IONetworkUserClient *   client;

    if (type != kIONUCType)
        return kIOReturnBadArgument;

    client = IONetworkUserClient::withTask(owningTask);

    if (!client || !client->attach(this) || !client->start(this))
    {
        if (client)
        {
            client->detach(this);
            client->release();
            client = 0;
        }
        err = kIOReturnNoMemory;
    }

    *handler = client;

    return err;
}
