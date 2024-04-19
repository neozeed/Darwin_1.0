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
 * 12 Nov 1998 suurballe  Created.
 */

#include <IOKit/IOSyncer.h>
#include "IOPMUADBController.h"
#include "pmupriv.h"

#define super IOADBController
OSDefineMetaClassAndStructors(IOPMUADBController, IOADBController)

// **********************************************************************************
// init
//
// **********************************************************************************
bool IOPMUADBController::init ( OSDictionary * properties, ApplePMU * driver )
{
PMUdriver = driver;
pollList = 0;
autopollOn = false;

return super::init(properties);
}


// **********************************************************************************
// start
//
// **********************************************************************************
bool IOPMUADBController::start ( IOService * nub )
{
    if( !super::start(nub))
        return false;

    PMUdriver->registerForADBInterrupts ( autopollHandler, this );
    return true;
}


// **********************************************************************************
// setAutoPollPeriod
//
// **********************************************************************************
IOReturn IOPMUADBController::setAutoPollPeriod ( int )
{
return kPMUNotSupported;
}


// **********************************************************************************
// getAutoPollPeriod
//
// **********************************************************************************
IOReturn IOPMUADBController::getAutoPollPeriod ( int * )
{
return kPMUNotSupported;
}


// **********************************************************************************
// setAutoPollList
//
// **********************************************************************************
IOReturn IOPMUADBController::setAutoPollList ( UInt16 PollBitField )
{
PMUrequest	request;

pollList = PollBitField;				// remember the new poll list

if ( autopollOn ) {			
        request.sync = IOSyncer::create();
        request.pmCommand = kPMUpMgrADB;	// if PMU is currently autopolling,
        request.pmFlag = false;		// give it the new list
        request.pmSLength1 = 4;
        request.pmSBuffer2 = NULL;
        request.pmSLength2 = 0;
        request.pmRBuffer = NULL;
        request.pmSBuffer1[0] = 0;
        request.pmSBuffer1[1] = 0x86;
        request.pmSBuffer1[2] = (UInt8)(PollBitField >> 8);
        request.pmSBuffer1[3] = (UInt8)(PollBitField & 0xff);

        PMUdriver->enqueueCommand(&request);
        request.sync->wait();			// wait till done
}
return kPMUNoError;
}


// **********************************************************************************
// getAutoPollList
//
// **********************************************************************************
IOReturn IOPMUADBController::getAutoPollList ( UInt16 * activeAddressMask )
{
*activeAddressMask = pollList;
return kPMUNoError;
}


// **********************************************************************************
// setAutoPollEnable
//
// **********************************************************************************
IOReturn IOPMUADBController::setAutoPollEnable ( bool enable )
{
PMUrequest request;

if ( enable ) {							// enabling autopoll
        autopollOn = true;
        request.sync = IOSyncer::create();
        request.pmCommand = kPMUpMgrADB;       			 // give it the list we have
        request.pmFlag = false;
        request.pmSLength1 = 4;
        request.pmSBuffer2 = NULL;
        request.pmSLength2 = 0;
        request.pmRBuffer = NULL;
        request.pmSBuffer1[0] = 0;
        request.pmSBuffer1[1] = 0x86;
        request.pmSBuffer1[2] = (UInt8)(pollList >> 8);
        request.pmSBuffer1[3] = (UInt8)(pollList & 0xff);

        PMUdriver->enqueueCommand(&request);
        request.sync->wait();			// wait till done
        return kPMUNoError;
}
    else {								// disabling autopoll
        autopollOn = false;
        request.sync = IOSyncer::create();
        request.pmCommand = kPMUpMgrADBoff;
        request.pmFlag = false;
        request.pmSLength1 = 0;
        request.pmSBuffer2 = NULL;
        request.pmSLength2 = 0;
        request.pmRBuffer = NULL;

        PMUdriver->enqueueCommand(&request);
        request.sync->wait();			// wait till done

        return kPMUNoError;
}
}


