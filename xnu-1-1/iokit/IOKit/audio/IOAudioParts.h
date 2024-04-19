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
 * @header 
 * AudioClientImplementation
 */

#ifndef _IOAUDIOCLIENTIMPL_H
#define _IOAUDIOCLIENTIMPL_H

#include <IOKit/audio/IOAudio.h>
class IOWorkLoop;
class IOCommandQueue;
class IOTimerEventSource;
class IOAudioController;

/*!
 * @enum IOAudioCmd
 * @discussion Commands for IOAudio workloop's command event source
 * @constant kConnect
 * @constant kDetach
 * @constant kSetFlow
 * @constant kProbeStreams
 * @constant kSetVal
 */
typedef enum _IOAudioCmd {
    kConnect = 0,
    kDetach,
    kSetFlow,
    kProbeStreams,
    kSetVal,
    kFlush,
    kAllocMixBuffer
} IOAudioCmd;

/*!
 * @typedef AudioStreamIndex
 * @discussion Sound hardware contains several data streams which can be controlled to
 * and accessed to some extent by the CPU.
 * The driver assigns each of its streams an index, starting from 0.
 * Most calls to the driver specify the stream to manipulate.
 */
typedef int AudioStreamIndex;	// Just to be tidy

/*!
 * @defined
 * @discussion kNoStream Indicates no stream present
 */
#define kNoStream -1

/*!
 * @struct IOAudioStreamMap
 * @discussion Structure passed back when a stream is mapped, giving addresses in caller's
 * address space for the two shared data structures
 * @field fSampleBuffer Pointer to the buffer for an audio stream
 * @field fStatus Pointer to status information of an audio stream
 */
struct IOAudioStreamMap
{
    void *			fSampleBuffer;
    IOAudioStreamStatus *	fStatus;
    void *			fMixBuffer;
};

/*!
 * @class IOAudioComponentImpl
 * Implemenation for IOAudioComponenet
 */
class IOAudioComponentImpl : public IOAudioComponent
{	
    OSDeclareDefaultStructors(IOAudioComponentImpl)

protected:
    IOCommandQueue *	fCmdQueue;
    struct _notifyMsg *	fNotifyMsg;
    IOAudioController *	fOwner;

    /*! @function free Override to free memory */
    virtual void free();

    /*!
     * @function updateVal
     * @param val
     * @param control
     * @param direct
     * @result IOReturn
     */
   virtual IOReturn updateVal(UInt32 val, OSDictionary *control, bool direct);

public:
    /*!
     * @function initWithStuff
     * @discussion Initialization.
     * @param owner
     * @param props
     * @param queue
     * @result bool
     */
   virtual bool initWithStuff(IOAudioController *owner, OSDictionary *props,
							IOCommandQueue *queue);

   /*!
    * @function newUserClient
    * @discussion Create an IOUserClient object to handle marshaling across the kernel/User
    *  boundary. We use the IOAudioStream object itself.
    * @param owningTask
    * @param security_id
    * @param type
    * @param handler
    * @result IOReturn
    */
    virtual IOReturn newUserClient( task_t owningTask, void * security_id,
                                    UInt32 type, IOUserClient ** handler );
    /*!
     * @function clientClose
     * @discussion Methods for IOUserClient.
     * @param void
     * @result IOReturn
     */ 
    virtual IOReturn clientClose( void );

    /*!
     * @function clientDied
     * @discussion Methods for IOUserClient.
     * @param void
     * @result IOReturn
     */
    virtual IOReturn clientDied( void );

    /*!
     * @function registerNotificationPort
     * @discussion Methods for IOUserClient.
     * @param port
     * @param type
     * @param refCon
     * @result IOReturn
     */
   virtual IOReturn registerNotificationPort(
		mach_port_t port, UInt32 type, UInt32 refCon );

   /*!
    * @function setProperties
    * @discussion Request change in properties, eg. volume, treble, etc.
    * Properties should be an OSDictionary of the form:
    *  {Controls = {
    *  	'AControl' = {'Val' = newVal;};
    *  	'BControl' = {'Val' = newVal;};
    *  };};
    *
    *   Other dictionary entries are ignored (so you can pass in a modified
    *  version of the current properties).
    * @param properties
    * @result IOReturn
    */ 
    virtual IOReturn setProperties( OSObject * properties );

