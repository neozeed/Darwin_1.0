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
/*	CFBase.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFBase.h>
#include "CFInternal.h"
#if defined(__MACH__)
    #include <pthread.h>
#else
    #include <windows.h>
#endif

#define DO_SYSCALL_TRACE_HELPERS 1
#if defined(DO_SYSCALL_TRACE_HELPERS) && defined(__MACH__)
extern void ptrace(int, int, int, int);
#define SYSCALL_TRACE(N)        do ptrace(N, 0, 0, 0); while (0)
#else
#define SYSCALL_TRACE(N)        do {} while (0)
#endif



CFTypeID __kCFNotAtTypeTypeID = 0;
static const struct __CFTypeClass __CFNotATypeClass = {"Not A Type", (void *)__CFAbort, (void *)__CFAbort, (void *)__CFAbort, (void *)__CFAbort, (void *)__CFAbort};
CFTypeID __kCFTypeTypeID = 0;
static struct __CFTypeClass __CFTypeClass = {"CFType", (void *)__CFAbort, (void *)__CFAbort, (void *)__CFAbort, (void *)__CFAbort, (void *)__CFAbort};

CF_EXPORT CFTypeID CFTypeGetTypeID() {
    return __kCFTypeTypeID;
}

extern Boolean __CFStringEqual(CFTypeRef cf1, CFTypeRef cf2);
extern CFHashCode __CFStringHash(CFTypeRef cf);
extern CFStringRef __CFStringCopyDescription(CFTypeRef cf);
extern CFAllocatorRef __CFStringGetAllocator(CFTypeRef cf);
extern void __CFStringDeallocate(CFTypeRef cf);
CFTypeID __kCFStringTypeID = 0;
struct __CFTypeClass __CFStringClass = { "CFString",
	__CFStringEqual, __CFStringHash,
	__CFStringCopyDescription, __CFStringGetAllocator,
	__CFStringDeallocate};

#define DEFINE_CLASS(NAME)						\
extern Boolean __CF ## NAME ## Equal(CFTypeRef cf1, CFTypeRef cf2);	\
extern CFTypeID __CF ## NAME ## Hash(CFTypeRef cf);			\
extern CFStringRef __CF ## NAME ## CopyDescription(CFTypeRef cf);	\
extern CFAllocatorRef __CF ## NAME ## GetAllocator(CFTypeRef cf);	\
extern void __CF ## NAME ## Deallocate(CFTypeRef cf);			\
CFTypeID __kCF ## NAME ## TypeID = 0;					\
static struct __CFTypeClass __CF ## NAME ## Class = { "CF" #NAME,	\
	__CF ## NAME ## Equal, __CF ## NAME ## Hash,			\
	__CF ## NAME ## CopyDescription, __CF ## NAME ## GetAllocator,	\
	__CF ## NAME ## Deallocate};

DEFINE_CLASS(Allocator)
DEFINE_CLASS(Array)
DEFINE_CLASS(Bag)
DEFINE_CLASS(Boolean)
DEFINE_CLASS(CharacterSet)
DEFINE_CLASS(Data)
DEFINE_CLASS(Date)
DEFINE_CLASS(Dictionary)
DEFINE_CLASS(Number)
DEFINE_CLASS(Set)
DEFINE_CLASS(Storage)
DEFINE_CLASS(TimeZone)
DEFINE_CLASS(Tree)
DEFINE_CLASS(URL)
DEFINE_CLASS(XMLParser)



#if defined(__MACH__)
DEFINE_CLASS(MachPort)
#endif __MACH__

#if (defined(__MACH__) || defined(__WIN32__))
DEFINE_CLASS(MessagePort)
DEFINE_CLASS(RunLoop)
DEFINE_CLASS(RunLoopObserver)
DEFINE_CLASS(RunLoopSource)
DEFINE_CLASS(RunLoopTimer)
DEFINE_CLASS(Socket)
#endif (__MACH__ || __WIN32__)




// warning: this number must be large enough!
// increment this number for every new CF type added
// there is a check to assure this number is big enough; if not, CF will abort immediately
// warning: trying to allocate this with an allocator this early does not work
// also, accesses to __CFClassTable and __CFClassTableCount are not thread-safe, but
// the table is read-only after __CFInitialize(), and we assume that initialization
// occurs before we go multithreaded
#define kCFNumberOfClasses 41
struct __CFTypeClass *__CFClassTable[kCFNumberOfClasses] = {0};
static UInt32 __CFClassTableCount = 0;

static UInt32 __CFTypeRegisterClass(const struct __CFTypeClass *cfcls) {
    if (__CFClassTableCount == kCFNumberOfClasses) __CFAbort();
    __CFClassTable[__CFClassTableCount++] = (struct __CFTypeClass *)cfcls;
    return __CFClassTableCount - 1;
}

void __CFGenericValidateType_(CFTypeRef cf, CFTypeID type, char *func) {
    CFAssert2((cf != NULL) && (__CFGenericTypeID(cf) < __CFClassTableCount) && (__kCFNotAtTypeTypeID != __CFGenericTypeID(cf)) && (__kCFTypeTypeID != __CFGenericTypeID(cf)), __kCFLogAssertion, "%s(): pointer 0x%x is not a CF object", func, cf); \
    CFAssert3(__CFGenericTypeID(cf) == type, __kCFLogAssertion, "%s(): pointer 0x%x is not a %s", func, cf, __CFClassTable[type]->name);	\
}

static UInt32 __CFGlobalRetainLock = 0;

// Called for each thread as it exits
static void __CFFinalizeThreadData(void *arg) {
    __CFThreadSpecificData *data = (__CFThreadSpecificData *)arg;
    if (NULL == data) return; 
    if (data->_allocator) CFRelease(data->_allocator);
    if (data->_runLoop) CFRelease(data->_runLoop);
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, data);
}

#if defined(__MACH__)
static pthread_key_t __CFTSDKey = NULL;
#elif defined(__WIN32__)
static DWORD __CFTSDKey = 0xFFFFFFFF;
#endif

__CFThreadSpecificData *__CFGetThreadSpecificData(void) {
#if defined(__MACOS8__)
    /* for MacOS8, return a single global data structure declared in CFBase.c */
    extern __CFThreadSpecificData __CFMacOS8ThreadData;
    return &__CFMacOS8ThreadData;
