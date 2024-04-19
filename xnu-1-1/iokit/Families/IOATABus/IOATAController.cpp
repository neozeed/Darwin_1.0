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
 * IOATAController.cpp
 *
 */

#include <IOKit/ata/IOATA.h>
#include <IOKit/ata/IOATAController.h>

#undef  super
#define super IOService

OSDefineMetaClass( IOATAController, IOService );
OSDefineAbstractStructors( IOATAController, IOService );

#define round(x,y) (((int)(x) + (y) - 1) & ~((y)-1))

/*
 *
 *
 */
bool IOATAController::start( IOService *provider )
{

    if ( provider->open( this ) != true )
    { 
        return false;
    }

    if ( configure( provider, &controllerDataSize ) != true || scanATABus() != true ) 
    {
        provider->close( this );
        return false;
    }

    controllerDataSize = round( controllerDataSize, 16 );

    return true;
}

/*
 *
 *
 *
 */
void IOATAController::completeCommand( IOATACommand *cmd )
{
}

/*
 *
 *
 *
 */
bool IOATAController::scanATABus()
{
    if ( createResetWorker() != true )
    {
        return false;
    }

    if ( createWorkLoop( &workLoop ) != true )
    {
        return false;
    }

    if ( createDeviceNubs() != true )
    {
        return false;
    }

    disableControllerInterrupts();

    if ( probeController() == false )
    {
        return false;
    }

    enableControllerInterrupts();

    if ( resetBus() == false )
    {
        return false;
    }

    if ( probeDeviceNubs() != true )
    {
        return false;
    }

    if ( registerDeviceNubs() != true )
    {
        return false;
    }


    return true;
}        

/*
 *
 *
 *
 */
IOWorkLoop * IOATAController::getWorkLoop() const
{
    return workLoop;
}

/*
 *
 *
 *
 */
bool IOATAController::createResetWorker()
{
    resetWorkLoop = new IOWorkLoop;
    if ( resetWorkLoop == NULL )
    {
        return false;
    }

    if ( resetWorkLoop->init() != true )
    {
        return false;
    }

    resetCommandQ = IOCommandQueue::commandQueue( this, (IOCommandQueueAction) &IOATAController::resetWorker );
    if ( resetCommandQ == NULL )
    {
        return false;
    }

    if ( resetWorkLoop->addEventSource( resetCommandQ ) != kIOReturnSuccess )
    {
        resetCommandQ->release();
        return false;
    }

    return true;
}

/*
 *
 *
 *
 */
bool IOATAController::createWorkLoop( IOWorkLoop **wrkLoop)
{

    *wrkLoop = new IOWorkLoop;
    if ( *wrkLoop == NULL )
    {
        return false;
    }

    if ( (*wrkLoop)->init() != true )
    {
        return false;
    }
    return true;
}       
       

/*
 *
 *
 *
 */
IOCommandQueue *IOATAController::createDeviceQueue( IOATADevice *ataDev )
{
    IOCommandQueueAction 	queueHandler;
    IOCommandQueue	 	*deviceQueue;
   
    queueHandler = provideQueueHandler( ataDev );
    if ( queueHandler == NULL )
    {
        IOLog("IOATAController::%s() - Your subclass must provide a device queue handling routine\n\r", __FUNCTION__);
        return NULL;
    }

    deviceQueue = IOCommandQueue::commandQueue( this, (IOCommandQueueAction) queueHandler );
    if ( deviceQueue == NULL )
    {
        return NULL;
    }

    if ( workLoop->addEventSource( deviceQueue ) != kIOReturnSuccess )
    {
        deviceQueue->release();
        return NULL;
    }
    
    return deviceQueue;
}

/*
 *
 *
 */
void IOATAController::disableDeviceQueue( IOATADevice *ataDev = NULL )
{
    changeDeviceQueue( ataDev, false );
}

void IOATAController::enableDeviceQueue( IOATADevice *ataDev = NULL )
{
    changeDeviceQueue( ataDev, true );
}

