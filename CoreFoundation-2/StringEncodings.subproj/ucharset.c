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
/*	ucharset.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Aki Inoue
*/

#include <CoreFoundation/CFByteOrder.h>
#include "CFInternal.h"
#include "CFVeryPrivate.h"
#include "CFUniChar.h" 
#include "CFUniCharPriv.h"
#if defined(__MACOS8__)
#include <stdio.h>
#elif defined(__WIN32__)
#include <windows.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#elif defined(__MACH__)
#include <fcntl.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#endif
#include <string.h>

#if defined(__MACOS8__)
#define	MAXPATHLEN FILENAME_MAX
#elif defined WIN32
#define MAXPATHLEN MAX_PATH
#endif

#define BITSPERBYTE	8	/* (CHAR_BIT * sizeof(unsigned char)) */
#define LOG_BPB		3
#define NUMCHARACTERS	65536

/* Number of things in the array keeping the bits.
*/
#define __kCFBitmapSize		(NUMCHARACTERS / BITSPERBYTE)
/* How many elements in the "singles" array before we use binary search.
*/
#define __kCFSetBreakeven 10

/* This tells us, within 1k or so, whether a thing is POTENTIALLY in the set (in the bitmap blob of the private structure) before we bother to do specific checking.
*/
#define __CFCSetBitsInRange(n, i)	(i[n>>15] & (1L << ((n>>10) % 32)))

CF_INLINE void __CFCSetBitmapAddCharacer(UInt8 *bitmap, UniChar theChar) {
    bitmap[(theChar) >> LOG_BPB] |= (((unsigned)1) << (theChar & (BITSPERBYTE - 1)));
}

CF_INLINE Boolean __CFCSetIsMemberBitmap(UniChar theChar, UInt16 charset, const char *bitmap) {
    return ((bitmap[(theChar) >> LOG_BPB] & (((unsigned)1) << (theChar & (BITSPERBYTE - 1)))) ? TRUE : FALSE);
}
#if defined(__MACOS8__)
/* This structure MUST match the sets in NSRulebook.h  The "__CFCSetIsMemberSet()" function is a modified version of the one in Text shlib.
*/
typedef struct _CFCharSetPrivateStruct {
    int issorted;	/* 1=sorted or 0=unsorted ; 2=is_property_table */
    int bitrange[4];	/* bitmap (each bit is a 1k range in space of 2^17) */
    int nsingles;	/* number of single elements */
    int nranges;	/* number of ranges */
    int singmin;	/* minimum single element */
    int singmax;	/* maximum single element */
    int array[1];	/* actually bunch of singles followed by ranges */
} CFCharSetPrivateStruct;

/* Membership function for complex sets
*/
CF_INLINE Boolean __CFCSetIsMemberSet(const CFCharSetPrivateStruct *set, UniChar theChar) {
    int *tmp, *tmp2;
    int i, nel;
    int *p, *q, *wari;

    if (set->issorted != 1) {
        return FALSE;
    }
    theChar &= 0x0001FFFF;	/* range 1-131k */
    if (__CFCSetBitsInRange(theChar, set->bitrange)) {
        if (theChar >= set->singmin && theChar <= set->singmax) {
            tmp = (int *) &(set->array[0]);
            if ((nel = set->nsingles) < __kCFSetBreakeven) {
                for (i = 0; i < nel; i++) {
                    if (*tmp == theChar) return TRUE;
                    ++tmp;
                }
            }
            else {	// this does a binary search
                p = tmp; q = tmp + (nel-1);
                while (p <= q) {
                    wari = (p + ((q-p)>>1));
                    if (theChar < *wari) q = wari - 1;
                    else if (theChar > *wari) p = wari + 1;
                    else return TRUE;
                }
            }
        }
        tmp = (int *) &(set->array[0]) + set->nsingles;
        if ((nel = set->nranges) < __kCFSetBreakeven) {
            i = nel;
            tmp2 = tmp+1;
            while (i) {
                if (theChar <= *tmp2) {
                    if (theChar >= *tmp) return TRUE;
                }
                tmp += 2;
                tmp2 = tmp+1;
                --i;
            }
        } else {	/* binary search the ranges */
            p = tmp; q = tmp + (2*nel-2);
            while (p <= q) {
                i = (q - p) >> 1;	/* >>1 means divide by 2 */
                wari = p + (i & 0xFFFFFFFE); /* &fffffffe make it an even num */
                if (theChar < *wari) q = wari - 2;
                else if (theChar > *(wari + 1)) p = wari + 2;
                else return TRUE;
            }
        }
        return FALSE;
        /* fall through & return zero */
    }
    return FALSE;	/* not a member */
}

