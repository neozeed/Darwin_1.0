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
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */

#ifndef _IOKIT_IOHIDLIB_H
#define _IOKIT_IOHIDLIB_H

#include <IOKit/hidsystem/IOHIDShared.h>


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern kern_return_t
IOHIDCreateSharedMemory( io_connect_t connect,
	unsigned int version );

extern kern_return_t
IOHIDSetEventsEnable( io_connect_t connect,
	boolean_t enable );

extern kern_return_t
IOHIDSetCursorEnable( io_connect_t connect,
	boolean_t enable );

extern kern_return_t
IOHIDPostEvent( io_connect_t connect,
        int type, IOGPoint location, NXEventData *data,
	boolean_t setCursor, int flags, boolean_t setFlags);

extern kern_return_t
IOHIDSetMouseLocation( io_connect_t connect,
	int x, int y);

extern kern_return_t
IOHIDGetButtonEventNum( io_connect_t connect,
	NXMouseButton button, int * eventNum );

extern kern_return_t
IOHIDGetMouseAcceleration( io_connect_t handle, double * acceleration );

extern kern_return_t
IOHIDSetMouseAcceleration( io_connect_t handle, double acceleration );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#endif /* ! _IOKIT_IOHIDLIB_H */

