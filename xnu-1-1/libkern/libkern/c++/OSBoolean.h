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
/* OSBoolean.cpp created by rsulack on Tue Oct 12 1999 */

#ifndef _OS_OSBOOLEAN_H
#define _OS_OSBOOLEAN_H

#include <libkern/c++/OSObject.h>

class OSString;

/*!
    @class OSBoolean
    @abstract Container class for boolean values.
*/
class OSBoolean : public OSObject
{
    OSDeclareDefaultStructors(OSBoolean)

protected:
    bool value;

public:
    /*!
        @function withBoolean
        @abstract A static constructor function to create and initialize an instance of OSBoolean.
        @param value A boolean value.
        @result Returns and instance of OSBoolean, or 0 if an error occurred.
    */
    static OSBoolean *withBoolean(bool value);

    /*!
        @function init
        @abstract A member function to initialize an instance of OSBoolean.
        @param value A boolean value.
        @result Returns true if initialization was successful, false otherwise.
    */
    virtual bool init(bool value);
    /*!
        @function free
        @abstract A member function to release all resources used by the OSBoolean instance.
        @discussion This function should not be called directly, use release() instead.
    */
    virtual void free();

    /*!
        @function isTrue
        @abstract A member function to test if the boolean object is true.
        @result Returns true if the OSBoolean object is true, false otherwise.
    */
    virtual bool isTrue() const;
    /*!
        @function isFalse
        @abstract A member function to test if the boolean object is false.
        @result Returns true if the OSBoolean object is false, false otherwise.
    */
    virtual bool isFalse() const;

    /*!
        @function isEqualTo
        @abstract A member function to test the equality of two OSBoolean objects.
        @param boolean An OSBoolean object to be compared against the receiver.
        @result Returns true if the two objects are equivalent.
    */
    virtual bool isEqualTo(const OSBoolean *boolean) const;
    /*!
        @function isEqualTo
        @abstract A member function to test the equality between an arbitrary OSObject derived object and an OSBoolean object.
        @param obj An OSObject derived object to be compared against the receiver.
        @result Returns true if the two objects are equivalent.
    */
    virtual bool isEqualTo(const OSObject *obj) const;

    /*!
        @function serialize
        @abstract A member function which archives the receiver.
        @param s The OSSerialize object.
        @result Returns true if serialization was successful, false if not.
    */
    virtual bool serialize(OSSerialize *s) const;
};

#endif /* !_OS_OSBOOLEAN_H */




