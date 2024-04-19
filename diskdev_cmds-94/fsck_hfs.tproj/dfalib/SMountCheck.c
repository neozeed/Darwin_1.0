/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
	File:		SMountCheck.c

	Contains:	Consistency checking code for HFS and HFS Plus volumes.

	Version:	HFS Plus 1.0

	Copyright:	© 1995-1999 by Apple Computer, Inc., all rights reserved.

*/

//#include <Errors.h>
//#include <Files.h>
//#include <MacTypes.h>

#include "BTree.h"
#include "Scavenger.h"


enum {		
	// volume locked (software or hardware)
	kVCBAttrLocked	= (kHFSVolumeHardwareLockMask + kHFSVolumeSoftwareLockMask)
};


/* old B-tree constants */
enum {
	kBTreePrevious		= -1,
	kBTreeCurrent		= 0,
	kBTreeNext		= 1
};

enum {
#if 0
	kHFSOrphanedExtents				= 0x00000001,
	kHFSOverlapingExtents			= 0x00000002,
	kHFSCatalogBTreeLoop			= 0x00000004,
	kHFSExtentsBTreeLoop			= 0x00000008,
	kHFSOrphanedThreadRecords		= 0x00000010,
	kHFSMissingThreadRecords		= 0x00000020,
	kHFSInvalidPEOF					= 0x00000040,
	kHFSInvalidLEOF					= 0x00000080,
	kHFSInvalidValence				= 0x00000100,
	kHFSInvalidBTreeHeader			= 0x00000200,
	kHFSInvalidCatalogRecordType	= 0x00000400,	
	kHFSInconsistentVolumeName		= 0x00000800,
#endif
	kHFSInvalidExtentRecord			= 0x00001000

//	kHFSMinorRepairsWereMade		= 0x80000000
};

enum {
	kMinimumBtreeNodesForCache		= 100
};

#define	kMDBValidAttributesMask		0x8780

#define	badCatalogErr	-1311


/* Macros */

#define		GetBTCB(fcb)	((BTreeControlBlock*) (fcb)->fcbBtree)


Boolean HFSEqualString(ConstStr255Param str1, ConstStr255Param str2);
#define		HFSEqualString(s1, s2)			CaseAndMarkSensitiveEqualString((s1), (s2), (s1)[0], (s2)[0])



// local prototypes

static OSErr	CheckCatalog (SVCB* volume, UInt32* freeBlocks, UInt32 xCatalogBlocks, UInt32* consistencyStatus );

static OSErr	CheckExtentsOverflow ( SVCB* volume, UInt32 *freeBlocks, UInt32 *xCatalogBlocks, UInt32* consistencyStatus );

static OSErr	CheckAttributes ( SVCB *volume, UInt32 *freeBlocks, UInt32 *consistencyStatus );

static OSErr	CheckBitmapAndHeader ( SVCB *volume, UInt32 *freeBlocks, UInt32 *consistencyStatus );

static void	VolumeBitMapCheckExtents ( SVCB* volume, HFSPlusExtentDescriptor* theExtents, UInt32* consistencyStatus );

static OSErr	CheckCreateDate (SVCB* volume, UInt32* consistencyStatus );

static Boolean	ValidPEOF ( SVCB* volume, CatalogRecord *file );

// static OSErr	FixOrphanFileThreads ( SVCB* volume, UInt32 maxRecords, UInt32 *repairsDone);

static UInt32	CountExtentBlocks (HFSPlusExtentRecord extents, Boolean hfsPlus);

static OSErr	GetFileExtentRecord( const SVCB	*vcb, SFCB *fcb, HFSPlusExtentRecord extents);



/*-------------------------------------------------------------------------------

Routine:	MountCheck

Function:	Do a quick check of the volume meta-data to make sure that
			it is consistent. Minor inconsistencies in the MDB or volume
			bitmap will be repaired all other inconsistencies will be
			detected and reported in the consistencyStatus output.
			
			This check will terminated if any unexpected BTree errors
			are encountered.

Input:		volume				- pointer to volume control block
			consistencyStatus	- ????

Output:		none

Result:		noErr				- success

-------------------------------------------------------------------------------*/
OSErr
MountCheck( SVCB* volume, UInt32 *consistencyStatus )
{
	OSErr					result;
	UInt32					freeBlocks;
	UInt32					bitmapFreeBlocks;
	UInt32					xCatalogBlocks;

//	printf("# MountCheck\n");

	*consistencyStatus = 0;
	freeBlocks = volume->vcbTotalBlocks;	// initially all blocks are free

	if (volume->vcbSignature == kHFSPlusSigWord)
	{
		//--- make sure the volume header's create date matches the MDB's create date
		result = CheckCreateDate (volume, consistencyStatus);
		M_ExitOnError(result);
	}

	//--- make sure the allocation file, volume header, and alternate volume header are allocated
	result = CheckBitmapAndHeader ( volume, &freeBlocks, consistencyStatus );
	M_ExitOnError(result);

	//--- do a quick consistency check of the Extents Overflow b-tree ----------------------------
	result = CheckExtentsOverflow(volume, &freeBlocks, &xCatalogBlocks, consistencyStatus);
	M_ExitOnError(result);

	//--- do a quick consistency check of the Catalog b-tree ------------------------------------
	result = CheckCatalog( volume, &freeBlocks, xCatalogBlocks, consistencyStatus );
	M_ExitOnError( result );

	//--- do a quick consistency check of the Attributes b-tree ------------------------------------
	result = CheckAttributes( volume, &freeBlocks, consistencyStatus );
	M_ExitOnError( result );

	if( freeBlocks != volume->vcbFreeBlocks && volume->vcbAttributes & kVCBAttrLocked )
	{
		*consistencyStatus |= kHFSMinorRepairsWereMade;
	}

	(void) UpdateFreeCount( volume );	// update freeBlocks from volume bitmap

	// make sure there are not any doubly allocated blocks...
	// we are not concerned with orphaned blocks (since this is not fatal)
	
	bitmapFreeBlocks = volume->vcbFreeBlocks;
	
	if ( freeBlocks < bitmapFreeBlocks)			// there are doubly allocated blocks!
	{
		*consistencyStatus |= kHFSOverlapingExtents;
		goto ErrorExit;
	}
	else if ( freeBlocks > bitmapFreeBlocks)	// there are orphaned blocks
	{
		*consistencyStatus |= kHFSOrphanedExtents;
	}

	return noErr;
	
ErrorExit:

	if ( DEBUG_BUILD )
		DebugStr("\p MountCheck is returning an error!");

printf("# MountCheck is returning 0x%08lx\n", *consistencyStatus);

	return badMDBErr;
}


