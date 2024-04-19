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
#include <stdio.h>
#include <bsd/string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFMachPort.h>

#include <libkern/OSByteOrder.h>
#include <IOKit/IOKitLib.h>
#include "IOUSBLib.h"
#include <IOKit/usb/USB.h>

#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h> 	// mig generated
#include <IOKit/IOKitLibPrivate.h>
/*
 * Define structures that are opaque to clients
 */
struct IOUSBDeviceIterator {
    io_iterator_t	iter;
    io_object_t		obj;
    IOUSBMatch		match;
};
typedef struct IOUSBDeviceIterator IOUSBDeviceIterator;

struct IOUSBDevice {
    io_connect_t		kernDevice;
    mach_port_t			asyncPort;
    CFRunLoopSourceRef		cfSource;
    IOUSBDeviceDescriptor 	_descriptor;
    IOUSBConfiguration **	_configList;
    IOUSBInterface **		_interfaceList;
    IOUSBEndpoint **      	_endpointList;    
};

struct IOUSBUserPipe {
    IOUSBDeviceRef 	device;
    io_connect_t	kernDevice;
    int			kernPipe;
};
typedef struct IOUSBUserPipe IOUSBUserPipe;

/* --------------------------------------------------------- */
static IOReturn getDescFromProps(io_object_t device,
		CFStringRef propname, void * desc, UInt32 size);
static IOUSBDescriptorHeader *nextDescriptor(
				IOUSBDescriptorHeader *desc);

/* --------------------------------------------------------- */

IOReturn
IOUSBCreateIterator(mach_port_t master_device_port, mach_port_t port,
		IOUSBMatch * descMatch, IOUSBIteratorRef * iter)
{
    IOReturn kr;
    IOUSBIteratorRef it = 
		(IOUSBIteratorRef)malloc(sizeof(IOUSBDeviceIterator));

    if(!it)
        return kIOReturnNoMemory;

    it->obj = 0;
    kr = IOServiceAddNotification( master_device_port, kIOMatchedNotification,
                                   IOServiceMatching( "IOUSBNub" ),
                                    port, (int)it, &it->iter );
    if(kIOReturnSuccess != kr) {
        free(it);
    }
    else {
        bcopy(descMatch, &it->match, sizeof(IOUSBMatch));
        *iter = it;
    }
    return kr; 
}

IOReturn
IOUSBDisposeIterator(IOUSBIteratorRef iter)
{
    if(iter->obj)
	IOObjectRelease( iter->obj );
    if(iter->iter)
	IOObjectRelease( iter->iter );

    free(iter);
    return kIOReturnSuccess;
}

IOReturn
IOUSBIteratorNext(IOUSBIteratorRef iter)
{
    while(true) {
        if(iter->obj)
            IOObjectRelease( iter->obj );
        iter->obj = IOIteratorNext( iter->iter);
        if(iter->obj) {
            IOUSBDeviceDescriptor desc;
            IOReturn kr;
            kr = IOUSBGetDeviceDescriptor(iter, &desc, sizeof(desc));
            if(kIOReturnSuccess != kr)
                continue;
            if(iter->match.usbClass != kIOUSBAnyClass &&
                iter->match.usbClass != desc.deviceClass)
                continue;
            if(iter->match.usbSubClass != kIOUSBAnySubClass &&
                iter->match.usbSubClass != desc.deviceSubClass)
                continue;
            if(iter->match.usbProtocol != kIOUSBAnyProtocol &&
                iter->match.usbProtocol != desc.protocol)
                continue;
            if(iter->match.usbVendor != kIOUSBAnyVendor &&
                iter->match.usbVendor != OSReadLittleInt16(&desc.vendor, 0))
                continue;
            if(iter->match.usbProduct != kIOUSBAnyProduct &&
               iter->match.usbProduct != OSReadLittleInt16(&desc.product, 0))
                continue;

            return kIOReturnSuccess;
        }
        else
            return kIOReturnNoDevice;
    }
}

IOReturn
IOUSBGetDeviceDescriptor(IOUSBIteratorRef device,
                        IOUSBDeviceDescriptorPtr desc, UInt32 size)
{
    return getDescFromProps(device->obj,
		CFSTR("device descriptor"), desc, size);
}

