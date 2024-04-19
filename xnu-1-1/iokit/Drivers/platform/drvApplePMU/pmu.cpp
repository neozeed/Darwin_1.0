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
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
extern "C" {
#include <machine/machine_routines.h>
#include <kern/clock.h>
}
#include <IOKit/assert.h>
#include <IOKit/IOSyncer.h>

#include "pmupriv.h"
#include "pmutables.h"
#include "IOPMUADBController.h"
#include "IOPMUNVRAMController.h"
#include "IOPMURTCController.h"
//#include "IOPMUPwrController.h"

//extern void kprintf(const char *, ...);

void receiveMsg ( OSObject * unused, void * newRequest, void * unused1,
						void * unused2, void * unused3 );
//void shiftRegisterInt ( OSObject * PMUdriver, IOInterruptEventSource * unused, int alsoUnused );
void commandDoneInt ( OSObject * PMUdriver, IOInterruptEventSource * unused, int alsoUnused );
void unsolicitedInt ( OSObject * PMUdriver, IOInterruptEventSource * unused, int alsoUnused );

void PMUint ( OSObject * PMUdriver, IOInterruptEventSource * unused, int alsoUnused );
void serviceShiftRegister ( ApplePMU * );

static int PMU_PE_poll_input ( unsigned int, char *  );
static int PMU_PE_halt_restart ( unsigned int type );
static int PMU_PE_write_IIC ( unsigned char, unsigned char, unsigned char );
static int PMU_PE_read_write_time_of_day ( unsigned int, long * );

// Interrupt Vectors
#define VIA_DEV_VIA0	2
#define VIA_DEV_VIA2	4

#define super IOService
OSDefineMetaClassAndStructors(ApplePMU,IOService)

static ApplePMU * gOurself;

// **********************************************************************************
// init
//
// **********************************************************************************
bool ApplePMU::init ( OSDictionary * properties = 0 )
{
return super::init(properties);
}


