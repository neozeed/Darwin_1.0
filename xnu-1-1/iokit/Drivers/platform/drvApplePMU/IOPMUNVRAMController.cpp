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
 *  1 Dec 1998 suurballe  Created.
 */

#include <IOKit/IOSyncer.h>
#include "IOPMUNVRAMController.h"
#include "pmupriv.h"

#define super IONVRAMController
OSDefineMetaClassAndStructors(IOPMUNVRAMController, IONVRAMController)


// **********************************************************************************
// init
//
// **********************************************************************************
bool IOPMUNVRAMController::init ( OSDictionary * properties, ApplePMU * driver )
{
    PMUdriver = driver;

    return super::init(properties);
}



// **********************************************************************************
// openNVRAM
//
//
// **********************************************************************************
IOReturn IOPMUNVRAMController::openNVRAM ( void )
{
    return kPMUNoError;
}


// **********************************************************************************
// readNVRAM
//
// The NVRAM driver is calling to read part of the NVRAM.  We translate this into
// single-byte PMU commands and enqueue them to its command queue.
//
// **********************************************************************************
IOReturn IOPMUNVRAMController::readNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer )
{
PMUrequest	request;
IOByteCount	i;
UInt8 *		client_buffer = Buffer;
UInt32		our_offset = Offset;

if ( (Buffer == NULL) ||
         (*Length == 0) ||
         (*Length > 8192) ||
         (Offset > 8192) ||
         ((*Length + Offset) > 8192) ) {
        return kPMUParameterError;
}

for ( i = 0; i < *Length ; i++ ) {
        request.sync = IOSyncer::create();
        request.pmCommand = kPMUNVRAMRead;
        request.pmFlag = false;
        request.pmSLength1 = 2;
        request.pmSBuffer2 = NULL;
        request.pmSLength2 = 0;
        request.pmRBuffer = client_buffer++;
        request.pmSBuffer1[0] = our_offset >> 8;
        request.pmSBuffer1[1] = our_offset++;

        PMUdriver->enqueueCommand(&request);
        request.sync->wait();			// wait till done
}

return kPMUNoError;
}


// **********************************************************************************
// writeNVRAM
//
// The NVRAM driver is calling to write part of the NVRAM.  We translate this into
// single-byte PMU commands and enqueue them to our command queue.
//
// **********************************************************************************
IOReturn IOPMUNVRAMController::writeNVRAM ( UInt32 Offset, IOByteCount * Length, UInt8 * Buffer )
{
PMUrequest	request;
IOByteCount	i;
UInt32		our_offset = Offset;
UInt8 *		client_buffer = Buffer;

if ( (Buffer == NULL) ||
         (*Length == 0) ||
         (*Length > 8192) ||
         (Offset > 8192) ||
         ((*Length + Offset) > 8192) ) {
        return kPMUParameterError;
}

for ( i = 0; i < *Length ; i++ ) {
        request.sync = IOSyncer::create();
        request.pmCommand = kPMUNVRAMWrite;
        request.pmFlag = false;
        request.pmSLength1 = 3;
        request.pmSBuffer2 = NULL;
        request.pmSLength2 = 0;
        request.pmRBuffer = NULL;
        request.pmSBuffer1[0] = our_offset >> 8;
        request.pmSBuffer1[1] = our_offset++;
        request.pmSBuffer1[2] = *client_buffer++;

        PMUdriver->enqueueCommand(&request);
        request.sync->wait();			// wait till done
}

return kPMUNoError;
}
