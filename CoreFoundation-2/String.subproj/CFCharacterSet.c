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
/*	CFCharacterSet.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Aki Inoue
*/

#include <CoreFoundation/CFCharacterSet.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include "CFInternal.h"
#include "CFUniChar.h"
#include "CFUniCharPriv.h"
#include <stdlib.h>

#define BITSPERBYTE	8	/* (CHAR_BIT * sizeof(unsigned char)) */
#define LOG_BPB		3
#define NUMCHARACTERS	65536

/* Number of things in the array keeping the bits.
*/
#define __kCFBitmapSize		(NUMCHARACTERS / BITSPERBYTE)

/* How many elements max can be in an __kCFCharSetClassString CFCharacterSet
*/
#define __kCFStringCharSetMax 64

/* The last builtin set ID number
*/
#define __kCFLastBuiltinSetID kCFCharacterSetIllegal

/* How many elements in the "singles" array before we use binary search.
*/
#define __kCFSetBreakeven 10

/* This tells us, within 1k or so, whether a thing is POTENTIALLY in the set (in the bitmap blob of the private structure) before we bother to do specific checking.
*/
#define __CFCSetBitsInRange(n, i)	(i[n>>15] & (1L << ((n>>10) % 32)))

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

struct __CFCharacterSet {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFHashCode _hashValue;
    union {
        struct {
            CFIndex _type;
        } _builtin;
        struct {
            UniChar _firstChar;
            CFIndex _length;
        } _range;
        struct {
            UniChar *_buffer;
            CFIndex _length;
        } _string;
        struct {
            UInt8 *_bits;
        } _bitmap;
        CFCharSetPrivateStruct *_set;
   } _variants;
};

/* _base._info values interesting for CFCharacterSet
*/
enum {
    __kCFCharSetClassTypeMask = 0x0070,
      __kCFCharSetClassBuiltin = 0x0000,
      __kCFCharSetClassRange = 0x0010,
      __kCFCharSetClassString = 0x0020,
      __kCFCharSetClassBitmap = 0x0030,
      __kCFCharSetClassSet = 0x0040,

    __kCFCharSetIsInvertedMask = 0x0008,
      __kCFCharSetIsInverted = 0x0008,

    __kCFCharSetHasHashValueMask = 0x00004,
      __kCFCharSetHasHashValue = 0x0004,

    /* Generic CFBase values */
    __kCFIsMutableMask = 0x0001,
      __kCFIsMutable = 0x0001,
};

/* Inline accessor macros for _base._info
*/
CF_INLINE Boolean __CFCSetIsMutable(CFCharacterSetRef cset) {return (cset->_base._info & __kCFIsMutableMask) == __kCFIsMutable;}
CF_INLINE Boolean __CFCSetIsBuiltin(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetClassTypeMask) == __kCFCharSetClassBuiltin;}
CF_INLINE Boolean __CFCSetIsRange(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetClassTypeMask) == __kCFCharSetClassRange;}
CF_INLINE Boolean __CFCSetIsString(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetClassTypeMask) == __kCFCharSetClassString;}
CF_INLINE Boolean __CFCSetIsBitmap(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetClassTypeMask) == __kCFCharSetClassBitmap;}
CF_INLINE Boolean __CFCSetIsSet(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetClassTypeMask) == __kCFCharSetClassSet;}
CF_INLINE Boolean __CFCSetIsInverted(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetIsInvertedMask) == __kCFCharSetIsInverted;}
CF_INLINE Boolean __CFCSetHasHashValue(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetHasHashValueMask) == __kCFCharSetHasHashValue;}
CF_INLINE UInt32 __CFCSetClassType(CFCharacterSetRef cset) {return (cset->_base._info & __kCFCharSetClassTypeMask);}

CF_INLINE void __CFCSetPutIsMutable(CFMutableCharacterSetRef cset, Boolean isMutable) {(isMutable ? (cset->_base._info |= __kCFIsMutable) : (cset->_base._info &= ~__kCFIsMutable));}
CF_INLINE void __CFCSetPutIsInverted(CFMutableCharacterSetRef cset, Boolean isInverted) {(isInverted ? (cset->_base._info |= __kCFCharSetIsInverted) : (cset->_base._info &= ~__kCFCharSetIsInverted));}
CF_INLINE void __CFCSetPutHasHashValue(CFMutableCharacterSetRef cset, Boolean hasHash) {(hasHash ? (cset->_base._info |= __kCFCharSetHasHashValue) : (cset->_base._info &= ~__kCFCharSetHasHashValue));}
CF_INLINE void __CFCSetPutClassType(CFMutableCharacterSetRef cset, UInt32 classType) {cset->_base._info &= ~__kCFCharSetClassTypeMask;  cset->_base._info |= classType;}


/* Inline contents accessor macros
*/
CF_INLINE CFCharacterSetPredefinedSet __CFCSetBuiltinType(CFCharacterSetRef cset) {return cset->_variants._builtin._type;}
CF_INLINE UniChar __CFCSetRangeFirstChar(CFCharacterSetRef cset) {return cset->_variants._range._firstChar;}
CF_INLINE CFIndex __CFCSetRangeLength(CFCharacterSetRef cset) {return cset->_variants._range._length;}
CF_INLINE UniChar *__CFCSetStringBuffer(CFCharacterSetRef cset) {return (UniChar*)(cset->_variants._string._buffer);}
CF_INLINE CFIndex __CFCSetStringLength(CFCharacterSetRef cset) {return cset->_variants._string._length;}
CF_INLINE CFCharSetPrivateStruct *__CFCSetBitmapSet(CFCharacterSetRef cset) {return cset->_variants._set;}
CF_INLINE UInt8 *__CFCSetBitmapBits(CFCharacterSetRef cset) {return cset->_variants._bitmap._bits;}

CF_INLINE void __CFCSetPutBuiltinType(CFMutableCharacterSetRef cset, CFCharacterSetPredefinedSet type) {cset->_variants._builtin._type = type;}
CF_INLINE void __CFCSetPutRangeFirstChar(CFMutableCharacterSetRef cset, UniChar first) {cset->_variants._range._firstChar = first;}
CF_INLINE void __CFCSetPutRangeLength(CFMutableCharacterSetRef cset, CFIndex length) {cset->_variants._range._length = length;}
CF_INLINE void __CFCSetPutStringBuffer(CFMutableCharacterSetRef cset, UniChar *theBuffer) {cset->_variants._string._buffer = theBuffer;}
CF_INLINE void __CFCSetPutStringLength(CFMutableCharacterSetRef cset, CFIndex length) {cset->_variants._string._length = length;}
CF_INLINE void __CFCSetPutBitmapSet(CFMutableCharacterSetRef cset, CFCharSetPrivateStruct *set) {cset->_variants._set = set;}
CF_INLINE void __CFCSetPutBitmapBits(CFMutableCharacterSetRef cset, UInt8 *bits) {cset->_variants._bitmap._bits = bits;}

/* Validation funcs
*/
#if defined(CF_ENABLE_ASSERTIONS)
CF_INLINE void __CFCSetValidateBuiltinType(CFCharacterSetPredefinedSet type, const char *func) {
    CFAssert2(type > 0 && type <= __kCFLastBuiltinSetID, __kCFLogAssertion, "%s: Unknowen builtin type %d", func, type);
}
CF_INLINE void __CFCSetValidateRange(CFRange theRange, const char *func) {
    CFAssert3(theRange.location >= 0 && theRange.location + theRange.length <= 0xFFFF, __kCFLogAssertion, "%s: Range out of Unicode range (location -> %d length -> %d)", func, theRange.location, theRange.length);
}
CF_INLINE void __CFCSetValidateTypeAndMutability(CFCharacterSetRef cset, const char *func) {
    __CFGenericValidateType(cset, __kCFCharacterSetTypeID);
    CFAssert1(__CFCSetIsMutable(cset), __kCFLogAssertion, "%s: Immutable character set passed to mutable function", func);
}
#else
#define __CFCSetValidateBuiltinType(t,f)
#define __CFCSetValidateRange(r,f)
#define __CFCSetValidateTypeAndMutability(r,f)
#endif

