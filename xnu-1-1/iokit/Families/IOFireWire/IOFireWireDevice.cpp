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
 * 21 May 99 wgulland created.
 *
 */

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf
#include <IOKit/assert.h>

#include <IOKit/IOMessage.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireController.h>

#define super IOFireWireNub

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireDevice, IOFireWireNub)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFireWireDevice::attach(IOService *provider)
{
    assert(OSDynamicCast(IOFireWireController, provider));
    if( !super::attach(provider))
        return (false);
    fControl = (IOFireWireController *)provider;

    return(true);
}

void IOFireWireDevice::setNodeROM(UInt32 gen, UInt16 nodeID, UInt16 localID,
				OSData *rom, UInt32 *selfIDs, int numSelfIDs)
{
    OSObject *prop;
    OSArray *strings;
    IOMessage mess;
    int i;
    UInt32 newROMSize;
    const UInt32 *newROM;

    fNodeID = nodeID;
    fLocalNodeID = localID;
    fGeneration = gen;
    prop = OSNumber::withNumber(nodeID, 16);
    setProperty(gFireWireNodeID, prop);
    prop->release();

    // Notify clients of current state
    mess = kIOMessageServiceIsResumed;
    if(nodeID == 0xffff)
        mess = kIOMessageServiceIsSuspended;
    messageClients( mess );
    
    if(!rom)
        return;

    // Store selfIDs
    fNumSelfIDs = numSelfIDs;
    for(i=0; i<numSelfIDs; i++) {
        fSelfIDs[i] = selfIDs[i];
    }
    prop = OSData::withBytes(fSelfIDs, numSelfIDs*sizeof(UInt32));
    setProperty(gFireWireSelfIDs, prop);
    prop->release();

    // Process new ROM

    newROMSize = rom->getLength()/4;
    newROM = (UInt32 *)rom->getBytesNoCopy();

    if(newROMSize == fROMSize) {
        if(!bcmp(newROM, fROM, newROMSize*4))
            return;	// ROM unchanged
    }

    setProperty(gFireWireROM, rom);
    fROMSize = newROMSize;
    fROM = newROM;

    if(fROMSize > 3) {
        UInt32 vendorID = fROM[3] >> 8;
        prop = OSNumber::withNumber(vendorID, 32);
        setProperty(gFireWireVendor_ID, prop);
        prop->release();
    }
    if(fROMSize < 6)
	return;	// Done.

    strings = OSArray::withCapacity(3);
    // search for a unit directory - should create a nub for each one
    //zzz should search for nodes/modules if there are no unit directories.
    CSRROMEntryIterator		csrUnitIterator = kInvalidCSRROMIterator;
    CSRROMEntryID		csrROMEntryID = kInvalidCSRROMEntryID;
    CSRROMSearchCriteria	unitSearchCriteria;
    int				doneUnits;
    IOReturn			error;

    unitSearchCriteria.csrROMSearchType = kCSRROMSearchForKey;
    unitSearchCriteria.keyType = kCSRDirectoryKeyTypeBit;
    unitSearchCriteria.keyHi = kCSRUnitDirectoryKeyHiBit;
    unitSearchCriteria.keyLo = kCSRUnitDirectoryKeyLoBit;

    // Create CSR ROM search iterator.
    error = CSRROMCreateIterator (&csrUnitIterator);
    doneUnits = !(error == kIOReturnSuccess); // don't search if we couldn't create the iterator

    ///////////////////////////////
    // look for units

    while ( !doneUnits ) {
        // Search for a unit directory.
        error = FWCSRROMEntrySearch (csrUnitIterator,
                                     kIterateContinue,
                                     &csrROMEntryID,
                                     &doneUnits,
                                     &unitSearchCriteria,
                                     NULL,
                                     NULL);
        if(error != kIOReturnSuccess)
            doneUnits = true;
        if(doneUnits)
            break;

	CSRROMEntryIterator		csrROMIterator = NULL;
	CSRROMSearchCriteria		searchCriteria;
	UInt32				unitSpecID;
	UInt32				unitSoftwareVersion;
	UInt32				csrROMEntrySize;
        IOReturn			status, stat2;
        int				done;

	////////////////////////////////////////////////////////////////////////////
	//
	// Get unit spec ID.
	//

	// Create a CSR ROM iterator.
	status = CSRROMCreateIterator(&csrROMIterator);

	// Set it to unit directory.
	if (status == kIOReturnSuccess) {
            status = FWCSRROMSetIterator(csrROMIterator, csrROMEntryID, kIterateDescendants);
	}

	// Search for unit spec ID.
	//zzz what if none preset???
	if (status == kIOReturnSuccess) {
		searchCriteria.csrROMSearchType = kCSRROMSearchForKey;
		searchCriteria.keyType = kCSRImmediateKeyTypeBit;
		searchCriteria.keyHi = kCSRUnitSpecIdKeyHiBit;
		searchCriteria.keyLo = kCSRUnitSpecIdKeyLoBit;
		csrROMEntrySize = sizeof (UInt32);
		status =
			FWCSRROMEntrySearch
				(csrROMIterator,
				 kIterateContinue,
				 NULL,
				 &done,
				 &searchCriteria,
				 (UInt8 *) &unitSpecID,
				 &csrROMEntrySize);

		if ((status == kIOReturnSuccess) && (done))
			status = kIOReturnNoDevice;//zzz is this right?
	}

	// Clean up iterator.
	if (csrROMIterator != kInvalidCSRROMIterator) {
		FWCSRROMDisposeIterator (csrROMIterator);
		csrROMIterator = kInvalidCSRROMIterator;
	}

	////////////////////////////////////////////////////////////////////////////
	//
	// Get unit software version.
	//

	// Create a CSR ROM iterator.
	if (status == kIOReturnSuccess) {
		status = CSRROMCreateIterator(&csrROMIterator);
	}

	// Set it to unit directory.
	if (status == kIOReturnSuccess){
		status = FWCSRROMSetIterator
					(csrROMIterator, csrROMEntryID, kIterateDescendants);
	}

	// Search for the software version.
	if (status == kIOReturnSuccess) {
            searchCriteria.csrROMSearchType = kCSRROMSearchForKey;
            searchCriteria.keyType = kCSRImmediateKeyTypeBit;
            searchCriteria.keyHi = kCSRUnitSwVersionKeyHiBit;
            searchCriteria.keyLo = kCSRUnitSwVersionKeyLoBit;
            csrROMEntrySize = sizeof (UInt32);
            status =
                    FWCSRROMEntrySearch
                            (csrROMIterator,
                                  kIterateContinue,
                                  NULL,
                                  &done,
                                  &searchCriteria,
                                  (UInt8 *) &unitSoftwareVersion,
                                  &csrROMEntrySize);

            if ((status == kIOReturnSuccess) && (done))
                    status = kIOReturnNoDevice;//zzz is this right?
	}

	// Clean up iterator.
	if (csrROMIterator != kInvalidCSRROMIterator) {
		FWCSRROMDisposeIterator (csrROMIterator);
		csrROMIterator = kInvalidCSRROMIterator;
	}

	// Search for text strings.
        searchCriteria.csrROMSearchType = kCSRROMSearchForKey;
        searchCriteria.keyType = kCSRLeafKeyTypeBit;
        searchCriteria.keyHi = kCSRTextualDescriptorKeyHiBit;
        searchCriteria.keyLo = kCSRTextualDescriptorKeyLoBit;
        char text[128];
        done = false;
	// Create a CSR ROM iterator.
	stat2 = CSRROMCreateIterator(&csrROMIterator);
        while (stat2 == kIOReturnSuccess && !done) {
            unsigned int i;
            csrROMEntrySize = sizeof (text)-1;
            stat2 =
                        FWCSRROMEntrySearch
                                (csrROMIterator,
                                            kIterateDescendants,
                                            NULL,
                                            &done,
                                            &searchCriteria,
                                            (UInt8 *) text,
                                            &csrROMEntrySize);
                if(stat2 == kIOReturnSuccess && !done) {
                    // Skip leading zeros in text
                    for(i=0; i<csrROMEntrySize; i++)
                        if(text[i])
                            break;
                    text[csrROMEntrySize] = 0;
                    if(strings) {
                        OSString * astring = OSString::withCString(text+i);
                        if ( astring ) {
                            strings->setObject(astring);
                            astring->release();
                        }
                    }
                }
    
	}
	// Clean up iterator.
	if (csrROMIterator != kInvalidCSRROMIterator) {
		FWCSRROMDisposeIterator (csrROMIterator);
		csrROMIterator = kInvalidCSRROMIterator;
	}

	// Add entry to registry.
	if (status == kIOReturnSuccess) {
            OSDictionary * propTable = 0;
            IOFireWireUnit * newDevice = 0;
            do {
                propTable = OSDictionary::withCapacity(5);
                if (!propTable)
                    continue;
                /*
		 * Set the IOMatchCategory so that things that want to connect to
		 * the device still can even if it already has IOFireWireUnits attached
		 */
                prop = OSSymbol::withCString("FireWire Unit");
		propTable->setObject(gIOMatchCategoryKey, prop);
                prop->release();

                prop = OSNumber::withNumber(unitSpecID, 32);
                propTable->setObject(gFireWireUnit_Spec_ID, prop);
                prop->release();
                prop = OSNumber::withNumber(unitSoftwareVersion, 32);
                propTable->setObject(gFireWireUnit_SW_Version, prop);
                prop->release();

		// Copy over matching properties from Device
                prop = getProperty(gFireWireVendor_ID);
		if(prop)
                    propTable->setObject(gFireWireVendor_ID, prop);
                prop = getProperty(gFireWire_GUID);
                if(prop)
                    propTable->setObject(gFireWire_GUID, prop);

                newDevice = new IOFireWireUnit;

                if (!newDevice || !newDevice->init(propTable))
                    break;
                if (!newDevice->attach(this))
                    break;
                newDevice->registerService();
            } while (false);
            if(newDevice)
		newDevice->release();
            if(propTable)
		propTable->release();
	}
        if(strings) {
            setProperty("Description", strings);
            strings->release();
        }
    }
    // Clean up iterator and entryID 'object'
    if (csrUnitIterator != kInvalidCSRROMIterator) {
        FWCSRROMDisposeIterator (csrUnitIterator);
        csrUnitIterator = kInvalidCSRROMIterator;
    }
    if(csrROMEntryID != kInvalidCSRROMEntryID) {
        FWCSRROMDisposeEntryID(csrROMEntryID);
        csrROMEntryID = kInvalidCSRROMEntryID;
    }

}

