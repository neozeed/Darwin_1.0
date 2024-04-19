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
 *    	AppleATA.h
 *
 */
#include <IOKit/ata/IOATA.h>
#include <IOKit/ata/IOATAController.h>

class AppleATA : public IOATAController
{
    OSDeclareAbstractStructors( AppleATA )

/*
 *	Subclasses of AppleATA must implement these methods.
 */
public:

    virtual void		writeATAReg( UInt32 regIndex, UInt32 regValue ) 		= 0;
    virtual UInt32		readATAReg(  UInt32 regIndex ) 					= 0;

/*
 *	Subclasses of AppleATA must implement these methods if they support dma transfers. 
 */
    virtual bool		programDma( IOATACommand *cmd );
    virtual bool		startDma( IOATACommand *cmd );
    virtual bool		stopDma( IOATACommand *cmd, UInt32 *transferCount );

/*
 *	These methods are provided to the IOATAController superclass. The remaining methods
 *	required by the IOATAController superclass must be implemented by subclasses of 
 *      AppleATA.
 */
public:
   
    virtual bool 			resetBus();
    virtual bool 			executeCommand( IOATACommand *cmd );
    virtual bool 			abortCommand( IOATACommand *cmd );
    virtual IOCommandQueueAction        provideQueueHandler( IOATADevice * );


/*
 *	These methods provide a default ATA protocol implemenation to subclasses of AppleATA.
 *
 *	Subclasses of AppleATA may modify or replace these methods to suit their requirements.
 */
protected:

    virtual void 			commandOccurred( void *p0, void *p1, void *p2, void *p3 );

    virtual void			newDeviceSelected( IOATADevice *newDevice );

    virtual void			interruptOccurred();
    virtual void			processATAPioInt();
    virtual void			processATADmaInt();
    virtual void			processATAPIPioInt();
    virtual void			processATAPIDmaInt();

    virtual ATAReturnCode		readATAPIDevice( UInt32 n );
    virtual ATAReturnCode		writeATAPIDevice( UInt32 n );
    virtual ATAReturnCode		sendATAPIPacket();		

    virtual void			doProtocolSetRegs( IOATACommand *cmd );
    virtual void			doATAProtocolPio( IOATACommand *cmd );
    virtual void			doATAProtocolDma( IOATACommand *cmd );
    virtual void			doATAPIProtocolPio( IOATACommand *cmd );
    virtual void			doATAPIProtocolDma( IOATACommand *cmd );
    virtual void			doProtocolNotSupported( IOATACommand *cmd );
    virtual bool 			doRequestSense( IOATACommand *cmd );
    virtual void 			didRequestSense( IOService *, IOATACommand *reqSenseCmd, IOATACommand *origCmd );

    virtual bool			selectDrive( UInt32 driveHeadReg );  

    virtual void			completeCmd( IOATACommand *cmd, ATAReturnCode returnCode, UInt32 bytesTransferred = 0 );
    virtual void			completeCmd( IOATACommand *cmd );

    virtual void                	updateCmdStatus( IOATACommand *cmd, ATAReturnCode returnCode, UInt32 bytesTransferred );

    virtual bool 			waitForStatus( UInt32 statusBitsOn, UInt32 statusBitsOff, UInt32 timeoutmS );


protected:

    IOATACommand		*xferCmd;
    IOATACommand        	*xferCmdSave;
    UInt32			xferCmdTimer;
    IOMemoryDescriptor		*xferDesc;
    bool			xferIsWrite;
    UInt32			xferCount;
    UInt32			xferInts;
    UInt32			xferRemaining;

    IOATADevice			*currentDevice;

};  

#define ATATimerIntervalmS	250

enum
{
    ataBusyTimeoutmS = 10,
};