// **********************************************************************************
// start
//
// **********************************************************************************
bool ApplePMU::start ( IOService * nub )
{
IOMemoryMap *	viaMap;
unsigned char *	physicalAddress;
PMUrequest	theRequest;

workLoop = NULL;
commandQueue = NULL;
//SRintEventSrc = NULL;
cmdDoneEventSrc = NULL;
unsolicitedEventSrc = NULL;
PMUintEventSrc = NULL;
ourADBinterface = NULL;
ourRTCinterface = NULL;
/*
ourNVRAMinterface = NULL;
ourPwrinterface = NULL;
*/
ADBclient = NULL;
RTCclient = NULL;
queueHead = NULL;
queueTail = NULL;
PGE_ISR_state = kPMUidle;
adb_reading = false;
PMU_int_pending = false;

adb_read_timeout.tv_sec = 0;
adb_read_timeout.tv_nsec = 100000000;

gOurself = this;

if( 0 == (viaMap = nub->mapDeviceMemoryWithIndex( 0 )) ) {
        IOLog("%s: no via memory\n", getName());
        return false;
}
physicalAddress = (unsigned char *)viaMap->getVirtualAddress();

//kprintf("VIA base = %08x\n", (UInt32)physicalAddress);

VIA1_shift		= physicalAddress + 0x1400;	// initialize VIA addresses
VIA1_auxillaryControl	= physicalAddress + 0x1600;
VIA1_interruptFlag	= physicalAddress + 0x1A00;
VIA1_interruptEnable	= physicalAddress + 0x1C00;
VIA2_dataB		= physicalAddress + 0x0000;
    
PMreq	= 1 << HooperReq;
PMack	= 1 << HooperAck;

// max to wait for ack low after SR interrupt, 100us
clock_interval_to_absolutetime_interval(100, 1000, &SR_to_ack_transition_delay);

workLoop = IOWorkLoop::workLoop();	// make the workloop
if ( !workLoop ) {
        kprintf("can't create workloop\n");
	return false;
}
						// make the command queue
commandQueue = IOCommandQueue::commandQueue(this, receiveMsg);
if (!commandQueue ||
	(workLoop->addEventSource(commandQueue) != kIOReturnSuccess) ) {	// add it to the workloop
        kprintf("can't create or add commandQueue\n");
	return false;
}
#if 0
// make the shift register interrupt source
SRintEventSrc = IOInterruptEventSource::
interruptEventSource(this, shiftRegisterInt, nub, VIA_DEV_VIA0);
if ( !SRintEventSrc ||
(workLoop->addEventSource(SRintEventSrc) != kIOReturnSuccess) ) {	// add it to the workloop
kprintf("can't create or add SRintEventSrc\n");
return false;
}
#endif

nub->registerInterrupt(VIA_DEV_VIA0,this,(IOInterruptAction) serviceShiftRegister);
nub->enableInterrupt(VIA_DEV_VIA0);

// make the command-done interrupt source
cmdDoneEventSrc = IOInterruptEventSource::interruptEventSource(this, commandDoneInt);
if ( !cmdDoneEventSrc ||						// add it to the workloop
     (workLoop->addEventSource(cmdDoneEventSrc) != kIOReturnSuccess) ) { 
    kprintf("can't create or add cmdDoneEventSrc\n");
return false;
}

// make the unsolicited interrupt source
unsolicitedEventSrc = IOInterruptEventSource::interruptEventSource(this, unsolicitedInt);
if ( !unsolicitedEventSrc ||						// add it to the workloop
     (workLoop->addEventSource(unsolicitedEventSrc) != kIOReturnSuccess) ) {
    kprintf("can't create or add unsolicitedEventSrc\n");
return false;
}
						// make the PMU interrupt source
PMUintEventSrc = IOInterruptEventSource::
    interruptEventSource(this, PMUint, nub, VIA_DEV_VIA2);
if ( !PMUintEventSrc ||
	(workLoop->addEventSource(PMUintEventSrc) != kIOReturnSuccess) ) {	// add it to the workloop
        kprintf("can't create or add PMUintEventSrc\n");
	return false;
}

AcknowledgePMUInterrupt();                 		// turn off any pending PGE interrupt
workLoop->enableAllInterrupts();			// enable interrupt delivery
EnablePMUInterrupt();          				// enable PGE interrupts
EnableSRInterrupt();

theRequest.sync = IOSyncer::create();
theRequest.pmCommand = kPMUSetModem1SecInt;		// tell PGE why it may interrupt
theRequest.pmFlag = false;
theRequest.pmSLength1 = 1;
//theRequest.pmSBuffer1[0] = kPMUMD2Int | kPMUbrightnessInt | kPMUADBint;
theRequest.pmSBuffer1[0] = kPMUMD2Int | kPMUbrightnessInt | kPMUADBint | kPMUoneSecInt;
theRequest.pmSLength2 = 0;
commandQueue->enqueueCommand(true, &theRequest);
theRequest.sync->wait();			// wait till done

theRequest.sync = IOSyncer::create();
theRequest.pmCommand = kPMUreadINT;			// read any pending interrupt from PGE
theRequest.pmFlag = false;
theRequest.pmSLength1 = 0;					// just to clear it
theRequest.pmSLength2 = 0;
theRequest.pmRBuffer = &interruptState[0];
commandQueue->enqueueCommand(true, &theRequest);
theRequest.sync->wait();			// wait till done

							// initialize our interfaces

 // Do not create the IOPMUADBController if the no-adb property is set.
 if (!nub->getProperty("no-adb")) {
   ourADBinterface = new IOPMUADBController;
   if ( !ourADBinterface ) {
     return false;
   }
   if ( !ourADBinterface->init(0,this) ) {
     return false;
   }
   if ( !ourADBinterface->attach( this) ) {
     return false;
   }
 }
 
 // Do not create the IOPMUNVRAMController if the no-nvram property is set.
 if (!nub->getProperty("no-nvram")) { 
   ourNVRAMinterface = new IOPMUNVRAMController;
   if ( !ourNVRAMinterface ) {
     return false;
   }
   if ( !ourNVRAMinterface->init(0,this) ) {
     return false;
   }
   if ( !ourNVRAMinterface->attach( this) ) {
     return false;
   }
 }
 

ourRTCinterface = new IOPMURTCController;
if ( !ourRTCinterface ) {
	return false;
}
if ( !ourRTCinterface->init(0,this) ) {
	return false;
}

/*
ourPwrinterface = new IOPMUPwrController;
if ( !ourPwrinterface ) {
	return false;
}
if ( !ourPwrinterface->init(ourRegEntry,this) ) {
	return false;
}
*/

    if (ourADBinterface) ourADBinterface->start( this );
    if (ourNVRAMinterface) ourNVRAMinterface->start( this );
    if (ourRTCinterface) ourRTCinterface->start( this );
    if (ourRTCinterface) {
            PE_read_write_time_of_day = PMU_PE_read_write_time_of_day;
    }

    PE_poll_input =PMU_PE_poll_input;
    PE_halt_restart =PMU_PE_halt_restart;
    PE_write_IIC = PMU_PE_write_IIC;

    /* are these even implemented? */
    publishResource( "IOiic0", this );
    publishResource( "IORTC", this );

    OSSerializer * infoSerializer = OSSerializer::forTarget(
                (void *) this, &serializeBatteryInfo );
    if( infoSerializer) {
        IORegistryEntry * entry;
        if( (entry = IORegistryEntry::fromPath("mac-io/battery",
                                            gIODTPlane))) {
            entry->setProperty( kIOBatteryInfoKey, infoSerializer );
            entry->release();
        }
        infoSerializer->release();
    }

    publishResource(kPMUname, this);

    return true;
}


// *****************************************************************************
// getWorkLoop
//
// Return the PMU's workloop.
//
// *****************************************************************************
IOWorkLoop *ApplePMU::getWorkLoop() const
{
    return workLoop;
}

// *****************************************************************************
// free
//
// Release everything we may have allocated.
//
// *****************************************************************************
void ApplePMU::free ( void )
{
if ( workLoop ) {
	workLoop->release();
}
if ( commandQueue ) {
	commandQueue->release();
}
#if 0
if ( SRintEventSrc ) {
	SRintEventSrc->release();
}
#endif
if ( cmdDoneEventSrc ) {
    cmdDoneEventSrc->release();
}
if ( unsolicitedEventSrc ) {
    unsolicitedEventSrc->release();
}
if ( PMUintEventSrc ) {
	PMUintEventSrc->release();
}
if ( ourADBinterface ) {
	ourADBinterface->release();
}
if ( ourNVRAMinterface ) {
    ourNVRAMinterface->release();
}

if ( ourRTCinterface ) {
	ourRTCinterface->release();
}
/*
if ( ourPwrinterface ) {
	ourPwrinterface->release();
}
*/
super::free();
}


