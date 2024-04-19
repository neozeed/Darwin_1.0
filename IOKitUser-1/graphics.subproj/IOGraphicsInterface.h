/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOGRAPHICSINTERFACE_H
#define _IOKIT_IOGRAPHICSINTERFACE_H

#ifndef NO_CFPLUGIN
#include <IOKit/IOCFPlugIn.h>
#endif /* ! NO_CFPLUGIN */

#define kIOGraphicsAcceleratorTypeID			\
	(CFUUIDGetConstantUUIDWithBytes(NULL,		\
                                0xAC, 0xCF, 0x00, 0x00,	\
                                0x00, 0x00,		\
                                0x00, 0x00,		\
                                0x00, 0x00,		\
                                0x00, 0x0a, 0x27, 0x89, 0x90, 0x4e))

// IOGraphicsAcceleratorType objects must implement the
// IOGraphicsAcceleratorInterface

#define kIOGraphicsAcceleratorInterfaceID		\
	(CFUUIDGetConstantUUIDWithBytes(NULL, 		\
                                0x67, 0x66, 0xE9, 0x4A,	\
                                0x00, 0x00,		\
                                0x00, 0x00,		\
                                0x00, 0x00,		\
                                0x00, 0x0a, 0x27, 0x89, 0x90, 0x4e))

#define kCurrentGraphicsInterfaceVersion	0
#define kCurrentGraphicsInterfaceRevision	2

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef SInt32		IOBlitCompletionToken;

typedef UInt32		IOBlitType;
enum {
    kIOBlitTypeVerbMask			= 0x000000ff,
    kIOBlitTypeRects			= 0,
    kIOBlitTypeCopyRects,
    kIOBlitTypeLines,
    kIOBlitTypeScanlines,

    kIOBlitTypeMonoExpand		= 0x00000100,
    kIOBlitTypeColorSpaceConvert	= 0x00000200,
    kIOBlitTypeScale			= 0x00000400,
    kIOBlitTypeSourceKeyColorStuffMask	= 0x00003000,
    kIOBlitTypeDestKeyColorStuffMask	= 0x0000c000,

    kIOBlitTypeOperationMask		= 0x0fff0000,
    kIOBlitCopyOperation		= 0x00000000
};

typedef UInt32		IOBlitSourceDestType;
enum {
    kIOBlitSourceFramebuffer		= 0x00001000,
    kIOBlitSourceMemory			= 0x00002000,
    kIOBlitSourceOOLMemory		= 0x00003000,
    kIOBlitSourcePattern		= 0x00004000,
    kIOBlitSourceOOLPattern		= 0x00005000,
    kIOBlitSourceSolid			= 0x00006000,
    kIOBlitSourceType			= 0x7ffff000,
    kIOBlitSourceIsSame			= 0x80000000,

    kIOBlitDestFramebuffer		= 0x00000001,
    kIOBlitDestMemory			= 0x00000002,
    kIOBlitDestOOLMemory		= 0x00000003,
    kIOBlitDestType			= 0x000007ff,
    kIOBlitDestIsSame			= 0x00000800,

    kIOBlitSourceDestSame		= kIOBlitDestIsSame | kIOBlitSourceIsSame
};

typedef struct IOBlitOperationStruct {
    UInt32		color0;
    UInt32		color1;
    SInt32		offsetX;
    SInt32		offsetY;
    UInt32		sourceKeyColor;
    UInt32		destKeyColor;
    UInt32		specific[16];
} IOBlitOperation;

typedef struct IOBlitRectangleStruct {
    SInt32		x;
    SInt32		y;
    SInt32		width;
    SInt32		height;
} IOBlitRectangle;

typedef struct IOBlitRectanglesStruct {
    IOBlitOperation	operation;
    IOItemCount		count;
    IOBlitRectangle	rects[1];
} IOBlitRectangles;

typedef struct IOBlitCopyRectangleStruct {
    SInt32		sourceX;
    SInt32		sourceY;
    SInt32		x;
    SInt32		y;
    SInt32		width;
    SInt32		height;
} IOBlitCopyRectangle;

typedef struct IOBlitCopyRectanglesStruct {
    IOBlitOperation	operation;
    IOItemCount		count;
    IOBlitCopyRectangle	rects[1];
} IOBlitCopyRectangles;

typedef struct IOBlitVertexStruct {
    SInt32		x;
    SInt32		y;
} IOBlitVertex;

typedef struct IOBlitVerticesStruct {
    IOBlitOperation	operation;
    IOItemCount		count;
    IOBlitVertex	vertices[2];
} IOBlitVertices;

typedef struct IOBlitScanlinesStruct {
    IOBlitOperation	operation;
    IOItemCount		count;
    SInt32		y;
    SInt32		height;
    SInt32		x[2];
} IOBlitScanlines;


typedef struct _IOBlitMemory * IOBlitMemoryRef;


#if 0
/* Quickdraw.h pixel formats*/

enum {
        k1MonochromePixelFormat		= 0x00000001,		/* 1 bit indexed*/
        k2IndexedPixelFormat		= 0x00000002,		/* 2 bit indexed*/
        k4IndexedPixelFormat		= 0x00000004,		/* 4 bit indexed*/
        k8IndexedPixelFormat		= 0x00000008,		/* 8 bit indexed*/
        k16BE555PixelFormat		= 0x00000010,		/* 16 bit BE rgb 555 (Mac)*/
        k24RGBPixelFormat		= 0x00000018,		/* 24 bit rgb */
        k32ARGBPixelFormat		= 0x00000020,		/* 32 bit argb	(Mac)*/
        k1IndexedGrayPixelFormat	= 0x00000021,		/* 1 bit indexed gray*/
        k2IndexedGrayPixelFormat	= 0x00000022,		/* 2 bit indexed gray*/
        k4IndexedGrayPixelFormat	= 0x00000024,		/* 4 bit indexed gray*/
        k8IndexedGrayPixelFormat	= 0x00000028		/* 8 bit indexed gray*/
};

