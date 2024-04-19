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
	File:		SVerify1.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.

*/

//#include <MacTypes.h>
//#include <Errors.h>

#include "Scavenger.h"

//	internal routine prototypes

static	int	RcdValErr( SGlobPtr GPtr, OSErr type, UInt32 correct, UInt32 incorrect, HFSCatalogNodeID parid );

static	int	RcdFThdErr( SGlobPtr GPtr, UInt32 fid );

static	int	RcdNoDirErr( SGlobPtr GPtr, UInt32 did );
		
static	int	RcdNameLockedErr( SGlobPtr GPtr, OSErr type, UInt32 incorrect );

static	int	RcdCustomIconErr( SGlobPtr GPtr, OSErr type, UInt32 incorrect );
	
//static	OSErr	RcdOrphanedExtentErr ( SGlobPtr GPtr, SInt16 type, void *theKey );

static	OSErr	RcdMDBEmbededVolDescriptionErr( SGlobPtr GPtr, OSErr type, HFSMasterDirectoryBlock *mdb );

static	OSErr	RcdInvalidWrapperExtents( SGlobPtr GPtr, OSErr type );

static	OSErr	CustomIconCheck ( SGlobPtr GPtr, HFSCatalogNodeID folderID, UInt16 frFlags );

static	OSErr	CheckNodesFirstOffset( SGlobPtr GPtr, BTreeControlBlock *btcb );

static	Boolean	ExtentInfoExists( ExtentsTable **extentsTableH, ExtentInfo *extentInfo );

static	OSErr	CheckWrapperExtents( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb );

OSErr	ScavengeVolumeType( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb, UInt16 *volumeType );
OSErr	SeekVolumeHeader( SGlobPtr GPtr, UInt32 startSector, UInt32 numSectors, UInt32 *vHSector );


/*
 * Check if a volume is clean (unmounted safely)
 *
 * returns:    -1 not an HFS/HFS+ volume
 *		0 dirty
 *		1 clean
 *
 * if markClean is true it will mark volume clean
 */
int
CheckForClean(SGlobPtr GPtr, Boolean markClean)
{
#define kIDSector 2

	OSErr err;
	UInt16 volAttr;
	HFSMasterDirectoryBlock	*mdbp;
	HFSPlusVolumeHeader *vhp;
	SVCB *vcbp = GPtr->calculatedVCB;
	UInt16 rbOptions;

	volAttr = 0;
	vhp = NULL;
	rbOptions = 0;

	err = GetBlock_glue(gbDefault, kIDSector, (Ptr *)&mdbp, vcbp->vcbVRefNum, vcbp);
	if (err) return (-1);

	if (mdbp->drSigWord == kHFSPlusSigWord) {
		vhp = (HFSPlusVolumeHeader *)mdbp;

	} else if (mdbp->drSigWord == kHFSSigWord) {

		if (mdbp->drEmbedSigWord == kHFSPlusSigWord) {
			UInt32 vhSector;
			UInt32 blkSectors;
			
			blkSectors = mdbp->drAlBlkSiz / 512;
			vhSector  = mdbp->drAlBlSt;
			vhSector += blkSectors * mdbp->drEmbedExtent.startBlock;
			vhSector += kIDSector;
	
			err = GetBlock_glue(gbDefault, vhSector, (Ptr*)&vhp, vcbp->vcbVRefNum, vcbp);
			if (err) return (-1);

		} else /* plain old HFS */ {
			volAttr = mdbp->drAtrb;
			if (markClean) {
				mdbp->drAtrb |= kHFSVolumeUnmountedMask;
				rbOptions = rbWriteMask;
			}
		}
	} else {
		return (-1);
	}
	
	if (vhp != NULL) {
		if (ValidVolumeHeader(vhp) == noErr) {
			volAttr = (UInt16) vhp->attributes;
			if (markClean) {
				vhp->attributes |= kHFSVolumeUnmountedMask;
				rbOptions = rbWriteMask;
			}
		} else {
			return (-1);
		}

		if ((void *)vhp != (void *)mdbp) {
			(void) ReleaseCacheBlock((Ptr)vhp, rbOptions);
			rbOptions = 0; /* we just wrote the header */
		}
	}

	(void) ReleaseCacheBlock((Ptr)mdbp, rbOptions);

	return ((volAttr & kHFSVolumeUnmountedMask) != 0);
}


/*------------------------------------------------------------------------------

Function:	IVChk - (Initial Volume Check)

Function:	Performs an initial check of the volume to be scavenged to confirm
			that the volume can be accessed and that it is a HFS/HFS+ volume.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		IVChk	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

OSErr IVChk( SGlobPtr GPtr )
{
	#define					kBitsPerSector	4096
	#define					kLog2SectorSize	9
	UInt32					bitMapSizeInSectors;
	OSErr					err;
	HFSMasterDirectoryBlock	*alternateMDB;
	HFSPlusVolumeHeader		*alternateVolumeHeader;
	UInt32					numABlks;
	UInt32					alternateBlockLocation;
	UInt32					minABlkSz;
	UInt32					totalSectors, sectorSize;
	UInt32					maxNumberOfAllocationBlocks;
	UInt32					realAllocationBlockSize;
	UInt32					realTotalBlocks;
	UInt32					hfsBlockSize;
	UInt32					hfsBlockCount;
	UInt32					i;
	UInt32					hfsPlusIOPosOffset;
//	SFCB				*fcb;
	BTreeControlBlock		*btcb;
	SVCB				*calculatedVCB	= GPtr->calculatedVCB;
	
	//  Set up

	GPtr->TarID = AMDB_FNum;	//	target = alt MDB
	GPtr->TarBlock	= 0;
		
	// Determine volume size
	err = GetDeviceSize( GPtr->calculatedVCB->vcbDriveNumber, &totalSectors, &sectorSize);
	if ( (totalSectors < 3) || (err != noErr) )
		return( 123 );
	
	//	Get the Alternate MDB, 2nd to last block on disk
	//	On HFS+ disks this is still the HFS wrapper altMDB
	//	On HFS+ wrapperless disks, it's the AltVH
	alternateBlockLocation = totalSectors - 2;
	err = GetBlock_glue( gbDefault, alternateBlockLocation, (Ptr *)&alternateMDB, calculatedVCB->vcbVRefNum, calculatedVCB );	//	no Flags, MDB is always block
	ReturnIfError( err );
#if 0
	if (preen == 0) {
		printf("** Last Mounted on \"%s\"\n", &mdblock.drVN[1] /*sblock.fs_fsmnt*/);
		if (hotroot)
			printf("** Root file system\n");
#endif
	if ( alternateMDB->drSigWord == kHFSPlusSigWord )			//	Is it a wrapperless HFS+ volume
	{
		alternateVolumeHeader	= (HFSPlusVolumeHeader *)alternateMDB;
		GPtr->volumeType		= kPureHFSPlusVolumeType;
		GPtr->isHFSPlus			= true;
		WriteMsg( GPtr, M_CheckingHFSPlusVolume, kStatusMessage );
	}
	else if ( alternateMDB->drSigWord == kHFSSigWord )			//  Deturmine if its an HFS volume
	{
		//	Volume Type is constant, weather we are examining wrapper or HFS+ volume.
		//	Detect if this is a wrapped HFS+ volume.
		DFALockBlock( (Ptr) alternateMDB );
		err = ScavengeVolumeType( GPtr, alternateMDB, &GPtr->volumeType );
				
		if ( ((GPtr->inputFlags & examineWrapperMask) == 0) && (alternateMDB->drEmbedSigWord == kHFSPlusSigWord) )	//	Wrappered HFS+ volume
		{
			WriteMsg( GPtr, M_CheckingHFSPlusVolume, kStatusMessage );
			GPtr->isHFSPlus	= true;
		}
		else													//	plain old HFS
		{
			WriteMsg( GPtr, M_CheckingHFSVolume, kStatusMessage );
			GPtr->isHFSPlus	= false;
		//	calculatedVCB->allocationsRefNum	= 0;
		//	calculatedVCB->attributesRefNum		= 0;
		}
				
		if ( err == E_InvalidMDBdrAlBlSt )
			err = RcdMDBEmbededVolDescriptionErr( GPtr, E_InvalidMDBdrAlBlSt, alternateMDB );
		
	}
	else
	{
		err = R_BadSig;											//	doesn't bear the HFS signature
		goto ReleaseAndBail;
	}
	
	
	//
	//	If we're checking an HFS+ volume's wrapper, check for bad extents
	//
	
	if ( ((GPtr->inputFlags & examineWrapperMask) != 0) && (alternateMDB->drEmbedSigWord == kHFSPlusSigWord) )
	{
		err = CheckWrapperExtents( GPtr, alternateMDB );
		if (err != noErr)
			goto ReleaseAndBail;
	}
	
	//
	//	If this is an HFS+ disk
	//
	
	if ( GPtr->isHFSPlus == true )
	{	
		GPtr->numExtents = kHFSPlusExtentDensity;
		calculatedVCB->vcbSignature = kHFSPlusSigWord;
		
		//	Read the HFS+ VolumeHeader
		if ( GPtr->volumeType == kPureHFSPlusVolumeType )
		{
			hfsPlusIOPosOffset	=	0;			//	alternateBlockLocation is already set up
			HFSBlocksFromTotalSectors( totalSectors, &hfsBlockSize, (UInt16*)&hfsBlockCount );
		}
		else
		{
			totalSectors	= alternateMDB->drEmbedExtent.blockCount * ( alternateMDB->drAlBlkSiz / Blk_Size );
			hfsBlockSize	= alternateMDB->drAlBlkSiz;
			hfsBlockCount	= alternateMDB->drNmAlBlks;

			err = GetVolumeHeader( GPtr, alternateMDB, &alternateVolumeHeader, &alternateBlockLocation, &hfsPlusIOPosOffset );
		}
		
		err = ValidVolumeHeader( alternateVolumeHeader );
		
		//	If the alternate VolumeHeader is bad, just use the real VolumeHeader
		if ( err != noErr )
		{
			WriteError( GPtr, E_InvalidVolumeHeader, 1, 0 );
			err = E_InvalidVolumeHeader;								//	doesn't bear the HFS signature
			goto ReleaseAndBail;
		}
	
		//	Further populate the VCB with VolumeHeader info
		calculatedVCB->vcbAlBlSt = hfsPlusIOPosOffset / 512;
		calculatedVCB->vcbEmbeddedOffset = hfsPlusIOPosOffset;

		maxNumberOfAllocationBlocks	= 0xFFFFFFFF;
		realAllocationBlockSize		= alternateVolumeHeader->blockSize;
		realTotalBlocks				= alternateVolumeHeader->totalBlocks;
		calculatedVCB->vcbNextCatalogID	= alternateVolumeHeader->nextCatalogID;
		calculatedVCB->vcbCreateDate	= alternateVolumeHeader->createDate;
		
		if ( alternateVolumeHeader->attributesFile.totalBlocks == 0 )
			calculatedVCB->vcbAttributesFile = NULL;	/* XXX memory leak ? */

		//	Make sure the Extents B-Tree is set to use 16-bit key lengths.  We access it before completely setting
		//	up the control block.
		btcb = (BTreeControlBlock *) calculatedVCB->vcbExtentsFile->fcbBtree;
		btcb->attributes |= kBTBigKeysMask;
	}
	else	//	It's an HFS disk
	{
		GPtr->numExtents			= kHFSExtentDensity;
		calculatedVCB->vcbSignature	= alternateMDB->drSigWord;
		totalSectors				= alternateBlockLocation;
		maxNumberOfAllocationBlocks	= 0xFFFF;
		calculatedVCB->vcbNextCatalogID	= alternateMDB->drNxtCNID;			//	set up next file ID, CheckBTreeKey makse sure we are under this value
		calculatedVCB->vcbCreateDate	= alternateMDB->drCrDate;

		realAllocationBlockSize		= alternateMDB->drAlBlkSiz;
		realTotalBlocks				= alternateMDB->drNmAlBlks;
		hfsBlockSize				= alternateMDB->drAlBlkSiz;
		hfsBlockCount				= alternateMDB->drNmAlBlks;
	}
	
	
	GPtr->idSector	= alternateBlockLocation;							//	Location of ID block, AltMDB, MDB, AltVH or VH
	GPtr->TarBlock	= alternateBlockLocation;							//	target block = alt MDB

	//  verify volume allocation info
	//	Note: i is the number of sectors per allocation block
 	numABlks = totalSectors;
 	minABlkSz = Blk_Size;												//	init minimum ablock size
	for( i = 2; numABlks > maxNumberOfAllocationBlocks; i++ )			//	loop while #ablocks won't fit
	{
		minABlkSz = i * Blk_Size;										//	jack up minimum
		numABlks  = alternateBlockLocation / i;							//	recompute #ablocks, assuming this size
	}
	
	if ((realAllocationBlockSize >= minABlkSz) && (realAllocationBlockSize <= Max_ABSiz) && ((realAllocationBlockSize % Blk_Size) == 0))
	{
	//	calculatedVCB->vcbBlockSize = hfsBlockSize;
		calculatedVCB->vcbBlockSize = realAllocationBlockSize;
		numABlks = totalSectors / ( realAllocationBlockSize / Blk_Size );	//	max # of alloc blks
	}
	else
	{
		RcdError( GPtr, E_ABlkSz );
		err = E_ABlkSz;													//	bad allocation block size
		goto ReleaseAndBail;
	}		
	
	//	Calculate the volume bitmap size
	bitMapSizeInSectors	= ( numABlks + kBitsPerSector - 1 ) / kBitsPerSector;			//	VBM size in blocks
	
//	calculatedVCB->vcbNmAlBlks	= hfsBlockCount;
//	calculatedVCB->vcbFreeBks	= LongToShort( realTotalBlocks );
	calculatedVCB->vcbTotalBlocks	= realTotalBlocks;
	calculatedVCB->vcbFreeBlocks	= realTotalBlocks;
	
	//	Only do these tests on HFS volumes, since they are either irrellivent
	//	or, getting the VolumeHeader would have already failed.

	if ( GPtr->isHFSPlus == false )
	{

	//¥¥	Calculate the validaty of HFS+ Allocation blocks, I think realTotalBlocks == numABlks
		numABlks = (totalSectors - 3 - bitMapSizeInSectors) / (realAllocationBlockSize / Blk_Size);	//	actual # of alloc blks

		if ( realTotalBlocks > numABlks )
		{
			RcdError( GPtr, E_NABlks );
			err = E_NABlks;								//	invalid number of allocation blocks
			goto ReleaseAndBail;
		}

		if ( alternateMDB->drVBMSt <= MDB_BlkN )
		{
			RcdError(GPtr,E_VBMSt);
			err = E_VBMSt;								//	invalid VBM start block
			goto ReleaseAndBail;
		}	
		calculatedVCB->vcbVBMSt = alternateMDB->drVBMSt;
		
		if (alternateMDB->drAlBlSt < (alternateMDB->drVBMSt + bitMapSizeInSectors))
		{
			RcdError(GPtr,E_ABlkSt);
			err = E_ABlkSt;								//	invalid starting alloc block
			goto ReleaseAndBail;
		}
		calculatedVCB->vcbAlBlSt = alternateMDB->drAlBlSt;
	}
	
	//
	//	allocate memory for DFA's volume bit map
	//
	{
		UInt32	safeMemSize;
		UInt32	numberOfBitMapBuffers;									//	how many buffers do we need to read the bitmap
		UInt32	bitmapSizeInBytes;										//	 round up to the nearest sector or allocation block
		Boolean	useTempMem;
		UInt32	bitsPerBlock		= realAllocationBlockSize * 8;
		if ( GPtr->isHFSPlus == true )
			bitmapSizeInBytes	= ( ( alternateVolumeHeader->totalBlocks + bitsPerBlock - 1 ) / bitsPerBlock ) * realAllocationBlockSize;	//	round up to the nearest allocation block
		else
			bitmapSizeInBytes = (((totalSectors / (realAllocationBlockSize >> kLog2SectorSize)) + kBitsPerSector-1) / kBitsPerSector) * Blk_Size;	//	round up to the nearest sector
		
		safeMemSize = CalculateSafePhysicalTempMem( &useTempMem );
		safeMemSize = ( safeMemSize / Blk_Size ) * Blk_Size;	//	keep it a multiple of Blk_Size bytes
		
		if ( bitmapSizeInBytes < safeMemSize )
			safeMemSize = bitmapSizeInBytes;
			
		//	split it into portions for large bit maps
		numberOfBitMapBuffers = ( bitmapSizeInBytes + safeMemSize - 1 ) / safeMemSize;
		
		GPtr->volumeBitMapPtr = (VolumeBitMapHeader *) AllocateClearMemory( numberOfBitMapBuffers * sizeof(BitMapRec) + sizeof(VolumeBitMapHeader) );
		
		#if BSD
		{
			void * p;
			
			p = AllocateClearMemory(safeMemSize);
			if (p == nil)
			{
				err = R_NoMem;												//	not enough memory
				goto ReleaseAndBail;
			}
			GPtr->volumeBitMapPtr->buffer = p;
		}
		#else
		{
			Handle	h;

			//	Allocate space from the heap if our buffer is under 64K
			if ( (safeMemSize <= 64*1024) || (useTempMem == false) )
			{
				h = NewHandleClear( safeMemSize );
			}
			else	//	Allocate it from TempMem
			{
				h = TempNewHandle( safeMemSize, &err );
				if ( err )
					h = nil;
			}
	
			if ( h == nil ) 
			{
				err = R_NoMem;												//	not enough memory
				goto ReleaseAndBail;
			}
		
			HLock( h );
			GPtr->volumeBitMapPtr->buffer = *h;
		}
		#endif
		
		//	Set up the Volume bit map structure fields, all other fields are 0/false
		ClearMemory ( GPtr->volumeBitMapPtr->buffer, safeMemSize );
		GPtr->volumeBitMapPtr->bitMapSizeInBytes	= bitmapSizeInBytes;
		GPtr->volumeBitMapPtr->numberOfBuffers		= numberOfBitMapBuffers;
		GPtr->volumeBitMapPtr->bufferSize			= safeMemSize;

		InvalidateCalculatedVolumeBitMap( GPtr );	//	no buffer read yet
	}

