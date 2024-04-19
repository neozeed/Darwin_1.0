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

#include "IOUSBHub.h"
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>

#define super IOService
#define self this
#define DEBUGGING_LEVEL 0
#define DEBUGLOG kprintf

UInt32 GetErrataBits(IOUSBDeviceDescriptor *desc);

/* private API */
static void callStartHandler(void *hub)
{
    ((IOUSBHub *) hub)->StartHandler();
}

OSDefineMetaClassAndStructors(IOUSBHub, IOService)

bool IOUSBHub::init( OSDictionary * propTable )
{
    if( !super::init(propTable))
        return (false);

    _numCaptive = 0;

    return(true);
}

bool IOUSBHub::start(IOService * provider)
{
    IOReturn			err = 0;
    IOUSBDevice::FindInterfaceRequest	req;
    IOThread 			ioThread;
    
    if( !super::start(provider))
        return (false);

    // remember my device
    _device		= (IOUSBDevice *) provider;
    _address		= _device->address();
    _bus		= _device->bus();

    if (!_device->open(this)) return false;
    
    do {
        IOLog("%s: USB Generic Hub @ %d\n", getName(), _address);

        // Find the first config/interface
        if (_device->deviceDescriptor()->numConf < 1)
        {
            IOLog("%s: no hub configurations\n", getName());
            break;
        }

        req.theClass = kUSBHubClass;
	req.subClass = req.protocol = 0;
        req.maxPower = 0;
        if ((_hubInterface = _device->findNextInterface(NULL, &req)) == 0)
        {
            IOLog("%s: no interface found\n", getName());
            break;
        }
        _busPowered = req.busPowered == 2;	//FIXME
        _selfPowered = req.selfPowered == 2;	//FIXME

        if( !(_busPowered || _selfPowered) )
        {
            IOLog("%s: illegal device config - no power\n", getName());
            break;
        }

        // set my configuration

        if ((err = _device->SetConfiguration(_hubInterface->getConfigValue())))
            break;

        // Get the hub descriptor
        if ((err = GetHubDescriptor(&_hubDescriptor)))
            break;

	// Setup for reading status pipe
        _buffer = IOBufferMemoryDescriptor::withCapacity(8, kIODirectionIn);

        if (!_hubInterface->open(this))
            break;

        IOUSBFindEndpointRequest request;
        request.type = kUSBInterrupt;
        request.direction = kUSBIn;
        _interruptPipe = _hubInterface->findNextPipe(NULL, &request);

        if(!_interruptPipe)
            return false;

        // prepare the ports
	UnpackPortFlags();
        CountCaptivePorts();
#if (DEBUGGING_LEVEL > 0)
        PrintHubDescriptor(&_hubDescriptor);
#endif
        if (!CheckPortPowerRequirements())	continue;
        if (!AllocatePortMemory())		continue;
        if (!StartPorts())			continue;
        ioThread = IOCreateThread(callStartHandler, this);
        if (ioThread == NULL) 			continue;
                
        return(true);

    } while (false);

    IOLog("%s: aborting startup.  err = %d\n", getName(), err);

    _device->close(this);
    stop(provider);
    
    return(false);
}

void IOUSBHub::stop(IOService * provider)
{
    // stop/close all ports
    // deallocate ports
    StopPorts();

    if (_buffer) {
        _buffer->release();
	_buffer = 0;
    }
    if(_hubInterface) {
        _hubInterface->close(this);
        _hubInterface->release();
        _hubInterface = 0;
    }
    super::stop(provider);
}

/**********************************************************************
 **
 ** HUB FUNCTIONS
 **
 **********************************************************************/
void IOUSBHub::UnpackPortFlags(void)
{
    // ERIC FIXME?- I don't think the first +1 should be there.
    int numFlags = ((_hubDescriptor.numPorts + 1) / 8) + 1;
    int i;

    for(i = 0; i < numFlags; i++)
    {
        _hubDescriptor.pwrCtlPortFlags[i] =
        			_hubDescriptor.removablePortFlags[numFlags+i];
        _hubDescriptor.removablePortFlags[numFlags+i] = 0;
    }
}

void IOUSBHub::CountCaptivePorts(void)
{
    int 		portMask = 2;
    int 		portByte = 0;
    int			currentPort;


    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        /* determine if the port is a captive port */
        if ((_hubDescriptor.removablePortFlags[portByte] & portMask) != 0)
            _numCaptive++;		// save this for power calculations

        portMask <<= 1;
        if(portMask > 0x80)
        {
            portMask = 1;
            portByte++;
        }
    }
}