// **********************************************************************************
// PMU_PE_halt_restart
//
// **********************************************************************************
static int PMU_PE_halt_restart ( unsigned int type )
{
PMUrequest	theRequest;
UInt8		reply_byte;

switch( type ) {

    case kPERestartCPU:
        theRequest.sync = IOSyncer::create();
        theRequest.pmCommand = kPMUresetCPU;
        theRequest.pmFlag = false;
        theRequest.pmSLength1 = 0;
        theRequest.pmSLength2 = 0;
        theRequest.pmRBuffer = &reply_byte;
        gOurself->enqueueCommand(&theRequest);
        theRequest.sync->wait();			// wait till done
        break;

    case kPEHaltCPU:
        theRequest.sync = IOSyncer::create();
        theRequest.pmCommand = kPMUPmgrPWRoff;
        theRequest.pmFlag = false;
        theRequest.pmSLength1 = 4;
        theRequest.pmSBuffer1[0] = 'M';
        theRequest.pmSBuffer1[1] = 'A';
        theRequest.pmSBuffer1[2] = 'T';
        theRequest.pmSBuffer1[3] = 'T';
        theRequest.pmSLength2 = 0;
        theRequest.pmRBuffer = &reply_byte;
        gOurself->enqueueCommand(&theRequest);
        theRequest.sync->wait();			// wait till done
        break;

    default:
        return 1;
}

// workaround 2377033; avoid memory accesses after this point
ml_set_interrupts_enabled(false);
while(true) {}

return 1;
}


// **********************************************************************************
// PMU_PE_read_write_time_of_day
//
// **********************************************************************************
static int PMU_PE_read_write_time_of_day ( unsigned int options, long * secs )
{
    UInt8	currentTime[8];
    IOByteCount	length,i;
    long	longTime = 0;

    if( (options == kPEReadTOD) && (gOurself->ourRTCinterface != NULL) )
    {
        gOurself->ourRTCinterface->getRealTimeClock(currentTime,&length);
        
        for ( i = 0; i < length; i++ )
        {
            longTime |= currentTime[i] << ((length-i-1)*8);
        }
        *secs = longTime;
    }

    if( (options == kPEWriteTOD) && (gOurself->ourRTCinterface != NULL) )
    {
        gOurself->ourRTCinterface->setRealTimeClock((UInt8 *)secs);
    }    

    return 0;
}


// **********************************************************************************
// PMU_PE_write_IIC
//
// **********************************************************************************
static int PMU_PE_write_IIC ( unsigned char, unsigned char, unsigned char )
{
    kprintf ("PMU_PE_write_IIC - ");

    return 1;
}


// **********************************************************************************
// PMU_PE_poll_input
//
// System interrupts are disabled, but we are still operating the PMU for mini-
// monitor keyboard input.  We are called here in a loop to service the PMU.
//
// **********************************************************************************
static int PMU_PE_poll_input ( unsigned int, char *  )
{
    return 1;  // XXX -- svail: what is the correct value???
}

#if 0
// **********************************************************************************
// poll_device
//
// System interrupts are disabled, but we are still operating the PMU for mini-
// monitor keyboard input.  We are called here in a loop to service the PMU.
//
// **********************************************************************************
void ApplePMU::poll_device( void )
{
if ( *VIA1_interruptFlag & 0x04 ) {   		// is shift register done? ( ifSR )
	serviceShiftRegister();         	// yes, handle it
	return;
}
if ( *VIA1_interruptFlag & 0x10 ) { 		// is PMU requesting service? ( ifCB1 )
	*VIA1_interruptFlag = 0x10;       	// yes, clear interrupt ( ifCB1 )
	PGE_ISR_state = kPMUidle;     		// and handle it
	servicePMU();
}
}
#endif

// **********************************************************************************
// receiveMsg
//
// 
//
// **********************************************************************************
void receiveMsg ( OSObject * theDriver, void * newRequest, void *, void *, void * )
{
ApplePMU * PMUdriver = (ApplePMU *) theDriver;

if ( (PMUdriver->PGE_ISR_state == kPMUidle) && !PMUdriver->adb_reading ) {
	PMUdriver->StartPMUTransmission((PMUrequest *)newRequest);
}
else {
	((PMUrequest *) newRequest)->prev = PMUdriver->queueTail;
        ((PMUrequest *) newRequest)->next = NULL;
        if ( PMUdriver->queueTail != NULL ) {
            PMUdriver->queueTail->next = (PMUrequest *) newRequest;
        }
        else {
            PMUdriver->queueHead = (PMUrequest *)newRequest;
        }
        PMUdriver->queueTail =  (PMUrequest *)newRequest;
}
}


// **********************************************************************************
// timeoutOccurred
//
// Our adb-read timer has expired after sending an adb-read command to the PMU.
// This means there is no such addressed device on the ADB bus.
// We call back to the ADB driver with a zero-characters-received response and
// dequeue our command queue and carry on.
// **********************************************************************************
void ApplePMU::timeoutOccurred ( void )
{
adb_reading = false;
clientRequest->pmRLength = 0;				// nothing was read
clientRequest->sync->signal();			// unblock the caller
clientRequest = 0;
CheckRequestQueue();
}


