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
#ifndef _LIBKERN_OSMETACLASS_H
#define _LIBKERN_OSMETACLASS_H

#include <libkern/OSReturn.h>
#include <libkern/c++/OSObject.h>

class OSString;
class OSSymbol;
class OSDictionary;

/*!
    @class OSMetaClass : OSObject
    @abstract An instance of a OSMetaClass represents one class then the kernel's runtime type information system is aware of.
*/
class OSMetaClass : public OSObject
{

private:
/*! @var superClass Handle to the superclass' meta class. */
    OSMetaClass *superClass;

/*! @var className OSSymbol of the class' name. */
    const OSSymbol *className;

/*! @var classSize How big is a single instancde of this class. */
    unsigned int classSize;

/*! @var instanceCount Roughly number of instances of the object.  Used primarily as a code in use flag. */
    mutable unsigned int instanceCount;


/*! @function OSMetaClass
    @abstract Private the default constructor */
    OSMetaClass();

    // Called by postModLoad
/*! @function logError
    @abstract Given an error code log an error string using printf */
    static void logError(OSReturn result);

protected:
/*! @function free
    @abstract Don't allow OSObject::free to be called
    @discussion The allocation strategies implemented by OSObject don't work for OSMetaClasses, as OSObjects assume that all object are allocated using new and thus they must all be removed using delete, or in fact free.  As OSMetaClass is always a statically defined object, see OSDefineMetaClass, it isn't appropriate for any attempt to be made to implement the default reference counting and free mechanism.. */
    virtual void free();

/*! @function retain
    @abstract Don't allow OSObject::retain to be called, see free */
    virtual void retain();

/*! @function release
    @abstract Don't allow OSObject::release to be called, see free */
    virtual void release();

/*! @function OSMetaClass
    @abstract Constructor for OSMetaClass objects
    @discussion This constructor is protected and cannot not be used to instantiate an OSMetaClass object, i.e. OSMetaClass is an abstract class.  This function stores the currently constructing OSMetaClass instance away for later processing.  See preModLoad and postModLoad.
    @param inClassName cString of the name of the class this meta-class represents.
    @param inSuperClassName cString of the name of the super class.
    @param inClassSize sizeof the class. */
    OSMetaClass(const char *inClassName,
		const char *inSuperClassName,
		unsigned int inClassSize);

/*! @function ~OSMetaClass
    @abstract Destructor for OSMetaClass objects
    @discussion If this function is called it means that the object code that implemented this class is actually in the process of unloading.  The destructor removes all reference's to the subclass from the runtime type information system. */
    virtual ~OSMetaClass();

public:

/*! @function preModLoad
    @abstract Prepare the runtime type system for the load of a module.
    @discussion Prepare the runtime type information system for the loading of new all meta-classes constructed between now and the next postModLoad.  preModLoad grab's a lock so that the runtime type information system loading can be protected, the lock is released by the postModLoad function.  Any OSMetaClass that is constructed between the bracketing pre and post calls will be assosiated with the module name.
    @param kmodName globally unique cString name of the kernel module being loaded. 
    @result If success full return a handle to be used in later calls 0 otherwise. */
    static void *preModLoad(const char *kmodName);

/*! @function failModLoad
    @abstract Record an error during the loading of an kernel module.
    @discussion As constructor's can't return errors nor can they through exceptions in embedded-c++ an indirect error mechanism is necessary.  Check mod load returns a bool to indicate the current error state of the runtime type information system.  During object construction a call to failModLoad will cause an error code to be recorded.  Once an error has been set the continuing construction will be ignored until the end of the pre/post load.
    @param error Code of the error. */
    static void failModLoad(OSReturn error);

/*! @function checkModLoad
    @abstract Check if the current load attempt is still OK.
    @param loadHandle Handle returned when a successful call to preModLoad is made.
    @result true if no error's are outstanding and the system is primed to recieve more objects. */
    static bool checkModLoad(void *loadHandle);

/*! @function postModLoad
    @abstract Finish postprocessing on a kernel module's meta-classes.
    @discussion As the order of static object construction is undefined it is necessary to process the constructors in two phases.  These phases rely on global information that is created be the preparation step, preModLoad, which also guarantees single threading between multiple modules.  Phase one was the static construction of each meta-class object one by one withing the context prepared by the preModLoad call.  postModLoad is the second phase of processing.  Inserts links all of the super class inheritance chains up, inserts the meta-classes into the global register of classes and records for each meta-class which kernel module caused it's construction.  Finally it cleans up the temporary storage and releases the single threading lock and returns whatever error has been recorded in during the construction phase or the post processing phase. 
    @param loadHandle Handle returned when a successful call to preModLoad is made.
    @result Error code of the first error encountered. */
    static OSReturn postModLoad(void *loadHandle);

/*! @function modHasInstance
    @abstract Do any of the objects represented by OSMetaClass and associated with the given kernel module name have instances?
    @discussion Check all meta-classes associated with the module name and check their instance counts.  This function is used to check to see if a module can be unloaded.  Obviously if an instance is still outstanding it isn't safe to unload the code that relies on that object.
    @param kmodName cString of the kernel module name.
    @result true if there are any current instances of any class in the module.
*/
    static bool modHasInstance(const char *kmodName);

/*! @function reportModInstances
    @abstract Log any object that has instances in a module.
    @discussion When a developer ask for a module to be unloaded but the unload fails due to outstanding instances.  This function will report which classes still have instances.  It is intended mostly for developers to find problems with unloading classes and will be called automatically by 'verbose' unloads.
    @param kmodName cString of the kernel module name. */
    static void reportModInstances(const char *kmodName);

/*! @function getMetaClassWithName
    @abstract Lookup a meta-class in the runtime type information system
    @param name Name of the desired class's meta-class. 
    @result pointer to a meta-class object if found, 0 otherwise. */
    static const OSMetaClass *getMetaClassWithName(const OSSymbol *name);

/*! @function getMetaClassWithName
    @abstract Lookup a meta-class in the runtime type information system
    @param name Name of the desired class's meta-class. 
    @result pointer to a meta-class object if found, 0 otherwise. */
    static const OSMetaClass *getMetaClassWithName(const OSString *name);

/*! @function getMetaClassWithName
    @abstract Lookup a meta-class in the runtime type information system
    @param name Name of the desired class's meta-class. 
    @result pointer to a meta-class object if found, 0 otherwise. */
    static const OSMetaClass *getMetaClassWithName(const char *name);

/*! @function allocClassWithName
    @abstract Lookup a meta-class in the runtime type information system and return the results of an alloc call.
    @param name Name of the desired class. 
    @result pointer to an new object, 0 if not found or so memory. */
    static OSObject *allocClassWithName(const OSSymbol *name);

/*! @function allocClassWithName
    @abstract Lookup a meta-class in the runtime type information system and return the results of an alloc call.
    @param name Name of the desired class. 
    @result pointer to an new object, 0 if not found or so memory. */
    static OSObject *allocClassWithName(const OSString *name);

/*! @function allocClassWithName
    @abstract Lookup a meta-class in the runtime type information system and return the results of an alloc call.
    @param name Name of the desired class. 
    @result pointer to an new object, 0 if not found or so memory. */
    static OSObject *allocClassWithName(const char *name);

/*! @function checkMetaCastWithName
    @abstract Introspect an objects inheritance tree looking for a class of the given name.  Basis of MacOSX's kernel dynamic casting mechanism.
    @param name Name of the desired class or super class. 
    @param in object to be introspected. 
    @result in parameter if cast valid, 0 otherwise. */
    static OSObject *
	checkMetaCastWithName(const OSSymbol *name, const OSObject *in);

/*! @function checkMetaCastWithName
    @abstract Introspect an objects inheritance tree looking for a class of the given name.  Basis of MacOSX's kernel dynamic casting mechanism.
    @param name Name of the desired class or super class.
    @param in object to be introspected.
    @result in parameter if cast valid, 0 otherwise. */
    static OSObject *
	checkMetaCastWithName(const OSString *name, const OSObject *in);

/*! @function checkMetaCastWithName
    @abstract Introspect an objects inheritance tree looking for a class of the given name.  Basis of MacOSX's kernel dynamic casting mechanism.
    @param name Name of the desired class or super class.
    @param in object to be introspected.
    @result in parameter if cast valid, 0 otherwise. */
    static OSObject *
	checkMetaCastWithName(const char *name, const OSObject *in);


/*! @function instanceConstructed
    @abstract Counts the instances of the class behind this metaclass.
    @discussion Every non-abstract class that inherits from OSObject has a default constructor that calls it's own meta-class' instanceConstructed function.  This constructor is defined by the OSDefineMetaClassAndStructors macro (qv) that all OSObject subclasses must use.  Also if the instance count goes from 0 to 1, ie the first instance, then increment the instance count of the super class */
    void instanceConstructed() const;

/*! @function instanceDestructed
    @abstract Removes one instance of the class behind this metaclass.
    @discussion OSObject's free function calls this method just before it does a 'delete this' on itself.  If the instance count transitions from 1 to 0, i.e. the last object, then one instance of the superclasses is also removed. */
    void instanceDestructed() const;


/*! @function checkMetaCast
    @abstract Ask a OSMetaClass instance if the given object is either an instance of it or an instance of a subclass of it.
    @param check Pointer of object to introspect.
    @result check parameter if cast valid, 0 otherwise. */
    OSObject *checkMetaCast(const OSObject *check) const;


/*! @function getInstanceCount
    @abstract How many instances of the class have been created.
    @result Count of the number of instances. */
    unsigned int getInstanceCount() const;


/*! @function getSuperClass
    @abstract 'Get'ter for the super class.
    @result Pointer to superclass, chain ends with 0 for OSObject. */
    const OSMetaClass *getSuperClass() const;

/*! @function getClassName
    @abstract 'Get'ter for class name.
    @result cString of the class name. */
    const char *getClassName() const;

/*! @function getClassSize
    @abstract 'Get'ter for sizeof(class).
    @result sizeof of class that this OSMetaClass instance represents. */
    unsigned int getClassSize() const;


/*! @function alloc
    @abstract Allocate an instance of the class that this OSMetaClass instance represents.
    @discussion This alloc function is analogous to the old ObjC class alloc method.  Typically not used by clients as the static function allocClassWithName is more generally useful.  Infact that function is implemented in terms of this  virtual function.  All subclass's of OSMetaClass must implement this function but that is what the OSDefineMetaClassAndStructor's families of macros does for the developer automatically. 
    @result Pointer to a new object with a retain count of 1. */
    virtual OSObject *alloc() const = 0;

/*! @function OSDeclareCommonStructors
    @abstract Basic helper macro for the OSDeclare for Default and Abstract macros, qv.  DO NOT USE.
    @param className Name of class. NO QUOTES. */
#define OSDeclareCommonStructors(className)				\
	friend class className ## MetaClass;				\
    public:								\
	static const OSMetaClass * const metaClass;			\
	virtual const OSMetaClass *getMetaClass() const;		\
    protected:								\
	className ## (const OSMetaClass *);				\
	virtual ~ ## className ## ()


/*! @function OSDeclareDefaultStructors
    @abstract One of the macro's used in the class declaration of all subclasses of OSObject, declares runtime type information data and interfaces. 
    @discussion Macro used in the class declaration all subclasses of OSObject, declares runtime type information data and interfaces.  By convention it should be 'called' immediately after the opening brace in a class declaration.  It leaves the current privacy state as 'protected:'.
    @param className Name of class. NO QUOTES. */
#define OSDeclareDefaultStructors(className)				\
	OSDeclareCommonStructors(className);				\
    public:								\
	className ## ();						\
    protected:


/*! @function OSDeclareAbstractStructors
    @abstract One of the macro's used in the class declaration of all subclasses of OSObject, declares runtime type information data and interfaces. 
    @discussion This macro is used when the class being declared has one or more '= 0' pure virtual methods and thus it is illegal to create an instance of this class.  It leaves the current privacy state as 'protected:'.
    @param className Name of class. NO QUOTES. */
#define OSDeclareAbstractStructors(className)				\
	OSDeclareCommonStructors(className);				\
    private:								\
	className ## (); /* Make primary constructor private in abstract */ \
    protected:

/*! @function OSDefineMetaClassWithInit
    @abstract Basic helper macro for the OSDefineMetaClass for the default and Abstract macros, qv.  DO NOT USE.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS.
    @param init Name of a function to call after the OSMetaClass is constructed. */
#define OSDefineMetaClassWithInit(className, superClassName, init)	\
    class className ## MetaClass : public OSMetaClass {			\
    public:								\
	className ## MetaClass ## ()					\
	    : OSMetaClass(#className, #superClassName, sizeof(className)) \
	    { init; }							\
	virtual OSObject *alloc() const;				\
    };									\
									\
    static className ## MetaClass s ## className ## MetaClass;		\
    /* Implement the common methods for class after defining metaclass */ \
    className ## :: ## className(const OSMetaClass *meta)		\
	    : superClassName ## (meta) { }				\
    className ## ::~ ## className() { }					\
    const OSMetaClass * const className ## ::metaClass =		\
	&s ## className ## MetaClass;					\
    const OSMetaClass * ## className ## ::getMetaClass() const		\
	{ return &s ## className ## MetaClass; }


/*! @function OSDefineAbstractStructors
    @abstract Basic helper macro for the OSDefineMetaClass for the default and Abstract macros, qv.  DO NOT USE.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS. */
#define OSDefineAbstractStructors(className, superClassName)		\
    OSObject * ## className ## MetaClass::alloc()  const { return 0; }

/*! @function OSDefineDefaultStructors
    @abstract Basic helper macro for the OSDefineMetaClass for the default and Abstract macros, qv.  DO NOT USE.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS. */
#define OSDefineDefaultStructors(className, superClassName)		\
    OSObject * ## className ## MetaClass::alloc() const			\
	{ return new className; }					\
    className ## :: ## className ## ()					\
	: superClassName ## (className ## ::metaClass)			\
	{ className ## ::metaClass->instanceConstructed(); }


/*! @function OSDefineMetaClassAndAbstractStructorsWithInit
    @abstract Primary definition macro for all abstract classes that a subclasses of OSObject.
    @discussion Define an OSMetaClass subclass and the primary constructors and destructors for a subclass of OSObject that is an abstract class.  In general this 'function' is 'called' at the top of the file just before the first function is implemented for a particular class.  Once the OSMetaClass has been constructed, at load time, call the init routine.  NB you can not rely on the order of execution of the init routines.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS.
    @param init Name of a function to call after the OSMetaClass is constructed. */
#define OSDefineMetaClassAndAbstractStructorsWithInit(className, superClassName, init) \
    OSDefineMetaClassWithInit(className, superClassName, init)		\
    OSDefineAbstractStructors(className, superClassName)

/*! @function OSDefineMetaClassAndStructorsWithInit
    @abstract See OSDefineMetaClassAndStructors
    @discussion Define an OSMetaClass subclass and the primary constructors and destructors for a subclass of OSObject that isn't an abstract class.  In general this 'function' is 'called' at the top of the file just before the first function is implemented for a particular class.  Once the OSMetaClass has been constructed, at load time, call the init routine.  NB you can not rely on the order of execution of the init routines.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS.
    @param init Name of a function to call after the OSMetaClass is constructed. */
#define OSDefineMetaClassAndStructorsWithInit(className, superClassName, init) \
    OSDefineMetaClassWithInit(className, superClassName, init)		\
    OSDefineDefaultStructors(className, superClassName)

/* Helpers */
/*! @function OSDefineMetaClass
    @abstract Define an OSMetaClass instance, used for backward compatiblility only.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS. */
#define OSDefineMetaClass(className, superClassName)			\
    OSDefineMetaClassWithInit(className, superClassName, )

/*! @function OSDefineMetaClassAndStructors
    @abstract Define an OSMetaClass subclass and the runtime system routines.
    @discussion Define an OSMetaClass subclass and the primary constructors and destructors for a subclass of OSObject that isn't an abstract class.  In general this 'function' is 'called' at the top of the file just before the first function is implemented for a particular class.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS. */
#define OSDefineMetaClassAndStructors(className, superClassName)	\
    OSDefineMetaClassAndStructorsWithInit(className, superClassName, )

/*! @function OSDefineMetaClassAndAbstractStructors
    @abstract Define an OSMetaClass subclass and the runtime system routines.
    @discussion Define an OSMetaClass subclass and the primary constructors and destructors for a subclass of OSObject that is an abstract class.  In general this 'function' is 'called' at the top of the file just before the first function is implemented for a particular class.
    @param className Name of class. NO QUOTES and NO MACROS.
    @param superClassName Name of super class. NO QUOTES and NO MACROS. */
#define OSDefineMetaClassAndAbstractStructors(className, superClassName) \
    OSDefineMetaClassAndAbstractStructorsWithInit (className, superClassName, )

    // IOKit debug internal routines.
    static void printInstanceCounts();
    static OSDictionary *getClassDictionary();
    bool serialize(OSSerialize *s) const;
};

#endif /* !_LIBKERN_OSMETACLASS_H */


