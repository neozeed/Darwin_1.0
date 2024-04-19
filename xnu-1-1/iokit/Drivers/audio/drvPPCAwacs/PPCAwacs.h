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
 * Interface definition for the Awacs audio Controller 
 *
 * HISTORY
 *
 */

#ifndef _PPCAWACS_H
#define _PPCAWACS_H

#include <IOKit/audio/IOAudioBus.h>

struct awacs_regmap_t;
struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

// 2 Digital DMA streams
#define kNumDMAStreams 2

// Controls for awacs/screamer
enum {
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

class PPCAwacs : public IOAudioBus
{
    OSDeclareDefaultStructors(PPCAwacs)

protected:
    volatile awacs_regmap_t *	fIOBaseAwacs;
    u_int32_t               	fSoundControlRegister;
    u_int32_t                	fCodecControlRegister[8];
    u_int32_t			fCodecStatus;
    bool			fIICAudioDevPresent;
    bool			fIsG4WithAwacs;
    bool			fIsiMacDVWithAwacs;
    IOAudioComponentImpl *	fInputComponents[4];

    // Stuff extracted from the sound device tree nub
    int				fDetects;
    int				fOutputs;

public:
    bool init(OSDictionary * properties = 0);
    void free();
    bool start(IOService * provider);
    void stop(IOService *provider);

    /*
     * Set a control to a value
     * Called by IOAudioControls, on the workloop thread.
     */
    virtual IOReturn SetControl(UInt16 id, int val);

protected:
    virtual void CreateAudioTopology(IOCommandQueue *queue);
    
    // Override these so we can call checkStatus().
    virtual void DoClockTick(IOTimerEventSource *);
    virtual void calculateTickInterval(AbsoluteTime *tickInterval);

    // Poll awacs status, since it doesn't generate interrupts
    virtual void checkStatus();

};
#endif /* !_PPCAWACS_H */