// ****************************************************************************
// CheckRequestQueue
// Called at interrupt time when current request is complete.  We start
// another request here if one is in queue.
// ****************************************************************************
void ApplePMU::CheckRequestQueue ( void )
{
PMUrequest * nextRequest;

if ( queueHead != NULL ) {			// is queue empty?
        nextRequest = queueHead;		// no, dequeue first command
        queueHead = nextRequest->next;
        if ( queueHead == NULL ) {
            queueTail = NULL;
        }
        StartPMUTransmission(nextRequest);	// and send it to the PMU
}
}


// **********************************************************************************
// enqueueCommand
//
// **********************************************************************************
void ApplePMU::enqueueCommand ( PMUrequest * request )
{
commandQueue->enqueueCommand(true,request);
}


// **********************************************************************************
// ADBinput
//
// The PGE has interrupted with ADB data.  We package this up and send
// it to our ADB client, if there is one, either as the result to its previous
// read command, or as autopoll data.
//
// **********************************************************************************
void ApplePMU::ADBinput(UInt32 theLength, UInt8 * theInput)
{
if ( theInput[0] & kPMUautopoll ) {					// autopoll data?
        if ( ADBclient != NULL ) {
//            kprintf("autopoll: %d %02x %02x %02x %02x\n", theLength, theInput[0], theInput[1], theInput[2], theInput[3]);

            (* ADBclient)(ADBid, theInput[1], theLength-2, theInput+2 );// yes, call adb input handler
        }
        return;
}
if ( adb_reading ) {							// no, expecting adb input?
        if ( clientRequest->pmSBuffer1[0] == theInput[1] ) {		// yes, is it our input?
           // q8q							// yes, turn off our timer
            clientRequest->pmRLength = theLength-2;			// this much was read
            clientRequest->pmRBuffer = theInput+2;			// to here
            clientRequest->sync->signal();				// unblock the caller
	    clientRequest = 0;
            adb_reading = false;
            return;
            }
        }
kprintf("unexpected adb input: %02d %02x %02x %02x %02x %02x\n", theLength, theInput[0], theInput[1], theInput[2], theInput[3], clientRequest->pmSBuffer1[0]);
}


// **********************************************************************************
// registerForADBInterrupts
//
// Some driver is calling to say it is prepared to receive "unsolicited" adb
// interrupts (e.g. autopoll keyboard and trackpad data).  The parameters identify
// who to call when we get one.
// **********************************************************************************
void ApplePMU::registerForADBInterrupts ( ADB_callback_func client, IOService * caller )
{
ADBclient = client;
ADBid = caller;
}


// **********************************************************************************
// registerForPowerInterrupts
//
// Some driver is calling to say it is prepared to receive "unsolicited" power-system
// interrupts (e.g. battery low).  The parameters identify who to call when we get one.
// **********************************************************************************
void ApplePMU::registerForPowerInterrupts ( pmCallback_func buttonHandler, IOService * caller )
{
PWRclient = buttonHandler;
PWRid = caller;
}


// **********************************************************************************
// registerForClockInterrupts
//
// Some driver is calling to say it is prepared to receive "unsolicited" real-time clock
// interrupts (e.g. one-second tick).  The parameters identify who to call when we get one.
// **********************************************************************************
void ApplePMU::registerForClockInterrupts ( pmCallback_func tickHandler, IOService * caller )
{
RTCclient = tickHandler;
RTCid = caller;
}


// **********************************************************************************
// buttonInput
//
// The PGE has interrupted with Brightness/Contrast data.  We package this up and send
// it to our Display client, if there is one.
//
// **********************************************************************************
void ApplePMU::buttonInput ( UInt32 theLength, UInt8 * theInput )
{
if ( PWRclient != NULL ) {
	(* PWRclient)(PWRid, theLength-1, theInput+1);
}
}

// **********************************************************************************

bool ApplePMU::serializeBatteryInfo( void * target, void * ref, OSSerialize * s )
{
    UInt32		flags, current, capacity;
    OSArray *		array;
    OSDictionary *	dict;
    OSNumber	*	num;
    UInt32		battery;
    UInt8		data[16];
    IOByteCount		readLen;
    IOReturn		err;
    bool 		ok = true;

    array = OSArray::withCapacity(2);
    if( !array)
        return( false );
    
    for( battery = 1; battery <= 2; battery++) {
        data[0] = battery;
        err = ((ApplePMU *) target)->sendMiscCommand( kPMUGetSOB,
                                        1, data, &readLen, data );
        if( kIOReturnSuccess == err) {
            switch( data[0] ) {
                case 3:
                case 4:
                    current = data[2];
                    capacity = data[3];
                    break;
                case 5:
                    current = (data[2] << 8) | data[3];
                    capacity = (data[4] << 8) | data[5];
                    break;
                default:
                    continue;
            }
            flags = data[1];
            dict = OSDictionary::withCapacity(2);
            if( !dict)
                continue;
            num = OSNumber::withNumber(flags, 32);
            if( num) {
                dict->setObject(kIOBatteryFlagsKey, num);
                num->release();
            }
            num = OSNumber::withNumber(current, 32);
            if( num) {
                dict->setObject(kIOBatteryCurrentChargeKey, num);
                num->release();
            }
            num = OSNumber::withNumber(capacity, 32);
            if( num) {
                dict->setObject(kIOBatteryCapacityKey, num);
                num->release();
            }
            array->setObject( dict );
            dict->release();
        }
    }

    ok = array->serialize(s);
    array->release();
    
    return( ok );
}


