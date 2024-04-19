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


#include "AppleOHCI.h"
#include <libkern/OSByteOrder.h>

#define nil (0)
#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme

#define super IOUSBController
#define self this

/*
 * Root hub methods
 */
// FIXME  Should this routine go in the device?
IOReturn AppleOHCI::getRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOUSBDeviceDescriptor newDesc =
    {
        sizeof(IOUSBDeviceDescriptor),	// UInt8 length;
        kUSBDeviceDesc,			// UInt8 descType;
        USB_CONSTANT16(kUSBRel10),	// UInt16 usbRel;
        kUSBHubClass,			// UInt8 class;
        kUSBRootHubSubClass,		// UInt8 subClass;
        0,				// UInt8 protocol;
        8,				// UInt8 maxPacketSize;
        USB_CONSTANT16(_vendorID),	// UInt16 vendor;
        USB_CONSTANT16(_deviceID),	// UInt16 product;
        USB_CONSTANT16(_revisionID),	// UInt16 devRel;
        0,				// UInt8 manuIdx;
        0,				// UInt8 prodIdx;
        0,				// UInt8 serialIdx;
        1				// UInt8 numConf;
    };

    if (!desc)
        return(kIOReturnNoMemory);

    bcopy(&newDesc, desc, newDesc.length);

    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::getRootHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOUSBHubDescriptor hubDesc;
    UInt8 pps, nps, cd, ppoc, noc;
    UInt32 descriptorA, descriptorB;


    descriptorA = OSSwapInt32(pOHCIUIMData->pOHCIRegisters->hcRhDescriptorA);
    descriptorB = OSSwapInt32(pOHCIUIMData->pOHCIRegisters->hcRhDescriptorB);
    hubDesc.length = sizeof(IOUSBHubDescriptor);
    hubDesc.hubType = kUSBHubDescriptorType;
    hubDesc.numPorts = ((descriptorA & kOHCIHcRhDescriptorA_NDP)
                          >> kOHCIHcRhDescriptorA_NDPPhase);
    // Characteristics
    pps  = descriptorA & kOHCIHcRhDescriptorA_PSM;
    nps  = descriptorA & kOHCIHcRhDescriptorA_NPS;
    cd   = descriptorA & kOHCIHcRhDescriptorA_DT;
    ppoc = descriptorA & kOHCIHcRhDescriptorA_OCPM;
    noc  = descriptorA & kOHCIHcRhDescriptorA_NOCP;
    hubDesc.characteristics = 0;
    hubDesc.characteristics |= (pps  ? kPerPortSwitchingBit   : 0);
    hubDesc.characteristics |= (nps  ? kNoPowerSwitchingBit   : 0);
    hubDesc.characteristics |= (cd   ? kCompoundDeviceBit     : 0);
    hubDesc.characteristics |= (ppoc ? kPerPortOverCurrentBit : 0);
    hubDesc.characteristics |= (noc  ? kNoOverCurrentBit      : 0);
    hubDesc.characteristics = HostToUSBWord(hubDesc.characteristics);
    
    hubDesc.powerOnToGood = ((descriptorA & kOHCIHcRhDescriptorA_POTPGT)
                               >> kOHCIHcRhDescriptorA_POTPGTPhase);
    // the root hub has no power requirements
    hubDesc.hubCurrent = 0;

    // bitmap of removable ports
    *((UInt16*)&hubDesc.removablePortFlags[0]) =
        ((descriptorB & kOHCIHcRhDescriptorB_DR)
         >> kOHCIHcRhDescriptorB_DRPhase);
    *((UInt16 *)&hubDesc.removablePortFlags[2]) = 0;  // OHCI supports 15 ports
    *((UInt32 *)&hubDesc.removablePortFlags[4]) = 0;  // so zero out the rest

    // bitmap of power mode for each port
    *((UInt16 *)&hubDesc.pwrCtlPortFlags[0]) =
        ((descriptorB & kOHCIHcRhDescriptorB_PPCM)
         >> kOHCIHcRhDescriptorB_PPCMPhase);
    *((UInt16 *)&hubDesc.pwrCtlPortFlags[2]) = 0;  // OHCI supports 15 ports
    *((UInt32 *)&hubDesc.pwrCtlPortFlags[4]) = 0;  // so zero out the rest

    if (!desc)
        return(kIOReturnNoMemory);

    bcopy(&hubDesc, desc, hubDesc.length);

