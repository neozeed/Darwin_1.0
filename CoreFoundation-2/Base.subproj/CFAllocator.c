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
/*	CFAllocator.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFBase.h>
#include "CFInternal.h"
#if defined(__MACOS8__)
    #include <MacMemory.h>
#endif
#include <stdlib.h>

struct __CFAllocator {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFAllocatorContext _context;
};

#if defined(__MACOS8__)
CF_EXPORT void __CFRecordAllocationEvent(int eventnum, void * ptr, int size, int data, char *classname) {
}
#endif

/* !!! Note: If these functions are changed not to use malloc(), etc, then an alternate set of
   functions should be created for kCFAllocatorMalloc.
*/
static void *__CFAllocatorSystemAllocate(CFIndex size, CFOptionFlags hint, void *info) {
    void *ptr = malloc(size);
    return ptr;
}

static void *__CFAllocatorSystemReallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
    void *newptr = realloc(ptr, newsize);    
    return newptr;
}

static void __CFAllocatorSystemDeallocate(void *ptr, void *info) {
    free(ptr);
}

static CFIndex __CFAllocatorSystemPreferredSize(CFIndex size, CFOptionFlags hint, void *info) {
    return size;
}

static struct __CFAllocator __kCFAllocatorSystemDefault = {
#if CF_HAS_ISA
    {NULL, 0},
#else
    {0},
#endif
    NULL,
    {0, NULL, NULL, NULL, NULL, __CFAllocatorSystemAllocate, __CFAllocatorSystemReallocate, __CFAllocatorSystemDeallocate, __CFAllocatorSystemPreferredSize}
};

static struct __CFAllocator __kCFAllocatorMalloc = {
#if CF_HAS_ISA
    {NULL, 0},
#else
    {0},
#endif
    NULL,
    {0, NULL, NULL, NULL, NULL, __CFAllocatorSystemAllocate, __CFAllocatorSystemReallocate, __CFAllocatorSystemDeallocate, __CFAllocatorSystemPreferredSize}
};

static void *__CFAllocatorNullAllocate(CFIndex size, CFOptionFlags hint, void *info) {
    return NULL;
}

static void *__CFAllocatorNullReallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
    return NULL;
}

static void __CFAllocatorNullDeallocate(void *ptr, void *info) {
}

static CFIndex __CFAllocatorNullPreferredSize(CFIndex size, CFOptionFlags hint, void *info) {
    return size;
}

static struct __CFAllocator __kCFAllocatorNull = {
#if CF_HAS_ISA
    {NULL, 0},
#else
    {0},
#endif
    NULL,
    {0, NULL, NULL, NULL, NULL, __CFAllocatorNullAllocate, __CFAllocatorNullReallocate, __CFAllocatorNullDeallocate, __CFAllocatorNullPreferredSize}
};

const CFAllocatorRef kCFAllocatorDefault = NULL;
const CFAllocatorRef kCFAllocatorSystemDefault = &__kCFAllocatorSystemDefault;
const CFAllocatorRef kCFAllocatorMalloc = &__kCFAllocatorMalloc;
const CFAllocatorRef kCFAllocatorNull = &__kCFAllocatorNull;
const CFAllocatorRef kCFAllocatorUseContext = (CFAllocatorRef)0x0221;

void __CFInitializeAllocators(void) {
    __CFGenericInitBase(&__kCFAllocatorSystemDefault, NULL, __kCFAllocatorTypeID);
    __kCFAllocatorSystemDefault._allocator = &__kCFAllocatorSystemDefault;
    __CFGenericInitBase(&__kCFAllocatorMalloc, NULL, __kCFAllocatorTypeID);
    __kCFAllocatorMalloc._allocator = &__kCFAllocatorSystemDefault;
    __CFGenericInitBase(&__kCFAllocatorNull, NULL, __kCFAllocatorTypeID);
    __kCFAllocatorNull._allocator = &__kCFAllocatorSystemDefault;
}

CFTypeID CFAllocatorGetTypeID(void) {
    return __kCFAllocatorTypeID;
}

