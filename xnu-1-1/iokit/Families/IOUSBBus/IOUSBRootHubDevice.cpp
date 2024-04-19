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

#include <libkern/OSByteOrder.h>

#include "IOUSBRootHubDevice.h"

#define super	IOUSBDevice
#define self	this
#define DEBUGGING_LEVEL 0
#define DEBUGLOG kprintf

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors( IOUSBRootHubDevice, IOUSBDevice )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


// intercept regular hub requests since the controller simulates the root hub
IOReturn IOUSBRootHubDevice::deviceRequest(IOUSBDevRequest	*request,
                                           IOUSBCompletion	*completion = 0)
{
    IOReturn		err = 0;
    UInt16		theRequest;
    UInt16		wIndex;
    UInt16		wValue;
    UInt16		wLength;
    UInt8		dType, dIndex;

    
    if (!request)
        return(kIOReturnBadArgument);

    wValue = OSReadLittleInt16(&request->wValue, 0);
    wIndex = OSReadLittleInt16(&request->wIndex, 0);
    wLength = OSReadLittleInt16(&request->wLength, 0);

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: deviceRequest([%x,%s,%x,%x],[%x,%x],[%x,%lx])", getName(),
             request->bRequest,
             request->rqDirection?"in":"out",
             request->rqType, request->rqRecipient,
             wValue, wIndex, wLength, (UInt32)request->pData);
#endif

    theRequest = EncodeRequest(request->bRequest, request->rqDirection,
                               request->rqType,   request->rqRecipient);
    switch (theRequest)
    {
        // Standard Requests
        case kClearDeviceFeature:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkClearDeviceFeature\n");
#endif
            if (wIndex == 0)
                err = _controller->clearRootHubFeature(wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetDescriptor:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkGetDescriptor\n");
#endif
            dType = wValue >> 8;
            dIndex = wValue & 0x00FF;
            switch (dType) {
                case kUSBDeviceDesc:
                    err = _controller->getRootHubDeviceDescriptor(
                                   (IOUSBDeviceDescriptor*)request->pData);
                    break;

                case kUSBConfDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024); // FIXME
                    UInt16 newLength;
                    
                    err = _controller->getRootHubConfDescriptor(fullDesc);
                    newLength = fullDesc->getLength();
                    if (newLength < wLength)
                        wLength = newLength;
                    bcopy(fullDesc->getBytesNoCopy(),
                          (char *)request->pData,
                          wLength);
                    request->wLenDone = wLength;
                    fullDesc->free();
                    break;
                }

                case kUSBStringDesc:
                default:
                    err = kIOReturnInvalid;
            }
            break;

        case kGetDeviceStatus:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkGetDeviceStatus\n");
#endif
            if (wValue == 0 && wIndex == 0 && request->pData != 0)
                *(UInt16*)(request->pData) = HostToUSBWord(1); // self-powered
            else
                err = kIOReturnBadArgument;
            break;

        case kSetAddress:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkSetAddress\n");
#endif
            if (wIndex == 0)
                err = _controller->setHubAddress(wValue);
            else
                err = kIOReturnBadArgument;
            break;
                
        case kSetConfiguration:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkSetConfiguration\n");
#endif
            if (wIndex == 0)
                configuration = wValue;
            else
                err = kIOReturnBadArgument;
            break;

        case kSetDeviceFeature:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkSetDeviceFeature\n");
#endif
            if (wIndex == 0)
                err = _controller->setRootHubFeature(wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetConfiguration:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkGetConfiguration\n");
#endif
            if (wIndex == 0 && request->pData != 0)
                *(UInt8*)(request->pData) = configuration;
            else
                err = kIOReturnBadArgument;
            break;

        case kClearInterfaceFeature:
        case kClearEndpointFeature:
        case kGetInterface:
        case kGetInterfaceStatus:
        case kGetEndpointStatus:
        case kSetInterfaceFeature:
        case kSetEndpointFeature:
        case kSetDescriptor:
        case kSetInterface:
        case kSyncFrame:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tUNIMPLEMENTED request\n"); 
#endif
            err = kIOReturnUnsupported;
            break;

        // Class Requests
        case kClearHubFeature:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkClearHubFeature\n");
#endif
            if (wIndex == 0)
                err = _controller->clearRootHubFeature(wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kClearPortFeature:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkClearPortFeature\n");
#endif
            err = _controller->clearRootHubPortFeature(wValue, wIndex);
            break;

        case kGetPortState:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkGetPortState\n");
#endif
            if (wValue == 0 && request->pData != 0)
                err = _controller->getRootHubPortState((UInt8 *)request->pData,
                                                       wIndex);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubDescriptor:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkGetHubDescriptor\n");
#endif
            if (wValue == kUSBHubDescriptorType && request->pData != 0)
                err = _controller->getRootHubDescriptor(
                                        (IOUSBHubDescriptor *)request->pData);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubStatus:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkGetHubStatus\n");
#endif
            if (wValue == 0 && wIndex == 0 && request->pData != 0)
                err = _controller->getRootHubStatus(
                                              (IOUSBHubStatus *)request->pData);
            else
                err = kIOReturnBadArgument;
           break;

        case kGetPortStatus:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkGetPortStatus\n");
#endif
            if (wValue == 0 && request->pData != 0)
                err = _controller->getRootHubPortStatus(
                		(IOUSBHubPortStatus *)request->pData, wIndex);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubDescriptor:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkSetHubDescriptor\n");
#endif
            if (request->pData != 0)
                err = _controller->setRootHubDescriptor(
                                                   (OSData *)request->pData);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubFeature:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkSetHubFeature\n");
#endif
            if (wIndex == 0)
                err = _controller->setRootHubFeature(wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetPortFeature:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tkSetPortFeature\n");
#endif
            err = _controller->setRootHubPortFeature(wValue, wIndex);
            break;

        default:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("\tINVALID request\n"); 
#endif
            err = kIOReturnInvalid;

    }
    return(err);
}

