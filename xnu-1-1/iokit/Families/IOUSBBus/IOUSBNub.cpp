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
 * 08 Dec 98 ehewitt created.
 *
 */

#include <libkern/OSByteOrder.h>

#include <libkern/c++/OSDictionary.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/c++/OSData.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBPipe.h>

#include "IOUSBUserClient.h"
#include "IOUSBHub.h"

static int devdebug = 0;
#define DEBUGLOG if (devdebug) kprintf

static const IOUSBDescriptorHeader *nextDescriptor(const void *desc)
{
    const UInt8 *next = (const UInt8 *)desc;
    UInt8 length = next[0];
    next = &next[length];
    return((const IOUSBDescriptorHeader *)next);
}

const IOUSBDescriptorHeader *
IOUSBNub::findNextDescriptor(const void *cur, UInt8 descType)
{
    const IOUSBDescriptorHeader *hdr;
    hdr = (const IOUSBDescriptorHeader *)cur;
    do {
	hdr = nextDescriptor(hdr);
        if(hdr->length == 0)
            break;
        if(descType == 0)
            return hdr;	// type 0 is wildcard.
        else if(hdr->descriptorType == descType)
            return hdr;
    }
    while(true);

    return NULL;	// Fell off end of list
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOUSBNub, IOService )
OSDefineAbstractStructors(IOUSBNub, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOUSBNub::init(OSDictionary * propTable)
{
    OSData *	addressProperty;
    OSData *   	descProperty;
    OSData *   	powerProperty;
    OSData *   	speedProperty;

    if( !IOService::init(propTable))
        return (false);

    addressProperty = (OSData *) propTable->getObject("device address");
    descProperty = (OSData *) propTable->getObject("device descriptor");
    powerProperty = (OSData *) propTable->getObject("bus power available");
    speedProperty = (OSData *) propTable->getObject("low speed device");
    if (!addressProperty || !powerProperty || !speedProperty || !descProperty)
    {
        IOLog("%s: Unable to initialize\n", getName());
        return (false);
    }
    _address = *((USBDeviceAddress *) addressProperty->getBytesNoCopy());

    bcopy(descProperty->getBytesNoCopy(), &_descriptor,
              					descProperty->getLength());

    _busPowerAvailable = *((UInt32 *) powerProperty->getBytesNoCopy());
    _speed = *((UInt8 *) speedProperty->getBytesNoCopy());
    DEBUGLOG("%s @ %d (%ldmA available, %s speed)\n", getName(), _address,_busPowerAvailable*2, _speed ? "low" : "high");

#if (devdebug > 0)
    printDeviceDescriptor(&_descriptor);
#endif
    
    return (true);
}

bool IOUSBNub::finalize(IOOptionBits options)
{
    if(_pipeZero) {
        _pipeZero->release();
        _pipeZero = NULL;
    }
    return(IOService::finalize(options));
}

USBDeviceAddress IOUSBNub::address(void)
{
    return(_address);
}

const IOUSBDeviceDescriptor * IOUSBNub::deviceDescriptor(void)
{
    return(&_descriptor);
}

IOUSBController * IOUSBNub::bus(void)
{
    return(_controller);
}

UInt32 IOUSBNub::busPowerAvailable( void )
{
    return(_busPowerAvailable);
}

IOUSBPipe * IOUSBNub::pipeZero()
{
    return _pipeZero;
}

// Lowlevel requests for non-standard device requests
IOReturn IOUSBNub::deviceRequest(IOUSBDevRequest	*request,
                                    IOUSBCompletion	*completion = 0)
{
    UInt16	theRequest;
    theRequest = EncodeRequest(request->bRequest, request->rqDirection,
                               request->rqType,   request->rqRecipient);
    if (theRequest == kSetAddress) {
        DEBUGLOG("\tignoring kSetAddress\n");
        return kIOReturnNotPermitted;
    }
    return(_pipeZero->controlRequest(request, completion));
}

IOReturn IOUSBNub::deviceRequest(IOUSBDevRequestDesc	*request,
                                    IOUSBCompletion	*completion = 0)
{
    UInt16	theRequest;
    theRequest = EncodeRequest(request->bRequest, request->rqDirection,
                               request->rqType,   request->rqRecipient);
    if (theRequest == kSetAddress) {
        DEBUGLOG("\tignoring kSetAddress\n");
        return kIOReturnNotPermitted;
    }
    return(_pipeZero->controlRequest(request, completion));
}


IOReturn IOUSBNub::GetConfiguration(UInt8 *configNumber)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
 
    DEBUGLOG("************ GET CONFIGURATION *************\n");

    request.rqDirection = kUSBIn;
    request.rqType = kUSBStandard;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqGetConfig;
    OSWriteLittleInt16(&request.wValue, 0, 0);
    OSWriteLittleInt16(&request.wIndex, 0, 0);
    OSWriteLittleInt16(&request.wLength, 0, sizeof(*configNumber));
    request.pData = configNumber;

    err = deviceRequest(&request);

    if (err)
    {
        DEBUGLOG("%s: error getting config. err=%d\n", getName(), err);
    }

    return(err);
}

