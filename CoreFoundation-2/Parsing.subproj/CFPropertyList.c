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
/*	CFPropertyList.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Becky Willrich
*/

#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFNumber.h>
#include "CFVeryPrivate.h"
#include "CFStringEncodingConverter.h"
#include "CFInternal.h"
#include <limits.h>
#include <string.h>
#include <ctype.h>


#define PLIST_IX    0
#define ARRAY_IX    1
#define DICT_IX     2
#define KEY_IX      3
#define STRING_IX   4
#define DATA_IX     5
#define DATE_IX     6
#define REAL_IX     7
#define INTEGER_IX  8
#define TRUE_IX     9
#define FALSE_IX    10

#define PLIST_TAG_LENGTH  5
#define ARRAY_TAG_LENGTH  5
#define DICT_TAG_LENGTH   4
#define KEY_TAG_LENGTH    3
#define STRING_TAG_LENGTH 6
#define DATA_TAG_LENGTH   4
#define DATE_TAG_LENGTH   4
#define REAL_TAG_LENGTH   4
#define INTEGER_TAG_LENGTH 7
#define TRUE_TAG_LENGTH   4
#define FALSE_TAG_LENGTH  5

#define __CFAssertIsPList(cf) CFAssert2(CFGetTypeID(cf) == CFStringGetTypeID() || CFGetTypeID(cf) == CFArrayGetTypeID() || CFGetTypeID(cf) == CFBooleanGetTypeID() || CFGetTypeID(cf) == CFNumberGetTypeID() || CFGetTypeID(cf) == CFDictionaryGetTypeID() || CFGetTypeID(cf) == CFDateGetTypeID() || CFGetTypeID(cf) == CFDataGetTypeID(), __kCFLogAssertion, "%s(): 0x%x not of a property list type", __PRETTY_FUNCTION__, (UInt32)cf);


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

static UniChar CFXMLPlistTags[11][8]= {
{'p', 'l', 'i', 's', 't', '\0', '\0', '\0'},
{'a', 'r', 'r', 'a', 'y', '\0', '\0', '\0'},
{'d', 'i', 'c', 't', '\0', '\0', '\0', '\0'},
{'k', 'e', 'y', '\0', '\0', '\0', '\0', '\0'},
{'s', 't', 'r', 'i', 'n', 'g', '\0', '\0'},
{'d', 'a', 't', 'a', '\0', '\0', '\0', '\0'},
{'d', 'a', 't', 'e', '\0', '\0', '\0', '\0'},
{'r', 'e', 'a', 'l', '\0', '\0', '\0', '\0'},
{'i', 'n', 't', 'e', 'g', 'e', 'r', '\0'},
{'t', 'r', 'u', 'e', '\0', '\0', '\0', '\0'},
{'f', 'a', 'l', 's', 'e', '\0', '\0', '\0'}    
};

typedef struct {
    const UniChar *begin; // first character of the XML to be parsed
    const UniChar *curr;  // current parse location
    const UniChar *end;   // the first character _after_ the end of the XML
    CFStringRef errorString;
    CFAllocatorRef allocator;
    UInt32 mutabilityOption;
    Boolean allowNewTypes; // Whether to allow the new types supported by XML property lists, but not by the old, OPENSTEP ASCII property lists (CFNumber, CFBoolean, CFDate)
} _CFXMLPlistParseInfo;

static CFTypeRef parseOldStylePropertyListOrStringsFile(_CFXMLPlistParseInfo *pInfo);

/* Base-64 encoding/decoding */

/* The base-64 encoding packs three 8-bit bytes into four 7-bit ASCII
 * characters.  If the number of bytes in the original data isn't divisable
 * by three, "=" characters are used to pad the encoded data.  The complete
 * set of characters used in base-64 are:
 *
 *      'A'..'Z' => 00..25
 *      'a'..'z' => 26..51
 *      '0'..'9' => 52..61
 *      '+'      => 62
 *      '/'      => 63
 *      '='      => pad
 */

/* A bit wasteful to do everything with unichars (since we know all the characters we're going to see are 7-bit ASCII), but since our data is coming from or going to a CFString, this prevents the extra cost of converting formats. */

static const signed char __CFPLDataDecodeTable[128] = {
    /* 000 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 010 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 020 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 030 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* ' ' */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* '(' */ -1, -1, -1, 62, -1, -1, -1, 63,
    /* '0' */ 52, 53, 54, 55, 56, 57, 58, 59,
    /* '8' */ 60, 61, -1, -1, -1,  0, -1, -1,
    /* '@' */ -1,  0,  1,  2,  3,  4,  5,  6,
    /* 'H' */  7,  8,  9, 10, 11, 12, 13, 14,
    /* 'P' */ 15, 16, 17, 18, 19, 20, 21, 22,
    /* 'X' */ 23, 24, 25, -1, -1, -1, -1, -1,
    /* '`' */ -1, 26, 27, 28, 29, 30, 31, 32,
    /* 'h' */ 33, 34, 35, 36, 37, 38, 39, 40,
    /* 'p' */ 41, 42, 43, 44, 45, 46, 47, 48,
    /* 'x' */ 49, 50, 51, -1, -1, -1, -1, -1
};

static CFDataRef __CFPLDataDecode(const UniChar *bytes, int length, Boolean mutable, CFAllocatorRef allocator) {
    int tmpbufpos = 0;
    int tmpbuflen = length * 3 / 4 + 4;
    UInt8 *tmpbuf;
    int numeq = 0;
    int acc = 0;
    int cntr = 0;

    /* ??? Allocator computation should happen at higher level; this is a quick fix */
    allocator = allocator ? allocator : CFAllocatorGetDefault();
    tmpbuf = CFAllocatorAllocate(allocator, tmpbuflen, 0);

    while (length--) {
        UniChar c = (*bytes++) & 0x7f;
        if ('=' == c) {
            numeq++;
        } else if (!isspace(c)) {
            numeq = 0;
        }
        if (__CFPLDataDecodeTable[c] < 0)
            continue;
        cntr++;
        acc <<= 6;
        acc += __CFPLDataDecodeTable[c];
        if (0 == (cntr & 0x3)) {
            if (tmpbuflen <= tmpbufpos + 2) {
                tmpbuflen <<= 2;
                tmpbuf = CFAllocatorReallocate(allocator, tmpbuf, tmpbuflen, 0);
            }
            tmpbuf[tmpbufpos++] = (acc >> 16) & 0xff;
            if (numeq < 2)
                tmpbuf[tmpbufpos++] = (acc >> 8) & 0xff;
            if (numeq < 1)
                tmpbuf[tmpbufpos++] = acc & 0xff;
        }
    }
    if (mutable) {
        CFMutableDataRef result = CFDataCreateMutable(allocator, 0);
        CFDataAppendBytes(result, tmpbuf, tmpbufpos);
	CFAllocatorDeallocate(allocator, tmpbuf);
        return result;
    } else {
        return CFDataCreateWithBytesNoCopy(allocator, (char const *) tmpbuf, tmpbufpos, allocator);
    }
}

typedef struct {
    UniChar *buf;
    int pos, len;
} ByteBuffer;

static const char __CFPLDataEncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static UniChar *encodeBase64(CFAllocatorRef allocator, CFDataRef inputData, CFIndex indent, UInt32 *len) {
    const UInt8 *bytes = CFDataGetBytePtr(inputData);
    int length = CFDataGetLength(inputData);
    int i, k, linelen, capacity;
    const UInt8 *p;
    ByteBuffer bbuf;

    if (indent > 8) indent = 8; // refuse to indent more than 64 characters
    capacity = length * 4 / 3;
    // plus number of lines time extra bytes per line, plus a bit
    capacity += ((capacity / (76 - indent * 8)) + 1) * (1 + indent) + 50;
    bbuf.buf = (UniChar *)CFAllocatorAllocate(allocator, capacity * sizeof(UniChar), 0);
    bbuf.len = capacity;
    bbuf.pos = 0;

    for (k = 0; k < indent; k++) {
	bbuf.buf[bbuf.pos++] = '\t';
    }
    linelen = 8 * indent;
    for (i = 0, p = bytes; i < length; i++, p++) {
        /* 3 bytes are encoded as 4 */
        switch (i % 3) {
          case 0:
            bbuf.buf[bbuf.pos++] = __CFPLDataEncodeTable [ ((p[0] >> 2) & 0x3f)];
            linelen++;
            break;
          case 1:
            bbuf.buf[bbuf.pos++] = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 4) & 0x3f)];
            linelen++;
            break;
          case 2:
            bbuf.buf[bbuf.pos++] = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 6) & 0x3f)];
            bbuf.buf[bbuf.pos++] = __CFPLDataEncodeTable [ (p[0] & 0x3f)];
            linelen += 2;
            break;
        }
        /* Insert a newline every 76 chars */
        if (linelen >= 76) {
            bbuf.buf[bbuf.pos++] = '\n';
	    for (k = 0; k < indent; k++) {
		bbuf.buf[bbuf.pos++] = '\t';
	    }
	    linelen = 8 * indent;
        }
    }
    switch (i % 3) {
      case 0:
        break;
      case 1:
        bbuf.buf[bbuf.pos++] = __CFPLDataEncodeTable [ ((p[-1] << 4) & 0x30)];
        bbuf.buf[bbuf.pos++] = '=';
        bbuf.buf[bbuf.pos++] = '=';
        linelen += 3;
        break;
      case 2:
        bbuf.buf[bbuf.pos++] =  __CFPLDataEncodeTable [ ((p[-1] << 2) & 0x3c)];
        bbuf.buf[bbuf.pos++] = '=';
        linelen += 2;
        break;
    }
    if (linelen != 0)
        bbuf.buf[bbuf.pos++] = '\n';
    if (len) *len = bbuf.pos;
    return bbuf.buf;
}

