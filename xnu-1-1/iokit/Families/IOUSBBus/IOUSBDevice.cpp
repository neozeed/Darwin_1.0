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
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>

#include "IOUSBUserClient.h"
#include "IOUSBHub.h"

#define super IOUSBNub

static int devdebug = 0;
#define DEBUGLOG if (devdebug) kprintf

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOUSBDevice, IOUSBNub)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOUSBDevice::setPort(IOUSBHubPort *port)
{
    _port = port;
}

bool IOUSBDevice::init(OSDictionary * propTable)
{
    if(!IOUSBNub::init(propTable))
	return false;
    if(_descriptor.numConf) {
        _configList = (UInt8 **)IOMalloc(sizeof(UInt8 *) * _descriptor.numConf);
        if(!_configList)
            return false;
        bzero(_configList, sizeof(UInt8 *) * _descriptor.numConf);
    }
    return true;
}

void IOUSBDevice::free()
{
    if(_configList) {
	int i;
        for(i=0; i<_descriptor.numConf; i++) {
            if(_configList[i]) {
                int len = OSReadLittleInt16(&((IOUSBConfigurationDescriptor *)_configList[i])->totalLength, 0);
                IOFree(_configList[i], len+2);
		_configList[i] = NULL;
            }
	}
        IOFree(_configList, sizeof(UInt8 *) * _descriptor.numConf);
    }
    IOUSBNub::free();
}

bool IOUSBDevice::attach(IOService *provider)
{
    IOReturn err;
    char name[128];

    if( !IOUSBNub::attach(provider))
        return (false);
    // Set controller if we weren't given one earlier.
    if(_controller == NULL)
	_controller = OSDynamicCast(IOUSBController, provider);
    if(_controller == NULL)
	return false;

    // Don't do this until we have a controller
    _endpointZero.length = sizeof(_endpointZero);
    _endpointZero.descriptorType = kUSBEndpointDesc;
    _endpointZero.endpointAddress = 0;
    _endpointZero.attributes = kUSBControl;
    OSWriteLittleInt16(&_endpointZero.maxPacketSizeLo, 0, _descriptor.maxPacketSize);	
    _endpointZero.interval = 0;

    _pipeZero = IOUSBPipe::toEndpoint(&_endpointZero, _speed, _address, _controller);

    if(_descriptor.prodIdx) {
        err = GetStringDescriptor(_descriptor.prodIdx, name, sizeof(name));
	if(err == kIOReturnSuccess) {
            setName(name);
            setProperty("USB Product Name", name);
	}
    }
    if(_descriptor.manuIdx) {
        err = GetStringDescriptor(_descriptor.manuIdx, name, sizeof(name));
        if(err == kIOReturnSuccess) {
            setProperty("USB Vendor Name", name);
	}
    }
    if(_descriptor.serialIdx) {
        err = GetStringDescriptor(_descriptor.serialIdx, name, sizeof(name));
        if(err == kIOReturnSuccess) {
            setProperty("USB Serial Number", name);
        }
    }
    return true;
}

// Stop all activity, reset device, restart.
IOReturn IOUSBDevice::resetDevice()
{
    IOReturn err;

    if(_pipeZero) {
        _pipeZero->release();
        _pipeZero = NULL;
    }
    err = _port->resetDevice();

    // Recreate pipe 0 object
    if(err == kIOReturnSuccess) {
        _pipeZero = IOUSBPipe::toEndpoint(&_endpointZero, _speed, _address, _controller);
        if(!_pipeZero)
            err = kIOReturnNoMemory;
    }

    return err;
}

/******************************************************
 * Helper Methods
 ******************************************************/

const IOUSBConfigurationDescriptor *
IOUSBDevice::findConfig(UInt8 configValue, int *configIndex)
{
    int i;
    const IOUSBConfigurationDescriptor *cd = NULL;

    for(i = 0; i < _descriptor.numConf; i++) {
        cd = getFullConfigurationDescriptor(i);
        if(!cd)
            continue;
        if(cd->configValue == configValue)
            break;
    }
    if(cd && configIndex)
	*configIndex = i;

    return cd;
}

