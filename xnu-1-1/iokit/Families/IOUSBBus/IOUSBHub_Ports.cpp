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


#include "IOUSBHub.h"
#include <IOKit/usb/IOUSBController.h>

//#define super IOService
#define self this
#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf

static portStatusChangeVector defaultPortVectors[kNumChangeHandlers] =
{
    { 0, kHubPortOverCurrent,	kUSBHubPortOverCurrentChangeFeature },
    { 0, kHubPortBeingReset, 	kUSBHubPortResetChangeFeature },
    { 0, kHubPortSuspend,	kUSBHubPortSuspendChangeFeature },
    { 0, kHubPortEnabled,	kUSBHubPortEnableChangeFeature },
    { 0, kHubPortConnection,	kUSBHubPortConnectionChangeFeature },
};

void IOUSBHubPort::init( IOUSBHub *parent, int portNum, UInt32 powerAvailable )
{
    _hub		= parent;
    _bus		= parent->_bus;
    _hubDesc		= &parent->_hubDescriptor;
    _portNum		= portNum;
    _portDevice		= 0;
    _portPowerAvailable	= powerAvailable;
    initPortVectors();

    if (!_hub) DEBUGLOG("init: _hub is invalid\n");
    if (!_bus) DEBUGLOG("init: _bus is invalid\n");
    if (!_hubDesc) DEBUGLOG("init: _hubDesc is invalid\n");
    if (portNum < 1 || portNum > 64) DEBUGLOG("init: portNum is invalid\n");
}

/*
 * start:
 * Here we really just need to turn on power to the ports.  The change
 * handler will take it from there.  However, we need to check for 2 conditions
 * before we exit:
 *    1. are there permanent devices connected,
 *    2. some hubs don't generate a change connection if the ports are
 *       powered on with a device already connected.  
 * If either of these exist, then add the port.
 */
IOReturn IOUSBHubPort::start(void)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;


    do
    {
        /* turn on Power to the port */
        if ((err = _hub->SetPortFeature(kUSBHubPortPowerFeature, _portNum)))
        {
            fatalError(err, "setting port power");
            break;
        }

        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            fatalError(err, "getting port status (1)");
            break;
        }

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("0x%x\t(start) port %d status = %xs/%xc\n", _hub,
                 _portNum, status.statusFlags, status.changeFlags);
#endif

        // If there is a connection change, then let the handler take it
        // from here.  Also, in some cases the hub was in a suspend state
        // (rather than a reset state) and does not register a connection
        // change when the port is powered.  So also check for a connection
	// with no change connection.        
        // Is there a change connection OR no connection?
        // THEN we're all done for now; int handler will take it from here
        //if ((status.changeFlags & kHubPortConnection) ||
        //    (status.statusFlags & kHubPortConnection) == 0)
            break;
        
        /* wait for the power on good time */
        IOSleep(_hubDesc->powerOnToGood * 2);

        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            fatalError(err, "getting port status (2)");
            break;
        }

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("0x%x\t(start) port %d status = %xs/%xc\n", _hub,
              _portNum, status.statusFlags, status.changeFlags);
#endif

        /* we now have port status */
        if (status.changeFlags & kHubPortConnection)
        {
            if ((err = _hub->ClearPortFeature
                 		(kUSBHubPortConnectionChangeFeature, _portNum)))
            {
                fatalError(err, "clearing port connection change");
                break;
            }

            /* We should now be in the disconnected state */
            /* Do a port request on current port */
            if ((err = _hub->GetPortStatus(&status, _portNum)))
            {
                fatalError(err, "getting port status (3)");
                break;
            }
        }

        if (status.statusFlags & kHubPortConnection)
        {
            /* We have a connection on this port */
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("0x%x\t(start)device detected @ port %d\n", _hub,
                 _portNum);
#endif
            if ((err = addDevice()))
                fatalError(err, "adding device");
        }
    } while(false);

    return(err);
}

