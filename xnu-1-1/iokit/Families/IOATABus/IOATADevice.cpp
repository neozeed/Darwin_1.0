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
 * IOATADevice.cpp
 *
 */

#include <IOKit/IOSyncer.h>
#include <IOKit/ata/IOATA.h>
#include <IOKit/ata/IOATAController.h>
#include "ATAPrivate.h"

#undef  super
#define super IOService

OSDefineMetaClassAndStructors( IOATADevice, IOService );

extern EndianTable 		AppleIdentifyEndianTable[];

extern UInt32			AppleNumPIOModes;
extern ATAModeTable 		ApplePIOModes[];
extern UInt32			AppleNumDMAModes;
extern ATAModeTable		AppleDMAModes[];
extern UInt32			AppleNumUltraModes;
extern ATAModeTable		AppleUltraModes[];

/*
 *
 *
 */
bool IOATADevice::probeDevice()
{
    OSDictionary		*propTable = 0;

    if ( open( this ) != true )
    {
        goto probeDevice_error;
    }

    if ( doIdentify( (void **)&identifyData ) != ataReturnNoError )
    {
        goto probeDevice_error;
    }

    if ( deviceType == ataDeviceATA )
    {
        doSpinUp();
    }

    else if ( deviceType == ataDeviceATAPI )
    {
        atapiPktInt = ((identifyData->generalConfiguration & atapiPktProtocolIntDRQ) != 0);
      
        if ( doInquiry( (void **)&inquiryData ) != ataReturnNoError )
        {
            goto probeDevice_error;
        }

        reqSenseCmd = allocCommand();
    }      

    if ( getATATimings() != true )
    {
        goto probeDevice_error;
    }
    
    propTable = createProperties();
    if ( !propTable )
    {
        goto probeDevice_error;
    }
    
    setPropertyTable( propTable );

    propTable->release();
 
    close( this );

    return true; 

probeDevice_error: ;
    close( this );
    return false;
}

/*
 *
 *
 *
 */
ATADeviceType IOATADevice::probeDeviceType()
{
    ATATaskfile         taskfile;
    ATAResults          results;

    bzero( (void *)&taskfile, sizeof(taskfile) );

    taskfile.protocol     = ataProtocolSetRegs;
    taskfile.regmask      = ATARegtoMask(ataRegDriveHead);  

    taskfile.resultmask   = ATARegtoMask(ataRegSectorCount)
                          | ATARegtoMask(ataRegSectorNumber)
                          | ATARegtoMask(ataRegCylinderLow)
                          | ATARegtoMask(ataRegCylinderHigh)
                          | ATARegtoMask(ataRegStatus);

    taskfile.ataRegs[ataRegDriveHead] = ataModeLBA | (getUnit() << 4);

    utilCmd->setTaskfile( &taskfile );
    utilCmd->execute();

    if ( utilCmd->getResults( &results ) != ataReturnNoError )     
    {
        return (deviceType = ataDeviceNone);
    }
 
    if ( results.ataRegs[ataRegSectorCount] == ataSignatureSectorCount
          && results.ataRegs[ataRegSectorNumber] == ataSignatureSectorNumber
             && results.ataRegs[ataRegCylinderLow] == ataSignatureCylinderLow
                && results.ataRegs[ataRegCylinderHigh] == ataSignatureCylinderHigh )
    { 
        if ( !(results.ataRegs[ataRegStatus] & ataStatusBSY)  
                 && (results.ataRegs[ataRegStatus] & ataStatusDRDY) )
        {
            return (deviceType = ataDeviceATA);
        }
    }
            
    if ( results.ataRegs[ataRegCylinderLow] == atapiSignatureCylinderLow
                && results.ataRegs[ataRegCylinderHigh] == atapiSignatureCylinderHigh )
    {
        return (deviceType = ataDeviceATAPI);
    }       
  
    return (deviceType = ataDeviceNone);
}


/*
 *
 *
 *
 */
ATAReturnCode IOATADevice::doSpinUp()
{
    void		*buffer = NULL;
    ATAReturnCode	rc;

    rc = doSectorCommand( ataCommandReadSector, 0, 1, &buffer );

    if ( rc != ataReturnNoError )
    {
        return rc;
    }

    IOFree( buffer, 512 );

    return rc ; 
}    

/*
 *
 *
 *
 */
