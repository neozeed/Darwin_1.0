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
 *   AWACs - Audio Waveform Amplifier and Converter
 *
 *   Notes:
 *
 *   1. All values written to (or read from) hardware must be endian reversed
 *      when using the constants from this file.
 *   2. The hardware uses a serial-bus time multiplexed scheme to communicate
 *      with up to four addressable devices (subframes). AWACs currently only uses 
 *      'SubFrame 0'.  When a register field refers to a  subframe, then the SubFrame0 
 *      value  should be used.
 *   3. The CodecControlRegister is used to as a pointer/data reg to access four 12-bit
 *      registers. The kCodecCtlAddress* values indicate the 12-bit register to be
 *      written and the kCodecCtlDataMask indicates the 12-bits to be written. 
 *      kCodecCtlBusy is set/cleared by hardware when a pending value waiting to be
 *      written to one of the 12-bit registers. kCodecCtlEMSelect* indicates which
 *      SubFrame the data will go out on. (See Note 2)
 *  4.  The SoundControlRegister is used to set the data rate and indicates the input,
 *      output and status subframes. (See Note 2).
 *  5.  The CodecStatusRegister indicate when there was an error, or a change of input
 *      or output sources, i.e. a mic, line, or headphones were plugged in.
 *  6.  The ClippingCountRegister  is incremented on a left/right channel overflow. It
 *      is reset by reading its contents.
 *
 */

/*----------------------------- AWACs register mapping structure -------------------------*/

struct awacs_regmap_t
{
	volatile u_int32_t		       	SoundControlRegister;
        u_int32_t	       		       	pad0[3];
	volatile u_int32_t		       	CodecControlRegister;
        u_int32_t	       			pad1[3];
	volatile u_int32_t	       		CodecStatusRegister;
        u_int32_t	               		pad2[3];
	volatile u_int32_t	       		ClippingCountRegister;
        u_int32_t	       			pad3[3];
	volatile u_int32_t	       		ByteSwappingRegister;
};


/*------------------------ AWACs CODEC Control register constants -----------------------*/

enum AWACsCODEC_ControlRegisterGeneralConstants 
{
	kCodecCtlDataMask	=	0x00000FFF,	        /*CODEC control data bits*/

	kCodecCtlAddress0	=	0x00000000,		/*Select address 0*/
	kCodecCtlAddress1	=	0x00001000,		/*Select address 1*/
	kCodecCtlAddress2	=	0x00002000,		/*Select address 2*/
	kCodecCtlAddress4	=	0x00004000,		/*Select address 4*/
	kCodecCtlAddress5	=	0x00005000,		/*Select address 5 - Screamer only */
	kCodecCtlAddress6	=	0x00006000,		/*Select address 6 - Screamer only */
	kCodecCtlAddress7	=	0x00007000,		/*Select address 7 - Screamer only */
	
	kCodecCtlEMSelect0	=	0x00000000,		/*Select subframe 0*/
	kCodecCtlEMSelect1	=	0x00400000,		/*Select subframe 1*/
	kCodecCtlEMSelect2	=	0x00800000,		/*Select subframe 2*/
	kCodecCtlEMSelect4	=	0x00C00000,		/*Select subframe 4*/

	kCodecCtlBusy		=	0x01000000 		/*AWACS busy bit */    
};

enum AWACsCODEC_ControlRegister0Constants 
{
	kLeftInputGainMask		=	0x000000F0,	/*Bits used for left input gain*/
        kLeftInputGainShift		=	4,
	kRightInputGainMask		=	0x0000000F,	/*Bits used for right input gain*/
        kRightInputGainShift		=	0,
	
	kDefaultMicGain			=	0x000000CC,	/*Default right & left gain for a mic*/
	kDefaultCDGain			=	0x000000BB,	/*Default right & left gain for a CD*/
	
	kNotMicPreamp			=	0x00000100,	/*Select preamp on or off*/
	
	kInputMuxMask			=	0x00000E00,	/*Bits used to set which input to use */
	kCDInput		       	=	0x00000200,	/*Bit 9 = CD ROM audio input*/
	kMicInput		       	=	0x00000400,	/*Bit 10 = external mic input*/
	kUnusedInput			=	0x00000800,	/*Bit 11 = unused input*/
	kInitialAwacsR0Value    	=	0x00000000	/*Initial value set on boot*/
};


