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
 * Copyright (c) 1996 NeXT Software, Inc.  All rights reserved. 
 *
 * i82557.cpp
 *
 * HISTORY
 *
 * 22-Jan-96	Dieter Siegmund (dieter) at NeXT
 *	Created.
 *
 * 03-May-96	Dieter Siegmund (dieter) at NeXT
 *	Added a real ISR to improve performance.
 *
 * 10-June-96	Dieter Siegmund (dieter) at NeXT
 *	Added support for Splash 3 (10 Base-T only card).
 *
 * 18-June-96	Dieter Siegmund (dieter) at NeXT
 *	Keep the transmit queue draining by interrupting every
 *	N / 2 transmits (where N is the size of the hardware queue).
 *
 * 15-Dec-97	Joe Liu (jliu) at Apple
 *	Updated PHY programming to be 82558 aware.
 *	Misc changes to conform to new 82558 register flags.
 *	Changed RNR interrupt handler to restart RU instead of a reset.
 *	Interrupt handler now does a thread_call_func() to do most of its work.
 *	Interrupts are disabled until the thread callout finishes its work.
 *	Increased the size of TX/RX rings.
 *	buffer object removed, we use cluster mbufs to back up the receive ring.
 *
 * 29-May-98	Joe Liu (jliu) at Apple
 *	Updated _setupPhy method to take advantage of parallel detection whenever
 *	possible in order to detect the proper link speed.
 *
 * 17-Aug-98	Joe Liu (jliu) at Apple
 *	Re-enabled the setting of txready_sel PHY (PCS) bit for DP83840.
 *	Simplified interrupt handling, resulting in RCV performance improvements.
 *	Receive packets are sent upstream via a cached function pointer.
 */

#include "i82557.h"

#define ONE_SECOND_TICKS			1000
#define LOAD_STATISTICS_INTERVAL	(4 * ONE_SECOND_TICKS)

#define super IOEthernetController
OSDefineMetaClassAndStructors( Intel82557, IOEthernetController )

//---------------------------------------------------------------------------
// Function: init <IORegistryEntry>
//
// Initialize the driver instance and prepare it for a probe() call.

bool Intel82557::init(OSDictionary * properties)
{
	return super::init(properties);
}

//---------------------------------------------------------------------------
// Function: probe <IOService>
//
// Probe for the presence of Intel82557 by reading the PCI vendor ID
// from the PCI nub.

IOService * Intel82557::probe(IOService * provider,
                              SInt32 *    score)
{
	IOPCIDevice * pciNub = OSDynamicCast(IOPCIDevice, provider);
	UInt32        pciID;

	if (!pciNub) return 0;

	// Check PCI device ID.
	//
	pciID = pciNub->configRead32(PCI_CFID_OFFSET);
	switch (pciID) {
		case PCI_CFID_INTEL82557:
			break;
		default:
			// IOLog("%s: Unknown device ID %08lx\n", getName(), pciID);
			return 0;
	}

	return super::probe(provider, score);
}

//---------------------------------------------------------------------------
// Function: pciConfigInit
//
// Update PCI command register to enable the memory-mapped range,
// and bus-master interface.

bool Intel82557::pciConfigInit(IOPCIDevice * provider)
{
	UInt32	reg;
	
	reg = provider->configRead32(PCI_CFCS_OFFSET);
    reg |= (0x04 | 0x02 | 0x10);	// Master, Memory, PCI-MWI
    reg &= ~0x01;					// disable I/O space
	provider->configWrite32(PCI_CFCS_OFFSET, reg);

	return true;
}

//---------------------------------------------------------------------------
// Function: initDriver
//
// Create and initialize the driver objects before the hardware is 
// enabled.
//
// Returns true on sucess, and false if initialization failed.