ATAReturnCode IOATADevice::doIdentify( void **dataPtr )
{   
    ATACommand		ataCmd;
    ATAReturnCode	rc;
 
    ataCmd = (deviceType == ataDeviceATA) ? ataCommandIdentify : atapiCommandIdentify;

    rc = doSectorCommand( ataCmd, 0, 1, dataPtr );

    if ( rc != ataReturnNoError )
    {
        return rc;
    }
 
    endianConvertData( *dataPtr, AppleIdentifyEndianTable );

    return rc;
}



/*
 *
 *
 *
 */
ATAReturnCode IOATADevice::doSectorCommand( ATACommand ataCmd, UInt32 ataLBA, UInt32 ataCount, void **dataPtr )
{
    ATATaskfile			taskfile;
    ATAResults			result;
    IOMemoryDescriptor  	*desc;
    UInt32			size;
    void			*data;
    UInt32			i;
    ATAReturnCode		rc;

    *dataPtr = NULL;

    size = ataCount * 512;

    if ( !(data = (void *)IOMalloc(size)) )
    {
        return ataReturnNoResource;
    }

    bzero( &taskfile, sizeof(taskfile) );

    desc = IOMemoryDescriptor::withAddress( data, size, kIODirectionIn );
    if ( desc == NULL )
    {
        rc = ataReturnNoResource;
        goto doSectorCommand_error;
    }

    
    taskfile.protocol    		= ataProtocolPIO;
    taskfile.regmask      		= ATARegtoMask(ataRegDriveHead)
                          		| ATARegtoMask(ataRegSectorCount)
                          		| ATARegtoMask(ataRegSectorNumber)
                          		| ATARegtoMask(ataRegCylinderLow)
                          		| ATARegtoMask(ataRegCylinderHigh)
                          		| ATARegtoMask(ataRegFeatures)
                          		| ATARegtoMask(ataRegCommand);


    taskfile.resultmask			= ATARegtoMask(ataRegError) 
                                        | ATARegtoMask(ataRegStatus);

    taskfile.ataRegs[ataRegSectorCount]   = ataCount;
    taskfile.ataRegs[ataRegSectorNumber]  = ataLBA         & 0xff;
    taskfile.ataRegs[ataRegCylinderLow]   = (ataLBA >> 8)  & 0xff;
    taskfile.ataRegs[ataRegCylinderHigh]  = (ataLBA >> 16) & 0xff;
    taskfile.ataRegs[ataRegDriveHead]     = (ataLBA >> 24) & 0x0f;

    taskfile.ataRegs[ataRegDriveHead]   |= ataModeLBA | (getUnit() << 4);
    taskfile.ataRegs[ataRegCommand]      = ataCmd;

    for ( i = 0; i < 2; i++ )
    { 
        utilCmd->setTimeout( 25000 );
        utilCmd->setTaskfile( &taskfile );
        utilCmd->setPointers( desc, size, false ); 
        submitCommand( utilCmd );
    
        rc  = utilCmd->getResults( &result );
        if ( rc == ataReturnNoError )
        {
            break;
        }
    }


doSectorCommand_error: ;

    desc->release();

    if ( rc != ataReturnNoError )
    {
        IOFree( data, size );
        return result.returnCode;
    }

    *dataPtr = data;

    return ataReturnNoError;
}


/*
 *
 *
 */
ATAReturnCode IOATADevice::doInquiry( void **dataPtr )
{
    ATATaskfile			taskfile;
    ATAPICmd			atapiCmd;
    ATAResults			result;
    void                        *data;
    IOMemoryDescriptor  	*desc;
    UInt32			size = sizeof(ATAPIInquiry);

    *dataPtr = 0;

    if ( !(data = (void *)IOMalloc(size)) )
    {
        return ataReturnNoResource;;
    }

    bzero( &taskfile, sizeof(taskfile) );
    bzero( &atapiCmd, sizeof(atapiCmd) );

    desc = IOMemoryDescriptor::withAddress( data, size, kIODirectionIn );
    
    taskfile.protocol   		= atapiProtocolPIO;
    taskfile.regmask      		= ATARegtoMask(atapiRegDeviceSelect) 
                          		| ATARegtoMask(atapiRegCommand)
                                        | ATARegtoMask(atapiRegByteCountLow)
                                        | ATARegtoMask(atapiRegByteCountHigh)
                                        | ATARegtoMask(atapiRegFeatures);
    taskfile.ataRegs[atapiRegDeviceSelect]  = ataModeLBA | (getUnit() << 4);
    taskfile.ataRegs[atapiRegCommand]       = atapiCommandPacket;
    taskfile.ataRegs[atapiRegFeatures]      = 0;
    taskfile.ataRegs[atapiRegByteCountLow]  = 0xfe;
    taskfile.ataRegs[atapiRegByteCountHigh] = 0xff;

    atapiCmd.cdbLength = 12;  // Fix 16 byte cmdpkts??
    atapiCmd.cdb[0]    = 0x12;
    atapiCmd.cdb[4]    = size;

    utilCmd->setATAPICmd( &atapiCmd, 0, 0 );
    utilCmd->setTaskfile( &taskfile );
    utilCmd->setPointers( desc, size, false );
    utilCmd->setTimeout( 5000 ); 
    submitCommand( utilCmd );
 
    if ( utilCmd->getResults(&result) == ataReturnNoError )
    {
        *dataPtr = data;
    }
    else
    {
        IOFree( data, size );
    }

    desc->release();

    return result.returnCode;
}

