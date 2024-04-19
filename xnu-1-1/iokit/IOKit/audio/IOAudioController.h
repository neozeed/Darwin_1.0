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

/*!
 * @header AudioController
 * For controlling audio hardware, known as "codecs" to CPU S/W or "sound chips" or cards in the vernacular
 * Including but not limited to streaming of data in and out, as well as control parameters and topology
 */

#ifndef _IOAUDIOCONTOLLER_H
#define _IOAUDIOCONTOLLER_H

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/audio/IOAudioParts.h>

/*!
 * @class IOAudioController
 * @abstract Base class for abstracting streaming, control, and topology of audio hardware
 * @discussion Pressent a shared memory ring buffer for the DMA engine and provides auto erase capability
 */
class IOAudioController : public IOAudio
{	
    OSDeclareDefaultStructors(IOAudioController)

public:
    /*!
     * @function getMap
     * @abstract Get access to sample buffer and status for a stream
     * @param index
     * @param map
     * @result void
     */
    virtual void getMap(AudioStreamIndex index, IOAudioStreamMap * map);

    /*!
     * @function releaseMap
     * @abstract Call when done with stream
     * @param index
     * @result void
     */
    virtual void releaseMap(AudioStreamIndex index);
    
    /*!
     * @function setFlow
     * @abstract Control stream flow
     * @param index
     * @param flowing
     * @result void
     */
     virtual void setFlow(AudioStreamIndex index, bool flowing);

    /*
     * @function flushStream
     * @abstract Set the ending stream position gauranteed to be played after all clients have disconnected
     * @param endingPosition Ending position for the audio stream - gauranteed to be played
     * @result void
     */
    virtual void flushStream(AudioStreamIndex index, IOAudioStreamPosition *endingPosition);


    /*!
     * @function SetControl
     * @abstract Set a control to a value
     * @discussion Called by IOAudioControls, on the workloop thread.
     * @param id
     * @param val
     * @result IOReturn
     */
    virtual IOReturn SetControl(UInt16 id, int val) = 0;

    /*!
     * @function getWorkLoop
     * @result IOWorkLoop
     */
    virtual IOWorkLoop *getWorkLoop() const;

    /*!
     * @function setMasterVolumeLeft
     * @param newMasterVolumeLeft
     * @result void
     */
    virtual void setMasterVolumeLeft(UInt16 newMasterVolumeLeft);

    /*!
     * @function setMasterVolumeRight
     * @param newMasterVolumeRight
     * @result void
     */
    virtual void setMasterVolumeRight(UInt16 newMasterVolumeRight);

    /*!
     * @function setMasterMute
     * @param newMasterMute
     * @result void
     */
    virtual void setMasterMute(bool newMasterMute);

protected:
    /*!
     * @function free
     * @result void
     */
    virtual void free();

    /*!
     * @function createAudioStream
     * @abstract Create an audio stream.
     * @discussion Streams represent hardware input/output connectors such as headphone jacks,
     *  speakers, CD analog input etc. or I/O channels that the CPU can directly access such as
     *  simple D->A and A->D converters and more complex codecs.
     *  IOAudioController::createAudioStream can handle simple stream types
     *  (implemented by IOAudioStream and IOAudioMappedStream).
     *  Fancy things like streams with 3D positioning will probably need a new IOAudioStream subclass.
     * @param index
     * @result IOAudioStream A pointer if successful, NULL otherwise 
     */
    virtual IOAudioStream *createAudioStream(AudioStreamIndex index);

    /*!
     * @function getStreamProperties
     * @abstract Get properties of a (possibly not yet created) stream.
     * @discussion Properties are:
     *  Input or Output
     *  Directly Accessable by CPU
     *  Data type (16 bit samples, AC3 compressed etc)
     *  Sample size
     *  Channels (Mono/Stereo/5 channel/3D etc.
     *
     *  Must be provided by the driver.
     * @param index
     * @result OSDictionary
     */
    virtual OSDictionary * getStreamProperties(AudioStreamIndex index) = 0;

    /*!
     * @function probeStreams
     * @abstract How many cpu accessable streams does the device have?
     * @result int number of streams
     */
    virtual int probeStreams() = 0;

    /*!
     * @function CreateAudioTopology
     * @abstract Create all the AudioControls and AudioComponents for the device
     * @discussion This method is caled after creating the streams
     * @param queue
     * @result void
     */
    virtual void CreateAudioTopology(IOCommandQueue *queue) = 0;

    /*!
     * @function buildComponentAndAttach
     * @param parent
     * @param child
     * @param serialProps
     * @param queue
     * @result IOAudioComponentImpl
     */
    virtual IOAudioComponentImpl * buildComponentAndAttach(
		IORegistryEntry *parent, IORegistryEntry *child, 
		const char *serialProps, IOCommandQueue *queue);

