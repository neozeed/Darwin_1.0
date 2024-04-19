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
 * Hardware independent (relatively) code for the Awacs Controller 
 *
 * HISTORY
 *
 * 14-Jan-1999	 
 *	Created.
 *
 */
#include <IOKit/assert.h>
#include <IOKit/system.h>

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/ppc/IODBDMA.h>
#include "PPCAwacs.h"
#include "awacs_hw.h"

/*
 * From Excelsior:Toolbox:SoundMgr:Hardware:Ports:OutPorts.cp & GossamerOut.h
 */
#define kSGS7433Addr 0x8A
#define kSGSNumRegs 7
unsigned char SGSShadow[kSGSNumRegs] =
                                        { 0x09,		/* Adr:1, AutoIncr 				*/
                                          0x20, 	/* Reg 1: Vol  = 0dB				*/
                                          0xFF, 	/* Reg 2: Bass = 0dB, Treble = 0dB		*/
                                          0x00, 	/* Reg 3: Internal Spkr - Left  Atten = 0dB	*/
                                          0x0A,         /* Reg 4: Line Out	- Left  Atten = -10dB	*/
                                          0x00,         /* Reg 5: Internal Spkr - Right Atten = 0dB	*/
                                          0x0A };	/* Reg 6: Line Out	- Right Atten = -10dB   */
#define	sgsBalMuteBit		0x020	// When this bit is set in the output fader/ balance regs, the channel is muted
#define	sgsBalVolBits		0x1F	// mask for volume steps
#define	kLFAttnReg		0x03	// Left Front (ie everything except the rear spkr jack)
#define	kRFAttnReg		0x05	// Right Front
#define	kLRAttnReg		0x04	// Left Rear (the rear speaker jack only)
#define	kRRAttnReg		0x06	// Right Rear


// "deivce-id" values come from the sound block in Open Firmware on B&W G3's and later
const u_int32_t kG4WithAwacsDeviceID = 5;
const u_int32_t kiMacDVDeviceID      = 8;
 
/*
 *
 */

static void 	writeCodecControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value );
static void 	writeSoundControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value );
static u_int32_t readCodecStatusReg( volatile awacs_regmap_t *ioBaseAwacs );
static int 	readClippingCountReg( volatile awacs_regmap_t *ioBaseAwacs );

static void writeCodecControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value )
{
  int          CodecControlReg;

#if 0
      IOLog( "PPCSound(awacs): CodecControlReg @ %08x = %08x\n\r", (int)&ioBaseAwacs->CodecControlRegister, value);
#endif

  OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, value );
  eieio();

  do
    {
      CodecControlReg =  OSReadLittleInt32( &ioBaseAwacs->CodecControlRegister, 0 );
      eieio();
    }
  while ( CodecControlReg & kCodecCtlBusy );
}


static void writeSoundControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value )
{

#if 0
      IOLog( "PPCSound(awacs): SoundControlReg = %08x\n\r", value);
#endif

  OSWriteLittleInt32( &ioBaseAwacs->SoundControlRegister, 0, value );
  eieio();
}

static u_int32_t readCodecStatusReg( volatile awacs_regmap_t *ioBaseAwacs )
{
  return OSReadLittleInt32( &ioBaseAwacs->CodecStatusRegister, 0 );
}

static int readClippingCountReg( volatile awacs_regmap_t *ioBaseAwacs )
{
  return OSReadLittleInt32( &ioBaseAwacs->ClippingCountRegister, 0 );
}

static void writeSGSRegs()
{
    int i;
    for (i = 0; i < kSGSNumRegs; i++) {
        (*PE_write_IIC)(kSGS7433Addr, i, SGSShadow[i]);
    }
}

#define super IOAudioBus

OSDefineMetaClassAndStructors( PPCAwacs, IOAudioBus )

/*
 * Public Instance Methods
 */
bool PPCAwacs::init(OSDictionary * properties)
{
    if (!super::init(properties))
            return false;
    if(!properties)
	return false;	// Need to know where Awacs is!

    return true;
}

