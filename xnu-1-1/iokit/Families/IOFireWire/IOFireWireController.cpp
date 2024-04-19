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
 * HISTORY
 * 27 April 99 wgulland created.
 *
 */

#include <IOKit/assert.h>

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFWDCLProgram.h>
#include <IOKit/firewire/IOFWLocalIsochPort.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define kDevicePruneDelay 1000	// 1000 milliseconds

#define FWAddressToID(addr) (addr & 63)

const OSSymbol *gFireWireROM;
const OSSymbol *gFireWireNodeID;
const OSSymbol *gFireWireSelfIDs;
const OSSymbol *gFireWireUnit_Spec_ID;
const OSSymbol *gFireWireUnit_SW_Version;
const OSSymbol *gFireWireVendor_ID;
const OSSymbol *gFireWire_GUID;
const OSSymbol *gFireWireSpeed;

const IORegistryPlane * IOFireWireController::gIOFireWirePlane = NULL;

enum ReadROMState {
    kReadROMSize,
    kReadingROM
};

struct NodeScan {
    IOFireWireController *	fControl;
    FWAddress			fAddr;
    UInt32 *			fBuf;
    UInt32 *			fSelfIDs;
    int				fNumSelfIDs;
    ReadROMState		fState;
    IOFWReadQuadCommand *	fCmd;
    UInt32			fROMHdr;
    int				fROMSize;
    int				fRead;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//Utility functions

#define super IOFireWireBus

OSDefineMetaClass( IOFireWireController, IOFireWireBus )
OSDefineAbstractStructors(IOFireWireController, IOFireWireBus)

static void doneReset(void *refcon, IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    fwCmd->release();
}

void IOFireWireController::readROMGlue(void *refcon, IOReturn status,
			IOFireWireNub *device, IOFWCommand *fwCmd)
{
    NodeScan *scan = (NodeScan *)refcon;
    scan->fControl->readDeviceROM(scan, status);
}

IOReturn IOFireWireController::execCommand(OSObject * obj, void *field0, 
				void *field1, void *field2, void *field3)
{
    // obj is the IOFireWireController, if we need that

    IOFWCommand *cmd = (IOFWCommand *)field0;
//kprintf("Executing command 0x%x\n", cmd);
    return cmd->execute();
}

void IOFireWireController::clockTick(OSObject *obj, IOTimerEventSource *src)
{
    IOFireWireController *me = (IOFireWireController *)obj;

    me->pruneDevices();
}
IOWorkLoop * IOFireWireController::createWorkLoop()
{
    IOWorkLoop * wrkLoop;

    wrkLoop = new IOWorkLoop;
    if ( wrkLoop == NULL ) {
        return NULL;
    }

    if ( wrkLoop->init() != true ) {
        return NULL;
    }

    return wrkLoop;
}

bool IOFireWireController::init(OSDictionary * dict)
{
    if(!super::init(dict))
        return false;

    // Create firewire symbols.
    gFireWireROM = OSSymbol::withCString("FireWire Device ROM");
    gFireWireNodeID = OSSymbol::withCString("FireWire Node ID");
    gFireWireSelfIDs = OSSymbol::withCString("FireWire Self IDs");
    gFireWireUnit_Spec_ID = OSSymbol::withCString("Unit_Spec_ID");
    gFireWireUnit_SW_Version = OSSymbol::withCString("Unit_SW_Version");
    gFireWireVendor_ID = OSSymbol::withCString("Vendor_ID");
    gFireWire_GUID = OSSymbol::withCString("GUID");
    gFireWireSpeed = OSSymbol::withCString("FireWire Speed");

    if(NULL == gIOFireWirePlane) {
        gIOFireWirePlane = IORegistryEntry::makePlane( kIOFireWirePlane );
    }

    fLocalAddresses = OSSet::withCapacity(3);	// Local ROM + CSR registers + SBP-2 ORBs
    if(fLocalAddresses)
        fSpaceIterator =  OSCollectionIterator::withCollection(fLocalAddresses);

    fAllocatedChannels = OSSet::withCapacity(1);	// DV channel.
    if(fAllocatedChannels)
        fAllocChannelIterator =  OSCollectionIterator::withCollection(fAllocatedChannels);

    fLastTrans = kMaxPendingTransfers-1;
    fTransAllocLock = IOLockAlloc();
    fChannelLock = IOLockAlloc();

    return (gFireWireROM != NULL &&  gFireWireNodeID != NULL &&
        fChannelLock != NULL && fTransAllocLock != NULL &&
        gFireWireUnit_Spec_ID != NULL && gFireWireUnit_SW_Version != NULL && 
	fLocalAddresses != NULL && fSpaceIterator != NULL &&
            fAllocatedChannels != NULL && fAllocChannelIterator != NULL);
}

bool IOFireWireController::startWorkLoop()
{
    OSData *rom;
    UInt16 crc16;
    IOReturn status;
    CSRROMEntryID csrROMRootEntryID = kInvalidCSRROMEntryID;
    CSRROMEntryID csrROMEntryID = kInvalidCSRROMEntryID;
    UInt32 immediateEntry;


kprintf("Local GUID = 0x%x:0x%x\n", (UInt32)(fUniqueID >> 32), (UInt32)(fUniqueID & 0xffffffff));

    // Build local device ROM
    rom = OSData::withCapacity(20); // smallest possible real ROM
    if(!rom)
	return false;

    // Allocate address space for Configuration ROM and fill in Bus Info
    // block.
    fROMHeader[1] = kFWBIBBusName;
    fROMHeader[2] =
            kFWBIBIrmc | kFWBIBCmc | kFWBIBIsc | (8 << kFWBIBMaxRecPhase);
    fROMHeader[3] = fUniqueID >> 32;
    fROMHeader[4] = fUniqueID & 0xffffffff;

    crc16 = FWComputeCRC16 (&fROMHeader[1], 4);
    fROMHeader[0] = 0x04040000 | crc16;

    rom->appendBytes(fROMHeader, 20);
    setProperty(gFireWireROM, rom);
    rom->release(); // XXX -- svail: added.

    // Create root directory in FWIM data.//zzz should we have one for each FWIM or just one???
    fCSRROMRootDirectory.csrROMEntryData.pParentCSRROMEntryData = NULL;
    fCSRROMRootDirectory.csrROMEntryData.pPrevCSRROMEntryData = NULL;
    fCSRROMRootDirectory.csrROMEntryData.pNextCSRROMEntryData = NULL;
    fCSRROMRootDirectory.csrROMEntryData.entryType =
            kDirectoryCSRROMEntryType;
    //fCSRROMRootDirectory->csrROMEntryData.fwimID = fwimID;
    //fCSRROMRootDirectory.csrROMEntryData.pEntryLocation = NULL;
    fCSRROMRootDirectory.csrROMBasicEntryData.keyValue = 0;
    fCSRROMRootDirectory.pChildCSRROMEntryData = NULL;
    fCSRROMRootDirectory.numChildren = 0;

    status = CSRROMGetRootDirectory (&csrROMRootEntryID);

    // Set our CSR ROM generation.
    if (status == kIOReturnSuccess) {
        immediateEntry = 0;
        status = CSRROMCreateEntry (csrROMRootEntryID,
                                    &csrROMEntryID,
                                    kImmediateCSRROMEntryType,
                                    kCSRGenerationKey,
                                    (UInt8 *) &immediateEntry,
                                    3);
    }

    // Set our module vendor ID.
    if (status == kIOReturnSuccess) {
        immediateEntry = 0x00A040 << 8;
        status = CSRROMCreateEntry (csrROMRootEntryID,
                                    &csrROMEntryID,
                                    kImmediateCSRROMEntryType,
                                    kCSRModuleVendorIDKey,
                                    (UInt8 *) &immediateEntry,
                                    3);
    }

    // Set our capabilities.
    if (status == kIOReturnSuccess) {
        immediateEntry = 0x000083C0 << 8;//zzz need real values.
        status = CSRROMCreateEntry (csrROMRootEntryID,
                                    &csrROMEntryID,
                                    kImmediateCSRROMEntryType,
                                    kCSRNodeCapabilitiesKey,
                                    (UInt8 *) &immediateEntry,
                                    3);
    }

    // Set our node unique ID.
    if (status == kIOReturnSuccess) {
        status = CSRROMCreateEntry (csrROMRootEntryID,
                                    &csrROMEntryID,
                                    kLeafCSRROMEntryType,
                                    kCSRNodeUniqueIdKey,
                                    &fUniqueID,
                                    8);
    }

    // Will instantiate at end (causes a bus reset).
    // Clean up ROM entry references
    if (csrROMRootEntryID != kInvalidCSRROMEntryID)
        FWCSRROMDisposeEntryID (csrROMRootEntryID);

    if (csrROMEntryID != kInvalidCSRROMEntryID)
        FWCSRROMDisposeEntryID (csrROMEntryID);

    // Create command gate, start probing etc.
    fGate = IOCommandGate::commandGate(this, execCommand);
    if(!fGate)
	return false;

    // Create Timer Event source
    fTimer = IOTimerEventSource::timerEventSource(this, clockTick);
    if(!fTimer)
	return false;

    fWorkLoop = createWorkLoop();
    fWorkLoop->addEventSource(fGate);
    fWorkLoop->addEventSource(fTimer);


    IOFWUpdateROM *reset = new IOFWUpdateROM;
    if(reset) {
        if(reset->initWithController(this, doneReset, NULL))
            reset->submit();
	else {
            reset->release();
            reset = NULL;
	}
    }

    return fWorkLoop != NULL && reset != NULL;
}

IOWorkLoop *IOFireWireController::getWorkLoop() const
{
    return fWorkLoop;
}

AsyncPendingTrans *IOFireWireController::allocTrans(bool sleepOK)
{
    unsigned int i;
    unsigned int tran;

    do {
        IOTakeLock(fTransAllocLock);
        tran = fLastTrans;
        for(i=0; i<kMaxPendingTransfers; i++) {
            AsyncPendingTrans *t;
            tran++;
            if(tran >= kMaxPendingTransfers)
                tran = 0;
            t = &fTrans[tran];
            if(!t->fInUse) {
                t->fHandler = NULL;
                t->fInUse = true;
                t->fTCode = tran;
                fLastTrans = tran;
                IOUnlock(fTransAllocLock);
                return t;
            }
        }
        IOUnlock(fTransAllocLock);
        IOLog("Out of FireWire read request codes!\n");
        if(!sleepOK)
            break;
        IOSleep(10);
    } while (true);
    return NULL;
}

void IOFireWireController::freeTrans(AsyncPendingTrans *trans)
{
    // No lock needed - can't have two users of a tcode.
    trans->fInUse = false;
}


void IOFireWireController::readDeviceROM(NodeScan *scan, IOReturn status)
{
    bool done = true;
    if(status != kIOReturnSuccess) {
	// If status isn't bus reset, make a dummy registry entry.
        if(status == kIOFireWireBusReset)
            return;

        OSDictionary *propTable;
        UInt32 nodeID = FWAddressToID(scan->fAddr.nodeID);
        OSObject * prop;
        propTable = OSDictionary::withCapacity(3);
        prop = OSNumber::withNumber(scan->fAddr.nodeID, 16);
        propTable->setObject(gFireWireNodeID, prop);
        prop->release();

        prop = OSNumber::withNumber((scan->fSelfIDs[0] & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
        propTable->setObject(gFireWireSpeed, prop);
        prop->release();

        prop = OSData::withBytes(scan->fSelfIDs, scan->fNumSelfIDs*sizeof(UInt32));
        propTable->setObject(gFireWireSelfIDs, prop);
        prop->release();

        IORegistryEntry * newPhy;
        newPhy = new IORegistryEntry;
	if(newPhy) {
            if(!newPhy->init(propTable)) {
                newPhy->release();	
		newPhy = NULL;
            }
	}
        fNodes[nodeID] = newPhy;
        if(propTable)
            propTable->release();
        fNumROMReads--;
        if(fNumROMReads == 0) {
            fTimer->enable();
            fTimer->setTimeoutMS(kDevicePruneDelay);	// One second
        }

        if(scan->fBuf)
            IOFree(scan->fBuf, scan->fROMSize);
        scan->fCmd->release();
        IOFree(scan, sizeof(*scan));
        return;
    }

    if(scan->fState == kReadROMSize) {
	if( ((scan->fROMHdr & kCSRBusInfoBlockLength) >> kCSRBusInfoBlockLengthPhase) == 1) {
            // Minimal ROM
            scan->fROMSize = 4;
            done = true;
	}
	else {
            scan->fROMSize = 4*((scan->fROMHdr & kCSRROMCRCLength) >> kCSRROMCRCLengthPhase) + 4;
            scan->fBuf = (UInt32 *)IOMalloc(scan->fROMSize);
            *scan->fBuf = scan->fROMHdr;
            scan->fRead = 4;
            if(scan->fROMSize > 4) {
                scan->fState = kReadingROM;
                scan->fAddr.addressLo = kCSRROMBaseAddress+4;
                scan->fCmd->reinit(scan->fAddr, scan->fBuf+1, scan->fROMSize/4-1,
                                                            &readROMGlue, scan, true);
                scan->fCmd->execute();
                done = false;
            }
            else
		done = true;
	}
    }
    if(done) {
	// Check if node exists, if not create it
#if (DEBUGGING_LEVEL > 0)
	DEBUGLOG("Finished reading ROM for node 0x%x\n", scan->fNodeID);
#endif
        OSDictionary *		propTable = NULL;
        IOFireWireDevice *	newDevice = NULL;
        OSData *		rom = NULL;
        do {
            CSRNodeUniqueID guid;
            OSIterator *childIterator;
            if(scan->fROMSize >= 20)
            	guid = *(CSRNodeUniqueID *)(scan->fBuf+3);
            else
                guid = scan->fROMHdr;	// Best we can do.
            if(scan->fROMSize == 4)
                rom = OSData::withBytes(&scan->fROMHdr, 4);
            else
                rom = OSData::withBytes(scan->fBuf, scan->fROMSize);
            childIterator = getClientIterator();
            if( childIterator) {
                IOFireWireDevice * found;
		while( (found = (IOFireWireDevice *) childIterator->getNextObject())) {
                    if(found->fUniqueID == guid) {
			newDevice = found;
                        break;
                    }
		}
                childIterator->release();
            }

            if(newDevice) {
		// Just update device properties.
		newDevice->setNodeROM(fBusGeneration, scan->fAddr.nodeID, fLocalNodeID,
			rom, scan->fSelfIDs, scan->fNumSelfIDs);
		newDevice->retain();	// match release, since not newly created.
            }
            else {
                OSObject * prop;
                propTable = OSDictionary::withCapacity(6);
                if (!propTable)
                    continue;

                prop = OSNumber::withNumber(guid, 64);
                propTable->setObject(gFireWire_GUID, prop);
                prop->release();
                prop = OSNumber::withNumber((scan->fSelfIDs[0] & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
                propTable->setObject(gFireWireSpeed, prop);
                prop->release();
                newDevice = createDeviceNub(propTable);
                if (!newDevice)
                    continue;
                propTable->release();	// done with it after init
                propTable = NULL;

                if (!newDevice->attach(this))
                    continue;
                newDevice->setNodeROM(fBusGeneration, scan->fAddr.nodeID, fLocalNodeID,
                        rom, scan->fSelfIDs, scan->fNumSelfIDs);
                newDevice->registerService();
            }
            if(fIRMNodeID == scan->fAddr.nodeID) {
                IOFWIsochChannel *found;
		fIRMDevice = newDevice;
		// Tell all active ioschronous channels to re-allocate bandwidth
                IOTakeLock(fChannelLock);
                fAllocChannelIterator->reset();
                while( (found = (IOFWIsochChannel *) fAllocChannelIterator->getNextObject())) {
                    found->handleBusReset();
                }
                IOUnlock(fChannelLock);
            }
            UInt32 nodeID = FWAddressToID(scan->fAddr.nodeID);
            fNodes[nodeID] = newDevice;
            fNodes[nodeID]->retain();
	} while (false);
        if (propTable)
            propTable->release();
        if (newDevice)
            newDevice->release();
        if (rom)
            rom->release();
        if(scan->fBuf)
            IOFree(scan->fBuf, scan->fROMSize);
        scan->fCmd->release();
        IOFree(scan, sizeof(*scan));
        fNumROMReads--;
        if(fNumROMReads == 0) {
            fTimer->enable();
            fTimer->setTimeoutMS(kDevicePruneDelay);	// One second
        }
    }
}


//
// Hardware detected a bus reset.
// At this point we don't know what the hardware addresses are
void IOFireWireController::processBusReset()
{
    clock_get_uptime(&fResetTime);	// Update even if we're already processing a reset

    if(!fInReset) {
        fInReset = true;
        unsigned int i;

        fTimer->cancelTimeout();

        // Set all current device nodeIDs to something invalid
        OSIterator *childIterator;
        childIterator = getClientIterator();
        if( childIterator) {
            IOFireWireDevice * found;
            while( (found = (IOFireWireDevice *) childIterator->getNextObject())) {
                found->setNodeROM(fBusGeneration, kFWBadNodeID, kFWBadNodeID, NULL, NULL, 0);
            }
            childIterator->release();
        }


        // Invalidate current topology and speed map
        fBusGeneration++;
        bzero(fSpeedCodes, sizeof(fSpeedCodes));

        // Zap all outstanding async requests
        for(i=0; i<kMaxPendingTransfers; i++) {
            AsyncPendingTrans *t = &fTrans[i];
            if(t->fInUse) {
                IOFWAsyncCommand * cmd = t->fHandler;
                if(cmd) {
                    cmd->gotPacket(this, kFWResponseBusResetError, NULL, 0);
                }
            }
        }

	// Clear out the old firewire plane
        if(fNodes[fRootNodeID]) {
            fNodes[fRootNodeID]->detachAll(gIOFireWirePlane);
	}
        for(i=0; i<=fRootNodeID; i++) {
            if(fNodes[i]) {
		fNodes[i]->release();
		fNodes[i] = NULL;
            }
	}
    }
}

//
// SelfID packets received after reset.
void IOFireWireController::processSelfIDs(UInt32 *IDs, int numIDs, UInt32 *ownIDs, int numOwnIDs)
{
    OSObject *prop;
    int i;
    UInt32 id;
    UInt32 nodeID;
    UInt16 irmID, ourID;

#if 0
for(i=0; i<numIDs; i++)
    IOLog("ID %d: 0x%x <-> 0x%x\n", i, IDs[2*i], ~IDs[2*i+1]);

for(i=0; i<numOwnIDs; i++)
    IOLog("Own ID %d: 0x%x <-> 0x%x\n", i, ownIDs[2*i], ~ownIDs[2*i+1]);
#endif
    // If not processing a reset, then we should be
    // This can happen if we get two resets in quick succession
    if(!fInReset)
        processBusReset();
    fInReset = false;
    fNumROMReads = 0;

    // Update the registry entry for our local nodeID,
    // which will have been updated by the device driver.
    prop = OSNumber::withNumber(fLocalNodeID, 16);
    setProperty(gFireWireNodeID, prop);
    prop->release();
    prop = OSNumber::withNumber((ownIDs[0] & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
    setProperty(gFireWireSpeed, prop);
    prop->release();

    // Initialize root node to be our node, we'll update it below to be the highest node ID.
    fRootNodeID = ourID = (*ownIDs & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase;

    fNodes[ourID] = this;
    retain();

    // Copy over the selfIDs, checking validity and merging in our selfIDs if they aren't
    // already in the list.
    SInt16 prevID = -1;	// Impossible ID.
    UInt32 *idPtr = fSelfIDs;
    for(i=0; i<numIDs; i++) {
        UInt32 id = IDs[2*i];
        UInt16 currID = (id & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase;

        if(id != ~IDs[2*i+1]) {
            IOLog("Bad SelfID packet %d: 0x%x != 0x%x!\n", i, id, ~IDs[2*i+1]);
            resetBus();	// Could wait a bit in case somebody else spots the bad packet
            return;
        }
        if(currID != prevID) {
            // Check for ownids not in main list
            if(prevID < ourID && currID > ourID) {
		int j;
		fNodeIDs[ourID] = idPtr;
                for(j=0; j<numOwnIDs; j++)
                    *idPtr++ = ownIDs[2*j];
            }
            fNodeIDs[currID] = idPtr;
            prevID = currID;
            if(fRootNodeID < currID)
                fRootNodeID = currID;
        }
	*idPtr++ = id;
    }
    // Check for ownids at end & not in main list
    if(prevID < ourID) {
        int j;
        fNodeIDs[ourID] = idPtr;
        for(j=0; j<numOwnIDs; j++)
            *idPtr++ = ownIDs[2*j];
    }
    // Stick a known elephant at the end.
    fNodeIDs[fRootNodeID+1] = idPtr;

    buildTopology(false);

    prop = OSData::withBytes(fNodeIDs[ourID], numOwnIDs*sizeof(UInt32));
    setProperty(gFireWireSelfIDs, prop);
    prop->release();

    // Ask each device for its GUID.
    // In the completion handler we'll match the new IDs to existing
    // nubs and create new ones.
#if (DEBUGGING_LEVEL > 0)
    for(i=0; i<numIDs; i++) {
        id = IDs[2*i];
        if(id != ~IDs[2*i+1]) {
            DEBUGLOG("Bad SelfID: 0x%x <-> 0x%x\n", id, ~IDs[2*i+1]);
            continue;
        }
	DEBUGLOG("SelfID: 0x%x\n", id);
    }
    DEBUGLOG("Our ID: 0x%x\n", *ownIDs);
#endif
    irmID = 0;
    for(i=0; i<=fRootNodeID; i++) {
	if(i == ourID)
            continue;	// Skip ourself!
        id = *fNodeIDs[i];
        // Get nodeID.
        nodeID = (id & kFWSelfIDPhyID) >> kFWSelfIDPhyIDPhase;
        nodeID |= kFWLocalBusAddress>>kCSRNodeIDPhase;

        if((id & (kFWSelfID0C | kFWSelfID0L)) == (kFWSelfID0C | kFWSelfID0L)) {
            kprintf("IRM contender: %x\n", nodeID);
            if(nodeID > irmID)
		irmID = nodeID;
	}

        // Read ROM header if link is active (MacOS8 turns link on, why?)
	if(true) { //id & kFWSelfID0L) {
            NodeScan *scan;
            scan = (NodeScan *)IOMalloc(sizeof(*scan));
            fNumROMReads++;

            scan->fControl = this;
            scan->fAddr.nodeID = nodeID;
            scan->fAddr.addressHi = kCSRRegisterSpaceBaseAddressHi;
            scan->fAddr.addressLo = kCSRBIBHeaderAddress;
            scan->fSelfIDs = fNodeIDs[i];
            scan->fNumSelfIDs = fNodeIDs[i+1] - fNodeIDs[i];
            scan->fState = kReadROMSize;
            scan->fCmd = new IOFWReadQuadCommand;
            scan->fCmd->initAll(this, NULL, scan->fAddr, &scan->fROMHdr, 1, 
						&readROMGlue, scan, true);
            scan->fBuf = NULL;
            scan->fCmd->execute();
	}
    }
    if(irmID != 0)
        fIRMNodeID = irmID;
    else
        fIRMNodeID = kFWBadNodeID;
    if(fNumROMReads == 0) {
        fTimer->enable();
        fTimer->setTimeoutMS(kDevicePruneDelay);	// One second
    }
}

void IOFireWireController::buildTopology(bool doFWPlane)
{
    int i;
    IORegistryEntry *root;
    struct FWNodeScan
    {
        int nodeID;
        int childrenRemaining;
        IORegistryEntry *node;
    };
    FWNodeScan scanList[kFWMaxNodeHops];
    FWNodeScan *level;
    root = fNodes[fRootNodeID];
    level = scanList;

    // First build the topology.
    for(i=fRootNodeID; i>=0; i--) {
        UInt32 id0;
        UInt8 speedCode;
        IORegistryEntry *node = fNodes[i];
        id0 = *fNodeIDs[i];
        int children = 0;
        UInt32 port;
        port = (id0 & kFWSelfID0P0) >> kFWSelfID0P0Phase;
        if(port == kFWSelfIDPortStatusChild)
            children++;
        port = (id0 & kFWSelfID0P1) >> kFWSelfID0P1Phase;
        if(port == kFWSelfIDPortStatusChild)
            children++;
        port = (id0 & kFWSelfID0P2) >> kFWSelfID0P2Phase;
        if(port == kFWSelfIDPortStatusChild)
            children++;

        // Add node to bottom of tree
        level->nodeID = i;
        level->childrenRemaining = children;
        level->node = node;

        // Add node's self speed to speedmap
        speedCode = (id0 & kFWSelfID0SP) >> kFWSelfID0SPPhase;
        fSpeedCodes[(kFWMaxNodesPerBus + 1)*i + i] = speedCode;

        // Add to parent
        // Compute rest of this node's speed map entries unless it's the root.
        // We only need to compute speeds between this node and all higher node numbers.
        // These speeds will be the minimum of this node's speed and the speed between
        // this node's parent and the other higher numbered nodes.
        if (i != fRootNodeID) {
            int parentNodeNum, scanNodeNum;
            parentNodeNum = (level-1)->nodeID;
            if(doFWPlane)
                node->attachToParent((level-1)->node, gIOFireWirePlane);
            for (scanNodeNum = i + 1; scanNodeNum <= fRootNodeID; scanNodeNum++) {
                UInt8 scanSpeedCode;
                // Get speed code between parent and scan node.
                scanSpeedCode =
                        fSpeedCodes[(kFWMaxNodesPerBus + 1)*parentNodeNum + scanNodeNum];

                // Set speed map entry to minimum of scan speed and node's speed.
                if (speedCode < scanSpeedCode)
                        scanSpeedCode = speedCode;
                fSpeedCodes[(kFWMaxNodesPerBus + 1)*i + scanNodeNum] = scanSpeedCode;
                fSpeedCodes[(kFWMaxNodesPerBus + 1)*scanNodeNum + i] = scanSpeedCode;
            }
        }
        // Find next child port.
        if (i > 0) {
            while (level->childrenRemaining == 0) {
                // Go up one level in tree.
                level--;

                // One less child to scan.
                level->childrenRemaining--;
            }
            // Go down one level in tree.
            level++;
        }
    }


    if(doFWPlane) {
	IOLog("FireWire Speed map:\n");
        for(i=0; i <= fRootNodeID; i++) {
            int j;
            for(j=0; j <= fRootNodeID; j++) {
                IOLog("%d ", fSpeedCodes[(kFWMaxNodesPerBus + 1)*i + j]);
            }
            IOLog("\n");
        }
    }
    // Finally attach the full topology into the IOKit registry
    if(doFWPlane)
        root->attachToParent(IORegistryEntry::getRegistryRoot(), gIOFireWirePlane);
}

void IOFireWireController::pruneDevices()
{
    OSIterator *childIterator;
    childIterator = getClientIterator();
    if( childIterator) {
        IOFireWireDevice * found;
        while( (found = (IOFireWireDevice *) childIterator->getNextObject())) {
            if(found->fNodeID == kFWBadNodeID) {
                found->terminate();
            }
        }
        childIterator->release();
    }

    buildTopology(true);
}

////////////////////////////////////////////////////////////////////////////////
//
// processWriteRequest
//
//   process quad and block writes.
//
void IOFireWireController::processWriteRequest(UInt16 sourceID, UInt32 tLabel,
			UInt32 *hdr, void *buf, int len)
{
    UInt32 ret = kFWResponseAddressError;
    FWAddress addr((hdr[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, hdr[2]);
    IOFWAddressSpace * found;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        ret = found->doWrite(sourceID, addr, len, buf, false);
        if(ret != kFWResponseAddressError)
            break;
    }
    asyncWriteResponse(sourceID, FWSpeed(sourceID), tLabel, ret, addr.addressHi);
}

////////////////////////////////////////////////////////////////////////////////
//
// processLockRequest
//
//   process 32 and 64 bit locks.
//
void IOFireWireController::processLockRequest(UInt16 sourceID, UInt32 tLabel,
			UInt32 *hdr, void *buf, int len)
{
    UInt32 oldVal[2];
    UInt32 ret = kFWResponseAddressError;
    bool ok;
    int size;
    int i;
    int type = (hdr[3] &  kFWAsynchExtendedTCode) >> kFWAsynchExtendedTCodePhase;
    FWAddress addr((hdr[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, hdr[2]);
    IOFWAddressSpace * found;
    IOMemoryDescriptor *desc = NULL;
    IOByteCount offset;


    size = len/8;	// Depends on type, but right for 'compare and swap'

    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        ret = found->doRead(sourceID, addr, size*4, &desc, &offset, true);
        if(ret != kFWResponseAddressError)
            break;
    }
    if(NULL != desc) {
        desc->readBytes(offset,oldVal, size*4);
        switch (type) {
            case kFWExtendedTCodeCompareSwap:
                ok = true;
                for(i=0; i<size; i++)
                    ok = ok && oldVal[i] == ((UInt32 *)buf)[i];
                if(ok)
                    ret = found->doWrite(sourceID, addr, size*4, (UInt32 *)buf+size, true);
                break;

            default:
                ret = kFWResponseTypeError;
        }
        asyncLockResponse(sourceID, FWSpeed(sourceID), tLabel, ret, type, oldVal, size*4);
    }
    else {
        oldVal[0] = 0xdeadbabe;
        asyncLockResponse(sourceID, FWSpeed(sourceID), tLabel, ret, type, oldVal, 4);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// processRcvPacket
//
//   Dispatch received Async packet based on tCode.
//
void IOFireWireController::processRcvPacket(UInt32 *data, int size)
{
#if 0
    int i;
kprintf("Received packet 0x%x size %d\n", data, size);
    for(i=0; i<size; i++) {
	kprintf("0x%x ", data[i]);
    }
    kprintf("\n");
#endif
    UInt32	tCode, tLabel;
    UInt32	quad0;
    UInt16	sourceID;

    // Get first quad.
    quad0 = *data;

    tCode = (quad0 & kFWPacketTCode) >> kFWPacketTCodePhase;
    tLabel = (quad0 & kFWAsynchTLabel) >> kFWAsynchTLabelPhase;
    sourceID = (data[1] & kFWAsynchSourceID) >> kFWAsynchSourceIDPhase;

    // Dispatch processing based on tCode.
    switch (tCode)
    {
        case kFWTCodeWriteQuadlet :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("WriteQuadlet: addr 0x%x:0x%x\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            processWriteRequest(sourceID, tLabel, data, &data[3], 4);
            break;

        case kFWTCodeWriteBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("WriteBlock: addr 0x%x:0x%x\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            processWriteRequest(sourceID, tLabel, data, &data[4],
			(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase);
            break;

        case kFWTCodeWriteResponse :
            if(fTrans[tLabel].fInUse) {
                IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
		cmd->gotPacket(this,
			(data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase, 0, 0);
            }
            else {
#if (DEBUGGING_LEVEL > 0)
		DEBUGLOG("WriteResponse: label %d isn't in use!!\n", tLabel);
#endif
            }
            break;

        case kFWTCodeReadQuadlet :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("ReadQuadlet: addr 0x%x:0x%x\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            {
                // Rather inefficiently search for a matching address space
                UInt32 ret = kFWResponseAddressError;
                FWAddress addr((data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
                IOFWAddressSpace * found;
                IOMemoryDescriptor *buf = NULL;
		IOByteCount offset;
                fSpaceIterator->reset();
                while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
                    ret = found->doRead(sourceID, addr, 4, &buf, &offset, false);
                    if(ret != kFWResponseAddressError)
                        break;
                }
                if(NULL != buf) {
                    IOByteCount lengthOfSegment;
                    asyncReadQuadResponse(sourceID, FWSpeed(sourceID), tLabel, ret,
			*(UInt32 *)buf->getVirtualSegment(offset, &lengthOfSegment));
                }
                else {
                    asyncReadQuadResponse(sourceID, FWSpeed(sourceID),
                                       tLabel, ret, 0xdeadbeef);
                }
            }
            break;

        case kFWTCodeReadBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("ReadBlock: addr 0x%x:0x%x len %d\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2],
		(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase);
#endif
            {
                // Rather inefficiently search for a matching address space
                IOReturn ret = kFWResponseAddressError;
                int length = (data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase ;
                FWAddress addr((data[1] & kFWAsynchDestinationOffsetHigh) >> 
			kFWAsynchDestinationOffsetHighPhase, data[2]);
                IOFWAddressSpace * found;
                IOMemoryDescriptor *buf = NULL;
		IOByteCount offset;
                fSpaceIterator->reset();
                while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
                    ret = found->doRead(sourceID, addr, length, &buf, &offset, false);
                    if(ret != kFWResponseAddressError)
                        break;
                }
                if(NULL != buf) {
                    asyncReadResponse(sourceID, FWSpeed(sourceID),
                                       tLabel, ret, buf, offset, length);
                }
                else {
                    UInt32 bad = 0xcafebabe;
                    asyncReadResponse(sourceID, FWSpeed(sourceID),
                                       tLabel, ret, &bad, 4);
                }
            }
            break;

        case kFWTCodeReadQuadletResponse :
            if(fTrans[tLabel].fInUse) {
                IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
		cmd->gotPacket(this, (data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase,
									(UInt8*)(data+3), 4);
            }
            else {
#if (DEBUGGING_LEVEL > 0)
		DEBUGLOG("ReadQuadletResponse: label %d isn't in use!!\n", tLabel);
#endif
            }
            break;

        case kFWTCodeReadBlockResponse :
        case kFWTCodeLockResponse :
            if(fTrans[tLabel].fInUse) {
                IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
		cmd->gotPacket(this, (data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase,
			(UInt8*)(data+4), (data[3] & kFWAsynchDataLength)>>kFWAsynchDataLengthPhase);
            }
            else {
#if (DEBUGGING_LEVEL > 0)
		DEBUGLOG("ReadBlock/LockResponse: label %d isn't in use!!\n", tLabel);
#endif
            }
            break;

        case kFWTCodeLock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("Lock type %d: addr 0x%x:0x%x\n", 
		(data[3] & kFWAsynchExtendedTCode) >> kFWAsynchExtendedTCodePhase,
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase,
		data[2]);
#endif
            processLockRequest(sourceID, tLabel, data, &data[4],
			(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase);

            break;

        case kFWTCodeIsochronousBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("Async Stream Packet\n");
#endif
            break;

        default :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("Unexpected tcode in Asyncrecv: %d\n", tCode);
#endif
            break;
    }	
}

IOReturn IOFireWireController::allocAddress(IOFWAddressSpace *space)
{
    /*
     * Lots of scope for optimizations here, perhaps building a hash table for
     * addresses etc.
     * Drivers may want to override this if their hardware can match addresses
     * without CPU intervention.
     */
    if(!fLocalAddresses->setObject(space))
        return kIOReturnNoMemory;
    else
        return kIOReturnSuccess;
}

void IOFireWireController::freeAddress(IOFWAddressSpace *space)
{
    fLocalAddresses->removeObject(space);
}

void IOFireWireController::addAllocatedChannel(IOFWIsochChannel *channel)
{
    IOTakeLock(fChannelLock);
    fAllocatedChannels->setObject(channel);
    IOUnlock(fChannelLock);
}

void IOFireWireController::removeAllocatedChannel(IOFWIsochChannel *channel)
{
    IOTakeLock(fChannelLock);
    fAllocatedChannels->removeObject(channel);
    IOUnlock(fChannelLock);
}


IOFireWireDevice * IOFireWireController::nodeIDtoDevice(UInt16 nodeID)
{
    OSIterator *childIterator;
    IOFireWireDevice * found = NULL;

    childIterator = getClientIterator();

    if( childIterator) {
        while( (found = (IOFireWireDevice *) childIterator->getNextObject())) {
            if(found->fNodeID == nodeID)
		break;
        }
        childIterator->release();
    }
    return found;
}

IOFireWireDevice *IOFireWireController::createDeviceNub(OSDictionary *propTable)
{
    IOFireWireDevice *newDevice;
    newDevice = new IOFireWireDevice;

    if (!newDevice)
	return NULL;
    if(!newDevice->init(propTable)) {
	newDevice->release();
        return NULL;
    }
    return newDevice;
}

IOFWIsochChannel *IOFireWireController::createIsochChannel(
	bool doIRM, UInt32 bandwidth, IOFWSpeed prefSpeed,
	FWIsochChannelForceStopNotificationProc stopProc, void *stopRefCon)
{
    IOFWIsochChannel *channel;

    channel = new IOFWIsochChannel;
    if(!channel)
	return NULL;

    if(!channel->init(this, doIRM, bandwidth, prefSpeed, stopProc, stopRefCon)) {
	channel->release();
	channel = NULL;
    }
    return channel;
}

IOFWIsochPort *IOFireWireController::createLocalIsochPort(bool talking,
        DCLCommandStruct *opcodes, DCLTaskInfo *info,
	UInt32 startEvent, UInt32 startState, UInt32 startMask)
{
    IOFWLocalIsochPort *port;
    IODCLProgram *program;

    program = createDCLProgram(talking, opcodes, info, startEvent, startState, startMask);
    if(!program)
	return NULL;

    port = new IOFWLocalIsochPort;
    if(!port) {
	program->release();
	return NULL;
    }

    if(!port->init(program, this)) {
	port->release();
	port = NULL;
    }

    return port;
}

// How big (as a power of two) can packets sent to/received from the node be?
int IOFireWireController::maxPackLog(bool forSend, UInt16 nodeAddress) const
{
    return 9+FWSpeed(nodeAddress);
}

// How big (as a power of two) can packets sent from A to B be?
int IOFireWireController::maxPackLog(UInt16 nodeA, UInt16 nodeB) const
{
    return 9+FWSpeed(nodeA, nodeB);
}

