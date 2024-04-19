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
 * @header IOAudioBus
 * Interface definition for the AudioBus audio Controller
 * !! WARNING !! This is a temporary interface until the full bus abstraction has been worked out.
 */

#ifndef _IOAUDIOBUS_H
#define _IOAUDIOBUS_H

#include <IOKit/audio/IOAudio.h>
#include <IOKit/audio/IOAudioParts.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/assert.h>
#include <IOKit/audio/IOAudioController.h>
#include <IOKit/IOFilterInterruptEventSource.h>

/*!
 * @enum StreamDirection
 * @constant kOutput
 * @constant kInput
 */
enum StreamDirection
{
    kOutput,
    kInput
};

/*!
 * @defined kInvalidStreamIndex
 * @discussion Defines the invalid stream index value
 */
#define kInvalidStreamIndex	-1

#ifdef __ppc__
#include <IOKit/ppc/IODBDMA.h>

/*!
 * @class IOAudioBus
 * @abstract This class handles all the DMA on PPC hardware
 * @discussion FIXME: where it says __ppc__ it should be something like POWERRMAC
 *  since we can have PPC hardware with a different audio DMA implementation
 */
class IOAudioBus: public IOAudioController
{
    OSDeclareDefaultStructors(IOAudioBus)

    /*!
     * @struct StreamDMAInfo
     * @abstract Maintains information relevant to DMA operations
     * @field fSharedStatus
     * @field fIOBaseDMA
     * @field fCmds
     * @field fDmaCmd
     * @field fCmdSize
     * @field fNeedsErase
     * @field fIsInput
     * @field fSampleRate
     * @field streamProperty
     */
    typedef struct StreamDMAInfo {
        IOAudioStreamStatus * 			fSharedStatus;
        volatile IODBDMAChannelRegisters *	fIOBaseDMA;
        IODBDMADescriptor *			fCmds;
        int16_t *				fSampleBuffer;
        u_int32_t				fDmaCmd;
        int					fCmdSize;
        int					fNeedsErase;
        bool					fIsInput;
        UInt32					fSampleRate;
        char *                                  streamProperty;
    } StreamDMAInfo;

    UInt16				numDMAStreams;
    StreamDMAInfo			*ioAudioStreamsDMA;
    int					fBufMax;
    int					fBlockSize;
    IOFilterInterruptEventSource *	fTxInterruptSource;
    IOFilterInterruptEventSource *	fRxInterruptSource;
    
public:
    virtual bool init(OSDictionary *properties = 0);
    virtual void free();

    /*! 
     * @function AllocateStreams
     * @abstract Creates n empty streams (also a method to free them)
     * @param numberOfStreams
     * @result bool
     */
    virtual bool AllocateStreams(int numberOfStreams);

    /*!
     * @function FreeStreams
     * @abstract Deallocate audio streams
     * @result bool
     */
    virtual bool FreeStreams();

    /*!
     * @function DefineStream
     * @abstract For each stream defines its properties
     * @param index
     * @param direction
     * @param rate
     * @param base
     * @result bool
     */
    virtual bool DefineStream(AudioStreamIndex index, int direction, UInt32 rate, IODBDMAChannelRegisters *base);

    /*!
     * @function firstStreamAfter
     * @abstract Returns the first stream in the given direction after the given index
     * @discussion This is useful if we have to handle more than one stream for input and output
     * @param inDirection
     * @param afterIndex afterIndex is inclusive
     * @result AudioStreamIndex returns kInvalidStreamIndex upon failure
     */
    virtual AudioStreamIndex firstStreamAfter(int inDirection, AudioStreamIndex afterIndex);

    /*!
     * @function getStreamProperties
     * @param AudioStreamIndex
     * @result OSDictionary The dictionary of properties
     */
    virtual OSDictionary* getStreamProperties(AudioStreamIndex);

    /*! @function probeStreams */
    virtual int probeStreams();

    /*!
     * @function IOAudioStreamStatus
     * @param AudioStreamIndex
     * @result IOAudioStreamStatus
     */
    virtual struct IOAudioStreamStatus* getSharedStatus(AudioStreamIndex);

    /*!
     * @function getSampleBuffer
     * @param AudioStreamIndex
     * @result void*
     */
    virtual void* getSampleBuffer(AudioStreamIndex);

    /*!
     * @function startStream
     * @param AudioStreamIndex
     * @result IOAudioStreamStatus
     */
    virtual struct IOAudioStreamStatus* startStream(AudioStreamIndex);

    /*!
     * @function stopStream
     * @param AudioStreamIndex
     * @result void
     */
    virtual void stopStream(AudioStreamIndex);

    /*!
     * @function pauseStream
     * @param AudioStreamIndex
     * @result void
     */
    virtual void pauseStream(AudioStreamIndex);

    /*!
     * @function resumeStream
     * @param AudioStreamIndex
     * @result void
     */
    virtual void resumeStream(AudioStreamIndex);

    /*!
     * @function createAudioStream
     * @param AudioStreamIndex
     * @result IOAudioStream
     */
    virtual IOAudioStream *createAudioStream(AudioStreamIndex index);

protected:
    /*!
     * @var const kAudioDMAdeviceInt = 0
     * @discussion These constants define the one-to-one relationship between the transmit interrupt and a single output stream
     * and the receive interrupt and a single input stream.
     * It is up to the subclass to ensure that these relationships are properly used.
     */
    static const int kAudioDMAdeviceInt;

    /*! @var const kAudioDMAtxInt = 1 */
    static const int kAudioDMAtxInt;

    /*! @var const kAudioDMAtxInt = 2 */
    static const int kAudioDMArxInt;

    /*! @var const kAudioDMAtxInt = 0 */
    static const AudioStreamIndex kAudioDMAOutputStream;

    /*! @var const kAudioDMAtxInt = 1 */
    static const AudioStreamIndex kAudioDMAInputStream;

    /*! 
     * @function startWorkLoop
     * @result void
     */
    virtual void startWorkLoop();

    /*! 
     * @function registerInterrupts
     * @result void
     */
    virtual void registerInterrupts();

    /*!
     * @function filterInterrupt
     * @param index
     * @result bool
     */
    virtual bool filterInterrupt(int index);

    /*!
     * @function getStreamForInterrupt
     * @param index
     * @result AudioStreamIndex
     */
    virtual AudioStreamIndex getStreamForInterrupt(int index);

    /*!
     * @function AudioBusInterruptFilter
     * @param owner
     * @param source
     * @result bool
     */
    static bool AudioBusInterruptFilter(OSObject *owner,
                                        IOFilterInterruptEventSource *source);

    /*!
     * @function AudioBusInterruptHandler
     * @param owner
     * @param source
     * @param count
     * @result bool
     */
    static void AudioBusInterruptHandler(OSObject *owner,
                                         IOInterruptEventSource * source,
                                         int count);
};

#else // ! ppc

/*!
 * @class IOAudioBus
 * @abstract For non PPC hardware the calss still exists but the driver must implement all the DMA handling
 */
class IOAudioBus: public IOAudioController
{
    OSDeclareDefaultStructors(IOAudioBus)
};

#endif //ppc

#endif //_IOAUDIOBUS_H