void PPCAwacs::free()
{
    super::free();
}

bool PPCAwacs::start(IOService * provider)
{
    IOMemoryMap *	map;
    u_int32_t		awacsVersion;
    IORegistryEntry *	perch = NULL;
    IORegistryEntry *	sound = NULL;

    if( !super::start(provider))
        return (false);

    map = provider->mapDeviceMemoryWithIndex(kAudioDMAdeviceInt);
    if(!map) {
        return false;
    }
    fIOBaseAwacs = (awacs_regmap_t *)map->getVirtualAddress();

    if (!AllocateStreams(kNumDMAStreams))
        return false;

    map = provider->mapDeviceMemoryWithIndex(kAudioDMAtxInt);
    if(!map) {
        return false;
    }
    DefineStream(kAudioDMAOutputStream, kOutput, 44100L, (IODBDMAChannelRegisters*)map->getVirtualAddress());

    map = provider->mapDeviceMemoryWithIndex(kAudioDMArxInt);
    if(!map) {
        return false;
    }
    DefineStream(kAudioDMAInputStream, kInput, 44100L, (IODBDMAChannelRegisters*)map->getVirtualAddress());

    // Dig out the sound nub that's below our match
    fDetects = 2;	// Mic jack and lineout jack
    fOutputs = 2;	// internal speaker and lineout

    fIsG4WithAwacs = FALSE;
    fIsiMacDVWithAwacs = FALSE;

    sound = provider->childFromPath("sound", gIODTPlane);
    if(sound) {
        OSData *t;
        t = OSDynamicCast(OSData, sound->getProperty("#-detects"));
	if(t) {
            fDetects = *(UInt32*)(t->getBytesNoCopy());
	}
        t = OSDynamicCast(OSData, sound->getProperty("#-outputs"));
        if(t) {
            fOutputs = *(UInt32*)(t->getBytesNoCopy());
        }

#if 1
    IOLog( "PPCAwacs::start: about to get device-id\n" );
#endif
        // Check for G4WithAwacs & iMacDVWithAwacs
        // Note: "device-id" found on New World machines and later (B&W G3 is first such box)
        t = OSDynamicCast( OSData, sound->getProperty( "device-id" ) );
        if( t ) {
            u_int32_t deviceID = *(UInt32 *)( t->getBytesNoCopy() );
#if 1
    IOLog( "PPCAwacs::start: got deviceID = %d\n", deviceID );
#endif
            switch ( deviceID )
            {
                case kG4WithAwacsDeviceID:
                    fIsG4WithAwacs = TRUE;
                    break;
                case kiMacDVDeviceID:
                    fIsiMacDVWithAwacs = TRUE;
                    break;
                default: ;// NOP         
            }
        }
    }

    // Debugging aids
    OSNumber *num = OSNumber::withNumber(fDetects, 32);
    setProperty("Detects", num);
    num->release();
    num = OSNumber::withNumber(fOutputs, 32);
    setProperty("Outputs", num);
    num->release();

    perch = IORegistryEntry::fromPath("/perch", gIODTPlane );
    fIICAudioDevPresent = perch != NULL;
    if(fIICAudioDevPresent) {
        waitForService( resourceMatching( "IOiic0" ));
    }
    fCodecStatus = readCodecStatusReg( fIOBaseAwacs );
    awacsVersion = (fCodecStatus & kRevisionNumberMask) >> kRevisionNumberShft;

    /*
     * Mask out the input bits in the cached status register so that the AudioComponents
     * get updated to the correct values the first time we update the device status
     * (Everything is initialized on the assumption that nothing is plugged in)
     */
    fCodecStatus &= ~kAllSense;

    if ( awacsVersion > kAWACSMaxVersion ) {
        fCodecControlRegister[5] = kCodecCtlAddress5 | kCodecCtlEMSelect0;
        writeCodecControlReg(  fIOBaseAwacs, fCodecControlRegister[5] );
        fCodecControlRegister[6] = kCodecCtlAddress6 | kCodecCtlEMSelect0;
        writeCodecControlReg(  fIOBaseAwacs, fCodecControlRegister[6] );
    }

    if ( fIICAudioDevPresent ) {
	writeSGSRegs();
    }

    fSoundControlRegister = ( kInSubFrame0      |
                              kOutSubFrame0     |
                              kHWRate44100        );
    writeSoundControlReg( fIOBaseAwacs, fSoundControlRegister);

    fCodecControlRegister[0] = kCodecCtlAddress0 | kCodecCtlEMSelect0;
    fCodecControlRegister[1] = kCodecCtlAddress1 | kCodecCtlEMSelect0;
    fCodecControlRegister[2] = kCodecCtlAddress2 | kCodecCtlEMSelect0;
    fCodecControlRegister[4] = kCodecCtlAddress4 | kCodecCtlEMSelect0;

    fCodecControlRegister[0] |= (kMicInput | kDefaultMicGain);

    // Gossamer passes sound right through to be later controlled
    // by the SGS audio processor--turn on these pass-thru ports.
    if ( fIICAudioDevPresent || fIsG4WithAwacs ) {
        fCodecControlRegister[1] |= (kParallelOutputEnable);
    }

    writeCodecControlReg(  fIOBaseAwacs, fCodecControlRegister[0] );
    writeCodecControlReg(  fIOBaseAwacs, fCodecControlRegister[1] );
    writeCodecControlReg(  fIOBaseAwacs, fCodecControlRegister[2] );
    writeCodecControlReg(  fIOBaseAwacs, fCodecControlRegister[4] );

    startWorkLoop();

    if(perch)
	perch->release();
    if(sound)
	sound->release();
    return true;
}


