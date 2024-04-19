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
 *    AppleATADma.cpp
 *
 */
#include "AppleATA.h"

/*----------------------------------- ATA DMA Protocol ------------------------------*/

/*
 *
 *
 */
void AppleATA::doATAProtocolDma( IOATACommand *cmd )
{    
    ATATaskfile		taskfile;
    UInt32		regmask;
    UInt32		i;

    cmd->getTaskfile( &taskfile );

    regmask = taskfile.regmask;

    if ( regmask & ATARegtoMask(ataRegDriveHead) )
    {
        regmask &= ~ATARegtoMask(ataRegDriveHead);
        if ( selectDrive( taskfile.ataRegs[ataRegDriveHead] ) == false )
        {
            completeCmd( cmd, ataReturnErrorBusy );
            return;
        }          
    }

    xferCount = 0;
    xferInts  = 1;
    
    programDma( cmd );

    xferInts = 1;

    if ( cmd->getTimeout() != 0 )
    {
        xferCmdTimer = cmd->getTimeout() / ATATimerIntervalmS + 1;
    }
    
    for ( i = 0; regmask; i++ )
    {
        if ( regmask & 1 )
        {
            writeATAReg( i, taskfile.ataRegs[i] );
        }
        regmask >>= 1;
    }

    startDma( cmd );
}

/*
 *
 * 
 */
void AppleATA::processATADmaInt()
{    
    UInt32		status;
    UInt32		reqCount;
    ATAReturnCode	rc = ataReturnNoError;

    if ( waitForStatus( 0, ataStatusBSY, ataBusyTimeoutmS ) == false )
    {
        completeCmd( xferCmd, ataReturnErrorBusy, xferCount );
        return;
    }

    status = readATAReg( ataRegStatus );
   
    xferCmd->getPointers( 0, &reqCount, 0 );    

    if ( stopDma( xferCmd, &xferCount ) != true )
    {
        rc = ataReturnErrorDMA;
    }

    else if ( status & ataStatusDRQ )
    {
        rc = ataReturnErrorDMA;
    }

    else if ( status & ataStatusERR )
    {
        rc = ataReturnErrorStatus;
    }

    else if ( reqCount != xferCount )
    {
        rc = ataReturnErrorProtocol;
    }

    completeCmd( xferCmd, rc, xferCount );
} 

/*----------------------------------- ATAPI DMA Protocols ------------------------------*/

/*
 *
 *
 *
 */
void AppleATA::doATAPIProtocolDma( IOATACommand *cmd )
{    
    ATATaskfile		taskfile;
    ATAPICmd		atapiCmd;
    UInt16		*pCDB = NULL;
    UInt32		regmask;
    UInt32		i;
    UInt32		status = 0;

    xferCount  = 0;

    cmd->getTaskfile( &taskfile );
    cmd->getATAPICmd( &atapiCmd );

    regmask = taskfile.regmask;

    if ( regmask & ATARegtoMask(ataRegDriveHead) )
    {
        regmask &= ~ATARegtoMask(ataRegDriveHead);
        if ( selectDrive( taskfile.ataRegs[ataRegDriveHead] ) == false )
        {
            completeCmd( cmd, ataReturnErrorBusy );
            return;
        }          
    }

    xferInts = 1;

    if ( cmd->getTimeout() != 0 )
    {
        xferCmdTimer = cmd->getTimeout() / ATATimerIntervalmS + 1;
    }

    for ( i = 0; regmask; i++ )
    {
        if ( regmask & 1 )
        {
            writeATAReg( i, taskfile.ataRegs[i] );
        }
        regmask >>= 1;
    }

    xferCount = 0;

    programDma( cmd );
    
    if ( cmd->getDevice()->getATAPIPktInt() == false )
    {
        do
        {
            status = readATAReg( ataRegStatus );
        }
        while ( (status & atapiStatusBSY) || !(status & atapiStatusDRQ) );
 
        pCDB = (UInt16 *)atapiCmd.cdb;
        for ( i = 0; i < atapiCmd.cdbLength >> 1; i++ )
        {
            writeATAReg( ataRegData, *pCDB++ );
        }

        startDma( cmd );
    }
}


/*
 *
 * 
 */
void AppleATA::processATAPIDmaInt()
{
    ATAReturnCode	rc = ataReturnErrorProtocol;    
    UInt32		status;
    UInt32		intReason;

    if ( waitForStatus( 0, ataStatusBSY, ataBusyTimeoutmS ) == false )
    {
        completeCmd( xferCmd, ataReturnErrorBusy, xferCount );
        return;
    }

    status    = readATAReg( atapiRegStatus );
    intReason = readATAReg( atapiRegIntReason );

    if ( !xferInts ) return;

    if ( (status & atapiStatusDRQ) && (intReason & atapiIntReasonCD) && !(intReason & atapiIntReasonIO) )
    {
        rc = sendATAPIPacket();
        if ( rc != ataReturnNoError )
        {
            completeCmd( xferCmd, rc );
        }

        else if ( startDma( xferCmd ) != true )
        {
            rc = ataReturnErrorDMA;
            completeCmd( xferCmd, rc );    
        }
    }

    else if ( !(status & atapiStatusDRQ) && (intReason & atapiIntReasonCD) && (intReason & atapiIntReasonIO) )    
    {  
        xferInts = 0;

        if ( stopDma( xferCmd, &xferCount ) != true )
        {
            rc = ataReturnErrorDMA;
            xferCount = 0;      
        }
        else
        {
            rc = (status & atapiStatusCHK) ? ataReturnErrorStatus : ataReturnNoError; 
        }

        if ( rc == ataReturnErrorStatus )
        { 
            updateCmdStatus( xferCmd, rc, xferCount );

            if ( doRequestSense( xferCmd ) == false )
            {
                completeCmd( xferCmd );
            }        
        }
        else
        {
            completeCmd( xferCmd, rc, xferCount ); 
        }  
    }

    else 
    {
        stopDma( xferCmd, &xferCount );
        completeCmd( xferCmd, rc, 0 );
    }
}