/*
 *
 *
 */
bool IOATADevice::getDeviceCapacity( UInt32 *blockMax, UInt32 *blockSize )
{
    UInt32		i;
    UInt32		data[2];

    if ( deviceType == ataDeviceATA )
    {
        if ( identifyData != NULL )
        {
            *blockMax = *(UInt32 *)identifyData->userAddressableSectors - 1;
            *blockSize  = 512;
            return true;
        }
    }
    
    if ( deviceType == ataDeviceATAPI )
    {
        for ( i=0; i < 8; i++ )
        {
            if ( doTestUnitReady() == ataReturnNoError )
            {
                break;
            }
        }

        if ( doReadCapacity( data ) == ataReturnNoError )
        {
            *blockMax   = OSSwapBigToHostInt32( data[0] );
            *blockSize  = OSSwapBigToHostInt32( data[1] );
            return true;
        }      
    }

    return false;
}


ATAReturnCode IOATADevice::doTestUnitReady()
{
    ATATaskfile			taskfile;
    ATAPICmd			atapiCmd;
    ATAResults			result;

    bzero( &taskfile, sizeof(taskfile) );
    bzero( &atapiCmd, sizeof(atapiCmd) );

    taskfile.protocol   		= atapiProtocolPIO;

    taskfile.regmask      		= ATARegtoMask(atapiRegDeviceSelect) 
                          		| ATARegtoMask(atapiRegCommand)
                                        | ATARegtoMask(atapiRegByteCountLow)
                                        | ATARegtoMask(atapiRegByteCountHigh)
                                        | ATARegtoMask(atapiRegFeatures);

    taskfile.ataRegs[atapiRegDeviceSelect]  = ataModeLBA | (getUnit() << 4);
    taskfile.ataRegs[atapiRegCommand]       = atapiCommandPacket;
    taskfile.ataRegs[atapiRegFeatures]      = 0;
    taskfile.ataRegs[atapiRegByteCountLow]  = 0xfe;
    taskfile.ataRegs[atapiRegByteCountHigh] = 0xff;

    atapiCmd.cdbLength = 12;  // Fix 16 byte cmdpkts??
    atapiCmd.cdb[0]    = 0x00;

    utilCmd->setATAPICmd( &atapiCmd );
    utilCmd->setTaskfile( &taskfile );
    utilCmd->setPointers( (IOMemoryDescriptor *)NULL, 0, false ); 
    utilCmd->setTimeout( 5000 );
    submitCommand( utilCmd );
    utilCmd->getResults(&result);
 
    return result.returnCode;
}


/*
 *
 *
 */
