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
/*	CFURL.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Becky Willrich
*/

#include <CoreFoundation/CFURL.h>
#include "CFPriv.h"
#include "CFVeryPrivate.h"
#include "CFInternal.h"
#include "CFStringEncodingConverter.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__MACH__)
#include <unistd.h>
#endif

static CFStringRef HFSPathToURLPath(CFStringRef path, CFAllocatorRef alloc, Boolean isDir);
static CFStringRef URLPathToHFSPath(CFStringRef path, CFAllocatorRef allocator);

 
#if defined(__MACH__)
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined(__MACOS8__)
#include <string.h>
#include <Files.h>
#endif

#define DEBUG_URL_MEMORY_USAGE 0

/* The bit flags in myURL->_flags */
#define HAS_CUSTOM_ALLOCATOR        (0x80000000)
/* POSIX_AND_URL_PATHS_MATCH will only be true if the URL and POSIX paths are identical, character for character, except for the presence/absence of a trailing slash on directories */
#define POSIX_AND_URL_PATHS_MATCH   (0x00100000)
#define PATH_TYPE_MASK              (0x000F0000)

#define IS_DECOMPOSABLE (0x8000)
#define IS_ABSOLUTE     (0x4000)
#define IS_PARSED       (0x2000)
#define IS_DIRECTORY    (0x1000)

#define HAS_SCHEME      (0x0001)
#define HAS_USER        (0x0002)
#define HAS_PASSWORD    (0x0004)
#define HAS_HOST        (0x0008)
#define HAS_PORT        (0x0010)
#define HAS_PATH        (0x0020)
#define HAS_PARAMETERS  (0x0040)
#define HAS_QUERY       (0x0080)
#define HAS_FRAGMENT    (0x0100)

// Other useful defines
#define NET_LOCATION_MASK (HAS_HOST | HAS_USER | HAS_PASSWORD | HAS_PORT)
#define RESOURCE_SPECIFIER_MASK  (HAS_PARAMETERS | HAS_QUERY | HAS_FRAGMENT)
#define FULL_URL_REPRESENTATION (0xF)

/* URL_PATH_TYPE(anURL) will be one of the CFURLPathStyle constants, in which case string is a file system path, or will be FULL_URL_REPRESENTATION, in which case the string is the full URL string. One caveat - string always has a trailing path delimiter if the url is a directory URL.  This must be stripped before returning file system representations!  */
#define URL_PATH_TYPE(url) (((url->_flags) & PATH_TYPE_MASK) >> 16)
#define PATH_DELIM_FOR_TYPE(fsType) ((fsType) == kCFURLHFSPathStyle ? ':' : (((fsType) == kCFURLWindowsPathStyle) ? '\\' : '/'))
#define PATH_DELIM_AS_STRING_FOR_TYPE(fsType) ((fsType) == kCFURLHFSPathStyle ? CFSTR(":") : (((fsType) == kCFURLWindowsPathStyle) ? CFSTR("\\") : CFSTR("/")))

struct __CFURL {
    CFRuntimeBase _cfBase;
    UInt32 _flags;
    CFStringRef  _string; // Never NULL; the meaning of _string depends on URL_PATH_TYPE(myURL) (see above)
    CFURLRef  _base;       // NULL for absolute URLs; if present, _base is guaranteed to itself be absolute.
    CFRange *ranges;
    void *_reserved; // Reserved for URLHandle's use.
};
/* Note that an allocator may be tacked on at the end of this structure */

static void _convertToURLRepresentation(struct __CFURL *url);
static CFURLRef _CFURLCopyAbsoluteFileURL(CFURLRef relativeURL);
static CFStringRef _resolveFileSystemPaths(CFStringRef relativePath, CFStringRef basePath, Boolean baseIsDir, CFURLPathStyle fsType, CFAllocatorRef alloc);
static void _parseComponents(struct __CFURL *anURL);
static CFRange _rangeForComponent(CFURLRef url, UInt32 compFlag);

#define scheme_valid(c) (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '.') || (c == '-') || (c == '+'))

// Returns FALSE if ch1 or ch2 isn't properly formatted
CF_INLINE Boolean _translateBytes(UniChar ch1, UniChar ch2, UInt8 *result) {
    *result = 0;
    if (ch1 >= '0' && ch1 <= '9') *result += (ch1 - '0');
    else if (ch1 >= 'a' && ch1 <= 'f') *result += 10 + ch1 - 'a';
    else if (ch1 >= 'A' && ch1 <= 'F') *result += 10 + ch1 - 'A';
    else return FALSE;

    *result  = (*result) << 4;
    if (ch2 >= '0' && ch2 <= '9') *result += (ch2 - '0');
    else if (ch2 >= 'a' && ch2 <= 'f') *result += 10 + ch2 - 'a';
    else if (ch2 >= 'A' && ch2 <= 'F') *result += 10 + ch2 - 'A';
    else return FALSE;

    return TRUE;
}

static Boolean _appendPercentEscapesForCharacter(UniChar ch, CFStringEncoding encoding, CFMutableStringRef str) {
    UInt8 bytes[6]; // 6 bytes is the maximum a single character could require in UTF8 (most common case); other encodings could require more
    UInt8 *bytePtr = bytes, *currByte;
    UInt32 byteLength;
    CFAllocatorRef alloc = NULL;
    if (CFStringEncodingUnicodeToBytes(encoding, 0, &ch, 1, NULL, bytePtr, 6, &byteLength) != kCFStringEncodingConversionSuccess) {
        byteLength = CFStringEncodingByteLengthForCharacters(encoding, 0, &ch, 1);
        if (byteLength <= 6) {
            // The encoding cannot accomodate the character
            return FALSE;
        }
        alloc = CFGetAllocator(str);
        bytePtr = CFAllocatorAllocate(alloc, byteLength, 0);
        if (!bytePtr || CFStringEncodingUnicodeToBytes(encoding, 0, &ch, 1, NULL, bytePtr, byteLength, &byteLength) != 1) {
            if (bytePtr) CFAllocatorDeallocate(alloc, bytePtr);
            return FALSE;
        }
    }
    for (currByte = bytePtr; currByte < bytePtr + byteLength; currByte ++) {
        UniChar escapeSequence[3] = {'%', '\0', '\0'};
        unsigned char high, low;
        high = ((*currByte) & 0xf0) >> 4;
        low = (*currByte) & 0x0f;
        escapeSequence[1] = (high < 10) ? '0' + high : 'A' + high - 10;
        escapeSequence[2] = (low < 10) ? '0' + low : 'A' + low - 10;
        CFStringAppendCharacters(str, escapeSequence, 3);
    }
    if (bytePtr != bytes) {
        CFAllocatorDeallocate(alloc, bytePtr);
    }
    return TRUE;
}

// Uses UTF-8 to translate all percent escape sequences; returns NULL if it encounters a format failure.  May return the original string.
CFStringRef  CFURLCreateStringByReplacingPercentEscapes(CFAllocatorRef alloc, CFStringRef  originalString, CFStringRef  charactersToLeaveEscaped) {
    CFMutableStringRef newStr = NULL;
    CFIndex length;
    CFIndex mark = 0;
    CFRange percentRange, searchRange;
    CFMutableStringRef escapedStr = NULL;
    UniChar escapedChar;
    Boolean escapeAll = (charactersToLeaveEscaped && CFStringGetLength(charactersToLeaveEscaped) == 0);
    Boolean failed = FALSE;
    
    if (!originalString) return NULL;

    if (charactersToLeaveEscaped == NULL) {
        CFRetain(originalString);
        return originalString;
    }

    length = CFStringGetLength(originalString);
    searchRange = CFRangeMake(0, length);

    while (!failed && CFStringFindWithOptions(originalString, CFSTR("%"), searchRange, 0, &percentRange)) {
        UInt8 bytes[4]; // Single UTF-8 character could require up to 4 bytes.
        UInt8 numBytesExpected;
        UniChar ch1, ch2;

        // Make sure we have at least 2 more characters
        if (length - percentRange.location < 3) { failed = TRUE; break; }

        // if we don't have at least 2 more characters, we can't interpret the percent escape code,
        // so we assume the percent character is legit, and let it pass into the string
        ch1 = CFStringGetCharacterAtIndex(originalString, percentRange.location+1);
        ch2 = CFStringGetCharacterAtIndex(originalString, percentRange.location+2);
        if (!_translateBytes(ch1, ch2, bytes)) { failed = TRUE;  break; }
        if (!(bytes[0] & 0x80)) {
            numBytesExpected = 1;
        } else if (!(bytes[0] & 0x20)) {
            numBytesExpected = 2;
        } else if (!(bytes[0] & 0x10)) {
            numBytesExpected = 3;
        } else {
            numBytesExpected = 4;
        }
        if (numBytesExpected == 1) {
            // one byte sequence (most common case); handle this specially
            escapedChar = bytes[0];
        } else {
            CFIndex j;
            CFStringRef  smallString;
            // Make sure up front that we have enough characters
            if (length <= percentRange.location + numBytesExpected * 3) { failed = TRUE; break; }
            for (j = 1; j < numBytesExpected; j ++) {
                if (CFStringGetCharacterAtIndex(originalString, percentRange.location + 3*j) != '%') { failed = TRUE; break; }
                ch1 = CFStringGetCharacterAtIndex(originalString, percentRange.location + 3*j + 1);
            ch2 = CFStringGetCharacterAtIndex(originalString, percentRange.location + 3*j + 2);
                if (!_translateBytes(ch1, ch2, bytes+j)) { failed = TRUE; break; }
            }

            // !!! We should do the low-level bit-twiddling ourselves; this is expensive!  REW, 6/10/99
            smallString = CFStringCreateWithBytes(alloc, bytes, numBytesExpected, kCFStringEncodingUTF8, FALSE);
            if (!smallString || CFStringGetLength(smallString) != 1) {
                failed = TRUE;
                if (smallString) CFRelease(smallString);
                break;
            } else {
                escapedChar = CFStringGetCharacterAtIndex(smallString, 0);
                CFRelease(smallString);
            }
        }

        // The new character is in escapedChar; the number of percent escapes it took is in numBytesExpected.
        searchRange.location = percentRange.location + 3 * numBytesExpected;
        searchRange.length = length - searchRange.location;
        
        if (!escapeAll) {
            if (!escapedStr) {
                escapedStr = CFStringCreateMutableWithExternalCharactersNoCopy(CFGetAllocator(originalString), &escapedChar, 1, 1, kCFAllocatorNull);
            }
            if (CFStringFind(charactersToLeaveEscaped, escapedStr, 0).location != -1) {
                continue;
            } 
        }
        
        if (!newStr) {
            newStr = CFStringCreateMutable(alloc, length);
        }
        if (percentRange.location - mark > 0) {
            // The creation of this temporary string is unfortunate. 
            CFStringRef substring = CFStringCreateWithSubstring(alloc, originalString, CFRangeMake(mark, percentRange.location - mark));
            CFStringAppend(newStr, substring);
            CFRelease(substring);
        }
        CFStringAppendCharacters(newStr, &escapedChar, 1);
        mark = searchRange.location;// We need mark to be the index of the first character beyond the escape sequence
    }

    if (escapedStr) CFRelease(escapedStr);
    if (failed) {
        if (newStr) CFRelease(newStr);
        return NULL;
    } else if (newStr) {
        if (mark < length) {
            // Need to cat on the remainder of the string
            CFStringRef substring = CFStringCreateWithSubstring(alloc, originalString, CFRangeMake(mark, length - mark));
            CFStringAppend(newStr, substring);
            CFRelease(substring);
        }
        return newStr;
    } else {
        CFRetain(originalString);
        return originalString;
    }
}

CFTypeID CFURLGetTypeID(void) {
    return __kCFURLTypeID;
}

