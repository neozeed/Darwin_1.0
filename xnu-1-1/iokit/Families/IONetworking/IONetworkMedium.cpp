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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * IONetworkMedium.cpp
 *
 * HISTORY
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSSerialize.h>
#include <IOKit/network/IONetworkMedium.h>

//---------------------------------------------------------------------------
// OSMetaClass macros.

#define super OSObject
OSDefineMetaClassAndStructors( IONetworkMedium, OSObject )

//---------------------------------------------------------------------------
// Initialize an IONetworkMedium instance.
//
// type:  The medium type, the fields are encoded with bits defined in
//        IONetworkMedium.h.
//
// speed: The maximum (or the only) link speed supported over this medium
//        in units of bits per second.
//
// flags: An optional flag for the medium object.
//        See IONetworkMedium.h for defined flags.
//
// index: An optional 32-bit index assigned by the caller. Drivers can use
//        this to store an index or a pointer to a media table inside the 
//        driver, or it may map to a driver defined media type.
//
// name:  An name to assign to this medium object. If 0, then a name
//        will be created based on the medium type given using nameForType().
//
// Returns true on success, false otherwise.

bool IONetworkMedium::init(IOMediumType  type,
                           UInt64        speed,
                           UInt32        flags = 0,
                           UInt32        index = 0,
                           const char *  name  = 0)
{
    if (!super::init())
        return false;

    _desc = (IOMediumDescriptor *) IOMalloc(sizeof(*_desc));
    if (!_desc)
        return false;
    bzero(_desc, sizeof(*_desc));

    _desc->type  = type;
    _desc->flags = flags;
    _desc->speed = speed;
    _desc->data  = index;

    if (name)
        _name = OSSymbol::withCString(name);
    else
        _name = IONetworkMedium::nameForType(type);

    if (!_name)
        return false;

    return true;
}

//---------------------------------------------------------------------------
// Factory method which performs allocation and initialization
// of an IONetworkMedium instance.
//
// Returns an IONetworkMedium instance on success, or 0 otherwise.

IONetworkMedium * IONetworkMedium::medium(IOMediumType  type,
                                          UInt64        speed,
                                          UInt32        flags = 0,
                                          UInt32        index = 0,
                                          const char *  name  = 0)
{
    IONetworkMedium * medium = new IONetworkMedium;
    
    if (medium && !medium->init(type, speed, flags, index, name))
    {
        medium->release();
        medium = 0;
    }

    return medium;
}

//---------------------------------------------------------------------------
// Free the IONetworkMedium instance.

void IONetworkMedium::free()
{
    if (_name)
        _name->release();
    if (_desc)
        IOFree(_desc, sizeof(*_desc));

    super::free();
}

//---------------------------------------------------------------------------
// Return the assigned medium type.

IOMediumType IONetworkMedium::getType() const
{
    return _desc->type;
}

//---------------------------------------------------------------------------
// Return the medium flags.

UInt32 IONetworkMedium::getFlags() const
{
    return _desc->flags;
}

//---------------------------------------------------------------------------
// Return the maximum medium speed.

UInt64 IONetworkMedium::getSpeed() const
{
    return _desc->speed;
}

//---------------------------------------------------------------------------
// Return the assigned index.

UInt32 IONetworkMedium::getIndex() const
{
    return _desc->data;
}

//---------------------------------------------------------------------------
// Return the name for this instance.

const OSSymbol * IONetworkMedium::getName() const
{
    return _name;
}

//---------------------------------------------------------------------------
// descP: The IOMediumDescriptor structure associated with the
//        instance is copied to a structure provided by the caller.

void IONetworkMedium::getDescriptor(IOMediumDescriptor * descP) const
{
    assert(descP);
    bcopy(_desc, descP, sizeof(*descP));
}

//===========================================================================
// The definitions and the routines were adapted from FreeBSD
// (net/if_media.c). Should try to reuse the similar function in BSD.
//===========================================================================
/*
 * Copyright (c) 1997
 *  Jonathan Stone and Jason R. Thorpe.  All rights reserved.
 *
 * This software is derived from information provided by Matt Thomas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jonathan Stone
 *  and Jason R. Thorpe for the NetBSD Project.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Append a C string to a buffer. A counter is updated with the remaining
 * size of the buffer.
 */
static void append_string(char **      buf,
                          int *        remain,
                          const char * str)
{
    char * buffer = *buf;
    int needed_length;

    if (!str)
        return;

    // The remaining size must be big enough for 'str',
    // and a terminating null character.

    needed_length = strlen(str) + 1;
    
    if (*remain >= needed_length) {     // buffer is large enough
        bcopy(str, buffer, strlen(str));
        buffer += strlen(str);
        *buffer = '\0';

        // decrement the remaining count, not counting the null char.
        //
        *remain -= (needed_length - 1);
        *buf = buffer;
    }
}

struct IOMediumTypesAndOptions {
    struct IOMediumDescription * types;
    struct IOMediumDescription * options;
};

static struct IOMediumDescription familyDescriptions[] =
    kIOMediumFamilyDescriptions;

