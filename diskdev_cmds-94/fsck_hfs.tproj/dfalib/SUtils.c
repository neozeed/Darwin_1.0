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
	File:		SUtils.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.
*/

#include "Scavenger.h"


OSErr GetFBlk( SGlobPtr GPtr, SInt16 fileRefNum, SInt32 blockNumber, void **bufferH );


UInt32 gDFAStage;

UInt32	GetDFAStage( void )
{	
	return (gDFAStage);
}

void	SetDFAStage( UInt32 stage )
{
	gDFAStage = stage;
}



//
//	Calculates the maximum safe available TempMem for us to use
//
SInt32	CalculateSafePhysicalTempMem( Boolean *useTempMem )
{
#if !BSD
	#define	kProcessManagerSlop	512*1024	//	leave 512K for process manager
	OSErr				err;
	SInt32				availableMemory;
	ProcessSerialNumber	psn;
	ProcessInfoRec		processInfo;
	Size				dummy, maxFreeMem;

	*useTempMem	= false;

	err = Gestalt( gestaltPhysicalRAMSize, &availableMemory );
	if( err != noErr )
	{
		M_DebugStr("\p Gestalt gestaltPhysicalRAMSize FAILED");
		goto BAIL;
	}

	availableMemory -= ((SInt32)(LMGetSysZone())->bkLim);

	//	This is easier than calling GetCurrentProcess.  <13>
	psn.highLongOfPSN = 0;
	psn.lowLongOfPSN = kCurrentProcess;
	
	processInfo.processInfoLength = sizeof(processInfo);		//	This field must be initialized.  <13>
	processInfo.processName		= nil;
	processInfo.processAppSpec	= nil;
	
	err = GetProcessInformation( &psn, &processInfo );
	if( err != noErr )
	{
		M_DebugStr("\p GetProcessInformation FAILED");
		goto BAIL;
	}

	availableMemory -= processInfo.processSize;
	maxFreeMem	= TempMaxMem( &dummy );
	*useTempMem	= true;

	
	//	return the lesser of the two, and leave some slop for safety
	if ( availableMemory < maxFreeMem )
		return( availableMemory - kProcessManagerSlop );
	else
		return( maxFreeMem - kProcessManagerSlop );

BAIL:
	M_DebugStr("\p CalculateSafePhysicalTempMem FAILED");
#endif
	return( 64*1024 );
}


/*------------------------------------------------------------------------------

Routine:	RcdError

Function:	Record errors detetected by scavenging operation.
			
Input:		GPtr		-	pointer to scavenger global area.
			ErrCode		-	error code

Output:		None			
------------------------------------------------------------------------------*/

void RcdError( SGlobPtr GPtr, OSErr errorCode )
{
	GPtr->ErrCode = errorCode;
	
	WriteError( GPtr, errorCode, GPtr->TarID, GPtr->TarBlock );	//	log to summary window
}


/*------------------------------------------------------------------------------

Routine:	GetVBlk	(Get Volume Block)

Function:	Gets a specified logical block from the target volume.  The block
			is obtained through the cache.
			
			Note:  The cache buffer is released right away leaving the buffer
			unlocked.
			
Input:		GPtr		-	pointer to scavenger global area.
			BlkNum		-	logical block number
			BufPtr		-	pointer to cache buffer containing block

Output:		GetVBlk	-	function result:
								0 =	no error
								n =	error
------------------------------------------------------------------------------*/

int GetVBlk( SGlobPtr GPtr, UInt32 BlkNum, void **BufPtr)
{
	OSErr		result;
	
	result = GetBlock_glue( gbDefault, BlkNum, (Ptr *)BufPtr, GPtr->calculatedVCB->vcbVRefNum, GPtr->calculatedVCB );
	if (result != 0)
	{
		result = IntError( GPtr,result );
		return( result );							//	could't get it
	}	
 
	result = ReleaseCacheBlock( *BufPtr, 0 );			//	release it, to unlock buffer

	return( noErr );								//	all is fine
}



/*------------------------------------------------------------------------------

Routine:	AllocExt (Allocate Extent)

Function:	Allocates an extent in the Scavengers volume bit map.
			
Input:		GPtr		-	pointer to scavenger global area.
			startBlock	-	starting allocation block number.
			blockCount	-	number of allocation blocks.

Output:		AllocExt	-	function result:			
								0 = no error
								n = error
30Mar91		KSCT	Fixing a bug which used short for an unsigned short.
------------------------------------------------------------------------------*/

