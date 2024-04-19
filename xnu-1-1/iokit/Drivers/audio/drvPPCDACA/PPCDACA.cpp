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
 * Interface implementation for the DAC 3550A audio Controller
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

// Driver headers
#include "daca_hw.h"
#include "PPCDACA_inlined.h"
#include "PPCDACA.h"

#define super IOAudioBus
OSDefineMetaClassAndStructors( PPCDACA, IOAudioBus )

/* ==============
 * Public Methods
 * ============== */
bool
PPCDACA::init(OSDictionary * properties)
{
    if (!super::init(properties))
            return false;
 
    // Nulls the pointers for the arrays we allocate:
    fOutputComponents = NULL;
    numOutputComponents = NULL;

    // Initialize  the defualt registers to non zero values (so they will be revitten)
    sampleRateReg = 0xFF;
    analogVolumeReg = kPowerOnDefaultAVOL;
    configurationReg = 0xFF;

    // Mirror the analogVolumeReg for the speaker and for the headphones.
    internalSpeakerVolume = analogVolumeReg;
    internalHeadphonesVolume = analogVolumeReg;

    // Last value of the status is 0
    lastStatus = 0;
 
    // Clears the interface
    interface = NULL;
    
    // Forget the provider:
    sound = NULL;

    // Initialize the sound format:
    dacaSerialFormat = kSndIOFormatUnknown;

    return true;
}

void
PPCDACA::free()
{
    // Releases the sound:
    if(sound != NULL)
        sound->release();
    sound = NULL;

    super::free();
}

IOService*
PPCDACA::probe(IOService* provider, SInt32* score)
{
    // Finds the possible candidate for sound, to be used in
    // reading the caracteristics of this hardware:
    IORegistryEntry *soundCandidate = provider->childFromPath("sound", gIODTPlane);

    if (super::probe(provider, score) && implementsDACA(soundCandidate)) {
#ifdef DEBUGMODE
        IOLog("PPCDACA::probe we are on a PPCDACA device !!\n");
#endif // DEBUGMODE

        // we did not fail:
        *score = kIODefaultProbeScore;
        sound = soundCandidate;
        return this;
    }

    return (NULL);
}

#define kNumDMAStreams 1
bool
PPCDACA::start(IOService* provider)
{
    // Gets the base for the DAC-3550 registers:
    IOMemoryMap *map;
    int i;

#ifdef DEBUGMODE
    // Delay for 30 seconds so we have the time to attach the debugger and look at
    // what is going on here (only if DEBUGMODE of course).
    IOLog("Starting DAC-3550 with provider %s ...", provider->getName());
    IOSleep(30 * 1000);
    IOLog("...\n");
#endif // DEBUGMODE

    if(!super::start(provider)) {
#ifdef DEBUGMODE
        IOLog("PPCDACA::start if(!super::start(provider)) fails !!\n");
#endif // DEBUGMODE
        return (false);
    }

    if (!findAndAttachI2C(provider))  {
#ifdef DEBUGMODE
        IOLog("PPCDACA::start if (!findAndAttachI2C(IOService *provider)) fails !!\n");
#endif // DEBUGMODE
        return (false);
    }

    map = provider->mapDeviceMemoryWithIndex(kAudioDMAdeviceInt);
    if(!map)  {
#ifdef DEBUGMODE
        IOLog("PPCDACA::start if(!map) %d fails !!\n",kAudioDMAdeviceInt);
#endif // DEBUGMODE
        return (false);
    }
    soundConfigSpace = (UInt8 *)map->getPhysicalAddress();

    if (!AllocateStreams(kNumDMAStreams)) {
#ifdef DEBUGMODE
        IOLog("PPCDACA::start if (!AllocateStreams(kNumDMAStreams)) fails !!\n");
#endif // DEBUGMODE
        return (false);
    }

    map = provider->mapDeviceMemoryWithIndex(kAudioDMAtxInt);
    if(!map)  {
#ifdef DEBUGMODE
        IOLog("PPCDACA::start if(!map) %d fails !!\n",kAudioDMAtxInt);
#endif // DEBUGMODE
        return (false);
    }
    DefineStream(kAudioDMAOutputStream, kOutput, frameRate(0), (IODBDMAChannelRegisters*)map->getVirtualAddress());

    // Creates the array of output components
    numOutputComponents = numberOfInputComponents();

    fOutputComponents = (IOAudioComponentImplPtr*)IOMalloc(sizeof(IOAudioComponentImplPtr) * numOutputComponents);
    if (fOutputComponents == NULL) {
#ifdef DEBUGMODE
        IOLog("PPCDACA::start if (fOutputComponents == NULL) fails !!\n");
#endif // DEBUGMODE

        // Free the DMA and exit:
        FreeStreams();
        return false;
    }

    // Initalizes all the components:
    for (i = 0; i < numOutputComponents; i++)
        fOutputComponents[i] = NULL;

    // sets the clock base address figuring out which I2S cell we're on
    if ((((UInt32)soundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) {
        ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S0BaseOffset);
        i2SInterfaceNumber = 0;
    }
    else if ((((UInt32)soundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) {
        ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S1BaseOffset);
        i2SInterfaceNumber = 1;
    }
    else {
        IOLog("PPCDACA::start failed to setup ioBaseAddress and ioClockBaseAddress\n");
    }

    // This is the keylargo FC1 (Feature configuration 1)
    ioClockBaseAddress = (void *)((UInt32)ioBaseAddress + kI2SClockOffset);

    // Finds the position of the status register:
    ioStatusRegister_GPIO12 = (void *)((UInt32)ioBaseAddress + kGPio12);

    // Enables the I2S Interface:
    KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) | kI2S0InterfaceEnable);
 
    // Starts the workloop and exit since there is nothing we can be interested about anymore
    startWorkLoop();

    return true;
}

