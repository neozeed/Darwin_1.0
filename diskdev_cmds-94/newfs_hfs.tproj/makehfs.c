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
	File:		makehfs.c

	Contains:	Initialization code for HFS and HFS Plus volumes.

	Copyright:	© 1984-1999 by Apple Computer, Inc., all rights reserved.

*/

#include <unistd.h>
#include <err.h>
#include <fcntl.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "newfs_hfs.h"
#include <hfs/hfscommon/headers/HFSVolumes.h>
#include "readme.h"


#define HFS_BOOT_DATA	"/usr/share/misc/hfsbootdata"


typedef HFSMasterDirectoryBlock HFS_MDB;

struct filefork {
	UInt16	startBlock;
	UInt16	blockCount;
	SInt32	logicalSize;
	SInt32	physicalSize;
};

struct filefork		gDTDBFork, gSystemFork, gReadMeFork;


static void InitMDB __P((hfsparams_t *defaults, UInt32 driveBlocks, HFS_MDB *mdbp));

static void InitVH __P((hfsparams_t *defaults, UInt32 sectors,
		HFSPlusVolumeHeader *header));
static void InitBitmap __P((UInt32 alBlksUsed, UInt32 sectorSize, UInt8 *buffer,
		UInt32 *bytesUsed));

static void InitExtentsFile __P((const hfsparams_t *dp, HFSExtentDescriptor *bbextp,
		void *buffer, UInt32 *bytesUsed, UInt32 *mapNodes));
static void InitExtentsRoot __P((UInt16 btNodeSize, HFSExtentDescriptor *bbextp,
		void *buffer));
static void InitCatalogFile __P((const hfsparams_t *dp, void *buffer,
		UInt32 *bytesUsed, UInt32 *mapNodes));
static void InitCatalogRoot_HFSPlus __P((const hfsparams_t *dp, void * buffer));

static void InitCatalogRoot_HFS __P((const hfsparams_t *dp, void * buffer));

static void InitFirstCatalogLeaf __P((const hfsparams_t *dp, void * buffer,
		int wrapper));
static void InitSecondCatalogLeaf __P((const hfsparams_t *dp, void *buffer));

static void InitDesktopDB(const hfsparams_t *dp, void *buffer, UInt32 *mapNodes);

static void ClearDisk __P((const DriveInfo *driveInfo, UInt32 startingSector,
		UInt32 numberOfSectors));
static void WriteSystemFile __P((const DriveInfo *dip, UInt32 startingSector,
		UInt32 *filesize));
static void WriteReadMeFile __P((const DriveInfo *dip, UInt32 startingSector,
		UInt32 *filesize));
static void WriteMapNodes __P((const DriveInfo *driveInfo, UInt32 diskStart,
		UInt32 firstMapNode, UInt32 mapNodes, UInt16 btNodeSize, void *buffer));
static void WriteBuffer __P((const DriveInfo *driveInfo, UInt32 startingSector,
		UInt32 byteCount, const void *buffer));
static UInt32 Largest __P((UInt32 a, UInt32 b, UInt32 c, UInt32 d ));

static void MarkBitInAllocationBuffer __P((HFSPlusVolumeHeader *header,
		UInt32 allocationBlock, void* sectorBuffer, UInt32 *sector));
static UInt32 GuessTextEncoding __P((const UniChar *string, UInt32 charCount));

static UInt32 UTCToLocal __P((UInt32 utcTime));

static UInt32 DivideAndRoundUp __P((UInt32 numerator, UInt32 denominator));

static int ConvertUTF8toUnicode __P((const UInt8* source, UInt32 bufsize,
		UniChar* unibuf, UInt16 *charcount));


void SETOFFSET (void *buffer, UInt16 btNodeSize, SInt16 recOffset, SInt16 vecOffset);

#define SETOFFSET(buf,ndsiz,offset,rec)		\
	(*(SInt16 *)((UInt8 *)(buf) + (ndsiz) + (-2 * (rec))) = (offset))

#define BYTESTOBLKS(bytes,blks)		DivideAndRoundUp((bytes),(blks))


#define ROUNDUP(x, u)	(((x) % (u) == 0) ? (x) : ((x)/(u) + 1) * (u))


/*
 * make_hfs
 *	
 * This routine writes an initial HFS volume structure onto a volume.
 * It is assumed that the disk has already been formatted and verified.
 * 
 * For information on the HFS volume format see "Data Organization on Volumes"
 * in "Inside Macintosh: Files" (p. 2-52).
 * 
 */