static void _XMLPListAppendProlog(CFMutableStringRef result) {
    CFStringAppendCString(result, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE ", kCFStringEncodingASCII);
    CFStringAppendCharacters(result, CFXMLPlistTags[PLIST_IX], PLIST_TAG_LENGTH);
    CFStringAppendCString(result, " SYSTEM \"file://localhost/System/Library/DTDs/PropertyList.dtd\">\n<", kCFStringEncodingASCII);
    CFStringAppendCharacters(result, CFXMLPlistTags[PLIST_IX], PLIST_TAG_LENGTH);
    CFStringAppendCString(result, " version=\"0.9\">\n", kCFStringEncodingASCII);
}

static void _appendIndents(UInt32 numIndents, CFMutableStringRef str) {
    // Horrendously inefficient; should speed this up later
    UInt32 i;
    UniChar ch = '\t';
    for (i = 0; i < numIndents; i ++) {
        CFStringAppendCharacters(str, &ch, 1);
    }
}

/* Append the escaped version of origStr to mStr.
*/
static void _appendEscapedString(CFStringRef origStr, CFMutableStringRef mStr) {
#define BUFSIZE 64
    CFIndex i, length = CFStringGetLength(origStr);
    CFIndex bufCnt = 0;
    UniChar buf[BUFSIZE];
    CFStringInlineBuffer inlineBuffer;

    CFStringInitInlineBuffer(origStr, &inlineBuffer, CFRangeMake(0, length));

    for (i = 0; i < length; i ++) {
	UniChar ch = CFStringGetCharacterFromInlineBuffer(&inlineBuffer, i);
        switch(ch) {
            case '<':
		if (bufCnt) CFStringAppendCharacters(mStr, buf, bufCnt);
		bufCnt = 0;
	  	CFStringAppendCString(mStr, "&lt;", kCFStringEncodingASCII);
                break;
            case '>':
		if (bufCnt) CFStringAppendCharacters(mStr, buf, bufCnt);
		bufCnt = 0;
	  	CFStringAppendCString(mStr, "&gt;", kCFStringEncodingASCII);
                break;
            case '&':
		if (bufCnt) CFStringAppendCharacters(mStr, buf, bufCnt);
		bufCnt = 0;
	  	CFStringAppendCString(mStr, "&amp;", kCFStringEncodingASCII);
                break;
            default:
		buf[bufCnt++] = ch;
		if (bufCnt == BUFSIZE) {
		    CFStringAppendCharacters(mStr, buf, bufCnt);
		    bufCnt = 0;
		}
		break;
        }
    }
    if (bufCnt) CFStringAppendCharacters(mStr, buf, bufCnt);
}

static void _NSAppendXML(CFTypeRef object, UInt32 indentation, CFMutableStringRef xmlString) {
    UInt32 typeID = CFGetTypeID(object);
    CFStringEncoding sysEncoding = kCFStringEncodingASCII;
    _appendIndents(indentation, xmlString);
    if (typeID == CFStringGetTypeID()) {
        CFStringAppendCString(xmlString, "<", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[STRING_IX], STRING_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">", sysEncoding);
	_appendEscapedString(object, xmlString);
        CFStringAppendCString(xmlString, "</", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[STRING_IX], STRING_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
    } else if (typeID == CFArrayGetTypeID()) {
        UInt32 i, count = CFArrayGetCount(object);
        if (count == 0) {
            CFStringAppendCString(xmlString, "<", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[ARRAY_IX], ARRAY_TAG_LENGTH);
            CFStringAppendCString(xmlString, "/>\n", sysEncoding);
            return;
        }
        CFStringAppendCString(xmlString, "<", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[ARRAY_IX], ARRAY_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
        for (i = 0; i < count; i ++) {
            _NSAppendXML(CFArrayGetValueAtIndex(object, i), indentation+1, xmlString);
        }
        _appendIndents(indentation, xmlString);
        CFStringAppendCString(xmlString, "</", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[ARRAY_IX], ARRAY_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
    } else if (typeID == CFDictionaryGetTypeID()) {
        UInt32 i, count = CFDictionaryGetCount(object);
	CFAllocatorRef allocator = CFGetAllocator(xmlString);
        CFTypeRef *keys;
        CFTypeRef *values;
        if (count == 0) {
            CFStringAppendCString(xmlString, "<", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[DICT_IX], DICT_TAG_LENGTH);
            CFStringAppendCString(xmlString, "/>\n", sysEncoding);
            return;
        }
        CFStringAppendCString(xmlString, "<", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[DICT_IX], DICT_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
        keys = (CFTypeRef *)CFAllocatorAllocate(allocator, count * sizeof(CFTypeRef), 0);
        values = (CFTypeRef *)CFAllocatorAllocate(allocator, count * sizeof(CFTypeRef), 0);
        CFDictionaryGetKeysAndValues(object, keys, values);
        for (i = 0; i < count; i ++) {
            _appendIndents(indentation+1, xmlString);
            CFStringAppendCString(xmlString, "<", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[KEY_IX], KEY_TAG_LENGTH);
            CFStringAppendCString(xmlString, ">", sysEncoding);
	    _appendEscapedString(keys[i], xmlString);
            CFStringAppendCString(xmlString, "</", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[KEY_IX], KEY_TAG_LENGTH);
            CFStringAppendCString(xmlString, ">\n", sysEncoding);
            _NSAppendXML(values[i], indentation+1, xmlString);
        }
        CFAllocatorDeallocate(allocator, keys);
        CFAllocatorDeallocate(allocator, values);
        _appendIndents(indentation, xmlString);
        CFStringAppendCString(xmlString, "</", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[DICT_IX], DICT_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
    } else if (typeID == CFDataGetTypeID()) {
        UInt32 length;
        UniChar *encodedData = encodeBase64(CFGetAllocator(xmlString), object, indentation, &length);
        CFStringAppendCString(xmlString, "<", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[DATA_IX], DATA_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
        CFStringAppendCharacters(xmlString, encodedData, length);
        _appendIndents(indentation, xmlString);
        CFStringAppendCString(xmlString, "</", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[DATA_IX], DATA_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
        CFAllocatorDeallocate(CFGetAllocator(xmlString), encodedData);
    } else if (typeID == CFDateGetTypeID()) {
        // YYYY '-' MM '-' DD 'T' hh ':' mm ':' ss 'Z'
        CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(CFDateGetAbsoluteTime(object), NULL);

        CFStringAppendCString(xmlString, "<", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[DATE_IX], DATE_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">", sysEncoding);
        
        CFStringAppendFormat(xmlString, NULL, CFSTR("%04d-%02d-%02dT%02d:%02d:%02dZ"), date.year, date.month, date.day, date.hour, date.minute, (int)date.second);

        CFStringAppendCString(xmlString, "</", sysEncoding);
        CFStringAppendCharacters(xmlString, CFXMLPlistTags[DATE_IX], DATE_TAG_LENGTH);
        CFStringAppendCString(xmlString, ">\n", sysEncoding);
    } else if (typeID == CFNumberGetTypeID()) {
        if (CFNumberIsFloatType(object)) {
            Float64 val64;
            Float32 val32;
            float   val;
            CFNumberType type = CFNumberGetType(object);
            switch (type) {
                case kCFNumberFloat64Type:
                    CFNumberGetValue(object, kCFNumberFloat64Type, &val64);
                    break;
                case kCFNumberFloat32Type:
                    CFNumberGetValue(object, kCFNumberFloat32Type, &val32);
                    break;
                case kCFNumberFloatType:
                    CFNumberGetValue(object, kCFNumberFloatType, &val);
                    break;
                default: {
                	CFAssert1(FALSE, __kCFLogAssertion, "encountered unexpected float type %d from CFNumber", type);
                    CFNumberGetValue(object, kCFNumberFloatType, &val);
                }
            }

            CFStringAppendCString(xmlString, "<", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[REAL_IX], REAL_TAG_LENGTH);
            CFStringAppendCString(xmlString, ">", sysEncoding);

            CFStringAppendFormat(xmlString, NULL, CFSTR("%g"), type == kCFNumberFloat64Type ? val64 : (type == kCFNumberFloat32Type ? val32 : val));

            CFStringAppendCString(xmlString, "</", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[REAL_IX], REAL_TAG_LENGTH);
            CFStringAppendCString(xmlString, ">\n", sysEncoding);
        } else {
            CFStringAppendCString(xmlString, "<", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[INTEGER_IX], INTEGER_TAG_LENGTH);
            CFStringAppendCString(xmlString, ">", sysEncoding);

            CFStringAppendFormat(xmlString, NULL, CFSTR("%@"), object);

            CFStringAppendCString(xmlString, "</", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[INTEGER_IX], INTEGER_TAG_LENGTH);
            CFStringAppendCString(xmlString, ">\n", sysEncoding);
        }
    } else if (typeID == CFBooleanGetTypeID()) {
        if (CFBooleanGetValue(object)) {
            CFStringAppendCString(xmlString, "<", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[TRUE_IX], TRUE_TAG_LENGTH);
            CFStringAppendCString(xmlString, "/>\n", sysEncoding);
        } else {
            CFStringAppendCString(xmlString, "<", sysEncoding);
            CFStringAppendCharacters(xmlString, CFXMLPlistTags[FALSE_IX], FALSE_TAG_LENGTH);
            CFStringAppendCString(xmlString, "/>\n", sysEncoding);
        }
    }
}

static void _CFGenerateXMLPropertyListToString(CFMutableStringRef xml, CFTypeRef propertyList) {
    _XMLPListAppendProlog(xml);

    _NSAppendXML(propertyList, 0, xml);

    CFStringAppendCString(xml, "</", kCFStringEncodingASCII);
    CFStringAppendCharacters(xml, CFXMLPlistTags[PLIST_IX], PLIST_TAG_LENGTH);
    CFStringAppendCString(xml, ">\n", kCFStringEncodingASCII);
}


CFDataRef CFPropertyListCreateXMLData(CFAllocatorRef allocator, CFPropertyListRef propertyList) {
    CFMutableStringRef xml;
    CFDataRef result;
    CFAssert1(propertyList != NULL, __kCFLogAssertion, "%s(): Cannot be called with a NULL property list", __PRETTY_FUNCTION__);
    __CFAssertIsPList(propertyList);

    xml = CFStringCreateMutable(allocator, 0);
    _CFGenerateXMLPropertyListToString(xml, propertyList);
    result = CFStringCreateExternalRepresentation(allocator, xml, kCFStringEncodingUTF8, 0);
    CFRelease(xml);
    return result;
}

//
// ------------------------- Reading plists ------------------
// 

static void skipInlineDTD(_CFXMLPlistParseInfo *pInfo);
static CFTypeRef parseXMLElement(_CFXMLPlistParseInfo *pInfo, Boolean *isKey);

static UInt32 lineNumber(_CFXMLPlistParseInfo *pInfo) {
    const UniChar *p = pInfo->begin;
    UInt32 count = 0;
    while (p < pInfo->curr) {
        if (*p == '\r') {
            count ++;
            if (*(p + 1) == '\n')
                p ++;
        } else if (*p == '\n') {
            count ++;
        }
        p ++;
    }
    return count;
}


CF_INLINE void skipWhitespace(_CFXMLPlistParseInfo *pInfo) {
    while (pInfo->curr < pInfo->end) {
        switch (*(pInfo->curr)) {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                pInfo->curr ++;
                continue;
            default:
                return;
        }
    }
}

/* All of these advance to the end of the given construct and return a pointer to the first character beyond the construct.  If the construct doesn't parse properly, NULL is returned. */

// pInfo should be just past "<!--"
static void skipXMLComment(_CFXMLPlistParseInfo *pInfo) {
    const UniChar *p = pInfo->curr;
    const UniChar *end = pInfo->end - 3; // Need at least 3 characters to compare against
    while (p < end) {
        if (*p == '-' && *(p+1) == '-' && *(p+2) == '>') {
            pInfo->curr = p+3;
            return;
        }
        p ++; 
    }
    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Unterminated comment started on line %d"), lineNumber(pInfo));
}

// stringToMatch and buf must both be of at least len
static Boolean matchString(const UniChar *buf, const UniChar *stringToMatch, UInt32 len) {
    const UniChar *end = buf+len;
    const UniChar *bPtr=buf, *sPtr=stringToMatch;
    // Seems like there should be some kind of memcmp that would be faster than this
    // No; because the number of characters is so small, this is faster that memcmp.
    while (bPtr < end) {
        if (*bPtr != *sPtr) return FALSE;
        bPtr ++;
        sPtr ++;
    }
    return TRUE;
}

// pInfo should be set to the first character after "<?"
static void skipXMLProcessingInstruction(_CFXMLPlistParseInfo *pInfo) {
    const UniChar *begin = pInfo->curr, *end = pInfo->end - 2; // Looking for "?>" so we need at least 2 characters
    while (pInfo->curr < end) {
        if (*(pInfo->curr) == '?' && *(pInfo->curr+1) == '>') {
            pInfo->curr += 2;
            return;
        }
        pInfo->curr ++; 
    }
    pInfo->curr = begin;
    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected EOF while parsing the processing instruction begun on line %d"), lineNumber(pInfo));
}

static UniChar _DoctypeOpening[7] = {'D', 'O', 'C', 'T', 'Y', 'P', 'E'};

// first character should be immediately after the "<!"
static void skipDTD(_CFXMLPlistParseInfo *pInfo) {
    // First pass "DOCTYPE"
    if (pInfo->end - pInfo->curr < 7 || !matchString(pInfo->curr, _DoctypeOpening, 7)) {
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Malformed DTD on line %d"), lineNumber(pInfo));
        return;
    }
    pInfo->curr += 7;
    skipWhitespace(pInfo);

    // Look for either the beginning of a complex DTD or the end of the DOCTYPE structure
    while (pInfo->curr < pInfo->end) {
        UniChar ch = *(pInfo->curr);
        if (ch == '[') break; // inline DTD
        if (ch == '>') {  // End of the DTD
            pInfo->curr ++;
            return;
        }
        pInfo->curr ++;
    }
    if (pInfo->curr == pInfo->end) {
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF while parsing DTD", CFStringGetSystemEncoding());
        return;
    }

    // *Sigh* Must parse in-line DTD
    skipInlineDTD(pInfo);
    if (pInfo->errorString)  return;
    skipWhitespace(pInfo);
    if (pInfo->errorString) return;
    if (pInfo->curr < pInfo->end) {
        if (*(pInfo->curr) == '>') {
            pInfo->curr ++;
        } else {
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c on line %d while parsing DTD"), *(pInfo->curr), lineNumber(pInfo));
        }
    } else {
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF while parsing DTD", CFStringGetSystemEncoding());
    }
}

static void skipPERef(_CFXMLPlistParseInfo *pInfo) {
    const UniChar *p = pInfo->curr;
    while (p < pInfo->end) {
        if (*p == ';') {
            pInfo->curr = p+1;
            return;
        }
        p ++;
    }
    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected EOF while parsing percent-escape sequence begun on line %d"), lineNumber(pInfo));
}

// First character should be just past '['
static void skipInlineDTD(_CFXMLPlistParseInfo *pInfo) {
    while (!pInfo->errorString && pInfo->curr < pInfo->end) {
        UniChar ch;
        skipWhitespace(pInfo);
        ch = *pInfo->curr;
        if (ch == '%') {
            pInfo->curr ++;
            skipPERef(pInfo);
        } else if (ch == '<') {
            pInfo->curr ++;
            if (pInfo->curr >= pInfo->end) {
                pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF while parsing inline DTD", CFStringGetSystemEncoding());
                return;
            }
            ch = *(pInfo->curr);
            if (ch == '?') {
                pInfo->curr ++;
                skipXMLProcessingInstruction(pInfo);
            } else if (ch == '!') {
                if (pInfo->curr + 2 < pInfo->end && (*(pInfo->curr+1) == '-' && *(pInfo->curr+2) == '-')) {
                    pInfo->curr += 3;
                    skipXMLComment(pInfo);
                } else {
                    // Skip the myriad of DTD declarations of the form "<!string" ... ">"
                    pInfo->curr ++; // Past both '<' and '!'
                    while (pInfo->curr < pInfo->end) {
                        if (*(pInfo->curr) == '>') break;
                        pInfo->curr ++;
                    }
                    if (*(pInfo->curr) != '>') {
                        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF while parsing inline DTD", CFStringGetSystemEncoding());
                        return;
                 
   }
                    pInfo->curr ++;
                }
            } else {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c on line %d while parsing inline DTD"), ch, lineNumber(pInfo));
                return;
            }
        } else if (ch == ']') {
            pInfo->curr ++;
            return;
        } else {
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c on line %d while parsing inline DTD"), ch, lineNumber(pInfo));
            return;
        }
    }
    if (!pInfo->errorString)
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF while parsing inline DTD", CFStringGetSystemEncoding());
}

// content ::== (element | CharData | Reference | CDSect | PI | Comment)*
// In the context of a plist, CharData, Reference and CDSect are not legal (they all resolve to strings).  Skipping whitespace, then, the next character should be '<'.  From there, we figure out which of the three remaining cases we have (element, PI, or Comment).
static CFTypeRef getContentObject(_CFXMLPlistParseInfo *pInfo, Boolean *isKey) {
    if (isKey) *isKey = FALSE;
    while (!pInfo->errorString && pInfo->curr < pInfo->end) {
        skipWhitespace(pInfo);
        if (*(pInfo->curr) != '<') {
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c on line %d"), *(pInfo->curr), lineNumber(pInfo));
            return NULL;
        }
        pInfo->curr ++;
        if (pInfo->curr >= pInfo->end) {
            pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
            return NULL;
        }
        switch (*(pInfo->curr)) {
            case '?':
                // Processing instruction
                skipXMLProcessingInstruction(pInfo);
                break;
            case '!':
                // Could be a comment
                if (pInfo->curr+2 >= pInfo->end) {
                    pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
                    return NULL;
                }
                if (*(pInfo->curr+1) == '-' && *(pInfo->curr+2) == '-') {
                    pInfo->curr += 2;
                    skipXMLComment(pInfo);
                } else {
                    pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
                    return NULL;
                }
                break;
            case '/':
                // Whoops!  Looks like we got to the end tag for the element whose content we're parsing
                pInfo->curr --; // Back off to the '<'
                return NULL;
            default:
                // Should be an element
                return parseXMLElement(pInfo, isKey);
        }
    }
    // Do not set the error string here; if it wasn't already set by one of the recursive parsing calls, the caller will quickly detect the failure (b/c pInfo->curr >= pInfo->end) and provide a more useful one of the form "end tag for <blah> not found"
    return NULL;
}

static void _catFromMarkToBuf(const UniChar *mark, const UniChar *buf, CFMutableStringRef *string, CFAllocatorRef allocator ) {
    if (!(*string)) {
        *string = CFStringCreateMutable(allocator, 0);
    }
    CFStringAppendCharacters(*string, mark, buf-mark);
}

static UniChar _CDSectOpening[9] = {'<', '!', '[', 'C', 'D', 'A', 'T', 'A', '['};
static void parseCDSect(_CFXMLPlistParseInfo *pInfo, CFMutableStringRef string) {
    const UniChar *end, *begin;
    if (pInfo->end - pInfo->curr < 9) {
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
        return;
    }
    if (!matchString(pInfo->curr, _CDSectOpening, 9)) {
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered improper CDATA opening at line %d"), lineNumber(pInfo));
        return;
    }
    pInfo->curr += 9;
    begin = pInfo->curr; // Marks the first character of the CDATA content
    end = pInfo->end-2; // So we can safely look 2 characters beyond p
    while (pInfo->curr < end) {
        if (*(pInfo->curr) == ']' && *(pInfo->curr+1) == ']' && *(pInfo->curr+2) == '>') {
           // Found the end!
            CFStringAppendCharacters(string, begin, pInfo->curr-begin);
            pInfo->curr += 3;
            return;
        }
        pInfo->curr ++;
    }
    // Never found the end mark
    pInfo->curr = begin;
    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Could not find end of CDATA started on line %d"), lineNumber(pInfo));
}

// Only legal references are {lt, gt, amp, apos, quote, #ddd, #xAAA}
static void parseEntityReference(_CFXMLPlistParseInfo *pInfo, CFMutableStringRef string) {
    int len;
    UniChar ch;
    pInfo->curr ++; // move past the '&';
    len = pInfo->end - pInfo->curr; // how many characters we can safely scan
    if (len < 1) {
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
        return;
    }
    switch (*(pInfo->curr)) {
        case 'l':  // "lt"
            if (len >= 3 && *(pInfo->curr+1) == 't' && *(pInfo->curr+2) == ';') {
                ch = '<';
                pInfo->curr += 3;
                break;
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case 'g': // "gt"
            if (len >= 3 && *(pInfo->curr+1) == 't' && *(pInfo->curr+2) == ';') {
                ch = '>';
                pInfo->curr += 3;
                break;
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case 'a': // "apos" or "amp"
            if (len < 4) {   // Not enough characters for either conversion
                pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
                return;
            }
            if (*(pInfo->curr+1) == 'm') {
                // "amp"
                if (*(pInfo->curr+2) == 'p' && *(pInfo->curr+3) == ';') {
                    ch = '&';
                    pInfo->curr += 4;
                    break;
                }
            } else if (*(pInfo->curr+1) == 'p') {
                // "apos"
                if (len > 4 && *(pInfo->curr+2) == 'o' && *(pInfo->curr+3) == 's' && *(pInfo->curr+4) == ';') {
                    ch = '\'';
                    pInfo->curr += 5;
                    break;
                }
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case 'q':  // "quote"
            if (len >= 6 && *(pInfo->curr+1) == 'u' && *(pInfo->curr+2) == 'o' && *(pInfo->curr+3) == 't' && *(pInfo->curr+4) == 'e' && *(pInfo->curr+5) == ';') {
                ch = '\"';
                pInfo->curr += 6;
                break;
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case '#':
        {
            UInt16 num = 0;
            Boolean isHex = FALSE;
            if ( len < 4) {  // Not enough characters to make it all fit!  Need at least "&#d;"
                pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
                return;
            }
            pInfo->curr ++;
            if (*(pInfo->curr) == 'x') {
                isHex = TRUE;
                pInfo->curr ++;
            }
            while (pInfo->curr < pInfo->end) {
                ch = *(pInfo->curr);
                pInfo->curr ++;
                if (ch == ';') {
                    CFStringAppendCharacters(string, &num, 1);
                    return;
                }
                if (!isHex) num = num*10;
                else num = num << 4;
                if (ch <= '9' && ch >= '0') {
                    num += (ch - '0');
                } else if (!isHex) {
                    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c at line %d"), ch, lineNumber(pInfo));
                    return;
                } else if (ch >= 'a' && ch <= 'f') {
                    num += 10 + (ch - 'a');
                } else if (ch >= 'A' && ch <= 'F') {
                    num += 10 + (ch - 'A');
                } else {
                    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c at line %d"), ch, lineNumber(pInfo));
                    return;                    
                }
            }
            pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
            return;
        }
        default:
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
    }
    CFStringAppendCharacters(string, &ch, 1);
}

// String could be comprised of characters, CDSects, or references to one of the "well-known" entities ('<', '>', '&', ''', '"')
// returns a retained object in *string.
static CFStringRef getString(_CFXMLPlistParseInfo *pInfo) {
    const UniChar *mark = pInfo->curr; // At any time in the while loop below, the characters between mark and p have not yet been added to *string
    CFMutableStringRef string = NULL;
    while (!pInfo->errorString && pInfo->curr <= pInfo->end) {
        UniChar ch = *(pInfo->curr);
        if (ch == '<') {
            // Could be a CDSect; could be the end of the string
            if (*(pInfo->curr+1) != '!') break; // End of the string
            _catFromMarkToBuf(mark, pInfo->curr, &string, pInfo->allocator);
            parseCDSect(pInfo, string);
            mark = pInfo->curr;
        } else if (ch == '&') {
            _catFromMarkToBuf(mark, pInfo->curr, &string, pInfo->allocator);
            parseEntityReference(pInfo, string);
            mark = pInfo->curr;
        } else {
            pInfo->curr ++;
        }
    }

    if (pInfo->errorString) {
        if (string) CFRelease(string);
        return NULL;
    }
    if (!string) {
        if (pInfo->mutabilityOption != kCFPropertyListMutableContainersAndLeaves) {
            return CFStringCreateWithCharacters(pInfo->allocator, mark, pInfo->curr - mark);
        }
        string = CFStringCreateMutable(pInfo->allocator, 0);
        CFStringAppendCharacters(string, mark, pInfo->curr - mark);
        return string;
    }
    _catFromMarkToBuf(mark, pInfo->curr, &string, pInfo->allocator);
    return string;
}

static Boolean checkForCloseTag(_CFXMLPlistParseInfo *pInfo, const UniChar *tag, UInt32 tagLen) {
    if (pInfo->end - pInfo->curr < tagLen + 3) {
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
        return FALSE;
    }
    if (*(pInfo->curr) != '<' || *(++pInfo->curr) != '/') {
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c on line %d"), *(pInfo->curr), lineNumber(pInfo));
        return FALSE;
    }
    pInfo->curr ++;
    if (!matchString(pInfo->curr, tag, tagLen)) {
        CFStringRef str = CFStringCreateWithCharactersNoCopy(pInfo->allocator, tag, tagLen, kCFAllocatorNull);
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Close tag on line %d does not match open tag %@"), lineNumber(pInfo), str);
        CFRelease(str);
        return FALSE;
    }
    pInfo->curr += tagLen;
    skipWhitespace(pInfo);
    if (pInfo->curr == pInfo->end) {
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
        return FALSE;
    }
    if (*(pInfo->curr) != '>') {
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c on line %d"), *(pInfo->curr), lineNumber(pInfo));
        return FALSE;
    }
    pInfo->curr ++;
    return TRUE;
}

// pInfo should be set to the first content character of the <plist>
static CFTypeRef parsePListTag(_CFXMLPlistParseInfo *pInfo) {
    CFTypeRef result, tmp = NULL;
    const UniChar *save;
    result = getContentObject(pInfo, NULL);
    if (!result) {
        if (!pInfo->errorString) pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered empty plist tag", CFStringGetSystemEncoding());
        return NULL;
    }
    save = pInfo->curr; // Save this in case the next step fails
    tmp = getContentObject(pInfo, NULL);
    if (tmp) {
        // Got an extra object
        CFRelease(tmp);
        CFRelease(result);
        pInfo->curr = save;
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected element at line %d (plist can only include one object)"), lineNumber(pInfo));
        return NULL;
    }
    if (pInfo->errorString) {
        // Parse failed catastrophically
        CFRelease(result);
        return NULL;
    }
    if (checkForCloseTag(pInfo, CFXMLPlistTags[PLIST_IX], PLIST_TAG_LENGTH)) {
        return result;
    }
    CFRelease(result);
    return NULL;
}

static CFTypeRef parseArrayTag(_CFXMLPlistParseInfo *pInfo) {
    CFMutableArrayRef array = CFArrayCreateMutable(pInfo->allocator, 0, &kCFTypeArrayCallBacks);
    CFTypeRef tmp = getContentObject(pInfo, NULL);
    while (tmp) {
        CFArrayAppendValue(array, tmp);
        CFRelease(tmp);
        tmp = getContentObject(pInfo, NULL);
    }
    if (pInfo->errorString) { // getContentObject encountered a parse error
        CFRelease(array);
        return NULL;
    }
    if (checkForCloseTag(pInfo, CFXMLPlistTags[ARRAY_IX], ARRAY_TAG_LENGTH)) {
        return array;
    }
    CFRelease(array);
    return NULL;
}

static CFTypeRef parseDictTag(_CFXMLPlistParseInfo *pInfo) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(pInfo->allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef key=NULL, value=NULL;
    Boolean gotKey;
    const UniChar *base = pInfo->curr;
    key = getContentObject(pInfo, &gotKey);
    while (key) {
        if (!gotKey) {
            if (key) CFRelease(key);
            CFRelease(dict);
            pInfo->curr = base;
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Found non-key inside <dict> at line %d"), lineNumber(pInfo));
            return NULL;
        }
        value = getContentObject(pInfo, NULL);
        if (!value) {
            CFRelease(key);
            CFRelease(dict);
            if (!pInfo->errorString)
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Value missing for key inside <dict> at line %d"), lineNumber(pInfo));
            return NULL;
        }
        CFDictionarySetValue(dict, key, value);
        CFRelease(key);
        key = NULL;
        CFRelease(value);
        value = NULL;
        base = pInfo->curr;
        key = getContentObject(pInfo, &gotKey);
    }
    if (checkForCloseTag(pInfo, CFXMLPlistTags[DICT_IX], DICT_TAG_LENGTH)) {
        return dict;
    }
    CFRelease(dict);
    return NULL;
}

static CFTypeRef parseDataTag(_CFXMLPlistParseInfo *pInfo) {
    CFStringRef str = getString(pInfo);
    CFDataRef result;
    UniChar *buf;
    UInt32 len;
    const UniChar *base = pInfo->curr;
    if (!str) {
        if (!pInfo->errorString)
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered empty <data> on line %d"), lineNumber(pInfo));
        return NULL;
    }
    len = CFStringGetLength(str);
    // This copy should be avoidable.  Something to look at later....
    buf = (UniChar *)CFAllocatorAllocate(pInfo->allocator, len * sizeof(UniChar), 0);
    CFStringGetCharacters(str, CFRangeMake(0, len), buf);
    CFRelease(str);
    result = __CFPLDataDecode(buf, len, pInfo->mutabilityOption == kCFPropertyListMutableContainersAndLeaves, pInfo->allocator);
    CFAllocatorDeallocate(pInfo->allocator, buf);
    if (!result) {
        pInfo->curr = base;
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Could not interpret <data> at line %d (should be base64-encoded)"), lineNumber(pInfo));
        return NULL;
    }
    if (checkForCloseTag(pInfo, CFXMLPlistTags[DATA_IX], DATA_TAG_LENGTH)) return result;
    CFRelease(result);
    return NULL;
}

CF_INLINE Boolean read2DigitNumber(_CFXMLPlistParseInfo *pInfo, SInt8 *result) {
    UniChar ch1, ch2;
    if (pInfo->curr + 2 >= pInfo->end) return FALSE;
    ch1 = *pInfo->curr;
    ch2 = *(pInfo->curr + 1);
    pInfo->curr += 2;
    if (!isdigit(ch1) || !isdigit(ch2)) return FALSE;
    *result = (ch1 - '0')*10 + (ch2 - '0');
    return TRUE;
}

// YYYY '-' MM '-' DD 'T' hh ':' mm ':' ss 'Z'
static CFTypeRef parseDateTag(_CFXMLPlistParseInfo *pInfo) {
    CFGregorianDate date;
    SInt8 num;
    Boolean badFormat = FALSE;

    date.year = 0;
    while (pInfo->curr < pInfo->end && isdigit(*pInfo->curr)) {
        date.year = 10*date.year + (*pInfo->curr) - '0';
        pInfo->curr ++;
    }
    if (pInfo->curr >= pInfo->end || *pInfo->curr != '-') {
        badFormat = TRUE;
    } else {
        pInfo->curr ++;
    }

    if (!badFormat && read2DigitNumber(pInfo, &date.month) && pInfo->curr < pInfo->end && *pInfo->curr == '-') {
        pInfo->curr ++;
    } else {
        badFormat = TRUE;
    }

    if (!badFormat && read2DigitNumber(pInfo, &date.day) && pInfo->curr < pInfo->end && *pInfo->curr == 'T') {
        pInfo->curr ++;
    } else {
        badFormat = TRUE;
    }

    if (!badFormat && read2DigitNumber(pInfo, &date.hour) && pInfo->curr < pInfo->end && *pInfo->curr == ':') {
        pInfo->curr ++;
    } else {
        badFormat = TRUE;
    }

    if (!badFormat && read2DigitNumber(pInfo, &date.minute) && pInfo->curr < pInfo->end && *pInfo->curr == ':') {
        pInfo->curr ++;
    } else {
        badFormat = TRUE;
    }

    if (!badFormat && read2DigitNumber(pInfo, &num) && pInfo->curr < pInfo->end && *pInfo->curr == 'Z') {
        date.second = num;
        pInfo->curr ++;
    } else {
        badFormat = TRUE;
    }

    if (badFormat) {
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Could not interpret <date> at line %d"), lineNumber(pInfo));
        return NULL;
    }
    if (!checkForCloseTag(pInfo, CFXMLPlistTags[DATE_IX], DATE_TAG_LENGTH)) return NULL;
    return CFDateCreate(pInfo->allocator, CFGregorianDateGetAbsoluteTime(date, NULL));
}

static CFTypeRef parseRealTag(_CFXMLPlistParseInfo *pInfo) {
    CFStringRef str = getString(pInfo);
    SInt32 index, len;
    double val;
    CFNumberRef result;
    CFStringInlineBuffer buf;
    if (!str) {
        if (!pInfo->errorString)
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered empty <real> on line %d"), lineNumber(pInfo));
        return NULL;
    }
    len = CFStringGetLength(str);
    CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, len));
    index = 0;
    if (!__CFStringScanDouble(&buf, NULL, &index, &val) || index != len) {
        CFRelease(str);
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered misformatted real on line %d"), lineNumber(pInfo));
        return NULL;
    }
    CFRelease(str);
    result = CFNumberCreate(pInfo->allocator, kCFNumberDoubleType, &val);
    if (checkForCloseTag(pInfo, CFXMLPlistTags[REAL_IX], REAL_TAG_LENGTH)) return result;
    CFRelease(result);
    return NULL;
}

static CFTypeRef parseIntegerTag(_CFXMLPlistParseInfo *pInfo) {
    CFStringRef str = getString(pInfo);
    SInt32 index, len, val;
    CFNumberRef result = NULL;
    CFStringInlineBuffer buf;
    if (!str) {
        if (!pInfo->errorString)
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered empty <integer> on line %d"), lineNumber(pInfo));
        return NULL;
    }
    len = CFStringGetLength(str);
    CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, len));
    index = 0;
    if (__CFStringScanInteger(&buf, NULL, &index, FALSE, &val) && index == len) {
        if (val == INT_MAX || val == INT_MIN) {
            // over/underflow; use long long
            long long longVal;
            CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, len));
            index = 0;
            if (!__CFStringScanInteger(&buf, NULL, &index, TRUE, &longVal) || index != len) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered misformatted integer on line %d"), lineNumber(pInfo));
            } else if (longVal == QUAD_MAX || longVal == QUAD_MIN) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered integer too large to represent on line %d"), lineNumber(pInfo));
            } else {
                result = CFNumberCreate(pInfo->allocator, kCFNumberLongLongType, &longVal);
            }
        } else {
            result = CFNumberCreate(pInfo->allocator, kCFNumberSInt32Type, &val);
        }
        if (result && !checkForCloseTag(pInfo, CFXMLPlistTags[INTEGER_IX], INTEGER_TAG_LENGTH)) {
            CFRelease(result);
            result = NULL;
        }
    } else if (len > 2 && CFStringGetCharacterAtIndex(str, 0) == '0' && (CFStringGetCharacterAtIndex(str, 1) == 'x' || CFStringGetCharacterAtIndex(str, 1) == 'X')) {
        // Check for a valid hex value
        unsigned hexValue;
        CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, len));
        index = 0;
        if(__CFStringScanHex(&buf, &index, &hexValue) && index == len) {
            index = hexValue; // CFNumber doesn't support unsigned types
            result = CFNumberCreate(pInfo->allocator, kCFNumberCFIndexType, &index);
        } else {
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered misformatted integer on line %d"), lineNumber(pInfo));
        }
        if (result && !checkForCloseTag(pInfo, CFXMLPlistTags[INTEGER_IX], INTEGER_TAG_LENGTH)) {
            CFRelease(result);
            result = NULL;
        }
    } else {
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered misformatted integer on line %d"), lineNumber(pInfo));
    }
    CFRelease(str);
    return result;
}

// Returned object is retained; caller must free.  pInfo->curr expected to point to the first character after the '<'
static CFTypeRef parseXMLElement(_CFXMLPlistParseInfo *pInfo, Boolean *isKey) {
    const UniChar *marker = pInfo->curr;
    int markerLength = -1;
    Boolean isEmpty;
    int markerIx = -1;
    
    if (isKey) *isKey = FALSE;
    while (pInfo->curr < pInfo->end) {
        UniChar ch = *(pInfo->curr);
        if (ch == ' ' || ch ==  '\t' || ch == '\n' || ch =='\r') {
            if (markerLength == -1) markerLength = pInfo->curr - marker;
        } else if (ch == '>') {
            break;
        }
        pInfo->curr ++;
    }
    if (pInfo->curr >= pInfo->end) return NULL;
    isEmpty = (*(pInfo->curr-1) == '/');
    if (markerLength == -1)
        markerLength = pInfo->curr - (isEmpty ? 1 : 0) - marker;
    pInfo->curr ++; // Advance past '>'
    if (markerLength == 0) {
        // Back up to the beginning of the marker
        pInfo->curr = marker;
        pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Malformed tag on line %d"), lineNumber(pInfo));
        return NULL;
    }
    switch (*marker) {
        case 'a':   // Array
            if (markerLength == ARRAY_TAG_LENGTH && matchString(marker, CFXMLPlistTags[ARRAY_IX], ARRAY_TAG_LENGTH))
                markerIx = ARRAY_IX;
            break;
        case 'd': // Dictionary, data, or date; Fortunately, they all have the same marker length....
            if (markerLength != DICT_TAG_LENGTH)
                break;
            if (matchString(marker, CFXMLPlistTags[DICT_IX], DICT_TAG_LENGTH))
                markerIx = DICT_IX;
            else if (matchString(marker, CFXMLPlistTags[DATA_IX], DATA_TAG_LENGTH))
                markerIx = DATA_IX;
            else if (matchString(marker, CFXMLPlistTags[DATE_IX], DATE_TAG_LENGTH))
                markerIx = DATE_IX;
            break;
        case 'f': // false (boolean)
            if (markerLength == FALSE_TAG_LENGTH && matchString(marker, CFXMLPlistTags[FALSE_IX], FALSE_TAG_LENGTH)) {
                markerIx = FALSE_IX;
            }
            break;
        case 'i': // integer
            if (markerLength == INTEGER_TAG_LENGTH && matchString(marker, CFXMLPlistTags[INTEGER_IX], INTEGER_TAG_LENGTH))
                markerIx = INTEGER_IX;
            break;
        case 'k': // Key of a dictionary
            if (markerLength == KEY_TAG_LENGTH && matchString(marker, CFXMLPlistTags[KEY_IX], KEY_TAG_LENGTH)) {
                markerIx = KEY_IX;
                if (isKey) *isKey = TRUE;
            }
            break;
        case 'p': // Plist
            if (markerLength == PLIST_TAG_LENGTH && matchString(marker, CFXMLPlistTags[PLIST_IX], PLIST_TAG_LENGTH))
                markerIx = PLIST_IX;
            break;
        case 'r': // real
            if (markerLength == REAL_TAG_LENGTH && matchString(marker, CFXMLPlistTags[REAL_IX], REAL_TAG_LENGTH))
                markerIx = REAL_IX;
            break;
        case 's': // String
            if (markerLength == STRING_TAG_LENGTH && matchString(marker, CFXMLPlistTags[STRING_IX], STRING_TAG_LENGTH))
                markerIx = STRING_IX;
            break;
        case 't': // true (boolean)
            if (markerLength == TRUE_TAG_LENGTH && matchString(marker, CFXMLPlistTags[TRUE_IX], TRUE_TAG_LENGTH))
                markerIx = TRUE_IX;
            break;
    }

    if (!pInfo->allowNewTypes && markerIx != PLIST_IX && markerIx != ARRAY_IX && markerIx != DICT_IX && markerIx != STRING_IX && markerIx != KEY_IX && markerIx != DATA_IX) {
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered new tag when expecting only old-style property list objects", CFStringGetSystemEncoding());
        return NULL;
    }

    switch (markerIx) {
        case PLIST_IX:
            if (isEmpty) {
                pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered empty plist tag", CFStringGetSystemEncoding());
                return NULL;
            }
            return parsePListTag(pInfo);
        case ARRAY_IX: 
            if (isEmpty) {
                return pInfo->mutabilityOption == kCFPropertyListImmutable ?  CFArrayCreate(pInfo->allocator, NULL, 0, &kCFTypeArrayCallBacks) : CFArrayCreateMutable(pInfo->allocator, 0, &kCFTypeArrayCallBacks);
            } else {
                return parseArrayTag(pInfo);
            }
        case DICT_IX:
            if (isEmpty) {
                if (pInfo->mutabilityOption == kCFPropertyListImmutable) {
                    return CFDictionaryCreate(pInfo->allocator, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                } else {
                    return CFDictionaryCreateMutable(pInfo->allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                }
            } else {
                return parseDictTag(pInfo);
            }
        case KEY_IX:
        case STRING_IX:
        {
            CFStringRef str;
            int tagLen = (markerIx == KEY_IX) ? KEY_TAG_LENGTH : STRING_TAG_LENGTH;
            if (isEmpty) {
                return pInfo->mutabilityOption == kCFPropertyListMutableContainersAndLeaves ? CFStringCreateMutable(pInfo->allocator, 0) : CFStringCreateWithCharacters(pInfo->allocator, NULL, 0);
            }
            str = getString(pInfo);
            if (!str) return NULL; // getString will already have set the error string
            if (!checkForCloseTag(pInfo, CFXMLPlistTags[markerIx], tagLen)) {
                CFRelease(str);
                return NULL;
            }
            return str;
        }
        case DATA_IX:
            if (isEmpty) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered empty <data> on line %d"), lineNumber(pInfo));
                return NULL;
            } else {
                return parseDataTag(pInfo);
            }
        case DATE_IX:
            if (isEmpty) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered empty <date> on line %d"), lineNumber(pInfo));
                return NULL;
            } else {
                return parseDateTag(pInfo);
            }
        case TRUE_IX:
            if (!isEmpty) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered non-empty <true> tag on line %d"), lineNumber(pInfo));
                return NULL;
            } else {
                return kCFBooleanTrue;
            }
        case FALSE_IX:
            if (!isEmpty) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered non-empty <false> tag on line %d"), lineNumber(pInfo));
                return NULL;
            } else {
                return kCFBooleanFalse;
            }
        case REAL_IX:
            if (isEmpty) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered empty <real> on line %d"), lineNumber(pInfo));
                return NULL;
            } else {
                return parseRealTag(pInfo);
            }
        case INTEGER_IX:
            if (isEmpty) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered empty <integer> on line %d"), lineNumber(pInfo));
                return NULL;
            } else {
                return parseIntegerTag(pInfo);
            }
        default:  {
            CFStringRef markerStr = CFStringCreateWithCharacters(pInfo->allocator, marker, markerLength);
            pInfo->curr = marker;
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown tag %@ on line %d"), markerStr, lineNumber(pInfo));
            CFRelease(markerStr);
            return NULL;
        }
    }
}