/* Inline utility funcs
*/
CF_INLINE Boolean __CFCSetIsEqualBitmap(const UInt32 *bits1, const UInt32 *bits2) {
    CFIndex length = __kCFBitmapSize / sizeof(UInt32);

    if (bits1 == bits2) {
        return TRUE;
    } else if (bits1 && bits2) {
        if (bits1 == (const UInt32 *)-1) {
            while (length--) if ((UInt32)-1 != *bits2++) return FALSE;
        } else if (bits2 == (const UInt32 *)-1) {
            while (length--) if ((UInt32)-1 != *bits1++) return FALSE;
        } else {
            while (length--) if (*bits1++ != *bits2++) return FALSE;
        }
        return TRUE;
    } else if (!bits1 && !bits2) { // empty set
        return TRUE;
    } else {
        if (bits2) bits1 = bits2;
        if (bits1 == (const UInt32 *)-1) return FALSE;
        while (length--) if (*bits1++) return FALSE;
        return TRUE;
    }
}

CF_INLINE Boolean __CFCSetIsEmpty(CFCharacterSetRef cset) {
    switch (__CFCSetClassType(cset)) {
        case __kCFCharSetClassRange: if (!__CFCSetRangeLength(cset)) return TRUE; break;
        case __kCFCharSetClassString: if (!__CFCSetStringLength(cset)) return TRUE; break;
        case __kCFCharSetClassSet: if (!__CFCSetBitmapSet(cset)) return TRUE;
        case __kCFCharSetClassBitmap: if (!__CFCSetBitmapBits(cset)) return TRUE;
    }
    return FALSE;
}

CF_INLINE void __CFCSetBitmapAddCharacer(UInt8 *bitmap, UniChar theChar) {
    bitmap[(theChar) >> LOG_BPB] |= (((unsigned)1) << (theChar & (BITSPERBYTE - 1)));
}

CF_INLINE void __CFCSetBitmapRemoveCharacter(UInt8 *bitmap, UniChar theChar) {
    bitmap[(theChar) >> LOG_BPB] &= ~(((unsigned)1) << (theChar & (BITSPERBYTE - 1)));
}

CF_INLINE Boolean __CFCSetIsMemberBitmap(const UInt8 *bitmap, UniChar theChar) {
    return ((bitmap[(theChar) >> LOG_BPB] & (((unsigned)1) << (theChar & (BITSPERBYTE - 1)))) ? TRUE : FALSE);
}

/* Take a private "set" structure and make a bitmap from it.  Return the bitmap.  THE CALLER MUST RELEASE THE RETURNED MEMORY as necessary.
*/

CF_INLINE void __CFCSetBitmapProcessManyCharacters(unsigned char *map, unsigned n, unsigned m, Boolean isInverted) {
    unsigned tmp;
    for (tmp = n; tmp <= m; tmp++) (isInverted ? __CFCSetBitmapRemoveCharacter(map, tmp) : __CFCSetBitmapAddCharacer(map, tmp));
}

CF_INLINE void __CFCSetMakeSetBitmapFromSet(CFCharSetPrivateStruct *theSet, UInt8 *map, Boolean isInverted)
{
    int *ip;
    UniChar ctmp;
    int cnt;

    for (cnt = 0; cnt < theSet->nsingles; cnt++) {
        ctmp = theSet->array[cnt];
        (isInverted ? __CFCSetBitmapRemoveCharacter(map, ctmp) :  __CFCSetBitmapAddCharacer(map, ctmp));
    }
    ip = (int *) (&(theSet->array[0]) + theSet->nsingles);
    cnt = theSet->nranges;
    while (cnt) {
        /* This could be more efficient: turn on whole bytes at a time
           when there are such cases as 8 characters in a row... */
        __CFCSetBitmapProcessManyCharacters((unsigned char *)map, *ip, *(ip+1), isInverted);
        ip += 2;
        --cnt;
    }
}

static void __CFCSetGetBitmap(CFCharacterSetRef cset, UInt8 *bits) {
    UInt8 *bitmap;
    CFIndex length = __kCFBitmapSize;

    if (__CFCSetIsBitmap(cset) && (bitmap = __CFCSetBitmapBits(cset))) {
        CFMoveMemory(bits, bitmap, __kCFBitmapSize);
    } else {
        Boolean isInverted = __CFCSetIsInverted(cset);
        UInt8 value = (isInverted ? (UInt8)-1 : 0);

        bitmap = bits;
        while (length--) *bitmap++ = value; // Initialize the buffer

        if (!__CFCSetIsEmpty(cset)) {
            switch (__CFCSetClassType(cset)) {
                case __kCFCharSetClassBuiltin:
                    _UniCharGetBitmap(bits, __CFCSetBuiltinType(cset), isInverted);
                    break;

                case __kCFCharSetClassRange: {
                    UniChar theChar = __CFCSetRangeFirstChar(cset);
                    length = __CFCSetRangeLength(cset);
                    while (length--) (isInverted ? __CFCSetBitmapRemoveCharacter(bits, theChar++) : __CFCSetBitmapAddCharacer(bits, theChar++));
                }
                    break;

                case __kCFCharSetClassString: {
                    const UniChar *buffer = __CFCSetStringBuffer(cset);
                    length = __CFCSetStringLength(cset);
                    while (length--) (isInverted ? __CFCSetBitmapRemoveCharacter(bits, *buffer++) : __CFCSetBitmapAddCharacer(bits, *buffer++));
                }
                    break;

                case __kCFCharSetClassSet:
                    __CFCSetMakeSetBitmapFromSet(__CFCSetBitmapSet(cset), bits, isInverted);
                    break;
            }
        }
    }
}

/* Turn a number in range 0-131072 (2^17) into a bit in range 0-31 in one of
* four ints of the array passed as "i".  This is identical to the routine
* in the rulebook compiler.
*/

static const unsigned long bitmask[32] = {
       0x00000001, 	0x00000002, 	0x00000004, 	0x00000008,
       0x00000010, 	0x00000020, 	0x00000040, 	0x00000080,
       0x00000100, 	0x00000200, 	0x00000400, 	0x00000800,
       0x00001000, 	0x00002000, 	0x00004000, 	0x00008000,
       0x00010000, 	0x00020000, 	0x00040000, 	0x00080000,
       0x00100000, 	0x00200000, 	0x00400000, 	0x00800000,
       0x01000000, 	0x02000000, 	0x04000000, 	0x08000000,
       0x10000000, 	0x20000000, 	0x40000000, 	0x80000000 };

CF_INLINE void bitmapset(int *i, unsigned long n) {
   i[n>>15] |= bitmask[(n&0x7FFF)>>10];	// 7FFF: mask off the 2 high bits!
}

CF_INLINE void bitmapsetall(int *i, unsigned long n, unsigned long m) {
   int tmp;
   for (tmp = n; tmp <= m; tmp++) {
       i[tmp>>15] |= bitmask[(tmp&0x7FFF)>>10];
                                   // 7FFF: mask off the 2 high bits!
   }
}