int
make_hfs(const DriveInfo *driveInfo,
	  hfsparams_t *defaults,
	  UInt32 *plusSectors,
	  UInt32 *plusOffset)
{
	UInt32 sector;
	UInt32 diskBlocksUsed;
	UInt32 mapNodes;
	UInt32 sectorsPerBlock;
	void *nodeBuffer = NULL;
	HFS_MDB	*mdbp = NULL;
	UInt32 bytesUsed;
	
	*plusSectors = 0;
	*plusOffset = 0;

	/* assume sectorSize <= blockSize */
	sectorsPerBlock = defaults->blockSize / driveInfo->sectorSize;
	

	/*--- CREATE A MASTER DIRECTORY BLOCK:  */

	mdbp = (HFS_MDB*)malloc((size_t)kBytesPerSector);
	nodeBuffer = malloc(8192);  /* max bitmap bytes is 8192 bytes */
	if (nodeBuffer == NULL || mdbp == NULL) 
		err(1, NULL);

	InitMDB(defaults, driveInfo->totalSectors, mdbp);

	
	/*--- ZERO OUT BEGINNING OF DISK (bitmap and b-trees):  */
	
	diskBlocksUsed = (mdbp->drAlBlSt + 1) +
		(mdbp->drXTFlSize + mdbp->drCTFlSize) / kBytesPerSector;
	if (defaults->flags & kMakeHFSWrapper) {
		diskBlocksUsed += MAX(kDTDB_Size, mdbp->drAlBlkSiz) / kBytesPerSector;
		diskBlocksUsed += MAX(sizeof(hfswrap_readme), mdbp->drAlBlkSiz) / kBytesPerSector;
		diskBlocksUsed += MAX(24 * 1024, mdbp->drAlBlkSiz) / kBytesPerSector;
	}
	ClearDisk(driveInfo, 0, diskBlocksUsed);

	/* If this is a wrapper, add boot files... */
	if (defaults->flags & kMakeHFSWrapper) {

		InitDesktopDB(defaults, nodeBuffer, &mapNodes);
		sector = mdbp->drAlBlSt + 
			 mdbp->drXTFlSize/kBytesPerSector +
			 mdbp->drCTFlSize/kBytesPerSector;
		WriteBuffer(driveInfo, sector, kHFSNodeSize, nodeBuffer);
		if (mapNodes > 0)
			WriteMapNodes(driveInfo, (sector + 1), 1,
				mapNodes, kHFSNodeSize, nodeBuffer);

		gDTDBFork.logicalSize = MAX(kDTDB_Size, mdbp->drAlBlkSiz);
		gDTDBFork.startBlock = (sector - mdbp->drAlBlSt) / sectorsPerBlock;
		gDTDBFork.blockCount = BYTESTOBLKS(gDTDBFork.logicalSize, mdbp->drAlBlkSiz);
		gDTDBFork.physicalSize = gDTDBFork.blockCount * mdbp->drAlBlkSiz;

		sector += gDTDBFork.physicalSize / kBytesPerSector;

		WriteReadMeFile(driveInfo, sector, &gReadMeFork.logicalSize);
		gReadMeFork.startBlock = gDTDBFork.startBlock + gDTDBFork.blockCount;
		gReadMeFork.blockCount = BYTESTOBLKS(gReadMeFork.logicalSize, mdbp->drAlBlkSiz);
		gReadMeFork.physicalSize = gReadMeFork.blockCount * mdbp->drAlBlkSiz;

		sector += gReadMeFork.physicalSize / kBytesPerSector;

		WriteSystemFile(driveInfo, sector, &gSystemFork.logicalSize);
		gSystemFork.startBlock = gReadMeFork.startBlock + gReadMeFork.blockCount;
		gSystemFork.blockCount = BYTESTOBLKS(gSystemFork.logicalSize, mdbp->drAlBlkSiz);
		gSystemFork.physicalSize = gSystemFork.blockCount * mdbp->drAlBlkSiz;

		(UInt16)mdbp->drFreeBks -= gDTDBFork.blockCount +
				   	gReadMeFork.blockCount +
				   	gSystemFork.blockCount;
		mdbp->drEmbedExtent.startBlock = mdbp->drNmAlBlks - (UInt16)mdbp->drFreeBks;
		mdbp->drEmbedExtent.blockCount = (UInt16)mdbp->drFreeBks;
		(UInt16)mdbp->drFreeBks = 0;
	}


	/*--- WRITE ALLOCATION BITMAP TO DISK:  */

	InitBitmap(mdbp->drNmAlBlks - (UInt16)mdbp->drFreeBks,
		driveInfo->sectorSize, nodeBuffer, &bytesUsed);
	WriteBuffer(driveInfo, mdbp->drVBMSt, bytesUsed, nodeBuffer);


	/*--- WRITE FILE EXTENTS B*-TREE TO DISK:  */

	InitExtentsFile(defaults, &mdbp->drEmbedExtent, nodeBuffer, &bytesUsed, &mapNodes);

	sector = mdbp->drAlBlSt;	/* reset */

	WriteBuffer(driveInfo, sector, bytesUsed, nodeBuffer);
	if (mapNodes > 0)
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/kHFSNodeSize, mapNodes, kHFSNodeSize, nodeBuffer);

	sector += (mdbp->drXTFlSize/kBytesPerSector);


	/*--- WRITE CATALOG B*-TREE TO DISK:  */

	InitCatalogFile(defaults, nodeBuffer, &bytesUsed, &mapNodes);
	WriteBuffer(driveInfo, sector, bytesUsed, nodeBuffer);
	if (mapNodes > 0)
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/kHFSNodeSize, mapNodes, kHFSNodeSize, nodeBuffer);


	/*--- WRITE MASTER DIRECTORY BLOCK TO DISK:  */
	
	/* write mdb last in case we fail along the way */

	WriteBuffer(driveInfo, kMDBStart, kBytesPerSector, mdbp);
	WriteBuffer(driveInfo, driveInfo->totalSectors - 2, kBytesPerSector, mdbp);

	*plusSectors = mdbp->drEmbedExtent.blockCount *
		(mdbp->drAlBlkSiz / driveInfo->sectorSize);
	*plusOffset = mdbp->drAlBlSt + mdbp->drEmbedExtent.startBlock *
		(mdbp->drAlBlkSiz / driveInfo->sectorSize);

	free(nodeBuffer);		
	free(mdbp);	

	return (0);
}


/*
 * make_hfs
 *	
 * This routine writes an initial HFS volume structure onto a volume.
 * It is assumed that the disk has already been formatted and verified.
 *
 */
int
make_hfsplus(const DriveInfo *driveInfo, hfsparams_t *defaults)
{
	UInt16			btNodeSize;
	UInt32			sector;
	UInt32			sectorsPerBlock;
	UInt32			volumeBlocksUsed;
	UInt32			mapNodes;
	UInt32			sectorsPerNode;
	void			*nodeBuffer = NULL;
	HFSPlusVolumeHeader	*header = NULL;
	UInt32			temp;
	UInt32 bytesUsed;


	/* --- Create an HFS Plus header:  */

	header = (HFSPlusVolumeHeader*)malloc((size_t)kBytesPerSector);
	if (header == NULL)
		err(1, NULL);

	InitVH(defaults, driveInfo->totalSectors, header);

	sectorsPerBlock = header->blockSize / kBytesPerSector;


	/*--- ZERO OUT BEGINNING OF DISK:  */

	volumeBlocksUsed = header->totalBlocks - header->freeBlocks - 1;
	if ( header->blockSize == 512 )
		volumeBlocksUsed--;
	bytesUsed = (header->totalBlocks - header->freeBlocks) * sectorsPerBlock;
	ClearDisk(driveInfo, 0, bytesUsed);


	/*--- Allocate a buffer for the rest of our IO:  */

	temp = Largest( defaults->catalogNodeSize * 2,
			defaults->extentsNodeSize,
			header->blockSize,
			(header->totalBlocks - header->freeBlocks) / 8 );
	/* 
	 * If size is not a mutiple of 512, round up to nearest sector
	 */
	if ( (temp & 0x01FF) != 0 )
		temp = (temp + kBytesPerSector) & 0xFFFFFE00;
	
	nodeBuffer = valloc((size_t)temp);
	if (nodeBuffer == NULL)
		err(1, NULL);



		
	/*--- WRITE ALLOCATION BITMAP BITS TO DISK:  */

	InitBitmap((header->totalBlocks - header->freeBlocks) - 1,
		driveInfo->sectorSize, nodeBuffer, &bytesUsed);
	sector = header->allocationFile.extents[0].startBlock * sectorsPerBlock;
	WriteBuffer(driveInfo, sector, bytesUsed, nodeBuffer);

	/*
	 * Write alternate Volume Header bitmap bit to allocations file at
	 * 2nd to last sector on HFS+ volume
	 */
	bzero(nodeBuffer, kBytesPerSector);
	MarkBitInAllocationBuffer( header, header->totalBlocks - 1, nodeBuffer, &sector );

	if ( header->blockSize == 512 ) {
		UInt32	sector2;
		MarkBitInAllocationBuffer( header, header->totalBlocks - 2,
			nodeBuffer, &sector2 );
		
		/* cover the case when altVH and last block are on different bitmap sectors. */
		if ( sector2 != sector ) {
			bzero(nodeBuffer, kBytesPerSector);
			MarkBitInAllocationBuffer(header, header->totalBlocks - 1,
				nodeBuffer, &sector);
			WriteBuffer(driveInfo, sector, kBytesPerSector, nodeBuffer);
			bzero(nodeBuffer, kBytesPerSector);
			MarkBitInAllocationBuffer(header, header->totalBlocks - 2,
				nodeBuffer, &sector);
		}
	}
	WriteBuffer(driveInfo, sector, kBytesPerSector, nodeBuffer);

	/*--- WRITE FILE EXTENTS B-TREE TO DISK:  */

	btNodeSize = defaults->extentsNodeSize;
	sectorsPerNode = btNodeSize/kBytesPerSector;

	InitExtentsFile(defaults, NULL, nodeBuffer, &bytesUsed, &mapNodes);

	sector = header->extentsFile.extents[0].startBlock * sectorsPerBlock;
	WriteBuffer(driveInfo, sector, bytesUsed, nodeBuffer);
	if (mapNodes > 0)
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/kHFSNodeSize, mapNodes, btNodeSize, nodeBuffer);

	
	/*--- WRITE CATALOG B-TREE TO DISK:  */
	
	btNodeSize = defaults->catalogNodeSize;
	sectorsPerNode = btNodeSize/kBytesPerSector;

	InitCatalogFile(defaults, nodeBuffer, &bytesUsed, &mapNodes);

	sector = header->catalogFile.extents[0].startBlock * sectorsPerBlock;
	WriteBuffer(driveInfo, sector, bytesUsed, nodeBuffer);
	if (mapNodes > 0)
		WriteMapNodes(driveInfo, (sector + bytesUsed/kBytesPerSector),
			bytesUsed/kHFSNodeSize, mapNodes, btNodeSize, nodeBuffer);


	/*--- WRITE VOLUME HEADER TO DISK:  */

	/* write header last in case we fail along the way */

	WriteBuffer(driveInfo, 2, kBytesPerSector, header);
	WriteBuffer(driveInfo, driveInfo->totalSectors - 2, kBytesPerSector, header);

	free(nodeBuffer);
	free(header);

	return (0);
}


