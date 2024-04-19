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
/*	CFStringEncodingConverter.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Aki Inoue
*/

#include "CFInternal.h"
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include "CFStringEncodingConverterExt.h"
#include "CFStringEncodingConverterPriv.h"
#include <stdlib.h>
#if !defined(__MACOS8__)
#ifdef __WIN32__
#include <windows.h>
#else // Mach, HP-UX, Solaris
#include <pthread.h>
#endif
#endif __MACOS8__


/* Macros
*/
#define TO_BYTE(conv,flags,chars,numChars,bytes,max,used) (conv->_toBytes ? conv->toBytes(conv,flags,chars,numChars,bytes,max,used) : ((CFStringEncodingToBytesProc)conv->toBytes)(flags,chars,numChars,bytes,max,used))
#define TO_UNICODE(conv,flags,bytes,numBytes,chars,max,used) (conv->_toUnicode ?  (flags & kCFStringEncodingUseCanonical ? conv->toCanonicalUnicode(conv,flags,bytes,numBytes,chars,max,used) : conv->toUnicode(conv,flags,bytes,numBytes,chars,max,used)) : ((CFStringEncodingToUnicodeProc)conv->toUnicode)(flags,bytes,numBytes,chars,max,used))

static UInt32 __CFDefaultToBytesFallbackProc(const UniChar *characters, UInt32 numChars, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen) {
    if (maxByteLen) *bytes = '?';
    *usedByteLen = 1;
    return 1;
}

static UInt32 __CFDefaultToUnicodeFallbackProc(const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    if (maxCharLen) *characters = (UniChar)'?';
    *usedCharLen = 1;
    return 1;
}

#define TO_BYTE_FALLBACK(conv,chars,numChars,bytes,max,used) (conv->toBytesFallback(chars,numChars,bytes,max,used))
#define TO_UNICODE_FALLBACK(conv,bytes,numBytes,chars,max,used) (conv->toUnicodeFallback(bytes,numBytes,chars,max,used))

#define EXTRA_BASE (0x0F00)

/* Wrapper funcs for non-standard converters
*/
static UInt32 __CFToBytesCheapEightBitWrapper(const void *converter, UInt32 flags, const UniChar *characters, UInt32 numChars, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen) {
    UInt32 processedCharLen = 0;
    UInt32 length = (maxByteLen && (maxByteLen < numChars) ? maxByteLen : numChars);
    UInt8 byte;

    while (processedCharLen < length) {
        if (!((CFStringEncodingCheapEightBitToBytesProc)((const _CFEncodingConverter*)converter)->_toBytes)(flags, characters[processedCharLen], &byte)) break;

        if (maxByteLen) bytes[processedCharLen] = byte;
        processedCharLen++;
    }

    *usedByteLen = processedCharLen;
    return processedCharLen;
}

static UInt32 __CFToUnicodeCheapEightBitWrapper(const void *converter, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    UInt32 processedByteLen = 0;
    UInt32 length = (maxCharLen && (maxCharLen < numBytes) ? maxCharLen : numBytes);
    UniChar character;

    while (processedByteLen < length) {
        if (!((CFStringEncodingCheapEightBitToUnicodeProc)((const _CFEncodingConverter*)converter)->_toUnicode)(flags, bytes[processedByteLen], &character)) break;

        if (maxCharLen) characters[processedByteLen] = character;
        processedByteLen++;
    }

    *usedCharLen = processedByteLen;
    return processedByteLen;
}

static UInt32 __CFToCanonicalUnicodeCheapEightBitWrapper(const void *converter, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    UInt32 processedByteLen = 0;
    UInt32 theUsedCharLen = 0;
    UniChar charBuffer[MAX_DECOMPOSED_LENGTH];
    UInt32 usedLen;
    UniChar character;

    while ((processedByteLen < numBytes) && (!maxCharLen || (theUsedCharLen < maxCharLen))) {
        if (!((CFStringEncodingCheapEightBitToUnicodeProc)((const _CFEncodingConverter*)converter)->_toUnicode)(flags, bytes[processedByteLen], &character)) break;

        if (!(usedLen = CFStringEncodingDecomposeCharacter(flags, character, charBuffer))) {
            usedLen = 1;
            *charBuffer = character;
        }
        if (maxCharLen) {
            UInt16 index;

            if (theUsedCharLen + usedLen > maxCharLen) break;

            for (index = 0;index < usedLen;index++) {
                characters[theUsedCharLen + index] = charBuffer[index];
            }
        }
        theUsedCharLen += usedLen;
        processedByteLen++;
    }

    *usedCharLen = theUsedCharLen;
    return processedByteLen;
}