/*-------------------------------------------------------------------------------

Routine:	VolumeCheckCatalog

Function:	Make sure the name of the root matches the volume name in the MDB.
			Make sure the volume bitmap reflects the allocations of all file
			extents.

Assumption:	

Input:		volume			- pointer to volume control block
			freeBlocks
			xCatalogBlocks
			consistencyStatus

Output:		none

Result:		noErr			- success

-------------------------------------------------------------------------------*/
static OSErr
CheckCatalog ( SVCB *volume, UInt32 *freeBlocks, UInt32 xCatalogBlocks, UInt32 *consistencyStatus )
{
	UInt32				maxRecords;
	UInt32				largestCNID;
	UInt32				fileCount;
	UInt16				rootFileCount;			//XXX can this overflow on HFS Plus volumes?
	UInt16				rootDirectoryCount;		//XXX can this overflow on HFS Plus volumes?
	UInt32				directoryCount;
	UInt32				fileThreads, directoryThreads;
	UInt32				fileThreadsExpected;
	UInt32				catalogValence;
	UInt32				recordsFound;
	UInt32				catalogBlocks;
	OSErr				result;
//	CatalogNodeData		nodeData;
	CatalogRecord		*record;
	CatalogKey			*key;
//	FSSpec				spec;
	UInt16				recordSize;
	BTreeControlBlock*	btree;
//	UInt32				hint;
	SFCB*				file;
	HFSPlusExtentRecord	extentRecord;
	Boolean				hfsPlus = (volume->vcbSignature == kHFSPlusSigWord);
	UInt16				operation;
	BTreeIterator		btreeIterator;
	FSBufferDescriptor	btRecord;
	CatalogRecord		catalogRecord;

	fileThreads = fileThreadsExpected = directoryThreads = 0;
	fileCount = directoryCount = 0;
	rootFileCount = rootDirectoryCount = 0;
	catalogValence = 0;
	recordsFound = 0;
	largestCNID = kHFSFirstUserCatalogNodeID;

	file = volume->vcbCatalogFile;
	
	result = GetFileExtentRecord(volume, file, extentRecord);
	M_ExitOnError(result);

	catalogBlocks = CountExtentBlocks(extentRecord, hfsPlus);	
	*freeBlocks -= catalogBlocks;
	VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);

#if 0
	//	Read in the root directory's record so we can perform some special checks	
	hint = 0;
	result = GetCatalogNode(volume, kHFSRootFolderID, NULL, kNoHint, &spec, &nodeData, &hint);
	M_ExitOnError(result);

	if ( DEBUG_BUILD && nodeData.nodeID != kHFSRootFolderID )	// works for small and large folders
		DebugStr("\p CheckCatalog: nodeData.nodeID != kHFSRootFolderID");

	//	We'll see the root directory's record later, during the full BTree scan, so don't
	//	update the counts here.
//	++recordsFound;
//	catalogValence += nodeData.valence;
	
	// make sure the name of the root directory matches the volume name (HFS only)
	if ( !hfsPlus && !HFSEqualString( spec.name, volume->vcbVN ) )
	{
		if ( volume->vcbAttributes & kVCBAttrLocked )
		{
			*consistencyStatus |= kHFSInconsistentVolumeName;
		}
		else // try and repair it
		{
			if (StrLength( volume->vcbVN ) > 0)	// check length of volume name
			{
				if (volume->vcbVN[0] > kHFSMaxVolumeNameChars)	// if name is too long truncate it
					volume->vcbVN[0] = kHFSMaxVolumeNameChars;
					
				if ( GetDFAStage() == kRepairStage )
				{
					// the length of volume name is OK, rename the root directory
					result = RenameCatalogNode(volume, kHFSRootFolderID, NULL, volume->vcbVN, hint, &hint);
					M_ExitOnError(result);
				}
				*consistencyStatus |= kHFSMinorRepairsWereMade;
				
				// now we must reset for btree search below!
				result = GetCatalogNode(volume, kHFSRootFolderID, NULL, kNoHint, &spec, &nodeData, &hint);
				M_ExitOnError(result);
			}
			else if ( StrLength( spec.name ) > 0 )	// check root name length
			{
				if ( spec.name[0] > kHFSMaxVolumeNameChars )	// if name is too long truncate it
					 spec.name[0] = kHFSMaxVolumeNameChars;
	
				// the root name length is OK, change the volume name to match root name
				CopyMemory( spec.name, volume->vcbVN, spec.name[0]+1);
				volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
				*consistencyStatus |= kHFSMinorRepairsWereMade;
			}
		}
	}