#elif defined(__MACH__)
    __CFThreadSpecificData *data;
    data = pthread_getspecific(__CFTSDKey);
    if (data) {
	return data;
    }
    data = CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(__CFThreadSpecificData), 0);
    CFZeroMemory(data, sizeof(__CFThreadSpecificData));
    pthread_setspecific(__CFTSDKey, data);
    return data;
#elif defined(__WIN32__)
    __CFThreadSpecificData *data;
    data = TlsGetValue(__CFTSDKey);
    if (data) {
	return data;
    }
    data = CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(__CFThreadSpecificData), 0);
    CFZeroMemory(data, sizeof(__CFThreadSpecificData));
    TlsSetValue(__CFTSDKey, data);
    return data;
#endif
}


#if defined(__MACOS8__)
__CFThreadSpecificData __CFMacOS8ThreadData;
#endif

static CFMutableDictionaryRef externalRefCountTable = NULL;

//#pragma CALL_ON_MODULE_BIND __CFInitialize
CF_EXPORT void __CFInitialize(void) {
    static int __done = 0;
    if (!__done) {
	__done = 1;

	SYSCALL_TRACE(0xC000);

	/* Initialize Types */

	/* __kCFNotAtTypeTypeID should be first, to get index 0 */
	__kCFNotAtTypeTypeID = __CFTypeRegisterClass(&__CFNotATypeClass);
	__kCFTypeTypeID = __CFTypeRegisterClass(&__CFTypeClass);
	/* This needs to be here so that __kCFStringTypeID gets index 2. */
	__kCFStringTypeID = __CFTypeRegisterClass(&__CFStringClass);

	__kCFAllocatorTypeID = __CFTypeRegisterClass(&__CFAllocatorClass);
	__kCFArrayTypeID = __CFTypeRegisterClass(&__CFArrayClass);
	__kCFBagTypeID = __CFTypeRegisterClass(&__CFBagClass);
	__kCFBooleanTypeID = __CFTypeRegisterClass(&__CFBooleanClass);
        __kCFCharacterSetTypeID = __CFTypeRegisterClass(&__CFCharacterSetClass);
	__kCFDataTypeID = __CFTypeRegisterClass(&__CFDataClass);
	__kCFDateTypeID = __CFTypeRegisterClass(&__CFDateClass);
	__kCFDictionaryTypeID = __CFTypeRegisterClass(&__CFDictionaryClass);
	__kCFNumberTypeID = __CFTypeRegisterClass(&__CFNumberClass);
        __kCFSetTypeID = __CFTypeRegisterClass(&__CFSetClass);
        __kCFStorageTypeID = __CFTypeRegisterClass(&__CFStorageClass);
	__kCFTimeZoneTypeID = __CFTypeRegisterClass(&__CFTimeZoneClass);
	__kCFTreeTypeID = __CFTypeRegisterClass(&__CFTreeClass);
        __kCFURLTypeID = __CFTypeRegisterClass(&__CFURLClass);
        __kCFXMLParserTypeID = __CFTypeRegisterClass(&__CFXMLParserClass);
#if defined(__MACH__)
        __kCFMachPortTypeID = __CFTypeRegisterClass(&__CFMachPortClass);
#endif __MACH__
#if (defined(__MACH__) || defined(__WIN32__))       
	__kCFMessagePortTypeID = __CFTypeRegisterClass(&__CFMessagePortClass);
	__kCFRunLoopTypeID = __CFTypeRegisterClass(&__CFRunLoopClass);
	__kCFRunLoopObserverTypeID = __CFTypeRegisterClass(&__CFRunLoopObserverClass);
	__kCFRunLoopSourceTypeID = __CFTypeRegisterClass(&__CFRunLoopSourceClass);
	__kCFRunLoopTimerTypeID = __CFTypeRegisterClass(&__CFRunLoopTimerClass);
        __kCFSocketTypeID = __CFTypeRegisterClass(&__CFSocketClass);
#endif (__MACH__ || __WIN32__)

	if (__kCFStringTypeID_CONST != __kCFStringTypeID) {
	    /* Don't log here, because things aren't properly setup yet. */
	    __CFAbort();
	}

	if (__CFClassTableCount > kCFNumberOfClasses) {		/* Oops, we've overwritten the class table; should grow it. */
	    /* Don't log here, because things aren't properly setup yet. */
	    /* Don't put any calls to __CFTypeRegisterClass() after this point; 
               or if you do, then we should move this check into __CFTypeRegisterClass(). */
	    __CFAbort();
	}

	SYSCALL_TRACE(0xC001);

	/* Initialize constants */
	do {
	    extern void __CFInitializeAllocators(void);
	    extern void __CFInitializeNumbersAndBooleans(void);
	    extern void __CFInitializeDate(void);

	    __CFInitializeAllocators();
	    __CFInitializeNumbersAndBooleans();
	    __CFInitializeDate();
#if defined(__MACH__)
	    pthread_key_create(&__CFTSDKey, __CFFinalizeThreadData);
#elif defined(__WIN32__)
	    __CFTSDKey = TlsAlloc();
#endif
	} while (0);

	SYSCALL_TRACE(0xC003);

	// Creating this in CFRetain causes recursive call to CFRetain, so do it here
	externalRefCountTable = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

#if defined(DEBUG) && !defined(__MACOS8__)
// Don't log on MacOS 8 as this will create a log file unnecessarily
	CFLog (0, CFSTR("Assertions enabled"));
#endif

	SYSCALL_TRACE(0xC0FF);
    }
}


