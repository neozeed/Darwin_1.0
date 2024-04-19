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
 *    AppleATAPPC.cpp
 *
 */
#include "AppleATAPPC.h"

#undef super
#define super AppleATA

extern pmap_t 	kernel_pmap;

OSDefineMetaClassAndStructors( AppleATAPPC, AppleATA )

static inline int rnddiv( int x, int y )
{
    if ( x < 0 )
      return 0;
    else
      return ( (x / y) + (( x % y ) ? 1 : 0) );
}


/*
 *
 *
 */
bool AppleATAPPC::configure( IOService *forProvider, UInt32 *controllerDataSize )
{
    *controllerDataSize = 0;

    provider = forProvider;

    if ( identifyController() == false )
    {
        return false;
    }

    ioMapATA = provider->mapDeviceMemoryWithIndex(0);
    if ( ioMapATA == NULL ) return false;
    ioBaseATA = (volatile UInt32 *)ioMapATA->getVirtualAddress();

    ioMapDMA = provider->mapDeviceMemoryWithIndex(1);
    if ( ioMapDMA == NULL ) return false;
    ioBaseDMA = (volatile IODBDMAChannelRegisters *)ioMapDMA->getVirtualAddress();

    dmaDescriptors = (IODBDMADescriptor *)kalloc(page_size);
    if ( dmaDescriptors == 0 )
    {
        return false;
    }
    
    dmaDescriptorsPhys = (UInt32) pmap_extract(kernel_pmap, (vm_offset_t) dmaDescriptors);

    if ( (UInt32)dmaDescriptors & (page_size - 1) )
    {
        IOLog("AppleATAPPC::%s() - DMA Descriptor memory not page aligned!!", __FUNCTION__);
        return false;
    }
  
    bzero( dmaDescriptors, page_size );

    numDescriptors = page_size/sizeof(IODBDMADescriptor);

    dmaMemoryCursor = IOBigMemoryCursor::withSpecification( 64*1024-2, 0xffffffff );
    if ( dmaMemoryCursor == NULL )
    {
        return false;
    } 

    bitBucketAddr = IOMalloc(32);
    if ( bitBucketAddr == 0 )
    {
        return false;
    }
    bitBucketAddrPhys = (UInt32) pmap_extract(kernel_pmap, (vm_offset_t) (((UInt32)bitBucketAddr + 0xf) & ~0x0f));

    return true;
}


/*
 *
 *
 */
bool AppleATAPPC::identifyController()
{
    OSData		*compatibleEntry, *modelEntry;

    do    
    {
        controllerType = kControllerTypeDBDMAVersion1;

        compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
        if ( compatibleEntry == 0 ) break;

        if ( compatibleEntry->isEqualTo( "keylargo-ata", sizeof("keylargo-ata")-1 ) == true ) 
        {
            controllerType = kControllerTypeDBDMAVersion2;

            modelEntry = OSDynamicCast( OSData, provider->getProperty("model") );  
            if ( modelEntry == 0 ) break;
      
            if ( modelEntry->isEqualTo( "ata-4", sizeof("ata-4")-1 ) == true ) 
            {
                controllerType = kControllerTypeUltra66DBDMA;
            }
        }    
    } while ( 0 );

    return true;
}    


/*
 *
 *
 */
bool AppleATAPPC::createWorkLoop( IOWorkLoop **workLoop )
{
    if ( super::createWorkLoop( workLoop ) != true )
    {
        return false;
    }
    
    interruptEventSource = IOInterruptEventSource::interruptEventSource( (OSObject *)             this,
                                                                         (IOInterruptEventAction) &AppleATAPPC::interruptOccurred,
									 (IOService *)            provider,
									 (int)                    0 );

    if ( interruptEventSource == NULL )
    {
        return false;
    }

    disableControllerInterrupts();

    (*workLoop)->addEventSource( interruptEventSource ); 

    timerEventSource = IOTimerEventSource::timerEventSource( this, (IOTimerEventSource::Action) &AppleATAPPC::ataTimer );
    if ( timerEventSource == NULL )
    {
        return false;
    }
    (*workLoop)->addEventSource( timerEventSource ); 
 
    ataTimer( timerEventSource ); 

    return true;
}


