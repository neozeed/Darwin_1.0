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
/*	CFStringBuffer.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

/*
	Stream-oriented, fast access to CFString contents.

These inline functions allow you to go through characters in a CFString sequentially. 
In addition, buffering is provided to reduce the number of times the CFString functions
are called. If you need to enumerate the characters in a string, even a short
string, it turns out these functions are a lot more efficient than calling
CFStringGetCharacter() multiple times.

These functions work with a struct which you provide. This struct is not a CF-type like
CFString, CFArray, etc; it's simply a struct which is usually allocated on the stack,
and manipulated with these inline functions. You initialize it with CFStringBufferInit(),
and don't need to release or finalize it.

The functions are:

void CFStringBufferInit(CFStringBuffer *buf, CFStringRef str, CFIndexType startingLoc, CFIndexType endLoc);
void CFStringBufferIncrementLocation(CFStringBuffer *buf);
void CFStringBufferDecrementLocation(CFStringBuffer *buf);
UniChar CFStringBufferCurrentCharacter(CFStringBuffer *buf);
Boolean CFStringBufferIsAtEnd(CFStringBuffer *buf);
CFIndexType CFStringBufferCurrentLocation(CFStringBuffer *buf);
void CFStringBufferSetLocation(CFStringBuffer *buf, CFIndexType loc, Boolean center);

Typical usage:
	CFStringBuffer buf;
	...
	CFStringBufferInit(&buf, string, 0, CFStringLength(string));
	while (CFStringBufferCurrentCharacter(&buf) == ' ') CFStringBufferIncrementLocation(&buf);

Advancing off the end or backing up beyond 0 causes CFStringBufferIsAtEnd() to return TRUE.
If you ask for the character in that state, you get back CFStringBufferEndCharacter.
In those cases it is still possible to recover by going in the other direction.

*/

#if !defined(__COREFOUNDATION_CFSTRINGBUFFER__)
#define __COREFOUNDATION_CFSTRINGBUFFER__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>

typedef struct __CFStringBuffer CFStringBuffer;

#define CFStringBufferEndCharacter (UniChar)0xffff
#define __CFStringBufferSize 32

struct __CFStringBuffer {
    CFIndex bufferLen;		/* Number of valid chars in buffer */
    CFIndex bufferLoc;		/* Location of character after current character (always 1 or more) */
    CFStringRef string;		/* The string being read */
    CFIndex stringLen;		/* Number of characters in the string */
    CFIndex stringLoc;		/* Location in string where last fill happened from */
    UniChar buffer[__CFStringBufferSize];	/* The buffer */
    UniChar curChar;		/* The current character */
};

/* Called automatically; no need to call from the outside
*/
CF_INLINE void __CFStringBufferFill(CFStringBuffer *buf) {
    buf->bufferLen = buf->stringLen - buf->stringLoc;
    if (buf->bufferLen > __CFStringBufferSize) buf->bufferLen = __CFStringBufferSize;
    CFStringGetCharacters(buf->string, CFRangeMake(buf->stringLoc, buf->bufferLen), buf->buffer);
    buf->curChar = buf->buffer[0];
    buf->bufferLoc = 1;
}

/* Initializes everything so that buffer is being filled from the specified location of str. endLoc should point at one char past the last allowed character in the string. Typically loc == 0 and endLoc == [str length]. To look at a range, loc = range.location and endLoc = NSMaxRange(range).
*/
CF_INLINE void CFStringBufferInit(CFStringBuffer *buf, CFStringRef  str, CFIndex loc, CFIndex endLoc) {
    buf->string = str;
    buf->stringLoc = loc;
    buf->stringLen = endLoc;
    if (buf->stringLoc < buf->stringLen) {
	__CFStringBufferFill(buf);
    } else {
        buf->bufferLen = 0;
	buf->bufferLoc = 1;
        buf->curChar = CFStringBufferEndCharacter;
    }
}

/* Advances to the next character; sets currentCharacter to CFStringBufferEndCharacter if at end
*/
CF_INLINE void CFStringBufferIncrementLocation(CFStringBuffer *buf) { 
   if (buf->bufferLoc < buf->bufferLen) {	/* Buffer is OK */
	buf->curChar = buf->buffer[buf->bufferLoc++];
   } else if (buf->stringLoc + buf->bufferLen < buf->stringLen) {	/* Buffer is empty but can be filled */
	buf->stringLoc += buf->bufferLen;
	__CFStringBufferFill(buf);
   } else {	/* Buffer is empty and we're at the end */
        buf->bufferLoc = buf->bufferLen + 1;
	buf->curChar = CFStringBufferEndCharacter;
   }
}

/* Goes back one character; sets currentCharacter to CFStringBufferEndCharacter if at zero 
*/
CF_INLINE void CFStringBufferDecrementLocation(CFStringBuffer *buf) {
    if (buf->bufferLoc > 1) {	/* Buffer is OK */
        buf->bufferLoc--;
        buf->curChar = buf->buffer[buf->bufferLoc - 1];
    } else if (buf->stringLoc > 0) {	/* Buffer is empty but can be filled */
        buf->bufferLen = buf->bufferLoc = (buf->stringLoc > __CFStringBufferSize) ? __CFStringBufferSize : buf->stringLoc;
        buf->stringLoc -= buf->bufferLen;
	CFStringGetCharacters(buf->string, CFRangeMake(buf->stringLoc, buf->bufferLen), buf->buffer);
        buf->curChar = buf->buffer[buf->bufferLoc - 1];
    } else {	/* Buffer is empty and we're at the end */
        buf->bufferLoc = 0;
        buf->curChar = CFStringBufferEndCharacter;
    }
}

/* Load the character at the specified location. If the buffer has to be reloaded, it's loaded such that the specified character is at the middle. Doesn't handle setting loc past the end. 
*/
CF_INLINE void CFStringBufferSetLocation(CFStringBuffer *buf, CFIndex loc) {
    if (loc < buf->stringLoc || loc >= buf->stringLoc + buf->bufferLen) {
        if (loc < (__CFStringBufferSize / 2)) {	/* Get the first __CFStringBufferSize chars */
            buf->stringLoc = 0;
        } else if (loc > buf->stringLen - (__CFStringBufferSize / 2)) {	/* Get the last __CFStringBufferSize chars */
            buf->stringLoc = (buf->stringLen < __CFStringBufferSize) ? 0 : (buf->stringLen - __CFStringBufferSize);
        } else {
            buf->stringLoc = loc - (__CFStringBufferSize / 2);	/* Center around loc */
        }
        __CFStringBufferFill(buf);
    }
    buf->bufferLoc = loc - buf->stringLoc;
    buf->curChar = buf->buffer[buf->bufferLoc++];
}

/* Returns current character; may return CFStringBufferEndCharacter if at end
*/
CF_INLINE UniChar CFStringBufferCurrentCharacter(CFStringBuffer *buf) { return (UniChar)(buf->curChar); }

/* Returns TRUE if no more characters. 
*/
CF_INLINE Boolean CFStringBufferIsAtEnd(CFStringBuffer *buf) { return buf->curChar == CFStringBufferEndCharacter; }

/* Returns the location in string for the current character 
*/
CF_INLINE CFIndex CFStringBufferCurrentLocation(CFStringBuffer *buf) { return buf->stringLoc + buf->bufferLoc - 1; }


#endif /* ! __COREFOUNDATION_CFSTRINGBUFFER__ */

