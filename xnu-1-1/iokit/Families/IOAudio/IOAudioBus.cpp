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
 * Hardware independent (relatively) code for the AudioBus
 *
 * HISTORY
 *
 *
 */

#include <IOKit/audio/IOAudioBus.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#undef super
#define super IOAudioController

//************************************************************************
// Implementation of protocol clas.
//************************************************************************
OSDefineMetaClass(  IOAudioBus, IOAudioController )
OSDefineAbstractStructors(  IOAudioBus, IOAudioController )

#ifdef __ppc__

//************************************************************************
// Begin implementation of IOAudioBus class.
//************************************************************************

#define kNumberOfBuffers  4
#define kNumberOfSamples  8192
#define kNumberOfChannels 2	// left and right
#define kSampleSize 	  2	// 16 bit channels
#define kBlockSize 	  256

const int IOAudioBus::kAudioDMAdeviceInt	= 0;
const int IOAudioBus::kAudioDMAtxInt		= 1;
const int IOAudioBus::kAudioDMArxInt		= 2;

const int IOAudioBus::kAudioDMAOutputStream	= 0;
const int IOAudioBus::kAudioDMAInputStream	= 1;

// Constructs an empty audio bus:
bool
IOAudioBus::init(OSDictionary * properties)
{
    if (!super::init(properties))
            return false;

    ioAudioStreamsDMA = NULL;
    numDMAStreams = NULL;

    // Initialize my ivars.
    fBufMax = kNumberOfBuffers * kNumberOfSamples * kNumberOfChannels * kSampleSize;
    fBlockSize = kBlockSize;

    return true;
}

// This should free everything
void
IOAudioBus::free()
{
    FreeStreams();
    super::free();
}

void IOAudioBus::startWorkLoop()
{
    super::startWorkLoop();
    registerInterrupts();
}

void IOAudioBus::registerInterrupts()
{
    fTxInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                                                                                  IOAudioBus::AudioBusInterruptHandler,
                                                                                  IOAudioBus::AudioBusInterruptFilter,
                                                                                  fDevice,
                                                                                  kAudioDMAtxInt);
    fWorkLoop->addEventSource(fTxInterruptSource);

    fRxInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                                                                                  IOAudioBus::AudioBusInterruptHandler,
                                                                                  IOAudioBus::AudioBusInterruptFilter,
                                                                                  fDevice,
                                                                                  kAudioDMArxInt);
    fWorkLoop->addEventSource(fRxInterruptSource);

    fTxInterruptSource->enable();
    fRxInterruptSource->enable();
}

bool IOAudioBus::AudioBusInterruptFilter(OSObject *owner,
                               IOFilterInterruptEventSource *source)
{
    register IOAudioBus *bus = (IOAudioBus *)owner;
    bool result = true;

    if (bus) {
        result = bus->filterInterrupt(source->getIntIndex());
    }

    return result;
}

bool IOAudioBus::filterInterrupt(int index)
{
    IOAudioStreamStatus *status = getSharedStatus(getStreamForInterrupt(index));

    if (status) {
        clock_get_uptime(&status->fLastLoopTime);
        ++status->fCurrentLoopCount;
    }

    return false;
}

void IOAudioBus::AudioBusInterruptHandler(OSObject *owner,
                                IOInterruptEventSource * /*source*/,
                                int /*count*/)
{
    return;
}

AudioStreamIndex IOAudioBus::getStreamForInterrupt(int index)
{
    AudioStreamIndex result = kNoStream;

    switch (index) {
        case kAudioDMAtxInt:
            result = kAudioDMAOutputStream;
            break;
        case kAudioDMArxInt:
            result = kAudioDMAInputStream;
            break;
    }
    return result;
}

// Creates n empty streams (also a method to free them)
bool
IOAudioBus::AllocateStreams(int n)
{
    numDMAStreams = n;

    if (numDMAStreams > 0) {
        // Allocates as many StreamDMAInfo as needed:
        ioAudioStreamsDMA = (StreamDMAInfo*)IOMalloc(sizeof(StreamDMAInfo) * numDMAStreams);
        if (ioAudioStreamsDMA == NULL) {
            numDMAStreams = 0;
            return false;
        }
        for (n =0 ; n < numDMAStreams; n++) {
            ioAudioStreamsDMA[n].streamProperty = NULL;
            ioAudioStreamsDMA[n].fIOBaseDMA = NULL;
            ioAudioStreamsDMA[n].fSharedStatus = NULL;
        }
  
        return (true);
    }

    return (false);
}