static CFTypeRef parseXMLPropertyList(_CFXMLPlistParseInfo *pInfo) {
    while (!pInfo->errorString && pInfo->curr < pInfo->end) {
        UniChar ch;
        skipWhitespace(pInfo);
        if (pInfo->curr+1 >= pInfo->end) {
            pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "No XML content found", CFStringGetSystemEncoding());
            return NULL;
        }
        if (*(pInfo->curr) != '<') {
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Unexpected character %c at line %d"), *(pInfo->curr), lineNumber(pInfo));
            return NULL;
        }
        ch = *(++ pInfo->curr);
        if (ch == '!') {
            // Comment or DTD
            ++ pInfo->curr;
            if (pInfo->curr+1 < pInfo->end && *pInfo->curr == '-' && *(pInfo->curr+1) == '-') {
                // Comment
                pInfo->curr += 2;
                skipXMLComment(pInfo);
            } else {
                skipDTD(pInfo);
            }
        } else if (ch == '?') {
            // Processing instruction
            pInfo->curr++;
            skipXMLProcessingInstruction(pInfo);
        } else {
            // Tag or malformed
            return parseXMLElement(pInfo, NULL);
            // Note we do not verify that there was only one element, so a file that has garbage after the first element will nonetheless successfully parse
        }
    }
    // Should never get here
    if (!(pInfo->errorString))
        pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", CFStringGetSystemEncoding());
    return NULL;
}