ATAReturnCode IOATADevice::doReadCapacity( void *data )
{
    ATATaskfile			taskfile;
    ATAPICmd			atapiCmd;
    ATAResults			result;
    IOMemoryDescriptor  	*dataDesc;
    UInt32			size = 8;


    bzero( &taskfile, sizeof(taskfile) );
    bzero( &atapiCmd, sizeof(atapiCmd) );

    dataDesc = IOMemoryDescriptor::withAddress( data, size, kIODirectionIn );
    if ( dataDesc == NULL )
    {
        return ataReturnNoResource;
    }
    
    taskfile.protocol   		= atapiProtocolPIO;
    taskfile.regmask      		= ATARegtoMask(atapiRegDeviceSelect) 
                          		| ATARegtoMask(atapiRegCommand)
                                        | ATARegtoMask(atapiRegByteCountLow)
                                        | ATARegtoMask(atapiRegByteCountHigh)
                                        | ATARegtoMask(atapiRegFeatures);
    taskfile.ataRegs[atapiRegDeviceSelect]  = ataModeLBA | (getUnit() << 4);
    taskfile.ataRegs[atapiRegCommand]       = atapiCommandPacket;
    taskfile.ataRegs[atapiRegFeatures]      = 0;
    taskfile.ataRegs[atapiRegByteCountLow]  = 0xfe;
    taskfile.ataRegs[atapiRegByteCountHigh] = 0xff;

    atapiCmd.cdbLength = 12;  // Fix 16 byte cmdpkts??
    atapiCmd.cdb[0]    = 0x25;

    utilCmd->setATAPICmd( &atapiCmd );
    utilCmd->setTaskfile( &taskfile );
    utilCmd->setPointers( dataDesc, size, false ); 
    utilCmd->setTimeout( 5000 );
    submitCommand( utilCmd );
    
    utilCmd->getResults(&result);

    dataDesc->release();
 
    return result.returnCode;
}


/*
 *
 *
 */
IOATACommand *IOATADevice::makeRequestSense( IOATACommand *origCmd )
{
    ATATaskfile			taskfile;
    ATAPICmd			atapiCmd;

    if ( reqSenseCmd == NULL )
    {
        return NULL;
    }

    bzero( &taskfile, sizeof(taskfile) );
    bzero( &atapiCmd, sizeof(atapiCmd) );

    taskfile.protocol   		= atapiProtocolPIO;
 
    taskfile.regmask      		= ATARegtoMask(atapiRegDeviceSelect) 
                          		| ATARegtoMask(atapiRegCommand)
                                        | ATARegtoMask(atapiRegByteCountLow)
                                        | ATARegtoMask(atapiRegByteCountHigh)
                                        | ATARegtoMask(atapiRegFeatures);

    taskfile.ataRegs[atapiRegDeviceSelect]  = ataModeLBA | (getUnit() << 4);
    taskfile.ataRegs[atapiRegCommand]       = atapiCommandPacket;
    taskfile.ataRegs[atapiRegFeatures]      = 0;
    taskfile.ataRegs[atapiRegByteCountLow]  = origCmd->senseLength & 0xff;
    taskfile.ataRegs[atapiRegByteCountHigh] = 0;

    atapiCmd.cdbLength   = 12;  // Fix 16 byte cmdpkts??
    atapiCmd.cdb[0]      = 0x03;
    atapiCmd.cdb[4]      = origCmd->senseLength;

    reqSenseCmd->setATAPICmd( &atapiCmd, 0, 0 );
    reqSenseCmd->setTaskfile( &taskfile );
    reqSenseCmd->setPointers( origCmd->senseData, origCmd->senseLength, false ); 

    return reqSenseCmd;
}

/*
 *
 *
 */
bool IOATADevice::completeRequestSense( IOATACommand *origCmd, IOATACommand  *reqSense )
{
    ATAResults		result;

    if ( reqSense->getResults(&result) != ataReturnNoError )
    {
        return false;
    }
    
    origCmd->results.senseLength = result.bytesTransferred;
    return true;
}


/*
 *
 *
 */ 
bool IOATADevice::getTimingsSupported( ATATimingProtocol *timingsSupported )
{
    UInt32			i;

    *(UInt32 *)timingsSupported = 0;
 
    for ( i=0; i < numTimings; i++ )
    {
        *(UInt32 *) timingsSupported |= (UInt32)ataTimings[i].timingProtocol; 
    }

    return true;
}

/*
 *
 *
 */ 
bool IOATADevice::getTiming( ATATimingProtocol *timingProtocol, ATATiming *timing )
{
    UInt32			i;

    for ( i=0; i < numTimings; i++ )
    {
        if ( ataTimings[i].timingProtocol == *timingProtocol )
        {
            bcopy( &ataTimings[i], timing, sizeof(ATATiming) );
            return true;
        }
    }

    return false;
}


/*
 *
 *
 */ 
