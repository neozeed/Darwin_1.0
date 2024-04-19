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
 * HISTORY
 */
 
#include <IOKit/system.h>

extern "C" {
#include <machine/machine_routines.h>
}

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOCPU.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IORangeAllocator.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/IOLib.h>
#include <IOKit/nvram/IONVRAMController.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOKitDebug.h>
#include <IOKit/IOWorkLoop.h>

#include <IOKit/assert.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClassAndStructors(IOPlatformExpert, IOService)

static IOPlatformExpert * gIOPlatform;

OSSymbol * gPlatformInterruptControllerName;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPlatformExpert::attach( IOService * provider )
{

    if( !super::attach( provider ))
	return( false);

    return( true);
}

bool IOPlatformExpert::start( IOService * provider )
{
    IORangeAllocator *	physicalRanges;
    
    if (!super::start(provider))
      return false;
    
    gPlatformInterruptControllerName = (OSSymbol *)OSSymbol::withCStringNoCopy("IOPlatformInterruptController");
    
    physicalRanges = IORangeAllocator::withRange(0xffffffff, 1, 16,
						 IORangeAllocator::kLocking);
    assert(physicalRanges);
    setProperty("Platform Memory Ranges", physicalRanges);
    
    setPlatform( this );
    gIOPlatform = this;
    
    PMInstantiatePowerDomains();
    
    return( configure(provider) );
}

bool IOPlatformExpert::configure( IOService * provider )
{
    OSSet *		topLevel;
    OSDictionary *	dict;
    IOService * 	nub;

    topLevel = OSDynamicCast( OSSet, getProperty("top-level"));

    if( topLevel) {
        while( (dict = OSDynamicCast( OSDictionary,
				topLevel->getAnyObject()))) {
            dict->retain();
            topLevel->removeObject( dict );
            nub = createNub( dict );
            if( 0 == nub)
                continue;
            dict->release();
            nub->attach( this );
            nub->registerService();
        }
    }

    return( true );
}

IOService * IOPlatformExpert::createNub( OSDictionary * from )
{
    IOService *		nub;

    nub = new IOPlatformDevice;
    if(nub) {
	if( !nub->init( from )) {
	    nub->release();
	    nub = 0;
	}
    }
    return( nub);
}

bool IOPlatformExpert::compareNubName( const IOService * nub,
				OSString * name, OSString ** matched = 0 ) const
{
    return( nub->IORegistryEntry::compareName( name, matched ));
}

IOReturn IOPlatformExpert::getNubResources( IOService * nub )
{
    return( kIOReturnSuccess );
}

int IOPlatformExpert::getEpoch(void)
{
  return _peEpoch;
}

int IOPlatformExpert::getFamily(void)
{
  return _peFamily;
}

void IOPlatformExpert::setEpoch(int peEpoch)
{
  _peEpoch = peEpoch;
}

void IOPlatformExpert::setFamily(int peFamily)
{
  _peFamily = peFamily;
}

bool IOPlatformExpert::getMachineName( char * /*name*/, int /*maxLength*/)
{
    return( false );
}

bool IOPlatformExpert::getModelName( char * /*name*/, int /*maxLength*/)
{
    return( false );
}

IORangeAllocator * IOPlatformExpert::getPhysicalRangeAllocator(void)
{
    return(OSDynamicCast(IORangeAllocator,
			getProperty("Platform Memory Ranges")));
}

int (*PE_halt_restart)(unsigned int type) = 0;

int IOPlatformExpert::haltRestart(unsigned int type)
{
  
  if (PE_halt_restart) return (*PE_halt_restart)(type);
  else return -1;
}

long IOPlatformExpert::getGMTTimeOfDay(void)
{
    return(0);
}

void IOPlatformExpert::setGMTTimeOfDay(long secs)
{
}


IOReturn IOPlatformExpert::getConsoleInfo( PE_Video * consoleInfo )
{
    return( PE_current_console( consoleInfo));
}

IOReturn IOPlatformExpert::setConsoleInfo( PE_Video * consoleInfo,
						unsigned int op)
{
    return( PE_initialize_console( consoleInfo, op ));
}

