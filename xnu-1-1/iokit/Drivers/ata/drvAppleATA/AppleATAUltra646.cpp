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
 *    AppleATAUltra646.cpp
 *
 */
#include "AppleATAUltra646.h"

#include <IOKit/IODeviceTreeSupport.h>

#undef super
#define super AppleATA

extern pmap_t 	kernel_pmap;

OSDefineMetaClassAndStructors( AppleATAUltra646, AppleATA )


static struct
{
    UInt32	minDataAccess;
    UInt32	minDataCycle;
} pioModes[] =
{
    { 165,    600 },	/* Mode 0 */
    { 125,    383 },	/*      1 */ 
    { 100,    240 },	/*      2 */
    {  80,    180 },	/*      3 */
    {  70,    120 }	/*      4 */
};


/*
 *
 *
 */
bool AppleATAUltra646::configure( IOService *forProvider, UInt32 *controllerDataSize )
{
    *controllerDataSize = 0;

    provider = (IOPCIDevice *)forProvider;

    busNum = 0;

    ioMapATA[0] = provider->mapDeviceMemoryWithRegister( 0x10 + busNum * 8 + 0 );
    if ( ioMapATA[0] == NULL ) return false;
    ioBaseATA[0] = (volatile UInt32 *)ioMapATA[0]->getVirtualAddress();

    ioMapATA[1] = provider->mapDeviceMemoryWithRegister( 0x10 + busNum * 8 + 4 );
    if ( ioMapATA[1] == NULL ) return false;
    ioBaseATA[1] = (volatile UInt32 *)ioMapATA[1]->getVirtualAddress();

    pciWriteLong( 0x04, 0x05 );

    dmaDescriptors = (Ultra646Descriptor *)kalloc(page_size);
    if ( dmaDescriptors == 0 )
    {
        return false;
    }
    
    dmaDescriptorsPhys = (UInt32) pmap_extract(kernel_pmap, (vm_offset_t) dmaDescriptors);

    if ( (UInt32)dmaDescriptors & (page_size - 1) )
    {
        IOLog("AppleATAUltra646::%s() - DMA Descriptor memory not page aligned!!", __FUNCTION__);
        return false;
    }
  
    bzero( dmaDescriptors, page_size );

    numDescriptors = page_size/sizeof(Ultra646Descriptor);

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


    // assumes the first child determines the path OF uses to reference the controller
    pathProvider = OSDynamicCast(IOService, provider->getChildEntry(gIODTPlane));

    return true;
}


/*
 *
 *
 */
