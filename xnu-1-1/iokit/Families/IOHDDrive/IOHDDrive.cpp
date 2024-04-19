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

#include <IOKit/IOLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/assert.h>
#include <IOKit/storage/IOHDDrive.h>
#include <IOKit/storage/IOHDDriveNub.h>

#define	super	IODrive
OSDefineMetaClassAndStructors(IOHDDrive,IODrive)

void
gc_glue(IOService *target,void *param,UInt64 actualTransferCount,IOReturn result);

/* Accept a new piece of media, doing whatever's necessary to make it
 * show up properly to the system.
 */
IOReturn
IOHDDrive::acceptNewMedia(void)
{
    IOReturn result;
    bool ok;
    UInt64 nbytes;
    UInt64 nformats;
    char name[128];
    bool nameSep;

    lockForArbitration();

    /* Since the kernel printf doesn't handle 64-bit integers, we
     * simply make an assumption that the block count and size
     * will be 32-bit values max.
     */

#ifdef moreDebug
    IOLog("%s[IOHDDrive]::%s media: %ld blocks, %ld bytes each, write-%s.\n",
            getName(),
            getDeviceTypeName(),
            (UInt32)_maxBlockNumber + 1,(UInt32)getMediaBlockSize(),
            (_writeProtected ? "protected" : "enabled"));
#else
    IOLog("%s media: %ld blocks, %ld bytes each, write-%s.\n",          
            getName(),
            (UInt32)_maxBlockNumber + 1,(UInt32)getMediaBlockSize(),
            (_writeProtected ? "protected" : "enabled"));
#endif
    
    /* Instantiate a media object and attach it to ourselves. */

    nformats = getFormatCapacities(&nbytes,1);	/* get byte size of media */

    name[0] = 0;
    nameSep = false;
    if (_provider->getVendorString()) {
        strcat(name, _provider->getVendorString());
        nameSep = true;
    }
    if (_provider->getProductString()) {
        if (nameSep == true)  strcat(name, " ");
        strcat(name, _provider->getProductString());
        nameSep = true;
    }
    if (nameSep == true)  strcat(name, " ");
    strcat(name, "Media");

    result = instantiateMediaObject(&_mediaObject,0,nbytes,_mediaBlockSize,true,name);
    
    if (result == kIOReturnSuccess) {
        _mediaPresent = true;
        ok = _mediaObject->attach(this);       		/* attach media object above us */
        if (ok) {
	    // IOLog("IOHDDrive: registering media object\n");
            _mediaObject->registerService();		/* enable matching */
        }
    } else {
        IOLog("%s[IOHDDrive]::acceptNewMedia; couldn't instantiate media object.\n",getName());
    }
    
    unlockForArbitration();    

    return(result);
}

/* Allocate a new context struct. */
struct IOHDDrive::context *
IOHDDrive::allocateContext(void)
{
    struct IOHDDrive::context *cx;

    cx = IONew(struct IOHDDrive::context,1);
    if (cx == NULL) {
        return(NULL);
    }

    bzero(cx,sizeof(struct IOHDDrive::context));

    return(cx);
}

