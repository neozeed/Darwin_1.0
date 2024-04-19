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
 * Interface definition for the Sun GEM (UniN) Ethernet controller.
 *
 *
 */

/*
 *      Miscellaneous defines...
 */
#define CACHE_LINE_SIZE         32                                              /* Bytes */

#define RX_RING_LENGTH          128                                             /* Packet descriptors */

#define TX_RING_LENGTH          128                                             /* Packet descriptors */
#define TX_MAX_MBUFS            (TX_RING_LENGTH / 2)

#define NETWORK_BUFSIZE         (((ETHERMAXPACKET + ETHERCRC) + 7) & ~7)

#define TRANSMIT_QUEUE_SIZE     256

#define WATCHDOG_TIMER_MS       300
#define TX_KDB_TIMEOUT          1000

#define PCI_PERIOD_33MHz        30
#define PCI_PERIOD_66MHz        15
#define RX_INT_LATENCY_uS       250             

/*
 *      Register lengths 
 */
#define kGEMLength_1                            0x00010000
#define kGEMLength_2                            0x00020000
#define kGEMLength_4                            0x00040000


/*
 *      Receive/Transmit descriptor
 *
 */
typedef struct _GEMRxDescriptor
{
    u_int16_t           tcpPseudoChecksum;
    u_int16_t           frameDataSize;
    u_int32_t           flags;
    u_int32_t           bufferAddrLo;
    u_int32_t           bufferAddrHi;
} GEMRxDescriptor;

/*
 *    Note: Own is in the high bit of frameDataSize field
 */
#define kGEMRxDescFrameSize_Mask                0x7FFF
#define kGEMRxDescFrameSize_Own                 0x8000

/*
 * Rx flags field
 */
#define kGEMRxDescFlags_HashValueBit            0x00001000
#define kGEMRxDescFlags_HashValueMask           0x0FFFF000
#define kGEMRxDescFlags_HashPass                0x10000000
#define kGEMRxDescFlags_AlternateAddr           0x20000000
#define kGEMRxDescFlags_BadCRC                  0x40000000


typedef struct _GEMTxDescriptor
{
    u_int32_t           flags0;
    u_int32_t           flags1;
    u_int32_t           bufferAddrLo;
    u_int32_t           bufferAddrHi;
} GEMTxDescriptor;

/*
 * 
 */
#define kGEMTxDescFlags0_BufferSizeMask         0x00007FFF
#define kGEMTxDescFlags0_BufferSizeBit          0x00000001
#define kGEMTxDescFlags0_ChecksumStartMask      0x00FF8000
#define kGEMTxDescFlags0_ChecksumStartBit       0x00008000
#define kGEMTxDescFlags0_ChecksumStuffMask      0x1F000000
#define kGEMTxDescFlags0_ChecksupStuffBit       0x01000000
#define kGEMTxDescFlags0_ChecksumEnable         0x20000000
#define kGEMTxDescFlags0_EndOfFrame             0x40000000
#define kGEMTxDescFlags0_StartOfFrame           0x80000000

#define kGEMTxDescFlags1_Int                    0x00000001
#define kGEMTxDescFlags1_NoCRC                  0x00000002

/*
 *      Global Resources
 */
#define kGEMSEBState                            (0x0000 | kGEMLength_1)
#define kGEMSEBState_ArbStateMask               0x03
#define kGEMSEBState_RxWon                      0x04


#define kGEMConfig                              (0x0004 | kGEMLength_2)
#define kGEMConfig_InfiniteBurst                0x0001
#define kGEMConfig_TxDMALimitMask               0x003E
#define kGEMConfig_TxDMALimitBit                0x0002
#define kGEMConfig_RxDMALimitMask               0x07C0
#define kGEMConfig_RxDMALimitBit                0x0040

#define kGEMBurstSize                           (CACHE_LINE_SIZE / 8)           