IOReturn IOUSBNub::GetDeviceStatus(USBStatus *status)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    DEBUGLOG("*********** GET DEVICE STATUS ***********\n");

    request.rqDirection = kUSBIn;
    request.rqType = kUSBStandard;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqGetStatus;
    OSWriteLittleInt16(&request.wValue, 0, 0);
    OSWriteLittleInt16(&request.wIndex, 0, 0);
    OSWriteLittleInt16(&request.wLength, 0, sizeof(*status));
    request.pData = status;

    err = deviceRequest(&request);

    if (err)
        IOLog("%s: error getting device status. err=%d\n", getName(), err);

    return(err);
}

IOReturn IOUSBNub::GetStringDescriptor(UInt8 index,
                                          char *buf, int maxLen, UInt16 lang)
{
    IOReturn 		err;
    UInt8 		desc[256]; // Max possible descriptor length
    IOUSBDevRequest	request;
    int			i, len;

    DEBUGLOG("*********** GET STRING DESCRIPTOR %d, lang 0x%x***********\n", index, lang);

    // First get actual length (lame devices don't like being asked for too much data)
    request.rqDirection = kUSBIn;
    request.rqType = kUSBStandard;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqGetDescriptor;
    OSWriteLittleInt16(&request.wValue, 0, (kUSBStringDesc << 8) | index);
    OSWriteLittleInt16(&request.wIndex, 0, lang);

    OSWriteLittleInt16(&request.wLength, 0, 2);
    bzero(desc, 2);
    request.pData = &desc;

    err = deviceRequest(&request);

    if (err != kIOReturnSuccess) 
	return(err);

    request.rqDirection = kUSBIn;
    request.rqType = kUSBStandard;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqGetDescriptor;
    len = desc[0];
    if(len == 0)
	len = sizeof(desc);	// Silly result for small read, try big one.
    OSWriteLittleInt16(&request.wValue, 0, (kUSBStringDesc << 8) | index);
    OSWriteLittleInt16(&request.wIndex, 0, lang);
    OSWriteLittleInt16(&request.wLength, 0, len);
    request.pData = &desc;

    err = deviceRequest(&request);

    if (err != kIOReturnSuccess)
        return(err);
    DEBUGLOG("*********** Got STRING DESCRIPTOR %d, length 0x%x, got 0x%x***********\n",
             index, desc[0], OSReadLittleInt16(&request.wLength, 0));
    len = (desc[0]-2)/2;
    if(len > maxLen-1)
	len = maxLen-1;
    for(i=0; i<len; i++)
	buf[i] = desc[2*i+2];
    buf[len] = 0;

    return kIOReturnSuccess;
}

UInt32 IOUSBNub::GetBandwidthAvailable()
{
    return _controller->GetBandwidthAvailable();
}

UInt64 IOUSBNub::GetFrameNumber()
{
    return _controller->GetFrameNumber();
}

/**
 ** Matching methods
 **/
bool IOUSBNub::matchPropertyTable(OSDictionary * table)
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //

    if (!IOService::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.

    return compareProperty(table, "class") &&
        compareProperty(table, "subClass") &&
        compareProperty(table, "protocol") &&
        compareProperty(table, "vendor") &&
        compareProperty(table, "product") &&
        compareProperty(table, "version");
}

