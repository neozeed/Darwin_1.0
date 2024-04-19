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
 * IONetworkMedium.h
 *
 * HISTORY
 *
 */

#ifndef _IONETWORKMEDIUM_H
#define _IONETWORKMEDIUM_H

/*! @typedef IOMediumType
    @discussion A 32-bit value divided into fields describing the medium
    type. See IONetworkMedium.h. */

typedef UInt32 IOMediumType;

/*! @typedef IOMediumDescriptor
    @discussion A structure which describes the properties of an
    IONetworkMedium object. The fields in the structure consist of
    properties assigned to the object during init(). */

typedef struct {
    IOMediumType   type;
    UInt32         flags;
    UInt64         speed;
    UInt32         data;
    UInt32         rsvd[3];
} IOMediumDescriptor;

//===========================================================================
// Medium Type (IOMediumType).
//
// The medium type is encoded by a 32-bit value. The definitions of
// the fields and the encoding for each field is adapted from FreeBSD.
//
// Bits     Definition
// -------------------
//  3-0     medium index
//    4     reserved
//  7-5     medium family
// 15-8     type specific options
// 19-16    reserved
// 27-20    common options
// 31-28    instance number

// Common medium definitions.
//
enum {
    kIOMediumIndexAuto        = 0,   /* autoselect */
    kIOMediumIndexManual      = 1,   /* use manual configuration */
    kIOMediumIndexNone        = 2,   /* de-select all media */
};

// Ethernet medium definitions.
//
enum {
    kIOMediumFamilyEthernet = 0x00000020,
    kIOMediumEtherAuto      = (kIOMediumIndexAuto   | kIOMediumFamilyEthernet),
    kIOMediumEtherManual    = (kIOMediumIndexManual | kIOMediumFamilyEthernet),
    kIOMediumEtherNone      = (kIOMediumIndexNone   | kIOMediumFamilyEthernet),
    kIOMediumEther10BaseT     = (3  | kIOMediumFamilyEthernet),
    kIOMediumEther10Base2     = (4  | kIOMediumFamilyEthernet),
    kIOMediumEther10Base5     = (5  | kIOMediumFamilyEthernet),
    kIOMediumEther100BaseTX   = (6  | kIOMediumFamilyEthernet),
    kIOMediumEther100BaseFX   = (7  | kIOMediumFamilyEthernet),
    kIOMediumEther100BaseT4   = (8  | kIOMediumFamilyEthernet),
    kIOMediumEther100BaseVG   = (9  | kIOMediumFamilyEthernet),
    kIOMediumEther100BaseT2   = (10 | kIOMediumFamilyEthernet),
    kIOMediumEther1000BaseSX  = (11 | kIOMediumFamilyEthernet),
};

// Common options.
//
enum {
    kIOMediumFullDuplex       = 0x00100000,
    kIOMediumHalfDuplex       = 0x00200000,
    kIOMediumOptionFlag0      = 0x01000000,
    kIOMediumOptionFlag1      = 0x02000000,
    kIOMediumOptionFlag2      = 0x04000000,
};

// Medium type masks.
//
#define kIOMediumIndexMask          0x0000000f
#define kIOMediumFamilyMask         0x000000e0
#define kIOMediumTypeMask           (kIOMediumFamilyMask | kIOMediumIndexMask)
#define kIOMediumFamilyOptionsMask  0x0000ff00
#define kIOMediumCommonOptionsMask  0x0ff00000
#define kIOMediumInstanceShift      28
#define kIOMediumInstanceMask       0xf0000000

// Medium type field accessors.
//
#define IOMediumGetIndex(x)         ((x)  & kIOMediumIndexMask)
#define IOMediumGetFamily(x)        ((x)  & kIOMediumFamilyMask)
#define IOMediumGetType(x)          ((x)  & kIOMediumTypeMask)
#define IOMediumGetInstance(x)      (((x) & kIOMediumInstanceMask) >> \
                                            kIOMediumInstanceShift)

//===========================================================================
// Medium flags.

enum {
    kIOMediumFlagDriverMask   = 0xffff0000,     // fields for driver use
};

//===========================================================================
// Medium descriptions.

struct IOMediumDescription {
    SInt32       word;
    const char * string;
};

// Family descriptions.
//
#define kIOMediumFamilyDescriptions {              \
    { kIOMediumFamilyEthernet,  "Ethernet" },      \
    { 0, NULL },                                   \
}

