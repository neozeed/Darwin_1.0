/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/* IOData.m created by rsulack on Thu 25-Sep-1997 */


#include <libkern/c++/OSData.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSLib.h>

#define super OSObject

OSDefineMetaClassAndStructors(OSData, OSObject)

#define EXTERNAL ((unsigned int) -1)

#ifdef DEBUG
extern "C" {
    extern int debug_container_malloc_size;
};
#define ACCUMSIZE(s) do { debug_container_malloc_size += (s); } while(0)
#else
#define ACCUMSIZE(s)
#endif

bool OSData::initWithCapacity(unsigned int inCapacity)
{
    if (!super::init())
        return false;

    if(inCapacity) {
        data = (void *) kalloc(inCapacity);
        if (!data)
            return false;
    }

    length = 0;
    capacity = inCapacity;
    capacityIncrement = capacity;

    ACCUMSIZE(capacity);

    return true;
}

bool OSData::initWithBytes(const void *bytes, unsigned int inLength)
{
    if (!bytes || !initWithCapacity(inLength))
        return false;

    bcopy(bytes, data, inLength);
    length = inLength;

    return true;
}

bool OSData::initWithBytesNoCopy(void *bytes, unsigned int inLength)
{
    if (!super::init())
        return false;

    length = inLength;
    capacity = EXTERNAL;
    data = bytes;

    return true;
}

bool OSData::initWithData(const OSData *inData)
{
    return initWithBytes(inData->data, inData->length);
}

bool OSData::initWithData(const OSData *inData,
                          unsigned int start, unsigned int inLength)
{
    const void *localData = inData->getBytesNoCopy(start, inLength);

    if (localData)
        return initWithBytes(localData, inLength);
    else
        return false;
}

OSData *OSData::withCapacity(unsigned int inCapacity)
{
    OSData *me = new OSData;

    if (me && !me->initWithCapacity(inCapacity)) {
        me->free();
        return 0;
    }

    return me;
}

OSData *OSData::withBytes(const void *bytes, unsigned int inLength)
{
    OSData *me = new OSData;

    if (me && !me->initWithBytes(bytes, inLength)) {
        me->free();
        return 0;
    }
    return me;
}

OSData *OSData::withBytesNoCopy(void *bytes, unsigned int inLength)
{
    OSData *me = new OSData;

    if (me && !me->initWithBytesNoCopy(bytes, inLength)) {
        me->free();
        return 0;
    }

    return me;
}

OSData *OSData::withData(const OSData *inData)
{
    OSData *me = new OSData;

    if (me && !me->initWithData(inData)) {
        me->free();
        return 0;
    }

    return me;
}

OSData *OSData::withData(const OSData *inData,
                         unsigned int start, unsigned int inLength)
{
    OSData *me = new OSData;

    if (me && !me->initWithData(inData, start, inLength)) {
        me->free();
        return 0;
    }

    return me;
}

void OSData::free()
{
    if (capacity != EXTERNAL && data && capacity) {
        kfree((vm_offset_t)data, capacity);
        ACCUMSIZE( -capacity );
    }
    super::free();
}

unsigned int OSData::getLength() const { return length; }
unsigned int OSData::getCapacity() const { return capacity; }

unsigned int OSData::getCapacityIncrement() const 
{ 
    return capacityIncrement; 
}

unsigned int OSData::setCapacityIncrement(unsigned increment) 
{
    return capacityIncrement = increment; 
}

unsigned int OSData::ensureCapacity(unsigned int newCapacity)
{
    unsigned char * newData;

    if ( !capacityIncrement || (newCapacity <= capacity) )
        return capacity;

    newCapacity = (((newCapacity - 1) / capacityIncrement) + 1)
                * capacityIncrement;

    newData = (unsigned char *) kalloc(newCapacity);
    
    if ( newData ) {
        bcopy(data, newData, capacity);
        bzero(newData + capacity, newCapacity - capacity);
        kfree((vm_offset_t)data, capacity);
        ACCUMSIZE( newCapacity - capacity );
        data = (void *) newData;
        capacity = newCapacity;
    }

    return capacity;
}

bool OSData::appendBytes(const void *bytes, unsigned int inLength)
{
    unsigned int newSize;

    if (inLength == 0)
        return true;

    if (capacity == EXTERNAL)
        return false;
    
    newSize = length + inLength;
    if ( (newSize > capacity) && newSize > ensureCapacity(newSize) )
        return false;

    bcopy(bytes, &((unsigned char *)data)[length], inLength);
    length = newSize;

    return true;
}

bool OSData::appendBytes(const OSData *other)
{
    return appendBytes(other->data, other->length);
}

const void *OSData::getBytesNoCopy() const
{
    if (length == 0)
        return 0;
    else
        return data;
}

const void *OSData::getBytesNoCopy(unsigned int start,
                                   unsigned int inLength) const
{
    const void *outData = 0;

    if (length
    &&  start < length
    && (start + inLength) <= length)
        outData = (const void *) ((char *) data + start);

    return outData;
}

bool OSData::isEqualTo(const OSData *aData) const
{
    unsigned int len;

    len = aData->length;
    if ( length != len )
        return false;

    return isEqualTo(aData->data, len);
}

bool OSData::isEqualTo(const void *someData, unsigned int inLength) const
{
    return (length >= inLength) && (bcmp(data, someData, inLength) == 0);
}

bool OSData::isEqualTo(const OSObject *obj) const
{
    OSData *	data;
    if ((data = OSDynamicCast(OSData, (OSObject *)obj )))
        return isEqualTo(data);
    else
        return false;
}

//this was taken from CFPropertyList.c 
static const char __CFPLDataEncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool OSData::serialize(OSSerialize *s) const
{
    unsigned int i;
    const unsigned char *p;
    unsigned char c;

    if (s->previouslySerialized(this)) return true;

    if (!s->addXMLStartTag(this, "data")) return false;

    for (i = 0, p = (unsigned char *)data; i < length; i++, p++) {
        /* 3 bytes are encoded as 4 */
        switch (i % 3) {
	case 0:
		c = __CFPLDataEncodeTable [ ((p[0] >> 2) & 0x3f)];
		if (!s->addChar(c)) return false;
		break;
	case 1:
		c = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 4) & 0x3f)];
		if (!s->addChar(c)) return false;
		break;
	case 2:
		c = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 6) & 0x3f)];
		if (!s->addChar(c)) return false;
		c = __CFPLDataEncodeTable [ (p[0] & 0x3f)];
		if (!s->addChar(c)) return false;
		break;
	}
    }
    switch (i % 3) {
    case 0:
	    break;
    case 1:
	    c = __CFPLDataEncodeTable [ ((p[-1] << 4) & 0x30)];
	    if (!s->addChar(c)) return false;
	    if (!s->addChar('=')) return false;
	    if (!s->addChar('=')) return false;
	    break;
    case 2:
	    c = __CFPLDataEncodeTable [ ((p[-1] << 2) & 0x3c)];
	    if (!s->addChar(c)) return false;
	    if (!s->addChar('=')) return false;
	    break;
    }

    return s->addXMLEndTag("data");
}