static UInt32 __CFToBytesStandardEightBitWrapper(const void *converter, UInt32 flags, const UniChar *characters, UInt32 numChars, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen) {
    UInt32 processedCharLen = 0;
    UInt8 byte;
    UInt32 usedLen;

    *usedByteLen = 0;

    while (numChars && (!maxByteLen || (*usedByteLen < maxByteLen))) {
        if (!(usedLen = ((CFStringEncodingStandardEightBitToBytesProc)((const _CFEncodingConverter*)converter)->_toBytes)(flags, characters, numChars, &byte))) break;

        if (maxByteLen) bytes[*usedByteLen] = byte;
        (*usedByteLen)++;
        characters += usedLen;
        numChars -= usedLen;
        processedCharLen += usedLen;
    }

    return processedCharLen;
}

static UInt32 __CFToUnicodeStandardEightBitWrapper(const void *converter, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    UInt32 processedByteLen = 0;
#if defined(__MACOS8__)
    UniChar charBuffer[20]; // Dynamic stack allocation is GNU specific
#else
    UniChar charBuffer[((const _CFEncodingConverter*)converter)->maxLen];
#endif
    UInt32 usedLen;

    *usedCharLen = 0;

    while ((processedByteLen < numBytes) && (!maxCharLen || (*usedCharLen < maxCharLen))) {
        if (!(usedLen = ((CFStringEncodingCheapEightBitToUnicodeProc)((const _CFEncodingConverter*)converter)->_toUnicode)(flags, bytes[processedByteLen], charBuffer))) break;

        if (maxCharLen) {
            UInt16 index;

            if (*usedCharLen + usedLen > maxCharLen) break;

            for (index = 0;index < usedLen;index++) {
                characters[*usedCharLen + index] = charBuffer[index];
            }
        }
        *usedCharLen += usedLen;
        processedByteLen++;
    }

    return processedByteLen;
}

static UInt32 __CFToCanonicalUnicodeStandardEightBitWrapper(const void *converter, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    UInt32 processedByteLen = 0;
#if defined(__MACOS8__)
    UniChar charBuffer[20]; // Dynamic stack allocation is GNU specific
#else
    UniChar charBuffer[((const _CFEncodingConverter*)converter)->maxLen];
#endif
    UniChar decompBuffer[MAX_DECOMPOSED_LENGTH];
    UInt32 usedLen;
    UInt32 decompedLen;
    UInt32 index;

    *usedCharLen = 0;

    while ((processedByteLen < numBytes) && (!maxCharLen || (*usedCharLen < maxCharLen))) {
        if (!(usedLen = ((CFStringEncodingCheapEightBitToUnicodeProc)((const _CFEncodingConverter*)converter)->_toUnicode)(flags, bytes[processedByteLen], charBuffer))) break;

        for (index = 0;index < usedLen;index++) {
            if (!(decompedLen = CFStringEncodingDecomposeCharacter(flags, charBuffer[index], decompBuffer))) {
                decompedLen = 1;
                *decompBuffer = charBuffer[index];
            }

            *usedCharLen += decompedLen;
            if (maxCharLen) {
                UInt32 index2;

                if (*usedCharLen > maxCharLen) break;
                for (index2 = 0;index2 < decompedLen;index2++) *characters++ = decompBuffer[index2];
            }
        }
        processedByteLen++;
    }

    return processedByteLen;
}

static UInt32 __CFToBytesCheapMultiByteWrapper(const void *converter, UInt32 flags, const UniChar *characters, UInt32 numChars, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen) {
    UInt32 processedCharLen = 0;
#if defined(__MACOS8__)
    UniChar byteBuffer[20]; // Dynamic stack allocation is GNU specific
#else
    UniChar byteBuffer[((const _CFEncodingConverter*)converter)->maxLen];
#endif
    UInt32 usedLen;

    *usedByteLen = 0;

    while ((processedCharLen < numChars) && (!maxByteLen || (*usedByteLen < maxByteLen))) {
        if (!(usedLen = ((CFStringEncodingCheapMultiByteToBytesProc)((const _CFEncodingConverter*)converter)->_toBytes)(flags, characters[processedCharLen], (UInt8 *)byteBuffer))) break;

        if (maxByteLen) {
            UInt16 index;

            if (*usedByteLen + usedLen > maxByteLen) break;

            for (index = 0;index <usedLen;index++) {
                bytes[*usedByteLen + index] = byteBuffer[index];
            }
        }

        *usedByteLen += usedLen;
        processedCharLen++;
    }

    return processedCharLen;
}

