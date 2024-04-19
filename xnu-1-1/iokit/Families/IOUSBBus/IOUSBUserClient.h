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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_IOUSBUSERCLIENT_H
#define _IOKIT_IOUSBUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/usb/USB.h>

class IOUSBDevice;
class IOUSBInterface;
class OSSet;

class IOUSBUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBUserClient)

protected:
    task_t		fTask;
    IOExternalMethod	fMethods[ kNumUSBMethods ];
    IOExternalAsyncMethod fAsyncMethods[ kNumUSBMethods ];
    IOUSBPipe *		fPipes[kUSBMaxPipes];

    static void ReqComplete(void *obj, void *param, IOReturn status,
                            UInt32 remaining);

public:
    virtual IOExternalMethod * getExternalMethodForIndex( UInt32 index );
    virtual IOExternalAsyncMethod * getExternalAsyncMethodForIndex( UInt32 index );


    virtual bool start( IOService * provider );

    /*
     * There's a limit of max 6 arguments to user client methods,
     * so the type, recipient and request are packed into one 16 bit integer
     */
    virtual IOReturn ControlReqIn(UInt32 pipe, UInt16 bmreqtypeRequest, UInt16 wValue,
		UInt16 wIndex, void *buf, UInt32 *size);
    virtual IOReturn ControlReqOut(UInt32 pipe, UInt16 bmreqtypeRequest, UInt16 wValue,
                UInt16 wIndex, void *buf, UInt32 size);
    /*
     * Way too many arguments, and the data is out of line anyway.
     * Arguments are passed in a parameter block
     */
    virtual IOReturn ControlReqInOOL(DevReqOOL *req, UInt32 *sizeOut, 
		IOByteCount inCount, IOByteCount *outCount);
    virtual IOReturn ControlReqOutOOL(DevReqOOL *req, IOByteCount inCount);

    virtual IOReturn ControlReqInAsync(OSAsyncReference asyncRef,
                DevReqOOL *req, IOByteCount inCount);
    virtual IOReturn ControlReqOutAsync(OSAsyncReference asyncRef,
                DevReqOOL *req, IOByteCount inCount);
    
    virtual IOReturn ReadPipe(UInt32 pipe, void *buf, UInt32 *size);
    virtual IOReturn WritePipe(UInt32 pipe, void *buf, UInt32 size);
    virtual IOReturn ReadPipeOOL(UInt32 pipe, void *buf,
					 UInt32 sizeIn, UInt32 *sizeOut);
    virtual IOReturn WritePipeOOL(UInt32 pipe, void *buf, UInt32 size);

    virtual IOReturn ReadPipeAsync(OSAsyncReference asyncRef,
                                    UInt32 pipe, void *buf, UInt32 size);
    virtual IOReturn WritePipeAsync(OSAsyncReference asyncRef,
                                    UInt32 pipe, void *buf, UInt32 size);

    // Controlling pipe state
    virtual IOReturn getPipeStatus(UInt32 pipe);
    virtual IOReturn abortPipe(UInt32 pipe);
    virtual IOReturn resetPipe(UInt32 pipe);
    virtual IOReturn setPipeIdle(UInt32 pipe);
    virtual IOReturn setPipeActive(UInt32 pipe);
    virtual IOReturn clearPipeStall(UInt32 pipe);

    virtual IOReturn ClosePipe(UInt32 pipe);
    virtual IOReturn OpenPipe(UInt32 configIndex,
                              UInt32 intNo, UInt32 altSet,UInt32 endPtNo, UInt32 *pipe) = 0;
    virtual IOReturn GetConfigDescriptor(UInt8 configIndex,
                IOUSBConfigurationDescriptorPtr desc, UInt32 *size) = 0;
};

class IOUSBDeviceUserClient : public IOUSBUserClient
{
    OSDeclareDefaultStructors(IOUSBDeviceUserClient)

protected:
    IOUSBDevice *	fOwner;
    OSSet *		fInterfaces;

public:
    static IOUSBDeviceUserClient *withTask(task_t owningTask);

    virtual IOReturn clientClose( void );

    virtual bool start( IOService * provider );

    virtual IOReturn GetConfigDescriptor(UInt8 configIndex,
                IOUSBConfigurationDescriptorPtr desc, UInt32 *size);

    virtual IOReturn OpenPipe(UInt32 configIndex,
                              UInt32 intNo, UInt32 altSet,UInt32 endPtNo, UInt32 *pipe);
};

class IOUSBInterfaceUserClient : public IOUSBUserClient
{
    OSDeclareDefaultStructors(IOUSBInterfaceUserClient)

protected:
    IOUSBInterface *	fOwner;
    bool		fOpen;

public:
    static IOUSBInterfaceUserClient *withTask(task_t owningTask);

    virtual IOReturn clientClose( void );

    virtual bool start( IOService * provider );

    virtual IOReturn GetConfigDescriptor(UInt8 configIndex,
                IOUSBConfigurationDescriptorPtr desc, UInt32 *size);

    virtual IOReturn OpenPipe(UInt32 configIndex,
                              UInt32 intNo, UInt32 altSet,UInt32 endPtNo, UInt32 *pipe);
};

#endif /* ! _IOKIT_IOUSBUSERCLIENT_H */

