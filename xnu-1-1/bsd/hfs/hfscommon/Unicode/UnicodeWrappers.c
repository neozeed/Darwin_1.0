/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
	File:		UnicodeWrappers.c

	Contains:	Wrapper routines for Unicode conversion and comparison.

	Version:	HFS Plus 1.0

	Written by:	Mark Day

	Copyright:	© 1996-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Mark Day

		Other Contact:		Don Brady

		Technology:			xxx put technology here xxx

	Writers:

		(DSH)	Deric Horn
		(msd)	Mark Day
		(djb)	Don Brady

	Change History (most recent first):
	<MOSXS>	 6/10/99	djb		Add support for Euro Sign (0x20AC) to MacRoman/Unicode conversions.
	<MOSXS>	 2/09/99	djb		Fix UnicodeToMacRoman to handle a terminating decomposed char.
	<MOSXS>	 1/22/99	djb		Add more TARGET_OS_MAC conditionals to remove orphaned code.
	<MOSXS>	  7/6/98	djb		Handle hi-bit Mac Roman characters in basic latin conversions (radar #2247519).
	<MOSXS>	 6/11/98	PPD		Added a few special-case ASCII/Unicode mappings to cover installer's needs.

	  <CS41>	 1/28/98	msd		Bug 2207446: When mangling a name, check to see if the Unicode
									Converter is installed before we call it.
	  <CS40>	 1/21/98	msd		Bug 2206836: If a name contains a colon, change it to question
									mark and mangle the name.
	  <CS39>	12/11/97	msd		For Metrowerks and test tools, call the Get_xxx routines to get
									the Unicode table addresses.
	  <CS38>	12/10/97	djb		Radar #2005461, don't use fallback chars when converting to
									Unicode, instead let the client (Catalog) retry with MacRoman.
	  <CS37>	 12/2/97	DSH		Conditionalize out some unicode related routines for DFA
	  <CS36>	11/26/97	djb		Radar #2005461,2005688 don't swallow kTECPartialCharErr errors!
	  <CS35>	11/17/97	djb		Name mangling was broken with decomposed Unicode.
	  <CS34>	11/16/97	djb		Radar #2001928 - use kUnicodeCanonicalDecompVariant variant.
	  <CS33>	11/11/97	DSH		Use Get_gLowerCaseTable for DiskFirstAid builds to avoid loading
									in a branch to the table.
	  <CS32>	 11/7/97	msd		Replace FastSimpleCompareStrings with FastUnicodeCompare (which
									handles ignorable Unicode characters). Remove the wrapper
									routine, CompareUnicodeNames, and have its callers call
									FastUnicodeCompare directly.
	  <CS31>	10/17/97	djb		Change kUnicodeUseHFSPlusMapping to kUnicodeUseLatestMapping.
	  <CS30>	10/17/97	msd		Fix some type casts for char pointers.
	  <CS29>	10/13/97	djb		Add new SPIs for Finder View font (radar #1679073).
	  <CS28>	 10/1/97	djb		Preserve current heap zone in InitializeEncodingContext routine
									(radar #1682686).
	  <CS27>	 9/17/97	djb		Handle kTECPartialCharErr errors in ConvertHFSNameToUnicode.
	  <CS26>	 9/16/97	msd		In MockConvertFromPStringToUnicode, use pragma unused instead of
									commenting out unused parameter (so SC will compile it).
	  <CS25>	 9/15/97	djb		Fix MockConverters to do either 7-bit ascii or else mangle the
									name (radar #1672388). Use 'p2u#' resource for bootstrapping
									Unicode. Make sure InitializeEncodingContext uses System heap.
	  <CS24>	 9/10/97	msd		Make InitializeEncodingContext public.
	  <CS23>	  9/7/97	djb		Handle 'ª' char in BasicLatinUnicode converter.
	  <CS22>	  9/4/97	djb		Add logging to BasicLatinUnicodeToPascal.
	  <CS21>	 8/26/97	djb		Make FastSimpleCompareStrings faster. Add
									BasicLatinUnicodeToPascal to make 7-bit ascii conversions
									faster.
	  <CS20>	 8/14/97	djb		Add FastRelString here (to be next to the data tables).
	  <CS19>	 7/21/97	djb		LogEndTime now takes an error code.
	  <CS18>	 7/18/97	msd		Include LowMemPriv.h, Gestalt.h, TextUtils.h.
	  <CS17>	 7/16/97	DSH		FilesInternal.i renamed FileMgrInternal.i to avoid name
									collision
	  <CS16>	  7/8/97	DSH		Loading PrecompiledHeaders from define passed in on C line
	  <CS15>	  7/8/97	DSH		InitializeUnicode changed its API
	  <CS14>	  7/1/97	DSH		SC, DFA complier, requires parameters in functions. #pragma'd
									them out to eliminate C warnings.
	  <CS13>	 6/30/97	msd		Remove unused parameter warnings in FallbackProc by commenting
									out unused parameter names.
	  <CS12>	 6/26/97	DSH		FallbackProc declare variables before useage for SC,
									MockConverters no longer static for DFA.
	  <CS11>	 6/25/97	msd		In function InitStaticUnicodeConverter, the variable fsVars was
									being used before being initialized.
	  <CS10>	 6/24/97	DSH		Runtime checks to call through CFM or static linked routines.
	   <CS9>	 6/20/97	msd		Re-introduce fix from <CS7>. Fix another missing cast. Remove a
									spurious semicolon.
	   <CS8>	 6/18/97	djb		Add more ConversionContexts routines. Improved file mangling.
	   <CS7>	 6/16/97	msd		Add a missing cast in GetFileIDString.
	   <CS6>	 6/13/97	djb		Added support for long filenames. Switched to
									ConvertUnicodeToHFSName, ConvertHFSNameToUnicode, and
									CompareUnicodeNames.
	   <CS5>	  6/4/97	djb		Use system script instead of macRoman.
	   <CS4>	 5/19/97	djb		Add call to LockMappingTable so tables won't move!
	   <CS3>	  5/9/97	djb		Include HFSInstrumentation.h
	   <CS2>	  5/7/97	djb		Add summary traces. Add FastSimpleCompareStrings routine.
	   <CS1>	 4/24/97	djb		first checked in
	  <HFS5>	 3/27/97	djb		Add calls to real Unicode conversion routines.
	  <HFS4>	  2/6/97	msd		Add conditional code to use real Unicode comparison routines
									(default to off).
	  <HFS3>	  1/6/97	djb		Fix HFSUnicodeCompare - the final comparison of length1 and
									length2 was backwards.
	  <HFS2>	12/12/96	msd		Use precompiled headers.
	  <HFS1>	12/12/96	msd		first checked in

*/

#include "../../hfs_macos_defs.h"
#include "UCStringCompareData.h"

#include "../headers/FileMgrInternal.h"
#include "../headers/HFSUnicodeWrappers.h"

#include "ConvertUTF.h"

enum {
	kMinFileExtensionChars = 1,		// does not include dot
	kMaxFileExtensionChars = 5		// does not include dot
};

#define kASCIIPiSymbol				0xB9
#define kASCIIMicroSign				0xB5
#define kASCIIGreekDelta			0xC6


#define Is7BitASCII(c)				( (c) >= 0x20 && (c) <= 0x7F )

#define	IsSpecialASCIIChar(c)		( (c) == (UInt8) kASCIIMicroSign || (c) == (UInt8) kASCIIPiSymbol || (c) == (UInt8) kASCIIGreekDelta )

// Note:	'µ' has two Unicode representations 0x00B5 (micro sign) and 0x03BC (greek)
//			'Æ' has two Unicode representations 0x2206 (increment) and 0x0394 (greek)
#define	IsSpecialUnicodeChar(c)		( (c) == 0x00B5 || (c) == 0x03BC || (c) == 0x03C0 || (c) == 0x2206 || (c) == 0x0394 )

#define IsHexDigit(c)				( ((c) >= (UInt8) '0' && (c) <= (UInt8) '9') || ((c) >= (UInt8) 'A' && (c) <= (UInt8) 'F') )


static void	GetFilenameExtension( ItemCount length, ConstUniCharArrayPtr unicodeStr, Str15 extStr );

static void	GetFileIDString( HFSCatalogNodeID fileID, Str15 fileIDStr );

static void AppendPascalString( ConstStr15Param src, Str31 dst );

static UInt32 HexStringToInteger( UInt32 length, const UInt8 *hexStr );



//
// Get filename extension (if any) as a pascal string
//
#if TARGET_API_MAC_OS8
static void
GetFilenameExtension( ItemCount length, ConstUniCharArrayPtr unicodeStr, Str15 extStr )
{
	UInt32	i;
	UniChar	c;
	UInt16	extChars;			// number of extension characters (excluding the dot)
	UInt16	maxExtChars;
	Boolean	foundExtension;


	extStr[0] = (UInt8) 0;		// assume there's no extension

	if ( length < 3 )
		return;					// sorry, "x.y" is smallest possible extension	
	
	if ( length < (kMaxFileExtensionChars + 2) )
		maxExtChars = length - 2;	// we need at least one prefix character and dot
	else
		maxExtChars = kMaxFileExtensionChars;

	i = length;
	extChars = 0;
	foundExtension = false;

	while ( extChars <= maxExtChars )
	{
		c = unicodeStr[--i];

		if ( c == (UniChar) '.' )		// look for leading dot
		{
			if ( extChars > 0 )			// cannot end with a dot
				foundExtension = true;
			break;
		}

		if ( Is7BitASCII(c) || IsSpecialUnicodeChar(c) )
			++extChars;
		else
			break;
	}
	
	// if we found one then copy it
	if ( foundExtension )
	{
		UInt8 *extStrPtr = extStr;
		const UniChar *unicodeStrPtr = &unicodeStr[i];	// point to dot char
		
		*(extStrPtr++) = extChars + 1;		// set length to extension chars plus dot

		for ( i = 0; i <= extChars; ++i )
		{
			c = *(unicodeStrPtr++);
			
			// map any special characters
			switch (c)
			{
				case 0x00B5:			// micro sign
				case 0x03BC:			// greek mu
					c = (UniChar) 'µ';
					break;

				case 0x03C0:			// greek pi
					c = (UniChar) '¹';
					break;

				case 0x2206:			// increment sign
				case 0x0394:			// greek capital delta
					c = (UniChar) 'Æ';
					break;
			}

			*(extStrPtr++) = (UInt8) c;		// copy/convert to ascii
		}
	}

} // end GetFilenameExtension
#endif /* TARGET_API_MAC_OS8 */


//
// Count filename extension characters (if any)
//
static UInt32
CountFilenameExtensionChars( const unsigned char * filename, UInt32	length )
{
	UInt32	i;
	UniChar	c;
	UInt32	extChars;			// number of extension characters (excluding the dot)
	UInt16	maxExtChars;
	Boolean	foundExtension;


    if (length == kUndefinedStrLen)
		length = strlen(filename);

	if ( length < 3 )
		return 0;					// sorry, "x.y" is smallest possible extension	
	
	if ( length < (kMaxFileExtensionChars + 2) )
		maxExtChars = length - 2;	// we need at least on prefix character and dot
	else
		maxExtChars = kMaxFileExtensionChars;

	extChars = 0;				// assume there's no extension
	i = length - 1;				// index to last ascii character
	foundExtension = false;

	while ( extChars <= maxExtChars )
	{
		c = filename[i--];

		if ( c == (UInt8) '.' )		// look for leading dot
		{
			if ( extChars > 0 )			// cannot end with a dot
				return (extChars);

			break;
		}

		if ( Is7BitASCII(c) || IsSpecialASCIIChar(c) )
			++extChars;
		else
			break;
	}
	
	return 0;

} // end CountFilenameExtensionChars


//
// Convert file ID into a hexidecimal string with no leading zeros
//
#if TARGET_API_MAC_OS8
static void
GetFileIDString( HFSCatalogNodeID fileID, Str15 fileIDStr )
{
	SInt32	i, b;
	static UInt8 *translate = (UInt8 *) "0123456789ABCDEF";
	UInt8	c;
	
	fileIDStr[1] = '#';

	for ( i = 1, b = 28; b >= 0; b -= 4 )
	{
		c = *(translate + ((fileID >> b) & 0x0000000F));
		
		// if its not a leading zero add it to our string
		if ( (c != (UInt8) '0') || (i > 1) || (b == 0) )
			fileIDStr[++i] = c;
	}

	fileIDStr[0] = (UInt8) i;

} // end GetFileIDString
#endif /* TARGET_API_MAC_OS8 */


//
// Append a suffix to a pascal string
//
#if TARGET_API_MAC_OS8
static void
AppendPascalString( ConstStr15Param src, Str31 dst )
{
	UInt32	i, j;
	UInt32	srcLen;
	
	srcLen = StrLength(src);
	
	if ( (srcLen + StrLength(dst)) > 31 )	// safety net
		return;
	
	i = dst[0] + 1;		// get end of dst
	
	for (j = 1; j <= srcLen; ++j)
		dst[i++] = src[j];
		
	dst[0] += srcLen;

} // end AppendPascalString
#endif /* TARGET_API_MAC_OS8 */


HFSCatalogNodeID
GetEmbeddedFileID(const unsigned char * filename, UInt32 length, UInt32 *prefixLength)
{
	short	extChars;
	short	i;
	UInt8	c;			// current character in filename

	*prefixLength = 0;

	if ( filename == NULL )
		return 0;

    if (length == kUndefinedStrLen)
		length = strlen(filename);

	if ( length < 4 )
		return 0;		// too small to have a file ID

	if ( length >= 6 )	// big enough for a file ID (#10) and an extension (.x) ?
		extChars = CountFilenameExtensionChars(filename, length);
	else
		extChars = 0;
	
	if ( extChars > 0 )
		length -= (extChars + 1);	// skip dot plus extension characters

	// scan for file id digits...
	for ( i = length - 1; i >= 0; --i)
	{
		c = filename[i];

		if ( c == '#' )		// look for file ID marker
		{
			if ( (length - i) < 3 )
				break;		// too small to be a file ID

			*prefixLength = i;
			return HexStringToInteger(length - i - 1, &filename[i+1]);
		}

		if ( !IsHexDigit(c) )
			break;			// file ID string must have hex digits	
	}

	return 0;

} // end GetEmbeddedFileID


//_______________________________________________________________________

static UInt32
HexStringToInteger (UInt32 length, const UInt8 *hexStr)
{
	UInt32		value;	// decimal value represented by the string
	short		i;
	UInt8		c;		// next character in buffer
	const UInt8	*p;		// pointer to character string

	value = 0;
	p = hexStr;

	for ( i = 0; i < length; ++i )
	{
		c = *p++;

		if (c >= '0' && c <= '9')
		{
			value = value << 4;
			value += (UInt32) c - (UInt32) '0';
		}
		else if (c >= 'A' && c <= 'F')
		{
			value = value << 4;
			value += 10 + ((unsigned int) c - (unsigned int) 'A');
		}
		else
		{
			return 0;	// oops, how did this character get in here?
		}
	}

	return value;

} // end HexStringToInteger


//_______________________________________________________________________
//
//	Routine:	FastRelString
//
//	Output:		returns -1 if str1 < str2
//				returns  1 if str1 > str2
//				return	 0 if equal
//
//_______________________________________________________________________

extern unsigned short gCompareTable[];

SInt32	FastRelString( ConstStr255Param str1, ConstStr255Param str2 )
{
	UInt16*			compareTable;
	SInt32	 		bestGuess;
	UInt8 	 		length, length2;
	UInt8 	 		delta;

	delta = 0;
	length = *(str1++);
	length2 = *(str2++);

	if (length == length2)
		bestGuess = 0;
	else if (length < length2)
	{
		bestGuess = -1;
		delta = length2 - length;
	}
	else
	{
		bestGuess = 1;
		length = length2;
	}

	compareTable = (UInt16*) gCompareTable;

	while (length--)
	{
		UInt8	aChar, bChar;

		aChar = *(str1++);
		bChar = *(str2++);
		
		if (aChar != bChar)		//	If they don't match exacly, do case conversion
		{	
			UInt16	aSortWord, bSortWord;

			aSortWord = compareTable[aChar];
			if (bChar == 0 && delta == 1) {
				bChar = *(str2++);	/* skip over embedded null */
				bestGuess = 0;
			}
			bSortWord = compareTable[bChar];

			if (aSortWord > bSortWord)
				return 1;

			if (aSortWord < bSortWord)
				return -1;
		}
		
		//	If characters match exactly, then go on to next character immediately without
		//	doing any extra work.
	}
	
	//	if you got to here, then return bestGuess
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

extern UInt16 gLowerCaseTable[];
extern UInt16 gLatinCaseFold[];

SInt32 FastUnicodeCompare ( register ConstUniCharArrayPtr str1, register ItemCount length1,
							register ConstUniCharArrayPtr str2, register ItemCount length2)
{
	register UInt16		c1,c2;
	register UInt16		temp;
	register UInt16*	lowerCaseTable;

	lowerCaseTable = (UInt16*) gLowerCaseTable;

	while (1) {
		/* Set default values for c1, c2 in case there are no more valid chars */
		c1 = 0;
		c2 = 0;
		
		/* Find next non-ignorable char from str1, or zero if no more */
		while (length1 && c1 == 0) {
			c1 = *(str1++);
			--length1;
			/* check for basic latin first */
			if (c1 < 0x0100) {
				c1 = gLatinCaseFold[c1];
				break;
			}
			/* case fold if neccessary */
			if ((temp = lowerCaseTable[c1>>8]) != 0)
				c1 = lowerCaseTable[temp + (c1 & 0x00FF)];
		}
		
		
		/* Find next non-ignorable char from str2, or zero if no more */
		while (length2 && c2 == 0) {
			c2 = *(str2++);
			--length2;
			/* check for basic latin first */
			if (c2 < 0x0100) {
				if ((c2 = gLatinCaseFold[c2]) != 0)
					break;
				else
					continue; /* ignore this character */
			}
			/* case fold if neccessary */
			if ((temp = lowerCaseTable[c2>>8]) != 0)
				c2 = lowerCaseTable[temp + (c2 & 0x00FF)];
		}
		
		if (c1 != c2)		//	found a difference, so stop looping
			break;
		
		if (c1 == 0)		//	did we reach the end of both strings at the same time?
			return 0;		//	yes, so strings are equal
	}
	
	if (c1 < c2)
		return -1;
	else
		return 1;
}


OSErr
ConvertUTF8ToUnicode(ByteCount srcLen, const unsigned char* srcStr, ByteCount maxDstLen,
					 ByteCount *actualDstLen, UniCharArrayPtr dstStr)
{
	ConversionResult result;
	UTF8* sourceStart;
	UTF8* sourceEnd;
	UTF16* targetStart;
	UTF16* targetEnd;

	sourceStart = (UTF8*) srcStr;
	sourceEnd = sourceStart + srcLen;
	targetStart = (UTF16*) dstStr;
	targetEnd = targetStart + maxDstLen/2;

	result = ConvertUTF8toUTF16 (&sourceStart, sourceEnd, &targetStart, targetEnd);
	
	*actualDstLen = (targetStart - dstStr) * sizeof(UniChar);
	
	if (result == targetExhausted)
		return kTECOutputBufferFullStatus;
	else if (result == sourceExhausted)
		return kTextMalformedInputErr;

	return noErr;
}


OSErr
ConvertUnicodeToUTF8(ByteCount srcLen, ConstUniCharArrayPtr srcStr, ByteCount maxDstLen,
					 ByteCount *actualDstLen, unsigned char* dstStr)
{
	ConversionResult result;
	UTF16* sourceStart;
	UTF16* sourceEnd;
	UTF8* targetStart;
	UTF8* targetEnd;
	ByteCount outputLength;

	sourceStart = (UTF16*) srcStr;
	sourceEnd = (UTF16*) ((char*) srcStr + srcLen);
	targetStart = (UTF8*) dstStr;
	targetEnd = targetStart + maxDstLen;
	
	result = ConvertUTF16toUTF8 (&sourceStart, sourceEnd, &targetStart, targetEnd);
	
	*actualDstLen = outputLength = targetStart - dstStr;

	if (result == targetExhausted)
		return kTECOutputBufferFullStatus;
	else if (result == sourceExhausted)
		return kTECPartialCharErr;

	if (outputLength >= maxDstLen)
		return kTECOutputBufferFullStatus;
		
	dstStr[outputLength] = 0;	/* also add null termination */

	return noErr;
}

