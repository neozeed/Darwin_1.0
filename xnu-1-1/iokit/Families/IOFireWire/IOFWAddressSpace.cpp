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
 *
 *	IOFWAddressSpace.cpp
 *
 * Classes which describe addresses in the local node which are accessable to other nodes
 * via firewire asynchronous read/write/lock requests.
 */
#include <IOKit/firewire/IOFWAddressSpace.h>

/*
 * Base class for FireWire address space objects
 */
OSDefineMetaClass( IOFWAddressSpace, OSObject )
OSDefineAbstractStructors(IOFWAddressSpace, OSObject)

/*
 * Direct physical memory <-> FireWire address.
 * Accesses to these addresses will be handled automatically by the
 * hardware without notification.
 *
 * The 64 bit FireWire address of (32 bit) physical addr xxxx:xxxx is hostNode:0000:xxxx:xxxx
 */
OSDefineMetaClassAndStructors(IOFWPhysicalAddressSpace, IOFWAddressSpace)

bool IOFWPhysicalAddressSpace::initWithDesc(IOMemoryDescriptor *mem)
{
    if(!IOFWAddressSpace::init())
        return false;
    fMem = mem;
    fLen = mem->getLength();
{
    vm_size_t pos;
    IOPhysicalAddress phys;
    pos = 0;
    while(pos < fLen) {
        IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
        pos += PAGE_SIZE - (phys & (PAGE_SIZE-1));
    }
}

    return true;
}

UInt32 IOFWPhysicalAddressSpace::doRead(UInt16 nodeID, FWAddress addr, UInt32 len, 
					IOMemoryDescriptor **buf, IOByteCount * offset,
					bool lockRead)
{
    UInt32 res = kFWResponseAddressError;
    vm_size_t pos;
    IOPhysicalAddress phys;
    if(addr.addressHi != 0)
	return kFWResponseAddressError;

    pos = 0;
    while(pos < fLen) {
        IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
        if(addr.addressLo >= phys && addr.addressLo+len <= phys+lengthOfSegment) {
            // OK, block is in space and is within one VM page
	    // Set position to exact start
	    *offset = pos + addr.addressLo - phys;
            *buf = fMem;
            res = kFWResponseComplete;
            break;
        }
        pos += PAGE_SIZE - (phys & (PAGE_SIZE-1));
    }
    return res;
}

UInt32 IOFWPhysicalAddressSpace::doWrite(UInt16 nodeID, FWAddress addr, UInt32 len, 
					const void *buf, bool lockWrite)
{
    UInt32 res = kFWResponseAddressError;
    vm_size_t pos;
    IOPhysicalAddress phys;
    if(addr.addressHi != 0)
	return kFWResponseAddressError;

    pos = 0;
    while(pos < fLen) {
        IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
        if(addr.addressLo >= phys && addr.addressLo+len <= phys+lengthOfSegment) {
            // OK, block is in space and is within one VM page
	    // Set position to exact start
            fMem->writeBytes(pos + addr.addressLo - phys, buf, len);
            res = kFWResponseComplete;
            break;
        }
        pos += PAGE_SIZE - (phys & (PAGE_SIZE-1));
    }
    return res;
}

/*
 * Pseudo firewire addresses usually represent emulated registers of some kind.
 * Accesses to these addresses will result in the owner being notified.
 * 
 * Virtual addresses should not have zero as the top 16 bits of the 48 bit local address,
 * since that may look like a physical address to hardware (eg. OHCI).
 * if reader is NULL then reads will not be allowed.
 * if writer is NULL then writes will not be allowed.
 * if either is NULL then lock requests will not be allowed.
 * refcon is passed back as the first argument of read and write callbacks.
 */
OSDefineMetaClassAndStructors(IOFWPseudoAddressSpace, IOFWAddressSpace)

