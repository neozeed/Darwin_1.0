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
 *	IOATADevice.h
 *
 *
 *	Methods in this header provide information about the ATA/ATAPI device 
 *      the device client driver is submitting the ATACommand(s) to.
 *
 * 	Note: ATACommand(s) are allocated and freed by methods in this class. 
 *            The remaining methods to setup and submit ATACommands are defined in
 *            IOATACommand.h
 */

class IOATACommand;
class IOATAController;

class IOATADevice : public IOService
{
    OSDeclareDefaultStructors(IOATADevice)

    friend class IOATACommand;
    friend class IOATAController;


/*------------------Methods provided to ATA/ATAPI device clients-----------------------*/
public:
    /*
     * Allocate/release/clear an ATACommand
     */
    IOATACommand 	*allocCommand( UInt32 clientDataSize = 0 );
    void          	releaseCommand( IOATACommand *cmd );
    void          	clearCommand( IOATACommand *cmd );

    /*
     * Obtain information about this device
     */
    UInt32		getUnit();
    ATADeviceType	getDeviceType();
    bool		getIdentifyData( ATAIdentify *identifyBuffer );
    bool		getInquiryData( UInt32 inquiryBufSize, ATAPIInquiry *inquiryBuffer );
    bool 		getDeviceCapacity( UInt32 *blockMax, UInt32 *blockSize );
    bool 		getProtocols( ATAProtocol *protocols );
    bool 		getTimingsSupported( ATATimingProtocol *timingsSupported );
    bool		getTiming( ATATimingProtocol *timingProtocol, ATATiming *timing );
    IOCommandQueue	*getDeviceQueue();

    /*
     * Set default command timeout for this device
     */
    void 		setTimeout( UInt32 timeoutMS );

    /*
     * Select default device timing for this device
     */
    bool 		selectTiming( ATATimingProtocol timingProtocol );

    /*
     * Misc
     */
    bool		open( IOService *customer );
    void		close( IOService *customer );
    IOService 		*getClient();

    bool		getATAPIPktInt();

    virtual bool 	matchPropertyTable( OSDictionary * table );
    IOService		*matchLocation( IOService * );

/*------------------Methods private to the IOATADevice class----------------*/
public:
    bool		init( UInt32 unitNum, IOATAController *ctlr );
    IOReturn 		message( UInt32 p0, IOService *p1, void *p2 );
    void		free();

private:
    bool		probeDevice();
    ATADeviceType       probeDeviceType();
    ATAReturnCode       doIdentify(void **buffer );
    ATAReturnCode       doInquiry(void **buffer );
    ATAReturnCode       doSpinUp();
    ATAReturnCode       doReadCapacity( void *data );
    ATAReturnCode       doTestUnitReady();
    ATAReturnCode	doSectorCommand( ATACommand ataCmd, UInt32 ataLBA, UInt32 ataCount, void **dataPtr );

    void		submitCommand( IOATACommand *cmd );

    bool 		getATATimings();

    void		endianConvertData( void *data, void *endianTable );

    bool		executeCommand( IOATACommand *cmd );
    void		completeCommand( IOATACommand *cmd );

    OSDictionary        *createProperties();
    bool                addToRegistry( OSDictionary *propTable, OSObject *regObj, char *key );
    void                stripBlanks( char *d, char *s, UInt32 l );

    IOATACommand	*makeRequestSense( IOATACommand *origCmd );
    bool		completeRequestSense( IOATACommand *origCmd, IOATACommand *reqSenseCmd );
 

private:
    IOATAController	*controller;
    UInt32		unit;

    IOCommandQueue	*deviceQueue;

    IOService		*client;

    ATADeviceType	deviceType;
    ATAIdentify 	*identifyData;
    ATAPIInquiry	*inquiryData;

    IOATACommand	*utilCmd;
    IOATACommand        *reqSenseCmd;

    UInt32		numTimings;
    ATATiming		ataTimings[ataMaxTimings];

    bool		atapiPktInt;

    IORWLock *		resetSem;

};