OSErr AllocExt( SGlobPtr GPtr, const UInt32 startBlock, const UInt32 blockCount )
{
	UInt32				i;
	UInt32				currentAllocationBlock;
	unsigned char		mask;
	Ptr					byteP;
	UInt16				bitPos;
	UInt32				startBlockInBuffer;
	UInt32				blockCountInBuffer;
	Boolean				overlapRecorded				= false;
	OSErr				err							= noErr;						//	Used in detecting overlapped extents
	VolumeBitMapHeader	*volumeBitMap				= GPtr->volumeBitMapPtr;
	UInt32				blocksPerBuffer				= volumeBitMap->bufferSize * 8;	//	Number of allocation blocks represented in each buffer
	UInt32				bufferStartAllocationBlock	= volumeBitMap->currentBuffer * blocksPerBuffer;
	UInt32				bufferEndAllocationBlock	= bufferStartAllocationBlock + blocksPerBuffer;		//	First block after end of buffer


	//	If we have nearly 4G allocation blocks, then bufferEndAllocationBlock could wrap around.  If so,
	//	pin it to the maximum integer.
	if (bufferEndAllocationBlock < bufferStartAllocationBlock)
		bufferEndAllocationBlock = 0xFFFFFFFF;

	//	Since we have to alternate BitMap buffers, make sure this one is in the current buffer
	if ( (startBlock >= bufferEndAllocationBlock) || (startBlock+blockCount <= bufferStartAllocationBlock) )
		return( noErr );

	//
	//	Compute the part of this extent that intersects the current buffer.
	//	We use "i" to hold the block one beyond the end of the extent.
	//	Then, both start and end (i) are pinned to the buffer's start and end.
	//
	//	Rewrote this code for <13>.
	//
	i = startBlock + blockCount;						//	Compute one past the end of input extent
	if (startBlock < bufferStartAllocationBlock)		//	startBlockInBuffer = max(startBlock, bufferStartAllocationBlock)
		startBlockInBuffer = bufferStartAllocationBlock;
	else
		startBlockInBuffer = startBlock;
	if (i > bufferEndAllocationBlock)					//	i = min(i, bufferEndAllocationBlock)
		i = bufferEndAllocationBlock;
	blockCountInBuffer = i - startBlockInBuffer;		//	count = end - start
	

	//	Make startBlockInBuffer relative to the start of the buffer		<13>
	startBlockInBuffer -= bufferStartAllocationBlock;				//	<13>

	//
	//	Now do the real allocation
	//
	currentAllocationBlock = startBlockInBuffer;
	byteP = volumeBitMap->buffer + (startBlockInBuffer >> 3);		//	Ptr to starting byte
	
	for ( i = 0; i < blockCountInBuffer; i++ )
	{
		bitPos	= ( currentAllocationBlock++ % 8 );
		mask	= ( 0x80 >> bitPos );
		
		
		if ( ((*byteP & mask) != 0) && (overlapRecorded == false) )									//	Check for overlapped extents
		{
			if ( GetDFAStage() != kRepairStage )					//	No need to display error message during repair
				RcdError( GPtr, E_OvlExt );
			overlapRecorded	=  true;								//	<12> Only record one error per extent call
			GPtr->VIStat	|= S_OverlappingExtents;				//	Overlapping extents found
			err				=  E_OvlExt;							//	Let caller deal with the problem, we just mark 'em
		}


		*byteP = *byteP | mask;										//	Mark the block allocated!
		
		if ( bitPos == 7 )											//	Advance to the next byte
			byteP++;
	}
	//	MountCheck() calculates the freeBlocks for us
	//GPtr->calculatedVCB->freeBlocks -= blockCountInBuffer;		//	Adjust free block count
	volumeBitMap->bitMapRecord[volumeBitMap->currentBuffer].count += blockCountInBuffer;
	
	return( err );
	
}


/*------------------------------------------------------------------------------

Routine:	IntError

Function:	Records an internal Scavenger error.
			
Input:		GPtr		-	pointer to scavenger global area.
			ErrCode		-	internal error code

Output:		IntError	-	function result:			
								(E_IntErr for now)
------------------------------------------------------------------------------*/

int IntError( SGlobPtr GPtr, OSErr errorCode )
{
	GPtr->RepLevel = repairLevelUnrepairable;
	
	if ( errorCode == ioErr )				//	Cast I/O errors as read errors
		errorCode	= R_RdErr;
		
	if( (errorCode == R_RdErr) || (errorCode == R_WrErr) )
	{
		GPtr->ErrCode	= GPtr->volumeErrorCode;
		GPtr->IntErr	= 0;
		return( errorCode );		
	}
	else
	{
		GPtr->ErrCode	= R_IntErr;
		GPtr->IntErr	= errorCode;
		return( R_IntErr );
	}
	
}	//	End of IntError



/*------------------------------------------------------------------------------

Routine:	AllocBTN (Allocate BTree Node)

Function:	Allocates an BTree node in a Scavenger BTree bit map.
			
Input:		GPtr		-	pointer to scavenger global area.
			StABN		-	starting allocation block number.
			NmABlks		-	number of allocation blocks.

Output:		AllocBTN	-	function result:			
								0 = no error
								n = error
------------------------------------------------------------------------------*/

int AllocBTN( SGlobPtr GPtr, SInt16 fileRefNum, UInt32 nodeNumber )
{
	UInt16				bitPos;
	unsigned char		mask;
	char				*byteP;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( fileRefNum );

	//	Allocate the node 

	byteP = ( (BTreeExtensionsRec*)calculatedBTCB->refCon)->BTCBMPtr + (nodeNumber / 8 );	//	ptr to starting byte
	bitPos = nodeNumber % 8;						//	bit offset
	mask = ( 0x80 >> bitPos );
	if ( (*byteP & mask) != 0 )
	{	
		RcdError( GPtr, E_OvlNode );
		return( E_OvlNode );					//	node already allocated
	}
	*byteP = *byteP | mask;						//	allocate it
	calculatedBTCB->freeNodes--;		//	decrement free count
	
	return( noErr );
}



/*------------------------------------------------------------------------------

Routine:	GetFBlk	(Get File Block)

Function:	Gets a specified file block from the target volume.  The block
			is obtained through the cache.
			
			Note:  The cache buffer is released right away leaving the buffer
			unlocked.
			
Input:		GPtr		-	pointer to scavenger global area.
			fileRefNum	-	file refnum
			BlkNum		-	file block number
			BufPtr		-	pointer to cache buffer containing block

Output:		GetFBlk	-	function result:
								0 =	no error
								n =	error
------------------------------------------------------------------------------*/