#endif

	// make sure name locked bit is not set
//	if (record.hfsFolder.finderDirInfo.frFlags & kNameLocked)  //XXX need to use node data, not "record"
//	{
//	}
	
	// Make a conservative estimate of the upper limit of B*-Tree records that
	// could conceivably be encountered in a scan of all the leaf nodes.
	maxRecords = (file->fcbLogicalSize) / sizeof(HFSCatalogFolder);

	// scan in leaf node order (slower but more reliable)
	operation = kBTreeFirstRecord;	// first leaf record

	key = (CatalogKey*) &btreeIterator.key;

	btRecord.bufferAddress	= record = &catalogRecord;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(catalogRecord);


	// visit all the leaf node data records in the catalog
	while ( --maxRecords > 0 )
	{
		result = BTIterateRecord(file, operation, &btreeIterator, &btRecord, &recordSize);
					
		if ( result != noErr )
			break;

		++recordsFound;
		operation = kBTreeNextRecord;


	if (hfsPlus)
	{
		switch (record->recordType)
		{
			case kHFSPlusFolderRecord:
			{
				catalogValence += record->hfsPlusFolder.valence;
	
				//	Count all directories except the root itself
				if (key->hfsPlus.parentID != kHFSRootParentID)
					++directoryCount;

				//	Count directories directly inside the root
				if (key->hfsPlus.parentID == kHFSRootFolderID)
					++rootDirectoryCount;
					
				if (record->hfsPlusFolder.folderID > largestCNID)
					largestCNID = record->hfsPlusFolder.folderID;
				break;
			}
			case kHFSPlusFileRecord:
			{
				UInt32	blockSize = volume->vcbBlockSize;

				++fileCount;
				++fileThreadsExpected;	// HFS Plus files always have thread records

				if ( key->hfsPlus.parentID == kHFSRootFolderID )
					++rootFileCount;
	
				if ( record->hfsPlusFile.fileID > largestCNID )
					largestCNID = record->hfsPlusFile.fileID;
		
				// check the blocks allocated to this file (both forks)
				
				*freeBlocks -= CountExtentBlocks(record->hfsPlusFile.dataFork.extents, true);		
				VolumeBitMapCheckExtents(volume, record->hfsPlusFile.dataFork.extents, consistencyStatus);
		
				*freeBlocks -= CountExtentBlocks(record->hfsPlusFile.resourceFork.extents, true);		
				VolumeBitMapCheckExtents(volume, record->hfsPlusFile.resourceFork.extents, consistencyStatus);

				// check the LEOF and PEOF (both forks)
								
				if ( (record->hfsPlusFile.resourceFork.logicalSize > record->hfsPlusFile.resourceFork.totalBlocks * blockSize) ||
					 (record->hfsPlusFile.dataFork.logicalSize > record->hfsPlusFile.dataFork.totalBlocks * blockSize) )
				{
					*consistencyStatus |= kHFSInvalidLEOF;
				}

				if ( !ValidPEOF(volume, record) )
					*consistencyStatus |= kHFSInvalidPEOF;
				break;
			}
			case kHFSPlusFolderThreadRecord:
			{
				++directoryThreads;
				if ( key->hfsPlus.parentID > largestCNID )		// <CS7>
					largestCNID = key->hfsPlus.parentID;
				break;
			}


			case kHFSPlusFileThreadRecord:
			{
				++fileThreads;
				if ( key->hfsPlus.parentID > largestCNID )		// <CS7>
					largestCNID = key->hfsPlus.parentID;
				break;
			}
			default:
				*consistencyStatus |= kHFSInvalidCatalogRecordType;
				break;
		}
	}
	else
	{
		switch (record->recordType)
		{
			case kHFSFolderRecord:
			{
				catalogValence += record->hfsFolder.valence;
	
				//	Count all directories except the root itself
				if (key->hfs.parentID != kHFSRootParentID)
				++directoryCount;

				//	Count directories directly inside the root
				if (key->hfs.parentID == kHFSRootFolderID)
					++rootDirectoryCount;
	
				if (record->hfsFolder.folderID > largestCNID)
					largestCNID = record->hfsFolder.folderID;
				break;
			}
			case kHFSFileRecord:
			{
				++fileCount;
				if ( key->hfs.parentID == kHFSRootFolderID )
					++rootFileCount;
	
				if ( record->hfsFile.fileID > largestCNID )
					largestCNID = record->hfsFile.fileID;
	
				if ( record->hfsFile.flags & kHFSThreadExistsMask )
					++fileThreadsExpected;
	
				// check the blocks allocated to this file (both forks)
				
				ConvertToHFSPlusExtent(record->hfsFile.dataExtents, extentRecord);	
				*freeBlocks -= CountExtentBlocks(extentRecord, false);		
				VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);
		
				ConvertToHFSPlusExtent(record->hfsFile.rsrcExtents, extentRecord);
				*freeBlocks -= CountExtentBlocks(extentRecord, false);		
				VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);

				// check the LEOF and PEOF (both forks)
								
				if ( (record->hfsFile.rsrcLogicalSize > record->hfsFile.rsrcPhysicalSize)	||
					 (record->hfsFile.dataLogicalSize > record->hfsFile.dataPhysicalSize) )
				{
					*consistencyStatus |= kHFSInvalidLEOF;
				}

				if ( !ValidPEOF(volume, record) )
					*consistencyStatus |= kHFSInvalidPEOF;
				break;
			}
			case kHFSFolderThreadRecord:
			{
				++directoryThreads;
				if ( key->hfs.parentID > largestCNID )		// <CS7>
					largestCNID = key->hfs.parentID;				
				break;
			}
			case kHFSFileThreadRecord:
			{
				++fileThreads;
				if ( key->hfs.parentID > largestCNID )		// <CS7>
					largestCNID = key->hfs.parentID;				
				break;
			}
			default:
				*consistencyStatus |= kHFSInvalidCatalogRecordType;
				break;
		}
	}
	
	} // end while

	if ( result == noErr )
	{
		*consistencyStatus |= kHFSCatalogBTreeLoop;
		goto ErrorExit;		// punt on loop error
	}
	
	if ( result != fsBTRecordNotFoundErr )
		goto ErrorExit;		// punt on BTree errors


	// Check if calculated totals match the Volume Control Block
	
	if ((volume->vcbNextCatalogID <= largestCNID) && (largestCNID > kHFSFirstUserCatalogNodeID))
	{
		volume->vcbNextCatalogID = largestCNID + 1;
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
	}

	if (volume->vcbFileCount != fileCount)
	{
		volume->vcbFileCount = fileCount;
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
	}

	if (volume->vcbFolderCount != directoryCount)
	{
		volume->vcbFolderCount = directoryCount;
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
	}

	if (!hfsPlus && volume->vcbNmFls != rootFileCount)
	{
		volume->vcbNmFls = rootFileCount;
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
	}

	if (!hfsPlus && volume->vcbNmRtDirs != rootDirectoryCount)
	{
		volume->vcbNmRtDirs = rootDirectoryCount;
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
	}
	
	if ( catalogValence != (fileCount + directoryCount) )
	{
		*consistencyStatus |= kHFSInvalidValence;			// valence inconsistency detected
	}

