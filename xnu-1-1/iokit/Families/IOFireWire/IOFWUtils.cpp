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
 * Useful stuff called from several different FireWire objects.
 */
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/firewire/IOFWRegs.h>
#include <IOKit/firewire/IOFireWirePriv.h>

////////////////////////////////////////////////////////////////////////////////
//
// FWComputeCRC16
//
//   This proc computes a CRC 16 check.
//

UInt16	FWComputeCRC16(UInt32 *pQuads, UInt32 numQuads)
{
    SInt32	shift;
    UInt32	sum;
    UInt32	crc16;
    UInt32	quadNum;
    UInt32	quad;

    // Compute CRC 16 over all quads.
    crc16 = 0;
    for (quadNum = 0; quadNum < numQuads; quadNum++) {
        quad = *pQuads++;
        for (shift = 28; shift >= 0; shift -= 4) {
            sum = ((crc16 >> 12) ^ (quad >> shift)) & 0x0F;
            crc16 = (crc16 << 4) ^ (sum << 12) ^ (sum << 5) ^ (sum);
        }
    }

    return (crc16 & 0xFFFF);
}


////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMInvalidateEntryIDType
//
//   Invalidate a CSR ROM entry ID.  This will deallocate any memory allocated
// for the ID excluding the ID data record itself.
//
// Probably only thing needed is to release reference to the ROM.
//