// **********************************************************************************
// sendMiscCommand
//
// Some driver is calling to send some miscellaneous command.  We copy this into a
// PMU command and enqueue it to our command queue.
//
// The read-length parameter is ignored on entry.  On exit it is set to the number
// of bytes read in response to transmission of the command.
// **********************************************************************************
IOReturn ApplePMU::sendMiscCommand ( UInt32 Command, IOByteCount  SLength,
			UInt8 * SBuffer, IOByteCount * RLength, UInt8 * RBuffer )
{
PMUrequest 	request;
SInt32		rsp_length;
SInt32		send_length;

rsp_length = rspLengthTable[Command];			// get cmd and response lengths from table
send_length = cmdLengthTable[Command];

if ( ((SLength != 0) && (SBuffer == NULL)) ||			// validate pointers
     ((rsp_length != 0) && (RBuffer == NULL)) ||
     (RLength == NULL) ) {
    return kPMUParameterError;
}
if ( (Command != kPMUdownloadFlash) &&
     ((send_length != -1) && ((IOByteCount )send_length != SLength)) ) {
    return kPMUParameterError;
}

if ( send_length > MISC_LENGTH ) {
    return kPMUParameterError;
}

request.sync = IOSyncer::create();
request.pmCommand = Command;
request.pmFlag = false;		// this is usually correct, but the API needs to be enhanced
request.pmSLength1 = 0;
request.pmSBuffer2 = SBuffer;
request.pmSLength2 = SLength;
request.pmRBuffer = RBuffer;

commandQueue->enqueueCommand(true, &request);
request.sync->wait();			// wait till done

*RLength = request.pmRLength;				// set user's receive byte count
    
return kPMUNoError;
}


// **********************************************************************************
// StartPMUTransmission
//
// Transmission of the command byte is started.  The transaction will be
// completed by the Shift Register Interrupt Service Routine.
// **********************************************************************************
void ApplePMU::StartPMUTransmission ( PMUrequest * plugInMessage )
{
clientRequest = plugInMessage;
firstChar = plugInMessage->pmCommand;		// get command byte
charCountS1 = plugInMessage->pmSLength1;	// get caller's length counters
charCountS2 = plugInMessage->pmSLength2;
dataPointer1 = plugInMessage->pmSBuffer1;	// and transmit data pointers
dataPointer2 = plugInMessage->pmSBuffer2;
dataPointer = plugInMessage->pmRBuffer;		// set up read pointer for data bytes
charCountR = rspLengthTable[firstChar];		// get response length from table
charCountR2 = charCountR;
					// figure out what happens after command byte transmission
if ( cmdLengthTable[firstChar] < 0 ) {		// will we be sending a length byte next?
        PGE_ISR_state = kPMUxmtLen;		// yes
}
else {						// no, will we be sending data next?
        if ( cmdLengthTable[firstChar] > 0 ) {
            PGE_ISR_state = kPMUxmtData;	// yes
        }
        else {					// no, will we be receiving a length byte next?
            if ( charCountR < 0 ) {
                PGE_ISR_state = kPMUreadLen_cmd;	// yes
            }
            else {					// no, will we be receiving data next?
                if ( charCountR > 0 ) {
                    PGE_ISR_state = kPMUreadData;	// yes
                }
                else {
                    PGE_ISR_state = kPMUdone;		// no, this is a single-byte transaction
                }
            }
        }
}
   					// ready to start the command byte
*VIA1_auxillaryControl |= 0x1C;		// set shift register to output
*VIA1_shift = firstChar;		// give it the byte (this clears any pending SR interrupt)
// *VIA1_interruptEnable = 0x84;	// enable SR interrupt
*VIA2_dataB &= ~PMreq;			// assert /REQ
return;
}

#if 0
// ****************************************************************************
//	shiftRegisterInt
//
// ****************************************************************************

void shiftRegisterInt ( OSObject * PMUdriver, IOInterruptEventSource *, int )
{
((ApplePMU *)PMUdriver)->serviceShiftRegister();
}
#endif


// ****************************************************************************
//	commandDoneInt
//
// ****************************************************************************

void commandDoneInt ( OSObject * PMUdriver, IOInterruptEventSource *, int )
{
    ApplePMU * pmu = (ApplePMU *)PMUdriver;
    pmu->PGE_ISR_state = kPMUidle;			// set the state
    if ( pmu->clientRequest->pmFlag ) {			// does this command cause input?
        if ( ! pmu->adb_reading ) {			// yes, is this the input completion?
            pmu->adb_reading = true;			// no, don't unblock caller now
//q8q							// start timer
            if ( pmu->PMU_int_pending ) {			// is PMU now requesting service?
                pmu->PMU_int_pending = false;
                *(pmu->VIA1_auxillaryControl) |= 0x1C;		// set shift register to output
                *(pmu->VIA1_shift) = kPMUreadINT;		// give it this command byte
                *(pmu->VIA2_dataB) &= ~(pmu->PMreq);		// assert /REQ
//		*VIA1_interruptEnable = 0x84;			// enable SR interrupt
                pmu->PGE_ISR_state = kPMUreadLen_int;		// next int is cmd byte xmt done
                pmu->dataPointer = &(pmu->interruptState[0]);	// set up read ptr for data bytes
            }
            return;
        }
    }

    pmu->clientRequest->sync->signal();		// unblock the caller
    pmu->clientRequest = 0;
    if ( !(pmu->PMU_int_pending) ) {			// is PMU now requesting service?
        pmu->CheckRequestQueue();			// no, start next queued transaction
    }
    else {
        pmu->PMU_int_pending = false;
        *(pmu->VIA1_auxillaryControl) |= 0x1C;		// set shift register to output
        *(pmu->VIA1_shift) = kPMUreadINT;		// give it this command byte
        *(pmu->VIA2_dataB) &= ~(pmu->PMreq);		// assert /REQ
//	*VIA1_interruptEnable = 0x84;			// enable SR interrupt
        pmu->PGE_ISR_state = kPMUreadLen_int;		// next int is cmd byte xmt done
        pmu->dataPointer = &(pmu->interruptState[0]);	// set up read ptr for data bytes
    }
}