IOReturn
IOUSBGetInterfaceDescriptor(IOUSBIteratorRef device,
                        IOUSBInterfaceDescriptorPtr desc, UInt32 size)
{
    return getDescFromProps(device->obj,
		CFSTR("interface descriptor"), desc, size);
}

IOReturn
IOUSBIsInterface(IOUSBIteratorRef device, int *isIntf)
{
    kern_return_t	kr;
    CFDictionaryRef	properties;
    CFDataRef		dataDesc;

    kr = IORegistryEntryCreateCFProperties(device->obj, &properties,
                                            kCFAllocatorDefault, kNilOptions);
    if (kr)
        return kr;
    if(!properties)
        return kIOReturnBadArgument;

    dataDesc = (CFDataRef) CFDictionaryGetValue(properties, 
				CFSTR("interface descriptor"));
    *isIntf = dataDesc != 0;

    CFRelease(properties);
    return kIOReturnSuccess;
}

IOReturn IOUSBNewDeviceRef(IOUSBIteratorRef enumer,
					IOUSBDeviceRef *device)
{
    IOReturn kr;
    IOUSBDeviceRef newDevice;

    newDevice = (IOUSBDeviceRef)malloc(sizeof(IOUSBDevice));
    if(!newDevice)
        return kIOReturnNoMemory;
    newDevice->_configList = NULL;
    newDevice->asyncPort = MACH_PORT_NULL;
    newDevice->cfSource = NULL;
    kr = IOServiceOpen( enumer->obj,
                    mach_task_self(),
                    kIOUSBDeviceConnectType, &(newDevice->kernDevice));
    if(kIOReturnSuccess == kr)
	kr = IOUSBGetDeviceDescriptor(enumer, &newDevice->_descriptor, 
						sizeof(IOUSBDeviceDescriptor));
    if(kIOReturnSuccess == kr) {
        UInt8 cNum;
        newDevice->_configList = (IOUSBConfiguration **)
                    malloc(sizeof(IOUSBConfiguration *) * newDevice->_descriptor.numConf);
        bzero(newDevice->_configList,
            sizeof(IOUSBConfiguration *) * newDevice->_descriptor.numConf);
        for (cNum = 0; cNum < newDevice->_descriptor.numConf; cNum++)
        {
            IOUSBConfigurationDescriptor	*config;
            IOUSBConfigurationDescriptor	justConfig;

            int				t;
            mach_msg_type_number_t	size;

            t = cNum;
            size = sizeof(justConfig);
            kr = io_connect_method_scalarI_structureO( newDevice->kernDevice,
                kUSBGetConfigDescriptor,
                &t, 1, (char *)&justConfig, &size);
            if (kr)
                break;
            size = USBToHostWord(justConfig.totalLength);
            config = (IOUSBConfigurationDescriptor *)malloc(size);
            kr = io_connect_method_scalarI_structureO( newDevice->kernDevice,
                kUSBGetConfigDescriptor,
                &t, 1, (char *)config, &size);
            if (kr)
                break;

            newDevice->_configList[cNum] = (IOUSBConfiguration *)
                                            malloc(sizeof(IOUSBConfiguration));
            newDevice->_configList[cNum]->descriptor = config;
            newDevice->_configList[cNum]->device = newDevice;
        }
    }
    if(kIOReturnSuccess != kr)
        IOUSBDisposeRef(newDevice);
    else
        *device = newDevice;

    return kr;
}

IOReturn IOUSBDisposeRef(IOUSBDeviceRef device)
{
    IOReturn kr;

    if(device->cfSource)
        CFRelease(device->cfSource);
    if(device->asyncPort) {
        mach_port_deallocate( mach_task_self(), device->asyncPort);
    }
    if(device->kernDevice)
    	kr = IOServiceClose(device->kernDevice);
    else
        kr = kIOReturnSuccess;

    if(kIOReturnSuccess == kr) {
        if(device->_configList) {
            UInt8 cNum;
            for (cNum = 0; cNum < device->_descriptor.numConf; cNum++) {
		if(device->_configList[cNum]) {
                    if(device->_configList[cNum]->descriptor)
                        free(device->_configList[cNum]->descriptor);
                    free(device->_configList[cNum]);
                }
            }
            free(device->_configList);
        }
        free(device);
    }
    return kr;
}

