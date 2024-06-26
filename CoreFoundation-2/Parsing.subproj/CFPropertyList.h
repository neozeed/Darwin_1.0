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
/*	CFPropertyList.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFPROPERTYLIST__)
#define __COREFOUNDATION_CFPROPERTYLIST__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*
	Type to mean any instance of a property list type;
	currently, CFString, CFData, CFNumber, CFBoolean, CFDate,
	CFArray, and CFDictionary.
*/
typedef CFTypeRef CFPropertyListRef;

typedef enum {
    kCFPropertyListImmutable = 0,
    kCFPropertyListMutableContainers,
    kCFPropertyListMutableContainersAndLeaves
} CFPropertyListMutabilityOptions;
    
/*
	Creates a property list object from its XML description; xmlData should
	be the raw bytes of that description, possibly the contents of an XML
	file. Returns NULL if the data cannot be parsed; if the parse fails
	and errorString is non-NULL, a human-readable description of the failure
	is returned in errorString. It is the caller's responsibility to release
	either the returned object or the error string, whichever is applicable.
*/
CF_EXPORT
CFPropertyListRef CFPropertyListCreateFromXMLData(CFAllocatorRef allocator, CFDataRef xmlData, CFOptionFlags mutabilityOption, CFStringRef *errorString);

/*
	Returns the XML description of the given object; propertyList must
	be one of the supported property list types, and (for composite types
	like CFArray and CFDictionary) must not contain any elements that
	are not themselves of a property list type. If a non-property list
	type is encountered, NULL is returned. The returned data is
	appropriate for writing out to an XML file. Note that a data, not a
	string, is returned because the bytes contain in them a description
	of the string encoding used.
*/
CF_EXPORT
CFDataRef CFPropertyListCreateXMLData(CFAllocatorRef allocator, CFPropertyListRef propertyList);

/*
	Recursively creates a copy of the given property list (so nested arrays
	and dictionaries are copied as well as the top-most container). The
	resulting property list has the mutability characteristics determined
	by mutabilityOption.
*/
CF_EXPORT
CFPropertyListRef CFPropertyListCreateDeepCopy(CFAllocatorRef allocator, CFPropertyListRef propertyList, CFOptionFlags mutabilityOption);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFPROPERTYLIST__ */

