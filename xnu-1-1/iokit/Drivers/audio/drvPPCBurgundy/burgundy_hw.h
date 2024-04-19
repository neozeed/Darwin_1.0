/*
 * Copyright (c) 1998-1999 Apple Computer, Inc.  All rights reserved.
 *
 * Burgundy Hardware Registers
 *
 */

#define kSoundCtlReg				0x00
#define kSoundCtlReg_InSubFrame_Mask		0x0000000F	/*All of the input subframe bits*/
#define kSoundCtlReg_InSubFrame_Bit		0x00000001	/*All of the input subframe bits*/
#define kSoundCtlReg_InSubFrame0		0x00000001
#define kSoundCtlReg_InSubFrame1		0x00000002
#define kSoundCtlReg_InSubFrame2		0x00000004
#define kSoundCtlReg_InSubFrame3		0x00000008
#define kSoundCtlReg_OutSubFrame_Mask		0x000000F0
#define kSoundCtlReg_OutSubFrame_Bit		0x00000010
#define kSoundCtlReg_OutSubFrame0		0x00000010
#define kSoundCtlReg_OutSubFrame1		0x00000020
#define kSoundCtlReg_OutSubFrame2		0x00000040
#define kSoundCtlReg_OutSubFrame3		0x00000080
#define kSoundCtlReg_Rate_Mask			0x00000700
#define kSoundCtlReg_Rate_Bit			0x00000100
#define kSoundCtlReg_Rate_44100			0x00000000
#define kSoundCtlReg_Error			0x00000800
#define kSoundCtlReg_PortChange			0x00001000
#define kSoundCtlReg_ErrorInt			0x00002000
#define kSoundCtlReg_StatusSubFrame_Mask	0x00018000
#define kSoundCtlReg_StatusSubFrame_Bit		0x00008000
#define kSoundCtlReg_StatusSubFrame0		0x00000000
#define kSoundCtlReg_StatusSubFrame1		0x00008000
#define kSoundCtlReg_StatusSubFrame2		0x00010000
#define kSoundCtlReg_StatusSubFrame3		0x00018000

/*
 *
 *    Codec Control Fields Bit Format:	vuwr Pppp Aaaa nn qq Ddddddddd
 *					23			     0
 *
 *    Ddddddddd	- Data byte (to Codec)
 *    qq       	- Current byte in register transaction
 *    nn        - Last byte in register transaction
 *    Aaaa	- Codec register address - reg #
 *    Pppp      - Codec register address - page #
 *    r         - Reset transaction pointers
 *    w		- 0=read 1=write
 *    u		- unused
 *    v		- Valid (v is set by the hardware automatically)
 */
#define kCodecCtlReg				0x10
#define kCodecCtlReg_Data_Mask			0x000000FF
#define kCodecCtlReg_Data_Bit			0x00000001
#define kCodecCtlReg_CurrentByte_Mask		0x00000300
#define kCodecCtlReg_CurrentByte_Bit		0x00000100
#define kCodecCtlReg_LastByte_Mask		0x00000C00
#define kCodecCtlReg_LastByte_Bit		0x00000400
#define kCodecCtlReg_Addr_Mask			0x000FF000
#define kCodecCtlReg_Addr_Bit			0x00001000
#define kCodecCtlReg_Reset			0x00100000
#define kCodecCtlReg_Write			0x00200000
#define kCodecCtlReg_Busy			0x01000000

/*
 *
 *    Codec Status Fields Bit Format:	FC00 IIII BB LL Dddddddd Pppp
 *					23		            0
 *    Pppp	- Input sense lines
 *    Dddddddd  - Data byte (from Codec)
 *    LL	- Byte location being read (0-3)
 *    BB        - Byte counter (0-3) Increments when new byte is presented.
 *    IIII	- Indicator code
 *    C		- Codec ready
 *    F		- First valid byte
 */
#define kCodecStatusReg				0x20
#define kCodecStatusReg_Sense_Mask		0x0000000F
#define kCodecStatusReg_Sense_Bit		0x00000001
#define kCodecStatusReg_Sense_Mic               0x00000002
#define kCodecStatusReg_Sense_Headphones        0x00000004
#define kCodecStatusReg_Sense_Headphones2       0x00000008
#define kCodecStatusReg_Data_Mask		0x00000FF0
#define kCodecStatusReg_Data_Bit		0x00000010
#define kCodecStatusReg_CurrentByte_Mask	0x00003000
#define kCodecStatusReg_CurrentByte_Bit		0x00001000
#define kCodecStatusReg_ByteCounter_Mask	0x0000C000
#define kCodecStatusReg_ByteCounter_Bit		0x00004000
#define kCodecStatusReg_Indicator_Mask		0x000F0000
#define kCodecStatusReg_Indicator_Bit		0x00010000
#define kCodecStatusReg_Ready			0x00400000
#define kCodecStatusReg_FirstByte		0x00800000


#define kCodec_Indicator_ToneControl		0x01
#define kCodec_Indicator_Overflow0		0x02
#define kCodec_Indicator_Overflow1		0x03
#define kCodec_Indicator_Overflow2		0x04
#define kCodec_Indicator_InputLineChg		0x06
#define kCodec_Indicator_Threshold0		0x0B
#define kCodec_Indicator_Threshold1		0x0C
#define kCodec_Indicator_Threshold2		0x0D
#define kCodec_Indicator_Threshold3		0x0E
#define kCodec_Indicator_TwilightCmp		0x0F


/*
 * The I/O routines use the following convention to pass Codec register addresses:
 *
 *    Codec Register Address:	00LL Pppp Aaaa 0000 00BB
 *
 *    LL 	- Length of register
 *    Pppp	- Codec register address - page #
 *    Aaaa 	- Codec register address - reg #
 *    BB	- Offset to 1st byte of register.
 */

#define BURGUNDY_LENGTH_1			0x00010000
#define BURGUNDY_LENGTH_2			0x00020000
#define BURGUNDY_LENGTH_3			0x00030000
#define BURGUNDY_LENGTH_4			0x00040000

#define kInitReg				(0x0000 | BURGUNDY_LENGTH_1)
#define kInitReg_Recalibrate			0x01
#define kInitReg_Twilight			0x02

#define kRevisionReg				(0x0100 | BURGUNDY_LENGTH_1)
#define kVersionReg				(0x0101 | BURGUNDY_LENGTH_1)
#define kVendorReg				(0x0102 | BURGUNDY_LENGTH_1)
#define kIdReg					(0x0103 | BURGUNDY_LENGTH_1)