    /*! 
     * @function Set
     * @discussion called from device when hardware state changes.
     * @param type
     * @param name
     * @param val
     * @result void
     */
    virtual void Set(const OSSymbol *type, const OSSymbol *name, int val);

    /*!
     * @function GetType
     * @discussion Handy gettors for Type property
     * @result OSSymbol
     */
    virtual const OSSymbol *GetType() const;

    /*!
     * @function GetType
     * @discussion Handy gettors for Type property
     * @param obj
     * @result OSSymbol
     */
    virtual const OSSymbol *GetType(const OSObject *obj) const;
};

/*!
 * @class IOAudioStreamImpl
 * Implemenation for IOAudioStream
 */
class IOAudioStreamImpl : public IOAudioStream
{	
    OSDeclareDefaultStructors(IOAudioStreamImpl)

protected:
    IOExternalMethod fMethods[kNumCalls];
    IOAudioStreamMap fMappedMem;
    AudioStreamIndex	fIndex;
    IOCommandQueue *	fCmdQueue;

    /*! @function free Override to free memory */
    virtual void free();

public:
    /*!
     * @function initWithPropsIndexQueue
     * @discussion
     * @param props
     * @param index
     * @param queue
     * @result bool
     */
    virtual bool initWithPropsIndexQueue(OSDictionary *props,
                                AudioStreamIndex index, IOCommandQueue *queue);

    /*!
     * @function getInputDescriptor
     * @result OSNumber
     */
    const OSNumber *getInputDescriptor() const
        {return OSDynamicCast(OSNumber, getProperty("In"));};

    /*!
     * @function getOutputDescriptor
     * @result OSNumber
     */
   const OSNumber *getOutputDescriptor() const
        {return OSDynamicCast(OSNumber, getProperty("Out"));};

   /*!
    * @function newUserClient
    * @discussion Track client connection and departure so DMA and status buffers can be managed.
    * @result IOReturn
    */
    virtual IOReturn newUserClient( task_t owningTask, void * security_id,
                                    UInt32 type, IOUserClient ** handler );
    /*!
     * @function clientClose
     * @discussion Track client connection and departure so DMA and status buffers can be managed.
     * @result IOReturn
     */
    virtual IOReturn clientClose( void );

    /*!
     * @function clientDied
     * @discussion Track client connection and departure so DMA and status buffers can be managed.
     * @result IOReturn
     */
    virtual IOReturn clientDied( void );

    /*!
     * @function getExternalMethodForIndex
     * @param index
     * @result IOExternalMethod
     */
   virtual IOExternalMethod * getExternalMethodForIndex( UInt32 index );

    /*!
     * @function clientMemoryForType
     * @discussion Return info on shared memory so base class can share it out to user space
     * @param type
     * @param flags
     * @param memory
     * @result IOReturn
     */
    virtual IOReturn clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory );

    /*!
     * @function Flush
     * @discussion See base class
     * @param end 
     * @result IOReturn
     */
    virtual IOReturn Flush(IOAudioStreamPosition *end);

    /*!
     * @function setFlow
     * @discussion Control the flow of data
     * @param flowing True to flow, false to stop
     * @result IOReturn
     */
    virtual IOReturn setFlow(bool flowing);

    /*!
     * @function getMixBuffer
     * @discussion get the mix buffer for the stream
     * @param mixBuffer
     * @result IOReturn kIOReturnSuccess
     */
    
    virtual IOReturn getMixBuffer(void **mixBuffer);
    
    /*!
     * @function setErase
     * @discussion Control the erase head
     * @param erase
     * @param oldErase
     * @result IOReturn
     */
    virtual IOReturn setErase(bool erase, SInt32 *oldErase);

    /*!
     * @function isOutput
     * @discussion Handy shortcut to useful property.
     * @param res
     * @result IOReturn
     */
    IOReturn isOutput(SInt32 *res) const;

    /*!
     * @function setProperties
     * @discussion Set the audio properties.
     * @param properties The properties.
     * @result IOReturn
     */
    IOReturn setProperties( OSObject * properties );

};

#endif /* _IOAUDIOCLIENTIMPL_H */
