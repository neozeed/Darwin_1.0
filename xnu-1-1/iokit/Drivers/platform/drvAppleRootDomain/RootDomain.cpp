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
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandQueue.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "RootDomainUserClient.h"

extern "C" {
extern void kprintf(const char *, ...);
}

void PMreceiveCmd ( OSObject *,  void *, void *, void *, void * );


#define number_of_power_states 2
#define powerOff 0
#define powerOn 1

static IOPMPowerState ourPowerStates[number_of_power_states] = {
{1,0,0,0,0,0,0,0,0,0,0,0},
    {1,0,IOPMPowerOn,IOPMPowerOn,0,0,0,0,000000,0,0,0},
};

#define super IOService
OSDefineMetaClassAndStructors(IOPMrootDomain,IOService)


// **********************************************************************************
// start
//
// We don't do much here.  The real initialization occurs when the platform
// expert informs us we are the root.
// **********************************************************************************
bool IOPMrootDomain::start ( IOService * nub )
{
super::start(nub);
PMinit();
return true;
}


//*********************************************************************************
// youAreRoot
//
// Power Managment is informing us that we are the root power domain.
// The only difference between us and any other power domain is that
// we have no parent and therefore never call it.
//
// We complete our initialization here.
//*********************************************************************************
IOReturn IOPMrootDomain::youAreRoot ( void )
{
    super::youAreRoot();
    pm_vars->PMworkloop = IOWorkLoop::workLoop();					// make the workloop
    if ( ! pm_vars->PMworkloop ) {
        return IOPMNoErr;
    }
    pm_vars->commandQueue = IOCommandQueue::commandQueue(this, PMreceiveCmd);	// make a command queue
    if (! pm_vars->commandQueue ||
        (  pm_vars->PMworkloop->addEventSource( pm_vars->commandQueue) != kIOReturnSuccess) ) {
        return IOPMNoErr;
    }

    pm_vars->aggressiveness = 640;
    current_values[kPMGeneralAggressiveness] = pm_vars->aggressiveness;

    // Clamp power on.  We will revisit this decision when the login window is displayed
    // and we receive preferences via SetAggressiveness.
    makeUsable();

    registerControllingDriver(this,ourPowerStates,number_of_power_states);


    return IOPMNoErr;
}


// **********************************************************************************
// command_received
//
// We have received a command from ourselves on the command queue.
// If it is to send a recently-received aggressiveness factor, do so.
// Otherwise, it's something the superclass enqueued.
// **********************************************************************************
void IOPMrootDomain::command_received ( void * command, void * x, void * y, void * z )
{
    switch ( (int)command ) {
        case kPMbroadcastAggressiveness:
            super::setAggressiveness((unsigned long)x,(unsigned long)y);
            if ( (int)x == kPMMinutesToSleep ) {
                if ( (int)y == 0 ) {
                    changeStateToPriv(powerOn);		// no sleep, clamp power on
                }
                else {
                    changeStateToPriv(powerOff);		// let it sleep when children are all asleep
                }
            }
                break;
        case kPMsleepDemand:
            changeStateToPriv(powerOff);			// force root domain off
            overrideOnPriv();
            break;
        case kPMwakeSignal:
            super::systemWake();				// broadcast wake to power tree
            overrideOffPriv();				// allow children to wake us up
            break;
        default:
            super::command_received(command,x,y,z);
            break;
    }
}


//*********************************************************************************
// setAggressiveness
//
// Some aggressiveness factor has changed.  We put this change on our
// command queue so that we can broadcast it to the hierarchy while on
// the Power Mangement workloop thread.  This enables objects in the
// hierarchy to successfully alter their idle timers, which are all on the
// same thread.
//*********************************************************************************

IOReturn IOPMrootDomain::setAggressiveness ( unsigned long type, unsigned long newLevel )
{
    if ( type <= kMaxType ) {
        current_values[type] = newLevel;
    }
    pm_vars->commandQueue->enqueueCommand(true, (void *)kPMbroadcastAggressiveness, (void *) type, (void *) newLevel );
    return kIOReturnSuccess;
}


//*********************************************************************************
// getAggressiveness
//
// Called by the user client.
//*********************************************************************************

IOReturn IOPMrootDomain::getAggressiveness ( unsigned long type, unsigned long * currentLevel )
{
    if ( type <= kMaxType ) {
        *currentLevel = current_values[type];
    }
    return kIOReturnSuccess;
}


// **********************************************************************************
// sleepSystem
//
// **********************************************************************************
IOReturn IOPMrootDomain::sleepSystem ( void )
{
    kprintf("sleep demand received\n");
    pm_vars->commandQueue->enqueueCommand(true, (void *)kPMsleepDemand );
    
    return kIOReturnSuccess;
}



// **********************************************************************************
// wakeSystem
//
// **********************************************************************************
IOReturn IOPMrootDomain::wakeSystem ( void )
{
    pm_vars->commandQueue->enqueueCommand(true, (void *)kPMwakeSignal );

    return kIOReturnSuccess;
}



//*********************************************************************************
// initialPowerStateForDomainState
//
// Assume that power is initially on in the root power domain.
//*********************************************************************************

unsigned long IOPMrootDomain::initialPowerStateForDomainState ( IOPMPowerFlags )
{
  return powerOn;
}


// **********************************************************************************
// setPowerState
//
// **********************************************************************************
IOReturn IOPMrootDomain::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice)
{
    if ( powerStateOrdinal == powerOff ) {
        kprintf("system would sleep here\n");
    }
  return IOPMAckImplied;
}


// **********************************************************************************
// newUserClient
//
// **********************************************************************************
IOReturn IOPMrootDomain::newUserClient(  task_t owningTask,  void * /* security_id */, UInt32 type, IOUserClient ** handler )
{
    IOReturn		err = kIOReturnSuccess;
    RootDomainUserClient *	client;

    client = RootDomainUserClient::withTask(owningTask);

    if( !client || (false == client->attach( this )) ||
        (false == client->start( this )) ) {
        if(client) {
            client->detach( this );
            client->release();
            client = NULL;
        }
        err = kIOReturnNoMemory;
    }
    *handler = client;	
    return err;
}
