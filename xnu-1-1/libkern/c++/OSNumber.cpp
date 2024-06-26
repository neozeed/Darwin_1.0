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
/* IOOffset.m created by rsulack on Wed 17-Sep-1997 */

#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSLib.h>

#define sizeMask ((1ULL << (size)) - 1)

#define super OSObject

OSDefineMetaClassAndStructors(OSNumber, OSObject)

bool OSNumber::init(unsigned long long inValue, unsigned int numberOfBits)
{
    if (!super::init())
        return false;

    size = numberOfBits;
    value = (inValue & sizeMask);

    return true;
}

bool OSNumber::init(const char *value, unsigned int numberOfBits)
{
    unsigned long long thisOffset;

#ifdef q_works
    sscanf(value, "%qi", thisOffset);
#else
    unsigned int smallOffset;

    sscanf(value, "%i", &smallOffset);
    thisOffset = smallOffset;
#endif

    return init(thisOffset, numberOfBits);
}

void OSNumber::free() { super::free(); }

OSNumber *OSNumber::withNumber(unsigned long long value,
                           unsigned int numberOfBits)
{
    OSNumber *me = new OSNumber;

    if (me && !me->init(value, numberOfBits)) {
        me->free();
        return 0;
    }

    return me;
}

OSNumber *OSNumber::withNumber(const char *value, unsigned int numberOfBits)
{
    OSNumber *me = new OSNumber;

    if (me && !me->init(value, numberOfBits)) {
        me->free();
        return 0;
    }

    return me;
}

unsigned int OSNumber::numberOfBits() const { return size; }

unsigned int OSNumber::numberOfBytes() const { return (size + 7) / 8; }


unsigned char OSNumber::unsigned8BitValue() const
{
    return (unsigned char) value;
}

unsigned short OSNumber::unsigned16BitValue() const
{
    return (unsigned short) value;
}

unsigned int OSNumber::unsigned32BitValue() const
{
    return (unsigned int) value;
}

unsigned long long OSNumber::unsigned64BitValue() const
{
    return value;
}

void OSNumber::addValue(signed long long inValue)
{
    value = ((value + inValue) & sizeMask);
}

void OSNumber::setValue(unsigned long long inValue)
{
    value = (inValue & sizeMask);
}

bool OSNumber::isEqualTo(const OSNumber *integer) const
{
    return((value == integer->value));
}

bool OSNumber::isEqualTo(const OSObject *obj) const
{
    OSNumber *	offset;
    if ((offset = OSDynamicCast(OSNumber, (OSObject *)obj )))
	return isEqualTo(offset);
    else
	return false;
}

bool OSNumber::serialize(OSSerialize *s) const
{
    char temp[32];
    
    if (s->previouslySerialized(this)) return true;

    sprintf(temp, "integer size=\"%d\"", size); 
    if (!s->addXMLStartTag(this, temp)) return false;
    
    //XXX    sprintf(temp, "0x%qx", value);
    if ((value >> 32)) {
        sprintf(temp, "0x%lx%08lx", (unsigned long)(value >> 32),
                    (unsigned long)(value & 0xFFFFFFFF));
    } else { 
        sprintf(temp, "0x%lx", (unsigned long)value);
    }
    if (!s->addString(temp)) return false;

    return s->addXMLEndTag("integer");
}