void
PPCDACA::stop(IOService *provider)
{
    // Releases all DMA streams:
    FreeStreams();

    // Releases the output components
    if (fOutputComponents != NULL) {
        IOFree(fOutputComponents, sizeof(IOAudioComponentImplPtr) * numOutputComponents);
        fOutputComponents = NULL;
        numOutputComponents = 0;
    }

    // Call the parent stop:
    super::stop(provider);
}

void
PPCDACA::CreateAudioTopology(IOCommandQueue *queue)
{
    IOAudioComponentImpl *mixerOut;
    IOAudioComponentImpl *modem;
    IOAudioComponentImpl *headphones;
    IOAudioComponentImpl *cd;
    IOAudioComponentImpl *speaker;
    IOAudioComponentImpl *passthru;

    AudioStreamIndex outputStream;
    
    if (!fStreams)
        return;

    outputStream = firstStreamAfter(kOutput, 0);

    if (outputStream == kInvalidStreamIndex) {
        IOLog("PPCDACA::CreateAudioTopology called without available DMA streams\n");
        return;
    }
    else
        IOLog("PPCDACA::CreateAudioTopology output stream is %d\n", outputStream);

    mixerOut = buildComponentAndAttach(GetStream(outputStream), NULL, componentDictionaryMixerOut(), queue);
    headphones = buildComponentAndAttach(mixerOut, NULL, componentDictionaryHeadphones(), queue);
    speaker = buildComponentAndAttach(mixerOut, NULL, componentDictionarySpeaker(), queue);

    cd = buildComponentAndAttach(NULL, NULL,  componentDictionaryCD(), queue);
    modem = buildComponentAndAttach(NULL, NULL, componentDictionaryModem(), queue);

    passthru = buildComponentAndAttach(NULL, mixerOut, componentDictionaryPassThru(), queue);

    // FIXME: the component connection could be done better than so
    if (fOutputComponents != NULL) {
        fOutputComponents[kSpeaker] = speaker;

        if (numOutputComponents > 1)
            fOutputComponents[kHeadphones] = headphones;
    }

    // Sets ups the DAC-3550 as required by this machine wiring
    // configuration:
    if (!dependentSetup())
        IOLog("PPCDACA::CreateAudioTopology DAC-3550 setup failed\n");
}

void
PPCDACA::DoClockTick(IOTimerEventSource *t)
{
    checkStatusRegister();
    super::DoClockTick(t);
}