IOReturn
IOHDDrive::checkForMedia(void)
{
    IOReturn result;
    bool currentState;
    bool changed;
    
    IOTakeLock(_mediaStateLock);    

    result = _provider->reportMediaState(&currentState,&changed);
    if (result != kIOReturnSuccess) {		/* the poll operation failed */
	IOLog("%s[IOHDDrive]::checkForMedia; err '%s' from reportMediaState\n",
			getName(),stringFromReturn(result));

        IOUnlock(_mediaStateLock);
        return(result);
    }

    /* The poll succeeded. */

    if (!changed) {			/* media state hasn't changed */
        IOUnlock(_mediaStateLock);
        return(kIOReturnSuccess);
    }

    /* The media has changed state. See if it's just inserted or removed. */

    if (currentState == true) {		/* media is now present */

//        IOLog("%s[IOHDDrive]::checkForMedia; changed = %s, mediaPresent = %s\n",
//                getName(),changed ? "Y" : "N",currentState ? "Y" : "N");

        /* Allow a subclass to decide whether we accept the media. Such a decision
         * might be based on things like password-protection, etc.
         */

        if (validateNewMedia() == false) {	/* we're told to reject it */
            rejectMedia();			/* so let subclass do whatever it wants */
            IOUnlock(_mediaStateLock);
            return(kIOReturnSuccess);		/* pretend nothing happened */
        }

        result = recordMediaParameters();	/* learn about media */
        if (result != kIOReturnSuccess) {	/* couldn't record params */
            initMediaStates();		/* deny existence of new media */
	    IOLog("%s[IOHDDrive]::checkForMedia: err '%s' from recordMediaParameters\n",
			getName(),stringFromReturn(result));
            IOUnlock(_mediaStateLock);
            return(result);
        }

        /* Now we do what's necessary to make the new media
         * show up properly in the system.
         */

        result = acceptNewMedia();
        if (result != kIOReturnSuccess) {
            initMediaStates();		/* deny existence of new media */
	    IOLog("%s[IOHDDrive]::checkForMedia; err '%s' from acceptNewMedia\n",
			getName(),stringFromReturn(result));
            IOUnlock(_mediaStateLock);
            return(result);
        }

        IOUnlock(_mediaStateLock);
        return(kIOReturnSuccess);		/* all done, new media is ready */

    } else {				/* media is now absent */

        result = decommissionMedia(true);	/* force a teardown */
        if (result != kIOReturnSuccess && result != kIOReturnNoMedia) {
	    IOLog("%s[IOHDDrive]::checkForMedia; err '%s' from decommissionNewMedia\n",
			getName(),stringFromReturn(result));
            IOUnlock(_mediaStateLock);
            return(result);
        }

        IOUnlock(_mediaStateLock);
        return(kIOReturnSuccess);		/* all done; media is gone */
    }
}

UInt64
IOHDDrive::constrainByteCount(UInt64 /* requestedCount */ ,bool isWrite)
{
    if (isWrite) {
        return(_maxWriteByteTransfer);
    } else {
        return(_maxReadByteTransfer);
    }
}

OSDictionary *
IOHDDrive::constructMediaProperties(void)
{
#ifdef notyet //xxx
    
    OSDictionary *propTable;
    OSData *prop;

    propTable = OSDictionary::withCapacity(6);
    
    if (propTable) {
        
        prop = OSData::withBytes((void *)(&_mediaBlockSize),sizeof(UInt64));
        if (prop) {
            propTable->setObject(prop,"phys-blocksize");
            prop->release(); // XXX -- svail: added.
        }

        prop = OSData::withBytes((void *)(&_maxBlockNumber),sizeof(UInt64));
        if (prop) {
            propTable->setObject(prop,"highest-block");
            prop->relase(); // XXX -- svail: added.
        }

        prop = OSData::withBytes((void *)(&_writeProtected),sizeof(bool));
        if (prop) {
            propTable->setObject(prop,"write-protected");
            prop->release(); // XXX -- svail: added.
        }
    }

    return(propTable);

#else
    return(0);
#endif //xxx
}

/* Decommission a piece of media that has become unavailable either due to
 * ejection or some outside force (e.g. the Giant Hand of the User).
 * (I prefer the term "decommission" rather than "abandon." The former implies
 * a well-determined procedure, whereas the latter implies leaving the media
 * in an orphaned state.)
 */
/* Tear down the stack above the specified object. Usually these objects will
 * be of type IOMedia, but they could be any IOService.
 */
IOReturn
IOHDDrive::decommissionMedia(bool forcible)
{
    IOReturn result;

    result = tearDown(_mediaObject);

    /* If this is a forcible decommission (i.e. media is gone), we don't care
     * whether the teardown worked; we forget about the media.
     */
    
    if (forcible || (result == kIOReturnSuccess)) {	/* then it tore down successfully */
        _mediaObject = 0;
        initMediaStates();			/* deny existence of any media */
    }

    return(result);
}

void
IOHDDrive::deleteContext(struct IOHDDrive::context *cx)
{
    if (cx->buffer) {
        cx->buffer->release();		/* reduce retain count on Memory Descriptor */
    }
    IODelete(cx,struct IOHDDrive::context,1);
}

IOReturn
IOHDDrive::ejectMedia(void)
{
    IOReturn result;

    if (_removable) {
        
        IOTakeLock(_mediaStateLock);

        result = decommissionMedia(false);	/* try to teardown */

        if (result == kIOReturnSuccess) {	/* eject */
            (void)_provider->doEjectMedia();	/* ignore any error */
        }

        IOUnlock(_mediaStateLock);

        return(result);
            
    } else {
        return(kIOReturnUnsupported);        
    }
}