/*
 * InitMDB
 *
 * Initialize a Master Directory Block (MDB) record.
 * 
 * If the alignment parameter is non-zero, it indicates the aligment
 * (in 512 byte sectors) that should be used for allocation blocks.
 * For example, if alignment is 8, then allocation blocks will begin
 * on a 4K boundary relative to the start of the partition.
 *
 */
static void
InitMDB(hfsparams_t *defaults, UInt32 driveBlocks, HFS_MDB *mdbp)
{
	UInt32	alBlkSize;
	UInt16	numAlBlks;
	UInt32	timeStamp;
	UInt16	bitmapBlocks;
	UInt32	alignment;

	alignment = defaults->hfsAlignment;
	bzero(mdbp, kBytesPerSector);
	
	alBlkSize = defaults->blockSize;

	/* calculate the number of sectors needed for bitmap (rounded up) */
	bitmapBlocks = ((driveBlocks / (alBlkSize >> kLog2SectorSize)) +
			kBitsPerSector-1) / kBitsPerSector;

	mdbp->drAlBlSt = kVolBitMapStart + bitmapBlocks;  /* in sectors (disk blocks) */

	/* If requested, round up block start to a multiple of "alignment" blocks */
	if (alignment != 0)
		mdbp->drAlBlSt = ((mdbp->drAlBlSt + alignment - 1) / alignment) * alignment;
	
	/* Now find out how many whole allocation blocks remain... */
	numAlBlks = (driveBlocks - mdbp->drAlBlSt - kTailBlocks) /
			(alBlkSize >> kLog2SectorSize);

	timeStamp = UTCToLocal(defaults->createDate);
	
	mdbp->drSigWord = kHFSSigWord;
	mdbp->drCrDate = timeStamp;
	mdbp->drLsMod = timeStamp;
	mdbp->drAtrb = kHFSVolumeUnmountedMask;
	mdbp->drVBMSt = kVolBitMapStart;
	mdbp->drNmAlBlks = numAlBlks;
	mdbp->drAlBlkSiz = alBlkSize;
	mdbp->drClpSiz = defaults->dataClumpSize;
	mdbp->drNxtCNID = defaults->nextFreeFileID;
	(UInt16)mdbp->drFreeBks = numAlBlks;
	
	/* XXX UTF-8 to Mac Encoding needed... */
	mdbp->drVN[0] = strlen(defaults->volumeName);
	bcopy(defaults->volumeName, &mdbp->drVN[1], mdbp->drVN[0]);
	
	mdbp->drWrCnt = kWriteSeqNum;

	mdbp->drXTFlSize = mdbp->drXTClpSiz = defaults->extentsClumpSize;
	mdbp->drXTExtRec[0].startBlock = 0;
	mdbp->drXTExtRec[0].blockCount = mdbp->drXTFlSize / alBlkSize;
	(UInt16)mdbp->drFreeBks -= mdbp->drXTExtRec[0].blockCount;

	mdbp->drCTFlSize = mdbp->drCTClpSiz = defaults->catalogClumpSize;
	mdbp->drCTExtRec[0].startBlock = mdbp->drXTExtRec[0].startBlock +
					 mdbp->drXTExtRec[0].blockCount;
	mdbp->drCTExtRec[0].blockCount = mdbp->drCTFlSize / alBlkSize;
	(UInt16)mdbp->drFreeBks -= mdbp->drCTExtRec[0].blockCount;

	if (defaults->flags & kMakeHFSWrapper) {
		mdbp->drFilCnt = mdbp->drNmFls = kWapperFileCount;
		mdbp->drNxtCNID += kWapperFileCount;

		/* set blessed system folder to be root folder (2) */
		mdbp->drFndrInfo[0] = kHFSRootFolderID;

		mdbp->drEmbedSigWord = kHFSPlusSigWord;

		/* software lock it and tag as having "bad" blocks */
		mdbp->drAtrb |= kHFSVolumeSparedBlocksMask;
		mdbp->drAtrb |= kHFSVolumeSoftwareLockMask;
	}
}


/*
 * InitVH
 *
 * Initialize a Volume Header record.
 */
static void
InitVH(hfsparams_t *defaults, UInt32 sectors, HFSPlusVolumeHeader *hp)
{
	UInt32	blockSize;
	UInt32	blockCount;
	UInt32	blocksUsed;
	UInt32	bitmapBlocks;
	UInt16	burnedBlocksBeforeVH = 0;
	UInt16	burnedBlocksAfterAltVH = 0;
	
	bzero(hp, kBytesPerSector);

	blockSize = defaults->blockSize;
	blockCount = sectors / (blockSize >> kLog2SectorSize);

	/*
	 * HFSPlusVolumeHeader is located at sector 2, so we may need
	 * to invalidate blocks before HFSPlusVolumeHeader.
	 */
	if ( blockSize == 512 ) {
		burnedBlocksBeforeVH = 2;		/* 2 before VH */
		burnedBlocksAfterAltVH = 1;		/* 1 after altVH */
	} else if ( blockSize == 1024 ) {
		burnedBlocksBeforeVH = 1;
	}

	bitmapBlocks = defaults->allocationClumpSize / blockSize;

	/* note: add 2 for the Alternate VH, and VH */
	blocksUsed = 2 + burnedBlocksBeforeVH + burnedBlocksAfterAltVH + bitmapBlocks;

	hp->signature = kHFSPlusSigWord;
	hp->version = kHFSPlusVersion;
	hp->attributes = kHFSVolumeUnmountedMask;
	hp->lastMountedVersion = FOUR_CHAR_CODE('10.0');

	/* NOTE: create date is in local time, not GMT!  */
	hp->createDate = UTCToLocal(defaults->createDate);
	hp->modifyDate = defaults->createDate;
	hp->backupDate = 0;
	hp->checkedDate = defaults->createDate;

//	hp->fileCount = 0;
//	hp->folderCount = 0;

	hp->blockSize = blockSize;
	hp->totalBlocks = blockCount;
	hp->freeBlocks = blockCount;	/* will be adjusted at the end */

	hp->rsrcClumpSize = defaults->rsrcClumpSize;
	hp->dataClumpSize = defaults->dataClumpSize;
	hp->nextCatalogID = defaults->nextFreeFileID;
	hp->encodingsBitmap = 1;	/* just set to MacRoman */

	/* set up allocation bitmap file */
	hp->allocationFile.clumpSize = defaults->allocationClumpSize;
	hp->allocationFile.logicalSize = defaults->allocationClumpSize;
	hp->allocationFile.totalBlocks = bitmapBlocks;
  	hp->allocationFile.extents[0].startBlock = 1 + burnedBlocksBeforeVH;
	hp->allocationFile.extents[0].blockCount = bitmapBlocks;
	

	/* set up extents b-tree file */
	hp->extentsFile.clumpSize = defaults->extentsClumpSize;
	hp->extentsFile.logicalSize = defaults->extentsClumpSize;
	hp->extentsFile.totalBlocks = defaults->extentsClumpSize / blockSize;
	hp->extentsFile.extents[0].startBlock =
		hp->allocationFile.extents[0].startBlock +
		hp->allocationFile.extents[0].blockCount;
	hp->extentsFile.extents[0].blockCount = hp->extentsFile.totalBlocks;
	blocksUsed += hp->extentsFile.totalBlocks;


	/* set up catalog b-tree file */
	hp->catalogFile.clumpSize = defaults->catalogClumpSize;
	hp->catalogFile.logicalSize = defaults->catalogClumpSize;
	hp->catalogFile.totalBlocks = defaults->catalogClumpSize / blockSize;
	hp->catalogFile.extents[0].startBlock =
		hp->extentsFile.extents[0].startBlock +
		hp->extentsFile.extents[0].blockCount;
	hp->catalogFile.extents[0].blockCount = hp->catalogFile.totalBlocks;
	blocksUsed += hp->catalogFile.totalBlocks;

	hp->freeBlocks -= blocksUsed;
	
	/*
	 * Add some room for the catalog file to grow...
	 */
	hp->nextAllocation = blocksUsed - 1 - burnedBlocksAfterAltVH +
		(hp->catalogFile.clumpSize / hp->blockSize);
}