    /*!
     * @function AttachComponents
     * @param parent
     * @param child
     * @result void
     */
    virtual void AttachComponents(IORegistryEntry *parent,
			IORegistryEntry *child);

    /*!
     * @function createWorkLoop
     * @abstract Get the workloop object for the hardware.
     * @discussion The default implementation creates a new workloop.
     *  Drivers may want to override this
     *  eg. USBAudio will probably want to use an existing USB workloop.
     * @result IOWorkLoop
     */
    virtual IOWorkLoop * createWorkLoop();

    /*!
     * @function startWorkLoop
     * @abstract Called from the device's start() method after hardware is initialized
     * @result void
     */
    virtual void startWorkLoop();

    /*!
     * @function getSharedStatus
     * @param index
     * @result IOAudioStreamStatus
     */
    virtual IOAudioStreamStatus * getSharedStatus(AudioStreamIndex index) = 0;

    /*!
     * @function getSampleBuffer
     * @param index
     * @result void *
     */
    virtual void * getSampleBuffer(AudioStreamIndex index) = 0;

    /*!
     * @function allocateMixBuffer
     * @abstract Allocates the mix buffer for the given stream.
     * @discussion Only called by execCommand()
     * @param index
     * @result void *
     */

    virtual void *allocateMixBuffer(AudioStreamIndex index);

    /*!
     * @function getMixBuffer
     * @param index
     * @result void *
     */

    virtual void *getMixBuffer(AudioStreamIndex index);

    /*!
     * @function startStream
     * @abstract Start a stream
     * @param index Stream index
     * @result IOAudioStreamStatus
     */
    virtual IOAudioStreamStatus * startStream(AudioStreamIndex index) = 0;

    /*!
     * @function startStream
     * @abstract Stop a stream
     * @param index Stream index
     * @result void
     */
    virtual void stopStream(AudioStreamIndex index) = 0;

    /*!
     * @function pauseStream
     * @abstract Control stream flow
     * @param index Audio stream
     * @result void
     */
    virtual void pauseStream(AudioStreamIndex index) = 0;

    /*!
     * @function resumeStream
     * @abstract Control stream flow
     * @param index Audio stream
     * @result void
     */
    virtual void resumeStream(AudioStreamIndex index) = 0;

    /*!
     * @function DoClockTick
     * @param IOTimerEventSource
     * @result void
     */
    virtual void DoClockTick(IOTimerEventSource *);

    /*!
     * @function calculateTickInterval
     * @param tickInterval
     * @result void
     */
    virtual void calculateTickInterval(AbsoluteTime *tickInterval);

    /*!
     * @function execCommand
     * @param cmd
     * @param field2
     * @param field3
     * @result void
     */
    virtual void execCommand(IOAudioCmd cmd, void *field2, void *field3);

    /*!
     * @function GetStream
     * @param index Audio stream index
     * @result IOAudioStream
     */
    IOAudioStream * GetStream(AudioStreamIndex index) const;

    /*!
     * @function init
     * @param properties
     * @result bool
     */
    virtual bool init(OSDictionary *properties);

    /*!
     * @function start
     * @param provider
     * @result bool
     */
    virtual bool start( IOService * provider );

public:
    static const IORegistryPlane * gIOAudioPlane;

    // Common IOSymbols, precooked and ready to use.
    static const OSSymbol * gValSym;
    static const OSSymbol * gInputsSym;
    static const OSSymbol * gControlsSym;
    static const OSSymbol * gJackSym;
    static const OSSymbol * gSpeakerSym;
    static const OSSymbol * gHeadphonesSym;
    static const OSSymbol * gLineOutSym;
    static const OSSymbol * gTypeSym;
    static const OSSymbol * gMuteSym;
    static const OSSymbol * gIdSym;
    static const OSSymbol * gMasterSym;

protected:
    OSArray *fStreams;

    IOService *fDevice;

    IOTimerEventSource *	fTimer;
    IOCommandQueue *		fQueue;
    IOWorkLoop *		fWorkLoop;
    AbsoluteTime		fTickPeriod;
    AbsoluteTime		fNextTick;
    bool			fTimerInUse;
    AbsoluteTime		fMaxPollInterval;
    OSSet *			fMasterVolumeComponents;
    UInt32			fNumStreamsInUse;

    static void execAndSignal(OSObject * obj, void *field0, void *field1, void *field2, void *field3);
    static void clockTick(OSObject *, IOTimerEventSource *sender);

    void setMasterComponentsProperties(OSDictionary *properties);
};

inline IOAudioStream * 
IOAudioController::GetStream(AudioStreamIndex index) const
{
    return (IOAudioStream *)fStreams->getObject(index);
}

#endif /* _IOAUDIOCONTOLLER_H */