CF_INLINE void __CFCSetMakeCompact(CFMutableCharacterSetRef cset) {
    if (__CFCSetIsBitmap(cset) && __CFCSetBitmapBits(cset)) {
        int i, low, hi, sz;
        int nr, ns;	/* num ranges */
        int *ranges, *singles;	/* ranges & singles pointers */
        CFCharSetPrivateStruct *myset;	/* the set itself, in rulebook format */
        UInt8 *map = __CFCSetBitmapBits(cset);

        nr = ns = 0;
        low = hi = i = 0;
        /*
         * Figure out how many singles & ranges we have.
         */
        while (i < NUMCHARACTERS) {
            if (__CFCSetIsMemberBitmap(map, (UniChar) i)) {
                hi = low = i;
                while (__CFCSetIsMemberBitmap(map, (UniChar) i) && i < NUMCHARACTERS) {
                    if (i) hi = i;
                    ++i;
                }
                if (low == hi) ++ns;
                else ++nr;
            }
            ++i;
        }

        sz = sizeof(CFCharSetPrivateStruct) + (ns * sizeof(int)) + (nr * 2 * sizeof(int));
        if (sz > __kCFBitmapSize) return; // It's not compact, keep it as is.
        myset = (CFCharSetPrivateStruct *)CFAllocatorAllocate(CFGetAllocator(cset), sz, 0);

        for (i = 0; i < 4; i++) myset->bitrange[i] = 0;
        ranges = (int *) &(myset->array[ns]);
        singles = (int *) &(myset->array[0]);
        myset->singmin = 0x1FFFF;
        myset->singmax = 0;

        low = NUMCHARACTERS; hi = i = 0;
        while (i < NUMCHARACTERS) {
            if (__CFCSetIsMemberBitmap(map, (UniChar) i)) {
                hi = low = i;
                while (__CFCSetIsMemberBitmap(map, (UniChar) i) && i < NUMCHARACTERS) {
                    if (i) hi = i;
                    ++i;
                }
                if (low == hi) {	/* a single */
                    if (low < myset->singmin) myset->singmin = low;
                    if (low > myset->singmax) myset->singmax = low;
                    *singles++ = low;
                    bitmapset(&(myset->bitrange[0]), low);
                }
                else {				/* a range */
                    *ranges++ = low;
                    *ranges++ = hi;
                    bitmapsetall(&(myset->bitrange[0]), low, hi);
                }
            }
            ++i;
        }
        myset->issorted = 1;
        myset->nsingles = ns;
        myset->nranges = nr;
        CFAllocatorDeallocate(CFGetAllocator(cset), map);
        __CFCSetPutClassType(cset, __kCFCharSetClassSet);
        __CFCSetPutBitmapSet(cset, myset);
    }
}

CF_INLINE void __CFCSetMakeBitmap(CFMutableCharacterSetRef cset) {
    if (!__CFCSetIsBitmap(cset) || !__CFCSetBitmapBits(cset)) {
        UInt8 *bitmap = CFAllocatorAllocate(CFGetAllocator(cset), __kCFBitmapSize, 0);
        __CFCSetGetBitmap(cset, bitmap);
        if (__CFCSetIsSet(cset) && __CFCSetBitmapSet(cset)) {
            CFAllocatorDeallocate(CFGetAllocator(cset), __CFCSetBitmapSet(cset));
            __CFCSetPutBitmapSet(cset, NULL);
        } else if (__CFCSetIsString(cset) && __CFCSetStringBuffer(cset)) {
            CFAllocatorDeallocate(CFGetAllocator(cset), __CFCSetStringBuffer(cset));
            __CFCSetPutStringBuffer(cset, NULL);
        }
        __CFCSetPutClassType(cset, __kCFCharSetClassBitmap);
        __CFCSetPutBitmapBits(cset, bitmap);
        __CFCSetPutIsInverted(cset, FALSE);
    }    
}

CF_INLINE CFMutableCharacterSetRef __CFCSetGenericCreate(CFAllocatorRef allocator, UInt32 flags) {
    CFMutableCharacterSetRef cset;
    CFIndex size = sizeof(struct __CFCharacterSet);

    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    cset = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == cset) {
        CFRelease(allocator);
        return NULL;
    }
    __CFGenericInitBase(cset, __CFCharacterSetIsa(), __kCFCharacterSetTypeID);
    cset->_base._info |= flags;
    cset->_allocator = allocator;
    cset->_hashValue = 0;
    return cset;
}

/* Bsearch theChar for __kCFCharSetClassString
*/
CF_INLINE Boolean __CFCSetBsearchUniChar(const UniChar *theTable, CFIndex length, UniChar theChar) {
    const UniChar *p, *q, *divider;

    if ((theChar < theTable[0]) || (theChar > theTable[length - 1])) return FALSE;

    p = theTable;
    q = p + (length - 1);
    while (p <= q) {
        divider = p + ((q - p) >> 1);	/* divide by 2 */
        if (theChar < *divider) q = divider - 1;
        else if (theChar > *divider) p = divider + 1;
        else return TRUE;
    }
    return FALSE;
}

