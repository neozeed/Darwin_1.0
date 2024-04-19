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
 * HISTORY
 *
 */

#include <IOKit/assert.h>
#include <IOKit/system.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/IOKitDebug.h>

#include <libkern/c++/OSContainers.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOMemoryDescriptor, OSObject )
OSDefineAbstractStructors( IOMemoryDescriptor, OSObject )

#define super IOMemoryDescriptor

OSDefineMetaClassAndStructors(IOGeneralMemoryDescriptor, IOMemoryDescriptor)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * withAddress:
 *
 * Create a new IOMemoryDescriptor.  The buffer is a virtual address
 * relative to the specified task.  If no task is supplied, the kernel
 * task is implied.
 */
IOMemoryDescriptor *
IOMemoryDescriptor::withAddress(void *      address,
                                IOByteCount   withLength,
                                IODirection withDirection)
{
    IOGeneralMemoryDescriptor * that = new IOGeneralMemoryDescriptor;
    if (that)
    {
	if (that->initWithAddress(address, withLength, withDirection))
	    return that;

        that->release();
    }
    return 0;
}

IOMemoryDescriptor *
IOMemoryDescriptor::withAddress(vm_address_t address,
                                IOByteCount  withLength,
                                IODirection  withDirection,
                                task_t       withTask)
{
    IOGeneralMemoryDescriptor * that = new IOGeneralMemoryDescriptor;
    if (that)
    {
	if (that->initWithAddress(address, withLength, withDirection, withTask))
	    return that;

        that->release();
    }
    return 0;
}

IOMemoryDescriptor *
IOMemoryDescriptor::withPhysicalAddress(
				IOPhysicalAddress	address,
				IOByteCount		withLength,
				IODirection      	withDirection )
{
    return( IOMemoryDescriptor::withAddress( address, withLength,
					withDirection, (task_t) 0  ));
}


/*
 * withRanges:
 *
 * Create a new IOMemoryDescriptor. The buffer is made up of several
 * virtual address ranges, from a given task.
 *
 * Passing the ranges as a reference will avoid an extra allocation.
 */
IOMemoryDescriptor *
IOMemoryDescriptor::withRanges(	IOVirtualRange * ranges,
				UInt32           withCount,
				IODirection      withDirection,
				task_t           withTask,
				bool             asReference = false)
{
    IOGeneralMemoryDescriptor * that = new IOGeneralMemoryDescriptor;
    if (that)
    {
	if (that->initWithRanges(ranges, withCount, withDirection, withTask, asReference))
	    return that;

        that->release();
    }
    return 0;
}

IOMemoryDescriptor *
IOMemoryDescriptor::withPhysicalRanges(	IOPhysicalRange * ranges,
                                        UInt32          withCount,
                                        IODirection     withDirection,
                                        bool            asReference = false)
{
    IOGeneralMemoryDescriptor * that = new IOGeneralMemoryDescriptor;
    if (that)
    {
	if (that->initWithPhysicalRanges(ranges, withCount, withDirection, asReference))
	    return that;

        that->release();
    }
    return 0;
}

IOMemoryDescriptor *
IOMemoryDescriptor::withSubRange(IOMemoryDescriptor *	of,
				IOByteCount		offset,
				IOByteCount		length,
				IODirection		withDirection)
{
    IOSubMemoryDescriptor * that = new IOSubMemoryDescriptor;

    if (that && !that->initSubRange(of, offset, length, withDirection)) {
        that->release();
	that = 0;
    }
    return that;
}

/*
 * initWithAddress:
 *
 * Initialize an IOMemoryDescriptor. The buffer is a virtual address
 * relative to the specified task.  If no task is supplied, the kernel
 * task is implied.
 *
 * An IOMemoryDescriptor can be re-used by calling initWithAddress or
 * initWithRanges again on an existing instance -- note this behavior
 * is not commonly supported in other I/O Kit classes, although it is
 * supported here.
 */
bool
IOGeneralMemoryDescriptor::initWithAddress(void *      address,
                                    IOByteCount   withLength,
                                    IODirection withDirection)
{
    _singleRange.v.address = (vm_address_t) address;
    _singleRange.v.length  = withLength;

    return initWithRanges(&_singleRange.v, 1, withDirection, kernel_task, true);
}

bool
IOGeneralMemoryDescriptor::initWithAddress(vm_address_t address,
                                    IOByteCount    withLength,
                                    IODirection  withDirection,
                                    task_t       withTask)
{
    _singleRange.v.address = address;
    _singleRange.v.length  = withLength;

    return initWithRanges(&_singleRange.v, 1, withDirection, withTask, true);
}

bool
IOGeneralMemoryDescriptor::initWithPhysicalAddress(
				 IOPhysicalAddress	address,
				 IOByteCount		withLength,
				 IODirection      	withDirection )
{
    _singleRange.p.address = address;
    _singleRange.p.length  = withLength;

    return initWithPhysicalRanges( &_singleRange.p, 1, withDirection, true);
}

/*
 * initWithRanges:
 *
 * Initialize an IOMemoryDescriptor. The buffer is made up of several
 * virtual address ranges, from a given task
 *
 * Passing the ranges as a reference will avoid an extra allocation.
 *
 * An IOMemoryDescriptor can be re-used by calling initWithAddress or
 * initWithRanges again on an existing instance -- note this behavior
 * is not commonly supported in other I/O Kit classes, although it is
 * supported here.
 */