UInt32 IOFWPseudoAddressSpace::simpleReader(void *refcon, UInt16 nodeID,
					FWAddress addr, UInt32 len, IOMemoryDescriptor **buf,
					IOByteCount * offset, bool lockRead)
{
    IOFWPseudoAddressSpace * space = (IOFWPseudoAddressSpace *)refcon;
    *buf = space->fDesc;
    *offset = addr.addressLo - space->fBase.addressLo;

    return kFWResponseComplete;
}

UInt32 IOFWPseudoAddressSpace::simpleWriter(void *refcon, UInt16 nodeID,
				FWAddress addr, UInt32 len, const void *buf, bool lockRead)
{
    IOFWPseudoAddressSpace * space = (IOFWPseudoAddressSpace *)refcon;
    space->fDesc->writeBytes(addr.addressLo - space->fBase.addressLo, buf, len);

    return kFWResponseComplete;
}

IOFWPseudoAddressSpace *
IOFWPseudoAddressSpace::simpleRead(FWAddress addr, UInt32 len, const void *data)
{
    IOFWPseudoAddressSpace * me;
    me = new IOFWPseudoAddressSpace;
    do {
        if(!me)
            break;
        if(!me->initAll(addr, len, simpleReader, NULL, (void *)me)) {
            me->release();
            me = NULL;
            break;
        }
        me->fDesc = IOMemoryDescriptor::withAddress((void *)data, len, kIODirectionOut);
        if(!me->fDesc) {
            me->release();
            me = NULL;
        }
    } while(false);

    return me;
}

IOFWPseudoAddressSpace *IOFWPseudoAddressSpace::simpleRW(FWAddress addr, UInt32 len, void *data)
{
    IOFWPseudoAddressSpace * me;
    me = new IOFWPseudoAddressSpace;
    do {
        if(!me)
            break;
        if(!me->initAll(addr, len, simpleReader, simpleWriter, (void *)me)) {
            me->release();
            me = NULL;
            break;
        }
        me->fDesc = IOMemoryDescriptor::withAddress(data, len, kIODirectionOut);
        if(!me->fDesc) {
            me->release();
            me = NULL;
        }
    } while(false);

    return me;
}

bool IOFWPseudoAddressSpace::initAll(FWAddress addr, UInt32 len, 
		FWReadCallback reader, FWWriteCallback writer, void *refCon)
{
    if(!IOFWAddressSpace::init())
	return false;

    fBase = addr;
    fBase.addressHi &= 0xFFFF;	// Mask off nodeID part.
    fLen = len;
    fDesc = NULL;	// Only used by simpleRead case.
    fRefCon = refCon;
    fReader = reader;
    fWriter = writer;
    return true;
}

void IOFWPseudoAddressSpace::free()
{
    if(fDesc)
	fDesc->release();
    IOFWAddressSpace::free();
}

UInt32 IOFWPseudoAddressSpace::doRead(UInt16 nodeID, FWAddress addr, UInt32 len, 
					IOMemoryDescriptor **buf, IOByteCount * offset,
					bool lockRead)
{
    if(addr.addressHi != fBase.addressHi)
	return kFWResponseAddressError;
    if(addr.addressLo < fBase.addressLo)
	return kFWResponseAddressError;
    if(addr.addressLo > fBase.addressLo+fLen)
	return kFWResponseAddressError;
    if(!fReader)
        return kFWResponseTypeError;
    if(lockRead && !fWriter)
        return kFWResponseTypeError;

    return fReader(fRefCon, nodeID, addr, len, buf, offset, lockRead);
}

UInt32 IOFWPseudoAddressSpace::doWrite(UInt16 nodeID, FWAddress addr, UInt32 len, 
					const void *buf, bool lockWrite)
{
    if(addr.addressHi != fBase.addressHi)
	return kFWResponseAddressError;
    if(addr.addressLo < fBase.addressLo)
	return kFWResponseAddressError;
    if(addr.addressLo > fBase.addressLo+fLen)
	return kFWResponseAddressError;
    if(!fWriter)
        return kFWResponseTypeError;
    if(lockWrite && !fReader)
        return kFWResponseTypeError;

    return fWriter(fRefCon, nodeID, addr, len, buf, lockWrite);
}