static UInt32 __CFToUnicodeCheapMultiByteWrapper(const void *converter, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    UInt32 processedByteLen = 0;
    UniChar character;
    UInt32 usedLen;

    *usedCharLen = 0;

    while (numBytes && (!maxCharLen || (*usedCharLen < maxCharLen))) {
        if (!(usedLen = ((CFStringEncodingCheapMultiByteToUnicodeProc)((const _CFEncodingConverter*)converter)->_toUnicode)(flags, bytes, numBytes, &character))) break;

        if (maxCharLen) *(characters++) = character;
        (*usedCharLen)++;
        processedByteLen += usedLen;
        bytes += usedLen;
        numBytes -= usedLen;
    }

    return processedByteLen;
}

static UInt32 __CFToCanonicalUnicodeCheapMultiByteWrapper(const void *converter, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    UInt32 processedByteLen = 0;
    UniChar charBuffer[MAX_DECOMPOSED_LENGTH];
    UniChar character;
    UInt32 usedLen;
    UInt32 decomposedLen;
    UInt32 theUsedCharLen = 0;

    while (numBytes && (!maxCharLen || (theUsedCharLen < maxCharLen))) {
        if (!(usedLen = ((CFStringEncodingCheapMultiByteToUnicodeProc)((const _CFEncodingConverter*)converter)->_toUnicode)(flags, bytes, numBytes, &character))) break;

        if (!(decomposedLen = CFStringEncodingDecomposeCharacter(flags, character, charBuffer))) {
            decomposedLen = 1;
            *charBuffer = character;
        }
        if (maxCharLen) {
            UInt32 index;

            if (theUsedCharLen + decomposedLen > maxCharLen) break;

            for (index = 0;index < decomposedLen;index++) {
                characters[theUsedCharLen + index] = charBuffer[index];
            }
        }
        theUsedCharLen += decomposedLen;
        processedByteLen += usedLen;
        bytes += usedLen;
        numBytes -= usedLen;
    }
    *usedCharLen = theUsedCharLen;
    return processedByteLen;
}

/* static functions
*/
static _CFConverterEntry __CFConverterEntryASCII = {
    kCFStringEncodingASCII, NULL,
    "Western (ASCII)", {"us-ascii", "ascii", "iso-646-us", NULL}, NULL, NULL, NULL, NULL,
    kCFStringEncodingMacRoman // We use string encoding's script range here
};

static _CFConverterEntry __CFConverterEntryISOLatin1 = {
    kCFStringEncodingISOLatin1, NULL,
    "Western (ISO Latin 1)", {"iso-8859-1", "latin1","iso-latin-1", NULL}, NULL, NULL, NULL, NULL,
    kCFStringEncodingMacRoman // We use string encoding's script range here
};

static _CFConverterEntry __CFConverterEntryMacRoman = {
    kCFStringEncodingMacRoman, NULL,
    "Western (Mac OS Roman)", {"macintosh", "mac", "x-mac-roman", NULL}, NULL, NULL, NULL, NULL,
    kCFStringEncodingMacRoman // We use string encoding's script range here
};

static _CFConverterEntry __CFConverterEntryWinLatin1 = {
    kCFStringEncodingWindowsLatin1, NULL,
    "Western (Windows Latin 1)", {"windows-1252", "cp1252", "windows latin1", NULL}, NULL, NULL, NULL, NULL,
    kCFStringEncodingMacRoman // We use string encoding's script range here
};

static _CFConverterEntry __CFConverterEntryNextStepLatin = {
    kCFStringEncodingNextStepLatin, NULL,
    "Western (NextStep)", {"x-nextstep", NULL, NULL, NULL}, NULL, NULL, NULL, NULL,
    kCFStringEncodingMacRoman // We use string encoding's script range here
};

