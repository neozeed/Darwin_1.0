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
 *    AppleATAPio.cpp
 *
 */
#include "AppleATA.h"

/*----------------------------------- ATA SetRegs Protocol ------------------------------*/

/*
 *
 *
 */
void AppleATA::doProtocolSetRegs( IOATACommand *cmd )
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

    for ( i = 0; regmask; i++ )
    {
        if ( regmask & 1 )
        {
            writeATAReg( i, taskfile.ataRegs[i] );
        }
        regmask >>= 1;
    }

    IODelay( 100 );

    completeCmd( cmd, ataReturnNoError );
}

/*----------------------------------- ATA PIO Protocol ------------------------------*/

/*
 *
 *
 */
void AppleATA::doATAProtocolPio( IOATACommand *cmd )
{    
    ATATaskfile		taskfile;
    UInt32		regmask;
    UInt32		i;
    UInt32		status = 0;

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
    cmd->getPointers( &xferDesc, &xferRemaining, &xferIsWrite );
    
    xferInts = ( xferRemaining ) ? xferRemaining / 512 : 1;
    
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

    if ( xferIsWrite )
    {
        do
        {
            status = readATAReg( ataRegStatus );
        }
        while ( !(status & ataStatusDRQ) );
     
        xferInts++;   
        interruptOccurred();
    }     
}


/*
 *
 * 
 */
void AppleATA::processATAPioInt()
{    
    UInt16		tmpBuffer[256];
    UInt32		status;
    UInt32		i;
    ATAReturnCode	rc = ataReturnNoError;
    UInt32		reqCount;

    if ( waitForStatus( 0, ataStatusBSY, ataBusyTimeoutmS ) == false )
    {
        completeCmd( xferCmd, ataReturnErrorBusy, xferCount );
        return;
    }

    status = readATAReg( ataRegStatus );

    if ( status & ataStatusDRQ )
    {
        if ( xferIsWrite == true )
        {
            xferDesc->readBytes( xferCount, tmpBuffer, 512 );

            for ( i=0; i < 256; i++ )
            {
                writeATAReg( ataRegData, tmpBuffer[i] );
            }
        }
        else
        {
            for ( i=0; i < 256; i++ )
            {
                tmpBuffer[i] = readATAReg( ataRegData );
            }
            xferDesc->writeBytes( xferCount, tmpBuffer, 512 );
        }

        xferCount     += 512;
        xferRemaining -= 512;
    }

    if ( status & ataStatusERR )
    {
        completeCmd( xferCmd, ataReturnErrorStatus, xferCount );
    }
    else if ( !--xferInts )
    {
        xferCmd->getPointers( 0, &reqCount, 0 );
        if ( xferCount != reqCount )
        {
            rc = ataReturnErrorProtocol;
        }

        completeCmd( xferCmd, rc, xferCount );
    }
} 

/*----------------------------------- ATAPI PIO Protocols ------------------------------*/

/*
 *
 *
 *
 */
void AppleATA::doATAPIProtocolPio( IOATACommand *cmd )
{    
    ATATaskfile		taskfile;
    ATAPICmd		atapiCmd;
    UInt16		*pCDB = NULL;
    UInt32		regmask;
    UInt32		status;
    UInt32		i;

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
    cmd->getPointers( &xferDesc, &xferRemaining, &xferIsWrite );

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
    }
}

/*
 *
 * 
 */
void AppleATA::processATAPIPioInt()
{
    ATAReturnCode	rc = ataReturnErrorProtocol;    
    UInt32		status;
    UInt32		intReason;
    UInt32		n;

    if ( waitForStatus( 0, ataStatusBSY, ataBusyTimeoutmS ) == false )
    {
        completeCmd( xferCmd, ataReturnErrorBusy, xferCount );
        return;
    }

    status    = readATAReg( atapiRegStatus );
    intReason = readATAReg( atapiRegIntReason );

    if ( !xferInts ) return;

    if ( status & atapiStatusDRQ )
    {
        if ( intReason & atapiIntReasonCD ) 
        {
            if ( !(intReason & atapiIntReasonIO) )
            {
                rc = sendATAPIPacket();
             }
        }
        else
        {
            n  = readATAReg( atapiRegByteCountLow ) | (readATAReg( atapiRegByteCountHigh ) << 8);
            n = (n+1) & ~0x01;

            if ( !(intReason & atapiIntReasonIO) && (xferIsWrite == true) )
            {
                rc = writeATAPIDevice( n );
            }
            else if ( (intReason & atapiIntReasonIO) && (xferIsWrite == false) )
            {
                rc = readATAPIDevice( n );
            }
        } 
    }
    else if ( (intReason & atapiIntReasonCD) && (intReason & atapiIntReasonIO) )    
    {  
        xferInts = 0;
        rc = (status & atapiStatusCHK) ? ataReturnErrorStatus : ataReturnNoError; 

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
}

/*
 *
 *
 */
ATAReturnCode AppleATA::sendATAPIPacket()
{
    UInt32		i;
    ATAPICmd		atapiCmd;
    UInt16		*pCDB;

    xferCmd->getATAPICmd( &atapiCmd );

    pCDB = (UInt16 *)atapiCmd.cdb;
    for ( i=0; i < atapiCmd.cdbLength >> 1; i++ )
    {
        writeATAReg( ataRegData, *pCDB++ );
    }
    return ataReturnNoError;
}    


/*
 *
 *
 */
ATAReturnCode AppleATA::readATAPIDevice( UInt32 n )
{
    UInt16      tmpBuffer[256];
    UInt32	i,j,k;

    while ( n )
    {
        j = (n < 512) ? n : 512;

        j >>= 1;
        for ( i=0; i < j; i++ )
        {
            tmpBuffer[i] = readATAReg( ataRegData );
        }
        j <<= 1;
        n  -= j;

        k = (j > xferRemaining ) ? xferRemaining : j;
      
        xferDesc->writeBytes( xferCount, tmpBuffer, k );
        
        xferCount     += k;
        xferRemaining -= k;
    }

    return ataReturnNoError;
}    

/*
 *
 *
 */
ATAReturnCode AppleATA::writeATAPIDevice( UInt32 n )
{
    UInt16      tmpBuffer[256];
    UInt32	i,j,k;


    while ( n )
    {
        j = (n < 512) ? n : 512;

        k = (j > xferRemaining ) ? xferRemaining : j;

        xferDesc->readBytes( xferCount, tmpBuffer, k );

        j >>= 1;
        for ( i=0; i < j; i++ )
        {
            writeATAReg( ataRegData, tmpBuffer[i] );
        }            
        j <<= 1;
        n  -= j;

        xferCount     += k;
        xferRemaining -= k;
    }

    return ataReturnNoError;
}               


/*
 *
 *
 */
bool AppleATA::selectDrive( UInt32 driveHeadReg )
{
    bool		isBusy = true;
    UInt32		i;
    UInt32		status;

    writeATAReg( ataRegDriveHead, driveHeadReg );

    for ( i=0; i < 10; i++ )
    {
        status = readATAReg( ataRegStatus );
        isBusy = ((status & ataStatusBSY) != 0);
        if ( isBusy == false )
        {
            break;
        }
        IODelay( 10 );
    }

    return !isBusy;     
}