#define kGEMInterruptStatus                     (0x000C | kGEMLength_4)
#define kGEMInterruptStatus_TxInt               0x00000001
#define kGEMInterruptStatus_TxAll               0x00000002
#define kGEMInterruptStatus_TxDone              0x00000004
#define kGEMInterruptStatus_RxDone              0x00000010
#define kGEMInterruptStatus_RxEmpty             0x00000020
#define kGEMInterruptStatus_RxTagError          0x00000040
#define kGEMInterruptStatus_PCSInt              0x00002000
#define kGEMInterruptStatus_TxMacInt            0x00004000
#define kGEMInterruptStatus_RxMacInt            0x00008000
#define kGEMInterruptStatus_MacControlInt       0x00010000
#define kGEMInterruptStatus_MIFInt              0x00020000
#define kGEMInterruptStatus_PCIErrorInt         0x00040000
#define kGEMInterruptStatus_Mask                0x0007FFFF

#define kGEMInterruptMask                       (0x0010 | kGEMLength_4)
#define kGEMInterruptMask_TxInt                 0x00000001
#define kGEMInterruptMask_TxAll                 0x00000002
#define kGEMInterruptMask_TxDone                0x00000004
#define kGEMInterruptMask_RxDone                0x00000010
#define kGEMInterruptMask_RxEmpty               0x00000020
#define kGEMInterruptMask_RxTagError            0x00000040
#define kGEMInterruptMask_PCSInt                0x00002000
#define kGEMInterruptMask_TxMacInt              0x00004000
#define kGEMInterruptMask_RxMacInt              0x00008000
#define kGEMInterruptMask_MacControlInt         0x00010000
#define kGEMInterruptMask_MIFInt                0x00020000
#define kGEMInterruptMask_PCIErrorInt           0x00040000

#define kGEMInterruptAck                        (0x0014 | kGEMLength_4)
#define kGEMInterruptAck_TxInt                  0x00000001
#define kGEMInterruptAck_TxAll                  0x00000002
#define kGEMInterruptAck_TxDone                 0x00000004
#define kGEMInterruptAck_RxDone                 0x00000010
#define kGEMInterruptAck_RxEmpty                0x00000020
#define kGEMInterrupAck_RxTagError              0x00000040

#define kGEMInterruptAltStatus                  (0x001C | kGEMLength_4)
#define kGEMInterruptAltStatus_TxInt            0x00000001
#define kGEMInterruptAltStatus_TxAll            0x00000002
#define kGEMInterruptAltStatus_TxDone           0x00000004
#define kGEMInterruptAltStatus_RxDone           0x00000010
#define kGEMInterruptAltStatus_RxEmpty          0x00000020
#define kGEMInterruptAltStatus_RxTagError       0x00000040
#define kGEMInterruptAltStatus_PCSInt           0x00002000
#define kGEMInterruptAltStatus_TxMacInt         0x00004000
#define kGEMInterruptAltStatus_RxMacInt         0x00008000
#define kGEMInterruptAltStatus_MacControlInt    0x00010000
#define kGEMInterruptAltStatus_MIFInt           0x00020000
#define kGEMInterruptAltStatus_PCIErrorInt      0x00040000

#define kGEMPCIErrorStatus                      (0x1000 | kGEMLength_1)
#define kGEMPCIErrorStatus_BadAck64             0x01
#define kGEMPCIErrorStatus_TransactTimeout      0x02
#define kGEMPCIErrorStatus_Other                0x04            

#define kGEMPCIErrorMask                        (0x1004 | kGEMLength_1)
#define kGEMPCIErrorMask_BadAck64               0x01
#define kGEMPCIErrorMask_TransactTimeout        0x02
#define kGEMPCIErrorMask_Other                  0x04            

#define kGEMBIFConfig                           (0x1008 | kGEMLength_1)
#define kGEMBIFConfig_SlowClk                   0x01
#define kGEMBIFConfig_64BitDisable              0x04
#define kGEMBIFConfig_66Mhz                     0x08

#define kGEMBIFDiagnostic                       (0x100C | kGEMLength_4)
#define kGEMBIFDiagnostic_PCIBurstStateMask     0x007F0000
#define kGEMBIFDiagnostic_PCIBurstStateBit      0x00010000
#define kGEMBIFDiagnostic_BIFStateMachineMask   0xFF000000
#define kGEMBIFDiagnostic_BIFStateMachineBit    0x01000000

#define kGEMSoftwareReset                       (0x1010 | kGEMLength_1)
#define kGEMSoftwareReset_TxReset               0x01
#define kGEMSoftwareReset_RxReset               0x02
#define kGEMSoftwareReset_PhyReset              0x04