ReleaseAndBail:
	if ( GPtr->isHFSPlus )
		(void) ReleaseCacheBlock( (Ptr)alternateVolumeHeader, 0 );		//	release it, to unlock buffer

	if ( GPtr->volumeType != kPureHFSPlusVolumeType )
		(void) ReleaseCacheBlock( (Ptr)alternateMDB, 0 );					//	Release it, to unlock buffer
	
	return( err );		
}


OSErr	GetVolumeHeader( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb, HFSPlusVolumeHeader **alternateVolumeHeader, UInt32 *idSector, UInt32 *hfsPlusIOPosOffset )
{
	OSErr					err;
	SVCB				*calculatedVCB			= GPtr->calculatedVCB;
	UInt32					totalHFSPlusSectors		= (mdb->drAlBlkSiz / 512) * mdb->drEmbedExtent.blockCount;
	
	*hfsPlusIOPosOffset		=	(mdb->drEmbedExtent.startBlock * mdb->drAlBlkSiz) + ( mdb->drAlBlSt * 512 );

	*idSector	= mdb->drAlBlSt + ((mdb->drAlBlkSiz / 512) * mdb->drEmbedExtent.startBlock) + totalHFSPlusSectors - 2;	//	2nd to last sector
	
	err = GetBlock_glue( gbDefault, *idSector, (Ptr*) alternateVolumeHeader, calculatedVCB->vcbVRefNum, calculatedVCB );
	if ( err == noErr )	err = ValidVolumeHeader( *alternateVolumeHeader );
	
	if ( err != noErr )		//	If the alternate VolumeHeader is bad, just use the real VolumeHeader
	{
		*idSector		= (mdb->drEmbedExtent.startBlock * mdb->drAlBlkSiz / 512) + mdb->drAlBlSt + 2;

		err = GetBlock_glue( gbDefault, *idSector, (Ptr*) alternateVolumeHeader, calculatedVCB->vcbVRefNum, calculatedVCB );	//	VH is always 3rd sector of HFS+ partition
		if ( err == noErr )	err = ValidVolumeHeader( *alternateVolumeHeader );
	}
	
	return( err );
}


OSErr	ScavengeVolumeType( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb, UInt16 *volumeType  )
{
	UInt32					vHSector;
	UInt32					totalSectors;
	UInt32					sectorSize;
	UInt32					startSector;
	UInt32					altVHSector;
	UInt32					sectorsPerBlock;
	UInt32					hfsPlusSectors = 0;
	UInt32					numSectorsToSearch;
	OSErr					err;
	HFSPlusVolumeHeader 	*volumeHeader;
	HFSExtentDescriptor		embededExtent;
	SVCB				*calculatedVCB			= GPtr->calculatedVCB;
	UInt16					embedSigWord			= mdb->drEmbedSigWord;

	*volumeType	= kEmbededHFSPlusVolumeType;		//	Assume HFS+
	
	//
	//	First see if it is an HFS+ volume and the relevent structures look OK
	//
	if ( embedSigWord == kHFSPlusSigWord )
	{
		vHSector	= mdb->drAlBlSt + ((mdb->drAlBlkSiz / 512) * mdb->drEmbedExtent.startBlock) + 2;	//	2nd to last sector

		err		= GetBlock_glue( gbDefault, vHSector, (Ptr*) &volumeHeader, calculatedVCB->vcbVRefNum, calculatedVCB );	//	Alt VH is always last sector of HFS+ partition
		if ( err != noErr ) goto AssumeHFS;

		err		= ValidVolumeHeader( volumeHeader );
		(void) ReleaseCacheBlock( (Ptr)volumeHeader, 0 );		//	release it, to unlock buffer
		if ( err == noErr )
			return( noErr );
	}
	

	sectorsPerBlock = mdb->drAlBlkSiz / 512;

	//	Search the end of the disk to see if a Volume Header is present at all
	if ( embedSigWord != kHFSPlusSigWord )
	{
		err = GetDeviceSize( GPtr->calculatedVCB->vcbDriveNumber, &totalSectors, &sectorSize );
		if ( err != noErr ) goto AssumeHFS;
		
		numSectorsToSearch = mdb->drAlBlkSiz / sectorSize;
		startSector = totalSectors - 4 - numSectorsToSearch;
		
		err = SeekVolumeHeader( GPtr, startSector, numSectorsToSearch, &altVHSector );
		if ( err != noErr ) goto AssumeHFS;
		
		//	We found the Alt VH, so this must be a damaged embeded HFS+ volume
		//	Now Scavenge for the VolumeHeader
		startSector = mdb->drAlBlSt + (4 * sectorsPerBlock);		// Start looking at 4th HFS allocation block
		numSectorsToSearch = 10 * sectorsPerBlock;			// search for VH in next 10 allocation blocks
		
		err = SeekVolumeHeader( GPtr, startSector, numSectorsToSearch, &vHSector );
		if ( err != noErr ) goto AssumeHFS;
	
		hfsPlusSectors	= altVHSector - vHSector + 1 + 2 + 1;	// numSectors + BB + end
		
		//	Fix the embeded extent
		embededExtent.blockCount	= hfsPlusSectors / sectorsPerBlock;
		embededExtent.startBlock	= (vHSector - 2 - mdb->drAlBlSt ) / sectorsPerBlock;
		embedSigWord				= kHFSPlusSigWord;
	}
	else
	{
		embedSigWord				= mdb->drEmbedSigWord;
		embededExtent.blockCount	= mdb->drEmbedExtent.blockCount;
		embededExtent.startBlock	= mdb->drEmbedExtent.startBlock;
	}
		
	if ( embedSigWord == kHFSPlusSigWord )
	{
		startSector		= (embededExtent.startBlock * mdb->drAlBlkSiz / 512) + mdb->drAlBlSt + 2;
			
		err = SeekVolumeHeader( GPtr, startSector, mdb->drAlBlkSiz / 512, &vHSector );
		if ( err != noErr ) goto AssumeHFS;
	
		//	Now replace the bad fields and mark the error
		mdb->drEmbedExtent.blockCount	= hfsPlusSectors / sectorsPerBlock;
		mdb->drEmbedExtent.startBlock	= (vHSector - 2 - mdb->drAlBlSt ) / sectorsPerBlock;
		mdb->drEmbedSigWord				= kHFSPlusSigWord;
		mdb->drAlBlSt					+= vHSector - startSector;								//	Fix the bad field
		GPtr->VIStat = GPtr->VIStat | S_MDB;													//	write out our MDB
		return( E_InvalidMDBdrAlBlSt );
	}
	
	
AssumeHFS:
	*volumeType	= kHFSVolumeType;
	return( noErr );
}


OSErr	SeekVolumeHeader( SGlobPtr GPtr, UInt32 startSector, UInt32 numSectors, UInt32 *vHSector )
{
	OSErr					err;
	HFSPlusVolumeHeader		*volumeHeader;
	SVCB				*calculatedVCB			= GPtr->calculatedVCB;
	
	
	for ( *vHSector = startSector ; *vHSector < startSector + numSectors  ; (*vHSector)++ )
	{
		err	= GetBlock_glue( gbDefault, *vHSector, (Ptr*) &volumeHeader, calculatedVCB->vcbVRefNum, calculatedVCB );	//	Alt VH is always last sector of HFS+ partition
		if ( err != noErr ) return( err );
		
		err = ValidVolumeHeader( volumeHeader );
		(void) ReleaseCacheBlock( (Ptr)volumeHeader, 0 );		//	release it, to unlock buffer
		if ( err == noErr )
			return( noErr );

	}
	
	return( fnfErr );
}


static OSErr CheckWrapperExtents( SGlobPtr GPtr, HFSMasterDirectoryBlock *mdb )
{
	OSErr	err = noErr;
	
	//	See if Norton Disk Doctor 2.0 corrupted the catalog's first extent
	if ( mdb->drCTExtRec[0].startBlock >= mdb->drEmbedExtent.startBlock)
	{
		//	Fix the field in the in-memory copy, and record the error
		mdb->drCTExtRec[0].startBlock = mdb->drXTExtRec[0].startBlock + mdb->drXTExtRec[0].blockCount;
		GPtr->VIStat = GPtr->VIStat | S_MDB;													//	write out our MDB
		err = RcdInvalidWrapperExtents( GPtr, E_InvalidWrapperExtents );
	}
	
	return err;
}