#define kGlobalStatusReg			(0x0200 | BURGUNDY_LENGTH_2)
#define kGlobalStatusReg_ClippingStatus		0x0001
#define kGlobalStatusReg_OverflowStatus0	0x0002
#define kGlobalStatusReg_OverflowStatus1	0x0004
#define kGlobalStatusReg_SOSStatus1		0x0008
#define kGlobalStatusReg_ParallelInStatus	0x0020
#define kGlobalStatusReg_Threshold0		0x0400
#define kGlobalStatusReg_Threshold1		0x0800
#define kGlobalStatusReg_Threshold2		0x1000
#define kGlobalStatusReg_Threshold3		0x2000

#define kReturnZeroReg				(0x0F00 | BURGUNDY_LENGTH_1)


/*
 * This register controls a fixed gain preamp (+24dB) on input ports 5-7
 */
#define kInputPreampReg				(0x1000 | BURGUNDY_LENGTH_1)
#define kInputPreampReg_Port5L			0x04
#define kInputPreampReg_Port5R			0x08
#define kInputPreampReg_Port6L			0x10
#define kInputPreampReg_Port6R			0x20
#define kInputPreampReg_Port7L			0x40
#define kInputPreampReg_Port7R			0x80

/*
 * Mux01 and Mux2 determine which analog inputs will be presented to A/D Converters 0-2
 */
#define kMux01Reg				(0x1100 | BURGUNDY_LENGTH_1)
#define kMux01Reg_Mux0L_Mask			0x03
#define kMux01Reg_Mux0L_SelectPort1L		0x00
#define kMux01Reg_Mux0L_SelectPort2L		0x01
#define kMux01Reg_Mux0L_SelectPort3L		0x02
#define kMux01Reg_Mux0R_Mask			0x0C
#define kMux01Reg_Mux0R_SelectPort1R		0x00
#define kMux01Reg_Mux0R_SelectPort2R		0x04
#define kMux01Reg_Mux0R_SelectPort3R		0x08
#define kMux01Reg_Mux1L_Mask			0x30
#define kMux01Reg_Mux1L_SelectPort1L		0x00
#define kMux01Reg_Mux1L_SelectPort2L		0x10
#define kMux01Reg_Mux1L_SelectPort3L		0x20
#define kMux01Reg_Mux1R_Mask			0xC0
#define kMux01Reg_Mux1R_SelectPort1R		0x00
#define kMux01Reg_Mux1R_SelectPort2R		0x40
#define kMux01Reg_Mux1R_SelectPort3R		0x80

#define kMux2Reg				(0x1200 | BURGUNDY_LENGTH_1)
#define kMux2Reg_Mux2L_Mask			0x03
#define kMux2Reg_Mux2L_SelectPort5L		0x00
#define kMux2Reg_Mux2L_SelectPort6L		0x01
#define kMux2Reg_Mux2L_SelectPort7L		0x02
#define kMux2Reg_Mux2R_Mask			0x0C
#define kMux2Reg_Mux2R_SelectPort5R		0x00
#define kMux2Reg_Mux2R_SelectPort6R		0x04
#define kMux2Reg_Mux2R_SelectPort7R		0x08

/*
 * VGA0-3 control an analog amp at the input to each A/D Converter
 */
#define kVGA0Reg				(0x1300 | BURGUNDY_LENGTH_1)
#define kVGA0Reg_VGA0L_GainMask			0x0F
#define kVGA0Reg_VGA0L_GainBit			0x01
#define kVGA0Reg_VGA0R_GainMask			0xF0
#define kVGA0Reg_VGA0R_GainBit			0x10

#define kVGA1Reg				(0x1400 | BURGUNDY_LENGTH_1)
#define kVGA1Reg_VGA1L_GainMask			0x0F
#define kVGA1Reg_VGA1L_GainBit			0x01
#define kVGA1Reg_VGA1R_GainMask			0xF0
#define kVGA1Reg_VGA1R_GainBit			0x10

#define kVGA2Reg				(0x1500 | BURGUNDY_LENGTH_1)
#define kVGA2Reg_VGA2L_GainMask			0x0F
#define kVGA2Reg_VGA2L_GainBit			0x01
#define kVGA2Reg_VGA2R_GainMask			0xF0
#define kVGA2Reg_VGA2R_GainBit			0x10

#define kVGA3Reg				(0x1600 | BURGUNDY_LENGTH_1)
#define kVGA0Reg_VGA3M_GainMask			0x0F
#define kVGA0Reg_VGA3M_GainBit			0x01

#define kDigInputPortReg			(0x1700 | BURGUNDY_LENGTH_1)


/*
 * This register provides the status of various input sense lines
 */
#define kInputStateReg				(0x1800 | BURGUNDY_LENGTH_1)
#define kInputStateReg_InputIndicator		0x01
#define kInputStateReg_Input1Active		0x02
#define kInputStateReg_Input2Active		0x04
#define kInputStateReg_Input3Active		0x08
#define kInputStateReg_Sense0Active		0x10
#define kInputStateReg_Sense1Active		0x20
#define kInputStateReg_Sense2Active		0x40
#define kInputStateReg_Sense3Active		0x80

/*
 * This register provides the status of the A/D Converters
 */
#define kAD12StatusReg				(0x1900 | BURGUNDY_LENGTH_2)
#define kAD12StatusReg_AD0L_OverRange		0x0001
#define kAD12StatusReg_AD0R_OverRange		0x0002
#define kAD12StatusReg_AD1L_OverRange		0x0004
#define kAD12StatusReg_AD1R_OverRange		0x0008
#define kAD12StatusReg_AD2L_OverRange		0x0010
#define kAD12StatusReg_AD2R_OverRange		0x0020
#define kAD12StatusReg_AD3M_OverRange		0x0040
#define kAD12StatusReg_AD0L_OverRangeIndicator	0x0100
#define kAD12StatusReg_AD0R_OverRangeIndicator	0x0200
#define kAD12StatusReg_AD1L_OverRangeIndicator	0x0400
#define kAD12StatusReg_AD1R_OverRangeIndicator	0x0800
#define kAD12StatusReg_AD2L_OverRangeIndicator	0x1000
#define kAD12StatusReg_AD2R_OverRangeIndicator	0x2000
#define kAD12StatusReg_AD3M_OverRangeIndicator	0x4000


/*
 * These registers control digital scalars for the various input sources.
 * Sources 0-4 are derived from the A/D Converter outputs. Sources A-H
 * are either data from the host system or digital outputs from the chip
 * which are being internally wrapped back.
 */
#define kGAS0LReg				(0x2000 | BURGUNDY_LENGTH_1)
#define kPAS0LReg				(0x2001 | BURGUNDY_LENGTH_1)
#define kGAS0RReg				(0x2002 | BURGUNDY_LENGTH_1)
#define kPAS0RReg				(0x2003 | BURGUNDY_LENGTH_1)

#define kGAS_Default_Gain			0xDF