void PPCAwacs::stop(IOService *provider)
{
    super::stop(provider);
    FreeStreams();
}

void PPCAwacs::CreateAudioTopology(IOCommandQueue *queue)
{
/*
 * These should be resources in the loadable driver
 */
static const char *sSpeaker =
"{
    'Type' = 'Speaker';
    'Channels' = 2:8;
    'Master' = 1:8;
    'Controls' = {   
	'MuteAll' = {
            'Type' = 'Mute';
            'Id' = 0:16;
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
            'Chan' = 0:8;
        };
	'VolumeLeft' = {
            'Type' = 'Volume';
            'Id' = 1:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 1:8;
        };
        'VolumeRight' = {
            'Type' = 'Volume';
            'Id' = 2:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 2:8;
        };
    };
}";

static const char *sHeadphones =
"{
    'Type' = 'Headphones';
    'Channels' = 2:8;
    'Master' = 1:8;
    'Controls' = {   
	'MuteAll' = {
            'Type' = 'Mute';
            'Id' = 3:16;
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
            'Chan' = 0:8;
        };
	'VolumeLeft' = {
            'Type' = 'Volume';
            'Id' = 4:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 1:8;
        };
        'VolumeRight' = {
            'Type' = 'Volume';
            'Id' = 5:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 2:8;
        };
    };
    'Inputs' = {
        'Jack' = {
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
	};
    };
}";

static const char *sMicrophone =
"{
    'Type' = 'Microphone';
    'Channels' = 2:8;
    'Controls' = {   
	'MuteAll' = {
            'Type' = 'Mute';
            'Id' = 6:16;
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
            'Chan' = 0:8;
        };
    };
    'Inputs' = {
        'Jack' = {
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
	};
    };
}";

static const char *sLineout =
"{
    'Type' = 'LineOut';
    'Channels' = 2:8;
    'Controls' = {   
	'MuteAll' = {
            'Type' = 'Mute';
            'Id' = 13:16;
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
            'Chan' = 0:8;
        };
    };
    'Inputs' = {
        'Jack' = {
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
	};
    };
}";