/*------------------------------------------------------------------------------

Function:	CreateExtentsBTreeControlBlock

Function:	Create the calculated ExtentsBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/

OSErr	CreateExtentsBTreeControlBlock( SGlobPtr GPtr )
{
	OSErr					err;
	SInt32					size;
	UInt32					numABlks;
	BTHeaderRec				*header;
	BTreeControlBlock		*btcb		= GPtr->calculatedExtentsBTCB;
	Boolean					isHFSPlus	= GPtr->isHFSPlus;

	//	Set up
	GPtr->TarID		= kHFSExtentsFileID;										//	target = extent file
	GPtr->TarBlock	= kHeaderNodeNum;											//	target block = header node

	//
	//	check out allocation info for the Extents File 
	//
	if ( isHFSPlus == true )
	{
		HFSPlusVolumeHeader			*volumeHeader;
		
		err = GetVBlk( GPtr, GPtr->idSector, (void**)&volumeHeader );			//	get the alternate VH
		ReturnIfError( err );
	
		CopyMemory(volumeHeader->extentsFile.extents, GPtr->calculatedExtentsFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );
		
		err = CheckFileExtents( GPtr, kHFSExtentsFileID, 0, false, (void *)GPtr->calculatedExtentsFCB->fcbExtents32, &numABlks );	//	check out extent info
		ReturnIfError( err );													//	error, invalid file allocation
	
		if ( volumeHeader->extentsFile.totalBlocks != numABlks )				//	check out the PEOF
		{
			RcdError( GPtr, E_ExtPEOF );
			return( E_ExtPEOF );												//	error, invalid extent file PEOF
		}
		else
		{
			GPtr->calculatedExtentsFCB->fcbLogicalSize  = (UInt32) volumeHeader->extentsFile.logicalSize;					//	Set Extents tree's LEOF
			GPtr->calculatedExtentsFCB->fcbPhysicalSize = volumeHeader->extentsFile.totalBlocks * volumeHeader->blockSize;	//	Set Extents tree's PEOF
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//
		
		//	Read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader( GPtr, kCalculatedExtentRefNum, &header );
		ReturnIfError( err );
		
		btcb->maxKeyLength		= kHFSPlusExtentKeyMaximumLength;				//	max key length
		btcb->keyCompareProc	= (void *)CompareExtentKeysPlus;
		btcb->attributes		|=kBTBigKeysMask;								//	HFS+ Extent files have 16-bit key length
		btcb->leafRecords		= header->leafRecords;
		btcb->treeDepth			= header->treeDepth;
		btcb->rootNode			= header->rootNode;
		btcb->firstLeafNode		= header->firstLeafNode;
		btcb->lastLeafNode		= header->lastLeafNode;

		btcb->nodeSize			= header->nodeSize;
		btcb->totalNodes		= ( GPtr->calculatedExtentsFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes			= btcb->totalNodes;								//	start with everything free

		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err	= CheckNodesFirstOffset( GPtr, btcb );
		if ( (err != noErr) && (btcb->nodeSize != 1024) )		//	default HFS+ Extents node size is 1024
		{
			btcb->nodeSize			= 1024;
			btcb->totalNodes		= ( GPtr->calculatedExtentsFCB->fcbPhysicalSize / btcb->nodeSize );
			btcb->freeNodes			= btcb->totalNodes;								//	start with everything free
			
			err = CheckNodesFirstOffset( GPtr, btcb );
			ReturnIfError( err );
			
			GPtr->EBTStat |= S_BTH;								//	update the Btree header
		}
	}
	else	// Classic HFS
	{
		HFSMasterDirectoryBlock	*alternateMDB;
		
		err = GetVBlk( GPtr, GPtr->idSector, (void**)&alternateMDB );			//	get the alternate MDB
		ReturnIfError( err );
	
		CopyMemory(alternateMDB->drXTExtRec, GPtr->calculatedExtentsFCB->fcbExtents16, sizeof(HFSExtentRecord) );
	//	ExtDataRecToExtents(alternateMDB->drXTExtRec, GPtr->calculatedExtentsFCB->fcbExtents);

		
		err = CheckFileExtents( GPtr, kHFSExtentsFileID, 0, false, (void *)GPtr->calculatedExtentsFCB->fcbExtents16, &numABlks );	/* check out extent info */	
		ReturnIfError( err );													//	error, invalid file allocation
	
		if (alternateMDB->drXTFlSize != (numABlks * GPtr->calculatedVCB->vcbBlockSize))//	check out the PEOF
		{
			RcdError(GPtr,E_ExtPEOF);
			return(E_ExtPEOF);													//	error, invalid extent file PEOF
		}
		else
		{
			GPtr->calculatedExtentsFCB->fcbPhysicalSize = alternateMDB->drXTFlSize;		//	set up PEOF and EOF in FCB
			GPtr->calculatedExtentsFCB->fcbLogicalSize = GPtr->calculatedExtentsFCB->fcbPhysicalSize;
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//
			
		//	Read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader( GPtr, kCalculatedExtentRefNum, &header );
		ReturnIfError( err );

		btcb->maxKeyLength	= kHFSExtentKeyMaximumLength;						//	max key length
		btcb->keyCompareProc = (void *)CompareExtentKeys;
		btcb->leafRecords	= header->leafRecords;
		btcb->treeDepth		= header->treeDepth;
		btcb->rootNode		= header->rootNode;
		btcb->firstLeafNode	= header->firstLeafNode;
		btcb->lastLeafNode	= header->lastLeafNode;
		
		btcb->nodeSize		= header->nodeSize;
		btcb->totalNodes	= (GPtr->calculatedExtentsFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes		= btcb->totalNodes;									//	start with everything free

		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err	= CheckNodesFirstOffset( GPtr, btcb );
		ReturnIfError( err );
	}
	

	if ( header->btreeType != kHFSBTreeType )
	{
		GPtr->EBTStat |= S_ReservedBTH;						//	Repair reserved fields in Btree header
	}

	//
	//	set up our DFA extended BTCB area.  Will we have enough memory on all HFS+ volumes.
	//
	btcb->refCon = (UInt32) AllocateClearMemory( sizeof(BTreeExtensionsRec) );			// allocate space for our BTCB extensions
	if ( btcb->refCon == (UInt32) nil )
		return( R_NoMem );														//	no memory for BTree bit map
	size = (btcb->totalNodes + 7) / 8;											//	size of BTree bit map
	((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr = AllocateClearMemory(size);			//	get precleared bitmap
	if ( ((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr == nil )
	{
		return( R_NoMem );														//	no memory for BTree bit map
	}

	((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize = size;				//	remember how long it is
	((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount = header->freeNodes;//	keep track of real free nodes for progress
	
	return( noErr );
}



/*------------------------------------------------------------------------------

Function:	CheckNodesFirstOffset

Function:	Minimal check verifies that the 1st offset is within bounds.  If it's not
			the nodeSize may be wrong.  In the future this routine could be modified
			to try different size values until one fits.
			
------------------------------------------------------------------------------*/
#define GetRecordOffset(btreePtr,node,index)		(*(short *) ((UInt8 *)(node) + (btreePtr)->nodeSize - ((index) << 1) - kOffsetSize))
static	OSErr	CheckNodesFirstOffset( SGlobPtr GPtr, BTreeControlBlock *btcb )
{
	NodeRec		nodeRec;
	UInt16		offset;
	OSErr		err;
			
	err = GetNode( btcb, kHeaderNodeNum, &nodeRec );
	
	if ( err == noErr )
	{
		offset	= GetRecordOffset( btcb, (NodeDescPtr)nodeRec.buffer, 0 );
		if ( (offset < sizeof (BTNodeDescriptor)) ||			// offset < minimum
			 (offset & 1) ||									// offset is odd
			 (offset >= btcb->nodeSize) )						// offset beyond end of node
		{
			err	= fsBTInvalidNodeErr;
		}
	}
	
	if ( err != noErr )
		RcdError( GPtr, E_InvalidNodeSize );

	return( err );
}



/*------------------------------------------------------------------------------

Function:	ExtBTChk - (Extent BTree Check)

Function:	Verifies the extent BTree structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ExtBTChk	-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

OSErr ExtBTChk( SGlobPtr GPtr )
{
	OSErr					err;

	//	Set up
	GPtr->TarID		= kHFSExtentsFileID;										//	target = extent file
	GPtr->TarBlock	= GPtr->idSector;											//	target block = ID sector
 
	//
	//	check out the BTree structure
	//

	err = BTCheck( GPtr, kCalculatedExtentRefNum );
	ReturnIfError( err );														//	invalid extent file BTree

	//
	//	check out the allocation map structure
	//

	err = BTMapChk( GPtr, kCalculatedExtentRefNum );
	ReturnIfError( err );														//	Invalid extent BTree map

	//
	//	compare BTree header record on disk with scavenger's BTree header record 
	//

	err = CmpBTH( GPtr, kCalculatedExtentRefNum );
	ReturnIfError( err );

	//
	//	compare BTree map on disk with scavenger's BTree map
	//

	err = CmpBTM( GPtr, kCalculatedExtentRefNum );

	return( err );
}



/*------------------------------------------------------------------------------

Function:	ExtFlChk - (Extent File Check)

Function:	Verifies the extent file structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ExtFlChk		-	function result:			
								0	= no error
								+n 	= error code
------------------------------------------------------------------------------*/

OSErr ExtFlChk( SGlobPtr GPtr )
{
	UInt32			attributes;
	void			*p;
	OSErr			result;

	//
	//	process the bad block extents (created by the disk init pkg to hide badspots)
	//
 
	result = GetVBlk( GPtr, GPtr->idSector, &p );				//	get the alternate ID block
	ReturnIfError( result );									//	error, could't get it

	attributes = GPtr->isHFSPlus == true ? ((HFSPlusVolumeHeader*)p)->attributes : ((HFSMasterDirectoryBlock*)p)->drAtrb;

	//¥¥ Does HFS+ honnor the same mask?
	if ( attributes & kHFSVolumeSparedBlocksMask )				//	if any badspots
	{
		HFSPlusExtentRecord		zeroXdr;						//	dummy passed to 'CheckFileExtents'
		UInt32					numBadBlocks;
		
		ClearMemory ( zeroXdr, sizeof( HFSPlusExtentRecord ) );
		result = CheckFileExtents( GPtr, kHFSBadBlockFileID, 0, false, (void *)zeroXdr, &numBadBlocks );	//	check and mark bitmap
		ReturnIfError( result );								//	if error, propogate back up
	}
 
	return( noErr );
}


/*------------------------------------------------------------------------------

Function:	CreateCatalogBTreeControlBlock

Function:	Create the calculated ExtentsBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/
OSErr	CreateCatalogBTreeControlBlock( SGlobPtr GPtr )
{
	OSErr					err;
	SInt32					size;
	UInt32					numABlks;
	BTHeaderRec				*header;
	BTreeControlBlock		*btcb			= GPtr->calculatedCatalogBTCB;
	Boolean					isHFSPlus		= GPtr->isHFSPlus;
	SVCB				*calculateVCB	= GPtr->calculatedVCB;

	//	Set up
	GPtr->TarID		= kHFSCatalogFileID;											//	target = catalog file
	GPtr->TarBlock	= kHeaderNodeNum;												//	target block = header node
 

	//
	//	check out allocation info for the Catalog File 
	//

	if ( isHFSPlus )
	{
		HFSPlusVolumeHeader			*volumeHeader;

		err = GetVBlk( GPtr, GPtr->idSector, (void**)&volumeHeader );				//	get the alternate VH
		ReturnIfError( err );

		CopyMemory(volumeHeader->catalogFile.extents, GPtr->calculatedCatalogFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSCatalogFileID, 0, false, (void *)GPtr->calculatedCatalogFCB->fcbExtents32, &numABlks );	/* check out extent info */	
		ReturnIfError( err );														//	error, invalid file allocation

		err = GetVBlk( GPtr, GPtr->idSector, (void**)&volumeHeader );				//	get the alternate VH
		ReturnIfError( err );

		if ( volumeHeader->catalogFile.totalBlocks != numABlks )					//	check out the PEOF
		{
			RcdError( GPtr, E_CatPEOF );
			return( E_CatPEOF );													//	error, invalid extent file PEOF
		}
		else
		{
			GPtr->calculatedCatalogFCB->fcbLogicalSize  = (UInt32) volumeHeader->catalogFile.logicalSize;					//	Set Catalog tree's LEOF
			GPtr->calculatedCatalogFCB->fcbPhysicalSize = volumeHeader->catalogFile.totalBlocks * volumeHeader->blockSize;	//	Set Catalog tree's PEOF
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//

		//	read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader( GPtr, kCalculatedCatalogRefNum, &header );
		ReturnIfError( err );

		btcb->maxKeyLength		= kHFSPlusCatalogKeyMaximumLength;					//	max key length
		btcb->keyCompareProc	= (void *)CompareExtendedCatalogKeys;
		btcb->leafRecords		= header->leafRecords;
		btcb->nodeSize			= header->nodeSize;
		btcb->totalNodes		= ( GPtr->calculatedCatalogFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes			= btcb->totalNodes;									//	start with everything free
		btcb->attributes		|=(kBTBigKeysMask + kBTVariableIndexKeysMask);		//	HFS+ Catalog files have large, variable-sized keys

		btcb->treeDepth		= header->treeDepth;
		btcb->rootNode		= header->rootNode;
		btcb->firstLeafNode	= header->firstLeafNode;
		btcb->lastLeafNode	= header->lastLeafNode;


		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err	= CheckNodesFirstOffset( GPtr, btcb );
		if ( (err != noErr) && (btcb->nodeSize != 4096) )		//	default HFS+ Catalog node size is 4096
		{
			btcb->nodeSize			= 4096;
			btcb->totalNodes		= ( GPtr->calculatedCatalogFCB->fcbPhysicalSize / btcb->nodeSize );
			btcb->freeNodes			= btcb->totalNodes;								//	start with everything free
			
			err	= CheckNodesFirstOffset( GPtr, btcb );
			ReturnIfError( err );
			
			GPtr->CBTStat |= S_BTH;								//	update the Btree header
		}
	}
	else	//	HFS
	{
		HFSMasterDirectoryBlock	*alternateMDB;

		err = GetVBlk( GPtr, GPtr->idSector, (void**)&alternateMDB );				//	get the alternate MDB
		ReturnIfError( err );

		CopyMemory( alternateMDB->drCTExtRec, GPtr->calculatedCatalogFCB->fcbExtents16, sizeof(HFSExtentRecord) );
	//	ExtDataRecToExtents(alternateMDB->drCTExtRec, GPtr->calculatedCatalogFCB->fcbExtents);

		err = CheckFileExtents( GPtr, kHFSCatalogFileID, 0, false, (void *)GPtr->calculatedCatalogFCB->fcbExtents16, &numABlks );	/* check out extent info */	
		ReturnIfError( err );														//	error, invalid file allocation

		err = GetVBlk( GPtr, GPtr->idSector, (void**)&alternateMDB );				//	get the alternate MDB again
		ReturnIfError( err );														//	error, could't get alt MDB

		if (alternateMDB->drCTFlSize != (numABlks * calculateVCB->vcbBlockSize))	//	check out the PEOF
		{
			RcdError( GPtr, E_CatPEOF );
			return( E_CatPEOF );													//	error, invalid extent file PEOF
		}
		else
		{
			GPtr->calculatedCatalogFCB->fcbPhysicalSize	= alternateMDB->drCTFlSize;			//	set up PEOF and EOF in FCB
			GPtr->calculatedCatalogFCB->fcbLogicalSize	= GPtr->calculatedCatalogFCB->fcbPhysicalSize;
		}

		//
		//	Set up the minimal BTreeControlBlock structure
		//

		//	read the BTreeHeader from disk & also validate it's node size.
		err = GetBTreeHeader( GPtr, kCalculatedCatalogRefNum, &header );
		ReturnIfError( err );

		btcb->maxKeyLength		= kHFSCatalogKeyMaximumLength;						//	max key length
		btcb->keyCompareProc	= (void *) CompareCatalogKeys;
		btcb->leafRecords		= header->leafRecords;
		btcb->nodeSize			= header->nodeSize;
		btcb->totalNodes		= (GPtr->calculatedCatalogFCB->fcbPhysicalSize / btcb->nodeSize );
		btcb->freeNodes			= btcb->totalNodes;									//	start with everything free

		btcb->treeDepth		= header->treeDepth;
		btcb->rootNode		= header->rootNode;
		btcb->firstLeafNode	= header->firstLeafNode;
		btcb->lastLeafNode	= header->lastLeafNode;

		//	Make sure the header nodes size field is correct by looking at the 1st record offset
		err	= CheckNodesFirstOffset( GPtr, btcb );
		ReturnIfError( err );
	}
	

	if ( header->btreeType != kHFSBTreeType )
	{
		GPtr->CBTStat |= S_ReservedBTH;						//	Repair reserved fields in Btree header
	}

	//
	//	set up our DFA extended BTCB area.  Will we have enough memory on all HFS+ volumes.
	//

	btcb->refCon = (UInt32) AllocateClearMemory( sizeof(BTreeExtensionsRec) );			// allocate space for our BTCB extensions
	if ( btcb->refCon == (UInt32)nil )
		return( R_NoMem );														//	no memory for BTree bit map
	size = (btcb->totalNodes + 7) / 8;											//	size of BTree bit map
	((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr = AllocateClearMemory(size);			//	get precleared bitmap
	if ( ((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr == nil )
	{
		return( R_NoMem );														//	no memory for BTree bit map
	}

	((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize			= size;						//	remember how long it is
	((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount	= header->freeNodes;		//	keep track of real free nodes for progress

	return( noErr );
}


/*------------------------------------------------------------------------------

Function:	CreateExtendedAllocationsFCB

Function:	Create the calculated ExtentsBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/
OSErr	CreateExtendedAllocationsFCB( SGlobPtr GPtr )
{
	OSErr					err;
	UInt32					numABlks;

	//	Set up
	GPtr->TarID		= kHFSAllocationFileID;											//	target = Allocation file
	GPtr->TarBlock	= GPtr->idSector;												//	target block = ID sector
 
	//
	//	check out allocation info for the allocation File 
	//

	if ( GPtr->isHFSPlus )
	{
		HFSPlusVolumeHeader			*volumeHeader;

		err = GetVBlk( GPtr, GPtr->idSector, (void**)&volumeHeader );				//	get the alternate VH
		ReturnIfError( err );

		CopyMemory( volumeHeader->allocationFile.extents, GPtr->calculatedAllocationsFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSAllocationFileID, 0, false, (void *)GPtr->calculatedAllocationsFCB->fcbExtents32, &numABlks );	/* check out extent info */	
		ReturnIfError( err );														//	error, invalid file allocation

		if ( volumeHeader->allocationFile.totalBlocks != numABlks )					//	check out the PEOF
		{
			RcdError( GPtr, E_CatPEOF );
			return( E_CatPEOF );													//	error, invalid extent file PEOF
		}
		else
		{
			GPtr->calculatedAllocationsFCB->fcbLogicalSize  = (UInt32) volumeHeader->allocationFile.logicalSize;					//	Set allocationFile LEOF
			GPtr->calculatedAllocationsFCB->fcbPhysicalSize = volumeHeader->allocationFile.totalBlocks * volumeHeader->blockSize;	//	Set allocationFile PEOF
		}
	}
	
	return( noErr );
}



/*------------------------------------------------------------------------------

Function:	CatBTChk - (Catalog BTree Check)

Function:	Verifies the catalog BTree structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		CatBTChk	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

OSErr CatBTChk( SGlobPtr GPtr )
{
	OSErr		err;

	//	Set up
	GPtr->TarID		= kHFSCatalogFileID;							/* target = catalog file */
	GPtr->TarBlock	= GPtr->idSector;								//	target block = ID sector
 
	//
	//	check out the BTree structure
	//

	err = BTCheck( GPtr, kCalculatedCatalogRefNum );
	ReturnIfError( err );														//	invalid extent file BTree

	//
	//	check out the allocation map structure
	//

	err = BTMapChk( GPtr, kCalculatedCatalogRefNum );
	ReturnIfError( err );														//	invalid extent BTree map

	//
	//	compare BTree header record on disk with scavenger's BTree header record 
	//

	err = CmpBTH( GPtr, kCalculatedCatalogRefNum );
	ReturnIfError( err );

	//
	//	compare BTree map on disk with scavenger's BTree map
	//

	err = CmpBTM( GPtr, kCalculatedCatalogRefNum );

	return( err );
}



/*------------------------------------------------------------------------------

Function:	CatFlChk - (Catalog File Check)

Function:	Verifies the catalog file structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		CatFlChk	-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

OSErr CatFlChk( SGlobPtr GPtr )
{
	UInt16 				recordSize;
	UInt16				frFlags;
	SInt16				selCode;
	UInt32				hint;
	UInt32				numABlks;
	CatalogKey			key;
	CatalogKey			foundKey;
	CatalogRecord		record;
	UInt32				nextCatalogNodeID;
	UInt32				*reserved;
	UInt32				cNodeID;
	OSErr				result					= noErr;
	UInt32				fileThreadBallance		= 0;			//	files containing thread records - # threads, should be 0
	UInt32				folderThreadBallance	= 0;
	Boolean				isHFSPlus				= GPtr->isHFSPlus;
	SVCB			*calculatedVCB			= GPtr->calculatedVCB;

	//  set up
	GPtr->TarBlock = 0;	//	no target block yet


	//	locate the thread record for the root directory
	BuildCatalogKey( kHFSRootFolderID, (const CatalogName*) nil, isHFSPlus, &key );
	result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recordSize, &hint );

	GPtr->TarBlock = hint;										//	set target block
	if ( result != noErr )
	{
		if ( result == btNotFound )
		{
			RcdError( GPtr,E_NoRtThd );
			return( E_NoRtThd );
		}
		else
		{		
			result = IntError( GPtr,result );					//	error from BTSearch
			return( result );
		}
	}

	calculatedVCB->vcbNextCatalogID	= -2;							//	initialize next free CNID
	nextCatalogNodeID			= kHFSFirstUserCatalogNodeID;
	
	GPtr->ParID					= kHFSRootParentID;
	selCode						= 0x8001;
	folderThreadBallance		= 0;
	calculatedVCB->vcbFolderCount	= -1;							//	don't count root
	
	//
	//	Sequentially traverse the entire catalog
	//
 	while ( result == noErr )
	{
		GPtr->TarID = kHFSCatalogFileID;						//	target = catalog file
	
		result = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recordSize, &hint );
		
		GPtr->TarBlock = hint;									//	set target block
		if ( result == btNotFound )
		{
			break;				 								//	no more catalog records
		}
		else if ( result != noErr )
		{
			result = IntError( GPtr, result ); 					//	error from BTGetRecord
			return( result );
		}
		
		selCode = 1;											//	get next rec from now on

		GPtr->itemsProcessed++;
		
		//	Volume specific specific checks
		if ( isHFSPlus )
		{
			if ( record.recordType & 0xFF00 )
			{
				M_DebugStr("\p New Test FAILED, E_BadRecordType");
				result = E_BadRecordType;
			}
			else if ( recordSize > sizeof(CatalogRecord) )
			{
				result = E_LenCDR;
			}
		}
		else
		{
			if ( record.recordType & 0x00FF )						//	HFS volumes should have 0 here
			{
				M_DebugStr("\p New Test FAILED, E_BadRecordType");
				result = E_BadRecordType;
			}
			else if ( foundKey.hfs.nodeName[0] > 31 )				// too big?  bad news
			{
				result = E_BadFileName;
			}
			else if ( recordSize > sizeof(HFSCatalogFile) )
			{
				result = E_LenCDR;
			}
		}
		
		if ( result )
		{
			RcdError ( GPtr, result );
			break;
		}
		
 		result = CheckForStop( GPtr ); ReturnIfError( result );				//	Permit the user to interrupt
		
		//
		//	Record Checks
		//
		{
			CatalogRecord			catalogRecord;
			HFSCatalogFolder		*smallCatalogFolderP;
			HFSCatalogThread		*smallCatalogThreadP;
			UInt32					threadHint;
			HFSCatalogFile			*smallCatalogFileP;
			HFSPlusCatalogFolder	*largeCatalogFolderP;
			HFSPlusCatalogThread	*largeCatalogThreadP;
			HFSPlusCatalogFile		*largeCatalogFileP;
			
			
			/* copy the key and data records */
			CopyMemory( &foundKey, &key, CalcKeySize( GPtr->calculatedCatalogBTCB, (BTreeKey *)&foundKey ) );
			CopyMemory( &record, &catalogRecord, recordSize );	
		
			//
			//	For an HFS+ directory record ...
			//
			if ( catalogRecord.recordType == kHFSPlusFolderRecord )
			{
				largeCatalogFolderP		= (HFSPlusCatalogFolder *) &catalogRecord;
				GPtr->TarID	= cNodeID	= largeCatalogFolderP->folderID;		//	target ID = directory ID
				GPtr->CNType			= catalogRecord.recordType;				//	target CNode type = directory ID
				CopyCatalogName( (const CatalogName *)&key.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );

				if ( recordSize != sizeof(HFSPlusCatalogFolder) )
				{
					RcdError( GPtr, E_LenDir );
					result = E_LenDir;
					break;
				}
				if ( key.hfsPlus.parentID != GPtr->ParID )
				{
					RcdError( GPtr, E_NoThd );
					result = E_NoThd;
					break;
				}
				
				if ( largeCatalogFolderP->flags != 0 )
				{
					RcdError( GPtr, E_CatalogFlagsNotZero );
					GPtr->CBTStat |= S_ReservedNotZero;
				}
				if ( (cNodeID == 0) || (cNodeID < 16 && cNodeID > 2) )
				{
					RcdError( GPtr, E_InvalidID );
					result = E_InvalidID;
					break;
				}

				if ( largeCatalogFolderP->folderID >= nextCatalogNodeID )
					nextCatalogNodeID = largeCatalogFolderP->folderID +1;
	
				folderThreadBallance--;													//	#folderthreads - #directories should == 0
				calculatedVCB->vcbFolderCount++;
				
				if ( GPtr->ParID == kHFSRootFolderID )
					calculatedVCB->vcbNmRtDirs++;
					
				//	If the hasCustomIcon bit is set, verify the Icon<CR> file exists
				frFlags	= largeCatalogFolderP->userInfo.frFlags;
				if ( frFlags & kHasCustomIcon )
				{
					if (CustomIconCheck( GPtr, largeCatalogFolderP->folderID, frFlags ) == -1) {
						int ii;
						UniChar ch;

						printf("  for folder \"");		
						for (ii = 0; ii < key.hfsPlus.nodeName.length; ++ii) {
							ch = key.hfsPlus.nodeName.unicode[ii];
							if (ch & 0xff80)
								ch = '?';
							printf("%c", (char) ch);
						}
						printf("\"\n");		
					}
				}
				//	Update the TextEncodings bitmap in the VCB for each file
				UpdateVolumeEncodings( calculatedVCB, largeCatalogFolderP->textEncoding );
			}
	
			//
			//	For an HFS+ thread record ...
			//
			else if ( (catalogRecord.recordType == kHFSPlusFolderThreadRecord) || (catalogRecord.recordType == kHFSPlusFileThreadRecord) )
			{
				largeCatalogThreadP		= (HFSPlusCatalogThread *) &catalogRecord;
				GPtr->TarID	 = cNodeID	= key.hfsPlus.parentID;					//	target ID = directory ID
				GPtr->CNType			= catalogRecord.recordType;				//	target CNode type = thread
				GPtr->CName.ustr.length	= 0;									//	no target CName
			
				catalogRecord.recordType == kHFSPlusFolderThreadRecord ? folderThreadBallance++ : fileThreadBallance++;	//	number of threads must match number of files/folders

				if ( (recordSize > sizeof(HFSPlusCatalogThread)) || (recordSize < sizeof(HFSPlusCatalogThread) - sizeof(HFSUniStr255)) )
				{
					RcdError( GPtr, E_LenThd );
					result = E_LenThd;
					break;
				}
				else if ( recordSize == sizeof(HFSPlusCatalogThread) )			//	Find cases of 2210409
				{
					if ( largeCatalogThreadP->nodeName.length != sizeof(largeCatalogThreadP->nodeName.length) )
						GPtr->VeryMinorErrorsStat |= S_BloatedThreadRecordFound;
				}
				
				if ( key.hfsPlus.nodeName.length != 0 )
				{
					RcdError( GPtr, E_ThdKey );
					result = E_ThdKey;
					break;
				}
				result = ChkCName( GPtr, (const CatalogName*)&largeCatalogThreadP->nodeName, isHFSPlus );
				if ( result != noErr )
				{
					RcdError( GPtr, E_ThdCN );
					result = E_ThdCN;
					break;
				}	
				
				if ( (cNodeID == 0) || (cNodeID < 16 && cNodeID > 2) )
				{
					RcdError( GPtr, E_InvalidID );
					result = E_InvalidID;
					break;
				}				
				
				GPtr->ParID = key.hfsPlus.parentID;							//	we have a new parent directory (or fthd)
				
				threadHint = hint;											//	remember the thread hint
	
				//	locate the directory record for this thread or the file record for the fthd
				BuildCatalogKey( largeCatalogThreadP->parentID, (const CatalogName*) &largeCatalogThreadP->nodeName, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recordSize, &hint );
	
				if ( result != noErr )
				{
					if ( result == btNotFound )
					{
						if ( GPtr->CNType == kHFSPlusFileThreadRecord )
							RcdFThdErr( GPtr, GPtr->TarID );				//	record error for possible later repair
						else
							RcdNoDirErr( GPtr, GPtr->TarID );				//	record error for possible later repair
					}
					else
					{		
						result = IntError( GPtr, result );					//	error from BTSearch
						return( result );
					}
				}
				else if ( (GPtr->CNType == kHFSPlusFolderThreadRecord) && (record.recordType != kHFSPlusFolderRecord) )
				{
					RcdError( GPtr, E_NoDir );								//	should have been a directory
					return( E_NoDir );
				}
				
				//	we know result == noErr at this point
				if ( GPtr->CNType == kHFSPlusFileThreadRecord )				//	dealing with files
				{
					if ( result != btNotFound )
					{
						if ( record.recordType != kHFSPlusFileRecord )
						{
							RcdError( GPtr, E_NoFile );
							return( E_NoFile );
						}
						//¥¥ Delete this test, LargeFileThreadRecords don't use the mask, and assume all records have threads
						#if(0)
						else
						{
							largeCatalogFileP = (HFSPlusCatalogFile *) &record;
							if ( (largeCatalogFileP->flags & kHFSThreadExistsMask) == 0 )		//	file thread flag not set
							{	//	treat it as if the file were not found, will delete the thread later -- KST
								RcdFThdErr( GPtr, GPtr->TarID );								//	record error for possible later repair
							}
						}
						#endif
					}
				}
					
				
				//	now re-locate the thread
	
				BuildCatalogKey( GPtr->ParID, (const CatalogName*) nil, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, threadHint, &foundKey, &record, &recordSize, &hint );
				if ( result != noErr )
				{
					result = IntError( GPtr, result );										//	error from BTSearch
					return( result );
				}
			}
			
			//
			//	For an HFS+ file record ...
			//
			else if ( catalogRecord.recordType == kHFSPlusFileRecord )
			{
				largeCatalogFileP = (HFSPlusCatalogFile *) &catalogRecord;
	
				GPtr->TarID = cNodeID = largeCatalogFileP->fileID;							//	target ID = file number
				GPtr->CNType	= catalogRecord.recordType;									//	target CNode type = kHFSPlusFileRecord
				CopyCatalogName( (const CatalogName *) &key.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );	// copy the string name

				if ( recordSize != sizeof(HFSPlusCatalogFile) )
				{
					RcdError( GPtr, E_LenFil );
					result = E_LenFil;
					break;
				}
				if ( key.hfsPlus.parentID != GPtr->ParID )
				{
					RcdError( GPtr, E_NoThd );
					result = E_NoThd;
					break;
				}
				
				//	Check reserved fields
				//
				//	NOTE: the bit 7 (mask 0x80) of the flags byte isn't used by HFS or HFS Plus.
				//	It was used by MFS to indicate that a file record was in use.  However, Inside
				//	Macintosh: Files documents this bit for HFS volumes, and some non-Mac implementations
				//	appear to set the bit.  Therefore, we ignore it.
				if (   ( (largeCatalogFileP->flags & (UInt16) ~(0X83)) != 0 ) )
				{
					RcdError( GPtr, E_CatalogFlagsNotZero );
					GPtr->CBTStat |= S_ReservedNotZero;
				}
				if ( cNodeID < 16 )
				{
					RcdError( GPtr, E_InvalidID );
					result = E_InvalidID;
					break;
				}

				result = CheckFileExtents( GPtr, largeCatalogFileP->fileID, 0, false, (void *)largeCatalogFileP->dataFork.extents, &numABlks );
				if ( result != noErr )
				{
					break;
				}
					
				if ( largeCatalogFileP->dataFork.totalBlocks > numABlks )
				{
					RcdError( GPtr, E_PEOF );
					result = E_PEOF;
					break;
				}
					
				if ( largeCatalogFileP->dataFork.logicalSize > (UInt64) (largeCatalogFileP->dataFork.totalBlocks * calculatedVCB->vcbBlockSize) )
				{
					RcdError( GPtr, E_LEOF );
					result = E_LEOF;
					break;
				}
				
				result = CheckFileExtents( GPtr, largeCatalogFileP->fileID, 0xFF, false, (void *)largeCatalogFileP->resourceFork.extents, &numABlks );
				if ( result != noErr )
				{
					break;
				}
					
				if (largeCatalogFileP->resourceFork.totalBlocks > numABlks )
				{
					RcdError( GPtr, E_PEOF );
					result = E_PEOF;
					break;
				}
				if ( largeCatalogFileP->resourceFork.logicalSize > (UInt64) (largeCatalogFileP->resourceFork.totalBlocks * calculatedVCB->vcbBlockSize) )
				{
					RcdError( GPtr, E_LEOF );
					result = E_LEOF;
					break;
				}
				
				if ( largeCatalogFileP->fileID >= nextCatalogNodeID )
					nextCatalogNodeID = largeCatalogFileP->fileID +1;
	
				//	Update the TextEncodings bitmap in the VCB for each file
				UpdateVolumeEncodings( calculatedVCB, largeCatalogFileP->textEncoding );

				fileThreadBallance--;													//	#filethreads - #files with thread records should == 0
				calculatedVCB->vcbFileCount++;
				if ( GPtr->ParID == kHFSRootFolderID )
					calculatedVCB->vcbNmFls++;
			}

			//
			//	for an HFS directory record ...
			//
			else if ( catalogRecord.recordType == kHFSFolderRecord )
			{
				smallCatalogFolderP = (HFSCatalogFolder *) &catalogRecord;
				GPtr->TarID = cNodeID = smallCatalogFolderP->folderID;					//	target ID = directory ID
				GPtr->CNType	= catalogRecord.recordType;								//	target CNode type = directory ID
				CopyCatalogName( (const CatalogName *) &key.hfs.nodeName, &GPtr->CName, isHFSPlus );
			
				if ( recordSize != sizeof(HFSCatalogFolder) )
				{
					RcdError( GPtr, E_LenDir );
					result = E_LenDir;
					break;
				}
				if ( key.hfs.parentID != GPtr->ParID )
				{
					RcdError( GPtr, E_NoThd );
					result = E_NoThd;
					break;
				}
				if ( smallCatalogFolderP->flags != 0 )
				{
					RcdError( GPtr, E_CatalogFlagsNotZero );
					GPtr->CBTStat |= S_ReservedNotZero;
				}
				if ( (cNodeID == 0) || (cNodeID < 16 && cNodeID > 2) )
				{
					RcdError( GPtr, E_InvalidID );
					result = E_InvalidID;
					break;
				}
				
				if ( smallCatalogFolderP->folderID >= nextCatalogNodeID )
					nextCatalogNodeID = smallCatalogFolderP->folderID +1;
	
				folderThreadBallance--;											//	#folderthreads - #directories should == 0
				calculatedVCB->vcbFolderCount++;										//	track total number of directories
				if ( GPtr->ParID == kHFSRootFolderID )
					calculatedVCB->vcbNmRtDirs++;

				//	If the hasCustomIcon bit is set, verify the Icon\n file exists
				frFlags	= smallCatalogFolderP->userInfo.frFlags;
				if ( frFlags & kHasCustomIcon )
					(void) CustomIconCheck( GPtr, smallCatalogFolderP->folderID, frFlags );
			}
	
			//
			//	for a thread record ...
			//
			else if ( (catalogRecord.recordType == kHFSFolderThreadRecord) || (catalogRecord.recordType == kHFSFileThreadRecord) )
			{
				smallCatalogThreadP 	= (HFSCatalogThread *) &catalogRecord;
				GPtr->TarID	= cNodeID	= key.hfs.parentID;					//	target ID = directory ID
				GPtr->CNType			= catalogRecord.recordType;				//	target CNode type = thread
				GPtr->CName.ustr.length	= 0;									//	no target CName
			
				catalogRecord.recordType == kHFSFolderThreadRecord ? folderThreadBallance++ : fileThreadBallance++;	//	number of threads must match number of files/folders

				if ( recordSize != sizeof(HFSCatalogThread) )
				{
					RcdError( GPtr, E_LenThd );
					result = E_LenThd;
					break;
				}
				if ( key.hfs.nodeName[0] != 0 )
				{
					RcdError( GPtr, E_ThdKey );
					result = E_ThdKey;
					break;
				}
				result = ChkCName( GPtr, (const CatalogName*) &smallCatalogThreadP->nodeName, isHFSPlus );
				if ( result != noErr )
				{
					RcdError( GPtr, E_ThdCN );
					result = E_ThdCN;
					break;
				}	
				
				reserved = (UInt32*) &(smallCatalogThreadP->reserved);
				if ( reserved[0] || reserved[1] )
				{
					RcdError( GPtr, E_CatalogFlagsNotZero );
					GPtr->CBTStat |= S_ReservedNotZero;
				}
				if ( (cNodeID == 0) || (cNodeID < 16 && cNodeID > 2) )
				{
					RcdError( GPtr, E_InvalidID );
					result = E_InvalidID;
					break;
				}
				
				GPtr->ParID = key.hfs.parentID;							//	we have a new parent directory (or fthd)
				
				threadHint = hint;											//	remember the thread hint
	
				//	locate the directory record for this thread or the file record for the fthd
				BuildCatalogKey( smallCatalogThreadP->parentID, (const CatalogName*) smallCatalogThreadP->nodeName, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recordSize, &hint );
	
				if ( result != noErr )
				{
					if ( result == btNotFound )
					{
						if ( GPtr->CNType == kHFSFileThreadRecord )
							RcdFThdErr( GPtr, GPtr->TarID );				//	record error for possible later repair
						else
							RcdNoDirErr( GPtr, GPtr->TarID );				//	record error for possible later repair
					}
					else
					{		
						result = IntError( GPtr, result );					//	error from BTSearch
						return( result );
					}
				}
				else if ( (GPtr->CNType == kHFSFolderThreadRecord) && (record.recordType != kHFSFolderRecord) )
				{
					RcdError( GPtr, E_NoDir );								//	should have been a directory
					return( E_NoDir );
				}
				
				//	we know result == noErr at this point
				if ( GPtr->CNType == kHFSFileThreadRecord )				//	dealing with files
				{
					if ( result != btNotFound )
					{
						if ( record.recordType != kHFSFileRecord )
						{
							RcdError( GPtr, E_NoFile );
							return( E_NoFile );
						}
						else
						{
							smallCatalogFileP = (HFSCatalogFile *) &record;
							if ( (smallCatalogFileP->flags & kHFSThreadExistsMask) == 0 )	//	file thread flag not set
							{	//	treat it as if the file were not found, will delete the thread later -- KST
								RcdFThdErr( GPtr, GPtr->TarID );				//	record error for possible later repair
							}
						}
					}
				}
					
				
				//	now re-locate the thread
	
				BuildCatalogKey( GPtr->ParID, (const CatalogName*) nil, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, threadHint, &foundKey, &record, &recordSize, &hint );
				if ( result != noErr )
				{
					result = IntError( GPtr, result );						//	error from BTSearch
					return( result );
				}
			}
			
			//
			//	for a file record ...
			//
			else if ( catalogRecord.recordType == kHFSFileRecord )
			{
				smallCatalogFileP = (HFSCatalogFile *) &catalogRecord;
	
				GPtr->TarID = cNodeID = smallCatalogFileP->fileID;								//	target ID = file number
				GPtr->CNType	= catalogRecord.recordType;										//	target CNode type = kHFSFileRecord
				CopyCatalogName( (const CatalogName *) &key.hfs.nodeName, &GPtr->CName, isHFSPlus );	// copy the string name
			
				if ( (smallCatalogFileP->flags & kHFSThreadExistsMask) != 0 )					//	file should have a thread record
					fileThreadBallance--;
				
				if ( recordSize != sizeof(HFSCatalogFile) )
				{
					RcdError( GPtr, E_LenFil );
					result = E_LenFil;
					break;
				}
				if ( key.hfs.parentID != GPtr->ParID )
				{
					RcdError( GPtr, E_NoThd );
					result = E_NoThd;
					break;
				}
				
				//	Check reserved fields
				//
				//	NOTE: the upper bit (mask 0x80) of the flags byte isn't used by HFS or HFS Plus.
				//	It was used by MFS to indicate that a file record was in use.  However, Inside
				//	Macintosh: Files documents this bit for HFS volumes, and some non-Mac implementations
				//	appear to set the bit.  Therefore, we ignore it.
				if (   ( (smallCatalogFileP->flags & (UInt8) ~(0X83)) != 0 ) 
					|| ( smallCatalogFileP->dataStartBlock != 0 )
					|| ( smallCatalogFileP->rsrcStartBlock != 0 )
					|| ( smallCatalogFileP->reserved != 0 ) )
				{
					RcdError( GPtr, E_CatalogFlagsNotZero );
					GPtr->CBTStat |= S_ReservedNotZero;
				}
				if ( (cNodeID == 0) || (cNodeID < 16 && cNodeID > 2) )
				{
					RcdError( GPtr, E_InvalidID );
					result = E_InvalidID;
					break;
				}


				result = CheckFileExtents( GPtr, smallCatalogFileP->fileID, 0, false, (void *)smallCatalogFileP->dataExtents, &numABlks );
				if ( result != noErr )
				{
					break;
				}
					
				if ( smallCatalogFileP->dataPhysicalSize > (numABlks * calculatedVCB->vcbBlockSize) )
				{
					RcdError( GPtr, E_PEOF );
					result = E_PEOF;
					break;
				}
					
				if ( smallCatalogFileP->dataLogicalSize > smallCatalogFileP->dataPhysicalSize )
				{
					RcdError( GPtr, E_LEOF );
					result = E_LEOF;
					break;
				}
				
				result = CheckFileExtents( GPtr, smallCatalogFileP->fileID, 0xFF, false, (void *)smallCatalogFileP->rsrcExtents, &numABlks );
				if ( result != noErr )
				{
					break;
				}
					
				if (smallCatalogFileP->rsrcPhysicalSize > (numABlks * calculatedVCB->vcbBlockSize))
				{
					RcdError( GPtr, E_PEOF );
					result = E_PEOF;
					break;
				}
				if ( smallCatalogFileP->rsrcLogicalSize > smallCatalogFileP->rsrcPhysicalSize )
				{
					RcdError( GPtr, E_LEOF );
					result = E_LEOF;
					break;
				}
				
				if ( smallCatalogFileP->fileID >= nextCatalogNodeID )
					nextCatalogNodeID = smallCatalogFileP->fileID +1;
	
				//	Keeping handle in globals of file ID's for HFS volume only
				if ( PtrAndHand( &smallCatalogFileP->fileID, (Handle) GPtr->validFilesList, sizeof(UInt32) ) )
					return( R_NoMem );
					
				calculatedVCB->vcbFileCount++;
				if ( GPtr->ParID == kHFSRootFolderID )
					calculatedVCB->vcbNmFls++;
	
			}
			else											//	invalid record type
			{
				RcdError( GPtr, E_CatRec );
				result = E_CatRec;
				break;
			}
		}	//	End Record Checks
	}	//	End while
	
	if ( (folderThreadBallance != 0) || (fileThreadBallance != 0) )
	{
		GPtr->EBTStat |= S_Orphan;
	}
	
	if ( result == btNotFound )
		result = noErr;				 						//	all done, no more catalog records

	calculatedVCB->vcbNextCatalogID = nextCatalogNodeID;			//	update the field
	
	
	//	Run MountCheck to at least verify that we cover the same checks
	//	MountCheck will also test for orphaned files.
	if ( result == noErr )
	{
		UInt32			consistencyStatus;
		HFSCatalogNodeID	nextCatalogID = calculatedVCB->vcbNextCatalogID;
		
		result	= MountCheck( calculatedVCB, &consistencyStatus );
		
		if ( result != R_UInt )
		{			
			if ( result != noErr )				//	Serious errors at this point indicate there may be orphaned files.
			{
				WriteMsg( GPtr, M_MountCheckMajorError, kErrorMessage );
				GPtr->EBTStat |= S_OrphanedExtent;
			}
				
			if ( consistencyStatus != 0 )
			{
				if ( result == noErr )
					WriteMsg( GPtr, M_MountCheckMinorError, kErrorMessage );
				
				GPtr->VIStat	|=	S_MountCheckMinorErrors;
				
				if ( consistencyStatus & kHFSInvalidPEOF )
					GPtr->CBTStat	|= S_RebuildBTree;
				
				if ( consistencyStatus & kHFSMissingThreadRecords )
					GPtr->EBTStat |= S_Orphan;
				
				if ( (consistencyStatus & kHFSOrphanedExtents) || (nextCatalogID < calculatedVCB->vcbNextCatalogID) )
					GPtr->EBTStat	|= S_OrphanedExtent;

				calculatedVCB->vcbNextCatalogID	= nextCatalogID;	// Since MountCheck will not shrink this value to our calculated value
			}
			result	= noErr;
		}
	}


	return( result );
}


/*------------------------------------------------------------------------------

Function:	CatHChk - (Catalog Hierarchy Check)

Function:	Verifies the catalog hierarchy.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		CatHChk	-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

OSErr CatHChk( SGlobPtr GPtr )
{
	SInt16					i;
	OSErr					result;
	UInt16 					recSize;
	SInt16					selCode;
	UInt32					hint;
	UInt32					dirCnt;
	UInt32					filCnt;
	SInt16					rtdirCnt;
	SInt16					rtfilCnt;
	SVCB				*calculatedVCB;
	SDPR					*dprP;
	SDPR					*dprP1;
	CatalogKey				foundKey;
	Boolean					validKeyFound;
	CatalogKey				key;
	CatalogRecord			record;
	CatalogRecord			record2;
	HFSPlusCatalogFolder	*largeCatalogFolderP;
	HFSPlusCatalogFile		*largeCatalogFileP;
	HFSCatalogFile			*smallCatalogFileP;
	HFSCatalogFolder		*smallCatalogFolderP;
	CatalogName				catalogName;
	UInt32					valence;
	CatalogRecord			threadRecord;
	HFSCatalogNodeID		parID;
	Boolean					isHFSPlus		= GPtr->isHFSPlus;

	//	set up
	calculatedVCB	= GPtr->calculatedVCB;
	GPtr->TarID		= kHFSCatalogFileID;						/* target = catalog file */
	GPtr->TarBlock	= 0;										/* no target block yet */

	//
	//	position to the beginning of catalog
	//
	
	//¥¥ Can we ignore this part by just taking advantage of setting the selCode = 0x8001;
	{ 
		BuildCatalogKey( 1, (const CatalogName *)nil, isHFSPlus, &key );
		result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &threadRecord, &recSize, &hint );
	
		GPtr->TarBlock = hint;									/* set target block */
		if ( result != btNotFound )
		{
			RcdError( GPtr, E_CatRec );
			return( E_CatRec );
		}
	}
	
	GPtr->DirLevel		= 1;
	dprP				= &(*GPtr->DirPTPtr)[0];					
	dprP->directoryID	= 1;
	dirCnt				= filCnt = rtdirCnt = rtfilCnt = 0;

	result	= noErr;
	selCode = 0x8001;											/* start with root directory */			

	//
	//	enumerate the entire catalog 
	//
	while ( (GPtr->DirLevel > 0) && (result == noErr) )
	{
		dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];
		
		validKeyFound = true;
		record.recordType = 0;
		
		//	get the next record
		result = GetBTreeRecord( GPtr->calculatedCatalogFCB, selCode, &foundKey, &record, &recSize, &hint );
		
		GPtr->TarBlock = hint;									/* set target block */
		if ( result != noErr )
		{
			if ( result == btNotFound )
			{
				result = noErr;
				validKeyFound = false;
			}
			else
			{
				result = IntError( GPtr, result );				/* error from BTGetRecord */
				return( result );
			}
		}
		selCode = 1;											/* get next rec from now on */

		GPtr->itemsProcessed++;
		
		//
		//	 if same ParID ...
		//
		parID = isHFSPlus == true ? foundKey.hfsPlus.parentID : foundKey.hfs.parentID;
		if ( (validKeyFound == true) && (parID == dprP->directoryID) )
		{
			dprP->offspringIndex++;								/* increment offspring index */

			//	if new directory ...
	
			if ( record.recordType == kHFSPlusFolderRecord )
			{
 				result = CheckForStop( GPtr ); ReturnIfError( result );				//	Permit the user to interrupt
			
				largeCatalogFolderP = (HFSPlusCatalogFolder *) &record;				
				GPtr->TarID = largeCatalogFolderP->folderID;				//	target ID = directory ID 
				GPtr->CNType = record.recordType;							//	target CNode type = directory ID 
				CopyCatalogName( (const CatalogName *) &foundKey.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );

				if ( dprP->directoryID > 1 )
				{
					GPtr->DirLevel++;										//	we have a new directory level 
					dirCnt++;
				}
				if ( dprP->directoryID == kHFSRootFolderID )				//	bump root dir count 
					rtdirCnt++;

				if ( GPtr->DirLevel > CMMaxDepth )
				{
					RcdError(GPtr,E_CatDepth);								//	error, exceeded max catalog depth
					return noErr;											//	abort this check, but let other checks proceed
				}

				dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];
				dprP->directoryID		= largeCatalogFolderP->folderID;
				dprP->offspringIndex	= 1;
				dprP->directoryHint		= hint;
				dprP->parentDirID		= foundKey.hfsPlus.parentID;
				CopyCatalogName( (const CatalogName *) &foundKey.hfsPlus.nodeName, &dprP->directoryName, isHFSPlus );

				for ( i = 1; i < GPtr->DirLevel; i++ )
				{
					dprP1 = &(*GPtr->DirPTPtr)[i -1];
					if (dprP->directoryID == dprP1->directoryID)
					{
						RcdError( GPtr,E_DirLoop );							//	loop in directory hierarchy 
						return( E_DirLoop );
					}
				}
				
				//	Find thread record
				BuildCatalogKey( dprP->directoryID, (const CatalogName *) nil, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &threadRecord, &recSize, &hint );
				
				if  ( result != noErr )
				{
					result = IntError( GPtr,result );							//	error from BTSearch 
					return( result );
				}	
				dprP->threadHint	= hint;										//	save hint for thread 
				GPtr->TarBlock		= hint;										//	set target block 
			}

			//	LargeCatalogFile
			else if ( record.recordType == kHFSPlusFileRecord )
			{
				largeCatalogFileP = (HFSPlusCatalogFile *) &record;
				GPtr->TarID = largeCatalogFileP->fileID;					//	target ID = file number 
				GPtr->CNType = record.recordType;							//	target CNode type = thread 
				CopyCatalogName( (const CatalogName *) &foundKey.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );
				filCnt++;
				if (dprP->directoryID == kHFSRootFolderID)
					rtfilCnt++;
			}	

			else if ( record.recordType == kHFSFolderRecord )
			{
 				result = CheckForStop( GPtr ); ReturnIfError( result );				//	Permit the user to interrupt
			
				smallCatalogFolderP = (HFSCatalogFolder *) &record;				
				GPtr->TarID = smallCatalogFolderP->folderID;				/* target ID = directory ID */
				GPtr->CNType = record.recordType;							/* target CNode type = directory ID */
				CopyCatalogName( (const CatalogName *) &key.hfs.nodeName, &GPtr->CName, isHFSPlus );	/* target CName = directory name */

				if (dprP->directoryID > 1)
				{
					GPtr->DirLevel++;										/* we have a new directory level */
					dirCnt++;
				}
				if (dprP->directoryID == kHFSRootFolderID)					/* bump root dir count */
					rtdirCnt++;

				if (GPtr->DirLevel > CMMaxDepth)
				{
					RcdError(GPtr,E_CatDepth);								/* error, exceeded max catalog depth */
					return noErr;											/* abort this check, but let other checks proceed */
				}

				dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];			
				dprP->directoryID		= smallCatalogFolderP->folderID;
				dprP->offspringIndex	= 1;
				dprP->directoryHint		= hint;
				dprP->parentDirID		= foundKey.hfs.parentID;

				CopyCatalogName( (const CatalogName *) &foundKey.hfs.nodeName, &dprP->directoryName, isHFSPlus );

				for (i = 1; i < GPtr->DirLevel; i++)
				{
					dprP1 = &(*GPtr->DirPTPtr)[i -1];
					if (dprP->directoryID == dprP1->directoryID)
					{
						RcdError( GPtr,E_DirLoop );				/* loop in directory hierarchy */
						return( E_DirLoop );
					}
				}
				
				BuildCatalogKey( dprP->directoryID, (const CatalogName *)0, isHFSPlus, &key );
				result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &threadRecord, &recSize, &hint );
				if  (result != noErr )
				{
					result = IntError(GPtr,result);				/* error from BTSearch */
					return(result);
				}	
				dprP->threadHint	= hint;						/* save hint for thread */
				GPtr->TarBlock		= hint;						/* set target block */
			}

			//	HFSCatalogFile...
			else if ( record.recordType == kHFSFileRecord )
			{
				smallCatalogFileP = (HFSCatalogFile *) &record;
				GPtr->TarID = smallCatalogFileP->fileID;							/* target ID = file number */
				GPtr->CNType = record.recordType;									/* target CNode type = thread */
				CopyCatalogName( (const CatalogName *) &foundKey.hfs.nodeName, &GPtr->CName, isHFSPlus );	/* target CName = directory name */
				filCnt++;
				if (dprP->directoryID == kHFSRootFolderID)
					rtfilCnt++;
			}
			
			//	Unknown/Bad record type
			else
			{
				M_DebugStr("\p Unknown-Bad record type");
				return( 123 );
			}
		} 
		
		//
		//	 if not same ParID or no record
		//
		else if ( (record.recordType == kHFSFileThreadRecord) || (record.recordType == kHFSPlusFileThreadRecord) )			/* it's a file thread, skip past it */
		{
			GPtr->TarID				= parID;						//	target ID = file number
			GPtr->CNType			= record.recordType;			//	target CNode type = thread
			GPtr->CName.ustr.length	= 0;							//	no target CName
		}
		
		else
		{
			GPtr->TarID = dprP->directoryID;						/* target ID = current directory ID */
			GPtr->CNType = record.recordType;						/* target CNode type = directory */
//			DFA_PrepareOutputName( (const CatalogName *) &dprP->directoryName, isHFSPlus, GPtr->CName );
			CopyCatalogName( (const CatalogName *) &dprP->directoryName, &GPtr->CName, isHFSPlus );	// copy the string name

			//	re-locate current directory
			CopyCatalogName( (const CatalogName *) &dprP->directoryName, &catalogName, isHFSPlus );
			BuildCatalogKey( dprP->parentDirID, (const CatalogName *)&catalogName, isHFSPlus, &key );
			result = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, dprP->directoryHint, &foundKey, &record2, &recSize, &hint );
			
			if ( result != noErr )
			{
				result = IntError(GPtr,result);						/* error from BTSearch */
				return(result);
			}
			GPtr->TarBlock = hint;									/* set target block */
			

			valence = isHFSPlus == true ? record2.hfsPlusFolder.valence : (UInt32)record2.hfsFolder.valence;

			if ( valence != dprP->offspringIndex -1 ) 				/* check its valence */
				if ( result = RcdValErr( GPtr, E_DirVal, dprP->offspringIndex -1, valence, dprP->parentDirID ) )
					return( result );

			GPtr->DirLevel--;										/* move up a level */			
			
			if(GPtr->DirLevel > 0)
			{										
				dprP = &(*GPtr->DirPTPtr)[GPtr->DirLevel -1];
				GPtr->TarID	= dprP->directoryID;					/* target ID = current directory ID */
				GPtr->CNType = record.recordType;					/* target CNode type = directory */
				CopyCatalogName( (const CatalogName *) &dprP->directoryName, &GPtr->CName, isHFSPlus );
			}
		}
	}		//	end while

	//
	//	verify directory and file counts (all nonfatal, repairable errors)
	//
	if ( rtdirCnt != calculatedVCB->vcbNmRtDirs ) 						/* check count of dirs in root */
		if ( result = RcdValErr(GPtr,E_RtDirCnt,rtdirCnt,calculatedVCB->vcbNmRtDirs,0) )
			return( result );

	if ( rtfilCnt != calculatedVCB->vcbNmFls ) 							/* check count of files in root */
		if ( result = RcdValErr(GPtr,E_RtFilCnt,rtfilCnt,calculatedVCB->vcbNmFls,0) )
			return( result );
	
	if (dirCnt != calculatedVCB->vcbFolderCount ) 							/* check count of dirs in volume */
		if ( result = RcdValErr(GPtr,E_DirCnt,dirCnt,calculatedVCB->vcbFolderCount,0) )
			return( result );
		
	if ( filCnt != calculatedVCB->vcbFileCount ) 							/* check count of files in volume */
		if ( result = RcdValErr(GPtr,E_FilCnt,filCnt,calculatedVCB->vcbFileCount,0) )
			return( result );

	return( noErr );

}	/* end of CatHChk */



/*------------------------------------------------------------------------------

Function:	CreateAttributesBTreeControlBlock

Function:	Create the calculated AttributesBTree Control Block
			
Input:		GPtr	-	pointer to scavenger global area

Output:				-	0	= no error
						n 	= error code 
------------------------------------------------------------------------------*/
OSErr	CreateAttributesBTreeControlBlock( SGlobPtr GPtr )
{
	OSErr					err;
	SInt32					size;
	UInt32					numABlks;
	BTHeaderRec				*header;
	BTreeControlBlock		*btcb			= GPtr->calculatedAttributesBTCB;
	Boolean					isHFSPlus		= GPtr->isHFSPlus;

	//	Set up
	GPtr->TarID		= kHFSAttributesFileID;											//	target = attributes file
	GPtr->TarBlock	= kHeaderNodeNum;												//	target block = header node
 

	//
	//	check out allocation info for the Attributes File 
	//

	if ( isHFSPlus )
	{
		HFSPlusVolumeHeader			*volumeHeader;

		err = GetVBlk( GPtr, GPtr->idSector, (void**)&volumeHeader );				//	get the alternate VH
		ReturnIfError( err );

		CopyMemory( volumeHeader->attributesFile.extents, GPtr->calculatedAttributesFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSAttributesFileID, 0, false, (void *)GPtr->calculatedAttributesFCB->fcbExtents32, &numABlks );	/* check out extent info */	
		ReturnIfError( err );														//	error, invalid file allocation

		if ( volumeHeader->attributesFile.totalBlocks != numABlks )					//	check out the PEOF
		{
			RcdError( GPtr, E_CatPEOF );
			return( E_CatPEOF );													//	error, invalid extent file PEOF
		}
		else
		{
			GPtr->calculatedAttributesFCB->fcbLogicalSize  = (UInt64) volumeHeader->attributesFile.logicalSize;						//	Set Attributes tree's LEOF
			GPtr->calculatedAttributesFCB->fcbPhysicalSize = volumeHeader->attributesFile.totalBlocks * volumeHeader->blockSize;	//	Set Attributes tree's PEOF
		}

		//
		//	See if we actually have an attributes BTree
		//
		if (numABlks == 0)
		{
			btcb->maxKeyLength		= 0;
			btcb->keyCompareProc	= 0;
			btcb->leafRecords		= 0;
			btcb->nodeSize			= 0;
			btcb->totalNodes		= 0;
			btcb->freeNodes			= 0;
			btcb->attributes		= 0;

			btcb->treeDepth		= 0;
			btcb->rootNode		= 0;
			btcb->firstLeafNode	= 0;
			btcb->lastLeafNode	= 0;
			
		//	GPtr->calculatedVCB->attributesRefNum = 0;
			GPtr->calculatedVCB->vcbAttributesFile = NULL;
		}
		else
		{
			//	read the BTreeHeader from disk & also validate it's node size.
			err = GetBTreeHeader( GPtr, kCalculatedAttributesRefNum, &header );
			ReturnIfError( err );

			btcb->maxKeyLength		= kAttributeKeyMaximumLength;					//	max key length
			btcb->keyCompareProc	= (void *)CompareAttributeKeys;
			btcb->leafRecords		= header->leafRecords;
			btcb->nodeSize			= header->nodeSize;
			btcb->totalNodes		= ( GPtr->calculatedAttributesFCB->fcbPhysicalSize / btcb->nodeSize );
			btcb->freeNodes			= btcb->totalNodes;									//	start with everything free
			btcb->attributes		|=(kBTBigKeysMask + kBTVariableIndexKeysMask);		//	HFS+ Attributes files have large, variable-sized keys

			btcb->treeDepth		= header->treeDepth;
			btcb->rootNode		= header->rootNode;
			btcb->firstLeafNode	= header->firstLeafNode;
			btcb->lastLeafNode	= header->lastLeafNode;

			//
			//	Make sure the header nodes size field is correct by looking at the 1st record offset
			//
			err	= CheckNodesFirstOffset( GPtr, btcb );
			ReturnIfError( err );
		}
	}
	else
	{
		btcb->maxKeyLength		= 0;
		btcb->keyCompareProc	= 0;
		btcb->leafRecords		= 0;
		btcb->nodeSize			= 0;
		btcb->totalNodes		= 0;
		btcb->freeNodes			= 0;
		btcb->attributes		= 0;

		btcb->treeDepth		= 0;
		btcb->rootNode		= 0;
		btcb->firstLeafNode	= 0;
		btcb->lastLeafNode	= 0;
			
	//	GPtr->calculatedVCB->attributesRefNum = 0;
		GPtr->calculatedVCB->vcbAttributesFile = NULL;
	}
	

	//
	//	set up our DFA extended BTCB area.  Will we have enough memory on all HFS+ volumes.
	//
	btcb->refCon = (UInt32) AllocateClearMemory( sizeof(BTreeExtensionsRec) );			// allocate space for our BTCB extensions
	if ( btcb->refCon == (UInt32)nil )
		return( R_NoMem );														//	no memory for BTree bit map

	if (btcb->totalNodes == 0)
	{
		((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr			= nil;
		((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize			= 0;
		((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount	= 0;
	}
	else
	{
		if ( btcb->refCon == (UInt32)nil )
			return( R_NoMem );														//	no memory for BTree bit map
		size = (btcb->totalNodes + 7) / 8;											//	size of BTree bit map
		((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr = AllocateClearMemory(size);			//	get precleared bitmap
		if ( ((BTreeExtensionsRec*)btcb->refCon)->BTCBMPtr == nil )
		{
			return( R_NoMem );														//	no memory for BTree bit map
		}

		((BTreeExtensionsRec*)btcb->refCon)->BTCBMSize			= size;						//	remember how long it is
		((BTreeExtensionsRec*)btcb->refCon)->realFreeNodeCount	= header->freeNodes;		//	keep track of real free nodes for progress
	}
	
	return( noErr );
}



/*------------------------------------------------------------------------------

Function:	AttrBTChk - (Attributes BTree Check)

Function:	Verifies the attributes BTree structure.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		ExtBTChk	-	function result:			
								0	= no error
								n 	= error code 
------------------------------------------------------------------------------*/

OSErr AttrBTChk( SGlobPtr GPtr )
{
	OSErr					err;

	//
	//	If this volume has no attributes BTree, then skip this check
	//
	if (GPtr->calculatedVCB->vcbAttributesFile == NULL)
		return noErr;
	
	//	Write the status message here to avoid potential confusion to user.
	WriteMsg( GPtr, M_AttrBTChk, kStatusMessage );

	//	Set up
	GPtr->TarID		= kHFSAttributesFileID;										//	target = attributes file
	GPtr->TarBlock	= GPtr->idSector;											//	target block = ID Block
 
	//
	//	check out the BTree structure
	//

	err = BTCheck( GPtr, kCalculatedAttributesRefNum );
	ReturnIfError( err );														//	invalid attributes file BTree

	//
	//	check out the allocation map structure
	//

	err = BTMapChk( GPtr, kCalculatedAttributesRefNum );
	ReturnIfError( err );														//	Invalid attributes BTree map

	//
	//	compare BTree header record on disk with scavenger's BTree header record 
	//

	err = CmpBTH( GPtr, kCalculatedAttributesRefNum );
	ReturnIfError( err );

	//
	//	compare BTree map on disk with scavenger's BTree map
	//

	err = CmpBTM( GPtr, kCalculatedAttributesRefNum );

	return( err );
}



/*------------------------------------------------------------------------------

Name:		RcdFThdErr - (record file thread error)

Function:	Allocates a RepairOrder node describing a dangling file thread record,
			most likely caused by discarding a file with system 6 (or less) that
			had an alias created by system 7 (or greater).  System 6 isn't aware
			of aliases, and so won't remove the accompanying thread record.

Input:		GPtr 		- the scavenger globals
			fid			- the File ID in the thread record key

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate the record
------------------------------------------------------------------------------*/

static int RcdFThdErr( SGlobPtr	GPtr, UInt32 fid )			//	the dangling file ID
{
	RepairOrderPtr	p;										//	the node we compile
	
	RcdError( GPtr, E_NoFile );								//	first, record the error
	
	p = AllocMinorRepairOrder( GPtr, 0 );					//	then get a repair order node (std size)
	if ( p==NULL )											//	quit if out of room
		return( R_NoMem );
	
	p->type = E_NoFile;										//	repair type
	p->parid = fid;											//	this is the only info we need
	GPtr->CatStat |= S_FThd;								//	set flag to trigger repair
	
	return( noErr );										//	successful return
}


/*------------------------------------------------------------------------------

Name:		RcdNoDirErr - (record missing direcotury record error)

Function:	Allocates a RepairOrder node describing a missing directory record,
			most likely caused by disappearing folder bug.  This bug causes some
			folders to jump to Desktop from the root window.  The catalog directory
			record for such a folder has the Desktop folder as the parent but its
			thread record still the root directory as its parent.

Input:		GPtr 		- the scavenger globals
			did			- the directory ID in the thread record key

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate the record
------------------------------------------------------------------------------*/

static int RcdNoDirErr( SGlobPtr GPtr, UInt32 did )			//	the directory ID in the thread record key
{
	RepairOrderPtr	p;										//	the node we compile
	
	RcdError( GPtr, E_NoDir );								//	first, record the error
	
	p = AllocMinorRepairOrder( GPtr, 0 );					//	then get a repair order node (std size)
	if ( p==NULL )											//	quit if out of room
		return ( R_NoMem );
	
	p->type = E_NoDir;										//	repair type
	p->parid = did;											//	this is the only info we need
	GPtr->CatStat |= S_NoDir;								//	set flag to trigger repair
	
	return( noErr );										//	successful return
}


/*------------------------------------------------------------------------------

Name:		RcdValErr - (Record Valence Error)

Function:	Allocates a RepairOrder node and linkg it into the 'GPtr->RepairP'
			list, to describe an incorrect valence count for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			correct		- the correct valence, as computed here
			incorrect	- the incorrect valence as found in volume
			parid		- the parent id, if S_Valence error

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static int RcdValErr( SGlobPtr GPtr, OSErr type, UInt32 correct, UInt32 incorrect, HFSCatalogNodeID parid )										/* the ParID, if needed */
{
	RepairOrderPtr	p;										/* the new node we compile */
	SInt16			n;										/* size of node we allocate */
	
	RcdError( GPtr, type );									/* first, record the error */
	
	if (type == E_DirVal)									/* if normal directory valence error */
		n = CatalogNameSize( &GPtr->CName, GPtr->isHFSPlus);
	else
		n = 0;												/* other errors don't need the name */
	
	p = AllocMinorRepairOrder( GPtr,n );					/* get the node */
	if (p==NULL) 											/* quit if out of room */
		return (R_NoMem);
	
	p->type			= type;									/* save error info */
	p->correct		= correct;
	p->incorrect	= incorrect;
	p->parid		= parid;
	
	if ( n != 0 ) 											/* if name needed */
		CopyCatalogName( (const CatalogName *) &GPtr->CName, (CatalogName*)&p->name, GPtr->isHFSPlus );
	
	GPtr->CatStat |= S_Valence;								/* set flag to trigger repair */
	
	return( noErr );										/* successful return */
}


/*------------------------------------------------------------------------------

Name:		RcdMDBAllocationBlockStartErr - (Record Allocation Block Start Error)

Function:	Allocates a RepairOrder node and linking it into the 'GPtr->RepairP'
			list, to describe the error for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			correct		- the correct valence, as computed here
			incorrect	- the incorrect valence as found in volume

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static	OSErr	RcdMDBEmbededVolDescriptionErr( SGlobPtr GPtr, OSErr type, HFSMasterDirectoryBlock *mdb )
{
	RepairOrderPtr			p;											//	the new node we compile
	EmbededVolDescription	*desc;
		
	RcdError( GPtr, type );												//	first, record the error
	
	p = AllocMinorRepairOrder( GPtr, sizeof(EmbededVolDescription) );	//	get the node
	if ( p == nil )	return( R_NoMem );
	
	p->type							=  type;							//	save error info
	desc							=  (EmbededVolDescription *) &(p->name);
	desc->drAlBlSt					=  mdb->drAlBlSt;
	desc->drEmbedSigWord			=  mdb->drEmbedSigWord;
	desc->drEmbedExtent.startBlock	=  mdb->drEmbedExtent.startBlock;
	desc->drEmbedExtent.blockCount	=  mdb->drEmbedExtent.blockCount;
	
	GPtr->VIStat					|= S_InvalidWrapperExtents;			//	set flag to trigger repair
	
	return( noErr );													//	successful return
}


/*------------------------------------------------------------------------------

Name:		RcdInvalidWrapperExtents - (Record Invalid Wrapper Extents)

Function:	Allocates a RepairOrder node and linking it into the 'GPtr->RepairP'
			list, to describe the error for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			correct		- the correct valence, as computed here
			incorrect	- the incorrect valence as found in volume

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static	OSErr	RcdInvalidWrapperExtents( SGlobPtr GPtr, OSErr type )
{
	RepairOrderPtr			p;											//	the new node we compile
		
	RcdError( GPtr, type );												//	first, record the error
	
	p = AllocMinorRepairOrder( GPtr, 0 );	//	get the node
	if ( p == nil )	return( R_NoMem );
	
	p->type							=  type;							//	save error info
	
	GPtr->VIStat					|= S_BadMDBdrAlBlSt;				//	set flag to trigger repair
	
	return( noErr );													//	successful return
}


#if(0)	//	We just check and fix them in SRepair.c
/*------------------------------------------------------------------------------

Name:		RcdOrphanedExtentErr 

Function:	Allocates a RepairOrder node and linkg it into the 'GPtr->RepairP'
			list, to describe an locked volume name for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			incorrect	- the incorrect file flags as found in file record

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static OSErr RcdOrphanedExtentErr ( SGlobPtr GPtr, SInt16 type, void *theKey )
{
	RepairOrderPtr	p;										/* the new node we compile */
	SInt16			n;										/* size of node we allocate */
	
	RcdError( GPtr,type );									/* first, record the error */
	
	if ( GPtr->isHFSPlus )
		n = sizeof( HFSPlusExtentKey );
	else
		n = sizeof( HFSExtentKey );
	
	p = AllocMinorRepairOrder( GPtr, n );					/* get the node */
	if ( p == NULL ) 										/* quit if out of room */
		return( R_NoMem );
	
	CopyMemory( theKey, p->name, n );					/* copy in the key */
	
	p->type = type;											/* save error info */
	
	GPtr->EBTStat |= S_OrphanedExtent;						/* set flag to trigger repair */
	
	return( noErr );										/* successful return */
}
#endif


/*------------------------------------------------------------------------------

Function:	VInfoChk - (Volume Info Check)

Function:	Verifies volume level information.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		VInfoChk	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

OSErr VInfoChk( SGlobPtr GPtr )
{
	OSErr					result;
	UInt32					hint;
	UInt32					maxClump;
	SVCB				*calculatedVCB;
	CatalogRecord			record;
	CatalogKey				foundKey;
	UInt16					recSize;
	Boolean					isHFSPlus = GPtr->isHFSPlus;
	
	calculatedVCB = GPtr->calculatedVCB;
	

	// locate the catalog record for the root directoryÉ
	result = GetBTreeRecord( GPtr->calculatedCatalogFCB, 0x8001, &foundKey, &record, &recSize, &hint );
	GPtr->TarID = kHFSCatalogFileID;							/* target = catalog */
	GPtr->TarBlock = hint;										/* target block = returned hint */
	if ( result != noErr )
	{
		result = IntError( GPtr, result );
		return( result );
	}

	
	if ( isHFSPlus )
	{
		HFSPlusVolumeHeader			*volumeHeader;
		HFSPlusVolumeHeader			*alternateVolumeHeader;
			
		GPtr->TarID		= AMDB_FNum;								//	target = alternate MDB
		GPtr->TarBlock	= GPtr->idSector;							//	target block =  ID block (Alternate VolumeHeader)

		result = GetVBlk( GPtr, GPtr->idSector, (void**)&alternateVolumeHeader );
		ReturnIfError( result );

		GPtr->TarID		= MDB_FNum;								/* target = MDB */
		GPtr->TarBlock	= MDB_BlkN;								/* target block = MDB */

		result = GetVBlk( GPtr, (calculatedVCB->vcbEmbeddedOffset/512)+2, (void**)&volumeHeader );	//	VH is 3rd sector in
		ReturnIfError( result );
	
		maxClump = (calculatedVCB->vcbTotalBlocks / 4) * calculatedVCB->vcbBlockSize; /* max clump = 1/4 volume size */

		//	check out creation and last mod dates
		calculatedVCB->vcbCreateDate	= alternateVolumeHeader->createDate;	// use creation date in alt MDB
		calculatedVCB->vcbModifyDate	= volumeHeader->modifyDate;		// don't change last mod date
		calculatedVCB->vcbCheckedDate	= volumeHeader->checkedDate;		// don't change checked date

		//	verify volume attribute flags
		if ( ((UInt16)volumeHeader->attributes & VAtrb_Msk) == 0 )
			calculatedVCB->vcbAttributes = (UInt16)volumeHeader->attributes;
		else 
			calculatedVCB->vcbAttributes = VAtrb_DFlt;
	
		//	verify allocation map ptr
		if ( volumeHeader->nextAllocation < calculatedVCB->vcbTotalBlocks )
			calculatedVCB->vcbNextAllocation = volumeHeader->nextAllocation;
		else
			calculatedVCB->vcbNextAllocation = 0;

		
		//	verify default clump sizes
		if ( (volumeHeader->rsrcClumpSize > 0) && (volumeHeader->rsrcClumpSize <= kMaxClumpSize) && ((volumeHeader->rsrcClumpSize % calculatedVCB->vcbBlockSize) == 0) )
			calculatedVCB->vcbRsrcClumpSize = volumeHeader->rsrcClumpSize;
		else if ( (alternateVolumeHeader->rsrcClumpSize > 0) && (alternateVolumeHeader->rsrcClumpSize <= kMaxClumpSize) && ((alternateVolumeHeader->rsrcClumpSize % calculatedVCB->vcbBlockSize) == 0) )
			calculatedVCB->vcbRsrcClumpSize = alternateVolumeHeader->rsrcClumpSize;
		else
			calculatedVCB->vcbRsrcClumpSize = 4 * calculatedVCB->vcbBlockSize;
	
		if ( calculatedVCB->vcbRsrcClumpSize > kMaxClumpSize )
			calculatedVCB->vcbRsrcClumpSize = calculatedVCB->vcbBlockSize;	/* for very large volumes, just use 1 allocation block */


		if ( (volumeHeader->dataClumpSize > 0) && (volumeHeader->dataClumpSize <= kMaxClumpSize) && ((volumeHeader->dataClumpSize % calculatedVCB->vcbBlockSize) == 0) )
			calculatedVCB->vcbDataClumpSize = volumeHeader->dataClumpSize;
		else if ( (alternateVolumeHeader->dataClumpSize > 0) && (alternateVolumeHeader->dataClumpSize <= kMaxClumpSize) && ((alternateVolumeHeader->dataClumpSize % calculatedVCB->vcbBlockSize) == 0) )
			calculatedVCB->vcbDataClumpSize = alternateVolumeHeader->dataClumpSize;
		else
			calculatedVCB->vcbDataClumpSize = 4 * calculatedVCB->vcbBlockSize;
	
		if ( calculatedVCB->vcbDataClumpSize > kMaxClumpSize )
			calculatedVCB->vcbDataClumpSize = calculatedVCB->vcbBlockSize;	/* for very large volumes, just use 1 allocation block */


		//	verify next CNode ID
		if ( (volumeHeader->nextCatalogID > calculatedVCB->vcbNextCatalogID) && (volumeHeader->nextCatalogID <= (calculatedVCB->vcbNextCatalogID + 4096)) )
			calculatedVCB->vcbNextCatalogID = volumeHeader->nextCatalogID;
			
		//¥¥TBD location and unicode? volumename
		//	verify the volume name
		result = ChkCName( GPtr, (const CatalogName*) &foundKey.hfsPlus.nodeName, isHFSPlus );

		//	verify last backup date and backup seqence number
		calculatedVCB->vcbBackupDate = volumeHeader->backupDate;					/* don't change last backup date */
		
		//	verify write count
		calculatedVCB->vcbWriteCount = volumeHeader->writeCount;	/* don't change write count */


		//	check out extent file clump size
		if ( ((volumeHeader->extentsFile.clumpSize % calculatedVCB->vcbBlockSize) == 0) && (volumeHeader->extentsFile.clumpSize <= maxClump) )
			calculatedVCB->vcbExtentsFile->fcbClumpSize = volumeHeader->extentsFile.clumpSize;
		else if ( ((alternateVolumeHeader->extentsFile.clumpSize % calculatedVCB->vcbBlockSize) == 0) && (alternateVolumeHeader->extentsFile.clumpSize <= maxClump) )
			calculatedVCB->vcbExtentsFile->fcbClumpSize = alternateVolumeHeader->extentsFile.clumpSize;
		else		
			calculatedVCB->vcbExtentsFile->fcbClumpSize = (alternateVolumeHeader->extentsFile.extents[0].blockCount * calculatedVCB->vcbBlockSize);
			
		//	check out extent file clump size
		if ( ((volumeHeader->catalogFile.clumpSize % calculatedVCB->vcbBlockSize) == 0) && (volumeHeader->catalogFile.clumpSize <= maxClump) )
			calculatedVCB->vcbCatalogFile->fcbClumpSize = volumeHeader->catalogFile.clumpSize;
		else if ( ((alternateVolumeHeader->catalogFile.clumpSize % calculatedVCB->vcbBlockSize) == 0) && (alternateVolumeHeader->catalogFile.clumpSize <= maxClump) )
			calculatedVCB->vcbCatalogFile->fcbClumpSize = alternateVolumeHeader->catalogFile.clumpSize;
		else
			calculatedVCB->vcbCatalogFile->fcbClumpSize = (alternateVolumeHeader->catalogFile.extents[0].blockCount * calculatedVCB->vcbBlockSize);
	
		//	check out allocations file clump size
		if ( ((volumeHeader->allocationFile.clumpSize % calculatedVCB->vcbBlockSize) == 0) && (volumeHeader->allocationFile.clumpSize <= maxClump) )
			calculatedVCB->vcbAllocationFile->fcbClumpSize = volumeHeader->allocationFile.clumpSize;
		else if ( ((alternateVolumeHeader->allocationFile.clumpSize % calculatedVCB->vcbBlockSize) == 0) && (alternateVolumeHeader->allocationFile.clumpSize <= maxClump) )
			calculatedVCB->vcbAllocationFile->fcbClumpSize = alternateVolumeHeader->allocationFile.clumpSize;
		else
			calculatedVCB->vcbAllocationFile->fcbClumpSize = (alternateVolumeHeader->allocationFile.extents[0].blockCount * calculatedVCB->vcbBlockSize);
	

		CopyMemory( volumeHeader->finderInfo, calculatedVCB->vcbFinderInfo, sizeof(calculatedVCB->vcbFinderInfo) );
	
		//	just copy cache parameters for now
	//	calculatedVCB->vcbEmbedSigWord			= 0;
	//	calculatedVCB->vcbEmbedExtent.startBlock	= 0;
	//	calculatedVCB->vcbEmbedExtent.blockCount	= 0;
	
		//	Now compare verified MDB info with MDB info on disk
		result = CompareVolumeHeader( GPtr );
		ReturnIfError( result );

	}
	else		//	HFS
	{
		HFSMasterDirectoryBlock	*mdbP;
		HFSMasterDirectoryBlock	*alternateMDB;
		
		//	
		//	get volume name from BTree Key
		// 
		
		GPtr->TarID		= AMDB_FNum;								/* target = alternate MDB */
		GPtr->TarBlock	= GPtr->idSector;							/* target block =  alt MDB */

		result = GetVBlk( GPtr, GPtr->idSector, (void**)&alternateMDB );
		ReturnIfError( result );
	 
		GPtr->TarID		= MDB_FNum;								/* target = MDB */
		GPtr->TarBlock	= MDB_BlkN;								/* target block = MDB */
		
		result = GetVBlk( GPtr, MDB_BlkN, (void**)&mdbP );
		ReturnIfError( result );
	
		maxClump = (calculatedVCB->vcbTotalBlocks / 4) * calculatedVCB->vcbBlockSize; /* max clump = 1/4 volume size */

		//	check out creation and last mod dates
		calculatedVCB->vcbCreateDate	= alternateMDB->drCrDate;		/* use creation date in alt MDB */	
		calculatedVCB->vcbModifyDate	= mdbP->drLsMod;			/* don't change last mod date */

		//	verify volume attribute flags
		if ( (mdbP->drAtrb & VAtrb_Msk) == 0 )
			calculatedVCB->vcbAttributes = mdbP->drAtrb;
		else 
			calculatedVCB->vcbAttributes = VAtrb_DFlt;
	
		//	verify allocation map ptr
		if ( mdbP->drAllocPtr < calculatedVCB->vcbTotalBlocks )
			calculatedVCB->vcbNextAllocation = mdbP->drAllocPtr;
		else
			calculatedVCB->vcbNextAllocation = 0;

		//	verify default clump size
		if ( (mdbP->drClpSiz > 0) && (mdbP->drClpSiz <= maxClump) && ((mdbP->drClpSiz % calculatedVCB->vcbBlockSize) == 0) )
			calculatedVCB->vcbDataClumpSize = mdbP->drClpSiz;
		else if ( (alternateMDB->drClpSiz > 0) && (alternateMDB->drClpSiz <= maxClump) && ((alternateMDB->drClpSiz % calculatedVCB->vcbBlockSize) == 0) )
			calculatedVCB->vcbDataClumpSize = alternateMDB->drClpSiz;
		else
			calculatedVCB->vcbDataClumpSize = 4 * calculatedVCB->vcbBlockSize;
	
		if ( calculatedVCB->vcbDataClumpSize > kMaxClumpSize )
			calculatedVCB->vcbDataClumpSize = calculatedVCB->vcbBlockSize;	/* for very large volumes, just use 1 allocation block */
	
		//	verify next CNode ID
		if ( (mdbP->drNxtCNID > calculatedVCB->vcbNextCatalogID) && (mdbP->drNxtCNID <= (calculatedVCB->vcbNextCatalogID + 4096)) )
			calculatedVCB->vcbNextCatalogID = mdbP->drNxtCNID;
			
		//	verify the volume name
		result = ChkCName( GPtr, (const CatalogName*) &calculatedVCB->vcbVN, isHFSPlus );
		if ( result == noErr )		
			if ( CmpBlock( mdbP->drVN, calculatedVCB->vcbVN, calculatedVCB->vcbVN[0] + 1 ) == 0 )
				CopyMemory( mdbP->drVN, calculatedVCB->vcbVN, kMaxHFSVolumeNameLength + 1 ); /* ...we have a good one */		

		//	verify last backup date and backup seqence number
		calculatedVCB->vcbBackupDate = mdbP->drVolBkUp;		/* don't change last backup date */
		calculatedVCB->vcbVSeqNum = mdbP->drVSeqNum;		/* don't change last backup sequence # */
		
		//	verify write count
		calculatedVCB->vcbWriteCount = mdbP->drWrCnt;						/* don't change write count */

		//	check out extent file and catalog clump sizes
		if ( ((mdbP->drXTClpSiz % calculatedVCB->vcbBlockSize) == 0) && (mdbP->drXTClpSiz <= maxClump) )
			calculatedVCB->vcbExtentsFile->fcbClumpSize = mdbP->drXTClpSiz;
		else if ( ((alternateMDB->drXTClpSiz % calculatedVCB->vcbBlockSize) == 0) && (alternateMDB->drXTClpSiz <= maxClump) )
			calculatedVCB->vcbExtentsFile->fcbClumpSize = alternateMDB->drXTClpSiz;
		else		
			calculatedVCB->vcbExtentsFile->fcbClumpSize = (alternateMDB->drXTExtRec[0].blockCount * calculatedVCB->vcbBlockSize);
			
		if ( ((mdbP->drCTClpSiz % calculatedVCB->vcbBlockSize) == 0) && (mdbP->drCTClpSiz <= maxClump) )
			calculatedVCB->vcbCatalogFile->fcbClumpSize = mdbP->drCTClpSiz;
		else if ( ((alternateMDB->drCTClpSiz % calculatedVCB->vcbBlockSize) == 0) && (alternateMDB->drCTClpSiz <= maxClump) )
			calculatedVCB->vcbCatalogFile->fcbClumpSize = alternateMDB->drCTClpSiz;
		else
			calculatedVCB->vcbCatalogFile->fcbClumpSize = (alternateMDB->drCTExtRec[0].blockCount * calculatedVCB->vcbBlockSize);
	
		//	just copy Finder info for now
		CopyMemory(mdbP->drFndrInfo, calculatedVCB->vcbFinderInfo, sizeof(mdbP->drFndrInfo));
	
		//	just copy cache parameters for now
	//	calculatedVCB->vcbEmbedSigWord			= alternateMDB->drEmbedSigWord;
	//	calculatedVCB->vcbEmbedExtent.startBlock	= alternateMDB->drEmbedExtent.startBlock;
	//	calculatedVCB->vcbEmbedExtent.blockCount	= alternateMDB->drEmbedExtent.blockCount;
	
		//	now compare verified MDB info with MDB info on disk
		result = CmpMDB( GPtr );
		ReturnIfError( result );
	}

	return( noErr );											/* all done */
	
}	/* end of VInfoChk */



/*------------------------------------------------------------------------------

Function:	CustomIconCheck - (Custom Icon Check)

Function:	Makes sure there is an icon file when the custom icon bit is set.
			
Input:		GPtr		-	pointer to scavenger global area
			folderID	-	ID of folder with hasCustomIcon bit set.
			frFlags		-	current folder flags

Output:		OSErr	-	function result:			
								0	= no error
								n 	= error code
								
Assume:
			GPtr->CName		folder name
			GPtr->ParID		folders parent ID

------------------------------------------------------------------------------*/

static	OSErr CustomIconCheck ( SGlobPtr GPtr, HFSCatalogNodeID folderID, UInt16 frFlags )
{
	UInt32				hint;
	UInt16				recordSize;
	OSErr				err;
	CatalogKey			key;
	CatalogKey			foundKey;
	CatalogName			iconFileName;
	CatalogRecord		record;
	BTreeIterator		savedIterator;
	Boolean			isHFSPlus = GPtr->isHFSPlus;
	BTreeControlBlock	*btcb = GetBTreeControlBlock( kCalculatedCatalogRefNum );

	if (isHFSPlus)
	{
		iconFileName.ustr.length = 5;
		CopyMemory("\0I\0c\0o\0n\0\x0d", iconFileName.ustr.unicode, 10);
	}
	else
	{
		iconFileName.pstr[0] = 5;
		CopyMemory("Icon\x0d", &iconFileName.pstr[1], 5);
	}

	BuildCatalogKey( folderID, &iconFileName, isHFSPlus, &key );

	//	Save and restore BTree iterator around call to SearchBTreeRecord()
	CopyMemory( &btcb->lastIterator, &savedIterator, sizeof(BTreeIterator) );
	err = SearchBTreeRecord( GPtr->calculatedCatalogFCB, &key, kNoHint, &foundKey, &record, &recordSize, &hint );
	CopyMemory( &savedIterator, &btcb->lastIterator, sizeof(BTreeIterator) );
	
	if ( err == btNotFound )
	{
		RcdCustomIconErr( GPtr, E_MissingCustomIcon, frFlags );
		err = -1;
	}
	
	return( err );
}



/*------------------------------------------------------------------------------

Name:		RcdCustomIconErr 

Function:	Allocates a RepairOrder node and linkg it into the 'GPtr->RepairP'
			list, to describe a missing custom icon for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be > 0
			incorrect	- the incorrect finder flags as found in directory record

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static int RcdCustomIconErr( SGlobPtr GPtr, SInt16 type, UInt32 incorrect )									/* for a consistency check */
{
	RepairOrderPtr	p;										/* the new node we compile */
	int				n;										/* size of node we allocate */
	
	RcdError( GPtr, type );									/* first, record the error */
	
	n = CatalogNameSize( &GPtr->CName, GPtr->isHFSPlus );
	
	p = AllocMinorRepairOrder( GPtr, n );					/* get the node */
	if ( p == NULL ) 										/* quit if out of room */
		return( R_NoMem );
	
	CopyCatalogName( (const CatalogName *) &GPtr->CName, (CatalogName*)&p->name, GPtr->isHFSPlus );	/* copy in the name */
	
	p->type				= type;								/* save error info */
	p->correct	 		= incorrect & ~kHasCustomIcon;		/* mask off the custom icon bit */
	p->incorrect		= incorrect;
	p->maskBit			= (UInt16)kHasCustomIcon;
	p->parid			= GPtr->ParID;
	
	GPtr->CatStat |= S_BadCustomIcon;						/* set flag to trigger repair */
	
	return( noErr );										/* successful return */
}



/*------------------------------------------------------------------------------

Function:	VLockedChk - (Volume Name Locked Check)

Function:	Makes sure the volume name isn't locked.  If it is locked, generate a repair order.

			This function is not called if file sharing is operating.
			
Input:		GPtr		-	pointer to scavenger global area

Output:		VInfoChk	-	function result:			
								0	= no error
								n 	= error code
------------------------------------------------------------------------------*/

OSErr	VLockedChk( SGlobPtr GPtr )
{
	UInt32				hint;
	CatalogKey			foundKey;
	CatalogRecord		record;
	UInt16				recSize;
	OSErr				result;
	UInt16				frFlags;
	Boolean				isHFSPlus		= GPtr->isHFSPlus;
	SVCB			*calculatedVCB	= GPtr->calculatedVCB;
	
	GPtr->TarID		= kHFSCatalogFileID;								/* target = catalog file */
	GPtr->TarBlock	= 0;												/* no target block yet */
	
	//
	//	locate the catalog record for the root directory
	//
	result = GetBTreeRecord( GPtr->calculatedCatalogFCB, 0x8001, &foundKey, &record, &recSize, &hint );
	
	if ( result)
	{
		RcdError( GPtr, E_EntryNotFound );
		return( E_EntryNotFound );
	}

	//	put the vloume name in the VCB
	if ( isHFSPlus == false )
	{
		CopyMemory( foundKey.hfs.nodeName, calculatedVCB->vcbVN, sizeof(calculatedVCB->vcbVN) );
	}
	else if ( GPtr->volumeType != kPureHFSPlusVolumeType )
	{
		HFSMasterDirectoryBlock	*mdbP;
		
		result = GetVBlk( GPtr, MDB_BlkN, (void**)&mdbP );
		ReturnIfError( result );
		CopyMemory( mdbP->drVN, calculatedVCB->vcbVN, sizeof(mdbP->drVN) );
	}
	else		//	Because we don't have the unicode converters, just fill it with a dummy name.
	{
		CopyMemory( "\x0dPure HFS Plus", calculatedVCB->vcbVN, sizeof(Str27) );
	}
	
		
	GPtr->TarBlock = hint;
	if ( isHFSPlus )
		CopyCatalogName( (const CatalogName *)&foundKey.hfsPlus.nodeName, &GPtr->CName, isHFSPlus );
	else
		CopyCatalogName( (const CatalogName *)&foundKey.hfs.nodeName, &GPtr->CName, isHFSPlus );
	
	if ( (record.recordType == kHFSPlusFolderRecord) || (record.recordType == kHFSFolderRecord) )
	{
		frFlags = record.recordType == kHFSPlusFolderRecord ? record.hfsPlusFolder.userInfo.frFlags : record.hfsFolder.userInfo.frFlags;
	
		if ( frFlags & fNameLocked )												// name locked bit set?
			RcdNameLockedErr( GPtr, E_LockedDirName, frFlags );
	}	
	
	return( noErr );
}


/*------------------------------------------------------------------------------

Name:		RcdNameLockedErr 

Function:	Allocates a RepairOrder node and linkg it into the 'GPtr->RepairP'
			list, to describe an locked volume name for possible repair.

Input:		GPtr		- ptr to scavenger global data
			type		- error code (E_xxx), which should be >0
			incorrect	- the incorrect file flags as found in file record

Output:		0 			- no error
			R_NoMem		- not enough mem to allocate record
------------------------------------------------------------------------------*/

static int RcdNameLockedErr( SGlobPtr GPtr, SInt16 type, UInt32 incorrect )									/* for a consistency check */
{
	RepairOrderPtr	p;										/* the new node we compile */
	int				n;										/* size of node we allocate */
	
	RcdError( GPtr, type );									/* first, record the error */
	
	n = CatalogNameSize( &GPtr->CName, GPtr->isHFSPlus );
	
	p = AllocMinorRepairOrder( GPtr, n );					/* get the node */
	if ( p==NULL ) 											/* quit if out of room */
		return ( R_NoMem );
	
	CopyCatalogName( (const CatalogName *) &GPtr->CName, (CatalogName*)&p->name, GPtr->isHFSPlus );
	
	p->type				= type;								/* save error info */
	p->correct			= incorrect & ~fNameLocked;			/* mask off the name locked bit */
	p->incorrect		= incorrect;
	p->maskBit			= (UInt16)fNameLocked;
	p->parid			= 1;
	
	GPtr->CatStat |= S_LockedDirName;						/* set flag to trigger repair */
	
	return( noErr );										/* successful return */
}



OSErr	CheckVolumeBitMap( SGlobPtr GPtr )
{
	OSErr				err = noErr;
	VolumeBitMapHeader	*volumeBitMap;
	SInt32				currentBufferNumber;
	Boolean				isHFSPlus				= GPtr->isHFSPlus;
	
	//	Set up the fcb for the HFS+ Allocations file (Volume BitMap file)
	if ( isHFSPlus )
	{
		HFSPlusVolumeHeader		*volumeHeader;
		UInt32					calculatedBlockCount;

		err = GetVBlk( GPtr, GPtr->idSector, (void**)&volumeHeader );				//	get the alternate VH
		ReturnIfError( err );
	
		CopyMemory( (Ptr)volumeHeader->allocationFile.extents, (Ptr)GPtr->calculatedAllocationsFCB->fcbExtents32, sizeof(HFSPlusExtentRecord) );

		err = CheckFileExtents( GPtr, kHFSAllocationFileID, 0, false, (void *)GPtr->calculatedAllocationsFCB->fcbExtents32, &calculatedBlockCount );	//	check out extent info
		ReturnIfError( err );													//	error, invalid file allocation
	
		if (volumeHeader->allocationFile.totalBlocks != calculatedBlockCount )						//	check out the PEOF
		{
			RcdError( GPtr, E_ExtPEOF );
			return( E_ExtPEOF );													//	error, invalid extent file PEOF
		}
		else
		{
			GPtr->calculatedAllocationsFCB->fcbLogicalSize  = (UInt64) volumeHeader->allocationFile.logicalSize;					//	Set allocationFile tree's LEOF
			GPtr->calculatedAllocationsFCB->fcbPhysicalSize = volumeHeader->allocationFile.totalBlocks * volumeHeader->blockSize;	//	Set allocationFile tree's PEOF
		}
	}
	
	
	//
	//	Loop through each of the buffers to cover the entire bit map
	//
	volumeBitMap = GPtr->volumeBitMapPtr;
	for ( currentBufferNumber = 0 ; currentBufferNumber < volumeBitMap->numberOfBuffers ; currentBufferNumber++ )
	{

		err = CreateVolumeBitMapBuffer( GPtr, currentBufferNumber, false );
		if ( err != noErr )
		{
			M_DebugStr("\p Trouble in CreateVolumeBitMapBuffer");
			ReturnIfError( err );
		}

		//	Volume BitMap buffer is full, compare it to the on disk version.
		err = CompareVolumeBitMap( GPtr, volumeBitMap->currentBuffer );
		
		if ( err != noErr )
		{
			if ( err == E_VBMDamaged )				//	it's been marked for repair
			{
				err = noErr;
			}
			else
				return( err );
		}
	}
	
	return( err );
}



OSErr	CreateVolumeBitMapBuffer( SGlobPtr GPtr, SInt32 bufferNumber, Boolean preAllocateOverlappedExtents )
{
	OSErr				err;
	void				*extents;
	SInt16				selectionCode;
	UInt32				hint;
	UInt16 				recordSize;
	UInt32				blocksUsed;
	SVCB			*calculatedVCB	= GPtr->calculatedVCB;
	VolumeBitMapHeader	*volumeBitMap	= GPtr->volumeBitMapPtr;
	Boolean				isHFSPlus		= GPtr->isHFSPlus;
	Boolean				hasAttributes	= false;


	//	First check if this is even necessary.  Since this is the only routine
	//	that sets volumeBitMap->currentBuffer, if it is already equal to bufferNumber
	//	that buffer has already been created.
	if ( volumeBitMap->currentBuffer == bufferNumber )
		return( noErr );

	volumeBitMap->currentBuffer	= bufferNumber;									//	Set the current buffer
	volumeBitMap->bitMapRecord[volumeBitMap->currentBuffer].count		= 0;
	volumeBitMap->bitMapRecord[volumeBitMap->currentBuffer].isAMatch	= false;


	ClearMemory ( volumeBitMap->buffer, volumeBitMap->bufferSize );				//	start with an empty bitmap buffer
	

	//	If we are searching for all overlapped extents, we mark off the ones in our list, found
	//	in the first pass.  This way we can isolate the extents they overlap with.  This flag is set
	//	during the repair stage.
	if ( preAllocateOverlappedExtents == true )
	{
		UInt32			i;
		ExtentRecord	extentRecord;
		ExtentInfo		*extentInfo;
		ExtentsTable	**extentsTableH	= GPtr->overlappedExtents;
		
		if ( extentsTableH != nil )
		{
			ClearMemory( &extentRecord, sizeof(ExtentRecord) );
			
			for ( i = 0 ; i < (**extentsTableH).count ; i++ )
			{
				extentInfo	= &((**extentsTableH).extentInfo[i]);
				
				if ( isHFSPlus == true )
				{
					extentRecord.hfsPlus[0].startBlock	= extentInfo->startBlock;
					extentRecord.hfsPlus[0].blockCount	= extentInfo->blockCount;
				}
				else
				{
					extentRecord.hfs[0].startBlock	= extentInfo->startBlock;
					extentRecord.hfs[0].blockCount	= extentInfo->blockCount;
				}
				
				err = CheckFileExtents( GPtr, extentInfo->fileNumber, extentInfo->forkType, true, &extentRecord, &blocksUsed );
			}
		}
	}
	

	//	Allocate the extents in the volume bitmap
	extents = isHFSPlus == true ? (void *)GPtr->calculatedExtentsFCB->fcbExtents32 : (void *)GPtr->calculatedExtentsFCB->fcbExtents16;	
	err = CheckFileExtents( GPtr, kHFSExtentsFileID, 0, true, extents, &blocksUsed );
	ReturnIfError( err );
	
	//	Allocate the catalog in the volume bitmap
	extents = isHFSPlus == true ? (void *)GPtr->calculatedCatalogFCB->fcbExtents32 : (void *)GPtr->calculatedCatalogFCB->fcbExtents16;	
	err = CheckFileExtents( GPtr, kHFSCatalogFileID, 0, true, extents, &blocksUsed );
	ReturnIfError( err );
	
	//	Allocate the HFS+ private files
	if ( isHFSPlus )
	{
		HFSPlusVolumeHeader			*volumeHeader;
		
		err = GetVBlk( GPtr, GPtr->idSector, (void**)&volumeHeader );			//	get the alternate VH
		ReturnIfError( err );

		//	Allocate the extended attributes file in the volume bitmap
		if ( volumeHeader->attributesFile.totalBlocks != 0 )
		{
			hasAttributes = true;
			err = CheckFileExtents( GPtr, kHFSAttributesFileID, 0, true, volumeHeader->attributesFile.extents, &blocksUsed );
			ReturnIfError( err );
		}
		//	Allocate the Startup file in the volume bitmap
		if ( volumeHeader->startupFile.totalBlocks != 0 )
		{
			err = CheckFileExtents( GPtr, kHFSStartupFileID, 0, true, volumeHeader->startupFile.extents, &blocksUsed );
			ReturnIfError( err );
		}
	}

	//	Adding clause check for the bad block entry in the extent overflow file, and mark them off in the bitmap.
	{
		HFSPlusExtentKey		extentKey;
		HFSPlusExtentRecord		extentRecord;

		//	Set up the extent key
		BuildExtentKey( isHFSPlus, 0, kHFSBadBlockFileID, 0, (void *)&extentKey );

		err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, &extentKey, kNoHint, (void *) &extentKey, (void *) &extentRecord, &recordSize, &hint );

		if ( err == noErr )
		{
			err = CheckFileExtents( GPtr, kHFSBadBlockFileID, 0, true, (void *) &extentRecord, &blocksUsed );
			ReturnIfError( err );
		}
		else if ( err == btNotFound )
		{
			err = noErr;
		}
	}

	
	if ( isHFSPlus )
	{
		UInt16	burnedBlocks;
		
		//	Allocate the BitMap in the volume bitmap
		extents = (void *)GPtr->calculatedAllocationsFCB->fcbExtents32;	
		err = CheckFileExtents( GPtr, kHFSAllocationFileID, 0, true, extents, &blocksUsed );
		ReturnIfError( err );

		//	Allocate the VolumeHeader in the volume bitmap.
		//	Since the VH is the 3rd sector in we may need to burn the allocation
		//	blocks before it, if there are any.
		if ( calculatedVCB->vcbBlockSize == 512 )
			burnedBlocks	= 2;
		else if ( calculatedVCB->vcbBlockSize == 1024 )
			burnedBlocks	= 1;
		else
			burnedBlocks	= 0;
		
		err = AllocExt( GPtr, 0, 1 + burnedBlocks );
		ReturnIfError( err );

		//	Allocate the AlternateVolumeHeader in the volume bitmap.
		if ( calculatedVCB->vcbBlockSize == 512 )
			burnedBlocks	= 1;
		else
			burnedBlocks	= 0;
		
		err = AllocExt( GPtr, calculatedVCB->vcbTotalBlocks - 1 - burnedBlocks, 1 + burnedBlocks );
		ReturnIfError( err );
	}


	//
	//	Sequentially traverse the entire catalog
	//
	{
		CatalogKey			foundKey;
		CatalogRecord		record;

		selectionCode = 0x8001;										//	first record
		
		while ( err == noErr )
		{
 			err = CheckForStop( GPtr ); ReturnIfError( err );		//	Permit the user to interrupt
	
			//¥¥ Will we need to SearchBTree with the hint, then get the next record, cuz CheckFileExtents changes itterator
			GPtr->TarID = kHFSCatalogFileID;						//	target = catalog file
			err = GetBTreeRecord( GPtr->calculatedCatalogFCB, selectionCode, &foundKey, &record, &recordSize, &hint );
	
			GPtr->TarBlock = hint;									//	set target block
			
			if ( err != noErr )
			{
				if ( err == btNotFound )
				{
					err = noErr;									//	no more catalog records
					break;											//	continue the rest of the checks
				}
				else
				{
					return( err );
				}
			}
			
			selectionCode = 1;										//	next record
			
			GPtr->itemsProcessed++;
	
			if ( record.recordType == kHFSPlusFileRecord )
			{
				HFSPlusCatalogFile	*largeCatalogFileP = (HFSPlusCatalogFile *) &record;
				err = CheckFileExtents( GPtr, largeCatalogFileP->fileID, 0, true, (void *)largeCatalogFileP->dataFork.extents, &blocksUsed );
				if ( err == noErr )
					err = CheckFileExtents( GPtr, largeCatalogFileP->fileID, 0xFF, true, (void *)largeCatalogFileP->resourceFork.extents, &blocksUsed );
			}
			else if ( record.recordType == kHFSFileRecord )
			{
				HFSCatalogFile	*smallCatalogFileP = (HFSCatalogFile *) &record;
				err = CheckFileExtents( GPtr, smallCatalogFileP->fileID, 0, true, (void *)smallCatalogFileP->dataExtents, &blocksUsed );
				if ( err == noErr )
					err = CheckFileExtents( GPtr, smallCatalogFileP->fileID, 0xFF, true, (void *)smallCatalogFileP->rsrcExtents, &blocksUsed );
			}
		}
	}
	//
	//	If there is an attributes B-Tree, traverse all the records
	//
	if (hasAttributes)
	{
		UInt32			recordType;
		AttributeKey	foundKey;
		unsigned char	record[kHFSPlusAttrMinNodeSize];

		selectionCode = 0x8001;										//	first record
		
		while ( err == noErr )
		{
 			err = CheckForStop( GPtr ); ReturnIfError( err );		//	Permit the user to interrupt

			GPtr->TarID = kHFSAttributesFileID;						//	target = attributes file
			err = GetBTreeRecord( GPtr->calculatedAttributesFCB, selectionCode, &foundKey, record, &recordSize, &hint );

			GPtr->TarBlock = hint;									//	set target block
			
			if ( err != noErr )
			{
				if ( err == btNotFound )
				{
					err = noErr;									//	no more records
					break;											//	continue the rest of the checks
				}
				else
				{
					return( err );
				}
			}
			
			selectionCode = 1;										//	next record
			
			GPtr->itemsProcessed++;

			recordType = ((HFSPlusAttrRecord *)record)->recordType;
			
			if ( recordType == kHFSPlusAttrForkData )
			{
				//	Mark the extents as used in the bitmap
				HFSPlusAttrForkData	*attributeRecord = (HFSPlusAttrForkData *) record;
				SInt16				i;
				
				err = ChkExtRec( GPtr, attributeRecord->theFork.extents );	//	checkout the extent record first
				if (err == noErr)
				{
					for (i=0; i<GPtr->numExtents ; i++ )
					{
						err = AllocExt( GPtr, attributeRecord->theFork.extents[i].startBlock, attributeRecord->theFork.extents[i].blockCount );
						if ( err == E_OvlExt )
						{
							err = AddExtentToOverlapList( GPtr, kHFSAttributesFileID, attributeRecord->theFork.extents[i].startBlock, attributeRecord->theFork.extents[i].blockCount, 0x00 );
						}
					}
				}
			}
			else if ( recordType == kHFSPlusAttrExtents )
			{
				//	Mark the extents as used in the bitmap
				HFSPlusAttrExtents	*attributeRecord = (HFSPlusAttrExtents *) record;
				SInt16				i;
				
				err = ChkExtRec( GPtr, attributeRecord->extents );			//	checkout the extent record first
				if (err == noErr)
				{
					for (i=0; i<GPtr->numExtents ; i++ )
					{
						err = AllocExt( GPtr, attributeRecord->extents[i].startBlock, attributeRecord->extents[i].blockCount );
						if ( err == E_OvlExt )
						{
							err = AddExtentToOverlapList( GPtr, kHFSAttributesFileID, attributeRecord->extents[i].startBlock, attributeRecord->extents[i].blockCount, 0x00 );
						}
					}
				}
			}
		}
	}

	volumeBitMap->bitMapRecord[volumeBitMap->currentBuffer].processed = true;
	
	return( err );
}



/*------------------------------------------------------------------------------

Function:	CheckFileExtents - (Check File Extents)

Function:	Verifies the extent info for a file.
			
Input:		GPtr		-	pointer to scavenger global area
			checkBitMap	-	Should we also check the BitMap
			fileNumber	-	file number
			forkType	-	fork type ($00 = data fork, $FF = resource fork)
			extents		-	ptr to 1st extent record for the file

Output:
			CheckFileExtents	-	function result:			
								noErr	= no error
								n 		= error code
			blocksUsed	-	number of allocation blocks allocated to the file
------------------------------------------------------------------------------*/

OSErr	CheckFileExtents( SGlobPtr GPtr, UInt32 fileNumber, UInt8 forkType, Boolean checkBitMap, void *extents, UInt32 *blocksUsed )
{
	UInt32				blockCount;
	UInt32				extentBlockCount;
	UInt32				extentStartBlock;
	UInt32				hint;
	HFSPlusExtentKey	key;
	HFSPlusExtentKey	extentKey;
	HFSPlusExtentRecord	extentRecord;
	UInt16 				recSize;
	OSErr				err;
	SInt16				i;
	Boolean				firstRecord;
	Boolean				isHFSPlus;

	isHFSPlus	= GPtr->isHFSPlus;
	firstRecord	= true;
	err			= noErr;
	blockCount	= 0;
	
	while ( (extents != nil) && (err == noErr) )
	{
		err = ChkExtRec( GPtr, extents );			//	checkout the extent record first
		if ( err != noErr )							//	Bad extent record, don't mark it
			break;
			
		for ( i=0 ; i<GPtr->numExtents ; i++ )		//	now checkout the extents
		{
			//	HFS+/HFS moving extent fields into local variables for evaluation
			if ( isHFSPlus == true )
			{
				extentBlockCount = ((HFSPlusExtentDescriptor *)extents)[i].blockCount;
				extentStartBlock = ((HFSPlusExtentDescriptor *)extents)[i].startBlock;
			}
			else
			{
				extentBlockCount = ((HFSExtentDescriptor *)extents)[i].blockCount;
				extentStartBlock = ((HFSExtentDescriptor *)extents)[i].startBlock;
			}
	
			if ( extentBlockCount == 0 )
				break;
			
			if ( checkBitMap )
			{
				err = AllocExt( GPtr, extentStartBlock, extentBlockCount );
				if ( err == E_OvlExt )
				{
					err = AddExtentToOverlapList( GPtr, fileNumber, extentStartBlock, extentBlockCount, forkType );
				}
			}
			
			
			blockCount += extentBlockCount;
		}
		
		if ( fileNumber == kHFSExtentsFileID )		//	Extents file has no overflow extents
			break;
			
		if ( firstRecord == true )
		{
			firstRecord = false;

			//	Set up the extent key
			BuildExtentKey( isHFSPlus, forkType, fileNumber, blockCount, (void *)&key );

			err = SearchBTreeRecord( GPtr->calculatedExtentsFCB, &key, kNoHint, (void *) &extentKey, (void *) &extentRecord, &recSize, &hint );
			
			if ( err == btNotFound )
			{
				err = noErr;								//	 no more extent records
				extents = nil;
				break;
			}
			else if ( err != noErr )
			{
		 		err = IntError( GPtr, err );		//	error from SearchBTreeRecord
				return( err );
			}
		}
		else
		{
			err = GetBTreeRecord( GPtr->calculatedExtentsFCB, 1, &extentKey, extentRecord, &recSize, &hint );
			
			if ( err == btNotFound )
			{
				err = noErr;								//	 no more extent records
				extents = nil;
				break;
			}
			else if ( err != noErr )
			{
		 		err = IntError( GPtr, err ); 		/* error from BTGetRecord */
				return( err );
			}
			
			//	Check same file and fork
			if ( isHFSPlus )
			{
				if ( (extentKey.fileID != fileNumber) || (extentKey.forkType != forkType) )
					break;
			}
			else
			{
				if ( (((HFSExtentKey *) &extentKey)->fileID != fileNumber) || (((HFSExtentKey *) &extentKey)->forkType != forkType) )
					break;
			}
		}
		
		extents = (void *) &extentRecord;
	}
	
	*blocksUsed = blockCount;
	
	return( err );
}


void	BuildExtentKey( Boolean isHFSPlus, UInt8 forkType, HFSCatalogNodeID fileNumber, UInt32 blockNumber, void * key )
{
	if ( isHFSPlus )
	{
		HFSPlusExtentKey *hfsPlusKey	= (HFSPlusExtentKey*) key;
		
		hfsPlusKey->keyLength	= kHFSPlusExtentKeyMaximumLength;
		hfsPlusKey->forkType	= forkType;
		hfsPlusKey->pad			= 0;
		hfsPlusKey->fileID		= fileNumber;
		hfsPlusKey->startBlock	= blockNumber;
	}
	else
	{
		HFSExtentKey *hfsKey	= (HFSExtentKey*) key;

		hfsKey->keyLength		= kHFSExtentKeyMaximumLength;
		hfsKey->forkType		= forkType;
		hfsKey->fileID			= fileNumber;
		hfsKey->startBlock		= (UInt16) blockNumber;
	}
}



//
//	Adds this extent to our OverlappedExtentList for later repair.
//
OSErr	AddExtentToOverlapList( SGlobPtr GPtr, HFSCatalogNodeID fileNumber, UInt32 extentStartBlock, UInt32 extentBlockCount, UInt8 forkType )
{
	UInt32			newHandleSize;
	ExtentInfo		extentInfo;
	ExtentsTable	**extentsTableH;
	
	extentInfo.fileNumber	= fileNumber;
	extentInfo.startBlock	= extentStartBlock;
	extentInfo.blockCount	= extentBlockCount;
	extentInfo.forkType		= forkType;
	
	//	If it's uninitialized
	if ( GPtr->overlappedExtents == nil )
	{
		GPtr->overlappedExtents	= (ExtentsTable **) NewHandleClear( sizeof(ExtentsTable) );
		extentsTableH	= GPtr->overlappedExtents;
	}
	else
	{
		extentsTableH	= GPtr->overlappedExtents;

		if ( ExtentInfoExists( extentsTableH, &extentInfo ) == true )
			return( noErr );

		//	Grow the Extents table for a new entry.
		newHandleSize = ( sizeof(ExtentInfo) ) + ( GetHandleSize( (Handle)extentsTableH ) );
		SetHandleSize( (Handle)extentsTableH, newHandleSize );
	}

	//	Copy the new extents into the end of the table
	CopyMemory( &extentInfo, &((**extentsTableH).extentInfo[(**extentsTableH).count]), sizeof(ExtentInfo) );
	
	//	Update the extent table count
	(**extentsTableH).count++;
	
	return( noErr );
}


static	Boolean	ExtentInfoExists( ExtentsTable **extentsTableH, ExtentInfo *extentInfo )
{
	UInt32		i;
	ExtentInfo	*aryExtentInfo;
	
	for ( i = 0 ; i < (**extentsTableH).count ; i++ )
	{
		aryExtentInfo	= &((**extentsTableH).extentInfo[i]);
		
		if ( extentInfo->fileNumber == aryExtentInfo->fileNumber )
		{
			if (	(extentInfo->startBlock == aryExtentInfo->startBlock)	&& 
					(extentInfo->blockCount == aryExtentInfo->blockCount)	&&
					(extentInfo->forkType	== aryExtentInfo->forkType)		)
			{
				return( true );
			}
		}
	}
	
	return( false );
}