void IOUSBHubPort::stop(void)
{
    // Ugh.  This could get nasty.  What if stop is called while
    // a thread is adding a device?  I think we need to start tracking
    // states.  And we might need a flag to tell us where it's because
    // the device has been unplugged, or the system is shutting down.

    removeDevice();
}

IOReturn IOUSBHubPort::addDevice(void)
{
    IOReturn		err = kIOReturnSuccess;

    do
    {
        _devZero = _bus->AcquireDeviceZero() == kIOReturnSuccess;
        if (!_devZero)
        {
            fatalError(/*FIXME*/0, "acquiring device zero");
            break;
        }

        if ((err = _hub->SetPortFeature(kUSBHubPortResetFeature,
                                              _portNum)))
        {
            fatalError(err, "set feature (resetting port)");
            break;
        }
	{
            // Now poll for exit from reset.
            IOUSBHubPortStatus	status;
            int portByte;
            UInt8 portMask;
            bool waitReset = true;
            AbsoluteTime endResetTime, nowTime;
            AbsoluteTime_to_scalar(&nowTime) = 0;
            portByte = _portNum / 8;
            portMask = 1 << (_portNum % 8);
            clock_interval_to_deadline(1, kSecondScale, &endResetTime);
            do
            {
                const UInt8* stat = _hub->getStatusChanged();
                if(stat == NULL)
                    break;
                // Check if we've waited long enough!
                if( CMP_ABSOLUTETIME(&nowTime, &endResetTime) > 0) {
                    err = kIOReturnTimeout;
                    break;
                }
                clock_get_uptime(&nowTime);
		if(!stat[portByte] & portMask) {
		    IOSleep(2);
                    continue;
		}
                /* Do a port status request on current port */
                if ((err = _hub->GetPortStatus(&status, _portNum)))
                {
                    fatalError(err, "get status (waiting for reset end)");
                    break;
                }
#if (DEBUGGING_LEVEL > 0)
                DEBUGLOG("0x%x\t(waiting for reset end) port %d status = %xs/%xc\n", _hub,
                    _portNum, status.statusFlags, status.changeFlags);
#endif
                if(status.changeFlags & kHubPortBeingReset)
                {
                    if ((err = _hub->ClearPortFeature(
				kUSBHubPortResetChangeFeature, _portNum)))
                    {
                        fatalError(err, "clear port vector bit feature");
                        break;
                    }
                    err = addDeviceResetChangeHandler(status.changeFlags);
                    waitReset = false;
                }
            } while (waitReset);
	}
    } while(false);

    if (err && _devZero)
    {
        _bus->ReleaseDeviceZero();
        _devZero = false;
        
        // put it back to the default if there was an error
#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("error: setting change handler to default\n");
#endif
        setPortVector(&IOUSBHubPort::defaultResetChangeHandler,
                      kHubPortBeingReset);
    }

    return(err);
}

void IOUSBHubPort::removeDevice(void)
{
    bool	ok;

    if (_portDevice) {
	ok = _portDevice->terminate(kIOServiceRequired);
        if( !ok)
            DEBUGLOG("IOUSBHubPort: terminate() failed\n");
	_portDevice->release();
	_portDevice = 0;
    }
    initPortVectors();
}

IOReturn IOUSBHubPort::resetDevice()
{
    IOReturn err = kIOReturnSuccess;

    do {
        _devZero = _bus->AcquireDeviceZero() == kIOReturnSuccess;
        if (!_devZero)
        {
            fatalError(/*FIXME*/0, "acquiring device zero");
            err = kIOReturnCannotLock;
            break;
        }

        setPortVector(&IOUSBHubPort::handleResetDevice, kHubPortBeingReset);

        err = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
        if(err != kIOReturnSuccess)
            break;

    } while (false);

    if(err == kIOReturnSuccess)
    {
        _bus->WaitForReleaseDeviceZero();
    }
    else if(_devZero)
    {
        _bus->ReleaseDeviceZero();
        _devZero = false;
    }

    return err;
}