IOReturn IOUSBCreateAsyncPort(IOUSBDeviceRef device, mach_port_t *port)
{
    IOReturn kr;
    kr = IOCreateReceivePort(kOSAsyncCompleteMessageID,
                                                    &device->asyncPort);
    if((kr == kIOReturnSuccess) && port)
        *port = device->asyncPort;
    return kr;
}

mach_port_t IOUSBGetAsyncPort(IOUSBDeviceRef device)
{
    return device->asyncPort;
}

IOReturn IOUSBCreateAsyncEventSource(IOUSBDeviceRef device,
                                     CFRunLoopSourceRef *source)
{
    IOReturn res;
    CFMachPortRef cfPort;
    CFMachPortContext context;
    if(!device->asyncPort) {     
        res = IOUSBCreateAsyncPort(device, NULL);
        if(res != kIOReturnSuccess)
            return res;
    }

    context.version = 1;
    context.info = device;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;

    cfPort = CFMachPortCreateWithPort(NULL, device->asyncPort,
        (CFMachPortCallBack) IODispatchCalloutFromMessage, &context);
    if(!cfPort)
        return kIOReturnNoMemory;
    
    device->cfSource = CFMachPortCreateRunLoopSource(NULL, cfPort, 0);
    CFRelease(cfPort);
    if(!device->cfSource)
        return kIOReturnNoMemory;
    if(source)
        *source = device->cfSource;
    return kIOReturnSuccess;
}

CFRunLoopSourceRef IOUSBGetAsyncEventSource(IOUSBDeviceRef device)
{
    return device->cfSource;
}

IOReturn
IOUSBResetDevice(IOUSBDeviceRef device)
{
    kern_return_t		err;
    mach_msg_type_number_t	len = 0;

    err = io_connect_method_scalarI_scalarO( device->kernDevice, kUSBResetDevice,
                    NULL, 0, NULL, &len);

    return( err);
}


IOReturn IOUSBGetConfigDescriptor(IOUSBDeviceRef device, UInt8 configIndex,
                               IOUSBConfigurationDescriptorPtr *desc)
{
    if (configIndex < device->_descriptor.numConf) {
        *desc = device->_configList[configIndex]->descriptor;
        return(kIOReturnSuccess);
    }
    else
        return(kIOUSBConfigNotFound);
}

IOReturn
IOUSBSetConfiguration(IOUSBDeviceRef device, UInt8 config)
{
    kern_return_t		err;
    mach_msg_type_number_t	len = 0;
    int				t;

    t = config;
    err = io_connect_method_scalarI_scalarO( device->kernDevice, kUSBSetConfig,
                    &t, 1, NULL, &len);

    return( err);
}

IOReturn
IOUSBGetConfiguration(IOUSBDeviceRef device, UInt8 *config)
{
    kern_return_t		err;
    mach_msg_type_number_t	len = 1;

    err = io_connect_method_scalarI_scalarO( device->kernDevice, kUSBGetConfig,
                                             NULL, 0, (int *)config, &len);

    return( err);
}