enum AWACsCODEC_ControlRegister1Constants 
{
	kReservedR1Bit0			=	0x00000001,	/*Reserved - always set to zero*/
	kReservedR1Bit1			=	0x00000002,	/*Reserved - always set to zero*/
	kRecalibrate			=	0x00000004,	/*Recalibrate AWACs*/
	kExtraSampleRateMask    	=	0x00000038,	/*!!!Do these bits do anything???*/
	kLoopThruEnable			=	0x00000040,	/*Loop thru enable*/
	kMuteInternalSpeaker	        =	0x00000080,	/*Internal speaker mute*/
	kReservedR1Bit8			=	0x00000100,	/*Reserved - always set to zero*/
	kMuteHeadphone			=	0x00000200,	/*Headphone mute*/
        kOutputZero			=	0x00000400,
	kOutputOne			=	0x00000800,	/*Sometimes speaker enable*/
	kParallelOutputEnable		=	0x00000C00,	/*Parallel output port*/
	kInitialAwacsR1Value     	=	0x00000000	/*Initial value set on boot*/
};


enum AWACsCODEC_ControlRegister2Constants 
{
	kRightHeadphoneAttenMask	=	0x0000000F,	/*Attenuation for right headphone speaker*/
	kRightHeadphoneAttenShift	=	0,		/*Bit shift from LSB position*/
	kReservedR2Bit4			=	0x00000010,	/*Reserved - always set to zero*/
	kReservedR2Bit5			=	0x00000020,	/*Reserved - always set to zero*/
	kLeftHeadphoneAttenMask 	=	0x000003C0,	/*Attenuation for left headphone speaker*/
	kLeftHeadphoneAttenShift	=	6,		/*Bit shift from LSB position*/
	kReservedR2Bit10		=	0x00000400,	/*Reserved - always set to zero*/
	kReservedR2Bit11		=	0x00000800,	/*Reserved - always set to zero*/
	kHeadphoneAttenMask		=	(kRightHeadphoneAttenMask | kLeftHeadphoneAttenMask),
	kInitialAwacsR2Value	       	=	0x00000000	/*Initial value set on boot*/
};


enum AWACsCODEC_ControlRegister4Constants 
{
	kRightSpeakerAttenMask		=	0x0000000F,	/*Attenuation for right internal speaker*/
	kRightSpeakerAttenShift		=	0,	       	/*Bit shift from LSB position*/
	kReservedR4Bit4			=	0x00000010,	/*Reserved - always set to zero*/
	kReservedR4Bit5			=	0x00000020,	/*Reserved - always set to zero*/
	kLeftSpeakerAttenMask		=	0x000003C0,	/*Attenuation for left internal speaker*/
	kLeftSpeakerAttenShift		=	6,	       	/*Bit shift from LSB position*/
	kReservedR4Bit10		=	0x00000400,	/*Reserved - always set to zero*/
	kReservedR4Bit11		=	0x00000800,	/*Reserved - always set to zero*/
	kSpeakerAttenMask		=	(kRightSpeakerAttenMask | kLeftSpeakerAttenMask),
	kInitialAwacsR4Value		=	0x00000000	/*Initial value set on boot*/
};

/*---------------------- Screamer Extensions ----------------------------------------*/

enum AWACsVersions
{
	kAWACSMaxVersion		=	2
};

enum AWACsCODEC_ControlRegister5Constants 
{
	kRightLoopThruAttenMask		=	0x0000000F,
        kRightLoopThruAttenShift	=	0,
	kLeftLoopThruAttenMask		=	0x000003C0,
        kLeftLoopThruAttenShift         =       6
};

enum AWACsCODEC_ControlRegister6Constants
{
	kPowerModeDoze			=	0x00000001,
	kPowerModeIdle			= 	0x00000002,
	kMicPreampBoostEnable		=	0x00000004,
	kPCMCIASpeakerAttenMask		=       0x00000038,
        kPowerModeAnalogShutdown	=       0x00000040,
	kLittleDischarge		=       0x00000080,
        kBigDischarge  			=       0x00000100
};

enum AWACsCODEC_ControlRegister7Constants
{
	kReadBackEnable			= 	0x00000001,
	kReadBackRegisterMask		= 	0x0000000E,
	kReadBackRegisterShift		=	1
};

