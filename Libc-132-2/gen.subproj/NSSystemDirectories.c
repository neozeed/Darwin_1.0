/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <libc.h>
#import <stdio.h>
#import <stdlib.h>
#import "NSSystemDirectories.h"

// Names of directories; index into this with NSSearchPathDirectory - 1
#define numDirs 9
static const struct {
    unsigned char invalidDomainMask;	// Domains in which this dir does not appear
    const char *dirPath;
} dirInfo[numDirs] = {
    {0, "Applications"},
    {0, "Demos"},
    {0, "Developer/Applications"},
    {0, "Administration"},
    {0, "Library"},
    {0, "Developer"},
#ifdef WIN32
    {0xff, "Users"},
#else
    {0x09, "Users"},	// Not valid in the "System" and "~" domains...
#endif
    {0, "Documentation"},
    {0xfe, "Documents"}
};

// Ordered list of where to find applications in each domain (the numbers are NSSearchPathDirectory)
#define numApplicationDirs 4
static const char applicationDirs[numApplicationDirs] = {1, 4, 3, 2};

// Ordered list of where to find resources in each domain (the numbers are NSSearchPathDirectory)
#define numLibraryDirs 2
static const char libraryDirs[numLibraryDirs] = {5, 6};

// Names of domains; index into this log2(domainMask). If the search path ordering is ever changed, then we need an indirection (as the domainMask values cannot be changed).
#define numDomains 4
static const struct {
    char needsRootPrepended;
    const char *domainPath;
} domainInfo[numDomains] = {
    {0, "~"},
    {1, "/Local"},
    {1, "/Network"},
#ifdef WIN32
    {1, ""}
#else
    {1, "/System"}
#endif
};

#ifdef WIN32
#define invalidDomains 0x04
#else
#define invalidDomains 0x0
#endif

NSSearchPathEnumerationState NSStartSearchPathEnumeration(NSSearchPathDirectory dir, NSSearchPathDomainMask domainMask) {
    // The state is AABBCCCC, where
    // AA is the dir(s) requested
    // BB is the current state of dirs (if AA < 100, then this is always 0; otherwise it goes up to number of dirs)
    // CCCC is the domains requested
    // the state always contains the next item; if CCCC is 0, then we're done
    domainMask = domainMask & ((1 << numDomains) - 1) & ~invalidDomains;	// Just leave useful bits in there
    return (((unsigned int)dir) << 24) + ((unsigned int)domainMask);
}

NSSearchPathEnumerationState NSGetNextSearchPathEnumeration(NSSearchPathEnumerationState state, char *path) {
    static const char *nextRoot = NULL;
    unsigned dir = (state >> 24) & 0xff;
    unsigned dirState = (state >> 16) & 0xff;
    unsigned domainMask = state & 0xffff;
    unsigned int curDomain;	// The current domain we're at...
    unsigned int curDir = 0;	// The current dir...
    
    do {
        if (domainMask == 0) return 0; // Looks like we're done
        for (curDomain = 0; curDomain < numDomains; curDomain++) if ((domainMask & (1 << curDomain)) != 0) break;

        // Determine directory
        if (dir < NSAllApplicationsDirectory) {	// One directory per domain, simple...
            curDir = dir;
        } else {					// Can return multiple directories for each domain
            if (dir == NSAllApplicationsDirectory) {
                curDir = applicationDirs[dirState];
                if (++dirState == numApplicationDirs) dirState = 0;
            } else if (dir == NSAllLibrariesDirectory) {
                curDir = libraryDirs[dirState];
                if (++dirState == numLibraryDirs) dirState = 0;
            }
        }
        if (dirState == 0) domainMask &= ~(1 << curDomain);	// If necessary, jump to next domain
    } while ((dirInfo[curDir - 1].invalidDomainMask & (1 << curDomain)) != 0);	// If invalid, try again...

    // Get NEXT_ROOT, if necessary.
    if (domainInfo[curDomain].needsRootPrepended && nextRoot == 0) {
	nextRoot = getenv("NEXT_ROOT");
        if (nextRoot == NULL) {
            nextRoot = "";
        } else {
            strcpy(malloc(strlen(nextRoot) + 1), nextRoot);	// Be safe...
        }
    }

    sprintf(path, "%s%s/%s", domainInfo[curDomain].needsRootPrepended ? nextRoot : "", domainInfo[curDomain].domainPath, dirInfo[curDir - 1].dirPath);
        
    return (dir << 24) + (dirState << 16) + domainMask;
}