static const char *sCD =
"{
    'Type' = 'CD';
    'Channels' = 2:8;
    'Controls' = {   
	'MuteAll' = {
            'Type' = 'Mute';
            'Id' = 7:16;
            'Val' = 0:8;
            'Min' = 0:8;
            'Max' = 1:8;
            'Chan' = 0:8;
        };
    };
}";

static const char *sMixerIn =
"{
    'Type' = 'Mixer';
    'Channels' = 2:8;
    'Controls' = {   
	'VolumeLeft' = {
            'Type' = 'Volume';
            'Id' = 8:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 1:8;
        };
        'VolumeRight' = {
            'Type' = 'Volume';
            'Id' = 9:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 2:8;
        };
    };
}";

static const char *sPassThru = 
"{
    'Type' = 'Feature';
    'Channels' = 2:8;
    'Controls' = {   
	'MuteAll' = {
            'Type' = 'Mute';
            'Id' = 12:16;
            'Val' = 1:8;
            'Min' = 0:8;
            'Max' = 1:8;
            'Chan' = 0:8;
        };
	'VolumeLeft' = {
            'Type' = 'Volume';
            'Id' = 10:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 1:8;
        };
        'VolumeRight' = {
            'Type' = 'Volume';
            'Id' = 11:16;
            'Val' = 65535:16;
            'Min' = 0:16;
            'Max' = 65535:16;
            'Chan' = 2:8;
        };
    };
}";

static const char *sMixerOut =
"{
    'Type' = 'Mixer';
    'Channels' = 2:8;
}";

    IOAudioComponentImpl *mixerIn;
    IOAudioComponentImpl *mixerOut;
    IOAudioComponentImpl *microphone;
    IOAudioComponentImpl *lineout;
    IOAudioComponentImpl *headphones;
    AudioStreamIndex outputStream;
    AudioStreamIndex inputStream;

    if (!fStreams)
         return;

    outputStream = firstStreamAfter(kOutput, 0);
    inputStream = firstStreamAfter(kInput, 0);
    if ((outputStream == kInvalidStreamIndex) || (inputStream == kInvalidStreamIndex)) {
       IOLog("PPCAwacs::CreateAudioTopology called without available DMA streams\n");
        return;
    }
    else
       IOLog("PPCAwacs::CreateAudioTopology input stream is %d output is %d\n", inputStream, outputStream);

    mixerOut = buildComponentAndAttach(GetStream(outputStream), NULL, sMixerOut, queue);
    headphones = buildComponentAndAttach(mixerOut, NULL, sHeadphones, queue);
    buildComponentAndAttach(mixerOut, NULL, sSpeaker, queue);
    microphone = buildComponentAndAttach(NULL, NULL, sMicrophone, queue);
    mixerIn = buildComponentAndAttach(microphone, GetStream(inputStream), sMixerIn, queue);
    buildComponentAndAttach(mixerIn, mixerOut, sPassThru, queue);
    buildComponentAndAttach(NULL, mixerIn, sCD, queue);

    /*
     * This stuff should perhaps be table-driven, depending on how much it varies
     * between different Mac models that use versions of Awacs wired up in different ways
     *
     * 9500: 		Speaker Out jack = kHeadphoneSense
     *	 		Microphone jack = kAux1Sense
     *
     * AllInOne:	Line Out jack = kAux1Sense
     *			Microphone jack = kLineInSense
     *			Either Headphone jack = kHeadphoneSense
     *
     * Beige G3:	Line Out jack = kAux1Sense
     *			Microphone jack = kLineInSense
     * 		
     * B&W G3:		Line Out jack = kAux1Sense
     *			Microphone jack = kLineInSense
     *
     * A big problem is how to tell the difference between AllInOne and Beige G3,
     * and perhaps iMac and B&W G3
     */
    if ( fIICAudioDevPresent ) {
        fInputComponents[kLineInShft] = microphone;
        if(fOutputs == 3 && fDetects == 3) {
            // CPU has lineout and headphone jacks
            lineout = buildComponentAndAttach(mixerOut, NULL, sLineout, queue);
            fInputComponents[kAux1Shft] = lineout;
            fInputComponents[kHeadphoneShft] = headphones;
         }
         else {
             fInputComponents[kAux1Shft] = headphones;
         }
    }
    else if ( fIsG4WithAwacs ) {
        fInputComponents[kLineInShft] = microphone;
        fInputComponents[kAux1Shft] = headphones;    
    }
    else if ( fIsiMacDVWithAwacs ) {
#if 1
        IOLog( "PPCAwacs::CreateAudioTopology: fIsiMacDVWithAwacs\n" );
#endif
        // The DV iMac has 2 headphones outputs, 1 line out, and 1 microphone
        // The names of the bits do not match how they are actually wired :-)
        lineout = buildComponentAndAttach(mixerOut, NULL, sLineout, queue);
        fInputComponents[kNotMicShift]   = lineout;
        fInputComponents[kLineInShft]    = headphones;
        fInputComponents[kAux1Shft]      = headphones;
        fInputComponents[kHeadphoneShft] = microphone;
    }
    else {
        fInputComponents[kAux1Shft] = microphone;
        fInputComponents[kHeadphoneShft] = headphones;
    }
    // Update state of all inputs now that all the bookkeeping objects are created.
    checkStatus();
}