IOUSBInterface *IOUSBGetInterface(IOUSBDeviceRef device,
					UInt8 	configIndex,
                                        UInt8 	interfaceIndex,
                                        UInt8 	alternateIndex)
{
    IOUSBConfigurationDescriptor	*config = 0;
    IOUSBInterfaceDescriptor	*id;
    IOUSBEndpointDescriptor	*ed;
    IOUSBInterface		*interface;
    UInt8			eNum;
    IOUSBInterfaceDescriptor	*end;

    if (configIndex >= device->_descriptor.numConf)
        return(0);

    config = device->_configList[configIndex]->descriptor;
    end = (void *)(((UInt8 *)config) + USBToHostWord(config->totalLength));
    id = (IOUSBInterfaceDescriptor *)
                        nextDescriptor((IOUSBDescriptorHeader *)config);
    while(id < end)
    {
        if(id->descriptorType == kUSBInterfaceDesc)
        {
            if((id->interfaceNumber) == interfaceIndex &&
               id->alternateSetting == alternateIndex)
            {
                interface = (IOUSBInterface *)malloc(sizeof(IOUSBInterface));
                interface->descriptor = id;
                interface->device = device;
                interface->config = device->_configList[configIndex];
                interface->endpoints = (IOUSBEndpoint **)
                    malloc(sizeof(IOUSBEndpoint*) * id->numEndpoints);
                ed = (IOUSBEndpointDescriptor *)id;
                for (eNum = 0; eNum < id->numEndpoints; eNum++)
                {
                    ed = (IOUSBEndpointDescriptor *)
                                    nextDescriptor((IOUSBDescriptorHeader *)ed);

                    // skip over descriptors of wrong type, eg. HID descriptors
                    while(ed->descriptorType != kUSBEndpointDesc) {
#if (DEBUGGING_LEVEL > 0)
                        IOLog("Got descriptor type %d looking for endpoint descriptors\n",
                          ed->descriptorType );
#endif
                        ed = (IOUSBEndpointDescriptor *)
                                        nextDescriptor((IOUSBDescriptorHeader *)ed);
                    }
                    interface->endpoints[eNum] = (IOUSBEndpoint *)
                                                malloc(sizeof(IOUSBEndpoint));
                    interface->endpoints[eNum]->descriptor = ed;
                    interface->endpoints[eNum]->number = ed->endpointAddress & 0x0F;
                    interface->endpoints[eNum]->interface = interface;
                    interface->endpoints[eNum]->direction =
                        ed->endpointAddress & 0x80 ? kUSBIn : kUSBOut;
                    interface->endpoints[eNum]->transferType =
                        ed->attributes & 0x03;
                    interface->endpoints[eNum]->maxPacketSize =
                        OSReadLittleInt16(&ed->maxPacketSizeLo, 0);
                    interface->endpoints[eNum]->interval =
                        ed->interval;
                }
                return(interface);
            }
        }

        id = (IOUSBInterfaceDescriptor *)
                                nextDescriptor((IOUSBDescriptorHeader *)id);
    }
    return(0);
}

// Finding the correct interface
IOUSBInterface *IOUSBFindNextInterface(IOUSBDeviceRef device,
                                IOUSBFindInterfaceRequest	*request)
{
    IOUSBConfigurationDescriptor	*cd;
    IOUSBInterfaceDescriptor	*id;
    IOUSBInterface		*interface;
    UInt8			numInterfaces;
    UInt8			busPowered, selfPowered, remoteWakeup;


    interface = IOUSBGetInterface(device, request->configIndex,
                             request->interfaceIndex,
                             request->alternateIndex);
    if (interface == 0) return (0);

    cd = interface->config->descriptor;
    id = interface->descriptor;
    busPowered = cd->attributes & kUSBAtrBusPowered ? 2 : 1;
    selfPowered = cd->attributes & kUSBAtrSelfPowered ? 2 : 1;
    remoteWakeup = cd->attributes & kUSBAtrRemoteWakeup ? 2 : 1;

    // increment the counters
    numInterfaces = interface->config->descriptor->numInterfaces;

    request->configIndex += (request->interfaceIndex + 1) / numInterfaces;
    request->interfaceIndex = (request->interfaceIndex + 1) % numInterfaces;

    // check the request parameters
    if (request->theClass != 0 && request->theClass != id->interfaceClass)
        interface = 0;		// this is not it
    if (request->subClass != 0 && request->subClass != id->interfaceSubClass)
        interface = 0;		// this is not it
    if (request->protocol != 0 && request->protocol != id->interfaceProtocol)
        interface = 0;		// this is not it
    if (request->busPowered != 0 && request->busPowered != busPowered)
        interface = 0;		// this is not it
    if (request->selfPowered != 0 && request->selfPowered != selfPowered)
        interface = 0;		// this is not it
    if (request->remoteWakeup != 0 && request->remoteWakeup != remoteWakeup)
        interface = 0;		// this is not it
    if (request->maxPower != 0 && request->maxPower != cd->maxPower)
        interface = 0;		// this is not it

    if (interface == 0)
        return(0);

    request->theClass = id->interfaceClass;
    request->subClass = id->interfaceSubClass;
    request->protocol = id->interfaceProtocol;
    request->busPowered = busPowered;
    request->selfPowered = selfPowered;
    request->remoteWakeup = remoteWakeup;
    request->maxPower = cd->maxPower;

    return(interface);
}


void IOUSBDisposeInterface(IOUSBInterface * interface)
{
    if(interface->endpoints) {
	int eNum;
        for (eNum = 0; eNum < interface->descriptor->numEndpoints; eNum++)
        {
            if(interface->endpoints[eNum])
                free(interface->endpoints[eNum]);
        }
        free(interface->endpoints);
    }
    free(interface);
}