void
IOHDDrive::executeRequest(UInt64               byteStart,
                          IOMemoryDescriptor * buffer,
                          IOStorageCompletion  completion)
{
    struct IOHDDrive::context *cx;
    UInt32 block;
    UInt32 nblks;
    IOReturn result;

    _stats.clientReceived++;
    
    if (!_mediaPresent) {		/* no media? you lose */
        _stats.clientCompletionsSent++;
        complete(completion, kIOReturnNoMedia,0);
        _stats.clientCompletionsDone++;
        return;
    }

    result = buffer->prepare();                          // (prepare the buffer)
    if ( result != kIOReturnSuccess )  {
        // (wiring or permissions failure?)
        _stats.clientCompletionsSent++;
        complete(completion, result,0);
        _stats.clientCompletionsDone++;
        return;
    }

    /* We know that we are never called with a request too large,
     * nor one that is misaligned with a block.
     */
    assert((byteStart           % _mediaBlockSize) == 0);
    assert((buffer->getLength() % _mediaBlockSize) == 0);
    
    block = byteStart           / _mediaBlockSize;
    nblks = buffer->getLength() / _mediaBlockSize;

    cx = allocateContext();
    if (cx == NULL) {
        if (buffer) {
            buffer->complete();
        }
        _stats.clientCompletionsSent++;
        complete(completion,kIOReturnNoResources,0);
        _stats.clientCompletionsDone++;
        return;
    }

    /* Copy our params to our context. */

    cx->completion	= completion;
    cx->buffer		= buffer;
    cx->buffer->retain();
    cx->byteStart	= byteStart;
    cx->byteCount	= buffer->getLength();

/***
    if (isWrite) {
        assert(buffer->getDirection() == kIODirectionOut);
    } else {
        assert(buffer->getDirection() == kIODirectionIn);
    }
***/
/**    
    IOLog("%s[IOHDDrive]::executeRequest; blk %d, nblks %d\n",getName(),
          (int)block,(int)nblks);
**/
/* Now the protocol-specific provider implements the actual
     * start of the data transfer: */

    _stats.providerSent++;
    result = _provider->doAsyncReadWrite(cx->buffer,
                                            block,nblks,
                                            gc_glue,
                                            this,(void *)cx);
    
    if (result != kIOReturnSuccess) {		/* it failed to start */
        _stats.providerReject++;
        IOLog("%s[IOHDDrive]; executeRequest: request failed to start!\n",getName());
        if (buffer) {
            buffer->complete();
        }
        _stats.clientCompletionsSent++;
        complete(completion,result,0);
        _stats.clientCompletionsDone++;
        deleteContext(cx);
        return;
    }

    if (showStats()) {
        IOLog("%s[IOHDDrive]; executeRequest: cr=%5d,ps=%5d,pr=%5d,pcr=%5d,ccs=%5d,ccd=%5d\n",
            getName(),
            _stats.clientReceived,
            _stats.providerSent,
            _stats.providerReject,
            _stats.providerCompletionsRcvd,
            _stats.clientCompletionsSent,
            _stats.clientCompletionsDone);
    }
}

IOReturn
IOHDDrive::formatMedia(UInt64 byteCapacity)
{
    if (!_mediaPresent) {
        return(kIOReturnNoMedia);
    }

    return(_provider->doFormatMedia(byteCapacity));
}

void
IOHDDrive::free(void)
{
    if (_mediaStateLock) {
        IOLockFree(_mediaStateLock);
    }
    
    super::free();
}

/* The Callback (C) entry from our provider. We just glue
 * right into C++.
 */

void
gc_glue(IOService *target,void *param,UInt64 actualTransferCount,IOReturn result)
{
    IOHDDrive *self;
    struct IOHDDrive::context *cx;

    self = (IOHDDrive *) target;
    cx = (struct IOHDDrive::context *)param;
    cx->result = result;
    cx->actualTransferCount = actualTransferCount;
    
    if (cx->result == kIOReturnSuccess) {
        assert(cx->byteCount == cx->actualTransferCount);
    }
    
    self->RWCompletion(cx);
}

const char *
IOHDDrive::getDeviceTypeName(void)
{
    return(kDeviceTypeHardDisk);
}

