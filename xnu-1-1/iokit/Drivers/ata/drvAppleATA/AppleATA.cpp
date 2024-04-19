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
 *    AppleATA.cpp
 *
 */

#include "AppleATA.h"

#undef super
#define super IOATAController

OSDefineMetaClass( AppleATA, IOATAController );
OSDefineAbstractStructors( AppleATA, IOATAController );

#if 0
static UInt32 dropInt=0;
#endif

/*
 *
 *
 */
bool AppleATA::executeCommand( IOATACommand *cmd )
{
   
    cmd->getDeviceQueue()->enqueueCommand( true, (void *)0, cmd, (void *)0, (void *)0 );

    return true;
}


/*
 *
 *
 */
bool AppleATA::abortCommand( IOATACommand * /* cmd */ )
{
    return false;
}


/*
 *
 *
 */
bool AppleATA::resetBus()
{
    bool		rc;

    disableControllerInterrupts();
    rc = super::resetBus();
    enableControllerInterrupts();
    
    return		rc;
}

/*
 *
 *
 */
IOCommandQueueAction AppleATA::provideQueueHandler( IOATADevice * )
{
    IOCommandQueueAction ret = (IOCommandQueueAction) &AppleATA::commandOccurred;
    return ret;
}


/*
 *
 *
 */
void AppleATA::commandOccurred( void */*p0*/, void *p1, void */*p2*/, void */*p3*/ )
{
    /* UInt32		cmdType = (UInt32) p0; unused */
    IOATACommand	*cmd 	= (IOATACommand *)p1;
    IOATADevice		*newDevice;
 
//    IOLog("ApplePPCATA::%s() - Cmd = %08x\n\r",  __FUNCTION__, (int) cmd );

    disableDeviceQueue();

    xferCmd = cmd;

    newDevice = cmd->getDevice();
    if ( currentDevice != newDevice )
    {
        newDeviceSelected( newDevice );
        currentDevice = newDevice;
    }

    switch ( cmd->getProtocol() )
    {
        case ataProtocolSetRegs:
            doProtocolSetRegs( cmd );
            break;

        case ataProtocolPIO:
            doATAProtocolPio( cmd );
            break;

        case ataProtocolDMA:
            doATAProtocolDma( cmd );
	    break;

        case atapiProtocolPIO:
            doATAPIProtocolPio( cmd );
            break;

        case atapiProtocolDMA:
            doATAPIProtocolDma( cmd );
            break;

        default:  
            doProtocolNotSupported( cmd );
            break;
    }
}



void AppleATA::interruptOccurred()
{
    if ( !xferCmd || !xferInts ) 
    {
        UInt32		status;

        status = readATAReg( ataRegStatus );
        IOLog( "ApplePPCATA::%s - Spurious interrupt - ATA Status = %04lx\n\r", __FUNCTION__, status );
        return;
    }

#if 0
    if ( dropInt++ > 20 )
    {
        UInt32		status;

        IOLog("AppleATA::%s() - Dropping interrupt\n\r", __FUNCTION__ );
        status = readATAReg( ataRegStatus );
        dropInt = 0;
        xferInts = 0;
        return;
    }
#endif       

    switch ( xferCmd->getProtocol() )
    {
        case ataProtocolPIO:
            processATAPioInt();
            break;
    
        case ataProtocolDMA:
            processATADmaInt();
            break;

        case atapiProtocolPIO:
            processATAPIPioInt();
            break;

        case atapiProtocolDMA:
            processATAPIDmaInt();
            break;


        default:
            ;
     }
}


/*
 *
 *
 */