bool IOATADevice::selectTiming( ATATimingProtocol timingProtocol )
{
    ATATaskfile			taskfile;
    ATAResults			result;
    bool			rc = false;
    UInt32			i;

    for ( i=0; i < numTimings; i++ )
    {
        if ( ataTimings[i].timingProtocol == timingProtocol )
        {
            rc = true;
            break;
        }
    }

    if ( rc == false ) return rc;
    
    if ( controller->selectTiming( getUnit(), timingProtocol ) == false )
    {
        return false;
    }
     
    bzero( &taskfile, sizeof(taskfile) );
    bzero( &result, sizeof(result) );

    taskfile.protocol    		= ataProtocolPIO;
    taskfile.regmask      		= ATARegtoMask(ataRegFeatures) 
                          		| ATARegtoMask(ataRegSectorCount) 
                          		| ATARegtoMask(ataRegDriveHead) 
                          		| ATARegtoMask(ataRegCommand);
 
    taskfile.ataRegs[ataRegSectorCount]  = ataTimings[i].featureSetting;
    taskfile.ataRegs[ataRegFeatures]     = ataFeatureTransferMode;
    taskfile.ataRegs[ataRegDriveHead]    = ataModeLBA | (getUnit() << 4);
    taskfile.ataRegs[ataRegCommand]      = ataCommandSetFeatures;

    utilCmd->setTaskfile( &taskfile );
    utilCmd->setPointers( (IOMemoryDescriptor *)NULL, 0, false );
    utilCmd->setTimeout( 5000 );
    utilCmd->setCallback(); 
    submitCommand( utilCmd );
 
    if ( utilCmd->getResults( &result ) != ataReturnNoError )
    {
        rc = false;
    }
    return rc;
}
    

/*
 *
 *
 */ 
bool IOATADevice::getATATimings()
{
    int			i, n;
    UInt32 	        mode		= 0;
    UInt32     		cycleTime	= 0;

    ATATiming		*pTimings;

    pTimings = ataTimings;

    /*
     *  PIO Cycle timing......  
     *
     *  1. Try to match Word 51 (pioCycleTime) with cycle timings
     *     in our pioModes table to get mode/CycleTime. (Valid for Modes 0-2)
     *  2. If Words 64-68 are supported and Mode 3 or 4 supported check, 
     *     update CycleTime with Word 68 (CycleTimeWithIORDY).
     */

    cycleTime = identifyData->pioMode;

    if ( cycleTime > 2 )
    {
        for ( i=AppleNumPIOModes-1; i != -1; i-- )
        {
            if ( cycleTime <= ApplePIOModes[i].minDataCycle )
            {
                mode = i;
                break;
            }
         }

         if ( i == -1 )
         {
             cycleTime = ApplePIOModes[mode].minDataCycle;
         }
    }
    else
    {
        mode      = cycleTime;
        cycleTime = ApplePIOModes[mode].minDataCycle;
    }


    if ( identifyData->validFields & identifyWords_64to70_Valid ) 
    {
	if (identifyData->advancedPIOModes & advPIOModes_Mode4_Supported)
            mode = 4;
	else if (identifyData->advancedPIOModes & advPIOModes_Mode3_Supported)
            mode = 3;

        if ( (mode >= 3) && identifyData->minPIOCyclcTimeIORDY )
        {
            cycleTime = identifyData->minPIOCyclcTimeIORDY;
        }
    }
    
    pTimings->timingProtocol = ataTimingPIO;
    pTimings->mode	      = mode;
    pTimings->featureSetting  = mode | ataTransferModePIOwFC;
    pTimings->minDataCycle    = cycleTime;
    pTimings->minDataAccess   = ApplePIOModes[mode].minDataAccess;

    if ( controller->calculateTiming( getUnit(), pTimings ) == false )
    {
        IOLog("IOATADevice::%s() - Controller driver must support PIO timings\n\r", __FUNCTION__);
        return false;
    }

    pTimings++;
    numTimings++;

    /* 
     *  Multiword DMA timing.....
     *
     *  1. Check Word 63(7:0) (Multiword DMA Modes Supported). Lookup
     *     CycleTime for highest mode we support.
     *  2. If Words 64-68 supported, update CycleTime from Word 66
     *     (RecommendedMultiWordCycleTime) if specified.
     */                                                                

    n = identifyData->dmaModes & dmaModes_Supported;
    if ( n )
    {
        for ( i=0; n; i++, n>>=1 )
          ;

        mode = i - 1;
        if ( mode > AppleNumDMAModes-1 )
        {
            mode = AppleNumDMAModes-1;
        }
        cycleTime = AppleDMAModes[mode].minDataCycle;

        if (identifyData->validFields & identifyWords_64to70_Valid) 
        {
            if ( identifyData->recDMACycleTime )
            {
                cycleTime = identifyData->recDMACycleTime;
            }
        }
        pTimings->timingProtocol = ataTimingDMA;
        pTimings->mode	         = mode;
        pTimings->featureSetting = mode | ataTransferModeDMA;
        pTimings->minDataCycle   = cycleTime;
        pTimings->minDataAccess  = AppleDMAModes[mode].minDataAccess;

        if ( controller->calculateTiming( getUnit(), pTimings ) == true )
        {
            pTimings++;
            numTimings++;
        }
    }

    /* 
     *  Ultra DMA timing.....
     *
     */                                                                
    if ( identifyData->validFields & identifyWords_88to88_Valid ) 
    {
        n = identifyData->ultraDMAModes & ultraDMAModes_Supported;
        if ( n )
        {
            for ( i=0; n; i++, n>>=1 )
              ;

            mode = i - 1;
            if ( mode > AppleNumUltraModes-1 )
            {
                mode = AppleNumUltraModes-1;
            }

            /*
             * Build a separate timing entry for Ultra DMA/33 (mode <= 2) and Ultra DMA/66
             */
            while ( 1 )
            { 
                cycleTime = AppleUltraModes[mode].minDataCycle;

                pTimings->timingProtocol = (mode > 2) ? ataTimingUltraDMA66 : ataTimingUltraDMA33;
                pTimings->mode	         = mode;
                pTimings->featureSetting = mode | ataTransferModeUltraDMA33;
                pTimings->minDataCycle   = cycleTime;
                pTimings->minDataAccess  = AppleUltraModes[mode].minDataAccess;
  
                if ( controller->calculateTiming( getUnit(), pTimings ) == true )
                {
                    pTimings++;
                    numTimings++;
                }
                
                if ( mode < 3 ) break; 
           
                mode = 2;
            }
        }
    }

    return true;            
}

