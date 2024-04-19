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
/*	CFUtilities.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFBase.h>
#include "CFVeryPrivate.h"
#include "CFInternal.h"
#include <math.h>
#if defined(__MACH__)
    #include <mach/mach.h>
    #include <pthread.h>
#elif defined(__WIN32__)
    #include <windows.h>
    #include <process.h>
#else // __MACOS8__
	#include <string.h>
	#include <stdlib.h>
#endif

#define LESS16(A, W)	do { if (A < (1ULL << (W))) LESS8(A, (W) - 8); LESS8(A, (W) + 8); } while (0)
#define LESS8(A, W)	do { if (A < (1ULL << (W))) LESS4(A, (W) - 4); LESS4(A, (W) + 4); } while (0)
#define LESS4(A, W)	do { if (A < (1ULL << (W))) LESS2(A, (W) - 2); LESS2(A, (W) + 2); } while (0)
#define LESS2(A, W)	do { if (A < (1ULL << (W))) LESS1(A, (W) - 1); LESS1(A, (W) + 1); } while (0)
#define LESS1(A, W)	do { if (A < (1ULL << (W))) return (W) - 1; return (W); } while (0)

UInt32 CFLog2(UInt64 x) {
    if (x < (1ULL << 32))
	LESS16(x, 16);
    LESS16(x, 48);
    return 0;
}

#if 0
// faster version for PPC
int Lg2d(unsigned x) {
// use PPC-specific instruction to count leading zeros
    int ret;
    if (0 == x) return 0;
    __asm__ volatile("cntlzw %0,%1" : "=r" (ret) : "r" (x));
    return 31 - ret;
}
#endif

#undef LESS1
#undef LESS2
#undef LESS4
#undef LESS8
#undef LESS16

void CFZeroMemory(void *ptr, CFIndex length) {
    memset(ptr, 0, length);
}

void CFSetMemory(void *ptr, UInt8 value, CFIndex length) {
    memset(ptr, value, length);
}

void CFMoveMemory(void *dstptr, const void *srcptr, CFIndex length) {
    memmove(dstptr, srcptr, length);
}

Boolean CFCompareMemory(const void *ptr, const void *ptr2, CFIndex length) {
    return memcmp(ptr, ptr2, length) == 0 ? TRUE : FALSE;
}

/* Comparator is passed the address of the values. */
/* Binary searches a sorted-increasing array of some type.
   Return value is either 1) the index of the element desired,
   if the target value exists in the list, 2) greater than or
   equal to count, if the element is greater than all the values
   in the list, or 3) the index of the element greater than the
   target value.

   For example, a search in the list of integers:
	2 3 5 7 11 13 17

   For...		Will Return...
	2		    0
   	5		    2
	23		    7
	1		    0
	9		    4

   For instance, if you just care about found/not found:
   index = CFBSearch(list, count, elem);
   if (count <= index || list[index] != elem) {
   	* Not found *
   } else {
   	* Found *
   }
   
   This isn't optimal yet.
*/
CFIndex CFBSearch(const void *element, CFIndex elementSize, const void *list, CFIndex count, CFComparatorFunction comparator, void *context) {
    SInt32 idx, lg;
    const char *ptr = (const char *)list;
    if (count < 4) {
	switch (count) {
	case 3: if (comparator(ptr + elementSize * 2, element, context) < 0) return 3;
	case 2: if (comparator(ptr + elementSize * 1, element, context) < 0) return 2;
	case 1: if (comparator(ptr + elementSize * 0, element, context) < 0) return 1;
	}
	return 0;
    }
    if (comparator(ptr + elementSize * (count - 1), element, context) < 0) return count;
    if (comparator(element, ptr + elementSize * 0, context) < 0) return 0;
    lg = CFLog2(count); /* This takes about 1/3rd of the time, discounting calls to comparator */
    idx = (comparator(ptr + elementSize * (-1 + (1 << lg)), element, context) < 0) ? count - (1 << lg) : -1;
    switch (--lg) {
    case 30: if (comparator(ptr + elementSize * (idx + (1 << 30)), element, context) < 0) idx += (1 << 30);
    case 29: if (comparator(ptr + elementSize * (idx + (1 << 29)), element, context) < 0) idx += (1 << 29);
    case 28: if (comparator(ptr + elementSize * (idx + (1 << 28)), element, context) < 0) idx += (1 << 28);
    case 27: if (comparator(ptr + elementSize * (idx + (1 << 27)), element, context) < 0) idx += (1 << 27);
    case 26: if (comparator(ptr + elementSize * (idx + (1 << 26)), element, context) < 0) idx += (1 << 26);
    case 25: if (comparator(ptr + elementSize * (idx + (1 << 25)), element, context) < 0) idx += (1 << 25);
    case 24: if (comparator(ptr + elementSize * (idx + (1 << 24)), element, context) < 0) idx += (1 << 24);
    case 23: if (comparator(ptr + elementSize * (idx + (1 << 23)), element, context) < 0) idx += (1 << 23);
    case 22: if (comparator(ptr + elementSize * (idx + (1 << 22)), element, context) < 0) idx += (1 << 22);
    case 21: if (comparator(ptr + elementSize * (idx + (1 << 21)), element, context) < 0) idx += (1 << 21);
    case 20: if (comparator(ptr + elementSize * (idx + (1 << 20)), element, context) < 0) idx += (1 << 20);
    case 19: if (comparator(ptr + elementSize * (idx + (1 << 19)), element, context) < 0) idx += (1 << 19);
    case 18: if (comparator(ptr + elementSize * (idx + (1 << 18)), element, context) < 0) idx += (1 << 18);
    case 17: if (comparator(ptr + elementSize * (idx + (1 << 17)), element, context) < 0) idx += (1 << 17);
    case 16: if (comparator(ptr + elementSize * (idx + (1 << 16)), element, context) < 0) idx += (1 << 16);
    case 15: if (comparator(ptr + elementSize * (idx + (1 << 15)), element, context) < 0) idx += (1 << 15);
    case 14: if (comparator(ptr + elementSize * (idx + (1 << 14)), element, context) < 0) idx += (1 << 14);
    case 13: if (comparator(ptr + elementSize * (idx + (1 << 13)), element, context) < 0) idx += (1 << 13);
    case 12: if (comparator(ptr + elementSize * (idx + (1 << 12)), element, context) < 0) idx += (1 << 12);
    case 11: if (comparator(ptr + elementSize * (idx + (1 << 11)), element, context) < 0) idx += (1 << 11);
    case 10: if (comparator(ptr + elementSize * (idx + (1 << 10)), element, context) < 0) idx += (1 << 10);
    case 9:  if (comparator(ptr + elementSize * (idx + (1 << 9)), element, context) < 0) idx += (1 << 9);
    case 8:  if (comparator(ptr + elementSize * (idx + (1 << 8)), element, context) < 0) idx += (1 << 8);
    case 7:  if (comparator(ptr + elementSize * (idx + (1 << 7)), element, context) < 0) idx += (1 << 7);
    case 6:  if (comparator(ptr + elementSize * (idx + (1 << 6)), element, context) < 0) idx += (1 << 6);
    case 5:  if (comparator(ptr + elementSize * (idx + (1 << 5)), element, context) < 0) idx += (1 << 5);
    case 4:  if (comparator(ptr + elementSize * (idx + (1 << 4)), element, context) < 0) idx += (1 << 4);
    case 3:  if (comparator(ptr + elementSize * (idx + (1 << 3)), element, context) < 0) idx += (1 << 3);
    case 2:  if (comparator(ptr + elementSize * (idx + (1 << 2)), element, context) < 0) idx += (1 << 2);
    case 1:  if (comparator(ptr + elementSize * (idx + (1 << 1)), element, context) < 0) idx += (1 << 1);
    case 0:  if (comparator(ptr + elementSize * (idx + (1 << 0)), element, context) < 0) idx += (1 << 0);
    }
    return ++idx;
}