/*
 *
 *
 */
bool AppleATAPPC::provideProtocols( ATAProtocol *protocolsSupported )
{
    return false;
}


/*
 *
 *
 */
bool AppleATAPPC::provideTimings( UInt32 *numTimings, ATATiming *timingsSupported )
{
    return false;
}


/*
 *
 *
 */
bool AppleATAPPC::calculateTiming( UInt32 deviceNum, ATATiming *pTiming )
{
    bool		rc = false;

    switch ( controllerType )
    {
        case kControllerTypeDBDMAVersion1:
        case kControllerTypeDBDMAVersion2:
            switch ( pTiming->timingProtocol )
            {
                case ataTimingPIO:
                    rc = calculatePIOTiming( deviceNum, pTiming );
                    break;

                case ataTimingDMA:
                    rc = calculateDMATiming( deviceNum, pTiming );
                    break;
 
                default:
                    ;
            }
            break;

        case kControllerTypeUltra66DBDMA:
            switch ( pTiming->timingProtocol )
            {
                case ataTimingPIO:
                    rc = calculateUltra66PIOTiming( deviceNum, pTiming );
                    break;

                case ataTimingDMA:
                    rc = calculateUltra66DMATiming( deviceNum, pTiming );
                    break;
 
                case ataTimingUltraDMA66:
                    rc = calculateUltra66UDMATiming( deviceNum, pTiming );
                    break;

                default:
                    ;
            }
            break;

        default:
            ;
    }

    return rc;
}


/*
 *
 *
 */
bool AppleATAPPC::calculatePIOTiming( UInt32 unitNum, ATATiming *pTiming )
{
    int			accessTime;
    int			accessTicks;
    int         	recTime;
    int			recTicks;
    int        		cycleTime;

    /*
     * Calc PIO access time >= minDataAccess in SYSCLK increments
     */
    accessTicks = rnddiv(pTiming->minDataAccess, IDE_SYSCLK_NS);
    /*
     * Hardware limits access times to >= 120 ns 
     */
    accessTicks -= IDE_PIO_ACCESS_BASE;
    if (accessTicks < IDE_PIO_ACCESS_MIN )
    {
        accessTicks = IDE_PIO_ACCESS_MIN;
    }
    accessTime = (accessTicks + IDE_PIO_ACCESS_BASE) * IDE_SYSCLK_NS;

    /*
     * Calc recovery time in SYSCLK increments based on time remaining in cycle
     */
    recTime = pTiming->minDataCycle - accessTime;
    recTicks = rnddiv( recTime, IDE_SYSCLK_NS );
    /*
     * Hardware limits recovery time to >= 150ns 
     */
    recTicks -= IDE_PIO_RECOVERY_BASE;
    if ( recTicks < IDE_PIO_RECOVERY_MIN )
    {
      recTicks = IDE_PIO_RECOVERY_MIN;
    }

    cycleTime = (recTicks + IDE_PIO_RECOVERY_BASE + accessTicks + IDE_PIO_ACCESS_BASE) * IDE_SYSCLK_NS;

    ideTimingWord[unitNum] &= ~0x7ff;
    ideTimingWord[unitNum] |= accessTicks | (recTicks << 5);

#if 0
    IOLog("AppleATAPPC::%s() Unit %1d PIO Requested Timings: Access: %3dns Cycle: %3dns \n\r", 
             __FUNCTION__, (int)unitNum, (int)pTiming->minDataAccess, (int)pTiming->minDataCycle);
    IOLog("AppleATAPPC::%s()        PIO Actual    Timings: Access: %3dns Cycle: %3dns\n\r",
             __FUNCTION__, accessTime, cycleTime );
#endif

    return true;
}


/*
 *
 *
 */