OSErr GetFBlk( SGlobPtr GPtr, SInt16 fileRefNum, SInt32 blockNumber, void **bufferH )
{
	OSErr			result;
	
	result = GetBlock_glue( gbDefault, blockNumber, (Ptr *)bufferH, fileRefNum, GPtr->calculatedVCB );
	
	if ( result != noErr )
	{
		result = IntError( GPtr, result );				 //	could't get it
		return( result );
	}	
 
 	//¥¥	perhaps we should copy block to a buffer before releasing, nahhh
	result = ReleaseCacheBlock( *bufferH, 0 );				//	release it, to unlock buffer

	return( noErr );									//	all is fine
}


OSErr	GetBTreeHeader( SGlobPtr GPtr, SInt16 fileRefNum, BTHeaderRec **header )
{
	OSErr err;
	Ptr headerNodePtr;
	
	GPtr->TarBlock = kHeaderNodeNum;					/* set target node number */

	err = GetBlock_glue( gbReleaseMask, kHeaderNodeNum, &headerNodePtr, fileRefNum, GPtr->calculatedVCB );
	if (err == noErr)
	{
		*header = (BTHeaderRec *) ((char*)headerNodePtr + sizeof(BTNodeDescriptor));
	
		//	Validate Node Size
		switch ( (**header).nodeSize )	// node size == 512*2^n
		{
			case   512:
			case  1024:
			case  2048:
			case  4096:
			case  8192:
			case 16384:
			case 32768:
				break;
			default:
				RcdError( GPtr, E_InvalidNodeSize );
				return( E_InvalidNodeSize );
		}
	}

	return( err );
}



/*------------------------------------------------------------------------------

Routine:	Alloc[Minor/Major]RepairOrder

Function:	Allocate a repair order node and link into the 'GPtr->RepairXxxxxP" list.
			These are descriptions of minor/major repairs that need to be performed;
			they are compiled during verification, and executed during minor/major repair.

Input:		GPtr	- scavenger globals
			n		- number of extra bytes needed, in addition to standard node size.

Output:		Ptr to node, or NULL if out of memory or other error.
------------------------------------------------------------------------------*/

RepairOrderPtr AllocMinorRepairOrder( SGlobPtr GPtr, int n )						/* #extra bytes needed */
{
	RepairOrderPtr	p;							//	the node we allocate
	
	n += sizeof( RepairOrder );					//	add in size of basic node
	
	p = (RepairOrderPtr) AllocateClearMemory( n );		//	get the node
	
	if ( p != NULL )							//	if we got one...
	{
		p->link = GPtr->MinorRepairsP;			//	then link into list of repairs
		GPtr->MinorRepairsP = p;
	}
	
	if ( GPtr->RepLevel == repairLevelNoProblemsFound )
		GPtr->RepLevel = repairLevelVolumeRecoverable;

	return( p );								//	return ptr to node
}



void	InvalidateCalculatedVolumeBitMap( SGlobPtr GPtr )
{
	short				bufferNumber;
	VolumeBitMapHeader	*volumeBitMap	= GPtr->volumeBitMapPtr;

	volumeBitMap->currentBuffer	= -1;
	
	for ( bufferNumber=0 ; bufferNumber<volumeBitMap->numberOfBuffers ; bufferNumber++ )
	{
		volumeBitMap->bitMapRecord[bufferNumber].processed	= false;
	}
}



//------------------------------------------------------------------------------
//	Routine:	GetVolumeFeatures
//
//	Function:	Sets up some OS and volume specific flags
//
//	Input:		GPtr->DrvNum			The volume to check
//			
//	Output:		GPtr->volumeFeatures	Bit vector
//				GPtr->realVCB			Real in-memory vcb
//------------------------------------------------------------------------------

#if !BSD	
OSErr GetVolumeFeatures( SGlobPtr GPtr )
{
	OSErr					err;
	HParamBlockRec			pb;
	GetVolParmsInfoBuffer	buffer;
	long					response;

	GPtr->volumeFeatures	= 0;					//	Initialize to zero

	//	Get the "real" vcb
	err = GetVCBDriveNum( &GPtr->realVCB, GPtr->DrvNum );
	ReturnIfError( err );

	if ( GPtr->realVCB != nil )
	{
		GPtr->volumeFeatures	|= volumeIsMountedMask;

		pb.ioParam.ioNamePtr	= nil;
		pb.ioParam.ioVRefNum	= GPtr->realVCB->vcbVRefNum;
		pb.ioParam.ioBuffer		= (Ptr) &buffer;
		pb.ioParam.ioReqCount	= sizeof( buffer );
		
		if ( PBHGetVolParms( &pb, false ) == noErr )
		{
			if ( buffer.vMAttrib & (1 << bSupportsTrashVolumeCache) )
				GPtr->volumeFeatures	|= supportsTrashVolumeCacheFeatureMask;
		}
	}
	//	Check if the running system is HFS+ savy
	err = Gestalt( gestaltFSAttr, &response );
	ReturnIfError( err );
	if ( (response & (1 << gestaltFSSupportsHFSPlusVols)) != 0 )
		GPtr->volumeFeatures |= supportsHFSPlusVolsFeatureMask;
	
	return( noErr );
}
#endif



