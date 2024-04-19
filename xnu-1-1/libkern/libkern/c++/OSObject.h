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
/*
Copyright (c) 1998 Apple Computer, Inc.	 All rights reserved.
HISTORY
    1998-10-30	Godfrey van der Linden(gvdl)
	Created
*/
#ifndef _LIBKERN_OSOBJECT_H
#define _LIBKERN_OSOBJECT_H

#include <sys/types.h>

class OSMetaClass;
class OSSymbol;
class OSString;
class OSSerialize;

/*!
    @class OSObject
    @abstract The root base class for Mac OS X kernel and just generally all-round useful class to have around.
    @discussion
Defines the minimum functionality that an object can expect.  Implements reference counting, type safe object casting, allocation primitives & serialisation among other functionality.	 This object is an abstract base class and can not be copied, nor can it be constructed by itself.

<br><br> Construction <br><br>

As Mac OS X's C++ is based upon Embedded C++ we have a problem with the typical C++ method of using constructors.  Embedded C++ does not allow exceptions.  This means that the standard constructors can not report a failure.  Well obviously initialisation of a new object can fail so we have had to work around this language limitation.  In the Mac OS X kernel we have chosen to break object construction into two phases.  Phase one is the familiar C++ new operator, the only initialisation is the object has exactly one reference after creation.  Once the new is called the client MUST call init and check it's return value.  If the init call fails then the object MUST be immediately released.  IOKit usually implements factory methods to make construction a one step process for clients.  

<br><br>Reference Counting<br><br>

OSObject provides reference counting services using the $link retain(), $link release(), $link release(int when) and $link free() functions.  The public interface to the reference counting is retain() & release().  release() is implemented as a simple call to release(1).  The actual implementation of release(when) is a little subtle.  If the current reference count is less than or equal to the 'when' parameter the object will call free on itself.  
<br>
In general a subclass is expected to only override $link free().  It may also choose to override release() if the object has a circular retain count, see $link release(int when);

<br><br>Runtime Type Information System<br><br>

The Mac OS X C++ implements a basic runtime type information system using meta class information and a number of macros, $link OSDynamicCast, $link OSTypeID, $link OSTypeIDInst, $link OSCheckTypeInst and $link OSMetaClass.
*/
class OSObject
{
    // Explicitly expand rather than using a macro avoids a header circularity.
    // OSDeclareAbstractStructors(OSObject).
    friend class OSObjectMetaClass;
public:
    static const OSMetaClass * const metaClass;
    virtual const OSMetaClass *getMetaClass() const;
protected:
    OSObject(const OSMetaClass *meta);
    virtual ~OSObject();
private:
    OSObject();
protected:

    // Start of non-infrastructure part of OSObject declaration
    friend class OSMetaClass;
public:

/*! @function OSTypeID
    @abstract Given the name of a class return it's typeID
    @param type Name of the desired type, eg. OSObject.
    @result 'this' if object is of desired type, otherwise 0.
*/
#define OSTypeID(type)	(type::metaClass)

/*! @function OSTypeIDInst
    @abstract Given a pointer to an object return it's typeID
    @param typeinst An instance of an OSObject subclass.
    @result The typeID, ie. OSMetaClass *.
*/
#define OSTypeIDInst(typeinst)	((typeinst)->getMetaClass())

/*! @function OSDynamicCast
    @abstract Roughly analagous to (type *) inst, but check if valid first.
    @discussion OSDynamicCast is an attempt to implement a rudimentary equivalent to rtti's dynamic_cast<T> operator.  Embedded-C++ doesn't allow the use of rtti.  OSDynamicCast is build on the OSMetaClass mechanism.  Note it is safe to call this with a 0 paramter.  
    @param type name of desired class name.  Notice that it is assumed that you desire to cast to a pointer to an object of this type.	Also type qualifiers, like const, are not recognised and will cause an, usually obscure, compile error.
    @param inst Pointer to object that you wish to attempt to type cast.  May be 0.
    @result inst if object non-zero and it is of the desired type, otherwise 0.
*/
#define OSDynamicCast(type, inst)	\
    ((type *) OSObject::safeMetaCast((inst), OSTypeID(type)))

/*! @function OSCheckTypeInst
    @abstract Is the target object a subclass of the reference object?
    @param typeinst Reference instance of an object, desired type.
    @param inst Instance of object to check for type compatibility.
    @result false if typeinst or inst are 0 or inst is not a subclass of typeinst's class. true otherwise.
*/
#define OSCheckTypeInst(typeinst, inst) \
    OSObject::checkTypeInst(inst, typeinst)
    
private:
/*! @var retainCount Number of references held on this instance. */
    int retainCount;