bool
IOAudioBus::FreeStreams()
{
    int n;

    if (ioAudioStreamsDMA != NULL) {
        // First stops all the streams:
        for (n =0 ; n < numDMAStreams; n++)
            if (ioAudioStreamsDMA[n].fIOBaseDMA != NULL) {
                stopStream(n);

                // If we ever created a stream property fpr the stream, delete it:
                IOFree(ioAudioStreamsDMA[n].streamProperty, 128);
                ioAudioStreamsDMA[n].streamProperty = NULL;
            }

        IOFree(ioAudioStreamsDMA, sizeof(StreamDMAInfo) * numDMAStreams);
        ioAudioStreamsDMA = NULL;

        return true;
    }

    return false;
}

// For each stream defines its properties:
bool
IOAudioBus::DefineStream(AudioStreamIndex i, int direction, UInt32 rate, IODBDMAChannelRegisters *base)
{
    if ((ioAudioStreamsDMA != NULL) && (i < numDMAStreams) && (base != NULL)) {
        ioAudioStreamsDMA[i].fIOBaseDMA = base;

        // The stream direction is "hardwired" so that ecah channel
        // can go in one direction. 
        if (direction == kInput) {
            ioAudioStreamsDMA[i].fDmaCmd = kdbdmaInputMore;
            ioAudioStreamsDMA[i].fNeedsErase = false;
            ioAudioStreamsDMA[i].fIsInput = true;
        }
        else {
            ioAudioStreamsDMA[i].fDmaCmd = kdbdmaOutputMore;
            ioAudioStreamsDMA[i].fNeedsErase = true;
            ioAudioStreamsDMA[i].fIsInput = false;
        }
        ioAudioStreamsDMA[i].fSampleRate = rate;

#ifdef DEBUGMODE
        IOLog("IOAudioBus::DefineStream(%d, %s, %ld, 0x%08lx)\n",i, (direction == kInput ? "kInput" : "kOutput"),rate, (UInt32)base);
#endif
    }
    
    return false;
}

// Returns the first stream in the given direction after the given
// index. (this is useful if we have to handle more than one stream
// for input and output). (afterIndex is inclusive)

AudioStreamIndex
IOAudioBus::firstStreamAfter(int inDirection, AudioStreamIndex afterIndex)
{
    if ((ioAudioStreamsDMA != NULL) && (afterIndex < numDMAStreams)) {
        for (;afterIndex < numDMAStreams; afterIndex++) {
            if ((inDirection == kInput) && (ioAudioStreamsDMA[afterIndex].fIsInput))
                return (afterIndex);
            else if ((inDirection == kOutput) && (!ioAudioStreamsDMA[afterIndex].fIsInput))
                return (afterIndex);
        }
    }

    // We did not find a stream with the wanted properties
    return (kInvalidStreamIndex);
}

OSDictionary*
IOAudioBus::getStreamProperties(AudioStreamIndex i)
{
    OSDictionary *dict = NULL;
    OSString *errorString = NULL;

    if ((ioAudioStreamsDMA != NULL) && (i < numDMAStreams))
        if (ioAudioStreamsDMA[i].fIOBaseDMA != NULL) {

            ioAudioStreamsDMA[i].streamProperty = (char*)IOMalloc(128);
            if (ioAudioStreamsDMA[i].streamProperty != NULL) {
                // Depending from the direction of the stream this builds the property
                // string:
                if (ioAudioStreamsDMA[i].fIsInput) {
                    sprintf(ioAudioStreamsDMA[i].streamProperty, "{'In'=%d:8;'Out'=%d:8;'Channels'=%d:8;'Rate'=%ld:32;}",
                            (UInt8)1, (UInt8)0, (UInt8)2,
                            ioAudioStreamsDMA[i].fSampleRate);
                }
                else{
                    sprintf(ioAudioStreamsDMA[i].streamProperty, "{'In'=%d:8;'Out'=%d:8;'Channels'=%d:8;'Rate'=%ld:32;}",
                            (UInt8)0, (UInt8)1, (UInt8)2,
                            ioAudioStreamsDMA[i].fSampleRate);
                }

                dict = OSDynamicCast(OSDictionary, OSUnserialize(ioAudioStreamsDMA[i].streamProperty, &errorString));
            }
        }
            else
                IOLog("IOAudioBus::getStreamProperties: bad index %d\n", i);

    if (dict == NULL) {
        if (errorString != NULL) {
            IOLog("IOAudioBus::getStreamProperties %s (\"%s\")\n", errorString->getCStringNoCopy(), ioAudioStreamsDMA[i].streamProperty);
            errorString->release();
        }
    }

    return dict;
}