/*
 *      Transmit DMA Registers
 */
#define kGEMTxKick                              (0x2000 | kGEMLength_2)

#define kGEMTxConfig                            (0x2004 | kGEMLength_4)
#define kGEMTxConfig_TxDMAEnable                0x00000001
#define kGEMTxConfig_TxDescRingSizeMask         0x0000001E
#define kGEMTxConfig_TxDescRingSizeBit          0x00000002
#define kGEMTxConfig_TxRingSizeMin              32
#define kGEMTxConfig_TxFIFOPioSel               0x00000020
#define kGEMTxConfig_TxFIFOThresholdMask        0x000FFC00
#define kGEMTxConfig_TxFIFOThresholdBit         0x00000400
#define kGEMTxConfig_PacedMode                  0x00100000

#define kGEMTxConfig_TxFIFOThresholdDefault     0x7FF

#define kGEMTxDescBaseLo                        (0x2008 | kGEMLength_4)
#define kGEMTxDescBaseHi                        (0x200C | kGEMLength_4)

#define kGEMTxFIFOWritePtr                      (0x2014 | kGEMLength_2)
#define kGEMTxFIFOShadowWritePtr                (0x2018 | kGEMLength_2)

#define kGEMTxFIFOReadPtr                       (0x201C | kGEMLength_2)
#define kGEMTxFIFOShadowReadPtr                 (0x2020 | kGEMLength_2)

#define kGEMTxFIFOPktCounter                    (0x2024 | kGEMLength_2)

#define kGEMTxStateMachine                      (0x2028 | kGEMLength_4)
#define kGEMTxStateMachine_ChainStateMask       0x000003FF
#define kGEMTxStateMachine_ChainStateBit        0x00000001
#define kGEMTxStateMachine_ChecksumStateMask    0x00000C00
#define kGEMTxStateMachine_ChecksumStateBit     0x00000400
#define kGEMTxStateMachine_FIFOLoadStateMask    0x0001F000
#define kGEMTxStateMachine_FIFOLoadStateBit     0x00001000
#define kGEMTxStateMachine_FIFOUnloadStateMask  0x003C0000
#define kGEMTxStateMachine_FIFOUnloadStateBit   0x00040000

#define kGEMTxDataPtrLo                         (0x2030 | kGEMLength_4)
#define kGEMTxDataPtrHi                         (0x2034 | kGEMLength_4)

#define kGEMTxCompletion                        (0x2100 | kGEMLength_2)

#define kGEMTxFIFOAddr                          (0x2104 | kGEMLength_2)
#define kGEMTxFIFOTag                           (0x2108 | kGEMLength_1)
#define kGEMTxFIFODataLo                        (0x210C | kGEMLength_4)
#define kGEMTxFIFODataHiTag1                    (0x2110 | kGEMLength_4)
#define kGEMTxFIFODataHiTag0                    (0x2114 | kGEMLength_4)

#define kGEMTxFIFOSize                          (0x2118 | kGEMLength_2)
#define kGEMTxFIFOSize_Units                    64

/*
 *      Receive DMA Registers
 */
#define kGEMRxConfig                            (0x4000 | kGEMLength_4)
#define kGEMRxConfig_RxDMAEnable                0x00000001
#define kGEMRxConfig_RxDescRingSizeMask         0x0000001E              
#define kGEMRxConfig_RxDescRingSizeBit          0x00000002
#define kGEMRxConfig_RxRingSizeMin              32
#define kGEMRxConfig_BatchDisable               0x00000020
#define kGEMRxConfig_FirstByteOffsetMask        0x00001C00              
#define kGEMRxConfig_FirstByteOffsetBit         0x00000400
#define kGEMRxConfig_ChecksumStartOffsetMask    0x000FE000              
#define kGEMRxConfig_ChecksumStartOffsetBit     0x00002000
#define kGEMRxConfig_RxDMAThresholdMask         0x07000000
#define kGEMRxConfig_RxDMAThresholdBit          0x01000000
#define kGEMRxConfig_RxDMAThresholdMin          64

#define kGEMRxConfig_RxDMAThresholdDefault      1               /* (128)  Log2(RxDMAThresh) - 6 */