#if 0	
	if (volume->vcbAttributes & ~kMDBValidAttributesMask)	// check for invalid bits
	{
		volume->vcbAttributes &= kMDBValidAttributesMask;
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
	}
#endif
	
	if ( (volume->vcbFlags & kVCBFlagsVolumeDirtyMask) && (volume->vcbAttributes & kVCBAttrLocked) == 0 )
		*consistencyStatus |= kHFSMinorRepairsWereMade;

	if (directoryThreads > (directoryCount + 1))
	{
		*consistencyStatus |= kHFSOrphanedThreadRecords;		// too many directory thread records
	}
		
	if (fileThreads > fileThreadsExpected)
	{
		if ( volume->vcbAttributes & kVCBAttrLocked )
		{
			*consistencyStatus |= kHFSOrphanedThreadRecords;	// too many file thread records
		}
		else if ( !hfsPlus ) // try and repair orphaned thread records
		{
			// If we fix up some threads, see if we can come out even without counting all the file
			// threads again. If we don't, then looks like we have to keep count and do a second
			// pass of file records (gotta do that anyway when we fix the KHFSMissingThreadRecords
			// case below) and to count thread records again with the FixOrphanFileThreads call.
#if 0	
			if ( GetDFAStage() == kRepairStage )
			{
				UInt32 repairsDone = 0;
					
				(void) FixOrphanFileThreads(volume, recordsFound, &repairsDone);
				if (repairsDone)
					*consistencyStatus |= kHFSMinorRepairsWereMade;
	
				if (fileThreads > (fileThreadsExpected + repairsDone) ) 
					*consistencyStatus |= kHFSOrphanedThreadRecords;	// still too many thread records
			}
#endif
		}
	}

	if ((directoryThreads < (directoryCount + 1)) || (fileThreads < fileThreadsExpected))
		*consistencyStatus |= kHFSMissingThreadRecords;		// not enough thread records

	btree = GetBTCB(volume->vcbCatalogFile);

	// okay, lets see if the blocks used by the catalog in BOTH btrees looks right
	catalogBlocks += xCatalogBlocks;
	if ( (catalogBlocks * volume->vcbBlockSize) != file->fcbPhysicalSize )
		*consistencyStatus |= kHFSInvalidPEOF;

	if ( btree->totalNodes != (file->fcbPhysicalSize / btree->nodeSize) )
		*consistencyStatus |= kHFSInvalidBTreeHeader;


	if (btree->leafRecords != recordsFound)
	{
		if (volume->vcbAttributes & kVCBAttrLocked)
		{
			*consistencyStatus |= kHFSInvalidBTreeHeader;		// invalid record count
		}
		else // go ahead and make this simple repair!
		{		
			btree->leafRecords = recordsFound;
			btree->flags |= kBTHeaderDirty;						// mark BTreeControlBlock dirty
			*consistencyStatus |= kHFSMinorRepairsWereMade;
		}
	}
	
	if ( GetDFAStage() == kRepairStage )
	{
		if ( btree->flags & kBTHeaderDirty )	// if we made changes, write them out
			(void) BTFlushPath(file);
	}

	return noErr;
	
ErrorExit:
	return badCatalogErr;

} // end CheckCatalog


