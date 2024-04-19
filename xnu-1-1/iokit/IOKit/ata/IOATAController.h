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
 *	IOATAController.h
 *
 *	Methods in this header list the methods an ATA controller driver must implement. 
 */
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandQueue.h>
#include <IOKit/IOInterruptEventSource.h>


class IOATACommand;

class IOATAController : public IOService
{
    OSDeclareAbstractStructors(IOATAController)

    friend class IOATACommand;
    friend class IOATADevice;

/*------------------Methods the controller subclass must implement-----------------------*/
protected:
    /*
     *   Initialize controller hardware.
     *
     *   Note: The controller driver's configure() method will be called prior to any other
     *         methods. If the controller driver returns successfully from this method it
     *         should be ready to accept any other method call listed.
     */
    virtual bool 	configure( IOService *provider, UInt32 *controllerDataSize ) = 0;

    /*
     *   Setup device queues.
     *
     *   Note: The controller driver should subclass and respond to provideQueueHandler(). 
     *         The super class implementation returns NULL. 
     *         
     *         If the controller driver implementation manages its own queues, it
     *         should subclass createWorkLoop() and return true with *workLoop = NULL.
     */
    virtual IOCommandQueueAction	provideQueueHandler( IOATADevice *ataDev );

    virtual bool			createWorkLoop( IOWorkLoop **workLoop );
    virtual IOCommandQueue 		*createDeviceQueue( IOATADevice *ataDev );

    virtual void			enableDeviceQueue( IOATADevice *ataDev = NULL );
    virtual void			disableDeviceQueue( IOATADevice *ataDev = NULL );

    /*    
     *  Methods information about driver/hardware capabilities
     */
    virtual bool 	provideProtocols( ATAProtocol *protocolsSupported )			= 0;
    virtual bool 	provideTimings( UInt32 *numTimings, ATATiming *timingsSupported )	= 0;

    /*
     *  Methods to set timing for individual devices
     */	
    virtual bool 	calculateTiming( UInt32 deviceNum,  ATATiming *timing )			= 0;
    virtual bool 	selectTiming( UInt32 deviceNum, ATATimingProtocol timingProtocol )	= 0;

    /*
     *  Method to perform an ATA Bus Reset
     */
    virtual bool 	resetBus();
    virtual void        resetBusRequest();

    /*
     *  Methods to execute and abort an ATACommand
     */
    virtual bool 	executeCommand( IOATACommand *cmd ) 	= 0;
    virtual bool 	abortCommand( IOATACommand *cmd ) 	= 0;

    /*
     *  ATAPI specific
     */
    IOATACommand 	*makeRequestSense( IOATACommand *origCmd );
    bool		completeRequestSense( IOATACommand *origCmd, IOATACommand *reqSenseCmd );

    /*
     *
     */
    virtual void	enableControllerInterrupts();
    virtual void	disableControllerInterrupts();
    

/*------------------Methods private to the IOATAController class----------------------*/
public:
    bool		matchNubWithPropertyTable( IOService *nub, OSDictionary *table );
    virtual void	free();

    // Implement IOService::getWorkLoop()
    virtual IOWorkLoop *getWorkLoop() const;

private:
    bool 		start( IOService *provider );

    bool		scanATABus();

    bool 		probeController();
    bool		createResetWorker();
    bool                createDeviceNubs();
    bool		probeDeviceNubs();
    bool 		registerDeviceNubs();

    void 		purgeDeviceQueue( IOATADevice *device );
    void                purgeCommand( void *p0, void *p1, void *p2, void *p3 );

    void 		resetWorker( void *, void *, void *, void * );

    void		changeDeviceQueue( IOATADevice *ataDev, bool fEnable );

    IOATACommand        *allocCommand( UInt32 clientDataSize = 0 );
    void 		completeCommand( IOATACommand *cmd );
    void		releaseCommand();

private:
    UInt32		controllerDataSize;

    struct
    {
        IOATADevice	*device;
        IOService	*client;
    } deviceInfo[2];

    IOWorkLoop		*workLoop;
    IOWorkLoop		*resetWorkLoop;
    IOCommandQueue	*resetCommandQ;

    IOATACommand	*utilCmd;

};