/*
 * InitBitmap
 * 	
 * This routine initializes the Allocation Bitmap. Allocation blocks
 * that are in use have their corresponding bit set.
 * 
 * It assumes that initially there are no gaps between allocated blocks.
 * 
 * It also assumes the buffer is big enough to hold all the bits
 * (ie its at least (alBlksUsed/8) bytes in size.
 */
static void
InitBitmap(UInt32 alBlksUsed, UInt32 sectorSize, UInt8 *buffer, UInt32 *bytesUsed)
{
	UInt32	bytes, bits;

	bytes = alBlksUsed >> 3;
	bits  = alBlksUsed & 0x0007;

	(void)memset(buffer, 0xFF, bytes);

	if (bits) {
		*(UInt8 *)(buffer + bytes) = (0xFF00 >> bits) & 0xFF;
		++bytes;
	}
	
	*bytesUsed = ROUNDUP(bytes, sectorSize);

	if (*bytesUsed > bytes)
		bzero(buffer + bytes, *bytesUsed - bytes);
}


static void
InitExtentsFile(const hfsparams_t *dp, HFSExtentDescriptor *bbextp, void *buffer,
	UInt32 *bytesUsed, UInt32 *mapNodes)
{
	BTNodeDescriptor *ndp;
	BTHeaderRec	*bthp;
	UInt32		*bmp;
	UInt32		nodeBitsInHeader;
	UInt32		fileSize;
	UInt32		nodeSize;
	SInt16		offset;
	int		wrapper = (dp->flags & kMakeHFSWrapper);

	*mapNodes = 0;
	fileSize = dp->extentsClumpSize;
	nodeSize = dp->extentsNodeSize;

	bzero(buffer, nodeSize);


	/* FILL IN THE NODE DESCRIPTOR:  */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind = kBTHeaderNode;
	ndp->numRecords = 3;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/* FILL IN THE HEADER RECORD:  */
	bthp = (BTHeaderRec *)((UInt8 *)buffer + offset);
	bthp->treeDepth = 0;
	bthp->rootNode = bthp->firstLeafNode = bthp->lastLeafNode = 0;
	bthp->leafRecords = 0;
	bthp->nodeSize = nodeSize;
	bthp->totalNodes = fileSize / nodeSize;
	bthp->freeNodes = bthp->totalNodes - 1;  /* header */
	bthp->clumpSize = fileSize;

	if (dp->signature == kHFSPlusSigWord) {
		bthp->attributes |= kBTBigKeysMask;
		bthp->maxKeyLength = kHFSPlusExtentKeyMaximumLength;
	} else {
		bthp->maxKeyLength = kHFSExtentKeyMaximumLength;

		/* wrapper has a bad-block extent record */
		if (wrapper) {
			bthp->treeDepth++;
			bthp->leafRecords++;
			bthp->rootNode = 1;
			bthp->firstLeafNode = 1;
			bthp->lastLeafNode = 1;
			bthp->freeNodes--;
		}
	}
	offset += sizeof(BTHeaderRec);

	SETOFFSET(buffer, nodeSize, offset, 2);

	offset += kBTreeHeaderUserBytes;

	SETOFFSET(buffer, nodeSize, offset, 3);


	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	nodeBitsInHeader = 8 * (nodeSize
					- sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)) );

	if (bthp->totalNodes > nodeBitsInHeader) {
		UInt32	nodeBitsInMapNode;
		
		ndp->fLink = bthp->lastLeafNode + 1;
		nodeBitsInMapNode = 8 * (nodeSize
						- sizeof(BTNodeDescriptor)
						- (2 * sizeof(SInt16))
						- 2 );
		*mapNodes = (bthp->totalNodes - nodeBitsInHeader +
			(nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		bthp->freeNodes -= *mapNodes;
	}


	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	bmp = (UInt32 *)((UInt8 *)buffer + offset);
	*bmp = ~((UInt32) 0xFFFFFFFF >> (bthp->totalNodes - bthp->freeNodes));
	offset += nodeBitsInHeader/8;

	SETOFFSET(buffer, nodeSize, offset, 4);

	if (wrapper) {
		InitExtentsRoot(nodeSize, bbextp, (buffer + nodeSize));
	}
	
	*bytesUsed = (bthp->totalNodes - bthp->freeNodes) * nodeSize;
}


static void
InitExtentsRoot(UInt16 btNodeSize, HFSExtentDescriptor *bbextp, void *buffer)
{
	BTNodeDescriptor	*ndp;
	HFSExtentKey		*ekp;
	HFSExtentRecord		*edp;
	SInt16			offset;

	bzero(buffer, btNodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind = kBTLeafNode;
	ndp->numRecords = 1;
	ndp->height = 1;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, btNodeSize, offset, 1);


	/*
	 * First and only record is bad block extents...
	 */
	ekp = (HFSExtentKey *)((UInt8 *) buffer + offset);
	ekp->keyLength = kHFSExtentKeyMaximumLength;
	ekp->forkType = 0;
	ekp->fileID = kHFSBadBlockFileID;
	ekp->startBlock = 0;
	offset += sizeof(HFSExtentKey);

	edp = (HFSExtentRecord *)((UInt8 *)buffer + offset);
	edp[0]->startBlock = bbextp->startBlock;
	edp[0]->blockCount = bbextp->blockCount;
	offset += sizeof(HFSExtentRecord);

	SETOFFSET(buffer, btNodeSize, offset, 2);
}


/*
 * InitCatalogFile
 *	
 * This routine initializes a Catalog B-Tree.
 *
 * Note: Since large volumes can have bigger b-trees they
 * might need to have map nodes setup.
 */
static void
InitCatalogFile(const hfsparams_t *dp, void *buffer, UInt32 *bytesUsed,
	UInt32 *mapNodes)
{
	BTNodeDescriptor *ndp;
	BTHeaderRec	*bthp;
	UInt32		*bmp;
	UInt32		nodeBitsInHeader;
	UInt32		fileSize;
	UInt32		nodeSize;
	SInt16		offset;
	int		wrapper = (dp->flags & kMakeHFSWrapper);

	*mapNodes = 0;
	fileSize = dp->catalogClumpSize;
	nodeSize = dp->catalogNodeSize;

	bzero(buffer, nodeSize);


	/* FILL IN THE NODE DESCRIPTOR:  */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind = kBTHeaderNode;
	ndp->numRecords = 3;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/* FILL IN THE HEADER RECORD:  */
	bthp = (BTHeaderRec *)((UInt8 *)buffer + offset);
	bthp->treeDepth = 1;
	bthp->rootNode  = 1;
	bthp->firstLeafNode = bthp->lastLeafNode = 1;
	bthp->leafRecords = 2;
	bthp->nodeSize = nodeSize;
	bthp->totalNodes = fileSize / nodeSize;
	bthp->freeNodes = bthp->totalNodes - 2;  /* header and root */
	bthp->clumpSize = fileSize;

	if (dp->signature == kHFSPlusSigWord) {
		bthp->attributes |= kBTVariableIndexKeysMask + kBTBigKeysMask;
		bthp->maxKeyLength = kHFSPlusCatalogKeyMaximumLength;
	} else {
		bthp->maxKeyLength = kHFSCatalogKeyMaximumLength;

		if (dp->flags & kMakeHFSWrapper) {
			bthp->treeDepth++;
			bthp->leafRecords += kWapperFileCount;
			bthp->firstLeafNode = bthp->rootNode + 1;
			bthp->lastLeafNode = bthp->firstLeafNode + 1;
			bthp->freeNodes -= 2;  /* tree now split with 2 leaf nodes */
		}
	}
	offset += sizeof(BTHeaderRec);

	SETOFFSET(buffer, nodeSize, offset, 2);

	offset += kBTreeHeaderUserBytes;

	SETOFFSET(buffer, nodeSize, offset, 3);


	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	nodeBitsInHeader = 8 * (nodeSize
					- sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)) );

	if (bthp->totalNodes > nodeBitsInHeader) {
		UInt32	nodeBitsInMapNode;
		
		ndp->fLink = bthp->lastLeafNode + 1;
		nodeBitsInMapNode = 8 * (nodeSize
						- sizeof(BTNodeDescriptor)
						- (2 * sizeof(SInt16))
						- 2 );
		*mapNodes = (bthp->totalNodes - nodeBitsInHeader +
			(nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		bthp->freeNodes -= *mapNodes;
	}


	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	bmp = (UInt32 *)((UInt8 *)buffer + offset);
	*bmp = ~((UInt32)0xFFFFFFFF >> (bthp->totalNodes - bthp->freeNodes));
	offset += nodeBitsInHeader/8;

	SETOFFSET(buffer, nodeSize, offset, 4);
	

	if (dp->signature == kHFSPlusSigWord) {
		buffer += nodeSize;
		InitCatalogRoot_HFSPlus(dp, buffer);

	} else if (wrapper) {
		buffer += nodeSize;
		InitCatalogRoot_HFS(dp, buffer);
		
		buffer += nodeSize;
		InitFirstCatalogLeaf(dp, buffer, TRUE);

		buffer += nodeSize;
		InitSecondCatalogLeaf(dp, buffer);

	} else /* plain HFS */ {
		buffer += nodeSize;
		InitFirstCatalogLeaf(dp, buffer, FALSE);
	}

	*bytesUsed = (bthp->totalNodes - bthp->freeNodes) * nodeSize;
}


static void
InitCatalogRoot_HFSPlus(const hfsparams_t *dp, void * buffer)
{
	BTNodeDescriptor	*ndp;
	HFSPlusCatalogKey	*ckp;
	HFSPlusCatalogKey	*tkp;
	HFSPlusCatalogFolder	*cdp;
	HFSPlusCatalogThread	*ctp;
	UInt16			nodeSize;
	SInt16			offset;
	UInt32			unicodeBytes;


	nodeSize = dp->catalogNodeSize;
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind = kBTLeafNode;
	ndp->numRecords = 2;
	ndp->height = 1;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/*
	 * First record is always the root directory...
	 */
	ckp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);

	if (ConvertUTF8toUnicode(dp->volumeName, sizeof(ckp->nodeName.unicode),
		ckp->nodeName.unicode, &ckp->nodeName.length)) {
		errx(1, "invalid volume name: \"%s\"", dp->volumeName);
	}

	unicodeBytes = sizeof(UniChar) * ckp->nodeName.length;

	ckp->keyLength = kHFSPlusCatalogKeyMinimumLength + unicodeBytes;
	ckp->parentID = kHFSRootParentID;
	offset += ckp->keyLength + 2;

	cdp = (HFSPlusCatalogFolder *)((UInt8 *)buffer + offset);
	cdp->recordType = kHFSPlusFolderRecord;
	cdp->folderID = kHFSRootFolderID;
	cdp->createDate = dp->createDate;
	cdp->contentModDate = dp->createDate;
	cdp->textEncoding = GuessTextEncoding(ckp->nodeName.unicode,
				ckp->nodeName.length);;
	offset += sizeof(HFSPlusCatalogFolder);

	SETOFFSET(buffer, nodeSize, offset, 2);


	/*
	 * Second record is always the root directory thread...
	 */
	tkp = (HFSPlusCatalogKey *)((UInt8 *)buffer + offset);
	tkp->keyLength = kHFSPlusCatalogKeyMinimumLength;
	tkp->parentID = kHFSRootFolderID;
	tkp->nodeName.length = 0;

	offset += tkp->keyLength + 2;

	ctp = (HFSPlusCatalogThread *)((UInt8 *)buffer + offset);
	ctp->recordType = kHFSPlusFolderThreadRecord;
	ctp->parentID = kHFSRootParentID;
	bcopy(&ckp->nodeName, &ctp->nodeName, sizeof(UInt16) + unicodeBytes);
	offset += ( sizeof(HFSPlusCatalogThread)
			- (sizeof(ctp->nodeName.unicode) - unicodeBytes) );

	SETOFFSET(buffer, nodeSize, offset, 3);
}


