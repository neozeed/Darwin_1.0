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
	File:		SVerify2.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	й 1997-1999 by Apple Computer, Inc., all rights reserved.
*/

#include "BTree.h"
#include "BTreePrivate.h"

#include "Scavenger.h"

//	Prototypes for internal subroutines
static int BTKeyChk( SGlobPtr GPtr, NodeDescPtr nodeP, BTreeControlBlock *btcb );

		

/*------------------------------------------------------------------------------

Routine:	ChkExtRec (Check Extent Record)

Function:	Checks out a generic extent record.
			
Input:		GPtr		-	pointer to scavenger global area.
			extP		-	pointer to extent data record.
			
Output:		ChkExtRec	-	function result:			
								0 = no error
								n = error
------------------------------------------------------------------------------*/
OSErr ChkExtRec ( SGlobPtr GPtr, const void *extents )
{
	short		i;
	UInt32		numABlks;
	UInt32		maxNABlks;
	UInt32		extentBlockCount;
	UInt32		extentStartBlock;

	maxNABlks = GPtr->calculatedVCB->vcbTotalBlocks;
	numABlks = 1;

	for ( i=0 ; i<GPtr->numExtents ; i++ )
	{
		if ( GPtr->isHFSPlus )
		{
			extentBlockCount = ((HFSPlusExtentDescriptor *)extents)[i].blockCount;
			extentStartBlock = ((HFSPlusExtentDescriptor *)extents)[i].startBlock;
		}
		else
		{
			extentBlockCount = ((HFSExtentDescriptor *)extents)[i].blockCount;
			extentStartBlock = ((HFSExtentDescriptor *)extents)[i].startBlock;
		}
		
		if ( extentStartBlock >= maxNABlks )
		{
			RcdError( GPtr, E_ExtEnt );
			return( E_ExtEnt );
		}
		if ( extentBlockCount >= maxNABlks )
		{
			RcdError( GPtr, E_ExtEnt );
			return( E_ExtEnt );
		}			
		if ( numABlks == 0 )
		{
			if ( extentBlockCount != 0 )
			{
				RcdError( GPtr, E_ExtEnt );
				return( E_ExtEnt );
			}
		}
		numABlks = extentBlockCount;
	}
	
	return( noErr );
	
}



/*------------------------------------------------------------------------------

Routine:	BTCheck - (BTree Check)

Function:	Checks out the internal structure of a Btree file.  The BTree 
		structure is enunumerated top down starting from the root node.
			
Input:		GPtr		-	pointer to scavenger global area
			realRefNum		-	file refnum

Output:		BTCheck	-	function result:			
		0	= no error
		n 	= error code
------------------------------------------------------------------------------*/

