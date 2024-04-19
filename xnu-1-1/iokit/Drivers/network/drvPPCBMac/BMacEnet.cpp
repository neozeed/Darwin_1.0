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
 * Copyright (c) 1998-1999 by Apple Computer, Inc., All rights reserved.
 *
 * Hardware independent (relatively) code for the BMac Ethernet Controller 
 *
 * HISTORY
 *
 * dd-mmm-yy	 
 *	Created.
 *
 * Dec 10, 1998		jliu
 *  Converted to IOKit/C++.
 */

#include "BMacEnet.h"
#include "BMacEnetPrivate.h"

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/platform/AppleMacIODevice.h>
#include <IOKit/assert.h>

// #define DEBUG_JOE	1

//------------------------------------------------------------------------

#define super IOEthernetController

OSDefineMetaClassAndStructors( BMacEnet, IOEthernetController )

//------------------------------------------------------------------------

#define PROVIDER_DEV	0
#define	PROVIDER_DMA_TX	1
#define	PROVIDER_DMA_RX	2

/*
 * Public Instance Methods
 */

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool BMacEnet::init(OSDictionary * properties = 0)
{
	/*
	 * Initialize my ivars.
	 */
	networkInterface  = 0;
	transmitQueue     = 0;
	debugQueue        = 0;
	debugger          = 0;
	rxIntSrc          = 0;
	txDebuggerPkt     = 0;
	phyId             = 0xff;
	phyMIIDelay	      = MII_DEFAULT_DELAY;
    sromAddressBits   = 6;
    enetAddressOffset = 20;
	phyStatusPrev     = 0;
	ready             = false;
	debugClient       = false;
	debugTxPoll       = false;
	netifClient       = false;
    isPromiscuous	  = false;
    multicastEnabled  = false;

	if (!super::init(properties))
		return false;

	/*
	 * Clear the mbuf rings.
	 */
	for (int i = 0; i < TX_RING_LENGTH; i++)
		txMbuf[i] = 0;
	for (int i = 0; i < RX_RING_LENGTH; i++)
		rxMbuf[i] = 0;

	/*
	 * Clear the memory map objects.
	 */
	for (int i = 0; i < MEMORY_MAP_COUNT; i++)
		maps[i] = 0;

	return true;
}

