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
 * 3 June 99 wgulland created.
 *
 * IOFireWireController methods to build the local device ROM.
 */

#include <assert.h>

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf

#include <IOKit/IOWorkLoop.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireDevice.h>

static void FWCSRROMDisposeLeafEntryData(CSRROMEntryDataPtr pCSRROMEntryData);
static void FWCSRROMDisposeEntryData(CSRROMEntryDataPtr pCSRROMEntryData);


////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMCreateDirectoryEntryData
//
//   Create a local directory entry data record.
//

static IOReturn	FWCSRROMCreateDirectoryEntryData(
	CSRROMDirectoryDataPtr		pParentCSRROMDirectoryData,
	CSRROMEntryDataPtr			*ppCSRROMEntryData,
	UInt32						entryKeyValue)
{
    CSRROMDirectoryDataPtr	pCSRROMDirectoryData;
    IOReturn			status = kIOReturnSuccess;

    // Allocate memory for entry data record.
    pCSRROMDirectoryData =
            (CSRROMDirectoryDataPtr) IOMalloc (sizeof (CSRROMDirectoryData));
    if (pCSRROMDirectoryData == NULL)
            status = kIOReturnNoMemory;

    // Fill in entry data record fields.
    if (status == kIOReturnSuccess) {
        // Copy parent's FWIM.
        //pCSRROMDirectoryData->csrROMEntryData.fwimID =
        //        pParentCSRROMDirectoryData->csrROMEntryData.fwimID;

        // Set entry type and key value.
        pCSRROMDirectoryData->csrROMEntryData.entryType = kDirectoryCSRROMEntryType;
        pCSRROMDirectoryData->csrROMBasicEntryData.keyValue = entryKeyValue;

        // Fill in entry data.
        pCSRROMDirectoryData->pChildCSRROMEntryData = NULL;
        pCSRROMDirectoryData->numChildren = 0;
    }

    // Return results.
    if (status == kIOReturnSuccess)
        *ppCSRROMEntryData = (CSRROMEntryDataPtr) pCSRROMDirectoryData;
    else
        *ppCSRROMEntryData = NULL;

    return (status);
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMCreateImmediateEntryData
//
//   Create a local immediate entry data record.
//

static IOReturn	FWCSRROMCreateImmediateEntryData(
	CSRROMDirectoryDataPtr		pParentCSRROMDirectoryData,
	CSRROMEntryDataPtr		*ppCSRROMEntryData,
	UInt32				entryKeyValue,
	UInt8 *				pEntryData)
{
	CSRROMImmediateDataPtr		pCSRROMImmediateData;
	IOReturn			status = kIOReturnSuccess;

	// Allocate memory for entry data record.
	pCSRROMImmediateData =
		(CSRROMImmediateDataPtr) IOMalloc (sizeof (CSRROMImmediateData));
	if (pCSRROMImmediateData == NULL)
            status = kIOReturnNoMemory;

	// Fill in entry data record fields.
	if (status == kIOReturnSuccess) {
            // Copy parent's FWIM.
            //pCSRROMImmediateData->csrROMEntryData.fwimID =
            //        pParentCSRROMDirectoryData->csrROMEntryData.fwimID;

            // Set entry type and key value.
            pCSRROMImmediateData->csrROMEntryData.entryType = kImmediateCSRROMEntryType;
            pCSRROMImmediateData->csrROMBasicEntryData.keyValue = entryKeyValue;

            // Fill in entry data.
            pCSRROMImmediateData->immediateData = (*((UInt32 *) pEntryData)) >> 8;
	}

	// Return results.
	if (status == kIOReturnSuccess)
            *ppCSRROMEntryData = (CSRROMEntryDataPtr) pCSRROMImmediateData;
	else
            *ppCSRROMEntryData = NULL;

	return (status);
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMCreateLeafEntryData
//
//   Create a local leaf entry data record.
//

static IOReturn	FWCSRROMCreateLeafEntryData(
	CSRROMDirectoryDataPtr		pParentCSRROMDirectoryData,
	CSRROMEntryDataPtr		*ppCSRROMEntryData,
	UInt32				entryKeyValue,
	UInt8 *				pEntryData,
	UInt32				entrySize)
{
    CSRROMLeafDataPtr	pCSRROMLeafData = NULL;
    UInt8 *		leafStorage;
    UInt32		leafDataSize;
    IOReturn		status = kIOReturnSuccess;

    // Allocate memory for entry data record.
    pCSRROMLeafData = (CSRROMLeafDataPtr) IOMalloc (sizeof (CSRROMLeafData));
    if (pCSRROMLeafData == NULL)
        status = kIOReturnNoMemory;

    // Allocate leaf storage.
    if (status == kIOReturnSuccess) {
        leafDataSize = (entrySize + 3) & (~0x03);
        leafStorage = (UInt8 *)IOMalloc (leafDataSize);
        if (leafStorage != NULL) {
            *((UInt32 *) (leafStorage + leafDataSize - 4)) = 0;
            pCSRROMLeafData->leafData = leafStorage;
            pCSRROMLeafData->leafDataSize = leafDataSize;
        }
        else
            status = kIOReturnNoMemory;
    }

    // Fill in entry data record fields.
    if (status == kIOReturnSuccess) {
        // Copy parent's FWIM.
        //pCSRROMLeafData->csrROMEntryData.fwimID =
        //        pParentCSRROMDirectoryData->csrROMEntryData.fwimID;

        // Set entry type and key value.
        pCSRROMLeafData->csrROMEntryData.entryType = kLeafCSRROMEntryType;
        pCSRROMLeafData->csrROMBasicEntryData.keyValue = entryKeyValue;

        // Fill in entry data.
        bcopy (pEntryData, pCSRROMLeafData->leafData, entrySize);
    }

    // Clean up on error.
    if (status != kIOReturnSuccess) {
        if (pCSRROMLeafData != NULL)
            FWCSRROMDisposeLeafEntryData ((CSRROMEntryDataPtr) pCSRROMLeafData);
    }

    // Return results.
    if (status == kIOReturnSuccess)
        *ppCSRROMEntryData = (CSRROMEntryDataPtr) pCSRROMLeafData;
    else
        *ppCSRROMEntryData = NULL;

    return (status);
}


////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMCreateOffsetEntryData
//
//   Create a local offset entry data record.
//

static IOReturn	FWCSRROMCreateOffsetEntryData(
	CSRROMDirectoryDataPtr	pParentCSRROMDirectoryData,
	CSRROMEntryDataPtr	*ppCSRROMEntryData,
	UInt32			entryKeyValue,
	FWAddressPtr		pFWAddress)
{
    CSRROMOffsetDataPtr	pCSRROMOffsetData;
    IOReturn		status = kIOReturnSuccess;

    // Check address range and alignment.
    if ((pFWAddress->addressHi != kCSRRegisterSpaceBaseAddressHi) ||
        (pFWAddress->addressLo < kCSRROMBaseAddress) ||
        (pFWAddress->addressLo >= 0xF4000000)) {
        status = kIOReturnBadArgument;
    }
    else if ((pFWAddress->addressLo & 0x03) != 0) {
        status = kIOReturnNotAligned;
    }

    // Allocate memory for entry data record.
    if (status == kIOReturnSuccess) {
        pCSRROMOffsetData =
                (CSRROMOffsetDataPtr) IOMalloc (sizeof (CSRROMOffsetData));
        if (pCSRROMOffsetData == NULL)
            status = kIOReturnNoMemory;
    }

    // Fill in entry data record fields.
    if (status == kIOReturnSuccess) {
        // Copy parent's FWIM.
        //pCSRROMOffsetData->csrROMEntryData.fwimID =
        //        pParentCSRROMDirectoryData->csrROMEntryData.fwimID;

        // Set entry type and key value.
        pCSRROMOffsetData->csrROMEntryData.entryType = kOffsetCSRROMEntryType;
        pCSRROMOffsetData->csrROMBasicEntryData.keyValue = entryKeyValue;

        // Fill in entry data.
        pCSRROMOffsetData->address = pFWAddress->addressLo;
    }

    // Return results.
    if (status == kIOReturnSuccess)
        *ppCSRROMEntryData = (CSRROMEntryDataPtr) pCSRROMOffsetData;
    else
        *ppCSRROMEntryData = NULL;

    return (status);
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMDisposeDirectoryEntryData
//
//   Dispose of a local directory entry data record.
//

static void FWCSRROMDisposeDirectoryEntryData(CSRROMEntryDataPtr pCSRROMEntryData)
{
    CSRROMDirectoryDataPtr		pCSRROMDirectoryData;
    CSRROMEntryDataPtr		pChildCSRROMEntryData, pNextCSRROMEntryData;
    UInt32				numChildren, childNum;

    if (pCSRROMEntryData != NULL) {
        // Recast entry data.
        pCSRROMDirectoryData = (CSRROMDirectoryDataPtr) pCSRROMEntryData;

        // Deallocate all children entry data records.
        pChildCSRROMEntryData = pCSRROMDirectoryData->pChildCSRROMEntryData;
        numChildren = pCSRROMDirectoryData->numChildren;
        for (childNum = 0; childNum < numChildren; childNum++) {
            // Get next child entry data record.
            pNextCSRROMEntryData = pChildCSRROMEntryData->pNextCSRROMEntryData;

            // Deallocate current child entry data record.
            FWCSRROMDisposeEntryData (pChildCSRROMEntryData);

            // Go to next child entry data.
            pChildCSRROMEntryData = pNextCSRROMEntryData;
        }

        // Deallocate memory for entry data record.
        IOFree(pCSRROMEntryData, sizeof(CSRROMDirectoryData));
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMDisposeImmediateEntryData
//
//   Dispose of a local immediate entry data record.
//

static void FWCSRROMDisposeImmediateEntryData(CSRROMEntryDataPtr pCSRROMEntryData)
{
    // Deallocate memory for entry data record.
    if (pCSRROMEntryData != NULL)
        IOFree (pCSRROMEntryData, sizeof(CSRROMImmediateData));
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMDisposeLeafEntryData
//
//   Dispose of a local leaf entry data record.
//

static void FWCSRROMDisposeLeafEntryData(CSRROMEntryDataPtr pCSRROMEntryData)
{
    CSRROMLeafDataPtr			pCSRROMLeafData;
    if (pCSRROMEntryData != NULL) {
        // Recast entry data.
        pCSRROMLeafData = (CSRROMLeafDataPtr) pCSRROMEntryData;

        // Deallocate leaf data storage.
        if (pCSRROMLeafData->leafData != NULL)
            IOFree(pCSRROMLeafData->leafData, pCSRROMLeafData->leafDataSize);

        // Deallocate memory for entry data record.
        IOFree(pCSRROMEntryData, sizeof(CSRROMLeafData));
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMDisposeOffsetEntryData
//
//   Dispose of a local offset entry data record.
//

static void FWCSRROMDisposeOffsetEntryData(CSRROMEntryDataPtr pCSRROMEntryData)
{
    // Deallocate memory for entry data record.
    if (pCSRROMEntryData != NULL)
        IOFree (pCSRROMEntryData, sizeof(CSRROMOffsetData));
}


////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMDisposeEntryData
//
//   Dispose of a local entry data record.
//

static void FWCSRROMDisposeEntryData(CSRROMEntryDataPtr pCSRROMEntryData)
{
    if (pCSRROMEntryData != NULL) {
        switch (pCSRROMEntryData->entryType) {
        case kImmediateCSRROMEntryType :
            FWCSRROMDisposeImmediateEntryData (pCSRROMEntryData);
            break;

        case kOffsetCSRROMEntryType :
            FWCSRROMDisposeOffsetEntryData (pCSRROMEntryData);
            break;

        case kLeafCSRROMEntryType :
            FWCSRROMDisposeLeafEntryData (pCSRROMEntryData);
            break;

        case kDirectoryCSRROMEntryType :
            FWCSRROMDisposeDirectoryEntryData (pCSRROMEntryData);
            break;

        default :
                  break;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMInsertEntryData
//
//   This routine inserts a CSR ROM entry data record into the local CSR ROM
// tree.
//

static void FWCSRROMInsertEntryData(
	CSRROMDirectoryDataPtr		pParentCSRROMDirectoryData,
	CSRROMEntryDataPtr			pCSRROMEntryData)
{
    CSRROMEntryDataPtr	pFirstCSRROMEntryData, pPrevCSRROMEntryData, pNextCSRROMEntryData;

    // Get the first entry data record in the parent directory's list.
    pFirstCSRROMEntryData = pParentCSRROMDirectoryData->pChildCSRROMEntryData;

    // Insert entry into list.
    if (pFirstCSRROMEntryData != NULL) {
        pPrevCSRROMEntryData = pFirstCSRROMEntryData->pPrevCSRROMEntryData;
        pNextCSRROMEntryData = pFirstCSRROMEntryData;

        pPrevCSRROMEntryData->pNextCSRROMEntryData = pCSRROMEntryData;
        pCSRROMEntryData->pPrevCSRROMEntryData = pPrevCSRROMEntryData;

        pNextCSRROMEntryData->pPrevCSRROMEntryData = pCSRROMEntryData;
        pCSRROMEntryData->pNextCSRROMEntryData = pNextCSRROMEntryData;
    }
    else {
        pParentCSRROMDirectoryData->pChildCSRROMEntryData = pCSRROMEntryData;

        pCSRROMEntryData->pPrevCSRROMEntryData = pCSRROMEntryData;
        pCSRROMEntryData->pNextCSRROMEntryData = pCSRROMEntryData;
    }

    pCSRROMEntryData->pParentCSRROMEntryData = (CSRROMEntryDataPtr) pParentCSRROMDirectoryData;
    pParentCSRROMDirectoryData->numChildren++;
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMEntrySize
//
// Calculate size of entry (including children)
//

static int FWCSRROMEntrySize(CSRROMEntryDataPtr	pCSRROMEntryData)
{
    CSRROMEntryDataPtr		pChildCSRROMEntryData;
    CSRROMLeafDataPtr		pCSRROMLeafData;
    CSRROMDirectoryDataPtr	pCSRROMDirectoryData;
    UInt32			numChildren;
    UInt32			i;
    int				size = 0;

    // Compute entry key type, value, and data.
    switch (pCSRROMEntryData->entryType) {
        case kImmediateCSRROMEntryType :
            break;

        case kOffsetCSRROMEntryType :
            break;

        case kLeafCSRROMEntryType :
            pCSRROMLeafData = (CSRROMLeafDataPtr) pCSRROMEntryData;
            size = (pCSRROMLeafData->leafDataSize >> 2) + 1;
            break;

        case kDirectoryCSRROMEntryType :
            pCSRROMDirectoryData = (CSRROMDirectoryDataPtr) pCSRROMEntryData;
            numChildren = pCSRROMDirectoryData->numChildren;
            size = 1;

            pChildCSRROMEntryData = pCSRROMDirectoryData->pChildCSRROMEntryData;
            size += numChildren;
            for (i = 0; i < numChildren; i++) {
                size += FWCSRROMEntrySize(pChildCSRROMEntryData);
                pChildCSRROMEntryData = pChildCSRROMEntryData->pNextCSRROMEntryData;
            }

            break;

        default :
            //zzz should we do anything on error?
            break;
    }

    return (size);
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMInstantiateEntry
//
//   Instantiate and entry in the local CSR ROM.
//

static void FWCSRROMInstantiateEntry(CSRROMEntryDataPtr pCSRROMEntryData,
	UInt32	**ppCSRROMStorage, UInt32 *pEntryLocation)
{
    CSRROMEntryDataPtr		pChildCSRROMEntryData;
    CSRROMImmediateDataPtr	pCSRROMImmediateData;
    CSRROMOffsetDataPtr		pCSRROMOffsetData;
    CSRROMLeafDataPtr		pCSRROMLeafData;
    CSRROMDirectoryDataPtr	pCSRROMDirectoryData;
    UInt32			*pCSRROMStorage = *ppCSRROMStorage;
    UInt32			*pCSRROMDirectoryStorage;
    UInt32			*pHeader;
    UInt32			crc16;
    UInt32			entryOffset;
    UInt32			entryValue;
    UInt32			entryKeyType, entryKeyValue;
    UInt32			numChildren;
    UInt32			i;

    // Compute entry key type, value, and data.
    switch (pCSRROMEntryData->entryType) {
        case kImmediateCSRROMEntryType :
            pCSRROMImmediateData = (CSRROMImmediateDataPtr) pCSRROMEntryData;
            entryKeyType = kCSRImmediateKeyType;
            entryKeyValue = pCSRROMImmediateData->csrROMBasicEntryData.keyValue;
            entryValue = pCSRROMImmediateData->immediateData;
            *pEntryLocation =
                    ((entryKeyType << kCSREntryKeyTypePhase) & kCSREntryKeyType) |
                    ((entryKeyValue << kCSREntryKeyValuePhase) & kCSREntryKeyValue) |
                    ((entryValue << kCSREntryValuePhase) & kCSREntryValue);
            break;

        case kOffsetCSRROMEntryType :
            pCSRROMOffsetData = (CSRROMOffsetDataPtr) pCSRROMEntryData;
            entryKeyType = kCSROffsetKeyType;
            entryKeyValue = pCSRROMOffsetData->csrROMBasicEntryData.keyValue;
            entryOffset =
                    (pCSRROMOffsetData->address - kCSRRegisterSpaceBaseAddressLo) >> 2;
            *pEntryLocation =
                    ((entryKeyType << kCSREntryKeyTypePhase) & kCSREntryKeyType) |
                    ((entryKeyValue << kCSREntryKeyValuePhase) & kCSREntryKeyValue) |
                    ((entryOffset << kCSREntryValuePhase) & kCSREntryValue);
            break;

        case kLeafCSRROMEntryType :
            pCSRROMLeafData = (CSRROMLeafDataPtr) pCSRROMEntryData;
            entryKeyType = kCSRLeafKeyType;
            entryKeyValue = pCSRROMLeafData->csrROMBasicEntryData.keyValue;
            entryOffset = pCSRROMStorage - pEntryLocation;
            *pEntryLocation =
                    ((entryKeyType << kCSREntryKeyTypePhase) & kCSREntryKeyType) |
                    ((entryKeyValue << kCSREntryKeyValuePhase) & kCSREntryKeyValue) |
                    ((entryOffset << kCSREntryValuePhase) & kCSREntryValue);

            crc16 = FWComputeCRC16 ((UInt32 *) pCSRROMLeafData->leafData,
                                                            pCSRROMLeafData->leafDataSize/4);

            *pCSRROMStorage++ =
                    (((pCSRROMLeafData->leafDataSize >> 2) << kCSRLeafDirLengthPhase) &
                                kCSRLeafDirLength) |
                    ((crc16 << kCSRLeafDirCRCPhase) & kCSRLeafDirCRC);

            bcopy(pCSRROMLeafData->leafData, pCSRROMStorage, pCSRROMLeafData->leafDataSize);
            pCSRROMStorage =
                    (UInt32 *) ((UInt8 *) pCSRROMStorage + pCSRROMLeafData->leafDataSize);
            break;

        case kDirectoryCSRROMEntryType :
            pCSRROMDirectoryData = (CSRROMDirectoryDataPtr) pCSRROMEntryData;
            entryKeyType = kCSRDirectoryKeyType;
            entryKeyValue = pCSRROMDirectoryData->csrROMBasicEntryData.keyValue;
            entryOffset = pCSRROMStorage - pEntryLocation;
            if (pEntryLocation != NULL) {
                    *pEntryLocation =
                            ((entryKeyType << kCSREntryKeyTypePhase) & kCSREntryKeyType) |
                            ((entryKeyValue << kCSREntryKeyValuePhase) & kCSREntryKeyValue) |
                            ((entryOffset << kCSREntryValuePhase) & kCSREntryValue);
            }

            numChildren = pCSRROMDirectoryData->numChildren;
            pHeader = pCSRROMStorage;
            pCSRROMStorage++;

            pChildCSRROMEntryData = pCSRROMDirectoryData->pChildCSRROMEntryData;
            pCSRROMDirectoryStorage = pCSRROMStorage;
            pCSRROMStorage += numChildren;
            for (i = 0; i < numChildren; i++) {
                FWCSRROMInstantiateEntry (pChildCSRROMEntryData, &pCSRROMStorage, pCSRROMDirectoryStorage++);
                pChildCSRROMEntryData = pChildCSRROMEntryData->pNextCSRROMEntryData;
            }

            crc16 = FWComputeCRC16 (pHeader + 1, numChildren);
            *pHeader =
                    ((numChildren << kCSRLeafDirLengthPhase) & kCSRLeafDirLength) |
                    ((crc16 << kCSRLeafDirCRCPhase) & kCSRLeafDirCRC);

            break;

        default :
            //zzz should we do anything on error?
            break;
    }

    // Update ROM storage.
    *ppCSRROMStorage = pCSRROMStorage;
}

////////////////////////////////////////////////////////////////////////////////
//
// CSRROMGetRootDirectory
//
//   Return the Entry ID of the root directory for our ROM.
//

IOReturn IOFireWireController::CSRROMGetRootDirectory(CSRROMEntryID *pCSRROMEntryID)
{
    CSRROMEntryID				csrROMEntryID;
    CSRROMEntryIDDataPtr		pCSRROMEntryIDData;
    CSRROMLocalIDDataPtr		pCSRROMLocalIDData;
    IOReturn			status = kIOReturnSuccess;

    // Get the output entry ID and allocate if invalid.
    csrROMEntryID = *pCSRROMEntryID;
    if (csrROMEntryID == kInvalidCSRROMEntryID) {
        csrROMEntryID = FWCSRROMCreateEntryID ();
        if (csrROMEntryID == kInvalidCSRROMEntryID)
                status = kIOReturnNoMemory;
    }

    // Set to root directory.
    if (status == kIOReturnSuccess) {
        pCSRROMEntryIDData = (CSRROMEntryIDDataPtr) csrROMEntryID;
        // Invalidate output entry ID type if it's not local.
        if ((pCSRROMEntryIDData->entryType != kInvalidCSRROMEntryIDType) &&
                (pCSRROMEntryIDData->entryType != kLocalCSRROMEntryIDType)) {
            FWCSRROMInvalidateEntryIDType (csrROMEntryID);
        }

        // Recast entry ID data.
        pCSRROMLocalIDData = (CSRROMLocalIDDataPtr) pCSRROMEntryIDData;

        // Make entry ID reference local root directory.
        pCSRROMLocalIDData->csrROMEntryIDData.entryType = kLocalCSRROMEntryIDType;
        pCSRROMLocalIDData->pCSRROMEntryData =
                (CSRROMEntryDataPtr) &fCSRROMRootDirectory;
        *pCSRROMEntryID = csrROMEntryID;
    }
    return (status);
}

////////////////////////////////////////////////////////////////////////////////
//
// CSRROMCreateEntry
//
//   Create an entry in the local CSR Configuration ROM.
//

IOReturn IOFireWireController::CSRROMCreateEntry(
	CSRROMEntryID parentCSRROMEntryID, CSRROMEntryID *pCSRROMEntryID,
	UInt32 entryType, UInt32 entryKeyValue, void *entryData, UInt32 entrySize)
{
    CSRROMEntryID		csrROMEntryID;
    CSRROMEntryIDDataPtr	pCSRROMEntryIDData;
    CSRROMLocalIDDataPtr	pParentCSRROMLocalIDData;
    CSRROMLocalIDDataPtr	pCSRROMLocalIDData;
    CSRROMDirectoryDataPtr	pParentCSRROMDirectoryData;
    CSRROMEntryDataPtr		pNewCSRROMEntryData = NULL;
    bool			csrROMEntryIDAllocated = false;
    IOReturn			status = kIOReturnSuccess;

    // Get and validate parent directory.
    pParentCSRROMLocalIDData = (CSRROMLocalIDDataPtr) parentCSRROMEntryID;
    if (pParentCSRROMLocalIDData->csrROMEntryIDData.entryType == kLocalCSRROMEntryIDType) {
        pParentCSRROMDirectoryData =
                (CSRROMDirectoryDataPtr) pParentCSRROMLocalIDData->pCSRROMEntryData;
        if (pParentCSRROMDirectoryData->csrROMEntryData.entryType != kDirectoryCSRROMEntryType)
            status = kIOReturnBadArgument;
    }
    else {
        status = kIOReturnBadArgument;
    }

    // Get the output entry ID and allocate if invalid.
    if (status == kIOReturnSuccess) {
        csrROMEntryID = *pCSRROMEntryID;
        if (csrROMEntryID == kInvalidCSRROMEntryID) {
            csrROMEntryID = FWCSRROMCreateEntryID ();
            if (csrROMEntryID != kInvalidCSRROMEntryID)
                csrROMEntryIDAllocated = true;
            else
                status = kIOReturnNoMemory;
        }

        if (csrROMEntryID != kInvalidCSRROMEntryID)
            pCSRROMEntryIDData = (CSRROMEntryIDDataPtr) csrROMEntryID;
    }

    // If output entry ID is not local, invalidate it.
    if (status == kIOReturnSuccess) {
        if ((pCSRROMEntryIDData->entryType != kInvalidCSRROMEntryIDType) &&
                (pCSRROMEntryIDData->entryType != kLocalCSRROMEntryIDType)) {
            FWCSRROMInvalidateEntryIDType (csrROMEntryID);
        }
    }

    // Create the requested type of entry data record.
    if (status == kIOReturnSuccess) {
        switch (entryType) {
            case kImmediateCSRROMEntryType :
                status = FWCSRROMCreateImmediateEntryData (pParentCSRROMDirectoryData,
                                                        &pNewCSRROMEntryData,
                                                        entryKeyValue,
                                                        (UInt8 *) entryData);
                break;

            case kOffsetCSRROMEntryType :
                status = FWCSRROMCreateOffsetEntryData (pParentCSRROMDirectoryData,
                                                        &pNewCSRROMEntryData,
                                                        entryKeyValue,
                                                        (FWAddressPtr) entryData);
                break;

            case kLeafCSRROMEntryType :
                status = FWCSRROMCreateLeafEntryData (pParentCSRROMDirectoryData,
                                                        &pNewCSRROMEntryData,
                                                        entryKeyValue,
                                                        (UInt8 *) entryData,
                                                        entrySize);
                break;

            case kDirectoryCSRROMEntryType :
                status = FWCSRROMCreateDirectoryEntryData (pParentCSRROMDirectoryData,
                                                            &pNewCSRROMEntryData,
                                                            entryKeyValue);
                break;

            default :
                status = kIOReturnBadArgument;
                break;
        }
    }

    // Insert new entry into CSR Configuration ROM.
    if (status == kIOReturnSuccess)
        FWCSRROMInsertEntryData (pParentCSRROMDirectoryData, pNewCSRROMEntryData);

    // Set ID to reference new entry.
    if (status == kIOReturnSuccess) {
        pCSRROMLocalIDData = (CSRROMLocalIDDataPtr) pCSRROMEntryIDData;
        pCSRROMLocalIDData->csrROMEntryIDData.entryType = kLocalCSRROMEntryIDType;
        pCSRROMLocalIDData->pCSRROMEntryData = pNewCSRROMEntryData;
    }

    // Clean up on error.
    if (status != kIOReturnSuccess) {
        if (pNewCSRROMEntryData != NULL)
            FWCSRROMDisposeEntryData (pNewCSRROMEntryData);

        if (csrROMEntryIDAllocated)
            FWCSRROMDisposeEntryID (csrROMEntryID);
    }

    // Return results.
    if (status == kIOReturnSuccess)
        *pCSRROMEntryID = csrROMEntryID;

    return (status);
}

////////////////////////////////////////////////////////////////////////////////
//
// UpdateROM()
//
//   Instantiate the local CSR ROM.
//   Always causes at least one bus reset.
//

IOReturn IOFireWireController::UpdateROM()
{
    CSRROMDirectoryDataPtr	pCSRROMDirectoryData;
    CSRROMImmediateDataPtr	pCSRROMImmediateData;
    UInt32			*pCSRROMStorage;
    UInt32			busInfoBlockSize;
    UInt32			*pHeader;
    UInt32			romSize;
    UInt32			crc16;
    OSData *			rom;
    IOReturn			ret;

    // Update CSR ROM generation.
    //zzz assumes first entry is generation value.
    pCSRROMDirectoryData = &fCSRROMRootDirectory;
    pCSRROMImmediateData =
            (CSRROMImmediateDataPtr) pCSRROMDirectoryData->pChildCSRROMEntryData;
    if (pCSRROMImmediateData->csrROMBasicEntryData.keyValue == kCSRGenerationKey)
            pCSRROMImmediateData->immediateData++;

    busInfoBlockSize = (fROMHeader[0] & kCSRBusInfoBlockLength) >>
                                                    kCSRBusInfoBlockLengthPhase;
    romSize = FWCSRROMEntrySize((CSRROMEntryDataPtr) pCSRROMDirectoryData) + 
	busInfoBlockSize;
kprintf("Rom size: %d\n", romSize);
    rom = OSData::withCapacity((romSize+1)*4);
    if(!rom)
	return kIOReturnNoMemory;
    pHeader = (UInt32 *)IOMalloc((romSize+1)*4);
    if(!pHeader) {
        rom->release();
	return kIOReturnNoMemory;
    }

    // Recursively build configuration ROM.
    bcopy(fROMHeader, pHeader, 4*(busInfoBlockSize+1));
    pCSRROMStorage = pHeader + busInfoBlockSize + 1;
    FWCSRROMInstantiateEntry((CSRROMEntryDataPtr) pCSRROMDirectoryData,
                                        &pCSRROMStorage, NULL);

    // Reconstruct Bus Info Block header.

    crc16 = FWComputeCRC16 (pHeader + 1, romSize);
    *pHeader =
            ((busInfoBlockSize << kCSRBusInfoBlockLengthPhase) & kCSRBusInfoBlockLength) |
            ((romSize << kCSRROMCRCLengthPhase) & kCSRROMCRCLength) |
            ((crc16 << kCSRROMCRCValuePhase) & kCSRROMCRCValue);
    rom->appendBytes(pHeader, (romSize+1)*4);
    IOFree(pHeader, (romSize+1)*4);
    setProperty(gFireWireROM, rom);

        // If we have a FWIM that is fully FSL 1.1 compatible, use the set CSR ROM function.
        // Otherwise just reset the bus.
#if 0
                // Tell the FWIM to use this new CSR ROM data (and reset the bus)
                //zzz Should support other (larger) ROM sizes
                status = FWSetCSRROM (pHeader, (romSize+1)*4);
#endif
kprintf("old romspace: 0x%x\n", fROMAddrSpace);
    if(fROMAddrSpace) {
        freeAddress(fROMAddrSpace);
        fROMAddrSpace->release();
        fROMAddrSpace = NULL;
    }
    fROMAddrSpace = IOFWPseudoAddressSpace::simpleRead(
		FWAddress(kCSRRegisterSpaceBaseAddressHi, kCSRROMBaseAddress),
		 (romSize+1)*4, rom->getBytesNoCopy());
    ret = allocAddress(fROMAddrSpace);

    return (ret);
}

