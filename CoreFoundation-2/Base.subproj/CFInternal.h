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
/*	CFInternal.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

/*
        NOT TO BE USED OUTSIDE CF!
*/

#if !defined(__COREFOUNDATION_CFINTERNAL__)
#define __COREFOUNDATION_CFINTERNAL__ 1

#if !defined(CF_BUILDING_CF)
#error The header file CFInternal.h is for the exclusive use of CoreFoundation. No other project should include it.
#endif


#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFURL.h>




#define CF_CLASS_FROM_TYPE(type) (NULL)

#define CF_OBJC_FUNCDISPATCH0(cftype, rettype, obj, selector) 
#define CF_OBJC_FUNCDISPATCH1(cftype, rettype, obj, selector, a1) 
#define CF_OBJC_FUNCDISPATCH2(cftype, rettype, obj, selector, a1, a2) 
#define CF_OBJC_FUNCDISPATCH3(cftype, rettype, obj, selector, a1, a2, a3) 
#define CF_OBJC_FUNCDISPATCH4(cftype, rettype, obj, selector, a1, a2, a3, a4) 

#define CF_OBJC_UNIMPLEMENTEDDISPATCH(cftype, obj, func)

#define CF_IS_OBJC(cftype, obj) (FALSE)



extern CFTypeID __kCFNotAtTypeTypeID;
extern CFTypeID __kCFTypeTypeID;
#define		__kCFStringTypeID_CONST	2	/* This is special for CFString */
extern CFTypeID __kCFStringTypeID;
#define		__CFStringIsa()		(CF_CLASS_FROM_TYPE(__kCFStringTypeID))
extern CFTypeID __kCFAllocatorTypeID;
extern CFTypeID __kCFArrayTypeID;
#define		__CFArrayIsa()		(CF_CLASS_FROM_TYPE(__kCFArrayTypeID))
extern CFTypeID __kCFBagTypeID;
extern CFTypeID __kCFBooleanTypeID;
#define		__CFBooleanIsa()	(CF_CLASS_FROM_TYPE(__kCFBooleanTypeID))
extern CFTypeID __kCFCharacterSetTypeID;
#define		__CFCharacterSetIsa()  	(CF_CLASS_FROM_TYPE(__kCFCharacterSetTypeID))
extern CFTypeID __kCFDataTypeID;
#define		__CFDataIsa()		(CF_CLASS_FROM_TYPE(__kCFDataTypeID))
extern CFTypeID __kCFDateTypeID;
#define		__CFDateIsa()		(CF_CLASS_FROM_TYPE(__kCFDateTypeID))
extern CFTypeID __kCFDictionaryTypeID;
#define		__CFDictionaryIsa()	(CF_CLASS_FROM_TYPE(__kCFDictionaryTypeID))
extern CFTypeID __kCFNumberTypeID;
#define		__CFNumberIsa()		(CF_CLASS_FROM_TYPE(__kCFNumberTypeID))
extern CFTypeID __kCFSetTypeID;
#define		__CFSetIsa()		(CF_CLASS_FROM_TYPE(__kCFSetTypeID))
extern CFTypeID __kCFStorageTypeID;
extern CFTypeID __kCFTimeZoneTypeID;
extern CFTypeID __kCFTreeTypeID;
extern CFTypeID __kCFURLTypeID;
#define		__CFURLIsa()		(CF_CLASS_FROM_TYPE(__kCFURLTypeID))
extern CFTypeID __kCFXMLParserTypeID;

#if defined(__MACH__)
extern CFTypeID __kCFMachPortTypeID;
#define		__CFMachPortIsa()	(CF_CLASS_FROM_TYPE(__kCFMachPortTypeID))
#endif __MACH__
#if (defined(__MACH__) || defined(__WIN32__))
extern CFTypeID __kCFMessagePortTypeID;
extern CFTypeID __kCFRunLoopTypeID;
extern CFTypeID __kCFRunLoopObserverTypeID;
extern CFTypeID __kCFRunLoopSourceTypeID;
extern CFTypeID __kCFRunLoopTimerTypeID;
extern CFTypeID __kCFSocketTypeID;
#define		__CFRunLoopTimerIsa()	(CF_CLASS_FROM_TYPE(__kCFRunLoopTimerTypeID))
#endif (__MACH__ || __WIN32__)