int BTCheck( SGlobPtr GPtr, short refNum )
{
	OSErr			result;
	short			i;
	short			strLen;
	UInt32			nodeNum;
	short			numRecs;
	short			index;
	UInt16			recSize;
	UInt8			parKey[ kMaxKeyLength + 2 + 2 ];
	char			*p;
	UInt8			*dataPtr;
	STPR			*tprP;
	STPR			*parentP;
	KeyPtr			keyPtr;
	BTHeaderRec		*header;
	NodeRec			node;
	NodeDescPtr		nodeDescP;
	UInt16			*statusFlag;
	UInt32			leafRecords = 0;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( refNum );

	//	Set up
	if ( refNum == kCalculatedCatalogRefNum )
		statusFlag	= &(GPtr->CBTStat);
	else if ( refNum == kCalculatedExtentRefNum )
		statusFlag	= &(GPtr->EBTStat);
	else
		statusFlag	= &(GPtr->ABTStat);

	GPtr->TarBlock = 0;

	/*
	 * Check out BTree header node 
	 */
	result = GetNode( calculatedBTCB, kHeaderNodeNum, &node );
	if ( result != noErr )
	{
		if ( result == fsBTInvalidNodeErr )	/* CheckNode failed */
		{
			RcdError( GPtr, E_BadNode );
			result	= E_BadNode;
		}
		return( result );
	}

	nodeDescP = node.buffer;

	result = AllocBTN( GPtr, refNum, 0 );
	ReturnIfError( result );	/* node already allocated */
	
	if ( nodeDescP->kind != kBTHeaderNode )
	{
		RcdError( GPtr, E_BadHdrN );
		return( E_BadHdrN );
	}	
	if ( nodeDescP->numRecords != Num_HRecs )
	{
		RcdError( GPtr, E_BadHdrN );
		return( E_BadHdrN );
	}	
	if ( nodeDescP->height != 0 )
	{
		RcdError( GPtr, E_NHeight );
		return( E_NHeight );
	}

	/*		
	 * check out BTree Header record
	 */
	header = (BTHeaderRec*) ((Byte*)nodeDescP + sizeof(BTNodeDescriptor));
	recSize = GetRecordSize( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, 0 );	
	
	if ( recSize != sizeof(BTHeaderRec) )
	{
		RcdError( GPtr, E_LenBTH );
		return( E_LenBTH );
	}
	if ( header->treeDepth > BTMaxDepth )
	{
		RcdError( GPtr, E_BTDepth );
		return( E_BTDepth );
	}
	calculatedBTCB->treeDepth = header->treeDepth;
	
	if ((header->rootNode < 0) || (header->rootNode >= calculatedBTCB->totalNodes))
	{
		RcdError( GPtr, E_BTRoot );
		return( E_BTRoot );
	}
	calculatedBTCB->rootNode = header->rootNode;

	if ( (calculatedBTCB->treeDepth == 0) || (calculatedBTCB->rootNode == 0) )
	{
		if ( calculatedBTCB->treeDepth == calculatedBTCB->rootNode )
		{
			return( noErr );	/* empty BTree */
		}
		else
		{
			RcdError( GPtr, E_BTDepth );
			return( E_BTDepth );	/* depth doesn't agree with root */
		}
	}		
		
	/*
	 * Set up tree path record for root level
	 */
 	GPtr->BTLevel	= 1;
	tprP		= &(*GPtr->BTPTPtr)[0];
	tprP->TPRNodeN	= calculatedBTCB->rootNode;
	tprP->TPRRIndx	= -1;
	tprP->TPRLtSib	= 0;
	tprP->TPRRtSib	= 0;
	parKey[0]	= 0;
		
	/*
	 * Now enumerate the entire BTree
	 */
	while ( GPtr->BTLevel > 0 )
	{
		tprP	= &(*GPtr->BTPTPtr)[GPtr->BTLevel -1];
		nodeNum	= tprP->TPRNodeN;
		index	= tprP->TPRRIndx;

		GPtr->TarBlock = nodeNum;

		result = GetNode( calculatedBTCB, nodeNum, &node );
		if ( result != noErr )
		{
			if ( result == fsBTInvalidNodeErr )	/* CheckNode failed */
			{
				RcdError( GPtr, E_BadNode );
				result	= E_BadNode;
			}
			return( result );
		}

		nodeDescP = node.buffer;
			
		/*
		 * Check out and allocate the node if its the first time its been seen
		 */		
		if ( index < 0 )
		{
			result = AllocBTN( GPtr, refNum, nodeNum );
			ReturnIfError( result );	/* node already allocated? */
				
			result = BTKeyChk( GPtr, nodeDescP, calculatedBTCB );
			ReturnIfError( result );
				
			if ( nodeDescP->bLink != tprP->TPRLtSib )
			{
				RcdError( GPtr, E_SibLk );
				return( E_SibLk );
			}	
			if ( tprP->TPRRtSib == -1 )
			{
				tprP->TPRRtSib = nodeNum;	/* set Rt sibling for later verification */		
			}
			else
			{
				if ( nodeDescP->fLink != tprP->TPRRtSib )
				{				
					RcdError( GPtr, E_SibLk );
					return( E_SibLk );
				}
			}
			
			if ( (nodeDescP->kind != kBTIndexNode) && (nodeDescP->kind != kBTLeafNode) )
			{
				*statusFlag |= S_RebuildBTree;
				RcdError( GPtr, E_NType );
				result		=  noErr;
			}	
			if ( nodeDescP->height != calculatedBTCB->treeDepth - GPtr->BTLevel + 1 )
			{
				*statusFlag |= S_RebuildBTree;
				RcdError( GPtr, E_NHeight );
				result		=  noErr;
			}
				
			if ( parKey[0] != 0 )
			{
				GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, 0, &keyPtr, &dataPtr, &recSize );
				if ( CompareKeys( (BTreeControlBlockPtr)calculatedBTCB, (BTreeKey *)parKey, keyPtr ) != 0 )
				{
					*statusFlag |= S_RebuildBTree;
					RcdError( GPtr, E_IKey );
				}
			}
			if ( nodeDescP->kind == kBTIndexNode )
 			{
				if ( result = CheckForStop( GPtr ) )
					return( result );
			}
			
			GPtr->itemsProcessed++;
		}
		
		numRecs = nodeDescP->numRecords;
	
		/* 
		 * for an index node ...
		 */
		if ( nodeDescP->kind == kBTIndexNode )
		{
			index++;	/* on to next index record */
			if ( index >= numRecs )
			{
				GPtr->BTLevel--;
				continue;	/* No more records */
			}
			
			tprP->TPRRIndx	= index;
			parentP			= tprP;
			GPtr->BTLevel++;

			if ( GPtr->BTLevel > BTMaxDepth )
			{
				*statusFlag |= S_RebuildBTree;
				RcdError( GPtr, E_BTDepth );
				result		=  noErr;
			}				
			tprP = &(*GPtr->BTPTPtr)[GPtr->BTLevel -1];
			
			GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, index, &keyPtr, &dataPtr, &recSize );
			
			nodeNum = *(UInt32*)dataPtr;
			if ( (nodeNum <= 0) || (nodeNum >= calculatedBTCB->totalNodes) )
			{
				RcdError( GPtr, E_IndxLk );
				return( E_IndxLk );
			}	
			p = (Ptr)keyPtr;	/* Save parents key */
			
			strLen = ( calculatedBTCB->attributes & kBTBigKeysMask ) ? keyPtr->length16 : keyPtr->length8;

			for ( i = 0; i <= strLen; i++ )
				parKey[i] = *p++;
				
			tprP->TPRNodeN = nodeNum;
			tprP->TPRRIndx = -1;
			tprP->TPRLtSib = 0;	/* left sibling */
			
			if ( index > 0 )
			{
				GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, index-1, &keyPtr, &dataPtr, &recSize );

				nodeNum = *(UInt32*)dataPtr;
				if ( (nodeNum <= 0) || (nodeNum >= calculatedBTCB->totalNodes) )
				{
					*statusFlag |= S_RebuildBTree;
					RcdError( GPtr, E_IndxLk );
					result =  noErr;	/* FLASHING */
				}
				tprP->TPRLtSib = nodeNum;
			}
			else
			{
				if ( parentP->TPRLtSib != 0 )
					tprP->TPRLtSib = tprP->TPRRtSib;	/* Fill in the missing link */
			}
				
			tprP->TPRRtSib = 0;	/* right sibling */
			if ( index < (numRecs -1) )
			{
				GetRecordByIndex( (BTreeControlBlock *)calculatedBTCB, nodeDescP, index+1, &keyPtr, &dataPtr, &recSize );
				nodeNum = *(UInt32*)dataPtr;
				if ( (nodeNum <= 0) || (nodeNum >= calculatedBTCB->totalNodes) )
				{
					*statusFlag |= S_RebuildBTree;
					RcdError( GPtr, E_IndxLk );
					result =  noErr;	/* FLASHING */
				}
				tprP->TPRRtSib = nodeNum;
			}
			else
			{
				if ( parentP->TPRRtSib != 0 )
					tprP->TPRRtSib = -1;	/* Link to be filled in later */
			}
		}			
	
		/*
		 * For a leaf node ...
		 */
		else
		{
			if ( tprP->TPRLtSib == 0 )
				calculatedBTCB->firstLeafNode = nodeNum;
			if ( tprP->TPRRtSib == 0 )
				calculatedBTCB->lastLeafNode = nodeNum;
			leafRecords	+= nodeDescP->numRecords;
				
			GPtr->BTLevel--;
			continue;
		}		
	} /* end while */

	calculatedBTCB->leafRecords	= leafRecords;
	
	return( result );

} /* end of BTCheck */