enum {
        k16LE555PixelFormat	= FOUR_CHAR_CODE('L555'),	/* 16 bit LE rgb 555 (PC)*/
        k16LE5551PixelFormat	= FOUR_CHAR_CODE('5551'),	/* 16 bit LE rgb 5551*/
        k16BE565PixelFormat	= FOUR_CHAR_CODE('B565'),	/* 16 bit BE rgb 565*/
        k16LE565PixelFormat	= FOUR_CHAR_CODE('L565'),	/* 16 bit LE rgb 565*/
        k24BGRPixelFormat	= FOUR_CHAR_CODE('24BG'),	/* 24 bit bgr */
        k32BGRAPixelFormat	= FOUR_CHAR_CODE('BGRA'),	/* 32 bit bgra	(Matrox)*/
        k32ABGRPixelFormat	= FOUR_CHAR_CODE('ABGR'),	/* 32 bit abgr	*/
        k32RGBAPixelFormat	= FOUR_CHAR_CODE('RGBA'),	/* 32 bit rgba	*/
        kYUVSPixelFormat	= FOUR_CHAR_CODE('yuvs'),	/* YUV 4:2:2 byte ordering 16-unsigned = 'YUY2'*/
        kYUVUPixelFormat	= FOUR_CHAR_CODE('yuvu'),	/* YUV 4:2:2 byte ordering 16-signed*/
        kYVU9PixelFormat	= FOUR_CHAR_CODE('YVU9'),	/* YVU9 Planar	9*/
        kYUV411PixelFormat	= FOUR_CHAR_CODE('Y411'),	/* YUV 4:1:1 Interleaved 16*/
        kYVYU422PixelFormat	= FOUR_CHAR_CODE('YVYU'),	/* YVYU 4:2:2 byte ordering 16*/
        kUYVY422PixelFormat	= FOUR_CHAR_CODE('UYVY'),	/* UYVY 4:2:2 byte ordering 16*/
        kYUV211PixelFormat	= FOUR_CHAR_CODE('Y211')	/* YUV 2:1:1 Packed	8*/
};
#endif

typedef struct IOBlitMemoryStruct {
    union {
        UInt8 *		bytes;
        IOBlitMemoryRef ref;
    }			memory;
    FourCharCode	pixelFormat;
    IOBlitRectangle	size;
    UInt32		rowBytes;
    UInt32		byteOffset;
    UInt32 *		palette;
    UInt32		more[16];
} IOBlitMemory;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    // options for Synchronize
    kIOBlitSynchronizeWaitBeamExit	= 0x00000001,
    kIOBlitSynchronizeFlushHostWrites	= 0x00000002
};

enum {
    // options for WaitForCompletion
    kIOBlitWaitGlobal			= 0x00000001
};

enum {
    // options for blit procs
    kIOBlitBeamSync			= 0x00000001,
    kIOBlitBeamSyncAlways		= 0x00000002,
    kIOBlitBeamSyncSpin			= 0x00000004,

    kIOBlitAllOptions			= 0xffffffff
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef
IOReturn (*IOBlitAccumulatePtr)(void *thisPointer,
                                SInt32 a, SInt32 b, SInt32 c,
                                SInt32 d, SInt32 f, SInt32 g );

typedef
IOReturn (*IOBlitProcPtr)(void *thisPointer,
                          IOOptionBits options,
                          IOBlitType type, IOBlitSourceDestType sourceDestType,
                          IOBlitOperation * operation,
                          void * source, void * destination,
                          IOBlitCompletionToken * completionToken );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef NO_CFPLUGIN

typedef struct IOGraphicsAcceleratorInterfaceStruct {
    IUNKNOWN_C_GUTS;
    IOCFPLUGINBASE;
    
    IOReturn (*Reset)
        (void *thisPointer, IOOptionBits options);
    IOReturn (*CopyCapabilities)
        (void *thisPointer, FourCharCode select, CFTypeRef * capabilities);
    IOReturn (*GetBlitProc)
        (void *thisPointer, IOOptionBits options,
         IOBlitType type, IOBlitSourceDestType sourceDestType,
         IOBlitProcPtr * blitProc );
    IOReturn (*Flush)
        (void *thisPointer, IOOptionBits options);
    IOReturn (*WaitForCompletion)
        (void *thisPointer, IOOptionBits options,
         IOBlitCompletionToken completionToken);
    IOReturn (*Synchronize)
        (void *thisPointer, UInt32 options,
         UInt32 x, UInt32 y, UInt32 w, UInt32 h );
    IOReturn (*GetBeamPosition)
        (void *thisPointer, IOOptionBits options, SInt32 * position);
    IOReturn (*AllocateMemory)
        (void *thisPointer, IOOptionBits options, vm_size_t length,
         IOBlitMemoryRef * ref);
    IOReturn (*FreeMemory)
        (void *thisPointer, IOOptionBits options, IOBlitMemoryRef ref);

    void * __gaInterfaceReserved[ 24 ];

} IOGraphicsAcceleratorInterface;

#endif /* ! NO_CFPLUGIN */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#endif /* !_IOKIT_IOGRAPHICSINTERFACE_H */