Boolean __CFURLEqual(CFTypeRef  cf1, CFTypeRef  cf2) {
    CFURLRef  url1 = cf1;
    CFURLRef  url2 = cf2;
    UInt32 pathType1, pathType2;
    
    __CFGenericValidateType(cf1, __kCFURLTypeID);
    __CFGenericValidateType(cf2, __kCFURLTypeID);

    if (url1 == url2) return TRUE;
    if ((url1->_flags & IS_DIRECTORY) != (url2->_flags & IS_DIRECTORY)) return FALSE;
    if (url1->_base) {
        if (!url2->_base) return FALSE;
        if (!CFEqual(url1->_base, url2->_base)) return FALSE;
    } else if (url2->_base) {
        return FALSE;
    }
    
    pathType1 = URL_PATH_TYPE(url1);
    pathType2 = URL_PATH_TYPE(url2);
    if (pathType1 == pathType2) {
        return CFEqual(url1->_string, url2->_string);
    } else {
        // Try hard to avoid the expensive conversion from a file system representation to the canonical form
        CFStringRef scheme1 = CFURLCopyScheme(url1);
        CFStringRef scheme2 = CFURLCopyScheme(url2);
        Boolean eq = CFEqual(scheme1, scheme2);
        CFRelease(scheme1);
        CFRelease(scheme2);
        if (!eq) return FALSE;

        if (pathType1 == FULL_URL_REPRESENTATION) {
            if (!(url1->_flags & IS_PARSED)) {
                _parseComponents((struct __CFURL *)url1);
            }
            if (url1->_flags & (HAS_USER | HAS_PORT | HAS_PASSWORD | HAS_QUERY | HAS_PARAMETERS | HAS_FRAGMENT )) {
                return FALSE;
            }
        }

        if (pathType2 == FULL_URL_REPRESENTATION) {
            if (!(url2->_flags & IS_PARSED)) {
                _parseComponents((struct __CFURL *)url2);
            }
            if (url2->_flags & (HAS_USER | HAS_PORT | HAS_PASSWORD | HAS_QUERY | HAS_PARAMETERS | HAS_FRAGMENT )) {
                return FALSE;
            }
        }

        // No help for it; we now must convert to the canonical representation and compare.
        if (pathType1 != FULL_URL_REPRESENTATION) {
            _convertToURLRepresentation((struct __CFURL *)url1);
        }
        if (pathType2 != FULL_URL_REPRESENTATION) {
            _convertToURLRepresentation((struct __CFURL *)url2);
        }
        return CFEqual(url1->_string, url2->_string);
    }
}

CFAllocatorRef __CFURLGetAllocator(CFTypeRef cf) {
    __CFGenericValidateType(cf, __kCFURLTypeID);
    if (((CFURLRef)cf)->_flags & HAS_CUSTOM_ALLOCATOR) {
        return *((CFAllocatorRef *)((UInt8 *)cf + sizeof(struct __CFURL)));
    } else {
        return kCFAllocatorSystemDefault;
    }
}

UInt32 __CFURLHash(CFTypeRef  cf) {
    /* This is tricky, because we do not want the hash value to change as a file system URL is changed to its canonical representation, nor do we wish to force the conversion to the canonical representation. We choose instead to take the last path component (or "/" in the unlikely case that the path is empty), then hash on that. */
    CFURLRef  url = cf;
    CFStringRef lastComp = CFURLCopyLastPathComponent(url);
    UInt32 result;
    result = CFHash(lastComp);
    CFRelease(lastComp);
    return result;
}

CF_EXPORT void CFShowURL(CFURLRef url) {
    if (!url) {
        printf("(null)\n");
        return;
    }
    printf("<CFURL 0x%x>{", (unsigned)url);
    if (CF_IS_OBJC(URL, url)) {
        printf("ObjC bridged object}\n");
        return;
    }
    printf("\n\tPath type: ");
    switch (URL_PATH_TYPE(url)) {
        case kCFURLPOSIXPathStyle:
            printf("POSIX");
            break;
        case kCFURLHFSPathStyle:
            printf("HFS");
            break;
        case kCFURLWindowsPathStyle:
            printf("NTFS");
            break;
        case FULL_URL_REPRESENTATION:
            printf("Native URL");
            break;
        default:
            printf("UNRECOGNIZED PATH TYPE %d", (char)URL_PATH_TYPE(url));
    }
    printf("\n\tRelative string: ");
    CFShow(url->_string);
    printf("\tBase URL: ");
    if (url->_base) {
        printf("<0x%x> ", (unsigned)url->_base);
        CFShow(url->_base);
    } else {
        printf("(null)\n");
    }
    printf("\tFlags: 0x%x\n}\n", (unsigned)url->_flags);
}

CFStringRef   __CFURLCopyDescription(CFTypeRef  cf) {
    CFURLRef  url = cf;
    __CFGenericValidateType(cf, __kCFURLTypeID);
    if (!url->_base) {
        CFRetain(url->_string);
        return url->_string;
    } else {
        // Do not dereference url->_base; it may be an ObjC object
        return CFStringCreateWithFormat(__CFURLGetAllocator(url), NULL, CFSTR("%@ -- %@"), url->_string, url->_base);
    }
}


#if DEBUG_URL_MEMORY_USAGE
static CFAllocatorRef URLAllocator = NULL;
static UInt32 numFileURLsCreated = 0;
static UInt32 numFileURLsConverted = 0;
static UInt32 numFileURLsDealloced = 0;
static UInt32 numURLs = 0;
static UInt32 numDealloced = 0;
void __CFURLDumpMemRecord(void) {
    CFStringRef str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d URLs created; %d destroyed\n%d file URLs created; %d converted; %d destroyed\n"), numURLs, numDealloced, numFileURLsCreated, numFileURLsConverted, numFileURLsDealloced);
    CFShow(str);
    CFRelease(str);
    if (URLAllocator) CFCountingAllocatorPrintPointers(URLAllocator);
}
#endif

void __CFURLDeallocate(CFTypeRef  cf) {
    CFURLRef  url = cf;
    CFAllocatorRef alloc;
    __CFGenericValidateType(cf, __kCFURLTypeID);
    alloc = __CFURLGetAllocator(url);
#if DEBUG_URL_MEMORY_USAGE
    numDealloced ++;
    if (URL_PATH_TYPE(url) != FULL_URL_REPRESENTATION) {
        numFileURLsDealloced ++;
    }
#endif
    CFRelease(url->_string);
    if (url->_base) CFRelease(url->_base);
    if (url->ranges) CFAllocatorDeallocate(alloc, url->ranges);
    CFAllocatorDeallocate(alloc, (void *)url);
    CFRelease(alloc);
}

/***************************************************/
/* URL creation and String/Data creation from URLS */
/***************************************************/


#define STRING_CHAR(x) (useCString ? cstring[(x)] : ustring[(x)])
static void _parseComponents(struct __CFURL *anURL) {
    CFRange ranges[9];
    /* index gives the URL part involved; to calculate the correct range index, use the number of the bit of the equivalent flag (i.e. the host flag is HAS_HOST, which is 0x8.  so the range index for the host is 3.)  Note that this is true in this function ONLY, since the ranges stored in anURL->ranges are actually packed, skipping those URL components that don't exist.  This is why the indices are hard-coded in this function. */

    CFIndex idx, base_idx = 0;
    CFIndex string_length;
    UInt32 flags = (IS_PARSED | anURL->_flags);
    Boolean useCString, freeCharacters;
    UInt8 numRanges = 0;
    const unsigned char *cstring = NULL;
    const UniChar *ustring = NULL;
    CFAllocatorRef alloc;
    
    CFAssert1(URL_PATH_TYPE(anURL) == FULL_URL_REPRESENTATION, __kCFLogAssertion, "%s(): Cannot be called with a file system URL", __PRETTY_FUNCTION__ );

    string_length = CFStringGetLength(anURL->_string);
    alloc = __CFURLGetAllocator(anURL);
    
    if ( (cstring = CFStringGetCStringPtr(anURL->_string, kCFStringEncodingISOLatin1)) ) {
        useCString = TRUE;
        freeCharacters = FALSE;
    } else if (ustring = CFStringGetCharactersPtr(anURL->_string)) {
        useCString = FALSE;
        freeCharacters = FALSE;
    } else {
        CFIndex neededLength;
        CFRange rg = CFRangeMake(0, string_length);
        freeCharacters = TRUE;
        CFStringGetBytes(anURL->_string, rg, kCFStringEncodingISOLatin1, 0, FALSE, NULL, INT_MAX, &neededLength);
        if (neededLength == string_length) {
            char *buf = CFAllocatorAllocate(alloc, string_length, 0);
            CFStringGetBytes(anURL->_string, rg, kCFStringEncodingISOLatin1, 0, FALSE, buf, string_length, NULL);
            cstring = buf;
            useCString = TRUE;
        } else {
            UniChar *buf = CFAllocatorAllocate(alloc, string_length * sizeof(UniChar), 0);
            CFStringGetCharacters(anURL->_string, rg, buf);
            useCString = FALSE;
            ustring = buf;
        }
    }

    // Algorithm is as described in RFC 1808
    // 1: parse the fragment; remainder after left-most "#" is fragment
    for (idx = base_idx; idx < string_length; idx++) {
        if ('#' == STRING_CHAR(idx)) {
            flags |= HAS_FRAGMENT;
            ranges[8].location = idx + 1;
            ranges[8].length = string_length - (idx + 1);
            numRanges ++;
            string_length = idx;	// remove fragment from parse string
            break;
        }
    }
    // 2: parse the scheme
    for (idx = base_idx; idx < string_length; idx++) {
        UniChar ch = STRING_CHAR(idx);
        if (':' == ch) {
            flags |= HAS_SCHEME;
            flags |= IS_ABSOLUTE;
            ranges[0].location = base_idx;
            ranges[0].length = idx;
            numRanges ++;
            base_idx = idx + 1;
            break;
        } else if (!scheme_valid(ch)) {
            break;	// invalid scheme character -- no scheme
        }
    }

    // Make sure we have an RFC-1808 compliant URL
    if ((flags & HAS_SCHEME) && !(2 <= (string_length - base_idx) && '/' == STRING_CHAR(base_idx) && '/' == STRING_CHAR(base_idx + 1))) {
        // Clear the fragment flag if it's been set
        flags &= (~HAS_FRAGMENT);
        anURL->_flags = flags;
        anURL->ranges = CFAllocatorAllocate(alloc, sizeof(CFRange), 0);
        anURL->ranges->location = ranges[0].location;
        anURL->ranges->length = ranges[0].length;
        if (freeCharacters) {
            CFAllocatorDeallocate(alloc, useCString ? (void *)cstring : (void *)ustring);
        }
        return;
    } 

    // URL is 1808-compliant
    flags |= IS_DECOMPOSABLE;
    
    // 3: parse the network location and login
    if (2 <= (string_length - base_idx) && '/' == STRING_CHAR(base_idx) && '/' == STRING_CHAR(base_idx+1)) {
        CFIndex base = 2 + base_idx, extent;
        for (idx = base; idx < string_length; idx++) {
            if ('/' == STRING_CHAR(idx)) break;
        }
        extent = idx;
        
        // net_loc parts extend from base to extent (but not including), which might be to end of string
        // net location is "<user>:<password>@<host>:<port>"
        if (extent != base) {
            for (idx = base; idx < extent; idx++) {
                if ('@' == STRING_CHAR(idx)) {   // there is a user
                    CFIndex idx2;
                    flags |= HAS_USER;
                    numRanges ++;
                    ranges[1].location = base;  // base of the user
                    for (idx2 = base; idx2 < idx; idx2++) {
                        if (':' == STRING_CHAR(idx2)) {	// found a password separator
                            flags |= HAS_PASSWORD;
                            numRanges ++;
                            ranges[2].location = idx2+1; // base of the password
                            ranges[2].length = idx-(idx2+1);  // password extent
                            ranges[1].length = idx2 - base; // user extent
                            break;
                        }
                    }
                    if (!(flags & HAS_PASSWORD)) {
                        // user extends to the '@'
                        ranges[1].length = idx - base; // user extent
                    }
                    base = idx + 1;
                    break;
                }
            }
            flags |= HAS_HOST;
            numRanges ++;
            ranges[3].location = base; // base of host

            // base has been advanced past the user and password if they existed
            for (idx = base; idx < extent; idx++) {
                if (':' == STRING_CHAR(idx)) {	// there is a port
                    flags |= HAS_PORT;
                    numRanges ++;
                    ranges[4].location = idx+1; // base of port
                    ranges[4].length = extent - (idx+1); // port extent
                    ranges[3].length = idx - base; // host extent
                    break;
                }
            }
            if (!(flags & HAS_PORT)) {
                ranges[3].length = extent - base;  // host extent
            }
        }
        base_idx = extent;
    }

    // 4: parse the query; remainder after left-most "?" is query
    for (idx = base_idx; idx < string_length; idx++) {
        if ('?' == STRING_CHAR(idx)) {
            flags |= HAS_QUERY;
            numRanges ++;
            ranges[7].location = idx + 1;
            ranges[7].length = string_length - (idx+1);
            string_length = idx;	// remove query from parse string
            break;
        }
    }
        
    // 5: parse the parameters; remainder after left-most ";" is parameters
    for (idx = base_idx; idx < string_length; idx++) {
        if (';' == STRING_CHAR(idx)) {
            flags |= HAS_PARAMETERS;
            numRanges ++;
            ranges[6].location = idx + 1;
            ranges[6].length = string_length - (idx+1);
            string_length = idx;	// remove parameters from parse string
            break;
        }
    }
        
    // 6: parse the path; it's whatever's left between string_length & base_idx
    if (string_length - base_idx != 0 || (flags & NET_LOCATION_MASK))
    {
        // If we have a net location, we are 1808-compliant, and an empty path substring implies a path of "/"
        UniChar ch;
        Boolean isDir;
        CFRange pathRg;
        flags |= HAS_PATH;
        numRanges ++;
        pathRg.location = base_idx;
        pathRg.length = string_length - base_idx;
        ranges[5] = pathRg;

        if (pathRg.length > 0) {
            ch = STRING_CHAR(pathRg.location + pathRg.length - 1);
            if (ch == '/') {
                isDir = TRUE;
            } else if (ch == '.') {
                if (pathRg.length == 1) {
                    isDir = TRUE;
                } else {
                    ch = STRING_CHAR(pathRg.location + pathRg.length - 2);
                    if (ch == '/') {
                        isDir = TRUE;
                    } else if (ch != '.') {
                        isDir = FALSE;
                    } else if (pathRg.length == 2) {
                        isDir = TRUE;
                    } else {
                        isDir = (STRING_CHAR(pathRg.location + pathRg.length - 3) == '/');
                    }
                }
            } else {
                isDir = FALSE;
            }
        } else {
            isDir = (anURL->_base != NULL) ? CFURLHasDirectoryPath(anURL->_base) : FALSE;
        }
        if (isDir) {
            flags |= IS_DIRECTORY;
        }
    }

    if (freeCharacters) {
        CFAllocatorDeallocate(alloc, useCString ? (void *)cstring : (void *)ustring);
    }
    anURL->_flags = flags;
    anURL->ranges = CFAllocatorAllocate(alloc, sizeof(CFRange)*numRanges, 0);
    numRanges = 0;
    for (idx = 0, flags = 1; flags != (1<<9); flags = (flags<<1), idx ++) {
        if (anURL->_flags & flags) {
            anURL->ranges[numRanges] = ranges[idx];
            numRanges ++;
        }
    }
    if ((anURL->_flags & HAS_PATH) && !CFStringFindWithOptions(anURL->_string, CFSTR("%"), ranges[5], 0, NULL)) {
        anURL->_flags |= POSIX_AND_URL_PATHS_MATCH;
    }
}
#undef STRING_CHAR