void
PPCDACA::calculateTickInterval(AbsoluteTime *tickInterval)
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
PPCDACA::SetControl(UInt16 id, int val)
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

        case kModemMute:
            setModemInput(val == 0);
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
// Method: findAndAttachI2C
//
// Purpose:
//   Attaches to the i2c interface:
bool
PPCDACA::findAndAttachI2C(IOService *provider)
{
    OSData *t;
    UInt32 baseAddress;
    UInt32 addressSteps;
    UInt32 rate;
    IORegistryEntry *i2cCandidate;

    // Searches the i2c:
    for (i2cCandidate = NULL;
         (provider != NULL) && (i2cCandidate == NULL);
         provider = provider->getProvider()) {
        i2cCandidate = provider->childFromPath("i2c", gIODTPlane);
#ifdef DEBUGMODE
        IOLog("Looking for i2c in %s -> 0x%08lx\n", provider->getName(), (UInt32)i2cCandidate);
#endif // DEBUGMODE
    }

    if (i2cCandidate == NULL) {
#ifdef DEBUGMODE
        IOLog("PPCDACA::findAndAttachI2C can't find the i2c in the registry\n");
#endif // DEBUGMODE
        return false;
    }

    // creates the interface for real:
    interface = new PPCI2CInterface;
    if (interface == NULL) {
#ifdef DEBUGMODE
        IOLog("PPCDACA::findAndAttachI2C can't allocate memory for the PPCI2CInterface\n");
#endif // DEBUGMODE
        return false;
    }

    // sets up the interface:
    t = OSDynamicCast(OSData, i2cCandidate->getProperty("AAPL,address"));
    if (t != NULL)
        baseAddress = *((UInt32*)t->getBytesNoCopy());
    else {
#ifdef DEBUGMODE
        IOLog( "PPCDACA::findAndAttachI2C missing property AAPL,address in i2c registry\n");
#endif
        return false;
    }

    t = OSDynamicCast(OSData, i2cCandidate->getProperty("AAPL,address-step"));
    if (t != NULL)
        addressSteps = *((UInt32*)t->getBytesNoCopy());
    else {
#ifdef DEBUGMODE
        IOLog( "PPCDACA::findAndAttachI2C missing property AAPL,address-step in i2c registry\n");
#endif
        return false;
    }

    t = OSDynamicCast(OSData, i2cCandidate->getProperty("AAPL,i2c-rate"));
    if (t != NULL)
        rate = *((UInt32*)t->getBytesNoCopy());
    else {
#ifdef DEBUGMODE
        IOLog( "PPCDACA::findAndAttachI2C missing property AAPL,i2c-rate in i2c registry\n");
#endif
        return false;
    }

    if (!interface->initI2CBus((UInt8*)baseAddress, (UInt8)addressSteps))
        return false;

    if (!interface->setKhzSpeed((UInt)rate))
        return false;

    // Also sets the default mode
    interface->setStandardSubMode();
  
    // Finds the port to use:
    return(interface->openI2CBus(getI2CPort()));
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//   detaches from the I2C
bool
PPCDACA::detachFromI2C(IOService* /*provider*/)
{
    if (interface)
        delete interface;

    return (true);
}

// --------------------------------------------------------------------------
// Method: implementsDACA
//
// Purpose:
//   Attempts to discover if the device implements DAC-3550:
bool
PPCDACA::implementsDACA(IORegistryEntry *device)
{
    if (device != NULL) {
        OSData *s = NULL;

#ifdef DEBUGMODE
        IOLog("Matching DAC-3550 compatibility with %s\n", device->getName());
#endif // DEBUGMODE
        s = OSDynamicCast(OSData, device->getProperty("compatible"));

        if (s != NULL) {
            if(s->isEqualTo("daca", sizeof("daca")-1)) {
                return true;
            }
            else {
#ifdef DEBUGMODE
                IOLog("PPCDAC-3550::probe sound is compatible with %s.\n", (char*)s->getBytesNoCopy(0, sizeof("daca")-1));
#endif // DEBUGMODE
            }
        }
        else {
#ifdef DEBUGMODE
            IOLog("PPCDAC-3550::probe sound does not have a compatible property.\n");
#endif // DEBUGMODE
        }
    }
    else{
#ifdef DEBUGMODE
        IOLog("PPCDAC-3550::probe this hardware does not have a sound chip\n");
#endif // DEBUGMODE
    }
    return false;
}

// --------------------------------------------------------------------------
// Method: numberOfInputComponents
//
// Purpose:
//        returns the number of components for the input lines
//        the name registry describes the CHIP (which has 3 inputs
//	  however, the iBook has no inputs, so I have to force 0
//        as output).
#define kCommonNumberOfInputComponents 0

int
PPCDACA::numberOfInputComponents()
{
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
PPCDACA::numberOfOutputComponents()
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
// Method: frameRate
//
// Purpose:
//        returns the frame rate as in the registry, if it is
//        not found in the registry, it returns the default value.
#define kCommonFrameRate 44100

UInt32
PPCDACA::frameRate(UInt32 index)
{
    if(sound) {
        OSData *t;

        t = OSDynamicCast(OSData, sound->getProperty("sample-rates"));
        if (t != NULL) {
            UInt32 *fArray = (UInt32*)(t->getBytesNoCopy());

            if ((fArray != NULL) && (index < fArray[0])){
                // I could do >> 16, but in this way the code is portable and
                // however any decent compiler will recognize this as a shift
                // and do the right thing.
                UInt32 fR = fArray[index + 1] / (UInt32)65536;

#ifdef DEBUGMODE
                IOLog( "PPCDACA::frameRate (%ld)\n",  fR);
#endif
                return fR;
            }
        }
    }

    return (UInt32)kCommonFrameRate;
}

// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//        returns the i2c port to use for the audio chip.
UInt32
PPCDACA::getI2CPort()
{
    if(sound) {
        OSData *t;

        t = OSDynamicCast(OSData, sound->getProperty("AAPL,i2c-port-select"));
        if (t != NULL) {
            UInt32 myPort = *((UInt32*)t->getBytesNoCopy());
            return myPort;
        }
#ifdef DEBUGMODE
        else
            IOLog( "PPCDACA::getI2CPort missing property port\n");
#endif
    }
    
    return 0;
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
PPCDACA::componentDictionarySpeaker()
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
PPCDACA::componentDictionaryHeadphones()
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
PPCDACA::componentDictionaryModem()
{
    static const char *sModem =
    "{
    'Type' = 'Modem';
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
    return (char*)sModem;
}

char*
PPCDACA::componentDictionaryCD()
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
PPCDACA::componentDictionaryMixerOut()
{
    static const char *sMixerOut =
    "{
    'Type' = 'Mixer';
    'Channels' = 2:8;
    }";
    return (char*)sMixerOut;
}

char*
PPCDACA::componentDictionaryPassThru()
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
//        this handles the setup of the DAC-3550 chip for each kind of
//        hosting hardware.
bool
PPCDACA::dependentSetup()
{
    // Sets the frame rate:
    UInt32 myFrameRate = frameRate(0);

    dacaSerialFormat = kSndIOFormatI2SSony;			// start out in Sony format

    // Reads the initial status of the DAC3550A registers:
    // The following functions return "false" on the first generation of iBooks. However I wish to
    // keep them, since:
    // 1] it is bad to make assumptions on what the hardware can do and can not do here (eventually these
    //    assumptions should be made in the i2c driver).
    // 2] the next generation of iBook may supprot reading the DACA registers, and we will have more precise
    //    values in the "mirror" registers.
    if (interface != NULL) {
        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, & sampleRateReg, sizeof(sampleRateReg));
        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8*)& analogVolumeReg, sizeof(analogVolumeReg));
        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, & configurationReg, sizeof(configurationReg));
    }

    // If nobody set a format before choose one:
    if (dacaSerialFormat ==  kSndIOFormatUnknown)
        dacaSerialFormat = kSndIOFormatI2SSony;

    // This call will set the next of the drame parametes
    // (dacaClockSource, dacaMclkDivisor,  dacaSclkDivisor)
    if (!setSampleParameters(myFrameRate, 0)) {
        IOLog("PPCDACA::dependentSetup can not set i2s sample rate\n");
        return false;
    }
    else if (!setDACASampleRate(myFrameRate)) {
        IOLog("PPCDACA::dependentSetup can not set DACA sample rate\n");
        return false;
    }
    else 
        setSerialFormatRegister(dacaClockSource, dacaMclkDivisor, dacaSclkDivisor, dacaSerialFormat);

    // Sets the molumes to max as in the audio registry:
    volumeInternalSpeakerLeft(65535);
    volumeInternalSpeakerRight(65535);
    volumeHeadphonesLeft(65535);
    volumeHeadphonesRight(65535);
    volumeCDLineLeft(65535);
    volumeCDLineRight(65535);

    return true;
}