bool
IOGeneralMemoryDescriptor::initWithRanges(
                                   IOVirtualRange * ranges,
                                   UInt32           withCount,
                                   IODirection      withDirection,
                                   task_t           withTask,
                                   bool             asReference = false)
{
    assert(ranges);
    assert(withCount);

    /*
     * We can check the _initialized  instance variable before having ever set
     * it to an initial value because I/O Kit guarantees that all our instance
     * variables are zeroed on an object's allocation.
     */

    if (_initialized == false)
    {
        if (super::init() == false)  return false;
        _initialized = true;
    }
    else
    {
        /*
         * An existing memory descriptor is being retargeted to point to
         * somewhere else.  Clean up our present state.
         */

        assert(_wireCount == 0);

        while (_wireCount)
            complete();
        if (_kernPtr)
            unmapFromKernel();
        if (_ranges.v && _rangesIsAllocated)
            IODelete(_ranges.v, IOVirtualRange, _rangesCount);
    }

    /*
     * Initialize the memory descriptor.
     */

    _ranges.v              = 0;
    _rangesCount           = withCount;
    _rangesIsAllocated     = asReference ? false : true;
    _direction             = withDirection;
    _length                = 0;
    _task                  = withTask;
    _position              = 0;
    _positionAtIndex       = 0;
    _positionAtOffset      = 0;
    _kernPtr               = 0;
    _cachedPhysicalAddress = 0;
    _cachedVirtualAddress  = 0;

    if (asReference)
        _ranges.v = ranges;
    else
    {
        _ranges.v = IONew(IOVirtualRange, withCount);
        if (_ranges.v == 0)  return false;
        bcopy(/* from */ ranges, _ranges.v, withCount * sizeof(IOVirtualRange));
    } 

    for (unsigned index = 0; index < _rangesCount; index++)
    {
        _length += _ranges.v[index].length;
    }

    return true;
}

bool
IOGeneralMemoryDescriptor::initWithPhysicalRanges(	IOPhysicalRange * ranges,
                                        	UInt32           withCount,
                                        	IODirection      withDirection,
                                        	bool             asReference = false)
{
#warning assuming virtual, physical addresses same size
    return( initWithRanges( (IOVirtualRange *) ranges,
			withCount, withDirection, (task_t) 0, asReference ));
}

/*
 * free
 *
 * Free resources.
 */
void IOGeneralMemoryDescriptor::free()
{
    while (_wireCount)
        complete();
    if (_kernPtr)
        unmapFromKernel();
    if (_ranges.v && _rangesIsAllocated)
        IODelete(_ranges.v, IOVirtualRange, _rangesCount);
    super::free();
}

void IOGeneralMemoryDescriptor::unmapFromKernel()
{
    kern_return_t krtn;
    vm_offset_t off;
    // Pull the shared pages out of the task map
    // Do we need to unwire it first?
    for ( off = 0; off < _kernSize; off += page_size )
    {
	pmap_change_wiring(
			kernel_pmap,
			_kernPtrAligned + off,
			FALSE);

	pmap_remove(
			kernel_pmap,
			_kernPtrAligned + off,
			_kernPtrAligned + off + page_size);
    }
    // Free the former shmem area in the task
    krtn = vm_deallocate(kernel_map,
			_kernPtrAligned,
			_kernSize );
    assert(krtn == KERN_SUCCESS);
    _kernPtr = 0;
}

void IOGeneralMemoryDescriptor::mapIntoKernel(unsigned rangeIndex)
{
    kern_return_t krtn;
    vm_offset_t off;

    if (_kernPtr)
    {
        if (_kernPtrAtIndex == rangeIndex)  return;
        unmapFromKernel();
        assert(_kernPtr == 0);
    }
 
    vm_offset_t srcAlign = trunc_page(_ranges.v[rangeIndex].address);

    _kernSize = trunc_page(_ranges.v[rangeIndex].address +
                           _ranges.v[rangeIndex].length  +
                           page_size - 1) - srcAlign;

    /* Find some memory of the same size in kernel task.  We use vm_allocate()
    to do this. vm_allocate inserts the found memory object in the
    target task's map as a side effect. */
    krtn = vm_allocate( kernel_map,
	    &_kernPtrAligned,
	    _kernSize,
	    TRUE );	    // Find first fit
    assert(krtn == KERN_SUCCESS);
    if(krtn)  return;

    /* For each page in the area allocated from the kernel map,
	    find the physical address of the page.
	    Enter the page in the target task's pmap, at the
	    appropriate target task virtual address. */
    for ( off = 0; off < _kernSize; off += page_size )
    {
	vm_offset_t kern_phys_addr, phys_addr;
	if( _task)
	    phys_addr = pmap_extract( get_task_pmap(_task), srcAlign + off );
	else
	    phys_addr = srcAlign + off;
        assert(phys_addr);
	if(phys_addr == 0)  return;

	// Check original state.
	kern_phys_addr = pmap_extract( kernel_pmap, _kernPtrAligned + off );
	// Set virtual page to point to the right physical one
	pmap_enter(
	    kernel_pmap,
	    _kernPtrAligned + off,
	    phys_addr,
	    VM_PROT_READ|VM_PROT_WRITE,
	    TRUE);
    }
    _kernPtr = _kernPtrAligned + (_ranges.v[rangeIndex].address - srcAlign);
    _kernPtrAtIndex = rangeIndex;
}

