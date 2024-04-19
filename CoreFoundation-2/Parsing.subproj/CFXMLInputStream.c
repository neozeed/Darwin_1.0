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
#include <CoreFoundation/CFCharacterSet.h>
#include "CFUniChar.h"
#include "CFXMLInputStream.h"

void _freeInputStream(_CFXMLInputStream *stream, CFAllocatorRef alloc) {
    /*
    if (stream->pushedChars) {
        CFRelease(stream->pushedChars);
        stream->pushedChars = NULL;
    }
     */
}

CFIndex _inputStreamCurrentLocation(_CFXMLInputStream *stream) {
    return stream->curr - stream->begin + 1;
}

CFIndex _inputStreamCurrentLine(_CFXMLInputStream *stream) {
    return stream->lineNum;
}

Boolean _inputStreamAtEOF(_CFXMLInputStream *stream) {
    return stream->curr == stream->end;
    //    return stream->curr == stream->end && (stream->pushedCurr == -1);
}

Boolean _inputStreamPeekCharacter(_CFXMLInputStream *stream, UniChar *ch) {
/*    if (stream->pushedCurr != -1) {
        *ch = CFStringGetCharacterAtIndex(stream->pushedChars, stream->pushedCurr);
    } else */
    if (stream->curr < stream->end) {
        *ch = *stream->curr;
    } else {
        return FALSE;
    }
    return TRUE;
}

Boolean _inputStreamGetCharacter(_CFXMLInputStream *stream, UniChar *ch) {
/*    if (stream->pushedCurr != -1) {
        *ch = CFStringGetCharacterAtIndex(stream->pushedChars, stream->pushedCurr);
        stream->pushedCurr ++;
        if (stream->pushedCurr == CFStringGetLength(stream->pushedChars)) {
            stream->pushedCurr = -1;
        }
    } else */
    if (stream->curr < stream->end) {
        *ch = *stream->curr;
        stream->curr ++;
        if (*ch == '\n' || (*ch == '\r' && (stream->curr == stream->end || *stream->curr != '\n'))) {
            stream->lineNum ++;
        }
    } else {
        return FALSE;
    }
    return TRUE;
}

// These next four functions are only rarely needed; if we can get rid of them, that would be good. -- REW, 1/31/2000

Boolean _inputStreamReturnCharacter(_CFXMLInputStream *stream, UniChar ch) {
    if (stream->curr == stream->begin || *(stream->curr - 1) != ch) {
        return FALSE;
    } else {
        stream->curr --;
        return TRUE;
    }
}

void _inputStreamSetMark(_CFXMLInputStream *stream) {
    stream->mark = stream->curr;
}

void _inputStreamGetCharactersFromMark(_CFXMLInputStream *stream, CFMutableStringRef string) {
    if (stream->curr != stream->mark) {
        CFStringSetExternalCharactersNoCopy(string, stream->mark, stream->curr - stream->mark, stream->curr - stream->mark);
    } else {
        CFStringSetExternalCharactersNoCopy(string, NULL, 0, 0);
    }
}

void _inputStreamBackUpToMark(_CFXMLInputStream *stream) {
#warning CF: line number will be inaccurate after this
    stream->curr = stream->mark;
}

void _inputStringInitialize(_CFXMLInputStream *stream, UniChar *characters, CFIndex length) {
    stream->begin = characters;
    stream->curr = stream->begin;
    stream->end = stream->begin + length;
    stream->lineNum = 1;
    stream->mark = NULL;
//    stream->pushedChars = NULL;
//    stream->pushedCurr = NULL;
}

CFIndex _inputStreamSkipWhitespace(_CFXMLInputStream *stream, CFMutableStringRef str) {
    UniChar *p = stream->curr;
    CFIndex len;
    while (p < stream->end) {
        UniChar ch = *p;
        if (ch == '\n') {
            stream->lineNum ++;
        } else if  (ch == '\r') {
            if (p+1 == stream->end || *(p+1) != '\n') stream->lineNum ++;
        } else if (ch != ' ' && ch != '\t') {
            break;
        }
        p ++;
    }
    len = p - stream->curr;
    if (p != stream->curr && str) {
        CFStringSetExternalCharactersNoCopy(str, (UniChar *)stream->curr, len, len);
    }
    stream->curr = p;
    return len;
}

CF_INLINE void scanToCharacter(_CFXMLInputStream *stream, UniChar ch) {
    while (stream->curr < stream->end) {
        if (*stream->curr == ch) return;
        else if (*stream->curr == '\n' || (*stream->curr == '\r' && (stream->curr + 1 >= stream->end || *(stream->curr + 1) != '\n')))
            stream->lineNum ++;
        stream->curr ++;
    }
}

