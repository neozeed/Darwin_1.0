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
	File:		SCache.c

	Contains:	The cache routines used by the scavenger.

	Version:	xxx put version here xxx

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.
*/


#include "Scavenger.h"



#define	kDFABlockType	'Horn'

enum {
	kBlockSize				= 512,
	kSectorShift			= 9,			// log2(kSectorSize); used for bit shifts
	kMaxNumBlocksInCache	= 16,
	//	Node attributes
	kEmptyMask				= 0x20,
	kInUseMask				= 0x40,
	kDirtyMask				= 0x80,

	//	error codes
	errNotInCache			= -123,
	errNotRecognizeable		= -124,
	errBufferFull			= -125,
	errBadBlockSize			= -126
};


struct CacheBlock {
	UInt32					blockType;
	short					flags;						//	status flags
	short					vRefNum;					//	vRefNum node belongs to
	SVCB				*vcb;						//	VCB for writes
	UInt32					blockNum;
	SInt16					refNum;
	SInt16					reserved;
	UInt32					blockSize;					//	This is repetitive, but the BTree node cache doesn't pass blockSize into ReleaseCacheBlock
	struct CacheBlock		*nextMRU;					//	next node in MRU order
	struct CacheBlock		*nextLRU;					//	next node in LRU order
	Ptr						buffer;						//	This always points right after ths field, but makes code cleaner (abstraction)
};
typedef struct CacheBlock CacheBlock;

struct CacheGlobals {
	UInt32					blockSize;					//	
	struct CacheGlobals		*nextCache;
	CacheBlock				*mru;
	CacheBlock				*lru;
};
typedef struct CacheGlobals CacheGlobals;


CacheGlobals *GetCacheGlobals( UInt32 blockSize );
OSErr		GetCachedBlock( CacheGlobals *cacheGlobals, SInt16 refNum, UInt32 blockNum, short vRefNum, CacheBlock **cBlock );
void		InsertAsMRU	( CacheGlobals *cacheGlobals, CacheBlock *cBlock );
CacheBlock	*GetLowestPriorityBlock( CacheGlobals *cacheGlobals );
void		DFAMarkBlock( Ptr buffer );
OSErr		ReadWriteBlock( CacheBlock *cBlock, Boolean isARead );

// static	OSErr PreCacheReadWriteInPlace( SVCB *vcb, UInt32 fileRefNum, HIOParam *iopb, UInt32 currentPosition, UInt32 maximumBytes, XIOParam *xPB );


CacheGlobals *gCacheGlobals = NULL;
Boolean gBlockCameFromDisk = true;


//	SPI level routines
OSErr	InitDFACache( UInt32 blockSize )
{
	CacheGlobals	*cacheGlobals;
	CacheGlobals	*existingCacheGlobals;
	CacheBlock		*cBlock;
	short			i, lastBuffer;
	Size			cBlockSize;
	short			numBlocksInCache;
	OSErr			err;
	
//printf("# InitDFACache(%ld)\n", blockSize);

	if ( GetCacheGlobals( blockSize ) != nil ) {			//	if cache for this blocksize has already been initialized
		printf(" Cache already initilized for %ld byte blocks \n", blockSize);
		return( noErr );
	}
	cBlockSize = blockSize + sizeof(CacheBlock);

	//	If we don't have enough memory, just try and allocate 3
	numBlocksInCache = kMaxNumBlocksInCache;
	cacheGlobals = (CacheGlobals *) AllocateClearMemory( sizeof( CacheGlobals ) + ( numBlocksInCache * cBlockSize ) );
	err = MemError();
	
	if ( err != noErr )
	{
		numBlocksInCache = 3;
		cacheGlobals = (CacheGlobals *) AllocateClearMemory( sizeof( CacheGlobals ) + ( numBlocksInCache * cBlockSize ) );
		err = MemError();
		ReturnIfError( err );
	}


	cacheGlobals->blockSize = blockSize;

	lastBuffer = numBlocksInCache - 1;									//	last buffer number, since they start at 0
	
	//	Initialize the LRU order for the cache
	cacheGlobals->lru = (CacheBlock *)((Ptr)cacheGlobals + sizeof( CacheGlobals ) + (lastBuffer * cBlockSize));
	cacheGlobals->lru->nextMRU = nil;
	
	//	Initialize the MRU order for the cache
	cacheGlobals->mru = (CacheBlock *)( (Ptr)cacheGlobals + sizeof( CacheGlobals ) );		//	points to 1st cache block
	cacheGlobals->mru->nextLRU = nil;
	
	//	Traverse nodes, setting initial mru, lru, and default values
	for ( i=0, cBlock=cacheGlobals->mru; i<lastBuffer ; i++ )
	{
		cBlock->blockNum	= -1;											//	initialize the block # to illegal while we're at it
		cBlock->blockSize	= blockSize;
		cBlock->blockType	= kDFABlockType;
		cBlock->buffer		= (Ptr)cBlock + sizeof(CacheBlock);
		cBlock->nextMRU		= (CacheBlock *) ( (Ptr)cBlock + cBlockSize );
		cBlock				= cBlock->nextMRU;
	}
	//	And the last Block
	cacheGlobals->lru->blockNum		= -1;
	cBlock->blockSize				= blockSize;
	cacheGlobals->lru->blockType	= kDFABlockType;
	cacheGlobals->lru->buffer		= (Ptr)cacheGlobals->lru + sizeof(CacheBlock);

	for ( i=0, cBlock=cacheGlobals->lru; i<lastBuffer ; i++ )
	{
		cBlock->nextLRU = (CacheBlock *) ( (Ptr)cBlock - cBlockSize );
		cBlock = cBlock->nextLRU;
	}
	
	existingCacheGlobals = gCacheGlobals;
	cacheGlobals->nextCache = existingCacheGlobals; //	We assume this value is initialized to nil
	gCacheGlobals = cacheGlobals;

	return( err );
}