static IOReturn doControlRequest(io_connect_t device, int pipe,
	UInt8 bmreqtype, UInt8 request, UInt16 wValue, UInt16 wIndex,
	void *buf, UInt16 *size)
{
    kern_return_t	err;
    if(*size > sizeof(io_struct_inband_t)) {
        DevReqOOL reqOOL;
        mach_msg_type_number_t outSize;
        reqOOL.buf = buf;
        reqOOL.sizeIn = *size;
        reqOOL.pipe = pipe;
        reqOOL.wValue= wValue;
        reqOOL.wIndex = wIndex;
        reqOOL.bmreqtype = bmreqtype;
        reqOOL.request = request;
        if( (bmreqtype & (kUSBRqDirnMask << kUSBRqDirnShift)) ==
                                            (kUSBOut << kUSBRqDirnShift) ) {
            outSize = 0;
            err = io_connect_method_structureI_structureO(
                    device, kUSBControlReqOutOOL,
                    (char *)&reqOOL, sizeof(reqOOL), NULL, &outSize);
        }
        else {
            mach_msg_type_number_t reqSize = *size;
            outSize = sizeof(reqSize);
            err = io_connect_method_structureI_structureO(
                    device, kUSBControlReqInOOL,
                    (char *)&reqOOL, sizeof(reqOOL), (char *)&reqSize, &outSize);
            if(kIOReturnSuccess == err)
                *size = reqSize;
        }
    }
    else {
        int	in[4];

        in[0] = pipe;
        in[1] = bmreqtype | (request << 8); // Pack to save arguments in kernel method.
        in[2] = wValue;
        in[3] = wIndex;
        if( (bmreqtype & (kUSBRqDirnMask << kUSBRqDirnShift)) ==
                                            (kUSBOut << kUSBRqDirnShift) ) {
            err = io_connect_method_scalarI_structureI(
                    device, kUSBControlReqOut,
                    in, 4, (char *)buf, *size);
        }
        else {
            mach_msg_type_number_t reqSize = *size;
            err = io_connect_method_scalarI_structureO(
                    device, kUSBControlReqIn,
                    in, 4, (char *)buf, &reqSize);
            if(kIOReturnSuccess == err)
                *size = reqSize;
        }
    }
    return err;
}

static IOReturn doControlRequestAsync(IOUSBDeviceRef device, int pipe,
        UInt8 bmreqtype, UInt8 request, UInt16 wValue, UInt16 wIndex,
        void *buf, UInt16 size, IOAsyncCallback1 callback, void *refcon)
{
    kern_return_t		err;
    DevReqOOL 			reqOOL;
    natural_t			asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t 	outSize;
    int 			func;
    
    reqOOL.buf = buf;
    reqOOL.sizeIn = size;
    reqOOL.pipe = pipe;
    reqOOL.wValue= wValue;
    reqOOL.wIndex = wIndex;
    reqOOL.bmreqtype = bmreqtype;
    reqOOL.request = request;
    outSize = 0;
    
    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    if( (bmreqtype & (kUSBRqDirnMask << kUSBRqDirnShift)) ==
                                        (kUSBOut << kUSBRqDirnShift) ) {
        func = kUSBControlReqOutOOL;
    }
    else {
        func = kUSBControlReqInOOL;
    }
    err = io_async_method_structureI_structureO(
            device->kernDevice, device->asyncPort, asyncRef, kIOAsyncCalloutCount,
                func, (char *)&reqOOL, sizeof(reqOOL), NULL, &outSize);
    return err;
}

// Generic device request (to pipe 0)
// wValue and wIndex are host-endian
IOReturn IOUSBDeviceRequest(IOUSBDeviceRef device, UInt8 bmreqtype,
        UInt8 request, UInt16 wValue, UInt16 wIndex, void *buf, UInt16 *size)
{
    return doControlRequest(device->kernDevice, 0,
        bmreqtype, request, wValue, wIndex, buf, size);
}

// Generic device request (to pipe 0)
// wValue and wIndex are host-endian
IOReturn IOUSBDeviceRequestAsync(IOUSBDeviceRef device, UInt8 bmreqtype,
        UInt8 request, UInt16 wValue, UInt16 wIndex, void *buf, UInt16 size,
                                 IOAsyncCallback1 callback, void *refcon)
{
    return doControlRequestAsync(device, 0,
        bmreqtype, request, wValue, wIndex, buf, size, callback, refcon);
}