/*-------------------------------------------------------------------------------
Routine:	ClearMemory	-	clear a block of memory

-------------------------------------------------------------------------------*/
#if !BSD
void ClearMemory( void* start, UInt32 length )
{
	UInt32		zero = 0;
	UInt32*		dataPtr;
	UInt8*		bytePtr;
	UInt32		fragCount;		// serves as both a length and quadlong count
								// for the beginning and main fragment
	
	if ( length == 0 )
		return;

	// is request less than 4 bytes?
	if ( length < 4 )				// length = 1,2 or 3
	{
		bytePtr = (UInt8 *) start;
		
		do
		{
			*bytePtr++ = zero;		// clear one byte at a time
		}
		while ( --length );

		return;
	}

	// are we aligned on an odd boundry?
	fragCount = (UInt32) start & 3;

	if ( fragCount )				// fragCount = 1,2 or 3
	{
		bytePtr = (UInt8 *) start;
		
		do
		{
			*bytePtr++ = zero;		// clear one byte at a time
			++fragCount;
			--length;
		}
		while ( (fragCount < 4) && (length > 0) );

		if ( length == 0 )
			return;

		dataPtr = (UInt32*) (((UInt32) start & 0xFFFFFFFC) + 4);	// make it long word aligned
	}
	else
	{
		dataPtr = (UInt32*) ((UInt32) start & 0xFFFFFFFC);			// make it long word aligned
	}

	// At this point dataPtr is long aligned

	// are there odd bytes to copy?
	fragCount = length & 3;
	
	if ( fragCount )
	{
		bytePtr = (UInt8 *) ((UInt32) dataPtr + (UInt32) length - 1);	// point to last byte
		
		length -= fragCount;		// adjust remaining length
		
		do
		{
			*bytePtr-- = zero;		// clear one byte at a time
		}
		while ( --fragCount );

		if ( length == 0 )
			return;
	}

	// At this point length is a multiple of 4

	#if DEBUG_BUILD
	  if ( length < 4 )
		 DebugStr("\p ClearMemory: length < 4");
	#endif

	// fix up beginning to get us on a 64 byte boundary
	fragCount = length & (64-1);
	
	#if DEBUG_BUILD
	  if ( fragCount < 4 && fragCount > 0 )
		  DebugStr("\p ClearMemory: fragCount < 4");
	#endif
	
	if ( fragCount )
	{
		length -= fragCount; 		// subtract fragment from length now
		fragCount >>= 2; 			// divide by 4 to get a count, for DBRA loop
		do
		{
			// clear 4 bytes at a time...
			*dataPtr++ = zero;		
		}
		while (--fragCount);
	}

	// Are we finished yet?
	if ( length == 0 )
		return;
	
	// Time to turn on the fire hose
	length >>= 6;		// divide by 64 to get count
	do
	{
		// spray 64 bytes at a time...
		*dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero;
		*dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero;
		*dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero;
		*dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero; *dataPtr++ = zero;
	}
	while (--length);
}
#endif



void
CopyCatalogName(const CatalogName *srcName, CatalogName *dstName, Boolean isHFSPLus)
{
	UInt32	length;
	
	if ( srcName == NULL )
	{
		if ( dstName != NULL )
			dstName->ustr.length = 0;	// set length byte to zero (works for both unicode and pascal)		
		return;
	}
	
	if (isHFSPLus)
		length = sizeof(UniChar) * (srcName->ustr.length + 1);
	else
		length = sizeof(UInt8) + srcName->pstr[0];

	if ( length > 1 )
		CopyMemory(srcName, dstName, length);
	else
		dstName->ustr.length = 0;	// set length byte to zero (works for both unicode and pascal)		
}


UInt32
CatalogNameLength(const CatalogName *name, Boolean isHFSPlus)
{
	if (isHFSPlus)
		return name->ustr.length;
	else
		return name->pstr[0];
}