OSErr	DisposeDFACache( void )
{
	OSErr			err		= noErr;
	CacheGlobals	*cache;
	CacheGlobals	*nextCache;
	
	cache = gCacheGlobals;
	
	while ( cache != nil )
	{
		nextCache = cache->nextCache;
		DisposeMemory( (Ptr) cache );
		err += MemError();
		cache = nextCache;
	}
	gCacheGlobals = NULL;
	
	return( err );
}



OSErr	GetBlock_glue( UInt16 flags, UInt32 blockNum, Ptr *buffer, UInt16 refNum, SVCB *vcb )
{
	OSErr	err;
	
	err = DFAGetBlock( flags | gbReleaseMask, blockNum, buffer, (SInt16)refNum, vcb, kBlockSize );

	return( err );
}


#if 0
OSErr	CacheReadInPlace( SVCB *vcb, UInt32 fileRefNum,  HIOParam *iopb, UInt32 currentPosition, UInt32 maximumBytes, UInt32 *actualBytes )
{
	OSErr			err;
	XIOParam		xPB;
	
	err	= PreCacheReadWriteInPlace( vcb, fileRefNum, iopb, currentPosition, maximumBytes, &xPB );
	
	if ( err == noErr )
	{
		err = PBReadSync( (ParamBlockRec *)&xPB );
		*actualBytes	= xPB.ioActCount;
	}
	
	return( err );
}
#endif


OSErr	CacheWriteInPlace( SVCB *vcb, UInt32 fileRefNum,  HIOParam *iopb, UInt32 currentPosition, UInt32 maximumBytes, UInt32 *actualBytes )
{
	OSErr err;
	UInt32 diskBlock;
	UInt32 contiguousBytes;
	void* buffer;
	
	*actualBytes = 0;
	buffer = (char*)iopb->ioBuffer + iopb->ioActCount;

	err = MapFileBlockC(vcb, ResolveFCB(fileRefNum), maximumBytes, (currentPosition >> kSectorShift),
				&diskBlock, &contiguousBytes );
	if (err)
		return (err);

	err = DeviceWrite(vcb->vcbDriverWriteRef, vcb->vcbDriveNumber, buffer, (diskBlock << Log2BlkLo), contiguousBytes, actualBytes);
	
	return( err );
}


