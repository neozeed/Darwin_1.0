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


#include "AppleMouse.h"
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>

#define super IOHIPointing
#define DEBUGGING_LEVEL 0

#define kMaxButtons	32	// Is this defined anywhere in the event headers?
#define kMaxValues	32	// This should be plenty big to find the X, Y and wheel values - is there some absolute max?

OSDefineMetaClassAndStructors(AppleMouse, IOHIPointing)

bool AppleMouse::start(IOService * provider)
{
    IOReturn			err = 0;

    if( !super::start(provider))
        return (false);

    if( !provider->open(this))
        return (false);

    // remember my device
    _preparsedReportDescriptorData = NULL;
    _buttonCollection = -1;
    _xCollection = -1;
    _yCollection = -1;
    _tipPressureCollection = -1;
    _digitizerButtonCollection = -1;
    _scrollWheelCollection = -1;
    _hasInRangeReport = false;
    _tipPressureMin = 255;
    _tipPressureMax = 255;
    _numButtons = 1;

    _interface		= OSDynamicCast(IOUSBInterface, provider);
    _deviceDescriptor	= _interface->deviceDescriptor();
    
    do {
        IOUSBFindEndpointRequest request;

        IOLog("%s: USB Generic Mouse @ %d\n", getName(), _interface->address());

        request.type = kUSBInterrupt;
        request.direction = kUSBIn;
        _interruptPipe = _interface->findNextPipe(NULL, &request);

        if(!_interruptPipe)
            return false;

        if (!parseHIDDescriptor()) {
            break;
        }

        _maxPacketSize = request.maxPacketSize;
        _buffer = IOBufferMemoryDescriptor::withCapacity(8, kIODirectionIn);

        _completion.target = (void *)this;
        _completion.action = (IOUSBCompletionAction) &AppleMouse::readHandler;
        _completion.parameter = (void *)0;  // not used

        _buffer->setLength(_maxPacketSize);

        if ((err = _interruptPipe->read(_buffer, &_completion)))
            break;

#if (DEBUGGING_LEVEL > 0)
        printInterfaceDescriptor(_interface->descriptor);
#endif
        return(true);

    } while (false);

    IOLog("%s: aborting startup.  err = %d\n", getName(), err);

    provider->close(this);
    stop(provider);

    return(false);
}