/* Take a private "set" structure and make a bitmap from it.  Return the bitmap.  THE CALLER MUST RELEASE THE RETURNED MEMORY as necessary.
*/

CF_INLINE void __CFCSetBitmapProcessManyCharacters(unsigned char *map, unsigned n, unsigned m) {
    unsigned tmp;
    for (tmp = n; tmp <= m; tmp++) __CFCSetBitmapAddCharacer(map, tmp);
}

CF_INLINE void __CFCSetMakeSetBitmapFromSet(const CFCharSetPrivateStruct *theSet, UInt8 *map)
{
    int *ip;
    UniChar ctmp;
    int cnt;

    for (cnt = 0; cnt < theSet->nsingles; cnt++) {
        ctmp = theSet->array[cnt];
        __CFCSetBitmapAddCharacer(map, ctmp);
    }
    ip = (int *) (&(theSet->array[0]) + theSet->nsingles);
    cnt = theSet->nranges;
    while (cnt) {
        /* This could be more efficient: turn on whole bytes at a time
           when there are such cases as 8 characters in a row... */
        __CFCSetBitmapProcessManyCharacters((unsigned char *)map, *ip, *(ip+1));
        ip += 2;
        --cnt;
    }
}

extern const CFCharSetPrivateStruct *_CFdecimalDigitCharacterSetData;
extern const CFCharSetPrivateStruct *_CFletterCharacterSetData;
extern const CFCharSetPrivateStruct *_CFlowercaseLetterCharacterSetData;
extern const CFCharSetPrivateStruct *_CFuppercaseLetterCharacterSetData;
extern const CFCharSetPrivateStruct *_CFnonBaseCharacterSetData;
extern const CFCharSetPrivateStruct *_CFdecomposableCharacterSetData;
extern const CFCharSetPrivateStruct *_CFpunctuationCharacterSetData;
extern const CFCharSetPrivateStruct *_CFalphanumericCharacterSetData;
extern const CFCharSetPrivateStruct *_CFillegalCharacterSetData;
extern const CFCharSetPrivateStruct *_CFhasNonSelfLowercaseMappingData;
extern const CFCharSetPrivateStruct *_CFhasNonSelfUppercaseMappingData;
extern const CFCharSetPrivateStruct *_CFhasNonSelfTitlecaseMappingData;

#else
typedef Boolean (*_charsetFuncType)(UniChar theChar, UInt16 charset, const void *data);

static Boolean loadBitmap(UniChar theChar, UInt16 charset, const void *data);

static struct {
    _charsetFuncType func;
    const void *data;
} _CharsetFuncTable[] = {
    {NULL, NULL}, // kCFUniCharControlCharacterSet
    {NULL, NULL}, // kCFUniCharWhitespaceCharacterSet
    {NULL, NULL}, // kCFUniCharWhitespaceAndNewlineCharacterSet
    {loadBitmap, "decimalDigitCharacterSet"}, // kCFUniCharDecimalDigitCharacterSet
    {loadBitmap, "letterCharacterSet"}, // kCFUniCharLetterCharacterSet
    {loadBitmap, "lowercaseLetterCharacterSet"}, // kCFUniCharLowercaseLetterCharacterSet
    {loadBitmap, "uppercaseLetterCharacterSet"}, // kCFUniCharUppercaseLetterCharacterSet
    {loadBitmap, "nonBaseCharacterSet"}, // kCFUniCharNonBaseCharacterSet
    {loadBitmap, "decomposableCharacterSet"}, // kCFUniCharDecomposableCharacterSet
    {loadBitmap, "alphanumericCharacterSet"}, // kCFUniCharAlphaNumericCharacterSet
    {loadBitmap, "punctuationCharacterSet"}, // kCFUniCharPunctuationCharacterSet
    {loadBitmap, "illegalCharacterSet"}, // kCFUniCharIllegalCharacterSet
    {loadBitmap, "hasNonSelfLowercaseMapping"}, // kCFUniCharHasNonSelfLowercaseMapping
    {loadBitmap, "hasNonSelfUppercaseMapping"}, //  kCFUniCharHasNonSelfUppercaseMapping
    {loadBitmap, "hasNonSelfTitlecaseMapping"}, //  kCFUniCharHasNonSelfTitlecaseMapping
};
#endif !defined(__WIN32__) && !defined(__MACOS8__)