#define kGAS1LReg				(0x2100 | BURGUNDY_LENGTH_1)
#define kPAS1LReg				(0x2101 | BURGUNDY_LENGTH_1)
#define kGAS1RReg				(0x2102 | BURGUNDY_LENGTH_1)
#define kPAS1RReg				(0x2103 | BURGUNDY_LENGTH_1)

#define kGAS2LReg				(0x2200 | BURGUNDY_LENGTH_1)
#define kPAS2LReg				(0x2201 | BURGUNDY_LENGTH_1)
#define kGAS2RReg				(0x2202 | BURGUNDY_LENGTH_1)
#define kPAS2RReg				(0x2203 | BURGUNDY_LENGTH_1)

#define kGAS3LReg				(0x2300 | BURGUNDY_LENGTH_1)
#define kGAS3RReg				(0x2301 | BURGUNDY_LENGTH_1)
#define kGAS4LReg				(0x2302 | BURGUNDY_LENGTH_1)
#define kGAS4RReg				(0x2303 | BURGUNDY_LENGTH_1)

#define kGASALReg				(0x2500 | BURGUNDY_LENGTH_1)
#define kGASARReg				(0x2501 | BURGUNDY_LENGTH_1)
#define kGASBLReg				(0x2502 | BURGUNDY_LENGTH_1)
#define kGASBRReg				(0x2503 | BURGUNDY_LENGTH_1)

#define kGASCLReg				(0x2600 | BURGUNDY_LENGTH_1)
#define kGASCRReg				(0x2601 | BURGUNDY_LENGTH_1)
#define kGASDLReg				(0x2602 | BURGUNDY_LENGTH_1)
#define kGASDRReg				(0x2603 | BURGUNDY_LENGTH_1)

#define kGASELReg				(0x2700 | BURGUNDY_LENGTH_1)
#define kGASERReg				(0x2701 | BURGUNDY_LENGTH_1)
#define kGASFLReg				(0x2702 | BURGUNDY_LENGTH_1)
#define kGASFRReg				(0x2703 | BURGUNDY_LENGTH_1)

#define kGASGLReg				(0x2800 | BURGUNDY_LENGTH_1)
#define kGASGRReg				(0x2801 | BURGUNDY_LENGTH_1)
#define kGASHLReg				(0x2802 | BURGUNDY_LENGTH_1)
#define kGASHRReg				(0x2803 | BURGUNDY_LENGTH_1)

/*
 * These registers control the inputs of four digital mixers.
 *
 * Each mixer may be provided any combination of the input sources mentioned
 * listed above.
 */
#define kMX0Reg					(0x2900 | BURGUNDY_LENGTH_4)
#define kMX0Reg_Select_IS0L			0x00000001
#define kMX0Reg_Select_IS1L			0x00000002
#define kMX0Reg_Select_IS2L			0x00000004
#define kMX0Reg_Select_IS3L			0x00000008
#define kMX0Reg_Select_IS4L			0x00000010
#define kMX0Reg_Select_ISAL			0x00000100
#define kMX0Reg_Select_ISBL			0x00000200
#define kMX0Reg_Select_ISCL			0x00000400
#define kMX0Reg_Select_ISDL			0x00000800
#define kMX0Reg_Select_ISEL			0x00001000
#define kMX0Reg_Select_ISFL			0x00002000
#define kMX0Reg_Select_ISGL			0x00004000
#define kMX0Reg_Select_ISHL			0x00008000
#define kMX0Reg_Select_IS0R			0x00010000
#define kMX0Reg_Select_IS1R			0x00020000
#define kMX0Reg_Select_IS2R			0x00040000
#define kMX0Reg_Select_IS3R			0x00080000
#define kMX0Reg_Select_IS4R			0x00100000
#define kMX0Reg_Select_ISAR			0x01000000
#define kMX0Reg_Select_ISBR			0x02000000
#define kMX0Reg_Select_ISCR			0x04000000
#define kMX0Reg_Select_ISDR			0x08000000
#define kMX0Reg_Select_ISER			0x10000000
#define kMX0Reg_Select_ISFR			0x20000000
#define kMX0Reg_Select_ISGR			0x40000000
#define kMX0Reg_Select_ISHR			0x80000000

#define kMX1Reg					(0x2A00 | BURGUNDY_LENGTH_4)
#define kMX1Reg_Select_IS0L			0x00000001
#define kMX1Reg_Select_IS1L			0x00000002
#define kMX1Reg_Select_IS2L			0x00000004
#define kMX1Reg_Select_IS3L			0x00000008
#define kMX1Reg_Select_IS4L			0x00000010
#define kMX1Reg_Select_ISAL			0x00000100
#define kMX1Reg_Select_ISBL			0x00000200
#define kMX1Reg_Select_ISCL			0x00000400
#define kMX1Reg_Select_ISDL			0x00000800
#define kMX1Reg_Select_ISEL			0x00001000
#define kMX1Reg_Select_ISFL			0x00002000
#define kMX1Reg_Select_ISGL			0x00004000
#define kMX1Reg_Select_ISHL			0x00008000
#define kMX1Reg_Select_IS0R			0x00010000
#define kMX1Reg_Select_IS1R			0x00020000
#define kMX1Reg_Select_IS2R			0x00040000
#define kMX1Reg_Select_IS3R			0x00080000
#define kMX1Reg_Select_IS4R			0x00100000
#define kMX1Reg_Select_ISAR			0x01000000
#define kMX1Reg_Select_ISBR			0x02000000
#define kMX1Reg_Select_ISCR			0x04000000
#define kMX1Reg_Select_ISDR			0x08000000
#define kMX1Reg_Select_ISER			0x10000000
#define kMX1Reg_Select_ISFR			0x20000000
#define kMX1Reg_Select_ISGR			0x40000000
#define kMX1Reg_Select_ISHR			0x80000000

#define kMX2Reg					(0x2B00 | BURGUNDY_LENGTH_4)
#define kMX2Reg_Select_IS0L			0x00000001
#define kMX2Reg_Select_IS1L			0x00000002
#define kMX2Reg_Select_IS2L			0x00000004
#define kMX2Reg_Select_IS3L			0x00000008
#define kMX2Reg_Select_IS4L			0x00000010
#define kMX2Reg_Select_ISAL			0x00000100
#define kMX2Reg_Select_ISBL			0x00000200
#define kMX2Reg_Select_ISCL			0x00000400
#define kMX2Reg_Select_ISDL			0x00000800
#define kMX2Reg_Select_ISEL			0x00001000
#define kMX2Reg_Select_ISFL			0x00002000
#define kMX2Reg_Select_ISGL			0x00004000
#define kMX2Reg_Select_ISHL			0x00008000
#define kMX2Reg_Select_IS0R			0x00010000
#define kMX2Reg_Select_IS1R			0x00020000
#define kMX2Reg_Select_IS2R			0x00040000
#define kMX2Reg_Select_IS3R			0x00080000
#define kMX2Reg_Select_IS4R			0x00100000
#define kMX2Reg_Select_ISAR			0x01000000
#define kMX2Reg_Select_ISBR			0x02000000
#define kMX2Reg_Select_ISCR			0x04000000
#define kMX2Reg_Select_ISDR			0x08000000
#define kMX2Reg_Select_ISER			0x10000000
#define kMX2Reg_Select_ISFR			0x20000000
#define kMX2Reg_Select_ISGR			0x40000000
#define kMX2Reg_Select_ISHR			0x80000000