#if !defined(CFTYPE_OBJC_FUNCDISPATCH0)
#define CFTYPE_OBJC_FUNCDISPATCH0(rettype, obj, sel)
#endif

#if !defined(CFTYPE_OBJC_FUNCDISPATCH1)
#define CFTYPE_OBJC_FUNCDISPATCH1(rettype, obj, sel, a1)
#endif

#if !defined(CFTYPE_IS_OBJC)
#define CFTYPE_IS_OBJC(obj) (FALSE)
#endif

#define __CFGenericAssertIsCF(cf) \
    CFAssert2(cf != NULL && __CFGenericTypeID(cf) < __CFClassTableCount && __kCFNotAtTypeTypeID != __CFGenericTypeID(cf) && __kCFTypeTypeID != __CFGenericTypeID(cf), __kCFLogAssertion, "%s(): pointer 0x%x is not a CF object", __PRETTY_FUNCTION__, cf);


CFTypeID CFGetTypeID(CFTypeRef cf) {
    if (NULL == cf) __CFAbort();
    CFTYPE_OBJC_FUNCDISPATCH0(CFTypeID, cf, "_cfTypeID");
    __CFGenericAssertIsCF(cf);
    return __CFGenericTypeID(cf);
}

CFStringRef CFCopyTypeIDDescription(CFTypeID type) {
    CFAssert2(type < __CFClassTableCount && __kCFNotAtTypeTypeID != type && __kCFTypeTypeID != type, __kCFLogAssertion, "%s(): type %d is not a CF type ID", __PRETTY_FUNCTION__, type);
    return CFStringCreateWithCString(NULL, __CFClassTable[type]->name, CFStringGetSystemEncoding());
}