struct __CFTypeClass {
    const char *name;
    Boolean (*equal)(CFTypeRef cf1, CFTypeRef cf2);
    CFHashCode (*hash)(CFTypeRef cf);
    CFStringRef (*copyDesc)(CFTypeRef cf);
    CFAllocatorRef (*getAllocator)(CFTypeRef cf);
    void (*dealloc)(CFTypeRef cf);
};

extern struct __CFTypeClass *__CFClassTable[];

#if defined(DEBUG)
    #define __CFAssert(cond, prio, desc, a1, a2, a3, a4, a5)	\
	do {			\
	    if (!(cond)) {	\
		CFLog(prio, CFSTR(desc), a1, a2, a3, a4, a5); \
		__CFAssertAbort();	\
	    }			\
	} while (0)
#else
    #define __CFAssert(cond, prio, desc, a1, a2, a3, a4, a5)	\
	do {} while (0)
#endif

#define CFAssert(condition, priority, description)			\
    __CFAssert((condition), (priority), description, 0, 0, 0, 0, 0)
#define CFAssert1(condition, priority, description, a1)			\
    __CFAssert((condition), (priority), description, (a1), 0, 0, 0, 0)
#define CFAssert2(condition, priority, description, a1, a2)		\
    __CFAssert((condition), (priority), description, (a1), (a2), 0, 0, 0)
#define CFAssert3(condition, priority, description, a1, a2, a3)		\
    __CFAssert((condition), (priority), description, (a1), (a2), (a3), 0, 0)
#define CFAssert4(condition, priority, description, a1, a2, a3, a4)	\
    __CFAssert((condition), (priority), description, (a1), (a2), (a3), (a4), 0)

#define __kCFLogAssertion	15

#if defined(DEBUG)
extern void __CFGenericValidateType_(CFTypeRef cf, CFTypeID type, char *func);
#define __CFGenericValidateType(cf, type) __CFGenericValidateType_(cf, type, __PRETTY_FUNCTION__)
#else
#define __CFGenericValidateType(cf, type) 
#endif

#define __CFGenericValidateMutabilityFlags(flags) \
    CFAssert2(__CFMutableVarietyFromFlags(flags) != 0x2, __kCFLogAssertion, "%s(): flags 0x%x do not correctly specify the mutable variety", __PRETTY_FUNCTION__, flags);

/* Bit manipulation macros */
/* Bits are numbered from 31 on left to 0 on right */
/* May or may not work if you use them on bitfields in types other than UInt32, bitfields the full width of a UInt32, or anything else for which they were not designed. */
#define __CFBitfieldMask(N1, N2)	((((UInt32)~0UL) << (31UL - (N1) + (N2))) >> (31UL - N1))
#define __CFBitfieldValue(V, N1, N2)	(((V) & __CFBitfieldMask(N1, N2)) >> (N2))
#define __CFBitfieldSetValue(V, N1, N2, X)	((V) = ((V) & ~__CFBitfieldMask(N1, N2)) | (((X) << (N2)) & __CFBitfieldMask(N1, N2)))
#define __CFBitfieldMaxValue(N1, N2)	__CFBitfieldValue(0xFFFFFFFFUL, (N1), (N2))

#define __CFBitIsSet(V, N)  (((V) & (1UL << (N))) != 0)
#define __CFBitSet(V, N)  ((V) |= (1UL << (N)))
#define __CFBitClear(V, N)  ((V) &= ~(1UL << (N)))


