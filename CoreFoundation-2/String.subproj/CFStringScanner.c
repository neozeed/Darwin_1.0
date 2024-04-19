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
 /*	CFStringScanner.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Ali Ozer
*/

#include "CFInternal.h"
#include <CoreFoundation/CFString.h>
#include "CFVeryPrivate.h"
#if !defined(__MACOS8__)
#include <sys/types.h>
#endif
#include <limits.h>
#include <stdlib.h>

// This needs to be defined somewhere more public; I had to copy it in CFPropertyList.c -- REW, 4/8/99
#if !defined(QUAD_MAX)
    #if defined(LONG_LONG_MAX)
	#define QUAD_MAX	LONG_LONG_MAX
    #elif defined(LLONG_MAX)
    #define QUAD_MAX	LLONG_MAX
    #else
	#warning Arbitrarily defining QUAD_MAX
	#define QUAD_MAX	9223372036854775807LL
    #endif
#endif /* !defined(QUAD_MAX) */

#if !defined(QUAD_MIN)
    #if defined(LONG_LONG_MIN)
	#define QUAD_MIN	LONG_LONG_MIN
	#elif defined(LLONG_MIN)
	#define QUAD_MIN	LLONG_MIN
    #else
	#warning Arbitrarily defining QUAD_MIN
	#define QUAD_MIN	(-9223372036854775807LL-1LL)
    #endif
#endif /* !defined(QUAD_MIN) */

CF_INLINE Boolean __CFCharacterIsADigit(UniChar ch) {
    return (ch >= '0' && ch <= '9') ? TRUE : FALSE;
}

/* Returns -1 on illegal value */
CF_INLINE SInt32 __CFCharacterNumericOrHexValue (UniChar ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'A' && ch <= 'F') {
        return ch + 10 - 'A';
    } else if (ch >= 'a' && ch <= 'f') {
        return ch + 10 - 'a';
    } else {
        return -1;
    }
}
               
/* Returns -1 on illegal value */
CF_INLINE SInt32 __CFCharacterNumericValue(UniChar ch) {
    return (ch >= '0' && ch <= '9') ? (ch - '0') : -1;
}

CF_INLINE UniChar __CFStringGetFirstNonSpaceCharacterFromInlineBuffer(CFStringInlineBuffer *buf, SInt32 *indexPtr) {
    UniChar ch;
    while (__CFIsWhitespace(ch = __CFStringGetCharacterFromInlineBufferAux(buf, *indexPtr))) (*indexPtr)++;
    return ch;
}

/* result is long long or int, depending on doLonglong
*/
Boolean __CFStringScanInteger(CFStringInlineBuffer *buf, CFDictionaryRef locale, SInt32 *indexPtr, Boolean doLonglong, void *result) {
    Boolean doingLonglong = FALSE;	/* Set to TRUE if doLonglong, and we overflow an int... */
    Boolean neg = FALSE;
    int intResult = 0;
    register long long longlongResult = 0;	/* ??? long long is slow when not in regs; I hope this does the right thing. */
    UniChar ch;

    ch = __CFStringGetFirstNonSpaceCharacterFromInlineBuffer(buf, indexPtr);

    if (ch == '-' || ch == '+') {
	neg = (ch == '-');
	(*indexPtr)++;
    	ch = __CFStringGetFirstNonSpaceCharacterFromInlineBuffer(buf, indexPtr);
    }	

    if (! __CFCharacterIsADigit(ch)) return FALSE;	/* No digits, bail out... */
    do {
	if (doingLonglong) {
            if ((longlongResult >= QUAD_MAX / 10) && ((longlongResult > QUAD_MAX / 10) || (__CFCharacterNumericValue(ch) - (neg ? 1 : 0) >= QUAD_MAX - longlongResult * 10))) {
                /* ??? This might not handle QUAD_MIN correctly... */
                longlongResult = neg ? QUAD_MIN : QUAD_MAX;
                neg = FALSE;
                while (__CFCharacterIsADigit(ch = __CFStringGetCharacterFromInlineBufferAux(buf, ++(*indexPtr))));	/* Skip remaining digits */
            } else {
                longlongResult = longlongResult * 10 + __CFCharacterNumericValue(ch);
                ch = __CFStringGetCharacterFromInlineBufferAux(buf, ++(*indexPtr));
            }
	} else {
            if ((intResult >= INT_MAX / 10) && ((intResult > INT_MAX / 10) || (__CFCharacterNumericValue(ch) - (neg ? 1 : 0) >= INT_MAX - intResult * 10))) {
                // Overflow, check for long long...
                if (doLonglong) {
                    longlongResult = intResult;
                    doingLonglong = TRUE;
                } else {
                    /* ??? This might not handle INT_MIN correctly... */
                    intResult = neg ? INT_MIN : INT_MAX;
                    neg = FALSE;
                    while (__CFCharacterIsADigit(ch = __CFStringGetCharacterFromInlineBufferAux(buf, ++(*indexPtr))));	/* Skip remaining digits */
                }
            } else {
                intResult = intResult * 10 + __CFCharacterNumericValue(ch);
                ch = __CFStringGetCharacterFromInlineBufferAux(buf, ++(*indexPtr));
            }
	}
    } while (__CFCharacterIsADigit(ch));

    if (result) {
        if (doLonglong) {
	    if (!doingLonglong) longlongResult = intResult;
	    *(long long *)result = neg ? -longlongResult : longlongResult;
	} else {
	    *(int *)result = neg ? -intResult : intResult;
	}
    }

    return TRUE;
}