static CFStringEncoding encodingForXMLData(CFDataRef data, CFStringRef *error) {
    const UInt8 *bytes = (UInt8 *)CFDataGetBytePtr(data);
    UInt32 length = CFDataGetLength(data);
    const UInt8 *index, *end;
    char quote;
    
    // Check for the byte order mark first
    if (length > 2 &&
        ((*bytes == 0xFF && *(bytes+1) == 0xFE) ||
         (*bytes == 0xFE && *(bytes+1) == 0xFF) ||
         *bytes == 0x00 || *(bytes+1) == 0x00)) // This clause checks for a Unicode sequence lacking the byte order mark; technically an error, but this check is recommended by the XML spec
        return kCFStringEncodingUnicode;
    
    // Scan for the <?xml.... ?> opening
    if (length < 5 || strncmp((char const *) bytes, "<?xml", 5) != 0) return kCFStringEncodingUTF8;
    index = bytes + 5;
    end = bytes + length;
    // Found "<?xml"; now we scan for "encoding"
    while (index < end) {
        UInt8 ch = *index;
        const UInt8 *scan;
        if ( ch == '?' || ch == '>') return kCFStringEncodingUTF8;
        index ++;
        scan = index;
        if (ch == 'e' && *scan++ == 'n' && *scan++ == 'c' && *scan++ == 'o' && *scan++ == 'd' && *scan++ == 'i'
            && *scan++ == 'n' && *scan++ == 'g' && *scan++ == '=') {
            index = scan;
            break;
        }
    }
    if (index >= end) return kCFStringEncodingUTF8;
    quote = *index;
    if (quote != '\'' && quote != '\"') return kCFStringEncodingUTF8;
    else {
        CFStringRef encodingName;
        const UInt8 *base = index+1; // Move past the quote character
        CFStringEncoding enc;
        UInt32 len;
        index ++;
        while (index < end && *index != quote) index ++;
        if (index >= end) return kCFStringEncodingUTF8;
        len = index - base;
        if (len == 5 && (*base == 'u' || *base == 'U') && (base[1] == 't' || base[1] == 'T') || (base[2] == 'f' || base[2] == 'F') && base[3] == '-' && base[4] == '8')
            return kCFStringEncodingUTF8;
        encodingName = CFStringCreateWithBytes(NULL, base, len, kCFStringEncodingISOLatin1, FALSE);
        enc = CFStringConvertIANACharSetNameToEncoding(encodingName);
        if (enc != kCFStringEncodingInvalidId) {
            CFRelease(encodingName);
            return enc;
        }

        if (error) {
            *error = CFStringCreateWithFormat(NULL, NULL, CFSTR("Encountered unknown encoding (%@)"), encodingName);
            CFRelease(encodingName);
        }
        return 0;
    }
}