UInt32
IOHDDrive::getFormatCapacities(UInt64 * capacities,
                                            UInt32   capacitiesMaxCount) const
{
    return(_provider->doGetFormatCapacities(capacities,capacitiesMaxCount));
}

UInt64
IOHDDrive::getMediaBlockSize() const
{
    return(_mediaBlockSize);
}

IODrive::IOMediaState
IOHDDrive::getMediaState() const
{
    if (_mediaPresent) {
        return(kMediaOnline);
    } else {
        return(kMediaOffline);
    }
}

bool
IOHDDrive::handleStart(IOService * provider)
{
    IOReturn result;

    /* Print device name/type information on the console: */
    
//    IOLog("%s[IOHDDrive] ",getName());
    IOLog("%s drive: %s, %s, rev %s %s.\n",getName(),
                _provider->getVendorString(),
		_provider->getProductString(),
		_provider->getRevisionString(),
                _provider->getAdditionalDeviceInfoString());

    /*The protocol-specific provider determines whether the media is removable. */

    result = _provider->reportRemovability(&_removable);
    if (result != kIOReturnSuccess) {
	IOLog("%s[IOHDDrive]::handleStart; err '%s' from reportRemovability\n",
			getName(),stringFromReturn(result));
        return(false);
    }

    if (_removable) {

        /* The protocol-specific provider determines whether we must poll to detect
         * media insertion. Nonremovable devices never need polling.
         */
        
        result = _provider->reportPollRequirements(&_pollIsRequired,&_pollIsExpensive);
/**
	    IOLog("%s[IOHDDrive]::handleStart; pollIsRequired =  %s\n",
			getName(),_pollIsRequired ? "Y" : "N");
**/
            if (result != kIOReturnSuccess) {
	    IOLog("%s[IOHDDrive]::handleStart; err '%s' from reportPollRequirements\n",
			getName(),stringFromReturn(result));
            return(false);
        }
        
        /* The protocol-specific provider determines whether the media is ejectable
         * under software control.
         */
        result = _provider->reportEjectability(&_ejectable);
        if (result != kIOReturnSuccess) {
	    IOLog("%s[IOHDDrive]::handleStart; err '%s' from reportEjectability\n",
			getName(),stringFromReturn(result));
            return(false);
        }

        /* The protocol-specific provider determines whether the media is lockable
         * under software control.
         */
        result = _provider->reportLockability(&_lockable);
        if (result != kIOReturnSuccess) {
	    IOLog("%s[IOHDDrive]::handleStart; err '%s' from reportLockability\n",
			getName(),stringFromReturn(result));
            return(false);
        }

    } else {		/* fixed drive: not ejectable, not lockable */
        _ejectable	= false;
        _lockable	= false;
        _pollIsRequired	= true;		/* polling detects device disappearance */
    }
    
    /* Check for the device being ready with media inserted: */

//    IOLog("%s[IOHDDrive]::handleStart; calling checkForMedia\n",getName());

    result = checkForMedia();

    /* The poll should never fail for nonremovable media: */
    
    if (result != kIOReturnSuccess && !_removable) {
	IOLog("%s[IOHDDrive]::handleStart: err '%s' from checkForMedia\n",
			getName(),stringFromReturn(result));
        return(false);
    }

    return(true);
}

/* The driver has been instructed to stop. If the media is writable, issue a
 * Synchronize Cache command to ensure that all blocks have been written to
 * the media. Then attempt to eject it.
 */
void
IOHDDrive::handleStop(IOService * provider)
{
    if (_mediaPresent) {
        if (!_writeProtected) {
            (void)_provider->doSynchronizeCache();	/* ignore any error */
        }
        (void)ejectMedia();		/* eject media if it's removable */
    }

    super::detach(provider);
}

bool
IOHDDrive::init(OSDictionary * properties)
{
    /* Do minimal initialization: */

    initMediaStates();
    
    _removable		= false;
    _ejectable		= false;
    _lockable		= false;
    _pollIsExpensive	= false;
    _pollIsRequired	= false;
    
    _mediaBlockSize		= 0;
    _maxBlockNumber		= 0;
    _maxReadByteTransfer	= 0;
    _maxWriteByteTransfer	= 0;

    bzero(&_stats,sizeof(struct stats));

    _mediaStateLock = IOLockAlloc();
    if (_mediaStateLock == 0)  {
        return false;
    }
    IOLockInit(_mediaStateLock);

    return(super::init(properties));
}

