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
#if !defined(__COREFOUNDATION_CFXMLINPUTSTREAM__)
#define __COREFOUNDATION_CFXMLINPUTSTREAM__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>

struct __CFXMLInputStream {
    UniChar *begin; // first character of the XML to be parsed
    UniChar *curr;  // current parse location
    UniChar *end;   // the first character _after_ the end of the XML
    UniChar *mark;
    CFIndex lineNum;
//    CFStringRef pushedChars; // Characters pushed onto the parse string because some entity was resolved.  NULL-terminated.
//    CFIndex charIndex;       // The current parse location within pushedChars.
};

typedef struct __CFXMLInputStream _CFXMLInputStream;

void _freeInputStream(_CFXMLInputStream *stream, CFAllocatorRef alloc);
CFIndex _inputStreamCurrentLocation(_CFXMLInputStream *stream);
CFIndex _inputStreamCurrentLine(_CFXMLInputStream *stream);
Boolean _inputStreamAtEOF(_CFXMLInputStream *stream);
Boolean _inputStreamPeekCharacter(_CFXMLInputStream *stream, UniChar *ch);
Boolean _inputStreamGetCharacter(_CFXMLInputStream *stream, UniChar *ch);
Boolean _inputStreamReturnCharacter(_CFXMLInputStream *stream, UniChar ch);
void _inputStreamSetMark(_CFXMLInputStream *stream);
void _inputStreamGetCharactersFromMark(_CFXMLInputStream *stream, CFMutableStringRef string);
void _inputStreamBackUpToMark(_CFXMLInputStream *stream);
void _inputStringInitialize(_CFXMLInputStream *stream, UniChar *characters, CFIndex length);
CFIndex _inputStreamSkipWhitespace(_CFXMLInputStream *stream, CFMutableStringRef str);
Boolean _inputStreamScanToCharacters(_CFXMLInputStream *stream, const UniChar *scanChars, CFIndex numChars, CFMutableStringRef str);
Boolean _inputStreamMatchString(_CFXMLInputStream *stream, const UniChar *stringToMatch, CFIndex length);
Boolean _inputStreamScanQuotedString(_CFXMLInputStream *stream, CFMutableStringRef str);
Boolean _inputStreamScanXMLName(_CFXMLInputStream *stream, CFMutableStringRef str, Boolean isNMToken);

#endif