/*
 * getDirection:
 *
 * Get the direction of the transfer.
 */
IODirection IOMemoryDescriptor::getDirection() const
{
    return _direction;
}

/*
 * getLength:
 *
 * Get the length of the transfer (over all ranges).
 */
IOByteCount IOMemoryDescriptor::getLength() const
{
    return _length;
}

void IOMemoryDescriptor::setTag(
	IOOptionBits		tag )
{
    _tag = tag;    
}

IOOptionBits IOMemoryDescriptor::getTag( void )
{
    return( _tag);
}

/*
 * setPosition
 *
 * Set the logical start position inside the client buffer.
 *
 * It is convention that the position reflect the actual byte count that
 * is successfully transferred into or out of the buffer, before the I/O
 * request is "completed" (ie. sent back to its originator).
 */

void IOGeneralMemoryDescriptor::setPosition(IOByteCount position)
{
    assert(position <= _length);

    if (position >= _length)
    {
        _position         = _length;
        _positionAtIndex  = _rangesCount;          /* careful: out-of-bounds */
        _positionAtOffset = 0;
        return;
    }

    if (position < _position)
    {
	_positionAtOffset = position;
	_positionAtIndex  = 0;
    }
    else
    {
	_positionAtOffset += (position - _position);
    }
    _position = position;

    while (_positionAtOffset >= _ranges.v[_positionAtIndex].length)
    {
        _positionAtOffset -= _ranges.v[_positionAtIndex].length;
        _positionAtIndex++;
    }
}

/*
 * readBytes:
 *
 * Copy data from the memory descriptor's buffer into the specified buffer,
 * relative to the current position.   The memory descriptor's position is
 * advanced based on the number of bytes copied.
 */

IOByteCount IOGeneralMemoryDescriptor::readBytes(IOByteCount offset,
					void * bytes, IOByteCount withLength)
{
    IOByteCount bytesLeft;
    void *    segment;
    IOByteCount segmentLength;

    if( offset != _position)
	setPosition( offset );

    withLength = min(withLength, _length - _position);
    bytesLeft  = withLength;

#if 0
    while (bytesLeft && (_position < _length))
    {
	/* Compute the relative length to the end of this virtual segment. */
        segmentLength = min(_ranges.v[_positionAtIndex].length - _positionAtOffset, bytesLeft);

	/* Compute the relative address of this virtual segment. */
        segment = (void *)(_ranges.v[_positionAtIndex].address + _positionAtOffset);

	if (KERN_SUCCESS != vm_map_read_user(get_task_map(_task),
		/* from */ (vm_offset_t) segment, /* to */ (vm_offset_t) bytes,
		/* size */ segmentLength))
	{
	    assert( false );
            bytesLeft = withLength;
	    break;
	}
        bytesLeft -= segmentLength;
	offset += segmentLength;
	setPosition(offset);
    }
#else
    while (bytesLeft && (segment = getVirtualSegment(offset, &segmentLength)))
    {
        segmentLength = min(segmentLength, bytesLeft);
        bcopy(/* from */ segment, /* to */ bytes, /* size */ segmentLength);
        bytesLeft -= segmentLength;
	offset += segmentLength;
    }
#endif

    return withLength - bytesLeft;
}

/*
 * writeBytes:
 *
 * Copy data to the memory descriptor's buffer from the specified buffer,
 * relative to the current position.  The memory descriptor's position is
 * advanced based on the number of bytes copied.
 */
IOByteCount IOGeneralMemoryDescriptor::writeBytes(IOByteCount offset,
				const void* bytes,IOByteCount withLength)
{
    IOByteCount bytesLeft;
    void *    segment;
    IOByteCount segmentLength;

    if( offset != _position)
	setPosition( offset );

    withLength = min(withLength, _length - _position);
    bytesLeft  = withLength;

#if 0
    while (bytesLeft && (_position < _length))
    {
	assert(_position <= _length);

	/* Compute the relative length to the end of this virtual segment. */
        segmentLength = min(_ranges.v[_positionAtIndex].length - _positionAtOffset, bytesLeft);

	/* Compute the relative address of this virtual segment. */
        segment = (void *)(_ranges.v[_positionAtIndex].address + _positionAtOffset);

	if (KERN_SUCCESS != vm_map_write_user(get_task_map(_task),
		/* from */ (vm_offset_t) bytes, 
	        /* to */ (vm_offset_t) segment,
		/* size */ segmentLength))
	{
	    assert( false );
            bytesLeft = withLength;
	    break;
	}
        bytesLeft -= segmentLength;
	offset += segmentLength;
	setPosition(offset);
    }
#else
    while (bytesLeft && (segment = getVirtualSegment(offset, &segmentLength)))
    {
        segmentLength = min(segmentLength, bytesLeft);
        bcopy(/* from */ bytes, /* to */ segment, /* size */ segmentLength);
        bytesLeft -= segmentLength;
        offset += segmentLength;
    }
#endif

    return withLength - bytesLeft;
}

/*
 * getPhysicalSegment:
 *
 * Get the physical address of the buffer, relative to the current position.
 * If the current position is at the end of the buffer, a zero is returned.
 */
