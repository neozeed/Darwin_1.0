/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * Hardware independent (relatively) code for the Burgundy Controller 
 *
 * HISTORY
 *
 *
 */
#include <IOKit/assert.h>
#include <IOKit/system.h>

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/ppc/IODBDMA.h>
#include "PPCBurgundy.h"
#include "burgundy_hw.h"

/*
 * Prototyes for the "very private methods" at the end of this
 * file. They provide access to the burgundy registers:
 */
static void writeSoundControlReg( volatile UInt8*, int);
static int readCodecSenseLines( volatile UInt8*);
static int readCodecReg( volatile UInt8*, int);
static void writeCodecReg( volatile UInt8*, int, int);

#define super IOAudioBus
OSDefineMetaClassAndStructors( PPCBurgundy, IOAudioBus )

/* ==============
 * Public Methods
 * ============== */
bool
PPCBurgundy::init(OSDictionary * properties)
{
    if (!super::init(properties)) {
#ifdef DEBUGMODE
        IOLog( "PPCBurgundy::init (this = 0x%08lx) super fails\n", (UInt32)this);
#endif
        return false;
    }
 
    if(!properties){
#ifdef DEBUGMODE
        IOLog( "PPCBurgundy::init (this = 0x%08lx) Need to know where Burgundy is !!\n", (UInt32)this);
#endif
        return false;  // Need to know where Burgundy is!
    }
	
    // This invalidate the cache of the machine architecture
    machineType = kMachineTypeUnknown;

    // Nulls the pointers for the arrays we allocate:
    fInputComponents = NULL;
    fOutputComponents = NULL;
    sound = NULL;

    // We start with a clean status register mirror so first read will
    // set the driver in the correct mode:
    lastStatusRegister = 0L;

#ifdef DEBUGMODE
    IOLog( "PPCBurgundy::init (this = 0x%08lx)\n", (UInt32)this);
#endif
    return true;
}

void
PPCBurgundy::free()
{
    // Releases the sound:
    if(sound != NULL)
        sound->release();
    sound = NULL;

    super::free();
}

IOService*
PPCBurgundy::probe(IOService* provider, SInt32*    score)
{
    IOService *myService = this;

    // We CAN fail the tye check:
    super::probe(provider, score);
    *score = kIODefaultProbeScore;

    // finds and cashes the hardware we are running on:
    int mType = findHostingHardware(provider);

    // If it one of the machine I know supports
    // burgundy I'm going to increment the score
    if ((mType == kMachineTypeYosemite) || (mType == kMachineTypeiMac)) {
        // One score increment because this is a burgundy-hosting motherboard
        *score = *score + 1;
    }

    // Figure out if this is a bugundy and if it is a bordeaux
    sound = NULL;
    isBordeaux = false;

    // This is likely to be the case that this is perch
    if (implementsBurgundy(IORegistryEntry::fromPath("/perch", gIODTPlane))) {
        *score = *score + 1;
        myService = this;
    }
    else if ((sound = provider->childFromPath("sound", gIODTPlane)) != NULL) {
        // IOLog("PPCBurgundy::probe looking for sound\n");
        if (implementsBurgundy(sound)) {
            *score = *score + 1;
            myService = this;
        }
    }
    
    if (myService != NULL) {
        IOLog("Found Burgundy compatible chip %s\n", (isBordeaux ? "and Bordeaux card": "NON  Bordeaux card"));
    }

    return (myService);
}

#define kNumDMAStreams 2

bool
PPCBurgundy::start(IOService* provider)
{
    // Gets the base for the burgundy registers:
    IOMemoryMap *map;
    int i;

    if( !super::start(provider))
        return (false);

    map = provider->mapDeviceMemoryWithIndex(kAudioDMAdeviceInt);
    if(!map) {
        return false;
    }
    ioBaseBurgundy = (UInt8 *)map->getVirtualAddress();

    if (!AllocateStreams(kNumDMAStreams))
        return false;

    map = provider->mapDeviceMemoryWithIndex(kAudioDMAtxInt);
    if(!map) {
        return false;
    }
    DefineStream(kAudioDMAOutputStream, kOutput, outputFrameRate(), (IODBDMAChannelRegisters*)map->getVirtualAddress());

    map = provider->mapDeviceMemoryWithIndex(kAudioDMArxInt);
    if(!map) {
        return false;
    }
    DefineStream(kAudioDMAInputStream, kInput, inputFrameRate(), (IODBDMAChannelRegisters*)map->getVirtualAddress());

    // Creates the array of input and output components
    numInputComponents = numberOfOutputComponents();
    numOutputComponents = numberOfInputComponents();

    fInputComponents = (IOAudioComponentImplPtr*)IOMalloc(sizeof(IOAudioComponentImplPtr) * numInputComponents);
    if (fInputComponents == NULL) {
        // Free the DMA and exit:
        FreeStreams();
        return false;
    }

    fOutputComponents = (IOAudioComponentImplPtr*)IOMalloc(sizeof(IOAudioComponentImplPtr) * numOutputComponents);
    if (fOutputComponents == NULL) {
        // Free the input components
        IOFree(fInputComponents, sizeof(IOAudioComponentImplPtr) * numInputComponents);
        fInputComponents = NULL;
        numInputComponents = 0;
 
        // Free the DMA and exit:
        FreeStreams();
        return false;
    }

    // Initalizes all the components:
    for (i = 0; i < numInputComponents; i++)
        fInputComponents[i] = NULL;

    for (i = 0; i < numOutputComponents; i++)
        fOutputComponents[i] = NULL;

    // Initializes the sound control register:
    soundControlRegister = ( kSoundCtlReg_InSubFrame0      | \
                             kSoundCtlReg_OutSubFrame0     | \
                             kSoundCtlReg_Rate_44100        );
    writeSoundControlReg(ioBaseBurgundy, soundControlRegister);

    // Set Input Source A gain to default
    writeCodecReg( ioBaseBurgundy, kGASALReg, kGAS_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kGASARReg, kGAS_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kVGA2Reg,   0x44 );
    writeCodecReg( ioBaseBurgundy, kGAS2LReg,  255 );
    writeCodecReg( ioBaseBurgundy, kGAS2RReg,  255 );
    
    // Set mixer 1 input to Input Source A (Serial-in Subframe 0)
    writeCodecReg( ioBaseBurgundy, kMX1Reg, kMX1Reg_Select_ISAL | kMX1Reg_Select_ISAR );
    writeCodecReg( ioBaseBurgundy, kSDInReg, kSDInReg_ASA_From_SF0 );

    // Set mixer 0-3 outputs to default gain
    writeCodecReg( ioBaseBurgundy, kMXEQ1LReg, kMXEQ_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kMXEQ1RReg, kMXEQ_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kMXEQ2LReg, kMXEQ_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kMXEQ2RReg, kMXEQ_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kMXEQ3LReg, kMXEQ_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kMXEQ3RReg, kMXEQ_Default_Gain );

    // Set Output Source 0 gain to default
    writeCodecReg( ioBaseBurgundy, kGAP0LReg, kGAP_Default_Gain );
    writeCodecReg( ioBaseBurgundy, kGAP0RReg, kGAP_Default_Gain );

    startWorkLoop();
    
    return true;
}