IOReturn IOUSBHubPort::handleResetDevice(UInt16 changeFlags)
{
    IOReturn err = kIOReturnSuccess;

    setPortVector(&IOUSBHubPort::defaultResetChangeHandler, kHubPortBeingReset);
    /* Now address the device */
    _bus->ConfigureDeviceZero(_desc.maxPacketSize, _speed);
    err = _bus->SetDeviceZeroAddress(_portDevice->address(),
                                    _desc.maxPacketSize, _speed);

    _bus->ReleaseDeviceZero();
    _devZero = false;

    return err;
}

void IOUSBHubPort::fatalError(IOReturn err, char *str)
{
    DEBUGLOG("IOUSBHubPort: Error 0x%x on port %d: %s\n", err, _portNum, str);
    if (_portDevice != 0)
    {
        DEBUGLOG("IOUSBHubPort: Removing %s from port %d\n",
                 _portDevice->getName(), _portNum);
        removeDevice();	
    }
}


/**********************************************************************
 **
 ** CHANGE HANDLER FUNCTIONS
 **
 **********************************************************************/
IOReturn IOUSBHubPort::addDeviceResetChangeHandler(UInt16 changeFlags)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;

    do
    {
        /* Tell the USB to add the device */
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            fatalError(err, "getting port status (3)");
            break;
        }

        if (status.statusFlags & kHubPortBeingReset)
        {
            DEBUGLOG("%s: port not finished resetting, retrying\n", _hub->getName());
            // we should never be here, just wait for another status change int
            break;
        }

        // macally iKey doesn't tell us until now what the device speed is.
        _speed = ((status.statusFlags & kHubPortSpeed) != 0);
#if (DEBUGGING_LEVEL > 0)
                DEBUGLOG("0x%x\t(reset end) port %d status = %xs/%xc, %s speed\n", _hub,
                         _portNum, status.statusFlags, status.changeFlags, _speed ? "low" : "high");
#endif

        /* Now wait 10 ms after reset */
        IOSleep(10);

        // Configure algorithm:
	// * start with maxpacketsize of 8.
	// This is the smallest legal maxpacket, and the only legal size for
	// low-speed devices. The correct maxpacket size is in byte 8 of the
	// device descriptor, so even if the device sends back a bigger packet
	// (an overrun error) we should still get the correct value.
	// * get device descriptor.
	// * if we recieved the whole descriptor AND maxpacketsize is 64,
	//   success, so continue on.
        // * if descriptor returns with a different maxpacketsize, then
	//   reconfigure with the new one and try again.  Otherwise, 
	//   reconfigure with 8 and try again.
        if ((err = _bus->ConfigureDeviceZero(8, _speed)))
        {
            fatalError(err, "configuring endpoint zero");
            break;
        }		

        // Now do a device request to find out what it is
        // Some fast devices send back packets > 8 bytes to address 0.
        bzero(&_desc, sizeof(_desc));
        err = _bus->GetDeviceZeroDescriptor(&_desc);

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("IOUSBHubPort: getDeviceDescriptor err = 0x%x  max = %d numconf = %d\n", err, _desc.maxPacketSize, _desc.numConf);
#endif
        // Check for specific conditions here before going on.
        // 1) if the pipe cannot handle the default max packet size of 8,
        //    then reconfigure the pipe and try again.  If just the size is
        //    wrong, it should return at least the correct size if not
        //    the whole descriptor.
        // 2) If there is still an err, bail out.
        if (err || _desc.maxPacketSize != 8)
        {
            // Reconfigure the pipe if the max packet size is not the default
            // of 8, but we explicitly don't want to set it to 0.  It might be
            // good to do a little more checking here...later.
            if (_desc.maxPacketSize != 0 && _desc.maxPacketSize != 8)
            {
                err = _bus->ConfigureDeviceZero(_desc.maxPacketSize, _speed);
                err = _bus->GetDeviceZeroDescriptor(&_desc);
            }
            // The device really didn't like 8.  Probably got a stall or
            // something like that.  As a last ditch effort, try 64.
            else if (err)
            {
                err = _bus->ConfigureDeviceZero(64, _speed);
                err = _bus->GetDeviceZeroDescriptor(&_desc);
            }

            if (err == kIOReturnOverrun)
            {
                // Not sure what to do with this error.  It means more data
                // came back than the size of a descriptor.  Hmmm.  For now
                // just ignore it and assume the data that did come back is
                // useful.
#if (DEBUGGING_LEVEL > 0)
                DEBUGLOG("%s: overrun error reading device descriptor\n",
                      _hub->getName());
#endif
            }

            if (err)
            {
                fatalError(err, "getting full device descriptor");
                continue;
            }
        }

        /* ERIC FIXME?  what are we doing here?
        if ((_hubDescriptor.removablePortFlags[pp->portByte] & pp->portMask))
        {
            pb->usbOther = powerForCaptive;
        }
        else
        {
            pb->usbOther = selfPowerGood?kUSB500mAAvailable:kUSB100mAAvailable;
        }
        */		

        /* Now create and address the device */
        if ((_portDevice = _bus->MakeDevice(&_desc, _speed, _portPowerAvailable)) == 0)
        {
            _portDevice = 0;
            err = kIOReturnDeviceError;
            fatalError(err, "setting the device address");
            continue;
        }
        /* Tell the device who it's port is */
        _portDevice->setPort(this);

        /* Release */
        _bus->ReleaseDeviceZero();
        _devZero = false;

        /* Finally use the data gathered */
        _portDevice->registerService();

    } while(false);

    // reset the vector back to the default