void __CFDeallocate(CFTypeRef cf) {
    __CFGenericAssertIsCF(cf);
    __CFClassTable[__CFGenericTypeID(cf)]->dealloc(cf);
}


#if 0
// A PPC assembly version which increments the retain count in
// an internal lock (sort of) (rc is bits 31-17 of 2nd word of 'cf')
// [bit 16 is "overflow" bit]
CFTypeRef CFRetain2(CFTypeRef cf) {
    CFTYPE_OBJC_FUNCDISPATCH0(CFTypeRef , cf, "retain");
    __CFGenericAssertIsCF(cf);
    __asm__ volatile("addi	r4,%0,4" : : "r" (cf) : "r4", "r5");
    __asm__ volatile("1: lwarx	r5,0,r4" : : : "r4", "r5");
    __asm__ volatile("addis	r5,r5,0x2" : : : "r4", "r5");
    __asm__ volatile("stwcx.	r5,0,r4" : : : "r4", "r5");
    __asm__ volatile("bne-	1b" : : : "r4", "r5");
    /* Here, only one thread can have successfully stored 0x000(0|1).... */
    __asm__ volatile("andis.	r5,r5,0xFFFE" : : : "r5");
    __asm__ volatile("bne	out");
    __asm__ volatile("2: lwarx	r5,0,r4" : : : "r4", "r5");
    __asm__ volatile("oris	r5,r5,0x1" : : : "r4", "r5");
    __asm__ volatile("stwcx.	r5,0,r4" : : : "r4", "r5");
    __asm__ volatile("bne-	2b" : : : "r4", "r5");
//  do the overflow thing
    CFIndex externalRefCount = (CFIndex)CFDictionaryGetValue(externalRefCountTable, cf);  // NULL return -> 0 ref count
    externalRefCount++;
    CFDictionarySetValue(externalRefCountTable, cf, (void *)externalRefCount);

out:
    return cf;
}
void CFRelease2(CFTypeRef cf) {
    CFTYPE_OBJC_FUNCDISPATCH0(CFTypeRef , cf, "release");
    __CFGenericAssertIsCF(cf);
    __asm__ volatile("addi	r4,%0,4" : : "r" (cf) : "r4");
    __asm__ volatile("1: lwarx	r5,0,r4" : : : "r4", "r5");
// if r5 & 0xFFFF0000 == 0, deallocate
    __asm__ volatile("rlwinm	r6,r5,16,16,31" : : : "r5", "r6");
    __asm__ volatile("beq	2f");
// Here, if (r5 & 0xFFFE0000) == 0x0, 
//     set r5 to ((r5 & 0x0001FFFF) | 0xFFFE0000) and save
//     and do the overflow thing
// BUT: how does the overflow bit get unset... don't want to unset it later!
//   maybe we just never unset it after being set?
    __asm__ volatile("addis	r5,r5,0xFFFE" : : : "r5");
    __asm__ volatile("stwcx.	r5,0,r4" : : : "r4", "r5");
    __asm__ volatile("bne-	1b");
    __asm__ volatile("blr");
    __asm__ volatile("2: nop");
    __CFDeallocate((void *)cf);
}
#endif