bool Intel82557::initDriver(IOService * provider)
{
	currentMediumType = MEDIUM_TYPE_INVALID;

	// This driver will allocate and use an IOOQGateFIFOQueue.
	//
	transmitQueue = OSDynamicCast(IOOQGateFIFOQueue, getOutputQueue());
	if (!transmitQueue)
		return false;
	transmitQueue->retain();
	
	// Allocate two IOMBufLittleMemoryCursor instances. One for transmit and
	// the other for receive.
	//
	rxMbufCursor = IOMBufLittleMemoryCursor::withSpecification(MAX_BUF_SIZE,1);
	txMbufCursor = IOMBufLittleMemoryCursor::withSpecification(MAX_BUF_SIZE, 
	                                                           TBDS_PER_TCB);
	if (!rxMbufCursor || !txMbufCursor)
		return false;

	// Get a handle to our superclass' workloop.
	//
	IOWorkLoop * myWorkLoop = (IOWorkLoop *) getWorkLoop();
	if (!myWorkLoop)
		return false;

	// Create and register an interrupt event source. The provider will
	// take care of the low-level interrupt registration stuff.
	//
#if USE_FILTER_INTERRUPT_EVENT_SRC
    interruptSrc =
        IOFilterInterruptEventSource::filterInterruptEventSource(this,
                    (IOInterruptEventAction) &Intel82557::interruptOccurred,
                    (IOFilterInterruptAction) &Intel82557::checkForInterrupt,
                    provider);
#else
	interruptSrc = 
		IOInterruptEventSource::interruptEventSource(this,
                    (IOInterruptEventAction) &Intel82557::interruptOccurred,
                    provider);
#endif
	if (!interruptSrc ||
		(myWorkLoop->addEventSource(interruptSrc) != kIOReturnSuccess))
		return false;

	// Register a timer event source. This is used as a watchdog timer.
	//
	timerSrc = IOTimerEventSource::timerEventSource(this,
                    (IOTimerEventSource::Action) &Intel82557::timeoutOccurred);
	if (!timerSrc ||
		(myWorkLoop->addEventSource(timerSrc) != kIOReturnSuccess))
		return false;

	// Create a dictionary to hold IONetworkMedium objects.
	//
	mediumDict = OSDictionary::withCapacity(5);
	if (!mediumDict)
		return false;

	return true;
}

//---------------------------------------------------------------------------
// Function: getDefaultSettings
//
// Get the default driver settings chosen by the user. The properties
// are all stored in our property table (an OSDictionary).
// For convenience, all properties are OSNumbers.

bool Intel82557::getDefaultSettings()
{
	OSNumber * offset;

    // Check for PHY address override.
	//
    phyAddr = PHY_ADDRESS_DEFAULT;
	offset = OSDynamicCast(OSNumber, getProperty("PHY Address"));
	if (offset) {
		phyAddr = offset->unsigned32BitValue();
    }

    // Check for Verbose flag.
	//
    verbose = false;
        offset = OSDynamicCast(OSNumber, getProperty("Verbose"));
	if (offset && offset->unsigned32BitValue()) {
		IOLog("%s: verbose mode enabled\n", getName());
		verbose = true;
	}

    // Check for Flow-Control enable flag.
	//
    flowControl = false;
        offset = OSDynamicCast(OSNumber, getProperty("Flow Control"));
	if (offset && offset->unsigned32BitValue()) {
		IOLog("%s: 802.3x flow control enabled\n", getName());
		flowControl = true;
	}

	return true;
}

//---------------------------------------------------------------------------
// Function: start <IOService>
//
// Hardware was detected and initialized, start the driver.

bool Intel82557::start(IOService * provider)
{
	IOPCIDevice *        pciNub = OSDynamicCast(IOPCIDevice, provider);

	// Start our superclass first.
	//
	if (!pciNub || !super::start(provider))
		return false;

	// Initialize the driver's event sources and other support objects.
	//
	if (!initDriver(provider))
		return false;

	// Get the virtual address mapping of CSR registers located at
	// Base Address Range 0 (0x10). The size of this range is 4K.
	//
	csrMap = pciNub->mapDeviceMemoryWithRegister(PCI_BAR0_OFFSET);
	if (!csrMap)
		return false;
	CSR_p  = (CSR_t *) csrMap->getVirtualAddress();

	// Enables the bus-master and turns on memory space decoding.
	//
	if (!pciConfigInit(pciNub))
		return false;

	// Create the EEPROM object.
	//
	eeprom = i82557eeprom::withAddress(&CSR_p->eepromControl);
	if (!eeprom) {
		IOLog("%s: couldn't allocate eeprom object", getName());
		return false;
	}

	// Get default driver settings.
	//
	if (!getDefaultSettings())
		return false;

	if (verbose)
		eeprom->dumpContents();

	// Execute one-time initialization code.
	//
    if (!coldInit()) {
		IOLog("%s: coldInit failed\n", getName());
		return false;
    }

	if (!hwInit()) {
		IOLog("%s: hwInit failed\n", getName());
		return false;
    }

	// Publish our media capabilities.
	//
	_phyPublishMedia();
	if (!publishMediumDictionary(mediumDict)) {
		IOLog("%s: publishMediumDictionary failed\n", getName());
		return false;
	}

	// Announce the basic hardware configuration info.
	//
	IOLog("%s: Memory 0x%lx irq %d\n",
		getName(), csrMap->getPhysicalAddress(), 0);

	// Allocate and attach an IOEthernetInterface instance to this driver
	// object.
	//
	if (!attachInterface((IONetworkInterface **) &netif, false))
		return false;

	// Attach a kernel debugger client.
	//
	attachDebuggerClient(&debugger);

	netif->registerService();

    return true;
}

