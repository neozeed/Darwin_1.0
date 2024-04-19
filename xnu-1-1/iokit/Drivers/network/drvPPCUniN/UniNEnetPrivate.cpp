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
 * Copyright (c) 1998-1999 Apple Computer
 *
 * Implementation for hardware dependent (relatively) code 
 * for the Sun GEM Ethernet controller. 
 *
 * HISTORY
 *
 * 10-Sept-97        
 *  Created.
 *
 */
#include "UniNEnetPrivate.h"

extern void             *kernel_pmap;

/*
 * Private functions
 */
bool UniNEnet::allocateMemory()
{
    UInt32              rxRingSize, txRingSize;
    UInt32              i, n;
    UInt8               *virtAddr;
    UInt32              physBase;
    UInt32              physAddr;
    TxQueueElement      *txElement;
 
    /* 
     * Calculate total space for DMA channel commands
     */
    txRingSize = (TX_RING_LENGTH * sizeof(enet_txdma_cmd_t) + 2048 - 1) & ~(2048-1);
    rxRingSize = (RX_RING_LENGTH * sizeof(enet_dma_cmd_t)   + 2048 - 1) & ~(2048-1);
     
    dmaCommandsSize = round_page( txRingSize + rxRingSize ); 
    /*
     * Allocate required memory
     */
    if ( !dmaCommands )
    {
      dmaCommands = (UInt8 *)IOMallocContiguous( dmaCommandsSize, PAGE_SIZE, 0 );

      if ( dmaCommands == 0  )
      {
          IOLog( "Ethernet(UniN): Cant allocate channel dma commands\n\r" );
          return false;
      }
    }

    /*
     * If we needed more than one page, then make sure we received contiguous memory.
     */
    n = (dmaCommandsSize - PAGE_SIZE) / PAGE_SIZE;
    physBase = pmap_extract(kernel_pmap, (vm_address_t) dmaCommands);

    virtAddr = (UInt8 *) dmaCommands;
    for( i=0; i < n; i++, virtAddr += PAGE_SIZE )
    {
        physAddr =  pmap_extract(kernel_pmap, (vm_address_t) virtAddr);      
        if (physAddr != (physBase + i * PAGE_SIZE) )
        {
            IOLog( "Ethernet(UniN): Cant allocate contiguous memory for dma commands\n\r" );
            return false;
        }
    }           

    /* 
     * Setup the receive ring pointers
     */
    rxDMACommands = (enet_dma_cmd_t *)dmaCommands;
    rxMaxCommand  = RX_RING_LENGTH;

    /*
     * Setup the transmit ring pointers
     */
    txDMACommands = (enet_txdma_cmd_t *)(dmaCommands + rxRingSize);
    txMaxCommand  = TX_RING_LENGTH;
    
    
    queue_init( &txActiveQueue );
    queue_init( &txFreeQueue );
    
    for ( i=0; i < TX_MAX_MBUFS; i++ )
    {
        txElement = (TxQueueElement *)IOMalloc( sizeof(TxQueueElement) );        
        if ( txElement == 0 )
        {
            return false;
        }
            
        bzero( txElement, sizeof(TxQueueElement) );
         
        releaseTxElement( txElement );
    }     
 
    return true;
}

/*-------------------------------------------------------------------------
 *
 * Setup the Transmit Ring
 * -----------------------
 * Each transmit ring entry consists of two words to transmit data from buffer
 * segments (possibly) spanning a page boundary. This is followed by two DMA commands 
 * which read transmit frame status and interrupt status from the UniN chip. The last
 * DMA command in each transmit ring entry generates a host interrupt.
 * The last entry in the ring is followed by a DMA branch to the first
 * entry.
 *-------------------------------------------------------------------------*/

bool UniNEnet::initTxRing()
{
    TxQueueElement      *txElement;
    UInt32		i;

    /*
     * Clear the transmit DMA command memory
     */  
    bzero( (void *)txDMACommands, sizeof(enet_txdma_cmd_t) * txMaxCommand);
    txCommandHead = 0;
    txCommandTail = 0;
    
    
    txDMACommandsPhys = pmap_extract(kernel_pmap, (vm_address_t) txDMACommands);

    if ( txDMACommandsPhys == 0 )
    {
        IOLog( "Ethernet(UniN): Bad dma command buf - %08x\n\r", (int)txDMACommands );
    }
 
    for ( i=0; i < TX_RING_LENGTH; i++ )
    {  
        txElement = txElementPtrs[i];

        if ( (txElement != 0) && (txElement->list != &txFreeQueue) )
        {
            if ( txElement->mbuf != 0 )
            {
                freePacket( txElement->mbuf );
            }
            bzero( txElement, sizeof(TxQueueElement) );
            releaseTxElement( txElement );
        }

        txElementPtrs[i] = 0;
    }

    txCommandsAvail = txMaxCommand - 1; 

    return true;
}

/*-------------------------------------------------------------------------
 *
 * Setup the Receive ring
 * ----------------------
 * Each receive ring entry consists of two DMA commands to receive data
 * into a network buffer (possibly) spanning a page boundary. The second
 * DMA command in each entry generates a host interrupt.
 * The last entry in the ring is followed by a DMA branch to the first
 * entry. 
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::initRxRing()
{
    UInt32      i;
    bool        status;
    
    /*
     * Clear the receive DMA command memory
     */
    bzero( (void *)rxDMACommands, sizeof(enet_dma_cmd_t) * rxMaxCommand);

    rxDMACommandsPhys = pmap_extract(kernel_pmap, (vm_address_t) rxDMACommands);
    if ( rxDMACommandsPhys == 0 )
    {
        IOLog( "Ethernet(UniN): Bad dma command buf - %08x\n\r",  (int)rxDMACommands );
        return false;
    }

    /*
     * Allocate a receive buffer for each entry in the Receive ring
     */
    for (i = 0; i < rxMaxCommand; i++) 
    {
        if (rxMbuf[i] == NULL)    
        {
            rxMbuf[i] = allocatePacket(NETWORK_BUFSIZE);
            if (rxMbuf[i] == NULL)    
        {
                IOLog("Ethernet(UniN): allocateMbuf returned NULL in _initRxRing\n\r");
                return false;
        }
      }
      
      /* 
       * Set the DMA commands for the ring entry to transfer data to the Mbuf.
       */
        status = updateDescriptorFromMbuf(rxMbuf[i], &rxDMACommands[i], true);
        if (status == false)
        {    
            IOLog("Ethernet(UniN): cant map mbuf to physical memory in initRxRing\n\r");
            return false;
        }
    }

    /*
     * Set the receive queue head to point to the first entry in the ring. Set the
     * receive queue tail to point to a DMA Stop command after the last ring entry
     */    
    i-=4;
    rxCommandHead = 0;
    rxCommandTail = i;

    return true;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::startChip()
{
    UInt32  gemReg;
  
//    dumpRegisters();

    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMTxConfig );
    gemReg |= kGEMTxConfig_TxDMAEnable;
    WriteUniNRegister( ioBaseEnet, kGEMTxConfig, gemReg );

    IOSleep(20);

    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMRxConfig );
    gemReg |= kGEMRxConfig_RxDMAEnable;
    WriteUniNRegister( ioBaseEnet, kGEMRxConfig, gemReg  );

    IOSleep(20);

    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMMacTxMacConfig );
    gemReg |= kGEMMacTxMacConfig_TxMacEnable;    
    WriteUniNRegister( ioBaseEnet, kGEMMacTxMacConfig, gemReg  );

    IOSleep(20);

    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMMacRxMacConfig );
    gemReg |= kGEMMacRxMacConfig_RxMacEnable;    
    WriteUniNRegister( ioBaseEnet, kGEMMacRxMacConfig, gemReg  );