void IOPlatformExpert::getDefaultBusSpeeds(int *numSpeeds,
					   unsigned long **speedList)
{
  if (numSpeeds != 0) *numSpeeds = 0;
  if (speedList != 0) *speedList = 0;
}

IOReturn IOPlatformExpert::registerInterruptController(OSSymbol *name, IOInterruptController *interruptController)
{
  publishResource(name, interruptController);
  
  return kIOReturnSuccess;
}

IOInterruptController *IOPlatformExpert::lookUpInterruptController(OSSymbol *name)
{
  IOInterruptController *interruptController;
  IOService             *service;
  
  service = waitForService(resourceMatching(name));
  
  interruptController = OSDynamicCast(IOInterruptController, service->getProperty(name));  
  
  return interruptController;
}


void IOPlatformExpert::setCPUInterruptProperties(IOService *service)
{
  IOCPUInterruptController *controller;
  
  controller = OSDynamicCast(IOCPUInterruptController, waitForService(serviceMatching("IOCPUInterruptController")));
  if (controller) controller->setCPUInterruptProperties(service);
}

bool IOPlatformExpert::atInterruptLevel(void)
{
  return ml_at_interrupt_context();
}

bool IOPlatformExpert::platformAdjustService(IOService */*service*/)
{
  return true;
}


//*********************************************************************************
// PMLog
//
//*********************************************************************************

void IOPlatformExpert::PMLog(const char * who,unsigned long event,unsigned long param1, unsigned long param2)
{
    if( gIOKitDebug & kIOLogPower)
        kprintf("%s %02d %08x %08x\n",who,event,param1,param2);
}


//*********************************************************************************
// PMInstantiatePowerDomains
//
// In this vanilla implementation, a Root Power Domain is instantiated.
// All other objects which register will be children of this Root.
// Where this is inappropriate, PMInstantiatePowerDomains is overridden 
// in a platform-specific subclass.
//*********************************************************************************

void IOPlatformExpert::PMInstantiatePowerDomains ( void )
{
    root = new IOPMrootDomain;
    root->init();
    root->attach(this);
    root->start(this);
    root->youAreRoot();
}


//*********************************************************************************
// PMRegisterDevice
//
// In this vanilla implementation, all callers are made children of the root power domain.
// Where this is inappropriate, PMRegisterDevice is overridden in a platform-specific subclass.
//*********************************************************************************

void IOPlatformExpert::PMRegisterDevice(IOService * theNub, IOService * theDevice)
{
    root->addChild ( theDevice );
}


/*
 * Platform Expert handler for Non-volatile RAM.
   Called by BSD driver.
   Calls whatever NVRAM driver has registered.
   That's either the IOPMUNVRAMController or the IOMapNVRAMController.

 Suurballe 11 Feb 1999
 */


// global pointer to whichever NVRAM controller is present
IONVRAMController * gNVRAMController = 0;

extern "C" {

//int PEnvopen(dev_t dev, int flag, int devtype,struct proc *  pp)
int PEnvopen(dev_t, int, int,struct proc * )
{
  if (gNVRAMController) return gNVRAMController->openNVRAM();
  else return kNoNVRAM;
}


//int PEnvclose(dev_t dev, int flag, int mode, struct proc * pp)
int PEnvclose(dev_t, int, int, struct proc *)
{
  if (gNVRAMController) return gNVRAMController->closeNVRAM();
  else return kNoNVRAM;
}


int PEnvsync(void)
{
  if (gNVRAMController) return gNVRAMController->syncNVRAM();
  else return kNoNVRAM;
}


int PEnvread(long offset, int length, unsigned char * where)
{
    IOByteCount bytecount = length;
    
    if (gNVRAMController)
      return gNVRAMController->readNVRAM(offset, &bytecount, where);
    else return kNoNVRAM;
}



int PEnvwrite(long offset, int length, unsigned char * where)
{
    IOByteCount bytecount = length;
    
    if (gNVRAMController)
      return gNVRAMController->writeNVRAM(offset, &bytecount, where);
    else return kNoNVRAM;
}

/*
 * Callouts from BSD for machine name & model
 */ 

boolean_t PEGetMachineName( char * name, int maxLength )
{
    if( gIOPlatform)
	return( gIOPlatform->getMachineName( name, maxLength ));
    else
	return( false );
}

boolean_t PEGetModelName( char * name, int maxLength )
{
    if( gIOPlatform)
	return( gIOPlatform->getModelName( name, maxLength ));
    else
	return( false );
}

int PEGetPlatformEpoch(void)
{
    if( gIOPlatform)
	return( gIOPlatform->getEpoch());
    else
	return( -1 );
}

int PEHaltRestart(unsigned int type)
{
  if (gNVRAMController) gNVRAMController->syncNVRAM();
  
  if (gIOPlatform) return gIOPlatform->haltRestart(type);
  else return -1;
}

long PEGetGMTTimeOfDay(void)
{
    if( gIOPlatform)
	return( gIOPlatform->getGMTTimeOfDay());
    else
	return( 0 );
}

void PESetGMTTimeOfDay(long secs)
{
    if( gIOPlatform)
	gIOPlatform->setGMTTimeOfDay(secs);
}

} /* extern "C" */

