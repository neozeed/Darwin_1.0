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



#include "Scavenger.h"
#include "BTree.h"
#include "CaseFolding.h"


SInt32 FastUnicodeCompare ( register ConstUniCharArrayPtr str1, register ItemCount length1,
							register ConstUniCharArrayPtr str2, register ItemCount length2);

//_______________________________________________________________________
//
//	Routine:	FastRelString
//
//	Output:		returns -1 if str1 < str2
//				returns  1 if str1 > str2
//				return	 0 if equal
//
//_______________________________________________________________________

SInt32	FastRelString( ConstStr255Param str1, ConstStr255Param str2 )
{
	SInt32 bestGuess;
	UInt8 length, length2;

	
	length = *(str1++);
	length2 = *(str2++);

	if (length == length2)
		bestGuess = 0;
	else if (length < length2)
		bestGuess = -1;
	else
	{
		bestGuess = 1;
		length = length2;
	}

	while (length--)
	{
		UInt32	aChar, bChar;

		aChar = *(str1++);
		bChar = *(str2++);
		
		if (aChar != bChar)	/* If they don't match exacly, do case conversion */
		{	
			UInt16	aSortWord, bSortWord;

			aSortWord = gCompareTable[aChar];
			bSortWord = gCompareTable[bChar];

			if (aSortWord > bSortWord)
				return 1;

			if (aSortWord < bSortWord)
				return -1;
		}
		
		/*
		 * If characters match exactly, then go on to next character
		 * immediately without doing any extra work.
		 */
	}
	
	/* if you got to here, then return bestGuess */
	return bestGuess;
}	



//
//	FastUnicodeCompare - Compare two Unicode strings; produce a relative ordering
//
//	    IF				RESULT
//	--------------------------
//	str1 < str2		=>	-1
//	str1 = str2		=>	 0
//	str1 > str2		=>	+1
//
//	The lower case table starts with 256 entries (one for each of the upper bytes
//	of the original Unicode char).  If that entry is zero, then all characters with
//	that upper byte are already case folded.  If the entry is non-zero, then it is
//	the _index_ (not byte offset) of the start of the sub-table for the characters
//	with that upper byte.  All ignorable characters are folded to the value zero.
//
//	In pseudocode:
//
//		Let c = source Unicode character
//		Let table[] = lower case table
//
//		lower = table[highbyte(c)]
//		if (lower == 0)
//			lower = c
//		else
//			lower = table[lower+lowbyte(c)]
//
//		if (lower == 0)
//			ignore this character
//
//	To handle ignorable characters, we now need a loop to find the next valid character.
//	Also, we can't pre-compute the number of characters to compare; the string length might
//	be larger than the number of non-ignorable characters.  Further, we must be able to handle
//	ignorable characters at any point in the string, including as the first or last characters.
//	We use a zero value as a sentinel to detect both end-of-string and ignorable characters.
//	Since the File Manager doesn't prevent the NUL character (value zero) as part of a filename,
//	the case mapping table is assumed to map u+0000 to some non-zero value (like 0xFFFF, which is
//	an invalid Unicode character).
//
//	Pseudocode:
//
//		while (1) {
//			c1 = GetNextValidChar(str1)			//	returns zero if at end of string
//			c2 = GetNextValidChar(str2)
//
//			if (c1 != c2) break					//	found a difference
//
//			if (c1 == 0)						//	reached end of string on both strings at once?
//				return 0;						//	yes, so strings are equal
//		}
//
//		// When we get here, c1 != c2.  So, we just need to determine which one is less.
//		if (c1 < c2)
//			return -1;
//		else
//			return 1;
//

SInt32 FastUnicodeCompare ( register ConstUniCharArrayPtr str1, register ItemCount length1,
							register ConstUniCharArrayPtr str2, register ItemCount length2)
{
	register UInt16 c1,c2;
	register UInt16 temp;

	while (1) {
		/* Set default values for c1, c2 in case there are no more valid chars */
		c1 = 0;
		c2 = 0;
		
		/* Find next non-ignorable char from str1, or zero if no more */
		while (length1 && c1 == 0) {
			c1 = *(str1++);
			--length1;
			if ((temp = gLowerCaseTable[c1>>8]) != 0)		// is there a subtable for this upper byte?
				c1 = gLowerCaseTable[temp + (c1 & 0x00FF)];	// yes, so fold the char
		}
		
		
		/* Find next non-ignorable char from str2, or zero if no more */
		while (length2 && c2 == 0) {
			c2 = *(str2++);
			--length2;
			if ((temp = gLowerCaseTable[c2>>8]) != 0)		// is there a subtable for this upper byte?
				c2 = gLowerCaseTable[temp + (c2 & 0x00FF)];	// yes, so fold the char
		}
		
		if (c1 != c2)	/* found a difference, so stop looping */
			break;
		
		if (c1 == 0)		/* did we reach the end of both strings at the same time? */
			return 0;	/* yes, so strings are equal */
	}
	
	if (c1 < c2)
		return -1;
	else
		return 1;
}


//ΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡ
//	Routine:	CompareCatalogKeys
//
//	Function: 	Compares two catalog keys (a search key and a trial key).
//
// 	Result:		+n  search key > trial key
//				 0  search key = trial key
//				-n  search key < trial key
//ΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡ

SInt32
CompareCatalogKeys(HFSCatalogKey *searchKey, HFSCatalogKey *trialKey)
{
	HFSCatalogNodeID	searchParentID, trialParentID;
	SInt32	result;

	searchParentID = searchKey->parentID;
	trialParentID = trialKey->parentID;

	if ( searchParentID > trialParentID )	/* parent dirID is unsigned */
		result = 1;
	else if ( searchParentID < trialParentID )
		result = -1;
	else /* parent dirID's are equal, compare names */
		result = FastRelString(searchKey->nodeName, trialKey->nodeName);

	return result;
}


