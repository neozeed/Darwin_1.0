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

#include <IOKit/IOKitLib.h>

#include <IOKit/graphics/IOGLWindowControl.h>

#include "IOGLPrivate.h"

IOReturn IOGLCreateWindow( io_service_t device, UInt32 wid, eIOGLWindowModeBits modebits, IOGLConnect *glconnect )
{
	IOReturn      kr;
	io_connect_t  window = MACH_PORT_NULL;
	SInt32        data[2];
	io_service_t  service;
	io_iterator_t iter;

	*glconnect = NULL;

	kr = IORegistryEntryGetChildIterator( device, kIOServicePlane, &iter );
	if( kr != kIOReturnSuccess)
	{
		return kr;
	}

	for( kr = kIOReturnUnsupported;
		(!window && (service = IOIteratorNext(iter)));
		IOObjectRelease(service))
	{
		if( !IOObjectConformsTo( service, kIOGLAcceleratorClassName ))
			continue;

		/* Create a context */
		kr = IOServiceOpen( service,
			    mach_task_self(),
			    kIOGLWindowClientType,
			   &window );

		if( kr != kIOReturnSuccess)
		{
			window = MACH_PORT_NULL;
		}
	}

	IOObjectRelease(iter);

	if( !window )
	{
		return kr;
	}

	/* Set the window id */
	data[0] = wid;
	data[1] = modebits;
	kr = io_connect_method_scalarI_structureI(window, kIOGLWindowSetIDMode,
		data, 2, NULL, 0);
	if(kr != kIOReturnSuccess)
	{
		IOServiceClose(window);
		return kr;
	}

	*glconnect = (IOGLConnect) window;

	return kIOReturnSuccess;
}

IOReturn IOGLDestroyWindow( IOGLConnect glconnect )
{
	IOReturn kr;

	if(!glconnect)
		return kIOReturnError;

	kr = IOServiceClose((io_connect_t) glconnect);

	return kr;
}

IOReturn IOGLSetWindowShape( IOGLConnect glconnect, IOGLDeviceRegion *rgn, eIOGLWindowShapeBits options )
{
	return io_connect_method_scalarI_structureI((io_connect_t) glconnect, kIOGLWindowSetShape,
		(SInt32 *) &options, 1, (SInt32 *) rgn, IOGL_SIZEOF_DEVICE_REGION(rgn));
}

IOReturn IOGLLockWindow( IOGLConnect glconnect, UInt32 *offset, UInt32 *rowbytes, UInt32 *width, UInt32 *height )
{
	SInt32 vars[4];
	SInt32 countio = 4;
	IOReturn ret;

	ret = io_connect_method_scalarI_scalarO((io_connect_t) glconnect, kIOGLWindowLock,
		NULL, 0, vars, &countio);

	if(ret == kIOReturnSuccess && countio == 4)
	{
		*offset   = vars[0];
		*rowbytes = vars[1];
		*width    = vars[2];
		*height   = vars[3];
	}

	return ret;
}

IOReturn IOGLUnlockWindow( IOGLConnect glconnect )
{
	SInt32 countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) glconnect, kIOGLWindowUnlock,
		NULL, 0, NULL, &countio);
}

IOReturn IOGLWaitForWindow( IOGLConnect glconnect )
{
	return kIOReturnSuccess;
}