/*
 ExtPower   Good      off

Bus  Self

0     0     Illegal config
1     0     Always 100mA per port
0     1     500mA     0 (dead)
1     1     500      100
*/
bool IOUSBHub::CheckPortPowerRequirements(void)
{
    IOReturn	err = kIOReturnSuccess;
    /* Note hub current in units of 1mA, everything else in units of 2mA */
    UInt32	hubPower = _hubDescriptor.hubCurrent/2;
    UInt32	busPower = _device->busPowerAvailable();
    UInt32	powerAvailForPorts, powerNeededForPorts;
    bool	startExternal;

    do
    {
        if (hubPower > busPower)
        {
            IOLog("%s: hub needs more power than available", getName());
            err = kIOReturnNoPower;
        }

        powerAvailForPorts = busPower - hubPower;
        /* we minimally need make available 100mA per non-captive port */
        powerNeededForPorts =
            (_hubDescriptor.numPorts - _numCaptive) * kUSB100mA;
        _busPowerGood = powerAvailForPorts >= powerNeededForPorts;
        if(_numCaptive > 0)
        {
            if(_busPowerGood)
                _powerForCaptive =
                    (powerAvailForPorts - powerNeededForPorts) / _numCaptive;
            else
                _powerForCaptive = powerAvailForPorts / _numCaptive;
        }

        if( (_errataBits & kErrataCaptiveOK) != 0)
            _powerForCaptive = kUSB100mAAvailable;

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("%s: power:\n", getName());
        DEBUGLOG("\tbus power available = %ldmA\n", busPower * 2);
        DEBUGLOG("\thub power needed = %ldmA\n", hubPower * 2);
        DEBUGLOG("\tport power available = %ldmA\n", powerAvailForPorts * 2);
        DEBUGLOG("\tport power needed = %ldmA\n", powerNeededForPorts * 2);
        DEBUGLOG("\tpower for captives = %ldmA\n", _powerForCaptive * 2);
        DEBUGLOG("\tbus power is %s\n", _busPowerGood?"good" : "insufficient");
#endif
        
        _selfPowerGood = false;
        
        if(_selfPowered)
        {
            USBStatus	status = 0;

            if ((err = _device->GetDeviceStatus(&status))) 	break;
            status = USBToHostWord(status);
            _selfPowerGood = ((status & 1) != 0);	// FIXME 1?
        }

        if(_selfPowered && _busPowered)
        {
            /* Dual power hub */
            DEBUGLOG("%s: Hub attached - Self/Bus powered, ", getName());
            if(_selfPowerGood)
                DEBUGLOG("power supply good\n");
            else
                DEBUGLOG("no external power\n");
        }
        else
        {
            /* Single power hub */
            if(_selfPowered)
            {
                DEBUGLOG("%s: Hub attached - Self powered, ", getName());
                if(_selfPowerGood)
                    DEBUGLOG("power supply good\n");
                else
                    DEBUGLOG("no external power\n");
            }
            else
            {
                DEBUGLOG("%s: Hub attached - Bus powered\n", getName());
            }

        }
        startExternal = (_busPowerGood || _selfPowerGood);
        if( !startExternal )
        {	/* not plugged in or bus powered on a bus powered hub */
            err = kIOReturnNoPower;
            DEBUGLOG("%s: insufficient power to turn on ports\n", getName());
            if(!_busPowered)
            {
                /* may be able to turn on compound devices */
                break;	/* Now what ?? */
            }
        }
    } while (false);

    return(err == kIOReturnSuccess);
}


bool IOUSBHub::AllocatePortMemory(void)
{
    IOUSBHubPort 	*port;
    UInt32		power;
    int 		portMask = 2;
    int 		portByte = 0;
    int			currentPort;


    _ports = (IOUSBHubPort **)
    		IOMalloc(sizeof(IOUSBHubPort *) * _hubDescriptor.numPorts);
    if (!_ports)
        return(false);

    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        /* determine if the port is a captive port */
        if ((_hubDescriptor.removablePortFlags[portByte] & portMask) != 0)
        {
            power = _powerForCaptive;
        }
        else
        {
            power = _selfPowerGood ? kUSB500mAAvailable : kUSB100mAAvailable;
        }

        port = new IOUSBHubPort;
        port->init(self, currentPort, power);
        _ports[currentPort-1] = port;

        portMask <<= 1;
        if(portMask > 0x80)
        {
            portMask = 1;
            portByte++;
        }
    }
    
    return(true);
}


