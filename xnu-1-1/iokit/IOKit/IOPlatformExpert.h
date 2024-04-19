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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_IOPLATFORMEXPERT_H
#define _IOKIT_IOPLATFORMEXPERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pexpert/pexpert.h>

extern boolean_t PEGetMachineName( char * name, int maxLength );
extern boolean_t PEGetModelName( char * name, int maxLength );
extern int PEGetPlatformEpoch( void );

enum {
  kPEHaltCPU,
  kPERestartCPU
};
extern int (*PE_halt_restart)(unsigned int type);
extern int PEHaltRestart(unsigned int type);

extern long PEGetGMTTimeOfDay( void );
extern void PESetGMTTimeOfDay( long secs );
  
#ifdef __cplusplus
} /* extern "C" */

#include <IOKit/IOTypes.h>
#include <IOKit/IOService.h>
#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

extern OSSymbol * gPlatformInterruptControllerName;

class IORangeAllocator;
class IONVRAMController;
class IOPMrootDomain;

class IOPlatformExpert : public IOService
{
    OSDeclareDefaultStructors(IOPlatformExpert);

private:
    int _peEpoch;
    int _peFamily;

protected:
    IOPMrootDomain * root;
        
    virtual void setEpoch(int peEpoch);
    virtual void setFamily(int peFamily);

private:
    virtual void PMInstantiatePowerDomains ( void );

public:
    virtual bool attach( IOService * provider );
    virtual bool start( IOService * provider );
    virtual bool configure( IOService * provider );
    virtual IOService * createNub( OSDictionary * from );

    virtual bool compareNubName( const IOService * nub, OSString * name,
				 OSString ** matched = 0 ) const;
    virtual IOReturn getNubResources( IOService * nub );

    virtual int getEpoch(void);
    virtual int getFamily(void);

    virtual bool getModelName( char * name, int maxLength );
    virtual bool getMachineName( char * name, int maxLength );

    virtual int haltRestart(unsigned int type);

    virtual long getGMTTimeOfDay( void );
    virtual void setGMTTimeOfDay( long secs );

    virtual IOReturn getConsoleInfo( PE_Video * consoleInfo );
    virtual IOReturn setConsoleInfo( PE_Video * consoleInfo, unsigned int op );

    virtual void getDefaultBusSpeeds(int *numSpeeds,
				     unsigned long **speedList);

    virtual void registerNVRAMController( IONVRAMController * nvram );

    virtual IOReturn registerInterruptController(OSSymbol *name, IOInterruptController *interruptController);
    virtual IOInterruptController *lookUpInterruptController(OSSymbol *name);
    virtual void setCPUInterruptProperties(IOService *service);
    virtual bool atInterruptLevel(void);

    virtual IORangeAllocator * getPhysicalRangeAllocator(void);

    virtual bool platformAdjustService(IOService *service);

    virtual void PMRegisterDevice(IOService * theNub, IOService * theDevice);
    virtual void PMLog ( const char *,unsigned long, unsigned long, unsigned long );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IODTPlatformExpert : public IOPlatformExpert
{
    OSDeclareAbstractStructors(IODTPlatformExpert)

    UInt8	*	nvramBuffer;

protected:
    virtual bool searchNVRAMProperty( struct IONVRAMDescriptor * hdr,
					UInt32 * where );

    virtual IOReturn getNVRAMPartitionOffset( 
			IOItemCount partition, UInt32 * offset ) = 0;

    virtual IOReturn readNVRAMPropertyType0(
	IORegistryEntry * entry,
	const OSSymbol ** name, OSData ** value );

    virtual IOReturn writeNVRAMPropertyType0(
	IORegistryEntry * entry,
	const OSSymbol * name, OSData * value );

    virtual OSData * unescapeValueToBinary( UInt8 * value );

    virtual OSData * getType1NVRAM( void );

    virtual IOReturn readNVRAMPropertyType1(
	IORegistryEntry * entry,
	const OSSymbol ** name, OSData ** value );

    virtual IOReturn writeNVRAMPropertyType1(
	IORegistryEntry * entry,
	const OSSymbol * name, OSData * value );

public:
    virtual IOService * probe(	IOService * 	provider,
				SInt32 	  *	score );
    virtual bool configure( IOService * provider );

    virtual void processTopLevel( IORegistryEntry * root );
    virtual const char * deleteList( void ) = 0;
    virtual const char * excludeList( void ) = 0;
    virtual IOService * createNub( IORegistryEntry * from );
    virtual bool createNubs( IOService * parent, OSIterator * iter );

    virtual bool compareNubName( const IOService * nub, OSString * name,
				 OSString ** matched = 0 ) const;

    virtual IOReturn getNubResources( IOService * nub );

    virtual bool getModelName( char * name, int maxLength );
    virtual bool getMachineName( char * name, int maxLength );
    
    virtual void registerNVRAMController( IONVRAMController * nvram );

    virtual IOReturn readNVRAMProperty(
	IORegistryEntry * entry,
	const OSSymbol ** name, OSData ** value );

    virtual IOReturn writeNVRAMProperty(
	IORegistryEntry * entry,
	const OSSymbol * name, OSData * value );
};

enum {
    kIOOpenFirmwareNVRAMPartition	= 0,
    kIOXPRAMNVRAMPartition		= 1,
    kIONameRegistryNVRAMPartition	= 2,
    kIOMaxNVRAMPartitions		= 3
};

enum {
    kIOOpenFirmwareNVRAMPartitionSize	= 0x0000,
    kIOXPRAMNVRAMPartitionSize		= 0x0100,
    kIONameRegistryNVRAMPartitionSize	= 0x0400,
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* generic root nub of service tree */

class IOPlatformExpertDevice : public IOService
{
    OSDeclareDefaultStructors(IOPlatformExpertDevice)

public:
    virtual bool initWithArgs( void * p1, void * p2,
					void * p3, void *p4 );
    virtual bool compareName( OSString * name, OSString ** matched = 0 ) const;

    virtual IOWorkLoop *getWorkLoop() const;

    virtual void free();

private:
    IOWorkLoop *workLoop;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* generic nub for motherboard devices */

class IOPlatformDevice : public IOService
{
    OSDeclareDefaultStructors(IOPlatformDevice)

public:
    virtual bool compareName( OSString * name, OSString ** matched = 0 ) const;
    virtual IOService * matchLocation( IOService * client );
    virtual IOReturn getResources( void );
};

#endif /* __cplusplus */

#endif /* ! _IOKIT_IOPLATFORMEXPERT_H */