CF_EXPORT CFURLRef _CFURLAlloc(CFAllocatorRef allocator) {
    struct __CFURL *url;
#if DEBUG_URL_MEMORY_USAGE
    numURLs ++;
    if (!URLAllocator) {
        URLAllocator = CFCountingAllocatorCreate(NULL);
    }
    allocator = URLAllocator;
#endif
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    if (allocator == kCFAllocatorSystemDefault) {
        url = (struct __CFURL *)CFAllocatorAllocate(allocator, sizeof(struct __CFURL), 0);
        if (url) {
            url->_flags = 0;
        }
    } else {
        CFAllocatorRef *addr;
        url = (struct __CFURL *)CFAllocatorAllocate(allocator, sizeof(struct __CFURL) + sizeof(CFAllocatorRef), 0);
        if (url) {
            url->_flags = HAS_CUSTOM_ALLOCATOR;
            addr = (CFAllocatorRef *)((UInt8 *)url + sizeof(struct __CFURL));
            *addr = allocator;
        }
    }
    if (!url) {
        CFRelease(allocator);
        return NULL;
    }
    __CFGenericInitBase((void *)url, __CFURLIsa(), __kCFURLTypeID);
    url->_string = NULL;
    url->_base = NULL;
    url->ranges = NULL;
    url->_reserved = NULL;
    return url;
}

// It is the caller's responsibility to guarantee that if URLString is absolute, base is NULL.  This is necessary to avoid duplicate processing for file system URLs, which had to decide whether to compute the cwd for the base; we don't want to duplicate that work.  This ALSO means it's the caller's responsibility to set the IS_ABSOLUTE bit, since we may have a degenerate URL whose string is relative, but lacks a base.
static void _CFURLInit(struct __CFURL *url, CFStringRef URLString, UInt32 fsType, CFURLRef base) {
    CFAssert2((fsType == FULL_URL_REPRESENTATION) || (fsType == kCFURLPOSIXPathStyle) || (fsType == kCFURLWindowsPathStyle) || (fsType == kCFURLHFSPathStyle), __kCFLogAssertion, "%s(): Received bad fsType %d", __PRETTY_FUNCTION__, fsType);
    
    // Coming in, the url has its allocator flag properly set, and its base initialized, and nothing else.    
    url->_string = CFStringCreateCopy(__CFURLGetAllocator(url), URLString);
    url->_flags |= (fsType << 16);
    url->_base = base ? CFURLCopyAbsoluteURL(base) : NULL;
#if DEBUG_URL_MEMORY_USAGE
    if (fsType != FULL_URL_REPRESENTATION) {
        numFileURLsCreated ++;
    }
#endif
}

CF_EXPORT void _CFURLInitWithString(CFURLRef myURL, CFStringRef string, CFURLRef baseURL) {
    struct __CFURL *url = (struct __CFURL *)myURL; // Supress annoying compile warnings
    Boolean isAbsolute = FALSE;
    CFRange colon = CFStringFind(string, CFSTR(":"), 0);
    if (colon.location != -1) {
        CFRange hash = CFStringFind(string, CFSTR("#"), kCFCompareBackwards);
        if (hash.location == -1 || hash.location > colon.location) {
            isAbsolute = TRUE;
        }
    }
    _CFURLInit(url, string, FULL_URL_REPRESENTATION, isAbsolute ? NULL : baseURL);
    if (isAbsolute) {
        url->_flags |= IS_ABSOLUTE;
    }
}

#warning CF: Optimization opportunity
// There could be some lucrative optimizations in here - we are pulling apart the string character by character instead of grabbing a UniChar array, and there are several small allocations used as we go back and forth between encodings.  Seems like we could do better.  -- REW, 2/28/2000
/* Returned string is retained for the caller; if escapePercents is TRUE, then we do not look for any %-escape encodings in urlString */
static CFStringRef  _convertPercentEscapes(CFStringRef urlString, CFStringEncoding fromEncoding, CFStringEncoding toEncoding, Boolean escapeAllHighBitCharacters, Boolean escapePercents, const UniChar *addlCharsToEscape, int numAddlChars) {
    Boolean agreesOverASCII7 = __CFStringEncodingIsSupersetOfASCII(toEncoding) && __CFStringEncodingIsSupersetOfASCII(fromEncoding);
    Boolean encodingsMatch = (fromEncoding == toEncoding);
    UInt32 i, length = CFStringGetLength(urlString);
    CFMutableStringRef  newString = NULL;
    Boolean fail = FALSE;
    const UniChar *lastAddlChar = addlCharsToEscape && numAddlChars > 0 ? addlCharsToEscape + numAddlChars : NULL;
    for (i = 0; i < length && !fail; i ++) {
        UniChar ch = CFStringGetCharacterAtIndex(urlString, i);
        if (escapePercents || ch != '%') {
            Boolean mustEscape = FALSE;
            if (escapeAllHighBitCharacters && ch > 0x7F) {
                mustEscape = TRUE;
            } else if (lastAddlChar) {
                const UniChar *escChar;
                for (escChar = addlCharsToEscape; escChar < lastAddlChar; escChar ++) {
                    if (*escChar == ch) {
                        mustEscape = TRUE;
                        break;
                    }
                }
            }
            if (mustEscape) {
                if (!newString) {
                    newString = CFStringCreateMutableCopy(CFGetAllocator(urlString), 0, urlString);
                    CFStringDelete(newString, CFRangeMake(i, length - i));
                }
                if (!_appendPercentEscapesForCharacter(ch, toEncoding, newString)) {
                    fail = TRUE;
                }
            } else if (newString) {
                CFStringAppendCharacters(newString, &ch, 1);
            }
        } else if (encodingsMatch) {
            if (newString) {
                CFStringAppendCharacters(newString, &ch, 1);
            }
        } else {
            CFMutableDataRef  newData = CFDataCreateMutable(CFGetAllocator(urlString), 0);
            Boolean sawNonASCIICharacter = FALSE;
            UInt32 base = i;
            while (i < length && CFStringGetCharacterAtIndex(urlString, i) == '%') {
                UInt8 byte;
                if (i+2 >= length || !_translateBytes(CFStringGetCharacterAtIndex(urlString, i+1), CFStringGetCharacterAtIndex(urlString, i+2), &byte)) {
                    fail = TRUE;
                    break;
                }
                if (byte > 0x7f) sawNonASCIICharacter = TRUE;
                CFDataAppendBytes(newData, &byte, 1);
                i += 3;
            }
            i --; // We'll increment again next time through the loop
            if (fail) {
                CFRelease(newData);
                break;
            }
            if (!sawNonASCIICharacter && agreesOverASCII7) {
                if (newString) {
                    CFStringRef  tmp = CFStringCreateWithSubstring(CFGetAllocator(newString), urlString, CFRangeMake(base, i-base));
                    CFStringAppend(newString, tmp);
                    CFRelease(tmp);
                }
            } else {
                CFStringRef  tmp = CFStringCreateWithBytes(CFGetAllocator(urlString), CFDataGetBytePtr(newData), CFDataGetLength(newData), fromEncoding, FALSE);
                CFIndex tmpIndex, tmpLen;
                if (!tmp) {
                    fail = TRUE;
                } else {
                    if (!newString) {
                        newString = CFStringCreateMutableCopy(CFGetAllocator(urlString), 0, urlString);
                        CFStringDelete(newString, CFRangeMake(base, length - base));
                    }
                    tmpLen = CFStringGetLength(tmp);
                    for (tmpIndex = 0; tmpIndex < tmpLen && !fail; tmpIndex ++) {
                        if (!_appendPercentEscapesForCharacter(CFStringGetCharacterAtIndex(tmp, tmpIndex), toEncoding, newString)) {
                            fail = TRUE;
                            break;
                        }
                    }
                    CFRelease(tmp);
                }
            }
            CFRelease(newData);
        }
    }
    if (fail) {
        if (newString) CFRelease(newString);
        newString = NULL;
    } else if (!newString) {
        CFRetain(urlString); // So that no matter what, the returned string needs to be released
        newString = (CFMutableStringRef)urlString;
    }
    return newString;
}


// encoding will be used both to interpret the bytes of URLBytes, and to interpret any percent-escapes within the bytes.
CFURLRef CFURLCreateWithBytes(CFAllocatorRef allocator, const UInt8 *URLBytes, CFIndex length, CFStringEncoding encoding, CFURLRef baseURL) {
    CFStringRef  urlString = CFStringCreateWithBytes(allocator, URLBytes, length, encoding, FALSE);
    CFURLRef  result;
    if (!urlString) return NULL;
    if (encoding != kCFStringEncodingUTF8) {
        CFStringRef  tmp = _convertPercentEscapes(urlString, encoding, kCFStringEncodingUTF8, FALSE, FALSE, NULL, 0);
        CFRelease(urlString);
        urlString = tmp;
        if (!urlString) return NULL;
    }
    result = _CFURLAlloc(allocator);
    if (result) {
        _CFURLInitWithString(result, urlString, baseURL);
    }
    CFRelease(urlString); // it's retained by result, now.
    return result;
}

CFDataRef CFURLCreateData(CFAllocatorRef allocator, CFURLRef  url, CFStringEncoding encoding, Boolean escapeWhitespace) {
    static const UniChar whitespaceChars[4] = {' ', '\n', '\r', '\t'};
    CFStringRef  myStr = CFURLGetString(url);
    CFStringRef newStr = _convertPercentEscapes(myStr, kCFStringEncodingUTF8, encoding, TRUE, FALSE, escapeWhitespace ? whitespaceChars : NULL, escapeWhitespace ? 4 : 0);
    CFDataRef  result = CFStringCreateExternalRepresentation(allocator, newStr, encoding, 0);
    CFRelease(newStr);
    return result;
}

// Any escape sequences in URLString will be interpreted via UTF-8.
CFURLRef CFURLCreateWithString(CFAllocatorRef allocator, CFStringRef  URLString, CFURLRef  baseURL) {
    CFURLRef url = _CFURLAlloc(allocator);
    if (url) {
        _CFURLInitWithString(url, URLString, baseURL);
    }
    return url;
}