/*
 *
 *
 */
void IOATADevice::submitCommand( IOATACommand *cmd )
{
    cmd->execute();
    IOWriteLock( resetSem );
    IORWUnlock( resetSem );
}

/*
 *
 *
 */
bool IOATADevice::executeCommand( IOATACommand *cmd )
{
    bool	 	isSync;

    isSync = !(cmd->flags & IOATACommand::atacmdCallbackValid);

    if ( isSync )
    {
        cmd->completionInfo.sync.syncer = IOSyncer::create();
    }

    cmd->deviceQueue = deviceQueue;

    controller->executeCommand( cmd );
    
    if ( isSync )
    {
        cmd->completionInfo.sync.syncer->wait();
    }

    return true;
}

/*
 *
 *
 */
void IOATADevice::completeCommand( IOATACommand *cmd )
{
    ATATaskfile		tf;
    ATAResults		res;
    UInt32		i;

    cmd->getTaskfile(&tf);
    cmd->getResults(&res);

#if 0
    IOLog("ATA command = %02x, RegMask = %04x ResultMask = %04x ReturnCode = %04x\n\r", 
           tf.protocol, (int)tf.regmask, (int)tf.resultmask, (int)res.returnCode );

    IOLog("ATA command regs:      ");

    for (i=0; i < MAX_ATA_REGS; i++ )
    {
        IOLog("%04x ", tf.ataRegs[i]);
    }

    IOLog("\n\rATA result regs:       ");

    for (i=0; i < MAX_ATA_REGS; i++ )
    {
        IOLog("%04x ", res.ataRegs[i]);
    }
    IOLog("\n\r");
#endif

    if ( cmd->flags & IOATACommand::atacmdCallbackValid )
    {
        (*cmd->completionInfo.async.ataDoneFn)(              cmd->completionInfo.async.target, 
                                                (IOService *)this, 
                                                             cmd, 
                                                             cmd->completionInfo.async.refcon );
    }
    else
    {
        cmd->completionInfo.sync.syncer->signal();
    }
}

/*
 *
 *
 */
IOReturn IOATADevice::message( UInt32 p0, IOService *p1, void *p2 )
{
    UInt32		  	msgId;
    IOATADevice			*ataDev;    

    msgId  = (UInt32) p0;
    ataDev = (IOATADevice *)p1;

    switch ( msgId )
    {
        case ataMessageResetStarted:
            lock_init( resetSem, true, NULL, NULL );
            IORWLockWrite( resetSem );
            break;
        
        case ataMessageResetComplete:
            IORWLockUnlock( resetSem );
            break;
 
        default:
            ;
    }

    return kIOReturnSuccess;
}