// ****************************************************************************
//	unsolicitedInt
//
// ****************************************************************************

void unsolicitedInt ( OSObject * PMUdriver, IOInterruptEventSource *, int )
{
    ApplePMU * pmu = (ApplePMU *)PMUdriver;
    if ( pmu->interruptState[0] & kPMUADBint ) {	// what kind of int was it?
        pmu->ADBinput((UInt32)(pmu->charCountR), &(pmu->interruptState[0]));	// ADB
    }
    else {
        if ( pmu->interruptState[0] & kPMUbattInt ) {
//            kprintf("battery PGE interrupt\n");
        }
        else {
            if ( pmu->interruptState[0] & kPMUoneSecInt ) {
//	        kprintf("one-second PGE interrupt\n");
                if ( pmu->RTCclient != NULL ) {		// one-second interrupt
//                  (* RTCclient)(RTCid,0,0,0);
                }
            }
            else {
                if ( pmu->interruptState[0] & kPMUenvironmentInt ) {
//                    kprintf("environment interrupt\n");
                }
                else {
                    if ( pmu->interruptState[0] & kPMUbrightnessInt ) {
//                        kprintf("brightness button PGE interrupt\n");
                        pmu->buttonInput((UInt32)(pmu->charCountR),&(pmu->interruptState[0]));
                    }
                    else {
//                        kprintf("machine-dependent PGE interrupt\n");
                    }
                }
            }
        }
    }
    pmu->PGE_ISR_state = kPMUidle;			// set the state

    if ( !(pmu->PMU_int_pending) ) {			// is PMU requesting service again?
      if (0 == pmu->clientRequest)
        pmu->CheckRequestQueue();			// no, start next queued command
    }
    else {
        pmu->PMU_int_pending = false;
        *(pmu->VIA1_auxillaryControl) |= 0x1C;		// set shift register to output
        *(pmu->VIA1_shift) = kPMUreadINT;		// give it this command byte
        *(pmu->VIA2_dataB) &= ~(pmu->PMreq);		// assert /REQ
//	*VIA1_interruptEnable = 0x84;			// enable SR interrupt
        pmu->PGE_ISR_state = kPMUreadLen_int;		// next int is cmd byte xmt done
        pmu->dataPointer = &(pmu->interruptState[0]);	// set up read ptr for data bytes
    }
}

bool ApplePMU::WaitForAckLo ( void )
{
    AbsoluteTime	deadline, now;
    bool		ok = true;

    clock_absolutetime_interval_to_deadline(
            SR_to_ack_transition_delay, &deadline);

    while( ok && ((*VIA2_dataB) & PMack) ) {
        clock_get_uptime( &now );
	ok = (CMP_ABSOLUTETIME( &now, &deadline ) < 0);
    }
    eieio();

    return( ok );
}

// ****************************************************************************
//	serviceShiftRegister
//	The shift register has finished shifting in a byte from PG&E or finished
//	shifting out a byte to PG&E.  Here we continue the transaction by starting
//	the i/o of the next byte, or we finish the transaction by indicating an
//	interrupt on either the command-done interrupt source or the autopoll
//	interrupt source on the workloop.
//	Both the VIA interrupt flag register and the interrupt enable registers
//	have been cleared by the ohare ISR.
// ****************************************************************************

void serviceShiftRegister ( ApplePMU * pmu )
{

    if( (*(pmu->VIA2_dataB) & pmu->PMack) )
	pmu->WaitForAckLo();

    *(pmu->VIA2_dataB) |= pmu->PMreq;			// deassert /REQ line
							// what state are we in?
    switch ( pmu->PGE_ISR_state ) {
        // We are processing a PMU interrupt.  We are reading the response
        // to the kPMUreadINT command, and a byte has arrived.
        case kPMUrcvData_int:
            *(pmu->dataPointer)++ = *(pmu->VIA1_shift);		// read the data byte
            (pmu->charCountR2)--;
            if ( pmu->charCountR2 > 0 ) {			// is there more to read?
                while ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
                }
                *(pmu->VIA2_dataB) &= ~(pmu->PMreq);		// yes, assert /REQ
//	        *VIA1_interruptEnable = 0x84;			// enable SR interrupt
                return;						// next int is next data byte
            }
            pmu->unsolicitedEventSrc->interruptOccurred(0,0,0);	// no, inform workloop
            return;
            
				// We are processing a PMU interrupt.
				// We have finished transmitting the kPMUreadINT command byte,
				// and according to our table, we will be getting a response and
				// a length byte for it.  Finish the transmit handshake and set
        case kPMUreadLen_int:	// up a receive for the length byte.
            pmu->receivedByte = *(pmu->VIA1_shift);	// read shift reg to turn off SR int
            pmu->PGE_ISR_state = kPMUrcvLen_int;
            *(pmu->VIA1_auxillaryControl) &= 0xEF;	// set shift register to input
            while ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
            }
            *(pmu->VIA2_dataB) &= ~(pmu->PMreq);	// assert /REQ