// Finding the correct interface
/*
 * findNextInterface
 * This method should really be rewritten to use iterators or
 * be broken out into more functions without a request structure.
 * Or even better, make the interfaces and endpoint objects that
 * have their own methods for this stuff.
 *
 * returns:
 *   next interface matching criteria.  0 if no matches
 */
IOUSBInterface *
IOUSBDevice::findNextInterface(IOUSBInterface *current,
				FindInterfaceRequest *request)
{
    const IOUSBConfigurationDescriptor *cd;
    const IOUSBInterfaceDescriptor *	id;
    const IOUSBDescriptorHeader *	pos = NULL;
    bool				found;
    int					configIndex;
    if(current == NULL) {
	configIndex = 0;
        cd = getFullConfigurationDescriptor(configIndex);
        if(!cd)
            return NULL;
        pos = (IOUSBDescriptorHeader *)cd;
    }
    else {
        cd = findConfig(current->getConfigValue(), &configIndex);
        if(!cd)
            return NULL;
        pos = (IOUSBDescriptorHeader *)cd;
        while((pos = findNextDescriptor(pos, kUSBInterfaceDesc))) {
            if(bcmp(pos, current->interfaceDescriptor(),
                          sizeof(IOUSBInterfaceDescriptor)) == 0) {
                break;
            }
        }
    }
    while (configIndex < _descriptor.numConf)
    {
        pos = findNextDescriptor(pos, kUSBInterfaceDesc);
        if(pos == NULL) {
            // End of that configuration
            configIndex++;
            if(configIndex >= _descriptor.numConf)
                break;

            cd = getFullConfigurationDescriptor(configIndex);
            continue;
	}
        id = (const IOUSBInterfaceDescriptor *)pos;

        // check the request parameters
	found = true;
        if (request->theClass != 0 && request->theClass != id->interfaceClass)
            found = false;		// this is not it
        if (request->subClass != 0 && request->subClass != id->interfaceSubClass)
            found = false;		// this is not it
        if (request->protocol != 0 && request->protocol != id->interfaceProtocol)
            found = false;		// this is not it
        if (request->maxPower != 0 && request->maxPower < cd->maxPower)
            found = false;		// draws too much power
        if (id->alternateSetting != 0)
            found = false;		// Only want main setting

        if (found)
        {
            request->theClass = id->interfaceClass;
            request->subClass = id->interfaceSubClass;
            request->protocol = id->interfaceProtocol;
            request->maxPower = cd->maxPower;
            request->busPowered = cd->attributes & kUSBAtrBusPowered ? 2 : 1;
            request->selfPowered = cd->attributes & kUSBAtrSelfPowered ? 2 : 1;
            request->remoteWakeup = cd->attributes & kUSBAtrRemoteWakeup ? 2 : 1;
             return(GetInterface(id, cd));
        }
    }
    return(0);
}

const IOUSBConfigurationDescriptor *IOUSBDevice::getFullConfigurationDescriptor(UInt8 index)
{
    IOReturn err;

    if(_configList[index] == NULL) {
        int len;
        IOUSBConfigurationDescriptor temp;
        err = GetConfigDescriptor(index, &temp, sizeof(temp));
        if (err) {
            DEBUGLOG("getFullConfigurationDescriptor: Error 0x%x getting configuration descriptor\n", err);
            return(0);
        }
        len = OSReadLittleInt16(&temp.totalLength, 0);
        _configList[index] = (UInt8 *)IOMalloc(len + 2);
        if(_configList[index] == NULL)
            return 0;
        err = GetConfigDescriptor(index, _configList[index], len);
        if (err) {
            DEBUGLOG("getFullConfigurationDescriptor: Error %d getting full configuration\n", err);
            return(0);
        }
        *(_configList[index]+len) = 0;	// A Known ~Elephant.
        *(_configList[index]+len+1) = 0;// A Known ~Elephant.
    }
    return (IOUSBConfigurationDescriptor *)_configList[index];
}