static struct IOMediumDescription ethernetDescriptions[] =
    kIOMediumEthernetDescriptions;

static struct IOMediumDescription ethernetOptionDescriptions[] =
    kIOMediumEthernetOptionDescriptions;

static struct IOMediumDescription commonDescriptions[] =
    kIOMediumCommonDescriptions;

static struct IOMediumDescription commonOptionDescriptions[] =
   kIOMediumCommonOptionDescriptions;

static struct IOMediumTypesAndOptions typesAndOptionsTable[] = {
    {
      &ethernetDescriptions[0],
      &ethernetOptionDescriptions[0]
    },
};

#define IO_IFM_APPEND_OPTION(str) {                         \
    if (seen_option == 0)                                   \
       append_string(&buf, &maxlen, " (");                  \
    append_string(&buf, &maxlen, seen_option++ ? "," : ""); \
    append_string(&buf, &maxlen, str);                      \
}

/*
 * print a media word.
 */
static int
ifmedia_printword(int ifmw, char * buf, int maxlen)
{
    struct IOMediumDescription *     desc;
    struct IOMediumTypesAndOptions * ttos;
    int    seen_option = 0;

    /* Find the media family first */
    for (desc = familyDescriptions, ttos = typesAndOptionsTable;
        desc->string != NULL;
        desc++, ttos++)
        if (IOMediumGetFamily(ifmw) == desc->word)
            break;

    if (desc->string == NULL)
        return 0;   // unknown family type.

#if 0   // disable family printing (e.g. Ethernet).
    append_string(&buf, &maxlen, desc->string);
#endif

    /*
     * Print the family specific type description. If the description
     * is not found, then look at the common type descriptions.
     */
    for (desc = ttos->types; desc->string != NULL; desc++)
        if (IOMediumGetType(ifmw) == desc->word)
            goto got_subtype;

    for (desc = commonDescriptions; desc->string != NULL; desc++)
        if (IOMediumGetIndex(ifmw) == desc->word)
            break;

    if (desc->string == NULL)
        return 0;   // unknown family specific type.

got_subtype:
#if 0   // by disabling type printing, we don't need the space padding.
    append_string(&buf, &maxlen, " ");
#endif
    append_string(&buf, &maxlen, desc->string);

    /*
     * Look for common options.
     */
    for (desc = commonOptionDescriptions; desc->string != NULL; desc++) {
        if (ifmw & desc->word)
            IO_IFM_APPEND_OPTION(desc->string);
    }

    /*
     * Look for family specific options.
     */
    for (desc = ttos->options; desc->string != NULL; desc++) {
        if (ifmw & desc->word)
            IO_IFM_APPEND_OPTION(desc->string);
    }

    /*
     * Print the instance number.
     */
    if (IOMediumGetInstance(ifmw)) {
        char   instanceString[16];
        sprintf(instanceString, "%d", IOMediumGetInstance(ifmw));
        IO_IFM_APPEND_OPTION(instanceString);
    }

    append_string(&buf, &maxlen, seen_option ? ")" : "");

    return 1;
}
//===========================================================================
// End of FreeBSD import.
//===========================================================================

//---------------------------------------------------------------------------
// Given a 32-bit medium type, create an unique OSymbol name for the medium.
// The caller is responsible for releasing the OSSymbol object returned.
//
// type: A medium type. See IONetworkMedium.h for type encoding.
//
// Returns an OSSymbol created based on the type provided.

const OSSymbol * IONetworkMedium::nameForType(IOMediumType type)
{
    const UInt maxNameLen = 160;
    char       buffer[maxNameLen];
    
    if (ifmedia_printword(type, buffer, maxNameLen) == 0)
        return 0;   // error, cannot create name.

    // Caller must remember to free the OSSymbol!
    //
    return OSSymbol::withCString(buffer);
}

//---------------------------------------------------------------------------
// Test for equality between two IONetworkMedium objects.
// Two IONetworkMedium objects are considered equal if
// they have similar properties assigned to them during initialization.
//
// medium: An IONetworkMedium to test against the IONetworkMedium
//         object being called.
//
// Returns true if equal, false otherwise.

bool IONetworkMedium::isEqualTo(const IONetworkMedium * medium) const
{
    return ((bcmp(medium->_desc, _desc, sizeof(*_desc)) == 0) &&
            (medium->_name == _name));
}

//---------------------------------------------------------------------------
// Test for equality between a IONetworkMedium object and an OSObject.
// The OSObject is considered equal to the IONetworkMedium object if the 
// OSObject is an IONetworkMedium, and they have similar properties assigned
// to them during initialization.
//
// obj: An OSObject to test against the IONetworkMedium object being called.
//
// Returns true if equal, false otherwise.

bool IONetworkMedium::isEqualTo(const OSObject * obj) const
{
    IONetworkMedium * medium;
    if ((medium = OSDynamicCast(IONetworkMedium, (OSObject *) obj)))
        return isEqualTo(medium);
    else
        return false;
}