//---------------------------------------------------------------------------
// Function: stop <IOService>
//
// Stop all activities and prepare for termination.

void Intel82557::stop(IOService * provider)
{
#if 0	// This belongs in stop()?
    if (resetAndEnabled == YES) {		/* shut down the hardware */
		[self disableAdapterInterrupts];
		resetAndEnabled = NO;
    }
#endif
}

bool Intel82557::configureInterface(IONetworkInterface * netif)
{
	IONetworkData * param;
	
	if (super::configureInterface(netif) == false)
		return false;
	
	// Get the generic network statistics structure.
	//
	param = netif->getParameter(kIONetworkStatsKey);
	if (!param || !(netStats = (IONetworkStats *) param->getBuffer())) {
		return false;
	}

	// Get the Ethernet statistics structure.
	//
	param = netif->getParameter(kIOEthernetStatsKey);
	if (!param || !(etherStats = (IOEthernetStats *) param->getBuffer())) {
		return false;
	}
	
	return true;
}

//---------------------------------------------------------------------------
// Function: free <IOService>
//
// Deallocate all resources and destroy the instance.

void Intel82557::free()
{
	if (debugger)      debugger->release();
    if (netif)         netif->release();
	if (transmitQueue) transmitQueue->release();
	if (interruptSrc)  interruptSrc->release();
	if (timerSrc)      timerSrc->release();
	if (rxMbufCursor)  rxMbufCursor->release();
	if (txMbufCursor)  txMbufCursor->release();
	if (csrMap)        csrMap->release();
	if (eeprom)        eeprom->release();
	if (mediumDict)    mediumDict->release();

	_freeMemPage(&shared);
	_freeMemPage(&txRing);
	_freeMemPage(&rxRing);

	super::free();	// pass it to our superclass
}

//---------------------------------------------------------------------------
// Function: enableAdapter
//
// Enables the adapter & driver to the given level of support.

bool Intel82557::enableAdapter(UInt32 level)
{
	bool ret = false;

//	IOLog("%s::%s enabling level %ld\n", getName(), __FUNCTION__, level);

	switch (level) {
		case kAdapterLevel1:
			if (!_initRingBuffers())
				break;
		
			if (!_startReceive()) {
				_clearRingBuffers();
				break;
			}
		
			// Set current medium.
			//
			if (setMedium(getCurrentMedium()) != kIOReturnSuccess)
				IOLog("%s: setMedium error\n", getName());

			// Start the watchdog timer.
			//
			timerSrc->setTimeoutMS(LOAD_STATISTICS_INTERVAL);

			// Enable interrupt event sources and hardware interrupts.
			//
			if (getWorkLoop())
				getWorkLoop()->enableAllInterrupts();
			enableAdapterInterrupts();

			ret = true;
			break;
		
		case kAdapterLevel2:
			// Issue a dump statistics command.
			//
			_dumpStatistics();
		
			// Start our IOOutputQueue object.
			//
			transmitQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
			transmitQueue->start();

			ret = true;
			break;
	}

	if (!ret)
		IOLog("%s::%s error in level %ld\n", getName(), __FUNCTION__, level);

	return ret;
}

//---------------------------------------------------------------------------
// Function: disableAdapter
//
// Disables the adapter & driver to the given level of support.

bool Intel82557::disableAdapter(UInt32 level)
{
	bool ret = false;

//	IOLog("%s::%s disabling level %ld\n", getName(), __FUNCTION__, level);
		
	switch (level) {
		case kAdapterLevel1:
			// Disable interrupt handling and hardware interrupt sources.
			//
			disableAdapterInterrupts();
			if (getWorkLoop())
				getWorkLoop()->disableAllInterrupts();

			// Stop the timer event source, and initialize the watchdog state.
			//
			timerSrc->cancelTimeout();
			packetsReceived    = true;	// assume we're getting packets
			packetsTransmitted = false;
			txCount = 0;
		
			// Reset the hardware engine.
			//
			ret = hwInit();
		
			// Clear the descriptor rings after the hardware is idle.
			//
			_clearRingBuffers();
		
			// Report no link.
			//
			setLinkStatus(kIONetworkLinkValid, 0, 0);
		
			// Flush all packets held in the queue and prevent it
			// from accumulating any additional packets.
			//
			transmitQueue->setCapacity(0);
			transmitQueue->flush();

			break;
		
		case kAdapterLevel2:
			// Stop the transmit queue. outputPacket() will not get called
			// after this.
			//
			transmitQueue->stop();

			ret = true;
			break;
	}

	if (!ret)
		IOLog("%s::%s error in level %ld\n", getName(), __FUNCTION__, level);

	return ret;
}