Boolean __CFStringScanHex(CFStringInlineBuffer *buf, SInt32 *indexPtr, unsigned *result) {
    UInt32 value = 0;
    UInt32 curDigit;
    UniChar ch;

    ch = __CFStringGetFirstNonSpaceCharacterFromInlineBuffer(buf, indexPtr);
    /* Ignore the optional "0x" or "0X"; if it's followed by a non-hex, just parse the "0" and leave pointer at "x" */
    if (ch == '0') {
	ch = __CFStringGetCharacterFromInlineBufferAux(buf, ++(*indexPtr));
        if (ch == 'x' || ch == 'X') ch = __CFStringGetCharacterFromInlineBufferAux(buf, ++(*indexPtr));
	curDigit = __CFCharacterNumericOrHexValue(ch);
        if (curDigit == -1) {
	    (*indexPtr)--;	/* Go back over the "x" or "X" */
	    if (result) *result = 0;
            return TRUE;	/* We just saw "0" */
        }
    } else {
	curDigit = __CFCharacterNumericOrHexValue(ch);
        if (curDigit == -1) return FALSE;
    }    

    do {
        if (value > (UINT_MAX >> 4)) {	
	    value = UINT_MAX;	/* We do this over and over again, but it's an error case anyway */
        } else {
            value = (value << 4) + curDigit;
        }
	curDigit = __CFCharacterNumericOrHexValue(__CFStringGetCharacterFromInlineBufferAux(buf, ++(*indexPtr)));
    } while (curDigit != -1);

    if (result) *result = value;
    return TRUE;
}

// Packed array of Boolean
static const char __CFNumberSet[16] = {
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  nul soh stx etx eot enq ack bel
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  bs  ht  nl  vt  np  cr  so  si
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  dle dc1 dc2 dc3 dc4 nak syn etb
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  can em  sub esc fs  gs  rs  us
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  sp   !   "   #   $   %   &   '
    0X28, // 0, 0, 0, 1, 0, 1, 0, 0, //  (   )   *   +   ,   -   .   /
    0XFF, // 1, 1, 1, 1, 1, 1, 1, 1, //  0   1   2   3   4   5   6   7
    0X03, // 1, 1, 0, 0, 0, 0, 0, 0, //  8   9   :   ;   <   =   >   ?
    0X20, // 0, 0, 0, 0, 0, 1, 0, 0, //  @   A   B   C   D   E   F   G
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  H   I   J   K   L   M   N   O
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  P   Q   R   S   T   U   V   W
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  X   Y   Z   [   \   ]   ^   _
    0X20, // 0, 0, 0, 0, 0, 1, 0, 0, //  `   a   b   c   d   e   f   g
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  h   i   j   k   l   m   n   o
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0, //  p   q   r   s   t   u   v   w
    0X00, // 0, 0, 0, 0, 0, 0, 0, 0  //  x   y   z   {   |   }   ~  del
};

Boolean __CFStringScanDouble(CFStringInlineBuffer *buf, CFDictionaryRef locale, SInt32 *indexPtr, double *resultPtr) {
    #define STACK_BUFFER_SIZE 256
    #define ALLOC_CHUNK_SIZE 256 // first and subsequent malloc size.  Should be greater than STACK_BUFFER_SIZE
    char localCharBuffer[STACK_BUFFER_SIZE];
    char *charPtr = localCharBuffer;
    char *endCharPtr;
    UniChar decimalChar = '.';
    SInt32 numChars = 0;
    SInt32 capacity = STACK_BUFFER_SIZE;	// in chars
    double result;
    UniChar ch;
    CFAllocatorRef tmpAlloc = NULL;

#if 0
    if (locale != NULL) {
        CFStringRef decimalSeparator = [locale objectForKey: NSDecimalSeparator];
        if (decimalSeparator != nil) decimalChar = [decimalSeparator characterAtIndex:0];
    }
#endif
    ch = __CFStringGetFirstNonSpaceCharacterFromInlineBuffer(buf, indexPtr);
    // At this point indexPtr points at the first non-space char
    do {
	if (ch >= 128 || (__CFNumberSet[ch >> 3] & (1 << (ch & 7))) == 0) {
            // Not in __CFNumberSet
	    if (ch != decimalChar) break;
            ch = '.';	// Replace the decimal character with something strtod will understand
        }
        if (numChars >= capacity - 1) {
	    capacity += ALLOC_CHUNK_SIZE;
	    if (tmpAlloc == NULL) tmpAlloc = CFAllocatorGetDefault();
	    if (charPtr == localCharBuffer) {
		charPtr = CFAllocatorAllocate(tmpAlloc, capacity * sizeof(char), 0);
		CFMoveMemory(charPtr, localCharBuffer, numChars * sizeof(char));
 	    } else {
		charPtr = CFAllocatorReallocate(tmpAlloc, charPtr, capacity * sizeof(char), 0);
	    }
        }
	charPtr[numChars++] = (char)ch;
	ch = __CFStringGetCharacterFromInlineBufferAux(buf, *indexPtr + numChars);
    } while (TRUE);
    charPtr[numChars] = 0;	// Null byte for strtod
    result = strtod(charPtr, &endCharPtr);
    if (tmpAlloc) CFAllocatorDeallocate(tmpAlloc, charPtr);
    if (charPtr == endCharPtr) return FALSE;
    *indexPtr += (endCharPtr - charPtr);
    if (resultPtr) *resultPtr = result; // only store result if we succeed
    
    return TRUE;
}


#undef QUAD_MAX
#undef QUAD_MIN
#undef STACK_BUFFER_SIZE
#undef ALLOC_CHUNK_SIZE