/*
    if (!buffer->appendBytes(&hubDesc,  hubDesc.length))
        return(kIOReturnNoMemory);
*/
    
    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::getRootHubConfDescriptor(OSData *desc)
{
    IOUSBConfigurationDescriptor confDesc =
    {
        sizeof(IOUSBConfigurationDescriptor),//UInt8 length;
        kUSBConfDesc,               //UInt8 descriptorType;
        USB_CONSTANT16(sizeof(IOUSBConfigurationDescriptor) +
                       sizeof(IOUSBInterfaceDescriptor) +
                       sizeof(IOUSBEndpointDescriptor)),   //UInt16 totalLength;
        1,                          //UInt8 numInterfaces;
        1,                          //UInt8 configValue;
        0,                          //UInt8 configStrIndex;
        0x60,                       //UInt8 attributes; self powered,
				    //      supports remote wkup
        0,                          //UInt8 maxPower;
    };
    IOUSBInterfaceDescriptor intfDesc =
    {
        sizeof(IOUSBInterfaceDescriptor),//UInt8 length;
        kUSBInterfaceDesc,      //UInt8 descriptorType;
        0,                      //UInt8 interfaceNumber;
        0,                      //UInt8 alternateSetting;
        1,                      //UInt8 numEndpoints;
        kUSBHubClass,           //UInt8 interfaceClass;
        kUSBHubSubClass,        //UInt8 interfaceSubClass;
        1,                      //UInt8 interfaceProtocol;
        0                       //UInt8 interfaceStrIndex;
    };
    IOUSBEndpointDescriptor endptDesc =
    {
        sizeof(IOUSBEndpointDescriptor),//UInt8 length;
        kUSBEndpointDesc,       //UInt8 descriptorType;
        0x81,                   //UInt8  endpointAddress; In, 1
        kUSBInterrupt,          //UInt8 attributes;
        8, 0,      		//UInt16 maxPacketSize;
        255,                    //UInt8 interval;
    };

    if (!desc)
        return(kIOReturnNoMemory);

    if (!desc->appendBytes(&confDesc,  confDesc.length))
        return(kIOReturnNoMemory);

    if (!desc->appendBytes(&intfDesc,  intfDesc.length))
        return(kIOReturnNoMemory);

    if (!desc->appendBytes(&endptDesc, endptDesc.length))
        return(kIOReturnNoMemory);

    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::setRootHubDescriptor(OSData * /*buffer*/)
{
    IOLog("%s: unimplemented set root hub descriptor\n", getName());
    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::getRootHubStatus(IOUSBHubStatus *status)
{
    *(UInt32*)status = pOHCIUIMData->pOHCIRegisters->hcRhStatus;
    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::setRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
            IOLog("%s: unimplemented Set Power Change Feature\n", getName());
            // OHCIRootHubLPSChange(true);  // not implemented yet
            break;

        case kUSBHubOverCurrentChangeFeature :
            IOLog("%s: unimplemented Set Overcurrent Change Feature\n",
                  getName());
            // OHCIRootHubOCChange(true);  // not implemented yet
            break;

        default:
            IOLog("%s: Unknown hub set (%d) in root hub\n", getName(), wValue);
            break;
    }

    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::clearRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
            IOLog("%s: unimplemented Clear Power Change Feature\n", getName());
            // OHCIRootHubLPSChange(false);  // not implemented yet
            break;

        case kUSBHubOverCurrentChangeFeature :
            IOLog("%s: unimplemented Clear Overcurrent Change Feature\n",
                  getName());
            // OHCIRootHubOCChange(false);  // not implemented yet
            break;

        default:
            IOLog("%s: Unknown hub set (%d) in root hub\n", getName(), wValue);
            break;
    }

    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::getRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
    if (port < 1 || port > 15)
        return(kIOReturnBadArgument);  // FIXME change error code
    *(UInt32*)status = pOHCIUIMData->pOHCIRegisters->hcRhPortStatus[port-1];
    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::setRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    UInt16	port = wIndex-1;

    switch(wValue)
    {
        case kUSBHubPortSuspendFeature :
            OHCIRootHubPortSuspend(port, true);
            break;

        case kUSBHubPortResetFeature :
            OHCIRootHubResetPort(port);
            break;

        case kUSBHubPortEnableFeature :
            OHCIRootHubPortEnable(port, true);
            break;

        case kUSBHubPortPowerFeature :
            OHCIRootHubPortPower(port, true);
            OHCIRootHubPower(true);
            break;

        default:
            IOLog("%s: Unknown port set (%d) in root hub\n", getName(), wValue);
            break;
    }
    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::clearRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    UInt16	port = wIndex-1;

    switch(wValue)
    {
        case kUSBHubPortEnableFeature :
            OHCIRootHubPortEnable(port, false);
            break;

        case kUSBHubPortSuspendFeature :
            OHCIRootHubPortSuspend(port, false);
            break;

        case kUSBHubPortPowerFeature :
            OHCIRootHubPortPower(port, false);
            // Now need to check if all ports are switched off and
            // gang off if in gang mode
            break;

        // ****** Change features *******
        case kUSBHubPortConnectionChangeFeature :
            OHCIRootHubResetChangeConnection(port);
            break;

        case kUSBHubPortEnableChangeFeature :
            OHCIRootHubResetEnableChange(port);
            break;

        case kUSBHubPortSuspendChangeFeature :
            OHCIRootHubResetSuspendChange(port);
            break;

        case kUSBHubPortOverCurrentChangeFeature :
            OHCIRootHubResetOverCurrentChange(port);
            break;

        case kUSBHubPortResetChangeFeature :
            OHCIRootHubResetResetChange(port);
            break;

        default:
            IOLog("%s: Unknown port clear (%d) in root hub\n", getName(), wValue);
            break;
    }
    return(kIOReturnSuccess);
}