/*-------------------------------------------------------------------------------

Routine:	CheckExtentsOverflow

Function:	Make sure the volume bitmap reflects the allocations of all extents.

Assumption:	

Input:		volume			- pointer to volume control block
			freeBlocks
			xCatalogBlocks
			consistencyStatus

Output:		none

Result:		noErr			- success

-------------------------------------------------------------------------------*/
static OSErr
CheckExtentsOverflow ( SVCB *volume, UInt32 *freeBlocks, UInt32 *xCatalogBlocks, UInt32 *consistencyStatus )
{
	OSErr				result;
	UInt32				maxRecords;
	UInt32				fileID;
	UInt32				largestCNID;
	UInt32				recordsFound; 
	UInt32				extentFileBlocks;
	UInt32				extentRecordBlocks;
	HFSPlusExtentKey		*extentKeyPtr;
	HFSPlusExtentRecord	extentRecord;
	UInt16				recordSize;
	BTreeControlBlock	*btree;
	SFCB*				file;
	Boolean				hfsPlus = (volume->vcbSignature == kHFSPlusSigWord);
	UInt16				operation;
	BTreeIterator		btreeIterator;
	FSBufferDescriptor	btRecord;

	*xCatalogBlocks = 0;  // set this early since its a return value
	largestCNID = kHFSFirstUserCatalogNodeID;
	
	file = volume->vcbExtentsFile;

	result = GetFileExtentRecord(volume, file, extentRecord);
	ReturnIfError(result);

	extentFileBlocks = CountExtentBlocks(extentRecord, hfsPlus);
	*freeBlocks -= extentFileBlocks;
	VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);

	// now look at the extent tree...

	// Make a conservative estimate of the upper limit of B*-Tree records that
	// could conceivably be encountered in a scan of all the leaf nodes.
	recordsFound = 0;
	maxRecords = (file->fcbLogicalSize) / (hfsPlus ? sizeof(HFSPlusExtentRecord) : sizeof(HFSExtentRecord));

	// scan in leaf node order (slower but more reliable)
	operation = kBTreeFirstRecord;	// first leaf record
	extentKeyPtr = (HFSPlusExtentKey*) &btreeIterator.key;

	btRecord.bufferAddress	= &extentRecord;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(extentRecord);

	// visit all the leaf node data records in the extents B*-Tree
	while ( recordsFound < maxRecords )
	{
		result = BTIterateRecord(file, operation, &btreeIterator, &btRecord, &recordSize);

		if ( result != noErr )
			break;

		++recordsFound;

		// check the blocks allocated to this extent record
		
		operation = kBTreeNextRecord;
		if ( !hfsPlus )
			ConvertToHFSPlusExtent(*(HFSExtentRecord*) &extentRecord, extentRecord);	// convert it in place (since its a copy)
			
		extentRecordBlocks = CountExtentBlocks(extentRecord, hfsPlus);
		
		*freeBlocks -= extentRecordBlocks;		
		VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);
		
		if ( hfsPlus )
			fileID = extentKeyPtr->fileID;
		else
			fileID = ((HFSExtentKey*) extentKeyPtr)->fileID;
		
		// if these were catalog blocks, we need to keep track 
		if ( fileID == kHFSCatalogFileID )
			*xCatalogBlocks += extentRecordBlocks;
			
		if ( fileID > largestCNID )
			largestCNID = fileID;

	} // end while

	if ( result != fsBTRecordNotFoundErr) 
		return badMDBErr;		// punt on error (loop error or BTree error)

	if (volume->vcbNextCatalogID < largestCNID)
	{
		volume->vcbNextCatalogID = largestCNID + 1;
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;
		volume->vcbAttributes |= 1;	// need a constant for this!
	}	
	
	btree = GetBTCB(volume->vcbExtentsFile);

	if ( (extentFileBlocks * volume->vcbBlockSize) != file->fcbPhysicalSize )
		*consistencyStatus |= kHFSInvalidPEOF;

	if ( btree->totalNodes != (file->fcbLogicalSize / btree->nodeSize) )
		*consistencyStatus |= kHFSInvalidBTreeHeader;
	
	if (btree->leafRecords != recordsFound)
	{
		if (volume->vcbAttributes & kVCBAttrLocked)
		{
			*consistencyStatus |= kHFSInvalidBTreeHeader;
		}
		else // go ahead and make this simple repair!
		{
			btree->leafRecords = recordsFound;
			btree->flags |= kBTHeaderDirty;				// mark BTreeControlBlock dirty
			*consistencyStatus |= kHFSMinorRepairsWereMade;
		}
	}

	if ( GetDFAStage() == kRepairStage )
	{
		if ( btree->flags & kBTHeaderDirty )	// if we made changes, write them out
			(void) BTFlushPath(file);
	}
	
	return noErr;

} // end CheckExtentsOverflow



