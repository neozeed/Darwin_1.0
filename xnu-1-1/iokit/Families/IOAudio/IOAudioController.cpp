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
 * IOAudioController.cpp
 *
 * HISTORY
 *
 */

#include <IOKit/assert.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandQueue.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/audio/IOAudioController.h>

#include <libkern/c++/OSSet.h>

const IORegistryPlane * IOAudioController::gIOAudioPlane = NULL;
const OSSymbol * IOAudioController::gValSym = NULL;
const OSSymbol * IOAudioController::gTypeSym = NULL;
const OSSymbol * IOAudioController::gInputsSym = NULL;
const OSSymbol * IOAudioController::gControlsSym = NULL;
const OSSymbol * IOAudioController::gJackSym = NULL;
const OSSymbol * IOAudioController::gSpeakerSym = NULL;
const OSSymbol * IOAudioController::gHeadphonesSym = NULL;
const OSSymbol * IOAudioController::gLineOutSym = NULL;
const OSSymbol * IOAudioController::gMuteSym = NULL;
const OSSymbol * IOAudioController::gIdSym = NULL;
const OSSymbol * IOAudioController::gMasterSym = NULL;

#define TIMER_INTS_PER_BUF	4
#define MAX_POLL_INTERVAL_SEC	60

//************************************************************************
// Implementation of protocol clas.
//************************************************************************
OSDefineMetaClass(  IOAudio, IOService )
OSDefineAbstractStructors(  IOAudio, IOService )

//************************************************************************
// Begin implementation of IOAudioController class.
//************************************************************************

#undef super
#define super IOAudio

OSDefineMetaClass(  IOAudioController, IOAudio )
OSDefineAbstractStructors(  IOAudioController, IOAudio )

bool IOAudioController::init(OSDictionary *properties)
{
    if (!super::init(properties)) {
        return false;
    }

    fMasterVolumeComponents = OSSet::withCapacity(1);
    fNumStreamsInUse = 0;

    nanoseconds_to_absolutetime((UInt64)(MAX_POLL_INTERVAL_SEC) * (UInt64)NSEC_PER_SEC, &fMaxPollInterval);

    return true;
}

bool IOAudioController::start( IOService *provider )
{
    if (!super::start(provider))
        return false;

    fDevice = provider;

    return true;
}

void IOAudioController::execAndSignal(OSObject * obj, void *field0, void *field1, void *field2, void *field3)
{
    IOAudioController *me = (IOAudioController *)obj;
    IOSyncer *syncer = (IOSyncer *) field0;
    IOAudioCmd cmd = (IOAudioCmd) (int) field1;

    me->execCommand(cmd, field2, field3);

    if(syncer)
        syncer->signal();
}

void IOAudioController::execCommand(IOAudioCmd cmd, void *field2, void *field3)
{
    switch (cmd) {
        case kConnect:
        getMap((AudioStreamIndex)field2, (IOAudioStreamMap *)field3);
        break;

        case kDetach:
        releaseMap((AudioStreamIndex)field2);
        break;

        case kSetFlow:
        setFlow((AudioStreamIndex)field2, (bool)field3);
        break;

        case kProbeStreams:
            int numStreams, i;
            numStreams = probeStreams();
            fStreams = OSArray::withCapacity(numStreams);
            if(fStreams) {
                for(i=0; i<numStreams; i++) {
                    IOAudioStream *stream;
                    stream = createAudioStream(i);
                    if(stream) {
                        fStreams->setObject(i, stream);
                        stream->attach(this);
                        stream->registerService();
                    }
                }
                CreateAudioTopology(fQueue);
            }
            // Start the timer event source, if necessary (eg polling Awacs inputs)
            AbsoluteTime pollInterval, wantTick;

            pollInterval = fMaxPollInterval;
            calculateTickInterval(&pollInterval);
            if(CMP_ABSOLUTETIME(&pollInterval, &fMaxPollInterval) < 0) {
                clock_get_uptime(&wantTick);
                ADD_ABSOLUTETIME(&wantTick, &pollInterval);
                fNextTick = wantTick;
                fTimer->wakeAtTime(fNextTick);
                fTimerInUse = true;
                fWorkLoop->addEventSource(fTimer);
            }

	break;

    case kSetVal:
        SetControl((UInt16)field2, (UInt16)field3);
        break;

    case kFlush:
        flushStream((AudioStreamIndex)field2, (IOAudioStreamPosition *)field3);
        break;

    case kAllocMixBuffer:
        if (*(void **)field3 == NULL) {
            *(void **)field3 = allocateMixBuffer((AudioStreamIndex)field2);
        }
        break;
        
    default:
        IOLog("Unexpected command %d in IOAudioController::execCommand!\n", cmd);

    }
}

