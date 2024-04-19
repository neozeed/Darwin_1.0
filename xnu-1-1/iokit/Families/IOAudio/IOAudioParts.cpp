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
 * IOAudioDevice.cpp
 *
 * HISTORY
 *
 */

#include <IOKit/assert.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandQueue.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/audio/IOAudioController.h>

//************************************************************************
// Implementation of protocol classes.
//************************************************************************
OSDefineMetaClass(  IOAudioStream, IOUserClient )
OSDefineAbstractStructors(  IOAudioStream, IOUserClient )

OSDefineMetaClass(  IOAudioComponent, IOUserClient )
OSDefineAbstractStructors(  IOAudioComponent, IOUserClient )

//************************************************************************
// Begin implementation of IOAudioStreamImpl class.
//************************************************************************

#undef super
#define super IOAudioStream
OSDefineMetaClassAndStructors( IOAudioStreamImpl, IOAudioStream )

bool
IOAudioStreamImpl::initWithPropsIndexQueue(OSDictionary *props,
        AudioStreamIndex index, IOCommandQueue *queue)
{
    if(!super::init(props))
        return false;

    if(!queue)
        return false;

    fIndex = index;
    fCmdQueue = queue;

    fMappedMem.fMixBuffer = NULL;

    // make sure essential stuff is there
    assert(OSDynamicCast(OSNumber, getProperty("In")) != NULL);
    assert(OSDynamicCast(OSNumber, getProperty("Out")) != NULL);

    fMethods[kCallSetFlow].object = this;
    fMethods[kCallSetFlow].func = (IOMethod) &IOAudioStream::setFlow;
    fMethods[kCallSetFlow].count0 = 1;
    fMethods[kCallSetFlow].count1 = 0;
    fMethods[kCallSetFlow].flags = kIOUCScalarIScalarO;

    fMethods[kCallFlush].object = this;
    fMethods[kCallFlush].func = (IOMethod) &IOAudioStream::Flush;
    fMethods[kCallFlush].count0 = sizeof(IOAudioStreamPosition);
    fMethods[kCallFlush].count1 = 0;
    fMethods[kCallFlush].flags = kIOUCStructIStructO;

    fMethods[kCallSetErase].object = this;
    fMethods[kCallSetErase].func = (IOMethod) &IOAudioStream::setErase;
    fMethods[kCallSetErase].count0 = 1;
    fMethods[kCallSetErase].count1 = 1;
    fMethods[kCallSetErase].flags = kIOUCScalarIScalarO;

    return true;
}

void IOAudioStreamImpl::free()
{
    if (fMappedMem.fMixBuffer) {
        // Need to free mix buffer here - it will leak now
    }

    super::free();
}

/*
 * Connect a client, possibly starting DMA for the stream if it's
 * the first client.
 */
IOReturn
IOAudioStreamImpl::newUserClient( task_t owningTask, void * security_id,
					UInt32 type, IOUserClient ** handler )
{
    IOSyncer *syncWakeup;

    *handler = this;
    (*handler)->retain();

    syncWakeup = IOSyncer::create();

    fCmdQueue->enqueueCommand(true, syncWakeup, (void *)kConnect,
						(void *)fIndex, &fMappedMem);

    syncWakeup->wait();

    return kIOReturnSuccess;
}

IOReturn IOAudioStreamImpl::clientDied()
{
    /*
     * Decrememt connection count, possibly freeing DMA buffers
     */
    fCmdQueue->enqueueCommand(true, 0, (void *)kDetach, (void *)fIndex);

    return kIOReturnSuccess;
}

IOReturn IOAudioStreamImpl::clientClose()
{
    /*
     * Decrememt connection count, possibly freeing DMA buffers
     */
    //fCmdQueue->enqueueCommand(true, 0, (void *)kDetach, (void *)fIndex);

    return kIOReturnSuccess;
}

IOReturn IOAudioStreamImpl::Flush(IOAudioStreamPosition *end)
{
// Tell device when it'll be safe to stop the stream
    fCmdQueue->enqueueCommand(true, 0, (void *)kFlush, (void *)fIndex, (void *)end);
    return kIOReturnSuccess;
}