// FALSE return means EOF was encountered without finding scanChars
Boolean _inputStreamScanToCharacters(_CFXMLInputStream *stream, const UniChar *scanChars, CFIndex numChars, CFMutableStringRef str) {
    UniChar *end = stream->end - numChars;
    UniChar *base = stream->curr;
    CFIndex origLineNum = stream->lineNum;
    CFIndex len;
    Boolean done = FALSE;
    do {
        scanToCharacter(stream, scanChars[0]);
        if (stream->curr < end) {
            CFIndex i;
            for (i = 0; i < numChars; i ++) {
                if (stream->curr[i] != scanChars[i])
                    break;
            }
            if (i == numChars) {
                done = TRUE;
            } else {
                stream->curr ++;
            }
        } else {
            stream->lineNum = origLineNum;
            stream->curr = base;
            return FALSE; // Encountered unexpected EOF
        }
    } while (!done);
    len = stream->curr - base;
    if (str) {
        if (len) {
            CFStringSetExternalCharactersNoCopy(str, base, len, len);
        } else {
            CFStringSetExternalCharactersNoCopy(str, NULL, 0, 0);
        }
        
    }
    stream->curr += numChars;
    return TRUE;
}

Boolean _inputStreamMatchString(_CFXMLInputStream *stream, const UniChar *stringToMatch, CFIndex length) {
    if (stream->curr + length >= stream->end) {
        return FALSE;
    } else {
        const UniChar *end = stream->curr+length;
        const UniChar *bPtr=stream->curr, *sPtr=stringToMatch;
        // Seems like there should be some kind of memcmp that would be faster than this
        // No; because the number of characters is so small, this is faster that memcmp.
        while (bPtr < end) {
            if (*bPtr != *sPtr) return FALSE;
            bPtr ++;
            sPtr ++;
        }
        stream->curr += length;
        return TRUE;
    }

}

Boolean _inputStreamScanQuotedString(_CFXMLInputStream *stream, CFMutableStringRef str) {
    UniChar ch = *stream->curr;
    if (ch != '\'' && ch != '\"') {
        return FALSE;
    }
    stream->curr ++;
    if (!_inputStreamScanToCharacters(stream, &ch, 1, str)) {
        return FALSE;
    }
    return TRUE;

}

/*
 [4]  NameChar ::= Letter | Digit | '.' | '-' | '_' | ':' | CombiningChar | Extender
 [5]  Name ::= (Letter | '_' | ':') (NameChar)*
 [7]  Nmtoken ::= (NameChar)+
 [84] Letter ::= BaseChar | Ideographic

 We don't do this quite right; we rely on the Unicode charsets to do this analysis.  While
 the productions in the XML spec are based on the Unicode character sets, the definitions
 differ slightly to avoid those areas where the Unicode standard is still being resolved.
 At any rate, I'd lay money that using the Unicode charsets, we will be more correct than
 the vast majority of parsers out there.

 Letter == kCFUniCharLetterCharacterSet
 Digit == kCFUniCharDecimalDigitCharacterSet
 CombiningChar == kCFUniCharNonBaseCharacterSet
 Extender - complex, and not represented by a uniform character set.
 */
#warning CF: Revisit this code once CFCharacterSet is available - REW, 3/12/99
Boolean _inputStreamScanXMLName(_CFXMLInputStream *stream, CFMutableStringRef str, Boolean isNMToken) {
    UniChar *base = stream->curr;
    UniChar ch;

    if (stream->curr >= stream->end) return FALSE;
    ch = *stream->curr;
    if (!isNMToken) {
        // Only difference between an NMToken and a Name is Names have a stricter condition on the first character
        if (!CFUniCharIsMemberOf(ch, kCFUniCharLetterCharacterSet) && ch != '_' && ch != ':') return FALSE;
        stream->curr ++;
    }
    while (stream->curr < stream->end) {
        ch = *stream->curr;
        if (!CFUniCharIsMemberOf(ch, kCFUniCharLetterCharacterSet) && !CFUniCharIsMemberOf(ch, kCFUniCharDecimalDigitCharacterSet)  && ch != '.' && ch != '-' && ch != '_' && ch != ':' && !CFUniCharIsMemberOf(ch, kCFUniCharNonBaseCharacterSet)) {
            break;
        }
        stream->curr ++;
    }
    if (stream->curr == base) return FALSE; // Must have processed at least one character
    if (str) CFStringSetExternalCharactersNoCopy(str, base, stream->curr-base, stream->curr-base);
    return TRUE;
}