/* Based on UnicodeData-2.1.8.txt */
CF_INLINE Boolean isControl(UniChar theChar, UInt16 charset, const void *data) { // ISO Control
    if ((theChar <= 0x001F) || (theChar >= 0x007F && theChar <= 0x009F)) return TRUE;
    return FALSE;
}

CF_INLINE Boolean isWhitespace(UniChar theChar, UInt16 charset, const void *data) { // Space
    if ((theChar == 0x0020) || (theChar == 0x00A0) || (theChar >= 0x2000 && theChar <= 0x200B) || (theChar == 0x3000)) return TRUE;
    return FALSE;
}

CF_INLINE Boolean isWhitespaceAndNewLine(UniChar theChar, UInt16 charset, const void *data) { // White space
    if (isWhitespace(theChar, charset, data) || (theChar >= 0x0009 && theChar <= 0x000D) || (theChar == 0x2028) || (theChar == 0x2029)) return TRUE;
    return FALSE;
}

#if !defined(__MACOS8__)

#define CHARSET_DIR ("/CoreServices/CharacterSets/")

CF_INLINE void __characterSetPath(char *cpath) {
    strcpy(cpath, "/System/Library");
    strcat(cpath, CHARSET_DIR);
}

CF_INLINE Boolean __loadBytesFromFile(const char *fileName, void**bytes) {
#if defined(__WIN32__)
    HANDLE bitmapFileHandle;
    HANDLE mappingHandle;

    if ((bitmapFileHandle = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) return FALSE;
    mappingHandle = CreateFileMapping(bitmapFileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    CloseHandle(bitmapFileHandle);
    if (!mappingHandle) return FALSE;

    *bytes = MapViewOfFileEx(mappingHandle, FILE_MAP_READ, 0, 0, 0, NULL);
    CloseHandle(mappingHandle);

    return (*bytes ? TRUE : FALSE);
#else
    struct stat statBuf;
    int fd = -1;

    if ((fd = open(fileName, O_RDONLY, 0)) < 0) return FALSE;

#if defined(__MACH__)
    if (fstat(fd, &statBuf) < 0 || map_fd(fd, 0, (vm_offset_t *)bytes, TRUE, (vm_size_t)statBuf.st_size)) {
        close(fd);
        return FALSE;
    }
#else // PDO
    if (fstat(fd, &statBuf) < 0 || (*bytes = mmap(0, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == -1) {
        close(fd);

        return FALSE;
    }
#endif
    __CFRecordAllocationEvent(__kCFVMAllocateEvent, *bytes, statBuf.st_size, 0, NULL);
    close(fd);

    return TRUE;
#endif
}

static Boolean loadBitmap(UniChar theChar, UInt16 charset, const void *data) {
    if (!data) return FALSE;

    if (_UniCharLoadCharacterSetBitmapWithName((const char*)data, (void**)&(_CharsetFuncTable[charset - 1].data))) {
        _CharsetFuncTable[charset - 1].func = (_charsetFuncType)__CFCSetIsMemberBitmap;
        return __CFCSetIsMemberBitmap(theChar, charset, (const void*)_CharsetFuncTable[charset - 1].data);
    } else {
        _CharsetFuncTable[charset - 1].data = NULL;
        return FALSE;
    }
}

Boolean _UniCharLoadCharacterSetBitmapWithName(const char *bitmapName, void **bytes) {
    char cpath[MAXPATHLEN];

    __characterSetPath(cpath);
    strcat(cpath, bitmapName);
    strcat(cpath, ".bitmap");

    return __loadBytesFromFile(cpath, bytes);
}
#endif !defined(__MACOS8__)

Boolean CFUniCharIsMemberOf(UniChar theChar, UInt32 charset) {
   switch (charset) {
        case kCFUniCharControlCharacterSet:
            return isControl(theChar, charset, NULL);

        case kCFUniCharWhitespaceAndNewlineCharacterSet:
            return isWhitespaceAndNewLine(theChar, charset, NULL);

        case kCFUniCharWhitespaceCharacterSet:
            return isWhitespace(theChar, charset, NULL);

#if defined(__MACOS8__)
        case kCFUniCharDecimalDigitCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFdecimalDigitCharacterSetData, theChar);
        case kCFUniCharLetterCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFletterCharacterSetData, theChar);
        case kCFUniCharLowercaseLetterCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFlowercaseLetterCharacterSetData, theChar);
        case kCFUniCharUppercaseLetterCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFuppercaseLetterCharacterSetData, theChar);
        case kCFUniCharNonBaseCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFnonBaseCharacterSetData, theChar);
        case kCFUniCharAlphaNumericCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFalphanumericCharacterSetData, theChar);
        case kCFUniCharDecomposableCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFdecomposableCharacterSetData, theChar);
        case kCFUniCharPunctuationCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFpunctuationCharacterSetData, theChar);
        case kCFUniCharIllegalCharacterSet:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFillegalCharacterSetData, theChar);
        case kCFUniCharHasNonSelfLowercaseMapping:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFhasNonSelfLowercaseMappingData, theChar);
        case kCFUniCharHasNonSelfUppercaseMapping:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFhasNonSelfUppercaseMappingData, theChar);
        case kCFUniCharHasNonSelfTitlecaseMapping:
return __CFCSetIsMemberSet((const CFCharSetPrivateStruct *)&_CFhasNonSelfTitlecaseMappingData, theChar);
#else
        case kCFUniCharDecimalDigitCharacterSet:
        case kCFUniCharLetterCharacterSet:
        case kCFUniCharLowercaseLetterCharacterSet:
        case kCFUniCharUppercaseLetterCharacterSet:
        case kCFUniCharNonBaseCharacterSet:
        case kCFUniCharAlphaNumericCharacterSet:
        case kCFUniCharDecomposableCharacterSet:
        case kCFUniCharPunctuationCharacterSet:
        case kCFUniCharIllegalCharacterSet:
        case kCFUniCharHasNonSelfLowercaseMapping:
        case kCFUniCharHasNonSelfUppercaseMapping:
        case kCFUniCharHasNonSelfTitlecaseMapping:
            return _CharsetFuncTable[charset - 1].func(theChar, charset, _CharsetFuncTable[charset - 1].data);
#endif

        default:
            return FALSE;
    }
}

void _UniCharGetBitmap(char *bitmap, UInt32 charset, Boolean isInverted) {
    UInt32 *bits = (UInt32 *)bitmap;
    UInt32 length = __kCFBitmapSize / sizeof(UInt32);

    if (charset < kCFUniCharDecimalDigitCharacterSet) { // it's hard coded
        UInt32 index;

        while (length--) *bits++ = 0;

        if (charset == kCFUniCharControlCharacterSet) {
            for (index = 0;index < 0x0020;index++) __CFCSetBitmapAddCharacer(bitmap, (UniChar)index);
            for (index = 0x007F;index < 0x00A0;index++) __CFCSetBitmapAddCharacer(bitmap, (UniChar)index);
        } else {
            __CFCSetBitmapAddCharacer(bitmap, 0x0020); // Space
            __CFCSetBitmapAddCharacer(bitmap, 0x00A0); // no-break Space
            for (index = 0x2000;index < 0x200C;index++) __CFCSetBitmapAddCharacer(bitmap, (UniChar)index); // General punct spaces
            __CFCSetBitmapAddCharacer(bitmap, 0x3000); // Ideographics Space

            if (charset == kCFUniCharWhitespaceAndNewlineCharacterSet) {
                for (index = 0x0009;index < 0x000E;index++) __CFCSetBitmapAddCharacer(bitmap, (UniChar)index); // Controls
                __CFCSetBitmapAddCharacer(bitmap, 0x2028); // Line separator
                __CFCSetBitmapAddCharacer(bitmap, 0x2029); // Para separator
            }
        }
        bits = (UInt32 *)bitmap;
        length = __kCFBitmapSize / sizeof(UInt32);
    } else {
#if defined(__MACOS8__)
switch (charset) {
        case kCFUniCharDecimalDigitCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFdecimalDigitCharacterSetData, bitmap); break;
        case kCFUniCharLetterCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFletterCharacterSetData, bitmap); break;
        case kCFUniCharLowercaseLetterCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFlowercaseLetterCharacterSetData, bitmap); break;
        case kCFUniCharUppercaseLetterCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFuppercaseLetterCharacterSetData, bitmap); break;
        case kCFUniCharNonBaseCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFnonBaseCharacterSetData, bitmap); break;
        case kCFUniCharAlphaNumericCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFalphanumericCharacterSetData, bitmap); break;
        case kCFUniCharDecomposableCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFdecomposableCharacterSetData, bitmap); break;
        case kCFUniCharPunctuationCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFpunctuationCharacterSetData, bitmap); break;
        case kCFUniCharIllegalCharacterSet:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFillegalCharacterSetData, bitmap); break;
        case kCFUniCharHasNonSelfLowercaseMapping:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFhasNonSelfLowercaseMappingData, bitmap); break;
        case kCFUniCharHasNonSelfUppercaseMapping:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFhasNonSelfUppercaseMappingData, bitmap); break;
        case kCFUniCharHasNonSelfTitlecaseMapping:
__CFCSetMakeSetBitmapFromSet((const CFCharSetPrivateStruct *)&_CFhasNonSelfTitlecaseMappingData, bitmap); break;
}
#else
        _CharsetFuncTable[charset - 1].func(0, charset, _CharsetFuncTable[charset - 1].data); // force loading the bitmap
        CFMoveMemory(bitmap, _CharsetFuncTable[charset - 1].data, __kCFBitmapSize);
#endif
    }
    if (isInverted)
        while (length--) {
            *bits = ~(*bits);
            ++bits;
        }
}

static unsigned _CFConvertToLower(UniChar ch, UniChar *outch, unsigned int flags);
static unsigned _CFConvertToUpper(UniChar ch, UniChar *outch, unsigned int flags);
static unsigned _CFConvertToTitle(UniChar ch, UniChar *outch, unsigned int flags);
static unsigned _CFDecomposeCharacter(UniChar ch, UniChar *outch, UInt32 maxLength);

UInt32 CFUniCharMapTo(UniChar theChar, UniChar *convertedChar, UInt32 maxLength, UInt16 ctype, UInt32 flags) {
    switch (ctype) {
        case kCFUniCharToLowercase:
            return _CFConvertToLower(theChar, convertedChar, flags);
        case kCFUniCharToUppercase:
            return _CFConvertToUpper(theChar, convertedChar, flags);
        case kCFUniCharToTitlecase:
            return _CFConvertToTitle(theChar, convertedChar, flags);
        case kCFUniCharDecompose:
            return _CFDecomposeCharacter(theChar, convertedChar, maxLength);

        default:
            return 0;
    }
}

#define BUFFER_LEN (1)

UniChar toulower(UniChar theChar) {
    UniChar buffer;

    _CFConvertToLower(theChar, &buffer, 0);
    return buffer;
}

UniChar touupper(UniChar theChar) {
    UniChar buffer;

    _CFConvertToUpper(theChar, &buffer, 0);
    return buffer;
}

UniChar toutitle(UniChar theChar) {
    UniChar buffer;

    _CFConvertToTitle(theChar, &buffer, 0);
    return buffer;
}

/* ================================================================ */
/* Begin case conversion functions
 */

static unsigned __CFConvertToLowerCase(UniChar ch, UniChar *outch);
static unsigned __CFConvertToUpperCase(UniChar ch, UniChar *outch);
static unsigned __CFConvertToTitleCase(UniChar ch, UniChar *outch);

/* Conversion tables for 8859-1 characters.  These tables will NOT work for arbitrary Unicode characters, only for characters defined in the first 256.
_CFConvertToLower, _CFConvertToTitle, can all be called with any character.  For things outside the NextStep encoding & various "exceptions", tables have to be loaded, so the private functions: __CFConvertToLowerCase, __CFConvertToUpperCase, __CFConvertToTitleCase are used.
*/

static const UniChar _CFToUpperLatinTable[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
        0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
        0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
        0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
        0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
        0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
        0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
        0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
        0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
        0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
        0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
        0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
        0x0060, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
        0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
        0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
        0x0058, 0x0059, 0x005A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
        0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
        0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
        0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
        0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
        0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
        0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
        0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
        0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
        0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
        0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
        0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
        0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
        0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
        0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
        0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00F7,
        0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x0178
};