UInt32	CatalogNameSize( const CatalogName *name, Boolean isHFSPlus)
{
	UInt32	length = CatalogNameLength( name, isHFSPlus );
	
	if ( isHFSPlus )
		length *= sizeof(UniChar);
	
	return( length );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	BuildCatalogKey
//
//	Function: 	Constructs a catalog key record (ckr) given the parent
//				folder ID and CName.  Works for both classic and extended
//				HFS volumes.
//
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
BuildCatalogKey(HFSCatalogNodeID parentID, const CatalogName *cName, Boolean isHFSPlus, CatalogKey *key)
{
	if ( isHFSPlus )
	{
		key->hfsPlus.keyLength		= kHFSPlusCatalogKeyMinimumLength;	// initial key length (4 + 2)
		key->hfsPlus.parentID			= parentID;		// set parent ID
		key->hfsPlus.nodeName.length	= 0;			// null CName length
		if ( cName != NULL )
		{
			CopyCatalogName(cName, (CatalogName *) &key->hfsPlus.nodeName, isHFSPlus);
			key->hfsPlus.keyLength += sizeof(UniChar) * cName->ustr.length;	// add CName size to key length
		}
	}
	else
	{
		key->hfs.keyLength	= kHFSCatalogKeyMinimumLength;	// initial key length (1 + 4 + 1)
		key->hfs.reserved		= 0;				// clear unused byte
		key->hfs.parentID		= parentID;			// set parent ID
		key->hfs.nodeName[0]	= 0;				// null CName length
		if ( cName != NULL )
		{
			UpdateCatalogName(cName->pstr, key->hfs.nodeName);
			key->hfs.keyLength += key->hfs.nodeName[0];		// add CName size to key length
		}
	}
}


//		Defined in BTreesPrivate.h, but not implemented in the BTree code?
//		So... here's the implementation
SInt32	CompareKeys( BTreeControlBlockPtr btreePtr, KeyPtr searchKey, KeyPtr trialKey )
{
	KeyCompareProcPtr	compareProc = (KeyCompareProcPtr)btreePtr->keyCompareProc;
	
	return( compareProc(searchKey, trialKey) );
}


void
UpdateCatalogName(ConstStr31Param srcName, Str31 destName)
{
	Size length = srcName[0];
	
	if (length > kHFSMaxFileNameChars)
		length = kHFSMaxFileNameChars;		// truncate to max

	destName[0] = length;					// set length byte
	
	CopyMemory(&srcName[1], &destName[1], length);
}


void
UpdateVolumeEncodings(SVCB *volume, TextEncoding encoding)
{
	UInt32	index;

	encoding &= 0x7F;
	
	index = MapEncodingToIndex(encoding);

	volume->vcbEncodingsBitmap |= (1 << index);
		
	// vcb should already be marked dirty
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	ValidVolumeHeader
//
//	Function:	Run some sanity checks to make sure the HFSPlusVolumeHeader is valid
//
// 	Result:		error
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSErr	ValidVolumeHeader( HFSPlusVolumeHeader *volumeHeader )
{
	OSErr	err;
	
	if ( volumeHeader->signature == kHFSPlusSigWord && volumeHeader->version == kHFSPlusVersion )
	{
		if ( (volumeHeader->blockSize != 0) && ((volumeHeader->blockSize & 0x01FF) == 0) )			//	non zero multiple of 512
			err = noErr;
		else
			err = badMDBErr;							//¥¥	I want badVolumeHeaderErr in Errors.i
	}
	else
	{
		err = noMacDskErr;
	}
	
	return( err );
}


//_______________________________________________________________________
//
//	InitBTreeHeader
//	
//	This routine initializes a B-Tree header.
//
//	Note: Since large volumes will have bigger b-trees they need to
//	have map nodes setup.
//_______________________________________________________________________

void InitBTreeHeader (UInt32 fileSize, UInt32 clumpSize, UInt16 nodeSize, UInt16 recordCount, UInt16 keySize,
						UInt32 attributes, UInt32 *mapNodes, void *buffer)
{
	UInt32		 nodeCount;
	UInt32		 usedNodes;
	UInt32		 nodeBitsInHeader;
	BTHeaderRec	 *bth;
	BTNodeDescriptor *ndp;
	UInt32		*bitMapPtr;
	SInt16		*offsetPtr;


	ClearMemory(buffer, nodeSize);			// start out with clean node
	
	nodeCount = fileSize / nodeSize;
	nodeBitsInHeader = 8 * (nodeSize - sizeof(BTNodeDescriptor) - sizeof(BTHeaderRec) - kBTreeHeaderUserBytes - 4*sizeof(SInt16));
	
	usedNodes =	1;							// header takes up one node
	*mapNodes = 0;							// number of map nodes initially (0)


	// FILL IN THE NODE DESCRIPTOR:
	ndp = (BTNodeDescriptor*) buffer;	// point to node descriptor

	ndp->kind = kBTHeaderNode;		// this node contains the B-tree header
	ndp->numRecords = 3;			// there are 3 records (header, map, and user)

	if (nodeCount > nodeBitsInHeader)		// do we need additional map nodes?
	{
		UInt32	nodeBitsInMapNode;
		
		nodeBitsInMapNode = 8 * (nodeSize - sizeof(BTNodeDescriptor) - 2*sizeof(SInt16) - 2);	//¥¥ why (-2) at end???

		if (recordCount > 0)			// catalog B-tree?
			ndp->fLink = 2;			// link points to initial map node
											//¥¥ Assumes all records will fit in one node.  It would be better
											//¥¥ to put the map node(s) first, then the records.
		else
			ndp->fLink = 1;			// link points to initial map node

		*mapNodes = (nodeCount - nodeBitsInHeader + (nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		usedNodes += *mapNodes;
	}

	// FILL IN THE HEADER RECORD:
	bth = (BTHeaderRec*) ((char*)buffer + sizeof(BTNodeDescriptor));	// point to header

	if (recordCount > 0)
	{
		++usedNodes;								// one more node will be used

		bth->treeDepth = 1;							// tree depth is one level (leaf)
		bth->rootNode  = 1;							// root node is also leaf
		bth->firstLeafNode = 1;						// first leaf node
		bth->lastLeafNode = 1;						// last leaf node
	}

	bth->attributes	  = attributes;					// flags for 16-bit key lengths, and variable sized index keys
	bth->leafRecords  = recordCount;				// total number of data records
	bth->nodeSize	  = nodeSize;					// size of a node
	bth->maxKeyLength = keySize;					// maximum length of a key
	bth->totalNodes	  = nodeCount;					// total number of nodes
	bth->freeNodes	  = nodeCount - usedNodes;		// number of free nodes
	bth->clumpSize	  = clumpSize;					//
//	bth->btreeType	  = 0;						// 0 = meta data B-tree


	// FILL IN THE MAP RECORD:
	bitMapPtr = (UInt32*) ((Byte*) buffer + sizeof(BTNodeDescriptor) + sizeof(BTHeaderRec) + kBTreeHeaderUserBytes); // point to bitmap

	// MARK NODES THAT ARE IN USE:
	// Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	*bitMapPtr = ~((UInt32) 0xFFFFFFFF >> usedNodes);
	

	// PLACE RECORD OFFSETS AT THE END OF THE NODE:
	offsetPtr = (SInt16*) ((Byte*) buffer + nodeSize - 4*sizeof(SInt16));

	*offsetPtr++ = sizeof(BTNodeDescriptor) + sizeof(BTHeaderRec) + kBTreeHeaderUserBytes + nodeBitsInHeader/8;	// offset to free space
	*offsetPtr++ = sizeof(BTNodeDescriptor) + sizeof(BTHeaderRec) + kBTreeHeaderUserBytes;						// offset to allocation map
	*offsetPtr++ = sizeof(BTNodeDescriptor) + sizeof(BTHeaderRec);												// offset to user space
	*offsetPtr   = sizeof(BTNodeDescriptor);										// offset to BTH
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	HFSBlocksFromTotalSectors
//
//	Function:	Given the total number of sectors on the volume, calculate
//				the 16Bit number of allocation blocks, and allocation block size.
//
// 	Result:		none
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void	HFSBlocksFromTotalSectors( UInt32 totalSectors, UInt32 *blockSize, UInt16 *blockCount )
{
	UInt16	newBlockSizeInSectors	= 1;
	UInt32	newBlockCount			= totalSectors;
	
	while ( newBlockCount > 0XFFFF )
	{
		newBlockSizeInSectors++;
		newBlockCount	=  totalSectors / newBlockSizeInSectors;
	}
	
	*blockSize	= newBlockSizeInSectors * 512;
	*blockCount	= newBlockCount;
}


/*------------------------------------------------------------------------------

Routine:	CalculateItemCount

Function:	determines number of items for progress feedback

Input:		vRefNum:  the volume to count items

Output:		number of items

------------------------------------------------------------------------------*/

void	CalculateItemCount( SGlob *GPtr, UInt64 *itemCount, UInt64 *onePercent )
{
	BTreeControlBlock	*btcb;
	UInt64				items;
	UInt32				realFreeNodes;
	SVCB*		vcb				= GPtr->calculatedVCB;
	
	items = 0;
	
	//
	// Items is the used node count and leaf record count for each btree...
	//

	btcb = (BTreeControlBlock*) vcb->vcbCatalogFile->fcbBtree;
	realFreeNodes = ((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount;
	
	items = btcb->leafRecords * ( GPtr->volumeBitMapPtr->numberOfBuffers + 2 );
	items += (btcb->totalNodes - realFreeNodes);

	btcb = (BTreeControlBlock*) vcb->vcbExtentsFile->fcbBtree;
	realFreeNodes = ((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount;
	
	items += btcb->leafRecords + (btcb->totalNodes - realFreeNodes);

	if ( vcb->vcbAttributesFile != NULL )
	{
		btcb = (BTreeControlBlock*) vcb->vcbAttributesFile->fcbBtree;
		realFreeNodes = ((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount;
	
		items += (btcb->leafRecords + (btcb->totalNodes - realFreeNodes));
	}
	
	*onePercent = items/ 100;
	
	//
	//	[2239291]  We're calculating the progress for the wrapper and the embedded volume separately, which
	//	confuses the caller (since they see the progress jump to a large percentage while checking the wrapper,
	//	then jump to a small percentage when starting to check the embedded volume).  To avoid this behavior,
	//	we pretend the wrapper has 100 times as many items as it really does.  This means the progress will
	//	never exceed 1% for the wrapper.
	//
	if ( (GPtr->volumeType == kEmbededHFSPlusVolumeType) && (GPtr->inputFlags & examineWrapperMask) )
		items *= 100;
	
	//	Add en extra Å 5% to smooth the progress
	items += *onePercent * 5;
	
	*itemCount = items;
}


SFCB* ResolveFCB(short fileRefNum)
{
	return( (SFCB*)((unsigned long)GetFCBSPtr() + (unsigned long)fileRefNum)  );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Routine:	SetupFCB fills in the FCB info
//
//	Returns:	The filled up FCB
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void	SetupFCB( SVCB *vcb, SInt16 refNum, UInt32 fileID, UInt32 fileClumpSize )
{
	SFCB *fcb;

	fcb = ResolveFCB(refNum);
	
	fcb->fcbFileID		= fileID;
	fcb->fcbVolume		= vcb;
	fcb->fcbClumpSize	= fileClumpSize;
}


//******************************************************************************
//
//	Routine:	ResolveFileRefNum
//
//	Purpose:	Return a file reference number for a given file control block
//				pointer.
//
//	Input:
//		fileCtrlBlockPtr	Pointer to the SFCB
//
//	Output:
//		result				File reference number,
//							or 0 if fileCtrlBlockPtr is invalid
//
pascal short ResolveFileRefNum(SFCB * fileCtrlBlockPtr)
{
	return( (unsigned long)fileCtrlBlockPtr - (unsigned long)GetFCBSPtr() );
}



Ptr gFCBSPtr;

void	SetFCBSPtr( Ptr value )
{
	gFCBSPtr = value;
}

Ptr	GetFCBSPtr( void )
{
	return (gFCBSPtr);
}


//_______________________________________________________________________
//
//	Routine:	FlushVolumeControlBlock
//	Arguments:	SVCB		*vcb
//	Output:		OSErr			err
//
//	Function: 	Flush volume information to either the HFSPlusVolumeHeader of the Master Directory Block
//_______________________________________________________________________

OSErr	FlushVolumeControlBlock( SVCB *vcb )
{
	OSErr				err;
	HFSPlusVolumeHeader	*volumeHeader;
	SFCB				*fcb;
//	ExtendedFCB			*extendedFCB;
	
	if ( ! IsVCBDirty( vcb ) )			//	if it's not dirty
		return( noErr );

	if ( vcb->vcbSignature == kHFSPlusSigWord )
	{		
		err = GetBlock_glue( gbDefault, (vcb->vcbEmbeddedOffset / 512) + 2, (Ptr*)&volumeHeader, vcb->vcbVRefNum, vcb );	//	no Flags, VH is always block #0, vcb->vcbBufAdr == CacheQueueHeader
		ReturnIfError( err );
		
		//	2005507, Keep the MDB creation date and HFSPlusVolumeHeader creation date in sync.
		if ( vcb->vcbEmbeddedOffset != 0 )								//	It's a wrapped HFS+ volume
		{
			HFSMasterDirectoryBlock	*mdb;
			err = GetBlock_glue( gbDefault, 2, (Ptr*)&mdb, vcb->vcbVRefNum, vcb );
			if ( err == noErr )
			{
				if ( mdb->drCrDate != vcb->vcbCreateDate )	//	The creation date changed
				{
					mdb->drCrDate = vcb->vcbCreateDate;
					err = ReleaseCacheBlock((Ptr)mdb, rbWriteMask);
				}
				else
				{
					err = ReleaseCacheBlock((Ptr)mdb, rbFreeMask);
				}
			}
		}

	//	volumeHeader->signature			= vcb->signature;
	//	volumeHeader->version			=
	
		volumeHeader->attributes		= vcb->vcbAttributes;
		volumeHeader->lastMountedVersion	= kHFSPlusMountVersion;		//	Let others know we've mounted volume for writing
	//¥¥	volumeName
		volumeHeader->createDate		= vcb->vcbCreateDate;				//	NOTE: local time, not GMT!
		volumeHeader->modifyDate		= vcb->vcbModifyDate;
		volumeHeader->backupDate		= vcb->vcbBackupDate;
		volumeHeader->checkedDate		= vcb->vcbCheckedDate;
		volumeHeader->fileCount			= vcb->vcbFileCount;
		volumeHeader->folderCount		= vcb->vcbFolderCount;
		volumeHeader->blockSize			= vcb->vcbBlockSize;
		volumeHeader->totalBlocks		= vcb->vcbTotalBlocks;
		volumeHeader->freeBlocks		= vcb->vcbFreeBlocks;
		volumeHeader->nextAllocation		= vcb->vcbNextAllocation;
		volumeHeader->rsrcClumpSize		= vcb->vcbRsrcClumpSize;
		volumeHeader->dataClumpSize		= vcb->vcbDataClumpSize;
		volumeHeader->nextCatalogID		= vcb->vcbNextCatalogID;
		volumeHeader->writeCount		= vcb->vcbWriteCount;
		volumeHeader->encodingsBitmap		= vcb->vcbEncodingsBitmap;

		//¥¥Êshould we use the vcb or fcb clumpSize values ????? -djb
		volumeHeader->allocationFile.clumpSize	= vcb->vcbAllocationFile->fcbClumpSize;
		volumeHeader->extentsFile.clumpSize	= vcb->vcbExtentsFile->fcbClumpSize;
		volumeHeader->catalogFile.clumpSize	= vcb->vcbCatalogFile->fcbClumpSize;
		
		CopyMemory( vcb->vcbFinderInfo, volumeHeader->finderInfo, sizeof(volumeHeader->finderInfo) );
	
		fcb = vcb->vcbExtentsFile;
	//	extendedFCB = GetExtendedFCB( fcb );
		CopyMemory( fcb->fcbExtents32, volumeHeader->extentsFile.extents, sizeof(HFSPlusExtentRecord) );
		volumeHeader->extentsFile.logicalSize = fcb->fcbLogicalSize;
		volumeHeader->extentsFile.totalBlocks = fcb->fcbPhysicalSize / vcb->vcbBlockSize;
	
		fcb = vcb->vcbCatalogFile;
	//	extendedFCB = GetExtendedFCB( fcb );
		CopyMemory( fcb->fcbExtents32, volumeHeader->catalogFile.extents, sizeof(HFSPlusExtentRecord) );
		volumeHeader->catalogFile.logicalSize = fcb->fcbLogicalSize;
		volumeHeader->catalogFile.totalBlocks = fcb->fcbPhysicalSize / vcb->vcbBlockSize;

		fcb = vcb->vcbAllocationFile;
	//	extendedFCB = GetExtendedFCB( fcb );
		CopyMemory( fcb->fcbExtents32, volumeHeader->allocationFile.extents, sizeof(HFSPlusExtentRecord) );
		volumeHeader->allocationFile.logicalSize = fcb->fcbLogicalSize;
		volumeHeader->allocationFile.totalBlocks = fcb->fcbPhysicalSize / vcb->vcbBlockSize;
		
		if (vcb->vcbAttributesFile != NULL)	//	Only update fields if an attributes file existed and was open
		{
			fcb = vcb->vcbAttributesFile;
		//	extendedFCB = GetExtendedFCB( fcb );
			CopyMemory( fcb->fcbExtents32, volumeHeader->attributesFile.extents, sizeof(HFSPlusExtentRecord) );
			volumeHeader->attributesFile.logicalSize = fcb->fcbLogicalSize;
			volumeHeader->attributesFile.clumpSize = fcb->fcbClumpSize;
			volumeHeader->attributesFile.totalBlocks = fcb->fcbPhysicalSize / vcb->vcbBlockSize;
		}
		
		//--	Write the MDB out by releasing the block dirty
		
		err = ReleaseCacheBlock( (Ptr)volumeHeader, rbWriteMask );
		
		MarkVCBClean( vcb );
	}
	else
	{
		// This routine doesn't really return an error!!!
		// So for now, we will just return noErr
//		err = FlushMDB( vcb );		//	Go flush the VCB info BEFORE close
		return( noErr );
	}

	return( err );
}


//_______________________________________________________________________
//
//	Routine:	FlushAlternateVolumeControlBlock
//	Arguments:	SVCB		*vcb
//				Boolean			ifHFSPlus
//	Output:		OSErr			err
//
//	Function: 	Flush volume information to either the Alternate HFSPlusVolumeHeader of the 
//				Alternate Master Directory Block.  Called by the BTree when the catalog
//				or extent files grow.  Simply BlockMoves the original to the alternate
//				location.
//_______________________________________________________________________

OSErr	FlushAlternateVolumeControlBlock( SVCB *vcb, Boolean isHFSPlus )
{
	OSErr			err;
	UInt32			alternateBlockLocation;

	err = FlushVolumeControlBlock( vcb );
	
	if ( isHFSPlus )															//	Flush the HFSPlusVolumeHeader
	{
		HFSPlusVolumeHeader	*volumeHeader;
		HFSPlusVolumeHeader	*alternateVolumeHeader;

		//--	Get the HFSPlusVolumeHeader Block, 1st block in the HFS Plus partition
		err = GetBlock_glue( gbDefault, (vcb->vcbEmbeddedOffset / 512) + 2, (Ptr*)&volumeHeader, vcb->vcbVRefNum, vcb );	//	VH is always 1st sector of HFS Plus partition
		ReturnIfError( err );
		
		//--	Get the Alternate HFSPlusVolumeHeader Block, last sector in the HFS Plus partition
		alternateBlockLocation = (vcb->vcbEmbeddedOffset / 512) + vcb->vcbTotalBlocks * (vcb->vcbBlockSize / 512) - 2;
		err = GetBlock_glue( gbDefault, alternateBlockLocation, (Ptr*)&alternateVolumeHeader, vcb->vcbVRefNum, vcb );	//	Alt VH is always 2nd to last sector of HFS Plus partition
		if ( err == noErr )
		{
			//--	Copy the HFSPlusVolumeHeader to the alternate location
			CopyMemory( volumeHeader, alternateVolumeHeader, sizeof(HFSPlusVolumeHeader) );
	
			//--	And write it back to the disk, NOW!
			err = ReleaseCacheBlock( (Ptr)volumeHeader, rbFreeMask );
			err = ReleaseCacheBlock( (Ptr)alternateVolumeHeader, rbWriteMask );
		}
		else
		{			
			err = ReleaseCacheBlock( (Ptr)volumeHeader, rbFreeMask );
		}
	}
	else																		//	Flush the HFSMasterDirectoryBlock
	{
		HFSMasterDirectoryBlock	*mdb;
		HFSMasterDirectoryBlock	*alternateMDB;
		UInt32 sectors, sectorSize;

		err = GetDeviceSize( vcb->vcbDriveNumber, &sectors, &sectorSize );
		ReturnIfError( err );

		//--	Get the MDB, 2nd block on HFS disk
		err = GetBlock_glue( gbDefault, 2, (Ptr*)&mdb, vcb->vcbVRefNum, vcb );	//	no Flags, MDB is always block
		ReturnIfError( err );

		//--	Get the Alternate MDB, 2nd to last block on disk
		alternateBlockLocation = sectors - 2;
		err = GetBlock_glue( gbDefault, alternateBlockLocation, (Ptr*)&alternateMDB, vcb->vcbVRefNum, vcb );	//	no Flags, VH is always block #0, vcb->vcbBufAdr == CacheQueueHeader
		if ( err == noErr )
		{
			//--	Copy the MDB to the alternate location
			CopyMemory( mdb, alternateMDB, sizeof(HFSMasterDirectoryBlock) );
			
			//--	And write it back to the disk, NOW!
			err = ReleaseCacheBlock( (Ptr)mdb, rbFreeMask );
			err = ReleaseCacheBlock( (Ptr)alternateMDB, rbWriteMask );
		}
		else
		{
			err = ReleaseCacheBlock( (Ptr)mdb, rbFreeMask );
		}
	}

	return( err );
}

void
ConvertToHFSPlusExtent( const HFSExtentRecord oldExtents, HFSPlusExtentRecord newExtents)
{
	UInt16	i;

	// go backwards so we can convert in place!
	
	for (i = kHFSPlusExtentDensity-1; i > 2; --i)
	{
		newExtents[i].blockCount = 0;
		newExtents[i].startBlock = 0;
	}

	newExtents[2].blockCount = oldExtents[2].blockCount;
	newExtents[2].startBlock = oldExtents[2].startBlock;
	newExtents[1].blockCount = oldExtents[1].blockCount;
	newExtents[1].startBlock = oldExtents[1].startBlock;
	newExtents[0].blockCount = oldExtents[0].blockCount;
	newExtents[0].startBlock = oldExtents[0].startBlock;
}


/* Stub routines */

OSErr TrashAllFSCaches( SVCB *vcb )
{
	return (0);
}

void TrashVolumeDiskCache( SVCB *vcb )
{
}