/* This function is this way because I pulled it out of _resolvedURLPath (so that _resolvedFileSystemPath could use it), and I didn't want to spend a bunch of energy reworking the code.  So instead of being a bit more intelligent about inputs, it just demands a slightly perverse set of parameters, to match the old _resolvedURLPath code.  -- REW, 6/14/99 */
static CFStringRef _resolvedPath(UniChar *pathStr, UniChar *end, UniChar pathDelimiter, Boolean stripTrailingDelimiter, CFAllocatorRef alloc) {
    UniChar *index = pathStr;
    while (index < end) {
        if (*index == '.') {
            if (index+1 == end) {
                *index = '\0';
                end = index;
                break;
            } else if (*(index+1) == pathDelimiter) {
                CFMoveMemory(index, index+2, (end-(index+2)+1) * sizeof(UniChar));
                end -= 2;
                continue;
            } else if (*(index+1) == '.' && (index+2 == end || *(index+2) == pathDelimiter) && index - pathStr >= 2) {
               // Need at least 2 characters between index and pathStr, because we know if index != newPath, then *(index-1) == pathDelimiter, and we need something before that to compact out.
                UniChar *lastDelim = index-2;
                while (lastDelim >= pathStr && *lastDelim != pathDelimiter) lastDelim --;
                lastDelim ++;
                if (lastDelim != index && (index-lastDelim != 3 || *lastDelim != '.' || *(lastDelim +1) != '.')) {
                   // We have a genuine component to compact out
                    if (index+2 != end) {
                        unsigned numCharsToMove = end - (index+3) + 1; // +1 to move the '\0' as well
                        CFMoveMemory(lastDelim, index+3, numCharsToMove * sizeof(UniChar));
                        end -= (index + 3 - lastDelim);
                        index = lastDelim;
                        continue;
                    } else {
                        *lastDelim = '\0';
                        end = lastDelim;
                        break;
                    }
                }
            }
        }
        while (index < end && *index != pathDelimiter) index ++;
        index ++;
    }
    if (stripTrailingDelimiter && end != pathStr && end-1 != pathStr && *(end-1) == pathDelimiter) {
        end --;
    }
    return CFStringCreateWithCharactersNoCopy(alloc, pathStr, end - pathStr, alloc);
}

// Returned string needs to be released.  This can be implemented much more tidily (but inefficiently) via CFArrayCreateWithString and CFStringCreateWithArray, now that those functions exist.
static CFStringRef  resolvedURLPath(CFURLRef  anURL) {
    // As with CFDataCreateWithURL(), above, we choose to call CFURL... functions rather than dealing with the possibility that anURL is actually an NSURL.  Therefore, we must not dereference anURL anywhere below.  -- REW, 10/28/98
    // This is no longer a good strategy, because CFURLCopyPath() could be expensive.  -- REW, 6/14/99
    CFURLRef  myBase = CFURLGetBaseURL(anURL);
    CFStringRef  myPath = CFURLCopyPath(anURL);
    CFStringRef  basePath = NULL;
    UInt32 myLen = 0, baseLen = 0;
    UniChar *newPath, *index, *end;
    CFAllocatorRef alloc = CFGetAllocator(anURL);
    
    if (!myBase) {
        return myPath;
    } else if (!myPath || !(myLen = CFStringGetLength(myPath))) {
        if (myPath) CFRelease(myPath);
        return CFURLCopyPath(myBase);
    } else if (CFStringGetCharacterAtIndex(myPath, 0) == '/') {
        return myPath;
    } else {
        basePath = CFURLCopyPath(myBase);
        if (!basePath || !(baseLen = CFStringGetLength(basePath))) {
            if (basePath) CFRelease(basePath);
            return myPath;
        }
    }

    newPath = CFAllocatorAllocate(alloc, sizeof(UniChar) * (myLen + baseLen + 1), 0);
    CFStringGetCharacters(basePath, CFRangeMake(0, baseLen), newPath);
    CFRelease(basePath);
    index = newPath + baseLen - 1;
    while (index != newPath && *index != '/') index --;
    if (*index == '/') index ++;
    CFStringGetCharacters(myPath, CFRangeMake(0, myLen), index);
    CFRelease(myPath);
    end = index + myLen;
    *end = 0;
    return _resolvedPath(newPath, end, '/', FALSE, alloc);
}

CFURLRef  CFURLCopyAbsoluteURL(CFURLRef  relativeURL) {
    CFMutableStringRef  newString;
    CFURLRef  anURL, base;
    CFStringRef  str;
    CFURLPathStyle fsType;
    CFAllocatorRef alloc = CFGetAllocator(relativeURL);

    CFAssert1(relativeURL != NULL, __kCFLogAssertion, "%s(): Cannot create an absolute URL from a NULL relative URL", __PRETTY_FUNCTION__);
    CF_OBJC_FUNCDISPATCH0(URL, CFURLRef  , relativeURL, "_retainedAbsoluteURL");
    __CFGenericValidateType(relativeURL, __kCFURLTypeID);

    base = relativeURL->_base;
    if (!base) {
        return CFRetain(relativeURL);
    }
    fsType = URL_PATH_TYPE(relativeURL);
    if (!CF_IS_OBJC(URL, base) && fsType != FULL_URL_REPRESENTATION && fsType == URL_PATH_TYPE(base)) {
        return _CFURLCopyAbsoluteFileURL(relativeURL);
    }
    if (fsType != FULL_URL_REPRESENTATION) {
        _convertToURLRepresentation((struct __CFURL *)relativeURL);
    }
    if (!(relativeURL->_flags & IS_PARSED)) {
        _parseComponents((struct __CFURL *)relativeURL);
    }
    if ((relativeURL->_flags & POSIX_AND_URL_PATHS_MATCH) && !(relativeURL->_flags & (RESOURCE_SPECIFIER_MASK | NET_LOCATION_MASK)) && (!CF_IS_OBJC(URL, base)) && (URL_PATH_TYPE(base) == kCFURLPOSIXPathStyle)) {
        CFStringRef relPath = CFURLCopyPath(relativeURL); // This will be fast, because POSIX_AND_URL_PATHS_MATCH
        CFStringRef newPath = _resolveFileSystemPaths(relPath, base->_string, CFURLHasDirectoryPath(base), kCFURLPOSIXPathStyle, alloc);
        CFURLRef result = CFURLCreateWithFileSystemPath(alloc, newPath, kCFURLPOSIXPathStyle, CFURLHasDirectoryPath(relativeURL));
        CFRelease(relPath);
        CFRelease(newPath);
        return result;
    }

    newString = CFStringCreateMutable(alloc, 0);
    str = CFURLCopyScheme(base);
    if (str) {
        CFStringAppendFormat(newString, NULL, CFSTR("%@:"), str);
        CFRelease(str);
    }

    if (relativeURL->_flags & NET_LOCATION_MASK) {
        CFStringAppend(newString, relativeURL->_string);
    } else {
        CFStringAppend(newString, CFSTR("//"));
        str = CFURLCopyNetLocation(base);
        if (str) {
            CFStringAppend(newString, str);
            CFRelease(str);
        }

        if (relativeURL->_flags & HAS_PATH) {
            CFStringRef  newPath = resolvedURLPath(relativeURL);
            CFStringAppend(newString, newPath);
            CFRelease(newPath);
            str = CFURLCopyResourceSpecifier(relativeURL);
            if (str) {
                CFStringAppend(newString, str);
                CFRelease(str);
            }
        } else {
            str = CFURLCopyPath(base);
            if (str) {
                CFStringAppend(newString, str);
                CFRelease(str);
            }

            if (!(relativeURL->_flags & RESOURCE_SPECIFIER_MASK)) {
                str = CFURLCopyResourceSpecifier(base);
                if (str) {
                    CFStringAppend(newString, str);
                    CFRelease(str);
                }
            } else if (relativeURL->_flags & HAS_PARAMETERS) {
                str = CFURLCopyResourceSpecifier(relativeURL);
                CFStringAppend(newString, str);
                CFRelease(str);
            } else {
               // Sigh; we have to resolve these against one another
                str = CFURLCopyParameterString(base, NULL);
                if (str) {
                    CFStringAppendFormat(newString, NULL, CFSTR(";%@"), str);
                    CFRelease(str);
                }
                if (relativeURL->_flags & HAS_QUERY) {
                    str = CFURLCopyQueryString(relativeURL, NULL);
                    CFStringAppendFormat(newString, NULL, CFSTR("?%@"), str);
                    CFRelease(str);
                } else {
                    str = CFURLCopyQueryString(base, NULL);
                    if (str) {
                        CFStringAppendFormat(newString, NULL, CFSTR("?%@"), str);
                        CFRelease(str);
                    }
                }
                // Only the relative portion of the URL can supply the fragment; otherwise, what would be in the relativeURL?
                if (relativeURL->_flags & HAS_FRAGMENT) {
                    str = CFURLCopyFragment(relativeURL, NULL);
                    CFStringAppendFormat(newString, NULL, CFSTR("#%@"), str);
                    CFRelease(str);
                }
            }
        }
    }

    anURL = CFURLCreateWithString(alloc, newString, NULL);
    CFRelease(newString);
    return anURL;
}

/*******************/
/* Basic accessors */
/*******************/

Boolean CFURLCanBeDecomposed(CFURLRef  anURL) {
    CF_OBJC_FUNCDISPATCH0(URL, Boolean, anURL, "_isDecomposable");
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) return TRUE;
    if (!(anURL->_flags & IS_PARSED)) {
        _parseComponents((struct __CFURL *)anURL);
    }
    return ((anURL->_flags & IS_DECOMPOSABLE) != 0);
}

CFStringRef  CFURLGetString(CFURLRef  url) {
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , url, "relativeString");
    if (URL_PATH_TYPE(url) != FULL_URL_REPRESENTATION) {
        if (url->_base && (url->_flags & POSIX_AND_URL_PATHS_MATCH)) {
            return url->_string;
        }
        _convertToURLRepresentation((struct __CFURL *)url);
    }
    return url->_string;
}

CFURLRef  CFURLGetBaseURL(CFURLRef  anURL) {
    CF_OBJC_FUNCDISPATCH0(URL, CFURLRef, anURL, "baseURL");
    return anURL->_base;
}

// Assumes the URL is already parsed
static CFRange _rangeForComponent(CFURLRef url, UInt32 compFlag) {
    UInt32 flags = url->_flags;
    UInt32 index = 0;
    if (!(flags & compFlag)) return CFRangeMake(-1, 0);
    while (!(compFlag & 1)) {
        compFlag = compFlag >> 1;
        if (flags & 1) {
            index ++;
        }
        flags = flags >> 1;
    }
    return url->ranges[index];
}

static CFStringRef _retainedComponentString(CFURLRef url, UInt32 compFlag) {
    CFRange rg;
    CFAssert1(URL_PATH_TYPE(url) == FULL_URL_REPRESENTATION, __kCFLogAssertion, "%s(): passed a file system URL", __PRETTY_FUNCTION__);
    if (!(url->_flags & IS_PARSED)) {
        _parseComponents((struct __CFURL *)url);
    }
    rg = _rangeForComponent(url, compFlag);
    if (rg.location == -1) return NULL;
    return CFStringCreateWithSubstring(__CFURLGetAllocator(url), url->_string, rg);
}


CF_DEFINECONSTANTSTRING(kCFURLFileScheme, "file");
CF_DEFINECONSTANTSTRING(kCFURLLocalhost, "localhost");

CFStringRef  CFURLCopyScheme(CFURLRef  anURL) {
    CFStringRef scheme;
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedScheme");
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        CFRetain(kCFURLFileScheme); // because caller will release it
        return kCFURLFileScheme;
    }
    scheme = _retainedComponentString(anURL, HAS_SCHEME);
    if (scheme) {
        return scheme;
    } else if (anURL->_base) {
        return CFURLCopyScheme(anURL->_base);
    } else {
        return NULL;
    }
}

CFStringRef  CFURLCopyNetLocation(CFURLRef  anURL) {
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedNetLocation");
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        // !!! This won't work if we go to putting the vol ref num in the net location for HFS
        CFRetain(kCFURLLocalhost);
        return kCFURLLocalhost;
    }
    if (!(anURL->_flags & IS_PARSED)) {
        _parseComponents((struct __CFURL *)anURL);
    }
    if (anURL->_flags & NET_LOCATION_MASK) {
        // We provide the net location
        CFRange netRgs[4];
        CFRange netRg = {-1, 0};
        CFIndex i, c = 4;
        netRgs[0] = _rangeForComponent(anURL, HAS_USER);
        netRgs[1] = _rangeForComponent(anURL, HAS_PASSWORD);
        netRgs[2] = _rangeForComponent(anURL, HAS_HOST);
        netRgs[3] = _rangeForComponent(anURL, HAS_PORT);
        for (i = 0; i < c; i ++) {
            if (netRgs[i].location == -1) continue;
            if (netRg.location == -1) {
                netRg = netRgs[i];
            } else {
                netRg.length = netRgs[i].location + netRgs[i].length - netRg.location;
            }
        }
        return CFStringCreateWithSubstring(__CFURLGetAllocator(anURL), anURL->_string, netRg);
    } else if (anURL->_base) {
        return CFURLCopyNetLocation(anURL->_base);
    } else {
        return NULL;
    }
}