void IOATAController::changeDeviceQueue( IOATADevice *ataDev, bool fEnable )
{
   UInt32		i;

   if ( ataDev == NULL)
   {
       for ( i = 0; i < 2; i++ )
       {
           ataDev = deviceInfo[i].device;
           if ( ataDev == NULL )
           {
               continue;
           }
           
           if ( fEnable == true )
           {
               ataDev->getDeviceQueue()->enable();
           }
           else
           {
               ataDev->getDeviceQueue()->disable();
           }
       }
   }
   else 
   {
       if ( fEnable == true )
       {
           ataDev->getDeviceQueue()->enable();
       }
       else
       {
           ataDev->getDeviceQueue()->disable();
       }
   }
}
            

/*
 *
 *
 *
 */
IOCommandQueueAction IOATAController::provideQueueHandler( IOATADevice *ataDev )
{
    return NULL;
}
        
/*
 *
 *
 *
 */
bool IOATAController::createDeviceNubs()
{
    UInt32		i;
    IOATADevice		*ataDev;

    for (i = 0; i < 2; i++ )
    {       
        ataDev = deviceInfo[i].device = new IOATADevice;

        if ( ataDev->init( i, this ) != true )
        {
            ataDev->release();
            deviceInfo[i].device = NULL;
        }		
    }

    return true;
}       

/*
 *
 *
 *
 */
bool IOATAController::probeDeviceNubs()
{
    UInt32		i;
    IOATADevice		*ataDev;

    for (i = 0; i < 2; i++ )
    {       
        ataDev = deviceInfo[i].device;
        if ( ataDev->probeDeviceType() == ataDeviceNone )
        {
            ataDev->release();
            deviceInfo[i].device = NULL;
        }
    }

    for (i = 0; i < 2; i++ )
    {       
        ataDev = deviceInfo[i].device;
        if ( ataDev == NULL )
        {
            continue;
        }

        if ( ataDev->probeDevice() != true )
        {
            ataDev->release();
            deviceInfo[i].device = NULL;
        }
    }

    return true;
}       

/*
 *
 *
 *
 */
bool IOATAController::registerDeviceNubs()
{
    UInt32		i;
    IOATADevice		*ataDev;

    for (i = 0; i < 2; i++ )
    {       
        ataDev = deviceInfo[i].device;
        if ( ataDev != NULL )
        {
            ataDev->attach( this );
            ataDev->registerService();
        }
   }

    return true;
}       

/*
 *
 *
 *
 */
bool IOATAController::matchNubWithPropertyTable( IOService *nub, OSDictionary *table )
{
    bool		rc;

    rc = nub->compareProperty( table, OSString::withCStringNoCopy( ATAPropertyLocation ) );

    return rc;
}


/*
 *
 *
 *
 */