/*------------------------------------------------------------------------------

Routine:	BTMapChk - (BTree Map Check)

Function:	Checks out the structure of a BTree allocation map.
			
Input:		GPtr		- pointer to scavenger global area
		fileRefNum	- refnum of BTree file

Output:		BTMapChk	- function result:			
			0 = no error
			n = error
------------------------------------------------------------------------------*/

int BTMapChk( SGlobPtr GPtr, short fileRefNum )
{
	OSErr				result;
	UInt16				recSize;
	SInt16				mapSize;
	UInt32				nodeNum;
	SInt16				recIndx;
	NodeRec				node;
	NodeDescPtr			nodeDescP;
	BTreeControlBlock	*calculatedBTCB	= GetBTreeControlBlock( fileRefNum );

	nodeNum	= 0;	/* Start with header node */
	recIndx	= 2;	
	mapSize	= ( calculatedBTCB->totalNodes + 7 ) / 8;	/* size in bytes */

	/*
	 * Enumerate the map structure starting with the map record in the header node
	 */
	while ( mapSize > 0 )
	{
		GPtr->TarBlock = nodeNum;
			
		result = GetNode( calculatedBTCB, nodeNum, &node );
		if ( result != noErr )
		{
			if ( result == fsBTInvalidNodeErr )	/* CheckNode failed */
			{
				RcdError( GPtr, E_BadNode );
				result	= E_BadNode;
			}
			return( result );
		}
		
		nodeDescP = node.buffer;
		
		/* Check out the node if its not the header node */	

		if ( nodeNum != 0 )
		{
			result = AllocBTN( GPtr, fileRefNum, nodeNum );
			ReturnIfError( result );	/* Error, node already allocated? */
				
			if ( nodeDescP->kind != kBTMapNode )
			{
				RcdError( GPtr, E_BadMapN );
				return( E_BadMapN );
			}	
			if ( nodeDescP->numRecords != Num_MRecs )
			{
				RcdError( GPtr, E_BadMapN );
				return( E_BadMapN );
			}	
			if ( nodeDescP->height != 0 )
				RcdError( GPtr, E_NHeight );
		}

		//	Move on to the next map node
		recSize  = GetRecordSize( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, recIndx );
		mapSize -= recSize;	/* Adjust remaining map size */

		recIndx	= 0;	/* Map record is now record 0 */	
		nodeNum	= nodeDescP->fLink;						
		if (nodeNum == 0)
			break;
	
	} /* end while */

	if ( (nodeNum != 0) || (mapSize > 0) )
	{
		RcdError( GPtr, E_MapLk);
		return( E_MapLk);	/* bad map node linkage */
	}
	
	return( noErr );
	
} /* end BTMapChk */



