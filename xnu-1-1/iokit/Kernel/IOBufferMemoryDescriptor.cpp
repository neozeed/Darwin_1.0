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
#include <IOKit/assert.h>
#include <IOKit/system.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define super IOGeneralMemoryDescriptor
OSDefineMetaClassAndStructors(IOBufferMemoryDescriptor,
				IOGeneralMemoryDescriptor);

/*
 * initWithCapacity:
 *
 * Initialize a new IOBufferMemoryDescriptor with a buffer large enough to
 * hold capacity bytes.  The descriptor's length is initially set to zero.
 */
bool IOBufferMemoryDescriptor::initWithCapacity(vm_size_t   inCapacity,
                                                IODirection inDirection,
                                                bool        inContiguous)
{
    if (!inCapacity)
        return false;

    /* Allocate a wired-down buffer inside kernel space. */
    _buffer       = (inContiguous) ?
				IOMallocContiguous(inCapacity, inCapacity, 0) :
                                IOMalloc(inCapacity);
    _contiguous   = inContiguous;
    _capacity     = inCapacity;
    _physAddrs    = 0;
    _physSegCount = 0;

    if (!_buffer)
        return false;

    if (!super::initWithAddress(_buffer, inCapacity, inDirection))
	return false;

    /* Precompute virtual-to-physical page mappings. */
    vm_address_t inBuffer = (vm_address_t) _buffer;
    _physSegCount = atop(trunc_page(inBuffer + inCapacity - 1) -
                         trunc_page(inBuffer)) + 1;
    _physAddrs = IONew(IOPhysicalAddress, _physSegCount);
    if (!_physAddrs)
	return false;

    inBuffer = trunc_page(inBuffer);
    for (unsigned i = 0; i < _physSegCount; i++) {
        _physAddrs[i] = pmap_extract(get_task_pmap(kernel_task), inBuffer);
        assert(_physAddrs[i]); /* supposed to be wired */
        inBuffer += page_size;
    }

    // start out with no data
    setLength(0);
    
    return true;
}

/*
 * withCapacity:
 *
 * Returns a new IOBufferMemoryDescriptor with a buffer large enough to
 * hold capacity bytes.  The descriptor's length is initially set to zero.
 */
IOBufferMemoryDescriptor *
IOBufferMemoryDescriptor::withCapacity(vm_size_t   inCapacity,
                                       IODirection inDirection,
                                       bool        inContiguous)
{
    IOBufferMemoryDescriptor *me = new IOBufferMemoryDescriptor;
    
    if (me && !me->initWithCapacity(inCapacity, inDirection, inContiguous)) {
	me->release();
	me = 0;
    }
    return me;
}

/*
 * initWithBytes:
 *
 * Initialize a new IOBufferMemoryDescriptor preloaded with bytes (copied).
 * The descriptor's length and capacity are set to the input buffer's size.
 */
bool IOBufferMemoryDescriptor::initWithBytes(const void * inBytes,
                                             vm_size_t    inLength,
                                             IODirection  inDirection,
                                             bool         inContiguous)
{
    if (!initWithCapacity(inLength, inDirection, inContiguous))
        return false;

    if (!appendBytes(inBytes, inLength))
        return false;

    return true;
}

/*
 * withBytes:
 *
 * Returns a new IOBufferMemoryDescriptor preloaded with bytes (copied).
 * The descriptor's length and capacity are set to the input buffer's size.
 */
IOBufferMemoryDescriptor *
IOBufferMemoryDescriptor::withBytes(const void * inBytes,
                                    vm_size_t    inLength,
                                    IODirection  inDirection,
                                    bool         inContiguous)
{
    IOBufferMemoryDescriptor *me = new IOBufferMemoryDescriptor;

    if (me && !me->initWithBytes(inBytes, inLength, inDirection, inContiguous)){
        me->release();
        me = 0;
    }
    return me;
}

/*
 * free:
 *
 * Free resources
 */