/*
 * Create an iterator for the device ROM
 */
IOReturn IOFireWireDevice::CSRROMCreateIterator (CSRROMEntryIterator *pCSRROMIterator)
{
    CSRROMEntryIteratorRecPtr	pIteratorRec;
    UInt32			hdrSize;

    // Allocate memory for iterator record.
    pIteratorRec = (CSRROMEntryIteratorRecPtr)IOMalloc (sizeof (CSRROMEntryIteratorRec));
    if (pIteratorRec == NULL)
        return kIOReturnNoMemory;

    // Fill in other record fields.
    // Set relationship to descendants.
    pIteratorRec->relationship = kIterateDescendants;
    // Find start of root directory.
    hdrSize = ((fROM[0] & kCSRBusInfoBlockLength) >> kCSRBusInfoBlockLengthPhase) + 1;
    pIteratorRec->data = fROM;
    pIteratorRec->logicalPath[0] = 0;
    pIteratorRec->physicalPath[0] = hdrSize;

    pIteratorRec->logicalPath[1] = 0;
    pIteratorRec->physicalPath[1] = pIteratorRec->physicalPath[0] + 1;

    pIteratorRec->pathSize = 2;
    pIteratorRec->reset = true;

    // Return iterator.
    *pCSRROMIterator = (CSRROMEntryIterator) pIteratorRec;

    return (kIOReturnSuccess);
}

/**
 ** Matching methods
 **/
bool IOFireWireDevice::matchPropertyTable(OSDictionary * table)
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!super::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.

    return compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
}