//---------------------------------------------------------------------------
// Create an OSData containing an IOMediumDescriptor structure (not copied), 
// and ask the OSData to serialize.
//
// s: An OSSerialize object to handle the serialization.
//
// Returns true on success, false otherwise.

bool IONetworkMedium::serialize(OSSerialize * s) const
{
    bool      ret = false;
    OSData *  descriptorData;

    // Create an OSData that encapsulates our IOMediumDescriptor structure.
    // Then instruct the OSData to serialize itself.
    //
    descriptorData = OSData::withBytesNoCopy((void *) _desc, sizeof(*_desc));
    if (descriptorData) {
        ret = descriptorData->serialize(s);
        descriptorData->release();
    }

    return ret;
}

//---------------------------------------------------------------------------
// A helper function to add an IONetworkMedium object to a given dictionary.
// The name of the medium is used as the key for the new dictionary entry.
//
// dict:   An OSDictionary object where the medium object should be added to.
// medium: The IONetworkMedium object to add to the dictionary.
//
// Returns true on success, false otherwise.

bool IONetworkMedium::addMedium(OSDictionary *          dict,
                                const IONetworkMedium * medium)
{
    // Arguments type checking.
    //
    if (!OSDynamicCast(OSDictionary, dict) ||
        !OSDynamicCast(IONetworkMedium, medium))
        return false;
    
    return dict->setObject(medium->getName(), (OSObject *) medium);
}

//---------------------------------------------------------------------------
// A helper function to remove an entry in a dictionary with a key that
// matches the name of the IONetworkMedium object provided.
//
// dict:   An OSDictionary object where the medium object should be removed
//         from.
// medium: The name of this medium object is used as the removal key.

void IONetworkMedium::removeMedium(OSDictionary *          dict,
                                   const IONetworkMedium * medium)
{
    // Arguments type checking.
    //
    if (!OSDynamicCast(OSDictionary, dict) ||
        !OSDynamicCast(IONetworkMedium, medium))
        return;

    dict->removeObject(medium->getName());
}

//---------------------------------------------------------------------------
// Iterate through a dictionary and return an IONetworkMedium entry that 
// satisfies the matching criteria. Returns 0 if there is no match.
// Also see getMediumWithType() and getMediumWithIndex() that
// are specialized forms derived from this method.
//
// dict:  The dictionary to look for a match.
// match: An IOMediumDescriptor structure containing the matching fields.
// mask:  An IOMediumDescriptor structure containing the matching mask.
//        Bits set are used for matching, while cleared bits are don't
//        care bits.
//
// The first matching IONetworkMedium entry found, or 0 if no match was found.

IONetworkMedium * IONetworkMedium::getMedium(const OSDictionary *        dict,
                                             IOMediumDescriptor *  match,
                                             IOMediumDescriptor *  mask)
{
    OSCollectionIterator *  iter;
    OSSymbol *              keyObject;
    IONetworkMedium *       medium;
    IONetworkMedium *       retMedium = 0;
    IOMediumDescriptor *    desc;
    UInt                    i;

    assert(match && mask);

    if (!dict) return 0;

    // Shouldn't withCollection take an (const OSDictionary *) argument?

    iter = OSCollectionIterator::withCollection((OSDictionary *) dict);
    if (!iter)
        return 0;

    while ( (keyObject = (OSSymbol *) iter->getNextObject()) )
    {
        medium = OSDynamicCast(IONetworkMedium, dict->getObject(keyObject));
        if (!medium)
            continue;

        desc = medium->_desc;

        // Perform bytewise matching.
        //
        for (i = 0; i < sizeof(IOMediumDescriptor); i++) {
            if ( (((UInt8 *) desc)[i] ^ ((UInt8 *) match)[i]) &
                 ((UInt8 *) mask)[i] )
                break;
        }
        if (i == sizeof(IOMediumDescriptor)) {  // match found.
            retMedium = medium;
            break;
        }
    }

    iter->release();

    return retMedium;
}

//---------------------------------------------------------------------------
// getMedium___ macro.
//
// Returns a IONetworkMedium entry that has a IOMediumDescriptor
// with a 'field' which matches the given 'value'.
//
// name    - function name.
// vartype - data type of the IOMediumDescriptor field.
// field   - field in IOMediumDescriptor to perform search.
// mask    - don't care bits.

#define GET_MEDIUM_FUNC(name, vartype, field)                         \
IONetworkMedium *                                                     \
IONetworkMedium::getMediumWith ## name(const OSDictionary * dict,     \
                                       vartype              value,    \
                                       vartype              mask = 0) \
{                                                                     \
    IOMediumDescriptor  match[2];                                     \
                                                                      \
    bzero(&match[0], sizeof(match));                                  \
                                                                      \
    match[0]. ## field = value;                                       \
    match[1]. ## field = ~mask;                                       \
                                                                      \
    return IONetworkMedium::getMedium(dict, &match[0], &match[1]);    \
}

GET_MEDIUM_FUNC(Type, IOMediumType, type)   // getMediumWithType()
GET_MEDIUM_FUNC(Index, UInt32, data)        // getMediumWithIndex()