bool IOATAController::probeController()
{
    IOATADevice		*ataDev;
    ATATaskfile         taskfile;
    ATAResults          results;
    UInt32	       	j;
    bool                rc = true;
    UInt32		status;
    mach_timespec_t	startTime, currentTime;
    ATATiming           initPIOTiming;

    ataDev = deviceInfo[0].device;
    if ( ataDev == NULL )
    {
        return false;
    }

    initPIOTiming.timingProtocol = ataTimingPIO;
    initPIOTiming.featureSetting = 0;
    initPIOTiming.mode           = 0;
    initPIOTiming.minDataAccess  = 165;
    initPIOTiming.minDataCycle   = 600;
    initPIOTiming.minCmdAccess   = 290;
    initPIOTiming.minCmdCycle    = 600;

    calculateTiming( 0, &initPIOTiming );
    calculateTiming( 1, &initPIOTiming );

    selectTiming( 0, ataTimingPIO );
    selectTiming( 1, ataTimingPIO );

    utilCmd = ataDev->allocCommand();
    if ( utilCmd == NULL )
    {
        return false;
    }

    bzero( (void *)&taskfile, sizeof(taskfile) );

    taskfile.protocol                 = ataProtocolSetRegs;

    taskfile.regmask                  = ATARegtoMask(ataRegDriveHead);
    taskfile.ataRegs[ataRegDriveHead] = ataModeLBA;
    taskfile.resultmask               = 0;

    utilCmd->setTaskfile( &taskfile );
    utilCmd->execute();

    taskfile.regmask		      = 0;
    taskfile.resultmask               = ATARegtoMask(ataRegStatus);
    utilCmd->setTaskfile( &taskfile );

    IOGetTime( &startTime );

    do
    {
        utilCmd->execute();
        if ( utilCmd->getResults( &results ) != ataReturnNoError )
        {
            return false;
        }

        IOGetTime( &currentTime );
        SUB_MACH_TIMESPEC( &currentTime, &startTime );
        if ( currentTime.tv_sec > 25 )
        {
            return false;
        }

        status = results.ataRegs[ataRegStatus];

        if ( (status & ataStatusBSY) == 0)
        {
            break;
        }
        else if ( status & (ataStatusDF | ataStatusECC) )
        {
            return false;
        }
        IOSleep(20);
    }
    while ( 1 );

  
    taskfile.regmask      = ATARegtoMask(ataRegSectorCount) 
                          | ATARegtoMask(ataRegSectorNumber);
 
    taskfile.resultmask   = ATARegtoMask(ataRegSectorCount)
                          | ATARegtoMask(ataRegSectorNumber);

    for ( j=0; j < 16; j++ )
    {
        taskfile.ataRegs[ataRegSectorCount]  = j;
        taskfile.ataRegs[ataRegSectorNumber] = j << 4;
 
        utilCmd->setTaskfile( &taskfile );
        utilCmd->execute();
        utilCmd->getResults( &results );

        if ( results.returnCode != ataReturnNoError 
                 ||  results.ataRegs[ataRegSectorCount] != j 
                     || results.ataRegs[ataRegSectorNumber] != (j << 4) )
        {
            rc = false;
            break;
        }     
    }

    return rc;
}

/*
 *
 *
 *
 */
void IOATAController::resetBusRequest()
{
    UInt32			i;
    IOService			*client;
    IOATADevice			*device;

    for ( i = 0; i < 2; i++ )
    {
        device = deviceInfo[i].device;
        if ( device == NULL ) continue;
 
        client = device->getClient();
        if ( client == NULL ) continue;

        client->message( ataMessageResetStarted, device );
    }

    for ( i = 0; i < 2; i++ )
    {
        device = deviceInfo[i].device;
        if ( device == NULL ) continue;

        purgeDeviceQueue( device );
    }

    resetCommandQ->enqueueCommand();
}


/*
 *
 *
 *
 */
void IOATAController::purgeDeviceQueue( IOATADevice *device )
{
    enableDeviceQueue( device );
    device->getDeviceQueue()->performAndFlush( (OSObject *)NULL,
                (IOCommandQueueAction) &IOATAController::purgeCommand );
}


/*
 *
 *
 *
 */
void IOATAController::purgeCommand( void *p0, void *p1, void *p2, void *p3 )
{
    IOATACommand	*cmd = (IOATACommand *)p1;
    ATAResults		result;

    bzero( &result, sizeof(result) );

    result.returnCode = ataReturnBusReset;
    cmd->setResults( &result );

    cmd->complete();
}         

/*
 *
 *
 *
 */
void IOATAController::resetWorker( void *, void *, void *, void * )
{
    UInt32		i;
    IOATADevice		*device;
    IOService		*client;
    bool                rc = false;

    do 
    {
        rc = resetBus();
    } 
    while ( rc == false );

    for ( i = 0; i < 2; i++ )
    {
        device = deviceInfo[i].device;
        if ( device == NULL ) continue;

        client = device->getClient();
        if ( client == NULL ) continue;
        
        client->message( ataMessageInitDevice, device );
    }

    for ( i = 0; i < 2; i++ )
    {
        device = deviceInfo[i].device;
        if ( device == NULL ) continue;

        client = device->getClient();
        if ( client == NULL ) continue;
        
        client->message( ataMessageResetComplete, device );
    }
}        