// Generic control request (to a control pipe)
// wValue and wIndex are host-endian
IOReturn IOUSBControlRequest(IOUSBPipeRef pipe, UInt8 bmreqtype,
        UInt8 request, UInt16 wValue, UInt16 wIndex, void *buf, UInt16 *size)
{
    return doControlRequest(pipe->kernDevice, pipe->kernPipe,
	bmreqtype, request, wValue, wIndex, buf, size);
}

IOReturn IOUSBControlRequestAsync(IOUSBPipeRef pipe, UInt8 bmreqtype,
        UInt8 request, UInt16 wValue, UInt16 wIndex, void *buf, UInt16 size,
                                  IOAsyncCallback1 callback, void *refcon)
{
    return doControlRequestAsync(pipe->device, pipe->kernPipe,
        bmreqtype, request, wValue, wIndex, buf, size, callback, refcon);
}

IOReturn IOUSBOpenPipe(IOUSBDeviceRef device, IOUSBEndpoint * ep,
						IOUSBPipeRef *pipe)
{
    kern_return_t		err;
    mach_msg_type_number_t	len = 1;
    IOUSBPipeRef		newPipe;
    IOUSBInterfaceDescriptor *	idesc = ep->interface->descriptor;
    int				in[4];
    int				i;

    newPipe = (IOUSBPipeRef)malloc(sizeof(IOUSBUserPipe));
    if(!newPipe)
        return kIOReturnNoMemory;

    newPipe->device = device;
    newPipe->kernDevice = device->kernDevice;

    // Pass in enough info for the kernel to find its equivalent ep pointer,
    // We have to turn pointers into offsets so they can be converted back for the
    // kernel's data structures
    for(i=0; i< device->_descriptor.numConf; i++) {
        if(ep->interface->config == device->_configList[i]) {
            in[0] = i;
            break;
        }
    }
    in[1] = idesc->interfaceNumber;
    in[2] = idesc->alternateSetting;
    for(i=0; i<idesc->numEndpoints; i++) {
        if(ep == ep->interface->endpoints[i]) {
            in[3] = i;
            break;
        }
    }

    err = io_connect_method_scalarI_scalarO( device->kernDevice, kUSBOpenPipe,
                           in, 4, &(newPipe->kernPipe), &len);

    if(kIOReturnSuccess == err)
        *pipe = newPipe;
    else
        free(newPipe);

    return( err);
}

IOReturn IOUSBClosePipe(IOUSBPipeRef pipe)
{
    kern_return_t		err;
    mach_msg_type_number_t	len = 0;

    err = io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBClosePipe,
                                (int *)&(pipe->kernPipe), 1, NULL, &len);
    if(kIOReturnSuccess == err)
        free(pipe);
    return( err);
}

IOReturn IOUSBReadPipe(IOUSBPipeRef pipe, void *buf, UInt32 *size)
{
    kern_return_t	err;

    if(*size < sizeof(io_struct_inband_t)) {
        err = io_connect_method_scalarI_structureO( pipe->kernDevice,
            kUSBReadPipe, (int *)&(pipe->kernPipe), 1,
            (char *)buf, (unsigned int *)size);
    }
    else {
        int	in[3];
    	mach_msg_type_number_t	len = 1;
        in[0] = (int)pipe->kernPipe;
        in[1] = (int)buf;
    	in[2] = *size;
        err = io_connect_method_scalarI_scalarO( pipe->kernDevice,
                            kUSBReadPipeOOL, in, 3, (int *)size, &len);
    }

    return( err);
}

IOReturn IOUSBReadPipeAsync(IOUSBPipeRef pipe, void *buf, UInt32 size,
                            IOAsyncCallback1 callback, void *refcon)
{
    kern_return_t		err;
    int				in[3];
    natural_t			asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t	len = 0;
    
    in[0] = (int)pipe->kernPipe;
    in[1] = (int)buf;
    in[2] = size;

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    err = io_async_method_scalarI_scalarO( pipe->kernDevice,
            pipe->device->asyncPort, asyncRef, kIOAsyncCalloutCount,
            kUSBReadPipeOOL, in, 3, NULL, &len);

    return( err);
}