//---------------------------------------------------------------------------
// Function: setAdapterLevel
//
// Sets the adapter's run level depending on the type of client present.
//
// kAdapterLevel0 - Adapter is disabled.
// kAdapterLevel1 - Adapter is brought up just enough to support debugging.
// kAdapterLevel2 - Adapter is completely up.

bool Intel82557::setAdapterLevel(UInt32 newLevel)
{
	bool    ret = false;
	UInt32  nextLevel;

//	IOLog("---> DESIRED LEVEL : %d\n", newLevel);

	if (currentLevel == newLevel)
		return true;

	for ( ; currentLevel > newLevel; currentLevel--) 
	{
		if ((ret = disableAdapter(currentLevel)) == false)
			break;
	}

	for (nextLevel = currentLevel + 1; currentLevel < newLevel; 
	     currentLevel++, nextLevel++) 
	{
		if ((ret = enableAdapter(nextLevel)) == false)
			break;
	}

//	IOLog("---> PRESENT LEVEL : %d\n\n", currentLevel);

	return ret;
}

//---------------------------------------------------------------------------
// Function: enable <IONetworkController>
//
// A request from our interface client to enable the adapter.

IOReturn Intel82557::enable(IONetworkInterface * /*netif*/)
{
	if (enabledForNetif)
		return kIOReturnSuccess;

	enabledForNetif = setAdapterLevel(kAdapterLevel2);

	return (enabledForNetif ? kIOReturnSuccess : kIOReturnIOError);
}

//---------------------------------------------------------------------------
// Function: disable <IONetworkController>
//
// A request from our interface client to disable the adapter.