void
PPCBurgundy::stop(IOService *provider)
{
    // Releases all DMA streams:
    FreeStreams();

    // Releases the input components
    if (fInputComponents != NULL) {
        IOFree(fInputComponents, sizeof(IOAudioComponentImplPtr) * numInputComponents);
        fInputComponents = NULL;
        numInputComponents = 0;
    }

    // Releases the output components
    if (fOutputComponents != NULL) {
        IOFree(fOutputComponents, sizeof(IOAudioComponentImplPtr) * numOutputComponents);
        fOutputComponents = NULL;
        numOutputComponents = 0;
    }
}

void
PPCBurgundy::CreateAudioTopology(IOCommandQueue *queue)
{
    IOAudioComponentImpl *mixerIn;
    IOAudioComponentImpl *mixerOut;
    IOAudioComponentImpl *microphone;
    IOAudioComponentImpl *lineout;
    IOAudioComponentImpl *linein;
    IOAudioComponentImpl *headphones;
    IOAudioComponentImpl *cd;
    IOAudioComponentImpl *speaker;
    IOAudioComponentImpl *passtru;

    AudioStreamIndex outputStream;
    AudioStreamIndex inputStream;
    
    if (!fStreams)
        return;

    outputStream = firstStreamAfter(kOutput, 0);
    inputStream = firstStreamAfter(kInput, 0);

    if ((outputStream == kInvalidStreamIndex) || (inputStream == kInvalidStreamIndex)) {
        IOLog("PPCBurgundy::CreateAudioTopology called without available DMA streams\n");
        return;
    }
    else
        IOLog("PPCBurgundy::CreateAudioTopology input stream is %d output is %d\n", inputStream, outputStream);

    mixerOut = buildComponentAndAttach(GetStream(outputStream), NULL, componentDictionaryMixerOut(), queue);
    headphones = buildComponentAndAttach(mixerOut, NULL, componentDictionaryHeadphones(), queue);
    speaker = buildComponentAndAttach(mixerOut, NULL, componentDictionarySpeaker(), queue);
    lineout = buildComponentAndAttach(mixerOut, NULL, componentDictionaryLineout(), queue);

    mixerIn = buildComponentAndAttach(NULL, GetStream(inputStream), componentDictionaryMixerIn(), queue);
    cd = buildComponentAndAttach(NULL, NULL,  componentDictionaryCD(), queue);
    linein = buildComponentAndAttach(NULL, NULL,  componentDictionaryLinein(), queue);
    microphone = buildComponentAndAttach(NULL, mixerIn, componentDictionaryMicrophone(), queue);

    passtru = buildComponentAndAttach(mixerIn, mixerOut, componentDictionaryPassThru(), queue);

    buildComponentAndAttach(mixerIn, mixerOut, componentDictionaryPassThru(), queue);
    fInputComponents[kCD] = buildComponentAndAttach(NULL, mixerIn, componentDictionaryCD(), queue);

    // FIXME: the component connection could be done better than so
    if (fInputComponents != NULL) {
        fInputComponents[kMicroPhone] = microphone;

        if (numInputComponents > 1)
            fInputComponents[kCD] = cd;

        if (numInputComponents > 2)
            fInputComponents[kLineIn] = linein;
    }

    if (fOutputComponents != NULL) {
        fOutputComponents[kSpeaker] = speaker;

        if (numOutputComponents > 1)
            fOutputComponents[kHeadphones] = headphones;

        if (numOutputComponents > 2)
            fOutputComponents[kLineOut] = lineout;
    }

    // Sets ups the burgundy as required by this machine wiring
    // configuration:
    if (!dependentSetup())
        IOLog("PPCBurgundy::CreateAudioTopology burgundy setup failed\n");
}

void
PPCBurgundy::DoClockTick(IOTimerEventSource *t)
{
    checkStatusRegister();
    super::DoClockTick(t);
}

void
PPCBurgundy::calculateTickInterval(AbsoluteTime *tickInterval)
{
    AbsoluteTime maxInterval;

    // We want to check the status at least one a second
    nanoseconds_to_absolutetime(NSEC_PER_SEC, &maxInterval);
    if(CMP_ABSOLUTETIME(&maxInterval, tickInterval) < 0) {
        *tickInterval = maxInterval;
    }
    super::calculateTickInterval(tickInterval);
}