/*-------------------------------------------------------------------------------

Routine:	CheckAttributes

Function:	Make sure the volume bitmap reflects the allocations of all attributes
			that occupy extents.

Assumption:	

Input:		volume			- pointer to volume control block
			freeBlocks
			consistencyStatus

Output:		none

Result:		noErr			- success

-------------------------------------------------------------------------------*/
static OSErr
CheckAttributes ( SVCB *volume, UInt32 *freeBlocks, UInt32 *consistencyStatus )
{
	OSErr				result;
	UInt32				maxRecords;
	UInt32				recordsFound; 
	UInt32				attributesFileBlocks;
	UInt32				extentRecordBlocks;
	HFSPlusAttrRecord	*attributeDataPtr;
	HFSPlusAttrRecord	attributeData;
	HFSPlusExtentRecord	extentRecord;
	UInt16				recordSize = 0;
	BTreeControlBlock	*btree;
	SFCB*				file;
	BTreeIterator		btreeIterator;
	FSBufferDescriptor	btRecord;
	UInt16				operation;
	Boolean				hfsPlus = (volume->vcbSignature == kHFSPlusSigWord);

	//
	//	If there isn't an attributes B-tree, there's no work to do
	//
	if (volume->vcbAttributesFile == NULL)
		return noErr;

	file = volume->vcbAttributesFile;

	result = GetFileExtentRecord(volume, file, extentRecord);
	ReturnIfError(result);

	attributesFileBlocks = CountExtentBlocks(extentRecord, hfsPlus);
	*freeBlocks -= attributesFileBlocks;
	VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);


	// now look at the attributes tree...

	// Make a conservative estimate of the upper limit of B*-Tree records that
	// could conceivably be encountered in a scan of all the leaf nodes.
	recordsFound = 0;
	maxRecords = (file->fcbLogicalSize) / 8;	// 8 = minimum data length (for zero length inline attribute)

	operation = kBTreeFirstRecord;

	btRecord.bufferAddress = &attributeData;
	btRecord.itemCount = 1;
	btRecord.itemSize = sizeof(attributeData);
		
	attributeDataPtr = &attributeData;

	// visit all the leaf node data records in the extents B*-Tree
	while ( recordsFound < maxRecords )
	{
		result = BTIterateRecord(file, operation, &btreeIterator, &btRecord, (UInt16*) &recordSize);

		if ( result != noErr )
			break;

		++recordsFound;

		//	If this block has information about extents, make sure they're allocated
		switch (attributeDataPtr->recordType) {
			case kHFSPlusAttrInlineData:
				break;
			case kHFSPlusAttrForkData:
				extentRecordBlocks = CountExtentBlocks(attributeDataPtr->forkData.theFork.extents, true);
				*freeBlocks -= extentRecordBlocks;
				VolumeBitMapCheckExtents(volume, attributeDataPtr->forkData.theFork.extents, consistencyStatus);
				break;
			case kHFSPlusAttrExtents:
				extentRecordBlocks = CountExtentBlocks(attributeDataPtr->overflowExtents.extents, true);
				*freeBlocks -= extentRecordBlocks;
				VolumeBitMapCheckExtents(volume, attributeDataPtr->overflowExtents.extents, consistencyStatus);
				break;
			default:
				if (DEBUG_BUILD)
					DebugStr("\pHFS+: Unknown attribute record");
				break;
		}	// end switch

		operation = kBTreeNextRecord;

	} // end while

	if ( result != fsBTRecordNotFoundErr )
		return badMDBErr;		// punt on error (loop error or BTree error)

	btree = GetBTCB(volume->vcbAttributesFile);

	if ( (attributesFileBlocks * volume->vcbBlockSize) != file->fcbPhysicalSize )
		*consistencyStatus |= kHFSInvalidPEOF;

	if ( btree->totalNodes != (file->fcbLogicalSize / btree->nodeSize) )
		*consistencyStatus |= kHFSInvalidBTreeHeader;
	
	if (btree->leafRecords != recordsFound)
	{
		if (volume->vcbAttributes & kVCBAttrLocked)
		{
			*consistencyStatus |= kHFSInvalidBTreeHeader;
		}
		else // go ahead and make this simple repair!
		{
			btree->leafRecords = recordsFound;
			btree->flags |= kBTHeaderDirty;				// mark BTreeControlBlock dirty
			*consistencyStatus |= kHFSMinorRepairsWereMade;
		}
	}
	
	if ( GetDFAStage() == kRepairStage )
	{
		if ( btree->flags & kBTHeaderDirty )	// if we made changes, write them out
			(void) BTFlushPath(file);
	}
	return noErr;

} // end CheckAttributes



/*-------------------------------------------------------------------------------

Routine:	CheckBitmapAndHeader

Function:	Make sure the volume header, alternate volume header, and the bitmap
			itself are marked as used in the bitmap.

Assumption:	

Input:		volume			- pointer to volume control block
			freeBlocks
			consistencyStatus

Output:		none

Result:		noErr			- success

-------------------------------------------------------------------------------*/
static OSErr
CheckBitmapAndHeader ( SVCB *volume, UInt32 *freeBlocks, UInt32 *consistencyStatus )
{
	OSErr				result;
	UInt32				blocksUsed;
	SFCB*				file;
	HFSPlusExtentRecord	extentRecord;

	//
	//	If there isn't an allocation file, there's no work to do
	//
	if (volume->vcbSignature != kHFSPlusSigWord)
		return noErr;

	file = volume->vcbAllocationFile;

	result = GetFileExtentRecord(volume, file, extentRecord);
	ReturnIfError(result);

	//	Mark the space used by the allocation file (bitmap) itself
	blocksUsed = CountExtentBlocks(extentRecord, true);
	*freeBlocks -= blocksUsed;
	VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);

	//	Now mark sectors zero through two (boot blocks and volume header)
	//	and the last two sectors (alternate volume header and a reserved
	//	sector).  The number of allocation blocks depends on the size of
	//	an allocation block.  What we do is create a fake extent record
	//	for these blocks, and call VolumeBitMapCheckExtents as usual.

	ClearMemory(&extentRecord, sizeof(extentRecord));
	
	extentRecord[0].startBlock = 0;
	extentRecord[0].blockCount = 1 + (1024 / volume->vcbBlockSize);	// sectors 0-2

	if (volume->vcbBlockSize == 512) {
		extentRecord[1].startBlock = volume->vcbTotalBlocks - 2;
		extentRecord[1].blockCount = 2;
	} else {
		extentRecord[1].startBlock = volume->vcbTotalBlocks - 1;
		extentRecord[1].blockCount = 1;
	}
	
	*freeBlocks -= extentRecord[0].blockCount + extentRecord[1].blockCount;
	VolumeBitMapCheckExtents(volume, extentRecord, consistencyStatus);

	return noErr;
}