/* All CF "objects" start with this structure */
typedef struct __CFBase {
    void *_isa;
    UInt32 _info;
} CFRuntimeBase;

/* bits 31-16 in the base _info are retain count */
/* bits 15-8 in the base _info are type */
/* bits 7-0 are reserved for the object's use */

/* Flag bits for some types */
enum {
    kCFImmutable = 0x0,		/* unchangable and fixed capacity; default */
    kCFMutable = 0x1,		/* changeable and variable capacity */
    kCFFixedMutable = 0x3	/* changeable and fixed capacity */
};

CF_INLINE void __CFGenericInitBase(void *cf, void *isa, CFTypeID type) {
    ((CFRuntimeBase *)cf)->_info = 0;
    __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_info, 15, 8, type);
}

CF_INLINE CFTypeID __CFGenericTypeID(const void *cf) {
    return __CFBitfieldValue(((const CFRuntimeBase *)cf)->_info, 15, 8);
}

/* Bits 3-2 are used for mutability variation in some types */

CF_INLINE UInt32 __CFMutableVariety(const void *cf) {
    return __CFBitfieldValue(((const CFRuntimeBase *)cf)->_info, 3, 2);
}

CF_INLINE void __CFSetMutableVariety(void *cf, UInt32 v) {
    __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_info, 3, 2, v);
}

CF_INLINE UInt32 __CFMutableVarietyFromFlags(UInt32 flags) {
    return __CFBitfieldValue(flags, 1, 0);
}

typedef struct ___CFThreadSpecificData {
    void *_unused1;
    void *_allocator;
    void *_runLoop;
// If you add things to this struct, add cleanup to __CFFinalizeThreadData()
} __CFThreadSpecificData;

extern __CFThreadSpecificData *__CFGetThreadSpecificData(void);

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
    #define __CFMin(A,B) ({__typeof__(A) __a = (A); __typeof__(B) __b = (B); __a < __b ? __a : __b; })
    #define __CFMax(A,B) ({__typeof__(A) __a = (A); __typeof__(B) __b = (B); __a < __b ? __b : __a; })
#else /* __GNUC__ */
    #define __CFMin(A,B) ((A) < (B) ? (A) : (B))
    #define __CFMax(A,B) ((A) > (B) ? (A) : (B))
#endif /* __GNUC__ */

/* Secret CFAllocator hint bits */
#define __kCFAllocatorTempMemory	0x2
#define __kCFAllocatorNoPointers	0x10
#define __kCFAllocatorDoNotRecordEvent	0x100

CF_EXPORT CFAllocatorRef _CFTemporaryMemoryAllocator(void);

extern void	CFZeroMemory(void *ptr, CFIndex length);
extern void	CFSetMemory(void *ptr, UInt8 value, CFIndex length);
extern void	CFMoveMemory(void *dstptr, const void *srcptr, CFIndex length);
extern Boolean	CFCompareMemory(const void *ptr, const void *ptr2, CFIndex length);
extern UInt32	CFLog2(UInt64 x);

/* Buffer size for file pathname */
#if defined(__WIN32__)
    #define CFMaxPathSize ((CFIndex)262)
    #define CFMaxPathLength ((CFIndex)260)
#elif defined(__MACOS8__)
    #define CFMaxPathSize ((CFIndex)256)
    #define CFMaxPathLength ((CFIndex)255)
#else
    #define CFMaxPathSize ((CFIndex)1026)
    #define CFMaxPathLength ((CFIndex)1024)
#endif

#define __CFRecordAllocationEvent(a, b, c, d, e)
#define __CFSetLastAllocationEventName(a, b)

/* Comparators are passed the address of the values; this is somewhat different than CFComparatorFunction is used in public API usually. */
CF_EXPORT CFIndex	CFBSearch(const void *element, CFIndex elementSize, const void *list, CFIndex count, CFComparatorFunction comparator, void *context);
CF_EXPORT void		CFQSortArray(void *list, CFIndex count, CFIndex elementSize, CFComparatorFunction comparator, void *context);
CF_EXPORT void		CFMergeSortArray(void *list, CFIndex count, CFIndex elementSize, void *scratch, CFComparatorFunction comparator, void *context);