//	    *VIA1_interruptEnable = 0x84;		// enable SR interrupt
            return;					// next interrupt will be the length byte

				// We are processing a PMU interrupt.
        case kPMUrcvLen_int:	// The length byte has arrived.  Read it and start data read
            pmu->charCountR = *(pmu->VIA1_shift);	// read it

            pmu->charCountR2 = pmu->charCountR;
            pmu->PGE_ISR_state = kPMUrcvData_int;
            while ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
            }
            *(pmu->VIA2_dataB) &= ~(pmu->PMreq);	// assert /REQ
//	    *VIA1_interruptEnable = 0x84;		// enable SR interrupt
            return;					// next int will be the first data byte

				// We are doing a command transaction.  The command byte
        case kPMUxmtLen:	// transmission  has completed.  Start length byte transmission
            pmu->PGE_ISR_state = kPMUxmtData;
            while ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
            }
            *(pmu->VIA1_shift) = (UInt8)(pmu->charCountS1 + pmu->charCountS2);	// give it the length byte
            *(pmu->VIA2_dataB) &= ~(pmu->PMreq);		// assert /REQ
//	    *VIA1_interruptEnable = 0x84;			// enable SR interrupt
            return;						// next int start sending data

				// We are doing a command transaction.  A byte transmission has
        case kPMUxmtData:	// completed .  Continue data byte transmission
            while ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
            }
            if ( pmu->charCountS1 ) {
                *(pmu->VIA1_shift) = *(pmu->dataPointer1)++;	// give it the next data byte from buffer 1
                *(pmu->VIA2_dataB) &= ~(pmu->PMreq);		// assert /REQ
                if ( --(pmu->charCountS1) + pmu->charCountS2 ) {
//	        *VIA1_interruptEnable = 0x84;		// enable SR interrupt
                return;					// next interrupt do another byte
                }
            }
            else {
                if ( pmu->charCountS2 ) {				// buffer 1 empty,
                    *(pmu->VIA1_shift) = *(pmu->dataPointer2)++;	// give it the next byte from buffer 2
                    *(pmu->VIA2_dataB) &= ~(pmu->PMreq);		// assert /REQ
                    if ( --(pmu->charCountS2) ) {
//	                *VIA1_interruptEnable = 0x84;	// enable SR interrupt
                        return;				// next interrupt do another byte
                    }
                }
            }
						        // sending last byte, what's next?
            if ( pmu->charCountR < 0 ) {
                pmu->PGE_ISR_state = kPMUreadLen_cmd;	// we will receive a length byte
            }
            else {
                if ( pmu->charCountR > 0 ) {
                    pmu->PGE_ISR_state = kPMUreadData;	// we will receive constant-length data
                }
                else {
                    pmu->PGE_ISR_state = kPMUdone;	// nothing, we're done
                }
            }
//	    *VIA1_interruptEnable = 0x84;		// enable SR interrupt
            return;

			// We have finished the transmission part of a command transaction, and
			// according to our table, we will be getting a response and a
			// length byte for it.  Finish the transmit handshake and set up
        case kPMUreadLen_cmd:				// a receive for the length byte.
            pmu->receivedByte = *(pmu->VIA1_shift);	// read shift reg to turn off SR int
            pmu->PGE_ISR_state = kPMUrcvLen_cmd;
            *(pmu->VIA1_auxillaryControl) &= 0xEF;	// set shift register to input
            while ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
            }
            *(pmu->VIA2_dataB) &= ~(pmu->PMreq);	// assert /REQ
//	    *VIA1_interruptEnable = 0x84;		// enable SR interrupt
            return;					// next interrupt will be the length byte

        case kPMUrcvLen_cmd:		// the length byte has arrived, read it and start data read
            pmu->charCountR = *(pmu->VIA1_shift);	// read it
            pmu->charCountR2 = pmu->charCountR;
            pmu->PGE_ISR_state = kPMUrcvData_cmd;
            if ( pmu->charCountR2 > 0 ) {		// is there anything to read?
                if ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
                    if ( !(pmu->WaitForAckHi()) ) {
                        return;				// yes, make sure ACK is high
                    }
                }
                *(pmu->VIA2_dataB) &= ~(pmu->PMreq);	// assert /REQ
                return;					// next interrupt will be first data byte
            }
            pmu->clientRequest->pmRLength = pmu->charCountR;	// no, this much was read
            pmu->cmdDoneEventSrc->interruptOccurred(0,0,0);	// inform workloop
            return;
           		// We have finished the transmission part of a command transaction, and
          		// according to our table, we will be getting a response but not a
          		// length byte for it.  Finish the transmit handshake and set up
        case kPMUreadData:			// a receive for the first data byte.
            if ( pmu->charCountR > 1 ) {
                pmu->charCountR2--;		// make constant (byte count + 1) into byte count
                pmu->charCountR--;
            }
//	    receivedByte = *VIA1_shift;			// read shift reg to turn off SR int
            pmu->PGE_ISR_state = kPMUrcvData_cmd;
            *(pmu->VIA1_auxillaryControl) &= 0xEF;	// set shift register to input
            if ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
                if ( !(pmu->WaitForAckHi()) ) {
                    return;				// make sure ACK is high
                }
            }
            *(pmu->VIA2_dataB) &= ~(pmu->PMreq);// assert /REQ
