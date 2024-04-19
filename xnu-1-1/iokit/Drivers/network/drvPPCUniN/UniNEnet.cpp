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
 * Copyright (c) 1998-1999 Apple Computer
 *
 * Hardware independent (relatively) code for the Sun GEM Ethernet Controller 
 *
 * HISTORY
 *
 * dd-mmm-yy     
 *  Created.
 *
 */

//void call_kdp(void);

#include "UniNEnetPrivate.h"

#define super IOEthernetController

OSDefineMetaClassAndStructors( UniNEnet, IOEthernetController )

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/
bool UniNEnet::init(OSDictionary * properties = 0)
{
    /*
     * Initialize my ivars.
     */
    networkInterface  = 0;
    transmitQueue     = 0;
    debugQueue        = 0;
    debugger          = 0;
    interruptSource   = 0;
    txDebuggerPkt     = 0;
    phyId             = 0xff;
    phyStatusPrev     = 0;
    ready             = false;
    debugClient       = false;
    debugTxPoll       = false;
    netifClient       = false;
    isPromiscuous     = false;
    multicastEnabled  = false;

    if (!super::init(properties))
        return false;

    /*
     * Clear the mbuf rings.
     */
    for (int i = 0; i < TX_RING_LENGTH; i++)
    {
        txElementPtrs[i] = 0;
    }
            
    for (int i = 0; i < RX_RING_LENGTH; i++)
    {
        rxMbuf[i] = 0;

    }
     
    return true;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::start(IOService * provider)
{    
    volatile UInt32	clockReg;
    OSString		*matchEntry;

    nub = OSDynamicCast(IOPCIDevice, provider);

    if (!nub || !super::start(provider))
    {
        return false;
    }
    
    transmitQueue = OSDynamicCast(IOOQLockFIFOQueue, getOutputQueue());
    if (!transmitQueue) 
    {
        IOLog("Ethernet(UniN): Output queue initialization failed\n");
        return false;
    }
    transmitQueue->retain();

    /*
     * Allocate debug queue. This stores packets retired from the TX ring
     * by the polling routine. We cannot call freePacket() or m_free() within
     * the debugger context.
     *
     * The capacity of the queue is set at maximum to prevent the queue from
     * calling m_free() due to over-capacity. But we don't expect the size
     * of the queue to grow too large.
     */
    debugQueue = IOPacketQueue::withCapacity((UInt) -1);
    if (!debugQueue)
    {
        return false;
    }
    
    /*
     * Allocate a IOMBufDBDMAMemoryCursor instance. Currently, the maximum
     * number of segments is set to 1. The maximum length for each segment
     * is set to the maximum ethernet frame size (plus padding).
     */    
    mbufCursor = IOMBufBigMemoryCursor::withSpecification(NETWORK_BUFSIZE, 1);
    if (!mbufCursor) 
    {
        IOLog("Ethernet(UniN): IOMBufDBDMAMemoryCursor allocation failure\n");
        return false;
    }

    matchEntry = OSDynamicCast( OSString, getProperty( gIONameMatchedKey ) );
    if ( matchEntry == 0 )
    {
        IOLog("Ethernet(UniN): Cannot obtain matching property.\n");
        return false;
    }

    if ( matchEntry->isEqualTo( "gmac" ) == true )
    {
        platformNub = OSDynamicCast( Core99PE, getPlatform() );

        if ( platformNub == 0 )
        {
            IOLog("Ethernet(UniN): Cannot access platform registers.\n");
            return false;
        }

        clockReg = platformNub->readUniNReg( 0x20 );
        clockReg |= 0x02;
        platformNub->writeUniNReg( 0x20, clockReg );
    }

    /*
     * BUS MASTER, MEM I/O Space, MEM WR & INV
     */
    nub->configWrite32( 0x04, 0x16 );

    /*
     *  set Latency to Max , cache 32
     */
    nub->configWrite32( 0x0C, ((2 + (kGEMBurstSize * (0+1)))<< 8) | (CACHE_LINE_SIZE >> 2) );

    ioMapEnet = nub->mapDeviceMemoryWithRegister( 0x10 );
    if ( ioMapEnet == NULL )
    {
        return false;
    }
    ioBaseEnet = (volatile IOPPCAddress)ioMapEnet->getVirtualAddress();

    phyId             = (UInt8) -1;
    
    /*
     * Get a reference to the IOWorkLoop in our superclass.
     */
    IOWorkLoop * myWorkLoop = getWorkLoop();

    /*
     * Allocate three IOInterruptEventSources.
     */
    interruptSource = IOInterruptEventSource::interruptEventSource(
                        (OSObject *) this,
                        (IOInterruptEventAction) &UniNEnet::interruptOccurred,
                        (IOService *)            provider,
                        (int)                    0 );

    if ( interruptSource == NULL )
    {
        IOLog("Ethernet(UniN): Couldn't allocate Interrupt event source\n");    
        return false;
    }

    if ( myWorkLoop->addEventSource( interruptSource ) != kIOReturnSuccess )
    {
        IOLog("Ethernet(UniN): Couldn't add Interrupt event source\n");    
        return false;
    }     

        
    timerSource = IOTimerEventSource::timerEventSource
        (this, (IOTimerEventSource::Action) &UniNEnet::timeoutOccurred);
    if ( timerSource == NULL )
    {
        IOLog("Ethernet(UniN): Couldn't allocate timer event source\n");
        return false;
    }

    if ( myWorkLoop->addEventSource( timerSource ) != kIOReturnSuccess )
    {
        IOLog("Ethernet(UniN): Couldn't add timer event source\n");        
        return false;
    }     

    MGETHDR(txDebuggerPkt, M_DONTWAIT, MT_DATA);
    
    if (!txDebuggerPkt) 
    {
        IOLog("Ethernet(UniN): Couldn't allocate KDB buffer\n");
        return false;
    }

    /*
     * Perform a hardware reset.
     */
    if ( resetAndEnable(false) == false ) 
    {
        IOLog("Ethernet(UniN): resetAndEnable() failed\n");
        return false;
    }

    /*
     * Cache my MAC address.
     */
    if ( getHardwareAddress(&myAddress) != kIOReturnSuccess )
    {
        IOLog("Ethernet(UniN): getHardwareAddress() failed\n");
        return false;
    }

    /*
     * Allocate memory for ring buffers.
     */
    if ( allocateMemory() == false) 
    {
        IOLog("Ethernet(UniN): allocateMemory() failed\n");    
        return false;
    }

    /*
     * Attach a kernel debugger client.
     */
	attachDebuggerClient(&debugger);

    /*
     * Attach an IOEthernetInterface client.
     */
    if ( attachNetworkInterface((IONetworkInterface **) &networkInterface) == false )
    {
        IOLog("Ethernet(UniN): attachNetworkInterface() failed\n");      
        return false;
    }
    return true;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/
#if 1
bool UniNEnet::configureNetworkInterface(IONetworkInterface * netif)
{
    if ( super::configureNetworkInterface( netif ) == true )
    {
        return netif->setExtraFlags(IFEF_DVR_REENTRY_OK);
    }
    return false;
}
#endif

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::free()
{
    UInt                        i;
    TxQueueElement              *txElement;

    resetAndEnable(false);

	if (debugger)
		debugger->release();

    if (getWorkLoop())
    {
        getWorkLoop()->disableAllEventSources();
    }
    
    if (timerSource)
    {
        timerSource->release();
        timerSource = 0;
    }
    
    if (interruptSource)
    {
        interruptSource->release();
    }
    
    if (txDebuggerPkt)
    {
        freePacket(txDebuggerPkt);
    }
    
    if (transmitQueue)
    {
        transmitQueue->release();
    }
    
    if (debugQueue)
    {
        debugQueue->release();
    }
    
    if (networkInterface)
    {
        networkInterface->release();
    }
    
    if (mbufCursor)
    {
        mbufCursor->release();
    }
    
    for (i = 0; i < rxMaxCommand; i++)
    {
        if (rxMbuf[i])
        {
            freePacket(rxMbuf[i]);
        }
    }
        
    for (i = 0; i < txMaxCommand; i++)
    {
        txElement = txElementPtrs[i];
        txElementPtrs[i] = 0;
            
        if (txElement != 0)
        { 
            if ( --txElement->count == 0 )
            {
                freePacket(txElement->mbuf);
                IOFree( txElement, sizeof(TxQueueElement) );
            }    
        }
    }
    
    if ( ioMapEnet )
    {
        ioMapEnet->release();
    }     

    if ( dmaCommands != 0 )
    {
        IOFreeContiguous( (void *)dmaCommands, dmaCommandsSize );
    }

    super::free();
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::interruptOccurred(IOInterruptEventSource *src,
         int /*count*/)
{
    UInt32      interruptStatus;
    bool        doFlushQueue;
    bool        doService;

    do 
    {
        reserveDebuggerLock();

        interruptStatus = ReadUniNRegister( ioBaseEnet, kGEMInterruptStatus ) 
                                      & (kGEMInterruptStatus_TxInt | kGEMInterruptStatus_RxDone);

        doService  = false;

        if ( interruptStatus & kGEMInterruptStatus_TxInt )
        {
            txWDInterrupts++;
            KERNEL_DEBUG(DBG_GEM_TXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
            doService = transmitInterruptOccurred();
            KERNEL_DEBUG(DBG_GEM_TXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
        }

        doFlushQueue = false;

        if ( interruptStatus & kGEMInterruptStatus_RxDone )
        {
            KERNEL_DEBUG(DBG_GEM_RXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
            doFlushQueue = receiveInterruptOccurred();
            KERNEL_DEBUG(DBG_GEM_RXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
        }

        releaseDebuggerLock();

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
    }
    while ( interruptStatus );

    interruptSource->enable();

}


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

UInt32 UniNEnet::outputPacket(struct mbuf * pkt)
{
    UInt32 ret = kIOOQReturnSuccess;

    KERNEL_DEBUG(DBG_GEM_TXQUEUE | DBG_FUNC_NONE, (int) pkt, (int) pkt->m_pkthdr.len, 0, 0, 0 );

    /*
     * Hold the debugger lock so the debugger can't interrupt us
     */
    reserveDebuggerLock();
 
    if ( transmitPacket(pkt) == false )
    {
        ret = kIOOQReturnStall;
    }
      
    releaseDebuggerLock();

    return ret;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::resetAndEnable(bool enable)
{
    bool ret = true;

    reserveDebuggerLock();

    ready = false;

    if (timerSource) 
    {
        timerSource->cancelTimeout();
    }
    
    disableAdapterInterrupts();
    if (getWorkLoop())
    { 
        getWorkLoop()->disableAllInterrupts();
    }
    
    if ( resetChip() == false )
    {
        ret = false;
        goto resetAndEnable_exit;
    } 
    
    while (enable)
    {
        if (!initRxRing() || !initTxRing()) 
        {
            ret = false;
            break;
        }

        if ( phyId != 0xff )
        {
            miiInitializePHY(phyId);
        }

        if (initChip() == false)
        {
            ret = false;
            break;
        }

//        startChip();

        timerSource->setTimeoutMS(WATCHDOG_TIMER_MS);

        if (getWorkLoop())
        { 
            getWorkLoop()->enableAllInterrupts();
        }    
        enableAdapterInterrupts();

        ready = true;

//        sendDummyPacket();

        break;
    }

resetAndEnable_exit: ;
    
    releaseDebuggerLock();

    return ret;
}

/*-------------------------------------------------------------------------
 * Called by IOEthernetInterface client to enable the controller.
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::enable(IONetworkInterface * netif)
{
    IONetworkParameter * param;

    /*
     * If an interface client has previously enabled us,
     * and we know there can only be one interface client
     * for this driver, then simply return true.
     */
    if (netifClient) 
    {
        IOLog("EtherNet(UniN): already enabled\n");
        return kIOReturnSuccess;
    }

    /*
     * Grab a pointer to the statistics structure in the interface.
     */
    param = netif->getParameter(kIONetworkStatsKey);
    if (!param || !(netStats = (IONetworkStats *) param->getBuffer()))
    {
        IOLog("EtherNet(UniN): invalid network statistics\n");
        return kIOReturnError;
    }

    if ((ready == false) && !resetAndEnable(true))
        return kIOReturnIOError;

    /*
     * Record the interface as an active client.
     */
    netifClient = true;

    /*
     * Start our IOOutputQueue object.
     */
    transmitQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
    transmitQueue->start();

    return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 * Called by IOEthernetInterface client to disable the controller.
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/
 
IOReturn UniNEnet::disable(IONetworkInterface * /*netif*/)
{
    /*
     * If we have no active clients, then disable the controller.
     */
    if (debugClient == false)
    {
        resetAndEnable(false);
    }
    
    /*
     * Disable our IOOutputQueue object. This will prevent the
     * outputPacket() method from being called.
     */
    transmitQueue->stop();

    /*
     * Flush all packets currently in the output queue.
     */
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

IOReturn UniNEnet::handleDebuggerOpen(IOKernelDebugger * /*debugger*/)
{
    /*
     * Enable hardware and make it ready to support the debugger client.
     */
    if ((ready == false) && !resetAndEnable(true))
    {
        return kIOReturnIOError;
    }
    
    /*
     * Record the debugger as an active client of ours.
     */
    debugClient = true;

    /*
     * Returning true will allow the kdp registration to continue.
     * If we return false, then we will not be registered as the
     * debugger device, and the attachDebuggerClient() call will
     * return NULL.
     */
    return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 * This method is called by our debugger client to stop the controller.
 * The debugger will call this method when we issue a detachDebuggerClient().
 *
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::handleDebuggerClose(IOKernelDebugger * /*debugger*/)
{
    debugClient = false;

    /*
     * If we have no active clients, then disable the controller.
     */
    if (netifClient == false)
    {
        resetAndEnable(false);
    }

    return kIOReturnSuccess;
}


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::timeoutOccurred(IOTimerEventSource * /*timer*/)
{
    bool  		doService = false;
    UInt32		txRingIndex;

    if ( ready == false )
    {
        IOLog("EtherNet(UniN): Spurious timeout event!!\n");
        return;
    }
        
    reserveDebuggerLock();

    monitorLinkStatus();

    /*
     * If there are pending entries on the Tx ring
     */
    if ( txCommandHead != txCommandTail )
    {
        /* 
         * If the hardware tx pointer did not move since the last
         * check, increment the txWDCount.
         */
        txRingIndex = ReadUniNRegister( ioBaseEnet, kGEMTxCompletion );
        if ( txRingIndex == txRingIndexLast )
        {
            txWDCount++;         
        }
        else
        {
            txWDCount = 0;
            txRingIndexLast = txRingIndex;
        }
   
        if ( txWDCount > 2 )
        {
            /* 
             * We only take interrupts every 64 tx completions, so we may be here just
             * to do normal clean-up of tx packets. We check if the hardware tx pointer
             * points to the next available tx slot. This indicates that we transmitted all
             * packets that were scheduled vs rather than the hardware tx being stalled.
             */
            if ( txRingIndex != txCommandTail )
            {
                UInt32        interruptStatus, compReg, kickReg;
 
                interruptStatus = ReadUniNRegister( ioBaseEnet, kGEMInterruptStatus ); 
                compReg = ReadUniNRegister( ioBaseEnet, kGEMTxCompletion );
                kickReg = ReadUniNRegister( ioBaseEnet, kGEMTxKick );

                //IOLog( "Tx Int Timeout - Comp = %04x Kick = %04x Int = %08x\n\r", (int)compReg, (int)kickReg, (int)interruptStatus ); 
            }

//          dumpRegisters();

            transmitInterruptOccurred();

            doService = true;

            txRingIndexLast = txRingIndex;
            txWDCount = 0;
        }
    }
    else
    {
        txWDCount        = 0;
    }
    
    /*
     * Clean-up after the debugger if the debugger was active.
     */
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
	 * Make sure the queue is not stalled.
	 */
	if (doService && netifClient)
	{
		transmitQueue->service();
	}

    /*
     * Restart the watchdog timer
     */
    timerSource->setTimeoutMS(WATCHDOG_TIMER_MS);

}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

const char * UniNEnet::getVendorString() const
{
    return ("Apple");
}

const char * UniNEnet::getModelString() const
{
    return ("gmac+");
}

const char * UniNEnet::getRevisionString() const
{
    return ("");
}


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::setPromiscuousMode(IOEnetPromiscuousMode mode)
{
    UInt16      addressFilterReg;
    
    
    reserveDebuggerLock();

    addressFilterReg = ReadUniNRegister(ioBaseEnet, kGEMMacRxMacConfig);
    
    if (mode == kIOEnetPromiscuousModeOff) 
    {
        addressFilterReg &= ~(kGEMMacRxMacConfig_ReceiveAll);
        isPromiscuous     = false;

    }
    else
    {
        addressFilterReg |= kGEMMacRxMacConfig_ReceiveAll;
        isPromiscuous     = true;

    }    
    WriteUniNRegister(ioBaseEnet, kGEMMacRxMacConfig, addressFilterReg);

    releaseDebuggerLock();
    
    return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::setMulticastMode(IOEnetMulticastMode mode)
{
	multicastEnabled = (mode == kIOEnetMulticastModeOff) ? false : true;

	return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::setMulticastList(enet_addr_t *addrs, UInt count)
{
    reserveDebuggerLock();
    
    resetHashTableMask();
    for (UInt i = 0; i < count; i++) 
    {
        addToHashTableMask(addrs->ea_byte);
        addrs++;
    }
    updateHashTableMask();
    
    releaseDebuggerLock();
    return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOOutputQueue * UniNEnet::allocateOutputQueue()
{
    return IOOQLockFIFOQueue::withTarget( this, TRANSMIT_QUEUE_SIZE );
}
