#define kGEMRxDescBaseLo                        (0x4004 | kGEMLength_4)
#define kGEMRxDescBaseHi                        (0x4008 | kGEMLength_4)

#define kGEMRxFIFOWritePtr                      (0x400C | kGEMLength_2)
#define kGEMRxFIFOShadowWritePtr                (0x4010 | kGEMLength_2)
#define kGEMRxFIFOReadPtr                       (0x4014 | kGEMLength_2)
#define kGEMRxFIFOPktCounter                    (0x4018 | kGEMLength_2)

#define kGEMRxStateMachine                      (0x401C | kGEMLength_4)
#define kGEMRxStateMachine_UnloadDataStateMask  0x0000000F
#define kGEMRxStateMachine_UnloadDataStateBit   0x00000001
#define kGEMRxStateMachine_UnloadDescStateMask  0x000000F0
#define kGEMRxStateMachine_UnloadDescStateBit   0x00000010
#define kGEMRxStateMachine_FlowControlStateMask 0x00000F00
#define kGEMRxStateMachine_FlowControlStateBit  0x00000100
#define kGEMRxStateMachine_PackStateMask        0x00003C00
#define kGEMRxStateMachine_PackStateBit         0x00000400
#define kGEMRxStateMachine_LoadStateMask        0x0003C000
#define kGEMRxStateMachine_LoadStateBit         0x00004000


#define kGEMRxPauseThresholds                   (0x4020 | kGEMLength_4)
#define kGEMRxPauseThresholds_OffMask           0x000001FF
#define kGEMRxPauseThresholds_OffBit            0x00000001
#define kGEMRxPauseThresholds_OnMask            0x001FF000
#define kGEMRxPauseThresholds_OnBit             0x00001000

#define kGEMRxPauseThresholds_Units             64

#define kGEMRxDataPtrLo                         (0x4024 | kGEMLength_4)
#define kGEMRxDataPtrHi                         (0x4028 | kGEMLength_4)

#define kGEMRxKick                              (0x4100 | kGEMLength_2)

#define kGEMRxCompletion                        (0x4104 | kGEMLength_2)

#define kGEMRxIntBlanking                       (0x4108 | kGEMLength_4)
#define kGEMRxIntBlanking_NumPktMask            0x000001FF
#define kGEMRxIntBlanking_NumPktBit             0x00000001
#define kGEMRxIntBlanking_TimeMask              0x000FF000
#define kGEMRxIntBlanking_TimeBit               0x00001000
#define kGEMRxIntBlanking_Units                 2048

#define kGEMRxFIFOAddr                          (0x410C | kGEMLength_2)
#define kGEMRxFIFOTag                           (0x4110 | kGEMLength_1)
#define kGEMRxDataLo                            (0x4114 | kGEMLength_4)
#define kGEMRxDataHiTag0                        (0x4118 | kGEMLength_4)
#define kGEMRxDataHiTag1                        (0x411C | kGEMLength_4)

#define kGEMRxFIFOSize                          (0x4120 | kGEMLength_2)
#define kGEMRxFIFOSize_Units                    64


/*
 *      MAC Registers
 */
#define kGEMMacTxResetCmd                       (0x6000 | kGEMLength_1)
#define kGEMMacTxResetCmd_Reset                 0x01

#define kGEMMacRxResetCmd                       (0x6004 | kGEMLength_1)
#define kGEMMacRxResetCmd_Reset                 0x01

#define kGEMMacSendPauseCmd                     (0x6008 | kGEMLength_4)
#define kGEMMacSendPauseCmd_TimeMask            0x0000FFFF
#define kGEMMacSendPauseCmd_Bit                 0x00000001
#define kGEMMacSendPauseCmd_SendPause           0x00010000
#define kGEMMacSendPauseCmd_Default             0x1BF0

#define kGEMMacTxStatus                         (0x6010 | kGEMLength_2)
#define kGEMMacTxStatus_FrameSent               0x0001
#define kGEMMacTxStatus_TxUnderrun              0x0002
#define kGEMMacTxStatus_MaxPktError             0x0004
#define kGEMMacTxStatus_NormalCollsnOverflow    0x0008
#define kGEMMacTxStatus_ExcessCollsnOverflow    0x0010
#define kGEMMacTxStatus_LateCollsnOverflow      0x0020
#define kGEMMacTxStatus_FirstCollsnOverflow     0x0040
#define kGEMMacTxStatus_DeferTimerOverflow      0x0080
#define kGEMMacTxStatus_PeakAttemptsOverflow    0x0100

