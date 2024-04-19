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
/*	CFPriv.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

/*
        APPLE SPI:  NOT TO BE USED OUTSIDE APPLE!
*/

#if !defined(__COREFOUNDATION_CFPRIV__)
#define __COREFOUNDATION_CFPRIV__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#if defined(__MACH__)
#include <CoreFoundation/CFRunLoop.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__MACH__)
CF_EXPORT void _CFRunLoopSetCurrent(CFRunLoopRef rl);
#endif

#if !defined(__WIN32__)
/* Utility for translating URL to FSSpec and back.  Needed by various
apple-internal clients. The last argument is really an (FSSpec *) but
that type is not visible to us here. */
CF_EXPORT
Boolean _CFGetFSSpecFromURL(CFAllocatorRef alloc, CFURLRef url, void *spec);

CF_EXPORT
CFURLRef _CFCreateURLFromFSSpec(CFAllocatorRef alloc, const void *voidspec, Boolean isDirectory);
#endif

/* These return NULL on MacOS 8 */
CF_EXPORT
CFStringRef CFGetUserName(void);

CF_EXPORT
CFURLRef CFCopyHomeDirectoryURLForUser(CFStringRef uName);	/* Pass NULL for the current user's home directory */

/*
	CFCopySearchPathForDirectoriesInDomains returns the various
	standard system directories where apps, resources, etc get
	installed. Because queries can return multiple directories,
	you get back a CFArray (which you should free when done) of
	CFStrings. The directories are returned in search path order;
	that is, the first place to look is returned first. This API
	may return directories that do not exist yet. If NSUserDomain
	is included in a query, then the results will contain "~" to
	refer to the user's directory. Specify expandTilde to expand
	this to the current user's home. Some calls might return no
	directories!
	??? On MacOS 8 this function currently returns an empty array.
*/
typedef enum {
    kCFApplicationDirectory = 1,	/* supported applications (Applications) */
    kCFDemoApplicationDirectory,	/* unsupported applications, demonstration versions (Demos) */
    kCFDeveloperApplicationDirectory,	/* developer applications (Developer/Applications) */
    kCFAdminApplicationDirectory,	/* system and network administration applications (Administration) */
    kCFLibraryDirectory, 		/* various user-visible documentation, support, and configuration files, resources (Library) */
    kCFDeveloperDirectory,		/* developer resources (Developer) */
    kCFUserDirectory,			/* user home directories (Users) */
    kCFDocumentationDirectory,		/* documentation (Documentation) */
    kCFAllApplicationsDirectory = 100,	/* all directories where applications can occur (ie Applications, Demos, Administration, Developer/Applications) */
    kCFAllLibrariesDirectory = 101	/* all directories where resources can occur (Library, Developer) */
} CFSearchPathDirectory;

typedef enum {
    kCFUserDomainMask = 1,	/* user's home directory --- place to install user's personal items (~) */
    kCFLocalDomainMask = 2,	/* local to the current machine --- place to install items available to everyone on this machine (/Local) */
    kCFNetworkDomainMask = 4, 	/* publically available location in the local area network --- place to install items available on the network (/Network) */
    kCFSystemDomainMask = 8,	/* provided by Apple, unmodifiable (/System) */
    kCFAllDomainsMask = 0x0ffff	/* all domains: all of the above and more, future items */
} CFSearchPathDomainMask;

CF_EXPORT
CFArrayRef CFCopySearchPathForDirectoriesInDomains(CFSearchPathDirectory directory, CFSearchPathDomainMask domainMask, Boolean expandTilde);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFPRIV__ */