void IOAudioController::clockTick(OSObject * obj, IOTimerEventSource *timer)
{
    ((IOAudioController *)obj)->DoClockTick(timer);
}

void IOAudioController::DoClockTick(IOTimerEventSource *timer)
{
    AudioStreamIndex stream;
    AudioStreamIndex end;

    if(fStreams)
        end = fStreams->getCount();
    else
        end = 0;

    for(stream = 0; stream < end; stream++) {
        IOAudioStreamStatus *status = getSharedStatus(stream);

        if(status && status->fRunning) {
            AbsoluteTime now;

            clock_get_uptime(&now);

            // Do buffer clearing before potentially removing the buffer!
            if(status->fErases) {
                UInt32 currentBlock;

                // Clear buffer behind DMA back to last erase point
                char *buf = (char *)getSampleBuffer(stream);
                char *mixBuf = (char *)getMixBuffer(stream);

                currentBlock = status->fCurrentBlock;

                if(currentBlock < status->fEraseHeadBlock) {
                    if (buf) {
                        bzero(buf, currentBlock * status->fBlockSize);
                        bzero(buf + (status->fEraseHeadBlock * status->fBlockSize),
                              status->fBufSize - (status->fEraseHeadBlock * status->fBlockSize));
                    }
                    if (mixBuf) {
                        bzero(mixBuf, currentBlock * status->fBlockSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE);
                        bzero(mixBuf + (status->fEraseHeadBlock * status->fBlockSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE),
                              (status->fBufSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE) - (status->fEraseHeadBlock * status->fBlockSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE));
                    }
                }
                else {
                    if (buf) {
                        bzero(buf + (status->fEraseHeadBlock * status->fBlockSize),
                              (currentBlock - status->fEraseHeadBlock) * status->fBlockSize);
                    }
                    if (mixBuf) {
                        bzero(mixBuf + (status->fEraseHeadBlock * status->fBlockSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE),
                              (currentBlock - status->fEraseHeadBlock) * status->fBlockSize  / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE);
                    }
                }

                status->fEraseHeadBlock = currentBlock;
            }
            if(status->fConnections == 0) {
                UInt32 currentBlock, currentLoopCount;

                currentLoopCount = status->fCurrentLoopCount;
                currentBlock = status->fCurrentBlock;
                
                // See if its time to stop (we have completed flushing output buffers)
                if ((currentLoopCount > status->fFlushEnd.fLoopCount)
                 || ((currentLoopCount == status->fFlushEnd.fLoopCount)
                  && (currentBlock > status->fFlushEnd.fBlock))) {
                    AbsoluteTime pollInterval;
                    status->fRunning = 0;
                    stopStream(stream);
                    fNumStreamsInUse--;
                    if (fNumStreamsInUse == 0) {
                        pollInterval = fMaxPollInterval;
                        calculateTickInterval(&pollInterval);
                        if(CMP_ABSOLUTETIME(&pollInterval, &fMaxPollInterval) < 0) {
                            fWorkLoop->removeEventSource(fTimer);
                            fTimerInUse = false;
                        }
                    }
                }
            }
        }
    }

    if(fTimerInUse) {
        fTimer->setTimeout(fTickPeriod);
    }
}

