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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Interface definition for the DAC 3550A audio Controller
 *
 * HISTORY
 *
 */

#ifndef _PPCDACA_H
#define _PPCDACA_H

#include <IOKit/audio/IOAudioBus.h>

#include "PPCI2CInterface.h"

// Uncomment the following line to log the driver activity
//#define DEBUGMODE

// If this is uncommented we increase the number of checkpoints
// for data consistency:
//#define BEPARANOID

struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

class PPCDACA : public IOAudioBus
{
    OSDeclareDefaultStructors(PPCDACA)
    
private:
    // Controls for sound source and destination
    enum soundControls {
        kSpeakerMute = 0,
        kSpeakerVolLeft = 1,
        kSpeakerVolRight = 2,
        kHeadphonesMute = 3,
        kHeadphonesVolLeft = 4,
        kHeadphonesVolRight = 5,
        kModemMute = 6,
        kCDMute = 7,
        kMixerInVolLeft = 8,
        kMixerInVolRight = 9,
        kPassThruVolLeft = 10,
        kPassThruVolRight = 11,
        kPassThruMute = 12,
        kNumControls
    };

    // There is a set of components that depend on each other (e.g. speaker
    // and headphones) the following map helps to always retrive the right
    // component.
    enum standardInputComponents {
        kCD = 0,
        kModem = 1,
        kUndefinedInput
    };

    enum standardOutputComponents {
        kSpeaker = 0,
        kHeadphones = 1,
        kUndefinedOutput
    };

    // Characteristic constants:
    typedef enum TicksPerFrame {
        k64TicksPerFrame		= 64,			// 64 ticks per frame
        k32TicksPerFrame		= 32 			// 32 ticks per frame
    } TicksPerFrame;
    
    typedef enum ClockSource {
            kClock49MHz				= 49152000,		// 49 MHz clock source
            kClock45MHz				= 45158400,		// 45 MHz clock source
            kClock18MHz				= 18432000		 // 18 MHz clock source
    } ClockSource;

    // Sound Formats:
    // FIXME: these values are "interpreted" and mirrored in specific chip values
    // so wouldn't be better to have them in some parent class?
    typedef enum SoundFormat {
        kSndIOFormatI2SSony,
        kSndIOFormatI2S64x,
        kSndIOFormatI2S32x,

        // This says "we never decided for a sound format before"
        kSndIOFormatUnknown
    } SoundFormat;

    // *************************************
    // * DACA Mirror Regs  Member Function *
    // *************************************
    UInt8  sampleRateReg;
    UInt16 analogVolumeReg;
    UInt8  configurationReg;

    // This is to handle the volumes for the two output
    UInt16 internalSpeakerVolume;
    UInt16 internalHeadphonesVolume;

    // These handle the mute states:
    bool internalSpeakerMuted;
    bool internalHeadphonesMuted;