bool AppleATAPPC::calculateDMATiming( UInt32 unitNum, ATATiming *pTiming )
{
    int			accessTime;
    int			accessTicks;
    int         	recTime;
    int			recTicks;
    int        		cycleTime;
    int                 cycleTimeOrig;
    int         	halfTick = 0;

    /*
     * Calc DMA access time >= minDataAccess in SYSCLK increments
     */

    /*
     * OHare II erata - Cant handle write cycle times below 150ns
     */
    cycleTimeOrig = pTiming->minDataCycle;
#if 0
    if ( IsPowerStar() )
    {
        if ( cycleTimeOrig < 150 ) pTiming->minDataCycle = 150;
    }
#endif

    accessTicks = rnddiv(pTiming->minDataAccess, IDE_SYSCLK_NS);

    accessTicks -= IDE_DMA_ACCESS_BASE;
    if ( accessTicks < IDE_DMA_ACCESS_MIN )
    {
        accessTicks = IDE_DMA_ACCESS_MIN;
    }
    accessTime = (accessTicks + IDE_DMA_ACCESS_BASE) * IDE_SYSCLK_NS;

    /*
     * Calc recovery time in SYSCLK increments based on time remaining in cycle
     */
    recTime = pTiming->minDataCycle - accessTime;    
    recTicks = rnddiv( recTime, IDE_SYSCLK_NS );

    recTicks -= IDE_DMA_RECOVERY_BASE;
    if ( recTicks < IDE_DMA_RECOVERY_MIN )
    {
        recTicks = IDE_DMA_RECOVERY_MIN;
    }
    cycleTime = (recTicks + IDE_DMA_RECOVERY_BASE + accessTicks + IDE_DMA_ACCESS_BASE) * IDE_SYSCLK_NS;
 
    /*
     * If our calculated access time is at least SYSCLK/2 > than what the disk requires, 
     * see if selecting the 1/2 Clock option will help. This adds SYSCLK/2 to 
     * the access time and subtracts SYSCLK/2 from the recovery time.
     * 
     * By setting the H-bit and subtracting one from the current access tick count,
     * we are reducing the current access time by SYSCLK/2 and the current recovery
     * time by SYSCLK/2. Now, check if the new cycle time still meets the disk's requirements.
     */  
    if ( controllerType == kControllerTypeDBDMAVersion1 )
    {
        if ( (accessTicks > IDE_DMA_ACCESS_MIN) &&  ((UInt32)(accessTime - IDE_SYSCLK_NS/2) >= pTiming->minDataAccess) )
        {
            if ( (UInt32)(cycleTime - IDE_SYSCLK_NS) >= pTiming->minDataCycle )
            {
                halfTick    = 1;
                accessTicks--;
                accessTime -= IDE_SYSCLK_NS/2;
                cycleTime  -= IDE_SYSCLK_NS;
            }
        }
    }

    ideTimingWord[unitNum] &= ~0xffff800;
    ideTimingWord[unitNum] |= (accessTicks | (recTicks << 5) | (halfTick << 10)) << 11;

#if 0
    IOLog("AppleATAPPC::%s() Unit %1d DMA Requested Timings: Access: %3dns Cycle: %3dns \n\r",  
             __FUNCTION__, (int)unitNum, (int)pTiming->minDataAccess, (int)cycleTimeOrig);
    IOLog("AppleATAPPC::%s()        DMA Actual    Timings: Access: %3dns Cycle: %3dns\n\r",   
             __FUNCTION__, accessTime, cycleTime );
    IOLog("AppleATAPPC::%s() Ide DMA Timings = %08lx\n\r", __FUNCTION__, ideTimingWord[unitNum] );
#endif

    return true;
}


/*
 *
 *
 */