//  dumpRegisters();

}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::stopChip()
{
    UInt32	gemReg;
  
    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMTxConfig );
    gemReg &= ~kGEMTxConfig_TxDMAEnable;
    WriteUniNRegister( ioBaseEnet, kGEMTxConfig, gemReg );

    IOSleep(20);

    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMRxConfig );
    gemReg &= ~kGEMRxConfig_RxDMAEnable;
    WriteUniNRegister( ioBaseEnet, kGEMRxConfig, gemReg  );

    IOSleep(20);

    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMMacTxMacConfig );
    gemReg &= ~kGEMMacTxMacConfig_TxMacEnable;    
    WriteUniNRegister( ioBaseEnet, kGEMMacTxMacConfig, gemReg  );

    IOSleep(20);

    gemReg  = ReadUniNRegister( ioBaseEnet, kGEMMacRxMacConfig );
    gemReg &= ~kGEMMacRxMacConfig_RxMacEnable;    
    WriteUniNRegister( ioBaseEnet, kGEMMacRxMacConfig, gemReg  );
}



/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::resetChip()
{
    UInt8       resetReg;
    UInt16      *pPhyType;
    UInt16      phyWord;


    WriteUniNRegister( ioBaseEnet, kGEMSoftwareReset, kGEMSoftwareReset_TxReset | kGEMSoftwareReset_RxReset );

    do
    {
        resetReg = ReadUniNRegister( ioBaseEnet, kGEMSoftwareReset );
    } 
    while( resetReg & (kGEMSoftwareReset_TxReset | kGEMSoftwareReset_RxReset) );

    /*
     * Determine if PHY chip is configured. Reset and enable it (if present).
     */
    if ( phyId == 0xff )
    {
        if ( miiFindPHY(&phyId) == true )
        {
            miiResetPHY( phyId );

            pPhyType = (UInt16 *)&phyType;
            miiReadWord( pPhyType,   MII_ID0, phyId );
            miiReadWord( pPhyType+1, MII_ID1, phyId );

            if ( (phyType & MII_BCM5400_MASK) == MII_BCM5400_ID )
            {
                miiReadWord( &phyWord, MII_BCM5400_AUXCONTROL, phyId );
                phyWord |= MII_BCM5400_AUXCONTROL_PWR10BASET;
                miiWriteWord( phyWord, MII_BCM5400_AUXCONTROL, phyId );
              
                miiReadWord( &phyWord, MII_BCM5400_1000BASETCONTROL, phyId );
                phyWord |= MII_BCM5400_1000BASETCONTROL_FULLDUPLEXCAP;
                miiWriteWord( phyWord, MII_BCM5400_1000BASETCONTROL, phyId );

                IODelay(100);   
                            
                miiResetPHY( 0x1F );

                miiReadWord( &phyWord, MII_BCM5201_MULTIPHY, 0x1F );
                phyWord |= MII_BCM5201_MULTIPHY_SERIALMODE;
                miiWriteWord( phyWord, MII_BCM5201_MULTIPHY, 0x1F );

                miiReadWord( &phyWord, MII_BCM5400_AUXCONTROL, phyId );
                phyWord &= ~MII_BCM5400_AUXCONTROL_PWR10BASET;
                miiWriteWord( phyWord, MII_BCM5400_AUXCONTROL, phyId );

            }              
        }
    }
 
    return true;
}    
    