/*
 *
 *
 */
UInt32 IOATADevice::getUnit()
{
    return unit;
}

/*
 *
 *
 */
IOCommandQueue  *IOATADevice::getDeviceQueue()
{
    return deviceQueue;
}


/*
 *
 *
 */
ATADeviceType IOATADevice::getDeviceType()
{
    return deviceType;
}

/*
 *
 *
 */
bool IOATADevice::getATAPIPktInt()
{
    return atapiPktInt;
}

/*
 *
 *
 */
bool IOATADevice::getIdentifyData( ATAIdentify *identifyBuffer )
{
    if ( identifyData == NULL )
    {
        bzero( identifyBuffer, sizeof(ATAIdentify) );
        return false;
    }

    bcopy( identifyData, identifyBuffer, sizeof(ATAIdentify) );
    return true;
}

/*
 *
 *
 */
bool IOATADevice::getInquiryData( UInt32 inquiryBufLength, ATAPIInquiry *inquiryBuffer )
{        
    bzero( inquiryBuffer, inquiryBufLength );

    if ( inquiryData == NULL )
    {
        return false;
    }

    bcopy( inquiryData, inquiryBuffer, inquiryBufLength );

    return true;
}

/*
 *
 *
 */
bool IOATADevice::init( UInt32 unitNum, IOATAController *ctlr )
{
    controller = ctlr;
    unit       = unitNum;

    if ( super::init() != true )
    {
        return false;
    }

    deviceQueue = controller->createDeviceQueue( this ); 
    if ( deviceQueue == NULL )
    {
        return false;
    }

    utilCmd = allocCommand();
    if ( utilCmd == NULL )
    {
        return false;
    }

    resetSem = IORWLockAlloc();
    if ( resetSem == NULL )
    {
        return false;
    }

    return true;
}


/*
 *
 *
 */
OSDictionary *IOATADevice::createProperties()
{
    OSDictionary 	*propTable = 0;
    OSObject		*regObj;
    char		tmpbuf[81];
    char		*s, *d;
   

    propTable = OSDictionary::withCapacity(ATAMaxProperties);
    if ( propTable == NULL )
    {
        return NULL;
    }

    s = (deviceType == ataDeviceATA) ? ATAPropertyProtocolATA : ATAPropertyProtocolATAPI;
    regObj = (OSObject *)OSString::withCString( s );
    if ( addToRegistry( propTable, regObj, ATAPropertyProtocol ) != true )
    {
        goto createprop_error;
    }

    regObj = (OSObject *)OSNumber::withNumber(unit,32);
    if ( addToRegistry( propTable, regObj, ATAPropertyDeviceNumber ) != true )
    {
        goto createprop_error;
    }

    regObj = (OSObject *)OSNumber::withNumber(unit,32);
    if ( addToRegistry( propTable, regObj, ATAPropertyLocation ) != true )
    {
        goto createprop_error;
    }

    d = tmpbuf;
    stripBlanks( d, identifyData->modelNumber, sizeof(identifyData->modelNumber));
    regObj = (OSObject *)OSString::withCString( d );
    if ( addToRegistry( propTable, regObj, ATAPropertyModelNumber ) != true )
    {
        goto createprop_error;
    }

    d = tmpbuf;
    stripBlanks( d, identifyData->firmwareRevision, sizeof(identifyData->firmwareRevision));
    regObj = (OSObject *)OSString::withCString( d );
    if ( addToRegistry( propTable, regObj, ATAPropertyFirmwareRev ) != true )
    {
        goto createprop_error;
    }

    if ( inquiryData )
    {
        stripBlanks( d, inquiryData->vendorName, sizeof(inquiryData->vendorName) );
        regObj = (OSObject *)OSString::withCString( d );
        if ( addToRegistry( propTable, regObj, ATAPropertyVendorName ) != true )
        {
            goto createprop_error;
        }

        stripBlanks( d, inquiryData->productName, sizeof(inquiryData->productName) );
        regObj = (OSObject *)OSString::withCString( d );
        if ( addToRegistry( propTable, regObj, ATAPropertyProductName ) != true )
        {
            goto createprop_error;
        }

        stripBlanks( d, inquiryData->productRevision, sizeof(inquiryData->productRevision) );
        regObj = (OSObject *)OSString::withCString( d );
        if ( addToRegistry( propTable, regObj, ATAPropertyProductRevision ) != true )
        {
            goto createprop_error;
        }
    }
    return propTable;

createprop_error: ;
    propTable->release();
    return NULL;
}