#ifdef __cplusplus
extern "C" {
#endif

void printDescriptor(IOUSBDescriptorHeader *desc)
{
    if (!desc)
        return;

    switch (desc->descriptorType)
    {
        case kUSBDeviceDesc:
            printDeviceDescriptor((IOUSBDeviceDescriptor *)desc); break;
        case kUSBConfDesc:
            printConfigDescriptor((IOUSBConfigurationDescriptor *)desc); break;
        case kUSBInterfaceDesc:
            printInterfaceDescriptor((IOUSBInterfaceDescriptor *)desc); break;
        case kUSBEndpointDesc:
            printEndpointDescriptor((IOUSBEndpointDescriptor *)desc); break;
        default:
            DEBUGLOG("printDescriptor: unknown descriptor type (%d)\n",
                  desc->descriptorType);

    }
}

// for debugging.  Should print generic
// descriptor based on descriptor type.
void printDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    if (!desc)
        return;

    DEBUGLOG("device descriptor: v0x%x (%d bytes) type = %d  %d configs\n",
          OSReadLittleInt16(&desc->usbRel, 0), desc->length, desc->descType,
          desc->numConf);
    DEBUGLOG("\tclass = [%d:%d:%d]  maxPacketSize = %d\n", desc->deviceClass,
          desc->deviceSubClass, desc->protocol, desc->maxPacketSize);
    DEBUGLOG("\tvendor = [%x:%x:%x]  idx's = [%d:%d:%d]\n",
          OSReadLittleInt16(&desc->vendor, 0), USBToHostWord(desc->product),
          OSReadLittleInt16(&desc->devRel, 0), desc->manuIdx, desc->prodIdx,
          desc->serialIdx);
}

// for debugging -- nothing yet.  Should print generic
// descriptor based on descriptor type.
void printConfigDescriptor(IOUSBConfigurationDescriptor *cd)
{
    void *end;
    const IOUSBDescriptorHeader *dp;

    if (!cd)
        return;

    DEBUGLOG("CONFIG #%d: (%d bytes) T(%d bytes) ",
        cd->configValue, cd->length, OSReadLittleInt16(&cd->totalLength, 0));
    DEBUGLOG("descType = %d  numInterfaces = %d ",
          cd->descriptorType, cd->numInterfaces);
    DEBUGLOG("attributes = %d  maxPower = %d\n",
          cd->attributes, cd->maxPower);

    end = (void *)(((UInt8 *)cd) + OSReadLittleInt16(&cd->totalLength, 0));
    dp = nextDescriptor((const IOUSBDescriptorHeader *)cd);

    while(dp < end)
    {
        if (dp->descriptorType == kUSBInterfaceDesc)
            printInterfaceDescriptor((IOUSBInterfaceDescriptor *)dp);
        else if (dp->descriptorType == kUSBEndpointDesc)
            printEndpointDescriptor((IOUSBEndpointDescriptor *)dp);
        else
            DEBUGLOG("Unknown descriptor type %d\n", dp->descriptorType);

        dp = nextDescriptor(dp);
    }
}

void printInterfaceDescriptor(IOUSBInterfaceDescriptor *id)
{
    if (!id)
        return;

    DEBUGLOG("INTERFACE #%d/%d: (%d bytes)  type = %d  ",
          id->interfaceNumber, id->alternateSetting, id->length,
          id->descriptorType);
    DEBUGLOG("class = [%d:%d:%d]  numEndpoints = %d\n", id->interfaceClass,
          id->interfaceSubClass, id->interfaceProtocol, id->numEndpoints);
}

void printEndpointDescriptor(IOUSBEndpointDescriptor *ed)
{
    char *eType[] = { "control", "isochronous", "bulk", "interrupt" };
    if (!ed)
        return;

    DEBUGLOG("ENDPOINT #%x (%s): (%d bytes, type %d) type = %d (%s) ",
          ed->endpointAddress & 0x0F, ed->endpointAddress & 0x80 ? "in" : "out",
          ed->length, ed->descriptorType, ed->attributes & 0x03,
          eType[ed->attributes & 0x03]);
    DEBUGLOG("interval = %dms  maxPacketSize = %d\n",
          ed->interval, OSReadLittleInt16(&ed->maxPacketSizeLo, 0));
}

#ifdef __cplusplus
}
#endif