static void
InitFirstCatalogLeaf(const hfsparams_t *dp, void * buffer, int wrapper)
{
	BTNodeDescriptor	*ndp;
	HFSCatalogKey		*ckp;
	HFSCatalogKey		*tkp;
	HFSCatalogFolder	*cdp;
	HFSCatalogFile		*cfp;
	HFSCatalogThread	*ctp;
	UInt16			nodeSize;
	SInt16			offset;
	UInt32			timeStamp;

	nodeSize = dp->catalogNodeSize;
	timeStamp = UTCToLocal(dp->createDate);
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind = kBTLeafNode;
	ndp->numRecords = 2;
	ndp->height = 1;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);

	/*
	 * First record is always the root directory...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->nodeName[0] = strlen(dp->volumeName);
	bcopy(dp->volumeName, &ckp->nodeName[1], ckp->nodeName[0]);
	ckp->keyLength = 1 + 4 + ((ckp->nodeName[0] + 2) & 0xFE);  /* pad to word */
	ckp->parentID = kHFSRootParentID;
	offset += ckp->keyLength + 1;

	cdp = (HFSCatalogFolder *)((UInt8 *)buffer + offset);
	cdp->recordType = kHFSFolderRecord;
	if (wrapper)
		cdp->valence += kWapperFileCount;
	cdp->folderID = kHFSRootFolderID;
	cdp->createDate = timeStamp;
	cdp->modifyDate = timeStamp;
	offset += sizeof(HFSCatalogFolder);

	SETOFFSET(buffer, nodeSize, offset, 2);

	/*
	 * Second record is always the root directory thread...
	 */
	tkp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	tkp->keyLength = kHFSCatalogKeyMinimumLength;
	tkp->parentID = kHFSRootFolderID;
	tkp->nodeName[0] = 0;
	offset += tkp->keyLength + 2;

	ctp = (HFSCatalogThread *)((UInt8 *)buffer + offset);
	ctp->recordType = kHFSFolderThreadRecord;
	ctp->parentID = kHFSRootParentID;
	bcopy(ckp->nodeName, ctp->nodeName, ckp->nodeName[0]+1);
	offset += sizeof(HFSCatalogThread);

	SETOFFSET(buffer, nodeSize, offset, 3);
	
	/*
	 * For Wrapper volumes there are more file records...
	 */
	if (wrapper) {
		ndp->fLink = 3;
		ndp->numRecords += 2;

		/*
		 * Add "Desktop DB" file...
		 */
		ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
		ckp->keyLength = 1 + 4 + ((kDTDB_Chars + 2) & 0xFE);  /* pad to word */
		ckp->parentID = kHFSRootFolderID;
		ckp->nodeName[0] = kDTDB_Chars;
		bcopy(kDTDB_Name, &ckp->nodeName[1], kDTDB_Chars);
		offset += ckp->keyLength + 1;

		cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
		cfp->recordType = kHFSFileRecord;
		cfp->userInfo.fileType = kDTDB_Type;
		cfp->userInfo.fileCreator = kDTDB_Creator;
		cfp->userInfo.finderFlags = kIsInvisible;
		cfp->fileID = kDTDB_FileID;
		cfp->createDate = timeStamp;
		cfp->modifyDate = timeStamp;
		cfp->dataExtents[0].startBlock = gDTDBFork.startBlock;
		cfp->dataExtents[0].blockCount = gDTDBFork.blockCount;
		cfp->dataPhysicalSize = gDTDBFork.physicalSize;
		cfp->dataLogicalSize = gDTDBFork.logicalSize;
		offset += sizeof(HFSCatalogFile);

		SETOFFSET(buffer, nodeSize, offset, 4);

		/*
		 * Add empty "Desktop DF" file...
		 */
		ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
		ckp->keyLength = 1 + 4 + ((kDTDF_Chars + 2) & 0xFE);  /* pad to word */
		ckp->parentID = kHFSRootFolderID;
		ckp->nodeName[0] = kDTDF_Chars;
		bcopy(kDTDF_Name, &ckp->nodeName[1], kDTDF_Chars);
		offset += ckp->keyLength + 1;

		cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
		cfp->recordType = kHFSFileRecord;
		cfp->userInfo.fileType = kDTDF_Type;
		cfp->userInfo.fileCreator = kDTDF_Creator;
		cfp->userInfo.finderFlags = kIsInvisible;
		cfp->fileID = kDTDF_FileID;
		cfp->createDate = timeStamp;
		cfp->modifyDate = timeStamp;
		offset += sizeof(HFSCatalogFile);

		SETOFFSET(buffer, nodeSize, offset, 5);
	}
}


