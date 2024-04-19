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
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireController.h>

#define super IOFireWireNub

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireUnit, IOFireWireNub)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFireWireUnit::attach(IOService *provider)
{
    fDevice = OSDynamicCast(IOFireWireDevice, provider);
    if(!fDevice)
	return false;
    if( !super::attach(provider))
        return (false);
    fControl = fDevice->fControl;

    fROM = fDevice->fROM;
    fROMSize = fDevice->fROMSize;
    fNodeID = fDevice->fNodeID;
    fLocalNodeID = fDevice->fLocalNodeID;
    fGeneration = fDevice->fGeneration;

    return(true);
}

IOReturn IOFireWireUnit::message( UInt32 mess, IOService * provider,
                                void * argument )
{
    if(provider == fDevice) {
        fNodeID = fDevice->fNodeID;
        fLocalNodeID = fDevice->fLocalNodeID;
        fGeneration = fDevice->fGeneration;
	messageClients( mess );
    }
    return kIOReturnSuccess;
}

/*
 * Create an iterator for the device ROM
 */
IOReturn IOFireWireUnit::CSRROMCreateIterator (CSRROMEntryIterator *pCSRROMIterator)
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
bool IOFireWireUnit::matchPropertyTable(OSDictionary * table)
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

    bool res = compareProperty(table, gFireWireUnit_Spec_ID) &&
        compareProperty(table, gFireWireUnit_SW_Version) &&
        compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
    return res;
}