Boolean __CFAllocatorEqual(CFTypeRef cf1, CFTypeRef cf2) {
    return (cf1 == cf2);
}

CFHashCode __CFAllocatorHash(CFTypeRef cf) {
    return (CFHashCode)cf;
}

CFStringRef __CFAllocatorCopyDescription(CFTypeRef cf) {
    CFAllocatorRef self = cf;
    CFAllocatorRef allocator = (kCFAllocatorUseContext == self->_allocator) ? self : self->_allocator;
    return CFStringCreateWithFormat(allocator, NULL, CFSTR("<CFAllocator 0x%x [0x%x]>{info = 0x%x}"), (UInt32)cf, (UInt32)allocator, self->_context.info);
#warning CF: should use copyDescription function here to describe info field
// remember to release value returned from copydescr function when this happens
// also remember to fault the copyDesc callback first
}

CFAllocatorRef __CFAllocatorGetAllocator(CFTypeRef cf) {
    CFAllocatorRef allocator = cf;
    return (kCFAllocatorUseContext == allocator->_allocator) ? allocator : allocator->_allocator;
}

void __CFAllocatorDeallocate(CFTypeRef cf) {
    CFAllocatorRef self = cf;
    CFAllocatorRef allocator = self->_allocator;
    FAULT_CALLBACK((void **)&(self->_context.release));
    if (kCFAllocatorUseContext == allocator) {
	/* Rather a chicken and egg problem here, so we do things
	   in the reverse order from what was done at create time. */
	void (*releaseFunc)(const void *) = self->_context.release;
	void *info = self->_context.info;
	if (NULL != self->_context.deallocate) {
	    INVOKE_CALLBACK2(self->_context.deallocate, (void *)self, info);
	    __CFRecordAllocationEvent(__kCFAllocatorDeallocEvent, (void *)self, 0, 0, NULL);
	}
	if (NULL != releaseFunc) {
	    INVOKE_CALLBACK1(releaseFunc, info);
	}
    } else {
	if (NULL != self->_context.release) {
	    INVOKE_CALLBACK1(self->_context.release, self->_context.info);
	}
	CFAllocatorDeallocate(allocator, (void *)self);
    }
}

CFAllocatorRef CFAllocatorGetDefault(void) {
    CFAllocatorRef allocator = __CFGetThreadSpecificData()->_allocator;
    if (NULL == allocator) {
	allocator = kCFAllocatorSystemDefault;
    }
    return allocator;
}

void CFAllocatorSetDefault(CFAllocatorRef allocator) {
    CFAllocatorRef current = __CFGetThreadSpecificData()->_allocator;
#if defined(DEBUG) 
    if (NULL != allocator) {
	__CFGenericValidateType(allocator, __kCFAllocatorTypeID);
    }
#endif
    if (NULL != allocator && allocator != current) {
	if (current) CFRelease(current);
	CFRetain(allocator);
	// We retain an extra time so that anything set as the default
	// allocator never goes away.
	CFRetain(allocator);
	__CFGetThreadSpecificData()->_allocator = (void *)allocator;
    }
}