#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("0x%x\t(port %d)setting change handler to default\n", _hub, _portNum);
#endif
    setPortVector(&IOUSBHubPort::defaultResetChangeHandler, kHubPortBeingReset);

    if (err)
    {
        if (_devZero)
        {
            _bus->ReleaseDeviceZero();
            _devZero = false;
        }
    }
    return err;
}

IOReturn IOUSBHubPort::defaultOverCrntChangeHandler(UInt16 changeFlags)
{
#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("IOUSBHubPort: over current change notification\n");
#endif
    return kIOReturnSuccess;
}

IOReturn IOUSBHubPort::defaultResetChangeHandler(UInt16 changeFlags)
{
#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("IOUSBHubPort: reset change notification\n");
#endif
    return kIOReturnSuccess;
}

IOReturn IOUSBHubPort::defaultSuspendChangeHandler(UInt16 changeFlags)
{
#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("IOUSBHubPort: suspend change notification\n");
#endif
    return kIOReturnSuccess;
}

IOReturn IOUSBHubPort::defaultEnableChangeHandler(UInt16 changeFlags)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("IOUSBHubPort: enable change notification\n");
#endif

    if ((err = _hub->GetPortStatus(&status, _portNum)))
    {
        fatalError(err, "getting port status");
        return err;
    }

    if (!(status.statusFlags & kHubPortEnabled) &&
        !(changeFlags & kHubPortConnection))
    {
        // The hub gave us an enable status change and we're
        // now disabled, strange.  Cosmo does this sometimes,
        // try Re-enabling the port.
#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("IOUSBHubPort: re-enabling dead port %d\n", _portNum);
#endif
        if ((err = _hub->SetPortFeature(kUSBHubPortEnableFeature, _portNum)))
            fatalError(err, "re-enabling dead port");
    }
    return err;
}

IOReturn IOUSBHubPort::defaultConnectionChangeHandler(UInt16 changeFlags)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("0x%x\t(port %d)connection change notification\n",  _hub, _portNum);
#endif

    do
    {
        /* wait for the power on good time */
        IOSleep(_hubDesc->powerOnToGood * 2);

        // If we get to here, there was a connection change
        // if we already have a device it must have been disconnected
        // at sometime. We should kill it before servicing a connect event
        if (_portDevice != 0)
        {	
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("0x%x\t(port %d): removing %s device @ %d\n",
		_hub, _portNum, _portDevice->getName(), _portDevice->address());
#endif

            removeDevice();	
        }

        // BT 23Jul98 Check port again after delay. Get bounced connections
        /* Do a port status request on current port */
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            fatalError(err, "getting port status");
            break;
        }

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("0x%x\t(conn change) port %d status = %xs/%xc\n", _hub,
                 _portNum, status.statusFlags, status.changeFlags);