// NOTE - if you want an absolute path, you must first get the absolute URL.  If you want a file system path, use the file system methods above.
CFStringRef  CFURLCopyPath(CFURLRef  anURL) {
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef, anURL, "_retainedRelativeURLPath");
    if (URL_PATH_TYPE(anURL) == kCFURLPOSIXPathStyle && (anURL->_flags & POSIX_AND_URL_PATHS_MATCH)) {
        CFRetain(anURL->_string);
        return anURL->_string;
    }
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        _convertToURLRepresentation((struct __CFURL *)anURL);
    }
    return _retainedComponentString(anURL, HAS_PATH);
}

/* NULL if CFURLCanBeDecomposed(anURL) is FALSE; also does not resolve the URL against its base.  See also CFCreateAbsoluteURL().  Note that, strictly speaking, any leading '/' is not considered part of the URL's path, although its presence or absence determines whether the path is absolute.  CFURLCopyPath()'s return value includes any leading slash (giving the path the normal POSIX appearance); CFURLCopyStrictPath()'s return value omits any leading slash, and uses isAbsolute to report whether the URL's path is absolute.

  CFURLCopyFileSystemPath() returns the URL's path as a file system path for the given path style.  All percent escape sequences are replaced.  The URL is not resolved against its base before computing the path.
*/
CFStringRef CFURLCopyStrictPath(CFURLRef anURL, Boolean *isAbsolute) {
    CFStringRef path = CFURLCopyPath(anURL);
    if (!path || CFStringGetLength(path) == 0) {
        if (path) CFRelease(path);
        if (isAbsolute) *isAbsolute = FALSE;
        return NULL;
    }
    if (CFStringGetCharacterAtIndex(path, 0) == '/') {
        CFStringRef tmp;
        if (isAbsolute) *isAbsolute = TRUE;
        tmp = CFStringCreateWithSubstring(CFGetAllocator(path), path, CFRangeMake(1, CFStringGetLength(path)-1));
        CFRelease(path);
        path = tmp;
    } else {
        if (isAbsolute) *isAbsolute = FALSE;
    }
    return path;
}

Boolean CFURLHasDirectoryPath(CFURLRef  anURL) {
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) == FULL_URL_REPRESENTATION) {
        if (!(anURL->_flags & IS_PARSED)) {
            _parseComponents((struct __CFURL *)anURL);
        }
        if (!anURL->_base || (anURL->_flags & (HAS_PATH | NET_LOCATION_MASK))) {
            return ((anURL->_flags & IS_DIRECTORY) != 0);
        }
        return CFURLHasDirectoryPath(anURL->_base);
    }
    return ((anURL->_flags & IS_DIRECTORY) != 0);
}

CFStringRef  CFURLCopyResourceSpecifier(CFURLRef  anURL) {
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_CFResourceSpecifier");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        return NULL;
    }
    if (!(anURL->_flags & IS_PARSED)) {
        _parseComponents((struct __CFURL *)anURL);
    }
    if (!(anURL->_flags & IS_DECOMPOSABLE)) {
        CFRange schemeRg = _rangeForComponent(anURL, HAS_SCHEME);
        CFIndex base = schemeRg.location + schemeRg.length + 1;
        return CFStringCreateWithSubstring(__CFURLGetAllocator(anURL), anURL->_string, CFRangeMake(base, CFStringGetLength(anURL->_string)-base));
    } else {
        UInt32 firstRsrcSpecFlag = 0;
        UInt32 flag = HAS_FRAGMENT;
        while (flag != HAS_PATH) {
            if (anURL->_flags & flag) {
                firstRsrcSpecFlag = flag;
            }
            flag = flag >> 1;
        }
        if (firstRsrcSpecFlag) {
            CFRange rg = _rangeForComponent(anURL, firstRsrcSpecFlag);
            rg.location --; // Include the character that demarcates the component
            rg.length = CFStringGetLength(anURL->_string) - rg.location;
            return CFStringCreateWithSubstring(__CFURLGetAllocator(anURL), anURL->_string, rg);
        } else {
            // The resource specifier cannot possibly come from the base.
            return NULL;
        }
    }
}

/*************************************/
/* Accessors that create new objects */
/*************************************/

// For the next four methods, it is important to realize that, if a URL supplies any part of the net location (host, user, port, or password), it must supply all of the net location (i.e. none of it comes from its base URL).  Also, it is impossible for a URL to be relative, supply none of the net location, and still have its (empty) net location take precedence over its base URL (because there's nothing that precedes the net location except the scheme, and if the URL supplied the scheme, it would be absolute, and there would be no base).
CFStringRef  CFURLCopyHostName(CFURLRef  anURL) {
    CFStringRef tmp;
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedHost");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        CFRetain(kCFURLLocalhost);
        return kCFURLLocalhost;
    }
    tmp = _retainedComponentString(anURL, HAS_HOST);
    if (tmp) {
        CFStringRef host = CFURLCreateStringByReplacingPercentEscapes(__CFURLGetAllocator(anURL), tmp, CFSTR(""));
        CFRelease(tmp);
        return host;
    } else if (anURL->_base && !(anURL->_flags & NET_LOCATION_MASK) && !(anURL->_flags & HAS_SCHEME)) {
        return CFURLCopyHostName(anURL->_base);
    } else {
        return NULL;
    }
}

// Return -1 to indicate no port is specified
SInt32 CFURLGetPortNumber(CFURLRef  anURL) {
    CFStringRef port;
    CF_OBJC_FUNCDISPATCH0(URL, SInt32, anURL, "_portNumber");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        return -1;
    }
    port = _retainedComponentString(anURL, HAS_PORT);
    if (port) {
        SInt32 portNum, index, length = CFStringGetLength(port);
        CFStringInlineBuffer buf;
        CFStringInitInlineBuffer(port, &buf, CFRangeMake(0, length));
        index = 0;
        if (!__CFStringScanInteger(&buf, NULL, &index, FALSE, &portNum) || (index != length)) {
            portNum = -1;
        }
        CFRelease(port);
        return portNum;
    } else if (anURL->_base && !(anURL->_flags & NET_LOCATION_MASK) && !(anURL->_flags & HAS_SCHEME)) {
        return CFURLGetPortNumber(anURL->_base);
    } else {
        return -1;
    }
}

CFStringRef  CFURLCopyUserName(CFURLRef  anURL) {
    CFStringRef user;
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedUser");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        return NULL;
    }
    user = _retainedComponentString(anURL, HAS_USER);
    if (user) {
        CFStringRef result = CFURLCreateStringByReplacingPercentEscapes(__CFURLGetAllocator(anURL), user, CFSTR(""));
        CFRelease(user);
        return result;
    } else if (anURL->_base && !(anURL->_flags & NET_LOCATION_MASK) && !(anURL->_flags & HAS_SCHEME)) {
        return CFURLCopyUserName(anURL->_base);
    } else {
        return NULL;
    }
}

CFStringRef  CFURLCopyPassword(CFURLRef  anURL) {
    CFStringRef passwd;
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedPassword");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        return NULL;
    }
    passwd = _retainedComponentString(anURL, HAS_PASSWORD);
    if (passwd) {
        CFStringRef result = CFURLCreateStringByReplacingPercentEscapes(__CFURLGetAllocator(anURL), passwd, CFSTR(""));
        CFRelease(passwd);
        return result;
    } else if (anURL->_base && !(anURL->_flags & NET_LOCATION_MASK) && !(anURL->_flags & HAS_SCHEME)) {
        return CFURLCopyPassword(anURL->_base);
    } else {
        return NULL;
    }
}

// The NSURL methods do not deal with escaping escape characters at all; therefore, in order to properly bridge NSURL methods, and still provide the escaping behavior that we want, we need to create functions that match the ObjC behavior exactly, and have the public CFURL... functions call these. -- REW, 10/29/98

static CFStringRef  _unescapedParameterString(CFURLRef  anURL) {
    CFStringRef str;
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedParameterString");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        return NULL;
    }
    str = _retainedComponentString(anURL, HAS_PARAMETERS);
    if (str) return str;
    if (!(anURL->_flags & IS_DECOMPOSABLE)) return NULL;
    if (!anURL->_base || (anURL->_flags & (NET_LOCATION_MASK | HAS_PATH | HAS_SCHEME))) {
        return NULL;
        // Parameter string definitely coming from the relative portion of the URL
    }
    return _unescapedParameterString(anURL->_base);
}

CFStringRef  CFURLCopyParameterString(CFURLRef  anURL, CFStringRef  charactersToLeaveEscaped) {
    CFStringRef  param = _unescapedParameterString(anURL);
    if (param) {
        CFStringRef result = CFURLCreateStringByReplacingPercentEscapes(CFGetAllocator(anURL), param, charactersToLeaveEscaped);
        CFRelease(param);
        return result;
    }
    return NULL;
}

static CFStringRef  _unescapedQueryString(CFURLRef  anURL) {
    CFStringRef str;
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedQuery");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        return NULL;
    }
    str = _retainedComponentString(anURL, HAS_QUERY);
    if (str) return str;
    if (!(anURL->_flags & IS_DECOMPOSABLE)) return NULL;
    if (!anURL->_base || (anURL->_flags & (HAS_SCHEME | NET_LOCATION_MASK | HAS_PATH | HAS_PARAMETERS))) {
        return NULL;
    }
    return _unescapedQueryString(anURL->_base);
}

CFStringRef  CFURLCopyQueryString(CFURLRef  anURL, CFStringRef  charactersToLeaveEscaped) {
    CFStringRef  query = _unescapedQueryString(anURL);
    if (query) {
        CFStringRef tmp = CFURLCreateStringByReplacingPercentEscapes(CFGetAllocator(anURL), query, charactersToLeaveEscaped);
        CFRelease(query);
        return tmp;
    }
    return NULL;
}

// Fragments are NEVER taken from a base URL
static CFStringRef  _unescapedFragment(CFURLRef  anURL) {
    CFStringRef str;
    CF_OBJC_FUNCDISPATCH0(URL, CFStringRef  , anURL, "_retainedFragment");
    __CFGenericValidateType(anURL, __kCFURLTypeID);
    if (URL_PATH_TYPE(anURL) != FULL_URL_REPRESENTATION) {
        return NULL;
    }
    str = _retainedComponentString(anURL, HAS_FRAGMENT);
    return str;
}

CFStringRef  CFURLCopyFragment(CFURLRef  anURL, CFStringRef  charactersToLeaveEscaped) {
    CFStringRef  fragment = _unescapedFragment(anURL);
    if (fragment) {
        CFStringRef tmp = CFURLCreateStringByReplacingPercentEscapes(CFGetAllocator(anURL), fragment, charactersToLeaveEscaped);
        CFRelease(fragment);
        return tmp;
    }
    return NULL;
}

CF_EXPORT void *__CFURLReservedPtr(CFURLRef  url) {
    return url->_reserved;
}

CF_EXPORT void __CFURLSetReservedPtr(CFURLRef  url, void *ptr) {
    ((struct __CFURL *)url)->_reserved = ptr;
}


/* File system stuff */

// Characters illegal in a path component are ';', ':', '?', '#', '%', '/'
// Also the whitespace characters, although strictly legal, are dangerous.  Adding ' ', '\t', '\n'.  REW, 1/5/2000
// Also high-byte characters (> 0x7F); we do not want to let those end up in the URL's string  REW, 2/28/2000
// Returns NULL if str cannot be converted for whatever reason, str if str contains no characters in need of escaping, or a newly-created string with the appropriate % escape codes in place.  Caller must always release the returned string.
CF_INLINE CFStringRef _replacePathIllegalCharacters(CFStringRef str, CFAllocatorRef alloc, Boolean preserveSlashes) {
    static const UniChar pathIllegalChars[9] = {';', ':', '?', '#', '%', ' ', '\t', '\n', '/'};
    return _convertPercentEscapes(str, 0, kCFStringEncodingUTF8, TRUE, TRUE, pathIllegalChars, preserveSlashes ? 8 : 9);
}

/* HFSPath<->URLPath functions at the bottom of the file */

