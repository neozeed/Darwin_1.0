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
/*	CFTree.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFTree.h>
#include "CFInternal.h"

struct __CFTree {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFTreeRef _parent;	/* Not retained */
    CFTreeRef _sibling;	/* Not retained */
    CFTreeRef _child;	/* All children get a retain from the parent */
    CFTreeContext _context;
};

CFTypeID CFTreeGetTypeID(void) {
    return __kCFTreeTypeID;
}

Boolean __CFTreeEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFTreeRef tree1 = (CFTreeRef)cf1;
    CFTreeRef tree2 = (CFTreeRef)cf2;
    if (tree1 == tree2) return TRUE;
#warning CF: unimplemented
    return FALSE;
}

CFHashCode __CFTreeHash(CFTypeRef cf) {
    CFTreeRef tree = (CFTreeRef)cf;
#warning CF: unimplemented
    return 0;
}

CFStringRef __CFTreeCopyDescription(CFTypeRef cf) {
    CFTreeRef tree = (CFTreeRef)cf;
#warning CF: this description routine needs to be made more conformant with rest of collections formats
    CFMutableStringRef result = CFStringCreateMutable(CFGetAllocator(cf), 0);
    CFStringAppendFormat(result, NULL, CFSTR("<CFTree 0x%x>{%d children; description = {"), tree, CFTreeGetChildCount(tree));
    if (tree->_context.copyDescription) {
        CFStringRef desc = tree->_context.copyDescription(tree->_context.info);
        if (desc) {
            CFStringAppend(result, desc);
            CFRelease(desc);
        } else {
            CFStringAppendFormat(result, NULL, CFSTR("<null description returned for 0x%x>"), tree->_context.info);
        }
    } else {
        CFStringAppendFormat(result, NULL, CFSTR("<no description function for 0x%x>"), tree->_context.info);
    }
    CFStringAppend(result, CFSTR("}"));
    return result;
}

CFAllocatorRef __CFTreeGetAllocator(CFTypeRef cf) {
    CFTreeRef tree = (CFTreeRef)cf;
    return tree->_allocator;
}

void __CFTreeDeallocate(CFTypeRef cf) {
    CFTreeRef tree = (CFTreeRef)cf;
    CFAllocatorRef allocator = tree->_allocator;
    CFTreeRemoveAllChildren(tree);
    if (tree->_context.release) {
        tree->_context.release(tree->_context.info);
    }
    CFAllocatorDeallocate(allocator, tree);
    CFRelease(allocator);
}

static CFTreeRef __CFTreeInit(CFAllocatorRef allocator, const CFTreeContext *context) {
    CFTreeRef memory;
    UInt32 size;
    size = sizeof(struct __CFTree);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFTreeTypeID);
    memory->_allocator = allocator;
    memory->_parent = NULL;
    memory->_sibling = NULL;
    memory->_child = NULL;
    if (NULL != context) {
	if (NULL != context->retain) {
	    memory->_context.info = (void *)context->retain(context->info);
	} else {
	    memory->_context.info = context->info;
	}
	memory->_context.retain = context->retain;
	memory->_context.release = context->release;
	memory->_context.copyDescription = context->copyDescription;
    } else {
	memory->_context.info = 0;
	memory->_context.retain = 0;
	memory->_context.release = 0;
	memory->_context.copyDescription = 0;
    }
    return memory;
}

CFTreeRef CFTreeCreate(CFAllocatorRef allocator, const CFTreeContext *context) {
    return __CFTreeInit(allocator, context);
}

CFIndex CFTreeGetChildCount(CFTreeRef tree) {
    SInt32 cnt = 0;
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    tree = tree->_child;
    while (NULL != tree) {
	cnt++;
	tree = tree->_sibling;
    }
    return cnt;
}

CFTreeRef CFTreeGetParent(CFTreeRef tree) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    return tree->_parent;
}

CFTreeRef CFTreeGetNextSibling(CFTreeRef tree) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    return tree->_sibling;
}

CFTreeRef CFTreeGetFirstChild(CFTreeRef tree) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    return tree->_child;
}

CFTreeRef CFTreeFindRoot(CFTreeRef tree) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    while (NULL != tree->_parent) {
	tree = tree->_parent;
    }
    return tree;
}

void CFTreeGetContext(CFTreeRef tree, CFTreeContext *context) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = tree->_context;
}

void CFTreeSetContext(CFTreeRef tree, const CFTreeContext *context) {
#warning CF: unimplemented
}

#if 0
CFTreeRef CFTreeFindNextSibling(CFTreeRef tree, const void *info) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    tree = tree->_sibling;
    while (NULL != tree) {
	if (info == tree->_context.info || (tree->_context.equal && tree->_context.equal(info, tree->_context.info))) {
	    return tree;
	}
	tree = tree->_sibling;
    }
    return NULL;
}