/*----------------- AWACs CODEC Status Register constants -------------------------------*/

enum AWACsCODEC_StatusRegisterConstants 
{
	kNotMicSense	        	=	0x00000001,	/*This bit is 0 when a mic(input) is plugged in*/
        kNotMicShift			=	0,
kLineInSense	          	=	0x00000002,	/*This bit is 1, when a line(input) is pluggen in*/
        kLineInShft			=	1,
        kAux1Sense			=       0x00000004,     /*This bit is 1, when something is plugged into the Whisper line-out */
	kAux1Shft			=	2,
	kHeadphoneSense	        	=	0x00000008,	/*This bit is 1 when headphone is plugged in*/
	kHeadphoneShft			=	3,
        kAllSense			= 	0x0000000F,
	kManufacturerIDMask	        =	0x00000F00,	/*AWACS chip manufacturer ID bits*/
	kManufacturerIDShft	        =	8,	       	/*Bits to shift right to get ID in LSB position*/
	kRevisionNumberMask	        =	0x0000F000,	/*AWACS chip revision bits*/
	kRevisionNumberShft     	=	12,	       	/*Bits to shift right to get rev in LSB position*/
	kAwacsErrorStatus	      	=	0x000F0000,	/*AWACS error status bits*/
	kOverflowRight                  =       0x00100000,
	kOverflowLeft                   =       0x00200000,
	kValidData                      =       0x00400000,
	kExtend                         =       0x00800000
};

/*--------------------- AWACs Sound Control register ------------------------------------*/
	
enum AWACsSoundControlRegisterConstants 
{
	kInSubFrameMask			=	0x0000000F,	/*All of the input subframe bits*/
	kInSubFrame0			=	0x00000001,	/*Input subframe 0*/
	kInSubFrame1			=	0x00000002,	/*Input subframe 1*/
	kInSubFrame2			=	0x00000004,	/*Input subframe 2*/
	kInSubFrame3			=	0x00000008,	/*Input subframe 3*/
	
	kOutSubFrameMask		= 	0x000000F0, 	/*All of the output subframe bits*/
	kOutSubFrame0			=	0x00000010,	/*Output subframe 0*/
	kOutSubFrame1			=	0x00000020,	/*Output subframe 1*/
	kOutSubFrame2			=	0x00000040,	/*Output subframe 2*/
	kOutSubFrame3			=	0x00000080, 	/*Output subframe 3*/
	
	kHWRateMask	       		=	0x00000700,	/*All of the hardware sampling rate bits*/
	kHWRate44100			=	0x00000000,	/*Hardware sampling bits for 44100 Hz*/
	kHWRate29400			=	0x00000100,	/*Hardware sampling bits for 29400 Hz*/
	kHWRate22050			=	0x00000200,	/*Hardware sampling bits for 22050 Hz*/
	kHWRate17640			=	0x00000300,	/*Hardware sampling bits for 17640 Hz*/
	kHWRate14700			=	0x00000400,	/*Hardware sampling bits for 14700 Hz*/
	kHWRate11025			=	0x00000500,	/*Hardware sampling bits for 11025 Hz*/
	kHWRate08820			=	0x00000600,	/*Hardware sampling bits for  8820 Hz*/
	kHWRate07350			=	0x00000700,	/*Hardware sampling bits for  7350 Hz*/

	kHWRateShift                    =       8,

	kAWACSError	       	 	=	0x00000800,	/*AWACs error indicator*/
	kPortChange	       	 	=	0x00001000,	/*Port change indicator*/
	kEnableErrInt		 	=	0x00002000,	/*Interrupt on error enable*/
	kEnablePortChangeInt 	        =	0x00004000,	/*Interrupt on port change enable*/
	
	kStatusSubFrmSel0	 	=	0x00000000,	/*Select subframe zero  status*/
	kStatusSubFrmSel1 	 	=	0x00008000,	/*Select subframe one   status*/
	kStatusSubFrmSel2 	 	=	0x00010000,	/*Select subframe twoo  status*/
	kStatusSubFrmSel3 	 	=	0x00018000	/*Select subframe three status*/
};

/*--------------------- AWACs Clipping Count register ------------------------------------*/

enum AWACsClippingCountRegisterConstants 
{
       kRightClippingCount              =       0x000000FF,
       kLeftClippingCount               =       0x0000FF00
};

