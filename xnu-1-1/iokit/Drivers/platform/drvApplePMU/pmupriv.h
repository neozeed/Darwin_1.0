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
#ifdef __cplusplus

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOTypes.h>
#include <IOKit/adb/adb.h>
#include "pmu.h"

extern "C" {
#include <pexpert/pexpert.h>
}

class IOSyncer;
class IOPMUADBController;
class IOPMUNVRAMController;
class IOPMURTCController;
/*
class IOPMUPwrController;
*/

#else

#include <IOKit/IODevice.h>
#include <IOKit/adb/IOADBController.h>

#endif

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandQueue.h>
#include "pmumisc.h"

        typedef volatile unsigned char  *VIAAddress;	// This is an address on the bus

// **********************************************************************************
//
// VIA definitions
//
// **********************************************************************************

enum {				 	// port B
						// M2 uses VIA2
	M2Req		= 2,			      	// Power manager handshake request
	M2Ack		= 1,				// Power manager handshake acknowledge
	      					// Hooper uses VIA1
	HooperReq	= 4,			      	// request
	HooperAck	= 3				// acknowledge
};

enum {					// IFR/IER
	ifCA2 = 0,				// CA2 interrupt
	ifCA1 = 1,				// CA1 interrupt
	ifSR  = 2,				// SR shift register done
	ifCB2 = 3,				// CB2 interrupt
	ifCB1 = 4,				// CB1 interrupt
	ifT2  = 5,				// T2 timer2 interrupt
	ifT1  = 6,				// T1 timer1 interrupt
	ifIRQ = 7				// any interrupt
};

// **********************************************************************************
// bits in response to kPMUReadInt command
// **********************************************************************************

enum {
	kPMUMD0Int 		= 0x01,   // interrupt type 0 (machine-specific)
	kPMUMD1Int 		= 0x02,   // interrupt type 1 (machine-specific)
	kPMUMD2Int 		= 0x04,   // interrupt type 2 (machine-specific)
	kPMUbrightnessInt 	= 0x08,   // brightness button has been pressed, value changed
	kPMUADBint 		= 0x10,   // ADB
	kPMUbattInt 		= 0x20,   // battery
	kPMUenvironmentInt 	= 0x40,   // environment
	kPMUoneSecInt 		= 0x80    // one second interrupt
};

enum {					// when kPMUADBint is set
	kPMUautopoll		= 0x04		// input is autopoll data
};

// **********************************************************************************
// states of the ISR
// **********************************************************************************

enum {
	kPMUidle,
	kPMUxmtLen,
	kPMUxmtData,
	kPMUreadLen_cmd,
	kPMUrcvLen_cmd,
	kPMUreadData,
	kPMUrcvData_cmd,
	kPMUdone,
	kPMUreadLen_int,
	kPMUrcvLen_int,
	kPMUrcvData_int
};


enum {
	kPMUADBAddressField = 4
};

enum {
	kPMUResetADBBus	= 0x00,
	kPMUFlushADB		= 0x01,
	kPMUWriteADB		= 0x08,
	kPMUReadADB		= 0x0C,
	kPMURWMaskADB	= 0x0C
};

#define MISC_LENGTH 8


struct PMUrequest {
    UInt32		pmCommand;
    UInt32		pmSLength1;
    UInt8 * 		pmSBuffer2;
    UInt32		pmSLength2;
    UInt8 *		pmRBuffer;
    UInt32		pmRLength;
    bool		pmFlag;
    UInt8		pmSBuffer1[MISC_LENGTH];
    struct PMUrequest *	next;
    struct PMUrequest *	prev;
    IOSyncer *		sync;
};

typedef struct PMUrequest PMUrequest;


class ApplePMU : IOService
{
OSDeclareDefaultStructors(ApplePMU)

public:
					// this stuff is public for the c-functions in the driver
//    IOInterruptEventSource * SRintEventSrc;
IOInterruptEventSource * cmdDoneEventSrc;
IOInterruptEventSource * unsolicitedEventSrc;
IOInterruptEventSource * PMUintEventSrc;
UInt32		PGE_ISR_state;
VIAAddress	VIA1_shift;		// pointers to VIA registers
VIAAddress	VIA1_auxillaryControl;
VIAAddress	VIA2_dataB;
UInt8		PMreq;		      	// req bit
UInt8		PMack;			// ack bit
UInt8		interruptState[12];
UInt8		receivedByte;
SInt32		charCountR;
SInt32		charCountR2;
UInt32		charCountS1;
UInt32		charCountS2;
UInt8 *		dataPointer;
UInt8 *		dataPointer1;
UInt8 *		dataPointer2;
PMUrequest *	clientRequest;
PMUrequest *    queueHead;		// our command queue
PMUrequest *	queueTail;
bool		adb_reading;		// TRUE: we have a register read outstanding
bool		PMU_int_pending;	// TRUE: PMU has requested service
pmCallback_func	RTCclient;		// Tick handler in RTC client
IOPMURTCController *	ourRTCinterface;


private:

IOWorkLoop *		workLoop;
IOCommandQueue *	commandQueue;
IOPMUADBController *	ourADBinterface;
IOPMUNVRAMController *	ourNVRAMinterface;
/*
IOPMUPwrController *	ourPwrinterface;
*/
		// pointers to VIA registers
VIAAddress	VIA1_interruptFlag;
VIAAddress	VIA1_interruptEnable;
UInt8		savedPGEintEn;
UInt8		savedSRintEn;
ADB_callback_func ADBclient;		// autopoll handler in ADB client
IOService *	ADBid;			// pointer to ADB client
IOService *	RTCid;			// pointer to RTC client
pmCallback_func	PWRclient;		// Button handler in Power Management client
IOService *	PWRid;			// pointer to Power Management client
UInt8		firstChar;
mach_timespec_t	adb_read_timeout;	// timeout on read to absent adb device
AbsoluteTime	SR_to_ack_transition_delay;

void timeoutOccurred ( void );
void DisablePMUInterrupt ( void );
void EnablePMUInterrupt ( void );
void AcknowledgePMUInterrupt ( void );
void DisableSRInterrupt ( void );
void EnableSRInterrupt ( void );
static bool serializeBatteryInfo( void * target, void * ref, OSSerialize * s );

protected:
virtual void free ( void );


public:

bool init ( OSDictionary * properties = 0 );
bool start ( IOService * );
virtual IOWorkLoop *getWorkLoop() const;

//void poll_device ( void );
void enqueueCommand ( PMUrequest * theRequest );
void registerForADBInterrupts ( ADB_callback_func client, IOService * caller );
void registerForPowerInterrupts ( pmCallback_func buttonHandler, IOService * caller );
void registerForClockInterrupts ( pmCallback_func tickHandler, IOService * caller );
IOReturn sendMiscCommand ( UInt32 Command, IOByteCount  SLength,
			UInt8 * SBuffer, IOByteCount * RLength, UInt8 * RBuffer );
//void serviceShiftRegister ( void );
void servicePMU ( void );
void StartPMUTransmission ( PMUrequest * plugInMessage );
void CheckRequestQueue ( void );
void ADBinput ( UInt32 theLength, UInt8 * theInput );
void buttonInput ( UInt32 theLength, UInt8 * theInput );
bool WaitForAckHi ( void );
bool WaitForAckLo( void );

};