#if 0
static	OSErr PreCacheReadWriteInPlace( SVCB *vcb, UInt32 fileRefNum, HIOParam *iopb, UInt32 currentPosition, UInt32 maximumBytes, XIOParam *xPB )
{
	UInt32			diskBlock;
	OSErr			err				= noErr;
	FCBRecPtr		theFCBPtr;
	UInt32			tempPosition;
	
	CopyMemory( iopb, xPB, sizeof(HIOParam) );
	xPB->ioBuffer	= iopb->ioBuffer + iopb->ioActCount;
	xPB->ioReqCount	= maximumBytes;
	xPB->ioVRefNum	= vcb->vcbDrvNum;
	xPB->ioRefNum	= vcb->vcbDRefNum;
	xPB->ioPosMode	= fsFromStart;
		
	if ( iopb->ioRefNum > 0 )								//	It's a file
	{
		tempPosition = currentPosition >> kSectorShift;
		theFCBPtr = ResolveFCB(fileRefNum);
		err = MapFileBlockC( vcb, theFCBPtr, maximumBytes, tempPosition, &diskBlock, (UInt32*)&(xPB->ioReqCount) );
	}
	else													//	else read volume block
	{
		diskBlock		= currentPosition / 512;
	}


	if ( diskBlock > kMaximumBlocksIn4GB )
	{
		xPB->ioWPosOffset.lo	=  diskBlock << Log2BlkLo;		//	calculate lower 32 bits
		xPB->ioWPosOffset.hi	=  diskBlock >> Log2BlkHi;		//	calculate upper 32 bits
		xPB->ioPosMode			|= (1 << kWidePosOffsetBit);	//	set wide positioning mode
	}
	else
	{
		((IOParam*)xPB)->ioPosOffset = diskBlock << Log2BlkLo;
	}

	return( err );
}
#endif

OSErr GetCacheBlock(SInt16 fileRefNum, UInt32 blockNumber, UInt32 blockSize, UInt16 options, LogicalAddress *buffer, Boolean *readFromDisk)
{
	OSErr			err;
	SFCB		*fcb	=	ResolveFCB( fileRefNum );
	
	//	We'll auto release the nodes to avoid cleanup in the DFA code
	err = DFAGetBlock( options | gbReleaseMask, blockNumber, (Ptr *)buffer, fileRefNum, (SVCB *)fcb->fcbVolume, blockSize );
	*readFromDisk	= BlockCameFromDisk();

	return( err );
}

OSErr ReleaseCacheBlock( LogicalAddress buffer, UInt16 options )
{
	OSErr			err;
	CacheBlock		*cBlock	= (CacheBlock *) ((Ptr)buffer - sizeof(CacheBlock));
	
	err = DFAReleaseBlock( (Ptr)buffer, options, cBlock->blockSize );
	return( err );
}

OSErr InitializeBlockCache ( UInt32 blockSize, UInt32 blockCount )
{
	OSErr			err;
	
	err = InitDFACache( blockSize );
	return( err );
}

OSErr TrashCacheBlocks( SInt16 fileRefNum )
{
	M_DebugStr( "\p TrashCacheBlocks is unimplimented in DFA Cache");
	return( noErr );
}


OSErr	FlushBlockCache( void )
{
	return( noErr );
}


//	DFACache internal routines