IOReturn Intel82557::disable(IONetworkInterface * /*netif*/)
{
	enabledForNetif = false;

	if (enabledForDebugger)
		setAdapterLevel(kAdapterLevel1);
	else
		setAdapterLevel(kAdapterLevel0);

	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Function: enable <IONetworkController>
//
// A request from our debugger client to enable the adapter.

IOReturn Intel82557::enable(IOKernelDebugger * /*debugger*/)
{
	if (enabledForDebugger || enabledForNetif) {
		enabledForDebugger = true;
		return kIOReturnSuccess;
	}

	enabledForDebugger = setAdapterLevel(kAdapterLevel1);

	return (enabledForDebugger ? kIOReturnSuccess : kIOReturnIOError);
}

//---------------------------------------------------------------------------
// Function: disable <IONetworkController>
//
// A request from our debugger client to disable the adapter.

IOReturn Intel82557::disable(IOKernelDebugger * /*debugger*/)
{
	enabledForDebugger = false;

	if (!enabledForNetif)
		setAdapterLevel(kAdapterLevel0);

	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Function: timeoutOccurred
//
// Periodic timer that monitors the receiver status, updates error
// and collision statistics, and update the current link status.

void Intel82557::timeoutOccurred(IOTimerEventSource * /*timer*/)
{
	if ((packetsReceived == false) && (packetsTransmitted == true)) {
		/*
		 * The B-step of the i82557 requires that an mcsetup command be
		 * issued if the receiver stops receiving.  This is a documented
		 * errata.
		 */
		mcSetup(0, 0, true);
	}
	packetsReceived = packetsTransmitted = false;

	_updateStatistics();

	updateLinkStatus();

	timerSrc->setTimeoutMS(LOAD_STATISTICS_INTERVAL);
}

//---------------------------------------------------------------------------
// Function: setPromiscuousMode <IOEthernetController>

IOReturn Intel82557::setPromiscuousMode(IOEnetPromiscuousMode mode)
{
    bool rv;
	promiscuousEnabled = (mode == kIOEnetPromiscuousModeOff) ? false : true;
	reserveDebuggerLock();
	rv = config();
	releaseDebuggerLock();
    return (rv ? kIOReturnSuccess : kIOReturnIOError);
}

//---------------------------------------------------------------------------
// Function: setMulticastMode <IOEthernetController>

IOReturn Intel82557::setMulticastMode(IOEnetMulticastMode mode)
{
	multicastEnabled = (mode == kIOEnetMulticastModeOff) ? false : true;
	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Function: setMulticastList <IOEthernetController>

IOReturn Intel82557::setMulticastList(enet_addr_t * addrs, UInt count)
{
	IOReturn ret = kIOReturnSuccess;

    if (!mcSetup(addrs, count)) {
    	IOLog("%s: set multicast list failed\n", getName());
		ret = kIOReturnIOError;
	}
	return ret;
}

//---------------------------------------------------------------------------
// Function: getPacketBufferConstraints <IONetworkController>
//
// Return our driver's packet alignment requirements.

void
Intel82557::getPacketBufferConstraints(IOPacketBufferConstraints * constraints) const
{
	constraints->alignStart  = kIOPacketBufferAlign2;	// even word aligned.
	constraints->alignLength = kIOPacketBufferAlign1;	// no restriction.
}

//---------------------------------------------------------------------------
// Function: getHardwareAddress <IOEthernetController>
//
// Return the adapter's hardware address.

IOReturn Intel82557::getHardwareAddress(enet_addr_t * addrs)
{
	bcopy(&myAddress, addrs, sizeof(*addrs));
	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Function: allocateOutputQueue <IONetworkController>
//
// Allocate an IOOQGateFIFOQueue instance.

IOOutputQueue * Intel82557::createOutputQueue()
{	
	return IOOQGateFIFOQueue::withTarget(this, getWorkLoop());
}

//---------------------------------------------------------------------------
// Function: updateLinkStatus
//
// Get the current link status and call the inherited setLinkStatus()
// function to update the status. This function is called periodically
// by timeoutOccurred() to poll the current link status.

void Intel82557::updateLinkStatus()
{
	bool              linkUp;
	IONetworkMedium * activeMedium = 0;
	UInt32            linkStatus   = kIONetworkLinkValid;
	UInt64            linkSpeed    = 0;
	mediumType_t      activeMediumType;	

	// Query the hardware for the current active medium, and the link
	// status flag.
	//
	if (_phyGetLinkStatus(&linkUp,
                          &activeMediumType,
                          currentMediumType) == false) {
		return;
	}

	if (linkUp) {
		linkStatus |= kIONetworkLinkActive;

		// Link speeds are fixed per medium. Get the speed from the active
		// medium.
		//
		activeMedium = _phyGetMediumWithCode(activeMediumType);
		if (activeMedium)
			linkSpeed = activeMedium->getSpeed();
	}

	// Update the link status properties.
	//
	if (!setLinkStatus(linkStatus, linkSpeed, activeMedium))
		IOLog("%s: setLinkStatus error\n", getName());
}

//---------------------------------------------------------------------------
// Function: setMedium <IONetworkController>
//
// Transition the controller/PHY to use a new medium. Note that
// this function can be called my the driver, or by our client.

IOReturn Intel82557::setMedium(const IONetworkMedium * medium)
{
	bool  r;

	if (!OSDynamicCast(IONetworkMedium, medium))
		return kIOReturnBadArgument;

#if 0
	IOLog("%s: setMedium -> %s\n", getName(),
		medium->getName()->getCStringNoCopy());
#endif

	// Program PHY.
	//
	r = _phySetMedium((mediumType_t) medium->getIndex());

	// Update the current medium property.
	//
	if (r && !setCurrentMedium(medium))
		IOLog("%s: setCurrentMedium error\n", getName());

	return (r ? kIOReturnSuccess : kIOReturnIOError);
}

//---------------------------------------------------------------------------
// Function: getVendorString(), getModelString(), getRevisionString()
//           <IONetworkController>
//
// Report human readable hardware information strings.

const char * Intel82557::getVendorString() const
{
	return ("Intel");
}

const char * Intel82557::getModelString() const
{
	const char * model = 0;

	assert(eeprom && eeprom->getContents());

	switch(eeprom->getContents()->controllerType) {
		case I82558_CONTROLLER_TYPE:
			model = "82558";
			break;
		case I82557_CONTROLLER_TYPE:
		default:
			model = "82557";
			break;
	}
	return model;
}

const char * Intel82557::getRevisionString() const
{
	return NULL;
}

//---------------------------------------------------------------------------
// Kernel debugger entry points.
//
// KDP driven polling routines to send and transmit a frame.
// Remember, no memory allocation! Not even mbufs are safe.

void Intel82557::sendPacket(void * pkt, UInt pkt_len)
{
	_sendPacket(pkt, pkt_len);
}

void Intel82557::receivePacket(void * pkt, UInt * pkt_len, UInt timeout)
{
	_receivePacket(pkt, pkt_len, timeout);
}