static void
InitSecondCatalogLeaf(const hfsparams_t *dp, void * buffer)
{
	BTNodeDescriptor	*ndp;
	HFSCatalogKey		*ckp;
	HFSCatalogFile		*cfp;
	UInt16			nodeSize;
	SInt16			offset;
	UInt32			timeStamp;

	nodeSize = dp->catalogNodeSize;
	timeStamp = UTCToLocal(dp->createDate);
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->bLink = 2;
	ndp->kind = kBTLeafNode;
	ndp->numRecords = 3;
	ndp->height = 1;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/*
	 * Add "Finder" file...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength = 1 + 4 + ((kFinder_Chars + 2) & 0xFE);  /* pad to word */
	ckp->parentID = kHFSRootFolderID;
	ckp->nodeName[0] = kFinder_Chars;
	bcopy(kFinder_Name, &ckp->nodeName[1], kFinder_Chars);
	offset += ckp->keyLength + 1;

	cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
	cfp->recordType = kHFSFileRecord;
	cfp->userInfo.fileType = kFinder_Type;
	cfp->userInfo.fileCreator = kFinder_Creator;
	cfp->userInfo.finderFlags = kIsInvisible + kNameLocked + kHasBeenInited;
	cfp->fileID = kFinder_FileID;
	cfp->createDate = timeStamp;
	cfp->modifyDate = timeStamp;
	offset += sizeof(HFSCatalogFile);

	SETOFFSET(buffer, nodeSize, offset, 2);

	/*
	 * Add "ReadMe" file...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength = 1 + 4 + ((kReadMe_Chars + 2) & 0xFE);  /* pad to word */
	ckp->parentID = kHFSRootFolderID;
	ckp->nodeName[0] = kReadMe_Chars;
	bcopy(kReadMe_Name, &ckp->nodeName[1], kReadMe_Chars);
	offset += ckp->keyLength + 1;

	cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
	cfp->recordType = kHFSFileRecord;
	cfp->userInfo.fileType = kReadMe_Type;
	cfp->userInfo.fileCreator = kReadMe_Creator;
	cfp->userInfo.finderFlags = kIsInvisible;
	cfp->fileID = kReadMe_FileID;
	cfp->createDate = timeStamp;
	cfp->modifyDate = timeStamp;
	cfp->dataExtents[0].startBlock = gReadMeFork.startBlock;
	cfp->dataExtents[0].blockCount = gReadMeFork.blockCount;
	cfp->dataPhysicalSize = gReadMeFork.physicalSize;
	cfp->dataLogicalSize = gReadMeFork.logicalSize;
	offset += sizeof(HFSCatalogFile);

	SETOFFSET(buffer, nodeSize, offset, 3);

	/*
	 * Add "System" file...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength = 1 + 4 + ((kSystem_Chars + 2) & 0xFE);  /* pad to word */
	ckp->parentID = kHFSRootFolderID;
	ckp->nodeName[0] = kSystem_Chars;
	bcopy(kSystem_Name, &ckp->nodeName[1], kSystem_Chars);
	offset += ckp->keyLength + 1;

	cfp = (HFSCatalogFile *)((UInt8 *)buffer + offset);
	cfp->recordType = kHFSFileRecord;
	cfp->userInfo.fileType = kSystem_Type;
	cfp->userInfo.fileCreator = kSystem_Creator;
	cfp->userInfo.finderFlags = kIsInvisible + kNameLocked + kHasBeenInited;
	cfp->fileID = kSystem_FileID;
	cfp->createDate = timeStamp;
	cfp->modifyDate = timeStamp;
	cfp->rsrcExtents[0].startBlock = gSystemFork.startBlock;
	cfp->rsrcExtents[0].blockCount = gSystemFork.blockCount;
	cfp->rsrcPhysicalSize = gSystemFork.physicalSize;
	cfp->rsrcLogicalSize = gSystemFork.logicalSize;
	offset += sizeof(HFSCatalogFile);

	SETOFFSET(buffer, nodeSize, offset, 4);
}