//	For Disk First Aid, this routine will not update the BitMap, but will set the
//	kHFSMinorRepairsWereMade bit to indicate the bitmap needs updating.
static void VolumeBitMapCheckExtents (
							SVCB*			volume,
							HFSPlusExtentDescriptor*	theExtents,
							UInt32*					consistencyStatus )
{
	OSErr	err;
	
	err	= BlockCheck(volume, theExtents);
	
	if ( err == -1 )
		*consistencyStatus |= kHFSMinorRepairsWereMade;
	else if ( err == vcInvalidExtentErr )
		*consistencyStatus |= kHFSInvalidExtentRecord;
}


/*-------------------------------------------------------------------------------

Routine:	ValidPEOF

Function:	Make sure the local extents match the PEOF.
			Currently this only checks the local extent.
			We could also check the oveflow extents for
			a more complete (but slower) test.

Assumption:	

Input:		

Output:		

Result:	

-------------------------------------------------------------------------------*/

static Boolean
ValidPEOF( SVCB* volume, CatalogRecord *file )
{
	UInt32	minPEOF;
	UInt32 	allocBlkSize = volume->vcbBlockSize;
	
	if (file->recordType == kHFSPlusFileRecord)
	{
		UInt32	minBlocks;

		minBlocks = CountExtentBlocks(file->hfsPlusFile.resourceFork.extents, true);
		
		if ( file->hfsPlusFile.resourceFork.totalBlocks < minBlocks )
			return false;
		else if ( (file->hfsPlusFile.resourceFork.totalBlocks > minBlocks) && (file->hfsPlusFile.resourceFork.extents[kHFSPlusExtentDensity-1].blockCount == 0) )
			return false;
	
		minBlocks = CountExtentBlocks(file->hfsPlusFile.dataFork.extents, true);
		
		if ( file->hfsPlusFile.dataFork.totalBlocks < minBlocks )
			return false;
		else if ( (file->hfsPlusFile.dataFork.totalBlocks > minBlocks) && (file->hfsPlusFile.dataFork.extents[kHFSPlusExtentDensity-1].blockCount == 0) )
			return false;
	}
	else
	{
		HFSPlusExtentRecord	extentRecord;
		
		ConvertToHFSPlusExtent(file->hfsFile.rsrcExtents, extentRecord);	
		minPEOF = CountExtentBlocks(extentRecord, false) * allocBlkSize;
		
		if ( file->hfsFile.rsrcPhysicalSize < minPEOF )
			return false;
		else if ( (file->hfsFile.rsrcPhysicalSize > minPEOF) && (extentRecord[kHFSExtentDensity-1].blockCount == 0) )
			return false;
	

		ConvertToHFSPlusExtent(file->hfsFile.dataExtents, extentRecord);	
		minPEOF = CountExtentBlocks(extentRecord, false) * allocBlkSize;
		
		if ( file->hfsFile.dataPhysicalSize < minPEOF )
			return false;
		else if ( (file->hfsFile.dataPhysicalSize > minPEOF) && (extentRecord[kHFSExtentDensity-1].blockCount == 0) )
			return false;
	}

		
	return true;
}


