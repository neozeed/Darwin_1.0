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
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */

#ifndef _OS_OSBYTEORDERI386_H
#define _OS_OSBYTEORDERI386_H

#include <libkern/OSBase.h>

/* Functions for byte reversed loads. */

OS_INLINE
UInt16
OSReadSwapInt16(
    volatile void               * base,
    volatile UInt                 offset
)
{
    UInt16 result;

    result = *(UInt16 *)((UInt8 *)base + offset);
    __asm__ volatile("xchgb %b0,%h0"
                     : "=q" (result)
                     : "0"  (result));
    return result;
}

OS_INLINE
UInt32
OSReadSwapInt32(
    volatile void               * base,
    volatile UInt                 offset
)
{
    UInt32 result;

    result = *(UInt32 *)((UInt8 *)base + offset);
    __asm__ volatile("bswap %0"
                     : "=r" (result)
                     : "0"  (result));
    return result;
}

OS_INLINE
UInt64
OSReadSwapInt64(
    volatile void               * base,
    volatile UInt                 offset
)
{
    UInt64 * inp;
    union ullc {
        UInt64     ull;
        UInt       ul[2];
    } outv;

    inp = (UInt64 *)base;
    outv.ul[0] = OSReadSwapInt32(inp, offset + 4);
    outv.ul[1] = OSReadSwapInt32(inp, offset);
    return outv.ull;
}

OS_INLINE
UInt
OSReadSwapInt(
    volatile void               * base,
    volatile UInt                 offset
)
{
    UInt result;

    result = *(UInt *)((UInt8 *)base + offset);
    __asm__ volatile("bswap %0"
                     : "=r" (result)
                     : "0"  (result));
    return result;
}

/* Functions for byte reversed stores. */

OS_INLINE
void
OSWriteSwapInt16(
    volatile void               * base,
    volatile UInt                 offset,
    volatile UInt16               data
)
{
    __asm__ volatile("xchgb %b0,%h0"
                     : "=q" (data)
                     : "0"  (data));
    *(UInt16 *)((UInt8 *)base + offset) = data;
}

OS_INLINE
void
OSWriteSwapInt32(
    volatile void               * base,
    volatile UInt                 offset,
    volatile UInt32               data
)
{
    __asm__ volatile("bswap %0"
                     : "=r" (data)
                     : "0"  (data));
    *(UInt32 *)((UInt8 *)base + offset) = data;
}

OS_INLINE
void
OSWriteSwapInt64(
    volatile void               * base,
    volatile UInt                 offset,
    volatile UInt64               data
)
{
    UInt64 * outp;
    union ullc {
        UInt64     ull;
        UInt       ul[2];
    } *inp;

    outp = (UInt64 *)base;
    inp  = (union ullc *)&data;
    OSWriteSwapInt32(outp, offset, inp->ul[1]);
    OSWriteSwapInt32(outp, offset + 4, inp->ul[0]);
}

OS_INLINE
void
OSWriteSwapInt(
    volatile void               * base,
    volatile UInt                 offset,
    volatile UInt                 data
)
{
    __asm__ volatile("bswap %0"
                     : "=r" (data)
                     : "0"  (data));
    *(UInt *)((UInt8 *)base + offset) = data;
}

/* Generic byte swapping functions. */

OS_INLINE
UInt16
OSSwapInt16(
    UInt16                        data
)
{
    UInt16 temp = data;
    return OSReadSwapInt16(&temp, 0);
}

OS_INLINE
UInt32
OSSwapInt32(
    UInt32                        data
)
{
    UInt32 temp = data;
    return OSReadSwapInt32(&temp, 0);
}

OS_INLINE
UInt64
OSSwapInt64(
    UInt64                        data
)
{
    UInt64 temp = data;
    return OSReadSwapInt64(&temp, 0);
}

OS_INLINE
UInt
OSSwapInt(
    UInt                          data
)
{
    UInt temp = data;
    return OSReadSwapInt(&temp, 0);
}

#endif /* ! _OS_OSBYTEORDERI386_H */