static void
InitCatalogRoot_HFS(const hfsparams_t *dp, void * buffer)
{
	BTNodeDescriptor	*ndp;
	HFSCatalogKey		*ckp;
	UInt32			*prp;	/* pointer record */
	UInt16			nodeSize;
	SInt16			offset;

	nodeSize = dp->catalogNodeSize;
	bzero(buffer, nodeSize);

	/*
	 * All nodes have a node descriptor...
	 */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind = kBTIndexNode;
	ndp->numRecords = 2;
	ndp->height = 2;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);


	/*
	 * Add root directory index...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength = kHFSCatalogKeyMaximumLength;
	ckp->parentID = kHFSRootParentID;
	ckp->nodeName[0] = strlen(dp->volumeName);
	bcopy(dp->volumeName, &ckp->nodeName[1], ckp->nodeName[0]);
	offset += ckp->keyLength + 1;

	prp = (UInt32 *)((UInt8 *)buffer + offset);
	*prp = 2;	/* point to first leaf node */
	offset += sizeof(UInt32);

	SETOFFSET(buffer, nodeSize, offset, 2);


	/*
	 * Add finder file index...
	 */
	ckp = (HFSCatalogKey *)((UInt8 *)buffer + offset);
	ckp->keyLength = kHFSCatalogKeyMaximumLength;
	ckp->parentID = kHFSRootFolderID;
	ckp->nodeName[0] = kFinder_Chars;
	bcopy(kFinder_Name, &ckp->nodeName[1], kFinder_Chars);
	offset += ckp->keyLength + 1;

	prp = (UInt32 *)((UInt8 *)buffer + offset);
	*prp = 3;	/* point to last leaf node */
	offset += sizeof(UInt32);

	SETOFFSET(buffer, nodeSize, offset, 3);
}


static void
InitDesktopDB(const hfsparams_t *dp, void *buffer, UInt32 *mapNodes)
{
	BTNodeDescriptor *ndp;
	BTHeaderRec	*bthp;
	UInt32		*bmp;
	UInt32		nodeBitsInHeader;
	UInt32		fileSize;
	UInt32		nodeSize;
	SInt16		offset;
	UInt8		*keyDiscP;

	*mapNodes = 0;
	fileSize = gDTDBFork.logicalSize;
	nodeSize = kHFSNodeSize;

	bzero(buffer, nodeSize);

	/* FILL IN THE NODE DESCRIPTOR:  */
	ndp = (BTNodeDescriptor *)buffer;
	ndp->kind = kBTHeaderNode;
	ndp->numRecords = 3;
	offset = sizeof(BTNodeDescriptor);

	SETOFFSET(buffer, nodeSize, offset, 1);

	/* FILL IN THE HEADER RECORD:  */
	bthp = (BTHeaderRec *)((UInt8 *)buffer + offset);
	bthp->treeDepth = 0;
	bthp->rootNode = bthp->firstLeafNode = bthp->lastLeafNode = 0;
	bthp->leafRecords = 0;
	bthp->nodeSize = nodeSize;
	bthp->maxKeyLength = 37;
	bthp->totalNodes = fileSize / nodeSize;
	bthp->freeNodes = bthp->totalNodes - 1;  /* header */
	bthp->clumpSize = fileSize;
	bthp->btreeType = 0xFF;
	offset += sizeof(BTHeaderRec);

	SETOFFSET(buffer, nodeSize, offset, 2);
	
	keyDiscP = (UInt8 *)((UInt8 *)buffer + offset);
	*keyDiscP++ = 2;		/* length of descriptor */
	*keyDiscP++ = KD_USEPROC;	/* always uses a compare proc */
	*keyDiscP++ = 1;		/* just one of them */
	offset += kBTreeHeaderUserBytes;

	SETOFFSET(buffer, nodeSize, offset, 3);

	/* FIGURE OUT HOW MANY MAP NODES (IF ANY):  */
	nodeBitsInHeader = 8 * (nodeSize
					- sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)) );

	if (bthp->totalNodes > nodeBitsInHeader) {
		UInt32	nodeBitsInMapNode;
		
		ndp->fLink = bthp->lastLeafNode + 1;
		nodeBitsInMapNode = 8 * (nodeSize
						- sizeof(BTNodeDescriptor)
						- (2 * sizeof(SInt16))
						- 2 );
		*mapNodes = (bthp->totalNodes - nodeBitsInHeader +
				(nodeBitsInMapNode - 1)) / nodeBitsInMapNode;
		bthp->freeNodes -= *mapNodes;
	}

	/* 
	 * FILL IN THE MAP RECORD, MARKING NODES THAT ARE IN USE.
	 * Note - worst case (32MB alloc blk) will have only 18 nodes in use.
	 */
	bmp = (UInt32 *)((UInt8 *)buffer + offset);
	*bmp = ~((UInt32)0xFFFFFFFF >> (bthp->totalNodes - bthp->freeNodes));
	offset += nodeBitsInHeader/8;

	SETOFFSET(buffer, nodeSize, offset, 4);
}


static void
WriteSystemFile(const DriveInfo *dip, UInt32 startingSector, UInt32 *filesize)
{
	int fd;
	ssize_t	datasize, writesize;
	UInt8 *buf;
	struct stat stbuf;
	
	if (stat(HFS_BOOT_DATA, &stbuf) < 0)
		err(1, "stat %s", HFS_BOOT_DATA);
	
	datasize = stbuf.st_size;
	writesize = ROUNDUP(datasize, dip->sectorSize);

	if (datasize > (64 * 1024))
		errx(1, "hfsbootdata file too big.");

	if ((buf = malloc(writesize)) == NULL)
		err(1, NULL);
	
	if ((fd = open(HFS_BOOT_DATA, O_RDONLY, 0)) < 0)
		err(1, "open %s", HFS_BOOT_DATA);
	
	if (read(fd, buf, datasize) != datasize) {
		if (errno)
			err(1, "read %s", HFS_BOOT_DATA);
		else
			errx(1, "problems reading %s", HFS_BOOT_DATA);
	}

	if (writesize > datasize)
		bzero(buf + datasize, writesize - datasize);

	WriteBuffer(dip, startingSector, writesize, buf);
	
	close(fd);
	
	free(buf);
	
	*filesize = datasize;
}


static void
WriteReadMeFile(const DriveInfo *dip, UInt32 startingSector, UInt32 *filesize)
{
	ssize_t	datasize, writesize;
	UInt8 *buf;
	
	datasize = sizeof(hfswrap_readme);
	writesize = ROUNDUP(datasize, dip->sectorSize);

	if ((buf = malloc(writesize)) == NULL)
		err(1, NULL);
	
	bcopy(hfswrap_readme, buf, datasize);
	if (writesize > datasize)
		bzero(buf + datasize, writesize - datasize);
	
	WriteBuffer(dip, startingSector, writesize, buf);
	
	*filesize = datasize;
}


/*
 * WriteMapNodes
 *	
 * Initializes a B-tree map node and writes it out to disk.
 */