#define kMX3Reg					(0x2C00 | BURGUNDY_LENGTH_4)
#define kMX3Reg_Select_IS0L			0x00000001
#define kMX3Reg_Select_IS1L			0x00000002
#define kMX3Reg_Select_IS2L			0x00000004
#define kMX3Reg_Select_IS3L			0x00000008
#define kMX3Reg_Select_IS4L			0x00000010
#define kMX3Reg_Select_ISAL			0x00000100
#define kMX3Reg_Select_ISBL			0x00000200
#define kMX3Reg_Select_ISCL			0x00000400
#define kMX3Reg_Select_ISDL			0x00000800
#define kMX3Reg_Select_ISEL			0x00001000
#define kMX3Reg_Select_ISFL			0x00002000
#define kMX3Reg_Select_ISGL			0x00004000
#define kMX3Reg_Select_ISHL			0x00008000
#define kMX3Reg_Select_IS0R			0x00010000
#define kMX3Reg_Select_IS1R			0x00020000
#define kMX3Reg_Select_IS2R			0x00040000
#define kMX3Reg_Select_IS3R			0x00080000
#define kMX3Reg_Select_IS4R			0x00100000
#define kMX3Reg_Select_ISAR			0x01000000
#define kMX3Reg_Select_ISBR			0x02000000
#define kMX3Reg_Select_ISCR			0x04000000
#define kMX3Reg_Select_ISDR			0x08000000
#define kMX3Reg_Select_ISER			0x10000000
#define kMX3Reg_Select_ISFR			0x20000000
#define kMX3Reg_Select_ISGR			0x40000000
#define kMX3Reg_Select_ISHR			0x80000000

/*
 * This register controls a digital scalar at the output of each mixer.
 */
#define kMXEQ0LReg				(0x2D00 | BURGUNDY_LENGTH_1)
#define kMXEQ0RReg				(0x2D01 | BURGUNDY_LENGTH_1)
#define kMXEQ1LReg				(0x2D02 | BURGUNDY_LENGTH_1)
#define kMXEQ1RReg				(0x2D03 | BURGUNDY_LENGTH_1)

#define kMXEQ2LReg				(0x2E00 | BURGUNDY_LENGTH_1)
#define kMXEQ2RReg				(0x2E01 | BURGUNDY_LENGTH_1)
#define kMXEQ3LReg				(0x2E02 | BURGUNDY_LENGTH_1)
#define kMXEQ3RReg				(0x2E03 | BURGUNDY_LENGTH_1)

#define kMXEQ_Default_Gain			0xDF

/*
 * This register controls a digital demultiplexer which routes
 * the mixer 0-3 output to one 12 output sources.
 *
 * Output sources 0-2 can eventually be converted to analog. The
 * remaining output sources remain digital and may be either
 * sent to the host or wrapped back as digital input sources.
 */
#define kOSReg					(0x2F00 | BURGUNDY_LENGTH_4)
#define kOSReg_OS0_SelectMask			0x00000003
#define kOSReg_OS0_SelectBit			0x00000001
#define kOSReg_OS0_Select_MXO0			0x00000000
#define kOSReg_OS0_Select_MXO1			0x00000001
#define kOSReg_OS0_Select_MXO2			0x00000002
#define kOSReg_OS0_Select_MXO3			0x00000003

#define kOSReg_OS1_SelectMask			0x0000000C
#define kOSReg_OS1_SelectBit			0x00000004
#define kOSReg_OS1_Select_MXO0			0x00000000
#define kOSReg_OS1_Select_MXO1			0x00000004
#define kOSReg_OS1_Select_MXO2			0x00000008
#define kOSReg_OS1_Select_MXO3			0x0000000C

#define kOSReg_OS2_SelectMask			0x00000030
#define kOSReg_OS2_SelectBit			0x00000010
#define kOSReg_OS2_Select_MXO0			0x00000000
#define kOSReg_OS2_Select_MXO1			0x00000010
#define kOSReg_OS2_Select_MXO2			0x00000020
#define kOSReg_OS2_Select_MXO3			0x00000030

#define kOSReg_OS3_SelectMask			0x000000C0
#define kOSReg_OS3_SelectBit			0x00000040
#define kOSReg_OS3_Select_MXO0			0x00000000
#define kOSReg_OS3_Select_MXO1			0x00000040
#define kOSReg_OS3_Select_MXO2			0x00000080
#define kOSReg_OS3_Select_MXO3			0x000000C0

#define kOSReg_OSA_SelectMask			0x00030000
#define kOSReg_OSA_SelectBit			0x00010000
#define kOSReg_OSA_Select_MXO0			0x00000000
#define kOSReg_OSA_Select_MXO1			0x00010000
#define kOSReg_OSA_Select_MXO2			0x00020000
#define kOSReg_OSA_Select_MXO3			0x00030000

#define kOSReg_OSB_SelectMask			0x000C0000
#define kOSReg_OSB_SelectBit			0x00040000
#define kOSReg_OSB_Select_MXO0			0x00000000
#define kOSReg_OSB_Select_MXO1			0x00040000
#define kOSReg_OSB_Select_MXO2			0x00080000
#define kOSReg_OSB_Select_MXO3			0x000C0000

#define kOSReg_OSC_SelectMask			0x00100000
#define kOSReg_OSC_SelectBit			0x00300000
#define kOSReg_OSC_Select_MXO0			0x00000000
#define kOSReg_OSC_Select_MXO1			0x00100000
#define kOSReg_OSC_Select_MXO2			0x00200000
#define kOSReg_OSC_Select_MXO3			0x00300000

#define kOSReg_OSD_SelectMask			0x00C00000
#define kOSReg_OSD_SelectBit			0x00400000
#define kOSReg_OSD_Select_MXO0			0x00000000
#define kOSReg_OSD_Select_MXO1			0x00400000
#define kOSReg_OSD_Select_MXO2			0x00800000
#define kOSReg_OSD_Select_MXO3			0x00C00000