static _CFConverterEntry __CFConverterEntryUTF8 = {
    kCFStringEncodingUTF8, NULL,
    "UTF-8", {"utf-8", "unicode-1-1-utf8", NULL, NULL}, NULL, NULL, NULL, NULL,
    kCFStringEncodingUnicode // We use string encoding's script range here
};

CF_INLINE _CFConverterEntry *__CFStringEncodingConverterGetEntry(UInt32 encoding) {
    switch (encoding) {
        case kCFStringEncodingInvalidId:
        case kCFStringEncodingASCII:
            return &__CFConverterEntryASCII;

        case kCFStringEncodingISOLatin1:
            return &__CFConverterEntryISOLatin1;

        case kCFStringEncodingMacRoman:
            return &__CFConverterEntryMacRoman;

        case kCFStringEncodingWindowsLatin1:
            return &__CFConverterEntryWinLatin1;

        case kCFStringEncodingNextStepLatin:
            return &__CFConverterEntryNextStepLatin;

        case kCFStringEncodingUTF8:
            return &__CFConverterEntryUTF8;

        default: return NULL;
    }
}

CF_INLINE _CFEncodingConverter *__CFEncodingConverterFromDefinition(const CFStringEncodingConverter *definition) {
#define NUM_OF_ENTRIES_CYCLE (10)
    static UInt32 _indexLock = 0;
    static UInt32 _currentIndex = 0;
    static UInt32 _allocatedSize = 0;
    static _CFEncodingConverter *_allocatedEntries = NULL;
    _CFEncodingConverter *converter;


    if (_allocatedEntries == NULL) { // Not allocated yet
        // There could be leak here if multiple threads enter here, but it's harmless otherwise
        _allocatedEntries = (_CFEncodingConverter *)CFAllocatorAllocate(NULL, sizeof(_CFEncodingConverter) * NUM_OF_ENTRIES_CYCLE, 0);
        _allocatedSize = NUM_OF_ENTRIES_CYCLE;
        converter = &(_allocatedEntries[_currentIndex]);
    } else {
        __CFSpinLock(&_indexLock);
        if ((_currentIndex + 1) >= _allocatedSize) {
            _allocatedSize += NUM_OF_ENTRIES_CYCLE;
            _allocatedEntries = (_CFEncodingConverter*)CFAllocatorReallocate(NULL, (void*)_allocatedEntries, sizeof(_CFEncodingConverter) * _allocatedSize, 0);
        }

        converter = &(_allocatedEntries[++_currentIndex]);
        __CFSpinUnlock(&_indexLock);
    }

    switch (definition->encodingClass) {
        case kCFStringEncodingConverterStandard:
            converter->toBytes = definition->toBytes;
            converter->toUnicode = definition->toUnicode;
            converter->toCanonicalUnicode = definition->toUnicode;
            converter->_toBytes = NULL;
            converter->_toUnicode = NULL;
            converter->maxLen = 2;
            break;

        case kCFStringEncodingConverterCheapEightBit:
            converter->toBytes = __CFToBytesCheapEightBitWrapper;
            converter->toUnicode = __CFToUnicodeCheapEightBitWrapper;
            converter->toCanonicalUnicode = __CFToCanonicalUnicodeCheapEightBitWrapper;
            converter->_toBytes = definition->toBytes;
            converter->_toUnicode = definition->toUnicode;
            converter->maxLen = 1;
            break;

        case kCFStringEncodingConverterStandardEightBit:
            converter->toBytes = __CFToBytesStandardEightBitWrapper;
            converter->toUnicode = __CFToUnicodeStandardEightBitWrapper;
            converter->toCanonicalUnicode = __CFToCanonicalUnicodeStandardEightBitWrapper;
            converter->_toBytes = definition->toBytes;
            converter->_toUnicode = definition->toUnicode;
            converter->maxLen = definition->maxDecomposedCharLen;
            break;

        case kCFStringEncodingConverterCheapMultiByte:
            converter->toBytes = __CFToBytesCheapMultiByteWrapper;
            converter->toUnicode = __CFToUnicodeCheapMultiByteWrapper;
            converter->toCanonicalUnicode = __CFToCanonicalUnicodeCheapMultiByteWrapper;
            converter->_toBytes = definition->toBytes;
            converter->_toUnicode = definition->toUnicode;
            converter->maxLen = definition->maxBytesPerChar;
            break;

        case kCFStringEncodingConverterPlatformSpecific:
            converter->toBytes = NULL;
            converter->toUnicode = NULL;
            converter->toCanonicalUnicode = NULL;
            converter->_toBytes = NULL;
            converter->_toUnicode = NULL;
            converter->maxLen = 0;
            converter->toBytesLen = NULL;
            converter->toUnicodeLen = NULL;
            converter->toBytesFallback = NULL;
            converter->toUnicodeFallback = NULL;
            converter->toBytesPrecompose = NULL;
            converter->isValidCombiningChar = NULL;
            return converter;
            
        default: // Shouln't be here
            return NULL;
    }

    converter->toBytesLen = (definition->toBytesLen ? definition->toBytesLen : (CFStringEncodingToBytesLenProc)(UInt32)definition->maxBytesPerChar);
    converter->toUnicodeLen = (definition->toUnicodeLen ? definition->toUnicodeLen : (CFStringEncodingToUnicodeLenProc)(UInt32)definition->maxDecomposedCharLen);
    converter->toBytesFallback = (definition->toBytesFallback ? definition->toBytesFallback : __CFDefaultToBytesFallbackProc);
    converter->toUnicodeFallback = (definition->toUnicodeFallback ? definition->toUnicodeFallback : __CFDefaultToUnicodeFallbackProc);
    converter->toBytesPrecompose = (definition->toBytesPrecompose ? definition->toBytesPrecompose : NULL);
    converter->isValidCombiningChar = (definition->isValidCombiningChar ? definition->isValidCombiningChar : NULL);

    return converter;
}

