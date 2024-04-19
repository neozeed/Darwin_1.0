#ifndef __IOKIT_IOKITINTERNAL_H
#define __IOKIT_IOKITINTERNAL_H

#define OBJC_MAPPINGS 1
#define IO_HAS_ISA

#include <CoreFoundation/CFBase.h>

#define thread_errno() errno

#if OBJC_MAPPINGS
struct objc_class { long __fields__[10]; };
#endif

#define		__kCFStringTypeID_CONST	2	/* This is special for CFString */

struct __CFTypeClass {
    const char *name;
    Boolean (*equal)(CFTypeRef cf1, CFTypeRef cf2);
    CFHashCode (*hash)(CFTypeRef cf);
    CFStringRef (*copyDesc)(CFTypeRef cf);
    CFAllocatorRef (*getAllocator)(CFTypeRef cf);
    void (*dealloc)(CFTypeRef cf);
#if OBJC_MAPPINGS
    struct objc_class objcClass;
#endif
};

#define CFMaxPathSize ((CFIndex)1026)
#define CFMaxPathLength ((CFIndex)1024)

#ifdef IO_HAS_ISA
extern struct __CFTypeClass __CFStringClass;
#define CF_DEFINECONSTANTSTRING(name, cStr) static struct {void *isa; UInt32 info; const char *ptr; UInt32 moreInfo;} __ ## name = {&(__CFStringClass.objcClass), 0x88 + (__kCFStringTypeID_CONST << 8), cStr "", 0x00ffffff}; const CFStringRef name = (CFStringRef )&__ ## name;
#else
#define CF_DEFINECONSTANTSTRING(name, cStr) static struct {UInt32 info; const char *ptr; UInt32 moreInfo;} __ ## name = {0x88 + (__kCFStringTypeID_CONST << 8), cStr "", 0x00ffffff}; const CFStringRef name = (CFStringRef )&__ ## name;
#endif


#endif /* __IOKIT_IOKITINTERNAL_H */