IOReturn AppleOHCI::getRootHubPortState(UInt8 */*state*/, UInt16 /*port*/)
{
    IOLog("%s: unimplemented get hub bus state", getName());
    return(kIOReturnSuccess);
}


IOReturn AppleOHCI::setHubAddress(UInt16 wValue)
{
    pOHCIUIMData->rootHubFuncAddress = wValue;
    return (kIOReturnSuccess);
}

void AppleOHCI::OHCIRootHubPower(bool on)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    pOHCIRegisters->hcRhDescriptorA = OSSwapInt32 (OSSwapInt32(pOHCIUIMData->pOHCIRegisters->hcRhDescriptorA) | kOHCIHcRhDescriptorA_NPS); //FIXME ERIC

    if(on)
    {
        value |= kOHCIHcRhStatus_LPSC;/* power on to all ganged ports */
        value |= kOHCIHcRhStatus_CRWE;/* clear remove wakeup enable */
        value |= kOHCIHcRhStatus_OCIC;/* clear over current change indicator */
    }
    else
        value |= kOHCIHcRhStatus_LPS; /* turn global power off */

    pOHCIRegisters->hcRhStatus = OSSwapInt32 (value);

    return;
}

void AppleOHCI::OHCIRootHubResetChangeConnection(UInt16 port)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    
    value |= kOHCIHcRhPortStatus_CSC; /* clear status change */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}


void AppleOHCI::OHCIRootHubResetResetChange(UInt16 port)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    
    value |= kOHCIHcRhPortStatus_PRSC; /* clear reset status change */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}

void AppleOHCI::OHCIRootHubResetSuspendChange(UInt16 port)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    value |= kOHCIHcRhPortStatus_PSSC; /* clear suspend status change */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}

void AppleOHCI::OHCIRootHubResetEnableChange(UInt16 port)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    value |= kOHCIHcRhPortStatus_PESC; /* clear enable status change */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}

    

void AppleOHCI::OHCIRootHubResetOverCurrentChange(UInt16 port)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    value |= kOHCIHcRhPortStatus_OCIC; /* clear over current status change */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}


void AppleOHCI::OHCIRootHubResetPort (UInt16 port)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    value |= kOHCIHcRhPortStatus_PRS; /* sets Bit 8 in port root hub register */
    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();
    return;

}

void AppleOHCI::OHCIRootHubPortEnable(UInt16	port,
                                      bool	on)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    if(on)
        value |= kOHCIHcRhPortStatus_PES; /* enable port */
    else
        value |= kOHCIHcRhPortStatus_CCS; /* disable port */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}

void AppleOHCI::OHCIRootHubPortSuspend(UInt16	port,
                                       bool	on)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    if(on)
        value |= kOHCIHcRhPortStatus_PSS;  /* suspend port */
    else
        value |= kOHCIHcRhPortStatus_POCI; /* resume port */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}

void AppleOHCI::OHCIRootHubPortPower(UInt16	port,
                                     bool	on)
{
    UInt32		value = 0;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;


    if(on)
        value |= kOHCIHcRhPortStatus_PPS;  /* enable port power */
    else
        value |= kOHCIHcRhPortStatus_LSDA; /* disable port power */

    pOHCIRegisters->hcRhPortStatus[port] = OSSwapInt32 (value);
    IOSync();

    return;
}