#define kGEMMacRxStatus                         (0x6014 | kGEMLength_2)
#define kGEMMacRxStatus_FrameReceived           0x0001
#define kGEMMacRxStatus_RxFIFOOverflow          0x0002
#define kGEMMacRxStatus_FrameCounterOverflow    0x0004
#define kGEMMacRxStatus_AlignErrorOverflow      0x0008
#define kGEMMacRxStatus_CRCErrorOverflow        0x0010
#define kGEMMacRxStatus_LengthErrorOverflow     0x0020
#define kGEMMacRxStatus_CodeErrorOverflow       0x0040

#define kGEMMacControlStatus                    (0x6018 | kGEMLength_4)
#define kGEMMacControlStatus_PauseReceived      0x00000001
#define kGEMMacControlStatus_PauseState         0x00000002
#define kGEMMacControlStatus_NotPausedState     0x00000004
#define kGEMMacControlStatus_PauseTimeMask      0xFFFF0000
#define kGEMMacControlStatus_PauseTimeBit       0x00010000

#define kGEMMacTxMask                           (0x6020 | kGEMLength_2)
#define kGEMMacTxMask_FrameSent                 0x0001
#define kGEMMacTxMask_TxUnderrun                0x0002
#define kGEMMacTxMask_MaxPktError               0x0004
#define kGEMMacTxMask_NormalCollsnOverflow      0x0008
#define kGEMMacTxMask_ExcessCollsnOverflow      0x0010
#define kGEMMacTxMask_LateCollsnOverflow        0x0020
#define kGEMMacTxMask_FirstCollsnOverflow       0x0040
#define kGEMMacTxMask_DeferTimerOverflow        0x0080
#define kGEMMacTxMask_PeakAttemptsOverflow      0x0100

#define kGEMMacTxMaskDefault                    0xFFFF

#define kGEMMacRxMask                           (0x6024 | kGEMLength_2)
#define kGEMMacRxMask_FrameReceived             0x0001
#define kGEMMacRxMask_RxFIFOOverflow            0x0002
#define kGEMMacRxMask_FrameCounterOverflow      0x0004
#define kGEMMacRxMask_AlignErrorOverflow        0x0008
#define kGEMMacRxMask_CRCErrorOverflow          0x0010
#define kGEMMacRxMask_LengthErrorOverflow       0x0020
#define kGEMMacRxMask_CodeErrorOverflow         0x0040

#define kGEMMacRxMaskDefault                    0xFFFF

#define kGEMMacControlMask                      (0x6028 | kGEMLength_1)
#define kGEMMacControlMask_PauseReceived        0x01
#define kGEMMacControlMask_PauseState           0x02
#define kGEMMacControlMask_NotPausedState       0x04

#define kGEMMacControlMaskDefault               0xFF


#define kGEMMacTxMacConfig                      (0x6030 | kGEMLength_2)
#define kGEMMacTxMacConfig_TxMacEnable          0x0001
#define kGEMMacTxMacConfig_IgnoreCarrierSense   0x0002
#define kGEMMacTxMacConfig_IgnoreCollsn         0x0004
#define kGEMMacTxMacConfig_EnableIPG0           0x0008
#define kGEMMacTxMacConfig_NeverGiveUp          0x0010
#define kGEMMacTxMacConfig_NeverGiveUpNoLimit   0x0020
#define kGEMMacTxMacConfig_NoBackoff            0x0040
#define kGEMMacTxMacConfig_SlowDown             0x0080
#define kGEMMacTxMacConfig_NoFCS                0x0100
#define kGEMMacTxMacConfig_TxCarrierExt         0x0200