static CFStringRef WindowsPathToURLPath(CFStringRef path, CFAllocatorRef alloc, Boolean isDir) {
    CFArrayRef tmp;
    CFMutableArrayRef urlComponents = NULL;
    CFStringRef str;
    UInt32 i=0, c;

    if (CFStringGetLength(path) == 0) return CFStringCreateWithCString(alloc, "", CFStringGetSystemEncoding());
    tmp = CFStringCreateArrayBySeparatingStrings(alloc, path, CFSTR("\\"));
    urlComponents = CFArrayCreateMutableCopy(alloc, 0, tmp);
    CFRelease(tmp);
    str = CFArrayGetValueAtIndex(urlComponents, 0);
    if (CFStringGetLength(str) == 2 && CFStringGetCharacterAtIndex(str, 1) == ':') {
        CFStringRef newComponent = CFStringCreateWithFormat(alloc, NULL, CFSTR("%c|"), CFStringGetCharacterAtIndex(str, 0));
        CFArraySetValueAtIndex(urlComponents, 0, newComponent);
        CFRelease(newComponent);
        CFArrayInsertValueAtIndex(urlComponents, 0, CFSTR("")); // So we get a leading '/' below
        i = 2; // Skip over the drive letter and the empty string we just inserted
    }

    for (c = CFArrayGetCount(urlComponents); i < c; i ++) {
        CFStringRef fileComp = CFArrayGetValueAtIndex(urlComponents,i);
        CFStringRef urlComp = _replacePathIllegalCharacters(fileComp, alloc, FALSE);
        if (!urlComp) {
            // Couldn't decode fileComp
            CFRelease(urlComponents);
            return NULL;
        }
        if (urlComp != fileComp) {
            CFArraySetValueAtIndex(urlComponents, i, urlComp);
        }
        CFRelease(urlComp);
    }
    if (isDir) {
        if (CFStringGetLength(CFArrayGetValueAtIndex(urlComponents, c-1)) != 0)
            CFArrayAppendValue(urlComponents, CFSTR(""));
    }
    str = CFStringCreateByCombiningStrings(alloc, urlComponents, CFSTR("/"));
    CFRelease(urlComponents);
    return str;
}

static CFStringRef POSIXPathToURLPath(CFStringRef path, CFAllocatorRef alloc, Boolean isDirectory) {
    CFStringRef pathString = _replacePathIllegalCharacters(path, alloc, TRUE);
    if (isDirectory && CFStringGetCharacterAtIndex(path, CFStringGetLength(path)-1) != '/') {
        CFStringRef tmp = CFStringCreateWithFormat(alloc, NULL, CFSTR("%@/"), pathString);
        CFRelease(pathString);
        pathString = tmp;
    }
    return pathString;
}

static CFStringRef URLPathToPOSIXPath(CFStringRef path, CFAllocatorRef allocator) {
    // This is the easiest case; just remove the percent escape codes and we're done
    CFIndex length;
    CFStringRef result = CFURLCreateStringByReplacingPercentEscapes(allocator, path, CFSTR(""));
    if (result) {
        length = CFStringGetLength(result);
        if (CFStringGetCharacterAtIndex(result, length-1) == '/') {
            CFStringRef tmp = CFStringCreateWithSubstring(allocator, result, CFRangeMake(0, length-1));
            CFRelease(result);
            result = tmp;
        }
    }
    return result;
}


static CFStringRef URLPathToWindowsPath(CFStringRef path, CFAllocatorRef allocator) {
    // Check for a drive letter, then flip all the slashes
    CFStringRef result;
    CFArrayRef tmp = CFStringCreateArrayBySeparatingStrings(allocator, path, CFSTR("/"));
    SInt32 count = CFArrayGetCount(tmp);
    CFMutableArrayRef components = CFArrayCreateMutableCopy(allocator, count, tmp);
    CFStringRef newPath;
    
    CFRelease(tmp);
    if (CFStringGetLength(CFArrayGetValueAtIndex(components,count-1)) == 0) {
        CFArrayRemoveValueAtIndex(components, count-1);
        count --;
    }
    if (count > 1 && CFStringGetLength(CFArrayGetValueAtIndex(components, 0)) == 0) {
        // Absolute path; we need to remove the first component, and check for a drive letter in the second component
        CFStringRef firstComponent = CFURLCreateStringByReplacingPercentEscapes(allocator, CFArrayGetValueAtIndex(components, 1), CFSTR(""));
        CFArrayRemoveValueAtIndex(components, 0);
        if (CFStringGetLength(firstComponent) == 2 && CFStringGetCharacterAtIndex(firstComponent, 1) == '|') {
            // Drive letter
            CFStringRef driveStr = CFStringCreateWithFormat(allocator, NULL, CFSTR("%c:"), CFStringGetCharacterAtIndex(firstComponent, 0));
            CFArraySetValueAtIndex(components, 0, driveStr);
            CFRelease(driveStr);
        }
        CFRelease(firstComponent);
    }

    newPath = CFStringCreateByCombiningStrings(allocator, components, CFSTR("\\"));
    CFRelease(components);
    result = CFURLCreateStringByReplacingPercentEscapes(allocator, newPath, CFSTR(""));
    CFRelease(newPath);
    return result;
}

// converts url from a file system path representation to a standard representation
static void _convertToURLRepresentation(struct __CFURL *url) {
    CFStringRef path = NULL;
    Boolean isDir = ((url->_flags & IS_DIRECTORY) != 0);
    CFAllocatorRef alloc = __CFURLGetAllocator(url);

#if DEBUG_URL_MEMORY_USAGE
    numFileURLsConverted ++;
#endif

    switch (URL_PATH_TYPE(url)) {
        case kCFURLPOSIXPathStyle:
            if (url->_flags & POSIX_AND_URL_PATHS_MATCH) {
                path = CFRetain(url->_string);
            } else {
                path = POSIXPathToURLPath(url->_string, alloc, isDir);
            }
            break;
        case kCFURLHFSPathStyle:
            path = HFSPathToURLPath(url->_string, alloc, isDir);
            break;
        case kCFURLWindowsPathStyle:
            path = WindowsPathToURLPath(url->_string, alloc, isDir);
            break;
    }
    CFAssert2(path != NULL, __kCFLogAssertion, "%s(): Encountered malformed file system URL %@", __PRETTY_FUNCTION__, url);
    if (!url->_base) {
        CFStringRef str;
        str = CFStringCreateWithFormat(alloc, NULL, CFSTR("file://localhost%@"), path);
        url->_flags = (url->_flags & (HAS_CUSTOM_ALLOCATOR | IS_DIRECTORY)) | (FULL_URL_REPRESENTATION << 16) | IS_DECOMPOSABLE | IS_ABSOLUTE | IS_PARSED | HAS_SCHEME | HAS_HOST | HAS_PATH;
        CFRelease(url->_string);
        url->_string = str;
        url->ranges = (CFRange *)CFAllocatorAllocate(alloc, sizeof(CFRange) * 3, 0);
        url->ranges[0] = CFRangeMake(0, 4);
        url->ranges[1] = CFRangeMake(7, 9);
        url->ranges[2] = CFRangeMake(16, CFStringGetLength(path));
        CFRelease(path);
    } else {
        CFRelease(url->_string);
        url->_flags = (url->_flags & (HAS_CUSTOM_ALLOCATOR | IS_DIRECTORY)) | (FULL_URL_REPRESENTATION << 16) | IS_DECOMPOSABLE | IS_PARSED | HAS_PATH;
        url->_string = path;
        url->ranges = (CFRange *)CFAllocatorAllocate(alloc, sizeof(CFRange), 0);
        *(url->ranges) = CFRangeMake(0, CFStringGetLength(path));
    }
}

// relativeURL is known to be a file system URL whose base is a matching file system URL
static CFURLRef _CFURLCopyAbsoluteFileURL(CFURLRef relativeURL) {
    CFAllocatorRef alloc = __CFURLGetAllocator(relativeURL);
    CFURLPathStyle fsType = URL_PATH_TYPE(relativeURL);
    CFURLRef base = relativeURL->_base;
    CFStringRef newPath = _resolveFileSystemPaths(relativeURL->_string, base->_string, (base->_flags & IS_DIRECTORY) != 0, fsType, alloc);
    CFURLRef result =  CFURLCreateWithFileSystemPath(alloc, newPath, fsType, (relativeURL->_flags & IS_DIRECTORY) != 0);
    CFRelease(newPath);
    return result;
}

// Caller must release the returned string
static CFStringRef _resolveFileSystemPaths(CFStringRef relativePath, CFStringRef basePath, Boolean baseIsDir, CFURLPathStyle fsType, CFAllocatorRef alloc) {
    CFIndex baseLen = CFStringGetLength(basePath);
    CFIndex relLen = CFStringGetLength(relativePath);
    UniChar pathDelimiter = PATH_DELIM_FOR_TYPE(fsType);
    UniChar *buf = CFAllocatorAllocate(alloc, sizeof(UniChar)*(relLen + baseLen + 2), 0);
    CFStringGetCharacters(basePath, CFRangeMake(0, baseLen), buf);
    if (baseIsDir) {
        if (buf[baseLen-1] != pathDelimiter) {
            buf[baseLen] = pathDelimiter;
            baseLen ++;
        }
    } else {
        UniChar *ptr = buf + baseLen - 1;
        while (ptr > buf && *ptr != pathDelimiter) {
            ptr --;
        }
        baseLen = ptr - buf + 1;
    }
    if (fsType == kCFURLHFSPathStyle) {
        // HFS relative paths will begin with a colon, so we must remove the trailing colon from the base path first.
        baseLen --;
    }
    CFStringGetCharacters(relativePath, CFRangeMake(0, relLen), buf + baseLen);
    *(buf + baseLen + relLen) = '\0';
    return _resolvedPath(buf, buf + baseLen + relLen, pathDelimiter, TRUE, alloc);
}

#define _CFGetCurrentDirectory(A, B) (getcwd(A, B) != NULL)

static CFURLRef _CFURLCreateCurrentDirectoryURL(CFAllocatorRef allocator) {
    UInt8 buf[CFMaxPathSize + 1];
    if (_CFGetCurrentDirectory(buf, CFMaxPathLength)) {
        return CFURLCreateFromFileSystemRepresentation(allocator, buf, strlen(buf), TRUE);
    } else {
        return NULL;
    }
}

CFURLRef CFURLCreateWithFileSystemPath(CFAllocatorRef allocator, CFStringRef filePath, CFURLPathStyle fsType, Boolean isDirectory) {
    Boolean isAbsolute = TRUE;
    CFIndex len = CFStringGetLength(filePath);
    CFURLRef baseURL, result;

    CFAssert2(fsType == kCFURLPOSIXPathStyle || fsType == kCFURLHFSPathStyle || fsType == kCFURLWindowsPathStyle, __kCFLogAssertion, "%s(): encountered unknown path style %d", __PRETTY_FUNCTION__, fsType);
    CFAssert1(filePath != NULL, __kCFLogAssertion, "%s(): NULL filePath argument not permitted", __PRETTY_FUNCTION__);

    switch(fsType) {
        case kCFURLPOSIXPathStyle:
            isAbsolute = (len > 0 && CFStringGetCharacterAtIndex(filePath, 0) == '/');
            break;
        case kCFURLWindowsPathStyle:
            isAbsolute = (len > 3 && CFStringGetCharacterAtIndex(filePath, 1) == ':' && CFStringGetCharacterAtIndex(filePath, 2) == '\'');
            break;
        case kCFURLHFSPathStyle:
            isAbsolute = (len > 0 && CFStringGetCharacterAtIndex(filePath, 0) != ':');
            break;
    }
    if (isAbsolute) {
        baseURL = NULL;
    } else {
        baseURL = _CFURLCreateCurrentDirectoryURL(allocator);
    }
    result = CFURLCreateWithFileSystemPathRelativeToBase(allocator, filePath, fsType, isDirectory, baseURL);
    if (baseURL) CFRelease(baseURL);
    return result;
}

CF_EXPORT CFURLRef CFURLCreateWithFileSystemPathRelativeToBase(CFAllocatorRef allocator, CFStringRef filePath, CFURLPathStyle fsType, Boolean isDirectory, CFURLRef baseURL) {
    CFURLRef url;
    Boolean isAbsolute = TRUE, releaseFilePath = FALSE;
    UniChar pathDelim = '\0';
    CFIndex len = CFStringGetLength(filePath);
    
    CFAssert2(fsType == kCFURLPOSIXPathStyle || fsType == kCFURLHFSPathStyle || fsType == kCFURLWindowsPathStyle, __kCFLogAssertion, "%s(): encountered unknown path style %d", __PRETTY_FUNCTION__, fsType);
    
    switch(fsType) {
        case kCFURLPOSIXPathStyle:
            isAbsolute = (len > 0 && CFStringGetCharacterAtIndex(filePath, 0) == '/');
            pathDelim = '/';
            break;
        case kCFURLWindowsPathStyle: 
            isAbsolute = (len > 3 && CFStringGetCharacterAtIndex(filePath, 1) == ':' && CFStringGetCharacterAtIndex(filePath, 2) == '\'');
            pathDelim = '\\';
            
            break;
        case kCFURLHFSPathStyle: 
            isAbsolute = (len > 0 && CFStringGetCharacterAtIndex(filePath, 0) != ':');
            pathDelim = ':';
            break;
    }
    if (isAbsolute) {
        baseURL = NULL;
    } 
    if (isDirectory && len > 0 && CFStringGetCharacterAtIndex(filePath, len-1) != pathDelim) {
        filePath = CFStringCreateWithFormat(allocator, NULL, CFSTR("%@%c"), filePath, pathDelim);
        releaseFilePath = TRUE;
    } else if (!isDirectory && len > 0 && CFStringGetCharacterAtIndex(filePath, len-1) == pathDelim) {
        filePath = CFStringCreateWithSubstring(allocator, filePath, CFRangeMake(0, len-1));
        releaseFilePath = TRUE;
    }
    url = _CFURLAlloc(allocator);
    _CFURLInit((struct __CFURL *)url, filePath, fsType, baseURL);
    if (releaseFilePath) CFRelease(filePath);
    if (isDirectory) ((struct __CFURL *)url)->_flags |= IS_DIRECTORY;
    if (fsType == kCFURLPOSIXPathStyle) {
        // Check if relative path is equivalent to URL representation; this will be true if url->_string doesn't contain any illegal path components
        // Characters illegal in a path component are ';', ':', '?', '#', '%'
        CFStringInlineBuffer buf;
        CFIndex index, length = CFStringGetLength(url->_string);
        CFStringInitInlineBuffer(url->_string, &buf, CFRangeMake(0, length));
        for (index = 0; index < length; index ++) {
            UniChar ch = CFStringGetCharacterFromInlineBuffer(&buf, index);
            if (ch == ';' || ch == ':' || ch == '?' || ch == '#' || ch == '%' || ch == ' ' || ch == '\t' || ch == '\n')
                break;
        }
        if (index == length) {
            ((struct __CFURL *)url)->_flags |= POSIX_AND_URL_PATHS_MATCH;
        }
    }
    return url;
}