#define	kQSortSwapBufferSize	128
/* stack allocated swap area for quick sort functions */

/* Scratch is temp memory for the use of this function, and must at least as large as the list */
/* Comparator is passed the address of the values. */
void CFMergeSortArray_old(void *list, CFIndex count, CFIndex elementSize, void *scratch, CFComparatorFunction comparator, void *context) {
    int delta01, delta02, delta12;
    UInt32 count1, count2;
    void *list1, *list2;
    UInt8 *T, stackBuffer[kQSortSwapBufferSize];
    SInt8 *listp = (SInt8 *)list;	/* so we can do offsets from list */

    T = (elementSize <= kQSortSwapBufferSize) ? stackBuffer : CFAllocatorAllocate(kCFAllocatorSystemDefault, elementSize, 0);
    switch (count) {
    case 0:
    case 1:
	break;
    case 2:
	delta01 = comparator(listp + elementSize * 0, listp + elementSize * 1, context);
	if (0 < delta01) {
	    CFMoveMemory(T, listp + elementSize * 0, elementSize);
	    CFMoveMemory(listp + elementSize * 0, listp + elementSize * 1, elementSize);
	    CFMoveMemory(listp + elementSize * 1, T, elementSize);
	}
	break;
    case 3:
	delta01 = comparator(listp + elementSize * 0, listp + elementSize * 1, context);
	delta12 = comparator(listp + elementSize * 1, listp + elementSize * 2, context);
	if (delta01 <= 0) {
	    if (0 < delta12) {	/* else, abc, already sorted */
		delta02 = comparator(listp + elementSize * 0, listp + elementSize * 2, context);
		if (delta02 <= 0) {	/* acb */
		    CFMoveMemory(T, listp + elementSize * 1, elementSize);
		    CFMoveMemory(listp + elementSize * 1, listp + elementSize * 2, elementSize);
		    CFMoveMemory(listp + elementSize * 2, T, elementSize);
		} else {		/* cab */
		    CFMoveMemory(T, listp + elementSize * 0, elementSize);
		    CFMoveMemory(listp + elementSize * 0, listp + elementSize * 1, elementSize);
		    CFMoveMemory(listp + elementSize * 1, listp + elementSize * 2, elementSize);
		    CFMoveMemory(listp + elementSize * 2, T, elementSize);
		}
	    }
	} else {
	    if (0 < delta12) {		/* cba */
		CFMoveMemory(T, listp + elementSize * 0, elementSize);
		CFMoveMemory(listp + elementSize * 0, listp + elementSize * 2, elementSize);
		CFMoveMemory(listp + elementSize * 2, T, elementSize);
	    } else {
		delta02 = comparator(listp + elementSize * 0, listp + elementSize * 2, context);
		if (delta02 <= 0) {	/* bac */
		    CFMoveMemory(T, listp + elementSize * 0, elementSize);
		    CFMoveMemory(listp + elementSize * 0, listp + elementSize * 1, elementSize);
		    CFMoveMemory(listp + elementSize * 1, T, elementSize);
		} else {		/* bca */
		    CFMoveMemory(T, listp + elementSize * 0, elementSize);
		    CFMoveMemory(listp + elementSize * 0, listp + elementSize * 2, elementSize);
		    CFMoveMemory(listp + elementSize * 2, listp + elementSize * 1, elementSize);
		    CFMoveMemory(listp + elementSize * 1, T, elementSize);
		}
	    }
	}
	break;
    default:
	count1 = count >> 1;
	list1 = list;
	count2 = count - count1;
	list2 = listp + count1;
	CFMergeSortArray_old(list1, count1, elementSize, scratch, comparator, context);
	CFMergeSortArray_old(list2, count2, elementSize, scratch, comparator, context);
	if (0 < comparator((SInt8*)list1 + elementSize * (count1 - 1), (SInt8*)list2 + elementSize * 0, context)) {
	    while (0 < count1 && 0 < count2) {
		if (comparator(list1, list2, context) <= 0) {
		    CFMoveMemory(scratch, list1, elementSize);
		    scratch = (SInt8*)scratch + 1;
		    list1 = (SInt8*)list1 + 1;
		    count1--;
		} else {
		    CFMoveMemory(scratch, list2, elementSize);
		    scratch = (SInt8*)scratch + 1;
		    list2 = (SInt8*)list2 + 1;
		    count2--;
		}
	    }
	    if (0 < count1)
		CFMoveMemory(scratch, list1, count1 * elementSize);
	    if (0 < count2)
		CFMoveMemory(scratch, list2, count2 * elementSize);
	    CFMoveMemory(list, scratch, count * elementSize);
	}
	break;
    }
    if (T != stackBuffer) CFAllocatorDeallocate(kCFAllocatorSystemDefault, T);
}
	