void PPCAwacs::DoClockTick(IOTimerEventSource *t)
{
    checkStatus();
    super::DoClockTick(t);
}

void PPCAwacs::calculateTickInterval(AbsoluteTime *tickInterval)
{
    AbsoluteTime maxInterval;

    // We want to check the status at least one a second
    nanoseconds_to_absolutetime(NSEC_PER_SEC, &maxInterval);
    if(CMP_ABSOLUTETIME(&maxInterval, tickInterval) < 0) {
        *tickInterval = maxInterval;
    }
    super::calculateTickInterval(tickInterval);
}

void PPCAwacs::checkStatus()
{
    u_int32_t       codecStatus;

    codecStatus = readCodecStatusReg( fIOBaseAwacs );

    if(fCodecStatus != codecStatus) {
        int i;
					// Debugging aids
    	//kprintf("Awacs status: 0x%x\n", codecStatus);
#if 1
        IOLog( "PPCAwacs::checkStatusAwacs status: 0x%x\n", codecStatus );
#endif
        OSNumber * num = OSNumber::withNumber(codecStatus, 32);
        setProperty("Status", num);
        num->release();

	for(i=0; i<4; i++) {
            if( (fCodecStatus & (1<<i)) != (codecStatus & (1<<i)))
                if(fInputComponents[i]) {
                    fInputComponents[i]->Set(gInputsSym, gJackSym,
                        (codecStatus & (1<<i)) >> i);
                }
	}
    }
    fCodecStatus = codecStatus;
}