CF_EXPORT CFTypeRef _CFPropertyListCreateFromXMLData(CFAllocatorRef allocator, CFDataRef xmlData, CFOptionFlags option, CFStringRef *errorString, Boolean allowNewTypes) {
    CFStringEncoding encoding;
    CFStringRef xmlString;
    UInt32 length;

    if (!xmlData || CFDataGetLength(xmlData) == 0) {
        if (errorString) {
            *errorString = CFSTR("Cannot parse a NULL or zero-length data");
            CFRetain(*errorString); // Caller expects to release
        }
        return NULL;
    }
    allocator = allocator ? allocator : CFAllocatorGetDefault();
    CFRetain(allocator);
    
    if (errorString) *errorString = NULL;
    encoding = encodingForXMLData(xmlData, errorString); // 0 is an error return, NOT MacRoman.

    if (encoding == 0) {
        // Couldn't find an encoding; encodingForXMLData already set *errorString if necessary
        // Note that encodingForXMLData() will give us the right values for a standard plist, too.
        if (errorString) *errorString = CFStringCreateWithFormat(allocator, NULL, CFSTR("Could not determine the encoding of the XML data"));
        return NULL;
    }

    xmlString = CFStringCreateWithBytes(allocator, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData), encoding, TRUE);
    length = xmlString ? CFStringGetLength(xmlString) : 0;

    if (length) {
        _CFXMLPlistParseInfo pInfoBuf;
        _CFXMLPlistParseInfo *pInfo = &pInfoBuf;
        CFTypeRef result;
        UniChar *buf = (UniChar *)CFStringGetCharactersPtr(xmlString);

        if (errorString) *errorString = NULL;
        if (!buf) {
            buf = (UniChar *)CFAllocatorAllocate(allocator, length * sizeof(UniChar), 0);
            if (!buf) {
                if (*errorString) *errorString = CFStringCreateWithCString(allocator, "Insufficient memory", CFStringGetSystemEncoding());
                CFRelease(xmlString);
                return NULL;
            }
            CFStringGetCharacters(xmlString, CFRangeMake(0, length), buf);
            CFRelease(xmlString);
            xmlString = NULL;
        }
        pInfo->begin = buf;
        pInfo->end = buf+length;
        pInfo->curr = buf;
        pInfo->allocator = allocator;
        pInfo->errorString = NULL;
        pInfo->mutabilityOption = option;
        pInfo->allowNewTypes = allowNewTypes;
        
        // Haven't done anything XML-specific to this point.  However, the encoding we used to translate the bytes should be kept in mind; we used Unicode if the byte-order mark was present; UTF-8 otherwise.  If the system encoding is not UTF-8 or some variant of 7-bit ASCII, we'll be in trouble.....
        result = parseXMLPropertyList(pInfo);
        if (!result) {
            if (errorString) *errorString = pInfo->errorString; // Save the XML error string aside; if no further parses work, we will return the XML error code
            else CFRelease(pInfo->errorString);
            // Reset pInfo so we can try again
            pInfo->curr = pInfo->begin;
            pInfo->errorString = NULL;
            // Try pList
            result = parseOldStylePropertyListOrStringsFile(pInfo);
            if (result && errorString) {
                CFRelease(*errorString);
                *errorString = NULL;
            }
        }
        if (xmlString) {
            CFRelease(xmlString);
        } else {
            CFAllocatorDeallocate(allocator, (void *)pInfo->begin);
        }
        CFRelease(allocator);
        return result;
    } else {
        if (errorString)
            *errorString = CFStringCreateWithFormat(allocator, NULL, CFSTR("String encoding %d not implemented; could not translate data"), encoding);
        return NULL;
    }
}