/*
 *
 *
 */

bool IOATADevice::matchPropertyTable( OSDictionary * table )
{
  return( controller->matchNubWithPropertyTable( this, table ));
}

IOService *IOATADevice::matchLocation( IOService * )
{
//    IOLog( "IOATADevice::%s - called\n\r", __FUNCTION__ );

    return this;
}

/*
 *
 *
 */
bool IOATADevice::open( IOService *customer )
{
    bool		rc;

    rc = super::open( customer );
    
    if ( rc == true )
    {
        client = customer;
    }
    
    return rc;
}


/*
 *
 *
 */
void IOATADevice::close( IOService *customer )
{
    super::close( customer );
    client = NULL;
}


/*
 *
 *
 */
void IOATADevice::free()
{
    if ( identifyData ) IOFree( identifyData, sizeof(*identifyData) );
    if ( inquiryData  ) IOFree( inquiryData,  sizeof(*inquiryData)  );
    if ( utilCmd )      utilCmd->release();
    if ( reqSenseCmd )  reqSenseCmd->release();
    if ( deviceQueue )  deviceQueue->release();
    if ( resetSem )  	IORWLockFree( resetSem );

    super::free();
}
    
/*
 *
 *
 */
IOService *IOATADevice::getClient()
{
    return client;
}


/*
 *
 *
 */
IOATACommand *IOATADevice::allocCommand( UInt32 clientDataSize )
{
    IOATACommand	*cmd;

    if ( (cmd = controller->allocCommand( clientDataSize )) )
    {
        cmd->setDevice( this );
    }
    return cmd;
}

/*
 *
 *
 */
bool IOATADevice::addToRegistry( OSDictionary *propTable, OSObject *regObj, char *key )
{
    bool ret;

    if ( regObj == NULL )
    {
        return false;
    }

    ret = propTable->setObject( key, regObj );

    regObj->release();

    return ret;
}
           
/*
 *
 *
 */
void IOATADevice::stripBlanks( char *d, char *s, UInt32 l )
{
    char	*p, c;

    for ( p = d, c = *s; l && c ; l--)
    {
        c = (*d++ = *s++);
        if ( c != ' ' )
        {
            p = d;
        }
    }
    *p = 0;
}   


/*
 *
 *
 */
void IOATADevice::endianConvertData( void *data, void *endianTable )
{
    EndianTable		*t;

    union EndianPtr 
    {
        void            *voidPtr;
        UInt8		*bytePtr;
        UInt16		*shortPtr;
        UInt32		*longPtr;
        UInt64		*longlongPtr;
    } p;

    UInt32		i,j;

    p.voidPtr = data;

    t = (EndianTable *)endianTable;

    for ( ; t->type; t++ )
    {
        i = t->size/t->type;

        switch ( t->type )
        {
        
            /* Note:
             *
             * The ATA standard defines identify strings as arrays of short ints,
             * with the left-most character of the string as the most significant  
             * byte of the short int. Strings are not normally affected by the host
             * endianess. However, the way ATA defines strings would cause strings
             * to appear byte reversed. We do a manditory short int byte-swap here, 
             * although strictly speaking this is not an endian issue.
             *
             */
            case sizeof(UInt8):
              for ( j = 0; j < i/2; j++ )
              {
                  *p.shortPtr++ = OSSwapInt16(*p.shortPtr);
              }  
              
              break;
        
            case sizeof(UInt16):
              for ( j = 0; j < i; j++ )
              {
                  *p.shortPtr++ = OSSwapLittleToHostInt16(*p.shortPtr);
              }  
              break;

            case sizeof(UInt32):
              for ( j = 0; j < i; j++ )
              {
                  *p.longPtr++ = OSSwapLittleToHostInt32(*p.longPtr);
              }  
              break;

            case sizeof(UInt64):
              for ( j = 0; j < i; j++ )
              {
                  *p.longlongPtr++ = OSSwapLittleToHostInt64(*p.longlongPtr);
              }  
              break;

            default:
              ;
        }
    } 
}