bool AppleATAPPC::calculateUltra66PIOTiming( UInt32 unitNum, ATATiming *pTiming )
{
    int			accessTime;
    int			accessTicks;
    int         	recTime;
    int			recTicks;
    int        		cycleTime;

    /*
     * Calc PIO access time >= pioAccessTime in SYSCLK increments
     */
    accessTicks = rnddiv(pTiming->minDataAccess * 1000, IDE_ULTRA66_CLOCK_PS );
    accessTime = accessTicks * IDE_ULTRA66_CLOCK_PS;

    /*
     * Calc recovery time in SYSCLK increments based on time remaining in cycle
     */
    recTime = pTiming->minDataCycle * 1000 - accessTime;
    recTicks = rnddiv( recTime, IDE_ULTRA66_CLOCK_PS );

    cycleTime = (recTicks + accessTicks ) * IDE_ULTRA66_CLOCK_PS;

    ideTimingWord[unitNum] &= ~0xe00003ff;
    ideTimingWord[unitNum] |= accessTicks | (recTicks << 5);

#if 0
    IOLog("AppleATAPPC::%s()  Unit %1d PIO Requested Timings: Access: %3dns Cycle: %3dns \n\r",  
             __FUNCTION__, (int)unitNum, (int)pTiming->minDataAccess, (int)pTiming->minDataCycle);
    IOLog("AppleATAPPC::%s()         PIO Actual    Timings: Access: %3dns Cycle: %3dns\n\r",   
             __FUNCTION__, accessTime / 1000, cycleTime / 1000 );
    IOLog("AppleATAPPC::%s()  Ide PIO Timings = %08lx\n\r", __FUNCTION__, ideTimingWord[unitNum] );
#endif

    return true;
}


/*
 *
 *
 */
bool AppleATAPPC::calculateUltra66DMATiming( UInt32 unitNum, ATATiming *pTiming )
{
    int			accessTime;
    int			accessTicks;
    int         	recTime;
    int			recTicks;
    int        		cycleTime;

    /*
     * Calc DMA access time >= dmaAccessTime in SYSCLK increments
     */
    accessTicks = rnddiv(pTiming->minDataAccess * 1000, IDE_ULTRA66_CLOCK_PS);
    accessTime = accessTicks * IDE_ULTRA66_CLOCK_PS;

    /*
     * Calc recovery time in SYSCLK increments based on time remaining in cycle
     */
    recTime = pTiming->minDataCycle * 1000 - accessTime;    
    recTicks = rnddiv( recTime, IDE_ULTRA66_CLOCK_PS );

    cycleTime = (recTicks + accessTicks) * IDE_ULTRA66_CLOCK_PS;

    ideTimingWord[unitNum] &= ~0x001ffc00;
    ideTimingWord[unitNum] |= (accessTicks | (recTicks << 5)) << 10;

#if 0
    IOLog("AppleATAPPC::%s()  Unit %1d DMA Requested Timings: Access: %3dns Cycle: %3dns \n\r",  
             __FUNCTION__, (int)unitNum, (int)pTiming->minDataAccess, (int)pTiming->minDataCycle);
    IOLog("AppleATAPPC::%s()         DMA Actual    Timings: Access: %3dns Cycle: %3dns\n\r",   
             __FUNCTION__, accessTime / 1000, cycleTime / 1000 );
    IOLog("AppleATAPPC::%s()  Ide DMA Timings = %08lx\n\r", __FUNCTION__, ideTimingWord[unitNum] );
#endif

    return true;
}


/*
 *
 *
 */