bool AppleATAUltra646::createWorkLoop( IOWorkLoop **workLoop )
{
    if ( super::createWorkLoop( workLoop ) != true )
    {
        return false;
    }
    
    interruptEventSource = IOInterruptEventSource::interruptEventSource( (OSObject *)             this,
                                                                         (IOInterruptEventAction) &AppleATAUltra646::interruptOccurred,
									 (IOService *)            provider,
									 (int)                    0 );

    if ( interruptEventSource == NULL )
    {
        return false;
    }

    disableControllerInterrupts();

    (*workLoop)->addEventSource( interruptEventSource ); 

    timerEventSource = IOTimerEventSource::timerEventSource( this, (IOTimerEventSource::Action) &AppleATAUltra646::ataTimer );
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
bool AppleATAUltra646::provideProtocols( ATAProtocol *protocolsSupported )
{
    return false;
}


/*
 *
 *
 */
bool AppleATAUltra646::provideTimings( UInt32 *numTimings, ATATiming *timingsSupported )
{
    return false;
}


/*
 *
 *
 */
bool AppleATAUltra646::calculateTiming( UInt32 unit, ATATiming *pTiming )
{
    bool		rc = false;

    ideTimingRegs[unit].arttimReg  = 0x40;
    ideTimingRegs[unit].cmdtimReg  = 0xA9;

    switch ( pTiming->timingProtocol )
    {
        case ataTimingPIO:
            rc = calculatePIOTiming( unit, pTiming );
            break;

        case ataTimingDMA:
            rc = calculateDMATiming( unit, pTiming );
            break;
 
        case ataTimingUltraDMA33:
            rc = calculateUltraDMATiming( unit, pTiming );
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
bool AppleATAUltra646::calculatePIOTiming( UInt32 unit, ATATiming *pTiming )
{
    UInt32		accessTime;
    UInt32		drwActClks, drwRecClks;
    UInt32		drwActTime, drwRecTime;
   
    accessTime = pioModes[pTiming->mode].minDataAccess;

    drwActClks    =  accessTime / IDE_SYSCLK_NS;
    drwActClks   += (accessTime % IDE_SYSCLK_NS) ? 1 : 0;
    drwActTime    = drwActClks * IDE_SYSCLK_NS;

    drwRecTime    = pioModes[pTiming->mode].minDataCycle - drwActTime;
    drwRecClks    = drwRecTime / IDE_SYSCLK_NS;
    drwRecClks   += (drwRecTime % IDE_SYSCLK_NS) ? 1 : 0;

    if ( drwRecClks >= 16 ) 
        drwRecClks = 1;
    else if ( drwRecClks <= 1 )
        drwRecClks = 16;

    ideTimingRegs[unit].drwtimRegPIO = ((drwActClks & 0x0f) << 4) | ((drwRecClks-1)  & 0x0f);
 
    return true;
}


/*
 *
 *
 */
bool  AppleATAUltra646::calculateDMATiming( UInt32 unit, ATATiming *pTiming )
{   
    UInt32		accessTime;
    UInt32		drwActClks, drwRecClks;
    UInt32		drwActTime, drwRecTime;

    ideTimingRegs[unit].udidetcrReg = 0;      

    accessTime    = pTiming->minDataAccess;

    drwActClks    =  accessTime / IDE_SYSCLK_NS;
    drwActClks   += (accessTime % IDE_SYSCLK_NS) ? 1 : 0;
    drwActTime    = drwActClks * IDE_SYSCLK_NS;

    drwRecTime    = pTiming->minDataCycle - drwActTime;
    drwRecClks    = drwRecTime / IDE_SYSCLK_NS;
    drwRecClks   += (drwRecTime % IDE_SYSCLK_NS) ? 1 : 0;

    if ( drwRecClks >= 16 ) 
        drwRecClks = 1;
    else if ( drwRecClks <= 1 )
        drwRecClks = 16;

    ideTimingRegs[unit].drwtimRegDMA = ((drwActClks & 0x0f) << 4) | ((drwRecClks-1)  & 0x0f);    

    return true;    
}

/*
 *
 *
 */
bool  AppleATAUltra646::calculateUltraDMATiming( UInt32 unit, ATATiming *pTiming )
{
    UInt32		cycleClks;
    UInt32		cycleTime;

    cycleTime = pTiming->minDataCycle;
        
    cycleClks  = cycleTime / IDE_SYSCLK_NS;
    cycleClks += (cycleTime % IDE_SYSCLK_NS) ? 1 : 0;

    ideTimingRegs[unit].udidetcrReg = (0x01 << unit) | ((cycleClks-1) << ((!unit) ? 4 : 6)) ;
   
    return true;  
}

/*
 *
 *
 */
void AppleATAUltra646::newDeviceSelected( IOATADevice *newDevice )
{    
}


/*
 *
 *
 */
bool AppleATAUltra646::selectTiming( UInt32 unit, ATATimingProtocol timingProtocol )
{
    Ultra646Regs	       	*cfgRegs;
    UInt32			cfgByte;

    cfgRegs = &ideTimingRegs[unit];

    if ( busNum == 0 )
    {
        pciWriteByte( Ultra646CMDTIM, cfgRegs->cmdtimReg );             

        if ( unit == 0 )
        { 
            pciWriteByte( Ultra646ARTTIM0, cfgRegs->arttimReg );

            if ( timingProtocol == ataTimingPIO )
            {
                cfgByte = pciReadByte( Ultra646CNTRL );
                cfgByte &= ~Ultra646CNTRL_Drive0ReadAhead;
                cfgByte |= cfgRegs->cntrlReg;
                pciWriteByte( Ultra646CNTRL, cfgByte );

                pciWriteByte( Ultra646DRWTIM0, cfgRegs->drwtimRegPIO );             
            }
            else if ( timingProtocol == ataTimingDMA )
            {
                pciWriteByte( Ultra646DRWTIM0, cfgRegs->drwtimRegDMA );             
            }
            else if ( timingProtocol == ataTimingUltraDMA33 )
            {
                cfgByte = pciReadByte( Ultra646UDIDETCR0 );
                cfgByte &= ~(Ultra646UDIDETCR0_Drive0UDMACycleTime | Ultra646UDIDETCR0_Drive0UDMAEnable);
                cfgByte |= cfgRegs->udidetcrReg;
                pciWriteByte( Ultra646UDIDETCR0, cfgByte );
            }
        }        
        else
        {
            pciWriteByte( Ultra646ARTTIM1, cfgRegs->arttimReg );

            if ( timingProtocol == ataTimingPIO )
            {
                cfgByte = pciReadByte( Ultra646CNTRL );
                cfgByte &= ~Ultra646CNTRL_Drive1ReadAhead;
                cfgByte |= cfgRegs->cntrlReg;
                pciWriteByte( Ultra646CNTRL, cfgByte );

                pciWriteByte( Ultra646DRWTIM1, cfgRegs->drwtimRegPIO );
            }
            else if ( timingProtocol == ataTimingDMA )
            {
                pciWriteByte( Ultra646DRWTIM1, cfgRegs->drwtimRegDMA );
            }
            else if ( timingProtocol == ataTimingUltraDMA33 )
            {
                cfgByte = pciReadByte( Ultra646UDIDETCR0 );
                cfgByte &= ~(Ultra646UDIDETCR0_Drive1UDMACycleTime | Ultra646UDIDETCR0_Drive1UDMAEnable);
                cfgByte |= cfgRegs->udidetcrReg;
                pciWriteByte(  Ultra646UDIDETCR0, cfgByte );
            }
       }
    }
    else
    {
        pciWriteByte( Ultra646CMDTIM, cfgRegs->cmdtimReg );

        if ( unit == 0 )
        {
            cfgByte = pciReadByte( Ultra646ARTTIM23 ); 
            cfgByte &= ~(Ultra646ARTTIM23_Drive2ReadAhead | Ultra646ARTTIM23_AddrSetup);
            cfgByte |= (cfgRegs->cntrlReg >> 4) | cfgRegs->arttimReg;
            pciWriteByte( Ultra646ARTTIM23, cfgByte );

            if ( timingProtocol == ataTimingPIO )
            {
                pciWriteByte( Ultra646DRWTIM2, cfgRegs->drwtimRegPIO );
            }
            else if ( timingProtocol == ataTimingDMA )
            {
                pciWriteByte( Ultra646DRWTIM1, cfgRegs->drwtimRegDMA );
            }
            else if ( timingProtocol == ataTimingUltraDMA33 )
            {
                cfgByte = pciReadByte( Ultra646UDIDETCR1 );    
                cfgByte &= ~(Ultra646UDIDETCR1_Drive2UDMACycleTime | Ultra646UDIDETCR1_Drive2UDMAEnable);
                cfgByte |= cfgRegs->udidetcrReg;
                pciWriteByte(  Ultra646UDIDETCR1, cfgByte );
            }
        }        
        else
        {
            cfgByte = pciReadByte( Ultra646ARTTIM23 );
            cfgByte &= ~(Ultra646ARTTIM23_Drive3ReadAhead | Ultra646ARTTIM23_AddrSetup);
            cfgByte |= (cfgRegs->cntrlReg >> 4) | cfgRegs->arttimReg;
            pciWriteByte( Ultra646ARTTIM23, cfgByte ); 

            if ( timingProtocol == ataTimingPIO )
            {
                pciWriteByte( Ultra646DRWTIM3, cfgRegs->drwtimRegPIO );
            }
            else if ( timingProtocol == ataTimingDMA )
            {
                pciWriteByte( Ultra646DRWTIM3, cfgRegs->drwtimRegDMA );
            }
            else if ( timingProtocol == ataTimingUltraDMA33 )
            {
                cfgByte = pciReadByte( Ultra646UDIDETCR1 );
                cfgByte &= ~(Ultra646UDIDETCR1_Drive3UDMACycleTime | Ultra646UDIDETCR1_Drive3UDMAEnable);
                cfgByte |= cfgRegs->udidetcrReg;
                pciWriteByte( Ultra646UDIDETCR1, cfgByte );
            }
        }
    }

    return true;
}


/*
 *
 *
 */
void AppleATAUltra646::interruptOccurred()
{
    UInt32			intReg;
    UInt32			cfgReg;

    super::interruptOccurred();

    intReg = (busNum == 0) ? Ultra646CFR : Ultra646ARTTIM23;
    cfgReg = pciReadByte( intReg );
    pciWriteByte( intReg, cfgReg );
}

/*
 *
 *
 */
void AppleATAUltra646::ataTimer( IOTimerEventSource * /* sender */ )
{
    UInt32		transferCount;

    if ( xferCmdTimer != 0 )
    {
        if ( --xferCmdTimer == 0 )
        {
            IOLog("AppleATAUltra646::%s() - Timeout occurred\n\r", __FUNCTION__ );
           
            stopDma( xferCmd, &transferCount ); 

            resetBusRequest();

            if ( xferCmdSave != NULL )
            {
                xferCmd     = xferCmdSave;
                xferCmdSave = NULL;
            }
            completeCmd( xferCmd, ataReturnErrorInterruptTimeout );
        }
    }

    timerEventSource->setTimeoutMS(ATATimerIntervalmS);
}

/*
 *
 *
 */
bool AppleATAUltra646::programDma( IOATACommand *cmd )
{
    IOMemoryDescriptor			*memoryDesc;
    IOPhysicalSegment			physSeg;
    IOByteCount				offset;
    UInt32				i;
    UInt32				bytesLeft;
    UInt32				len;
    Ultra646Descriptor			*dmaDesc;
    UInt32                              startSeg, endSeg;

    cmd->getPointers( &memoryDesc, &dmaReqLength, &dmaIsWrite );

    if ( dmaReqLength == 0 )
    {
        return true;
    }

    offset = 0;

    dmaDesc = dmaDescriptors;

    bytesLeft = dmaReqLength;

    for (i = 0; i < numDescriptors-1; i++, dmaDesc++ )
    {
        if ( dmaMemoryCursor->getPhysicalSegments( memoryDesc, offset, &physSeg, 1 ) != 1 )
        {
            break;
        }
        
        startSeg = (physSeg.location & ~0xffff);
        endSeg   = (physSeg.location + physSeg.length - 1) & ~0xffff;

        OSWriteSwapInt32( &dmaDesc->start, 0, physSeg.location);

        if ( startSeg == endSeg )
        {
            OSWriteSwapInt32( &dmaDesc->length, 0, physSeg.length );
        }
        else
        {
            len = (-physSeg.location & 0xffff);
            OSWriteSwapInt32( &dmaDesc->length, 0, len );
            dmaDesc++;
            i++;
            OSWriteSwapInt32( &dmaDesc->start,  0, physSeg.location + len );
            OSWriteSwapInt32( &dmaDesc->length, 0, physSeg.length   - len ); 
        }

        bytesLeft -= physSeg.length;
        offset += physSeg.length;
    }

    if ( bytesLeft != 0 )
    {
        return false;
    }

    /*
     * Note: ATAPI always transfers even byte-counts. Send the extra byte to/from the bit-bucket
     *       if the requested transfer length is odd.
     */
    if ( dmaReqLength & 1 )
    {
        if ( i == numDescriptors ) return false;

        dmaDesc++;
        OSWriteSwapInt32( &dmaDesc->start,  0, bitBucketAddrPhys );
        OSWriteSwapInt32( &dmaDesc->length, 0, 1 );
    }


    dmaDesc--;
    dmaDesc->length |= 0x80;

    pciWriteLong( ((busNum == 0) ? 0x74 : 0x7C), dmaDescriptorsPhys );

    return true;
}                      

 
/*
 *
 *
 */
bool AppleATAUltra646::startDma( IOATACommand * )
{
    UInt32		  reg;
    UInt32                cfgReg;

    if ( dmaReqLength != 0 )
    {
        reg = (busNum == 0) ? 0x70 : 0x78;
        cfgReg = pciReadLong( reg );
        cfgReg &= ~0x08;
        cfgReg |= 0x01 | ((dmaIsWrite == false) ? 0x08 : 0x00);
        pciWriteLong( reg, cfgReg );
    }
    return true;
}


/*
 *
 *
 */
bool AppleATAUltra646::stopDma( IOATACommand *, UInt32 *transferCount )
{
    UInt32			reg;
    UInt32                      cfgReg;

    *transferCount = 0;

    if ( dmaReqLength == 0 )
    {
        return true;
    }

    reg = (busNum == 0) ? 0x70 : 0x78;
    cfgReg = pciReadLong( reg );
    cfgReg &= ~0x01;
    pciWriteLong( reg, cfgReg );

    *transferCount = dmaReqLength;

    return true;
}
 

/*
 *
 *
 */
void AppleATAUltra646::disableControllerInterrupts()
{
    interruptEventSource->disable();
}

/*
 *
 *
 */
void AppleATAUltra646::enableControllerInterrupts()
{
    interruptEventSource->enable();
}

/*
 *
 *
 */
void AppleATAUltra646::free()
{
    UInt32		i;

    if ( interruptEventSource != 0 )
    {
        interruptEventSource->disable();
        interruptEventSource->release();
    }
  
    if ( timerEventSource != 0 )
    {
        timerEventSource->release();
    }

    for (i = 0; i < 2; i++ )
    {
        if ( ioMapATA[i] != 0 ) ioMapATA[i]->release();
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
void AppleATAUltra646::writeATAReg( UInt32 regIndex, UInt32 regValue )
{
    if ( regIndex == 0 )
    {
        *(volatile UInt16 *)ioBaseATA[0] = regValue;
    }
    else if ( regIndex < ataRegDeviceControl )
    {
        *((volatile UInt8 *)ioBaseATA[0] + regIndex) = regValue;
    }     
    else
    {
        *((volatile UInt8 *)ioBaseATA[1] + regIndex - ataRegDeviceControl + 2) = regValue;
    }
    eieio();
}

UInt32 AppleATAUltra646::readATAReg( UInt32 regIndex )
{
    if ( regIndex == 0 )
    { 
        return *(volatile UInt16 *)ioBaseATA[0];
    }
    else if ( regIndex < ataRegDeviceControl )
    {
        return *((volatile UInt8 *)ioBaseATA[0] + regIndex);
    }

    return *((volatile UInt8 *)ioBaseATA[1] + regIndex - ataRegDeviceControl + 2);
}

/*
 *
 *
 */
UInt32 AppleATAUltra646::pciReadByte( UInt32 reg )
{
    volatile union
    {
        unsigned long	word;
        unsigned char   byte[4];
    } data;

    data.word = provider->configRead32( reg );
    return data.byte[3 - (reg & 0x03)];
}

void AppleATAUltra646::pciWriteByte( UInt32 reg, UInt32 value )
{
    volatile union
    {
        unsigned long	word;
        unsigned char   byte[4];
    } data;

    UInt32		regWord;
 
    regWord = reg & ~0x03;

    data.word = provider->configRead32( regWord );
    data.word = OSReadSwapInt32( &data.word, 0 );

    switch (regWord)
    {
        case Ultra646CFR:
            data.byte[Ultra646CFR & 0x03] &= ~Ultra646CFR_IDEIntPRI;
            break;
        case Ultra646DRWTIM0:
            data.byte[Ultra646ARTTIM23 & 0x03] &= ~Ultra646ARTTIM23_IDEIntSDY;
            break;
        case Ultra646BMIDECR0:
            data.byte[Ultra646MRDMODE & 0x03 ] &= ~(Ultra646MRDMODE_IDEIntPRI  | Ultra646MRDMODE_IDEIntSDY);
            data.byte[Ultra646BMIDESR0 & 0x03] &= ~(Ultra646BMIDESR0_DMAIntPRI | Ultra646BMIDESR0_DMAErrorPRI);
            break;
        case Ultra646BMIDECR1:
            data.byte[Ultra646BMIDESR1 & 0x03] &= ~(Ultra646BMIDESR1_DMAIntSDY | Ultra646BMIDESR1_DMAErrorSDY);
            break;
    }        
    data.byte[reg & 0x03] = value;

    data.word = OSReadSwapInt32(&data.word, 0); 

    provider->configWrite32( regWord, data.word );
}

UInt32 AppleATAUltra646::pciReadLong( UInt32 reg )
{
    return provider->configRead32( reg );
}

void AppleATAUltra646::pciWriteLong( UInt32 reg, UInt32 value )
{
    provider->configWrite32( reg, value );
}

/* These overrides take care of OpenFirmware referring to the controller
 * as a child of the PCI device, "ata-4" */

bool AppleATAUltra646::getPath(	char * path, int * length,
				const IORegistryPlane * plane ) const
{
    if (pathProvider && (plane == gIODTPlane))
        return pathProvider->getPath(path, length, plane);
    else
	return super::getPath(path, length, plane);
}

IOService * AppleATAUltra646::matchLocation( IOService * client )
{
    if (pathProvider)
	return pathProvider;
    else
	return super::matchLocation(client);
}