CF_INLINE const CFStringEncodingConverter *__CFStringEncodingConverterGetDefinition(_CFConverterEntry *entry) {
    if (!entry) return NULL;
    
    switch (entry->encoding) {
        case kCFStringEncodingASCII:
            return &__CFConverterASCII;

        case kCFStringEncodingISOLatin1:
            return &__CFConverterISOLatin1;

        case kCFStringEncodingMacRoman:
            return &__CFConverterMacRoman;

        case kCFStringEncodingWindowsLatin1:
            return &__CFConverterWinLatin1;

        case kCFStringEncodingNextStepLatin:
            return &__CFConverterNextStepLatin;

        case kCFStringEncodingUTF8:
            return &__CFConverterUTF8;

        default:
            return NULL;
    }
}

CF_INLINE const _CFEncodingConverter *__CFGetConverter(UInt32 encoding) {
    _CFConverterEntry *entry = __CFStringEncodingConverterGetEntry(encoding);

    if (!entry) return NULL;

    if (!entry->converter) {
        const CFStringEncodingConverter *definition = __CFStringEncodingConverterGetDefinition(entry);

        if (definition) {
            entry->converter = __CFEncodingConverterFromDefinition(definition);
            entry->toBytesFallback = definition->toBytesFallback;
            entry->toUnicodeFallback = definition->toUnicodeFallback;
        }
    }

    return (_CFEncodingConverter *)entry->converter;
}