// --------------------------------------------------------------------------
// Method: checkStatusRegister
//
// Purpose:
//        if the argument is true mutes the internal speaker, otherwise
//        it "unmutes" it.
void
PPCDACA::checkStatusRegister()
{
    if (statusChanged())  {
        int i;

        OSNumber * num = OSNumber::withNumber(lastStatus, 8);
        setProperty("Status", num);
        num->release();

#ifdef DEBUGMODE
        IOLog("New Status = 0x%02x\n", lastStatus);
#endif

        for (i = 0; i < numOutputComponents; i++)
            if (fOutputComponents[i] != NULL)
                fOutputComponents[i]->Set(gInputsSym, gJackSym, outputComponentStatus(i));
    }
}

// --------------------------------------------------------------------------
// Method: outputComponentStatus
//
// Purpose:
//        checks the status of the jack line of the ith output component:
bool
PPCDACA::outputComponentStatus(int index)
{
    switch(index)
    {
        case kSpeaker:
            return false;	// the internal speaker does not have a jack

        case kHeadphones:	// check the headphones:
            return headphonesInserted();
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
PPCDACA::muteInternalSpeaker(bool mute)
{
    if (mute){
        if (!headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, 0, kRightAVOLMask | kLeftAVOLMask);
        internalSpeakerMuted = mute;
    }
    else {
        internalSpeakerMuted = mute;

        if (!headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, internalSpeakerVolume, kRightAVOLMask | kLeftAVOLMask);
    }
}

// --------------------------------------------------------------------------
// Method: muteHeadphones
//
// Purpose:
//        if the argument is true mutes the rear panel mini jack, otherwise
//        it "unmutes" it.
void
PPCDACA::muteHeadphones(bool mute)
{
    if (mute){
        if (headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, 0, kRightAVOLMask | kLeftAVOLMask);
        internalHeadphonesMuted = mute;       
    }
    else {
        internalHeadphonesMuted = mute;        

        if (headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, internalHeadphonesVolume, kRightAVOLMask | kLeftAVOLMask);
    }
}

// --------------------------------------------------------------------------
// Method: muteCDLine
//
// Purpose:
//        if the argument is true mutes the line of the cd player
void
PPCDACA::muteCDLine(bool mute)
{
    // FIXME: Figure out how this is wired up:
}

// --------------------------------------------------------------------------
// Method: setModemInput
//
// Purpose:
//        selects the modem as input source (if bool = true)
void
PPCDACA::setModemInput(bool mod)
{
#ifdef DEBUGMODE
    IOLog( "PPCDACA::setModemInput: %s\n", (mod ? "True" : "False"));
#endif
}


// --------------------------------------------------------------------------
// Method: statusChanged()
//
// Purpose:
//        returns true if something changed in the status register:.
bool
PPCDACA::statusChanged()
{
    UInt8 currentStatusRegister = *(UInt8*)ioStatusRegister_GPIO12;

    if (lastStatus == currentStatusRegister)
        return (false);

    // Otherwise refresh the value of the last status register read and
    // returns that the value changed:
    lastStatus = currentStatusRegister;
    return (true);
}

// --------------------------------------------------------------------------
// Method: headphonesInserted
//
// Purpose:
//        returns true if the headphones are inserted.
bool
PPCDACA::headphonesInserted()
{
    return((lastStatus & kHeadphoneBit) == 0);
}

// --------------------------------------------------------------------------
// Method: volumeHeadphonesLeft & volumeHeadphonesRight
//
// Purpose:
//        sets the volume for the left and right channel of the internal
//        headphones.
void
PPCDACA::volumeHeadphonesLeft(int vol)
{
    analogVolumeReg = internalHeadphonesVolume;
    setDACAVolume(vol, true);
    internalHeadphonesVolume = analogVolumeReg;
}

void
PPCDACA::volumeHeadphonesRight(int vol)
{
    analogVolumeReg = internalHeadphonesVolume;
    setDACAVolume(vol, false);
    internalHeadphonesVolume = analogVolumeReg;
}

// --------------------------------------------------------------------------
// Method: volumeInternalSpeakerLeft & volumeInternalSpeakerRight
//
// Purpose:
//        sets the volume for the left and right channel of the internal
//        speaker.
void
PPCDACA::volumeInternalSpeakerLeft(int vol)
{
    analogVolumeReg = internalSpeakerVolume;
    setDACAVolume(vol, true);
    internalSpeakerVolume = analogVolumeReg;
}

void
PPCDACA::volumeInternalSpeakerRight(int vol)
{
    analogVolumeReg = internalSpeakerVolume;
    setDACAVolume(vol, false);
    internalSpeakerVolume = analogVolumeReg;
}

// --------------------------------------------------------------------------
// Method: volumeCDLineLeft & volumeCDLineRight
//
// Purpose:
//        sets the volume for the left and right channel of the CD
//        line.
void
PPCDACA::volumeCDLineLeft(int vol)
{
}

void
PPCDACA::volumeCDLineRight(int vol)
{
}

/* =============================================================
 * VERY Private Methods used to access to the DAC-3550 registers
 * ============================================================= */

// --------------------------------------------------------------------------
// Method: enableInterrupts
//
// Purpose:
//         enable the interrupts for the I2S interface:
bool
PPCDACA::enableInterrupts()
{
    I2SSetIntCtlReg(kFrameCountEnable | kClocksStoppedEnable);
    return true;
}

// Method: disableInterrupts
//
// Purpose:
//         disable the interrupts for the I2S interface:
bool
PPCDACA::disableInterrupts()
{
    I2SSetIntCtlReg(I2SGetIntCtlReg() & (~(kFrameCountEnable | kClocksStoppedEnable)));
    return true;
}

// --------------------------------------------------------------------------
// Method: clockRun
//
// Purpose:
//         starts and stops the clock count:
bool
PPCDACA::clockRun(bool start)
{
    bool success = true;
    UInt32 *baseInterrupt = (UInt32*)soundConfigSpace + kI2SIntCtlOffset;

#ifdef DEBUGMODE
    IOLog("PPCDACA::clockRun(%s) 1] 0x%08lx -> 0x%08lx\n", (start ? "true" : "false"), (UInt32)baseInterrupt, *baseInterrupt);
#endif

    if (start) {
        KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) | kI2S0ClockEnable);
    }
    else {
        UInt16 loop = 50;
        KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) & (~kI2S0ClockEnable));
        
        while (((I2SGetIntCtlReg() & kClocksStoppedPending) == 0) && (loop--)) {
            // it does not do anything, jut waites for the clock
            // to stop
            IOSleep(10);
#ifdef DEBUGMODE
            IOLog("PPCDACA::clockRun(%s) 2] 0x%08lx -> 0x%08lx\n", (start ? "true" : "false"), (UInt32)baseInterrupt, *baseInterrupt);
#endif
        }

        // we are successful if the clock actually stopped.
        success =  ((I2SGetIntCtlReg() & kClocksStoppedPending) != 0);
    }