CF_EXPORT void		CFLog(int p, CFStringRef str, ...);

CF_EXPORT CFHashCode	CFHashBytes(UInt8 *bytes, CFIndex length);

CF_EXPORT CFStringEncoding CFStringFileSystemEncoding(void);

extern const void *__CFTypeCollectionRetain(CFAllocatorRef allocator, const void *ptr);
extern void __CFTypeCollectionRelease(CFAllocatorRef allocator, const void *ptr);

extern void __CFSpinLock(volatile UInt32 *lock);
extern void __CFSpinUnlock(volatile UInt32 *lock);

extern void __CFAbort(void);
extern void __CFAssertAbort(void);


#if defined(__svr4__) || defined(__hpux__)
#include <errno.h>
#elif defined(__WIN32__)
#elif defined(__MACOS8__)
#include <errno.h>
#elif defined(__MACH__)
#include <sys/errno.h>
#endif

#define thread_errno() errno
#define thread_set_errno(V) do {errno = (V);} while (0)

extern void *__CFStartSimpleThread(void *func, void *arg);


/* CF_DEFINECONSTANTSTRING is for CF internal use only
*/
#if CF_HAS_ISA
extern struct __CFTypeClass __CFStringClass;
#define CF_DEFINECONSTANTSTRING(name, cStr) static struct {void *isa; UInt32 info; const char *ptr; UInt32 moreInfo;} __ ## name = {&(__CFStringClass.objcClass), 0x88 + (__kCFStringTypeID_CONST << 8), cStr "", 0x00ffffff}; const CFStringRef name = (CFStringRef )&__ ## name;
#else
#define CF_DEFINECONSTANTSTRING(name, cStr) static struct {UInt32 info; const char *ptr; UInt32 moreInfo;} __ ## name = {0x88 + (__kCFStringTypeID_CONST << 8), cStr "", 0x00ffffff}; const CFStringRef name = (CFStringRef )&__ ## name;
#endif


/* See comments in CFBase.c
*/
#if defined(__ppc__) && defined(__MACH__)
extern void __CF_FAULT_CALLBACK(void **ptr);
/* WARNING: Do NOT make the following declaration a prototype!!! */
extern int __CF_INVOKE_CALLBACK();

#define FAULT_CALLBACK(V) __CF_FAULT_CALLBACK(V)
#define INVOKE_CALLBACK1(P, A) __CF_INVOKE_CALLBACK(P, A)
#define INVOKE_CALLBACK2(P, A, B) __CF_INVOKE_CALLBACK(P, A, B)
#define INVOKE_CALLBACK3(P, A, B, C) __CF_INVOKE_CALLBACK(P, A, B, C)
#define INVOKE_CALLBACK4(P, A, B, C, D) __CF_INVOKE_CALLBACK(P, A, B, C, D)
#define INVOKE_CALLBACK5(P, A, B, C, D, E) __CF_INVOKE_CALLBACK(P, A, B, C, D, E)
#else
#define FAULT_CALLBACK(V)
#define INVOKE_CALLBACK1(P, A) (*(void *(*)())(P))(A)
#define INVOKE_CALLBACK2(P, A, B) (*(void *(*)())(P))(A, B)
#define INVOKE_CALLBACK3(P, A, B, C) (*(void *(*)())(P))(A, B, C)
#define INVOKE_CALLBACK4(P, A, B, C, D) (*(void *(*)())(P))(A, B, C, D)
#define INVOKE_CALLBACK5(P, A, B, C, D, E) (*(void *(*)())(P))(A, B, C, D, E)
#endif


#endif /* ! __COREFOUNDATION_CFINTERNAL__ */