/*
 * GetConfigDescriptor:
 *	In: pointer to buffer for config descriptor, length to get
 * Assumes: desc is has enough space to put it all
 */
IOReturn IOUSBDevice::GetConfigDescriptor(UInt8 configIndex,
                                          void *desc, int len)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    DEBUGLOG("********** GET CONFIG DESCRIPTOR (%d)**********\n", len);

    /*
     * with config descriptors, the device will send back all descriptors,
     * if the request is big enough.
     */

    request.rqDirection = kUSBIn;
    request.rqType = kUSBStandard;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqGetDescriptor;
    OSWriteLittleInt16(&request.wValue, 0, (kUSBConfDesc << 8) + configIndex);
    OSWriteLittleInt16(&request.wIndex, 0, 0);
    OSWriteLittleInt16(&request.wLength, 0, len);
    request.pData = desc;

    err = deviceRequest(&request);

    if (err)
    {
        DEBUGLOG("%s: error getting device config descriptor. err=0x%x\n", getName(), err);
    }

    return(err);
}

IOReturn IOUSBDevice::SetConfiguration(UInt8 configNumber)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    DEBUGLOG("********** SET CONFIGURATION TO %d **********\n", configNumber);

    request.rqDirection = kUSBOut;
    request.rqType = kUSBStandard;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqSetConfig;
    OSWriteLittleInt16(&request.wValue, 0, configNumber);
    OSWriteLittleInt16(&request.wIndex, 0, 0);
    OSWriteLittleInt16(&request.wLength, 0, 0);
    request.pData = 0;

    err = deviceRequest(&request);

    if (err)
    {
        DEBUGLOG("%s: error setting config. err=%d\n", getName(), err);
    }

    return(err);
}

