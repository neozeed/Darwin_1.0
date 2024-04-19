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
/*	CFURL.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFURL__)
#define __COREFOUNDATION_CFURL__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    kCFURLPOSIXPathStyle = 0,
    kCFURLHFSPathStyle,
    kCFURLWindowsPathStyle
} CFURLPathStyle;
    
typedef const struct __CFURL * CFURLRef;

CF_EXPORT
CFTypeID CFURLGetTypeID(void);

/* encoding will be used both to interpret the bytes of URLBytes, and to */
/* interpret any percent-escapes within the bytes. */
CF_EXPORT
CFURLRef CFURLCreateWithBytes(CFAllocatorRef allocator, const UInt8 *URLBytes, CFIndex length, CFStringEncoding encoding, CFURLRef baseURL);

/* Escapes any character that is not 7-bit ASCII with the byte-code */
/* for the given encoding.  If escapeWhitespace is true, whitespace */
/* characters (' ', '\t', '\r', '\n') will be escaped also (desirable */
/* if embedding the URL into a larger text stream like HTML) */
CF_EXPORT
CFDataRef CFURLCreateData(CFAllocatorRef allocator, CFURLRef url, CFStringEncoding encoding, Boolean escapeWhitespace);

/* Any escape sequences in URLString will be interpreted via UTF-8. */
CF_EXPORT
CFURLRef CFURLCreateWithString(CFAllocatorRef allocator, CFStringRef URLString, CFURLRef baseURL);

/* filePath should be the URL's path expressed as a path of the type */
/* fsType.  If filePath is not absolute, the resulting URL will be */
/* considered relative to the current working directory (evaluated */
/* at creation time).  isDirectory determines whether filePath is */
/* treated as a directory path when resolving against relative path */
/* components */
CF_EXPORT
CFURLRef CFURLCreateWithFileSystemPath(CFAllocatorRef allocator, CFStringRef filePath, CFURLPathStyle pathStyle, Boolean isDirectory);

CF_EXPORT
CFURLRef CFURLCreateFromFileSystemRepresentation(CFAllocatorRef allocator, const UInt8 *buffer, CFIndex bufLen, Boolean isDirectory);

CF_EXPORT
CFURLRef CFURLCreateWithFileSystemPathRelativeToBase(CFAllocatorRef allocator, CFStringRef filePath, CFURLPathStyle pathStyle, Boolean isDirectory, CFURLRef baseURL); 

CF_EXPORT
CFURLRef CFURLCreateFromFileSystemRepresentationRelativeToBase(CFAllocatorRef allocator, const UInt8 *buffer, CFIndex bufLen, Boolean isDirectory, CFURLRef baseURL);
                                                                         
/* Fills buffer with the file system's native representation of */
/* url's path. No more than bufLen bytes are written to buffer. */
/* If resolveAgainstBase is true, the url's relative portion is */
/* resolved against its base before the path is computed. */
/* Returns success or failure. */
CF_EXPORT
Boolean CFURLGetFileSystemRepresentation(CFURLRef url, Boolean resolveAgainstBase, UInt8 *buffer, CFIndex maxBufLen);

/* Creates a new URL by resolving the relative portion of relativeURL against its base. */
CF_EXPORT
CFURLRef CFURLCopyAbsoluteURL(CFURLRef relativeURL);

/* Returns the URL's string. */
CF_EXPORT
CFStringRef CFURLGetString(CFURLRef anURL);

/* Returns the base URL if it exists */
CF_EXPORT
CFURLRef CFURLGetBaseURL(CFURLRef anURL);

/*
All URLs can be broken into two pieces - the scheme (preceding the
first colon) and the resource specifier (following the first colon).
Most URLs are also "standard" URLs conforming to RFC 1808 (available
from www.w3c.org).  This category includes URLs of the file, http,
https, and ftp schemes, to name a few.  Standard URLs start the
resource specifier with two slashes ("//"), and can be broken into
four distinct pieces - the scheme, the net location, the path, and
further resource specifiers (typically an optional parameter, query,
and/or fragment).  The net location appears immediately following
the two slashes and goes up to the next slash; it's format is
scheme-specific, but is usually composed of some or all of a username,
password, host name, and port.  The path is a series of path components
separated by slashes; if the net location is present, the path always
begins with a slash.  Standard URLs can be relative to another URL,
in which case at least the scheme and possibly other pieces as well
come from the base URL (see RFC 1808 for precise details when resolving
a relative URL against its base).  The full URL is therefore

<scheme> "://" <net location> <path, always starting with slash> <add'l resource specifiers>

If a given CFURL can be decomposed (that is, conforms to RFC 1808), you
can ask for each of the four basic pieces (scheme, net location, path,
and resource specifer) separately, as well as for its base URL.  The
basic pieces are returned with any percent escape sequences still in
place (although note that the scheme may not legally include any
percent escapes); this is to allow the caller to distinguish between
percent sequences that may have syntactic meaning if replaced by the
character being escaped (for instance, a '/' in a path component).
Since only the individual schemes know which characters are
syntactically significant, CFURL cannot safely replace any percent
escape sequences.  However, you can use
CFURLCreateStringByReplacingPercentEscapes() to create a new string with
the percent escapes removed; see below.

If a given CFURL can not be decomposed, you can ask for its scheme and its
resource specifier; asking it for its net location or path will return NULL.

To get more refined information about the components of a decomposable
CFURL, you may ask for more specific pieces of the URL, expressed with
the percent escapes removed.  The available functions are CFURLCopyHostName(),
CFURLGetPortNumber() (returns an Int32), CFURLCopyUserName(),
CFURLCopyPassword(), CFURLCopyQuery(), CFURLCopyParameters(), and
CFURLCopyFragment().  Because the parameters, query, and fragment of an
URL may contain scheme-specific syntaxes, these methods take a second
argument, giving a list of characters which should NOT be replaced if
percent escaped.  For instance, the ftp parameter syntax gives simple
key-value pairs as "<key>=<value>;"  Clearly if a key or value includes
either '=' or ';', it must be escaped to avoid corrupting the meaning of
the parameters, so the caller may request the parameter string as

CFStringRef myParams = CFURLCopyParameters(ftpURL, CFSTR("=;%"));

requesting that all percent escape sequences be replaced by the represented
characters, except for escaped '=', '%' or ';' characters.  Pass the empty
string (CFSTR("")) to request that all percent escapes be replaced, or NULL
to request that none be.
*/

