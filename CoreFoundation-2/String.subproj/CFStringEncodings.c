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
/*	CFStringEncodings.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Aki Inoue
*/

#include "CFInternal.h"
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFByteOrder.h>
#include "CFStringBuffer.h"
#include "CFStringEncodingConverterExt.h"
#include "CFVeryPrivate.h"

UInt32 CFUnicodeToLossyASCII(CFStringBuffer *buf, UInt8 *target);

Boolean (*__CFCharToUniCharFunc)(UInt32 flags, UInt8 ch, UniChar *unicodeChar) = NULL;

void __CFStrConvertBytesToUnicode(const UInt8 *bytes, UniChar *buffer, CFIndex numChars) {
    if (__CFCharToUniCharFunc) {
        UInt32 idx;
        for (idx = 0;idx < numChars;idx++) buffer[idx] = (bytes[idx] < 0x80 ? bytes[idx] : (__CFCharToUniCharFunc(0, bytes[idx], &(buffer[idx])) ? buffer[idx] : 0xFFFE));
    } else if (__CFStringGetEightBitStringEncoding() == kCFStringEncodingASCII) {
        UInt32 idx;
        for (idx = 0;idx < numChars;idx++) buffer[idx] = bytes[idx];
    } else { // Should never come here
        if (CFStringEncodingIsValidEncoding(__CFStringGetEightBitStringEncoding())) {
            CFStringEncodingBytesToUnicode(__CFStringGetEightBitStringEncoding(), 0, bytes, numChars, NULL, buffer, numChars, NULL);
        }
    }
}


/* The minimum length the output buffers should be in the above functions
*/
#define kCFCharConversionBufferLength 512


#define MAX_LOCAL_CHARS		(sizeof(buffer->localBuffer) / sizeof(UInt8))
#define MAX_LOCAL_UNICHARS	(sizeof(buffer->localBuffer) / sizeof(UniChar))

#if defined(__BIG_ENDIAN__)
#define SHOULD_SWAP(BOM) (BOM == 0xFFFE)
#else
#define SHOULD_SWAP(BOM) (BOM != 0xFEFF)
#endif

CF_INLINE UInt8 hexToByte(UInt8 ch) {
    return (ch >= '0' && ch <= '9') ? (ch - '0') : (ch - 'A') + 10;
}

