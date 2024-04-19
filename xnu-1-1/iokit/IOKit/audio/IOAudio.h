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
 * @header IOAudio 
 * This is the include file defining classes for IOAudio clients.
 */

#ifndef _IOAUDIO_H
#define _IOAUDIO_H

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/audio/IOAudioTypes.h>

/*!
 * @enum <no name>
 * @constant kAllChannels
 */
enum {
    kAllChannels = 0
};

/*!
 * @class IOAudioComponent
 * @abstract An IOAudioComponent is a piece of hardware inside an audio device
 * that has interesting, and possibly modifiable, properties.
 * @discussion Examples are the input and output jacks, the volume control etc.
 * This class corresponds to the USB Audio class Units and Terminals.
 * The topology of the audio device (how streams and components are
 * connected) is represented in the IOAudio registry plane.
 * Sound flows from the parent to the child.
 */
class IOAudioComponent : public IOUserClient
{	
    OSDeclareDefaultStructors(IOAudioComponent)

public:

};

/*!
 * @class IOAudioStream
 * An IOAudioStream represents the access the CPU has to the A/D and D/A
 * converters of the sound device.
 */
class IOAudioStream : public IOUserClient
{	
    OSDeclareDefaultStructors(IOAudioStream)

public:

    /*!
     * @function Flush
     * @discussion Sets the ending stream position guaranteed to be played after all clients disconnect
     * @param end stream position to play through after all clients disconnect
     * @result IOReturn kIOReturnSuccess
     */
    virtual IOReturn Flush(IOAudioStreamPosition *end) = 0;

    /*!
     * @function setFlow
     * @discussion Pause/Play control
     * @param flowing
     * @result IOReturn kIOReturnSuccess
     */
    virtual IOReturn setFlow(bool flowing) = 0;

    /*!
     * @function setErase
     * @discussion Pause/Play control
     * @param erase true to turn on erase head, false otherwise
     * @param oldErase previous value
     * @result IOReturn kIOReturnSuccess
     */
   virtual IOReturn setErase(bool erase, SInt32 *oldErase) = 0;

    /*!
     * @function isOutput
     * @discussion Input stream or output one?
     * @param res non-zero if output, 0 otherwise
     * @result IOReturn kIOReturnSuccess
     */
   virtual IOReturn isOutput(SInt32 *res) const = 0;

   /*!
    * @function getMixBuffer
    * @param mixBuffer pointer to the mix buffer for this stream
    * @result IOReturn kIOReturnSuccess
    */

   virtual IOReturn getMixBuffer(void **mixBuffer) = 0;

};

/*!
 * @class IOAudio
 * Object representing the complete audio device.
 * Probably only interesting for devices that can create streams,
 * such as fancy PCI sound cards with onboard DSPs.
 */
class IOAudio : public IOService
{	
    OSDeclareAbstractStructors(IOAudio)

public:

};

#endif /* _IOAUDIO_H */