IOReturn IOAudioStreamImpl::setFlow(bool flowing)
{
    fCmdQueue->enqueueCommand(true, 0, (void *)kSetFlow, (void *)fIndex,
							(void *)flowing);
    return kIOReturnSuccess;
}

IOReturn IOAudioStreamImpl::isOutput(SInt32 *res) const
{
    const OSNumber *obj;
    obj = getOutputDescriptor();
    if(obj)
        *res = obj->unsigned8BitValue();
    else
        *res = 0;

    return kIOReturnSuccess;
}

IOReturn IOAudioStreamImpl::getMixBuffer(void **mixBuffer)
{
    *mixBuffer = fMappedMem.fMixBuffer;

    return kIOReturnSuccess;
}

IOReturn IOAudioStreamImpl::setErase(bool erase, SInt32 *oldErase)
{
    assert(fMappedMem.fStatus != NULL); // Must be a user attached.
    *oldErase = fMappedMem.fStatus->fErases;
    fMappedMem.fStatus->fErases = erase;
    return kIOReturnSuccess;
}

IOReturn
IOAudioStreamImpl::setProperties( OSObject * properties )
{
    return kIOReturnNotPrivileged;
}


IOReturn IOAudioStreamImpl::clientMemoryForType( UInt32 type,
    UInt32 * flags, IOMemoryDescriptor ** memory )
{
//    kprintf("IOAudioStream::clientMemoryForType(%d, %d, %d, %d)\n",
//        type, flags, address, size);

    switch(type) {
        case kSampleBuffer:
	    *memory = IOMemoryDescriptor::withAddress(
                                    (void *)fMappedMem.fSampleBuffer,
                                    fMappedMem.fStatus->fBufSize,
				    kIODirectionNone);
            *flags = 0;
        break;
        case kStatus:
	    *memory = IOMemoryDescriptor::withAddress(
                                    (void *)fMappedMem.fStatus,
                                    sizeof(IOAudioStreamStatus),
				    kIODirectionNone);
            *flags = kIOMapReadOnly;
        break;
        case kMixBuffer:
            if (fMappedMem.fMixBuffer == NULL) {
                IOSyncer *syncWakeup;

                syncWakeup = IOSyncer::create();

                fCmdQueue->enqueueCommand(true, syncWakeup, (void *)kAllocMixBuffer,
                                          (void *)fIndex, (void *)&fMappedMem.fMixBuffer);
                syncWakeup->wait();
            }
            fMappedMem.fStatus->fMixBufferInUse = true;
            *memory = IOMemoryDescriptor::withAddress(
                                    (void *)fMappedMem.fMixBuffer,
                                    (fMappedMem.fStatus->fBufSize / fMappedMem.fStatus->fSampleSize * sizeof(float)),
                                    kIODirectionNone);
            *flags = 0;
        break;
        default:
            return kIOReturnUnsupported;
        break;
    }
    return kIOReturnSuccess;
}

IOExternalMethod *
IOAudioStreamImpl::getExternalMethodForIndex( UInt32 index )
{
    IOExternalMethod *method = NULL;

    if(index < kNumCalls)
        method = &fMethods[index];
    return method;
}


//************************************************************************
// Begin implementation of IOAudioComponentImpl class.
//************************************************************************

#undef super
#define super IOAudioComponent
OSDefineMetaClassAndStructors( IOAudioComponentImpl, IOAudioComponent )


bool
IOAudioComponentImpl::initWithStuff(IOAudioController *owner, OSDictionary *props,
        					IOCommandQueue *queue)
{
    if(!super::init(props))
        return false;

    if(!queue || !owner)
        return false;

    fCmdQueue = queue;
    fOwner = owner;
    return true;
}

void
IOAudioComponentImpl::free()
{
    if(fNotifyMsg)
        IOFree(fNotifyMsg, sizeof (struct _notifyMsg));

    super::free();
}