    // Disable copy constructors of OSObject based objects
/*! @function operator =
    @abstract Disable implicit copy constructor by making private
    @param src Reference to source object that isn't allowed to be copied
*/
    void operator =(OSObject &src);

/*! @function OSObject
    @abstract Disable implicit copy constructor by making private
    @param src Reference to source object that isn't allowed to be copied
*/
    OSObject(OSObject &src);

protected:

/*! @function release
    @abstract Primary implementation of the release mechanism.
    @discussion  If $link retainCount <= the when argument then call $link free().  This indirect implementation of $link release allows the developer to break reference circularity.  An example of this sort of problem is a parent/child mutual reference, either the parent or child can implement: void release() { release(2); } thus breaking the cirularity. 
    @param when When retainCount == when then call free(). */
    void release(int when) const;

/*! @function init
    @abstract Mac OS X kernel's primary mechanism for constructing objects.
    @discussion Your responsibility as a subclass author is to override the init method of your parent.  In general most of our implementations call <super>::init() before doing local initialisation, if the parent fails then return false immediately.  If you have a failure during you local initialisation then return false.
    @result OSObject::init Always returns true, but subclasses will return false on init failure.
*/
    virtual bool init();

/*! @function free
    @abstract The last reference is gone so clean up your resources.
    @discussion Release all resources held by the object, then call your parent's free().  

<br><br>Caution:
<br>1> You can not assume that you have completed initialisation before your free is called, so be very careful in your implementation.  
<br>2> The implementation is OSObject::free() { delete this; } so do not call super::free() until just before you return.
<br>3> Free is not allowed to fail all resource must be released on completion. */
    virtual void free();

/*! @function operator delete
    @abstract Release the 'operator new'ed memory.
    @discussion Never attempt to delete an object that inherits from OSObject directly use $link release().
    @param mem pointer to block of memory
    @param size size of block of memory
*/
    static void operator delete(void *mem, size_t size);

public:

/*! @function operator new
    @abstract Allocator for all objects that inherit from OSObject
    @param size number of bytes to allocate
    @result returns pointer to block of memory if avaialable, 0 otherwise.
*/
    static void *operator new(size_t size);

/*! @function getRetainCount
    @abstract How many times has this object been retained?
    @result Current retain count
*/
    virtual int getRetainCount() const;

/*! @function retain
    @abstract Retain a reference in this object.
*/
    virtual void retain() const;

/*! @function release
    @abstract Release a reference to this object
*/
    virtual void release() const;

/*! @function isEqualTo
    @abstract Is this == anObj?
    @discussion OSObject::isEqualTo implements this as a shallow pointer comparison.  The OS container classes do a more meaningful comparison.  Your mileage may vary.
    @param anObj Object to compare 'this' to.
    @result true if the objects are equivalent, false otherwise.
*/
    virtual bool isEqualTo(const OSObject *anObj) const;

/*! @function serialize
    @abstract 
    @discussion 
    @param s
    @result 
*/
    virtual bool serialize(OSSerialize *s) const;


/*! @function metaCast
    @abstract Check to see if this object is or inherits from the given type.
    @discussion This function is the guts of the OSMetaClass system.  IODynamicCast, qv, is implemented using this function.
    @param toMeta Pointer to a constant OSMetaClass for the desired target type.
    @result 'this' if object is of desired type, otherwise 0.
*/
    OSObject *metaCast(const OSMetaClass *toMeta) const;


/*! @function metaCast
    @abstract See OSObject::metaCast(const OSMetaClass *)
    @param toMeta OSSymbol of the desired class' name.
    @result 'this' if object is of desired type, otherwise 0.
*/
    OSObject *metaCast(const OSSymbol *toMeta) const;

/*! @function metaCast
    @abstract See OSObject::metaCast(const OSMetaClass *)
    @param toMeta OSString of the desired class' name.
    @result 'this' if object is of desired type, otherwise 0.
*/
    OSObject *metaCast(const OSString *toMeta) const;

/*! @function metaCast
    @abstract See OSObject::metaCast(const OSMetaClass *)
    @param toMeta const char * C String of the desired class' name.
    @result 'this' if object is of desired type, otherwise 0.
*/
    OSObject *metaCast(const char *toMeta) const;

    // Helper inlines for runtime type preprocessor macros
    static OSObject *
    safeMetaCast(const OSObject *me, const OSMetaClass *toType)
	{ return (me)? me->metaCast(toType) : 0; }

    static bool
    checkTypeInst(const OSObject *inst, const OSObject *typeinst)
    {
	const OSMetaClass *toType = OSTypeIDInst(typeinst);
	return typeinst && inst && (0 != inst->metaCast(toType));
    }
};

#include <libkern/c++/OSMetaClass.h>

#endif /* !_LIBKERN_OSOBJECT_H */
