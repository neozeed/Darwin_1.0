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
/* IOSymbol.h created by gvdl on Fri 1998-10-30 */
/* IOSymbol must be created through the factory methods and thus is not subclassable. */

#ifndef _OS_OSSYMBOL_H
#define _OS_OSSYMBOL_H

#include <libkern/c++/OSString.h>

/*!
    @class OSSymbol
    @abstract A container class whose instances represent unique string values.
    @discussion
    An OSSymbol object represents a unique string value.  When creating an OSSymbol, a string is given and an OSSymbol representing this string is created if none exist for this string.  If a symbol for this string already exists, then a reference to an existing symbol is returned.
*/
class OSSymbol : public OSString
{
    friend class OSSymbolPool;

    OSDeclareAbstractStructors(OSSymbol)

private:
    static void initialize();

    static void checkForPageUnload(void *startAddr, void *endAddr);
    friend void checkModuleForSymbols(void); /* in catalogue? */

    // The string init methods have to be removed from the inheritance.
    virtual bool initWithString(const OSString *aString);
    virtual bool initWithCString(const char *cString);
    virtual bool initWithCStringNoCopy(const char *cString);

protected:
    /*!
        @function free
        @abstract A member function to release all resources created or used by the OSString object.
        @discussion This function should not be called directly, use release() instead.
    */
    virtual void free();

public:
    /*!
        @function withString
        @abstract A static constructor function to create an OSSymbol instance from an OSString object or returns an existing OSSymbol object based on the OSString object given.
        @param aString An OSString object.
        @result Returns a unique OSSymbol object for the string given.
    */
    static const OSSymbol *withString(const OSString *aString);
    /*!
        @function withCString
        @abstract A static constructor function to create an OSSymbol instance from a simple c-string returns an existing OSSymbol object based on the string object given.
        @param cString A c-string.
        @result Returns a unique OSSymbol object for the string given.
    */
    static const OSSymbol *withCString(const char *cString);
    /*!
        @function withCStringNoCopy
        @abstract A static constructor function to create an OSSymbol instance from a simple c-string, but does not copy the string to the container.
        @param cString A c-string.
        @result Returns a unique OSSymbol object for the string given.
    */
    static const OSSymbol *withCStringNoCopy(const char *cString);

    /*!
        @function isEqualTo
        @abstract A member function which tests the equality between two OSSymbol objects.  Two OSSymbol objects are only equivalent when their references are identical
        @param aSymbol The OSSymbol object to be compared against the receiver.
        @result Returns true if the two objects are equivalent, false otherwise.
    */
    virtual bool isEqualTo(const OSSymbol *aSymbol) const;
    /*!
        @function isEqualTo
        @abstract A member function which tests the equality between an OSSymbol object and a simple c-string.
        @param aCString The c-string to be compared against the receiver.
        @result Returns true if the OSSymbol's internal string representation is equivalent to the c-string it is being compared against, false otherwise.
    */
    virtual bool isEqualTo(const char *aCString) const;
    /*!
        @function isEqualTo
        @abstract A member function which tests the equality between an OSSymbol object and and arbitrary OSObject derived object.
        @param obj The OSObject derived object to be compared against the receiver.
        @result Returns true if the OSSymbol and the OSObject objects are equivalent.
    */
    virtual bool isEqualTo(const OSObject *obj) const;
};

#endif /* !_OS_OSSYMBOL_H */