//------------------------------------------------------------------------
/*
 * startWorkLoop()
 *
 * Drivers call this after initializing their hardware
 * We ask the device here how many initial audio channels it has, create appropriate nubs,
 * and then start the work loop running
 */
void IOAudioController::startWorkLoop()
{
    if(NULL == gIOAudioPlane) {
         gIOAudioPlane = IORegistryEntry::makePlane( kIOAudioPlane );
    }

    if(NULL == gValSym) {
        gValSym = OSSymbol::withCString("Val");
        gInputsSym = OSSymbol::withCString("Inputs");
        gControlsSym = OSSymbol::withCString("Controls");
        gJackSym = OSSymbol::withCString("Jack");
        gSpeakerSym = OSSymbol::withCString("Speaker");
        gHeadphonesSym = OSSymbol::withCString("Headphones");
        gLineOutSym = OSSymbol::withCString("LineOut");
        gTypeSym = OSSymbol::withCString("Type");
	gMuteSym = OSSymbol::withCString("Mute");
	gIdSym = OSSymbol::withCString("Id");
        gMasterSym = OSSymbol::withCString("Master");
    }

    fTimer = IOTimerEventSource::timerEventSource(this, &clockTick);
    fQueue = IOCommandQueue::commandQueue(this, &execAndSignal);

    fWorkLoop = createWorkLoop();
    fWorkLoop->addEventSource(fQueue);

    fQueue->enqueueCommand(true, 0, (void *)kProbeStreams, 0);

    registerService();
}

IOWorkLoop *IOAudioController::getWorkLoop() const
{
    return fWorkLoop;
}

IOWorkLoop * IOAudioController::createWorkLoop()
{
    IOWorkLoop *loop;

    loop = new IOWorkLoop();
    if(loop && !loop->init()) {
	loop->release();
	loop = NULL;
    }
    return loop;
}

IOAudioStream *IOAudioController::createAudioStream(AudioStreamIndex index)
{
    OSDictionary *props = getStreamProperties(index);
    IOAudioStreamImpl *stream;
    SInt32 isOut;
    IOReturn err;

    if(!props)
	return NULL;

    stream = new IOAudioStreamImpl;
    if(stream)
    	stream->initWithPropsIndexQueue(props, index, fQueue);

    props->release();	// Finished with this now.

    /*
     * Output streams are the start of a sound chain, so put
     * them at the root of the IOAudio plane.
     */
    err = stream->isOutput(&isOut);
    if(!err && isOut)
        stream->attachToParent(getRegistryRoot(), gIOAudioPlane);

    return stream;
}

IOAudioComponentImpl *
IOAudioController::buildComponentAndAttach(IORegistryEntry *parent,
			IORegistryEntry *child, const char *serialProps,
							IOCommandQueue *queue)
{
    OSDictionary *dict;
    OSString *errorString;
    IOAudioComponentImpl * aComp;
    OSNumber *masterVal;

    aComp = new IOAudioComponentImpl;
    if(!aComp)
        return NULL;

    dict = OSDynamicCast(OSDictionary, OSUnserialize(serialProps, &errorString));
    if (!dict && errorString) {
	IOLog("%s\n", errorString->getCStringNoCopy());
	errorString->release();
    }
    if(!aComp->initWithStuff(this, dict, queue)) {
        aComp->release();
        return NULL;
    }

    if(!parent)
        parent = getRegistryRoot();

    aComp->attachToParent(parent, gIOAudioPlane);
    
    if(child)	
        child->attachToParent(aComp, gIOAudioPlane);

    masterVal = (OSNumber *)dict->getObject(gMasterSym);
    if ((masterVal != NULL) && (masterVal->unsigned32BitValue() != 0)) {
        fMasterVolumeComponents->setObject(aComp);
    }
        
    return aComp;
}