CFTypeRef CFRetain(CFTypeRef cf) {
    CFIndex rc;
    if (NULL == cf) __CFAbort();
    CFTYPE_OBJC_FUNCDISPATCH0(CFTypeRef , cf, "retain");
    __CFGenericAssertIsCF(cf);
    __CFSpinLock(&__CFGlobalRetainLock);
    rc = __CFBitfieldValue(((CFRuntimeBase *)cf)->_info, 31, 16);
    rc++;
    if ((rc & 0x07fff) == 0) {	// Roll over another bit to the external ref count
	CFIndex externalRefCount = (CFIndex)CFDictionaryGetValue(externalRefCountTable, cf);  // NULL return -> 0 ref count
	externalRefCount++;
        CFDictionarySetValue(externalRefCountTable, cf, (void *)externalRefCount);
	rc = 0x8000;	// Bit 16 indicates that there is an external ref count
    }
    __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_info, 31, 16, rc);
    __CFSpinUnlock(&__CFGlobalRetainLock);
    __CFRecordAllocationEvent(__kCFRetainEvent, cf, 0, CFGetRetainCount(cf), NULL);
    return cf;
}

void CFRelease(CFTypeRef cf) {
    CFIndex rc;
    if (NULL == cf) __CFAbort();
    CFTYPE_OBJC_FUNCDISPATCH0(void, cf, "release");
    __CFGenericAssertIsCF(cf);
    __CFRecordAllocationEvent(__kCFReleaseEvent, cf, 0, CFGetRetainCount(cf), NULL);
    __CFSpinLock(&__CFGlobalRetainLock);
    rc = __CFBitfieldValue(((CFRuntimeBase *)cf)->_info, 31, 16);
    if (rc == 0) {
        __CFSpinUnlock(&__CFGlobalRetainLock);
        __CFDeallocate((void *)cf);
    } else {
	if (rc == 0x08000) {	// Time to remove a bit from the external ref count
            CFIndex externalRefCount = (CFIndex)CFDictionaryGetValue(externalRefCountTable, cf);	// NULL return -> 0 ref count
	    if (--externalRefCount == 0) {
		CFDictionaryRemoveValue(externalRefCountTable, cf);
		rc = 0x07fff;
	    } else {
           	CFDictionarySetValue(externalRefCountTable, cf, (void *)externalRefCount);
		rc = 0x0ffff;
	    }
 	} else {
	    rc--;
	}
        __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_info, 31, 16, rc);
        __CFSpinUnlock(&__CFGlobalRetainLock);
    }
}

CFIndex CFGetRetainCount(CFTypeRef cf) {
    CFIndex rc;
    if (NULL == cf) __CFAbort();
    CFTYPE_OBJC_FUNCDISPATCH0(CFIndex, cf, "retainCount");
    __CFGenericAssertIsCF(cf);
    rc = __CFBitfieldValue(((CFRuntimeBase *)cf)->_info, 31, 16);
    if ((rc & 0x08000) != 0) {
        CFIndex externalRefCount;
        CFAssert2(externalRefCountTable != NULL, __kCFLogAssertion, "%s(): invalid retain count %d", __PRETTY_FUNCTION__, rc);
        externalRefCount = (CFIndex)CFDictionaryGetValue(externalRefCountTable, cf);	// NULL return -> 0 ref count
        CFAssert2(externalRefCount > 0, __kCFLogAssertion, "%s(): invalid retain count %d", __PRETTY_FUNCTION__, rc);
	if (externalRefCount >= 0x0ffff) {
	    if (externalRefCount > 0x0ffff || rc == 0x0ffff) return 0x7fffffff;	// ???
	}
	rc = (rc & 0x7fff) + (externalRefCount << 15);
    }
    return rc + 1;
}

Boolean CFEqual(CFTypeRef cf1, CFTypeRef cf2) {
    if (NULL == cf1) __CFAbort();
    if (NULL == cf2) __CFAbort();
    if (cf1 == cf2) return TRUE;
    CFTYPE_OBJC_FUNCDISPATCH1(Boolean, cf1, "isEqual:", cf2);
    CFTYPE_OBJC_FUNCDISPATCH1(Boolean, cf2, "isEqual:", cf1);
    __CFGenericAssertIsCF(cf1);
    __CFGenericAssertIsCF(cf2);
    if (__CFGenericTypeID(cf1) != __CFGenericTypeID(cf2)) return FALSE;
    if (NULL != __CFClassTable[__CFGenericTypeID(cf1)]->equal) {
	return __CFClassTable[__CFGenericTypeID(cf1)]->equal(cf1, cf2);
    }
    return FALSE;
}