#define kGEMMacRxMacConfig                      (0x6034 | kGEMLength_2)
#define kGEMMacRxMacConfig_RxMacEnable          0x0001
#define kGEMMacRxMacConfig_StripPad             0x0002
#define kGEMMacRxMacConfig_StripFCS             0x0004
#define kGEMMacRxMacConfig_ReceiveAll           0x0008
#define kGEMMacRxMacConfig_ReceiveAllMulticast  0x0010
#define kGEMMacRxMacConfig_HashFilterEnable     0x0020
#define kGEMMacRxMacConfig_AddrFilterEnable     0x0040
#define kGEMMacRxMacConfig_PassErrorFrames      0x0080
#define kGEMMacRxMacConfig_RxCarrierExt         0x0100


#define kGEMMacControlConfig                    (0x6038 | kGEMLength_1)
#define kGEMMacControlConfig_SendPauseEnable    0x01
#define kGEMMacControlConfig_ReceivePauseEnable 0x02
#define kGEMMacControlConfig_PassControlFrames  0x04

#define kGEMMacXIFConfig                        (0x603C | kGEMLength_1)
#define kGEMMacXIFConfig_TxMIIOutputEnable      0x01
#define kGEMMacXIFConfig_MIIIntLoopback         0x02
#define kGEMMacXIFConfig_DisableEcho            0x04
#define kGEMMacXIFConfig_GMIIMode               0x08
#define kGEMMacXIFConfig_MIIBufferEnable        0x10
#define kGEMMacXIFConfig_LinkLed                0x20
#define kGEMMacXIFConfig_FullDuplexLed          0x40

#define kGEMMacInterPktGap0                     (0x6040 | kGEMLength_1)
#define kGEMMacInterPktGap0_Default             0x00

#define kGEMMacInterPktGap1                     (0x6044 | kGEMLength_1)
#define kGEMMacInterPktGap1_Default             0x08

#define kGEMMacInterPktGap2                     (0x6048 | kGEMLength_1)
#define kGEMMacInterPktGap2_Default             0x04

#define kGEMMacSlotTime                         (0x604C | kGEMLength_2)
#define kGEMMacSlotTime_Default                 0x0040

#define kGEMMacMinFrameSize                     (0x6050 | kGEMLength_2)
#define kGEMMacMinFrameSize_Default             0x0040

#define kGEMMacMaxFrameSize                     (0x6054 | kGEMLength_2)
#define kGEMMacMaxFrameSize_Default             0x05EE
#define kGEMMacMaxFrameSize_Aligned		((kGEMMacMaxFrameSize_Default + 7) & ~7)

#define kGEMMacPASize                           (0x6058 | kGEMLength_2)
#define kGEMMacPASize_Default                   0x07

#define kGEMMacJamSize                          (0x605C | kGEMLength_1)
#define kGEMMacJamSize_Default                  0x04

#define kGEMMacAttemptLimit                     (0x6060 | kGEMLength_1)
#define kGEMMacAttemptLimit_Default             0x10

#define kGEMMacTypeControl                      (0x6064 | kGEMLength_2)
#define kGEMMacTypeControl_Default              0x8808

#define kGEMMacAddr                             (0x6080 | kGEMLength_2)
#define kGEMMacAddr6_Default                    0x0001
#define kGEMMacAddr7_Default                    0xC200
#define kGEMMacAddr8_Default                    0x0180

#define kGEMMacAddrFilter                       (0x60A4 | kGEMLength_2)

#define kGEMMacAddrFilterMask1                  (0x60B0 | kGEMLength_1)

#define kGEMMacAddrFilterMask0                  (0x60B4 | kGEMLength_2)

#define kGEMMacHashTable                        (0x60C0 | kGEMLength_2)

#define kGEMMacCollisions                       (0x6100 | kGEMLength_2)
#define kGEMMacSingleCollision                  (0x6104 | kGEMLength_2)
#define kGEMMacExcessCollisions                 (0x6108 | kGEMLength_2)
#define kGEMMacLateCollisions                   (0x610C | kGEMLength_2)

#define kGEMMacDeferTimer                       (0x6110 | kGEMLength_2)

#define kGEMMacPeakAttempts                     (0x6114 | kGEMLength_2)

#define kGEMMacRxFrameCounter                   (0x6118 | kGEMLength_2)

#define kGEMMacRxLengthErrors                   (0x611C | kGEMLength_2)

#define kGEMMacRxAlignErrors                    (0x6120 | kGEMLength_2)

#define kGEMMacRxFCSErrors                      (0x6124 | kGEMLength_2)

