/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
#include <mach/std_types.h>


#ifndef __cplusplus
#include <IOKit/IODevice.h>

typedef void (*pmCallback_func)(id obj_id, UInt32 refNum, UInt32 length, UInt8 * buffer);
#else
typedef void (*pmCallback_func)(IOService * client, UInt32 length, UInt8 * buffer);

#endif

#define kPMUname  "PMU Driver"

enum {
    kPMUNoError         = 0,
    kPMUInitError	= 1,	// PMU failed to initialize
    kPMUParameterError	= 2,	// Bad parameters
    kPMUNotSupported	= 3,	// PMU don't do that (Cuda does, though)
    kPMUIOError         = 4	// Nonspecific I/O failure
    };

#ifndef __cplusplus

// **********************************************************************************
//
// exported protocols
//
// **********************************************************************************

@protocol RTCservice

- (void) registerForClockTicks	:(pmCallback_func)tickHandler
				:(id)caller;

- (IOReturn)setRealTimeClock   :(UInt8 *)newTime;

- (IOReturn)getRealTimeClock:	(UInt8 *)currentTime
                        		length:	(IOByteCount *) length;

@end



@protocol NVRAMservice

- (IOReturn) readNVRAM:	(UInt32)Offset
                		length:	(IOByteCount *)Length
               		 contents:(UInt8 *)Buffer;

- (IOReturn) writeNVRAM	:	(UInt32)Offset
                        		length:	(IOByteCount *)Length
                        		contents:	(UInt8 *)Buffer;

@end



@protocol PowerService

- (void) registerForPowerInterrupts	:(pmCallback_func)buttonHandler
					:(id)caller;

@end

#endif