//	    *VIA1_interruptEnable = 0x84;	// enable SR interrupt
            return;				// next interrupt will be the first data character

         			// We are reading the response in a command transaction, and
        case kPMUrcvData_cmd:	// a data byte has arrived
            *(pmu->dataPointer)++ = *(pmu->VIA1_shift);	// read the data byte
            pmu->charCountR2--;
            if ( pmu->charCountR2 > 0 ) {		// is there more to read?
                if ( !(*(pmu->VIA2_dataB) & pmu->PMack) ) {
                    if ( !(pmu->WaitForAckHi()) ) {
                        return;				// yes, make sure ACK is high
                    }
                }
                *(pmu->VIA2_dataB) &= ~(pmu->PMreq);	// assert /REQ
                return;					// next interrupt will be next data byte
            }
            pmu->clientRequest->pmRLength = pmu->charCountR;	// no, this much was read
            pmu->cmdDoneEventSrc->interruptOccurred(0,0,0);	// inform workloop
            return;
            
        case kPMUdone:		// this was the last xmt SR interrupt of a command transaction
//	    receivedByte = *VIA1_shift;			// read shift reg to turn off SR int
            pmu->clientRequest->pmRLength = 0;			// nothing was read
            pmu->cmdDoneEventSrc->interruptOccurred(0,0,0);	// inform workloop
            return;
    }
    return;
}


// ****************************************************************************
//	PMUint
//
// ****************************************************************************

void PMUint ( OSObject * PMUdriver, IOInterruptEventSource *, int )
{
((ApplePMU *)PMUdriver)->servicePMU();
}


// ****************************************************************************
//	servicePMU
//	PGE has interrupted.  Send the ReadInt command to find out why.
//	When the command byte is sent, the Shift Register will interrupt.
//	If we are mid-transaction when we find out about the interrupt,
//	set a flag and find out why later.
//
// ****************************************************************************

void ApplePMU::servicePMU ( void )
{
if ( PGE_ISR_state != kPMUidle ) {
        PMU_int_pending = true;
        return;
}

// *VIA1_interruptFlag = 0x10;			// acknowledge VIA interrupt ( ifCB1 )
// *VIA1_interruptEnable = 0x10;		// and disable it entirely ( ifCB1 )
while ( !(*VIA2_dataB & PMack) ) {		// make sure ACK is high
}
*VIA1_auxillaryControl |= 0x1C;			// set shift register to output
*VIA1_shift = kPMUreadINT;			// give it this command byte
*VIA2_dataB &= ~PMreq;				// assert /REQ
// *VIA1_interruptEnable = 0x84;		// enable SR interrupt
PGE_ISR_state = kPMUreadLen_int;		// set the state
dataPointer = &interruptState[0];		// set up read pointer for data bytes
return;						// return till character transmission completes
}

	
// ****************************************************************************
// WaitForAckHi
// ****************************************************************************
bool ApplePMU::WaitForAckHi ( void )
{
// q8q struct timeval startTime;
// q8q struct timeval currentTime;
// q8q ns_time_t x;

// wait up to 32 milliseconds for Ack signal from PG&E to go high

// q8q IOGetTimestamp(&x);
// q8q ns_time_to_timeval(x, &startTime);		        // get current time

while ( true ) {
        if ( *VIA2_dataB & PMack ) {
            return ( true );					// ack is high, return
        }
// q8q  IOGetTimestamp(&x);
// q8q  ns_time_to_timeval(x, &currentTime);
// q8q  if ( startTime.tv_usec > currentTime.tv_usec ) {
// q8q      currentTime.tv_usec += 1000000;			// clock has wrapped, adjust it
// q8q  }
// q8q  if ( currentTime.tv_usec > (startTime.tv_usec + 32000) ) {	// has 32 ms elapsed?
// q8q      return ( false );                                       	// yes, return
// q8q  }
}
}


// ****************************************************************************
// DisablePMUInterrupt
// ****************************************************************************
void ApplePMU::DisablePMUInterrupt ( void )
{
*VIA1_interruptEnable = 1<<ifCB1;
eieio();
}


// ****************************************************************************
// EnablePMUInterrupt
// ****************************************************************************
void ApplePMU::EnablePMUInterrupt ( void )
{
*VIA1_interruptEnable = (1<<ifCB1) | 0x80;
eieio();
}


// ****************************************************************************
// AcknowledgePMUInterrupt
// ****************************************************************************
void ApplePMU::AcknowledgePMUInterrupt ( void )
{
*VIA1_interruptFlag = 1<<ifCB1;
eieio();
}


// ****************************************************************************
// DisableSRInterrupt
// ****************************************************************************
void ApplePMU::DisableSRInterrupt ( void )
{
    *VIA1_interruptEnable = 1<<ifSR;
}


// ****************************************************************************
// EnableSRInterrupt
// ****************************************************************************
void ApplePMU::EnableSRInterrupt ( void )
{
    *VIA1_interruptEnable = (1<<ifSR) | 0x80;
}



// ****************************************************************************
// timer_expired
//
// Our adb-read timer has expired, so we have to notify our i/o thread by
// enqueuing a Timeout message to its interrupt port.
// ****************************************************************************
// q8q void timer_expired(port_t mach_port)
// q8q {

// q8q }