bool AppleATA::doRequestSense( IOATACommand *cmd )
{
    IOATACommand        *reqSenseCmd;
    IOMemoryDescriptor	*senseData;

    cmd->getATAPICmd( (ATAPICmd *)NULL, &senseData, (UInt32 *)NULL );
    
    if ( senseData == 0 )
    {
        return false;
    }
       
    reqSenseCmd = makeRequestSense( cmd );
    if ( reqSenseCmd == NULL )
    {
        return false;
    }

    reqSenseCmd->setCallback( this, (ATACallback)
                              &AppleATA::didRequestSense, cmd  );

    xferCmdSave = xferCmd;
    commandOccurred( (void *)0, (void *)reqSenseCmd, (void *)0, (void *)0 );

    return true;
}


/*
 *
 *
 */
void AppleATA::didRequestSense( IOService *, IOATACommand *reqSenseCmd, IOATACommand *origCmd )
{
    xferCmdSave = NULL;
    completeRequestSense( origCmd, reqSenseCmd );
    completeCmd( origCmd );
}


/*
 *
 *
 */
void AppleATA::doProtocolNotSupported( IOATACommand *cmd )
{    
    completeCmd( cmd, ataReturnNotSupported );
}


/*
 *
 *
 */
void AppleATA::completeCmd( IOATACommand *cmd, ATAReturnCode returnCode, UInt32 bytesTransferred )
{
    updateCmdStatus( cmd, returnCode, bytesTransferred );
    completeCmd( cmd );
}

/*
 * 
 *
 */
void AppleATA::updateCmdStatus( IOATACommand *cmd, ATAReturnCode returnCode, UInt32 bytesTransferred )
{
    UInt32		resultmask;
    UInt32		i;
    ATAResults		result;

    bzero( &result, sizeof(result) );

    resultmask = cmd->getResultMask();

    if ( cmd->getProtocol() != ataProtocolSetRegs )
    {
         if ( waitForStatus( 0, ataStatusBSY, ataBusyTimeoutmS ) == false )
         {
             if ( returnCode == ataReturnNoError )
             {
                 returnCode = ataReturnErrorBusy;
             }
         }
    }

    for ( i=0; resultmask; i++ )
    {
        if ( resultmask & 1 )
        {
            result.ataRegs[i] = readATAReg( i );
        }
        resultmask >>= 1;
    }

    result.returnCode       = returnCode;
    result.bytesTransferred = bytesTransferred;
    cmd->setResults( &result );
  
    xferCmdTimer = 0;
}

/*
 *
 *
 */
void AppleATA::completeCmd( IOATACommand *cmd )
{
    xferCmd      = NULL;
    xferCmdTimer = 0;
    cmd->complete();
    enableDeviceQueue();
}


/*
 *
 *
 */
void AppleATA::newDeviceSelected( IOATADevice * )
{
}


/*
 *
 *
 */
bool AppleATA::programDma( IOATACommand * )
{
    IOLog( "AppleATA::%s - Subclass must implement\n\r", __FUNCTION__ );
    return false;
}


/*
 *
 *
 */
bool AppleATA::startDma( IOATACommand * )
{
    IOLog( "AppleATA::%s - Subclass must implement\n\r", __FUNCTION__ );
    return false;
}


/*
 *
 *
 */
bool AppleATA::stopDma( IOATACommand *, UInt32 * )
{
    IOLog( "AppleATA::%s - Subclass must implement\n\r", __FUNCTION__ );
    return false;
}

/*
 *
 *
 */
bool AppleATA::waitForStatus( UInt32 statusBitsOn, UInt32 statusBitsOff, UInt32 timeoutmS )
{
    mach_timespec_t	endTime, tmpTime;
    UInt32		status;

    tmpTime.tv_sec  = 0;
    tmpTime.tv_nsec = timeoutmS * 1000 * 1000;    

    IOGetTime( &endTime );
    
    ADD_MACH_TIMESPEC( &endTime, &tmpTime);
    
    do
    {
        status = readATAReg( ataRegStatus );

        if ( (status & statusBitsOn) == statusBitsOn
                             && (status & statusBitsOff) == 0 )
        {
            return true;
        }

        IOGetTime( &tmpTime );

   } while ( CMP_MACH_TIMESPEC( &endTime, &tmpTime ) > 0 );

   return false;
}