bool IOUSBHub::StartPorts(void)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBHubPort 	*port;
    int			currentPort;
    //IOThread 	     	ioThread;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: starting ports (%d)\n", getName(), _hubDescriptor.numPorts);
#endif

    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        port = _ports[currentPort-1];
	if (port)
            err = port->start();

        // Temporarily fork off a thread for each port.  Each port could
        // potentially call addDevice which will generate hub events.
        //ioThread = IOCreateThread(callStart, port);
        //if (ioThread == NULL)
        //{
        //    err = kIOReturnNoMemory;
        //    break;
        //}
    }

    // Ignore any error for now.  Maybe the hub would want to know
    // if all ports failed, but if at least one succeeds, then we
    // should continue.
    return(true);
}

bool IOUSBHub::StopPorts(void)
{
    IOUSBHubPort 	*port;
    int			currentPort;


#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: stopping ports (%d)\n", getName(), _hubDescriptor.numPorts);
#endif

    if( _ports) for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        port = _ports[currentPort-1];
	if (port) {
            port->stop();
            delete port;
            _ports[currentPort-1] = 0;
	}
    }

    IOFree(_ports, sizeof(IOUSBHubPort *) * _hubDescriptor.numPorts);
    _ports = 0;
    return(true);
}

const UInt8 * IOUSBHub::getStatusChanged(void)
{
    IOReturn		err = kIOReturnSuccess;
    const UInt8 *	status = NULL;

    _buffer->setLength(8);

    do
    {
        if ((err = _interruptPipe->read(_buffer)))
        {
            DEBUGLOG("%s: error 0x%x reading interrupt pipe.\n", getName(), err);
            break;
        }

        if (err == kIOReturnNotResponding)
        {
            // This probably means the device was unplugged.
            break;
        }
        else if (err != kIOReturnSuccess)
        {
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            //kprintf("%s: error %d reading interrupt pipe\n", getName(), err);
            break;
        }
        status = (const UInt8 *)_buffer->getBytesNoCopy();
    } while (false);
    return status;
}

bool IOUSBHub::StartHandler(void)
{
    IOReturn	err = kIOReturnSuccess;
    const UInt8	*statusChangedBitmap;
    int 	portMask;
    int 	portByte;
    int 	portIndex;
    IOUSBHubPort *port;

    do
    {
        portMask = 2;
        portByte = 0;
        statusChangedBitmap = getStatusChanged();
        if (statusChangedBitmap == 0)
        {
            DEBUGLOG("%s: no data returned from int pipe\n", getName());
            break;
        }

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("read returned 0x%x (%d)\n", statusChangedBitmap[portByte],
                 _buffer->getLength());
#endif

        for (portIndex = 1; portIndex <=_hubDescriptor.numPorts; portIndex++)
        {
            if ((statusChangedBitmap[portByte] & portMask) != 0)
            {
                port = _ports[portIndex-1];
                port->statusChanged();
            }

            portMask <<= 1;
            if (portMask > 0x80)
            {
                portMask = 1;
                portByte++;
            }
        }

        /* hub status changed */
        if ((statusChangedBitmap[0] & 1) != 0)
        {	
            statusChanged();
        }

    } while (true);

    DEBUGLOG("%s: error %d.  Exiting Hub Handler\n", getName(), err);

    _device->close(this);
    
    return(false);
}


bool IOUSBHub::statusChanged(void)
{
    IOReturn	err = kIOReturnSuccess;

    do
    {
        if ((err = GetHubStatus(&_hubStatus)))
        {
            fatalError(err, "get status (first in hub status change)");
            break;            
        }
        _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
        _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("%s: hub status = %x/%x\n", getName(),
                 _hubStatus.statusFlags, _hubStatus.changeFlags);
#endif

        if (_hubStatus.changeFlags & kHubLocalPowerStatusChange)
        {
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("%s: Local Power Status Change detected\n", getName());
#endif
            if ((err = ClearHubFeature(kHubLocalPowerStatusChange)))
            {
                fatalError(err, "clear hub power status feature");
                break;
            }
            if ((err = GetHubStatus(&_hubStatus)))
            {
                fatalError(err, "get status (second in hub status change)");
                break;
            }
            _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
            _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);
        }

        if (_hubStatus.changeFlags & kHubOverCurrentIndicatorChange)
        {
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("%s: OverCurrent Indicator Change detected\n", getName());
#endif
            if ((err =
                 ClearHubFeature(kHubOverCurrentIndicatorChange)))
            {
                fatalError(err, "clear hub over current feature");
                break;
            }
            if ((err = GetHubStatus(&_hubStatus)))
            {
                fatalError(err, "get status (second in hub status change)");
                break;
            }
            _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
            _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);
        }

        //DEBUGLOG("%s: calling delay reentering poll loop\n", getName());
        //USBDelay(10);  //FIXME

    } while(false);

    return(err == kIOReturnSuccess);
}