bool AppleATAPPC::calculateUltra66UDMATiming( UInt32 unitNum, ATATiming *pTiming )
{
    int			rdyToPauseTicks;
    int			rdyToPauseTime;
    int        		cycleTime;
    int        		cycleTicks;

    /*
     * Ready to Pause delay in PCI_66_CLOCK / 2 increments
     */
    rdyToPauseTicks = rnddiv(pTiming->minDataAccess * 1000, IDE_ULTRA66_CLOCK_PS);
    rdyToPauseTime  = rdyToPauseTicks * IDE_ULTRA66_CLOCK_PS;

    /*
     * Calculate cycle time in PCI_66_CLOCK / 2 increments
     */
    cycleTicks = rnddiv(pTiming->minDataCycle * 1000, IDE_ULTRA66_CLOCK_PS);
    cycleTime  = cycleTicks * IDE_ULTRA66_CLOCK_PS;

    ideTimingWord[unitNum] &= ~0x1ff00000;
    ideTimingWord[unitNum] |= ((rdyToPauseTicks << 5) | (cycleTicks << 1) | 1) << 20;

#if 0
    IOLog("AppleATAPPC::%s() Unit %1d UDMA66 Requested Timings: ReadyToPause: %3dns Cycle: %3dns \n\r",  
             __FUNCTION__, (int)unitNum, (int)pTiming->minDataAccess, (int)pTiming->minDataCycle);
    IOLog("AppleATAPPC::%s()        UDMA66 Actual    Timings: ReadyToPause: %3dns Cycle: %3dns\n\r",   
             __FUNCTION__, rdyToPauseTime / 1000, cycleTime / 1000 );
    IOLog("AppleATAPPC::%s() Ide DMA Timings = %08lx\n\r", __FUNCTION__, ideTimingWord[unitNum] );
#endif

    return true;
}


/*
 *
 *
 */
void AppleATAPPC::newDeviceSelected( IOATADevice *newDevice )
{    
    OSWriteSwapInt32( ioBaseATA, 0x200, ideTimingWord[newDevice->getUnit()] );
    eieio();
}


/*
 *
 *
 */
bool AppleATAPPC::selectTiming( UInt32 unitNum, ATATimingProtocol timingProtocol )
{
    if ( controllerType == kControllerTypeUltra66DBDMA )
    {
        switch ( timingProtocol )
        {
             case ataTimingUltraDMA66:
                 ideTimingWord[unitNum] |=  0x00100000;
                 break;
             case ataTimingDMA:   
                 ideTimingWord[unitNum] &= ~0x00100000;
                 break;
             default:
                  ;
        }        
    }

    currentDevice = (IOATADevice *)NULL;
    return true;
}


/*
 *
 *
 */
void AppleATAPPC::ataTimer( IOTimerEventSource * /* sender */ )
{
    if ( xferCmdTimer != 0 )
    {
        if ( --xferCmdTimer == 0 )
        {
            IOLog("AppleATAPPC::%s() - Timeout occurred\n\r", __FUNCTION__ );

            if ( xferCmdSave != NULL )
            {
                xferCmd     = xferCmdSave;
                xferCmdSave = NULL;
            }

            updateCmdStatus( xferCmd, ataReturnErrorInterruptTimeout, xferCount );

            resetBusRequest();

            completeCmd( xferCmd );
        }
    }

    timerEventSource->setTimeoutMS(ATATimerIntervalmS);
}

/*
 *
 *
 */
bool AppleATAPPC::programDma( IOATACommand *cmd )
{
    IOMemoryDescriptor			*memoryDesc;
    IODBDMADescriptor			*dmaDesc;
    UInt32				dmaCmd;
    bool				isWrite;
    IOPhysicalSegment			physSeg;
    IOByteCount				offset;
    UInt32				i;

    IODBDMAReset( ioBaseDMA );

    cmd->getPointers( &memoryDesc, &dmaReqLength, &isWrite );

    if ( dmaReqLength == 0 )
    {
        return true;
    }

    offset = 0;

    dmaCmd  = (isWrite == true) ? kdbdmaOutputMore : kdbdmaInputMore;
    dmaDesc = dmaDescriptors;

   for ( i = 0; i < numDescriptors; i++, dmaDesc++ )
    {
        if ( dmaMemoryCursor->getPhysicalSegments( memoryDesc, offset, &physSeg, 1 ) != 1 )
        {
            break;
        }

        IOMakeDBDMADescriptor( dmaDesc,
                               dmaCmd,
			       kdbdmaKeyStream0,
			       kdbdmaIntNever,
			       kdbdmaBranchNever,
			       kdbdmaWaitNever,
                               physSeg.length,
                               physSeg.location );
	offset += physSeg.length;
    }

    if ( i == numDescriptors )
    {
        return false;
    }

    /*
     * Note: ATAPI always transfers even byte-counts. Send the extra byte to/from the bit-bucket
     *       if the requested transfer length is odd.
     */
    if ( dmaReqLength & 1 )
    {
        i++;
        IOMakeDBDMADescriptor( dmaDesc++,
	    		       dmaCmd,
	  		       kdbdmaKeyStream0,
			       kdbdmaIntNever,
			       kdbdmaBranchNever,
			       kdbdmaWaitNever,
			       1,
			       bitBucketAddrPhys );
    }


    if ( i == numDescriptors )
    {
        return false;
    }


    IOMakeDBDMADescriptor( dmaDesc,
                           kdbdmaStop,
                           kdbdmaKeyStream0,
			   kdbdmaIntNever,
			   kdbdmaBranchNever,
			   kdbdmaWaitNever,
                           0,
                           0 );

    IOSetDBDMACommandPtr( ioBaseDMA, dmaDescriptorsPhys );


    return true;
}                      

 
/*
 *
 *
 */