OSErr	DFAGetBlock( UInt16 flags, UInt32 blockNum, Ptr *buffer, SInt16 refNum, SVCB *vcb, UInt32 blockSize )
{
	OSErr			err;
	CacheGlobals	*cacheGlobals;
	CacheBlock		*cBlock;

//	printf("# DFAGetBlock(b=%ld, s=%ld) ", blockNum, blockSize);

	gBlockCameFromDisk = false;
	
	cacheGlobals = GetCacheGlobals( blockSize );
	if ( cacheGlobals == nil )
	{
		err = InitDFACache( blockSize );
		if ( err != noErr )
		{
			if ( err == iMemFullErr )
				err	= R_NoMem;
			return( err );
		}
		
		cacheGlobals = GetCacheGlobals( blockSize );
		
		if ( cacheGlobals == nil )
		{
			M_DebugStr("\p Cant initialize cache!");
			return( errBadBlockSize );
		}
	}

	err = GetCachedBlock( cacheGlobals, refNum, blockNum, vcb->vcbVRefNum, &cBlock );	//	See if the block has already been cached

	if ( !( flags & gbReadMask ) )		//	if OK to read node from cache
	{	
		if ( err == noErr )
		{
			if ( !(flags & gbReleaseMask) )		//	if node is not for immediate release
				cBlock->flags |= kInUseMask;	//	mark the node in use, so we don't accidentally release it.
			InsertAsMRU( cacheGlobals, cBlock );
//	printf(" ...in cache at 0x%08x\n", cBlock->buffer);
			*buffer = cBlock->buffer;
			return( noErr );
		}
		else if ( flags & gbExistMask )		//	asked for a node in the chache
		{
//	printf(" ...ooops\n");
			*buffer = (Ptr)kBusErrorValue;
			return( err );					//	it wasn't there, so bail with the error
		}
	}
	
//	printf("\n");
	//	Now we have to read the block in from disk
	if ( cBlock == nil )
		cBlock = GetLowestPriorityBlock( cacheGlobals );
	
	if ( cBlock == nil )					//	We can't get a cache block!
	{
		*buffer = (Ptr)kBusErrorValue;
		return( chNoBuf );					//	it wasn't there, so bail with the error
	}

	cBlock->vcb		= vcb;
	cBlock->vRefNum		= vcb->vcbVRefNum;
	cBlock->refNum		= refNum;
	cBlock->blockNum	= blockNum;
	cBlock->flags		= kInUseMask;
	
	err = ReadWriteBlock( cBlock, true );
	
	if ( err == noErr )
	{
		InsertAsMRU( cacheGlobals, cBlock );
		
		(flags & gbReleaseMask) ? (cBlock->flags &= ~kInUseMask) : (cBlock->flags |= kInUseMask);

		*buffer 		= cBlock->buffer;
		gBlockCameFromDisk = true;
		return( err );
	}
	else
	{
		cBlock->blockNum	= -1;
		cBlock->flags		= 0;
		*buffer				= (Ptr)kBusErrorValue;
		return( err );
	}
}


OSErr	DFAReleaseBlock( Ptr buffer, SInt8 flags, UInt32 blockSize )
{
	CacheBlock		*cBlock;
	OSErr			err				= noErr;
	
//printf("# DFAReleaseBlock(0x08lx)\n", buffer);

	cBlock = (CacheBlock *) ( buffer - sizeof( CacheBlock ) );
	
	if ( cBlock->blockType == kDFABlockType )
	{
		if ( flags & rbTrashMask )							//	Trash the block
		{
			cBlock->flags		= 0;						//	Clear the flags
			cBlock->blockNum	= -1;						//	Make it an illegal value
			//	InsertAsLRU( cacheGlobals, cBlock );
		}
		else
		{
			if (	(flags & (rbDirtyMask + rbWriteMask))	//	parameters specify to write the node
				||	(cBlock->flags & kDirtyMask) )			//	or if the node was "marked" earlier
			{
				err = ReadWriteBlock( cBlock, false );
			}
		
			cBlock->flags &= ~( kDirtyMask + kInUseMask );	//	clear these fields
		}
	}
	else
	{
		M_DebugStr("\p Trying to Release a NON-DFA cache block");
		err = errNotRecognizeable;
	}
	
	return ( err );
}



void	DFAMarkBlock( Ptr buffer )
{
	CacheBlock		*cBlock;
	
	cBlock = (CacheBlock *) ( buffer - sizeof( CacheBlock ) );
	
	cBlock->flags |= kDirtyMask;
}

void	DFALockBlock( Ptr buffer )
{
	CacheBlock		*cBlock;
	
	cBlock = (CacheBlock *) ( buffer - sizeof( CacheBlock ) );
	
	cBlock->flags |= kInUseMask;
}


void	DFAFlushCache( void )
{
	OSErr			err;
	CacheGlobals	*cacheGlobals;
	CacheBlock		*cBlock;
	
	if ( GetDFAStage() == kRepairStage )
	{
		cacheGlobals = gCacheGlobals;
		
		while ( cacheGlobals != nil )
		{
			for ( cBlock = cacheGlobals->lru ; cBlock ; cBlock = cBlock->nextLRU )
			{
				err = DFAReleaseBlock( cBlock->buffer, 0, cBlock->blockSize );		//	Write the block
				cBlock->blockNum	= -1;											//	initialize the block # to illegal while we're at it
			}
	
			cacheGlobals = cacheGlobals->nextCache;
		}
	}
}




CacheGlobals *GetCacheGlobals( UInt32 blockSize )
{
	CacheGlobals	*cacheGlobals;
	
	cacheGlobals = gCacheGlobals;
	
	while( (cacheGlobals != nil) && (cacheGlobals->blockSize != blockSize) )
	{
		cacheGlobals = cacheGlobals->nextCache;
	}
			
	return( cacheGlobals );
}

