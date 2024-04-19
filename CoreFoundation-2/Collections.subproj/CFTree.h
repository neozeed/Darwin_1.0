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
/*	CFTree.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFTREE__)
#define __COREFOUNDATION_CFTREE__ 1

#include <CoreFoundation/CFBase.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef const void *	(*CFTreeRetainCallBack)(const void *info);
typedef void		(*CFTreeReleaseCallBack)(const void *info);
typedef CFStringRef	(*CFTreeCopyDescriptionCallBack)(const void *info);
typedef struct {
    CFIndex				version;
    void *				info;
    CFTreeRetainCallBack		retain;
    CFTreeReleaseCallBack		release;	
    CFTreeCopyDescriptionCallBack	copyDescription;
} CFTreeContext;

typedef void (*CFTreeApplierFunction)(const void *value, void *context);

typedef struct __CFTree * CFTreeRef;

CF_EXPORT
CFTypeID CFTreeGetTypeID(void);

CF_EXPORT
CFTreeRef CFTreeCreate(CFAllocatorRef allocator, const CFTreeContext *context);

CF_EXPORT
CFTreeRef CFTreeGetParent(CFTreeRef tree);

CF_EXPORT
CFTreeRef CFTreeGetNextSibling(CFTreeRef tree);

CF_EXPORT
CFTreeRef CFTreeGetFirstChild(CFTreeRef tree);

CF_EXPORT
void CFTreeGetContext(CFTreeRef tree, CFTreeContext *context);

CF_EXPORT
CFIndex CFTreeGetChildCount(CFTreeRef tree);

CF_EXPORT
CFTreeRef CFTreeGetChildAtIndex(CFTreeRef tree, CFIndex idx);

CF_EXPORT
void CFTreeGetChildren(CFTreeRef tree, CFTreeRef *children);

CF_EXPORT
void CFTreeApplyFunctionToChildren(CFTreeRef tree, CFTreeApplierFunction applier, void *context);

CF_EXPORT
CFTreeRef CFTreeFindRoot(CFTreeRef tree);

CF_EXPORT
void CFTreeSetContext(CFTreeRef tree, const CFTreeContext *context);

/* adds newChild as tree's first child */
CF_EXPORT
void CFTreePrependChild(CFTreeRef tree, CFTreeRef newChild);

/* adds newChild as tree's last child */
CF_EXPORT
void CFTreeAppendChild(CFTreeRef tree, CFTreeRef newChild);

/* Inserts newSibling after tree.  tree and newSibling will have the same parent */
CF_EXPORT
void CFTreeInsertSibling(CFTreeRef tree, CFTreeRef newSibling);

/* Removes tree from its parent */
CF_EXPORT
void CFTreeRemove(CFTreeRef tree);

/* Removes all the children of tree */
CF_EXPORT
void CFTreeRemoveAllChildren(CFTreeRef tree);

CF_EXPORT
void CFTreeSortChildren(CFTreeRef tree, CFComparatorFunction comparator, void *context);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFTREE__ */