void IOPlatformExpert::registerNVRAMController(IONVRAMController * caller)
{
    gNVRAMController = caller;

    publishResource("IONVRAM");
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOPlatformExpert

OSDefineMetaClass(IODTPlatformExpert, IOPlatformExpert)
OSDefineAbstractStructors( IODTPlatformExpert, IOPlatformExpert )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService * IODTPlatformExpert::probe( IOService * provider,
			       		SInt32 * score )
{
    if( !super::probe( provider, score))
	return( 0 );

    // check machine types
    if( !provider->compareNames( getProperty( gIONameMatchKey ) ))
        return( 0 );

    return( this);
}

bool IODTPlatformExpert::configure( IOService * provider )
{
    if( !super::configure( provider))
	return( false);

    processTopLevel( provider );

    return( true );
}

IOService * IODTPlatformExpert::createNub( IORegistryEntry * from )
{
    IOService *		nub;

    nub = new IOPlatformDevice;
    if( nub) {
	if( !nub->init( from, gIODTPlane )) {
	    nub->free();
	    nub = 0;
	}
    }
    return( nub);
}

bool IODTPlatformExpert::createNubs( IOService * parent, OSIterator * iter )
{
    IORegistryEntry *	next;
    IOService *		nub;
    bool		ok = true;

    if( iter) {
	while( (next = (IORegistryEntry *) iter->getNextObject())) {

            if( 0 == (nub = createNub( next )))
                continue;

            nub->attach( parent );
            nub->registerService();
        }
	iter->release();
    }

    return( ok );
}

void IODTPlatformExpert::processTopLevel( IORegistryEntry * root )
{
    OSIterator * 	kids;
    IORegistryEntry *	next;
    IORegistryEntry *	cpus;

    // infanticide
    kids = IODTFindMatchingEntries( root, 0, deleteList() );
    if( kids) {
	while( (next = (IORegistryEntry *)kids->getNextObject())) {
	    next->detachAll( gIODTPlane);
	}
	kids->release();
    }

    // publish top level, minus excludeList
    createNubs( this, IODTFindMatchingEntries( root, kIODTExclusive, excludeList()));
    
    cpus = root->childFromPath( "cpus", gIODTPlane);
//    kids = IODTFindMatchingEntries( root, 0, "cpus");
//    cpus = (IORegistryEntry *)kids->getNextObject();
    if ( cpus)
        // publish cpus/*
        createNubs( this, IODTFindMatchingEntries( cpus, kIODTExclusive, 0));

}

IOReturn IODTPlatformExpert::getNubResources( IOService * nub )
{
  if( nub->getDeviceMemory())
    return( kIOReturnSuccess );

  IODTResolveAddressing( nub, "reg", 0);

  return( kIOReturnSuccess);
}

bool IODTPlatformExpert::compareNubName( const IOService * nub,
				OSString * name, OSString ** matched ) const
{
    return( IODTCompareNubName( nub, name, matched )
	  || super::compareNubName( nub, name, matched) );
}

bool IODTPlatformExpert::getModelName( char * name, int maxLength )
{
    OSData *		prop;
    const char *	str;
    int			len;
    char		c;
    bool		ok = false;

    maxLength--;

    prop = (OSData *) getProvider()->getProperty( gIODTCompatibleKey );
    if( prop ) {
	str = (const char *) prop->getBytesNoCopy();

	if( 0 == strncmp( str, "AAPL,", strlen( "AAPL," ) ))
	    str += strlen( "AAPL," );

	len = 0;
	while( (c = *str++)) {
	    if( (c == '/') || (c == ' '))
		c = '-';

	    name[ len++ ] = c;
	    if( len >= maxLength)
		break;
	}

	name[ len ] = 0;
	ok = true;
    }
    return( ok );
}

bool IODTPlatformExpert::getMachineName( char * name, int maxLength )
{
    OSData *		prop;
    bool		ok = false;

    maxLength--;
    prop = (OSData *) getProvider()->getProperty( gIODTModelKey );
    ok = (0 != prop);

    if( ok )
	strncpy( name, (const char *) prop->getBytesNoCopy(), maxLength );

    return( ok );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    kMaxNVNameLen	= 4,
    kMaxNVDataLen	= 8
};

#pragma options align=mac68k
struct NVRAMProperty
{
    IONVRAMDescriptor	header;
    UInt8		nameLen;
    UInt8		name[ kMaxNVNameLen ];
    UInt8		dataLen;
    UInt8		data[ kMaxNVDataLen ];
};
#pragma options align=reset


void IODTPlatformExpert::registerNVRAMController( IONVRAMController * nvram )
{
    UInt32	off;
    IOReturn	err;
    IOByteCount len;

    err = getNVRAMPartitionOffset( kIONameRegistryNVRAMPartition, &off );

    if( kIOReturnSuccess == err) {

        if( 0 == nvramBuffer)
            nvramBuffer = IONew( UInt8, kIONameRegistryNVRAMPartitionSize );
        assert( nvramBuffer );

        err = nvram->openNVRAM();
        if( kIOReturnSuccess == err) {
            len = kIONameRegistryNVRAMPartitionSize;
            err = nvram->readNVRAM( off, &len, nvramBuffer );
            nvram->closeNVRAM();
        }

        if( err)
            bzero( nvramBuffer, kIONameRegistryNVRAMPartitionSize );
    }

    super::registerNVRAMController( nvram );
}

bool IODTPlatformExpert::searchNVRAMProperty( IONVRAMDescriptor * hdr,
		UInt32 * where )
{
    UInt32	off;
    SInt32	nvEnd;

    getNVRAMPartitionOffset( kIONameRegistryNVRAMPartition, &off );

    nvEnd = *((UInt16 *) nvramBuffer);
    if( getEpoch())
	nvEnd -= 0x100;		// on new world, offset to partition start
    else
        nvEnd -= off;		// on old world, absolute
    if( (nvEnd < 0) || (nvEnd >= kIONameRegistryNVRAMPartitionSize) )
	nvEnd = 2;

    off = 2;
    while( (off + sizeof( NVRAMProperty)) <= (UInt32) nvEnd) {

	if( 0 == bcmp( nvramBuffer + off, hdr, sizeof( *hdr))) {
	    *where = off;
	    return( true );
	}
	off += sizeof( NVRAMProperty);
    }

    if( (nvEnd + sizeof( NVRAMProperty)) <= kIONameRegistryNVRAMPartitionSize)
        *where = nvEnd;
    else
	*where = 0;

    return( false );
}

IOReturn IODTPlatformExpert::readNVRAMPropertyType0(
	IORegistryEntry * entry,
	const OSSymbol ** name, OSData ** value )
{
    IONVRAMDescriptor   hdr;
    NVRAMProperty *	prop;
    IOByteCount		len;
    UInt32		off;
    IOReturn		err;
    char		nameBuf[ kMaxNVNameLen + 1 ];

    if( !nvramBuffer)
	return( kIOReturnUnsupported );
    if( !entry || !name || !value)
	return( kIOReturnBadArgument );

    if( kIOReturnSuccess != (err = IODTMakeNVDescriptor( entry, &hdr)))
	return( err);

    if( searchNVRAMProperty( &hdr, &off)) {

	prop = (NVRAMProperty *) (nvramBuffer + off);

	len = prop->nameLen;
	if( len > kMaxNVNameLen)
	    len = kMaxNVNameLen;
	strncpy( nameBuf, (const char *) prop->name, len );
	nameBuf[ len ] = 0;
	*name = OSSymbol::withCString( nameBuf );

	len = prop->dataLen;
	if( len > kMaxNVDataLen)
	    len = kMaxNVDataLen;
	*value = OSData::withBytes( prop->data, len );

	if( *name && *value )
            return( kIOReturnSuccess );
	else
            return( kIOReturnNoMemory );
    }

    return( kIOReturnNoResources );
}

IOReturn IODTPlatformExpert::writeNVRAMPropertyType0(
	IORegistryEntry * entry,
	const OSSymbol * name, OSData * value )
{
    IONVRAMDescriptor   hdr;
    NVRAMProperty *	prop;
    IOByteCount		len;
    IOByteCount		nameLen;
    IOByteCount		dataLen;
    UInt32		off;
    IOReturn		err;
    UInt32		start;
    UInt16		nvLen;
    bool		exists;

    if( !nvramBuffer)
	return( kIOReturnUnsupported );
    if( !entry || !name || !value)
	return( kIOReturnBadArgument );

    nameLen = name->getLength();
    dataLen = value->getLength();
    if( nameLen > kMaxNVNameLen)
	return( kIOReturnNoSpace);
    if( dataLen > kMaxNVDataLen)
	return( kIOReturnNoSpace);

    if( kIOReturnSuccess != (err = IODTMakeNVDescriptor( entry, &hdr)))
	return( err);

    exists = searchNVRAMProperty( &hdr, &off);
    if( !off)
	return( kIOReturnNoMemory);

    prop = (NVRAMProperty *) (nvramBuffer + off);
    if( !exists)
	bcopy( &hdr, &prop->header, sizeof( hdr));

    prop->nameLen = nameLen;
    bcopy( name->getCStringNoCopy(), prop->name, nameLen );
    prop->dataLen = dataLen;
    bcopy( value->getBytesNoCopy(), prop->data, dataLen );

    getNVRAMPartitionOffset( kIONameRegistryNVRAMPartition, &start );
    if( !exists) {
	nvLen = off + sizeof( NVRAMProperty);
	if( getEpoch())
	    nvLen += 0x100;
	else
	    nvLen += start;
        *((UInt16 *) nvramBuffer) = nvLen;
    }

    err = gNVRAMController->openNVRAM();
    if( kIOReturnSuccess == err) {
	len = 2;
        if( !exists)
            err = gNVRAMController->writeNVRAM( start, &len, nvramBuffer );
        if( kIOReturnSuccess == err) {
	    len = sizeof( NVRAMProperty);
            err = gNVRAMController->writeNVRAM( start + off, &len,
						nvramBuffer + off );
	}
	gNVRAMController->closeNVRAM();
    }

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSData * IODTPlatformExpert::getType1NVRAM( void )
{
    OSData	    *	data;
    IORegistryEntry *	options;

    options = IORegistryEntry::fromPath( "/options", gIODTPlane);
    if( !options)
	return( 0 );

    data = OSDynamicCast( OSData, options->getProperty( "aapl,pci" ));
    if( data && (0 == data->getLength()))
	data = 0;

    return( data );
}

OSData * IODTPlatformExpert::unescapeValueToBinary( UInt8 * value )
{
    OSData *	data = 0;
    UInt8 *	buffer;
    UInt32	len, totalLen = 0;
    UInt8 	byte;
    enum {	kMaxValueSize = 256 };

    buffer = IONew( UInt8, kMaxValueSize );
    if( !buffer)
	return( 0 );

    while( 0 != (byte = *(value++))) {

        if( 0xff == byte) {
            byte = *(value++);
            len = byte & 0x7f;
            byte = (byte & 0x80) ? 0xff : 0;
        } else
            len = 1;

        if( (totalLen + len) >= kMaxValueSize) {
            totalLen = 0;
            break;
        }

        memset( (void *) (buffer + totalLen), byte, len );
        totalLen += len;
    }

    if( totalLen)
        data = OSData::withBytes( buffer, totalLen );

    IODelete( buffer, UInt8, kMaxValueSize );

    return( data );
}

IOReturn IODTPlatformExpert::readNVRAMPropertyType1(
	IORegistryEntry * entry,
	const OSSymbol ** name, OSData ** value )
{
    IOReturn		err = kIOReturnNoResources;
    OSData	*	data;
    UInt8	*	start;
    UInt8	*	end;
    UInt8	*	where;
    UInt8	*	nvPath = 0;
    UInt8	*	nvName = 0;
    UInt8 		byte;

    if( 0 == (data = getType1NVRAM()))
	return( kIOReturnUnsupported );

    // data must have length from getType1NVRAM()
    start = (UInt8 *) data->getBytesNoCopy();
    if( strlen( (const char *) start) == (data->getLength() - 1)) {
	data = unescapeValueToBinary( start );
        if( 0 == data)
	    return( kIOReturnUnsupported );
    } else
	data->retain();

    start = (UInt8 *) data->getBytesNoCopy();
    end = start + data->getLength();

    where = start;
    while( where < end) {

	byte = *(where++);
	if( byte)
	    continue;

	if( !nvPath)
	    nvPath = start;
	else if( !nvName)
	    nvName = start;

	else if( entry == IORegistryEntry::fromPath( (const char *) nvPath, gIODTPlane )) {

            *name = OSSymbol::withCString( (const char *) nvName );
	    *value = unescapeValueToBinary( start );
            if( *name && *value )
		err = kIOReturnSuccess;
           else
		err = kIOReturnNoMemory;
	    break;

	} else
	    nvPath = nvName = 0;

        start = where;
    }
    data->release();

    return( err );
}

IOReturn IODTPlatformExpert::writeNVRAMPropertyType1(
	IORegistryEntry * entry,
	const OSSymbol * name, OSData * value )
{
    OSData	*	data;

    if( 0 == (data = getType1NVRAM()))
	return( kIOReturnUnsupported );

    // tidbit left for PR2
    return( kIOReturnNoResources );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


IOReturn IODTPlatformExpert::readNVRAMProperty(
	IORegistryEntry * entry,
	const OSSymbol ** name, OSData ** value )
{
    IOReturn	err;

    err = readNVRAMPropertyType1( entry, name, value );

    if( kIOReturnUnsupported == err)
        err = readNVRAMPropertyType0( entry, name, value );

    return( err );
}

IOReturn IODTPlatformExpert::writeNVRAMProperty(
	IORegistryEntry * entry,
	const OSSymbol * name, OSData * value )
{
    IOReturn	err;

    err = writeNVRAMPropertyType1( entry, name, value );

    if( kIOReturnUnsupported == err)
        err = writeNVRAMPropertyType0( entry, name, value );

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndStructors(IOPlatformExpertDevice, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPlatformExpertDevice::compareName( OSString * name,
                                        OSString ** matched = 0 ) const
{
    return( IODTCompareNubName( this, name, matched ));
}

bool
IOPlatformExpertDevice::initWithArgs(
                            void * dtTop, void * p2, void * p3, void * p4 )
{
    IORegistryEntry * 	dt = 0;
    void *		argsData[ 4 ];
    bool		ok;

    // dtTop may be zero on non- device tree systems
    if( dtTop && (dt = IODeviceTreeAlloc( dtTop )))
	ok = super::init( dt, gIODTPlane );
    else
	ok = super::init();

    if( !ok)
	return( false);

    workLoop = IOWorkLoop::workLoop();
    if (!workLoop)
        return false;

    argsData[ 0 ] = dtTop;
    argsData[ 1 ] = p2;
    argsData[ 2 ] = p3;
    argsData[ 3 ] = p4;

    setProperty("IOPlatformArgs", (void *)argsData, sizeof( argsData));

    return( true);
}

IOWorkLoop *IOPlatformExpertDevice::getWorkLoop() const
{
    return workLoop;
}

void IOPlatformExpertDevice::free()
{
    if (workLoop)
        workLoop->release();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndStructors(IOPlatformDevice, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPlatformDevice::compareName( OSString * name,
					OSString ** matched = 0 ) const
{
    return( ((IOPlatformExpert *)getProvider())->
		compareNubName( this, name, matched ));
}

IOService * IOPlatformDevice::matchLocation( IOService * /* client */ )
{
    return( this );
}

IOReturn IOPlatformDevice::getResources( void )
{
    return( ((IOPlatformExpert *)getProvider())->getNubResources( this ));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