void  FWCSRROMInvalidateEntryIDType(CSRROMEntryID csrROMEntryID)
{
    CSRROMEntryIDDataPtr		pCSRROMEntryIDData;

    if (csrROMEntryID != kInvalidCSRROMEntryID)
    {
        pCSRROMEntryIDData = (CSRROMEntryIDDataPtr) csrROMEntryID;

        switch (pCSRROMEntryIDData->entryType)
        {
                case kLocalCSRROMEntryIDType :
                        break;

                case kRemoteCSRROMEntryIDType :

                        break;

                default :
                        break;
        }

        // Entry is now invalid.
        pCSRROMEntryIDData->entryType = kInvalidCSRROMEntryIDType;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMCreateEntryID
//
//   Allocate memory for a CSR ROM entry ID.
//zzz could be a little more efficient computing allocation size.
//

CSRROMEntryID	FWCSRROMCreateEntryID(void)
{
    CSRROMEntryID		csrROMEntryID = kInvalidCSRROMEntryID;
    CSRROMEntryIDDataPtr	pCSRROMEntryIDData;
    UInt32			maxSize;

    // Determine maximum size of all entry types.
    // Need enough for local ID type.
    maxSize = sizeof (CSRROMLocalIDData);
    if (sizeof (CSRROMRemoteIDData) > maxSize)
        maxSize = sizeof (CSRROMRemoteIDData);

    // Allocate the data.
    pCSRROMEntryIDData = (CSRROMEntryIDDataPtr) IOMalloc (maxSize);
    if (pCSRROMEntryIDData != NULL) {
        pCSRROMEntryIDData->entryType = kInvalidCSRROMEntryIDType;
        csrROMEntryID = (CSRROMEntryID) pCSRROMEntryIDData;
    }
    return (csrROMEntryID);
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMDisposeEntryID
//
//   Deallocate memory for a CSR ROM entry ID.
//

void  FWCSRROMDisposeEntryID(CSRROMEntryID csrROMEntryID)
{
    CSRROMEntryIDDataPtr pCSRROMEntryIDData;

    if (csrROMEntryID != kInvalidCSRROMEntryID) {
        UInt32 maxSize;

        // Determine maximum size of all entry types.
        // Need enough for local ID type.
        maxSize = sizeof (CSRROMLocalIDData);
        if (sizeof (CSRROMRemoteIDData) > maxSize)
            maxSize = sizeof (CSRROMRemoteIDData);
        pCSRROMEntryIDData = (CSRROMEntryIDDataPtr) csrROMEntryID;

        // Invalidate entry.
        FWCSRROMInvalidateEntryIDType (csrROMEntryID);

        // Deallocate entry.
        IOFree(pCSRROMEntryIDData, maxSize);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMCreateRemoteEntryID
//
//   Create a remote CSR ROM entry ID.
//

static IOReturn	FWCSRROMCreateRemoteEntryID(
	CSRROMEntryID			*pCSRROMEntryID,
	const UInt32 *			data,
	UInt32				*physicalPath,
	UInt32				pathSize)
{
    CSRROMEntryID			csrROMEntryID = *pCSRROMEntryID;
    CSRROMEntryIDDataPtr		pCSRROMEntryIDData;
    CSRROMRemoteIDDataPtr		pCSRROMRemoteIDData;

    // Allocate an entry ID if given entry ID is invalid.
    if (csrROMEntryID == kInvalidCSRROMEntryID) {
        csrROMEntryID = FWCSRROMCreateEntryID ();
        // If entry ID is still invalid, there must have been a memory error.
        if (csrROMEntryID == kInvalidCSRROMEntryID)
            return kIOReturnNoMemory;
   }


    // Get data from ID.
    pCSRROMEntryIDData = (CSRROMEntryIDDataPtr) csrROMEntryID;
    pCSRROMRemoteIDData = (CSRROMRemoteIDDataPtr) pCSRROMEntryIDData;

    // If entry is not a remote type, invalidate its type.
    if ((pCSRROMEntryIDData->entryType != kRemoteCSRROMEntryIDType) &&
        (pCSRROMEntryIDData->entryType != kInvalidCSRROMEntryIDType)) {
        FWCSRROMInvalidateEntryIDType (csrROMEntryID);
    }

    // Make entry a remote type.
    pCSRROMEntryIDData->entryType = kRemoteCSRROMEntryIDType;


    // Copy directory paths and data pointer
    pCSRROMRemoteIDData->data = data;
    bcopy (physicalPath, pCSRROMRemoteIDData->physicalPath, pathSize * sizeof (UInt32));
    pCSRROMRemoteIDData->pathSize = pathSize;

    // Return entry ID.
    *pCSRROMEntryID = csrROMEntryID;

    return (kIOReturnSuccess);
}

////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMDisposeIterator
//
//   Dispose of storage allocated for iterator.
//

void FWCSRROMDisposeIterator(CSRROMEntryIterator csrROMIterator)
{
    CSRROMEntryIteratorRecPtr	pIteratorRec;

    // Deallocate if iterator is valid.
    if (csrROMIterator != kInvalidCSRROMIterator) {
        // Get iterator record.
        pIteratorRec = (CSRROMEntryIteratorRecPtr) csrROMIterator;

        // Deallocate iterator record.
        IOFree (pIteratorRec, sizeof (CSRROMEntryIteratorRec));
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMSetIterator
//
//   Set current path and relationship of iterator.
//zzz should support all entry ID types.
//zzz should support kIterateRoot
//

IOReturn FWCSRROMSetIterator(
	CSRROMEntryIterator			csrROMIterator,
	CSRROMEntryID				csrROMEntryID,
	CSRROMIterationOp			relationship)
{
    CSRROMEntryIteratorRecPtr	pIteratorRec;
    CSRROMEntryIDDataPtr		pCSRROMEntryIDData;
    CSRROMRemoteIDDataPtr		pCSRROMRemoteIDData;

    // Get CSR ROM search iterator record.
    pIteratorRec = (CSRROMEntryIteratorRecPtr) csrROMIterator;

    // Set to given entry.
    if (csrROMEntryID != kInvalidCSRROMEntryID) {
        // Get entry data from ID.
        pCSRROMEntryIDData = (CSRROMEntryIDDataPtr) csrROMEntryID;

        if (pCSRROMEntryIDData->entryType == kRemoteCSRROMEntryIDType) {
            // Recast entry data.
            pCSRROMRemoteIDData = (CSRROMRemoteIDDataPtr) pCSRROMEntryIDData;

            // Copy physical path.
            bcopy (pCSRROMRemoteIDData->physicalPath,
                                            pIteratorRec->physicalPath,
                                            pCSRROMRemoteIDData->pathSize * sizeof (UInt32));

            // Copy data pointer and path depth
            pIteratorRec->data = pCSRROMRemoteIDData->data;
            pIteratorRec->pathSize = pCSRROMRemoteIDData->pathSize;
        }
    }

    // Set to given relationship.
    if (relationship != kIterateContinue)
            pIteratorRec->relationship = relationship;

    return (kIOReturnSuccess);
}


////////////////////////////////////////////////////////////////////////////////
//
// FWCSRROMEntrySearch
//
//   Search for the next ROM entry that matches the search criteria.
//zzz need to check the search type in the criteria record and the relationship
//zzz could be more efficient.  should make and use more defs.
//zzz need to return information on what exactly was found
//zzz fill in entry value better
//zzz should break this up
//zzz should support all entry ID types.
//

// zzz
// Made a quick fix here so that if we are searching for Unit Directories (key d1)
// we will ignore any Instance Directories (key d8) because they contain redundant
// pointers to Unit Directories.  This is almost surely not the best soltution.

IOReturn FWCSRROMEntrySearch(
    CSRROMEntryIterator		csrROMIterator,
    CSRROMIterationOp		relationship,
    CSRROMEntryID		*pCSRROMEntryID,
    int			*pDone,
    CSRROMSearchCriteriaPtr	pSearchCriteria,
    UInt8 *			pEntryValue,
    UInt32			*pEntrySize)
{
    CSRROMEntryIteratorRecPtr	pIteratorRec;
    UInt32				currentAddress, entryValueAddress;
    UInt32				pathSize;
    UInt32				directoryEnd;
    UInt32				address;
    UInt32				directoryHeader, directoryEntry;
    UInt32				leafEntry;
    UInt32				keyType, keyValue;
    UInt32				keyTypeBit, keyValueBitHi, keyValueBitLo;
    UInt32				returnedEntrySize;
    bool				found, done;
    IOReturn			status = kIOReturnSuccess;

//zzz Part of the Instance Directory hack
    bool				unitDirSearch;
    bool				ignoreInstanceDir;

    // If we are searching for Unit Directories (in FWExpertLoader.c/FWUpdateDevice)
    // then just don't go down any Instance Directories (key d8), because we'll find a
    // redundant pointer to the Unit Directory in there.

    unitDirSearch = (pSearchCriteria->csrROMSearchType == kCSRROMSearchForKey) &&
            (pSearchCriteria->keyType == kCSRDirectoryKeyTypeBit) &&
            (pSearchCriteria->keyHi == kCSRUnitDirectoryKeyHiBit) &&
            (pSearchCriteria->keyLo == kCSRUnitDirectoryKeyLoBit);

    // Get CSR ROM search iterator record.
    pIteratorRec = (CSRROMEntryIteratorRecPtr) csrROMIterator;

    // Read the current directory length.  First, get base address of current
    // directory by going up one node in the directory path.  Special case
    // when we're in the root directory.  Second, read the directory length
    // at the base address.
    pathSize = pIteratorRec->pathSize;
    if (pathSize > 2) {
        address = pIteratorRec->physicalPath[pathSize - 2];
        directoryEntry = pIteratorRec->data[address];
        address += (directoryEntry & 0x00FFFFFF);
    }
    else {
        address = pIteratorRec->physicalPath[0];
    }
    directoryHeader = pIteratorRec->data[address];
    directoryEnd = address + ((directoryHeader >> 16) + 1);

    // Loop until we've found what we're looking for, or we've searched
    // everything.
    found = false;
    done = false;
    while ((!found) && (!done)) {
        // Read our current location.
        pathSize = pIteratorRec->pathSize;
        currentAddress = pIteratorRec->physicalPath[pathSize - 1];

        // Check if search was reset.
        if (pIteratorRec->reset) {
            pIteratorRec->reset = false;
        }
        else {
            // Read directory entry at current address.
            //zzz should have some address defs.
            directoryEntry = pIteratorRec->data[currentAddress];
            // If the current directory entry is a directory, add it to
            // the path.  Otherwise, go to next directory entry.
            keyType = (directoryEntry & 0xC0000000) >> 30;
//zzz
//The Instance Directory hack
            ignoreInstanceDir = (directoryEntry >> 24) == 0xd8;
//zzz
            if ((keyType == kCSRDirectoryKeyType) && !ignoreInstanceDir) {
                // Go to and read directory header.
                //zzz should do this the right way if offset > 1024.
                currentAddress += (directoryEntry & 0x00FFFFFF);
                directoryHeader = pIteratorRec->data[currentAddress];
                // Add directory to path and set location to first
                // entry in directory.
                pathSize++;

                directoryEnd =
                        currentAddress + ((directoryHeader >> 16) + 1);

                currentAddress += 1;
            }
            else {
                currentAddress += 1;
            }
        }

        // If we're past the end of the current directory, take it out
        // of path.
        while ((currentAddress >= directoryEnd) && (!done)) {
            pathSize--;
            currentAddress = pIteratorRec->physicalPath[pathSize - 1] + 1;

            // Read the current directory length.
            if (pathSize > 2) {
                address = pIteratorRec->physicalPath[pathSize - 2];
                directoryEntry = pIteratorRec->data[address];
                address += (directoryEntry & 0x00FFFFFF);
            }
            else {
                address = pIteratorRec->physicalPath[0];
            }
            directoryHeader = pIteratorRec->data[address];
            directoryEnd = address + ((directoryHeader >> 16) + 1);
            if (pathSize < 2) {
                done = true;
            }
        }
        // If we're not done searching, check the current directory entry
        // against the search criteria.
        if (!done) {
            directoryEntry = pIteratorRec->data[currentAddress];
            // Read key type and value.
            keyType = (directoryEntry & 0xC0000000) >> 30;
            keyValue = (directoryEntry & 0x3F000000) >> 24;

            // Convert to bit flag.
            keyTypeBit = 1 << keyType;
            if (keyValue > 31) {
                keyValueBitHi = 1 << (keyValue - 32);
                keyValueBitLo = 0;
            }
            else {
                keyValueBitHi = 0;
                keyValueBitLo = 1 << keyValue;
            }

            // Compare bit flags against search criteria.

            if ((keyTypeBit & pSearchCriteria->keyType) &&
                    ((keyValueBitHi & pSearchCriteria->keyHi) ||
                                (keyValueBitLo & pSearchCriteria->keyLo))) {
                switch (keyType) {
                    case kCSRImmediateKeyType :
                        if ((pEntrySize != NULL) && (pEntryValue != NULL)) {
                                // Determine size of data.
                                if (*pEntrySize < 4)
                                        returnedEntrySize = *pEntrySize;
                                else
                                        returnedEntrySize = 4;

                                // Copy data.//zzz not right.
                                //zzz need def here.
                                *((UInt32 *) pEntryValue) =
                                        directoryEntry & 0x00FFFFFF;

                                // Return size.
                                *pEntrySize = returnedEntrySize;
                        }
                        break;

                    case kCSROffsetKeyType :
                        if ((pEntrySize != NULL) && (pEntryValue != NULL)) {
                                // Determine size of data.
                                if (*pEntrySize < 4)
                                        returnedEntrySize = *pEntrySize;
                                else
                                        returnedEntrySize = 4;

                                // Copy data.//zzz not right.
                                //zzz need def here.
                                *((UInt32 *) pEntryValue) =
                                        directoryEntry & kCSREntryValue;

                                // Return size.
                                *pEntrySize = returnedEntrySize;
                        }
                        break;

                    case kCSRLeafKeyType :
                        if ((pEntrySize != NULL) && (pEntryValue != NULL)) {
                            // Get address of leaf.
                            //zzz need to do indirect stuff right.
                            entryValueAddress = currentAddress + (directoryEntry & kCSREntryValue);

                            // Get size of leaf.
                            leafEntry = pIteratorRec->data[entryValueAddress];
                            returnedEntrySize = (leafEntry >> 16) * sizeof (UInt32);
                            entryValueAddress += 1;
                            // Determine size of data.
                            if (returnedEntrySize > *pEntrySize)
                                returnedEntrySize = *pEntrySize;
                            // Copy data.
                            bcopy(&pIteratorRec->data[entryValueAddress], pEntryValue, returnedEntrySize);
                            // Return size.
                            *pEntrySize = returnedEntrySize;
                        }

                        break;

                    case kCSRDirectoryKeyType ://zzz
                        break;

                    default : //zzz
                        break;
                }

                found = true;
            }
        }

        // Update current address.
        pIteratorRec->physicalPath[pathSize - 1] = currentAddress;
        pIteratorRec->pathSize = pathSize;
    }

    // Return CSR ROM entry ID.
    if (pCSRROMEntryID != NULL) {
        if (!done) {
            status = FWCSRROMCreateRemoteEntryID
                                    (pCSRROMEntryID,
                                                pIteratorRec->data,
                                                pIteratorRec->physicalPath,
                                                pIteratorRec->pathSize);
        }
        else {
            if (*pCSRROMEntryID != kInvalidCSRROMEntryID)
                    FWCSRROMDisposeEntryID (*pCSRROMEntryID);
            *pCSRROMEntryID = kInvalidCSRROMEntryID;
        }
    }

    *pDone = done;
    return (status);
}