IOPhysicalAddress
IOGeneralMemoryDescriptor::getPhysicalSegment(IOByteCount offset,
						IOByteCount * lengthOfSegment)
{
    vm_address_t      virtualAddress;
    IOByteCount       virtualLength;
    pmap_t            virtualPMap;
    IOPhysicalAddress physicalAddress;
    IOPhysicalLength  physicalLength;

    if ((0 == _task) && (1 == _rangesCount))
    {
	assert(offset <= _length);
	if (offset >= _length)
	{
	    physicalAddress = 0;
	    physicalLength  = 0;
	}
	else
	{
	    physicalLength = _length - offset;
	    physicalAddress = offset + _ranges.v[0].address;
	}

	if (lengthOfSegment)
	    *lengthOfSegment = physicalLength;
	return physicalAddress;
    }

    if( offset != _position)
	setPosition( offset );

    assert(_position <= _length);

    /* Fail gracefully if the position is at (or past) the end-of-buffer. */
    if (_position >= _length)
    {
        *lengthOfSegment = 0;
        return 0;
    }

    /* Prepare to compute the largest contiguous physical length possible. */

    virtualAddress  = _ranges.v[_positionAtIndex].address + _positionAtOffset;
    virtualLength   = _ranges.v[_positionAtIndex].length  - _positionAtOffset;
    if( _task)
	virtualPMap     = get_task_pmap(_task);
    else
	virtualPMap	= 0;

    physicalAddress = (virtualAddress == _cachedVirtualAddress) ?
                        _cachedPhysicalAddress :              /* optimization */
			virtualPMap ?
                        	pmap_extract(virtualPMap, virtualAddress) :
				virtualAddress;
    physicalLength  = trunc_page(physicalAddress) + page_size - physicalAddress;

    if (physicalAddress == 0)     /* memory must be wired in order to proceed */
    {
        assert(physicalAddress);
        *lengthOfSegment = 0;
        return 0;
    }

    /* Compute the largest contiguous physical length possible, within range. */
    vm_address_t      virtualPage  = trunc_page(virtualAddress);
    IOPhysicalAddress physicalPage = trunc_page(physicalAddress);

    while (physicalLength < virtualLength)
    {
        physicalPage          += page_size;
        virtualPage           += page_size;
        _cachedVirtualAddress  = virtualPage;
        _cachedPhysicalAddress = virtualPMap ?
                        		pmap_extract(virtualPMap, virtualPage) :
					virtualPage;

        if (_cachedPhysicalAddress != physicalPage)  break;

        physicalLength += page_size;
    }

    /* Clip contiguous physical length at the end of this range. */
    if (physicalLength > virtualLength)
        physicalLength = virtualLength;

    if( lengthOfSegment)
	*lengthOfSegment = physicalLength;

    return physicalAddress;
}


/*
 * getVirtualSegment:
 *
 * Get the virtual address of the buffer, relative to the current position.
 * If the memory wasn't mapped into the caller's address space, it will be
 * mapped in now.   If the current position is at the end of the buffer, a
 * null is returned.
 */
void * IOGeneralMemoryDescriptor::getVirtualSegment(IOByteCount offset,
							IOByteCount * lengthOfSegment)
{
    if( offset != _position)
	setPosition( offset );

    assert(_position <= _length);

    /* Fail gracefully if the position is at (or past) the end-of-buffer. */
    if (_position >= _length)
    {
        *lengthOfSegment = 0;
        return 0;
    }

    /* Compute the relative length to the end of this virtual segment. */
    *lengthOfSegment = _ranges.v[_positionAtIndex].length - _positionAtOffset;

    /* Compute the relative address of this virtual segment. */
    if (_task == kernel_task)
        return (void *)(_ranges.v[_positionAtIndex].address + _positionAtOffset);
    else
    {
        mapIntoKernel(_positionAtIndex);
        return (void *)(_kernPtr + _positionAtOffset);
    }
}

/*
 * prepare
 *
 * Prepare the memory for an I/O transfer.  This involves paging in
 * the memory, if necessary, and wiring it down for the duration of
 * the transfer.  The complete() method completes the processing of
 * the memory after the I/O transfer finishes.  This method needn't
 * called for non-pageable memory.
 */