/*
 * Map stream data for caller.
 */

IOAudioStream *
IOAudioBus::createAudioStream(AudioStreamIndex index)
{
    assert(index < numDMAStreams);
    IOAudioStream * stream = super::createAudioStream(index);
    return stream;
}

int IOAudioBus::probeStreams()
{
    return numDMAStreams;
}

IOAudioStreamStatus * IOAudioBus::startStream(AudioStreamIndex index)
{
    assert(index < numDMAStreams);

    int numBlocks, bufSize, i, cmdSize;
    u_int32_t cmdPhys, bufPhys, seqPhys, offset;
    IOAudioStreamStatus *status;
    char *bufs;
    bool doInterrupt = false;

    // Calculate size and allocate dbdma command area, sample buffer and shared status.
    numBlocks = fBufMax/fBlockSize;
    bufSize = fBlockSize * numBlocks;
    cmdSize = (numBlocks * 2 + 1) * sizeof(IODBDMADescriptor);
    IODBDMADescriptor *cmds = (IODBDMADescriptor *)IOMallocAligned(cmdSize, 4);
    bufs = (char *)IOMallocAligned(round_page(bufSize), PAGE_SIZE);

    // This makes sure we get an entire page for the status buffer
    // to prevent the problem of other memory being allocated
    // in the same page that we're sharing read-only with user space.
    // Since we don't need an entire page for each status struct,
    // we could keep track of how much of the page that we've used and
    // assign chunks of it for each stream...

    status = (IOAudioStreamStatus *)IOMallocAligned(round_page(sizeof(IOAudioStreamStatus)), PAGE_SIZE);

    // Everything after this check should always succeed.
    if(!cmds || !bufs || !status)
        return NULL;

    bzero(bufs, bufSize);

    // get physical addresses of everything for the DBDMA controller.
    seqPhys = pmap_extract(kernel_pmap, (vm_address_t) &(status->fCurrentBlock));
    cmdPhys = pmap_extract(kernel_pmap, (vm_address_t) cmds);
    bufPhys = pmap_extract(kernel_pmap, (vm_address_t) bufs);
    offset = 0;
    
    // The address of the stop command:
    u_int32_t cmdStopPys = pmap_extract(kernel_pmap, (vm_address_t) (&cmds[numBlocks * 2]));

    for(i=0; i<numBlocks; i++) {
        u_int32_t cmdDest;

        if(offset >= PAGE_SIZE) {
            bufPhys = pmap_extract(kernel_pmap, (vm_address_t) (bufs + i*fBlockSize));
            offset = 0;
        }

        // Need a DBDMA branch if the next command is on a different page or
        // if we have to loop back to the first DBDMA command.
        if(i == numBlocks-1) {
            cmdDest = cmdPhys;
            doInterrupt = true;
        } else if( ((2*(i+1)*sizeof(IODBDMADescriptor)) % PAGE_SIZE) == 0)
            cmdDest = pmap_extract(kernel_pmap, (vm_address_t) (cmds+2*(i+1)));
        else
            cmdDest = 0;

        IOMakeDBDMADescriptorDep( &cmds[2*i],
                                        kdbdmaStoreQuad,
                                        kdbdmaKeyStream0,
                                        kdbdmaIntNever,
                                        kdbdmaBranchNever,
                                        kdbdmaWaitNever,
                                        sizeof(u_int32_t),
                                        seqPhys,
                                        OSReadLittleInt32(&i, 0)  );
        if(cmdDest) {
            IOMakeDBDMADescriptorDep( &cmds[2*i+1],
                                      ioAudioStreamsDMA[index].fDmaCmd,
                                      kdbdmaKeyStream0,
                                      doInterrupt ? kdbdmaIntAlways : kdbdmaIntNever,
                                      kdbdmaBranchAlways,
                                      kdbdmaWaitNever,
                                      fBlockSize,
                                      bufPhys+offset,
                                      cmdDest);
        }
        else {
           IOMakeDBDMADescriptorDep(  &cmds[2*i+1],
                                    ioAudioStreamsDMA[index].fDmaCmd,
                                    kdbdmaKeyStream0,
                                    kdbdmaIntNever,
                                    kdbdmaBranchIfTrue,
                                    kdbdmaWaitNever,
                                    fBlockSize,
                                    bufPhys+offset,
                                    cmdStopPys);

        }
        offset += fBlockSize;
    }
    
    // Add a STOP:
    IOMakeDBDMADescriptor(  &cmds[2*i],
                        kdbdmaStop,
                        kdbdmaKeyStream0,
                        kdbdmaIntNever,
                        kdbdmaBranchNever,
                        kdbdmaWaitNever,
                        0,
                        NULL);
    
    ioAudioStreamsDMA[index].fSharedStatus = status;
    ioAudioStreamsDMA[index].fCmds = cmds;
    ioAudioStreamsDMA[index].fCmdSize = cmdSize;
    ioAudioStreamsDMA[index].fSampleBuffer = (int16_t *)bufs;
    status->fVersion = 1;
    status->fErases = ioAudioStreamsDMA[index].fNeedsErase;
    status->fRunning = 0;
    status->fConnections = 0;

    status->fBufSize = bufSize;
    status->fBlockSize = fBlockSize;
    status->fNumBlocks = numBlocks;
    status->fSampleSize = 2;
    status->fChannels = 2;
    status->fDataRate = ioAudioStreamsDMA[index].fSampleRate * status->fSampleSize * status->fChannels;
    status->fCurrentBlock = 0;
    status->fEraseHeadBlock = 0;
    status->fCurrentLoopCount = 0;
    status->fMixBufferInUse = false;

#ifdef DEBUGMODE
    IOLog("DMA commands at 0x%x, %d blocks, seq at 0x%x\n", cmds, numBlocks, seqVirt);
    IOLog("Block size %d, total size %d\n", fBlockSize, bufSize);
#endif

    flush_dcache((vm_offset_t) cmds, cmdSize, false );

    clock_get_uptime(&status->fLastLoopTime);

    IOSetDBDMAChannelControl( ioAudioStreamsDMA[index].fIOBaseDMA, IOClearDBDMAChannelControlBits(kdbdmaS0));
    IOSetDBDMABranchSelect( ioAudioStreamsDMA[index].fIOBaseDMA, IOSetDBDMAChannelControlBits(kdbdmaS0));
    IODBDMAStart( ioAudioStreamsDMA[index].fIOBaseDMA, cmdPhys );

    return status;
}