static const UniChar _CFToLowerLatinTable[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
        0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
        0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
        0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
        0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
        0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
        0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
        0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
        0x0040, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
        0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
        0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
        0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
        0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
        0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
        0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
        0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
        0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
        0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
        0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
        0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
        0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
        0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
        0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
        0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
        0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00D7,
        0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00DF,
        0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
        0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
        0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
        0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

static unsigned _CFConvertToUpper(UniChar ch, UniChar *outch, unsigned int flags) {
    if (flags & kCFUniCharDottedICaseFlag) {
        if (ch == 'i') { /* Turkish lower dotted-i to upper dotted-i */
            *outch = 0x0130; return 1;
        } else if (ch == 0x0131) { /* Turkish lower UNdotted-i to upper UNdotted-i */
            *outch = 0x0049; return 1;
        }
    }
    switch (ch) {
    case 0x00DF: *outch++ = (UniChar)'S'; *outch = (UniChar)'S'; return 2; // eszette
    case 0x0153: *outch = 0x0152; return 1;
    case 0x0142: *outch = 0x0141; return 1;	// l-slash, L-slash
    case 0xFB00: *outch++ = 0x0046; *outch = 0x0046; return 2; // FF
    case 0xFB01: *outch++ = 0x0046; *outch = 0x0049; return 2; // FI
    case 0xFB02: *outch++ = 0x0046; *outch = 0x004C; return 2; // FL
    case 0xFB03: *outch++ = 0x0046; *outch++ = 0x0046; *outch = 0x0049; return 3; // FFI
    case 0xFB04: *outch++ = 0x0046; *outch++ = 0x0046; *outch = 0x004C; return 3; // FFL
    case 0xFB05: *outch++ = 0x0053; *outch = 0x0074; return 2; // ST
    case 0xFB06: *outch++ = 0x0053; *outch = 0x0074; return 2; // ST
    }
    if (ch < 256) {
        *outch = _CFToUpperLatinTable[ch];
        return 1;
    }
    return __CFConvertToUpperCase(ch, outch);
}

static unsigned _CFConvertToTitle(UniChar ch, UniChar *outch, unsigned int flags) {
    int cnt;
    if (flags & kCFUniCharDottedICaseFlag) {
        if (ch == 'i') { /* Turkish lower dotted-i to upper dotted-i */
            *outch = 0x0130; return 1;
        } else if (ch == 0x0131) { /* Turkish lower UNdotted-i to upper UNdotted-i */
            *outch = 0x0049; return 1;
        }
    }
    switch (ch) { // these convert ligatures to one upper + lower
    case 0x00DF: *outch++ = 0x0053; *outch = 0x0073; return 2; // eszette-> Ss
    case 0xFB00: *outch++ = 0x0046; *outch = 0x0066; return 2; // ff
    case 0xFB01: *outch++ = 0x0046; *outch = 0x0069; return 2; // fi
    case 0xFB02: *outch++ = 0x0046; *outch = 0x006C; return 2; // fl
    case 0xFB03: *outch++ = 0x0046; *outch++ = 0x0066; *outch = 0x0069; return 3; // ffi
    case 0xFB04: *outch++ = 0x0046; *outch++ = 0x0066; *outch = 0x006C; return 3; // ffl
    case 0xFB05: *outch++ = 0x0053; *outch = 0x0074; return 2; // long-s t
    case 0xFB06: *outch++ = 0x0053; *outch = 0x0074; return 2; // st
    }

    if (ch < 128) {
        return _CFConvertToUpper(ch, outch, flags);
    } else {
        if ((cnt = __CFConvertToTitleCase(ch, outch))) return cnt;
    }
    return _CFConvertToUpper(ch, outch, flags);
}

static unsigned _CFConvertToLower(UniChar ch, UniChar *outch, unsigned int flags) {
    if (flags & kCFUniCharDottedICaseFlag) {
        if (ch == 0x0130) { /* Turkish upper dotted-i to lower dotted-I */
            *outch = 0x0069; return 1;
        } else if (ch == 0x0049) { /* Turkish upper UNdotted-i to lower UNdotted-i */
            *outch = 0x0131; return 1;
        }
    }
    switch (ch) {
    case 0x0152: *outch = 0x0153; return 1;
    case 0x0141: *outch = 0x0142; return 1;
    case 0x0178: *outch = 0x00FF; return 1;
    }
    if (ch < 256) {
        *outch = _CFToLowerLatinTable[ch];
        return 1;
    }
    return __CFConvertToLowerCase(ch, outch);
}

struct _nx_short_stru {
    unsigned short _from;	/* ??? really UniChar */
    unsigned short _to;
};

#if !defined(__MACOS8__)
CF_INLINE Boolean _loadConvTable(struct _nx_short_stru **addr, const char *fileName) {
    const char *suffix = (CFByteOrderGetCurrent() == CFByteOrderBigEndian) ? "-B.data" : "-L.data";
    char cpath[MAXPATHLEN];
    char *sansSuffixEnd;
    void *bytes;

    __characterSetPath(cpath);
    strcat(cpath, fileName);
    sansSuffixEnd = cpath + strlen(cpath);
    strcpy(sansSuffixEnd, suffix);

    if (!__loadBytesFromFile(cpath, &bytes)) {
        // Try without suffix
        *sansSuffixEnd = '\0';
        if (!__loadBytesFromFile(cpath, &bytes))
            return FALSE;
    }
    *addr = (struct _nx_short_stru *)bytes;
    return TRUE;
}
#endif

static unsigned short _CF16To16BitEncoding(const struct _nx_short_stru *theTable, unsigned int numElem, unsigned short aChar) {
    const struct _nx_short_stru *p, *q, *divider;

    if ((aChar < theTable[0]._from) || (aChar > theTable[numElem-1]._from)) {
        return 0;
    }
    p = theTable;
    q = p + (numElem-1);
    while (p <= q) {
        divider = p + ((q - p) >> 1);	/* divide by 2 */
        if (aChar < divider->_from) { q = divider - 1; }
        else if (aChar > divider->_from) { p = divider + 1; }
        else return divider->_to;
    }
    return 0;
}

static unsigned short *__CFCaseTableStart = (unsigned short *)0;
static struct _nx_short_stru *__CFToLowerTable = (struct _nx_short_stru *)0;
static struct _nx_short_stru *__CFToUpperTable = (struct _nx_short_stru *)0;
static struct _nx_short_stru *__CFToTitleTable = (struct _nx_short_stru *)0;

static void __CFLoadCaseConversionTables(void) {
    int tablesize;

#if defined(__MACOS8__)
extern unsigned short *_CFCaseMappingData;
    __CFCaseTableStart = _CFCaseMappingData;
#else
    if (!(tablesize = _loadConvTable((struct _nx_short_stru **)&__CFCaseTableStart, "CaseMapping"))) {
        return;
    }
#endif
    __CFToLowerTable = (struct _nx_short_stru *)&(__CFCaseTableStart[4]);
    __CFToUpperTable = (struct _nx_short_stru *)&(__CFToLowerTable[__CFCaseTableStart[1]]);
    __CFToTitleTable = (struct _nx_short_stru *)&(__CFToUpperTable[__CFCaseTableStart[2]]);
}

static unsigned __CFConvertToLowerCase(UniChar ch, UniChar *outch) {
    UniChar tmpBuffer;
    if (CFUniCharIsMemberOf(ch, kCFUniCharHasNonSelfLowercaseMapping)) {
        if (! __CFToLowerTable) __CFLoadCaseConversionTables();
        tmpBuffer = _CF16To16BitEncoding(__CFToLowerTable, __CFCaseTableStart[1], ch);
        if (tmpBuffer) {
            *outch = tmpBuffer; return 1;
        }
    }
    *outch = ch;
    return 1;
}

static unsigned __CFConvertToUpperCase(UniChar ch, UniChar *outch) {
    UniChar tmpBuffer;
    if (CFUniCharIsMemberOf(ch, kCFUniCharHasNonSelfUppercaseMapping)) {
        if (! __CFToUpperTable) __CFLoadCaseConversionTables();
        tmpBuffer = _CF16To16BitEncoding(__CFToUpperTable, __CFCaseTableStart[2], ch);
        if (tmpBuffer) {
            *outch = tmpBuffer; return 1;
        }
    }
    *outch = ch;
    return 1;
}

static unsigned __CFConvertToTitleCase(UniChar ch, UniChar *outch) {
    UniChar tmpBuffer;
    if (CFUniCharIsMemberOf(ch, kCFUniCharHasNonSelfTitlecaseMapping)) {
        if (! __CFToTitleTable) __CFLoadCaseConversionTables();
        tmpBuffer = _CF16To16BitEncoding(__CFToTitleTable, __CFCaseTableStart[3], ch);
        if (tmpBuffer) {
            *outch = tmpBuffer; return 1;
        }
    }
    *outch = ch;
    return 1;
}

#if defined(__MACOS8__)
static unsigned _CFDecomposeCharacter(UniChar ch, UniChar *outbuf, UInt32 maxLength) { // ret num elems in output
}
#else
/* DECOMPOSITION FUNCTIONS.  Simple decomposition is a one-level lookup;
   that is, it finds the first decomposition for a character, if any.
   Simple decomposition suffices to remove "compatibility" characters.
   Full decomposition is a recursive process that breaks a character
   into its most atomic elements.  For each character looked up, the
   process continues until there are no further decompositions for any
   element.  If A->BC and B->DE, then simple decomposition yields A->BC
   while full decomposition yields A->DEC.  Each returns the NUMBER
   of elements that it put into its output buffer.
*/

#define MAXDECOMPLENGTH	16
#define DECOMPHEADERSIZE	4	/* number of shorts in header */

/* This gets the address minus the 4-short header. */
#define DECOMP_BLOCK_ADDR ((unsigned short *)((char *)_CFDecompBuffer + (DECOMPHEADERSIZE * sizeof(short))))

static unsigned short *_CFDecompBuffer = 0;
static int _CFTotalDecompElements = 0;
static Boolean _decompTableCouldNotBeLoaded = FALSE;

static void _CFReadDecompFile(void) {
    char cpath[MAXPATHLEN];
    void *bytes;

    if (_decompTableCouldNotBeLoaded) return;
    __characterSetPath(cpath);
    strcat(cpath, "NonHangulDecomp.data");

    if (!__loadBytesFromFile(cpath, &bytes)) {
        _decompTableCouldNotBeLoaded = TRUE;
        return;
    }
    _CFDecompBuffer = (unsigned short *)bytes;
    _CFTotalDecompElements = CFSwapInt16LittleToHost(_CFDecompBuffer[2]);
}

/* This looks up the OFFSET of a decomposition string in the table and returns it, relative to the beginning of the table.  If there is no decomposition for the given character, it returns (-1).
*/
CF_INLINE unsigned int findDecomposition(UniChar ch) {
    UniChar *block = DECOMP_BLOCK_ADDR;
    int result, pee, que, divider;

    /* p & q are in terms of num elements; each element is 2 shorts long, so jab at it with "block[index * 2]" to get the correct number. */
    pee = 0; que = (_CFTotalDecompElements - 1);
    while (pee <= que) {
        divider = (pee + que) >> 1;
        result = ch - CFSwapInt16LittleToHost(block[divider*2]);
        if (result < 0) { que = divider - 1; }
        else if (result > 0) { pee = divider + 1; }
        else { return CFSwapInt16LittleToHost(block[(divider*2)+1]); }
    }
    return (unsigned)-1;
}

#define HANGUL_SBASE 0xAC00
#define HANGUL_LBASE 0x1100
#define HANGUL_VBASE 0x1161
#define HANGUL_TBASE 0x11A7
#define HANGUL_SCOUNT 11172
#define HANGUL_LCOUNT 19
#define HANGUL_VCOUNT 21
#define HANGUL_TCOUNT 28
#define HANGUL_NCOUNT (HANGUL_VCOUNT * HANGUL_TCOUNT)

/* This does a single-level character lookup in the table and puts what it finds there into the given output buffer.  It is not recursive.
*/
CF_INLINE unsigned _CFSimplyDecomposeCharacter(UniChar ch, UniChar *outbuf) {
    UniChar *str;
    unsigned int len;
    int offset;

    if ((offset = findDecomposition(ch)) < 0) {
        *outbuf = ch;
        return 1;
    }
    str = &(_CFDecompBuffer[offset]);
    len = 0;
    while (str && *str) {
        outbuf[len] = CFSwapInt16LittleToHost(*str++);
        ++len;
    }
    return len;
}

/* This is the main entry point for character decomposition.  It can be called numerous times to decompose characters into a buffer.  This function works recursively to decompose the character into its most atomic components.
*/
static unsigned _CFDecomposeCharacter(UniChar ch, UniChar *outbuf, UInt32 maxLength) { // ret num elems in output

    UniChar *decompstr, nextch;
    int tmp, offset;
    unsigned int numelements;

    if (! _CFDecompBuffer) {
        _CFReadDecompFile();
        if (! _CFDecompBuffer) {
            *outbuf = ch;
            return 1;
        }
    }
    if (!CFUniCharIsMemberOf(ch, kCFUniCharDecomposableCharacterSet)) { // if has no decomp
        *outbuf = ch;
        return 1;
    } else if (ch >= HANGUL_SBASE && ch <= (HANGUL_SBASE + HANGUL_SCOUNT)) {
        ch -= HANGUL_SBASE;

        *outbuf++ = ch / HANGUL_NCOUNT + HANGUL_LBASE;
        *outbuf++ = (ch % HANGUL_NCOUNT) / HANGUL_TCOUNT + HANGUL_VBASE;
        if ((*outbuf = (ch % HANGUL_TCOUNT) + HANGUL_TBASE) == HANGUL_TBASE) {
            return 2;
        } else {
            return 3;
        }
    } else {
        if ((offset = findDecomposition(ch)) < 0) {
            return 0;
        }
        numelements = 0;
        decompstr = &(_CFDecompBuffer[offset]);
        while (1) {
            nextch = CFSwapInt16LittleToHost(*decompstr++);
            if (! nextch) break;
            tmp = _CFSimplyDecomposeCharacter(nextch, outbuf);
            outbuf += tmp;
            numelements += tmp;
        }
        return numelements;
    }
}
#endif