IOReturn
PPCBurgundy::SetControl(UInt16 id, int val)
{
    IOReturn res = kIOReturnSuccess;
    
    switch(id) {
        case kSpeakerMute:
            muteInternalSpeaker(val != 0);
            muteHeadphones(val == 0);
            break;

        case kHeadphonesMute:
            muteHeadphones(val != 0);
            muteInternalSpeaker(val == 0);
            break;

        case kMicrophoneMute:
            setMicInput(val == 0);
            break;

        case kSpeakerVolLeft:
            volumeInternalSpeakerLeft(val);
            break;

        case kSpeakerVolRight:
            volumeInternalSpeakerRight(val);
            break;

        case kHeadphonesVolLeft:
            volumeHeadphonesLeft(val);
            break;

        case kHeadphonesVolRight:
            volumeHeadphonesRight(val);
            break;

        case kCDMute:
            muteCDLine(val != 0);
            break;

        case kMixerInVolLeft:
            volumeMixerInLeft(val);
            break;

        case kMixerInVolRight:
            volumeMixerInRight(val);
            break;

        case kPassThruVolLeft:
            break;

        case kPassThruVolRight:
            break;

        case kPassThruMute:
            break;

        default:
            res = kIOReturnUnsupported;
    }
    return res;
}

/* ===============
 * Private Methods
 * =============== */

// --------------------------------------------------------------------------
// Method: implementsBurgundy
//
// Purpose:
//   Attempts to discover if the device implements burgundy:
bool
PPCBurgundy::implementsBurgundy(IORegistryEntry *device)
{    
    if (device != NULL) {
        OSData *s = NULL;

        //IOLog("Matching burgundy compatibility with %s\n", device->getName());

        s = OSDynamicCast(OSData, device->getProperty("compatible"));

        if (s != NULL) {
            if(s->isEqualTo("burgundy", sizeof("burgundy")-1)) {
                return true;
            }
            else if(s->isEqualTo("DVD-Video and Audio/Video", sizeof("DVD-Video and Audio/Video")-1)) {
                // also sets the isBordeaux flag
                isBordeaux = true;
                return true;
            }
            else {
                //IOLog("PPCBurgundy::probe sound is not burgundy compatible.\n");
            }
        }
        else {
            //IOLog("PPCBurgundy::probe sound does not have a compatible property.\n");
        }
    }
    else{
        //IOLog("PPCBurgundy::probe this hardware does not have a sound chip\n");
    }
    return false;
}

// --------------------------------------------------------------------------
// Method: findHostingHardware
//
// Purpose:
//   Finds the kind of hardware we are running on. Since all the older machines
//   do not have all the information about how burgundy is wired, knowing which
//   machine we are running on helps to set up the driver correctly.
//   The method recognizes more machines than the ones that use bugundy (e.g.
//   "'AAPL,9500'" has an AWACS).

int
PPCBurgundy::findHostingHardware(IOService *provider)
{
    IOService *topProvider = NULL;

    // If we already found the machine the driver is running on
    // just returns the value previously found:
    if ((machineType >= kMachineTypeUnknown) ||
	(machineType == 0)) {
        // if the argument is missing I've to get from myself:
        if (provider == NULL)
            provider = getProvider();

          // See if we find the top of the tree (with the machine type)
        // iterating all the way up:
        while (provider != NULL) {
            topProvider = provider;
            provider = topProvider->getProvider();
        }

         // by default the hardware is unknown:
        machineType = kMachineTypeUnknown;

        if (topProvider != NULL) {
            if (IODTMatchNubWithKeys(topProvider, "'AAPL,9500'"))
                machineType = kMachine9500;
            if (IODTMatchNubWithKeys(topProvider, "'AAPL,Gossamer'"))
                machineType = kMachineGenericGossamer;
            if (IODTMatchNubWithKeys(topProvider, "'AAPL,PowerMac G3'"))
                machineType = kMachineTypeSilk;
            else if (IODTMatchNubWithKeys(topProvider, "'AAPL,PowerBook1998'"))
                machineType = kMachineTypeWallstreet;
            else if (IODTMatchNubWithKeys(topProvider, "'iMac,1'"))
                machineType = kMachineTypeiMac;
            else if (IODTMatchNubWithKeys(topProvider, "('PowerMac1,1', 'PowerMac1,2')"))
                machineType = kMachineTypeYosemite;
            else if (IODTMatchNubWithKeys(topProvider, "'PowerMac3,1'"))
                machineType = kMachineTypeSawtooth;
            else if (IODTMatchNubWithKeys(topProvider, "'PowerBook1,1'"))
                machineType = kMachineType101;
            else if (IODTMatchNubWithKeys(topProvider, "'PowerBook2,1'"))
                machineType = kMachineTypeiBook;
            else
                IOLog("PPCBurgundy::findHostingHardware unknown machine type\n");
        }
        else
            IOLog("PPCBurgundy::findHostingHardware misses the top provider\n");
    }

    //Just to ease up the debugging
    //IOLog("PPCBurgundy::findHostingHardware found %d\n", machineType);

    return (machineType);
}

// --------------------------------------------------------------------------
// Method: numberOfInputComponents
//
// Purpose:
//        returns the number of components for the input lines
//        usually are 2 one is the microphone and the other is
//        microphone jack.
#define kCommonNumberOfInputComponents 2

int
PPCBurgundy::numberOfInputComponents()
{
    if(sound) {
        OSData *t;
  
        t = OSDynamicCast(OSData, sound->getProperty("#-detects"));
        if (t != NULL) {
            return *(UInt32*)(t->getBytesNoCopy());
        }
    }

    return kCommonNumberOfInputComponents;
}

// --------------------------------------------------------------------------
// Method: numberOfOutputComponents
//
// Purpose:
//        returns the number of components for the output lines
//        usually are 2 one is the internal speacker and the other
//        is the headphone line.
#define kCommonNumberOfOutputComponents 2

int
PPCBurgundy::numberOfOutputComponents()
{
    if(sound) {
        OSData *t;
        
        t = OSDynamicCast(OSData, sound->getProperty("#-outputs"));
        if (t != NULL) {
            return *(UInt32*)(t->getBytesNoCopy());
        }
    }
    return kCommonNumberOfOutputComponents;
}


// --------------------------------------------------------------------------
// Method: inputFrameRate
//
// Purpose:
//        returns the input frame rate as in the registry, if it is
//        not found in the registry, it returns the default value.
#define kCommonInputFrameRate 44100