/*------------------------------------------------------------------------------

Routine:	CmpBTH - (Compare BTree Header)

Function:	Compares the scavenger BTH info with the BTH on disk.
			
Input:		GPtr - pointer to scavenger global area
		fileRefNum - file refnum

Output:		CmpBTH - function result:			
			0 = no error
			n = error
------------------------------------------------------------------------------*/

OSErr	CmpBTH( SGlobPtr GPtr, SInt16 fileRefNum )
{
	OSErr err;
	BTHeaderRec *bTreeHeader;
	BTreeControlBlock *calculatedBTCB = GetBTreeControlBlock( fileRefNum );
	SInt16 *statP = (SInt16 *)( ( fileRefNum == kCalculatedCatalogRefNum ) ? &GPtr->CBTStat : &GPtr->EBTStat );

	/* 
	 * Get BTree header record from disk 
	 */
	GPtr->TarBlock = 0;	//	Set target node number

	err = GetBTreeHeader( GPtr, fileRefNum, (BTHeaderRec **)&bTreeHeader );
	ReturnIfError( err );

	if ((calculatedBTCB->treeDepth	   != bTreeHeader->treeDepth)	  ||
	    (calculatedBTCB->rootNode	   != bTreeHeader->rootNode)	  ||
	    (calculatedBTCB->leafRecords   != bTreeHeader->leafRecords)	  ||
	    (calculatedBTCB->firstLeafNode != bTreeHeader->firstLeafNode) ||
	    (calculatedBTCB->lastLeafNode  != bTreeHeader->lastLeafNode)  ||
	    (calculatedBTCB->nodeSize	   != bTreeHeader->nodeSize)	  ||
	    (calculatedBTCB->maxKeyLength  != bTreeHeader->maxKeyLength)  ||
	    (calculatedBTCB->totalNodes    != bTreeHeader->totalNodes)	  ||
	    (calculatedBTCB->freeNodes	   != bTreeHeader->freeNodes))
	{
		*statP = *statP | S_BTH;	/* one didn't match, mark it damaged */
		WriteError( GPtr, E_InvalidBTreeHeader, 0, 0 );
	}
	
	return( noErr );
}



/*------------------------------------------------------------------------------

Routine:	CmpBlock

Function:	Compares two data blocks for equality.
			
Input:		Blk1Ptr		-	pointer to 1st data block.
			Blk2Ptr		-	pointer to 2nd data block.
			len			-	size of the blocks (in bytes)

Output:		CmpBlock	-	result code	
			0 = equal
			1 = not equal
------------------------------------------------------------------------------*/

OSErr	CmpBlock( void *block1P, void *block2P, UInt32 length )
{
	Byte	*blk1Ptr = block1P;
	Byte	*blk2Ptr = block2P;

	while ( length-- ) 
		if ( *blk1Ptr++ != *blk2Ptr++ )
			return( -1 );
	
	return( noErr );
	
}



/*------------------------------------------------------------------------------

Routine:	CmpBTM - (Compare BTree Map)

Function:	Compares the scavenger BTM with the BTM on disk.
			
Input:		GPtr		-	pointer to scavenger global area
			fileRefNum		-	file refnum

Output:		CmpBTM	-	function result:			
								0	= no error
								n 	= error
------------------------------------------------------------------------------*/