/* Public API
*/
UInt32 CFStringEncodingUnicodeToBytes(UInt32 encoding, UInt32 flags, const UniChar *characters, UInt32 numChars, UInt32 *usedCharLen, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen) {
    const _CFEncodingConverter *converter = __CFGetConverter(encoding);
    UInt32 usedLen = 0;
    UInt32 localUsedByteLen;
    UInt32 theUsedByteLen = 0;
    UInt32 theResult = kCFStringEncodingConversionSuccess;
    CFStringEncodingToBytesPrecomposeProc toBytesPrecompose = NULL;
    CFStringEncodingIsValidCombiningCharacterProc isValidCombiningChar = NULL;

    if (!converter) return kCFStringEncodingConverterUnavailable;

    if (flags & kCFStringEncodingSubstituteCombinings) {
        if (!(flags & kCFStringEncodingAllowLossyConversion)) isValidCombiningChar = converter->isValidCombiningChar;
   } else {
        isValidCombiningChar = converter->isValidCombiningChar;
        if (!(flags & kCFStringEncodingIgnoreCombinings)) {
            toBytesPrecompose = converter->toBytesPrecompose;
            flags |= kCFStringEncodingComposeCombinings;
        }
    }


    while ((usedLen < numChars) && (!maxByteLen || (theUsedByteLen < maxByteLen))) {
        if ((usedLen += TO_BYTE(converter, flags, characters + usedLen, numChars - usedLen, bytes + theUsedByteLen, (maxByteLen ? maxByteLen - theUsedByteLen : 0), &localUsedByteLen)) < numChars) {
            UInt32 dummy;
            if (maxByteLen && ((maxByteLen == theUsedByteLen + localUsedByteLen) || TO_BYTE(converter, flags, characters + usedLen, numChars - usedLen, NULL, 0, &dummy))) { // buffer was filled up
                theUsedByteLen += localUsedByteLen;
                theResult = kCFStringEncodingInsufficientOutputBufferLength;
                break;
            } else if (isValidCombiningChar && (usedLen > 0) && isValidCombiningChar(characters[usedLen])) {
                if (toBytesPrecompose) {
                    UInt32 localUsedLen = usedLen;

                    while (isValidCombiningChar(characters[--usedLen]));
                    theUsedByteLen += localUsedByteLen;
                    if (converter->maxLen > 1) {
                        TO_BYTE(converter, flags, characters + usedLen, localUsedLen - usedLen, NULL, 0, &localUsedByteLen);
                        theUsedByteLen -= localUsedByteLen;
                    } else {
                        theUsedByteLen--;
                    }
                    if ((localUsedLen = toBytesPrecompose(flags, characters + usedLen, numChars - usedLen, bytes + theUsedByteLen, (maxByteLen ? maxByteLen - theUsedByteLen : 0), &localUsedByteLen)) > 0) {
                        usedLen += localUsedLen;
                    } else if (flags & kCFStringEncodingAllowLossyConversion) {
                        usedLen++;
                        usedLen += TO_BYTE_FALLBACK(converter, characters + usedLen, numChars - usedLen, bytes + theUsedByteLen, (maxByteLen ? maxByteLen - theUsedByteLen : 0), &localUsedByteLen);
                    } else {
                        theResult = kCFStringEncodingInvalidInputStream;
                        break;
                    }
                } else if (flags & kCFStringEncodingIgnoreCombinings) {
                    while ((++usedLen < numChars) && isValidCombiningChar(characters[usedLen]));
                } else {
                    theUsedByteLen += localUsedByteLen;
                    usedLen += TO_BYTE_FALLBACK(converter, characters + usedLen, numChars - usedLen, bytes + theUsedByteLen, (maxByteLen ? maxByteLen - theUsedByteLen : 0), &localUsedByteLen);
                }
            } else if (flags & kCFStringEncodingAllowLossyConversion) {
                theUsedByteLen += localUsedByteLen;
                usedLen += TO_BYTE_FALLBACK(converter, characters + usedLen, numChars - usedLen, bytes + theUsedByteLen, (maxByteLen ? maxByteLen - theUsedByteLen : 0), &localUsedByteLen);
            } else {
                theUsedByteLen += localUsedByteLen;
                theResult = kCFStringEncodingInvalidInputStream;
                break;
            }
        }
        theUsedByteLen += localUsedByteLen;
    }

    if (usedByteLen) *usedByteLen = theUsedByteLen;
    if (usedCharLen) *usedCharLen = usedLen;

    return theResult;
}

UInt32 CFStringEncodingBytesToUnicode(UInt32 encoding, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UInt32 *usedByteLen, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen) {
    const _CFEncodingConverter *converter = __CFGetConverter(encoding);
    UInt32 usedLen = 0;
    UInt32 theUsedCharLen = 0;
    UInt32 localUsedCharLen;
    UInt32 theResult = kCFStringEncodingConversionSuccess;

    if (!converter) return kCFStringEncodingConverterUnavailable;


    while ((usedLen < numBytes) && (!maxCharLen || (theUsedCharLen < maxCharLen))) {
        if ((usedLen += TO_UNICODE(converter, flags, bytes + usedLen, numBytes - usedLen, characters + theUsedCharLen, (maxCharLen ? maxCharLen - theUsedCharLen : 0), &localUsedCharLen)) < numBytes) {
            if (maxCharLen && ((maxCharLen == theUsedCharLen + localUsedCharLen) || ((flags & (kCFStringEncodingUseCanonical|kCFStringEncodingUseHFSPlusCanonical)) && TO_UNICODE(converter, flags, bytes + usedLen, numBytes - usedLen, NULL, 0, NULL)))) { // buffer was filled up
                theUsedCharLen += localUsedCharLen;
                theResult = kCFStringEncodingInsufficientOutputBufferLength;
                break;
            } else if (flags & kCFStringEncodingAllowLossyConversion) {
                theUsedCharLen += localUsedCharLen;
                usedLen += TO_UNICODE_FALLBACK(converter, bytes + usedLen, numBytes - usedLen, characters + theUsedCharLen, (maxCharLen ? maxCharLen - theUsedCharLen : 0), &localUsedCharLen);
            } else {
                theUsedCharLen += localUsedCharLen;
                theResult = kCFStringEncodingInvalidInputStream;
                break;
            }
        }
        theUsedCharLen += localUsedCharLen;
    }

    if (usedCharLen) *usedCharLen = theUsedCharLen;
    if (usedByteLen) *usedByteLen = usedLen;

    return theResult;
}