UInt16
PPCBurgundy::inputFrameRate()
{
    if(sound) {
        OSData *t;

        t = OSDynamicCast(OSData, sound->getProperty("input-frame-rates"));
        if (t != NULL) {
            UInt16 fr = *(UInt32*)(t->getBytesNoCopy());

#ifdef DEBUGMODE
            IOLog( "PPCBurgundy::inputFrameRate = %d\n", fr);
#endif
            return fr;
        }
    }

    return kCommonInputFrameRate;
}


// --------------------------------------------------------------------------
// Method: outputFrameRate
//
// Purpose:
//        returns the output frame rate as in the registry, if it is
//        not found in the registry, it returns the default value.
#define kCommonOutputFrameRate 44100

UInt16
PPCBurgundy::outputFrameRate()
{
     if(sound) {
        OSData *t;

        t = OSDynamicCast(OSData, sound->getProperty("output-frame-rates"));
        if (t != NULL) {
            UInt16 fr = *(UInt32*)(t->getBytesNoCopy());

#ifdef DEBUGMODE
            IOLog( "PPCBurgundy::outputFrameRate = %d\n", fr);
#endif
            return fr;
        }
    }

    return kCommonOutputFrameRate;
}

// --------------------------------------------------------------------------
// Method(s): componentDictionaryXXXXXX
//
// Purpose:
//        the next N methods return the strings for the component dictionaries
//        for each kind of component. As stated in the AWACS driver, these should
//        end in the driver resources.

// NOTE: (This is important !!!) the soundControls enum in the header file is
//       bounded to the following stings in this way:
//       for each component the control Id in the string must have the same value
//       of the corrispondent soundControls enum item. For example the sound control
//       enum item kHeadphonesVolLeft has value 4, so in sHeadphones the ID of the
//       volume left control must be 4.

char*
PPCBurgundy::componentDictionarySpeaker()
{
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
    return (char*)sSpeaker;
}

char*
PPCBurgundy::componentDictionaryHeadphones()
{
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
    return (char*)sHeadphones;
}

char*
PPCBurgundy::componentDictionaryMicrophone()
{
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
    return (char*)sMicrophone;
}

char*
PPCBurgundy::componentDictionaryLinein()
{
    static const char *sLinein =
    "{
    'Type' = 'LineIn';
    'Channels' = 2:8;
    'Controls' = {
        'MuteAll' = {
            'Type' = 'Mute';
            'Id' = 14:16;
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
    return (char*)sLinein;
}

char*
PPCBurgundy::componentDictionaryLineout()
{
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
    return (char*)sLineout;
}

char*
PPCBurgundy::componentDictionaryCD()
{
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
    return (char*)sCD;
}

char*
PPCBurgundy::componentDictionaryMixerIn()
{
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
    return (char*)sMixerIn;
}

char*
PPCBurgundy::componentDictionaryMixerOut()
{
    static const char *sMixerOut =
    "{
    'Type' = 'Mixer';
    'Channels' = 2:8;
    }";
    return (char*)sMixerOut;
}

char*
PPCBurgundy::componentDictionaryPassThru()
{
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
    return (char*)sPassThru;
}

// --------------------------------------------------------------------------
// Method: dependentSetup
//
// Purpose:
//        this handles the setup of the burgundy chip for each kind of
//        hosting hardware.
bool
PPCBurgundy::dependentSetup()
{
    UInt32 OSReg = 0;
 
    //
    // G3-Gossamer DVD-card assignments (Bordeaux)
    //
    // 		Port 17  - Internal speaker
    //          Port 15  - Rear panel RCA Out
    //		Port 16	 - Rear panel MiniJack Out
    //
    // Yosemite assignments
    //
    //		Port 17 - Internal speaker
    //		Port 14 - Rear panel MiniJack Out
    //
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort17Reg,	kOutputLvl_Default );

    switch (findHostingHardware(NULL))
    {
        case kMachineTypeSilk:
            // G3-Gossamer DVD-card
            assert(isBordeaux);
            writeCodecReg( ioBaseBurgundy, kOutputLvlPort15Reg, kOutputLvl_Default );
            writeCodecReg( ioBaseBurgundy, kOutputLvlPort16Reg, kOutputLvl_Default );

            writeCodecReg( ioBaseBurgundy, kGAP1LReg, kGAP_Default_Gain );
            writeCodecReg( ioBaseBurgundy, kGAP1RReg, kGAP_Default_Gain );

            currentOutputMuteReg = kOutputMuteReg_Port15L | kOutputMuteReg_Port15R |
                kOutputMuteReg_Port16L | kOutputMuteReg_Port16R |
                kOutputMuteReg_Port17M;

            OSReg = kOSReg_OS0_Select_MXO1 | kOSReg_OS1_Select_MXO1;
            break;

        case kMachineTypeiMac:
        case kMachineTypeYosemite:
            // Yosemite:  (FIXME as soon as newwolds works I should move this in the newworld handling
            //            this should be done also for the others switch statements in this file)
            writeCodecReg( ioBaseBurgundy, kOutputLvlPort14Reg, kOutputLvl_Default );

            currentOutputMuteReg = kOutputMuteReg_Port14L | kOutputMuteReg_Port14R |
                kOutputMuteReg_Port17M;

            OSReg = kOSReg_OS0_Select_MXO1;
            break;
        default:
            // FIXME: this may be a newworld, so the registry holds
            // all we need to know about how the burgundy is wired:
            // Add here the code to interpret it.
            break;
    }
    
    writeCodecReg( ioBaseBurgundy, kOSReg,         OSReg );
    writeCodecReg( ioBaseBurgundy, kOutputMuteReg, currentOutputMuteReg	 );
    writeCodecReg( ioBaseBurgundy, kOutputCtl0Reg, kOutputCtl0Reg_OutCtl1_High );

    // while this acts on the analog volume controls
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort13Reg, 0x00);
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort14Reg, 0x00);
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort15Reg, 0x00);
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort16Reg, 0x00);
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort17Reg, 0x00);

    // and writes the digital volumes:
    writeCodecReg( ioBaseBurgundy, kGAP0LReg, 0xFF);
    writeCodecReg( ioBaseBurgundy, kGAP1LReg, 0xFF);
    writeCodecReg( ioBaseBurgundy, kGAP2LReg, 0xFF);
    writeCodecReg( ioBaseBurgundy, kGAP3LReg, 0xFF);
    
    writeCodecReg( ioBaseBurgundy, kGAP0RReg, 0xFF);
    writeCodecReg( ioBaseBurgundy, kGAP1RReg, 0xFF);
    writeCodecReg( ioBaseBurgundy, kGAP2RReg, 0xFF);
    writeCodecReg( ioBaseBurgundy, kGAP3RReg, 0xFF);
    return true;
}