int CmpBTM( SGlobPtr GPtr, short fileRefNum )
{
	OSErr			result;
	UInt16			recSize;
	short			mapSize;
	short			size;
	UInt32			nodeNum;
	short			recIndx;
	char			*p;
	char			*sbtmP;
	UInt8 *			dataPtr;
	NodeRec			node;
	NodeDescPtr		nodeDescP;
	BTreeControlBlock	*calculatedBTCB;
	SInt16			*statP;

	calculatedBTCB	= GetBTreeControlBlock( fileRefNum );
	statP = (SInt16 *)((fileRefNum == kCalculatedCatalogRefNum) ? &GPtr->CBTStat : &GPtr->EBTStat);

	nodeNum	= 0;	/* start with header node */
	recIndx	= 2;
	recSize = size = 0;
	mapSize	= (calculatedBTCB->totalNodes + 7) / 8;		/* size in bytes */
	sbtmP	= ((BTreeExtensionsRec*)calculatedBTCB->refCon)->BTCBMPtr;
	dataPtr = NULL;

	/*
	 * Enumerate BTree map records starting with map record in header node
	 */
	while ( mapSize > 0 )
	{
		GPtr->TarBlock = nodeNum;
			
		result = GetNode( calculatedBTCB, nodeNum, &node );
		ReturnIfError( result );	/* error, could't get map node */

		nodeDescP = node.buffer;

		recSize = GetRecordSize( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, recIndx );
		dataPtr = GetRecordAddress( (BTreeControlBlock *)calculatedBTCB, (BTNodeDescriptor *)nodeDescP, recIndx );
	
		size	= ( recSize  > mapSize ) ? mapSize : recSize;
			
		result = CmpBlock( sbtmP, dataPtr, size );
		if ( result != noErr )
		{ 	
			*statP = *statP | S_BTM;	/* didn't match, mark it damaged */
			return( noErr );
		}
	
		recIndx	= 0;			/* map record is now record 0 */			
		mapSize	-= size;		/* adjust remaining map size */
		sbtmP	= sbtmP + size;
		nodeNum	= nodeDescP->fLink;	/* next node number */						
		if (nodeNum == 0)
			break;
	
	} /* end while */

	/* 
	 * Make sure the unused portion of the last map record is zero
	 */
	for ( p = (Ptr)dataPtr + size ; p < (Ptr)dataPtr + recSize ; p++ )
		if ( *p != 0 ) 
			*statP	= *statP | S_BTM;	/* didn't match, mark it damaged */

	return( noErr );
	
} /* end CmpBTM */


/*------------------------------------------------------------------------------

Routine:	BTKeyChk - (BTree Key Check)

Function:	Checks out the key structure within a Btree node.
			
Input:		GPtr		-	pointer to scavenger global area
		NodePtr		-	pointer to target node
		BTCBPtr		-	pointer to BTreeControlBlock

Output:		BTKeyChk	-	function result:			
			0 = no error
			n = error code
------------------------------------------------------------------------------*/

static int BTKeyChk( SGlobPtr GPtr, NodeDescPtr nodeP, BTreeControlBlock *btcb )
{
	SInt16				index;
	UInt16				dataSize;
	UInt16				keyLength;
	KeyPtr 				keyPtr;
	UInt8				*dataPtr;
	KeyPtr				prevkeyP	= nil;


	if ( nodeP->numRecords == 0 )
	{
		if ( (nodeP->fLink == 0) && (nodeP->bLink == 0) )
		{
			RcdError( GPtr, E_BadNode );
			return( E_BadNode );
		}
	}
	else
	{
		/*
		 * Loop on number of records in node
		 */
		for ( index = 0; index < nodeP->numRecords; index++)
		{
			GetRecordByIndex( (BTreeControlBlock *)btcb, nodeP, (UInt16) index, &keyPtr, &dataPtr, &dataSize );
	
			if (btcb->attributes & kBTBigKeysMask)
				keyLength = keyPtr->length16;
			else
				keyLength = keyPtr->length8;
				
			if ( keyLength > btcb->maxKeyLength )
			{
				RcdError( GPtr, E_KeyLen );
				return( E_KeyLen );
			}
	
			if ( prevkeyP != nil )
			{
				if ( CompareKeys( (BTreeControlBlockPtr)btcb, prevkeyP, keyPtr ) >= 0 )
				{
					RcdError( GPtr, E_KeyOrd );
					return( E_KeyOrd );
				}
			}
			prevkeyP = keyPtr;
		}
	}

	return( noErr );
}



/*------------------------------------------------------------------------------

Routine:	ChkCName (Check Catalog Name)

Function:	Checks out a generic catalog name.
			
Input:		GPtr		-	pointer to scavenger global area.
		CNamePtr	-	pointer to CName.
			
Output:		ChkCName	-	function result:			
					0 = CName is OK
					E_CName = invalid CName
------------------------------------------------------------------------------*/

OSErr ChkCName( SGlobPtr GPtr, const CatalogName *name, Boolean unicode )
{
	UInt32	length;
	OSErr	err		= noErr;
	
	length = CatalogNameLength( name, unicode );
	
	if ( unicode )
	{
		if ( (length == 0) || (length > kHFSPlusMaxFileNameChars) )
			err = E_CName;
	}
	else
	{
		if ( (length == 0) || (length > kHFSMaxFileNameChars) )
			err = E_CName;
	}
	
	return( err );
}