IOReturn IOGeneralMemoryDescriptor::prepare(
		IODirection forDirection = kIODirectionNone)
{
    IOReturn rc = kIOReturnSuccess;

    _wireCount++;
    if(_task && (_task != kernel_task) && (_wireCount == 1)) {
        UInt rangeIndex;
        kern_return_t rc;
        vm_prot_t access = VM_PROT_DEFAULT;	// Could be cleverer using _direction

        //
        // Check user read/write access to the data buffer.
        //

        for (rangeIndex = 0; rangeIndex < _rangesCount; rangeIndex++)
        {
            vm_offset_t checkBase = trunc_page(_ranges.v[rangeIndex].address);
            vm_size_t   checkSize = round_page(_ranges.v[rangeIndex].length );

            while (checkSize)
            {
                vm_region_basic_info_data_t regionInfo;
                mach_msg_type_number_t      regionInfoSize = sizeof(regionInfo);
                vm_size_t                   regionSize;

                if ( (vm_region(
                          /* map         */ get_task_map(_task),
                          /* address     */ &checkBase,
                          /* size        */ &regionSize,
                          /* flavor      */ VM_REGION_BASIC_INFO,
                          /* info        */ (vm_region_info_t) &regionInfo,
                          /* info size   */ &regionInfoSize,
                          /* object name */ 0 ) != KERN_SUCCESS             ) ||
                     ( (_direction & kIODirectionIn ) &&
                                   !(regionInfo.protection & VM_PROT_WRITE) ) ||
                     ( (_direction & kIODirectionOut) && 
                                   !(regionInfo.protection & VM_PROT_READ ) ) )
                {
                    return kIOReturnVMError;
                }

                assert((regionSize & PAGE_MASK) == 0);

                regionSize = min(regionSize, checkSize);
                checkSize -= regionSize;
                checkBase += regionSize;
            } // (for each vm region)
        } // (for each io range)

        for(rangeIndex = 0; rangeIndex < _rangesCount; rangeIndex++) {

            vm_offset_t srcAlign = trunc_page(_ranges.v[rangeIndex].address);
            IOByteCount srcAlignEnd = trunc_page(_ranges.v[rangeIndex].address +
                                _ranges.v[rangeIndex].length  +
                                page_size - 1);
            rc = vm_map_wire(get_task_map(_task), srcAlign,
                             srcAlignEnd, access, FALSE);
            if(rc != KERN_SUCCESS) {
		UInt doneIndex;
                IOLog("IOMemoryDescriptor::prepare: vm_map_wire failed: %d\n", rc);
                for(doneIndex = 0; doneIndex < rangeIndex; doneIndex++) {
                    vm_offset_t srcAlign = trunc_page(_ranges.v[doneIndex].address);
                    IOByteCount srcAlignEnd = trunc_page(_ranges.v[doneIndex].address +
                                        _ranges.v[doneIndex].length  +
                                        page_size - 1);
                    vm_map_unwire(get_task_map(_task), srcAlign,
                                     srcAlignEnd, FALSE);
		}
                _wireCount = 0;	// Back to unwired state now.
            }
	}
    }
    return rc;
}

/*
 * complete
 *
 * Complete processing of the memory after an I/O transfer finishes.
 * This method should not be called unless a prepare was previously
 * issued; the prepare() and complete() must occur in pairs, before
 * before and after an I/O transfer involving pageable memory.
 */
IOReturn IOGeneralMemoryDescriptor::complete(
		IODirection forDirection = kIODirectionNone)
{
    assert(_wireCount);
    _wireCount--;
    if(_task && (_task != kernel_task) && (_wireCount == 0)) {
        UInt rangeIndex;
        kern_return_t rc;
        for(rangeIndex = 0; rangeIndex < _rangesCount; rangeIndex++) {

            vm_offset_t srcAlign = trunc_page(_ranges.v[rangeIndex].address);
            IOByteCount srcAlignEnd = trunc_page(_ranges.v[rangeIndex].address +
                                _ranges.v[rangeIndex].length  +
                                page_size - 1);
            rc = vm_map_unwire(get_task_map(_task), srcAlign,
                                  srcAlignEnd, FALSE);
            if(rc != KERN_SUCCESS)
                IOLog("IOMemoryDescriptor::complete: vm_map_unwire failed: %d\n", rc);
        }
    }
    return kIOReturnSuccess;
}

IOReturn IOGeneralMemoryDescriptor::doMap(
	task_t		addressTask,
	IOVirtualAddress *	atAddress,
	IOOptionBits		options,
	IOByteCount		sourceOffset = 0,
	IOByteCount		length = 0 )
{
    // could be much better
    if( (addressTask == _task) && (options & kIOMapAnywhere)
	&& (1 == _rangesCount) && (0 == sourceOffset)
	&& (length <= _ranges.v[0].length) ) {
	    *atAddress = _ranges.v[0].address;
	    return( kIOReturnSuccess );
    }

    return( super::doMap( addressTask, atAddress,
				options, sourceOffset, length ));
}