#ifdef DEBUGMODE
    if (!success)
        IOLog("PPCDACA::clockRun(%s) failed\n", (start ? "true" : "false"));
#endif

    return success;
}

// --------------------------------------------------------------------------
// Method: setSampleRate
//
// Purpose:
//        Sets the sample rate on the I2S bus
bool
PPCDACA::setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio)
{
    UInt32	mclkRatio;
    UInt32	reqMClkRate;

    mclkRatio = mclkToFsRatio;			// remember the MClk ratio required

    if ( mclkRatio == 0 )																			       // or make one up if MClk not required
        mclkRatio = 64;				// use 64 x ratio since that will give us the best characteristics
    
    reqMClkRate = sampleRate * mclkRatio;	// this is the required MClk rate

    // look for a source clock that divides down evenly into the MClk
    if ((kClock18MHz % reqMClkRate) == 0) {
        // preferential source is 18 MHz
        dacaClockSource = kClock18MHz;
    }
    else if ((kClock45MHz % reqMClkRate) == 0) {
        // next check 45 MHz clock
        dacaClockSource = kClock45MHz;
    }
    else if ((kClock49MHz % reqMClkRate) == 0) {
        // last, try 49 Mhz clock
        dacaClockSource = kClock49MHz;
    }
    else {
        IOLog("PPCDACA::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)\n");
        return false;
    }

    // get the MClk divisor
    IOLog("PPCDACA:setSampleParameters %ld / %ld =", (UInt32)dacaClockSource, (UInt32)reqMClkRate); 
    dacaMclkDivisor = dacaClockSource / reqMClkRate;
    IOLog("%ld\n", dacaMclkDivisor);
    switch (dacaSerialFormat)					// SClk depends on format
    {
        case kSndIOFormatI2SSony:
        case kSndIOFormatI2S64x:
            dacaSclkDivisor = mclkRatio / k64TicksPerFrame;	// SClk divisor is MClk ratio/64
            break;
        case kSndIOFormatI2S32x:
            dacaSclkDivisor = mclkRatio / k32TicksPerFrame;	// SClk divisor is MClk ratio/32
            break;
        default:
            IOLog("PPCDACA::setSampleParameters Invalid serial format\n");
            return false;
            break;
    }

    return true;
 }