/*------------------------------------------------------------------------------

Routine:	CompareVolumeBitMap - (Compare Volume Bit Map)

Function:	Compares the scavenger VBM with the VBM on disk.
			
------------------------------------------------------------------------------*/
OSErr	CompareVolumeBitMap( SGlobPtr GPtr, SInt32 whichBuffer )
{
	UInt32			startBlock;
	UInt32			blockCount;
	UInt32			i;
	UInt8			*vbmBlockP;
	OSErr			err;
	UInt32			size;
	
	VolumeBitMapHeader	*volumeBitMap = GPtr->volumeBitMapPtr;
	
	if ( volumeBitMap->bitMapRecord[whichBuffer].count == -1 )
	{
		return( E_UninitializedBuffer );
	}
	
	/* If it hasn't yet been compared, and no VBM errors have been reported yet. */
	if ( (volumeBitMap->bitMapRecord[whichBuffer].isAMatch == false) && (GPtr->VIStat | S_VBM != 0) )
	{
		/* Calculate the start and length of represented BitMap buffer. */
		startBlock = whichBuffer * ( volumeBitMap->bufferSize / Blk_Size );	// whenever whichBuffer is non zero it's a multiple of 512.
		
		if ( whichBuffer == volumeBitMap->numberOfBuffers-1 )
			blockCount = ( ( volumeBitMap->bitMapSizeInBytes - (whichBuffer * volumeBitMap->bufferSize) + Blk_Size - 1 ) / Blk_Size );	// last block may not be 512 byte alligned.
		else
			blockCount = volumeBitMap->bufferSize / Blk_Size;
			
		//	Loop through all the blocks composing the physical buffer.
		for ( i=0 ; i<blockCount ; i++ )
		{
			if ( err = CheckForStop(GPtr) )													//	Permit the user to interrupt
				return( err );

			GPtr->TarBlock = startBlock + i;													//	Set target block number
			
			if ( GPtr->isHFSPlus )
				err = GetBlock_glue( gbReleaseMask, startBlock+i, (Ptr *)&vbmBlockP, kCalculatedAllocationsRefNum, GPtr->calculatedVCB );
			else
				err = GetVBlk( GPtr, startBlock + GPtr->calculatedVCB->vcbVBMSt + i, (void**)&vbmBlockP );	//	get map block from disk
			ReturnIfError( err );
	
			//	check if we don't fill an entire block
			if ( (whichBuffer == volumeBitMap->numberOfBuffers - 1) && (i == blockCount - 1) && (volumeBitMap->bitMapSizeInBytes % Blk_Size != 0) )	//	very last bitmap block
				size = volumeBitMap->bitMapSizeInBytes % Blk_Size;
			else
				size = Blk_Size;
			
			err = memcmp( (volumeBitMap->buffer + i*Blk_Size), vbmBlockP, size );			//	Compare them
			if ( err )										//ее Debugging code
				err = CmpBlock( (volumeBitMap->buffer + i*Blk_Size), vbmBlockP, size );
			
			if ( err != noErr )
			{ 
				RcdError( GPtr, E_VBMDamaged );
				GPtr->VIStat = GPtr->VIStat | S_VBM;
				return( E_VBMDamaged );
			}
		}
		
		volumeBitMap->bitMapRecord[whichBuffer].isAMatch = true;
	}
	
	return( noErr );
}

/*------------------------------------------------------------------------------

Routine:	CmpMDB - (Compare Master Directory Block)

Function:	Compares the scavenger MDB info with the MDB on disk.
			
Input:		GPtr			-	pointer to scavenger global area

Output:		CmpMDB			- 	function result:
									0 = no error
									n = error
			GPtr->VIStat	-	S_MDB flag set in VIStat if MDB is damaged.
------------------------------------------------------------------------------*/

int CmpMDB( SGlobPtr GPtr )
{
	OSErr					result;
	short					i;
	SFCB						*fcbP;
	HFSMasterDirectoryBlock	*mdbP;
	SVCB				*calculatedVCB	= GPtr->calculatedVCB;

	//	Set up
	GPtr->TarID = MDB_FNum;
	
	result = GetVBlk( GPtr, MDB_BlkN, (void**)&mdbP );
	if ( result != noErr )
		return( result );	/* could't get MDB */

	/* 
	 * compare VCB info with MDB
	 */
	if ( mdbP->drSigWord	!= calculatedVCB->vcbSignature )		goto MDBDamaged;
	if ( mdbP->drCrDate	!= calculatedVCB->vcbCreateDate )		goto MDBDamaged;
	if ( mdbP->drLsMod	!= calculatedVCB->vcbModifyDate )		goto MDBDamaged;
	if ( mdbP->drAtrb	!= (UInt16)calculatedVCB->vcbAttributes )	goto MDBDamaged;
	if ( mdbP->drVBMSt	!= calculatedVCB->vcbVBMSt )			goto MDBDamaged;
	if ( mdbP->drNmAlBlks	!= calculatedVCB->vcbTotalBlocks )		goto MDBDamaged;
	if ( mdbP->drClpSiz	!= calculatedVCB->vcbDataClumpSize )		goto MDBDamaged;
	if ( mdbP->drAlBlSt	!= calculatedVCB->vcbAlBlSt )			goto MDBDamaged;
	if ( mdbP->drNxtCNID	!= calculatedVCB->vcbNextCatalogID )		goto MDBDamaged;
	if ( CmpBlock( mdbP->drVN, calculatedVCB->vcbVN, mdbP->drVN[0]+1 ) )	goto MDBDamaged;
	goto ContinueChecking;

MDBDamaged:
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 1, 0 );
		return( noErr );
	}
	