/*
 *
 *
 *
 */
bool IOATAController::resetBus()
{
    ATATaskfile         taskfile;
    ATAResults          result;
    bool                rc = true;
    mach_timespec_t	startTime, currentTime;

    bzero( (void *)&taskfile, sizeof(taskfile) );

    taskfile.protocol     = ataProtocolSetRegs;
    taskfile.regmask      = ATARegtoMask(ataRegDeviceControl); 
    taskfile.resultmask   = 0;

    taskfile.ataRegs[ataRegDeviceControl] = ataDevControlnIEN | ataDevControlSRST;

    utilCmd->setTaskfile( &taskfile );
    utilCmd->execute();

    taskfile.ataRegs[ataRegDeviceControl] &= ~(ataDevControlnIEN | ataDevControlSRST);
    utilCmd->setTaskfile( &taskfile );
    utilCmd->execute();

    taskfile.regmask    = 0;
    taskfile.resultmask = ATARegtoMask(ataRegStatus); 
    utilCmd->setTaskfile( &taskfile );

    IOSleep(5);

    IOGetTime( &startTime );

    do 
    {
        utilCmd->execute();
        utilCmd->getResults(&result);  

        IOGetTime( &currentTime );
        SUB_MACH_TIMESPEC( &currentTime, &startTime );
        if ( currentTime.tv_sec > 25 )
        {
            return false;
        }

        if ( (result.ataRegs[ataRegStatus] & ataStatusBSY) == 0 )
        {
            break;
        }
        IOSleep(20);
    }
    while ( 1 );
                             
    return rc;
}

/*
 *
 *
 *
 */
void IOATAController::enableControllerInterrupts()
{
}

/*
 *
 *
 *
 */
void IOATAController::disableControllerInterrupts()
{
}


/*
 *
 *
 *
 */
IOATACommand *IOATAController::makeRequestSense( IOATACommand *origCmd )
{
    return origCmd->getDevice()->makeRequestSense( origCmd );
}

/*
 *
 *
 *
 */
bool IOATAController::completeRequestSense( IOATACommand *origCmd, IOATACommand *reqSenseCmd )
{
    return origCmd->getDevice()->completeRequestSense( origCmd, reqSenseCmd );
}

       
/*
 *
 *
 *
 */
IOATACommand *IOATAController::allocCommand(UInt32 clientDataSize )
{
    IOATACommand	*cmd;
    UInt32		size;

    size = controllerDataSize + round(clientDataSize, 16);

    cmd = new IOATACommand;
    if ( !cmd )
    {
        return 0;
    }
    cmd->init();

    if ( size )
    {
        cmd->dataArea = (void *)IOMalloc(size);
        if ( !cmd->dataArea )
        {
            cmd->release();
            return 0;
        }
        
        bzero( cmd->dataArea, size );

        cmd->dataSize = size;

        if ( controllerDataSize )
        {
            cmd->controllerData = cmd->dataArea;
        }
        if ( clientDataSize )
        {
            cmd->clientData = (void *)((UInt8 *)cmd->dataArea + controllerDataSize);
        }
    }

    cmd->setController( this );

    return cmd;
}

/*
 *
 *
 *
 */
void IOATAController::free()
{
    if ( workLoop )      workLoop->release();
    if ( resetCommandQ ) resetCommandQ->release();
    if ( resetWorkLoop ) resetWorkLoop->release();
    if ( utilCmd )       utilCmd->release();

    super::free();
}

/*
 *
 *
 *
 */
void IOATACommand::free()
{
    if ( dataArea )
    {
        IOFree( dataArea, dataSize );        
    }

    OSObject::free();
}


   
