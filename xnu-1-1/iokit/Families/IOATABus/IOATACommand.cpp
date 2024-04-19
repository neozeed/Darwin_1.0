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
 *
 * IOATACommand.cpp
 *
 */

#include <IOKit/ata/IOATA.h>
#include <IOKit/ata/IOATAController.h>

#undef super
#define super OSObject

OSDefineMetaClassAndStructors( IOATACommand, OSObject ) 

IOATADevice *IOATACommand::getDevice()
{
    return device;
}

IOCommandQueue *IOATACommand::getDeviceQueue()
{
    return deviceQueue;
}

IOATAController *IOATACommand::getController()
{
    return controller;
}

void *IOATACommand::getClientData()
{
    return clientData;
}

void *IOATACommand::getControllerData()
{
    return controllerData;
}

void IOATACommand::setTaskfile( ATATaskfile *srcTaskfile )
{
    taskfile  = *srcTaskfile;
    flags    |= atacmdTaskfileValid;
}

void IOATACommand::getTaskfile( ATATaskfile *dstTaskfile )
{
    *dstTaskfile = taskfile;
}

ATAProtocol IOATACommand::getProtocol()
{
    return taskfile.protocol;
}

UInt32 IOATACommand::getResultMask()
{
    return taskfile.resultmask;
}

void IOATACommand::setTimeout( UInt32 timeoutMS )
{
    timeout = timeoutMS;
}

UInt32 IOATACommand::getTimeout()
{
    return timeout;
}


void IOATACommand::setResults( ATAResults *srcResults )
{
    results   = *srcResults;

    flags    |= atacmdResultsValid;
}

ATAReturnCode IOATACommand::getResults( ATAResults *dstResults )
{
    *dstResults = results;
    return results.returnCode;
}

void IOATACommand::setPointers(  IOMemoryDescriptor *clientDesc, UInt32 transferCount, bool isWrite  )
{
    xferDesc       = clientDesc;
    xferCount      = transferCount;
    xferDirection  = isWrite;

    flags    |= atacmdPointersValid;    
}

void IOATACommand::getPointers(  IOMemoryDescriptor **clientDesc, UInt32 *transferCount, bool *isWrite  )
{
    if ( clientDesc != NULL )
    {
        *clientDesc = xferDesc;
    }
    
    if ( transferCount != NULL )
    {
        *transferCount = xferCount;
    }
 
    if ( isWrite != NULL )
    {
        *isWrite       = xferDirection;
    }
}

void IOATACommand::setATAPICmd( ATAPICmd *clientATAPICmd, IOMemoryDescriptor *clientSenseData, UInt32 clientSenseLength  )	
{
    atapiCmd    = *clientATAPICmd;
    senseLength = clientSenseLength;
    senseData   = clientSenseData;

    flags      |= atacmdATAPIInfoValid;
}

void IOATACommand::getATAPICmd( ATAPICmd *clientATAPICmd, IOMemoryDescriptor **clientSenseData, UInt32 *clientSenseLength )	
{
    if ( clientATAPICmd != NULL )
    {
        *clientATAPICmd = atapiCmd;
    }

    if ( clientSenseLength != NULL )
    {
        *clientSenseLength = senseLength;
    }

    if ( clientSenseData != NULL )
    {
        *clientSenseData = senseData;
    }
}

void IOATACommand::setCallback( void *clientTarget, ATACallback clientATADoneFn, void *clientRefcon )
{
    if ( clientATADoneFn == NULL )
    {
        flags &= ~atacmdCallbackValid;
    }
    else
    {
        completionInfo.async.target    = clientTarget;
        completionInfo.async.ataDoneFn = clientATADoneFn;
        completionInfo.async.refcon    = clientRefcon;

        flags      |= atacmdCallbackValid;
    }
}

bool IOATACommand::execute()
{
    return device->executeCommand( this );
}

void IOATACommand::complete()
{
    device->completeCommand( this );
}

void IOATACommand::clear()
{
    if ( dataSize )
    {
        bzero( dataArea, dataSize );
    }
      
   flags = 0;
}

void IOATACommand::setController( IOATAController *ctlr )
{
    controller = ctlr;
}

void IOATACommand::setDevice( IOATADevice *dev )
{
    device = dev;
}
    