ContinueChecking:
	if ((mdbP->drVolBkUp	!= calculatedVCB->vcbBackupDate)		||
	    (mdbP->drVSeqNum	!= calculatedVCB->vcbVSeqNum)			||
	    (mdbP->drWrCnt	!= calculatedVCB->vcbWriteCount)		||
	    (mdbP->drXTClpSiz	!= calculatedVCB->vcbExtentsFile->fcbClumpSize)	||
	    (mdbP->drCTClpSiz	!= calculatedVCB->vcbCatalogFile->fcbClumpSize)	||
	    (mdbP->drNmRtDirs	!= calculatedVCB->vcbNmRtDirs)			||
	    (mdbP->drFilCnt	!= calculatedVCB->vcbFileCount)			||
	    (mdbP->drDirCnt	!= calculatedVCB->vcbFolderCount)		||
	    (CmpBlock(mdbP->drFndrInfo, calculatedVCB->vcbFinderInfo, 32 ))	//||
	//  (mdbP->drEmbedSigWord		!= calculatedVCB->vcbEmbedSigWord)		||
	//  (mdbP->drEmbedExtent.startBlock	!= calculatedVCB->vcbEmbedExtent.startBlock)	||
	//  (mdbP->drEmbedExtent.blockCount	!= calculatedVCB->vcbEmbedExtent.blockCount)
	   )
	{ 
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 2, 0 );
		return( noErr );
	}

	/* 
	 * compare extent file allocation info with MDB
	 */
	fcbP = calculatedVCB->vcbExtentsFile;	/* compare PEOF for extent file */
	if ( mdbP->drXTFlSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 3, 0 );
		return( noErr );
	}
	for ( i = 0; i < GPtr->numExtents; i++ )
	{
		if ( (mdbP->drXTExtRec[i].startBlock != fcbP->fcbExtents16[i].startBlock) ||
		     (mdbP->drXTExtRec[i].blockCount != fcbP->fcbExtents16[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_MDBDamaged, 4, 0 );
			return( noErr );
		}
	}

	/*
	 * compare catalog file allocation info with MDB
	 */		
	fcbP = calculatedVCB->vcbCatalogFile;	/* compare PEOF for catalog file */
	if ( mdbP->drCTFlSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_MDBDamaged, 5, 0 );
		return( noErr );
	}
	for ( i = 0; i < GPtr->numExtents; i++ )
	{
		if ( (mdbP->drCTExtRec[i].startBlock != fcbP->fcbExtents16[i].startBlock) ||
		     (mdbP->drCTExtRec[i].blockCount != fcbP->fcbExtents16[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_MDBDamaged, 6, 0 );
			return( noErr );
		}
	}
		
	return( noErr );
} /* end CmpMDB */



/*------------------------------------------------------------------------------

Routine:	CompareVolumeHeader - (Compare VolumeHeader Block)

Function:	Compares the scavenger VolumeHeader info with the VolumeHeader on disk.
			
Input:		GPtr			-	pointer to scavenger global area

Output:		CmpMDB			- 	function result:
									0 = no error
									n = error
			GPtr->VIStat	-	S_MDB flag set in VIStat if MDB is damaged.
------------------------------------------------------------------------------*/