#define kOSReg_OSE_SelectMask			0x03000000
#define kOSReg_OSE_SelectBit			0x01000000
#define kOSReg_OSE_Select_MXO0			0x00000000
#define kOSReg_OSE_Select_MXO1			0x01000000
#define kOSReg_OSE_Select_MXO2			0x02000000
#define kOSReg_OSE_Select_MXO3			0x03000000

#define kOSReg_OSF_SelectMask			0x0C000000
#define kOSReg_OSF_SelectBit			0x04000000
#define kOSReg_OSF_Select_MXO0			0x00000000
#define kOSReg_OSF_Select_MXO1			0x04000000
#define kOSReg_OSF_Select_MXO2			0x08000000
#define kOSReg_OSF_Select_MXO3			0x0C000000

#define kOSReg_OSG_SelectMask			0x30000000
#define kOSReg_OSG_SelectBit			0x10000000
#define kOSReg_OSG_Select_MXO0			0x00000000
#define kOSReg_OSG_Select_MXO1			0x10000000
#define kOSReg_OSG_Select_MXO2			0x20000000
#define kOSReg_OSG_Select_MXO3			0x30000000

#define kOSReg_OSH_SelectMask			0xC0000000
#define kOSReg_OSH_SelectBit			0x40000000
#define kOSReg_OSH_Select_MXO0			0x00000000
#define kOSReg_OSH_Select_MXO1			0x40000000
#define kOSReg_OSH_Select_MXO2			0x80000000
#define kOSReg_OSH_Select_MXO3			0xC0000000

/*
 * This register controls a digital scalar for Output sources 0-3
 */
#define kGAP0LReg				(0x3000 | BURGUNDY_LENGTH_1)
#define kGAP0RReg				(0x3001 | BURGUNDY_LENGTH_1)
#define kGAP1LReg				(0x3002 | BURGUNDY_LENGTH_1)
#define kGAP1RReg				(0x3003 | BURGUNDY_LENGTH_1)

#define kGAP2LReg				(0x3100 | BURGUNDY_LENGTH_1)
#define kGAP2RReg				(0x3101 | BURGUNDY_LENGTH_1)
#define kGAP3LReg				(0x3102 | BURGUNDY_LENGTH_1)
#define kGAP3RReg				(0x3103 | BURGUNDY_LENGTH_1)

#define kGAP_Default_Gain			0xDF

/*
 * These registers access the values of four peak-level meters. Inputs to the
 * digital mixers MX0-3 or the mixer outputs may be routed these meters.
 */
#define kPeakLvl0Reg				(0x3300 | BURGUNDY_LENGTH_2)
#define kPeakLvl1Reg				(0x3302 | BURGUNDY_LENGTH_2)

#define kPeakLvl3Reg				(0x3400 | BURGUNDY_LENGTH_2)
#define kPeakLvl4Reg				(0x3402 | BURGUNDY_LENGTH_2)

/*
 * These registers select the digital inputs to be monitored by each level meter.
 */
#define kPeakLvl0SourceReg			(0x3500 | BURGUNDY_LENGTH_1)
#define kPeakLvl0SourceReg_SelectMask		0x3F
#define kPeakLvl0SourceReg_Select_AS0L		0x00
#define kPeakLvl0SourceReg_Select_AS0R		0x01
#define kPeakLvl0SourceReg_Select_AS1L		0x02
#define kPeakLvl0SourceReg_Select_AS1R		0x03
#define kPeakLvl0SourceReg_Select_AS2L		0x04
#define kPeakLvl0SourceReg_Select_AS2R		0x05
#define kPeakLvl0SourceReg_Select_AS3L		0x06
#define kPeakLvl0SourceReg_Select_AS3R		0x07
#define kPeakLvl0SourceReg_Select_AS4L		0x08
#define kPeakLvl0SourceReg_Select_AS4R		0x09
#define kPeakLvl0SourceReg_Select_IS0L		0x10
#define kPeakLvl0SourceReg_Select_IS0R		0x11
#define kPeakLvl0SourceReg_Select_IS1L		0x12
#define kPeakLvl0SourceReg_Select_IS1R		0x13
#define kPeakLvl0SourceReg_Select_IS2L		0x14
#define kPeakLvl0SourceReg_Select_IS2R		0x15
#define kPeakLvl0SourceReg_Select_IS3L		0x16
#define kPeakLvl0SourceReg_Select_IS3R		0x17
#define kPeakLvl0SourceReg_Select_IS4L		0x18
#define kPeakLvl0SourceReg_Select_IS5R		0x19
#define kPeakLvl0SourceReg_Select_ISAL		0x20
#define kPeakLvl0SourceReg_Select_ISAR		0x21
#define kPeakLvl0SourceReg_Select_ISBL		0x22
#define kPeakLvl0SourceReg_Select_ISBR		0x23
#define kPeakLvl0SourceReg_Select_ISCL		0x24
#define kPeakLvl0SourceReg_Select_ISCR		0x25
#define kPeakLvl0SourceReg_Select_ISDL		0x26
#define kPeakLvl0SourceReg_Select_ISDR		0x27
#define kPeakLvl0SourceReg_Select_ISEL		0x28
#define kPeakLvl0SourceReg_Select_ISER		0x29
#define kPeakLvl0SourceReg_Select_ISFL		0x2A
#define kPeakLvl0SourceReg_Select_ISFR		0x2B
#define kPeakLvl0SourceReg_Select_ISGL		0x2C
#define kPeakLvl0SourceReg_Select_ISGR		0x2D
#define kPeakLvl0SourceReg_Select_ISHL		0x2E
#define kPeakLvl0SourceReg_Select_ISHR		0x2F
#define kPeakLvl0SourceReg_Select_MXO0L		0x30
#define kPeakLvl0SourceReg_Select_MXO0R		0x31
#define kPeakLvl0SourceReg_Select_MXO1L		0x32
#define kPeakLvl0SourceReg_Select_MXO1R		0x33
#define kPeakLvl0SourceReg_Select_MXO2L		0x34
#define kPeakLvl0SourceReg_Select_MXO2R		0x35
#define kPeakLvl0SourceReg_Select_MXO3L		0x36
#define kPeakLvl0SourceReg_Select_MXO3R		0x37
#define kPeakLvl0SourceReg_Zero			0x40