//
//	Given the vRefNum, and physical disk block, try to fing the block in our cache
//
OSErr	GetCachedBlock( CacheGlobals *cacheGlobals, SInt16 refNum, UInt32 blockNum, short vRefNum, CacheBlock **cBlock )
{
	for ( *cBlock = cacheGlobals->mru ; *cBlock ; *cBlock = (**cBlock).nextMRU )
	{
		if ( ((**cBlock).vRefNum == vRefNum) && ((**cBlock).refNum == refNum) && ((**cBlock).blockNum == blockNum) )
			break;												//	node found!, break for loop
	}
	
	if ( *cBlock )
		InsertAsMRU( cacheGlobals, *cBlock );
	else
		return( errNotInCache );

	return( noErr );
}


//
//	Moves cache block to head of mru order in double linked list of cached blocks
//
void	InsertAsMRU	( CacheGlobals *cacheGlobals, CacheBlock *cBlock )
{
	CacheBlock	*swapBlock;

	if ( cacheGlobals->mru != cBlock )							//	if it's not already the mru cBlock
	{
		swapBlock = cacheGlobals->mru;						//	put it in the front of the double queue
		cacheGlobals->mru = cBlock;
		cBlock->nextLRU->nextMRU = cBlock->nextMRU;
		if ( cBlock->nextMRU != nil )
			cBlock->nextMRU->nextLRU = cBlock->nextLRU;
		else
			cacheGlobals->lru = cBlock->nextLRU;
		cBlock->nextMRU = swapBlock;
		cBlock->nextLRU = nil;
		swapBlock->nextLRU = cBlock;
	}
}



CacheBlock	*GetLowestPriorityBlock( CacheGlobals *cacheGlobals )
{
	CacheBlock	*cBlock;
	OSErr		err;
	
	for ( cBlock = cacheGlobals->lru ; cBlock ; cBlock = cBlock->nextLRU )
	{
		if ( !( cBlock->flags & kInUseMask ) )
		{
			//	MountCheck just marks the blocks as dirty and expects the cache to write them out
			if ( (GetDFAStage() == kRepairStage) && (cBlock->flags & kDirtyMask) )
			{
				err = DFAReleaseBlock( cBlock->buffer, 0, cBlock->blockSize );		//	Write the block
				if ( err == noErr )
					break;
			}
			else
			{
				break;												//	node found!, break for loop
			}
		}
	}
	
	if ( cBlock == nil )
		M_DebugStr( "\pAll the nodes are already in use!" );
		
	return( cBlock );
}



Boolean	BlockCameFromDisk( void )
{
	return (gBlockCameFromDisk);
}

OSErr ReadWriteBlock( CacheBlock *cBlock, Boolean isARead )
{
	OSErr	err = 0;
	long	bytesRead = 0;
	UInt32	diskBlock;
	UInt32	contiguousBytes;
	long	reqBytes;	
	long 	actBytes;
	char*	buffer;
	SInt64 offset;
	int drive;

	drive = cBlock->vcb->vcbDriveNumber;
	buffer = (char*) cBlock->buffer;

	do
	{
		if ( cBlock->refNum > 0 )	// It's a file, need to map blocks...
		{
			err = MapFileBlockC( cBlock->vcb, ResolveFCB(cBlock->refNum), cBlock->blockSize,
				(((cBlock->blockNum * cBlock->blockSize) + bytesRead) >> kSectorShift), &diskBlock, &contiguousBytes );
			reqBytes	= cBlock->blockSize - bytesRead;
			if ( contiguousBytes < reqBytes )
				reqBytes = contiguousBytes;
		}
		else	// just a volume block
		{
			diskBlock = cBlock->blockNum;
			reqBytes = cBlock->blockSize;
		}


		buffer += bytesRead;
		offset = (SInt64) ((UInt64) diskBlock) << Log2BlkLo;

		if ( isARead == true )
			err = DeviceRead(cBlock->vcb->vcbDriverReadRef, drive, buffer, offset, reqBytes, &actBytes);
		else
			err = DeviceWrite(cBlock->vcb->vcbDriverWriteRef, drive, buffer, offset, reqBytes, &actBytes);

		bytesRead += actBytes;
	
	} while ( (err == noErr) && (bytesRead < cBlock->blockSize) );
	
	return( err );
}