// --------------------------------------------------------------------------
// Method: setSerialFormatRegister
//
// Purpose:
//        Set global values to the serial format register
void
PPCDACA::setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat)
{
    UInt32	regValue = 0;

#ifdef DEBUGMODE
    IOLog("PPCDACA::SetSerialFormatRegister(%d,%d,%d,%d)\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
#endif // DEBUGMODE

    switch ((int)clockSource)
    {
        case kClock18MHz:
            regValue = kClockSource18MHz;
            break;
        case kClock45MHz:
            regValue = kClockSource45MHz;
            break;
        case kClock49MHz:
            regValue = kClockSource49MHz;
            break;
        default:
            IOLog("PPCDACA::SetSerialFormatRegister(%d,%d,%d,%d): Invalid clock source\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
            break;
    }

    switch (mclkDivisor)
    {
        case 1:
            regValue |= kMClkDivisor1;
            break;
        case 3:
            regValue |= kMClkDivisor3;
            break;
        case 5:
            regValue |= kMClkDivisor5;
            break;
        default:
            regValue |= (((mclkDivisor / 2) - 1) << kMClkDivisorShift) & kMClkDivisorMask;
            break;
    }

    switch ((int)sclkDivisor)
    {
        case 1:
            regValue |= kSClkDivisor1;
            break;
        case 3:
            regValue |= kSClkDivisor3;
            break;
        default:
            regValue |= (((sclkDivisor / 2) - 1) << kSClkDivisorShift) & kSClkDivisorMask;
            break;
    }
    regValue |= kSClkMaster;										// force master mode

    switch (serialFormat)
    {
        case kSndIOFormatI2SSony:
            regValue |= kSerialFormatSony;
            break;
        case kSndIOFormatI2S64x:
            regValue |= kSerialFormat64x;
            break;
        case kSndIOFormatI2S32x:
            regValue |= kSerialFormat32x;
            break;
        default:
            IOLog("PPCDACA::SetSerialFormatRegister(%d,%d,%d,%d): Invalid serial format\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
            break;
    }

    // This is a 3 step process:

    // 1] Stop the clock:
    clockRun(false);

    // 2] Setup the serial format register
    I2SSetSerialFormatReg(regValue);

    // 3 restarts the clock:
    clockRun(true);    
}

// --------------------------------------------------------------------------
// Method: setDACASampleRate
//
// Purpose:
//        Gets the sample rate and makes it in a format that is compatible
//        with the adac register. The funtion returs false if it fails.
bool
PPCDACA::setDACASampleRate(UInt rate)
{
    UInt32 dacRate = 0;

#ifdef DEBUGMODE
    IOLog("PPCDACA::setDACASampleRate(%d)\n", rate);
#endif // DEBUGMODE

        switch (rate)
        {
                case 44100: 				// 32 kHz - 48 kHz
                        dacRate = kSRC_48SR_REG;
                        break;

                default:
                IOLog("PPCDACA::setDACASampleRate, supports only 44100 Hz (for now)\n");
                        break;
        }

        return(writeRegisterBits(i2cBusSubAddrSR_REG, dacRate, kSampleRateControlMask));
}


// --------------------------------------------------------------------------
// Method: setDACAVolume
//
// Purpose:
//        sets the volume for the left or right channel. The first argument is
//        the wnated volume (as a range from 0 to 255), while the second argument
//        is a boolean (true for the right channel, false for the left channel).
bool
PPCDACA::setDACAVolume(UInt16 volume, bool isLeft)
{
    UInt8 newVolume = ((UInt32)((UInt32)volume * (UInt32)kVolumeRangeLevel_VOL) / (UInt32)65535 + (UInt32)kMinVolumeLevel_VOL);
    bool success = true;

    if (isLeft) // Sets the volume on the right channel
        success = writeRegisterBits(i2cBusSubAddrAVOL, (newVolume << kLeftAVOLShift), kLeftAVOLMask);
    else	// Sets the volume on the left channel
        success = writeRegisterBits(i2cBusSubAddrAVOL, (newVolume << kRightAVOLShift), kRightAVOLMask);

    return success;
}

// ---------------------------------------------------------------------------------------
// Method: writeRegisterBits
//
// Purpose:
//      This function sets or clears bits in the Daca registers.  The first argument is the
//      register sub-address, the second argument is a mask for turning bits on, while the third
//      argument is a mask for turning bits off.
bool
PPCDACA::writeRegisterBits(UInt8 subAddress, UInt32 bitMaskOn, UInt32 bitMaskOff)
{
    UInt8 bitsOn = 0, bitsOff = 0, value;
    UInt16 shortBitsOn = 0, shortBitsOff = 0, value16;
    bool success = false;

#ifdef DEBUGMODE
    IOLog("PPCDACA::writeRegisterBits(0x%x, 0x%08lx, 0x%08lx)\n", (UInt16)subAddress, bitMaskOn, bitMaskOff);
#endif // DEBUGMODE

    // mask off irrelevant bytes
    if (subAddress == i2cBusSubAddrAVOL) {
        // 16-bit register
        shortBitsOn = bitMaskOn & 0x0000FFFF;
        shortBitsOff = bitMaskOff & 0x0000FFFF;
    }
    else {
        // 8-bit registers
        bitsOn = bitMaskOn & 0x000000FF;
        bitsOff = bitMaskOff & 0x000000FF;
    }

    switch (subAddress)
    {
        case i2cBusSubAddrSR_REG:
            value = sampleRateReg | bitsOn;
            value &= ~bitsOff;
            // continue only if on bits are not already on and off bits are not already off
            if (value != sampleRateReg) {
                success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, subAddress, &value, sizeof(value));

                if (success)
                    sampleRateReg = value;
            }
                else {
                    // If the sample rate is already set at the wanted value
                    // we do not do anything, but we return success since from the
                    // caller point of view it is as the sample rate was correctly
                    // set.
                    success = true;
                }
                break;

        case i2cBusSubAddrAVOL:
            value16 = analogVolumeReg;
            value16 &= ~shortBitsOff;
            value16 |= shortBitsOn;
            // continue only if on bits are not already on and off bits are not already off
            if (value16 !=analogVolumeReg)
            {
                // It changes the volume for real only if the current output is not muted.
                if (((!headphonesInserted()) && (!internalSpeakerMuted)) ||
                    (headphonesInserted() && (!internalHeadphonesMuted)))
                    success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, subAddress, (UInt8*)&value16, sizeof(value16));
                else
                    success = true;

                if (success)
                    analogVolumeReg = value16;
            }
                else {
                    // If the volume is already set at the wanted value
                    // we do not do anything, but we return success since from the
                    // caller point of view it is as the volume was correctly
                    // set.
                    success = true;
                }
                break;

        case i2cBusSubaddrGCFG:
            value = configurationReg | bitsOn;
            value &= ~bitsOff;
            // continue only if on bits are not already on and off bits are not already off
            if (value != configurationReg)
            {
                success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, subAddress, &value, sizeof(value));
                if (success)
                    configurationReg = value;
            }
                else {
                    // If the config register is already set at the wanted value
                    // we do not do anything, but we return success since from the
                    // caller point of view it is as the configuration was correctly
                    // set.
                    success = true;
                }
                break;

        default:
            IOLog("PPCDACA::writeRegisterBits 0x%x unknown subaddress\n", (UInt16)subAddress);
            break;
    }
    return success;
}