OSErr CompareVolumeHeader( SGlobPtr GPtr )
{
	OSErr					err;
	SInt16					i;
	SVCB				*calculatedVCB;
	SFCB						*fcbP;
	HFSPlusVolumeHeader		*volumeHeader;
	UInt32					hfsPlusIOPosOffset;

	calculatedVCB = GPtr->calculatedVCB;
	GPtr->TarID = MDB_FNum;
	
	hfsPlusIOPosOffset = calculatedVCB->vcbEmbeddedOffset;

	err = GetVBlk( GPtr, (hfsPlusIOPosOffset/512)+2, (void**)&volumeHeader );	/* VH is 3rd sector */
	ReturnIfError( err );

	if ( kHFSPlusSigWord				!= calculatedVCB->vcbSignature )	goto VolumeHeaderDamaged;
	if ( volumeHeader->encodingsBitmap		!= calculatedVCB->vcbEncodingsBitmap )	goto VolumeHeaderDamaged;
	if ( (SInt16) (hfsPlusIOPosOffset/512)		!= calculatedVCB->vcbAlBlSt )		goto VolumeHeaderDamaged;
	if ( volumeHeader->createDate			!= calculatedVCB->vcbCreateDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->modifyDate			!= calculatedVCB->vcbModifyDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->backupDate			!= calculatedVCB->vcbBackupDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->checkedDate			!= calculatedVCB->vcbCheckedDate )	goto VolumeHeaderDamaged;
	if ( volumeHeader->rsrcClumpSize		!= calculatedVCB->vcbRsrcClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->dataClumpSize		!= calculatedVCB->vcbDataClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->nextCatalogID		!= calculatedVCB->vcbNextCatalogID )	goto VolumeHeaderDamaged;
	if ( volumeHeader->writeCount			!= calculatedVCB->vcbWriteCount )	goto VolumeHeaderDamaged;
	if ( volumeHeader->fileCount			!= calculatedVCB->vcbFileCount )	goto VolumeHeaderDamaged;
	if ( volumeHeader->folderCount			!= calculatedVCB->vcbFolderCount )	goto VolumeHeaderDamaged;
	if ( volumeHeader->nextAllocation		!= calculatedVCB->vcbNextAllocation )	goto VolumeHeaderDamaged;
	if ( volumeHeader->totalBlocks			!= calculatedVCB->vcbTotalBlocks )	goto VolumeHeaderDamaged;
	if ( volumeHeader->freeBlocks			!= calculatedVCB->vcbFreeBlocks )	goto VolumeHeaderDamaged;
	if ( volumeHeader->blockSize			!= calculatedVCB->vcbBlockSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->attributes			!= calculatedVCB->vcbAttributes )	goto VolumeHeaderDamaged;

	if ( volumeHeader->extentsFile.clumpSize	!= calculatedVCB->vcbExtentsFile->fcbClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->catalogFile.clumpSize	!= calculatedVCB->vcbCatalogFile->fcbClumpSize )	goto VolumeHeaderDamaged;
	if ( volumeHeader->allocationFile.clumpSize	!= calculatedVCB->vcbAllocationFile->fcbClumpSize )	goto VolumeHeaderDamaged;

	if ( CmpBlock( volumeHeader->finderInfo, calculatedVCB->vcbFinderInfo, sizeof(calculatedVCB->vcbFinderInfo) ) )	goto VolumeHeaderDamaged;
	goto ContinueChecking;
	
		
VolumeHeaderDamaged:
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_VolumeHeaderDamaged, 1, 0 );
		return( noErr );

ContinueChecking:

	/*
	 * compare extent file allocation info with VolumeHeader
	 */		
	fcbP = calculatedVCB->vcbExtentsFile;
	if ( volumeHeader->extentsFile.totalBlocks * calculatedVCB->vcbBlockSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_VolumeHeaderDamaged, 3, 0 );
		return( noErr );
	}
	for ( i=0; i < GPtr->numExtents; i++ )
	{
		if ( (volumeHeader->extentsFile.extents[i].startBlock != fcbP->fcbExtents32[i].startBlock) ||
		     (volumeHeader->extentsFile.extents[i].blockCount != fcbP->fcbExtents32[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_VolumeHeaderDamaged, 4, 0 );
			return( noErr );
		}
	}

	/*
	 * compare catalog file allocation info with MDB
	 */	
	fcbP = calculatedVCB->vcbCatalogFile;	/* compare PEOF for catalog file */
	if ( volumeHeader->catalogFile.totalBlocks * calculatedVCB->vcbBlockSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_VolumeHeaderDamaged, 5, 0 );
		return( noErr );
	}
	for ( i=0; i < GPtr->numExtents; i++ )
	{
		if ( (volumeHeader->catalogFile.extents[i].startBlock != fcbP->fcbExtents32[i].startBlock) ||
		     (volumeHeader->catalogFile.extents[i].blockCount != fcbP->fcbExtents32[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;
			WriteError ( GPtr, E_VolumeHeaderDamaged, 6, 0 );
			return( noErr );
		}
	}


	/*
	 * compare bitmap file allocation info with MDB
	 */		
	fcbP = calculatedVCB->vcbAllocationFile;
	if ( volumeHeader->allocationFile.totalBlocks * calculatedVCB->vcbBlockSize != fcbP->fcbPhysicalSize )
	{
		GPtr->VIStat = GPtr->VIStat | S_MDB;
		WriteError ( GPtr, E_VolumeHeaderDamaged, 7, 0 );
		return( noErr );
	}
	for ( i=0; i < GPtr->numExtents; i++ )
	{
		if ( (volumeHeader->allocationFile.extents[i].startBlock != fcbP->fcbExtents32[i].startBlock) ||
		     (volumeHeader->allocationFile.extents[i].blockCount != fcbP->fcbExtents32[i].blockCount) )
		{
			GPtr->VIStat = GPtr->VIStat | S_MDB;				/* didn't match, mark MDB damaged */
			WriteError ( GPtr, E_VolumeHeaderDamaged, 8, 0 );
			return( noErr );
		}
	}

	return( noErr );
}