#define kPeakLvl1SourceReg			(0x3501 | BURGUNDY_LENGTH_1)
#define kPeakLvl1SourceReg_SelectMask		0x3F
#define kPeakLvl1SourceReg_Select_AS0L		0x00
#define kPeakLvl1SourceReg_Select_AS0R		0x01
#define kPeakLvl1SourceReg_Select_AS1L		0x02
#define kPeakLvl1SourceReg_Select_AS1R		0x03
#define kPeakLvl1SourceReg_Select_AS2L		0x04
#define kPeakLvl1SourceReg_Select_AS2R		0x05
#define kPeakLvl1SourceReg_Select_AS3L		0x06
#define kPeakLvl1SourceReg_Select_AS3R		0x07
#define kPeakLvl1SourceReg_Select_AS4L		0x08
#define kPeakLvl1SourceReg_Select_AS4R		0x09
#define kPeakLvl1SourceReg_Select_IS0L		0x10
#define kPeakLvl1SourceReg_Select_IS0R		0x11
#define kPeakLvl1SourceReg_Select_IS1L		0x12
#define kPeakLvl1SourceReg_Select_IS1R		0x13
#define kPeakLvl1SourceReg_Select_IS2L		0x14
#define kPeakLvl1SourceReg_Select_IS2R		0x15
#define kPeakLvl1SourceReg_Select_IS3L		0x16
#define kPeakLvl1SourceReg_Select_IS3R		0x17
#define kPeakLvl1SourceReg_Select_IS4L		0x18
#define kPeakLvl1SourceReg_Select_IS5R		0x19
#define kPeakLvl1SourceReg_Select_ISAL		0x20
#define kPeakLvl1SourceReg_Select_ISAR		0x21
#define kPeakLvl1SourceReg_Select_ISBL		0x22
#define kPeakLvl1SourceReg_Select_ISBR		0x23
#define kPeakLvl1SourceReg_Select_ISCL		0x24
#define kPeakLvl1SourceReg_Select_ISCR		0x25
#define kPeakLvl1SourceReg_Select_ISDL		0x26
#define kPeakLvl1SourceReg_Select_ISDR		0x27
#define kPeakLvl1SourceReg_Select_ISEL		0x28
#define kPeakLvl1SourceReg_Select_ISER		0x29
#define kPeakLvl1SourceReg_Select_ISFL		0x2A
#define kPeakLvl1SourceReg_Select_ISFR		0x2B
#define kPeakLvl1SourceReg_Select_ISGL		0x2C
#define kPeakLvl1SourceReg_Select_ISGR		0x2D
#define kPeakLvl1SourceReg_Select_ISHL		0x2E
#define kPeakLvl1SourceReg_Select_ISHR		0x2F
#define kPeakLvl1SourceReg_Select_MXO0L		0x30
#define kPeakLvl1SourceReg_Select_MXO0R		0x31
#define kPeakLvl1SourceReg_Select_MXO1L		0x32
#define kPeakLvl1SourceReg_Select_MXO1R		0x33
#define kPeakLvl1SourceReg_Select_MXO2L		0x34
#define kPeakLvl1SourceReg_Select_MXO2R		0x35
#define kPeakLvl1SourceReg_Select_MXO3L		0x36
#define kPeakLvl1SourceReg_Select_MXO3R		0x37
#define kPeakLvl1SourceReg_Zero			0x40

#define kPeakLvl2SourceReg			(0x3502 | BURGUNDY_LENGTH_1)
#define kPeakLvl2SourceReg_SelectMask		0x3F
#define kPeakLvl2SourceReg_Select_AS0L		0x00
#define kPeakLvl2SourceReg_Select_AS0R		0x01
#define kPeakLvl2SourceReg_Select_AS1L		0x02
#define kPeakLvl2SourceReg_Select_AS1R		0x03
#define kPeakLvl2SourceReg_Select_AS2L		0x04
#define kPeakLvl2SourceReg_Select_AS2R		0x05
#define kPeakLvl2SourceReg_Select_AS3L		0x06
#define kPeakLvl2SourceReg_Select_AS3R		0x07
#define kPeakLvl2SourceReg_Select_AS4L		0x08
#define kPeakLvl2SourceReg_Select_AS4R		0x09
#define kPeakLvl2SourceReg_Select_IS0L		0x10
#define kPeakLvl2SourceReg_Select_IS0R		0x11
#define kPeakLvl2SourceReg_Select_IS1L		0x12
#define kPeakLvl2SourceReg_Select_IS1R		0x13
#define kPeakLvl2SourceReg_Select_IS2L		0x14
#define kPeakLvl2SourceReg_Select_IS2R		0x15
#define kPeakLvl2SourceReg_Select_IS3L		0x16
#define kPeakLvl2SourceReg_Select_IS3R		0x17
#define kPeakLvl2SourceReg_Select_IS4L		0x18
#define kPeakLvl2SourceReg_Select_IS5R		0x19
#define kPeakLvl2SourceReg_Select_ISAL		0x20
#define kPeakLvl2SourceReg_Select_ISAR		0x21
#define kPeakLvl2SourceReg_Select_ISBL		0x22
#define kPeakLvl2SourceReg_Select_ISBR		0x23
#define kPeakLvl2SourceReg_Select_ISCL		0x24
#define kPeakLvl2SourceReg_Select_ISCR		0x25
#define kPeakLvl2SourceReg_Select_ISDL		0x26
#define kPeakLvl2SourceReg_Select_ISDR		0x27
#define kPeakLvl2SourceReg_Select_ISEL		0x28
#define kPeakLvl2SourceReg_Select_ISER		0x29
#define kPeakLvl2SourceReg_Select_ISFL		0x2A
#define kPeakLvl2SourceReg_Select_ISFR		0x2B
#define kPeakLvl2SourceReg_Select_ISGL		0x2C
#define kPeakLvl2SourceReg_Select_ISGR		0x2D
#define kPeakLvl2SourceReg_Select_ISHL		0x2E
#define kPeakLvl2SourceReg_Select_ISHR		0x2F
#define kPeakLvl2SourceReg_Select_MXO0L		0x30
#define kPeakLvl2SourceReg_Select_MXO0R		0x31
#define kPeakLvl2SourceReg_Select_MXO1L		0x32
#define kPeakLvl2SourceReg_Select_MXO1R		0x33
#define kPeakLvl2SourceReg_Select_MXO2L		0x34
#define kPeakLvl2SourceReg_Select_MXO2R		0x35
#define kPeakLvl2SourceReg_Select_MXO3L		0x36
#define kPeakLvl2SourceReg_Select_MXO3R		0x37
#define kPeakLvl2SourceReg_Zero			0x40