CFHashCode CFHash(CFTypeRef cf) {
    if (NULL == cf) __CFAbort();
    CFTYPE_OBJC_FUNCDISPATCH0(CFHashCode, cf, "hash");
    __CFGenericAssertIsCF(cf);
    if (NULL != __CFClassTable[__CFGenericTypeID(cf)]->hash) {
	return __CFClassTable[__CFGenericTypeID(cf)]->hash(cf);
    }
    return (CFHashCode)cf;
}

CFStringRef CFCopyDescription(CFTypeRef cf) {
    if (NULL == cf) __CFAbort();
    CFTYPE_OBJC_FUNCDISPATCH0(CFStringRef, cf, "_copyDescription");
    __CFGenericAssertIsCF(cf);
    return __CFClassTable[__CFGenericTypeID(cf)]->copyDesc(cf);
}

CFAllocatorRef CFGetAllocator(CFTypeRef cf) {
    if (NULL == cf) __CFAbort();
    // ??? Need to do this better
    if (CFTYPE_IS_OBJC(cf)) return CFAllocatorGetDefault();
    __CFGenericAssertIsCF(cf);
    return __CFClassTable[__CFGenericTypeID(cf)]->getAllocator(cf);
}

CFRange __CFRangeMake(CFIndex loc, CFIndex len) {
    CFRange range;
    range.location = loc;
    range.length = len;
    return range;
}

const void *__CFTypeCollectionRetain(CFAllocatorRef allocator, const void *ptr) {
    return (const void *)CFRetain(ptr);
}

void __CFTypeCollectionRelease(CFAllocatorRef allocator, const void *ptr) {
    CFRelease(ptr);
}

CF_DEFINECONSTANTSTRING(_CFSpecialEmptyString, "")


#if defined(__MACOS8__)

extern void __CFStringCleanup(void);
extern void __CFTimeZoneCleanup(void);

/* This function is called when the library is unloaded by the CFM.
 * Place any cleanup routine calls here.
 */
OSErr __CFMacOS8Initialize(void* /*InitBlockPtr*/ initBlkPtr) {
    __CFInitialize();
    return noErr;
}

void __CFMacOS8Terminate(void) {
    __CFStringCleanup();
    __CFTimeZoneCleanup();
}

#endif


#if defined(__MACH__) && defined(__ppc__)

extern int _dyld_image_containing_address(void *addr);

/* See comments below */
void __CF_FAULT_CALLBACK(void **ptr) {
    if (NULL == *ptr) return;
    if (0x0 == ((unsigned long)(*ptr) & 0x1)) {
	int __known = _dyld_image_containing_address(*ptr);
	*ptr = (void *)((unsigned long)(*ptr) | (__known ? 0x1 : 0x3));	
    }
}

/*
Jump to callback function.  r2 is not saved and restored
in the jump-to-CFM case, since we assume that dyld code
never uses that register and that CF is dyld.

There are three states for (ptr & 0x3):
	0b00:	check not yet done
	0b01:	check done, dyld function pointer
	0b11:	check done, CFM tvector pointer
(but a NULL callback just stays NULL)

There may be up to 5 word-sized arguments. Floating point
arguments can be done, but count as two word arguments.
Return value can be integral or real.
*/

__asm__ (
".globl ___CF_INVOKE_CALLBACK\n"
"___CF_INVOKE_CALLBACK:\n"
	"rlwinm r12,r3,0,0,29\n"
	"andi. r0,r3,0x2\n"
	"or r3,r4,r4\n"
	"or r4,r5,r5\n"
	"or r5,r6,r6\n"
	"or r6,r7,r7\n"
	"or r7,r8,r8\n"
	"beq- Lcall\n"
//	"lwz r2,0x4(r12)\n"	// don't need this with 4 byte TVectors now
	"lwz r12,0x0(r12)\n"
"Lcall:  mtspr ctr,r12\n"
	"bctr\n");

#endif