/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::initChip()
{
    UInt32          	i, j;
    mach_timespec_t	timeStamp;
    UInt32          	rxFifoSize;
    UInt32          	rxOff;
    UInt32          	rxOn;
    u_int16_t       	*p16;

    if ( phyId == 0xff )
    {
        WriteUniNRegister( ioBaseEnet, kGEMPCSDatapathMode, kGEMPCSDatapathMode_ExtSERDESMode );

        WriteUniNRegister( ioBaseEnet, kGEMPCSSerdesControl, kGEMPCSSerdesControl_DisableLoopback | 
                                                             kGEMPCSSerdesControl_EnableSyncDet     );

        WriteUniNRegister( ioBaseEnet, kGEMPCSAdvert, kGEMPCSAdvert_FullDuplexCap |
                                                      kGEMPCSAdvert_SymPauseCap     );    

        WriteUniNRegister( ioBaseEnet, kGEMPCSControl, kGEMPCSControl_EnableAutoNegot |
                                                       kGEMPCSControl_RestartAutoNegot    );    

        WriteUniNRegister( ioBaseEnet, kGEMPCSConfig, kGEMPCSConfig_Enable );  

        WriteUniNRegister( ioBaseEnet, kGEMMacXIFConfig, kGEMMacXIFConfig_TxMIIOutputEnable 	|
                                                         kGEMMacXIFConfig_GMIIMode		|
                                                         kGEMMacXIFConfig_FullDuplexLed );
    }
    else
    {
        WriteUniNRegister( ioBaseEnet, kGEMPCSDatapathMode, kGEMPCSDatapathMode_GMIIMode );

        WriteUniNRegister( ioBaseEnet, kGEMMacXIFConfig, kGEMMacXIFConfig_TxMIIOutputEnable 	|
                                                         kGEMMacXIFConfig_FullDuplexLed );
   }

    WriteUniNRegister( ioBaseEnet, kGEMMacSendPauseCmd, kGEMMacSendPauseCmd_Bit * kGEMMacSendPauseCmd_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacControlConfig, kGEMMacControlConfig_ReceivePauseEnable );

    WriteUniNRegister( ioBaseEnet, kGEMInterruptMask,  0xffffffff );

    WriteUniNRegister( ioBaseEnet, kGEMMacTxMask, kGEMMacTxMaskDefault );

    WriteUniNRegister( ioBaseEnet, kGEMMacRxMask, kGEMMacRxMaskDefault );  

    WriteUniNRegister( ioBaseEnet, kGEMMacControlMask, kGEMMacControlMaskDefault );

    WriteUniNRegister( ioBaseEnet, kGEMConfig,  31*kGEMConfig_TxDMALimitBit | 
						31*kGEMConfig_RxDMALimitBit | 
      						kGEMConfig_InfiniteBurst);

    WriteUniNRegister( ioBaseEnet, kGEMMacInterPktGap0, kGEMMacInterPktGap0_Default );
    WriteUniNRegister( ioBaseEnet, kGEMMacInterPktGap1, kGEMMacInterPktGap1_Default );
    WriteUniNRegister( ioBaseEnet, kGEMMacInterPktGap2, kGEMMacInterPktGap2_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacSlotTime, kGEMMacSlotTime_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacMinFrameSize, kGEMMacMinFrameSize_Default );
    WriteUniNRegister( ioBaseEnet, kGEMMacMaxFrameSize, kGEMMacMaxFrameSize_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacPASize, kGEMMacPASize_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacJamSize, kGEMMacJamSize_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacAttemptLimit, kGEMMacAttemptLimit_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacTypeControl, kGEMMacTypeControl_Default );

    /*
     *
     */
    p16 = (u_int16_t *) myAddress.ea_byte;
    for ( i=0; i < sizeof(enet_addr_t) / 2; i++ )
    {
        WriteUniNRegister( ioBaseEnet, kGEMMacAddr + i * 4, p16[2-i] );
    }

    for ( i=0; i < 3; i ++ )
    {
        WriteUniNRegister( ioBaseEnet, kGEMMacAddr       + (i+3) * 4, 0 );
        WriteUniNRegister( ioBaseEnet, kGEMMacAddrFilter + (i+0) * 4, 0 );
    }

    WriteUniNRegister( ioBaseEnet, kGEMMacAddr + (6*4), kGEMMacAddr6_Default );
    WriteUniNRegister( ioBaseEnet, kGEMMacAddr + (7*4), kGEMMacAddr7_Default );
    WriteUniNRegister( ioBaseEnet, kGEMMacAddr + (8*4), kGEMMacAddr8_Default );

    WriteUniNRegister( ioBaseEnet, kGEMMacAddrFilterMask1, 0 );
    WriteUniNRegister( ioBaseEnet, kGEMMacAddrFilterMask0, 0 );

    for ( i=0; i < 16; i++ )
    {
        WriteUniNRegister( ioBaseEnet, kGEMMacHashTable + i * 4, 0 );
    }

    /*
     *
     */
    for ( i = kGEMMacCollisions; i <= kGEMMacRxCodeErrors; i+=4 );
    {
        WriteUniNRegister( ioBaseEnet, i, 0 );
    }

    IOGetTime(&timeStamp); 
    WriteUniNRegister( ioBaseEnet, kGEMMacRandomSeed, (u_int16_t) timeStamp.tv_nsec );      

    /*
     *
     */
    WriteUniNRegister( ioBaseEnet, kGEMTxDescBaseLo, txDMACommandsPhys );
    WriteUniNRegister( ioBaseEnet, kGEMTxDescBaseHi, 0 );

    for ( i=0, j=TX_RING_LENGTH / kGEMTxConfig_TxRingSizeMin - 1; 
         (i < 13) && j; 
         i++, j >>= 1 )
            ;

    WriteUniNRegister( ioBaseEnet, kGEMTxConfig, (i                                   * kGEMTxConfig_TxDescRingSizeBit) | 
                                                 (kGEMTxConfig_TxFIFOThresholdDefault * kGEMTxConfig_TxFIFOThresholdBit) );

    WriteUniNRegister( ioBaseEnet, kGEMMacTxMacConfig, 0 );

    setDuplexMode( (phyId == 0xff) ? true : false );
   
    /*
     *
     */
    WriteUniNRegister( ioBaseEnet, kGEMRxDescBaseLo, rxDMACommandsPhys );
    WriteUniNRegister( ioBaseEnet, kGEMRxDescBaseHi, 0 );

    WriteUniNRegister( ioBaseEnet, kGEMRxKick, RX_RING_LENGTH-4 );

    for ( i=0, j=RX_RING_LENGTH / kGEMRxConfig_RxRingSizeMin - 1;
          (i < 13) && j; 
          i++, j >>= 1 )
             ;

    WriteUniNRegister( ioBaseEnet, kGEMRxConfig, (i                                  * kGEMRxConfig_RxDescRingSizeBit) | 
                                                 (kGEMRxConfig_RxDMAThresholdDefault * kGEMRxConfig_RxDMAThresholdBit) 
                                                        );

    WriteUniNRegister( ioBaseEnet, kGEMMacRxMacConfig,  0 );

    rxFifoSize  = ReadUniNRegister( ioBaseEnet, kGEMRxFIFOSize );

    rxOff  = rxFifoSize - ((kGEMMacMaxFrameSize_Aligned + 8) * 2 / kGEMRxPauseThresholds_Units);
    rxOn   = rxFifoSize - ((kGEMMacMaxFrameSize_Aligned + 8) * 3 / kGEMRxPauseThresholds_Units);

    WriteUniNRegister( ioBaseEnet, kGEMRxPauseThresholds, (rxOff * kGEMRxPauseThresholds_OffBit) | 
                                                          (rxOn  * kGEMRxPauseThresholds_OnBit) );

    i = (ReadUniNRegister(ioBaseEnet, kGEMBIFConfig) & kGEMBIFConfig_66Mhz) ? PCI_PERIOD_66MHz : PCI_PERIOD_33MHz;
    i = (RX_INT_LATENCY_uS * 1000) / (2048 * i);
    
    WriteUniNRegister( ioBaseEnet, kGEMRxIntBlanking, i*kGEMRxIntBlanking_TimeBit | 5*kGEMRxIntBlanking_NumPktBit );

    return true;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::disableAdapterInterrupts()
{

    WriteUniNRegister( ioBaseEnet, kGEMInterruptMask, 0xffffffff );
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::enableAdapterInterrupts()
{
    UInt32             gemReg;

    gemReg = ReadUniNRegister( ioBaseEnet, kGEMInterruptMask );
    gemReg &= ~(kGEMInterruptMask_TxInt | kGEMInterruptMask_RxDone |  kGEMInterruptStatus_RxMacInt);
    WriteUniNRegister( ioBaseEnet, kGEMInterruptMask, gemReg );
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::setDuplexMode( bool duplexMode )
{
    UInt16      txMacConfig;
    UInt16      xifConfig;

    isFullDuplex = duplexMode;

    txMacConfig = ReadUniNRegister( ioBaseEnet, kGEMMacTxMacConfig );

    WriteUniNRegister( ioBaseEnet, kGEMMacTxMacConfig, txMacConfig & ~kGEMMacTxMacConfig_TxMacEnable );
    while( ReadUniNRegister(ioBaseEnet, kGEMMacTxMacConfig) & kGEMMacTxMacConfig_TxMacEnable )
      ;

    xifConfig = ReadUniNRegister( ioBaseEnet, kGEMMacXIFConfig );

    if ( isFullDuplex )
    {
        txMacConfig |= (kGEMMacTxMacConfig_IgnoreCollsn | kGEMMacTxMacConfig_IgnoreCarrierSense);
        xifConfig   &= ~kGEMMacXIFConfig_DisableEcho;
    }
    else
    {
        txMacConfig &= ~(kGEMMacTxMacConfig_IgnoreCollsn | kGEMMacTxMacConfig_IgnoreCarrierSense);
        xifConfig   |= kGEMMacXIFConfig_DisableEcho;
    }

    WriteUniNRegister( ioBaseEnet, kGEMMacTxMacConfig, txMacConfig );
    WriteUniNRegister( ioBaseEnet, kGEMMacXIFConfig,   xifConfig );
}    


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::restartTransmitter()
{
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::restartReceiver()
{
}

/*-------------------------------------------------------------------------
 *
 * Orderly stop of receive DMA.
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::stopReceiveDMA()
{
}    

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::stopTransmitDMA()
{
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::transmitPacket(struct mbuf *packet)
{
    UInt32              i,j,k;
    struct mbuf         *m;
    TxQueueElement      *txElement;
    UInt32              dataPhys;
    
    
    for ( m = packet, i=1; m->m_next; m=m->m_next, i++ )
      ;
      
    
    if ( i > txCommandsAvail )  
    {
        transmitInterruptOccurred();
        
        if ( i > txCommandsAvail )
        {
            return false;
        } 
    }    

    
    if ( (txElement=getTxElement()) == 0 )
    {
        transmitInterruptOccurred();
        
        if ( (txElement=getTxElement()) == 0 )
        {
            return false;
        } 
    }    
    
    j = txCommandTail;
        
    txElement->mbuf      = packet;
    txElement->slot      = j;
    txElement->count     = i;
    
    txCommandsAvail     -= i;    

    m = packet;

    do
    {        
        k = j; 
    
        txElementPtrs[j] = txElement;
        
        dataPhys = (UInt32)mcl_to_paddr( mtod(m, char *) );
        if ( dataPhys == 0 )
        {
            dataPhys = pmap_extract( kernel_pmap, mtod(m, vm_offset_t));
        }    
        
        txDMACommands[j].desc_seg[0].bufferAddrLo  = EndianSwap32(dataPhys);
        txDMACommands[j].desc_seg[0].flags0        = EndianSwap32(kGEMTxDescFlags0_BufferSizeBit * m->m_len);
               
        j++;   
        if ( j >= txMaxCommand ) j = 0;               
    }
    while ( (m=m->m_next) != 0 );

    txDMACommands[k].desc_seg[0].flags0             |= EndianSwap32(kGEMTxDescFlags0_EndOfFrame);
    txDMACommands[txCommandTail].desc_seg[0].flags0 |= EndianSwap32(kGEMTxDescFlags0_StartOfFrame);
     
    txCommandTail = j;
          
    WriteUniNRegister( ioBaseEnet, kGEMTxKick, j );
     
    return true;          
}          


/*-------------------------------------------------------------------------
 * _receivePacket
 * --------------
 * This routine runs the receiver in polled-mode (yuk!) for the kernel debugger.
 * Don't mess with the interrupt source here that can deadlock in the debugger
 *
 * The _receivePackets allocate MBufs and pass them up the stack. The kernel
 * debugger interface passes a buffer into us. To reconsile the two interfaces,
 * we allow the receive routine to continue to allocate its own buffers and
 * transfer any received data to the passed-in buffer. This is handled by 
 * _receivePacket calling _packetToDebugger.
 *-------------------------------------------------------------------------*/

void UniNEnet::receivePacket( void *pkt, unsigned int *pkt_len, unsigned int timeout )
{
    mach_timespec_t	startTime;
    mach_timespec_t	currentTime;
    UInt32          	elapsedTimeMS;

    *pkt_len = 0;

    if (ready == false)
      return;

    debuggerPkt     = pkt;
    debuggerPktSize = 0;

    IOGetTime(&startTime);
    do
    {
      receivePackets( true );
      IOGetTime( &currentTime );
      elapsedTimeMS = (currentTime.tv_nsec - startTime.tv_nsec) / (1000*1000);
    } 
    while ( (debuggerPktSize == 0) && (elapsedTimeMS < timeout) );

    *pkt_len = debuggerPktSize;

    return;
}

/*-------------------------------------------------------------------------
 * _packetToDebugger
 * -----------------
 * This is called by _receivePackets when we are polling for kernel debugger
 * packets. It copies the MBuf contents to the buffer passed by the debugger.
 * It also sets the var debuggerPktSize which will break the polling loop.
 *-------------------------------------------------------------------------*/

void UniNEnet::packetToDebugger( struct mbuf * packet, u_int size )
{
    debuggerPktSize = size;
    bcopy( mtod(packet, char *), debuggerPkt, size );
}

/*-------------------------------------------------------------------------
 * _sendPacket
 * -----------
 *
 * This routine runs the transmitter in polled-mode (yuk!) for the kernel debugger.
 * Don't mess with the interrupt source here that can deadlock in the debugger
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::sendPacket( void *pkt, unsigned int pkt_len )
{
    mach_timespec_t	startTime;
    mach_timespec_t	currentTime;
    UInt32		elapsedTimeMS;

    if (!ready || !pkt || (pkt_len > ETHERMAXPACKET))
    {
        return;
    }

    /*
     * Wait for the transmit ring to empty
     */
    IOGetTime(&startTime); 
    do
    {   
      debugTransmitInterruptOccurred();
      IOGetTime(&currentTime);
      elapsedTimeMS = (currentTime.tv_nsec - startTime.tv_nsec) / (1000*1000);
    }
    while ( (txCommandHead != txCommandTail) && (elapsedTimeMS < TX_KDB_TIMEOUT) ); 
    
    if ( txCommandHead != txCommandTail )
    {
      IOLog( "Ethernet(UniN): Polled tranmit timeout - 1\n\r");
      return;
    }

    /*
     * Allocate a MBuf and copy the debugger transmit data into it.
     *
     * jliu - no allocation, just recycle the same buffer dedicated to
     * KDB transmit.
     */
    txDebuggerPkt->m_next = 0;
    txDebuggerPkt->m_data = (caddr_t) pkt;
    txDebuggerPkt->m_pkthdr.len = txDebuggerPkt->m_len = pkt_len;

    /*
     * Send the debugger packet. txDebuggerPkt must not be freed by
     * the transmit routine.
     */
    transmitPacket(txDebuggerPkt);

    /*
     * Poll waiting for the transmit ring to empty again
     */
    do 
    {
        debugTransmitInterruptOccurred();
        IOGetTime(&currentTime);
        elapsedTimeMS = (currentTime.tv_nsec - startTime.tv_nsec) / (1000*1000);
    }
    while ( (txCommandHead != txCommandTail) &&
            (elapsedTimeMS < TX_KDB_TIMEOUT) ); 

    if ( txCommandHead != txCommandTail )
    {
        IOLog( "Ethernet(UniN): Polled tranmit timeout - 2\n\r");
    }

    return;
}

/*-------------------------------------------------------------------------
 * _sendDummyPacket
 * ----------------
 * The UniN receiver seems to be locked until we send our first packet.
 *
 *-------------------------------------------------------------------------*/
void UniNEnet::sendDummyPacket()
{
    union
    {
        UInt8           bytes[64];
        enet_addr_t     enet_addr[2];
    } dummyPacket;

    bzero( &dummyPacket, sizeof(dummyPacket) );


    dummyPacket.enet_addr[0] = myAddress;   
    dummyPacket.enet_addr[1] = myAddress;

    sendPacket((void *)dummyPacket.bytes, (unsigned int)sizeof(dummyPacket));
}



/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::receiveInterruptOccurred()
{
    return receivePackets(false);
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::receivePackets( bool fDebugger )
{
    struct mbuf     *packet;
    UInt32          i,last;
    int             receivedFrameSize = 0;
    UInt16          dmaFlags;
    UInt32          rxPktStatus = 0;
    UInt32          badFrameCount;
    bool            passPacketUp;
    bool            reusePkt;
    bool            status;
    bool	    useNetif = !fDebugger && netifClient;
    bool            packetsQueued = false;

   
    last      = (UInt32)-1;  
    i         = rxCommandHead;

    while ( 1 )
    {
        passPacketUp = false;
        reusePkt     = false;

        dmaFlags = EndianSwap16(rxDMACommands[i].desc_seg[0].frameDataSize);

        /* 
         * If the current entry has not been written, then stop at this entry
         */
        if ( dmaFlags & kGEMRxDescFrameSize_Own )
        {
            break;
        }


        receivedFrameSize = dmaFlags & kGEMRxDescFrameSize_Mask;
        rxPktStatus       = EndianSwap32(rxDMACommands[i].desc_seg[0].flags);


        /*
         * Reject packets that are runts or that have other mutations.
         */
        if ( receivedFrameSize < (ETHERMINPACKET - ETHERCRC) || 
                     receivedFrameSize > (ETHERMAXPACKET + ETHERCRC) ||
                         rxPktStatus & kGEMRxDescFlags_BadCRC )
        {
            if (useNetif) netStats->inputErrors++;
            reusePkt = true;
        }
        else if ( useNetif == false )
        {
            /*
             * Always reuse packets in debugger mode. We also refuse to
             * pass anything up the stack unless the driver is open. The
             * hardware is enabled before the stack has opened us, to
             * allow earlier debug interface registration. But we must
             * not pass any packets up.
             */
            reusePkt = true;
            if (fDebugger)
            {
                packetToDebugger(rxMbuf[i], receivedFrameSize);
            }
        }
        
 
        /*
         * Before we pass this packet up the networking stack. Make sure we
         * can get a replacement. Otherwise, hold on to the current packet and
         * increment the input error count.
         * Thanks Justin!
         */

        packet = 0;

        if ( reusePkt == false )
        {
            bool replaced;
        
            packet = replaceOrCopyPacket(&rxMbuf[i], receivedFrameSize, &replaced);

            reusePkt = true;

            if (packet && replaced)
            {
                status = updateDescriptorFromMbuf(rxMbuf[i], &rxDMACommands[i], true);

                if (status)
                {
                    reusePkt = false;
                }
                else
                {
                    // Assume descriptor has not been corrupted.
                    freePacket(rxMbuf[i]);  // release new packet.
                    rxMbuf[i] = packet;     // get the old packet back.
                    packet = 0;             // pass up nothing.
                    IOLog("Ethernet(UniN): updateDescriptorFromMbuf error\n");
                }
            }
            
            if (packet == 0)
                netStats->inputErrors++;
        }

        /*
         * Install the new MBuf for the one we're about to pass to the network stack
         */

        if ( reusePkt == true )
        {
            rxDMACommands[i].desc_seg[0].frameDataSize = EndianSwap16(NETWORK_BUFSIZE | kGEMRxDescFrameSize_Own);
            rxDMACommands[i].desc_seg[0].flags         = 0;
        }

        /*
         * Keep track of the last receive descriptor processed
         */            
        last = i;

        /*
         * Implement ring wrap-around
         */
        if (++i >= rxMaxCommand) i = 0;

        if (fDebugger)
        {
            break;
        }

        /*
         * Transfer received packet to network
         */
        if (packet)
        {
            KERNEL_DEBUG(DBG_UniN_RXCOMPLETE | DBG_FUNC_NONE, (int) packet, 
                (int)receivedFrameSize, 0, 0, 0 );

            networkInterface->inputPacket(packet, receivedFrameSize, true);
            netStats->inputPackets++;
			packetsQueued = true;
        }
    }

    /*
     *
     *
     */
#if 0
    IOLog( "Ethernet(UniN): Prev - Rx Head = %2d Rx Tail = %2d Rx Last = %2d\n\r", rxCommandHead, rxCommandTail, last );
#endif

    if ( last != (UInt32)-1 )
    {
        rxCommandTail = last;
        rxCommandHead = i;
    }

    /*
     * Tap the DMA to wake it up
     */
    WriteUniNRegister(ioBaseEnet, kGEMRxKick, rxCommandTail & ~(4-1));


    /*
     * Update receive error statistics
     */

    badFrameCount =    ReadUniNRegister(ioBaseEnet, kGEMMacRxLengthErrors)
                     + ReadUniNRegister(ioBaseEnet, kGEMMacRxFCSErrors);

    /*
     * Clear Hardware counters
     */
    WriteUniNRegister(ioBaseEnet, kGEMMacRxLengthErrors,   0);
    WriteUniNRegister(ioBaseEnet, kGEMMacRxFCSErrors,      0);
    
    if (useNetif) netStats->inputErrors += badFrameCount;


    return packetsQueued;
}
 
/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::transmitInterruptOccurred()
{
    UInt32          	i;
    bool        	fServiced = false;
    UInt32		txHeadOrig, txCompOrig;
    TxQueueElement      *txElement;

    i = ReadUniNRegister( ioBaseEnet, kGEMTxCompletion );

    txHeadOrig = txCommandHead;
    txCompOrig = i;
    
    while ( i != txCommandHead )
    {
        /*
         * Free the MBuf we just transmitted
         */
        txElement = txElementPtrs[txCommandHead];         
         
        KERNEL_DEBUG(DBG_UniN_TXCOMPLETE | DBG_FUNC_NONE, (int) txElement->mbuf, 0, 0, 0, 0 );

        txElementPtrs[txCommandHead] = 0;
        txCommandsAvail++;

        if ( --txElement->count == 0 )
        {
            freePacket( txElement->mbuf );
            releaseTxElement( txElement );   
            if (netifClient) netStats->outputPackets++;     
        }    
        
        if ( ++txCommandHead >= txMaxCommand ) txCommandHead = 0;

        i = ReadUniNRegister( ioBaseEnet, kGEMTxCompletion );

        fServiced = true;
    }

    return fServiced;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::debugTransmitInterruptOccurred()
{
    bool        	fServiced = false;
    UInt32		i;
    TxQueueElement      *txElement;

    // Set the debugTxPoll flag to indicate the debugger was active
    // and some cleanup may be needed when the driver returns to
    // normal operation.
    //
    debugTxPoll = true;

    i = ReadUniNRegister( ioBaseEnet, kGEMTxCompletion );

    while ( i != txCommandHead )
    {
        fServiced = true;

        /*
         * Free the mbuf we just transmitted.
         *
         * If it is the debugger packet, just remove it from the ring.
         * and reuse the same packet for the next sendPacket() request.
         */
         
        /*
         * While in debugger mode, do not touch the mbuf pool.
         * Queue any used mbufs to a local queue. This queue
         * will get flushed after we exit from debugger mode.
         *
         * During continuous debugger transmission and
         * interrupt polling, we expect only the txDebuggerPkt
         * to show up on the transmit mbuf ring.
         */
        txElement = txElementPtrs[txCommandHead];
        txElementPtrs[txCommandHead] = 0;
        txCommandsAvail++;
        
        KERNEL_DEBUG(DBG_UniN_TXCOMPLETE | DBG_FUNC_NONE,
            (int)txElement->mbuf,
            (int)txElement->mbuf->m_pkthdr.len, 0, 0, 0 );

        if ( --txElement->count == 0 )
        {
            if (txElement->mbuf != txDebuggerPkt) 
            {
                debugQueue->enqueue( txElement->mbuf );
            }    
            releaseTxElement( txElement );            
        }                 

        if ( ++(txCommandHead) >= txMaxCommand )
            txCommandHead = 0;
    }

    return fServiced;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::debugTransmitCleanup()
{
    // Debugger was active, clear all packets in the debugQueue, and
    // issue a start(), just in case the debugger became active while the
    // ring was full and the output queue stopped. Since the debugger
    // does not restart the output queue, to avoid calling
    // semaphore_signal() which may reenable interrupts, we need to
    // make sure the output queue is not stalled after the debugger has
    // flushed the ring.
    
    debugQueue->flush();

    transmitQueue->start();
}


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

static UInt32 txCnt = 1;

bool UniNEnet::updateDescriptorFromMbuf(struct mbuf * m,  enet_dma_cmd_t *desc, bool isReceive)
{
    struct IOPhysicalSegment    	segVector[1];
    UInt32			segments;

    segments = mbufCursor->getPhysicalSegmentsWithCoalesce(m, segVector);
    
    if ( segments == 0 || segments > 1 )
    {
        IOLog("Ethernet(UniN): updateDescriptorFromMbuf error, %d segments\n", (int)segments);
        return false;
    }    
    
    if ( isReceive )
    {
        enet_dma_cmd_t      *rxCmd = (enet_dma_cmd_t *)desc;

        rxCmd->desc_seg[0].bufferAddrLo   = EndianSwap32(segVector[0].location);
        rxCmd->desc_seg[0].frameDataSize  = EndianSwap16(segVector[0].length | kGEMRxDescFrameSize_Own); 
        rxCmd->desc_seg[0].flags          = 0; 
    }
    else
    {
        enet_txdma_cmd_t    *txCmd = (enet_txdma_cmd_t *)desc;

        txCmd->desc_seg[0].bufferAddrLo  = EndianSwap32(segVector[0].location);
        txCmd->desc_seg[0].flags0        = EndianSwap32( kGEMTxDescFlags0_BufferSizeBit * segVector[0].length   |
                                                         kGEMTxDescFlags0_StartOfFrame      |
                                     kGEMTxDescFlags0_EndOfFrame );
            
        txCmd->desc_seg[0].flags1        = ((txCnt++ % 64) == 0) ? EndianSwap32(kGEMTxDescFlags1_Int) : 0;
    }                                          

    return true;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

TxQueueElement *UniNEnet::getTxElement()
{
    TxQueueElement              *txElement = 0;

    if ( queue_empty( &txFreeQueue ) == false )
    {
        queue_remove_first( &txFreeQueue, txElement, TxQueueElement *, next );
        
        txElement->list = &txActiveQueue;
        
        queue_enter( txElement->list, txElement, TxQueueElement *, next );
    }
    
    return txElement;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::releaseTxElement(TxQueueElement *txElement)
{
    if ( txElement->list != 0 )
    {
        queue_remove( txElement->list, txElement, TxQueueElement *, next );
    }
    
    txElement->list = &txFreeQueue;   

    queue_enter(  txElement->list, txElement, TxQueueElement *, next);
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::monitorLinkStatus()
{
    UInt16          phyStatus;
    UInt16          linkStatus;
    UInt16          linkMode;
    UInt16          lpAbility;
    UInt16          gemReg;
    UInt16          phyStatusChange;
    bool            fullDuplex = false;
    UInt8           *linkSpeedTxt;

    if ( phyId == 0xff )
    {
        phyStatus = ReadUniNRegister( ioBaseEnet, kGEMPCSStatus );
        lpAbility =  ReadUniNRegister( ioBaseEnet, kGEMPCSLPAbility );
    }
    else 
    {
        if ( miiReadWord( &phyStatus, MII_STATUS, phyId) != true )
        {
            return;
        }
       miiReadWord( &lpAbility, MII_STATUS, phyId);
    }

    phyStatusChange = (phyStatusPrev ^ phyStatus) & (MII_STATUS_LINK_STATUS | MII_STATUS_NEGOTIATION_COMPLETE);
    if ( phyStatusChange )
    {
        gemReg = ReadUniNRegister( ioBaseEnet, kGEMMacControlConfig );
        if ( lpAbility & MII_LPAR_PAUSE )
        {
            gemReg |= kGEMMacControlConfig_SendPauseEnable;
        }
        else
        {
            gemReg &= ~kGEMMacControlConfig_SendPauseEnable;
        }
        WriteUniNRegister( ioBaseEnet, kGEMMacControlConfig, gemReg );
    
        if ( (phyStatus & MII_STATUS_LINK_STATUS) && (phyStatus & MII_STATUS_NEGOTIATION_COMPLETE ) )
        {
            if ( phyId == 0xff )
            {
                IOLog( "Ethernet(UniN): Link is up at 1Gb - Full Duplex\n\r" );
                fullDuplex = true;
            }
            else if ( (phyType & MII_BCM5201_MASK) == MII_BCM5201_ID )
            {
                miiReadWord( &linkStatus, MII_BCM5201_AUXSTATUS, phyId );

                fullDuplex =  (linkStatus & MII_BCM5201_AUXSTATUS_DUPLEX) ? true : false;

                IOLog( "Ethernet(UniN): Link is up at %sMb - %s Duplex\n\r",
                       (linkStatus & MII_BCM5201_AUXSTATUS_SPEED)  ? "100" : "10",
                       (fullDuplex)                                ? "Full" : "Half" );                        

            }  
            else if ( (phyType & MII_BCM5400_MASK) == MII_BCM5400_ID )
            {
                miiReadWord( &linkStatus, MII_BCM5400_AUXSTATUS, phyId );

                linkMode = (linkStatus & MII_BCM5400_AUXSTATUS_LINKMODE_MASK) / MII_BCM5400_AUXSTATUS_LINKMODE_BIT;

                gemReg = ReadUniNRegister( ioBaseEnet, kGEMMacXIFConfig );
                if ( linkMode < 6 )
                {
                    gemReg &= ~kGEMMacXIFConfig_GMIIMode;
                }
                else
                {
                    gemReg |= kGEMMacXIFConfig_GMIIMode;
                }
                WriteUniNRegister( ioBaseEnet, kGEMMacXIFConfig, gemReg );

                if ( linkMode == 0 )
                {
                    linkSpeedTxt = NULL;
                }
                else if ( linkMode < 3 )
                {
                    linkSpeedTxt = (UInt8 *)"10Mb";
                    fullDuplex =  ( linkMode < 2 ) ? false : true;                    

                }
                else if ( linkMode < 6 )
                {
                    linkSpeedTxt = (UInt8 *)"100Mb";
                    fullDuplex =  ( linkMode < 5 ) ? false : true;                                        
                }
                else
                {
                    linkSpeedTxt = (UInt8 *)"1Gb";
                    fullDuplex = true;
                }                    

                if ( linkSpeedTxt )
                {
                    IOLog( "Ethernet(UniN): Link is up at %s - %s Duplex\n\r",
                            linkSpeedTxt,
                           (fullDuplex) ? "Full" : "Half" );                        
                }
                else
                {
                    IOLog( "Ethernet(UniN): Link is up\n\r" );
                }

            }

            if ( fullDuplex != isFullDuplex )
            {
                setDuplexMode( fullDuplex );    
            }

            if ( ready == true )
            {
                startChip();
            }

            linkStatusPrev = true;
        }
        else
        {
            if ( linkStatusPrev == true )
            {
                stopChip();
                IOLog( "Ethernet(UniN): Link is down.\n\r" );
            }
            linkStatusPrev = false;
        }
        phyStatusPrev = phyStatus;
    }
}


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::dumpRegisters()
{
    UInt32      i;

    struct _regtable
    {
        UInt32      reg;
        char        *text;
    } 
    regtable[] =
    {
        { kGEMSEBState                  ,"SEBState"         },
        { kGEMConfig                    ,"Config"       },
        { kGEMInterruptStatus           ,"InterruptStatus"  },
        { kGEMInterruptMask             ,"InterruptMask"    },
        { kGEMInterruptAck              ,"InterruptAck"     },
        { kGEMInterruptAltStatus        ,"InterruptAltStatus"   },
        { kGEMPCIErrorStatus            ,"PCIErrorStatus"   },
        { kGEMPCIErrorMask              ,"PCIErrorMask"     },
        { kGEMBIFConfig                 ,"BIFConfig"        },
        { kGEMBIFDiagnostic             ,"BIFDiagnostic"    },
        { kGEMSoftwareReset             ,"SoftwareReset"    },
        { kGEMTxKick                    ,"TxKick"       },
        { kGEMTxConfig                  ,"TxConfig"         },
        { kGEMTxDescBaseLo              ,"TxDescBaseLo"     },
        { kGEMTxDescBaseHi              ,"TxDescBaseHi"     },
        { kGEMTxFIFOWritePtr            ,"TxFIFOWritePtr"   },
        { kGEMTxFIFOShadowWritePtr      ,"TxFIFOShadowWritePtr" },
        { kGEMTxFIFOReadPtr             ,"TxFIFOReadPtr"    },
        { kGEMTxFIFOShadowReadPtr       ,"TxFIFOShadowReadPtr"  },
        { kGEMTxFIFOPktCounter          ,"TxFIFOPktCounter"     },
        { kGEMTxStateMachine            ,"TxStateMachine"   },
        { kGEMTxDataPtrLo               ,"TxDataPtrLo"      },
        { kGEMTxDataPtrHi               ,"TxDataPtrHi"      },
        { kGEMTxCompletion              ,"TxCompletion"     },
        { kGEMTxFIFOAddr                ,"TxFIFOAddr"       },
        { kGEMTxFIFOTag                 ,"TxFIFOTag"        },
        { kGEMTxFIFODataLo              ,"TxFIFODataLo"     },
        { kGEMTxFIFODataHiTag1          ,"TxFIFODataHiTag1"     },
        { kGEMTxFIFODataHiTag0          ,"TxFIFODataHiTag0"     },
        { kGEMTxFIFOSize                ,"TxFIFOSize"       },
        { kGEMRxConfig                  ,"RxConfig"         },
        { kGEMRxDescBaseLo              ,"RxDescBaseLo"     },
        { kGEMRxDescBaseHi              ,"RxDescBaseHi"     },
        { kGEMRxFIFOWritePtr            ,"RxFIFOWritePtr"   },
        { kGEMRxFIFOShadowWritePtr      ,"RxFIFOShadowWritePtr" },
        { kGEMRxFIFOReadPtr             ,"RxFIFOReadPtr"    },
        { kGEMRxFIFOPktCounter          ,"RxFIFOPktCounter"     },
        { kGEMRxStateMachine            ,"RxStateMachine"   },
        { kGEMRxPauseThresholds         ,"RxPauseThresholds"    },
        { kGEMRxDataPtrLo               ,"RxDataPtrLo"      },
        { kGEMRxDataPtrHi               ,"RxDataPtrHi"      },
        { kGEMRxKick                    ,"RxKick"       },
        { kGEMRxCompletion              ,"RxCompletion"     },
        { kGEMRxIntBlanking             ,"RxIntBlanking"    },
        { kGEMRxFIFOAddr                ,"RxFIFOAddr"       },
        { kGEMRxFIFOTag                 ,"RxFIFOTag"        },
        { kGEMRxDataLo                  ,"RxDataLo"         },
        { kGEMRxDataHiTag0              ,"RxDataHiTag0"     },
        { kGEMRxDataHiTag1              ,"RxDataHiTag1"     },
        { kGEMRxFIFOSize                ,"RxFIFOSize"       },
        { kGEMMacTxResetCmd             ,"MacTxResetCmd"    },
        { kGEMMacRxResetCmd             ,"MacRxResetCmd"    },
        { kGEMMacSendPauseCmd           ,"MacSendPauseCmd"  },
        { kGEMMacTxStatus               ,"MacTxStatus"      },
        { kGEMMacRxStatus               ,"MacRxStatus"      },
        { kGEMMacControlStatus          ,"MacControlStatus"     },
        { kGEMMacTxMask                 ,"MacTxMask"        },
        { kGEMMacRxMask                 ,"MacRxMask"        },
        { kGEMMacControlMask            ,"MacControlMask"   },
        { kGEMMacTxMacConfig            ,"MacTxMacConfig"   },
        { kGEMMacRxMacConfig            ,"MacRxMacConfig"   },
        { kGEMMacControlConfig          ,"MacControlConfig"     },
        { kGEMMacXIFConfig              ,"MacXIFConfig"     },
        { kGEMMacInterPktGap0           ,"MacInterPktGap0"  },
        { kGEMMacInterPktGap1           ,"MacInterPktGap1"  },
        { kGEMMacInterPktGap2           ,"MacInterPktGap2"  },
        { kGEMMacSlotTime               ,"MacSlotTime"      },
        { kGEMMacMinFrameSize           ,"MacMinFrameSize"  },
        { kGEMMacMaxFrameSize           ,"MacMaxFrameSize"  },
        { kGEMMacPASize                 ,"MacPASize"        },
        { kGEMMacJamSize                ,"MacJamSize"       },
        { kGEMMacAttemptLimit           ,"MacAttemptLimit"  },
        { kGEMMacTypeControl            ,"MacTypeControl"   },
        { kGEMMacAddr                   ,"MacAddr"      },
        { kGEMMacAddrFilter             ,"MacAddrFilter"    },
        { kGEMMacAddrFilterMask1        ,"MacAddrFilterMask1"   },
        { kGEMMacAddrFilterMask0        ,"MacAddrFilterMask0"   },
        { kGEMMacHashTable              ,"MacHashTable"     },
        { kGEMMacCollisions             ,"MacCollisions"    },
        { kGEMMacSingleCollision        ,"MacSingleCollision"   },
        { kGEMMacExcessCollisions       ,"MacExcessCollisions"  },
        { kGEMMacLateCollisions         ,"MacLateCollisions"    },
        { kGEMMacDeferTimer             ,"MacDeferTimer"    },
        { kGEMMacPeakAttempts           ,"MacPeakAttempts"  },
        { kGEMMacRxFrameCounter         ,"MacRxFrameCounter"    },
        { kGEMMacRxLengthErrors         ,"MacRxLengthErrors"    },
        { kGEMMacRxAlignErrors          ,"MacRxAlignErrors"     },
        { kGEMMacRxFCSErrors            ,"MacRxFCSErrors"   },
        { kGEMMacRxCodeErrors           ,"MacRxCodeErrors"  },
        { kGEMMacRandomSeed             ,"MacRandomSeed"    },
        { kGEMMacStateMachine           ,"MacStateMachine"  },
        { kGEMMIFCLock                  ,"MIFCLock"         },
        { kGEMMIFData                   ,"MIFData"      },
        { kGEMMIFOutputEnable           ,"MIFOutputEnable"  },
        { kGEMMIFFrame                  ,"MIFFrame"         },
        { kGEMMIFConfig                 ,"MIFConfig"        },
        { kGEMMIFMask                   ,"MIFMask"      },  
        { kGEMMIFStatus                 ,"MIFStatus"        },
        { kGEMMIFStateMachine           ,"MIFStateMachine"  },
        { kGEMPCSControl                ,"PCSControl"       },
        { kGEMPCSStatus                 ,"PCSStatus"        },
        { kGEMPCSAdvert                 ,"PCSAdvert"        },
        { kGEMPCSLPAbility              ,"PCSLPAbility"     },
        { kGEMPCSConfig                 ,"PCSConfig"        },
        { kGEMPCSStateMachine           ,"PCSStateMachine"  },
        { kGEMPCSInterruptStatus        ,"PCSInterruptStatus"   },
        { kGEMPCSDatapathMode           ,"PCSDatapathMode"  },
        { kGEMPCSSerdesControl          ,"PCSSerdesControl"     },
        { kGEMPCSSerdesOutputSelect     ,"PCSSerdesOutputSelect"},
        { kGEMPCSSerdesState            ,"PCSSerdesState"   }
    };

    IOLog("\nEthernet(UniN): IO Address = %08x\n\r", (int)ioBaseEnet );

    for ( i=0; i < sizeof(regtable)/sizeof(regtable[0]); i++ )
    {
        switch ( regtable[i].reg >> 16 )
        {
            case 1:
                IOLog( "Ethernet(UniN): %04x: %s = %02x\n\r", 
                  (int)regtable[i].reg & 0xffff,
                       regtable[i].text,
                       ReadUniNRegister( ioBaseEnet, regtable[i].reg ) );
                break;
            case 2:
                IOLog( "Ethernet(UniN): %04x: %s = %04x\n\r", 
                  (int)regtable[i].reg & 0xffff,
                       regtable[i].text,
                       ReadUniNRegister( ioBaseEnet, regtable[i].reg ) );
                break;
            case 4:
                IOLog( "Ethernet(UniN): %04x: %s = %08x\n\r", 
                  (int)regtable[i].reg & 0xffff,
                       regtable[i].text,
                       ReadUniNRegister( ioBaseEnet, regtable[i].reg ) );
                break;
        }
    }
}


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::getHardwareAddress(enet_addr_t *ea)
{
    UInt32      i;
    OSData	*macEntry;
    UInt8       *macAddress;
    UInt32      len;

    macEntry    = OSDynamicCast( OSData, nub->getProperty( "local-mac-address" ) );
    if ( macEntry == 0 )
    {
        return kIOReturnError;
    }

    macAddress  = (UInt8 *)macEntry->getBytesNoCopy();
    if ( macAddress == 0 )
    {
        return kIOReturnError;
    }

    len = macEntry->getLength();
    if ( len != 6 )
    {
        return kIOReturnError;
    }
   
    for (i = 0; i < sizeof(*ea); i++)   
    {
        ea->ea_byte[i] = macAddress[i];
    }
    return kIOReturnSuccess;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

#define ENET_CRCPOLY 0x04c11db7

static UInt32 crc416(UInt32 current, UInt16 nxtval )
{
    register UInt32 counter;
    register int highCRCBitSet, lowDataBitSet;

    /* Swap bytes */
    nxtval = ((nxtval & 0x00FF) << 8) | (nxtval >> 8);

    /* Compute bit-by-bit */
    for (counter = 0; counter != 16; ++counter)
    {   /* is high CRC bit set? */
      if ((current & 0x80000000) == 0)  
        highCRCBitSet = 0;
      else
        highCRCBitSet = 1;
        
      current = current << 1;
    
      if ((nxtval & 0x0001) == 0)
        lowDataBitSet = 0;
      else
    lowDataBitSet = 1;

      nxtval = nxtval >> 1;
    
      /* do the XOR */
      if (highCRCBitSet ^ lowDataBitSet)
        current = current ^ ENET_CRCPOLY;
    }
    return current;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

static UInt32 mace_crc(UInt16 *address)
{   
    register UInt32 newcrc;

    newcrc = crc416(0xffffffff, *address);  /* address bits 47 - 32 */
    newcrc = crc416(newcrc, address[1]);    /* address bits 31 - 16 */
    newcrc = crc416(newcrc, address[2]);    /* address bits 15 - 0  */

    return(newcrc);
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

/*
 * Add requested mcast addr to UniN's hash table filter.  
 *  
 */
void UniNEnet::addToHashTableMask(UInt8 *addr)
{   
    UInt32   i,j;
    UInt32   crcBitIndex;
    UInt16   mask;

    j = mace_crc((UInt16 *)addr) & 0xFF; /* Big-endian alert! */
   
    for ( crcBitIndex = i = 0; i < 8; i++ )
    {
        crcBitIndex >>= 1;
        crcBitIndex  |= (j & 0x80);
        j           <<= 1;
    }
     
    crcBitIndex ^= 0xFF;
            
    if (hashTableUseCount[crcBitIndex]++)   
      return;           /* This bit is already set */
    mask = crcBitIndex % 16;
    mask = 1 << mask;
    hashTableMask[crcBitIndex/16] |= mask;
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::resetHashTableMask()
{
    bzero(hashTableUseCount, sizeof(hashTableUseCount));
    bzero(hashTableMask, sizeof(hashTableMask));
}

/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

/*
 * Sync the adapter with the software copy of the multicast mask
 *  (logical address filter).
 */
void UniNEnet::updateHashTableMask()
{
    UInt32      i;
    UInt16      rxCFGReg;

    rxCFGReg = ReadUniNRegister(ioBaseEnet, kGEMMacRxMacConfig);
    WriteUniNRegister(ioBaseEnet, kGEMMacRxMacConfig, rxCFGReg & ~(kGEMMacRxMacConfig_RxMacEnable | 
                                                                  kGEMMacRxMacConfig_HashFilterEnable) );

    while ( ReadUniNRegister(ioBaseEnet, kGEMMacRxMacConfig) & (kGEMMacRxMacConfig_RxMacEnable | 
                                                               kGEMMacRxMacConfig_HashFilterEnable) )
      ;
    for (i=0; i<16; i++ )
    {
        WriteUniNRegister( ioBaseEnet, kGEMMacHashTable + i*4, hashTableMask[15-i] );
    }

    rxCFGReg |= kGEMMacRxMacConfig_HashFilterEnable;
    WriteUniNRegister(ioBaseEnet, kGEMMacRxMacConfig, rxCFGReg );
}