IOReturn
IOAudioComponentImpl::updateVal(UInt32 val, OSDictionary *control, bool direct)
{
    IOReturn ret = kIOReturnSuccess;
    OSNumber * oldVal;

    oldVal = (OSNumber *)control->getObject(IOAudioController::gValSym);
    if(val != oldVal->unsigned32BitValue()) {
        OSNumber * id;
        OSNumber * newVal;
        newVal = OSNumber::withNumber(val, oldVal->numberOfBits());
        control->setObject(IOAudioController::gValSym, newVal);
        id = (OSNumber *)control->getObject(IOAudioController::gIdSym);
        if(id) {
            if(direct)
                fOwner->SetControl(id->unsigned16BitValue(), val);
            else
                fCmdQueue->enqueueCommand(true, 0, (void *)kSetVal,
                    (void *)id->unsigned16BitValue(), (void *)val);

        }
    }
    return ret;
}

void
IOAudioComponentImpl::Set(const OSSymbol *type, const OSSymbol *name, int val)
{
    OSDictionary *controls;

    controls = OSDynamicCast(OSDictionary, getProperty(type));

    if(controls) {
        OSDictionary *valDict;
        OSNumber * old;
        OSNumber * newObj;
        valDict = OSDynamicCast(OSDictionary,
					controls->getObject(name));
	if(valDict) {
            old = OSDynamicCast(OSNumber, valDict->getObject(IOAudioController::gValSym));
            if(old) {
                newObj = OSNumber::withNumber(val, old->numberOfBits());
                valDict->setObject(IOAudioController::gValSym, newObj);
                newObj->release(); // XXX -- svail: added.
                /* FIXME: Don't block */
                if(fNotifyMsg)
                    mach_msg_send_from_kernel((mach_msg_header_t *)fNotifyMsg,
                                                            fNotifyMsg->h.msgh_size);
                else if(type == IOAudioController::gInputsSym &&
                        name == IOAudioController::gJackSym &&
                        ( GetType() == IOAudioController::gHeadphonesSym ||
			  GetType() == IOAudioController::gLineOutSym)) {
                    /*
                     * This is a headphone or line out jack, mute/unmute any speakers
                     * with the same IOAudio parent
                     */
                    OSIterator * parents =
                        getParentIterator(IOAudioController::gIOAudioPlane);
                    IORegistryEntry *parent;
                    while((parent = OSDynamicCast(IORegistryEntry, parents->getNextObject()))) {
                        OSIterator * siblings =
                            parent->getChildIterator(IOAudioController::gIOAudioPlane);
                        IOAudioComponentImpl *sibling;
                        while((sibling = OSDynamicCast(IOAudioComponentImpl, siblings->getNextObject()))) {
                            if(sibling->GetType() == IOAudioController::gSpeakerSym) {
                                // Set all controls of type Mute to the new value
                                OSDictionary *spkrCtls =
                                    OSDynamicCast(OSDictionary, sibling->getProperty(IOAudioController::gControlsSym));
                                OSCollectionIterator *iter = OSCollectionIterator::withCollection(spkrCtls);
                                OSSymbol *iterKey;
                                while((iterKey = OSDynamicCast(OSSymbol, iter->getNextObject()))){
                                    OSObject *spkrCtlObj = spkrCtls->getObject(iterKey);
                                    if(GetType(spkrCtlObj) == IOAudioController::gMuteSym) {
                                        // Safe to just cast because GetType checks object type.
                                        // Update hardware directly
                                        updateVal(val, (OSDictionary *)spkrCtlObj, true);
                                    }
                                }
                            }
                        }
                    }                        
		}
            }
	}
    }
}

IOReturn IOAudioComponentImpl::clientClose( void )
{
    return kIOReturnSuccess;
}

IOReturn IOAudioComponentImpl::clientDied( void )
{
    return kIOReturnSuccess;
}
/*
 * Return a subclass of IOUserClient for User<->kernel comminication
 * Since IOAudioStream is such a suclass, we can return the object
 * itself (remembering to increment its retain count!).
 */