static ErrataListEntry	errataList[] = {

/* For the Cherry 4 port KB, From Cherry:
We use the bcd_releasenumber-highbyte for hardware- and the lowbyte for
firmwarestatus. We have 2 different for the hardware 03(eprom) and
06(masked microcontroller). Firmwarestatus is 05 today.
So high byte can be 03 or 06 ----  low byte can be 01, 02, 03, 04, 05

Currently we are working on a new mask with the new descriptors. The
firmwarestatus will be higher than 05.
*/
      {0x046a, 0x003, 0x0301, 0x0305, kErrataCaptiveOK}, // Cherry 4 port KB
      {0x046a, 0x003, 0x0601, 0x0605, kErrataCaptiveOK}  // Cherry 4 port KB
};

#define errataListLength (sizeof(errataList)/sizeof(ErrataListEntry))

UInt32 GetErrataBits(IOUSBDeviceDescriptor *desc)
{
      UInt16		vendID, deviceID, revisionID;
      ErrataListEntry	*entryPtr;
      UInt32		i, errata = 0;

      // get this chips vendID, deviceID, revisionID
      vendID = USBToHostWord(desc->vendor);
      deviceID = USBToHostWord(desc->product);
      revisionID = USBToHostWord(desc->devRel);

      for(i=0, entryPtr = errataList; i < errataListLength; i++, entryPtr++)
      {
          if (vendID == entryPtr->vendID
              && deviceID == entryPtr->deviceID
              && revisionID >= entryPtr->revisionLo
              && revisionID <= entryPtr->revisionHi)
          {
              errata |= entryPtr->errata;  // we match, add this errata to our list
          }
      }
      return(errata);
}

void IOUSBHub::fatalError(IOReturn err, char *str)
{
    DEBUGLOG("IOUSBHub: Error %d: %s\n", err, str);
}

IOReturn IOUSBHub::GetHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("*********** GET HUB DESCRIPTOR ***********\n");
#endif

    if (!desc) return (kIOReturnBadArgument);

    request.rqDirection = kUSBIn;
    request.rqType = kUSBClass;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqGetDescriptor;
    OSWriteLittleInt16(&request.wValue, 0, kUSBHubDescriptorType);
    OSWriteLittleInt16(&request.wIndex, 0, 0);
    OSWriteLittleInt16(&request.wLength, 0, sizeof(IOUSBHubDescriptor));
    request.pData = desc;

    err = _device->deviceRequest(&request);

    if (err)
    {
        /*
         * Is this a bogus hub?  Some hubs require 0 for the descriptor type
         * to get their device descriptor.  This is a bug, but it's actually
         * spec'd out in the USB 1.1 docs.
         */
        OSWriteLittleInt16(&request.wValue, 0, 0);
        OSWriteLittleInt16(&request.wLength, 0, sizeof(IOUSBHubDescriptor));
        err = _device->deviceRequest(&request);
    }

    if (err)
        IOLog("%s: error getting hub descriptor. err=%d\n", getName(), err);

    return(err);
}

IOReturn IOUSBHub::GetHubStatus(IOUSBHubStatus *status)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("************* GET HUB STATUS *************\n");
#endif

    request.rqDirection = kUSBIn;
    request.rqType = kUSBClass;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqGetStatus;
    OSWriteLittleInt16(&request.wValue, 0, 0);
    OSWriteLittleInt16(&request.wIndex, 0, 0);
    OSWriteLittleInt16(&request.wLength, 0, sizeof(IOUSBHubStatus));
    request.pData = status;

    err = _device->deviceRequest(&request);

    if (err)
        IOLog("%s: error getting hub status. err=%d\n", getName(), err);

    return(err);
}

IOReturn IOUSBHub::GetPortState(UInt8 *state, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("************* GET PORT STATE *************\n");
#endif

    request.rqDirection = kUSBIn;
    request.rqType = kUSBClass;
    request.rqRecipient = kUSBOther;
    request.bRequest = kUSBRqGetState;
    OSWriteLittleInt16(&request.wValue, 0, 0);
    OSWriteLittleInt16(&request.wIndex, 0, port);
    OSWriteLittleInt16(&request.wLength, 0, sizeof(*state));
    request.pData = state;

    err = _device->deviceRequest(&request);

    if (err)
        IOLog("%s: error getting hub state. err=%d\n", getName(), err);

    return(err);
}