CFTypeRef CFPropertyListCreateFromXMLData(CFAllocatorRef allocator, CFDataRef xmlData, CFOptionFlags option, CFStringRef *errorString) {
    CFAssert1(xmlData != NULL, __kCFLogAssertion, "%s(): NULL data not allowed", __PRETTY_FUNCTION__);
    CFAssert2(option == kCFPropertyListImmutable || option == kCFPropertyListMutableContainers || option == kCFPropertyListMutableContainersAndLeaves, __kCFLogAssertion, "%s(): Unrecognized option %d", __PRETTY_FUNCTION__, option);
    return _CFPropertyListCreateFromXMLData(allocator, xmlData, option, errorString, TRUE);
}

//
// Old NeXT-style property lists
//

static CFTypeRef parsePlistObject(_CFXMLPlistParseInfo *pInfo);

#define isValidUnquotedStringCharacter(x) (((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z') || ((x) >= '0' && (x) <= '9') || (x) == '_' || (x) == '$' || (x) == '/' || (x) == ':' || (x) == '.')

static void advanceToNonSpace(_CFXMLPlistParseInfo *pInfo) {
    UniChar ch2;
    while (pInfo->curr < pInfo->end) {
	ch2 = *(pInfo->curr);
        pInfo->curr ++;
        if (ch2 == '\n') continue;
	if (ch2 == '/') {
            if (pInfo->curr >= pInfo->end) {
                //Whoops; back up and return
                pInfo->curr --;
                return;
            } else if (*(pInfo->curr) == '/') {
                pInfo->curr ++;
                while (pInfo->curr < pInfo->end && *(pInfo->curr) != '\n') {
                    pInfo->curr ++;
		}
	    } else if (*(pInfo->curr) == '*') {
                pInfo->curr ++;
		while (pInfo->curr < pInfo->end) {
		    ch2 = *(pInfo->curr);
                    pInfo->curr ++;
		    if (ch2 == '*' && pInfo->curr < pInfo->end && *(pInfo->curr) == '/') {
                        pInfo->curr ++; // advance past the '/'
                        break;
                    }
                }
            } else {
                pInfo->curr --;
                return;
                }
        } else if ( (0x80 < ch2) || (ch2 < 0x9) || ((ch2 != ' ') && (0xd < ch2)) ) {
            pInfo->curr --;
            return;
        }
    }
}