void IOBufferMemoryDescriptor::free()
{
    if (_physAddrs)
        IODelete(_physAddrs, IOPhysicalAddress, _physSegCount);

    super::free();

    if (_buffer) {
        if (_contiguous)
            IOFreeContiguous(_buffer, _capacity);
        else
            IOFree(_buffer, _capacity);
    }
}

/*
 * getCapacity:
 *
 * Get the buffer capacity
 */
vm_size_t IOBufferMemoryDescriptor::getCapacity() const
{
    return _capacity;
}

/*
 * setLength:
 *
 * Change the buffer length of the memory descriptor.  When a new buffer is
 * created with initWithBytes, the descriptor's length is set to capacity,
 * or its length is initially zero.  This method allows one to resize the
 * buffer length up to capacity or less.  This eliminates the need to
 * destroy and create new buffers when different sizes are needed.  The
 * descriptor position is implicitly reset to zero as a result of this call.
 */
void IOBufferMemoryDescriptor::setLength(vm_size_t length)
{
    assert(length <= _capacity);

    _length = length;
    _singleRange.v.length = length;
}

/*
 * setDirection:
 *
 * Change the direction of the transfer.  This method allows one to redirect
 * the descriptor's transfer direction.  This eliminates the need to destroy
 * and create new buffers when different transfer directions are needed.
 */
void IOBufferMemoryDescriptor::setDirection(IODirection direction)
{
    _direction = direction;
}

/*
 * appendBytes:
 *
 * Add some data to the end of the buffer.  This method automatically
 * maintains the memory descriptor buffer length.  Note that appendBytes
 * will not copy past the end of the memory descriptor's current capacity.
 */
bool
IOBufferMemoryDescriptor::appendBytes(const void * bytes, vm_size_t withLength)
{
    vm_size_t actualBytesToCopy = min(withLength, _capacity - _length);

    assert(_length <= _capacity);
    bcopy(/* from */ bytes, (void *)(_singleRange.v.address + _length),
          actualBytesToCopy);
    _length += actualBytesToCopy;
    _singleRange.v.length += actualBytesToCopy;

    return true;
}

/*
 * getBytesNoCopy:
 *
 * Return the virtual address of the beginning of the buffer
 */
void * IOBufferMemoryDescriptor::getBytesNoCopy()
{
    return (void *)_singleRange.v.address;
}

/*
 * getBytesNoCopy:
 *
 * Return the virtual address of an offset from the beginning of the buffer
 */
void *
IOBufferMemoryDescriptor::getBytesNoCopy(vm_size_t start, vm_size_t withLength)
{
    if (start < _length && (start + withLength) <= _length)
        return (void *)(_singleRange.v.address + start);
    return 0;
}

/*
 * getPhysicalSegment:
 *
 * Get the physical address of the buffer, relative to the current position.
 * If the current position is at the end of the buffer, a zero is returned.
 */
IOPhysicalAddress
IOBufferMemoryDescriptor::getPhysicalSegment(IOByteCount offset,
					IOPhysicalLength * lengthOfSegment)
{
    if( offset != _position)
	setPosition( offset );

    assert(_position <= _length);

    /* Fail gracefully if the position is at (or past) the end-of-buffer. */
    if (_position >= _length) {
        *lengthOfSegment = 0;
        return 0;
    }

    /* Compute the largest contiguous physical length possible. */
    vm_address_t actualPos  = _singleRange.v.address + _position;
    vm_address_t actualPage = trunc_page(actualPos);
    unsigned     physInd    = atop(actualPage-trunc_page(_singleRange.v.address));

    vm_size_t physicalLength = actualPage + page_size - actualPos;
    for (unsigned index = physInd + 1; index < _physSegCount &&
         _physAddrs[index] == _physAddrs[index-1] + page_size; index++) {
        physicalLength += page_size;
    }

    /* Clip contiguous physical length at the end-of-buffer. */
    if (physicalLength > _length - _position)
        physicalLength = _length - _position;

    *lengthOfSegment = physicalLength;
    return _physAddrs[physInd] + (actualPos - actualPage);
}