IOReturn
PPCAwacs::SetControl(UInt16 id, int val)
{
    IOReturn res = kIOReturnSuccess;
    switch(id) {
        case kSpeakerMute:
            if(val)
            	fCodecControlRegister[1] |= kMuteInternalSpeaker;
            else
            	fCodecControlRegister[1] &= ~kMuteInternalSpeaker;
            if ( fIICAudioDevPresent ) {
#if 0
                /*
                 * Mute the left and right front channels
                 */
                SGSShadow[kLFAttnReg] &= ~sgsBalMuteBit;
                SGSShadow[kRFAttnReg] &= ~sgsBalMuteBit;

                if ( val ) {
                    SGSShadow[kLFAttnReg] |= sgsBalMuteBit;
                    SGSShadow[kRFAttnReg] |= sgsBalMuteBit;
                }
                writeSGSRegs();

#else
                if(val)
                    fCodecControlRegister[1] &= ~kOutputOne;
                else
                    fCodecControlRegister[1] |= kOutputOne;
#endif
            }
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[1] );

            break;

        case kSpeakerVolLeft:
            if ( fIICAudioDevPresent ) {
                // Actually sets volume of speaker and front headphone
                val = 31 - ((val * 32 / 65536) & 31);
                SGSShadow[kLFAttnReg] = (SGSShadow[kLFAttnReg] & sgsBalMuteBit) | val;
                writeSGSRegs();
            }
            else {
                val = 15 - ((val * 16 / 65536) & 15);
                fCodecControlRegister[4] = (fCodecControlRegister[4] & ~kLeftSpeakerAttenMask) |
                            (val << kLeftSpeakerAttenShift);
                writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[4] );
            }

            break;

        case kSpeakerVolRight:
            if ( fIICAudioDevPresent ) {
                // Actually sets volume of speaker and front headphone
                val = 31 - ((val * 32 / 65536) & 31);
                SGSShadow[kRFAttnReg] = (SGSShadow[kRFAttnReg] & sgsBalMuteBit) | val;
                writeSGSRegs();
            }
            else {
                val = 15 - ((val * 16 / 65536) & 15);
                fCodecControlRegister[4] = (fCodecControlRegister[4] & ~kRightSpeakerAttenMask) |
                            (val << kRightSpeakerAttenShift);
                writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[4] );
            }
            break;

        case kHeadphonesMute:
            if(val)
            	fCodecControlRegister[1] |= kMuteHeadphone;
            else
            	fCodecControlRegister[1] &= ~kMuteHeadphone;
            if ( fIICAudioDevPresent ) {
                if(val)
                    fCodecControlRegister[1] &= ~kOutputOne;
                else
                    fCodecControlRegister[1] |= kOutputOne;
            }
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[1] );

            break;

        case kHeadphonesVolLeft:
            val = 15 - ((val * 16 / 65536) & 15);
            fCodecControlRegister[2] = (fCodecControlRegister[2] & ~kLeftHeadphoneAttenMask) |
                        (val << kLeftHeadphoneAttenShift);
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[2] );
            break;

        case kHeadphonesVolRight:
            val = 15 - ((val * 16 / 65536) & 15);
            fCodecControlRegister[2] = (fCodecControlRegister[2] & ~kRightHeadphoneAttenMask) |
                        (val << kRightHeadphoneAttenShift);
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[2] );
            break;

	case kMicrophoneMute:
            if(val)
            	fCodecControlRegister[0] &= ~kMicInput;
            else
            	fCodecControlRegister[0] |= kMicInput;
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[0] );
            break;

	case kCDMute:
            if(val)
            	fCodecControlRegister[0] &= ~kCDInput;
            else
            	fCodecControlRegister[0] |= kCDInput;
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[0] );
            break;

        case kMixerInVolLeft:
            val = (val * 16 / 65536) & 15;
            fCodecControlRegister[0] = (fCodecControlRegister[0] & ~kLeftInputGainMask) |
                        (val << kLeftInputGainShift);
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[0] );
            break;

        case kMixerInVolRight:
            val = (val * 16 / 65536) & 15;
            fCodecControlRegister[0] = (fCodecControlRegister[0] & ~kRightInputGainMask) |
                        (val << kRightInputGainShift);
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[0] );
            break;

        case kPassThruVolLeft:
            val = 15 - ((val * 16 / 65536) & 15);
            fCodecControlRegister[5] = (fCodecControlRegister[5] & ~kLeftLoopThruAttenMask) |
                        (val << kLeftLoopThruAttenShift);
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[5] );
            break;

        case kPassThruVolRight:
            val = 15 - ((val * 16 / 65536) & 15);
            fCodecControlRegister[5] = (fCodecControlRegister[5] & ~kRightLoopThruAttenMask) |
                        (val << kRightLoopThruAttenShift);
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[5] );
            break;

	case kPassThruMute:
            if(val)
            	fCodecControlRegister[1] &= ~kLoopThruEnable;
            else
            	fCodecControlRegister[1] |= kLoopThruEnable;
            writeCodecControlReg( fIOBaseAwacs, fCodecControlRegister[1] );
            break;


        default:
            res = kIOReturnUnsupported;
    }
    return res;
}