// Ethernet type descriptions.
//
#define kIOMediumEthernetDescriptions {            \
    { kIOMediumEther10BaseT,    "10Base-T" },      \
    { kIOMediumEther10Base2,    "10Base-2" },      \
    { kIOMediumEther10Base5,    "10Base-5" },      \
    { kIOMediumEther100BaseTX,  "100Base-TX" },    \
    { kIOMediumEther100BaseFX,  "100Base-FX" },    \
    { kIOMediumEther100BaseT4,  "100Base-T4" },    \
    { kIOMediumEther100BaseVG,  "100Base-VG" },    \
    { kIOMediumEther100BaseT2,  "100Base-T2" },    \
    { kIOMediumEther1000BaseSX, "1000Base-SX" },   \
    { 0, NULL },                                   \
}

// Ethernet option descriptions.
//
#define kIOMediumEthernetOptionDescriptions {      \
    { 0, NULL },                                   \
}

// Common type descriptions.
//
#define kIOMediumCommonDescriptions {              \
    { kIOMediumIndexAuto,       "Auto" },          \
    { kIOMediumIndexManual,     "Manual" },        \
    { kIOMediumIndexNone,       "None" },          \
    { 0, NULL },                                   \
}

// Common option descriptions.
//
#define kIOMediumCommonOptionDescriptions {        \
    { kIOMediumFullDuplex,      "full-duplex" },   \
    { kIOMediumHalfDuplex,      "half-duplex" },   \
    { kIOMediumOptionFlag0,     "flag0" },         \
    { kIOMediumOptionFlag1,     "flag1" },         \
    { kIOMediumOptionFlag2,     "flag2" },         \
    { 0, NULL },                                   \
}

//===========================================================================
// Link status bits.
//
enum {
    kIONetworkLinkValid        = 0x00000001,    // link status is valid
    kIONetworkLinkActive       = 0x00000002,    // link is up/active.
};

//===========================================================================
// IONetworkMedium class.

#ifdef KERNEL

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSSymbol.h>

/*! @class IONetworkMedium
    @abstract An object that encapsulates information about a network
    medium (i.e. 10Base-T, 100Base-T Full Duplex). The primary use of
    this object is for network drivers to advertise its media
    capability, by adding a collection of IONetworkMedium objects stored
    in a dictionary to its property table.
    IONetworkMedium supports serialization, and will encode its
    properties in the form of an IOMediumDescriptor structure,
    wrapped by an OSData to the serialization stream when instructed. */

class IONetworkMedium : public OSObject
{
    OSDeclareDefaultStructors(IONetworkMedium)

protected:
    IOMediumDescriptor * _desc;
    const OSSymbol *     _name;

/*! @function free
    @abstract Free the IONetworkMedium instance. */

    virtual void free();

public:

/*! @function nameForType
    @abstract Create a name for a medium type.
    @discussion Given a 32-bit medium type, create an unique OSymbol name
    for the medium. The caller is responsible for releasing the OSSymbol 
    object returned.
    @param type A medium type. See IONetworkMedium.h for type encoding.
    @result An OSSymbol created based on the type provided. */

    static const OSSymbol * nameForType(IOMediumType type);

/*! @function addMedium
    @abstract Add an IONetworkMedium object to a dictionary.
    @discussion A helper function to add an IONetworkMedium object to a
    given dictionary. The name of the medium is used as the key for the
    new dictionary entry.
    @param dict An OSDictionary object where the medium object should be
    added to.
    @param medium The IONetworkMedium object to add to the dictionary.
    @result true on success, false otherwise. */

    static bool addMedium(OSDictionary * dict, const IONetworkMedium * medium);

/*! @function removeMedium
    @abstract Remove an IONetworkMedium object from a dictionary.
    @discussion A helper function to remove an entry in a dictionary.
    @param dict An OSDictionary object where the medium object should be
    removed from.
    @param medium The name of this medium object is used as the key. */

    static void removeMedium(OSDictionary *          dict,
                             const IONetworkMedium * medium);

/*! @function getMedium
    @abstract Find a medium object from a dictionary.
    @discussion Iterate through a dictionary and return an IONetworkMedium
    entry that satisfies the matching criteria. Returns 0 if there is
    no match. Also see getMediumWithType() and getMediumWithIndex() that
    are specialized forms derived from this method.
    @param dict The dictionary to look for a match.
    @param match An IOMediumDescriptor structure containing the matching
    fields.
    @param mask An IOMediumDescriptor structure containing the matching
    mask. Bits set in the mask are used for matching, while bits cleared
    are considered to be don't care bits.
    @result The first matching IONetworkMedium entry found,
    or 0 if no match was found. */