IOUSBInterface * IOUSBDevice::GetInterface(const IOUSBInterfaceDescriptor *interface,
                                                    const IOUSBConfigurationDescriptor *config)
{
    OSDictionary *		propTable = 0;
    OSArray *			epArray = 0;
    IOUSBInterface *		newDevice = 0;
    OSData *			data;
    OSNumber *			offset;
    int				i;
    char			location[8];
    OSIterator *		clients;
    /*
     * First see if the interface has already been created,
     * ie. there's already one with the given interface descriptor and config descriptor
     */
    clients = getClientIterator();
    if(clients) {
        OSObject *next;
        while((next = clients->getNextObject())) {
            IOUSBInterface *testIt = OSDynamicCast(IOUSBInterface, next);
            if(testIt) {
                if(config->configValue == testIt->getConfigValue() &&
                   bcmp(interface, testIt->interfaceDescriptor(), sizeof(IOUSBInterfaceDescriptor)) == 0) {
                    newDevice = testIt;
                    break;
		}
            }
	}
	clients->release();
    }
    if(newDevice) {
	newDevice->retain();
	return newDevice;
    }
    do {
        propTable = OSDictionary::withCapacity(13);
        if (!propTable)
            continue;

        data = OSData::withBytes( &_descriptor, sizeof( _descriptor ));
        if (data) {
            propTable->setObject("device descriptor", data);
            data->release();
        }
        offset = OSNumber::withNumber(interface->interfaceClass, 8);
        if(offset) {
            propTable->setObject("class", offset);
            offset->release();
        }
        offset = OSNumber::withNumber(interface->interfaceSubClass, 8);
        if(offset) {
            propTable->setObject("subClass", offset);
            offset->release();
        }
        offset = OSNumber::withNumber(interface->interfaceProtocol, 8);
        if(offset) {
            propTable->setObject("protocol", offset);
            offset->release();
        }
        offset = OSNumber::withNumber(OSReadLittleInt16(&_descriptor.vendor, 0), 16);
        if(offset) {
            propTable->setObject("vendor", offset);
            offset->release();
        }
        offset = OSNumber::withNumber(OSReadLittleInt16(&_descriptor.product, 0), 16);
        if(offset) {
            propTable->setObject("product", offset);
            offset->release();
        }
        offset = OSNumber::withNumber(OSReadLittleInt16(&_descriptor.devRel, 0), 16);
        if(offset) {
            propTable->setObject("version", offset);
            offset->release();
        }
        data = OSData::withBytes(&_address, sizeof( _address ));
        if (data) {
            propTable->setObject("device address", data);
            data->release();
        }
        data = OSData::withBytes( config,
                                  sizeof( *config ));
        if (data) {
            propTable->setObject("configuration descriptor", data);
            data->release();
        }
        data = OSData::withBytes( interface,
                                   sizeof(*interface));
        if (data) {
            propTable->setObject("interface descriptor", data);
            data->release();
        }
        data = OSData::withBytes(&_busPowerAvailable, sizeof(_busPowerAvailable));
        if (data) {
            propTable->setObject("bus power available", data);
            data->release();
        }
        data = OSData::withBytes(&_speed, sizeof(_speed));
        if (data) {
            propTable->setObject("low speed device", data);
            data->release();
        }

        epArray = OSArray::withCapacity(interface->numEndpoints);
        if (!epArray)
            continue;
        IOUSBEndpointDescriptor *ep = (IOUSBEndpointDescriptor *)interface;
        for(i=0; i<interface->numEndpoints; i++) {
            ep = (IOUSBEndpointDescriptor *)findNextDescriptor(ep, kUSBEndpointDesc);
            if(!ep)
		break;
            data = OSData::withBytes(ep, sizeof(IOUSBEndpointDescriptor));
            if (data) {
                epArray->setObject(i, data);
                data->release();
            }
	}
        propTable->setObject("Endpoints", epArray);
        epArray->release();

        newDevice = createInterface(propTable, config, interface);

        if (!newDevice )
            continue;

        propTable->release();	// done with it after init
        propTable = 0;

        sprintf( location, "%x", _address );
        newDevice->setLocation(location);
        if (!newDevice->attach(this))
            break;

        return(newDevice);

    } while (false);

    if (propTable)
        propTable->release();
    if (epArray)
	epArray->release();
    if (newDevice)
        newDevice->release();

    return(NULL);
}

IOUSBInterface *
IOUSBDevice::createInterface(OSDictionary *table,
                   const IOUSBConfigurationDescriptor *config,
                                        const IOUSBInterfaceDescriptor *interface)
{
    IOUSBInterface *newIface;

    newIface = new IOUSBInterface;
    if(!newIface)
	return NULL;
    if(!newIface->init(table, config, interface)) {
        newIface->release();
        newIface = NULL;
    }
    return newIface;
}

// Copy data into supplied buffer, up to 'len' bytes.
IOReturn IOUSBDevice::getConfigurationDescriptor(UInt8 configValue, void *data, UInt32 len)
{
    unsigned int toCopy;
    const IOUSBConfigurationDescriptor *cd;
    cd = findConfig(configValue);
    if(!cd)
        return kIOUSBConfigNotFound;

    toCopy = OSReadLittleInt16(&cd->totalLength, 0);
    if(len < toCopy)
	toCopy = len;
    bcopy(cd, data, toCopy);
    return kIOReturnSuccess;
}


/**
 ** IOUserClient methods
 **/

IOReturn IOUSBDevice::newUserClient(  task_t		owningTask,
                                      void * 		/* security_id */,
                                      UInt32  		type,
                                      IOUserClient **	handler )

{
    IOReturn			err = kIOReturnSuccess;
    IOUSBDeviceUserClient *	client;

    if( type != kIOUSBDeviceConnectType)
        return( kIOReturnBadArgument);

    client = IOUSBDeviceUserClient::withTask(owningTask);

    if( !client || (false == client->attach( this )) ||
        (false == client->start( this )) ) {
        if(client) {
            client->detach( this );
            client->release();
        }
        err = kIOReturnNoMemory;
    }

    *handler = client;
    return( err );
}