// --------------------------------------------------------------------------
// Method: checkStatusRegister
//
// Purpose:
//        if the argument is true mutes the internal speaker, otherwise
//        it "unmutes" it.
void
PPCBurgundy::checkStatusRegister()
{
    if (burgundyStatusChanged())  {
        int i;
        
        OSNumber * num = OSNumber::withNumber(lastStatusRegister, 32);
        setProperty("Status", num);
        num->release();

#ifdef DEBUGMODE
	IOLog("Status changes in: 0x%08lx\n", lastStatusRegister);
#endif
        // Something changed, act on it:
        // For each sense line we call the action attached
        // to thar sense line:
        for (i = 0; i < numInputComponents; i++)
            if (fInputComponents[i] != NULL)
                fInputComponents[i]->Set(gInputsSym, gJackSym, inputComponentStatus(i));

        for (i = 0; i < numOutputComponents; i++)
            if (fOutputComponents[i] != NULL)
                fOutputComponents[i]->Set(gInputsSym, gJackSym, outputComponentStatus(i));
    }
}

// --------------------------------------------------------------------------
// Method: inputComponentStatus
//
// Purpose:
//        checks the status of the jack line of the ith output component:
bool
PPCBurgundy::inputComponentStatus(int index)
{
    switch(index)
    {
        case kMicroPhone: // check the microphone
            return microphoneInserted();

        case kCD:	 // the cd input is not associated to a jack
            return false;

        case kLineIn:	// the RCA line does not have a jack
            return false;
    }

    return false;	// by deafult there is no jack
}


// --------------------------------------------------------------------------
// Method: outputComponentStatus
//
// Purpose:
//        checks the status of the jack line of the ith output component:
bool
PPCBurgundy::outputComponentStatus(int index)
{
    switch(index)
    {
        case kSpeaker:
            return false;	// the internal speaker does not have a jack

        case kHeadphones:	// check the headphones:
            return headphonesInserted();

        case kLineOut:
            return false;	// the RCA line does not have a jack
    }

    return false;	// by deafult there is no jack
}

// --------------------------------------------------------------------------
// Method: muteInternalSpeaker
//
// Purpose:
//        if the argument is true mutes the internal speaker, otherwise
//        it "unmutes" it.

void
PPCBurgundy::muteInternalSpeaker(bool mute)
{
    UInt32 outputSourceMask = 0;

    switch (findHostingHardware(NULL))
    {
        case kMachineTypeSilk:
            // G3-Gossamer DVD-card
            assert(isBordeaux);
            outputSourceMask = kOutputMuteReg_Port17M;
            break;

        case kMachineTypeiMac:
              // iMac:  (FIXME as soon as newwolds works I should move this in the newworld handling)
              outputSourceMask = kOutputMuteReg_Port14L | kOutputMuteReg_Port14R;
            break;

        case kMachineTypeYosemite:
            outputSourceMask = kOutputMuteReg_Port17M;
            break;

        default:
            // FIXME: this may be a newworld, so the registry holds
            // all we need to know about how the burgundy is wired:
            // Add here the code to interpret it.
            break;
    }
 
    if (mute)
        currentOutputMuteReg &= ~outputSourceMask;
    else
        currentOutputMuteReg |= outputSourceMask;

    writeCodecReg(ioBaseBurgundy, kOutputMuteReg, currentOutputMuteReg);
}

// --------------------------------------------------------------------------
// Method: muteRCAOutput
//
// Purpose:
//        if the argument is true mutes the rear panel RCA, otherwise
//        it "unmutes" it.
void
PPCBurgundy::muteRCAOutput(bool mute)
{
    UInt32 outputSourceMask = 0;

    switch (findHostingHardware(NULL))
     {
         case kMachineTypeSilk:
             // G3-Gossamer DVD-card
             assert(isBordeaux);
             outputSourceMask = kOutputMuteReg_Port15L | kOutputMuteReg_Port15R;
             break;

         case kMachineTypeYosemite:
             // Yosemite does not have an RCA
             outputSourceMask = 0;
             break;

         default:
             // FIXME: this may be a newworld, so the registry holds
             // all we need to know about how the burgundy is wired:
             // Add here the code to interpret it.
             outputSourceMask = 0;
             break;
     }

    if (mute)
        currentOutputMuteReg &= ~outputSourceMask;
    else
        currentOutputMuteReg |= outputSourceMask;

    writeCodecReg(ioBaseBurgundy, kOutputMuteReg, currentOutputMuteReg);
}