IOReturn IOGeneralMemoryDescriptor::doUnmap(
	task_t		addressTask,
	IOVirtualAddress	logical,
	IOByteCount		length )
{
    // could be much better
    if( (addressTask == _task) && (1 == _rangesCount)
	 && (logical == _ranges.v[0].address)
	 && (length <= _ranges.v[0].length) )
	    return( kIOReturnSuccess );

    return( super::doUnmap( addressTask, logical, length ));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" {
// osfmk/device/iokit_rpc.c
extern kern_return_t IOMapPages(vm_map_t map, vm_offset_t va, vm_offset_t pa,
			vm_size_t length, unsigned int mapFlags);
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOLock * gIOMemoryLock;

#define LOCK	IOTakeLock( gIOMemoryLock)
#define UNLOCK	IOUnlock( gIOMemoryLock)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOMemoryMap, OSObject )
OSDefineAbstractStructors( IOMemoryMap, OSObject )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class _IOMemoryMap : public IOMemoryMap
{
    OSDeclareDefaultStructors(_IOMemoryMap)

    IOMemoryDescriptor *		memory;
    IOMemoryMap *	superMap;
    IOByteCount		offset;
    IOByteCount		length;
    IOVirtualAddress	logical;
    task_t	addressTask;
    IOOptionBits	options;

public:
    virtual void free();

    // IOMemoryMap methods
    virtual IOVirtualAddress 	getVirtualAddress();
    virtual IOByteCount 	getLength();
    virtual task_t	getAddressTask();
    virtual IOMemoryDescriptor * getMemoryDescriptor();
    virtual IOOptionBits 	getMapOptions();

    virtual IOReturn 		unmap();

    virtual IOPhysicalAddress 	getPhysicalSegment(IOByteCount offset,
	       					   IOByteCount * length);

    // for IOMemoryDescriptor use
    _IOMemoryMap *		isCompatible(
		IOMemoryDescriptor *		owner,
                task_t		task,
                IOVirtualAddress	toAddress,
                IOOptionBits		options,
                IOByteCount		offset,
                IOByteCount		length );

    bool _IOMemoryMap::init(
	IOMemoryDescriptor *		memory,
	IOMemoryMap *		superMap,
        IOByteCount		offset,
        IOByteCount		length );

    bool _IOMemoryMap::init(
	IOMemoryDescriptor *		memory,
	task_t		intoTask,
	IOVirtualAddress	toAddress,
	IOOptionBits		options,
        IOByteCount		offset,
        IOByteCount		length );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOMemoryMap

OSDefineMetaClassAndStructors(_IOMemoryMap, IOMemoryMap)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool _IOMemoryMap::init(
	IOMemoryDescriptor *	_memory,
	IOMemoryMap *		_superMap,
        IOByteCount		_offset,
        IOByteCount		_length )
{

    if( !super::init())
	return( false);

    if( (_offset + _length) > _superMap->getLength())
	return( false);

    _memory->retain();
    memory	= _memory;
    _superMap->retain();
    superMap 	= _superMap;

    offset	= _offset;
    if( _length)
        length	= _length;
    else
        length	= _memory->getLength();

    options	= superMap->getMapOptions();
    addressTask	= superMap->getAddressTask();
    logical	= superMap->getVirtualAddress() + offset;

    return( true );
}

bool _IOMemoryMap::init(
	IOMemoryDescriptor *	_memory,
	task_t		intoTask,
	IOVirtualAddress	toAddress,
	IOOptionBits		_options,
        IOByteCount		_offset,
        IOByteCount		_length )
{
    bool	ok;

    if( (!_memory) || !super::init())
	return( false);

    if( (_offset + _length) > _memory->getLength())
	return( false);

    _memory->retain();
    memory	= _memory;

    offset	= _offset;
    if( _length)
        length	= _length;
    else
        length	= _memory->getLength();

    addressTask	= intoTask;
    logical	= toAddress;
    options 	= _options;

    if( options & kIOMapStatic)
	ok = true;
    else
	ok = (kIOReturnSuccess == memory->doMap( addressTask, &logical,
						options, offset, length ));
    if( !ok)
	logical = 0;

    return( ok );
}

// documentation lies
#define VM_ALLOCATE_ANYWHERE TRUE
#define VM_ALLOCATE_RESERVED FALSE

IOReturn IOMemoryDescriptor::doMap(
	task_t		addressTask,
	IOVirtualAddress *	atAddress,
	IOOptionBits		options,
	IOByteCount		sourceOffset = 0,
	IOByteCount		length = 0 )
{
    IOReturn		err = kIOReturnSuccess;
    vm_size_t		ourSize;
    vm_size_t		bytes;
    vm_offset_t		mapped;
    vm_address_t	logical;
    IOByteCount		pageOffset;
    IOPhysicalLength	segLen;
    IOPhysicalAddress	physAddr;

    if( 0 == length)
	length = getLength();

    physAddr = getPhysicalSegment( sourceOffset, &segLen );
    assert( physAddr );

    pageOffset = physAddr - trunc_page( physAddr );
    ourSize = length + pageOffset;
    physAddr -= pageOffset;

    logical = *atAddress;
    if( 0 == (options & kIOMapAnywhere)) {
        mapped = trunc_page( logical );
	if( (logical - mapped) != pageOffset)
	    err = kIOReturnVMError;
    }
    if( kIOReturnSuccess == err)
        err = vm_allocate( get_task_map(addressTask), &mapped, ourSize,
            (options & kIOMapAnywhere)
		? VM_ALLOCATE_ANYWHERE : VM_ALLOCATE_RESERVED);

    if( err) {
        kprintf("IOMemoryDescriptor::doMap: vm_allocate()"
		" returned %08x\n", err);
        return( err);
    }
    logical = mapped;
    *atAddress = mapped + pageOffset;

    segLen += pageOffset;
    bytes = ourSize;
    do {
	// in the middle of the loop only map whole pages
	if( segLen >= bytes)
	    segLen = bytes;
	else if( segLen != trunc_page( segLen))
	    err = kIOReturnVMError;
        if( physAddr != trunc_page( physAddr))
	    err = kIOReturnBadArgument;

#ifdef DEBUG
	if( kIOLogMapping & gIOKitDebug)
	    kprintf("_IOMemoryMap::map(%x) %08x->%08x:%08x\n",
                addressTask, mapped + pageOffset, physAddr + pageOffset,
		segLen - pageOffset);
#endif

	if( kIOReturnSuccess == err)
            err = IOMapPages( get_task_map(addressTask), mapped, physAddr, segLen, options );
	if( err)
	    break;

	sourceOffset += segLen - pageOffset;
	mapped += segLen;
	bytes -= segLen;
	pageOffset = 0;

    } while( bytes
	&& (physAddr = getPhysicalSegment( sourceOffset, &segLen )));

    if( bytes)
        err = kIOReturnBadArgument;
    if( err)
	doUnmap( addressTask, logical, ourSize );
    else
        mapped = true;

    return( err );
}

IOReturn IOMemoryDescriptor::doUnmap(
	task_t		addressTask,
	IOVirtualAddress	logical,
	IOByteCount		length )
{
    IOReturn	err;

#ifdef DEBUG
    if( kIOLogMapping & gIOKitDebug)
	kprintf("IOMemoryDescriptor::doUnmap(%x) %08x:%08x\n",
                addressTask, logical, length );
#endif
    err = vm_deallocate( get_task_map(addressTask), logical, length );
    assert( err == kIOReturnSuccess );

    return( err );
}

IOReturn _IOMemoryMap::unmap( void )
{
    IOReturn	err;

    if( logical && (0 == superMap)
	&& (0 == (options & kIOMapStatic))) {

	err = memory->doUnmap( addressTask, logical, length );
	logical = 0;

    } else
	err = kIOReturnSuccess;

    return( err );
}

void _IOMemoryMap::free()
{
    unmap();

    if( memory) {
        LOCK;
	memory->removeMapping( this);
	UNLOCK;
	memory->release();
    }

    if( superMap)
	superMap->release();

    super::free();
}

IOByteCount _IOMemoryMap::getLength()
{
    return( length );
}

IOVirtualAddress _IOMemoryMap::getVirtualAddress()
{
    return( logical);
}

task_t _IOMemoryMap::getAddressTask()
{
    return( addressTask);
}

IOOptionBits _IOMemoryMap::getMapOptions()
{
    return( options);
}

IOMemoryDescriptor * _IOMemoryMap::getMemoryDescriptor()
{
    return( memory );
}

_IOMemoryMap * _IOMemoryMap::isCompatible(
		IOMemoryDescriptor *		owner,
                task_t		task,
                IOVirtualAddress	toAddress,
                IOOptionBits		_options,
                IOByteCount		_offset,
                IOByteCount		_length )
{
    _IOMemoryMap * mapping;

    if( addressTask != task)
	return( 0 );
    if( (options ^ _options) & (kIOMapCacheMask | kIOMapReadOnly))
	return( 0 );

    if( (0 == (_options & kIOMapAnywhere)) && (logical != toAddress))
	return( 0 );

    if( _offset < offset)
	return( 0 );

    _offset -= offset;

    if( (_offset + _length) > length)
	return( 0 );

    if( (length == _length) && (!_offset)) {
        retain();
	mapping = this;

    } else {
        mapping = new _IOMemoryMap;
        if( mapping
        && !mapping->init( owner, this, _offset, _length )) {
            mapping->release();
            mapping = 0;
        }
    }

    return( mapping );
}

IOPhysicalAddress _IOMemoryMap::getPhysicalSegment( IOByteCount _offset,
	       					    IOPhysicalLength * length)
{
    IOPhysicalAddress	address;

    LOCK;
    address = memory->getPhysicalSegment( offset + _offset, length );
    UNLOCK;

    return( address );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super OSObject

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOMemoryDescriptor::initialize( void )
{
    if( 0 == gIOMemoryLock)
	gIOMemoryLock = IOLockAlloc();
}

void IOMemoryDescriptor::free( void )
{
    if( _mappings)
	_mappings->release();

    super::free();
}

IOMemoryMap * IOMemoryDescriptor::setMapping(
	task_t		intoTask,
	IOVirtualAddress	mapAddress,
	IOOptionBits		options = 0 )
{
    _IOMemoryMap *		map;

    map = new _IOMemoryMap;

    LOCK;

    if( map
     && !map->init( this, intoTask, mapAddress,
                    options | kIOMapStatic, 0, getLength() )) {
	map->release();
	map = 0;
    }

    addMapping( map);

    UNLOCK;

    return( map);
}

IOMemoryMap * IOMemoryDescriptor::map( 
	IOOptionBits		options = 0 )
{

    return( makeMapping( this, kernel_task, 0,
			options | kIOMapAnywhere,
			0, getLength() ));
}

IOMemoryMap * IOMemoryDescriptor::map(
	task_t		intoTask,
	IOVirtualAddress	toAddress,
	IOOptionBits		options,
	IOByteCount		offset = 0,
	IOByteCount		length = 0 )
{
    if( 0 == length)
	length = getLength();

    return( makeMapping( this, intoTask, toAddress, options, offset, length ));
}

IOMemoryMap * IOMemoryDescriptor::makeMapping(
	IOMemoryDescriptor *	owner,
	task_t		intoTask,
	IOVirtualAddress	toAddress,
	IOOptionBits		options,
	IOByteCount		offset,
	IOByteCount		length )
{
    _IOMemoryMap *	mapping = 0;
    OSIterator *	iter;

    LOCK;

    do {
	// look for an existing mapping
	if( (iter = OSCollectionIterator::withCollection( _mappings))) {

            while( (mapping = (_IOMemoryMap *) iter->getNextObject())) {

		if( (mapping = mapping->isCompatible( 
					owner, intoTask, toAddress,
					options | kIOMapReference,
					offset, length )))
		    break;
            }
            iter->release();
            if( mapping)
                continue;
        }


	if( mapping || (options & kIOMapReference))
	    continue;

	owner = this;

        mapping = new _IOMemoryMap;
	if( mapping
	&& !mapping->init( owner, intoTask, toAddress, options,
			   offset, length )) {

	    IOLog("Didn't make map %08lx : %08lx\n", offset, length );
	    mapping->release();
            mapping = 0;
	}

    } while( false );

    owner->addMapping( mapping);

    UNLOCK;

    return( mapping);
}

void IOMemoryDescriptor::addMapping(
	IOMemoryMap * mapping )
{
    if( mapping) {
        if( 0 == _mappings)
            _mappings = OSSet::withCapacity(1);
	if( _mappings && _mappings->setObject( mapping ))
	    mapping->release(); 	/* really */
    }
}

void IOMemoryDescriptor::removeMapping(
	IOMemoryMap * mapping )
{

    assert( _mappings );
    assert( _mappings->containsObject( mapping ) );

    mapping->retain();
    mapping->retain();
    if( _mappings)
        _mappings->removeObject( mapping);
}

// a minimalist serializer
bool IOMemoryDescriptor::serialize(OSSerialize *s) const
{
    IOMemoryDescriptor *	self = (IOMemoryDescriptor *) this;
    OSNumber *	off;
    bool	ok = false;

    if( s->previouslySerialized(this)) return true;

    off = OSNumber::withNumber(self->getPhysicalAddress(), IOPhysSize);
    if( off)
	ok = off->serialize(s);
    if (off)
	off->release();

    return ok;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOMemoryDescriptor

OSDefineMetaClassAndStructors(IOSubMemoryDescriptor, IOMemoryDescriptor)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOSubMemoryDescriptor::initSubRange( IOMemoryDescriptor * parent,
					IOByteCount offset, IOByteCount length,
					IODirection withDirection )
{
    if( !super::init())
	return( false );

    if( !parent)
	return( false);

    if( (offset + length) > parent->getLength())
	return( false);

    parent->retain();
    _parent	= parent;
    _start	= offset;
    _length	= length;
    _direction  = withDirection;
    _tag	= parent->getTag();

    return( true );
}

void IOSubMemoryDescriptor::free( void )
{
    if( _parent)
	_parent->release();

    super::free();
}


IOPhysicalAddress IOSubMemoryDescriptor::getPhysicalSegment( IOByteCount offset,
						      	IOByteCount * length )
{
    IOPhysicalAddress	address;
    IOByteCount		actualLength;

    assert(offset <= _length);

    if( length)
        *length = 0;

    if( offset >= _length)
        return( 0 );

    address = _parent->getPhysicalSegment( offset + _start, &actualLength );

    if( address && length)
	*length = min( _length - offset, actualLength );

    return( address );
}

void * IOSubMemoryDescriptor::getVirtualSegment(IOByteCount offset,
					IOByteCount * lengthOfSegment)
{
    return( 0 );
}

IOByteCount IOSubMemoryDescriptor::readBytes(IOByteCount offset,
					void * bytes, IOByteCount withLength)
{
    IOByteCount	byteCount;

    assert(offset <= _length);

    if( offset >= _length)
        return( 0 );

    LOCK;
    byteCount = _parent->readBytes( _start + offset, bytes,
				min(withLength, _length - offset) );
    UNLOCK;

    return( byteCount );
}

IOByteCount IOSubMemoryDescriptor::writeBytes(IOByteCount offset,
				const void* bytes, IOByteCount withLength)
{
    IOByteCount	byteCount;

    assert(offset <= _length);

    if( offset >= _length)
        return( 0 );

    LOCK;
    byteCount = _parent->writeBytes( _start + offset, bytes,
				min(withLength, _length - offset) );
    UNLOCK;

    return( byteCount );
}

IOReturn IOSubMemoryDescriptor::prepare(
		IODirection forDirection = kIODirectionNone)
{
    IOReturn	err;

    LOCK;
    err = _parent->prepare( forDirection);
    UNLOCK;

    return( err );
}

IOReturn IOSubMemoryDescriptor::complete(
		IODirection forDirection = kIODirectionNone)
{
    IOReturn	err;

    LOCK;
    err = _parent->complete( forDirection);
    UNLOCK;

    return( err );
}

IOMemoryMap * IOSubMemoryDescriptor::makeMapping(
	IOMemoryDescriptor *	owner,
	task_t		intoTask,
	IOVirtualAddress	toAddress,
	IOOptionBits		options,
	IOByteCount		offset,
	IOByteCount		length )
{
    IOMemoryMap * mapping;

     mapping = (IOMemoryMap *) _parent->makeMapping(
					_parent, intoTask,
					toAddress - (_start + offset),
					options | kIOMapReference,
					_start + offset, length );

    if( !mapping)
	mapping = super::makeMapping( owner, intoTask, toAddress, options,
					offset, length );

    return( mapping );
}

/* ick */

bool
IOSubMemoryDescriptor::initWithAddress(void *      address,
                                    IOByteCount   withLength,
                                    IODirection withDirection)
{
    return( false );
}

bool
IOSubMemoryDescriptor::initWithAddress(vm_address_t address,
                                    IOByteCount    withLength,
                                    IODirection  withDirection,
                                    task_t       withTask)
{
    return( false );
}

bool
IOSubMemoryDescriptor::initWithPhysicalAddress(
				 IOPhysicalAddress	address,
				 IOByteCount		withLength,
				 IODirection      	withDirection )
{
    return( false );
}

bool
IOSubMemoryDescriptor::initWithRanges(
                                   	IOVirtualRange * ranges,
                                   	UInt32           withCount,
                                   	IODirection      withDirection,
                                   	task_t           withTask,
                                  	bool             asReference = false)
{
    return( false );
}

bool
IOSubMemoryDescriptor::initWithPhysicalRanges(	IOPhysicalRange * ranges,
                                        	UInt32           withCount,
                                        	IODirection      withDirection,
                                        	bool             asReference = false)
{
    return( false );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