    static IONetworkMedium * getMedium(const OSDictionary * dict,
                                       IOMediumDescriptor * match,
                                       IOMediumDescriptor * mask);

/*! @function getMediumWithType
    @abstract Find a medium object from a dictionary with a given type.
    @discussion Iterate through a dictionary and return an IONetworkMedium
    entry with the given type. An optional mask supplies the don't care bits.
    See getMedium().
    @param dict The dictionary to look for a match.
    @param type Search for an entry with the given type.
    @param mask The don't care bits in IOMediumType. Defaults to 0, which
    implies a perfect match is desired.
    @result The first matching IONetworkMedium entry found,
    or 0 if no match was found. */

    static IONetworkMedium * getMediumWithType(const OSDictionary * dict,
                                               IOMediumType         type,
                                               IOMediumType         mask = 0);

/*! @function getMediumWithIndex
    @abstract Find a medium object from a dictionary with a given index.
    @discussion Iterate through a dictionary and return an IONetworkMedium
    entry with the given index. A optional mask supplies the don't care bits.
    See getMedium().
    @param dict The dictionary to look for a match.
    @param index Search for an entry with the given index.
    @param mask The don't care bits in index. Defaults to 0, which
    implies a perfect match is desired.
    @result The first matching IONetworkMedium entry found,
    or 0 if no match was found. */

    static IONetworkMedium * getMediumWithIndex(const OSDictionary * dict,
                                                UInt32               index,
                                                UInt32               mask = 0);

/*! @function init
    @abstract Initialize an IONetworkMedium instance.
    @param type The medium type, the fields are encoded with bits defined in
           IONetworkMedium.h.
    @param speed The maximum (or the only) link speed supported over this 
           medium in units of bits per second.
    @param flags An optional flag for the medium object.
           See IONetworkMedium.h for defined flags.
    @param index An optional 32-bit index assigned by the caller.
           Drivers can use this to store an index or a pointer to a media table
           inside the driver, or it may map to a driver defined media type.
    @param name An optional name assigned to this medium object. If 0,
           then a name will be created based on the medium type given
           using nameForType().
    @result true on success, false otherwise. */

    virtual bool init(IOMediumType  type,
                      UInt64        speed,
                      UInt32        flags = 0,
                      UInt32        index = 0,
                      const char *  name  = 0);

/*! @function medium
    @abstract Factory method which performs allocation and initialization
    of an IONetworkMedium instance.
    @param type See init().
    @param speed See init().
    @param flags See init().
    @param index See init().
    @param name See init().
    @result An IONetworkMedium instance on success, or 0 otherwise. */

    static IONetworkMedium * medium(IOMediumType  type,
                                    UInt64        speed,
                                    UInt32        flags = 0,
                                    UInt32        index = 0,
                                    const char *  name  = 0);

/*! @function getType
    @result The assigned medium type. */

    virtual IOMediumType  getType() const;

/*! @function getSpeed
    @result The maximum medium speed. */

    virtual UInt64        getSpeed() const;

/*! @function getFlags
    @result The medium flags. */

    virtual UInt32        getFlags() const;

/*! @function getIndex
    @result The assigned index. */

    virtual UInt32        getIndex() const;

/*! @function getDescriptor
    @param descP The IOMediumDescriptor structure associated with the
    instance is copied to the address provided. */

    virtual void          getDescriptor(IOMediumDescriptor * descP) const;

/*! @function getName
    @result The name for this instance. */

    virtual const OSSymbol * getName() const;

/*! @function getKey
    @result The key for this instance. Same as getName(). */

    virtual const OSSymbol * getKey() const;

/*! @function isEqualTo
    @abstract Test for equality between two IONetworkMedium objects.
    @discussion Two IONetworkMedium objects are considered equal if
    they have similar properties assigned to them during initialization.
    @param medium An IONetworkMedium to test against the IONetworkMedium
    object being called.
    @result true if equal, false otherwise. */

    virtual bool isEqualTo(const IONetworkMedium * medium) const;

/*! @function isEqualTo
    @abstract Test for equality between a IONetworkMedium object and an
    OSObject.
    @discussion The OSObject is considered equal to the IONetworkMedium
    object if the OSObject is an IONetworkMedium, and they have
    similar properties assigned to them during initialization.
    @param obj An OSObject to test against the IONetworkMedium object
    being called.
    @result true if equal, false otherwise. */

    virtual bool isEqualTo(const OSObject * obj) const;

/*! @function serialize
    @abstract Serialize the IONetworkMedium object.
    @discussion An OSData is created containing an IOMediumDescriptor
    structure (not copied), and this OSData is serialized.
    @param s An OSSerialize object.
    @result true on success, false otherwise. */

    virtual bool serialize(OSSerialize * s) const;
};

// Translate getKey() to getName().
//
inline const OSSymbol * IONetworkMedium::getKey() const
{
    return getName();
}

#endif /* KERNEL */

#endif /* !_IONETWORKMEDIUM_H */