IOReturn IOUSBWritePipe(IOUSBPipeRef pipe, void *buf, UInt32 size)
{
    kern_return_t	err;

    if(size < sizeof(io_struct_inband_t)) {
        err = io_connect_method_scalarI_structureI( pipe->kernDevice, kUSBWritePipe,
            (int *)&(pipe->kernPipe), 1,
            (char *)buf, size);
    }
    else {
        int	in[3];
    	mach_msg_type_number_t	len = 0;
        in[0] = (int)pipe->kernPipe;
        in[1] = (int)buf;
    	in[2] = size;
        err = io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBWritePipeOOL,
            in, 3, NULL, &len);
    }
    return( err);
}

IOReturn IOUSBWritePipeAsync(IOUSBPipeRef pipe, void *buf, UInt32 size,
                             IOAsyncCallback1 callback, void *refcon)
{
    kern_return_t		err;
    int				in[3];
    natural_t			asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t	len = 0;
    
    in[0] = (int)pipe->kernPipe;
    in[1] = (int)buf;
    in[2] = size;
    
    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    err = io_async_method_scalarI_scalarO( pipe->kernDevice,
            pipe->device->asyncPort, asyncRef, kIOAsyncCalloutCount,
            kUSBWritePipeOOL, in, 3, NULL, &len);
    return( err);
}

IOReturn IOUSBGetPipeStatus(IOUSBPipeRef pipe)
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBGetPipeStatus,
                                (int *)&(pipe->kernPipe), 1, NULL, &len);
}

IOReturn IOUSBAbortPipe(IOUSBPipeRef pipe)
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBAbortPipe,
                                (int *)&(pipe->kernPipe), 1, NULL, &len);
}

IOReturn IOUSBResetPipe(IOUSBPipeRef pipe)
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBResetPipe,
                                (int *)&(pipe->kernPipe), 1, NULL, &len);
}

IOReturn IOUSBSetPipeIdle(IOUSBPipeRef pipe)
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBSetPipeIdle,
                                (int *)&(pipe->kernPipe), 1, NULL, &len);
}

IOReturn IOUSBSetPipeActive(IOUSBPipeRef pipe)
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBSetPipeActive,
                                (int *)&(pipe->kernPipe), 1, NULL, &len);
}

IOReturn IOUSBClearPipeStall(IOUSBPipeRef pipe)
{
    mach_msg_type_number_t	len = 0;

    return io_connect_method_scalarI_scalarO( pipe->kernDevice, kUSBClearPipeStall,
                                (int *)&(pipe->kernPipe), 1, NULL, &len);
}


/**********************************************************************
 **
 ** UTILITY FUNCTIONS
 **
 **********************************************************************/
// Should be moved to a generic library
UInt8 USBMakeBMRequestType(UInt8	rqDirection,
                           UInt8	rqType,
                           UInt8	rqRecipient)
{
    UInt8 rq = 0xff;
    do
    {
        if(rqDirection == kUSBNone) rqDirection = kUSBOut;

        if(rqDirection != kUSBOut && rqDirection != kUSBIn) break;

        if(rqRecipient > kUSBOther) break;

        rq = rqRecipient +
            (rqType << kUSBRqTypeShift) +
            (rqDirection << kUSBRqDirnShift);
    } while(false);

    return(rq);
}

static IOReturn
getDescFromProps(io_object_t device,
                    CFStringRef propname, void * desc, UInt32 size)
{
    kern_return_t	kr;
    CFDictionaryRef	properties;
    CFDataRef		dataDesc;

    kr = IORegistryEntryCreateCFProperties(device, &properties,
                                            kCFAllocatorDefault, kNilOptions);
    if (kr)
        return kr;
    if(!properties)
        return kIOReturnBadArgument;

    dataDesc = (CFDataRef) CFDictionaryGetValue(properties,propname);
    if(0 == dataDesc)
        return kIOReturnNoDevice;
    if(size > CFDataGetLength(dataDesc))
        size = CFDataGetLength(dataDesc);
    bcopy(CFDataGetBytePtr(dataDesc), desc, size);

    CFRelease(properties);

    return(kr);
}

IOUSBDescriptorHeader *nextDescriptor(IOUSBDescriptorHeader *desc)
{
    UInt8 *next = (UInt8 *)desc;
    UInt8 length = next[0];
    next = &next[length];
    return((IOUSBDescriptorHeader *)next);
}