CFAllocatorRef CFAllocatorCreate(CFAllocatorRef allocator, CFAllocatorContext *context) {
    struct __CFAllocator *memory;
    const void *(*retain)(const void *);
    void *(*allocate)(CFIndex, CFOptionFlags, void *);
    void *retainedInfo;
    if (NULL == context->allocate) {
	__CFAbort();
    }
    retain = context->retain;
    FAULT_CALLBACK((void **)&(retain));
    if (retain) {
	retainedInfo = (void *)INVOKE_CALLBACK1(retain, context->info);
    } else {
	retainedInfo = context->info;
    }
    allocate = context->allocate;
    FAULT_CALLBACK((void **)&(allocate));
    if (kCFAllocatorUseContext == allocator) {
	memory = (void *)INVOKE_CALLBACK3(context->allocate, sizeof(struct __CFAllocator), 0, retainedInfo);
	if (NULL == memory) {
	    return NULL;
	}
	__CFRecordAllocationEvent(__kCFAllocatorAllocEvent, memory, sizeof(struct __CFAllocator), 0, NULL);
    } else {
	allocator = (NULL == allocator) ? CFAllocatorGetDefault() : allocator;
	memory = CFAllocatorAllocate(allocator, sizeof(struct __CFAllocator), 0);
	if (NULL == memory) {
	    return NULL;
	}
    }
    __CFGenericInitBase((struct __CFAllocator *)memory, NULL, __kCFAllocatorTypeID);
    ((struct __CFAllocator *)memory)->_allocator = allocator;
    ((struct __CFAllocator *)memory)->_context.info = retainedInfo;
    ((struct __CFAllocator *)memory)->_context.retain = retain;
    ((struct __CFAllocator *)memory)->_context.release = context->release;
    ((struct __CFAllocator *)memory)->_context.copyDescription = context->copyDescription;
    ((struct __CFAllocator *)memory)->_context.allocate = allocate;
    ((struct __CFAllocator *)memory)->_context.reallocate = context->reallocate;
    ((struct __CFAllocator *)memory)->_context.deallocate = context->deallocate;
    ((struct __CFAllocator *)memory)->_context.preferredSize = context->preferredSize;
    // Only the allocate, reallocate, deallocate, and preferredSize
    // callbacks are pre-faulted, so we don't have to do them over
    // and over; the others are used rarely. Note that the allocate
    // callback was faulted above so we don't do it again.
    FAULT_CALLBACK((void **)&(memory->_context.reallocate));
    FAULT_CALLBACK((void **)&(memory->_context.deallocate));
    FAULT_CALLBACK((void **)&(memory->_context.preferredSize));
    return memory;
}

void *CFAllocatorAllocate(CFAllocatorRef allocator, CFIndex size, CFOptionFlags hint) {
    void *newptr;
    allocator = (NULL == allocator) ? CFAllocatorGetDefault() : allocator;
    __CFGenericValidateType(allocator, __kCFAllocatorTypeID);
    if (0 == size) return NULL;
    newptr = (void *)INVOKE_CALLBACK3(allocator->_context.allocate, size, hint, allocator->_context.info);
    if (0 == (hint & __kCFAllocatorDoNotRecordEvent)) {
        __CFRecordAllocationEvent(__kCFAllocatorAllocEvent, newptr, size, 0, NULL);
    }
    return newptr;
}

void *CFAllocatorReallocate(CFAllocatorRef allocator, void *ptr, CFIndex newsize, CFOptionFlags hint) {
    void *newptr;
    allocator = (NULL == allocator) ? CFAllocatorGetDefault() : allocator;
    __CFGenericValidateType(allocator, __kCFAllocatorTypeID);
    if (NULL == ptr && 0 < newsize) {
	newptr = (void *)INVOKE_CALLBACK3(allocator->_context.allocate, newsize, hint, allocator->_context.info);
        if (0 == (hint & __kCFAllocatorDoNotRecordEvent)) {
	    __CFRecordAllocationEvent(__kCFAllocatorAllocEvent, newptr, newsize, 0, NULL);
	}
	return newptr;
    }
    if (NULL != ptr && 0 == newsize) {
	if (NULL != allocator->_context.deallocate) {
	    INVOKE_CALLBACK2(allocator->_context.deallocate, ptr, allocator->_context.info);
	    __CFRecordAllocationEvent(__kCFAllocatorDeallocEvent, ptr, 0, 0, NULL);
	}
	return NULL;
    }
    if ((NULL == ptr && 0 == newsize) || NULL == allocator->_context.reallocate) {
	return NULL;
    }
    newptr = (void *)INVOKE_CALLBACK4(allocator->_context.reallocate, ptr, newsize, hint, allocator->_context.info);
    if (0 == (hint & __kCFAllocatorDoNotRecordEvent)) {
        __CFRecordAllocationEvent(__kCFAllocatorReallocEvent, newptr, newsize, (int)ptr, NULL);
    }
    return newptr;
}