#define ELF_STEP(B) T1 = (H << 4) + B; T2 = T1 & 0xF0000000; if (T2) T1 ^= (T2 >> 24); T1 &= (~T2); H = T1;

CFHashCode CFHashBytes(UInt8 *bytes, CFIndex length) {
    /* The ELF hash algorithm, used in the ELF object file format */
    UInt32 H = 0, T1, T2;
    SInt32 rem = length;
    while (3 < rem) {
	ELF_STEP(bytes[length - rem]);
	ELF_STEP(bytes[length - rem + 1]);
	ELF_STEP(bytes[length - rem + 2]);
	ELF_STEP(bytes[length - rem + 3]);
	rem -= 4;
    }
    switch (rem) {
    case 3:  ELF_STEP(bytes[length - 3]);
    case 2:  ELF_STEP(bytes[length - 2]);
    case 1:  ELF_STEP(bytes[length - 1]);
    case 0:  ;
    }
    return H;
}

#undef ELF_STEP

#if defined(__MACOS8__)

void __CFSpinLock(volatile UInt32 *p) {
#pragma unused (p)
}
void __CFSpinUnlock(volatile UInt32 *p) {
#pragma unused (p)
}

#else

// void __CFSpinLock(volatile UInt32 *p);
__asm__ (
".globl ___CFSpinLock\n"
"___CFSpinLock:\n"
#if defined(__ppc__)
	"or r11,r3,r3\n"
	"b 1f\n"
	"2: addi r3,0,0\n"
	"li r0,-59\n"	// call swtch_pri(0)
	"sc\n"		// really, really bad to do it this way!
	"1: addi r4,0,1\n"
	"lwarx r5,0,r11\n"
        "stwcx. r4,0,r11\n"
        "cmpwi cr1,r5,0\n"
	"crand 2,2,6\n"
        "bc 4,2,2b\n"	// if either EQ bit was 0
        "isync\n"
	"blr\n"
#elif defined(__i386__)
	"pushl %ebp\n"
	"movl %esp,%ebp\n"
	"movl 8(%ebp),%ecx\n"
        "movl $1,%eax\n"
	"1: lock\n"
	"xchgl %eax,(%ecx)\n"
        "testl %eax,%eax\n"
        "jnz 1b\n"
	"leave\n"
	"ret\n"
#else
#error __CFSpinLock not implemented on this architecture
#endif
);