/*
 * UIMRootHubStatusChange
 *
 * This method gets called when there is a change in the root hub status
 * or a change in the status of one of the ports.  It is assumed that the
 * interrupt will be cleared for us, but we don't clear the change
 * condition, we will get another interrupt.  So turn off the interrupt.
 * Entering this function means that someone turned on the RHSC interrupt
 * and that there is a client waiting in the queue.  The client expects a
 * status change bitmap.
 * To fix: currently there can only be one client in the queue.  The RHSC
 * interrupt should only be turned off if there is no one else in the queue,
 * or, all clients should be responded to with the one interrupt.
 */
void AppleOHCI::UIMRootHubStatusChange(void)
{
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;
    UInt32 		hubStatus, portStatus, statusBit;
    UInt16 		statusChangedBitmap;   /* only have 15 ports in OHCI */
    UInt8		numPorts;
    unsigned int      	index, port, move;
    struct InterruptTransaction last;
    UInt32		descriptorA;


    /* turn off RHSC interrupt */
    pOHCIRegisters->hcInterruptDisable = OSSwapInt32(kOHCIHcInterrupt_RHSC);
    IOSync();

    /*
     * Encode the status change bitmap.  The format of the bitmap:
     * bit0 = hub status changed
     * bit1 = port 1 status changed
     * bit2 = port 2 status changed
     * ...
     * See USB 1.0 spec section 11.8.3 for more info.
     */

    statusChangedBitmap = 0;
    statusBit = 1;

    getRootHubStatus((IOUSBHubStatus *)&hubStatus);
    if ((hubStatus & kOHCIHcRhStatus_Change ) != 0)
        statusChangedBitmap |= statusBit; /* Hub status change bit */

    descriptorA = OSSwapInt32(pOHCIRegisters->hcRhDescriptorA);
    numPorts = ((descriptorA & kOHCIHcRhDescriptorA_NDP)
                >> kOHCIHcRhDescriptorA_NDPPhase);

    for (port = 1; port <= numPorts; port++)
    {
        statusBit <<= 1;    /* Next bit */

        getRootHubPortStatus((IOUSBHubPortStatus *)&portStatus, port);
        if ((portStatus & kOHCIHcRhPortStatus_Change) != 0)
            statusChangedBitmap |= statusBit; /* Hub status change bit */
    }

    /*
     * If a transaction is queued, handle it
     */
    if(_outstandingTrans[0].completion.action != nil)
    {
        last = _outstandingTrans[0];
        IOTakeLock(_intLock);
        for (index = 1; index < kMaxOutstandingTrans ; index++)
        {
            _outstandingTrans[index-1] = _outstandingTrans[index];
            if (_outstandingTrans[index].completion.action == nil)
                break;
        }

        move = last.bufLen;
        if (move > sizeof(statusChangedBitmap))
            move = sizeof(statusChangedBitmap);
        if (numPorts < 8)
            move = 1;

        statusChangedBitmap = HostToUSBWord(statusChangedBitmap);
        last.buf->writeBytes(0,&statusChangedBitmap, move);
        IOUnlock(_intLock); /* Unlock the queue */
        complete(last.completion, kIOReturnSuccess, last.bufLen - move);
    }
}

/*
 * SimulateRootHubInt
 * Simulate the interrupt pipe (status change pipe) of a hub for the root
 * hub.  The basic concept is to simulate the UIMCreateInterruptTransfer
 * so we keep our own little queue -> _outstandingTrans.  We are turning
 * on the RHSC interrupt so UIMRootHubStatusChange() should handle the
 * dequeueing.
 */
void AppleOHCI::SimulateRootHubInt(
            UInt8					endpoint,
            IOMemoryDescriptor *			buf,
            UInt32 					bufLen,
            IOUSBCompletion				completion)
{
    int 		index;
    OHCIRegistersPtr	pOHCIRegisters = pOHCIUIMData->pOHCIRegisters;

    if (endpoint != 1)
    {
        complete(completion, -1, bufLen);
        return;
    }

    IOTakeLock(_intLock);
    
    for (index = 0; index < kMaxOutstandingTrans; index++)
    {
        if (_outstandingTrans[index].completion.action == nil)
        {
            /* found free trans */
            _outstandingTrans[index].buf = buf;
            _outstandingTrans[index].bufLen = bufLen;
            _outstandingTrans[index].completion = completion;
            IOUnlock(_intLock); /* Unlock the queue */

            /* turn on RHSC interrupt */
            pOHCIRegisters->hcInterruptEnable =pOHCIRegisters->hcInterruptEnable
                | OSSwapInt32(kOHCIHcInterrupt_RHSC);
            IOSync();

            return;
        }
    }

    IOUnlock(_intLock); /* Unlock the queue */
    complete(completion, -1, bufLen); /* too many trans */
}

