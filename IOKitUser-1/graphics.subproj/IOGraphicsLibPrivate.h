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
typedef void (*BlitEngineCloseProc)(void * chipRef);
typedef void (*BlitEngineResetProc)(void * chipRef);
typedef UInt32 (*BlitEngineGetStateProc)(void * chipRef);
typedef kern_return_t (*BlitCopyProc)(void * chipRef,
			UInt32 srcLeft, UInt32 srcTop,
			UInt32 width, UInt32 height, 
			UInt32 destLeft, UInt32 destTop,
			IOOptionBits options );
typedef kern_return_t (*BlitFillProc)(void * chipRef,
			UInt32 x, UInt32 y,  UInt32 w,  UInt32 h,
			UInt32 color );
typedef kern_return_t (*BlitInvertProc)(void * chipRef,
			UInt32 x, UInt32 y,  UInt32 w,  UInt32 h );
typedef kern_return_t (*BlitSynchronizeProc)(void * chipRef,
			UInt32 x, UInt32 y,  UInt32 w,  UInt32 h,
			UInt32 options );

typedef kern_return_t (*BlitBeamPositionProc)(void * chipRef,
			UInt32 options, SInt32 * position );

typedef kern_return_t (*BlitSetupFIFOBurstProc)(void * chipRef,
			UInt32 x, UInt32 y,  UInt32 w,  UInt32 h,
			IOOptionBits options, void ** burstRef );

#if 0
typedef kern_return_t (*BlitSetupBlitBufferProc)(void * chipRef,
			IOOptionBits options,
			void ** buffer, IOByteCount ** size );
typedef kern_return_t (*BlitBufferProc)(void * chipRef,
			UInt32 x, UInt32 y,  UInt32 w,  UInt32 h,
			IOOptionBits options, void ** burstRef );

    BlitSetupBlitBufferProc	setupBlitBuffer;
    BlitBufferProc		blitBuffer;

#endif

typedef kern_return_t (*BlitMemoryCopyProc)(void * chipRef,
			UInt32 destLeft, UInt32 destTop,
			UInt32 width, UInt32 height, 
			UInt32 srcByteOffset, UInt32 srcRowBytes,
			SInt32 * token );

typedef kern_return_t (*BlitMemoryCopyMonoProc)(void * chipRef,
			UInt32 destLeft, UInt32 destTop,
			UInt32 width, UInt32 height, 
			UInt32 srcByteOffset, UInt32 srcRowBytes,
			UInt32 foreColor, UInt32 backColor,
			SInt32 * token );

typedef kern_return_t (*BlitMemoryOrMonoProc)(void * chipRef,
			UInt32 destLeft, UInt32 destTop,
			UInt32 width, UInt32 height, 
			UInt32 srcByteOffset, UInt32 srcRowBytes,
			UInt32 foreColor,
			SInt32 * token );

typedef kern_return_t (*BlitCompleteProc)(void * chipRef, SInt32 token );

struct BlitterProcs {
    BlitEngineCloseProc		close;
    BlitEngineResetProc		reset;
    BlitEngineGetStateProc	getState;
    BlitCopyProc		copy;
    BlitFillProc		fill;
    BlitInvertProc		invert;
    BlitSynchronizeProc		synchronize;
    BlitBeamPositionProc	beamPosition;
    BlitCompleteProc		complete;
    BlitSetupFIFOBurstProc	setupFIFOBurst;
    BlitMemoryCopyProc		memoryCopy;
    BlitMemoryCopyMonoProc	memoryCopyMono;
    BlitMemoryOrMonoProc	memoryOrMono;
};
typedef struct BlitterProcs BlitterProcs;

enum {
    kBlitEngineIdle	= 0x00000001
};

enum {
    kBlitEngineBeamSync	= 0x00000001
};