#define kGEMMacRxCodeErrors                     (0x6128 | kGEMLength_2)

#define kGEMMacRandomSeed                       (0x6130 | kGEMLength_2)

#define kGEMMacStateMachine                     (0x6134 | kGEMLength_1)


/*
 *       MIF Registers
 */
#define kGEMMIFCLock                            (0x6200 | kGEMLength_1)
#define kGEMMIFClock_One                        0x01

#define kGEMMIFData                             (0x6204 | kGEMLength_1)
#define kGEMMIFData_One                         0x01

#define kGEMMIFOutputEnable                     (0x6208 | kGEMLength_1)
#define kGEMMIFOutputEnable_Enable              0x01

#define kGEMMIFFrame                            (0x620C | kGEMLength_4)
#define kGEMMIFFrame_StartFrameMask             0xC0000000
#define kGEMMIFFrame_StartFrameBit              0x40000000
#define kGEMMIFFrame_OpcodeMask                 0x30000000                      
#define kGEMMIFFrame_OpcodeBit                  0x10000000
#define kGEMMIFFrame_PhyAddrMask                0x0F800000                                      
#define kGEMMIFFrame_PhyAddrBit                 0x00800000
#define kGEMMIFFrame_RegAddrMask                0x007C0000
#define kGEMMIFFrame_RegAddrBit                 0x00040000
#define kGEMMIFFrame_TurnAroundMSB              0x00020000
#define kGEMMIFFrame_TurnAroundLSB              0x00010000
#define kGEMMIFFrame_DataMask                   0x0000FFFF
#define kGEMMIFFrame_DataBit                    0x00000001

#define kGEMMIFConfig                           (0x6210 | kGEMLength_2)
#define kGEMMIFConfig_PhySelect                 0x0001
#define kGEMMIFConfig_PollEnable                0x0002
#define kGEMMIFConfig_BitShiftMode              0x0004
#define kGEMMIFConfig_PollRegAddrMask           0x00F8          
#define kGEMMIFConfig_PollRegAddrBit            0x0008
#define kGEMMIFConfig_ReadMDI_0                 0x0100
#define kGEMMIFConfig_ReadMDI_1                 0x0200
#define kGEMMIFConfig_PollPhyAddrMask           0x7C00
#define kGEMMIFConfig_PollPhyAddrBit            0x0400

#define kGEMMIFMask                             (0x6214 | kGEMLength_2)

#define kGEMMIFStatus                           (0x6218 | kGEMLength_4)
#define kGEMMIFStatus_PollDataMask              0xFFFF0000 
#define kGEMMIFStatus_PollDataBit               0x00010000 
#define kGEMMIFStatus_PollStatusMask            0x0000FFFF 
#define kGEMMIFStatus_PollStatusBit             0x00000001 

#define kGEMMIFStateMachine                     (0x621C | kGEMLength_1)
#define kGEMMIFStateMachine_ControlStateMask    0x07
#define kGEMMIFStateMachine_ControlStateBit     0x01
#define kGEMMIFStateMachine_ExecuteStateMask    0x60
#define kGEMMIFStateMachine_ExecuteStateBit     0x20

/*
 *      PCS/Serdes
 */
#define kGEMPCSControl                          (0x9000 | kGEMLength_2)
#define kGEMPCSControl_1000Mbs                  0x0040  
#define kGEMPCSControl_CollsnTest               0x0080  
#define kGEMPCSControl_DuplexMode               0x0100  
#define kGEMPCSControl_RestartAutoNegot         0x0200          
#define kGEMPCSControl_Isolate                  0x0400  
#define kGEMPCSControl_PowerDown                0x0800  
#define kGEMPCSControl_EnableAutoNegot          0x1000  
#define kGEMPCSControl_100Mbs                   0x2000  
#define kGEMPCSControl_Reset                    0x8000  

#define kGEMPCSStatus                           (0x9004 | kGEMLength_2)
#define kGEMPCSStatus_ExtendedCap               0x0001
#define kGEMPCSStatus_JabberDetect              0x0002
#define kGEMPCSStatus_LinkStatusUp              0x0004
#define kGEMPCSStatus_AutoNegotCap              0x0008
#define kGEMPCSStatus_RemoteFault               0x0010
#define kGEMPCSStatus_AutoNegotComplete         0x0020
#define kGEMPCSStatus_ExtStatus                 0x0100

