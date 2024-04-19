/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Interface definition for the Burgundy audio Controller
 *
 * HISTORY
 *
 */

#ifndef _PPCBURGUNDY_H
#define _PPCBURGUNDY_H

#include <IOKit/audio/IOAudioBus.h>

// Uncomment the following line to log the driver activity
//#define DEBUGMODE

struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

class PPCBurgundy : public IOAudioBus
{
    OSDeclareDefaultStructors(PPCBurgundy)

    // Controls for sound source and destination
    enum soundControls {
        kSpeakerMute = 0,
        kSpeakerVolLeft = 1,
        kSpeakerVolRight = 2,
        kHeadphonesMute = 3,
        kHeadphonesVolLeft = 4,
        kHeadphonesVolRight = 5,
        kMicrophoneMute = 6,
        kCDMute = 7,
        kMixerInVolLeft = 8,
        kMixerInVolRight = 9,
        kPassThruVolLeft = 10,
        kPassThruVolRight = 11,
        kPassThruMute = 12,
        kLineOutMute = 13,
        kNumControls
    };

    // machines that support PPCBurgundy:
    enum supportedHardware {
        kMachineNone = 0,
        kMachine9500,
        kMachineGenericGossamer,
        kMachineTypeSilk,
        kMachineTypeWallstreet,
        kMachineTypeiMac,
        kMachineTypeYosemite,
        kMachineTypeSawtooth,
        kMachineType101,
        kMachineTypeiBook,
        // Do not add anything after this:
        kMachineTypeUnknown
    };

    // We save here whatever machine type we find:
    int machineType;

    // Burgundy specific stuff:
    bool          			bOutputActive;
    bool         			bInputActive;
    volatile UInt16  			intsPending;
    UInt16           			intsSent;
    UInt16           			intsReceived;
    UInt16           			intsDelivered;

    // This registry entry contains a lot of useful informations about the
    // way burgundy is wired:
    IORegistryEntry *sound;

    // This is the base of the burgundy registers:
    UInt8                              *ioBaseBurgundy;

    bool				isBordeaux;
    UInt16                    		recsrc;
    UInt32				fBlockSize;
    UInt32				fBufMax;
    
    // Register Mirrors:
    UInt32               		soundControlRegister;
    UInt32                		CodecControlRegister[8];
    UInt32				currentOutputMuteReg;
    UInt32               		lastStatusRegister;

    // for each line (in input and output) we have an audio component:
    typedef IOAudioComponentImpl *IOAudioComponentImplPtr;
    UInt16				numInputComponents;
    UInt16				numOutputComponents;
    IOAudioComponentImplPtr		*fInputComponents;
    IOAudioComponentImplPtr		*fOutputComponents;

    // There is a set of components that depend on each other (e.g. speaker
    // and headphones) the following map helps to always retrive the right
    // component.
    enum standardInputComponents {
        kMicroPhone = 0,
        kCD = 1,
        kLineIn = 2,
        kUndefinedInput
    };

    enum standardOutputComponents {
        kSpeaker = 0,
        kHeadphones = 1,
        kLineOut = 2,
        kUndefinedOutput
    };
    
protected:
    // Discovers if this is a burgundy compatible device:
    bool implementsBurgundy(IORegistryEntry*);

    // Finds the hardware the driver is running in:
    int findHostingHardware(IOService*);

    // Finds the number of DMA channels and their direction:
    int numOfDMAChannels();

    // finds the number of components for the
    // input and output lines:
    int numberOfOutputComponents();
    int numberOfInputComponents();

    // returns the frame rate fpr the input and output
    // channels. Possibly reading them from the registry
    UInt16 inputFrameRate();
    UInt16 outputFrameRate();

    // Acts on changes of the status register:
    void checkStatusRegister();
    bool inputComponentStatus(int);
    bool outputComponentStatus(int);

    // this handles the setup of the burgundy chip for each kind of
    // hosting hardware.
    bool dependentSetup();
    
    // Returns true if some sense line changed in the
    // status register since the last check:
    bool burgundyStatusChanged();
    
    // A mute function for each supported mute line:
    void muteInternalSpeaker(bool);
    void muteRCAOutput(bool);
    void muteHeadphones(bool);
    void muteCDLine(bool);

    // Sets the microphone as input source:
    void setMicInput(bool);

    // Scale the volume to the range used by burgundy:
    UInt8 scaleVolume(int);

    // sets the volume for the output lines:
    void volumeInternalSpeakerLeft(int);
    void volumeInternalSpeakerRight(int);
    
    void volumeRCAOutputLeft(int);
    void volumeRCAOutputRight(int);
    
    void volumeHeadphonesLeft(int);
    void volumeHeadphonesRight(int);
    
    void volumeCDLineLeft(int);
    void volumeCDLineRight(int);
   
    // Sets the volume of all the above together
    void volumeAllRight(int);
    void volumeAllLeft(int);
    
    // Sets the gain for the input line:
    void volumeMixerInLeft(int);
    void volumeMixerInRight(int);
    
    // A sense function for each "jacked" input:
    bool headphonesInserted();
    bool microphoneInserted();

    // they return the strings for the component dictionaries
    // for each kind of component
    char* componentDictionarySpeaker();
    char* componentDictionaryHeadphones();
    char* componentDictionaryMicrophone();
    char* componentDictionaryLineout();
    char* componentDictionaryCD();
    char* componentDictionaryMixerIn();
    char* componentDictionaryMixerOut();
    char* componentDictionaryPassThru();
    char* componentDictionaryLinein();

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

#endif /* !_PPCBURGUNDY_H */