static UniChar getSlashedChar(_CFXMLPlistParseInfo *pInfo) {
    UniChar ch = *(pInfo->curr);
    pInfo->curr ++;
    switch (ch) {
	case '0':
	case '1':	
	case '2':	
	case '3':	
	case '4':	
	case '5':	
	case '6':	
	case '7':  {
            UInt8 num = ch - '0';
            UniChar result;
            UInt32 usedCharLen;
	    /* three digits maximum to avoid reading \000 followed by 5 as \5 ! */
	    if ((ch = *(pInfo->curr)) >= '0' && ch <= '7') { // we use in this test the fact that the buffer is zero-terminated
                pInfo->curr ++;
		num = (num << 3) + ch - '0';
		if ((pInfo->curr < pInfo->end) && (ch = *(pInfo->curr)) >= '0' && ch <= '7') {
                    pInfo->curr ++;
		    num = (num << 3) + ch - '0';
		}
	    }
#if defined(__MACOS8__)
		// I know nothing about NextStepLatin encoding!  Just return the raw bytes and hope for the best
		return num;
#else
            CFStringEncodingBytesToUnicode(kCFStringEncodingNextStepLatin, 0, &num, sizeof(UInt8), NULL,  &result, 1, &usedCharLen);
            return (usedCharLen == 1) ? result : 0;
#endif
	}
	case 'U': {
	    unsigned num = 0, numDigits = 4;	/* Parse four digits */
	    while (pInfo->curr < pInfo->end && numDigits--) {
                if (((ch = *(pInfo->curr)) < 128) && isxdigit(ch)) { 
                    pInfo->curr ++;
		    num = (num << 4) + ((ch <= '9') ? (ch - '0') : ((ch <= 'F') ? (ch - 'A' + 10) : (ch - 'a' + 10)));
		}
	    }
	    return num;
	}
	case 'a':	return '\a';
	case 'b':	return '\b';
	case 'f':	return '\f';
	case 'n':	return '\n';
	case 'r':	return '\r';
	case 't':	return '\t';
	case 'v':	return '\v';
	case '"':	return '\"';
	case '\n':	return '\n';
    }
    return ch;
}

static CFStringRef parseQuotedPlistString(_CFXMLPlistParseInfo *pInfo, UniChar quote) {
    CFMutableStringRef str = CFStringCreateMutable(pInfo->allocator, 0);
    while (pInfo->curr < pInfo->end) {
	UniChar ch = *(pInfo->curr);
        pInfo->curr ++;
	if (ch == quote) return str;
        if (ch == '\\') {
            ch = getSlashedChar(pInfo);
            CFStringAppendCharacters(str, &ch, 1);
	} else {
            // Note that the original NSParser code was much more complex at this point, but it had to deal with 8-bit characters in a non-UniChar stream.  We always have UniChar (we translated the data by the system encoding at the very beginning, hopefully), so this is safe.
            CFStringAppendCharacters(str, &ch, 1);
        }
    }
    return str;
}

static CFStringRef parseUnquotedPlistString(_CFXMLPlistParseInfo *pInfo) {
    const UniChar *mark = pInfo->curr;
    while (pInfo->curr < pInfo->end) {
        UniChar ch = *pInfo->curr;
        if (isValidUnquotedStringCharacter(ch))
            pInfo->curr ++;
        else break;
    }
    if (pInfo->curr != mark) {
        CFMutableStringRef str;
        if (pInfo->mutabilityOption == kCFPropertyListMutableContainersAndLeaves)
            return CFStringCreateWithCharacters(pInfo->allocator, mark, pInfo->curr - mark);
        str = CFStringCreateMutable(pInfo->allocator, 0);
        CFStringAppendCharacters(str, mark, pInfo->curr - mark);
        return str;
    }
    return NULL;
}

static CFStringRef parsePlistString(_CFXMLPlistParseInfo *pInfo) {
    UniChar ch;
    advanceToNonSpace(pInfo);
    if (pInfo->curr >= pInfo->end) {
        return NULL;
    }
    ch = *(pInfo->curr);
    if (ch == '\'' || ch == '\"') {
        pInfo->curr ++;
        return parseQuotedPlistString(pInfo, ch);
    } else if (isValidUnquotedStringCharacter(ch)) {
        return parseUnquotedPlistString(pInfo);
    } else {
        return NULL;
    }
}

static CFTypeRef parsePlistArray(_CFXMLPlistParseInfo *pInfo) {
    CFMutableArrayRef array = CFArrayCreateMutable(pInfo->allocator, 0, &kCFTypeArrayCallBacks);
    CFTypeRef tmp = parsePlistObject(pInfo);
    while (tmp) {
        CFArrayAppendValue(array, tmp);
        CFRelease(tmp);
        advanceToNonSpace(pInfo);
        if (*pInfo->curr != ',') {
            tmp = NULL;
        } else {
            pInfo->curr ++;
            tmp = parsePlistObject(pInfo);
        }
    }
    advanceToNonSpace(pInfo);
    if (*pInfo->curr != ')') {
        CFRelease(array);
        return NULL;
    }
    pInfo->curr ++;
    return array;
}

static CFDictionaryRef parsePlistDictContent(_CFXMLPlistParseInfo *pInfo) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(pInfo->allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef key = NULL;
    Boolean failedParse = FALSE;
    key = parsePlistString(pInfo);
    while (key) {
        CFTypeRef value;
        advanceToNonSpace(pInfo);
	if (*pInfo->curr == ';') {
	    /* This is a strings file using the shortcut format */
	    /* although this check here really applies to all plists. */
	    value = CFRetain(key);
	} else if (*pInfo->curr == '=') {
	    pInfo->curr ++;
	    value = parsePlistObject(pInfo);
	    if (!value) {
		failedParse = TRUE;
		break;
	    }
	} else {
	    failedParse = TRUE;
	    break;
	}
	CFDictionarySetValue(dict, key, value);
	CFRelease(key);
	CFRelease(value);
	advanceToNonSpace(pInfo);
	if (*pInfo->curr == ';') {
	    pInfo->curr ++;
	    key = parsePlistString(pInfo);
	} else {
	    key = NULL;
	}
    }

    if (failedParse) {
        if (key) CFRelease(key);
        CFRelease(dict);
        return NULL;
    }
    return dict;
}