#define kGEMPCSAdvert                           (0x9008 | kGEMLength_2)
#define kGEMPCSAdvert_FullDuplexCap             0x0020 
#define kGEMPCSAdvert_HalfDuplexCap             0x0040 
#define kGEMPCSAdvert_SymPauseCap               0x0080 
#define kGEMPCSAdvert_AsymPauseCap              0x0100 
#define kGEMPCSAdvert_RemoteFaultMask           0x3000 
#define kGEMPCSAdvert_RemoteFaultBit            0x1000 
#define kGEMPCSAdvert_Ack                       0x4000 
#define kGEMPCSAdvert_NextPage                  0x8000 

#define kGEMPCSLPAbility                        (0x900C | kGEMLength_2)
#define kGEMPCSLPAbility_FullDuplexCap          0x0020 
#define kGEMPCSLPAbility_HalfDuplexCap          0x0040 
#define kGEMPCSLPAbility_SymPauseCap            0x0080 
#define kGEMPCSLPAbility_AsymPauseCap           0x0100 
#define kGEMPCSLPAbility_RemoteFaultMask        0x3000 
#define kGEMPCSLPAbility_RemoteFaultBit         0x1000 
#define kGEMPCSLPAbility_Ack                    0x4000 
#define kGEMPCSLPAbility_NextPage               0x8000 

#define kGEMPCSConfig                           (0x9010 | kGEMLength_1)
#define kGEMPCSConfig_Enable                    0x01
#define kGEMPCSConfig_SignalDetOverride         0x02
#define kGEMPCSConfig_SignalDetInvert           0x04
#define kGEMPCSConfig_JitterStudyMask           0x18
#define kGEMPCSConfig_JitterStudyBit            0x08

#define kGEMPCSStateMachine                     (0x9014 | kGEMLength_4)
#define kGEMPCSStateMachine_TxControlStateMask  0x0000000F
#define kGEMPCSStateMachine_TxControlStateBit   0x00000001
#define kGEMPCSStateMachine_RxControlStateMask  0x000000F0
#define kGEMPCSStateMachine_RxControlStateBit   0x00000010
#define kGEMPCSStateMachine_WordSyncStateMask   0x00000700
#define kGEMPCSStateMachine_WordSyncStateBit    0x00000100
#define kGEMPCSStateMachine_SeqDetectStateMask  0x00001800
#define kGEMPCSStateMachine_SeqDetectStateBit   0x00000800
#define kGEMPCSStateMachine_LinkConfigStateMask 0x0001E000
#define kGEMPCSStateMachine_LinkConfigStateBit  0x00002000
#define kGEMPCSStateMachine_LinkLostCCodes      0x10000000
#define kGEMPCSStateMachine_LinkLostSync        0x20000000
#define kGEMPCSStateMachine_LinkLostSignalDet   0x40000000

#define kGEMPCSInterruptStatus                  (0x9018 | kGEMLength_1)
#define kGEMPCSInterruptStatus_LinkStatusChg    0x04

#define kGEMPCSDatapathMode                     (0x9050 | kGEMLength_1)
#define kGEMPCSDatapathMode_XMode               0x01
#define kGEMPCSDatapathMode_ExtSERDESMode       0x02
#define kGEMPCSDatapathMode_GMIIMode            0x04
#define kGEMPCSDatapathMode_GMIIOutputEnable    0x08

#define kGEMPCSSerdesControl                    (0x9054 | kGEMLength_1)
#define kGEMPCSSerdesControl_DisableLoopback    0x01
#define kGEMPCSSerdesControl_EnableSyncDet      0x02
#define kGEMPCSSerdesControl_LockRefClk         0x04

#define kGEMPCSSerdesOutputSelect               (0x9058 | kGEMLength_1)
#define kGEMPCSSerdesOutputSelect_PROMAddrMask  0x03
#define kGEMPCSSerdesOutputSelect_PROMAddrBit   0x01

#define kGEMPCSSerdesState                      (0x905C | kGEMLength_1)
#define kGEMPCSSerdesState_StateMask            0x03
#define kGEMPCSSerdesState_Bit                  0x01