#define kPeakLvl3SourceReg			(0x3503 | BURGUNDY_LENGTH_1)
#define kPeakLvl3SourceReg_SelectMask		0x3F
#define kPeakLvl3SourceReg_Select_AS0L		0x00
#define kPeakLvl3SourceReg_Select_AS0R		0x01
#define kPeakLvl3SourceReg_Select_AS1L		0x02
#define kPeakLvl3SourceReg_Select_AS1R		0x03
#define kPeakLvl3SourceReg_Select_AS2L		0x04
#define kPeakLvl3SourceReg_Select_AS2R		0x05
#define kPeakLvl3SourceReg_Select_AS3L		0x06
#define kPeakLvl3SourceReg_Select_AS3R		0x07
#define kPeakLvl3SourceReg_Select_AS4L		0x08
#define kPeakLvl3SourceReg_Select_AS4R		0x09
#define kPeakLvl3SourceReg_Select_IS0L		0x10
#define kPeakLvl3SourceReg_Select_IS0R		0x11
#define kPeakLvl3SourceReg_Select_IS1L		0x12
#define kPeakLvl3SourceReg_Select_IS1R		0x13
#define kPeakLvl3SourceReg_Select_IS2L		0x14
#define kPeakLvl3SourceReg_Select_IS2R		0x15
#define kPeakLvl3SourceReg_Select_IS3L		0x16
#define kPeakLvl3SourceReg_Select_IS3R		0x17
#define kPeakLvl3SourceReg_Select_IS4L		0x18
#define kPeakLvl3SourceReg_Select_IS5R		0x19
#define kPeakLvl3SourceReg_Select_ISAL		0x20
#define kPeakLvl3SourceReg_Select_ISAR		0x21
#define kPeakLvl3SourceReg_Select_ISBL		0x22
#define kPeakLvl3SourceReg_Select_ISBR		0x23
#define kPeakLvl3SourceReg_Select_ISCL		0x24
#define kPeakLvl3SourceReg_Select_ISCR		0x25
#define kPeakLvl3SourceReg_Select_ISDL		0x26
#define kPeakLvl3SourceReg_Select_ISDR		0x27
#define kPeakLvl3SourceReg_Select_ISEL		0x28
#define kPeakLvl3SourceReg_Select_ISER		0x29
#define kPeakLvl3SourceReg_Select_ISFL		0x2A
#define kPeakLvl3SourceReg_Select_ISFR		0x2B
#define kPeakLvl3SourceReg_Select_ISGL		0x2C
#define kPeakLvl3SourceReg_Select_ISGR		0x2D
#define kPeakLvl3SourceReg_Select_ISHL		0x2E
#define kPeakLvl3SourceReg_Select_ISHR		0x2F
#define kPeakLvl3SourceReg_Select_MXO0L		0x30
#define kPeakLvl3SourceReg_Select_MXO0R		0x31
#define kPeakLvl3SourceReg_Select_MXO1L		0x32
#define kPeakLvl3SourceReg_Select_MXO1R		0x33
#define kPeakLvl3SourceReg_Select_MXO2L		0x34
#define kPeakLvl3SourceReg_Select_MXO2R		0x35
#define kPeakLvl3SourceReg_Select_MXO3L		0x36
#define kPeakLvl3SourceReg_Select_MXO3R		0x37
#define kPeakLvl3SourceReg_Zero			0x40

/*
 * This register holds the threshold value at which the level meters will
 * indicate an event.
 */
#define kPeakLvlThresholdReg			(0x3600 | BURGUNDY_LENGTH_2)
#define kPeakLvlThresholdReg_Default		0x7FFF

/*
 * These registers indicate overflows in the digital scalars prior to the mixers.
 * See GAS0-4.
 */
#define kISOverflowReg				(0x3700 | BURGUNDY_LENGTH_1)
#define kISOverflowReg_IS0			0x01
#define kISOverflowReg_IS1			0x02
#define kISOverflowReg_IS2			0x04
#define kISOverflowReg_IS3			0x08
#define kISOverflowReg_IS4			0x10
#define kISOverflowReg_Indicator		0x80

/*
 * This register monitors overflows in digital mixers 0-3.
 */
#define kMXOverflowReg				(0x3701 | BURGUNDY_LENGTH_2)
#define kMXOverflowReg_MX0L			0x0001
#define kMXOverflowReg_MX0R			0x0002
#define kMXOverflowReg_MX1L			0x0004
#define kMXOverflowReg_MX1R			0x0008
#define kMXOverflowReg_MX2L			0x0010
#define kMXOverflowReg_MX2R			0x0020
#define kMXOverflowReg_MX3L			0x0040
#define kMXOverflowReg_MX3R			0x0080
#define kMXOverflowReg_MX0L_Indicator		0x0100
#define kMXOverflowReg_MX0R_Indicator		0x0200
#define kMXOverflowReg_MX1L_Indicator		0x0400
#define kMXOverflowReg_MX1R_Indicator		0x0800
#define kMXOverflowReg_MX2L_Indicator		0x1000
#define kMXOverflowReg_MX2R_Indicator		0x2000
#define kMXOverflowReg_MX3L_Indicator		0x4000
#define kMXOverflowReg_MX3R_Indicator		0x8000

/*
 * This register controls a digital tone filter connected to the output
 * of mixer 0.
 *
 * The registers are programmed with 3-byte coefficients for the filters.
 */
#define kSOSS0B0Reg				(0x4000 | BURGUNDY_LENGTH_3)
#define kSOSS0B1Reg				(0x4100 | BURGUNDY_LENGTH_3)
#define kSOSS0B2Reg				(0x4200 | BURGUNDY_LENGTH_3)
#define kSOSS0A1Reg				(0x4300 | BURGUNDY_LENGTH_3)
#define kSOSS0A2Reg				(0x4400 | BURGUNDY_LENGTH_3)

#define kSOSS1B0Reg				(0x4500 | BURGUNDY_LENGTH_3)
#define kSOSS1B1Reg				(0x4600 | BURGUNDY_LENGTH_3)
#define kSOSS1B2Reg				(0x4700 | BURGUNDY_LENGTH_3)
#define kSOSS1A1Reg				(0x4800 | BURGUNDY_LENGTH_3)
#define kSOSS1A2Reg				(0x4900 | BURGUNDY_LENGTH_3)

#define kSOSS2B0Reg				(0x4A00 | BURGUNDY_LENGTH_3)
#define kSOSS2B1Reg				(0x4B00 | BURGUNDY_LENGTH_3)
#define kSOSS2B2Reg				(0x4C00 | BURGUNDY_LENGTH_3)
#define kSOSS2A1Reg				(0x4D00 | BURGUNDY_LENGTH_3)
#define kSOSS2A2Reg				(0x4E00 | BURGUNDY_LENGTH_3)

#define kSOSS3B0Reg				(0x5000 | BURGUNDY_LENGTH_3)
#define kSOSS3B1Reg				(0x5100 | BURGUNDY_LENGTH_3)
#define kSOSS3A1Reg				(0x5200 | BURGUNDY_LENGTH_3)
#define kSOSS3A2Reg				(0x5300 | BURGUNDY_LENGTH_3)

#define kSOSControlReg				(0x5500 | BURGUNDY_LENGTH_1)
#define kSOSControlReg_Mode0			0x00
#define kSOSControlReg_Mode1			0x01