// --------------------------------------------------------------------------
// Method: muteHeadphones
//
// Purpose:
//        if the argument is true mutes the rear panel mini jack, otherwise
//        it "unmutes" it.
void
PPCBurgundy::muteHeadphones(bool mute)
{
    UInt32 outputSourceMask = 0;

    switch (findHostingHardware(NULL))
      {
          case kMachineTypeSilk:
              // G3-Gossamer DVD-card
              assert(isBordeaux);
              outputSourceMask = kOutputMuteReg_Port16L | kOutputMuteReg_Port16R;
              break;

          case kMachineTypeiMac:
              // iMac:  (FIXME I do not know how this is wired, as soon  as I have the iMac motherboard I should fix it)
              outputSourceMask |= kOutputMuteReg_Port15L | kOutputMuteReg_Port15R;
              outputSourceMask |= kOutputMuteReg_Port16L | kOutputMuteReg_Port16R;
              outputSourceMask |= kOutputMuteReg_Port13M | kOutputMuteReg_Port17M;
              break;

          case kMachineTypeYosemite:
              // Yosemite:  (FIXME as soon as newwolds works I should move this in the newworld handling)
              outputSourceMask = kOutputMuteReg_Port14L | kOutputMuteReg_Port14R;
              break;

          default:
              // FIXME: this may be a newworld, so the registry holds
              // all we need to know about how the burgundy is wired:
              // Add here the code to interpret it.
              break;
      }

    if (mute)
        currentOutputMuteReg &= ~outputSourceMask;
    else
        currentOutputMuteReg |= outputSourceMask;

    writeCodecReg(ioBaseBurgundy, kOutputMuteReg, currentOutputMuteReg);
}

// --------------------------------------------------------------------------
// Method: muteCDLine
//
// Purpose:
//        if the argument is true mutes the line of the cd player
void
PPCBurgundy::muteCDLine(bool mute)
{
    // FIXME: Figure out how this is wired up:
}

// --------------------------------------------------------------------------
// Method: setMicInput
//
// Purpose:
//        selects the microphone as input source (if bool = true)
void
PPCBurgundy::setMicInput(bool mic)
{
    UInt32 reg;

#ifdef DEBUGMODE
    IOLog( "PPCBurgundy::setMicInput: %s\n", (mic ? "True" : "False"));
#endif

    if (mic) {
        reg  = readCodecReg( ioBaseBurgundy, kMux2Reg );
        reg |= kMux2Reg_Mux2L_SelectPort6L | kMux2Reg_Mux2R_SelectPort6R;
        writeCodecReg( ioBaseBurgundy, kMux2Reg, reg );

        reg  = readCodecReg( ioBaseBurgundy, kMX3Reg );
        reg |= kMX3Reg_Select_IS2L | kMX3Reg_Select_IS2R;
        writeCodecReg( ioBaseBurgundy, kMX3Reg, reg);

        reg  = readCodecReg( ioBaseBurgundy, kOSReg );
        reg |= kOSReg_OSE_Select_MXO3;
        writeCodecReg( ioBaseBurgundy, kOSReg, reg );

        reg  = readCodecReg( ioBaseBurgundy, kSDOutReg );
        reg |= kSDOutReg_OSE_To_SF0;
        writeCodecReg( ioBaseBurgundy, kSDOutReg, reg );
    }
    else {
        reg  = readCodecReg( ioBaseBurgundy, kMux2Reg );
        reg &= ~(kMux2Reg_Mux2L_SelectPort6L | kMux2Reg_Mux2R_SelectPort6R);
        writeCodecReg( ioBaseBurgundy, kMux2Reg, reg );

        reg  = readCodecReg( ioBaseBurgundy, kMX3Reg );
        reg &= ~(kMX3Reg_Select_IS2L | kMX3Reg_Select_IS2R);
        writeCodecReg( ioBaseBurgundy, kMX3Reg, reg);

        reg  = readCodecReg( ioBaseBurgundy, kOSReg );
        reg &= ~kOSReg_OSE_Select_MXO3;
        writeCodecReg( ioBaseBurgundy, kOSReg, reg );

        reg  = readCodecReg( ioBaseBurgundy, kSDOutReg );
        reg &= ~kSDOutReg_OSE_To_SF0;
        writeCodecReg( ioBaseBurgundy, kSDOutReg, reg );
   }
}


// --------------------------------------------------------------------------
// Method: burgundyStatusChanged()
//
// Purpose:
//        returns true if something changed in the status register:.
bool
PPCBurgundy::burgundyStatusChanged()
{
    UInt32 currentStatusRegister = readCodecSenseLines(ioBaseBurgundy);
    bool returnValue = (currentStatusRegister != lastStatusRegister);
    lastStatusRegister = currentStatusRegister;

    return (returnValue);
}

// --------------------------------------------------------------------------
// Method: headphonesInserted
//
// Purpose:
//        returns true if the headphones are inserted.
bool
PPCBurgundy::headphonesInserted()
{
    UInt32 codecSense;
    UInt32 inputSourceMask = 0;

    switch (findHostingHardware(NULL))
      {
          case kMachineTypeSilk:
              // G3-Gossamer DVD-card
              assert(isBordeaux);
              inputSourceMask = kCodecStatusReg_Sense_Headphones;
              break;

          case kMachineTypeiMac:
              // iMac:  (FIXME as soon as newwolds works I should move this in the newworld handling)
              inputSourceMask = kCodecStatusReg_Sense_Headphones | kCodecStatusReg_Sense_Headphones2 | kCodecStatusReg_Sense_Bit;
              break;
		
          case kMachineTypeYosemite:
              // Yosemite:  (FIXME as soon as newwolds works I should move this in the newworld handling)
              inputSourceMask = kCodecStatusReg_Sense_Headphones;
              break;

          default:
              // FIXME: this may be a newworld, so the registry holds
              // all we need to know about how the burgundy is wired:
              // Add here the code to interpret it.
              break;
      }

    codecSense = readCodecSenseLines(ioBaseBurgundy);

#ifdef DEBUGMODE
    IOLog( "PPCBurgundy::headphonesInserted = %d\n\r", (codecSense & inputSourceMask) != 0);
#endif

    return (codecSense & inputSourceMask) != 0;
}