// **********************************************************************************
// resetBus
//
// **********************************************************************************
IOReturn IOPMUADBController::resetBus ( void )
{
PMUrequest request;

request.sync = IOSyncer::create();
request.pmCommand = kPMUpMgrADB;
request.pmFlag = true;					// this op solicits input from PGE
request.pmSLength1 = 3;
request.pmSBuffer2 = NULL;
request.pmSLength2 = 0;
request.pmRBuffer = NULL;
request.pmSBuffer1[0] = kPMUResetADBBus;
request.pmSBuffer1[1] = 0;
request.pmSBuffer1[2] = 0;

PMUdriver->enqueueCommand(&request);
request.sync->wait();			// wait till done

return kPMUNoError;
}


// **********************************************************************************
// flushDevice
//
// **********************************************************************************
IOReturn IOPMUADBController::flushDevice ( IOADBAddress address )
{
PMUrequest request;

request.sync = IOSyncer::create();
request.pmCommand = kPMUpMgrADB;
request.pmFlag = true;
request.pmSLength1 = 3;
request.pmSBuffer2 = NULL;
request.pmSLength2 = 0;
request.pmRBuffer = NULL;
request.pmSBuffer1[0] = kPMUFlushADB | (address << kPMUADBAddressField);
if ( autopollOn ) {
	request.pmSBuffer1[1] = 2;
}
else {
        request.pmSBuffer1[1] = 0;
}
request.pmSBuffer1[2] = 0;

PMUdriver->enqueueCommand(&request);
request.sync->wait();			// wait till done

return kPMUNoError;
}


// **********************************************************************************
// readFromDevice
//
// The length parameter is ignored on entry.  It is set on exit to reflect
// the number of bytes read from the device.
// **********************************************************************************
IOReturn IOPMUADBController::readFromDevice ( IOADBAddress address, IOADBRegister adbRegister,
			UInt8 * data, IOByteCount * length )
{
PMUrequest	request;
UInt8 *		source;
UInt8 *		dest;
int		copylength, i;

if ( (length == NULL) || (data == NULL) ) {
	return kPMUParameterError;
}

request.sync = IOSyncer::create();
request.pmCommand = kPMUpMgrADB;
request.pmFlag = true;					// this op solicits input from PGE
request.pmSLength1 = 3;
request.pmSBuffer2 = NULL;
request.pmSLength2 = 0;
request.pmSBuffer1[0] = kPMUReadADB | (address << kPMUADBAddressField) | (adbRegister);
if ( autopollOn ) {
	request.pmSBuffer1[1] = 2;
}
else {
        request.pmSBuffer1[1] = 0;
}
request.pmSBuffer1[2] = 0;

PMUdriver->enqueueCommand(&request);
request.sync->wait();			// wait till done

*length = request.pmRLength;				// set caller's length
    
if (request.pmRLength == 0 ) {				// nothing read; device isn't there
	return ADB_RET_NOTPRESENT;
}
else {
	source = request.pmRBuffer;			// copy all but first two bytes of adb data
        dest = data;
        if ( *length > request.pmRLength ) {
            copylength = request.pmRLength;
        }
        else {
            copylength =*length;
        }
        for ( i = 0; i < copylength; i++ ) {
            *dest++ = *source++;
        }
        return ADB_RET_OK;
}
}


// **********************************************************************************
// writeToDevice
//
// **********************************************************************************
IOReturn IOPMUADBController::writeToDevice ( IOADBAddress address, IOADBRegister adbRegister,
			UInt8 * data, IOByteCount * length )
{
PMUrequest request;

if ( (* length == 0) || (data == NULL) || (* length > (MISC_LENGTH)) )
{
	return kPMUParameterError;
}

request.sync = IOSyncer::create();
request.pmCommand = kPMUpMgrADB;
request.pmFlag = true;					// this op solicits input from PGE
request.pmSLength1 = 3;
request.pmSBuffer2 = data;
request.pmSLength2 = *length;
request.pmRBuffer = NULL;
request.pmSBuffer1[0] = kPMUWriteADB | (address << kPMUADBAddressField) | (adbRegister);
if ( autopollOn ) {
	request.pmSBuffer1[1] = 2;
}
else {
        request.pmSBuffer1[1] = 0;
}
request.pmSBuffer1[2] = *length;

PMUdriver->enqueueCommand(&request);
request.sync->wait();			// wait till done

return kPMUNoError;
}