    // ****************************
    // * Generic  Member Function *
    // ****************************
    UInt32 ReadWordLittleEndian(void *address,UInt32 offset);
    void WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value);

    // *********************************
    // * I 2 S  DATA & Member Function *
    // *********************************
    void *soundConfigSpace;        // address of sound config space
    void *ioBaseAddress;           // base address of our I/O controller
    void *ioClockBaseAddress;	   // base address for the clock
    void *ioStatusRegister_GPIO12; // the register with the input detection

    // Recalls which i2s interface we are attached to:
    UInt8 i2SInterfaceNumber;
    
    // Access to all the I2S registers:
    // -------------------------------
    UInt32 I2SGetIntCtlReg();
    void   I2SSetIntCtlReg(UInt32 value);
    UInt32 I2SGetSerialFormatReg();
    void   I2SSetSerialFormatReg(UInt32 value);
    UInt32 I2SGetCodecMsgOutReg();
    void   I2SSetCodecMsgOutReg(UInt32 value);
    UInt32 I2SGetCodecMsgInReg();
    void   I2SSetCodecMsgInReg(UInt32 value);
    UInt32 I2SGetFrameCountReg();
    void   I2SSetFrameCountReg(UInt32 value);
    UInt32 I2SGetFrameMatchReg();
    void   I2SSetFrameMatchReg(UInt32 value);
    UInt32 I2SGetDataWordSizesReg();
    void   I2SSetDataWordSizesReg(UInt32 value);
    UInt32 I2SGetPeakLevelSelReg();
    void   I2SSetPeakLevelSelReg(UInt32 value);
    UInt32 I2SGetPeakLevelIn0Reg();
    void   I2SSetPeakLevelIn0Reg(UInt32 value);
    UInt32 I2SGetPeakLevelIn1Reg();
    void   I2SSetPeakLevelIn1Reg(UInt32 value);
    UInt32 I2SCounterReg();

    // starts and stops the clock count:
    void   KLSetRegister(void *klRegister, UInt32 value);
    UInt32   KLGetRegister(void *klRegister);
    bool clockRun(bool start);

    // Enables and disables the interrupts:
    bool enableInterrupts();
    bool disableInterrupts();
        
    // Define the sound parameters for the adac:
    bool setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio);
    bool setDACAVolume(UInt16 volume, bool isLeft);
    void setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat);

    // *********************************
    // * I 2 C  DATA & Member Function *
    // *********************************

    // This provides access to the DACA registers:
    PPCI2CInterface *interface;

    // Attaches to the i2c interface:
    bool findAndAttachI2C(IOService *provider);
    bool detachFromI2C(IOService *provider);

    // sets the sample rate:
    bool setDACASampleRate(UInt);
    
    // Access o the i2c registers:
    bool writeRegisterBits(UInt8 subAddress, UInt32 bitMaskOn, UInt32 bitMaskOff);
    
    // for each line (in input and output) we have an audio component:
    typedef IOAudioComponentImpl *IOAudioComponentImplPtr;
    UInt16				numOutputComponents;
    IOAudioComponentImplPtr		*fOutputComponents;

    // Remember the provider
    IORegistryEntry *sound;

    // holds the current frame rate settings:
    ClockSource dacaClockSource;
    UInt32      dacaMclkDivisor;
    UInt32      dacaSclkDivisor;
    SoundFormat dacaSerialFormat;

    // Remebers the last value of the status register
    UInt8	lastStatus;
    
protected:
    // Discovers if this is a DAC-3550 compatible device:
    bool implementsDACA(IORegistryEntry*);

    // Finds the number of DMA channels and their direction:
    int numOfDMAChannels();

    // finds the number of components for the
    // input and output lines:
    int numberOfOutputComponents();
    int numberOfInputComponents();

    // returns the frame rate or the input and output
    // channels. Possibly reading them from the registry
    UInt32 frameRate(UInt32);

    // Finds the i2c port to use for the audio chip
    UInt32 getI2CPort();
    
    // Acts on changes of the status register:
    void checkStatusRegister();
    bool inputComponentStatus(int);
    bool outputComponentStatus(int);

    // this handles the setup of the DAC-3550 chip for each kind of
    // hosting hardware.
    bool dependentSetup();

    // Returns true if some sense line changed in the
    // status register since the last check:
    bool statusChanged();

    // A mute function for each supported mute line:
    void muteInternalSpeaker(bool);
    void muteHeadphones(bool);
    void muteCDLine(bool);

    // Sets the modem as input source:
    void setModemInput(bool);

    // sets the volume for the output lines:
    void volumeInternalSpeakerLeft(int);
    void volumeInternalSpeakerRight(int);
        
    void volumeHeadphonesLeft(int);
    void volumeHeadphonesRight(int);
    
    void volumeCDLineLeft(int);
    void volumeCDLineRight(int);
       
    // Sets the gain for the input line:
    void volumeMixerInLeft(int);
    void volumeMixerInRight(int);
    
    // A sense function for each "jacked" input:
    bool headphonesInserted();

    // they return the strings for the component dictionaries
    // for each kind of component
    char* componentDictionarySpeaker();
    char* componentDictionaryHeadphones();
    char* componentDictionaryModem();
    char* componentDictionaryCD();
    char* componentDictionaryMixerIn();
    char* componentDictionaryMixerOut();
    char* componentDictionaryPassThru();

public:
    virtual bool init(OSDictionary *properties = 0);
    virtual void free();
    virtual IOService* probe(IOService *provider, SInt32*);
    virtual bool start(IOService*);
    virtual void stop(IOService*);
    virtual void DoClockTick(IOTimerEventSource*);
    virtual void calculateTickInterval(AbsoluteTime *tickInterval);
    
    // These are all the functions we need to override:
    virtual IOReturn SetControl(UInt16, int);
    virtual void CreateAudioTopology(class IOCommandQueue *);
};

#endif /* !_PPCDACA_H */
