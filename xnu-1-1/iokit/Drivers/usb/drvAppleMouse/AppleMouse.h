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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_APPLEMOUSE_H
#define _IOKIT_APPLEMOUSE_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USB.h>

class AppleMouse : public IOHIPointing
{
    OSDeclareDefaultStructors(AppleMouse)
    
    const IOUSBDeviceDescriptor *_deviceDescriptor;
    IOUSBInterface *		_interface;
    IOUSBPipe * 		_interruptPipe;
    IOBufferMemoryDescriptor *	_buffer;
    IOUSBCompletion		_completion;
    UInt16			_maxPacketSize;
    HIDPreparsedDataRef		_preparsedReportDescriptorData;
    SInt32			_buttonCollection;
    SInt32			_xCollection;
    SInt32			_yCollection;
    SInt32			_tipPressureCollection;
    SInt32			_digitizerButtonCollection;
    SInt32			_scrollWheelCollection;
    int				_tipPressureMin;
    SInt16			_tipPressureMax;
    bool			_absoluteCoordinates;
    bool			_hasInRangeReport;
    Bounds			_bounds;
    IOItemCount			_numButtons;

public:

    virtual bool	start(IOService * provider);
    virtual void 	stop(IOService *  provider);

    virtual bool	parseHIDDescriptor();

    virtual UInt32 interfaceID( void );
    virtual UInt32 deviceType( void );

    void accelerationTable(IOHIAccelerationPoint ** table,
                                   IOItemCount *            numEntries);
    IOFixed     resolution();
    IOItemCount buttonCount();

    void                readHandler(void * 	parameter,
                                    IOReturn	status,
                                    UInt32	bufferSizeRemaining);

    void                MoveMouse(UInt8 * 	mouseData,
                                  UInt32	ret_bufsize);
};

#endif _IOKIT_APPLEMOUSE_H