// --------------------------------------------------------------------------
// Method: microphoneInserted
//
// Purpose:
//        returns true if the microphone is inserted.
bool
PPCBurgundy::microphoneInserted()
{
    UInt32 codecSense;
    UInt32 inputSourceMask = 0;

    switch (findHostingHardware(NULL))
      {
          case kMachineTypeSilk:
              // G3-Gossamer DVD-card
              assert(isBordeaux);
              inputSourceMask = kCodecStatusReg_Sense_Mic;
              break;

          case kMachineTypeiMac:
              // iMac:  (FIXME as soon as newwolds works I should move this in the newworld handling)
              inputSourceMask = kCodecStatusReg_Sense_Mic;
              break;

          case kMachineTypeYosemite:
              // Yosemite:  (FIXME as soon as newwolds works I should move this in the newworld handling)
              inputSourceMask = kCodecStatusReg_Sense_Mic;
              break;

          default:
              // FIXME: this may be a newworld, so the registry holds
              // all we need to know about how the burgundy is wired:
              // Add here the code to interpret it.
              break;
      }

    codecSense = readCodecSenseLines(ioBaseBurgundy);

#ifdef DEBUGMODE
    IOLog( "PPCBurgundy::microphoneInserted = %d\n\r", (codecSense & inputSourceMask) != 0);
#endif

    // FIXME: This should not be necessary, but since it does not work by default
    // I'll add this call myself:
    setMicInput((codecSense & inputSourceMask) != 0);
    
    return (codecSense & inputSourceMask) != 0;
}

// --------------------------------------------------------------------------
// Method: scaleVolume
//
// Purpose:
//        Scale the volume to the range used by burgundy:
//        note, this considers that the volume can range from
//        0 to 65536 as in the control strings above (e.g. componentDictionaryMixerIn)
UInt8
PPCBurgundy::scaleVolume(int vol)
{
    int output;

    output  = vol / 4096;
    
    if ( output  > 15 )
        output  = 15;

    if (output  < 0 )
        output  = 0;

#ifdef DEBUGMODE
    IOLog("Volume input: %d, output (scaled) %d\n", vol, output);
#endif

    return (UInt8)output;
}

// --------------------------------------------------------------------------
// Method: volumeHeadphonesLeft & volumeHeadphonesRight
//
// Purpose:
//        sets the volume for the left and right channel of the internal
//        headphones.
void
PPCBurgundy::volumeHeadphonesLeft(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllLeft(vol);
}

void
PPCBurgundy::volumeHeadphonesRight(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllRight(vol);
}

// --------------------------------------------------------------------------
// Method: volumeInternalSpeakerLeft & volumeInternalSpeakerRight
//
// Purpose:
//        sets the volume for the left and right channel of the internal
//        speaker.
void
PPCBurgundy::volumeInternalSpeakerLeft(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllLeft(vol);
}

void
PPCBurgundy::volumeInternalSpeakerRight(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllRight(vol);
}

// --------------------------------------------------------------------------
// Method: volumeCDLineLeft & volumeCDLineRight
//
// Purpose:
//        sets the volume for the left and right channel of the CD
//        line.
void
PPCBurgundy::volumeCDLineLeft(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllLeft(vol);
}

void
PPCBurgundy::volumeCDLineRight(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllRight(vol);
}

// --------------------------------------------------------------------------
// Method: volumeRCAOutputLeft & volumeRCAOutputRight
//
// Purpose:
//        sets the volume for the left and right channel of the RCA
//        line.
void
PPCBurgundy::volumeRCAOutputLeft(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllLeft(vol);
}

void
PPCBurgundy::volumeRCAOutputRight(int vol)
{
    // FIXME: This sets all the output volumes, but we may wish to have
    // a more discriminating volume handling. As soon as I know for sure
    // how burgundy is wired we can do it. For now this should be fine.
    volumeAllRight(vol);
}

// --------------------------------------------------------------------------
// Method: volumeAllRight & volumeAllLeft
//
// Purpose:
//        sets the volume to all the output ports. This is because I do not
//        really know how burgundy is wired up. (and because however the
//        OS has only one control for the volume).
void
PPCBurgundy::volumeAllLeft(int vol)
{
    UInt8 newVol = scaleVolume(vol);

    // This changes all the digital volume controls (used only to mute and unmute the oputput)
    if (newVol > 0) {
        writeCodecReg( ioBaseBurgundy, kGAP0LReg, 0xFF);
        writeCodecReg( ioBaseBurgundy, kGAP1LReg, 0xFF);
        writeCodecReg( ioBaseBurgundy, kGAP2LReg, 0xFF);
        writeCodecReg( ioBaseBurgundy, kGAP3LReg, 0xFF);        
    }
    else {
        writeCodecReg( ioBaseBurgundy, kGAP0LReg, 0x00);
        writeCodecReg( ioBaseBurgundy, kGAP1LReg, 0x00);
        writeCodecReg( ioBaseBurgundy, kGAP2LReg, 0x00);
        writeCodecReg( ioBaseBurgundy, kGAP3LReg, 0x00);        
    }
    
    // while this acts on the analog volume controls
    newVol = 15 - newVol;
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort13Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort13Reg) & 0xF0) | (newVol & 0x0F));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort14Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort14Reg) & 0xF0) | (newVol & 0x0F));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort15Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort15Reg) & 0xF0) | (newVol & 0x0F));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort16Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort16Reg) & 0xF0) | (newVol & 0x0F));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort17Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort17Reg) & 0xF0) | (newVol & 0x0F));
}

void
PPCBurgundy::volumeAllRight(int vol)
{
    UInt8 newVol = scaleVolume(vol);

    // This changes all the digital volume controls (used only to mute and unmute the oputput)
    if (newVol > 0) {
        writeCodecReg( ioBaseBurgundy, kGAP0RReg, 0xFF);
        writeCodecReg( ioBaseBurgundy, kGAP1RReg, 0xFF);
        writeCodecReg( ioBaseBurgundy, kGAP2RReg, 0xFF);
        writeCodecReg( ioBaseBurgundy, kGAP3RReg, 0xFF);
    }
    else {
        writeCodecReg( ioBaseBurgundy, kGAP0RReg, 0x00);
        writeCodecReg( ioBaseBurgundy, kGAP1RReg, 0x00);
        writeCodecReg( ioBaseBurgundy, kGAP2RReg, 0x00);
        writeCodecReg( ioBaseBurgundy, kGAP3RReg, 0x00);
    }

    // while this acts on the analog volume controls
    newVol = (15 - newVol) << 4;
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort13Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort13Reg) & 0x0F) | (newVol & 0xF0));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort14Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort14Reg) & 0x0F) | (newVol & 0xF0));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort15Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort15Reg) & 0x0F) | (newVol & 0xF0));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort16Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort16Reg) & 0x0F) | (newVol & 0xF0));
    writeCodecReg( ioBaseBurgundy, kOutputLvlPort17Reg, (readCodecReg(ioBaseBurgundy, kOutputLvlPort17Reg) & 0x0F) | (newVol & 0xF0)); 
}