CFTreeRef CFTreeFindFirstChild(CFTreeRef tree, const void *info) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    tree = tree->_child;
    while (NULL != tree) {
	if (info == tree->_context.info || (tree->_context.equal && tree->_context.equal(info, tree->_context.info))) {
	    return tree;
	}
	tree = tree->_sibling;
    }
    return NULL;
}

CFTreeRef CFTreeFind(CFTreeRef tree, const void *info) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    if (info == tree->_context.info || (tree->_context.equal && tree->_context.equal(info, tree->_context.info))) {
	return tree;
    }
    tree = tree->_child;
    while (NULL != tree) {
	CFTreeRef found = CFTreeFind(tree, info);
	if (NULL != found) {
	    return found;
	}
	tree = tree->_sibling;
    }
    return NULL;
}
#endif

CFTreeRef CFTreeGetChildAtIndex(CFTreeRef tree, CFIndex idx) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    tree = tree->_child;
    while (NULL != tree) {
	if (0 == idx) return tree;
	idx--;
	tree = tree->_sibling;
    }
    return NULL;
}

void CFTreeGetChildren(CFTreeRef tree, CFTreeRef *children) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    tree = tree->_child;
    while (NULL != tree) {
	*children++ = tree;
	tree = tree->_sibling;
    }
}

void CFTreeApplyFunctionToChildren(CFTreeRef tree, CFTreeApplierFunction applier, void *context) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    CFAssert1(NULL != applier, __kCFLogAssertion, "%s(): pointer to applier function may not be NULL", __PRETTY_FUNCTION__);
    tree = tree->_child;
    while (NULL != tree) {
	applier(tree, context);
	tree = tree->_sibling;
    }
}

void CFTreePrependChild(CFTreeRef tree, CFTreeRef newChild) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    __CFGenericValidateType(newChild, __kCFTreeTypeID);
    CFAssert1(NULL == newChild->_parent, __kCFLogAssertion, "%s(): must remove newChild from previous parent first", __PRETTY_FUNCTION__);
    CFAssert1(NULL == newChild->_sibling, __kCFLogAssertion, "%s(): must remove newChild from previous parent first", __PRETTY_FUNCTION__);
    CFRetain(newChild);
    newChild->_parent = tree;
    newChild->_sibling = tree->_child;
    tree->_child = newChild;
}

void CFTreeAppendChild(CFTreeRef tree, CFTreeRef newChild) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    __CFGenericValidateType(newChild, __kCFTreeTypeID);
    CFAssert1(NULL == newChild->_parent, __kCFLogAssertion, "%s(): must remove newChild from previous parent first", __PRETTY_FUNCTION__);
    CFAssert1(NULL == newChild->_sibling, __kCFLogAssertion, "%s(): must remove newChild from previous parent first", __PRETTY_FUNCTION__);
    if (newChild->_parent) {
        __CFAbort();
    }
    CFRetain(newChild);
    newChild->_parent = tree;
    newChild->_sibling = NULL;
    if (!tree->_child) {
        tree->_child = newChild;
    } else {
        CFTreeRef child;
        for (child = tree->_child; child->_sibling; child = child->_sibling)
            ;
        child->_sibling = newChild;
    }
}

void CFTreeInsertSibling(CFTreeRef tree, CFTreeRef newSibling) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    __CFGenericValidateType(newSibling, __kCFTreeTypeID);
    CFAssert1(NULL == newSibling->_parent, __kCFLogAssertion, "%s(): must remove newSibling from previous parent first", __PRETTY_FUNCTION__);
    CFAssert1(NULL == newSibling->_sibling, __kCFLogAssertion, "%s(): must remove newSibling from previous parent first", __PRETTY_FUNCTION__);
    CFRetain(newSibling);
    newSibling->_parent = tree;
    newSibling->_sibling = tree->_sibling;
    tree->_sibling = newSibling;
}

void CFTreeRemove(CFTreeRef tree) {
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    if (NULL != tree->_parent) {
	if (tree == tree->_parent->_child) {
	    tree->_parent->_child = tree->_sibling;
	} else {
	    CFTreeRef prevSibling = NULL;
	    for (prevSibling = tree->_parent->_child; prevSibling; prevSibling = prevSibling->_sibling) {
		if (prevSibling->_sibling == tree) {
		    prevSibling->_sibling = tree->_sibling;
		    break;
		}
	    }
	}
	tree->_parent = NULL;
	tree->_sibling = NULL;
	CFRelease(tree);
    }
}

void CFTreeRemoveAllChildren(CFTreeRef tree) {
    CFTreeRef nextChild;
    __CFGenericValidateType(tree, __kCFTreeTypeID);
    nextChild = tree->_child;
    tree->_child = NULL;
    while (NULL != nextChild) {
	CFTreeRef nextSibling = nextChild->_sibling;
	nextChild->_parent = NULL;
	nextChild->_sibling = NULL;
	CFRelease(nextChild);
	nextChild = nextSibling;
    }
}

void CFTreeSortChildren(CFTreeRef tree, CFComparatorFunction comparator, void *context) {
#warning CF: unimplemented
}