void CFAllocatorDeallocate(CFAllocatorRef allocator, void *ptr) {
    allocator = (NULL == allocator) ? CFAllocatorGetDefault() : allocator;
    __CFGenericValidateType(allocator, __kCFAllocatorTypeID);
    if (NULL != ptr && NULL != allocator->_context.deallocate) {
	INVOKE_CALLBACK2(allocator->_context.deallocate, ptr, allocator->_context.info);
	__CFRecordAllocationEvent(__kCFAllocatorDeallocEvent, ptr, 0, 0, NULL);
    }
}

CFIndex CFAllocatorGetPreferredSizeForSize(CFAllocatorRef allocator, CFIndex size, CFOptionFlags hint) {
    CFIndex newsize = 0;
    allocator = (NULL == allocator) ? CFAllocatorGetDefault() : allocator;
    __CFGenericValidateType(allocator, __kCFAllocatorTypeID);
    if (0 < size && NULL != allocator->_context.preferredSize) {
	newsize = (CFIndex)(INVOKE_CALLBACK3(allocator->_context.preferredSize, size, hint, allocator->_context.info));
    }
    if (newsize < size) newsize = size;
    return newsize;
}

void CFAllocatorGetContext(CFAllocatorRef allocator, CFAllocatorContext *context) {
    allocator = (NULL == allocator) ? CFAllocatorGetDefault() : allocator;
    __CFGenericValidateType(allocator, __kCFAllocatorTypeID);
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = allocator->_context;
}

#if defined(__MACOS8__)
static CFStringRef _tempMemDescription(const void *info) {
    CFStringRef descStr = CFSTR("Allocator to get memory from TempMem on MacOS 8");
    CFRetain(descStr);
    return descStr;
}

static Boolean _isHeapMemory(void *ptr) {
        THz appZone = ApplicationZone();
        return (ptr >= &(appZone->heapData) && ptr < appZone->bkLim);

}

static void *_tempMemAllocate(CFIndex size, CFOptionFlags hint, void *info) {
    OSErr error;
    Handle memHandle = TempNewHandle(size, &error);
    if (memHandle) {
        HLock(memHandle);
        return *memHandle;
    } else {
        // Try to get the memory from the heap 
        return CFAllocatorAllocate(kCFAllocatorSystemDefault, size, hint);
    }
}

static void *_tempMemReallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info){
        if (_isHeapMemory(ptr)) {
                return CFAllocatorReallocate(kCFAllocatorSystemDefault, ptr, newsize, hint);
        } else {
            Handle handle = RecoverHandle(ptr);
        if (!handle) {
                return NULL;
        } else {
                HUnlock(handle);
                SetHandleSize(handle, newsize);
                HLock(handle);
                return *handle;
        }
        }
}

static void _tempMemDeallocate(void *ptr, void *info) {
        if (_isHeapMemory(ptr)) {
                CFAllocatorDeallocate(kCFAllocatorSystemDefault, ptr);
        } else {
        Handle handle = RecoverHandle(ptr);
            DisposeHandle(handle);
        }
}

static CFIndex  _tempMemPreferredSize(CFIndex size, CFOptionFlags hint, void *info) {
    return size;
}

CF_EXPORT CFAllocatorRef _CFTemporaryMemoryAllocator(void) {
    static CFAllocatorRef tempMemAllocator = NULL;
    if (!tempMemAllocator) {
        CFAllocatorContext ctxt;
        ctxt.version = 0;
        ctxt.info = NULL;
        ctxt.retain = NULL;
        ctxt.release = NULL;
        ctxt.copyDescription = _tempMemDescription;
        ctxt.allocate = _tempMemAllocate;
        ctxt.reallocate = _tempMemReallocate;
        ctxt.deallocate = _tempMemDeallocate;
        ctxt.preferredSize = _tempMemPreferredSize;
        tempMemAllocator = CFAllocatorCreate(kCFAllocatorUseContext, &ctxt);
    }
    return tempMemAllocator;
}
#endif