bool AppleATAPPC::startDma( IOATACommand * )
{
    if ( dmaReqLength != 0 )
    {
        IODBDMAContinue( ioBaseDMA );
    }
    return true;
}


/*
 *
 *
 */
bool AppleATAPPC::stopDma( IOATACommand *, UInt32 *transferCount )
{
    UInt32			i;
    UInt32 	       		ccResult;
    UInt32 		       	byteCount = 0;

    *transferCount = 0;

    if ( dmaReqLength == 0 )
    {
        return true;
    }

    IODBDMAStop( ioBaseDMA );

    for ( i=0; i < numDescriptors; i++ )
    {
        ccResult = IOGetCCResult( &dmaDescriptors[i] );
    
        if ( !(ccResult & kdbdmaStatusRun) )
        {
            break;
        } 
        byteCount += (IOGetCCOperation( &dmaDescriptors[i] ) & kdbdmaReqCountMask) - (ccResult & kdbdmaResCountMask); 
    }

    *transferCount = byteCount;

    return true;
}
 
/*
 *
 *
 */
void AppleATAPPC::disableControllerInterrupts()
{
    interruptEventSource->disable();
}

/*
 *
 *
 */
void AppleATAPPC::enableControllerInterrupts()
{
    interruptEventSource->enable();
}

/*
 *
 *
 */
void AppleATAPPC::free()
{
    if ( interruptEventSource != 0 )
    {
        interruptEventSource->disable();
        interruptEventSource->release();
    }
  
    if ( timerEventSource != 0 )
    {
        timerEventSource->release();
    }

    if ( ioMapATA != 0 )
    {
        ioMapATA->release();
    }
 
    if ( ioMapDMA != 0 )
    {
        ioMapDMA->release();
    }

    if ( bitBucketAddr != 0 )
    {
        IOFree( bitBucketAddr, 32 );
    }

    if ( dmaDescriptors != 0 )
    {
        kfree( (void *)dmaDescriptors, page_size );
    } 
}

/*
 *
 *
 */
void AppleATAPPC::writeATAReg( UInt32 regIndex, UInt32 regValue )
{
    regIndex += (regIndex >= ataRegDeviceControl ) ? (kCS3RegBase - ataRegDeviceControl + 6) : 0;

    if ( regIndex )
    {
        *((volatile UInt8 *)ioBaseATA + (regIndex<<4)) = regValue;
    }
    else
    {
        *(volatile UInt16 *)ioBaseATA = regValue;
    }     
    eieio();
}

UInt32 AppleATAPPC::readATAReg( UInt32 regIndex )
{
    regIndex += (regIndex >= ataRegAltStatus ) ? (kCS3RegBase - ataRegAltStatus + 6) : 0;

    if ( regIndex )
    { 
        return *((volatile UInt8 *)ioBaseATA + (regIndex<<4));
    }
    else
    {
        return *(volatile UInt16 *)ioBaseATA;
    }
}