// --------------------------------------------------------------------------
// Method: volumeMixerInLeft
//
// Purpose:
//        sets the gain for the input left line
void
PPCBurgundy::volumeMixerInLeft(int vol)
{
    UInt8 left = scaleVolume(vol);
    
    writeCodecReg( ioBaseBurgundy, kVGA2Reg,   0x44 );
    writeCodecReg( ioBaseBurgundy, kGAS2LReg,  left );
}

// --------------------------------------------------------------------------
// Method: volumeMixerInRight
//
// Purpose:
//        sets the gain for the input right line
void
PPCBurgundy::volumeMixerInRight(int vol)
{
    UInt8 right = scaleVolume(vol);

    writeCodecReg( ioBaseBurgundy, kVGA2Reg,   0x44 );
    writeCodecReg( ioBaseBurgundy, kGAS2RReg,  right );
}


/* =============================================================
 * VERY Private Methods used to access to the burgundy registers
 * ============================================================= */

static void
writeCodecReg( volatile UInt8 *ioBaseBurgundy, int regInfo, int value )
{
  u_int32_t		regBits;
  u_int32_t		regValue;
  u_int32_t		i;
  volatile u_int32_t	CodecControlReg;

  struct _reg
  {
      UInt8	unused0;
      UInt8  size;
      UInt8  addr;
      UInt8  offset;
  } *reg = (struct _reg *)&regInfo;


  regBits =   (kCodecCtlReg_Write | kCodecCtlReg_Reset)
            | ((u_int32_t) reg->addr                    * kCodecCtlReg_Addr_Bit)
            | ((u_int32_t)(reg->size + reg->offset - 1) * kCodecCtlReg_LastByte_Bit);

  for ( i=0; i < reg->size; i++ )
  {
      regValue = regBits | ((u_int32_t)(reg->offset + i) * kCodecCtlReg_CurrentByte_Bit) | (value & 0xFF);
      OSWriteLittleInt32(ioBaseBurgundy, kCodecCtlReg, regValue );
      eieio();

#ifdef DEBUGMODE
      IOLog( "PPCSound(burgundy): CodecWrite = %08x\n\r", regValue );
#endif

      value >>= 8;

      regBits &= ~kCodecCtlReg_Reset;

      do
      {
          CodecControlReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecCtlReg  );
          eieio();
      }
      while ( CodecControlReg & kCodecCtlReg_Busy );
  }
}


static int
readCodecReg( volatile UInt8 *ioBaseBurgundy, int regInfo )
{
  u_int32_t		regBits;
  u_int32_t		regValue = 0;
  u_int32_t		i,j;
  int			value;
  u_int32_t		byteCounter;
  u_int32_t		currentCounter;
  volatile u_int32_t	CodecControlReg;
  volatile u_int32_t	CodecStatusReg;


  struct _reg
  {
      UInt8	unused0;
      UInt8  size;
      UInt8  addr;
      UInt8  offset;
  } *reg = (struct _reg *)&regInfo;

  value   = 0;

  regBits =   (kCodecCtlReg_Reset)
            | ((u_int32_t) reg->addr                    * kCodecCtlReg_Addr_Bit)
            | ((u_int32_t)(reg->size + reg->offset - 1) * kCodecCtlReg_LastByte_Bit);

  CodecStatusReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  );
  eieio();
  byteCounter = ((CodecStatusReg & kCodecStatusReg_ByteCounter_Mask) / kCodecStatusReg_ByteCounter_Bit + 1) & 0x03;

  for ( i=0; i < reg->size; i++ )
  {
      regValue = regBits | ((u_int32_t)(reg->offset + i) * kCodecCtlReg_CurrentByte_Bit);
      OSWriteLittleInt32(ioBaseBurgundy, kCodecCtlReg, regValue );
      eieio();
      regBits &= ~kCodecCtlReg_Reset;

      do
      {
          CodecControlReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecCtlReg  );
          eieio();
      }
      while ( CodecControlReg & kCodecCtlReg_Busy );

      j=0;
      do
      {
          CodecStatusReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  );
          eieio();
          currentCounter = ((CodecStatusReg & kCodecStatusReg_ByteCounter_Mask) / kCodecStatusReg_ByteCounter_Bit) & 0x03;
      }
      while ( (byteCounter != currentCounter) && (j++ < 1000));

      byteCounter++;

      IODelay(10);
      CodecStatusReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  );

      value |= ((CodecStatusReg & kCodecStatusReg_Data_Mask) / kCodecStatusReg_Data_Bit) << 8*i;
  }

#ifdef DEBUGMODE
      IOLog( "PPCSound(burgundy): CodecRead = %08x %08x\n\r", regValue, value );
#endif

  return value;
}

static int
readCodecSenseLines( volatile UInt8 *ioBaseBurgundy )
{
    return ((OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  ) / kCodecStatusReg_Sense_Bit) & kCodecStatusReg_Sense_Mask);
}


static void
writeSoundControlReg( volatile UInt8 *ioBaseBurgundy, int value )
{

#ifdef DEBUGMODE
      IOLog( "PPCSound(burgundy): SoundControlReg = %08x\n", value);
#endif

  OSWriteLittleInt32( ioBaseBurgundy, kSoundCtlReg, value );
  eieio();
}  



 