void IOAudioBus::stopStream(AudioStreamIndex index)
{
    IOFilterInterruptEventSource *interruptEventSource;
    UInt8 attemptsToStop = 100;

    assert(!ioAudioStreamsDMA[index].fSharedStatus->fRunning);

    if (index == kAudioDMAOutputStream) {
        interruptEventSource = fTxInterruptSource;
    } else {	// index == kAudioDMAInputStream
        interruptEventSource = fRxInterruptSource;
    }
    interruptEventSource->disable();

    IOSetDBDMAChannelControl( ioAudioStreamsDMA[index].fIOBaseDMA, IOSetDBDMAChannelControlBits(kdbdmaS0));
    while ((IOGetDBDMAChannelStatus(ioAudioStreamsDMA[index].fIOBaseDMA) & kdbdmaActive ) && (attemptsToStop--)) {
        eieio();
        IOSleep(10);
    }

    IODBDMAStop( ioAudioStreamsDMA[index].fIOBaseDMA );
    IODBDMAReset( ioAudioStreamsDMA[index].fIOBaseDMA );

    IOFreeAligned(ioAudioStreamsDMA[index].fCmds, ioAudioStreamsDMA[index].fCmdSize);
    IOFreeAligned(ioAudioStreamsDMA[index].fSampleBuffer, round_page(ioAudioStreamsDMA[index].fSharedStatus->fBufSize));
    IOFreeAligned(ioAudioStreamsDMA[index].fSharedStatus, round_page(sizeof(IOAudioStreamStatus)));

    ioAudioStreamsDMA[index].fCmds = NULL;
    ioAudioStreamsDMA[index].fSampleBuffer = NULL;
    ioAudioStreamsDMA[index].fSharedStatus = NULL;

    interruptEventSource->enable();
}

void IOAudioBus::pauseStream(AudioStreamIndex index)
{
    assert(index < numDMAStreams);
    IODBDMAPause( ioAudioStreamsDMA[index].fIOBaseDMA );
}

void IOAudioBus::resumeStream(AudioStreamIndex index)
{
    assert(index < numDMAStreams);

    IODBDMAContinue( ioAudioStreamsDMA[index].fIOBaseDMA );
}

IOAudioStreamStatus * IOAudioBus::getSharedStatus(AudioStreamIndex index)
{
    assert(index < numDMAStreams);
    return ioAudioStreamsDMA[index].fSharedStatus;
}

void * IOAudioBus::getSampleBuffer(AudioStreamIndex index)
{
    assert(index < numDMAStreams);
    return ioAudioStreamsDMA[index].fSampleBuffer;
}

#endif // __ppc__