/* Returns TRUE if anURL conforms to RFC 1808 */
CF_EXPORT
Boolean CFURLCanBeDecomposed(CFURLRef anURL); 

/* The next several methods leave any percent escape sequences intact */

CF_EXPORT
CFStringRef CFURLCopyScheme(CFURLRef anURL);

/* NULL if CFURLCanBeDecomposed(anURL) is FALSE */
CF_EXPORT
CFStringRef CFURLCopyNetLocation(CFURLRef anURL); 

/* NULL if CFURLCanBeDecomposed(anURL) is FALSE; also does not resolve the URL */
/* against its base.  See also CFURLCopyAbsoluteURL().  Note that, strictly */
/* speaking, any leading '/' is not considered part of the URL's path, although */
/* its presence or absence determines whether the path is absolute. */
/* CFURLCopyPath()'s return value includes any leading slash (giving the path */
/* the normal POSIX appearance); CFURLCopyStrictPath()'s return value omits any */
/* leading slash, and uses isAbsolute to report whether the URL's path is absolute. */

/* CFURLCopyFileSystemPath() returns the URL's path as a file system path for the */
/* given path style.  All percent escape sequences are replaced.  The URL is not */
/* resolved against its base before computing the path. */
CF_EXPORT
CFStringRef CFURLCopyPath(CFURLRef anURL);

CF_EXPORT
CFStringRef CFURLCopyStrictPath(CFURLRef anURL, Boolean *isAbsolute);

CF_EXPORT
CFStringRef CFURLCopyFileSystemPath(CFURLRef anURL, CFURLPathStyle pathStyle);

/* Returns whether anURL's path represents a directory */
/* (TRUE returned) or a simple file (FALSE returned) */
CF_EXPORT
Boolean CFURLHasDirectoryPath(CFURLRef anURL);

/* Any additional resource specifiers after the path.  For URLs */
/* that cannot be decomposed, this is everything except the scheme itself. */
CF_EXPORT
CFStringRef CFURLCopyResourceSpecifier(CFURLRef anURL); 

CF_EXPORT
CFStringRef CFURLCopyHostName(CFURLRef anURL);

CF_EXPORT
SInt32 CFURLGetPortNumber(CFURLRef anURL); /* Returns -1 if no port number is specified */

CF_EXPORT
CFStringRef CFURLCopyUserName(CFURLRef anURL);

CF_EXPORT
CFStringRef CFURLCopyPassword(CFURLRef anURL);

/* These remove all percent escape sequences except those for */
/* characters in charactersToLeaveEscaped.  If charactersToLeaveEscaped */
/* is empty (""), all percent escape sequences are replaced by their */
/* corresponding characters.  If charactersToLeaveEscaped is NULL, */
/* then no escape sequences are removed at all */
CF_EXPORT
CFStringRef CFURLCopyParameterString(CFURLRef anURL, CFStringRef charactersToLeaveEscaped);

CF_EXPORT
CFStringRef CFURLCopyQueryString(CFURLRef anURL, CFStringRef charactersToLeaveEscaped);

CF_EXPORT
CFStringRef CFURLCopyFragment(CFURLRef anURL, CFStringRef charactersToLeaveEscaped);

CF_EXPORT
CFStringRef CFURLCopyLastPathComponent(CFURLRef url);

CF_EXPORT
CFStringRef CFURLCopyPathExtension(CFURLRef url);

CF_EXPORT
CFURLRef CFURLCreateCopyAppendingPathComponent(CFAllocatorRef allocator, CFURLRef url, CFStringRef pathComponent, Boolean isDirectory);

CF_EXPORT
CFURLRef CFURLCreateCopyDeletingLastPathComponent(CFAllocatorRef allocator, CFURLRef url);

CF_EXPORT
CFURLRef CFURLCreateCopyAppendingPathExtension(CFAllocatorRef allocator, CFURLRef url, CFStringRef extension);

CF_EXPORT
CFURLRef CFURLCreateCopyDeletingPathExtension(CFAllocatorRef allocator, CFURLRef url);

/* Returns a string with any percent escape sequences that do NOT */
/* correspond to characters in charactersToLeaveEscaped with their */
/* equivalent.  Returns NULL on failure (if an invalid percent sequence */
/* is encountered), or the original string (retained) if no characters */
/* need to be replaced. Pass NULL to request that no percent escapes be */
/* replaced, or the empty string (CFSTR("")) to request that all percent */
/* escapes be replaced.  Uses UTF8 to interpret percent escapes. */
CF_EXPORT
CFStringRef CFURLCreateStringByReplacingPercentEscapes(CFAllocatorRef allocator, CFStringRef originalString, CFStringRef charactersToLeaveEscaped);

/* DEPRECATED - use CFURLCopyFileSystemPath instead */
CF_EXPORT
CFStringRef CFURLCreateStringWithFileSystemPath(CFAllocatorRef allocator, CFURLRef anURL, CFURLPathStyle pathStyle, Boolean resolveAgainstBase);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFURL__ */