/* Convert a byte stream to ASCII (7-bit!) or Unicode, with a CFVarWidthCharBuffer struct on the stack. FALSE return indicates an error occured during the conversion. The caller needs to free the returned buffer in either ascii or unicode (indicated by isASCII), if shouldFreeChars is true. 
9/18/98 __CFStringDecodeByteStream now avoids to allocate buffer if buffer->chars is not NULL
Added useClientsMemoryPtr; if not-NULL, and the provided memory can be used as is, this is set to TRUE
*/
Boolean __CFStringDecodeByteStream2(const UInt8 *bytes, UInt32 len, CFStringEncoding encoding, Boolean alwaysUnicode, CFVarWidthCharBuffer *buffer, Boolean *useClientsMemoryPtr) {
    UInt32 idx;
    const UniChar *uniChars = (const UniChar *)bytes;
    const UInt8 *chars = (const UInt8 *)bytes;
    const UInt8 *end = chars + len;
    UInt16 bom;
    Boolean allASCII = FALSE;

    if (useClientsMemoryPtr) *useClientsMemoryPtr = FALSE;

    buffer->isASCII = !alwaysUnicode;
    buffer->shouldFreeChars = FALSE;
    buffer->numChars = 0;
    if (0 == len) return TRUE;

    buffer->allocator = (buffer->allocator ? buffer->allocator : CFAllocatorGetDefault());
    switch (encoding) {
    case kCFStringEncodingUnicode:
        bom = (*uniChars == 0xfffe || *uniChars == 0xfeff) ? (*uniChars++) : 0;
	/* If the byte order mark is missing, we assume big endian... */
	len = len / 2 - (0 == bom ? 0 : 1);

        if (buffer->isASCII) {	// Let's see if we can reduce the Unicode down to ASCII...
            if (SHOULD_SWAP(bom)) {
                for (idx = 0; idx < len; idx++) if ((uniChars[idx] & 0x80ff) != 0) {buffer->isASCII = FALSE; break;}
            } else {
                for (idx = 0; idx < len; idx++) if (uniChars[idx] > 127) {buffer->isASCII = FALSE; break;}
            }
        }

        if (buffer->isASCII) {
            buffer->numChars = len;
            buffer->shouldFreeChars = !buffer->chars.ascii && (len <= MAX_LOCAL_CHARS) ? FALSE : TRUE;
            buffer->chars.ascii = (buffer->chars.ascii ? buffer->chars.ascii : (len <= MAX_LOCAL_CHARS) ? (UInt8 *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UInt8), 0));
            if (SHOULD_SWAP(bom)) {	// !!! Can be somewhat trickier here and use a single loop with a properly inited ptr
                for (idx = 0; idx < len; idx++) buffer->chars.ascii[idx] = (uniChars[idx] >> 8);
            } else {
                for (idx = 0; idx < len; idx++) buffer->chars.ascii[idx] = uniChars[idx];
            }
        } else {
            buffer->numChars = len;
            if (useClientsMemoryPtr && (bom == 0) && !SHOULD_SWAP(bom)) {	// If the caller is ready to deal with no-copy situation, and the situation is possible, indicate it...
                *useClientsMemoryPtr = TRUE;
                buffer->shouldFreeChars = FALSE;
                buffer->chars.unicode = (UniChar *)bytes;
            } else {
                buffer->shouldFreeChars = !buffer->chars.unicode && (len <= MAX_LOCAL_UNICHARS) ? FALSE : TRUE;
                buffer->chars.unicode = (buffer->chars.unicode ? buffer->chars.unicode : (len <= MAX_LOCAL_UNICHARS) ? (UniChar *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UniChar), 0));
            if (SHOULD_SWAP(bom)) {
                    for (idx = 0; idx < len; idx++) buffer->chars.unicode[idx] = CFSwapInt16(uniChars[idx]);
                } else {
                    CFMoveMemory(buffer->chars.unicode, uniChars, len * sizeof(UniChar));
                }
            }
        }
	return TRUE;

    case kCFStringEncodingNonLossyASCII:
	buffer->isASCII = FALSE;
        buffer->shouldFreeChars = !buffer->chars.unicode && (len <= MAX_LOCAL_UNICHARS) ? FALSE : TRUE;
        buffer->chars.unicode = (buffer->chars.unicode ? buffer->chars.unicode : (len <= MAX_LOCAL_UNICHARS) ? (UniChar *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UniChar), 0));
	buffer->numChars = 0;
	while (chars < end) {
	    UniChar ch = (UniChar)*chars++;
	    if (ch == '\\') {
		ch = (UniChar)*chars++;
		if ((ch == 'U') || (ch == 'u')) {
		    ch = hexToByte(*chars++);
		    ch = (ch << 4) | hexToByte(*chars++);
		    ch = (ch << 4) | hexToByte(*chars++);
		    ch = (ch << 4) | hexToByte(*chars++);
		} else if (ch != '\\') {
		    ch = ch - '0';
		    ch = (ch << 3) | (*chars++ - '0');
		    ch = (ch << 3) | (*chars++ - '0');
		}
	    }
	    buffer->chars.unicode[buffer->numChars++] = ch;
	}
        return TRUE;

    case kCFStringEncodingUTF8:
        allASCII = !alwaysUnicode;
        if (allASCII) {
            for (idx = 0; idx < len; idx++)
                if (128 <= chars[idx]) {
                    allASCII = FALSE;
                    break;
                }
        }
            buffer->isASCII = allASCII;
        if (allASCII) {
            buffer->numChars = len;
            buffer->shouldFreeChars = !buffer->chars.ascii && (len <= MAX_LOCAL_CHARS) ? FALSE : TRUE;
            buffer->chars.ascii = (buffer->chars.ascii ? buffer->chars.ascii : (len <= MAX_LOCAL_CHARS) ? (UInt8 *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UInt8), 0));
            CFMoveMemory(buffer->chars.ascii, chars, len * sizeof(UInt8));
        } else {
            UInt32 numDone;
            static CFStringEncodingToUnicodeProc __CFFromUTF8 = NULL;

            if (!__CFFromUTF8) {
                const CFStringEncodingConverter *converter = CFStringEncodingGetConverter(kCFStringEncodingUTF8);
                __CFFromUTF8 = (CFStringEncodingToUnicodeProc)converter->toUnicode;
            }

            buffer->shouldFreeChars = !buffer->chars.unicode && (len <= MAX_LOCAL_UNICHARS) ? FALSE : TRUE;
            buffer->chars.unicode = (buffer->chars.unicode ? buffer->chars.unicode : (len <= MAX_LOCAL_UNICHARS) ? (UniChar *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UniChar), 0));
            buffer->numChars = 0;
            while (chars < end) {
                numDone = 0;
                chars += __CFFromUTF8(0, chars, end - chars, &(buffer->chars.unicode[buffer->numChars]), len - buffer->numChars, &numDone);

                if (0 == numDone) {
                    if (buffer->shouldFreeChars) CFAllocatorDeallocate(buffer->allocator, buffer->chars.unicode);
                    buffer->isASCII = !alwaysUnicode;
                    buffer->shouldFreeChars = FALSE;
                    buffer->chars.ascii = NULL;
                    buffer->numChars = 0;
                    return FALSE;
                }
                buffer->numChars += numDone;
            }
        }
        return TRUE;

    default:
        if (CFStringEncodingIsValidEncoding(encoding)) {
            const CFStringEncodingConverter *converter = CFStringEncodingGetConverter(encoding);
            Boolean isASCIISuperset = __CFStringEncodingIsSupersetOfASCII(encoding);
            
            if (!converter) return FALSE;

            if (converter->encodingClass == kCFStringEncodingConverterCheapEightBit) {
                allASCII = !alwaysUnicode && isASCIISuperset;
                    if (allASCII) {
                        for (idx = 0; idx < len; idx++) {
                            if (128 <= chars[idx]) {
                                allASCII = FALSE;
                                break;
                            }
                        }
                    }
                    buffer->isASCII = allASCII;
                    if (allASCII) {
                        buffer->numChars = len;
                        buffer->shouldFreeChars = !buffer->chars.ascii && (len <= MAX_LOCAL_CHARS) ? FALSE : TRUE;
                        buffer->chars.ascii = (buffer->chars.ascii ? buffer->chars.ascii : (len <= MAX_LOCAL_CHARS) ? (UInt8 *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UInt8), 0));
                        CFMoveMemory(buffer->chars.ascii, chars, len * sizeof(UInt8));
                    } else {
                        buffer->shouldFreeChars = !buffer->chars.unicode && (len <= MAX_LOCAL_UNICHARS) ? FALSE : TRUE;
                        buffer->chars.unicode = (buffer->chars.unicode ? buffer->chars.unicode : (len <= MAX_LOCAL_UNICHARS) ? (UniChar *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UniChar), 0));
                        buffer->numChars = len;
                        if (kCFStringEncodingASCII == encoding || kCFStringEncodingISOLatin1 == encoding) {
                            for (idx = 0; idx < len; idx++) buffer->chars.unicode[idx] = (UniChar)chars[idx];
                        } else {
                            for (idx = 0; idx < len; idx++)
                                if (chars[idx] < 0x80 && isASCIISuperset)
                                    buffer->chars.unicode[idx] = (UniChar)chars[idx];
                                else if (!((CFStringEncodingCheapEightBitToUnicodeProc)converter->toUnicode)(0, chars[idx], buffer->chars.unicode + idx))
                                    return FALSE;
                        }
                    }
                    return TRUE;
            } else {
                allASCII = !alwaysUnicode && isASCIISuperset;
                if (allASCII) {
                    for (idx = 0; idx < len; idx++)
                        if (128 <= chars[idx]) {
                            allASCII = FALSE;
                            break;
                        }
                }
                    buffer->isASCII = allASCII;
                if (allASCII) {
                    buffer->numChars = len;
                    buffer->shouldFreeChars = !buffer->chars.ascii && (len <= MAX_LOCAL_CHARS) ? FALSE : TRUE;
                    buffer->chars.ascii = (buffer->chars.ascii ? buffer->chars.ascii : (len <= MAX_LOCAL_CHARS) ? (UInt8 *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, len * sizeof(UInt8), 0));
                    CFMoveMemory(buffer->chars.ascii, chars, len * sizeof(UInt8));
                } else {
                    UInt32 guessedLength = CFStringEncodingCharLengthForBytes(encoding, 0, bytes, len);

                    buffer->shouldFreeChars = !buffer->chars.unicode && (guessedLength <= MAX_LOCAL_UNICHARS) ? FALSE : TRUE;
                    buffer->chars.unicode = (buffer->chars.unicode ? buffer->chars.unicode : (guessedLength <= MAX_LOCAL_UNICHARS) ? (UniChar *)buffer->localBuffer : CFAllocatorAllocate(buffer->allocator, guessedLength * sizeof(UniChar), 0));

                    if (CFStringEncodingBytesToUnicode(encoding, kCFStringEncodingAllowLossyConversion, bytes, len, NULL, buffer->chars.unicode, (guessedLength > MAX_LOCAL_UNICHARS ? guessedLength : MAX_LOCAL_UNICHARS), &(buffer->numChars))) {
                        if (buffer->shouldFreeChars) CFAllocatorDeallocate(buffer->allocator, buffer->chars.unicode);
                        buffer->isASCII = !alwaysUnicode;
                        buffer->shouldFreeChars = FALSE;
                        buffer->chars.ascii = NULL;
                        buffer->numChars = 0;
                        return FALSE;
                    }
                }
                return TRUE;
            }
	} else {
	    return FALSE;
        }
    }
}