static void
WriteMapNodes(const DriveInfo *driveInfo, UInt32 diskStart, UInt32 firstMapNode,
	UInt32 mapNodes, UInt16 btNodeSize, void *buffer)
{
	UInt32	sectorsPerNode;
	UInt32	mapRecordBytes;
	UInt16	i;
	BTNodeDescriptor *nd = (BTNodeDescriptor *)buffer;

	bzero(buffer, btNodeSize);

	nd->kind = kBTMapNode;
	nd->numRecords = 1;
	
	/* note: must belong word aligned (hence the extra -2) */
	mapRecordBytes = btNodeSize - sizeof(BTNodeDescriptor) - 2*sizeof(SInt16) - 2;	

	SETOFFSET(buffer, btNodeSize, sizeof(BTNodeDescriptor), 1);
	SETOFFSET(buffer, btNodeSize, sizeof(BTNodeDescriptor) + mapRecordBytes, 2);
	
	sectorsPerNode = btNodeSize/kBytesPerSector;
	
	/*
	 * Note - worst case (32MB alloc blk) will have
	 * only 18 map nodes. So don't bother optimizing
	 * this section to do multiblock writes!
	 */
	for (i = 0; i < mapNodes; i++) {
		if ((i + 1) < mapNodes)
			nd->fLink = ++firstMapNode;  /* point to next map node */
		else
			nd->fLink = 0;  /* this is the last map node */

		WriteBuffer(driveInfo, diskStart, btNodeSize, buffer);
			
		diskStart += sectorsPerNode;
	}
}

/*
 * ClearDisk
 *
 * Clear out consecutive sectors on the disk.
 *
 */
static void
ClearDisk(const DriveInfo *driveInfo, UInt32 startingSector, UInt32 numberOfSectors)
{
	UInt32 bufferSize;
	UInt32 bufferSizeInSectors;
	void *tempBuffer = NULL;
	
	if ( numberOfSectors > 0x100 )	/* pin at 128K */
		bufferSizeInSectors = 0x100;
	else
		bufferSizeInSectors = numberOfSectors;

	bufferSize = bufferSizeInSectors << kLog2SectorSize;

	tempBuffer = valloc((size_t)bufferSize);
	if (tempBuffer == NULL)
		err(1, NULL);

        /* XXX calloc instead ? */
	bzero(tempBuffer, bufferSize);

	while (numberOfSectors > 0) {
		WriteBuffer(driveInfo, startingSector, bufferSize, tempBuffer);
	
		startingSector += bufferSizeInSectors;	
		numberOfSectors -= bufferSizeInSectors;
		
		/* is remainder less than size of buffer? */
		if (numberOfSectors < bufferSizeInSectors) {
			bufferSizeInSectors = numberOfSectors;
			bufferSize = bufferSizeInSectors << kLog2SectorSize;
		}
	}
	
	if (tempBuffer)
		free(tempBuffer);
}


static void
WriteBuffer(const DriveInfo *driveInfo, UInt32 startingSector, UInt32 byteCount,
	const void *buffer)
{
	off_t sector;

	if ((byteCount % driveInfo->sectorSize) != 0)
		errx(1, "WriteBuffer: byte count %ld is not sector size multiple", byteCount);

	sector = driveInfo->sectorOffset + startingSector;

	if (lseek(driveInfo->fd, sector * driveInfo->sectorSize, SEEK_SET) < 0)
		err(1, "seek (sector %ld)", sector);

	if (write(driveInfo->fd, buffer, byteCount) != byteCount)
		err(1, "seek (sector %ld, %ld bytes)", sector, byteCount);
}


static UInt32 Largest( UInt32 a, UInt32 b, UInt32 c, UInt32 d )
{
	/* a := max(a,b) */
	if (a < b)
		a = b;
	/* c := max(c,d) */
	if (c < d)
		c = d;
	
	/* return max(a,c) */
	if (a > c)
		return a;
	else
		return c;
}


/*
 * MarkBitInAllocationBuffer
 * 	
 * Given a buffer and allocation block, will mark off the corresponding
 * bitmap bit, and return the sector number the block belongs in.
 */
static void MarkBitInAllocationBuffer( HFSPlusVolumeHeader *header,
	UInt32 allocationBlock, void* sectorBuffer, UInt32 *sector )
{

	UInt8 *byteP;
	UInt8 mask;
	UInt32 sectorsPerBlock;
	UInt16 bitInSector = allocationBlock % kBitsPerSector;
	UInt16 bitPosition = allocationBlock % 8;
	
	sectorsPerBlock = header->blockSize / kBytesPerSector;

	*sector = (header->allocationFile.extents[0].startBlock * sectorsPerBlock) +
		  (allocationBlock / kBitsPerSector);
	
	byteP = (UInt8 *)sectorBuffer + (bitInSector >> 3);
	mask = ( 0x80 >> bitPosition );
	*byteP |= mask;
}


/*
 * UTCToLocal - convert from Mac OS GMT time to Mac OS local time
 */
static UInt32 UTCToLocal(UInt32 utcTime)
{
	UInt32 localTime = utcTime;
	struct timezone timeZone;
	struct timeval	timeVal;
	
	if (localTime != 0) {

                /* HFS volumes need timezone info to convert local to GMT */
                (void)gettimeofday( &timeVal, &timeZone );


		localTime -= (timeZone.tz_minuteswest * 60);
		if (timeZone.tz_dsttime)
			localTime += 3600;
	}

        return (localTime);
}


static UInt32
DivideAndRoundUp(UInt32 numerator, UInt32 denominator)
{
	UInt32	quotient;
	
	quotient = numerator / denominator;
	if (quotient * denominator != numerator)
		quotient++;
	
	return quotient;
}



/*
 * make a guess at the text encoding value for a given Unicode string
 */
static UInt32
GuessTextEncoding(const UniChar *string, UInt32 charCount)
{
	int i;
	UniChar ch;

	for (i = 0; i < charCount; ++i) {
		ch = string[i];
		/* CJK codepoints are 0x3000 thru 0x9FFF */
		if (ch >= 0x3000) {
			if (ch < 0xa000)
				return (kTextEncodingMacJapanese);

			/* fullwidth codepoints are 0xFF00 thru 0xFFEF */
			if (ch >= 0xff00 && ch <= 0xffef)
				return (kTextEncodingMacJapanese);
		}
	}
	
	return (kTextEncodingMacRoman);
}


static int
ConvertUTF8toUnicode(const UInt8* source, UInt32 bufsize, UniChar* unibuf,
	UInt16 *charcount)
{
	UInt8 byte;
	UniChar* target;
	UniChar* targetEnd;

	*charcount = 0;
	target = unibuf;
	targetEnd = (UniChar *)((UInt8 *)unibuf + bufsize);

	while ((byte = *source++)) {
		
		/* check for single-byte ascii */
		if (byte < 128) {
			*target++ = byte;
		} else {
			UniChar ch;
			UInt8 seq = (byte >> 4);

			switch (seq) {
			case 0xc: /* double-byte sequence (1100 and 1101) */
			case 0xd:
				ch = (byte & 0x1F) << 6;  /* get 5 bits */
				if (((byte = *source++) >> 6) != 2)
					return (EINVAL);
				break;

			case 0xe: /* triple-byte sequence (1110) */
				ch = (byte & 0x0F) << 6;  /* get 4 bits */
				if (((byte = *source++) >> 6) != 2)
					return (EINVAL);
				ch += (byte & 0x3F); ch <<= 6;  /* get 6 bits */
				if (((byte = *source++) >> 6) != 2)
					return (EINVAL);
				break;

			default:
				return (EINVAL);  /* malformed sequence */
			}

			ch += (byte & 0x3F);  /* get last 6 bits */

			if (target >= targetEnd)
				return (ENOBUFS);

			*target++ = ch;
		}
	}

	*charcount = target - unibuf;

	return (0);
}