IOReturn IOAudioComponentImpl::newUserClient( task_t owningTask,
                void * security_id, UInt32 type, IOUserClient ** handler )
{
//kprintf("task:%d, sec_id:0x%x, type:%d\n", owningTask, security_id, type);
    *handler = this;
    (*handler)->retain();

    return kIOReturnSuccess;
}


IOReturn
IOAudioComponentImpl::setProperties( OSObject * properties )
{
    OSDictionary *myControls;
    OSDictionary *newControls;
    OSDictionary *newDict;
    OSCollectionIterator *iter;
    OSObject *iterKey;
    IOReturn ret = kIOReturnSuccess;

    newDict = OSDynamicCast(OSDictionary, properties);
    if(!newDict)
        return kIOReturnBadArgument;

    myControls = OSDynamicCast(OSDictionary, getProperty(IOAudioController::gControlsSym));
    newControls = OSDynamicCast(OSDictionary, newDict->getObject(IOAudioController::gControlsSym));
    if(!newControls)
        return kIOReturnBadArgument;

    /*
     * Look through the Controls dictionary, for each one that has a new Val
     * make a kSetVal command, updating the hardware.
     */
    iter = OSCollectionIterator::withCollection(newControls);
    while((iterKey = iter->getNextObject())) {
        OSSymbol *key = OSDynamicCast(OSSymbol, iterKey);
        OSDictionary *newCtrl;
        OSNumber* newVal;
        if(!key) {
            ret = kIOReturnBadArgument;
            break;
        }
        OSDictionary *myCtrl;
        myCtrl = (OSDictionary *)myControls->getObject(key);
        if(!myCtrl) {
            ret = kIOReturnBadArgument;
            break;
        }
        newCtrl = OSDynamicCast(OSDictionary, newControls->getObject(key));
        if(!newCtrl) {
            ret = kIOReturnBadArgument;
            break;
        }
        newVal = OSDynamicCast(OSNumber, newCtrl->getObject(IOAudioController::gValSym));
        if(!newVal) {
            ret = kIOReturnBadArgument;
            break;
        }
        // Update hardware via command queue
        ret = updateVal(newVal->unsigned16BitValue(), myCtrl, false);
        if(kIOReturnSuccess != ret)
            break;
    }
    
    return ret;
}

IOReturn
IOAudioComponentImpl::registerNotificationPort(
            mach_port_t port, UInt32 type, UInt32 refCon)
{
    static IOAudioNotifyMsg notify_msg = {{
        // mach_msg_bits_t	msgh_bits;
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0),
        // mach_msg_size_t	msgh_size;
        sizeof (IOAudioNotifyMsg),
        // mach_port_t	msgh_remote_port;
        MACH_PORT_NULL,
        // mach_port_t	msgh_local_port;
        MACH_PORT_NULL,
        // mach_msg_size_t 	msgh_reserved;
        0,
        // mach_msg_id_t	msgh_id;
        type },
        // UInt32 refCon
        refCon
    };

    if( type != kIOAudioInputNotification)
	return( kIOReturnUnsupported);
    if ( fNotifyMsg == NULL )
        fNotifyMsg = (struct _notifyMsg *)
                        IOMalloc( sizeof (IOAudioNotifyMsg) );
    // Initialize the events available message.
    *fNotifyMsg = notify_msg;

    fNotifyMsg->h.msgh_remote_port = port;

    return kIOReturnSuccess;
}

const OSSymbol *
IOAudioComponentImpl::GetType() const
{
    const OSString *type;
    type = OSDynamicCast(OSString, getProperty(IOAudioController::gTypeSym));

    return OSSymbol::withString(type);
}

const OSSymbol *
IOAudioComponentImpl::GetType(const OSObject *obj) const
{
    const OSString *type;
    const OSDictionary *dict;
    dict = OSDynamicCast(OSDictionary, obj);
    if(NULL == dict)
	return NULL;
    type = OSDynamicCast(OSString, dict->getObject(IOAudioController::gTypeSym));

    return OSSymbol::withString(type);
}