/* Create a byte stream from a CFString backing. Can convert a string piece at a time
   into a fixed size buffer. Returns number of characters converted. 
   Characters that cannot be converted to the specified encoding are represented
   with the char specified by lossByte; if 0, then lossy conversion is not allowed
   and conversion stops, returning partial results.
   Pass buffer==NULL if you don't care about the converted string (but just the convertability,
   or number of bytes required, indicated by usedBufLen). 
   Does not zero-terminate. If you want to create Pascal or C string, allow one extra byte at start or end. 

   Note: This function is intended to work through CFString functions, so it should work
   with NSStrings as well as CFStrings.
*/
CFIndex __CFStringEncodeByteStream(CFStringRef string, CFIndex rangeLoc, CFIndex rangeLen, Boolean generatingExternalFile, CFStringEncoding encoding, char lossByte, UInt8 *buffer, CFIndex max, CFIndex *usedBufLen) {
    UInt32 totalBytesWritten = 0;	/* Number of written bytes */
    UInt32 numCharsProcessed = 0;	/* Number of processed chars */
    const UniChar *unichars;

    if (encoding == kCFStringEncodingUTF8 && (unichars = CFStringGetCharactersPtr(string))) {
        static CFStringEncodingToBytesProc __CFToUTF8 = NULL;

        if (!__CFToUTF8) {
            const CFStringEncodingConverter *utf8Converter = CFStringEncodingGetConverter(kCFStringEncodingUTF8);
            __CFToUTF8 = (CFStringEncodingToBytesProc)utf8Converter->toBytes;
        }
        numCharsProcessed = __CFToUTF8((generatingExternalFile ? kCFStringEncodingPrependBOM : 0), unichars + rangeLoc, rangeLen, buffer, (buffer ? max : 0), &totalBytesWritten);

    } else if (encoding == kCFStringEncodingNonLossyASCII) {
	const char *hex = "0123456789ABCDEF";
	UniChar ch;
	CFStringBuffer buf;
	CFStringBufferInit(&buf, string, rangeLoc, rangeLen);
	while ((ch = CFStringBufferCurrentCharacter(&buf)) != CFStringBufferEndCharacter && (numCharsProcessed < rangeLen)) {
	    UInt32 reqLength; /* Required number of chars to encode this UniChar */
	    UInt32 cnt;
	    char tmp[6];
	    if ((ch >= ' ' && ch <= '~' && ch != '\\') || (ch == '\n' || ch == '\r' || ch == '\t')) {
		reqLength = 1;
		tmp[0] = ch;
	    } else {
		if (ch == '\\') {
		    tmp[1] = '\\';
		    reqLength = 2;
		} else if (ch < 256) {	/* \nnn; note that this is not NEXTSTEP encoding but a (small) UniChar */
		    tmp[1] = '0' + (ch >> 6);
		    tmp[2] = '0' + ((ch >> 3) & 7);
		    tmp[3] = '0' + (ch & 7);
		    reqLength = 4;
		} else {	/* \Unnnn */
		    tmp[1] = 'u'; // Changed to small+u in order to be aligned with Java
		    tmp[2] = hex[(ch >> 12) & 0x0f];
		    tmp[3] = hex[(ch >> 8) & 0x0f];
		    tmp[4] = hex[(ch >> 4) & 0x0f];
		    tmp[5] = hex[ch & 0x0f];
		    reqLength = 6;
		}
		tmp[0] = '\\';
	    }
            if (buffer) {
                if (totalBytesWritten + reqLength > max) break; /* Doesn't fit..
.*/
                for (cnt = 0; cnt < reqLength; cnt++) {
                    buffer[totalBytesWritten + cnt] = tmp[cnt];
                }
            }
	    totalBytesWritten += reqLength;
	    numCharsProcessed++;
	    CFStringBufferIncrementLocation(&buf);
	}
    } else if (encoding == kCFStringEncodingUnicode) {
   	SInt32 extraForBOM = generatingExternalFile ? sizeof(UniChar) : 0;
        numCharsProcessed = rangeLen;
        if (buffer && (numCharsProcessed * sizeof(UniChar) + extraForBOM > max)) {
            numCharsProcessed = (max > extraForBOM) ? ((max - extraForBOM) / sizeof(UniChar)) : 0;
        }
        totalBytesWritten = (numCharsProcessed * sizeof(UniChar)) + extraForBOM;
	if (buffer) {
	    if (generatingExternalFile) {	/* Generate BOM */
#if defined(__BIG_ENDIAN__)
		*buffer++ = 0xfe; *buffer++ = 0xff;
#else
		*buffer++ = 0xff; *buffer++ = 0xfe;
#endif
	    }
	    CFStringGetCharacters(string, CFRangeMake(0, numCharsProcessed), (UniChar *)buffer);
	}
    } else {
        UInt32 numChars;
        UInt32 flags = (lossByte ? kCFStringEncodingAllowLossyConversion : 0) | (generatingExternalFile ? kCFStringEncodingPrependBOM : 0);
        const unsigned char *cString = NULL;

        if (!CF_IS_OBJC(String, string) && __CFStringEncodingIsSupersetOfASCII(encoding)) { // Checking for NSString to avoid infinite recursion
            const unsigned char *ptr;
            if ((cString = CFStringGetCStringPtr(string, __CFStringGetEightBitStringEncoding()))) {
                ptr = (cString += rangeLoc);
                if (__CFStringGetEightBitStringEncoding() == encoding) {
                    numCharsProcessed = (rangeLen < max || buffer == NULL ? rangeLen : max);
                    if (buffer) CFMoveMemory(buffer, cString, numCharsProcessed);
                    if (usedBufLen) *usedBufLen = numCharsProcessed;
                    return numCharsProcessed;
                }
                while (*ptr < 0x80 && rangeLen > 0) {
                    ++ptr;
                    --rangeLen;
                }
                numCharsProcessed = ptr - cString;
                if (buffer) {
                    numCharsProcessed = (numCharsProcessed < max ? numCharsProcessed : max);
                    CFMoveMemory(buffer, cString, numCharsProcessed);
                    buffer += numCharsProcessed;
		    max -= numCharsProcessed;
                }
                if (!rangeLen || (buffer && numCharsProcessed == max)) {
                    if (usedBufLen) *usedBufLen = numCharsProcessed;
                    return numCharsProcessed;
                }
                rangeLoc += numCharsProcessed;
                totalBytesWritten += numCharsProcessed;
            }
            if (!cString && (cString = CFStringGetPascalStringPtr(string, __CFStringGetEightBitStringEncoding()))) {
                ptr = (cString += (rangeLoc + 1));
                if (__CFStringGetEightBitStringEncoding() == encoding) {
                    numCharsProcessed = (rangeLen < max || buffer == NULL ? rangeLen : max);
                    if (buffer) CFMoveMemory(buffer, cString, numCharsProcessed);
                    if (usedBufLen) *usedBufLen = numCharsProcessed;
                    return numCharsProcessed;
                }
                while (*ptr < 0x80 && rangeLen > 0) {
                    ++ptr;
                    --rangeLen;
                }
                numCharsProcessed = ptr - cString;
                if (buffer) {
                    numCharsProcessed = (numCharsProcessed < max ? numCharsProcessed : max);
                    CFMoveMemory(buffer, cString, numCharsProcessed);
                    buffer += numCharsProcessed;
		    max -= numCharsProcessed;
                }
                if (!rangeLen || (buffer && numCharsProcessed == max)) {
                    if (usedBufLen) *usedBufLen = numCharsProcessed;
                    return numCharsProcessed;
                }
                rangeLoc += numCharsProcessed;
                totalBytesWritten += numCharsProcessed;
            }
        }

        if (!buffer) max = 0;

        if (!cString && (cString = (const char*)CFStringGetCharactersPtr(string))) { // Must be Unicode string
            if (CFStringEncodingIsValidEncoding(encoding)) { // Converter available in CF
                CFStringEncodingUnicodeToBytes(encoding, flags, (const UniChar*)cString + rangeLoc, rangeLen, &numCharsProcessed, buffer, max, &totalBytesWritten);
            } else {
                return 0;
            }
        } else {
            UniChar charBuf[kCFCharConversionBufferLength];
            UInt32 currentLength;
            UInt32 usedLen;
            Boolean isCFBuiltin = CFStringEncodingIsValidEncoding(encoding);

            while (rangeLen > 0) { // ??? This could chop down decomposed sequences into multiple iteration
                currentLength = (rangeLen > kCFCharConversionBufferLength ? kCFCharConversionBufferLength : rangeLen);
                CFStringGetCharacters(string, CFRangeMake(rangeLoc, currentLength), charBuf);

                if (isCFBuiltin) { // Converter available in CF
                    if (CFStringEncodingUnicodeToBytes(encoding, flags, charBuf, currentLength, &numChars, buffer, max, &usedLen)) { // Somehow conversion failed in the middle
                        totalBytesWritten += usedLen;
                        numCharsProcessed += numChars;
                        break;
                    }
                } else {
                    return 0;
                }

                totalBytesWritten += usedLen;
                numCharsProcessed += numChars;

                rangeLoc += numChars;
                rangeLen -= numChars;
                if (max) {
                    buffer += usedLen;
                    max -= usedLen;
                    if (max <= 0) break;
                }
                flags &= ~kCFStringEncodingPrependBOM;
            }
        }
    }
    if (usedBufLen) *usedBufLen = totalBytesWritten;
    return numCharsProcessed;
}