/* Membership function for complex sets
*/
CF_INLINE Boolean __CFCSetIsMemberSet(CFCharSetPrivateStruct *set, UniChar theChar) {
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

/* Predefined cset names
 Need to add entry here for new builtin types
*/
CF_DEFINECONSTANTSTRING(__kCFCSetNameControl, "<CFCharacterSet Predefined Control Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameWhitespace, "<CFCharacterSet Predefined Whitespace Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameWhitespaceAndNewline, "<CFCharacterSet Predefined WhitespaceAndNewline Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameDecimalDigit, "<CFCharacterSet Predefined DecimalDigit Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameLetter, "<CFCharacterSet Predefined Letter Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameLowercaseLetter, "<CFCharacterSet Predefined LowercaseLetter Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameUppercaseLetter, "<CFCharacterSet Predefined UppercaseLetter Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameNonBase, "<CFCharacterSet Predefined NonBase Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameDecomposable, "<CFCharacterSet Predefined Decomposable Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameAlphaNumeric, "<CFCharacterSet Predefined AlphaNumeric Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNamePunctuation, "<CFCharacterSet Predefined Punctuation Set>")
CF_DEFINECONSTANTSTRING(__kCFCSetNameIllegal, "<CFCharacterSet Predefined Illegal Set>")

CF_DEFINECONSTANTSTRING(__kCFCSetNameStringTypeFormat, "<CFCharacterSet Items(")

/* Array of instantiated builtin set. Note builtin set ID starts with 1 so the array index is ID - 1
*/
static CFCharacterSetRef *__CFBuiltinSets = NULL;

/* Global lock for character set
*/
static UInt32 __CFCharacterSetLock = 0;

/* CFBase API functions
*/
Boolean __CFCharacterSetEqual(CFTypeRef cf1, CFTypeRef cf2) {
    Boolean isInvertStateIdentical = (__CFCSetIsInverted(cf1) == __CFCSetIsInverted(cf2) ? TRUE: FALSE);

    if (__CFCSetHasHashValue(cf1) && __CFCSetHasHashValue(cf2) && ((CFCharacterSetRef)cf1)->_hashValue != ((CFCharacterSetRef)cf2)->_hashValue) return FALSE;
    if (__CFCSetIsEmpty(cf1) && __CFCSetIsEmpty(cf2) && isInvertStateIdentical) return TRUE;

    if (__CFCSetClassType(cf1) == __CFCSetClassType(cf2)) { // Types are identical, we can do it fast
        switch (__CFCSetClassType(cf1)) {
            case __kCFCharSetClassBuiltin:
                return (__CFCSetBuiltinType(cf1) == __CFCSetBuiltinType(cf2) && isInvertStateIdentical ? TRUE : FALSE);

            case __kCFCharSetClassRange:
                return (__CFCSetRangeFirstChar(cf1) == __CFCSetRangeFirstChar(cf2) && __CFCSetRangeLength(cf1) && __CFCSetRangeLength(cf2) && isInvertStateIdentical ? TRUE : FALSE);

            case __kCFCharSetClassString:
                if (__CFCSetStringLength(cf1) == __CFCSetStringLength(cf2) && isInvertStateIdentical) {
                    const UniChar *buf1 = __CFCSetStringBuffer(cf1);
                    const UniChar *buf2 = __CFCSetStringBuffer(cf2);
                    CFIndex length = __CFCSetStringLength(cf1);

                    while (length--) if (*buf1++ != *buf2++) return FALSE;
                    return TRUE;
                }
                return FALSE;

            case __kCFCharSetClassBitmap:
                return __CFCSetIsEqualBitmap((const UInt32 *)__CFCSetBitmapBits(cf1), (const UInt32 *)__CFCSetBitmapBits(cf2));
        }
    }

    if (__CFCSetIsBitmap(cf1)) {
        if (__CFCSetIsEmpty(cf2)) {
            return __CFCSetIsEqualBitmap((const UInt32 *)__CFCSetBitmapBits(cf1), (__CFCSetIsInverted(cf2) ? (const UInt32*)-1 : NULL));
        } else {
            UInt8 bitsBuf[__kCFBitmapSize];
            __CFCSetGetBitmap(cf2, bitsBuf);
            return __CFCSetIsEqualBitmap((const UInt32 *)__CFCSetBitmapBits(cf1), (const UInt32 *)bitsBuf);
        }
    } else if (__CFCSetIsEmpty(cf1)) {
        if (__CFCSetIsBitmap(cf2)) {
            return __CFCSetIsEqualBitmap((__CFCSetIsInverted(cf1) ? (const UInt32*)-1 : NULL), (const UInt32 *)__CFCSetBitmapBits(cf2));
        } else {
            UInt8 bitsBuf2[__kCFBitmapSize];
            __CFCSetGetBitmap(cf2, bitsBuf2);
            return __CFCSetIsEqualBitmap((__CFCSetIsInverted(cf1) ? (const UInt32*)-1 : NULL), (const UInt32 *)bitsBuf2);
        }
    } else {
        UInt8 bitsBuf1[__kCFBitmapSize];
        __CFCSetGetBitmap(cf1, bitsBuf1);

        if (__CFCSetIsEmpty(cf2)) {
            return __CFCSetIsEqualBitmap((const UInt32 *)bitsBuf1, (__CFCSetIsInverted(cf2) ? (const UInt32*)-1 : NULL));
        } else if (__CFCSetIsBitmap(cf2)) {
            return __CFCSetIsEqualBitmap((const UInt32 *)bitsBuf1, (const UInt32 *)__CFCSetBitmapBits(cf2));
        } else {
#if defined(__MACOS8__)
            // Can't use stack buffer because of 32K barrier, arggh
            UInt8 *bits2 = CFAllocatorAllocate(CFGetAllocator(cf2), __kCFBitmapSize, 0);
            Boolean result;
            __CFCSetGetBitmap(cf2, bits2);
            result = __CFCSetIsEqualBitmap((const UInt32 *)bitsBuf1, (const UInt32 *)bits2);
            CFAllocatorDeallocate(CFGetAllocator(cf2), bits2);
            return result;
#else
            UInt8 bitsBuf2[__kCFBitmapSize];
            __CFCSetGetBitmap(cf2, bitsBuf2);
            return __CFCSetIsEqualBitmap((const UInt32 *)bitsBuf1, (const UInt32 *)bitsBuf2);
#endif /* __MACOS8__ */
        }
    }
}

CFHashCode __CFCharacterSetHash(CFTypeRef cf) {
    if (!__CFCSetHasHashValue(cf)) {
        if (__CFCSetIsEmpty(cf)) {
            ((CFMutableCharacterSetRef)cf)->_hashValue = (__CFCSetIsInverted(cf) ? 0xFFFFFFFF : 0);
        } else if (__CFCSetIsBitmap(cf)) {
            ((CFMutableCharacterSetRef)cf)->_hashValue = CFHashBytes(__CFCSetBitmapBits(cf), __kCFBitmapSize);
        } else {
            UInt8 bitsBuf[__kCFBitmapSize];
            __CFCSetGetBitmap(cf, bitsBuf);
            ((CFMutableCharacterSetRef)cf)->_hashValue = CFHashBytes(bitsBuf, __kCFBitmapSize);
        }
        __CFCSetPutHasHashValue((CFMutableCharacterSetRef)cf, TRUE);
    }
    return ((CFCharacterSetRef)cf)->_hashValue;
}

CFStringRef  __CFCharacterSetCopyDescription(CFTypeRef cf) {
    CFMutableStringRef string;
    CFIndex index;
    CFIndex length;

    if (__CFCSetIsEmpty(cf)) {
	return (__CFCSetIsInverted(cf) ? CFRetain(CFSTR("<CFCharacterSet All>")) : CFRetain(CFSTR("<CFCharacterSet Empty>")));
    }

    switch (__CFCSetClassType(cf)) {
        case __kCFCharSetClassBuiltin:
            switch (__CFCSetBuiltinType(cf)) {
                case kCFCharacterSetControl: return CFRetain(__kCFCSetNameControl);
                case kCFCharacterSetWhitespace : return CFRetain(__kCFCSetNameWhitespace);
                case kCFCharacterSetWhitespaceAndNewline: return CFRetain(__kCFCSetNameWhitespaceAndNewline);
                case kCFCharacterSetDecimalDigit: return CFRetain(__kCFCSetNameDecimalDigit);
                case kCFCharacterSetLetter: return CFRetain(__kCFCSetNameLetter);
                case kCFCharacterSetLowercaseLetter: return CFRetain(__kCFCSetNameLowercaseLetter);
                case kCFCharacterSetUppercaseLetter: return CFRetain(__kCFCSetNameUppercaseLetter);
                case kCFCharacterSetNonBase: return CFRetain(__kCFCSetNameNonBase);
                case kCFCharacterSetDecomposable: return CFRetain(__kCFCSetNameDecomposable);
                case kCFCharacterSetAlphaNumeric: return CFRetain(__kCFCSetNameAlphaNumeric);
                case kCFCharacterSetPunctuation: return CFRetain(__kCFCSetNamePunctuation);
                case kCFCharacterSetIllegal: return CFRetain(__kCFCSetNameIllegal);
            }
            break;

        case __kCFCharSetClassRange:
            return CFStringCreateWithFormat(CFGetAllocator(cf), NULL, CFSTR("<CFCharacterSet Range(%d, %d)>"), __CFCSetRangeFirstChar(cf), __CFCSetRangeLength(cf));

        case __kCFCharSetClassString:
            length = __CFCSetStringLength(cf);
            string = CFStringCreateMutable(CFGetAllocator(cf), CFStringGetLength(__kCFCSetNameStringTypeFormat) + 7 * length + 2); // length of__kCFCSetNameStringTypeFormat + "U+XXXX "(7) * length + ")>"(2)
            CFStringAppend(string, __kCFCSetNameStringTypeFormat);
            for (index = 0;index < length;index++) {
                CFStringAppendFormat(string, NULL, CFSTR("%sU+%04X"), (index > 0 ? " " : ""), (UInt32)((__CFCSetStringBuffer(cf))[index]));
            }
            CFStringAppend(string, CFSTR(")>"));
            return string;

        case __kCFCharSetClassBitmap:
        case __kCFCharSetClassSet:
            return CFRetain(CFSTR("<CFCharacterSet Bitmap>")); // ??? Should generate description for 8k bitmap ?
    }
    CFAssert1(0, __kCFLogAssertion, "%s: Internal inconsistency error: unknown character set type", __PRETTY_FUNCTION__); // We should never come here
    return NULL;
}

CFAllocatorRef __CFCharacterSetGetAllocator(CFTypeRef cf) {
    return ((CFCharacterSetRef)cf)->_allocator;
}

void __CFCharacterSetDeallocate(CFTypeRef cf) {
    CFAllocatorRef allocator = CFGetAllocator(cf);

    if (__CFCSetIsBuiltin(cf) && !__CFCSetIsMutable(cf)) {
        CFAssert1(0, __kCFLogAssertion, "%s: Trying to deallocate predefined set", __PRETTY_FUNCTION__);
        return; // We never deallocate builtin set
    }

    if (__CFCSetIsString(cf) && __CFCSetStringBuffer(cf)) CFAllocatorDeallocate(allocator, __CFCSetStringBuffer(cf));
    else if (__CFCSetIsBitmap(cf) && __CFCSetBitmapBits(cf)) CFAllocatorDeallocate(allocator, __CFCSetBitmapBits(cf));
    else if (__CFCSetIsSet(cf) && __CFCSetBitmapSet(cf)) CFAllocatorDeallocate(allocator, __CFCSetBitmapSet(cf));
    CFAllocatorDeallocate(allocator, (void*)cf);
    CFRelease(allocator);
}

/* Public functions
*/
#if defined(__MACOS8__)
CFTypeID CFCharacterSetTypeID(void) {
#ifdef DEBUG
    CFLog(__kCFLogAssertion, CFSTR("CFCharacterSetTypeID should be CFCharacterSetGetTypeID"));
#endif /* DEBUG */
    return __kCFCharacterSetTypeID;
}
#endif /* __MACOS8__ */

CFTypeID CFCharacterSetGetTypeID(void) {
    return __kCFCharacterSetTypeID;
}

/*** CharacterSet creation ***/
/* Functions to create basic immutable characterset.
*/
CFCharacterSetRef CFCharacterSetGetPredefined(CFCharacterSetPredefinedSet theSetIdentifier) {
    CFMutableCharacterSetRef cset;

    __CFCSetValidateBuiltinType(theSetIdentifier, __PRETTY_FUNCTION__);

    if (__CFBuiltinSets && __CFBuiltinSets[theSetIdentifier - 1]) return __CFBuiltinSets[theSetIdentifier - 1];

    if (!(cset = __CFCSetGenericCreate(kCFAllocatorSystemDefault, __kCFCharSetClassBuiltin))) return NULL;
    __CFCSetPutBuiltinType(cset, theSetIdentifier);

    if (!__CFBuiltinSets) {
        __CFSpinLock(&__CFCharacterSetLock);
	__CFBuiltinSets = (CFCharacterSetRef *)CFAllocatorAllocate(CFRetain(CFAllocatorGetDefault()), sizeof(CFCharacterSetRef) * __kCFLastBuiltinSetID, 0);
	CFZeroMemory(__CFBuiltinSets, sizeof(CFCharacterSetRef) * __kCFLastBuiltinSetID);
        __CFSpinUnlock(&__CFCharacterSetLock);
    }

    __CFBuiltinSets[theSetIdentifier - 1] = cset;

    return cset;
}

CFCharacterSetRef CFCharacterSetCreateWithCharactersInRange(CFAllocatorRef allocator, CFRange theRange) {
    CFMutableCharacterSetRef cset;

    __CFCSetValidateRange(theRange, __PRETTY_FUNCTION__);

    if (!(cset = __CFCSetGenericCreate(allocator, __kCFCharSetClassRange))) return NULL;
    __CFCSetPutRangeFirstChar(cset, theRange.location);
    __CFCSetPutRangeLength(cset, theRange.length);
    if (!theRange.length) __CFCSetPutHasHashValue(cset, TRUE); // _hashValue is 0

    return cset;
}

static int chcompar(const void *a, const void *b) {
    return -(int)(*(UniChar *)b - *(UniChar *)a);
}

CFCharacterSetRef CFCharacterSetCreateWithCharactersInString(CFAllocatorRef allocator, CFStringRef theString) {
    CFIndex length;

    length = CFStringGetLength(theString);
    if (length < __kCFStringCharSetMax) {
        CFMutableCharacterSetRef cset;

        if (!(cset = __CFCSetGenericCreate(allocator, __kCFCharSetClassString))) return NULL;
        __CFCSetPutStringBuffer(cset, CFAllocatorAllocate(CFGetAllocator(cset), __kCFStringCharSetMax, 0));
        __CFCSetPutStringLength(cset, length);
        CFStringGetCharacters(theString, CFRangeMake(0, length), __CFCSetStringBuffer(cset));
        qsort(__CFCSetStringBuffer(cset), length, sizeof(UniChar), chcompar);
        if (!length) __CFCSetPutHasHashValue(cset, TRUE); // _hashValue is 0
        return cset;
    } else {
        CFMutableCharacterSetRef mcset = CFCharacterSetCreateMutable(allocator);
        CFCharacterSetAddCharactersInString(mcset, theString);
        __CFCSetMakeCompact(mcset);
        __CFCSetPutIsMutable(mcset, FALSE);
        return mcset;
    }
}

#if defined(__MACOS8__)
CFCharacterSetRef CFCharacterSetCreateWithBitmapReresentation(CFAllocatorRef allocator, CFDataRef theData) {
#ifdef DEBUG
    CFLog(__kCFLogAssertion, CFSTR("CFCharacterSetCreateWithBitmapReresentation should be CFCharacterSetCreateWithBitmapRepresentation"));
#endif /* DEBUG */
    return CFCharacterSetCreateWithBitmapRepresentation(allocator, theData);
}
#endif /* __MACOS8__ */

CFCharacterSetRef CFCharacterSetCreateWithBitmapRepresentation(CFAllocatorRef allocator, CFDataRef theData) {
    CFMutableCharacterSetRef cset;
    CFIndex length;

    if (!(cset = __CFCSetGenericCreate(allocator, __kCFCharSetClassBitmap))) return NULL;

    if (theData && (length = CFDataGetLength(theData)) > 0) {
        UInt8 *bitmap =  (UInt8 *)CFAllocatorAllocate(CFGetAllocator(cset), __kCFBitmapSize, 0);

        CFMoveMemory(bitmap, CFDataGetBytePtr(theData), length);
        __CFCSetPutBitmapBits(cset, bitmap);
        bitmap += length;
        length = __kCFBitmapSize - length;
        while (length--) *bitmap++ = 0;
    } else {
        __CFCSetPutHasHashValue(cset, TRUE); // Hash value is 0
    }

    return cset;
}

/* Functions to create mutable characterset.
*/
CFMutableCharacterSetRef CFCharacterSetCreateMutable(CFAllocatorRef allocator) {
    CFMutableCharacterSetRef cset;

    if (!(cset = __CFCSetGenericCreate(allocator, __kCFCharSetClassBitmap|__kCFIsMutable))) return NULL;
    __CFCSetPutBitmapBits(cset, NULL);
    __CFCSetPutHasHashValue(cset, TRUE); // Hash value is 0

    return cset;
}

CFMutableCharacterSetRef CFCharacterSetCreateMutableCopy(CFAllocatorRef alloc, CFCharacterSetRef theSet) {
    CFMutableCharacterSetRef cset;

    CF_OBJC_FUNCDISPATCH0(CharacterSet, CFMutableCharacterSetRef , theSet, "mutableCopy");

    __CFGenericValidateType(theSet, __kCFCharacterSetTypeID);

    cset = CFCharacterSetCreateMutable(alloc);

    __CFCSetPutClassType(cset, __CFCSetClassType(theSet));
    __CFCSetPutHasHashValue(cset, __CFCSetHasHashValue(theSet));
    __CFCSetPutIsInverted(cset, __CFCSetIsInverted(theSet));
    cset->_hashValue = theSet->_hashValue;

    switch (__CFCSetClassType(theSet)) {
        case __kCFCharSetClassBuiltin:
            __CFCSetPutBuiltinType(cset, __CFCSetBuiltinType(theSet));
            break;

        case __kCFCharSetClassRange:
            __CFCSetPutRangeFirstChar(cset, __CFCSetRangeFirstChar(theSet));
            __CFCSetPutRangeLength(cset, __CFCSetRangeLength(theSet));
            break;

        case __kCFCharSetClassString:
            __CFCSetPutStringBuffer(cset, CFAllocatorAllocate(CFGetAllocator(cset), __kCFStringCharSetMax, 0));
            __CFCSetPutStringLength(cset, __CFCSetStringLength(theSet));
            CFMoveMemory(__CFCSetStringBuffer(cset), __CFCSetStringBuffer(theSet), __CFCSetStringLength(theSet) * sizeof(UniChar));
            break;

        case __kCFCharSetClassBitmap:
            if (__CFCSetBitmapBits(theSet)) {
                UInt8 * bitmap = (UInt8 *)CFAllocatorAllocate(CFGetAllocator(cset), sizeof(UInt8) * __kCFBitmapSize, 0);
                CFMoveMemory(bitmap, __CFCSetBitmapBits(theSet), __kCFBitmapSize);
                __CFCSetPutBitmapBits(cset, bitmap);
            }
            break;

        case __kCFCharSetClassSet:
            if (__CFCSetBitmapSet(theSet)) {
                UInt8 * bitmap = (UInt8 *)CFAllocatorAllocate(CFGetAllocator(cset), sizeof(UInt8) * __kCFBitmapSize, 0);
                __CFCSetGetBitmap(theSet, bitmap);
                __CFCSetPutBitmapBits(cset, bitmap);
            }
            break;

        default:
            CFAssert1(0, __kCFLogAssertion, "%s: Internal inconsistency error: unknown character set type", __PRETTY_FUNCTION__); // We should never come here
    }
    return cset;
}

/*** Basic accessors ***/
Boolean CFCharacterSetIsCharacterMember(CFCharacterSetRef theSet, UniChar theChar) {
    Boolean isInverted;
    CFIndex length;

    CF_OBJC_FUNCDISPATCH1(CharacterSet, Boolean, theSet, "characterIsMember:", theChar);

    __CFGenericValidateType(theSet, __kCFCharacterSetTypeID);

    isInverted = __CFCSetIsInverted(theSet);

    switch (__CFCSetClassType(theSet)) {
        case __kCFCharSetClassBuiltin:
            return (CFUniCharIsMemberOf(theChar, __CFCSetBuiltinType(theSet)) ? !isInverted : isInverted);

        case __kCFCharSetClassRange:
            length = __CFCSetRangeLength(theSet);
            return (length && __CFCSetRangeFirstChar(theSet) <= theChar && theChar < __CFCSetRangeFirstChar(theSet) + length ? !isInverted : isInverted);

        case __kCFCharSetClassString:
            if ((length = __CFCSetStringLength(theSet))) {
                return (__CFCSetBsearchUniChar(__CFCSetStringBuffer(theSet), length, theChar) ? !isInverted : isInverted);
            }
            return isInverted;

        case __kCFCharSetClassBitmap:
            if (__CFCSetBitmapBits(theSet)) {
                return (__CFCSetIsMemberBitmap(__CFCSetBitmapBits(theSet), theChar) ? TRUE : FALSE); // Bitmap rep is always correct regardless of __CFCSetIsInverted
            }
            return isInverted;

        case __kCFCharSetClassSet:
            if (__CFCSetBitmapSet(theSet)) {
                return (__CFCSetIsMemberSet(__CFCSetBitmapSet(theSet), theChar) ? !isInverted : isInverted);
            }
            return isInverted;

        default:
            CFAssert1(0, __kCFLogAssertion, "%s: Internal inconsistency error: unknown character set type", __PRETTY_FUNCTION__); // We should never come here
            return FALSE; // To make compiler happy
    }
}

CFDataRef CFCharacterSetCreateBitmapRepresentation(CFAllocatorRef alloc, CFCharacterSetRef theSet) {
    CFMutableDataRef data;

    CF_OBJC_FUNCDISPATCH0(CharacterSet, CFDataRef , theSet, "bitmapRepresentation");

    __CFGenericValidateType(theSet, __kCFCharacterSetTypeID);

    data = CFDataCreateMutable(alloc, __kCFBitmapSize);
    CFDataSetLength(data, __kCFBitmapSize);
    __CFCSetGetBitmap(theSet, CFDataGetMutableBytePtr(data));

    return data;
}

/*** MutableCharacterSet functions ***/
void CFCharacterSetAddCharactersInRange(CFMutableCharacterSetRef theSet, CFRange theRange) {
    CF_OBJC_FUNCDISPATCH1(CharacterSet, void, theSet, "addCharactersInRange:", theRange);

    __CFCSetValidateTypeAndMutability(theSet, __PRETTY_FUNCTION__);
    __CFCSetValidateRange(theRange, __PRETTY_FUNCTION__);

    if (!theRange.length || (__CFCSetIsInverted(theSet) && __CFCSetIsEmpty(theSet))) return; // Inverted && empty set contains all char

    if (!__CFCSetIsInverted(theSet)) {
        if (__CFCSetIsEmpty(theSet)) {
            __CFCSetPutClassType(theSet, __kCFCharSetClassRange);
            __CFCSetPutRangeFirstChar(theSet, theRange.location);
            __CFCSetPutRangeLength(theSet, theRange.length);
            __CFCSetPutHasHashValue(theSet, FALSE);
            return;
       } else if (__CFCSetIsRange(theSet)) {
            UniChar firstChar = __CFCSetRangeFirstChar(theSet);
            CFIndex length = __CFCSetRangeLength(theSet);

            if (firstChar == theRange.location) {
                __CFCSetPutRangeLength(theSet, __CFMin(length, theRange.length));
                __CFCSetPutHasHashValue(theSet, FALSE);
                return;
            } else if (firstChar < theRange.location && theRange.location <= firstChar + length) {
                if (firstChar + length < theRange.location + theRange.length) __CFCSetPutRangeLength(theSet, theRange.length + (theRange.location - firstChar));
                __CFCSetPutHasHashValue(theSet, FALSE);
                return;
            } else if (theRange.location < firstChar && firstChar <= theRange.location + theRange.length) {
                __CFCSetPutRangeFirstChar(theSet, theRange.location);
                __CFCSetPutRangeLength(theSet, length + (firstChar - theRange.location));
                __CFCSetPutHasHashValue(theSet, FALSE);
                return;
            }
        } else if (__CFCSetIsString(theSet) && __CFCSetStringLength(theSet) + theRange.length < __kCFStringCharSetMax) {
            UniChar *buffer;
            if (!__CFCSetStringBuffer(theSet))
                __CFCSetPutStringBuffer(theSet, CFAllocatorAllocate(CFGetAllocator(theSet), __kCFStringCharSetMax, 0));
            buffer = __CFCSetStringBuffer(theSet) + __CFCSetStringLength(theSet);
            while (theRange.length--) *buffer++ = theRange.location++;
            __CFCSetPutStringLength(theSet, __CFCSetStringLength(theSet) + theRange.length);
            qsort(__CFCSetStringBuffer(theSet), __CFCSetStringLength(theSet), sizeof(UniChar), chcompar);
            __CFCSetPutHasHashValue(theSet, FALSE);
            return;
        }
    }

    // OK, I have to be a bitmap
    __CFCSetMakeBitmap(theSet);
    while (theRange.length--) __CFCSetBitmapAddCharacer(__CFCSetBitmapBits(theSet), theRange.location++);
    __CFCSetPutHasHashValue(theSet, FALSE);
}

void CFCharacterSetRemoveCharactersInRange(CFMutableCharacterSetRef theSet, CFRange theRange) {
    CF_OBJC_FUNCDISPATCH1(CharacterSet, void, theSet, "removeCharactersInRange:", theRange);

    __CFCSetValidateTypeAndMutability(theSet, __PRETTY_FUNCTION__);
    __CFCSetValidateRange(theRange, __PRETTY_FUNCTION__);

    if (!theRange.length || (!__CFCSetIsInverted(theSet) && __CFCSetIsEmpty(theSet))) return; // empty set

    if (__CFCSetIsInverted(theSet)) {
        if (__CFCSetIsEmpty(theSet)) {
            __CFCSetPutClassType(theSet, __kCFCharSetClassRange);
            __CFCSetPutRangeFirstChar(theSet, theRange.location);
            __CFCSetPutRangeLength(theSet, theRange.length);
            __CFCSetPutHasHashValue(theSet, FALSE);
            return;
       } else if (__CFCSetIsRange(theSet)) {
            UniChar firstChar = __CFCSetRangeFirstChar(theSet);
            CFIndex length = __CFCSetRangeLength(theSet);

            if (firstChar == theRange.location) {
                __CFCSetPutRangeLength(theSet, __CFMin(length, theRange.length));
                __CFCSetPutHasHashValue(theSet, FALSE);
                return;
            } else if (firstChar < theRange.location && theRange.location <= firstChar + length) {
                if (firstChar + length < theRange.location + theRange.length) __CFCSetPutRangeLength(theSet, theRange.length + (theRange.location - firstChar));
                __CFCSetPutHasHashValue(theSet, FALSE);
                return;
            } else if (theRange.location < firstChar && firstChar <= theRange.location + theRange.length) {
                __CFCSetPutRangeFirstChar(theSet, theRange.location);
                __CFCSetPutRangeLength(theSet, length + (firstChar - theRange.location));
                __CFCSetPutHasHashValue(theSet, FALSE);
                return;
            }
        } else if (__CFCSetIsString(theSet) && __CFCSetStringLength(theSet) + theRange.length < __kCFStringCharSetMax) {
            UniChar *buffer;
            if (!__CFCSetStringBuffer(theSet))
                __CFCSetPutStringBuffer(theSet, CFAllocatorAllocate(CFGetAllocator(theSet), __kCFStringCharSetMax, 0));
            buffer = __CFCSetStringBuffer(theSet) + __CFCSetStringLength(theSet);
            while (theRange.length--) *buffer++ = theRange.location++;
            __CFCSetPutStringLength(theSet, __CFCSetStringLength(theSet) + theRange.length);
            qsort(__CFCSetStringBuffer(theSet), __CFCSetStringLength(theSet), sizeof(UniChar), chcompar);
            __CFCSetPutHasHashValue(theSet, FALSE);
            return;
        }
    }

    // OK, I have to be a bitmap
    __CFCSetMakeBitmap(theSet);
    while (theRange.length--) __CFCSetBitmapRemoveCharacter(__CFCSetBitmapBits(theSet), theRange.location++);
    __CFCSetPutHasHashValue(theSet, FALSE);
}

void CFCharacterSetAddCharactersInString(CFMutableCharacterSetRef theSet,  CFStringRef theString) {
    const UniChar *buffer;
    CFIndex length;

    CF_OBJC_FUNCDISPATCH1(CharacterSet, void, theSet, "addCharactersInString:", theString);

    __CFCSetValidateTypeAndMutability(theSet, __PRETTY_FUNCTION__);

    if ((__CFCSetIsEmpty(theSet) && __CFCSetIsInverted(theSet)) || !(length = CFStringGetLength(theString))) return;

    if (!__CFCSetIsInverted(theSet)) {
        CFIndex newLength = length + (__CFCSetIsEmpty(theSet) ? 0 : (__CFCSetIsString(theSet) ? __CFCSetStringLength(theSet) : __kCFStringCharSetMax));

        if (newLength < __kCFStringCharSetMax) {
            if (__CFCSetIsEmpty(theSet)) __CFCSetPutStringLength(theSet, 0); // Make sure to reset this

            if (!__CFCSetStringBuffer(theSet))
                __CFCSetPutStringBuffer(theSet, CFAllocatorAllocate(CFGetAllocator(theSet), __kCFStringCharSetMax, 0));
            buffer = __CFCSetStringBuffer(theSet) + __CFCSetStringLength(theSet);

            __CFCSetPutClassType(theSet, __kCFCharSetClassString);
            __CFCSetPutStringLength(theSet, newLength);
            CFStringGetCharacters(theString, CFRangeMake(0, length), (UniChar*)buffer);
            qsort(__CFCSetStringBuffer(theSet), newLength, sizeof(UniChar), chcompar);
            __CFCSetPutHasHashValue(theSet, FALSE);
            return;
        }
    }

    // OK, I have to be a bitmap
    __CFCSetMakeBitmap(theSet);
    if ((buffer = CFStringGetCharactersPtr(theString))) {
        while (length--) __CFCSetBitmapAddCharacer(__CFCSetBitmapBits(theSet), *buffer++);
    } else {
        CFStringInlineBuffer inlineBuffer;
        CFIndex index;

        CFStringInitInlineBuffer(theString, &inlineBuffer, CFRangeMake(0, length));
        for (index = 0;index < length;index++) __CFCSetBitmapAddCharacer(__CFCSetBitmapBits(theSet), CFStringGetCharacterFromInlineBuffer(&inlineBuffer, index));
    }
    __CFCSetPutHasHashValue(theSet, FALSE);
}

void CFCharacterSetRemoveCharactersInString(CFMutableCharacterSetRef theSet, CFStringRef theString) {
    const UniChar *buffer;
    CFIndex length;

    CF_OBJC_FUNCDISPATCH1(CharacterSet, void, theSet, "removeCharactersInString:", theString);

    __CFCSetValidateTypeAndMutability(theSet, __PRETTY_FUNCTION__);

    if ((__CFCSetIsEmpty(theSet) && !__CFCSetIsInverted(theSet)) || !(length = CFStringGetLength(theString))) return;

    if (__CFCSetIsInverted(theSet)) {
        CFIndex newLength = length + (__CFCSetIsEmpty(theSet) ? 0 : (__CFCSetIsString(theSet) ? __CFCSetStringLength(theSet) : __kCFStringCharSetMax));

        if (newLength < __kCFStringCharSetMax) {
            if (__CFCSetIsEmpty(theSet)) __CFCSetPutStringLength(theSet, 0); // Make sure to reset this

            if (!__CFCSetStringBuffer(theSet))
                __CFCSetPutStringBuffer(theSet, CFAllocatorAllocate(CFGetAllocator(theSet), __kCFStringCharSetMax, 0));
            buffer = __CFCSetStringBuffer(theSet) + __CFCSetStringLength(theSet);

            __CFCSetPutClassType(theSet, __kCFCharSetClassString);
            __CFCSetPutStringLength(theSet, newLength);
            CFStringGetCharacters(theString, CFRangeMake(0, length), (UniChar *)buffer);
            qsort(__CFCSetStringBuffer(theSet), newLength, sizeof(UniChar), chcompar);
            __CFCSetPutHasHashValue(theSet, FALSE);
            return;
        }
    }

    // OK, I have to be a bitmap
    __CFCSetMakeBitmap(theSet);
    if ((buffer = CFStringGetCharactersPtr(theString))) {
        while (length--) __CFCSetBitmapRemoveCharacter(__CFCSetBitmapBits(theSet), *buffer++);
    } else {
        CFStringInlineBuffer inlineBuffer;
        CFIndex index;

        CFStringInitInlineBuffer(theString, &inlineBuffer, CFRangeMake(0, length));
        for (index = 0;index < length;index++) __CFCSetBitmapRemoveCharacter(__CFCSetBitmapBits(theSet), CFStringGetCharacterFromInlineBuffer(&inlineBuffer, index));
    }
    __CFCSetPutHasHashValue(theSet, FALSE);
}

void CFCharacterSetUnion(CFMutableCharacterSetRef theSet, CFCharacterSetRef theOtherSet) {
    CF_OBJC_FUNCDISPATCH1(CharacterSet, void, theSet, "formUnionWithCharacterSet:", theOtherSet);

    __CFCSetValidateTypeAndMutability(theSet, __PRETTY_FUNCTION__);

    if (__CFCSetIsEmpty(theSet) && __CFCSetIsInverted(theSet)) return; // Inverted empty set contains all char

    if (__CFGenericTypeID(theOtherSet) == __kCFCharacterSetTypeID) { // Really CF, we can do some trick here
        if (__CFCSetIsEmpty(theOtherSet)) return; // Nothing to do here

        if (__CFCSetIsBuiltin(theOtherSet) && __CFCSetIsEmpty(theSet)) { // theSet can be builtin set
            __CFCSetPutClassType(theSet, __kCFCharSetClassBuiltin);
            __CFCSetPutBuiltinType(theSet, __CFCSetBuiltinType(theOtherSet));
            __CFCSetPutHasHashValue(theSet, FALSE);
        } else if (__CFCSetIsRange(theOtherSet)) {
            CFCharacterSetAddCharactersInRange(theSet, CFRangeMake(__CFCSetRangeFirstChar(theOtherSet), __CFCSetRangeLength(theOtherSet)));
        } else if (__CFCSetIsString(theOtherSet)) {
            CFStringRef string = CFStringCreateWithCharactersNoCopy(CFGetAllocator(theSet), __CFCSetStringBuffer(theOtherSet), __CFCSetStringLength(theOtherSet), kCFAllocatorNull);
            CFCharacterSetAddCharactersInString(theSet, string);
            CFRelease(string);
        } else {
            __CFCSetMakeBitmap(theSet);
            if (__CFCSetIsBitmap(theOtherSet)) {
                UInt32 *bitmap1 = (UInt32*)__CFCSetBitmapBits(theSet);
                UInt32 *bitmap2 = (UInt32*)__CFCSetBitmapBits(theOtherSet);
                CFIndex length = __kCFBitmapSize / sizeof(UInt32);
                while (length--) *bitmap1++ |= *bitmap2++;
            } else {
                UInt32 *bitmap1 = (UInt32*)__CFCSetBitmapBits(theSet);
                UInt32 *bitmap2;
                CFIndex length = __kCFBitmapSize / sizeof(UInt32);
                UInt8 bitmapBuffer[__kCFBitmapSize];
                __CFCSetGetBitmap(theOtherSet, bitmapBuffer);
                bitmap2 = (UInt32*)bitmapBuffer;
                while (length--) *bitmap1++ |= *bitmap2++;
            }
            __CFCSetPutHasHashValue(theSet, FALSE);
        }
    } else { // It's NSCharacterSet
        CFDataRef bitmapRep = CFCharacterSetCreateBitmapRepresentation(CFGetAllocator(theSet), theSet);
        const UInt32 *bitmap2 = (bitmapRep && CFDataGetLength(bitmapRep) ? (const UInt32 *)CFDataGetBytePtr(bitmapRep) : NULL);
        if (bitmap2) {
            UInt32 *bitmap1;
            CFIndex length = __kCFBitmapSize / sizeof(UInt32);
            __CFCSetMakeBitmap(theSet);
            bitmap1 = (UInt32*)__CFCSetBitmapBits(theSet);
            while (length--) *bitmap1++ |= *bitmap2++;
            __CFCSetPutHasHashValue(theSet, FALSE);
        }
    }
}

void CFCharacterSetIntersect(CFMutableCharacterSetRef theSet, CFCharacterSetRef theOtherSet) {
    CF_OBJC_FUNCDISPATCH1(CharacterSet, void, theSet, "formIntersectionWithCharacterSet:", theOtherSet);

    __CFCSetValidateTypeAndMutability(theSet, __PRETTY_FUNCTION__);

    if (__CFCSetIsEmpty(theSet) && !__CFCSetIsInverted(theSet)) return; // empty set

    if (__CFGenericTypeID(theOtherSet) == __kCFCharacterSetTypeID) { // Really CF, we can do some trick here
        if (__CFCSetIsEmpty(theOtherSet)) {
           if (!__CFCSetIsInverted(theOtherSet)) {
               if (__CFCSetIsString(theSet) && __CFCSetStringBuffer(theSet)) {
                   CFAllocatorDeallocate(CFGetAllocator(theSet), __CFCSetStringBuffer(theSet));
               } else if (__CFCSetIsBitmap(theSet) && __CFCSetBitmapBits(theSet)) {
                   CFAllocatorDeallocate(CFGetAllocator(theSet), __CFCSetBitmapBits(theSet));
               } else if (__CFCSetIsSet(theSet) && __CFCSetBitmapSet(theSet)) {
                   CFAllocatorDeallocate(CFGetAllocator(theSet), __CFCSetBitmapSet(theSet));
               }
               __CFCSetPutClassType(theSet, __kCFCharSetClassRange);
               __CFCSetPutRangeLength(theSet, 0);
               theSet->_hashValue = 0;
               __CFCSetPutHasHashValue(theSet, TRUE);
            }
        } else if (__CFCSetIsEmpty(theSet)) { // non inverted empty set contains all character
            __CFCSetPutClassType(theSet, __CFCSetClassType(theOtherSet));
            __CFCSetPutHasHashValue(theSet, __CFCSetHasHashValue(theOtherSet));
            __CFCSetPutIsInverted(theSet, __CFCSetIsInverted(theOtherSet));
            theSet->_hashValue = theOtherSet->_hashValue;

            switch (__CFCSetClassType(theOtherSet)) {
                case __kCFCharSetClassBuiltin:
                    __CFCSetPutBuiltinType(theSet, __CFCSetBuiltinType(theOtherSet));
                    break;

                case __kCFCharSetClassRange:
                    __CFCSetPutRangeFirstChar(theSet, __CFCSetRangeFirstChar(theOtherSet));
                    __CFCSetPutRangeLength(theSet, __CFCSetRangeLength(theOtherSet));
                    break;

                case __kCFCharSetClassString:
                    __CFCSetPutStringLength(theSet, __CFCSetStringLength(theOtherSet));
                    if (!__CFCSetStringBuffer(theSet))
                        __CFCSetPutStringBuffer(theSet, CFAllocatorAllocate(CFGetAllocator(theSet), __kCFStringCharSetMax, 0));
                   CFMoveMemory(__CFCSetStringBuffer(theSet), __CFCSetStringBuffer(theOtherSet), __CFCSetStringLength(theSet) * sizeof(UniChar));
                    break;

                case __kCFCharSetClassBitmap:
                    __CFCSetPutBitmapBits(theSet, CFAllocatorAllocate(CFGetAllocator(theSet), sizeof(UInt8) * __kCFBitmapSize, 0));
                    CFMoveMemory(__CFCSetBitmapBits(theSet), __CFCSetBitmapBits(theOtherSet), __kCFBitmapSize);
                    break;

                case __kCFCharSetClassSet:
                    __CFCSetPutBitmapBits(theSet, CFAllocatorAllocate(CFGetAllocator(theSet), sizeof(UInt8) * __kCFBitmapSize, 0));
                    __CFCSetGetBitmap(theOtherSet, __CFCSetBitmapBits(theSet));
                    break;

                default:
                    CFAssert1(0, __kCFLogAssertion, "%s: Internal inconsistency error: unknown character set type", __PRETTY_FUNCTION__); // We should never come here
            }
        } else {
            __CFCSetMakeBitmap(theSet);
            if (__CFCSetIsBitmap(theOtherSet)) {
                UInt32 *bitmap1 = (UInt32*)__CFCSetBitmapBits(theSet);
                UInt32 *bitmap2 = (UInt32*)__CFCSetBitmapBits(theOtherSet);
                CFIndex length = __kCFBitmapSize / sizeof(UInt32);
                while (length--) *bitmap1++ &= *bitmap2++;
            } else {
                UInt32 *bitmap1 = (UInt32*)__CFCSetBitmapBits(theSet);
                UInt32 *bitmap2;
                CFIndex length = __kCFBitmapSize / sizeof(UInt32);
                UInt8 bitmapBuffer[__kCFBitmapSize];
                __CFCSetGetBitmap(theOtherSet, bitmapBuffer);
                bitmap2 = (UInt32*)bitmapBuffer;
                while (length--) *bitmap1++ &= *bitmap2++;
            }
            __CFCSetPutHasHashValue(theSet, FALSE);
        }
    } else { // It's NSCharacterSet
        CFDataRef bitmapRep = CFCharacterSetCreateBitmapRepresentation(CFGetAllocator(theSet), theSet);
        const UInt32 *bitmap2 = (bitmapRep && CFDataGetLength(bitmapRep) ? (const UInt32 *)CFDataGetBytePtr(bitmapRep) : NULL);
        if (bitmap2) {
            UInt32 *bitmap1;
            CFIndex length = __kCFBitmapSize / sizeof(UInt32);
            __CFCSetMakeBitmap(theSet);
            bitmap1 = (UInt32*)__CFCSetBitmapBits(theSet);
            while (length--) *bitmap1++ &= *bitmap2++;
            __CFCSetPutHasHashValue(theSet, FALSE);
        }
    }
}

void CFCharacterSetInvert(CFMutableCharacterSetRef theSet) {
    CF_OBJC_FUNCDISPATCH0(CharacterSet, void, theSet, "invert");

    __CFCSetValidateTypeAndMutability(theSet, __PRETTY_FUNCTION__);

    if (__CFCSetClassType(theSet) == __kCFCharSetClassBitmap) {
        CFIndex index;
        CFIndex count = __kCFBitmapSize / sizeof(UInt32);
        UInt32 *bitmap = (UInt32*) __CFCSetBitmapBits(theSet);

        for (index = 0;index < count;index++) {
            bitmap[index] = ~(bitmap[index]);
        }
    } else {
        __CFCSetPutIsInverted(theSet, !__CFCSetIsInverted(theSet));
    }
    __CFCSetPutHasHashValue(theSet, FALSE);
}

CF_EXPORT void CFCharacterSetCompact(CFMutableCharacterSetRef theSet) {
    if (__CFCSetIsBitmap(theSet) && __CFCSetBitmapBits(theSet)) __CFCSetMakeCompact(theSet);
}

CF_EXPORT void CFCharacterSetFast(CFMutableCharacterSetRef theSet) {
    if (__CFCSetIsSet(theSet) && __CFCSetBitmapSet(theSet)) __CFCSetMakeBitmap(theSet);
}