CFStringRef CFURLCopyFileSystemPath(CFURLRef anURL, CFURLPathStyle pathStyle) {
    CFAssert2(pathStyle == kCFURLPOSIXPathStyle || pathStyle == kCFURLHFSPathStyle || pathStyle == kCFURLWindowsPathStyle, __kCFLogAssertion, "%s(): Encountered unknown path style %d", __PRETTY_FUNCTION__, pathStyle);
    return CFURLCreateStringWithFileSystemPath(CFGetAllocator(anURL), anURL, pathStyle, FALSE);
}

// There is no matching ObjC method for this functionality; because this function sits on top of the CFURL primitives, it's o.k. not to check for the need to dispatch an ObjC method instead, but this means care must be taken that this function never call anything that will result in dereferencing anURL without first checking for an ObjC dispatch.  -- REW, 10/29/98
CFStringRef CFURLCreateStringWithFileSystemPath(CFAllocatorRef allocator, CFURLRef anURL, CFURLPathStyle fsType, Boolean resolveAgainstBase) {
    CFURLRef base = resolveAgainstBase ? CFURLGetBaseURL(anURL) : NULL;
    CFStringRef basePath = base ? CFURLCreateStringWithFileSystemPath(allocator, base, fsType, FALSE) : NULL;
    CFStringRef relPath = NULL;
    
    if (!CF_IS_OBJC(URL, anURL)) {
        // We can grope the ivars
        CFURLPathStyle myType = URL_PATH_TYPE(anURL);
        if (myType == fsType) {
            relPath = CFRetain(anURL->_string);
        } else if (fsType == kCFURLPOSIXPathStyle && myType == FULL_URL_REPRESENTATION) {
            if (!(anURL->_flags & IS_PARSED)) {
                _parseComponents((struct __CFURL *)anURL);
            }
            if (anURL->_flags & POSIX_AND_URL_PATHS_MATCH) {
                relPath = CFURLCopyPath(anURL);
            }
        }
    }

    if (relPath == NULL) {
        CFStringRef urlPath = CFURLCopyPath(anURL);
        switch (fsType) {
            case kCFURLPOSIXPathStyle:
                relPath = URLPathToPOSIXPath(urlPath, allocator);
                break;
            case kCFURLHFSPathStyle:
                relPath = URLPathToHFSPath(urlPath, allocator);
                break;
            case kCFURLWindowsPathStyle:
                relPath = URLPathToWindowsPath(urlPath, allocator);
                break;
            default:
                CFAssert2(TRUE, __kCFLogAssertion, "%s(): Received unknown path type %d", __PRETTY_FUNCTION__, fsType);
        }
        CFRelease(urlPath);
    }
    if (relPath && CFURLHasDirectoryPath(anURL) && CFStringGetCharacterAtIndex(relPath, CFStringGetLength(relPath)-1) == PATH_DELIM_FOR_TYPE(fsType)) {
        CFStringRef tmp = CFStringCreateWithSubstring(allocator, relPath, CFRangeMake(0, CFStringGetLength(relPath)-1));
        CFRelease(relPath);
        relPath = tmp;
    }

    // Note that !resolveAgainstBase implies !base
    if (!base || !relPath) {
        return relPath;
    } else {
        CFStringRef result = _resolveFileSystemPaths(relPath, basePath, CFURLHasDirectoryPath(base), fsType, allocator);
        CFRelease(basePath);
        CFRelease(relPath);
        return result;
    }
}