void
IOHDDrive::initMediaStates(void)
{
    _mediaPresent	= false;
    _writeProtected    	= false;
}

IOMedia *
IOHDDrive::instantiateDesiredMediaObject(void)
{
    return(new IOMedia);
}

IOReturn
IOHDDrive::instantiateMediaObject(IOMedia **media,UInt64 base,UInt64 byteSize,UInt32 blockSize,
                                        bool isWholeMedia,char *mediaName)
{
    IOMedia *m;
    bool result;

    *media = NULL;				/* just for safety */
    
    m = instantiateDesiredMediaObject();
    if (m == NULL) {
        return(kIOReturnNoMemory);
    }

    result = m->init(   base,			/* base byte offset */
                        byteSize,		/* byte size */
                        blockSize,		/* preferred block size */
        		_ejectable,		/* TRUE if ejectable */
                        isWholeMedia,		/* TRUE if whole physical media */
                        !_writeProtected,	/* TRUE if writable */
        		"");			/* content hint */

    if (result) {
        *media = m;
        m->setName(mediaName);
        return(kIOReturnSuccess);
        
    } else {					/* some init error */
        m->release();
        return(kIOReturnBadArgument);		/* beats me...call it this error */
    }
}

bool
IOHDDrive::isMediaEjectable(void) const
{
    return(_ejectable);
}

bool
IOHDDrive::isMediaPollExpensive(void) const
{
    return(_pollIsExpensive);
}

bool
IOHDDrive::isMediaPollRequired(void) const
{
    return(_pollIsRequired);
}

IOReturn
IOHDDrive::lockMedia(bool locked)
{
    if (_lockable) {
        return(_provider->doLockUnlockMedia(locked));
    } else {
        return(kIOReturnUnsupported);        
    }
}

IOReturn
IOHDDrive::pollMedia(void)
{
    if (!_pollIsRequired) {			/* shouldn't poll; it's an error */
        
        return(kIOReturnUnsupported);
        
    } else {					/* poll is required...do it */

        return(checkForMedia());
        
    }
}

IOService *
IOHDDrive::probe(IOService * provider,SInt32 * score)
{
    OSObject *object;
    OSString *prop;
    const char *string;
    
    if (!super::probe(provider,score)) {
        return(NULL);
    }

    /* Make sure the device type is what we expect. We can't simply use
     * the class type because many nubs are subclasses of the same superclass.
     */

    object = provider->getProperty(kDeviceTypeProperty);
    if (object == NULL) {
        IOLog("%s[IOHDDrive]:;probe; match failed: provider '%s' has no device-type property\n",
            getName(),provider->getName());
        return(NULL);
    }
    
    prop = OSDynamicCast(OSString,provider->getProperty(kDeviceTypeProperty));
    
    if (prop == NULL) {		/* the property doesn't exist */
        IOLog("%s[IOHDDrive]:;probe; match failed: provider '%s' device-type property is wrong type\n",
            getName(),provider->getName());
        return(NULL);
        
    } else {			/* we got the property */
        string = prop->getCStringNoCopy();
/**
        IOLog("%s[IOHDDrive]:;probe; provider '%s' device-type property is '%s'\n",
              getName(),provider->getName(),string);
**/
        if (strcmp(string,getDeviceTypeName()) != 0) {	/* the value isn't what we want */
/***
            IOLog("%s[IOHDDrive]:;probe; match failed: provider '%s' device type '%s' is wrong\n",
              getName(),provider->getName(),string);
***/
            return(NULL);
        }

    /* The device type is right; now make sure the class is. */
        
        if (setProvider(provider)) {		/* the provider's class is what we expect */
//            IOLog("%s[IOHDDrive]:;probe; matching provider '%s'\n",getName(),_provider->getName());
            return(this);
        } else {				/* the provider's class is wrong, ignore it */
            IOLog("%s[IOHDDrive]:;probe; match failed: provider '%s' is wrong class\n",
                  getName(),provider->getName());
            return(NULL);
        }
    }
}

IOReturn
IOHDDrive::recordAdditionalMediaParameters(void)
{
    return(kIOReturnSuccess);			/* default does nothing */
}
    