#define kSOSOverflowReg				(0x5600 | BURGUNDY_LENGTH_1)
#define kSOSOverflowReg_S0			0x01
#define kSOSOverflowReg_S1			0x02
#define kSOSOverflowReg_S2			0x04
#define kSOSOverflowReg_S3			0x08
#define kSOSOverflowReg_Indicator		0x10

/*
 * This register controls the muting the of analog outputs
 */
#define kOutputMuteReg	 			(0x6000 | BURGUNDY_LENGTH_1)
#define kOutputMuteReg_Port13M			0x01
#define kOutputMuteReg_Port14L			0x02
#define kOutputMuteReg_Port14R			0x04
#define kOutputMuteReg_Port15L			0x08
#define kOutputMuteReg_Port15R			0x10
#define kOutputMuteReg_Port16L			0x20
#define kOutputMuteReg_Port16R			0x40
#define kOutputMuteReg_Port17M			0x80

/*
 * These registers control attenuators at each analog output
 */
#define kOutputLvlPort13Reg			(0x6100 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort13Reg_Mask		0x0F
#define kOutputLvlPort13Reg_Bit			0x01

#define kOutputLvl_Default			0x00

#define kOutputLvlPort14Reg			(0x6200 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort14Reg_LeftMask		0x0F
#define kOutputLvlPort14Reg_LeftBit		0x01
#define kOutputLvlPort14Reg_RightMask		0xF0
#define kOutputLvlPort14Reg_RightBit		0x10

#define kOutputLvlPort15Reg			(0x6300 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort15Reg_LeftMask		0x0F
#define kOutputLvlPort15Reg_LeftBit		0x01
#define kOutputLvlPort15Reg_RightMask		0xF0
#define kOutputLvlPort15Reg_RightBit		0x10

#define kOutputLvlPort16Reg			(0x6400 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort16Reg_LeftMask		0x0F
#define kOutputLvlPort16Reg_LeftBit		0x01
#define kOutputLvlPort16Reg_RightMask		0xF0
#define kOutputLvlPort16Reg_RightBit		0x10

#define kOutputLvlPort17Reg			(0x6500 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort17Reg_Mask		0x0F
#define kOutputLvlPort17Reg_Bit			0x01

/*
 * This register controls discharge current to reduce noise if the chip
 * is powered down.
 */
#define kOutputSettleTimeReg			(0x6600 | BURGUNDY_LENGTH_2)
#define kOutputSettleTimeReg_Port13_Mask	0x0003
#define kOutputSettleTimeReg_Port13_Bit		0x0001
#define kOutputSettleTimeReg_Port14_Mask	0x000C
#define kOutputSettleTimeReg_Port14_Bit		0x0004
#define kOutputSettleTimeReg_Port15_Mask	0x0030
#define kOutputSettleTimeReg_Port15_Bit		0x0010
#define kOutputSettleTimeReg_Port16_Mask	0x00C0
#define kOutputSettleTimeReg_Port16_Bit		0x0040
#define kOutputSettleTimeReg_Port17_Mask	0x0300
#define kOutputSettleTimeReg_Port17_Bit		0x0100


#define kOutputCtl0Reg				(0x6700 | BURGUNDY_LENGTH_1)
#define kOutputCtl0Reg_OutCtl0_High		0x10
#define kOutputCtl0Reg_OutCtl0_Tristate		0x20
#define kOutputCtl0Reg_OutCtl1_High		0x40
#define kOutputCtl0Reg_OutCtl1_Tristate		0x80

#define kOutputCtl2Reg				(0x6800 | BURGUNDY_LENGTH_1)
#define kOutputCtl2Reg_OutCtl2_High		0x01
#define kOutputCtl2Reg_OutCtl2_Tristate		0x02
#define kOutputCtl2Reg_OutCtl3_High		0x04
#define kOutputCtl2Reg_OutCtl3_Tristate		0x08
#define kOutputCtl2Reg_OutCtl4_High		0x10
#define kOutputCtl2Reg_OutCtl4_Tristate		0x20

#define kDOutConfigReg				(0x6900 | BURGUNDY_LENGTH_1)
#define kDOutConfigReg_Zero			0x80
#define kDOutConfigReg_Enable			0x80

#define kMClkReg				(0x7000 | BURGUNDY_LENGTH_1)
#define kMClkReg_Tristate			0x00
#define kMClkReg_Div1				0x01
#define kMClkReg_Div2				0x02
#define kMClkReg_Div4				0x03


/*
 * These registers control the digital input sources A-C to the chip.
 * A source may be derived from data comming from the host (SF0-3) or
 * from an internally generated digital output source (OSA-D)
 */
#define kSDInReg				(0x7800 | BURGUNDY_LENGTH_1)
#define kSDInReg_OSA_To_SF0			0x01
#define kSDInReg_ASA_Mask			0x02
#define kSDInReg_ASA_From_SF0			0x00
#define kSDInReg_ASA_From_OSA			0x02
#define kSDInReg_OSB_To_SF1			0x04
#define kSDInReg_ASB_From_SF1			0x00
#define kSDInReg_ASB_From_OSB			0x08
#define kSDInReg_OSC_To_SF2			0x10
#define kSDInReg_ASC_From_SF2			0x00
#define kSDInReg_ASC_From_OSC			0x20
#define kSDInReg_OSD_To_SF3			0x40
#define kSDInReg_ASD_From_SF3			0x00
#define kSDInReg_ASD_From_OSD			0x80

/*
 * This register controls digital output sources (OSE-H) from the chip.
 * These sources may be routed to the host (SF0-3) or may be internally
 * wrapped back to input sources (ASE-H).
 */
#define kSDOutReg				(0x7A00 | BURGUNDY_LENGTH_1)
#define kSDOutReg_OSE_To_SF0			0x01
#define kSDOutReg_ASE_Mask			0x02
#define kSDOutReg_ASE_From_SF0			0x00
#define kSDOutReg_ASE_From_OSE			0x02
#define kSDOutReg_OSF_To_SF1			0x04
#define kSDOutReg_ASF_From_SF1			0x00
#define kSDOutReg_ASF_From_OSF			0x08
#define kSDOutReg_OSG_To_SF2			0x10
#define kSDOutReg_ASG_From_SF2			0x00
#define kSDOutReg_ASG_From_OSG			0x20
#define kSDOutReg_OSH_To_SF3			0x40
#define kSDOutReg_ASH_From_SF3			0x00
#define kSDOutReg_ASH_From_OSH			0x80

#define kThresholdMaskReg			(0x7A00 | BURGUNDY_LENGTH_1)
#define kThresholdMaskReg_Threshold0		0x01
#define kThresholdMaskReg_Threshold1		0x02
#define kThresholdMaskReg_Threshold2		0x04
#define kThresholdMaskReg_Threshold3		0x08