Boolean CFURLGetFileSystemRepresentation(CFURLRef url, Boolean resolveAgainstBase, UInt8 *buffer, CFIndex bufLen) {
    CFIndex usedLen;
    CFStringRef path;
    CFAllocatorRef alloc = CFGetAllocator(url);

    if (!url) return FALSE;
#if defined(__WIN32__)
    path = CFURLCreateStringWithFileSystemPath(alloc, url, kCFURLWindowsPathStyle, resolveAgainstBase);
#elif defined(__MACOS8__)
    path = CFURLCreateStringWithFileSystemPath(alloc, url, kCFURLHFSPathStyle, resolveAgainstBase);
#else
    path = CFURLCreateStringWithFileSystemPath(alloc, url, kCFURLPOSIXPathStyle, resolveAgainstBase);
#endif
    if (path) {
        CFIndex pathLen = CFStringGetLength(path);
        CFIndex numConverted = CFStringGetBytes(path, CFRangeMake(0, pathLen), CFStringFileSystemEncoding(), 0, TRUE, buffer, bufLen-1, &usedLen); // -1 because we need one byte to zero-terminate.
        CFRelease(path);
        if (numConverted == pathLen) {
            buffer[usedLen] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

CFURLRef CFURLCreateFromFileSystemRepresentation(CFAllocatorRef allocator, const UInt8 *buffer, CFIndex bufLen, Boolean isDirectory) {
    CFStringRef path = CFStringCreateWithBytes(allocator, buffer, bufLen, CFStringFileSystemEncoding(), FALSE);
    CFURLRef newURL;
    if (!path) return NULL;
#if defined(__WIN32__)
    newURL = CFURLCreateWithFileSystemPath(allocator, path, kCFURLWindowsPathStyle, isDirectory);
#elif defined(__MACOS8__)
    newURL = CFURLCreateWithFileSystemPath(allocator, path, kCFURLHFSPathStyle, isDirectory);
#else
    newURL = CFURLCreateWithFileSystemPath(allocator, path, kCFURLPOSIXPathStyle, isDirectory);
#endif
    CFRelease(path);
    return newURL;
}

CF_EXPORT CFURLRef CFURLCreateFromFileSystemRepresentationRelativeToBase(CFAllocatorRef allocator, const UInt8 *buffer, CFIndex bufLen, Boolean isDirectory, CFURLRef baseURL) {
    CFStringRef path = CFStringCreateWithBytes(allocator, buffer, bufLen, CFStringFileSystemEncoding(), FALSE);
    CFURLRef newURL;
    if (!path) return NULL;
#if defined(__WIN32__)
    newURL = CFURLCreateWithFileSystemPathRelativeToBase(allocator, path, kCFURLWindowsPathStyle, isDirectory, baseURL);
#elif defined(__MACOS8__)
    newURL = CFURLCreateWithFileSystemPathRelativeToBase(allocator, path, kCFURLHFSPathStyle, isDirectory, baseURL);
#else
    newURL = CFURLCreateWithFileSystemPathRelativeToBase(allocator, path, kCFURLPOSIXPathStyle, isDirectory, baseURL);
#endif
    CFRelease(path);
    return newURL;
}


/******************************/
/* Support for path utilities */
/******************************/

// Assumes url is a CFURL (not an Obj-C NSURL)
static CFRange _rangeOfLastPathComponent(CFURLRef url) {
    UInt32 pathType = URL_PATH_TYPE(url);
    CFRange pathRg, componentRg;
    
    if (pathType ==  FULL_URL_REPRESENTATION) {
        if (!(url->_flags & IS_PARSED)) _parseComponents((struct __CFURL *)url);
        pathRg = _rangeForComponent(url, HAS_PATH);
    } else {
        pathRg = CFRangeMake(0, CFStringGetLength(url->_string));
    }

    if (pathRg.location == -1 || pathRg.length == 0) {
        // No path
        return pathRg;
    }
    if (CFStringGetCharacterAtIndex(url->_string, pathRg.location + pathRg.length - 1) == PATH_DELIM_FOR_TYPE(pathType)) {
        pathRg.length --;
        if (pathRg.length == 0) {
            pathRg.length ++;
            return pathRg;
        }
    }
    if (CFStringFindWithOptions(url->_string, PATH_DELIM_AS_STRING_FOR_TYPE(pathType), pathRg, kCFCompareBackwards, &componentRg)) {
        componentRg.location ++;
        componentRg.length = pathRg.location + pathRg.length - componentRg.location;
    } else {
        componentRg = pathRg;
    }
    return componentRg;
}

CFStringRef CFURLCopyLastPathComponent(CFURLRef url) {
    CFStringRef result;

    if (CF_IS_OBJC(URL, url)) {
        CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
        CFIndex length;
        CFRange rg, compRg;
        if (!path) return NULL;
        rg = CFRangeMake(0, CFStringGetLength(path));
        length = rg.length; // Remember this for comparison later
        if (CFStringGetCharacterAtIndex(path, rg.length - 1) == '/') {
            rg.length --;
        }
        if (CFStringFindWithOptions(path, CFSTR("/"), rg, kCFCompareBackwards, &compRg)) {
            rg.length = rg.location + rg.length - (compRg.location+1);
            rg.location = compRg.location + 1;
        }
        if (rg.location == 0 && rg.length == length) {
            result = path;
        } else {
            result = CFStringCreateWithSubstring(NULL, path, rg);
            CFRelease(path);
        }
    } else {
        CFRange rg = _rangeOfLastPathComponent(url);
        if (rg.location == -1 || rg.length == 0) {
            // No path
            return CFRetain(CFSTR(""));
        }
        if (rg.length == 1 && CFStringGetCharacterAtIndex(url->_string, rg.location) == PATH_DELIM_FOR_TYPE(URL_PATH_TYPE(url))) {
            return CFRetain(CFSTR("/"));
        }
        result = CFStringCreateWithSubstring(__CFURLGetAllocator(url), url->_string, rg);
        if (URL_PATH_TYPE(url) == FULL_URL_REPRESENTATION && !(url->_flags & POSIX_AND_URL_PATHS_MATCH)) {
            CFStringRef tmp = CFURLCreateStringByReplacingPercentEscapes(__CFURLGetAllocator(url), result, CFSTR(""));
            CFRelease(result);
            result = tmp;
        }
    }
    return result;
}

CFStringRef CFURLCopyPathExtension(CFURLRef url) {
    CFStringRef lastPathComp = CFURLCopyLastPathComponent(url);
    CFStringRef ext = NULL;

    if (lastPathComp) {
        CFRange rg = CFStringFind(lastPathComp, CFSTR("."), kCFCompareBackwards);
        if (rg.location != -1) {
            rg.location ++;
            rg.length = CFStringGetLength(lastPathComp) - rg.location;
            if (rg.length > 0) {
                ext = CFStringCreateWithSubstring(CFGetAllocator(url), lastPathComp, rg);
            } else {
                ext = CFRetain(CFSTR(""));
            }
        }
        CFRelease(lastPathComp);
    }
    return ext;
}

CFURLRef CFURLCreateCopyAppendingPathComponent(CFAllocatorRef allocator, CFURLRef url, CFStringRef pathComponent, Boolean isDirectory) {
    UInt32 fsType;
    CFURLRef result;
    if (CF_IS_OBJC(URL, url)) {
        CFURLRef urlCopy = CFURLCreateWithString(allocator, CFURLGetString(url), CFURLGetBaseURL(url));
        if (urlCopy) {
            result = CFURLCreateCopyAppendingPathComponent(allocator, urlCopy, pathComponent, isDirectory);
            CFRelease(urlCopy);
        } else {
            result = NULL;
        }
    } else {
        __CFGenericValidateType(url, __kCFURLTypeID);
        CFAssert1(pathComponent != NULL, __kCFLogAssertion, "%s(): Cannot be called with a NULL component to append", __PRETTY_FUNCTION__);

        fsType = URL_PATH_TYPE(url);
        if (fsType != FULL_URL_REPRESENTATION && CFStringFindWithOptions(pathComponent, PATH_DELIM_AS_STRING_FOR_TYPE(fsType), CFRangeMake(0, CFStringGetLength(pathComponent)), 0, NULL)) {
            // Must convert to full representation, and then work with it
            fsType = FULL_URL_REPRESENTATION;
            _convertToURLRepresentation((struct __CFURL *)url);
        }

        if (fsType == FULL_URL_REPRESENTATION) {
            CFMutableStringRef newString;
            CFStringRef newComp;
            CFRange pathRg;
            if (!(url->_flags & IS_PARSED)) _parseComponents((struct __CFURL *)url);
            if (!(url->_flags & HAS_PATH)) return NULL;

            newString = CFStringCreateMutableCopy(allocator, 0, url->_string);
            newComp = _replacePathIllegalCharacters(pathComponent, allocator, FALSE);
            pathRg = _rangeForComponent(url, HAS_PATH);
            if (!pathRg.length || CFStringGetCharacterAtIndex(url->_string, pathRg.location + pathRg.length - 1) != '/') {
                CFStringInsert(newString, pathRg.location + pathRg.length, CFSTR("/"));
                pathRg.length ++;
            }
            CFStringInsert(newString, pathRg.location + pathRg.length, newComp);
            if (isDirectory) {
                CFStringInsert(newString, pathRg.location + pathRg.length + CFStringGetLength(newComp), CFSTR("/"));
            }
            CFRelease(newComp);
            result = CFURLCreateWithString(allocator, newString, url->_base);
            CFRelease(newString);
        } else {
            UniChar pathDelim = PATH_DELIM_FOR_TYPE(fsType);
            CFStringRef newString;
            if (CFStringGetCharacterAtIndex(url->_string, CFStringGetLength(url->_string) - 1) != pathDelim) {
                if (isDirectory) {
                    newString = CFStringCreateWithFormat(allocator, NULL, CFSTR("%@%c%@%c"), url->_string, pathDelim, pathComponent, pathDelim);
                } else {
                    newString = CFStringCreateWithFormat(allocator, NULL, CFSTR("%@%c%@"), url->_string, pathDelim, pathComponent);
                }
            } else {
                if (isDirectory) {
                    newString = CFStringCreateWithFormat(allocator, NULL, CFSTR("%@%@%c"), url->_string, pathComponent, pathDelim);
                } else {
                    newString = CFStringCreateWithFormat(allocator, NULL, CFSTR("%@%@"), url->_string, pathComponent);
                }
            }
            result = CFURLCreateWithFileSystemPathRelativeToBase(allocator, newString, fsType, isDirectory, url->_base);
            CFRelease(newString);
        }
    }
    return result;
}

CFURLRef CFURLCreateCopyDeletingLastPathComponent(CFAllocatorRef allocator, CFURLRef url) {
    CFURLRef result;
    CFMutableStringRef newString;
    CFRange lastCompRg, pathRg;
    Boolean appendDotDot = FALSE;
    UInt32 fsType;

    if (CF_IS_OBJC(URL, url)) {
        CFURLRef urlCopy = CFURLCreateWithString(allocator, CFURLGetString(url), CFURLGetBaseURL(url));
        if (urlCopy) {
            result = CFURLCreateCopyDeletingLastPathComponent(allocator, urlCopy);
            CFRelease(urlCopy);
        } else {
            result = NULL;
        }
    } else {
        CFAssert1(url != NULL, __kCFLogAssertion, "%s(): NULL argument not allowed", __PRETTY_FUNCTION__);
        __CFGenericValidateType(url, __kCFURLTypeID);

        fsType = URL_PATH_TYPE(url);
        if (fsType == FULL_URL_REPRESENTATION) {
            if (!(url->_flags & IS_PARSED)) _parseComponents((struct __CFURL *)url);
            if (!(url->_flags & HAS_PATH)) return NULL;
            pathRg = _rangeForComponent(url, HAS_PATH);
        } else {
            pathRg = CFRangeMake(0, CFStringGetLength(url->_string));
        }
        lastCompRg = _rangeOfLastPathComponent(url);
        if (lastCompRg.length == 0) {
            appendDotDot = TRUE;
        } else if (lastCompRg.length == 1) {
            UniChar ch = CFStringGetCharacterAtIndex(url->_string, lastCompRg.location);
            if (ch == '.' || ch == PATH_DELIM_FOR_TYPE(fsType)) {
                appendDotDot = TRUE;
            }
        } else if (lastCompRg.length == 2 && CFStringGetCharacterAtIndex(url->_string, lastCompRg.location) == '.' && CFStringGetCharacterAtIndex(url->_string, lastCompRg.location+1) == '.') {
            appendDotDot = TRUE;
        }

        newString = CFStringCreateMutableCopy(allocator, 0, url->_string);
        if (appendDotDot) {
            CFIndex delta = 0;
            if (pathRg.length > 0 && CFStringGetCharacterAtIndex(url->_string, pathRg.location + pathRg.length - 1) != PATH_DELIM_FOR_TYPE(fsType)) {
                CFStringInsert(newString, pathRg.location + pathRg.length, PATH_DELIM_AS_STRING_FOR_TYPE(fsType));
                delta ++;
            }
            CFStringInsert(newString, pathRg.location + pathRg.length + delta, CFSTR(".."));
            delta += 2;
            CFStringInsert(newString, pathRg.location + pathRg.length + delta, PATH_DELIM_AS_STRING_FOR_TYPE(fsType));
        } else {
            CFStringDelete(newString, CFRangeMake(lastCompRg.location, pathRg.location + pathRg.length - lastCompRg.location));
        }
        if (fsType == FULL_URL_REPRESENTATION) {
            result = CFURLCreateWithString(allocator, newString, url->_base);
        } else {
            result = CFURLCreateWithFileSystemPathRelativeToBase(allocator, newString, fsType, TRUE, url->_base);
        }
        CFRelease(newString);
    }
    return result;
}

CFURLRef CFURLCreateCopyAppendingPathExtension(CFAllocatorRef allocator, CFURLRef url, CFStringRef extension) {
    CFMutableStringRef newString;
    CFURLRef result;
    CFRange rg;
    CFURLPathStyle fsType;
    
    CFAssert1(url != NULL && extension != NULL, __kCFLogAssertion, "%s(): NULL argument not allowed", __PRETTY_FUNCTION__);
    if (CF_IS_OBJC(URL, url)) {
        CFURLRef urlCopy = CFURLCreateWithString(allocator, CFURLGetString(url), CFURLGetBaseURL(url));
        if (urlCopy) {
            result = CFURLCreateCopyAppendingPathExtension(allocator, urlCopy, extension);
            CFRelease(urlCopy);
        } else {
            result = NULL;
        }
    } else {
        __CFGenericValidateType(url, __kCFURLTypeID);
        __CFGenericValidateType(extension, __kCFStringTypeID);

        rg = _rangeOfLastPathComponent(url);
        if (rg.location < 0) return NULL; // No path
        fsType = URL_PATH_TYPE(url);
        if (fsType != FULL_URL_REPRESENTATION && CFStringFindWithOptions(extension, PATH_DELIM_AS_STRING_FOR_TYPE(fsType), CFRangeMake(0, CFStringGetLength(extension)), 0, NULL)) {
            _convertToURLRepresentation((struct __CFURL *)url);
            fsType = FULL_URL_REPRESENTATION;
            rg = _rangeOfLastPathComponent(url);
        }

        newString = CFStringCreateMutableCopy(allocator, 0, url->_string);
        CFStringInsert(newString, rg.location + rg.length, CFSTR("."));
        if (fsType == FULL_URL_REPRESENTATION) {
            CFStringRef newExt = _replacePathIllegalCharacters(extension, allocator, FALSE);
            CFStringInsert(newString, rg.location + rg.length + 1, newExt);
            CFRelease(newExt);
            result =  CFURLCreateWithString(allocator, newString, url->_base);
        } else {
            CFStringInsert(newString, rg.location + rg.length + 1, extension);
            result = CFURLCreateWithFileSystemPathRelativeToBase(allocator, newString, fsType, (url->_flags & IS_DIRECTORY) != 0 ? TRUE : FALSE, url->_base);
        }
        CFRelease(newString);
    }
    return result;
}

CFURLRef CFURLCreateCopyDeletingPathExtension(CFAllocatorRef allocator, CFURLRef url) {
    CFRange rg, dotRg;
    CFURLRef result;

    CFAssert1(url != NULL, __kCFLogAssertion, "%s(): NULL argument not allowed", __PRETTY_FUNCTION__);
    if (CF_IS_OBJC(URL, url)) {
        CFURLRef urlCopy = CFURLCreateWithString(allocator, CFURLGetString(url), CFURLGetBaseURL(url));
        if (urlCopy) {
            result = CFURLCreateCopyDeletingPathExtension(allocator, urlCopy);
            CFRelease(urlCopy);
        } else {
            result = NULL;
        }
    } else {
        __CFGenericValidateType(url, __kCFURLTypeID);
        rg = _rangeOfLastPathComponent(url);
        if (rg.location < 0) {
            result = NULL;
        } else if (rg.length && CFStringFindWithOptions(url->_string, CFSTR("."), rg, kCFCompareBackwards, &dotRg)) {
            CFMutableStringRef newString = CFStringCreateMutableCopy(allocator, 0, url->_string);
            dotRg.length = rg.location + rg.length - dotRg.location;
            CFStringDelete(newString, dotRg);
            if (URL_PATH_TYPE(url) == FULL_URL_REPRESENTATION) {
                result = CFURLCreateWithString(allocator, newString, url->_base);
            } else {
                result = CFURLCreateWithFileSystemPathRelativeToBase(allocator, newString, URL_PATH_TYPE(url), (url->_flags & IS_DIRECTORY) != 0 ? TRUE : FALSE, url->_base);
            }
            CFRelease(newString);
        } else {
            result = CFRetain(url);
        }
    }
    return result;
}


static CFStringRef HFSPathToURLPath(CFStringRef path, CFAllocatorRef alloc, Boolean isDir) {
    CFArrayRef components = CFStringCreateArrayBySeparatingStrings(alloc, path, CFSTR(":"));
    CFMutableArrayRef newComponents = CFArrayCreateMutableCopy(alloc, 0, components);
    CFStringRef result;
    UInt32 i, cnt;

    if (CFStringGetCharacterAtIndex(path, 0) != ':') {
        CFArrayInsertValueAtIndex(newComponents, 0, CFSTR(""));
    } else {
        CFArrayRemoveValueAtIndex(newComponents, 0);
    }

    cnt = CFArrayGetCount(newComponents);
    for (i = 0; i < cnt; i ++) {
        CFStringRef comp = CFArrayGetValueAtIndex(newComponents, i);
        CFStringRef newComp = _replacePathIllegalCharacters(comp, alloc, FALSE);
        if (newComp != comp) {
            CFArraySetValueAtIndex(newComponents, i, newComp);
        }
        CFRelease(newComp);
    }
    if (isDir && CFStringGetLength(CFArrayGetValueAtIndex(newComponents, cnt-1)) != 0) {
            CFArrayAppendValue(newComponents, CFSTR(""));
    }

    result = CFStringCreateByCombiningStrings(alloc, newComponents, CFSTR("/"));
    CFRelease(components);
    CFRelease(newComponents);
    return result;
}

static CFStringRef URLPathToHFSPath(CFStringRef path, CFAllocatorRef allocator) {
    // Slashes become colons; escaped slashes stay slashes.
    CFArrayRef components = CFStringCreateArrayBySeparatingStrings(allocator, path, CFSTR("/"));
    CFMutableArrayRef mutableComponents = CFArrayCreateMutableCopy(allocator, 0, components);
    SInt32 count = CFArrayGetCount(mutableComponents);
    CFStringRef newPath, result;
    CFRelease(components);

    if (count && CFStringGetLength(CFArrayGetValueAtIndex(mutableComponents, count-1)) == 0) {
        CFArrayRemoveValueAtIndex(mutableComponents, count-1);
        count --;
    }
    // On MacOS absolute paths do NOT begin with colon while relative paths DO.
    if ((count > 0) && CFEqual(CFArrayGetValueAtIndex(mutableComponents, 0), CFSTR(""))) {
        CFArrayRemoveValueAtIndex(mutableComponents, 0);
    } else {
        CFArrayInsertValueAtIndex(mutableComponents, 0, CFSTR(""));
    }
    newPath = CFStringCreateByCombiningStrings(allocator, mutableComponents, CFSTR(":"));
    CFRelease(mutableComponents);
    result = CFURLCreateStringByReplacingPercentEscapes(allocator, newPath, CFSTR(""));
    CFRelease(newPath);
    return result;
}