static CFTypeRef parsePlistDict(_CFXMLPlistParseInfo *pInfo) {
    CFDictionaryRef dict = parsePlistDictContent(pInfo);
    if (!dict) return NULL;
    advanceToNonSpace(pInfo);
    if (*pInfo->curr != '}') {
        CFRelease(dict);
        return NULL;
    }
    pInfo->curr ++;
    return dict;
}

static unsigned char fromHexDigit(unsigned char ch) {
    if (isdigit(ch)) return ch - '0';
    ch = tolower(ch);
    if ((ch >= 'a') && (ch <= 'f')) return ch - 'a' + 10;
    return 0xff; // Just choose a large number for the error code
}

static CFTypeRef parsePlistData(_CFXMLPlistParseInfo *pInfo) {
    CFStringRef token;
    unsigned length = 0;
    CFMutableDataRef result = CFDataCreateMutable(pInfo->allocator, 0);

    advanceToNonSpace(pInfo);
    while ( (token = parseUnquotedPlistString(pInfo)) ) {
        unsigned tlength = CFStringGetLength(token);
        unsigned char *bytes;
        unsigned idx;
        if (tlength & 1) { // Token must have an even number of characters
            CFRelease(token);
            CFRelease(result);
            return NULL;
        }
        CFDataSetLength(result, length + tlength/2);
        bytes = (unsigned char *) CFDataGetMutableBytePtr(result) + length;
        length += tlength / 2;
        for (idx = 0; idx < tlength; idx += 2) {
            unsigned char hi = fromHexDigit(CFStringGetCharacterAtIndex(token, idx)), lo = fromHexDigit(CFStringGetCharacterAtIndex(token, idx+1));
            if (hi == 0xff || lo == 0xff) {
                CFRelease(token);
                CFRelease(result);
                return NULL;
            }
            *bytes = (hi << 4) + lo;
            bytes++;
        }
        CFRelease(token);
        token = NULL;
        advanceToNonSpace(pInfo);
    }

    if (*(pInfo->curr) == '>') {
        pInfo->curr ++; // Move past '>'
        return result;
    } else {
        CFRelease(result);
        return NULL;
    }
}

// Returned object is retained; caller must free.
static CFTypeRef parsePlistObject(_CFXMLPlistParseInfo *pInfo) {
    UniChar ch;
    advanceToNonSpace(pInfo);
    ch = *(pInfo->curr);
    pInfo->curr ++;
    if (ch == '{') {
        return parsePlistDict(pInfo);
    } else if (ch == '(') {
        return parsePlistArray(pInfo);
    } else if (ch == '<') {
        return parsePlistData(pInfo);
    } else if (ch == '\'' || ch == '\"') {
        return parseQuotedPlistString(pInfo, ch);
    } else if (isValidUnquotedStringCharacter(ch)) {
        pInfo->curr --;
        return parseUnquotedPlistString(pInfo);
    } else {
        pInfo->curr --;  // Must back off the charcter we just read
        return NULL;
    }
}

static CFTypeRef parseOldStylePropertyListOrStringsFile(_CFXMLPlistParseInfo *pInfo) {
    const UniChar *begin = pInfo->curr;
    CFTypeRef result = parsePlistObject(pInfo);
    advanceToNonSpace(pInfo);
    if (pInfo->curr == pInfo->end) return result;
    if (!result) return NULL;
    if (CFGetTypeID(result) != CFStringGetTypeID()) {
        CFRelease(result);
        return NULL;
    }
    CFRelease(result);
    // Check for a strings file (looks like a dictionary without the opening/closing curly braces)
    pInfo->curr = begin;
    return parsePlistDictContent(pInfo);
}

#undef isValidUnquotedStringCharacter

static CFArrayRef _arrayDeepImmutableCopy(CFAllocatorRef allocator, CFArrayRef array, CFOptionFlags mutabilityOption) {
    CFArrayRef result = NULL;
    CFIndex i, c = CFArrayGetCount(array);
    CFTypeRef *values;
    if (c == 0) {
        result = CFArrayCreate(allocator, NULL, 0, &kCFTypeArrayCallBacks);
    } else if ((values = CFAllocatorAllocate(allocator, c*sizeof(CFTypeRef), 0)) != NULL) {
        CFArrayGetValues(array, CFRangeMake(0, c), values);
        for (i = 0; i < c; i ++) {
            values[i] = CFPropertyListCreateDeepCopy(allocator, values[i], mutabilityOption);
            if (values[i] == NULL) {
                break;
            }
        }
        result = (i == c) ? CFArrayCreate(allocator, values, c, &kCFTypeArrayCallBacks) : NULL;
        c = i;
        for (i = 0; i < c; i ++) {
            CFRelease(values[i]);
        }
        CFAllocatorDeallocate(allocator, values);
    }
    return result;
}

static CFMutableArrayRef _arrayDeepMutableCopy(CFAllocatorRef allocator, CFArrayRef array, CFOptionFlags mutabilityOption) {
    CFIndex i, c = CFArrayGetCount(array);
    CFMutableArrayRef result = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
    if (result) {
        for (i = 0; i < c; i ++) {
            CFTypeRef newValue = CFPropertyListCreateDeepCopy(allocator, CFArrayGetValueAtIndex(array, i), mutabilityOption);
            if (!newValue) break;
            CFArrayAppendValue(result, newValue);
            CFRelease(newValue);
        }
        if (i != c) {
            CFRelease(result);
            result = NULL;
        }
    }
    return result;
}

CFPropertyListRef CFPropertyListCreateDeepCopy(CFAllocatorRef allocator, CFPropertyListRef propertyList, CFOptionFlags mutabilityOption) {
    CFTypeID typeID;
    CFPropertyListRef result = NULL;
    CFAssert1(propertyList != NULL, __kCFLogAssertion, "%s(): cannot copy a NULL property list", __PRETTY_FUNCTION__);
    __CFAssertIsPList(propertyList);
    CFAssert2(mutabilityOption == kCFPropertyListImmutable || mutabilityOption == kCFPropertyListMutableContainers || mutabilityOption == kCFPropertyListMutableContainersAndLeaves, __kCFLogAssertion, "%s(): Unrecognized option %d", __PRETTY_FUNCTION__, mutabilityOption);
    
    if (allocator == NULL) {
        allocator = CFRetain(CFAllocatorGetDefault());
    } else {
        CFRetain(allocator);
    }
    
    typeID = CFGetTypeID(propertyList);
    if (typeID == CFDictionaryGetTypeID()) {
        CFDictionaryRef dict = (CFDictionaryRef)propertyList;
        Boolean mutable = (mutabilityOption != kCFPropertyListImmutable);
        CFIndex count = CFDictionaryGetCount(dict);
        CFTypeRef *keys, *values;
        if (count == 0) {
            result = mutable ? CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks): CFDictionaryCreate(allocator, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        } else if ((keys = CFAllocatorAllocate(allocator, 2 * count * sizeof(CFTypeRef), 0)) != NULL)   {
            CFIndex i;
            values = keys+count;
            CFDictionaryGetKeysAndValues(dict, keys, values);
            for (i = 0; i < count; i ++) {
                keys[i] = CFStringCreateCopy(allocator, keys[i]);
                if (keys[i] == NULL) {
                    break;
                }
                values[i] = CFPropertyListCreateDeepCopy(allocator, values[i], mutabilityOption);
                if (values[i] == NULL) {
                    CFRelease(keys[i]);
                    break;
                }
            }
            if (i == count) {
                result = mutable ? CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) : CFDictionaryCreate(allocator, keys, values, count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                for (i = 0; i < count; i ++) {
                    if (mutable) {
                        CFDictionarySetValue((CFMutableDictionaryRef)result, keys[i], values[i]);
                    }
                    CFRelease(keys[i]);
                    CFRelease(values[i]);
                }
            } else {
                result = NULL;
                count = i;
                for (i = 0; i < count; i ++) {
                    CFRelease(keys[i]);
                    CFRelease(values[i]);
                }
            }
            CFAllocatorDeallocate(allocator, keys);
        } else {
            result = NULL;
        }
    } else if (typeID == CFArrayGetTypeID()) {
        if (mutabilityOption == kCFPropertyListImmutable) {
            result = _arrayDeepImmutableCopy(allocator, (CFArrayRef)propertyList, mutabilityOption);
        } else {
            result = _arrayDeepMutableCopy(allocator, (CFArrayRef)propertyList, mutabilityOption);
        }
    } else if (typeID == CFDataGetTypeID()) {
        if (mutabilityOption == kCFPropertyListMutableContainersAndLeaves) {
            result = CFDataCreateMutableCopy(allocator, 0, (CFDataRef)propertyList);
        } else {
            result = CFDataCreateCopy(allocator, (CFDataRef)propertyList);
        }
    } else if (typeID == CFNumberGetTypeID()) {
        // Warning - this will break if byteSize is ever greater than 16
        UInt8 bytes[16];
        CFNumberType numType = CFNumberGetType((CFNumberRef)propertyList);
        CFNumberGetValue((CFNumberRef)propertyList, numType, (void *)bytes);
        result = CFNumberCreate(allocator, numType, (void *)bytes);
    } else if (typeID == CFBooleanGetTypeID()) {
        // Booleans are immutable & shared instances
        CFRetain(propertyList);
        result = propertyList;
    } else if (typeID == CFDateGetTypeID()) {
        // Dates are immutable
        result = CFDateCreate(allocator, CFDateGetAbsoluteTime((CFDateRef)propertyList));
    } else if (typeID == CFStringGetTypeID()) {
        if (mutabilityOption == kCFPropertyListMutableContainersAndLeaves) {
            result = CFStringCreateMutableCopy(allocator, 0, (CFStringRef)propertyList);
        } else {
            result = CFStringCreateCopy(allocator, (CFStringRef)propertyList);
        }
    } else {
        CFAssert2(FALSE, __kCFLogAssertion, "%s(): 0x%x is not a property list type", __PRETTY_FUNCTION__, propertyList);
        result = NULL;
    }
    CFRelease(allocator);
    return result;
}