IOReturn IOUSBHub::ClearHubFeature(UInt16 feature)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("********** CLEAR HUB FEATURE %d **********\n", feature);
#endif

    request.rqDirection = kUSBOut;
    request.rqType = kUSBClass;
    request.rqRecipient = kUSBDevice;
    request.bRequest = kUSBRqClearFeature;
    OSWriteLittleInt16(&request.wValue, 0, feature);
    OSWriteLittleInt16(&request.wIndex, 0, 0);
    OSWriteLittleInt16(&request.wLength, 0, 0);
    OSWriteLittleInt16(&request.pData, 0, 0);

    err = _device->deviceRequest(&request);

    if (err)
        IOLog("%s: error clearing hub feature. err=%d\n", getName(), err);

    return(err);
}

IOReturn IOUSBHub::GetPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("*********** GET PORT %d STATUS ************\n", port);
#endif

    request.rqDirection = kUSBIn;
    request.rqType = kUSBClass;
    request.rqRecipient = kUSBOther;
    request.bRequest = kUSBRqGetStatus;
    OSWriteLittleInt16(&request.wValue, 0, 0);
    OSWriteLittleInt16(&request.wIndex, 0, port);
    OSWriteLittleInt16(&request.wLength, 0, sizeof(IOUSBHubPortStatus));
    request.pData = status;

    err = _device->deviceRequest(&request);

    if (err)
        IOLog("%s: error getting port status. err=%d\n", getName(), err);

    // Get things the right way round.
    status->statusFlags = OSReadLittleInt16(&status->statusFlags, 0);
    status->changeFlags = OSReadLittleInt16(&status->changeFlags, 0);

    return(err);
}

IOReturn IOUSBHub::SetPortFeature(UInt16 feature, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("********* SET PORT %d FEATURE %d **********\n", port, feature);
#endif

    request.rqDirection = kUSBOut;
    request.rqType = kUSBClass;
    request.rqRecipient = kUSBOther;
    request.bRequest = kUSBRqSetFeature;
    OSWriteLittleInt16(&request.wValue, 0, feature);
    OSWriteLittleInt16(&request.wIndex, 0, port);
    OSWriteLittleInt16(&request.wLength, 0, 0);
    request.pData = 0;

    err = _device->deviceRequest(&request);

    if (err)
        IOLog("%s: error setting port feature. err=%d\n", getName(), err);

    return(err);
}

IOReturn IOUSBHub::ClearPortFeature(UInt16 feature, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("******** CLEAR PORT %d FEATURE %d *********\n", port, feature);
#endif

    request.rqDirection = kUSBOut;
    request.rqType = kUSBClass;
    request.rqRecipient = kUSBOther;
    request.bRequest = kUSBRqClearFeature;
    OSWriteLittleInt16(&request.wValue, 0, feature);
    OSWriteLittleInt16(&request.wIndex, 0, port);
    OSWriteLittleInt16(&request.wLength, 0, 0);
    request.pData = 0;

    err = _device->deviceRequest(&request);

    if (err)
        IOLog("%s: error clearing port feature. err=%d\n", getName(), err);

    return(err);
}

void IOUSBHub::PrintHubDescriptor(IOUSBHubDescriptor *desc)
{
    int i = 0;
    char *characteristics[] =
        { "ppsw", "nosw", "comp", "ppoc", "nooc", 0 };


    if (desc->length == 0) return;

    DEBUGLOG("hub descriptor: (%d bytes)\n", desc->length);
    DEBUGLOG("\thubType = %d\n", desc->hubType);
    DEBUGLOG("\tnumPorts = %d\n", desc->numPorts);
    DEBUGLOG("\tcharacteristics = %x ( ",
                                USBToHostWord(desc->characteristics));
    do
    {
        if (USBToHostWord(desc->characteristics) & (1 << i))
            DEBUGLOG("%s ", characteristics[i]);
    } while (characteristics[++i]);
    DEBUGLOG(")\n");
    DEBUGLOG("\tpowerOnToGood = %d ms\n", desc->powerOnToGood * 2);
    DEBUGLOG("\thubCurrent = %d\n", desc->hubCurrent);
    DEBUGLOG("\tremovablePortFlags = %x %x\n", &desc->removablePortFlags[1], &desc->removablePortFlags[0]);
    DEBUGLOG("\tpwrCtlPortFlags    = %x %x\n", &desc->pwrCtlPortFlags[1], &desc->removablePortFlags[0]);
}
