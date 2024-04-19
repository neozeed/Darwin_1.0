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
 *	IOATACommand.h
 *
 */

class IOSyncer;
class IOATADevice;
class IOATACommand;

typedef void (*ATACallback)(void *target, IOService *device, IOATACommand *ataCmd, void *refcon );

class IOATACommand : public OSObject
{
    OSDeclareDefaultStructors(IOATACommand)

    friend class IOATAController;
    friend class IOATADevice;

public:  
    /*  
     *  	Methods to initialize an ATACommand
     *
     *  	Note: 	These methods are usually invoked by the device client to setup and
     *      	submit an ATACommand.
     */
    void		clear();
    void 		setTaskfile( ATATaskfile *taskfile );						
    void 		setATAPICmd( ATAPICmd *atapiCmd, IOMemoryDescriptor *senseData = NULL, UInt32 senseLength = 0 );	
    void 		setPointers( IOMemoryDescriptor *desc, UInt32 transferCount, bool isWrite ); 	
    void 		setTimeout( UInt32  timeout );						  
    void 		setCallback( void *target = 0, ATACallback ataDoneFn = 0, void *refcon = 0 );	
    ATAReturnCode	getResults( ATAResults *results );

    /*	
     *	Methods to submit/cancel an ATACommand
     */				
    bool 		execute();
    bool 		abort();


    /*
     *  	Methods to interrogate an ATACommand
     *
     *  	Note: 	These methods are usually invoked by the controller driver to find out about 
     *        	the ATA Command. They may be also invoked by the device client to find out
     *		about completed command.
     */
    IOATADevice 	*getDevice();
    IOATAController 	*getController();
    IOCommandQueue      *getDeviceQueue();
    void 		getTaskfile( ATATaskfile *taskfile ); 			
    void 		getATAPICmd( ATAPICmd *cdb, IOMemoryDescriptor **senseData = NULL, UInt32 *senseLength = NULL );	
    void 		getPointers( IOMemoryDescriptor **desc, UInt32 *transferCount, bool *isWrite );	
    UInt32 		getTimeout();							
    ATAProtocol		getProtocol();
    UInt32		getResultMask();


    /*
     *  	Methods to complete an ATACommand
     *
     *  	Note: 	These methods are usually invoked by the controller driver. 
     */
    void 		setResults( ATAResults *results );						
    void	 	complete();
 
    /*
     *	Methods to address controller and client private per command data areas.
     */
    void		*getControllerData();
    void		*getClientData();

/*------------------Methods private to the IOATACommand class-------------------------*/
enum ATACommandFlags
{
    atacmdTaskfileValid		= 0x00000001,
    atacmdPointersValid		= 0x00000002,
    atacmdResultsValid		= 0x00000004,
    atacmdCallbackValid		= 0x00000008,
    atacmdATAPIInfoValid	= 0x00000010,
};

public:
    void		free();

private:
    void		setController( IOATAController *ctrlr );
    void		setDevice( IOATADevice *dev );
 
private:
    UInt32		flags;
    
    IOATAController     *controller;
    IOATADevice         *device;
    IOCommandQueue	*deviceQueue;
 
    ATATaskfile		taskfile;
    ATAResults		results;
    UInt32		timeout;

    IOMemoryDescriptor	*xferDesc;
    UInt32		xferCount;
    UInt32		xferDirection;

    ATAPICmd		atapiCmd;
    UInt32		senseLength;
    IOMemoryDescriptor	*senseData;
    
    union
    {
        struct
        {
            IOSyncer *syncer;
        } sync;
        struct
        {
	    ATACallback		ataDoneFn;
    	    void		*target;
    	    void		*refcon;
        } async;
     } completionInfo;

    UInt32		dataSize;
    void                *dataArea;
    void		*controllerData;
    void		*clientData;		  
};