/* Mapping 128..255 to lossy ASCII
*/
static const struct {
    unsigned char chars[4];
} _toLossyASCIITable[] = {
    {{' ', 0, 0, 0}}, // NO-BREAK SPACE
    {{'!', 0, 0, 0}}, // INVERTED EXCLAMATION MARK
    {{'c', 0, 0, 0}}, // CENT SIGN
    {{'L', 0, 0, 0}}, // POUND SIGN
    {{'$', 0, 0, 0}}, // CURRENCY SIGN
    {{'Y', 0, 0, 0}}, // YEN SIGN
    {{'|', 0, 0, 0}}, // BROKEN BAR
    {{0, 0, 0, 0}}, // SECTION SIGN
    {{0, 0, 0, 0}}, // DIAERESIS
    {{'(', 'C', ')', 0}}, // COPYRIGHT SIGN
    {{'a', 0, 0, 0}}, // FEMININE ORDINAL INDICATOR
    {{'<', '<', 0, 0}}, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    {{0, 0, 0, 0}}, // NOT SIGN
    {{'-', 0, 0, 0}}, // SOFT HYPHEN
    {{'(', 'R', ')', 0}}, // REGISTERED SIGN
    {{0, 0, 0, 0}}, // MACRON
    {{0, 0, 0, 0}}, // DEGREE SIGN
    {{'+', '-', 0, 0}}, // PLUS-MINUS SIGN
    {{'2', 0, 0, 0}}, // SUPERSCRIPT TWO
    {{'3', 0, 0, 0}}, // SUPERSCRIPT THREE
    {{0, 0, 0, 0}}, // ACUTE ACCENT
    {{0, 0, 0, 0}}, // MICRO SIGN
    {{0, 0, 0, 0}}, // PILCROW SIGN
    {{0, 0, 0, 0}}, // MIDDLE DOT
    {{0, 0, 0, 0}}, // CEDILLA
    {{'1', 0, 0, 0}}, // SUPERSCRIPT ONE
    {{'o', 0, 0, 0}}, // MASCULINE ORDINAL INDICATOR
    {{'>', '>', 0, 0}}, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    {{'1', '/', '4', 0}}, // VULGAR FRACTION ONE QUARTER
    {{'1', '/', '2', 0}}, // VULGAR FRACTION ONE HALF
    {{'3', '/', '4', 0}}, // VULGAR FRACTION THREE QUARTERS
    {{'?', 0, 0, 0}}, // INVERTED QUESTION MARK
    {{'A', 0, 0, 0}}, // LATIN CAPITAL LETTER A WITH GRAVE
    {{'A', 0, 0, 0}}, // LATIN CAPITAL LETTER A WITH ACUTE
    {{'A', 0, 0, 0}}, // LATIN CAPITAL LETTER A WITH CIRCUMFLEX
    {{'A', 0, 0, 0}}, // LATIN CAPITAL LETTER A WITH TILDE
    {{'A', 0, 0, 0}}, // LATIN CAPITAL LETTER A WITH DIAERESIS
    {{'A', 0, 0, 0}}, // LATIN CAPITAL LETTER A WITH RING ABOVE
    {{'A', 'E', 0, 0}}, // LATIN CAPITAL LETTER AE
    {{'C', 0, 0, 0}}, // LATIN CAPITAL LETTER C WITH CEDILLA
    {{'E', 0, 0, 0}}, // LATIN CAPITAL LETTER E WITH GRAVE
    {{'E', 0, 0, 0}}, // LATIN CAPITAL LETTER E WITH ACUTE
    {{'E', 0, 0, 0}}, // LATIN CAPITAL LETTER E WITH CIRCUMFLEX
    {{'E', 0, 0, 0}}, // LATIN CAPITAL LETTER E WITH DIAERESIS
    {{'I', 0, 0, 0}}, // LATIN CAPITAL LETTER I WITH GRAVE
    {{'I', 0, 0, 0}}, // LATIN CAPITAL LETTER I WITH ACUTE
    {{'I', 0, 0, 0}}, // LATIN CAPITAL LETTER I WITH CIRCUMFLEX
    {{'I', 0, 0, 0}}, // LATIN CAPITAL LETTER I WITH DIAERESIS
    {{'T', 'H', 0, 0}}, // LATIN CAPITAL LETTER ETH (Icelandic)
    {{'N', 0, 0, 0}}, // LATIN CAPITAL LETTER N WITH TILDE
    {{'O', 0, 0, 0}}, // LATIN CAPITAL LETTER O WITH GRAVE
    {{'O', 0, 0, 0}}, // LATIN CAPITAL LETTER O WITH ACUTE
    {{'O', 0, 0, 0}}, // LATIN CAPITAL LETTER O WITH CIRCUMFLEX
    {{'O', 0, 0, 0}}, // LATIN CAPITAL LETTER O WITH TILDE
    {{'O', 0, 0, 0}}, // LATIN CAPITAL LETTER O WITH DIAERESIS
    {{'X', 0, 0, 0}}, // MULTIPLICATION SIGN
    {{'O', 0, 0, 0}}, // LATIN CAPITAL LETTER O WITH STROKE
    {{'U', 0, 0, 0}}, // LATIN CAPITAL LETTER U WITH GRAVE
    {{'U', 0, 0, 0}}, // LATIN CAPITAL LETTER U WITH ACUTE
    {{'U', 0, 0, 0}}, // LATIN CAPITAL LETTER U WITH CIRCUMFLEX
    {{'U', 0, 0, 0}}, // LATIN CAPITAL LETTER U WITH DIAERESIS
    {{'Y', 0, 0, 0}}, // LATIN CAPITAL LETTER Y WITH ACUTE
    {{'t', 'h', 0, 0}}, // LATIN CAPITAL LETTER THORN (Icelandic)
    {{'s', 0, 0, 0}}, // LATIN SMALL LETTER SHARP S (German)
    {{'a', 0, 0, 0}}, // LATIN SMALL LETTER A WITH GRAVE
    {{'a', 0, 0, 0}}, // LATIN SMALL LETTER A WITH ACUTE
    {{'a', 0, 0, 0}}, // LATIN SMALL LETTER A WITH CIRCUMFLEX
    {{'a', 0, 0, 0}}, // LATIN SMALL LETTER A WITH TILDE
    {{'a', 0, 0, 0}}, // LATIN SMALL LETTER A WITH DIAERESIS
    {{'a', 0, 0, 0}}, // LATIN SMALL LETTER A WITH RING ABOVE
    {{'a', 'e', 0, 0}}, // LATIN SMALL LETTER AE
    {{'c', 0, 0, 0}}, // LATIN SMALL LETTER C WITH CEDILLA
    {{'e', 0, 0, 0}}, // LATIN SMALL LETTER E WITH GRAVE
    {{'e', 0, 0, 0}}, // LATIN SMALL LETTER E WITH ACUTE
    {{'e', 0, 0, 0}}, // LATIN SMALL LETTER E WITH CIRCUMFLEX
    {{'e', 0, 0, 0}}, // LATIN SMALL LETTER E WITH DIAERESIS
    {{'i', 0, 0, 0}}, // LATIN SMALL LETTER I WITH GRAVE
    {{'i', 0, 0, 0}}, // LATIN SMALL LETTER I WITH ACUTE
    {{'i', 0, 0, 0}}, // LATIN SMALL LETTER I WITH CIRCUMFLEX
    {{'i', 0, 0, 0}}, // LATIN SMALL LETTER I WITH DIAERESIS
    {{'T', 'H', 0, 0}}, // LATIN SMALL LETTER ETH (Icelandic)
    {{'n', 0, 0, 0}}, // LATIN SMALL LETTER N WITH TILDE
    {{'o', 0, 0, 0}}, // LATIN SMALL LETTER O WITH GRAVE
    {{'o', 0, 0, 0}}, // LATIN SMALL LETTER O WITH ACUTE
    {{'o', 0, 0, 0}}, // LATIN SMALL LETTER O WITH CIRCUMFLEX
    {{'o', 0, 0, 0}}, // LATIN SMALL LETTER O WITH TILDE
    {{'o', 0, 0, 0}}, // LATIN SMALL LETTER O WITH DIAERESIS
    {{'/', 0, 0, 0}}, // DIVISION SIGN
    {{'o', 0, 0, 0}}, // LATIN SMALL LETTER O WITH STROKE
    {{'u', 0, 0, 0}}, // LATIN SMALL LETTER U WITH GRAVE
    {{'u', 0, 0, 0}}, // LATIN SMALL LETTER U WITH ACUTE
    {{'u', 0, 0, 0}}, // LATIN SMALL LETTER U WITH CIRCUMFLEX
    {{'u', 0, 0, 0}}, // LATIN SMALL LETTER U WITH DIAERESIS
    {{'y', 0, 0, 0}}, // LATIN SMALL LETTER Y WITH ACUTE
    {{'t', 'h', 0, 0}}, // LATIN SMALL LETTER THORN (Icelandic)
    {{'y', 0, 0, 0}}, // LATIN SMALL LETTER Y WITH DIAERESIS
};

UInt32 CFUnicodeToLossyASCII(CFStringBuffer *chars, UInt8 *ch) {
    UniChar curChar = CFStringBufferCurrentCharacter(chars);

    CFStringBufferIncrementLocation(chars);

    if (curChar < 0x80) {
        *ch = (UInt8)curChar;
        return 1;
    } else if (curChar > 0x9F && curChar <= 0xFF) {
        const char *losChars = (const unsigned char*)_toLossyASCIITable + (curChar - 0xA0) * sizeof(unsigned char[4]);

        if (losChars[0]) {ch[0] = losChars[0]; return 1;}
        if (losChars[1]) {ch[1] = losChars[1]; return 2;}
        if (losChars[2]) {ch[2] = losChars[2]; return 3;}
        if (losChars[3]) {ch[3] = losChars[3]; return 4;}
    }

    return 0;
}