IOReturn
IOHDDrive::recordMediaParameters(void)
{
    IOReturn result;
    OSDictionary *propTable;

    /* Determine the device's block size and max block number.
     * What should an unformatted device report? All zeroes, or an error?
     */

    result = _provider->reportBlockSize(&_mediaBlockSize);    
    if (result != kIOReturnSuccess) {
        goto err;
    }

    result = _provider->reportMaxValidBlock(&_maxBlockNumber);    
    if (result != kIOReturnSuccess) {
        goto err;
    }

    /* Calculate the maximum allowed byte transfers for reads and writes. */

    result = _provider->reportMaxReadTransfer(_mediaBlockSize,&_maxReadByteTransfer);
    if (result != kIOReturnSuccess) {
        goto err;
    }

    result = _provider->reportMaxWriteTransfer(_mediaBlockSize,&_maxWriteByteTransfer);
    if (result != kIOReturnSuccess) {
        goto err;
    }

    /* Is the media write-protected? */

    result = _provider->reportWriteProtection(&_writeProtected);
    if (result != kIOReturnSuccess) {
        goto err;
    }

    /* Create media-related properties: */

    propTable = constructMediaProperties();
    
   // xxx what to do with propTable? xxx
        
    /* Record additional media-related information,
     * and perhaps create properties.
     */

    result = recordAdditionalMediaParameters();
    if (result != kIOReturnSuccess) {
        goto err;
    }

    return(kIOReturnSuccess);		/* everything was successful */

    /* If we fall thru to here, we had some kind of error. Set everything to
     * a reasonable state since we haven't got any real information.
     */

err:;
    _mediaPresent = false;
    _writeProtected = true;

    return(result);
}

void
IOHDDrive::rejectMedia(void)
{
    (void)_provider->doEjectMedia();	/* eject it, ignoring any error */
    initMediaStates();			/* deny existence of new media */
}

void                    
IOHDDrive::RWCompletion(struct IOHDDrive::context *cx)   
{
    bool isWrite;

    _stats.providerCompletionsRcvd++;
    
    if (cx->result == kIOReturnSuccess) {
        assert(cx->byteCount == cx->actualTransferCount);
    } else {
        IOLog("%s[IOHDDrive]::RWCompletion; result = %s, req=%d, actual=%d\n",
              getName(),stringFromReturn(cx->result),(int)cx->byteCount,
              (int)cx->actualTransferCount);
    }
    
    if (cx->buffer->getDirection() == kIODirectionOut) {
        isWrite = true;
    } else {
        isWrite = false;
    }

    addToBytesTransferred(cx->actualTransferCount,0,0,isWrite);
    if (cx->result != kIOReturnSuccess) {
        incrementErrors(isWrite);
    }   

    if (cx->buffer) {
        cx->buffer->complete();
    }

    /* Complete the client request, which was issued at executeRequest: */

    assert(cx->completion.action);

    _stats.clientCompletionsSent++;
    complete(cx->completion,cx->result,cx->actualTransferCount);
    _stats.clientCompletionsDone++;

    if (showStats()) {
        IOLog("%s[IOHDDrive]; RWCompletion:   cr=%5d,ps=%5d,pr=%5d,pcr=%5d,ccs=%5d,ccd=%5d\n",
            getName(),
            _stats.clientReceived,
            _stats.providerSent,
            _stats.providerReject,
            _stats.providerCompletionsRcvd,
            _stats.clientCompletionsSent,
            _stats.clientCompletionsDone);
    }
    deleteContext(cx);
}   

bool
IOHDDrive::setProvider(IOService * provider)
{
    _provider = OSDynamicCast(IOHDDriveNub,provider);

    if (_provider == NULL) {
        return(false);
    } else {
        return(true);
    }
}

bool
IOHDDrive::showStats(void)
{
    return(false);
}

/* Tear down the stack above the specified object. Usually these objects will
 * be of type IOMedia, but they could be any IOService.
 */
IOReturn
IOHDDrive::tearDown(IOService *media)
{
    IOReturn result;

    lockForArbitration();

    if (media) {
        if (media->terminate()) {
            media->release();

            initMediaStates();        /* clear all knowledge of the media */
            result = kIOReturnSuccess;

        } else {
            result = kIOReturnBusy;
        }
    } else {
        result = kIOReturnNoMedia;
    }

    unlockForArbitration();

    return(result);
}

bool
IOHDDrive::validateNewMedia(void)
{
    return(true);
}