bool BMacEnet::start(IOService * provider)
{
	AppleMacIODevice *nub = OSDynamicCast(AppleMacIODevice, provider);
	IOInterruptEventSource *intES;

	if (!nub || !super::start(provider))
		return false;

	transmitQueue = OSDynamicCast(IOOQGateFIFOQueue, getOutputQueue());
	if (!transmitQueue) {
		IOLog("BMac: output queue initialization failed\n");
		return false;
	}
	transmitQueue->retain();

	// Allocate debug queue. This stores packets retired from the TX ring
	// by the polling routine. We cannot call freePacket() or m_free() within
	// the debugger context.
	//
	// The capacity of the queue is set at maximum to prevent the queue from
	// calling m_free() due to over-capacity. But we don't expect the size
	// of the queue to grow too large.
	//
	debugQueue = IOPacketQueue::withCapacity((UInt) -1);
	if (!debugQueue)
		return false;

	// Allocate a IOMBufDBDMAMemoryCursor instance. Currently, the maximum
	// number of segments is set to 2. The maximum length for each segment
	// is set to the maximum ethernet frame size (plus padding).

	mbufCursor = IOMBufBigMemoryCursor::withSpecification(NETWORK_BUFSIZE, 2);
	if (!mbufCursor) {
		IOLog("Ethernet(BMac): IOMBufDBDMAMemoryCursor allocation failure\n");
		return false;
	}

	//
	// Our provider is the nub representing the BMacEnet hardware
	// controller. We will query it for our resource information.
	//

	for (int i = 0; i < MEMORY_MAP_HEATHROW_INDEX; i++) {
		IOMemoryMap *		map;
		
		map = provider->mapDeviceMemoryWithIndex(i);
		if (!map)
			return false;
#ifdef DEBUG_JOE
		IOLog("map %d: Phys:%08x Virt:%08x len:%d\n",
			i,
			(unsigned) map->getPhysicalAddress(),
			(unsigned) map->getVirtualAddress(),
			(unsigned) map->getLength());
#endif DEBUG_JOE

		switch (i) {
			case MEMORY_MAP_ENET_INDEX:
				ioBaseEnet = (IOPPCAddress) map->getVirtualAddress();
				break;
			
			case MEMORY_MAP_TXDMA_INDEX:
				ioBaseEnetTxDMA = (IODBDMAChannelRegisters *)
					map->getVirtualAddress();
				break;
			
			case MEMORY_MAP_RXDMA_INDEX:
				ioBaseEnetRxDMA = (IODBDMAChannelRegisters *)
					map->getVirtualAddress();
				break;
		}
		
		maps[i] = map;
	}

#ifdef DEBUG_JOE
	IOLog("ioBaseEnet:      0x%08x\n", ioBaseEnet);
	IOLog("ioBaseEnetTxDMA: 0x%08x\n", ioBaseEnetTxDMA);
	IOLog("ioBaseEnetRxDMA: 0x%08x\n", ioBaseEnetRxDMA);
#endif DEBUG_JOE

	//
	// We need to get the I/O address for the Heathrow controller.
	// We ask the provider (bmac) for its device tree parent.
	//
	IOService *heathrow;
	if (!(heathrow = OSDynamicCast(IOService,
                            provider->getParentEntry( gIODTPlane ))))
		return false;

	// Check whether the hardware is susceptible to the broken unicast
	// filter problem.
	//
	OSData * devIDData;
	devIDData = OSDynamicCast(OSData, heathrow->getProperty("device-id"));

	if (devIDData) {
		useUnicastFilter = ( *((UInt32 *) devIDData->getBytesNoCopy()) ==
                             0x10 );
		if (useUnicastFilter)
			IOLog("%s: Enabling workaround for broken unicast filter\n", 
				getName());
	}

	IOMemoryMap *		map = heathrow->mapDeviceMemoryWithIndex(0);
	if (map)
	{
#ifdef DEBUG_JOE
		IOLog("Heathrow: Phys:%08x Virt:%08x len:%d\n",
			(unsigned) map->getPhysicalAddress(),
			(unsigned) map->getVirtualAddress(),
			(unsigned) map->getLength());
#endif DEBUG_JOE
		ioBaseHeathrow = (IOPPCAddress) map->getVirtualAddress();
		
		maps[MEMORY_MAP_HEATHROW_INDEX] = map;
	}
	else {
		return false;
	}

	//
	// Get a reference to the IOWorkLoop in our superclass.
	//
	IOWorkLoop * myWorkLoop = getWorkLoop();
	assert(myWorkLoop);

	//
	// Allocate three IOInterruptEventSources.
	//
	rxIntSrc = IOInterruptEventSource::interruptEventSource
            (this,
             (IOInterruptEventAction) &BMacEnet::interruptOccurredForSource,
             provider, PROVIDER_DMA_RX);
        if (!rxIntSrc
        || (myWorkLoop->addEventSource(rxIntSrc) != kIOReturnSuccess)) {
		IOLog("Ethernet(BMac): rxIntSrc init failure\n");
		return false;
	}

	intES = IOInterruptEventSource::interruptEventSource
            (this,
             (IOInterruptEventAction) &BMacEnet::interruptOccurredForSource,
             provider, PROVIDER_DMA_TX);
        if (intES) {
            bool res = (myWorkLoop->addEventSource(intES) != kIOReturnSuccess);
            intES->release();
            if (res) {
		IOLog("Ethernet(BMac): PROVIDER_DMA_TX add failure\n");
                return false;
            }
        }
        else {
		IOLog("Mace: PROVIDER_DMA_TX init failure\n");
		return false;
	}
	
	intES = IOInterruptEventSource::interruptEventSource
            (this,
             (IOInterruptEventAction) &BMacEnet::interruptOccurredForSource,	
	     provider, PROVIDER_DEV);	
	if (intES) {	
	    bool res = (myWorkLoop->addEventSource(intES) != kIOReturnSuccess);	
	    intES->release();	
	    if (res) {
		IOLog("Ethernet(BMac): PROVIDER_DEV add failure\n");	
	        return false;	
	    }	
	}	
	else {
		IOLog("Ethernet(BMac): PROVIDER_DEV init failure\n");
		return false;
	}
	
	timerSrc = IOTimerEventSource::timerEventSource
            (this, (IOTimerEventSource::Action) &BMacEnet::timeoutOccurred);
	if (!timerSrc
	|| (myWorkLoop->addEventSource(timerSrc) != kIOReturnSuccess)) {
		IOLog("Ethernet(BMac): timerSrc init failure\n");
		return false;
	}

	MGETHDR(txDebuggerPkt, M_DONTWAIT, MT_DATA);
	if (!txDebuggerPkt) {
		IOLog("Ethernet(BMac): Couldn't allocate KDB buffer\n");
		return false;
	}

#if 0
	// Enable the interrupt event sources. The hardware interrupts
	// sources remain disabled until _resetAndEnable(true) is called.
	//
	// myWorkLoop->enableAllInterrupts();
#endif

	//
	// Perform a hardware reset.
	//
    if ( !_resetAndEnable(false) ) 
    {
		return false;
    }

	//
	// Cache my MAC address.
	//
	getHardwareAddress(&myAddress);

	//
	// Allocate memory for ring buffers.
	//
    if (_allocateMemory() == false) 
    {
		return false;
    }

	//
	// Attach a kernel debugger client.
	//
	attachDebuggerClient(&debugger);

	//
	// Attach an IOEthernetInterface client.
	//
	if (!attachNetworkInterface((IONetworkInterface **) &networkInterface))
		return false;

	return true;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void BMacEnet::free()
{
    UInt i;

	_resetAndEnable(false);

	if (debugger)
		debugger->release();

	if (getWorkLoop())
		getWorkLoop()->disableAllEventSources();

	if (timerSrc)
		timerSrc->release();

	if (rxIntSrc)
		rxIntSrc->release();

	if (txDebuggerPkt)
		freePacket(txDebuggerPkt);

	if (transmitQueue)
		transmitQueue->release();

	if (debugQueue)
		debugQueue->release();

    if (networkInterface)
		networkInterface->release();

	if (mbufCursor)
		mbufCursor->release();

    for (i = 0; i < rxMaxCommand; i++)
    	if (rxMbuf[i]) freePacket(rxMbuf[i]);

    for (i = 0; i < txMaxCommand; i++)
    	if (txMbuf[i]) freePacket(txMbuf[i]);

	for (i = 0; i < MEMORY_MAP_COUNT; i++)
		if (maps[i]) maps[i]->release();

	if (dmaMemory.ptr)
	{
		IOFree(dmaMemory.ptrReal, dmaMemory.sizeReal);
		dmaMemory.ptr = 0;
	}

	super::free();
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void BMacEnet::interruptOccurredForSource(IOInterruptEventSource *src,
		 int /*count*/)
{
	bool doFlushQueue = false;
	bool doService    = false;

	reserveDebuggerLock();

    statReg = ReadBigMacRegister( ioBaseEnet, kSTAT );

	if (src == rxIntSrc) {	
		KERNEL_DEBUG(DBG_BMAC_RXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );		
		doFlushQueue = _receiveInterruptOccurred();	
		KERNEL_DEBUG(DBG_BMAC_RXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );	
	}
	else {			
      /*
       * On the transmit side, we use the chipset interrupt. Using the
	   * transmit DMA interrupt (or having multiple transmit DMA entries)
	   * would allows us to send the next frame to the chipset prior the 
	   * transmit fifo going empty. 
       * However, this aggrevates a BMac chipset bug where the next frame going
	   * out gets corrupted (first two bytes lost) if the chipset had to retry 
	   * the previous frame.
       */ 
        txWDInterrupts++;
        KERNEL_DEBUG(DBG_BMAC_TXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
		doService = _transmitInterruptOccurred();
        KERNEL_DEBUG(DBG_BMAC_TXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
	}

	releaseDebuggerLock();

	/*
	 * Submit all received packets queued up by _receiveInterruptOccurred()
	 * to the network stack. The up call is performed without holding the
	 * debugger lock.
	 */
	if (doFlushQueue)
		networkInterface->flushInputQueue();

	/*
	 * Unstall the output queue if some space was made available.
	 */
	if (doService && netifClient)
		transmitQueue->service();
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

UInt32 BMacEnet::outputPacket(struct mbuf * pkt)
{
	u_int32_t	i;
	UInt32      ret = kIOOQReturnSuccess;

    KERNEL_DEBUG(DBG_BMAC_TXQUEUE | DBG_FUNC_NONE, (int) pkt,
		(int) pkt->m_pkthdr.len, 0, 0, 0 );

    /*
     * Hold the debugger lock so the debugger can't interrupt us
     */
	reserveDebuggerLock();
 
    do
    {
		/* 
		 * Preliminary sanity checks
		 */
		assert(pkt && netifClient);

#if 0
		/*
		 * Remove any completed packets from the Tx ring 
		 */
		if ( chipId >= kCHIPID_PaddingtonXmitStreaming )
		{
			_transmitInterruptOccurred(); 
		}
#endif

		i = txCommandTail + 1;
		if ( i >= txMaxCommand ) i = 0;
		if ( i == txCommandHead )
		{
			/*
			 * Ring buffer is full. Disable the dequeueing process.
			 * We reenable it when an entry is made available by the
			 * transmit interrupt handler, or if a timeout occurs.
			 */
			ret = kIOOQReturnStall;
			continue;
		}

		/*
		 * If there is space on the Tx ring, add the packet directly to the
		 * ring.
		 */
		_transmitPacket(pkt);
    }
    while ( 0 );

	releaseDebuggerLock();

	return ret;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool BMacEnet::_resetAndEnable(bool enable)
{
	bool ret = true;

//	reserveDebuggerLock();

	ready = false;

	if (timerSrc) timerSrc->cancelTimeout();

	_disableAdapterInterrupts();
	if (getWorkLoop()) getWorkLoop()->disableAllInterrupts();

	_resetChip();
    
	while (enable)
	{
		if (!_initRxRing() || !_initTxRing())	
		{
			ret = false;
			break;
		}

		if ( phyId != 0xff )
		{
			miiInitializePHY(phyId);
		}

		if (_initChip() == false)
		{
			ret = false;
			break;
		}

		_startChip();

		timerSrc->setTimeoutMS(WATCHDOG_TIMER_MS);

		if (getWorkLoop()) getWorkLoop()->enableAllInterrupts();
		_enableAdapterInterrupts();

		ready = true;

		_sendDummyPacket();

		break;
	}
	
//	releaseDebuggerLock();

    return ret;
}

/*-------------------------------------------------------------------------
 * Called by IOEthernetInterface client to enable the controller.
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/

IOReturn BMacEnet::enable(IONetworkInterface * netif)
{
	IONetworkParameter * param;

	// If an interface client has previously enabled us,
	// and we know there can only be one interface client
	// for this driver, then simply return true.
	//
	if (netifClient) {
		IOLog("EtherNet(BMac): already enabled\n");
		return kIOReturnSuccess;
	}

	// Grab a pointer to the statistics structure in the interface.
	//
	param = netif->getParameter(kIONetworkStatsKey);
	if (!param || !(netStats = (IONetworkStats *) param->getBuffer()))
	{
		IOLog("EtherNet(BMac): invalid network statistics\n");
		return kIOReturnError;
	}

	if ((ready == false) && !_resetAndEnable(true))
		return kIOReturnIOError;

	// Record the interface as an active client.
	//
	netifClient = true;

	// Start our IOOutputQueue object.
	//
	transmitQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
	transmitQueue->start();

	return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 * Called by IOEthernetInterface client to disable the controller.
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/
 
IOReturn BMacEnet::disable(IONetworkInterface * /*netif*/)
{
	// If we have no active clients, then disable the controller.
	//
	if (debugClient == false)
		_resetAndEnable(false);

	// Disable our IOOutputQueue object. This will prevent the
	// outputPacket() method from being called.
	//
	transmitQueue->stop();

	// Flush all packets currently in the output queue.
	//
	transmitQueue->setCapacity(0);
	transmitQueue->flush();
	
	netifClient = false;

	return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 * This method is called by our debugger client to bring up the controller
 * just before the controller is registered as the debugger device. The
 * debugger client is attached in response to the attachDebuggerClient()
 * call.
 *
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/

IOReturn BMacEnet::handleDebuggerOpen(IOKernelDebugger * /*debugger*/)
{
	// Enable hardware and make it ready to support the debugger client.
	//
	if ((ready == false) && !_resetAndEnable(true))
		return kIOReturnIOError;

	// Record the debugger as an active client of ours.
	//
	debugClient = true;

	// Returning true will allow the kdp registration to continue.
	// If we return false, then we will not be registered as the
	// debugger device, and the attachDebuggerClient() call will
	// return NULL.
	//
	return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 * This method is called by our debugger client to stop the controller.
 * The debugger will call this method when we issue a detachDebuggerClient().
 *
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/

IOReturn BMacEnet::handleDebuggerClose(IOKernelDebugger * /*debugger*/)
{
	debugClient = false;

	// If we have no active clients, then disable the controller.
	//
	if (netifClient == false)
		_resetAndEnable(false);

	return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void BMacEnet::timeoutOccurred(IOTimerEventSource * /*timer*/)
{
    u_int32_t		dmaStatus;
    u_int16_t		phyStatus;
    u_int16_t		linkStatus;
    u_int16_t		phyStatusChange;
	bool			fullDuplex   = false;
	bool			doFlushQueue = false;
	bool			doService    = false;

	// IOLog("Ethernet(BMac): watchdog timer\n");

	reserveDebuggerLock();
	
    /*
     * Check for DMA shutdown on receive channel
     */
    dmaStatus = IOGetDBDMAChannelStatus( ioBaseEnetRxDMA );
    if ( !(dmaStatus & kdbdmaActive) )
    {
#if 0
		IOLog( "Ethernet(BMac): Timeout check - RxHead = %d RxTail = %d\n", 
			rxCommandHead, rxCommandTail);

      IOLog( "Ethernet(BMac): Rx Commands = %08x(p) Rx DMA Ptr = %08x(p)\n\r", rxDMACommandsPhys, IOGetDBDMACommandPtr(ioBaseEnetRxDMA) ); 
      [self _dumpDesc:(void *)rxDMACommands Size:rxMaxCommand * sizeof(enet_dma_cmd_t)];
#endif

		doFlushQueue = _receiveInterruptOccurred();
	}

    /*
     * If there are pending entries on the Tx ring
     */
    if ( txCommandHead != txCommandTail )
    {
		/* 
		 * If we did not service the Tx ring during the last timeout interval,
		 * then force servicing of the Tx ring
		 */
		if ( txWDInterrupts == 0 )
		{
			if ( txWDCount++ > 0 )
			{
				if (_transmitInterruptOccurred() == false)
				{
#if 0
					IOLog( "Ethernet(BMac): Timeout check - TxHead = %d TxTail = %d\n", 
						txCommandHead, txCommandTail);
#endif
					_restartTransmitter();
				}
				doService = true;
			}
		}
		else
		{
			txWDInterrupts   = 0;
			txWDCount        = 0;
		}
	}
    else
	{
		txWDInterrupts   = 0;
		txWDCount        = 0;
	}

    if ( phyId != 0xff )
    {
        if ( miiReadWord(&phyStatus, MII_STATUS, phyId) == true )
        {
            phyStatusChange = (phyStatusPrev ^ phyStatus) & 
				(MII_STATUS_LINK_STATUS | MII_STATUS_NEGOTIATION_COMPLETE);
            if ( phyStatusChange )
            {
                if ( (phyStatus & MII_STATUS_LINK_STATUS) &&
					 (phyStatus & MII_STATUS_NEGOTIATION_COMPLETE ) )
                {
					if ( (phyType & MII_ST10040_MASK) == MII_ST10040_ID )
					{
						miiReadWord(&linkStatus, MII_ST10040_CHIPST, phyId);
	
						fullDuplex =  (linkStatus & MII_ST10040_CHIPST_DUPLEX) ? true : false;
	
						IOLog( "Ethernet(BMac): Link is up at %sMb - %s Duplex\n\r",
								(linkStatus & MII_ST10040_CHIPST_SPEED)  ? "100" : "10",
								(fullDuplex)                             ? "Full" : "Half" );                        
					}
					else if ( (phyType & MII_DP83843_MASK) == MII_DP83843_ID )
					{
						miiReadWord(&linkStatus, MII_DP83843_PHYSTS, phyId);
	
						fullDuplex =  (linkStatus & MII_DP83843_PHYSTS_DUPLEX) ? true : false;
	
						IOLog( "Ethernet(BMac): Link is up at %sMb - %s Duplex\n\r",
								(linkStatus & MII_DP83843_PHYSTS_SPEED10) ? "10" : "100",
								(fullDuplex)                              ? "Full" : "Half" );           
					}  
					else
					{
						fullDuplex = false ;
						IOLog( "Ethernet(BMac): Link is up\n\r" );
					}
	
					if ( fullDuplex != isFullDuplex )
					{							
							_setDuplexMode(fullDuplex);
					}
				}
                else
                {
                    IOLog( "Ethernet(BMac): Link is down.\n\r" );
                }
                phyStatusPrev = phyStatus;
            }
        }
    }

	// Clean-up after the debugger if the debugger was active.
	//
	if (debugTxPoll)
	{
		debugQueue->flush();
		debugTxPoll = false;
		releaseDebuggerLock();
		doService = true;
	}
	else
	{
		releaseDebuggerLock();
	}

	/*
	 * Submit all received packets queued up by _receiveInterruptOccurred()
	 * to the network stack. The up call is performed without holding the
	 * debugger lock.
	 */
	if (doFlushQueue)
	{
		networkInterface->flushInputQueue();
	}

	/*
	 * Make sure the output queue is not stalled.
	 */
	if (doService && netifClient)
	{
		transmitQueue->service();
	}

    /*
     * Restart the watchdog timer
     */
	timerSrc->setTimeoutMS(WATCHDOG_TIMER_MS);
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

const char * BMacEnet::getVendorString() const
{
	return ("Apple");
}

const char * BMacEnet::getModelString() const
{
	return ("BMac");
}

const char * BMacEnet::getRevisionString() const
{
	return ("");
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn BMacEnet::setPromiscuousMode(IOEnetPromiscuousMode mode)
{
    u_int16_t		rxCFGVal;
    
	reserveDebuggerLock();

    /*
     * Turn off the receiver and wait for the chipset to acknowledge
     */
    rxCFGVal = ReadBigMacRegister(ioBaseEnet, kRXCFG);
    WriteBigMacRegister(ioBaseEnet, kRXCFG, rxCFGVal & ~kRxMACEnable );
    while( ReadBigMacRegister(ioBaseEnet, kRXCFG) & kRxMACEnable )
		;

    /*
     * Set or reset promiscuous mode and restore receiver state
     */
	if (mode == kIOEnetPromiscuousModeOff) {
		rxCFGVal &= ~kRxPromiscEnable;
		isPromiscuous = false;
	}
	else {
		rxCFGVal |= kRxPromiscEnable;
		isPromiscuous = true;
	}

    WriteBigMacRegister( ioBaseEnet, kRXCFG, rxCFGVal );		

	releaseDebuggerLock();
    
    return kIOReturnSuccess;
}

IOReturn BMacEnet::setMulticastMode(IOEnetMulticastMode mode)
{
	multicastEnabled = (mode == kIOEnetMulticastModeOff) ? false : true;

	return kIOReturnSuccess;
}

IOReturn BMacEnet::setMulticastList(enet_addr_t *addrs, UInt count)
{
	reserveDebuggerLock();
	_resetHashTableMask();
	for (UInt i = 0; i < count; i++) {
		_addToHashTableMask(addrs->ea_byte);
		addrs++;
	}
	_updateBMacHashTableMask();
	releaseDebuggerLock();
	return kIOReturnSuccess;
}

/*
 * Kernel Debugger Support 
 */
void BMacEnet::sendPacket(void *pkt, unsigned int pkt_len)
{
	_sendPacket(pkt, pkt_len);
}

void BMacEnet::receivePacket(void *pkt, unsigned int *pkt_len,
	unsigned int timeout)
{
    _receivePacket(pkt, pkt_len, timeout);
}

/*
 * Allocate a WorkLoop serialized output queue object.
 */
IOOutputQueue * BMacEnet::allocateOutputQueue()
{
	return IOOQGateFIFOQueue::withTarget( this,
	                                      getWorkLoop(), 
	                                      TRANSMIT_QUEUE_SIZE );
}

#ifdef PM_SUPPORT
/*
 * Power management methods. 
 */
- (IOReturn)getPowerState:(PMPowerState *)state_p
{
    return kIOReturnUnsupported;
}

- (IOReturn)setPowerState:(PMPowerState)state
{
    if (state == PM_OFF) {
	resetAndEnabled = NO;
        [self _resetChip];
	return kIOReturnSuccess;
    }
    return kIOReturnUnsupported;
}

- (IOReturn)getPowerManagement:(PMPowerManagementState *)state_p
{
    return kIOReturnUnsupported;
}

- (IOReturn)setPowerManagement:(PMPowerManagementState)state
{
    return kIOReturnUnsupported;
}
#endif PM_SUPPORT