#endif

        if (status.changeFlags & kHubPortConnection)
        {
            DEBUGLOG("IOUSBHubPort: connection bounce\n");
            break;
        }

        if (status.statusFlags & kHubPortConnection)
        {
            /* We have a connection on this port */
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("0x%x\tdevice detected @ port %d\n", _hub, _portNum);
#endif
            for(int i=0; i<2; i++) {
                err = addDevice();
                if (err == kIOReturnSuccess)
                    break;
                err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
                if (err != kIOReturnSuccess)
                    fatalError(err, "adding device");
            }
        }
        
    } while(false);
    return err;
}

bool IOUSBHubPort::statusChanged(void)
{
    IOReturn	err = kIOReturnSuccess;
    int 	which;
    IOUSBHubPortStatus	status;

    
    do
    {
        /* Do a port status request on current port */
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            fatalError(err, "get status (first in port status change)");
            break;
        }

#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("0x%x\t(status changed 1) port %d status = %xs/%xc\n", _hub,
              _portNum, status.statusFlags, status.changeFlags);
#endif

        // First clear the change condition before we return.  This prevents
        // a race condition for handling the change.
        for (which = 0; which < kNumChangeHandlers; which++)
        {
            // sometimes a change is reported but there really is
            // no change.  This will catch that.
            if (!(status.changeFlags & _changeHandler[which].bit))
                continue;

            if ((err = _hub->ClearPortFeature(
                    _changeHandler[which].clearFeature, _portNum)))
            {
                fatalError(err, "clear port vector bit feature");
                continue;
            }
        }

        err = statusChangeHandler(status.changeFlags);

    } while(false);

    return(err == kIOReturnSuccess);
}


IOReturn IOUSBHubPort::statusChangeHandler(UInt16 changeFlags)
{
    int which;
    IOReturn res = kIOReturnSuccess;

    // Handle each change in sequential order.
    for (which = 0; which < kNumChangeHandlers; which++)
    {
        if (!(changeFlags & _changeHandler[which].bit))
            continue;
        res = (self->*_changeHandler[which].handler)(changeFlags);
        if(res != kIOReturnSuccess)
            break;
    }

    return(res);
}


void IOUSBHubPort::initPortVectors(void)
{
    int vector;
    for (vector = 0; vector < kNumChangeHandlers; vector++)
    {
        _changeHandler[vector] = defaultPortVectors[vector];
        switch (defaultPortVectors[vector].bit)
        {
            case kHubPortOverCurrent:
                _changeHandler[vector].handler =
                &IOUSBHubPort::defaultOverCrntChangeHandler;
                break;
            case kHubPortBeingReset:
                _changeHandler[vector].handler =
                &IOUSBHubPort::defaultResetChangeHandler;
                break;
            case kHubPortSuspend:
                _changeHandler[vector].handler =
                &IOUSBHubPort::defaultSuspendChangeHandler;
                break;
            case kHubPortEnabled:
                _changeHandler[vector].handler =
                &IOUSBHubPort::defaultEnableChangeHandler;
                break;
            case kHubPortConnection:
                _changeHandler[vector].handler =
                &IOUSBHubPort::defaultConnectionChangeHandler;
                break;
        }
    }
}

void IOUSBHubPort::setPortVector(ChangeHandlerFuncPtr	routine,
                                 UInt32			condition)
{
    int vector;
    for(vector = 0; vector < kNumChangeHandlers; vector++)
    {
        if(condition == _changeHandler[vector].bit)
        {
            _changeHandler[vector].handler = routine;
        }
    }
}