void IOAudioController::AttachComponents(IORegistryEntry *parent,
			IORegistryEntry *child)
{
    child->attachToParent(parent, gIOAudioPlane);
}


void IOAudioController::free()
{
    if (fMasterVolumeComponents)
        fMasterVolumeComponents->release();
    if(fWorkLoop)
        fWorkLoop->release();
    if(fQueue)
        fQueue->release();
    if(fTimer)
        fTimer->release();
    if(fStreams)
	fStreams->release();
    super::free();
}

void IOAudioController::calculateTickInterval(AbsoluteTime *tickInterval)
{
    AudioStreamIndex stream;
    AudioStreamIndex end;

    if(fStreams)
        end = fStreams->getCount();
    else
        end = 0;

    for(stream = 0; stream < end; stream++) {
        IOAudioStreamStatus *status = getSharedStatus(stream);
        if(status) {
            AbsoluteTime pollInterval;

            nanoseconds_to_absolutetime(((UInt64)NSEC_PER_SEC * (UInt64)status->fBufSize / (UInt64)status->fDataRate / (UInt64)TIMER_INTS_PER_BUF), &pollInterval);

            if (CMP_ABSOLUTETIME(&pollInterval, tickInterval) < 0) {
                *tickInterval = pollInterval;
            }
        }
    }

    // Save the minimum period
    fTickPeriod = *tickInterval;
}

/*
 * Map stream data for caller.
 */
void IOAudioController::getMap(AudioStreamIndex index, IOAudioStreamMap *map)
{
    IOAudioStreamStatus * status = getSharedStatus(index);

    if(!status) {
        fNumStreamsInUse++;
        status = startStream(index);
        if(status) {
            AbsoluteTime pollInterval, wantTick;

            //map->fMixBuffer = (void *)IOMallocAligned(round_page(status->fBufSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE), PAGE_SIZE);

            clock_get_uptime(&wantTick);
            if(status->fErases) {
                status->fEraseHeadBlock = 0;
            }

            status->fFlushEnd.fBlock = 0;
            status->fFlushEnd.fLoopCount = 0;
            status->fMixBufferInUse = false;

            pollInterval = fMaxPollInterval;
            calculateTickInterval(&pollInterval);

            ADD_ABSOLUTETIME(&wantTick, &pollInterval);

            if(!fTimerInUse || (CMP_ABSOLUTETIME(&wantTick, &fNextTick) < 0)) {
                fNextTick = wantTick;
                fTimer->wakeAtTime(fNextTick);
            }
            if(!fTimerInUse) {
                fTimerInUse = true;
                fWorkLoop->addEventSource(fTimer);
            }
            setFlow(index, true);
        }
        else {
            map->fStatus = NULL;
            map->fSampleBuffer = NULL;
            //map->fMixBuffer = NULL;
            fNumStreamsInUse--;
            return;
        }
    }
    status->fConnections = 1;
    map->fStatus = status;
    map->fSampleBuffer = getSampleBuffer(index);
}

void IOAudioController::releaseMap(AudioStreamIndex index)
{
    IOAudioStreamStatus * status = getSharedStatus(index);

    if (status) {
        status->fConnections = 0;
    }
}

void IOAudioController::setFlow(AudioStreamIndex index, bool flowing)
{
    IOAudioStreamStatus * status = getSharedStatus(index);
    if(status) {
        if(flowing) {
            if(status->fRunning++ == 0) {
                fNumStreamsInUse++;
                resumeStream(index);
            }
        }
        else {
            if(--status->fRunning == 0) {
                pauseStream(index);
                fNumStreamsInUse--;
            }
        }
    }
}

void IOAudioController::flushStream(AudioStreamIndex index, IOAudioStreamPosition *endingPosition)
{
    IOAudioStreamStatus *status = getSharedStatus(index);

    if (status) {
        if ((endingPosition->fLoopCount > status->fFlushEnd.fLoopCount) || ((endingPosition->fLoopCount == status->fFlushEnd.fLoopCount) && (endingPosition->fBlock > status->fFlushEnd.fBlock))) {
            status->fFlushEnd = *endingPosition;
        }
    }
}