// void __CFSpinUnlock(volatile UInt32 *p);
__asm__ (
".globl ___CFSpinUnlock\n"
"___CFSpinUnlock:\n"
#if defined(__ppc__)
	"sync\n"
	"addi r4,0,0\n"
	"stw r4,0(r3)\n"
	"blr\n"
#elif defined(__i386__)
	"pushl %ebp\n"
	"movl %esp,%ebp\n"
	"movl 8(%ebp),%ecx\n"
	"movl $0,%eax\n"
	"lock\n"
	"xchgl %eax,(%ecx)\n"
	"leave\n"
	"ret\n"
#else
#error __CFSpinUnlock not implemented on this architecture
#endif
);

#if defined(__WIN32__)
struct _args {
    void *func;
    void *arg;
    HANDLE handle;
};
static unsigned __CFWinThreadFunc(void *arg) {
    struct _args *args = arg; 
    ((void (*)(void *))args->func)(args->arg);
    CloseHandle(args->handle);
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, arg);
    _endthreadex(0);
    return 0; 
}
#endif

void *__CFStartSimpleThread(void *func, void *arg) {
#if defined(__MACH__)
    pthread_attr_t attr;
    pthread_t tid;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, 96 * 1024);	// 96K stack for our internal threads is sufficient
    pthread_create(&tid, &attr, func, arg);
    pthread_attr_destroy(&attr);
//warning CF: we dont actually know that a pthread_t is the same size as void *
    return (void *)tid;
#else
    unsigned tid;
    struct _args *args = CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(struct _args), 0);
    HANDLE handle;
    args->func = func;
    args->arg = arg;
    /* The thread is created suspended, because otherwise there would be a race between the assignment below of the handle field, and it's possible use in the thread func above. */
    args->handle = (HANDLE)_beginthreadex(NULL, 0, __CFWinThreadFunc, args, CREATE_SUSPENDED, &tid);
    handle = args->handle;
    ResumeThread(handle);
    return handle;
#endif
}

#endif

void __CFAbort(void)
{
#if defined (__MACOS8__) && defined(DEBUG)
    DebugStr("\pCoreFoundation abort");
#else
    abort();
#endif
}

void __CFAssertAbort(void)
{
#if defined (__MACOS8__) && defined(DEBUG)
    DebugStr("\pCoreFoundation assertion failure");
#else
    abort();
#endif
}


CFStringRef _CFCreateLimitedUniqueString() {
    /* this unique string is only unique to the current host during the current boot */
    UInt64 tsr = __CFReadTSR();
    UInt32 tsrh = (tsr >> 32), tsrl = (tsr & 0xFFFFFFFFLL);
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("CFUniqueString-%lu%lu$"), tsrh, tsrl);
}