/*-------------------------------------------------------------------------------

Routine:	FixOrphanFileThreads

Function:	For orphan thread records during VolumeCheckConsistency have the catalog
			searched again for file records that match. If their thread record flag
			is off, mark it. If it is on, do nothing. If the file record doesn't exist,
			delete the thread record.

Assumption:	This is a function to be called only if the thread records detected during
			CheckCatalog was greater than the file records that had their flag set for
			file thread existance. I've thought of doing this differently, like keeping
			a table of all file records or threads scanned in the first pass, but the
			table gets large. You can't predict how large so may to hold upto the max
			fileIDs possible. (2**32). Using two such tables to match file threads
			against file records would be bad. Less space would be keeping a bitmap of
			all fileIDS (to 2**32) and marking those with threads. Then a separate
			bitmap for those file records with thread flags on marked.You do a byte by
			byte comparison across the whole bitmap. When they differ, you know if you're
			missing
			threads or orphans threads. (Note 0s would also mean IDs represented by that
			bit in the bitmap that is non-existant). Though, this sounds clever, it still
			seems it would take 8bits/byte * 512 bytes/sector * 8 sectors/ vmpage = 2**15
			bits per vmpage and 2**17 vmpages to cover the bitmap! Too large. SO, let's
			just scan the catalog again. Yep, it's timeconsuming, but we are only taking
			the hit in repair mode for that particular problem. No extra memory for tables
			or bitmaps are even required during the original scan.

Input:		
								
Output:		

Result:		E_NoError				- success
			!= E_NoError			- failure

-------------------------------------------------------------------------------*/
#if 0
static OSErr FixOrphanFileThreads(SVCB* volume, UInt32 maxRecords, UInt32 *repairsDone)
{
	UInt16				recordSize;
	OSErr				tempResult, result = noErr;
	CatalogKey			key;
	FSSpec				spec;
	CatalogRecord		record;
	CatalogNodeData		nodeData;
	UInt32				hint = 0;
		
	// let's iterate the catalog looking for file thread records. As we hit each one, 
	// do a search for the file record. If it exists and the thread flag is off,set
	// it. If it exists, and the thread flag is on, all is fine and dandy. It it
	// doesn't exist, remove the orphaned thread record. 
	
	*repairsDone = 0;
	
	result = GetCatalogNode(volume, kHFSRootFolderID, NULL, kNoHint, &spec, &nodeData, &hint);
	M_ExitOnError(result);
	
	// visit all the leaf node data records in the catalog
	while ( --maxRecords > 0 )  // starts out at recordsFound value passed in
	{
		tempResult = GetBTreeRecord(volume->catalogRefNum, kBTreeNext, &key, &record,
								 &recordSize, &hint );
		if ( tempResult )
		{
			//	DebugStr("\p FixOrphanFileThreads: did we run out of records?");
			break;
		}
		
		// if this is a file thread record, check it out...
		if (record.recordType == kHFSFileThreadRecord)
		{
			CatalogKey	currentKey;
			CatalogKey	fileKey;
			UInt32		fileHint;
				
			// save our current position
			currentKey = key;	
			
			// create a key for the file record
			fileHint = 0;
			fileKey.hfs.parentID = record.hfsThread.parentID;
			CopyMemory ( record.hfsThread.nodeName, fileKey.hfs.nodeName,
							record.hfsThread.nodeName[0] + 1);
			
			// try and get the file record for this thread record...
			tempResult = SearchBTreeRecord ( volume->catalogRefNum, &fileKey, fileHint, &key, &record, &recordSize, &fileHint );
				
			if (tempResult == noErr)
			{		
				if ((record.hfsFile.flags & kHFSThreadExistsMask) == 0)
				{
					// thread record exists but file record doesn't think so....set it
					record.hfsFile.flags |= kHFSThreadExistsMask;
					
					// update the file record on disk
					if ( ReplaceBTreeRecord(volume->catalogRefNum, &key, fileHint, &record, recordSize, &fileHint) == noErr )		
						++(*repairsDone);
				}
			}		
			else if (tempResult == btNotFound)
			{
				// BTDelete invalidates the current node mark so before we delete the thread record,
				// point currentKey and hint at something valid (ie the previous record)
				
				(void) SearchBTreeRecord( volume->catalogRefNum, &currentKey, hint, &key, &record, &recordSize, &hint );
				(void) GetBTreeRecord( volume->catalogRefNum, kBTreePrevious, &key, &record, &recordSize, &hint );

				// file record doesn't exist. Delete the thread record.
				if ( DeleteBTreeRecord( volume->catalogRefNum, &currentKey) == noErr )
					++(*repairsDone);  
				
				currentKey = key;	// now point to the record before the thread record
			}

			// now we need to reset the current node mark for GetBTreeRecord iteration...
			(void) SearchBTreeRecord( volume->catalogRefNum, &currentKey, hint, &key, &record, &recordSize, &hint );

		} // end if thread
	
	} // end while
	
	return( result );
ErrorExit:
	return ( result ); //XXX which error should it be ???
} // end FixOrphanFileThreads
#endif



static UInt32
CountExtentBlocks(HFSPlusExtentRecord extents, Boolean hfsPlus)
{
	UInt32	blocks;
	
	// grab the first 3
	blocks = extents[0].blockCount + extents[1].blockCount + extents[2].blockCount;

	if (hfsPlus)
	{
		UInt32	i;
		
		for (i = 3; i < kHFSPlusExtentDensity; ++i)
			blocks += extents[i].blockCount;		// HFS Plus has additional extents
	}

	return blocks;
}


static OSErr
GetFileExtentRecord( const SVCB	*vcb, SFCB *fcb, HFSPlusExtentRecord extents)
{
	if (vcb->vcbSignature == kHFSPlusSigWord)
		CopyMemory(fcb->fcbExtents32, extents, sizeof(HFSPlusExtentRecord));
	else
		ConvertToHFSPlusExtent(fcb->fcbExtents16, extents);
	
	return noErr;
}



//_______________________________________________________________________
//
//	CheckCreateDate
//	
//	For HFS Plus volumes, make sure the createDate in the HFSPlusVolumeHeader
//	is the same as in the MDB.  If not, just fix it.
//
//_______________________________________________________________________

static OSErr CheckCreateDate (SVCB* volume, UInt32* consistencyStatus )
{
	OSErr					err;
	HFSMasterDirectoryBlock	*mdb;
	UInt32					createDate;
	
	if (volume->vcbSignature != kHFSPlusSigWord)
		return noErr;
	
	//
	//	Read in the MDB
	//
	err = GetBlock_glue(gbReleaseMask, 2, (Ptr *) &mdb, volume->vcbVRefNum, volume);
	if (err != noErr) return err;
	
	//
	//	Make sure it really is a wrappered volume with an MDB
	//
	if (mdb->drSigWord != kHFSSigWord || mdb->drEmbedSigWord != kHFSPlusSigWord)
		return noErr;			// must not be a wrapper, so nothing to fix
	
	createDate = mdb->drCrDate;
	
	if (volume->vcbCreateDate != createDate) {
		volume->vcbCreateDate = createDate;					// fix create date in the VCB
		volume->vcbFlags |= kVCBFlagsVolumeDirtyMask;		// make it dirty so change will be written to volume header
		*consistencyStatus |= kHFSMinorRepairsWereMade;	// the problem is now fixed
	}
	
	return noErr;
}