void *IOAudioController::allocateMixBuffer(AudioStreamIndex index)
{
    IOAudioStreamStatus * status = getSharedStatus(index);
    void *mixBuffer = NULL;

    if(status) {
        mixBuffer = (void *)IOMallocAligned(round_page(status->fBufSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE), PAGE_SIZE);
        bzero (mixBuffer, round_page(status->fBufSize / status->fSampleSize * MIX_BUFFER_SAMPLE_SIZE));
        status->fMixBufferInUse = true;
    }

    return mixBuffer;
}
void *IOAudioController::getMixBuffer(AudioStreamIndex index)
{
    void *mixBuffer = NULL;
    if (fStreams) {
        IOAudioStreamImpl *stream = (IOAudioStreamImpl *)fStreams->getObject(index);
        if (stream) {
            if (stream->getMixBuffer(&mixBuffer) != kIOReturnSuccess) {
                mixBuffer = NULL;
            }
        }
    }

    return mixBuffer;
}

void IOAudioController::setMasterComponentsProperties(OSDictionary *properties)
{
    if (properties) {
        OSCollectionIterator *iter;

        iter = OSCollectionIterator::withCollection(fMasterVolumeComponents);
        if (iter) {
            IOAudioComponent *component;

            while ((component = OSDynamicCast(IOAudioComponent, iter->getNextObject())) != NULL) {
                component->setProperties(properties);
            }
        }
    }
}
void IOAudioController::setMasterVolumeLeft(UInt16 newMasterVolumeLeft)
{
    OSDictionary *controlDict;
    OSString *errorString;
    char controlDictString[51]; // strlen(formatString) + 5 (max chars in 16-bit int) + 1 (terminator)

    sprintf (controlDictString, "{'Controls'={'VolumeLeft'={'Val'=%d:16;};};};", newMasterVolumeLeft);
    controlDict = OSDynamicCast(OSDictionary, OSUnserialize(controlDictString, &errorString));

    if (controlDict) {
        setMasterComponentsProperties(controlDict);
        controlDict->release();
    } else if (errorString) {
        IOLog("IOAudioController: %s\n", errorString->getCStringNoCopy());
        errorString->release();
    }
}

void IOAudioController::setMasterVolumeRight(UInt16 newMasterVolumeRight)
{
    OSDictionary *controlDict;
    OSString *errorString;
    char controlDictString[52]; // strlen(formatString) + 5 (max chars in 16-bit int) + 1 (terminator)

    // FIXME - We really should scale to min/max in this control...
    sprintf (controlDictString, "{'Controls'={'VolumeRight'={'Val'=%d:16;};};};", newMasterVolumeRight);
    controlDict = OSDynamicCast(OSDictionary, OSUnserialize(controlDictString, &errorString));

    if (controlDict) {
        setMasterComponentsProperties(controlDict);
        controlDict->release();
    } else if (errorString) {
        IOLog("IOAudioController: %s\n", errorString->getCStringNoCopy());
        errorString->release();
    }
}

void IOAudioController::setMasterMute(bool newMasterMute)
{
    OSDictionary *controlDict;
    OSString *errorString;
    char controlDictString[43]; // strlen(formatString) + 1 (single digit) + 1 (terminator)

    sprintf (controlDictString, "{'Controls'={'MuteAll'={'Val'=%d:8;};};};", newMasterMute ? 1 : 0);
    controlDict = OSDynamicCast(OSDictionary, OSUnserialize(controlDictString, &errorString));

    if (controlDict) {
        setMasterComponentsProperties(controlDict);
        controlDict->release();
    } else if (errorString) {
        IOLog("IOAudioController: %s\n", errorString->getCStringNoCopy());
        errorString->release();
    }
}