/* 
 * Routine:	CompareExtendedCatalogKeys
 *
 * Function:	Compares two large catalog keys (a search key and a trial key).
 *
 * Result:	+n  search key > trial key
 *		0  search key = trial key
 *		-n  search key < trial key
 */

SInt32
CompareExtendedCatalogKeys(HFSPlusCatalogKey *searchKey, HFSPlusCatalogKey *trialKey)
{
	SInt32			result;
	HFSCatalogNodeID	searchParentID, trialParentID;

	searchParentID = searchKey->parentID;
	trialParentID = trialKey->parentID;
	
	if ( searchParentID > trialParentID ) 	// parent node IDs are unsigned
	{
		result = 1;
	}
	else if ( searchParentID < trialParentID )
	{
		result = -1;
	}
	else // parent node ID's are equal, compare names
	{
		if ( searchKey->nodeName.length == 0 || trialKey->nodeName.length == 0 )
			result = searchKey->nodeName.length - trialKey->nodeName.length;
		else
			result = FastUnicodeCompare(&searchKey->nodeName.unicode[0], searchKey->nodeName.length,
										&trialKey->nodeName.unicode[0], trialKey->nodeName.length);
	}

	return result;
}


//ΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡ
//	Routine:	CompareExtentKeys
//
//	Function: 	Compares two extent file keys (a search key and a trial key) for
//				an HFS volume.
//ΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡ

SInt32 CompareExtentKeys( const HFSExtentKey *searchKey, const HFSExtentKey *trialKey )
{
	SInt32	result;		//	± 1
	
	#if DEBUG_BUILD
		if (searchKey->keyLength != kHFSExtentKeyMaximumLength)
			DebugStr("\pHFS: search Key is wrong length");
		if (trialKey->keyLength != kHFSExtentKeyMaximumLength)
			DebugStr("\pHFS: trial Key is wrong length");
	#endif
	
	result = -1;		//	assume searchKey < trialKey
	
	if (searchKey->fileID == trialKey->fileID) {
		//
		//	FileNum's are equal; compare fork types
		//
		if (searchKey->forkType == trialKey->forkType) {
			//
			//	Fork types are equal; compare allocation block number
			//
			if (searchKey->startBlock == trialKey->startBlock) {
				//
				//	Everything is equal
				//
				result = 0;
			}
			else {
				//
				//	Allocation block numbers differ; determine sign
				//
				if (searchKey->startBlock > trialKey->startBlock)
					result = 1;
			}
		}
		else {
			//
			//	Fork types differ; determine sign
			//
			if (searchKey->forkType > trialKey->forkType)
				result = 1;
		}
	}
	else {
		//
		//	FileNums differ; determine sign
		//
		if (searchKey->fileID > trialKey->fileID)
			result = 1;
	}
	
	return( result );
}



//ΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡ
//	Routine:	CompareExtentKeysPlus
//
//	Function: 	Compares two extent file keys (a search key and a trial key) for
//				an HFS volume.
//ΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡΡ

SInt32 CompareExtentKeysPlus( const HFSPlusExtentKey *searchKey, const HFSPlusExtentKey *trialKey )
{
	SInt32	result;		//	± 1
	
	#if DEBUG_BUILD
		if (searchKey->keyLength != kHFSPlusExtentKeyMaximumLength)
			DebugStr("\pHFS: search Key is wrong length");
		if (trialKey->keyLength != kHFSPlusExtentKeyMaximumLength)
			DebugStr("\pHFS: trial Key is wrong length");
	#endif
	
	result = -1;		//	assume searchKey < trialKey
	
	if (searchKey->fileID == trialKey->fileID) {
		//
		//	FileNum's are equal; compare fork types
		//
		if (searchKey->forkType == trialKey->forkType) {
			//
			//	Fork types are equal; compare allocation block number
			//
			if (searchKey->startBlock == trialKey->startBlock) {
				//
				//	Everything is equal
				//
				result = 0;
			}
			else {
				//
				//	Allocation block numbers differ; determine sign
				//
				if (searchKey->startBlock > trialKey->startBlock)
					result = 1;
			}
		}
		else {
			//
			//	Fork types differ; determine sign
			//
			if (searchKey->forkType > trialKey->forkType)
				result = 1;
		}
	}
	else {
		//
		//	FileNums differ; determine sign
		//
		if (searchKey->fileID > trialKey->fileID)
			result = 1;
	}
	
	return( result );
}


SInt32 CompareAttributeKeys( const void *inSearchKey, const void *inTrialKey )
{
	const AttributeKey *searchKey = inSearchKey;
	const AttributeKey *trialKey = inTrialKey;
	SInt32 temp;
		
	/*
	 * First, compare the CNID's
	 */
	if (searchKey->cnid != trialKey->cnid) {
		return (searchKey->cnid < trialKey->cnid ? -1 : 1);
	}

	/*
	 * CNID's are equal; compare names
	 */
	if (searchKey->attributeName.length == 0 || trialKey->attributeName.length == 0)
		return (searchKey->attributeName.length < trialKey->attributeName.length ? -1 : 1);

	temp = FastUnicodeCompare(&searchKey->attributeName.unicode[0],
				searchKey->attributeName.length,
				&trialKey->attributeName.unicode[0],
				trialKey->attributeName.length);
	if (temp != 0)
		return (temp);

	/*
	 * Names are equal; compare startBlock
	 */
	if (searchKey->startBlock == trialKey->startBlock)
		return (0);
	else
		return (searchKey->startBlock < trialKey->startBlock ? -1 : 1);
}