bool AppleMouse::parseHIDDescriptor()
{
    bool 			success = true;
    IOReturn			err;
    IOUSBDevRequest		devReq;
    IOUSBHIDDescriptor		hidDescriptor;
    UInt8 *			reportDescriptor = NULL;
    UInt16			size = 0;
    OSStatus			result;
    HIDButtonCaps		buttonCaps[kMaxButtons];
    UInt32			numButtonCaps = kMaxButtons;
    HIDValueCaps		valueCaps[kMaxValues];
    UInt32			numValueCaps = kMaxValues;

    do {
        devReq.rqDirection = kUSBIn;
        devReq.rqType = kUSBStandard;
        devReq.rqRecipient = kUSBInterface;
        devReq.bRequest = kUSBRqGetDescriptor;
        OSWriteLittleInt16(&devReq.wValue, 0, ((0x20 | kUSBHIDDesc) << 8));
        OSWriteLittleInt16(&devReq.wIndex, 0, _interface->interfaceDescriptor()->interfaceNumber);
        OSWriteLittleInt16(&devReq.wLength, 0, sizeof(IOUSBHIDDescriptor));
        devReq.pData = &hidDescriptor;

        err = _interface->deviceRequest(&devReq);
        if (err) {
            IOLog ("%s: error getting HID Descriptor.  err=0x%x\n", getName(), err);
            success = false;
            break;
        }

        size = (hidDescriptor.hidDescriptorLengthHi * 256) + hidDescriptor.hidDescriptorLengthLo;
        reportDescriptor = (UInt8 *)IOMalloc(size);

        OSWriteLittleInt16(&devReq.wValue, 0, ((0x20 | kUSBReportDesc) << 8));
        OSWriteLittleInt16(&devReq.wLength, 0, size);
        devReq.pData = reportDescriptor;

        err = _interface->deviceRequest(&devReq);
        if (err) {
            IOLog ("%s: error getting HID report descriptor.  err=0x%x\n", getName(), err);
            success = false;
            break;
        }
    
        result = HIDOpenReportDescriptor (reportDescriptor, size, &_preparsedReportDescriptorData, 0);
        if (result != noErr) {
            IOLog ("%s: error parsing HID report descriptor.  err=0x%lx\n", getName(), result);
            success = false;
            break;
        }

        result = HIDGetSpecificButtonCaps(kHIDInputReport,
                                          kHIDPage_Button,
                                          0,
                                          0,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if ((result == noErr) && (numButtonCaps > 0)) {
            _buttonCollection = buttonCaps[0].collection;	// Do we actually need to look at and store all of the button page collections?
            if (buttonCaps[0].isRange) {
                _numButtons = buttonCaps[0].u.range.usageMax - buttonCaps[0].u.range.usageMin + 1;
            }
            
        }

        numButtonCaps = kMaxButtons;
        result = HIDGetSpecificButtonCaps(kHIDInputReport,
                                          kHIDPage_Digitizer,
                                          0,
                                          0,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if ((result == noErr) && (numButtonCaps > 0)) {
            _digitizerButtonCollection = buttonCaps[0].collection;
        }

        numButtonCaps = kMaxButtons;
        result = HIDGetSpecificButtonCaps(kHIDInputReport,
                                          kHIDPage_Digitizer,
                                          0,
                                          kHIDUsage_Dig_InRange,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if (result == noErr) {
            _hasInRangeReport = true;
        }

        result = HIDGetSpecificValueCaps(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_X,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _xCollection = valueCaps[0].collection;
            _absoluteCoordinates = valueCaps[0].isAbsolute;
            _bounds.minx = valueCaps[0].logicalMin;
            _bounds.maxx = valueCaps[0].logicalMax;
        } else {
            IOLog ("%s: error getting X axis information from HID report descriptor.  err=0x%lx\n", getName(), result);
            success = false;
            break;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCaps(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_Y,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _yCollection = valueCaps[0].collection;
            _bounds.miny = valueCaps[0].logicalMin;
            _bounds.maxy = valueCaps[0].logicalMax;
        } else {
            IOLog ("%s: error getting Y axis information from HID report descriptor.  err=0x%lx\n", getName(), result);
            success = false;
            break;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCaps(kHIDInputReport,
                                         kHIDPage_Digitizer,
                                         0,
                                         kHIDUsage_Dig_TipPressure,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _tipPressureCollection = valueCaps[0].collection;
            _tipPressureMin = valueCaps[0].logicalMin;
            _tipPressureMax = valueCaps[0].logicalMax;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCaps(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_Wheel,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _scrollWheelCollection = valueCaps[0].collection;
        }
    } while (false);

    if (reportDescriptor) {
        IOFree(reportDescriptor, size);
    }
    
    return success;
}

void AppleMouse::stop(IOService * provider)
{
    if (_buffer) {
	_buffer->release();
        _buffer = 0;
    }
    if (_preparsedReportDescriptorData) {
        HIDCloseReportDescriptor(_preparsedReportDescriptorData);
    }
    super::stop(provider);
}

void AppleMouse::readHandler(void * 	parameter,
                             IOReturn	status,
                             UInt32	bufferSizeRemaining)
{
    switch (status)
    {
        case kIOReturnSuccess:
            break;

        case kIOReturnOverrun:
            // Not sure what to do with this error.  It means more data
            // came back than the size of a descriptor.  Hmmm.  For now
            // just ignore it and assume the data that did come back is
            // useful.
            IOLog("%s: overrun error.  ignoring.\n", getName());
            break;

        case kIOReturnNotResponding:
            // This probably means the device was unplugged.  Now
            // we need to close the driver.
            IOLog("%s: Device unplugged.  Goodbye\n", getName());
            goto close;

        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            IOLog("%s: error reading interrupt pipe\n", getName());
            goto queueAnother;
    }

    // Handle the data
    MoveMouse((UInt8 *) _buffer->getBytesNoCopy(),
              (UInt32)  _maxPacketSize - bufferSizeRemaining);

queueAnother:
    // Reset the buffer
    _buffer->setLength(_maxPacketSize);

    // Queue up another one before we leave.
    if ((status = _interruptPipe->read(_buffer, &_completion)))
    {
        // This is bad.  We probably shouldn't continue on from here.
        IOLog("%s: immediate error %d queueing read\n", getName(), status);
        goto close;
    }

    return;

close:
    _interface->close(this);
    return;
}

void AppleMouse::MoveMouse(UInt8 *	mouseData,
                           UInt32	ret_bufsize)
{
    OSStatus	status;
    HIDUsage	usageList[kMaxButtons];
    UInt32	usageListSize = kMaxButtons;
    UInt32	buttonState = 0;
    SInt32	usageValue;
    SInt32	pressure = MAXPRESSURE;
    int		dx = 0, dy = 0, scrollWheelDelta = 0;
    AbsoluteTime now;
    bool	inRange = !_hasInRangeReport;

    if (_buttonCollection != -1) {
        status = HIDGetButtonsOnPage (kHIDInputReport,
                                      kHIDPage_Button,
                                      _buttonCollection,
                                      usageList,
                                      &usageListSize,
                                      _preparsedReportDescriptorData,
                                      mouseData,
                                      ret_bufsize);
        if (status == noErr) {
            UInt32 usageNum;
            for (usageNum = 0; usageNum < usageListSize; usageNum++) {
                if (usageList[usageNum] <= kMaxButtons) {
                    buttonState |= (1 << (usageList[usageNum] - 1));
                }
            }
        }

    }

    if (_tipPressureCollection != -1) {
        status = HIDGetUsageValue (kHIDInputReport,
                                   kHIDPage_Digitizer,
                                   _tipPressureCollection,
                                   kHIDUsage_Dig_TipPressure,
                                   &usageValue,
                                   _preparsedReportDescriptorData,
                                   mouseData,
                                   ret_bufsize);
        if (status == noErr) {
            pressure = usageValue;
        }
    }

    if (_digitizerButtonCollection != -1) {
        usageListSize = kMaxButtons;
        status = HIDGetButtonsOnPage (kHIDInputReport,
                                      kHIDPage_Digitizer,
                                      _digitizerButtonCollection,
                                      usageList,
                                      &usageListSize,
                                      _preparsedReportDescriptorData,
                                      mouseData,
                                      ret_bufsize);
        if (status == noErr) {
            UInt32 usageNum;
            for (usageNum = 0; usageNum < usageListSize; usageNum++) {
                switch (usageList[usageNum]) {
                    case kHIDUsage_Dig_BarrelSwitch:
                        buttonState |= 2;	// Set the right (secondary) button for the barrel switch
                        break;
                    case kHIDUsage_Dig_TipSwitch:
                        buttonState |= 1;	// Set the left (primary) button for the tip switch
                        break;
                    case kHIDUsage_Dig_InRange:
                        inRange = 1;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    if (_scrollWheelCollection != -1) {
        status = HIDGetUsageValue (kHIDInputReport,
                                   kHIDPage_GenericDesktop,
                                   _scrollWheelCollection,
                                   kHIDUsage_GD_Wheel,
                                   &usageValue,
                                   _preparsedReportDescriptorData,
                                   mouseData,
                                   ret_bufsize);
        if (status == noErr) {
            scrollWheelDelta = usageValue;
        }
    }

    status = HIDGetUsageValue (kHIDInputReport,
                               kHIDPage_GenericDesktop,
                               _xCollection,
                               kHIDUsage_GD_X,
                               &usageValue,
                               _preparsedReportDescriptorData,
                               mouseData,
                               ret_bufsize);
    if (status == noErr) {
        dx = usageValue;
    }

    status = HIDGetUsageValue (kHIDInputReport,
                               kHIDPage_GenericDesktop,
                               _yCollection,
                               kHIDUsage_GD_Y,
                               &usageValue,
                               _preparsedReportDescriptorData,
                               mouseData,
                               ret_bufsize);
    if (status == noErr) {
        dy = usageValue;
    }

    clock_get_uptime(&now);

    if (_absoluteCoordinates) {
        Point newLoc;

        newLoc.x = dx;
        newLoc.y = dy;

        dispatchAbsolutePointerEvent(&newLoc, &_bounds, buttonState, inRange, pressure, _tipPressureMin, _tipPressureMax, 90, now);
    } else {
        dispatchRelativePointerEvent(dx, dy, buttonState, now);
    }

    if (scrollWheelDelta != 0) {
        dispatchScrollWheelEvent(scrollWheelDelta, 0, 0, now);
    }
}

UInt32 AppleMouse::interfaceID( void )
{
    return( NX_EVS_DEVICE_INTERFACE_OTHER );
}

UInt32 AppleMouse::deviceType( void )
{
    return( 0 );
}

/*
 * This was taken directly from the AppleADBMouse driver.
 * We need to determine the best generic acceleration curve for USB devices.
 */

void AppleMouse::accelerationTable ( IOHIAccelerationPoint ** table, IOItemCount * numEntries )
{
static IOHIAccelerationPoint defaultTable[] = {
        { 0x0000000, 0x000000 },
        { 0x000713b, 0x006000 },
        { 0x0010000, 0x010000 },
        { 0x0044ec5, 0x108000 },
        { 0x00c0000, 0x5f0000 },
        { 0x016ec4f, 0x8b0000 },
        { 0x01d3b14, 0x948000 },
        { 0x0227627, 0x960000 },
        { 0x7ffffff, 0x960000 }
    };

*table = defaultTable;
*numEntries = sizeof(defaultTable) / sizeof(defaultTable[0]);
}

IOFixed AppleMouse::resolution()
{
    return (400 << 16);
}

IOItemCount AppleMouse::buttonCount()
{
    return _numButtons;
}