Boolean CFStringEncodingIsValidEncoding(UInt32 encoding) {
    return (CFStringEncodingGetConverter(encoding) ? TRUE : FALSE);
}

const char *CFStringEncodingName(UInt32 encoding) {
    _CFConverterEntry *entry = __CFStringEncodingConverterGetEntry(encoding);
    if (entry) return entry->encodingName;
    return NULL;
}

const char **CFStringEncodingCanonicalCharsetNames(UInt32 encoding) {
    _CFConverterEntry *entry = __CFStringEncodingConverterGetEntry(encoding);
    if (entry) return entry->ianaNames;
    return NULL;
}

UInt32 CFStringEncodingGetScriptCodeForEncoding(CFStringEncoding encoding) {
    _CFConverterEntry *entry = __CFStringEncodingConverterGetEntry(encoding);
    if (entry) return entry->scriptCode;

    return (entry ? entry->scriptCode : (encoding == kCFStringEncodingUnicode ? kCFStringEncodingUnicode : (encoding < 0xFF ? encoding : kCFStringEncodingInvalidId)));
}

UInt32 CFStringEncodingCharLengthForBytes(UInt32 encoding, UInt32 flags, const UInt8 *bytes, UInt32 numBytes) {
    const _CFEncodingConverter *converter = __CFGetConverter(encoding);

    if (converter) {
        UInt32 switchVal = (UInt32)(converter->toUnicodeLen);

            if (switchVal < 0xFFFF)
            return switchVal * numBytes;
        else
            return converter->toUnicodeLen(flags, bytes, numBytes);
    }

    return 0;
}

UInt32 CFStringEncodingByteLengthForCharacters(UInt32 encoding, UInt32 flags, const UniChar *characters, UInt32 numChars) {
    const _CFEncodingConverter *converter = __CFGetConverter(encoding);

    if (converter) {
        UInt32 switchVal = (UInt32)(converter->toBytesLen);

            if (switchVal < 0xFFFF)
            return switchVal * numChars;
        else
            return converter->toBytesLen(flags, characters, numChars);
    }

    return 0;
}

void CFStringEncodingRegisterFallbackProcedures(UInt32 encoding, CFStringEncodingToBytesFallbackProc toBytes, CFStringEncodingToUnicodeFallbackProc toUnicode) {
    _CFConverterEntry *entry = __CFStringEncodingConverterGetEntry(encoding);

    if (entry && __CFGetConverter(encoding)) {
        ((_CFEncodingConverter*)entry->converter)->toBytesFallback = (toBytes ? toBytes : entry->toBytesFallback);
        ((_CFEncodingConverter*)entry->converter)->toUnicodeFallback = (toUnicode ? toUnicode : entry->toUnicodeFallback);
    }
}

const CFStringEncodingConverter *CFStringEncodingGetConverter(UInt32 encoding) {
    return __CFStringEncodingConverterGetDefinition(__CFStringEncodingConverterGetEntry(encoding));
}

static const UInt32 __CFBuiltinEncodings[] = {
    kCFStringEncodingMacRoman,
    kCFStringEncodingWindowsLatin1,
    kCFStringEncodingISOLatin1,
    kCFStringEncodingNextStepLatin,
    kCFStringEncodingASCII,
    kCFStringEncodingUTF8,
    /* These two are available only in CFString-level */
    kCFStringEncodingUnicode,
    kCFStringEncodingNonLossyASCII,
    kCFStringEncodingInvalidId,
};


const UInt32 *CFStringEncodingListOfAvailableEncodings(void) {
    return __CFBuiltinEncodings;
}

