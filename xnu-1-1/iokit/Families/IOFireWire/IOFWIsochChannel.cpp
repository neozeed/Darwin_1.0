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
 * HISTORY
 *
 */

#include <IOKit/assert.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWIsochChannel.h>
#include <IOKit/firewire/IOFWIsochPort.h>
#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireDevice.h>

OSDefineMetaClassAndStructors(IOFWIsochChannel, OSObject)

void IOFWIsochChannel::threadFunc(thread_call_param_t arg, thread_call_param_t)
{
    ((IOFWIsochChannel *)arg)->reallocBandwidth();
}

bool IOFWIsochChannel::init(IOFireWireController *control, bool doIRM,
	UInt32 bandwidth, IOFWSpeed prefSpeed,
	FWIsochChannelForceStopNotificationProcPtr stopProc, void *stopRefCon)
{
    if(!OSObject::init())
	return false;
    fControl = control;
    fDoIRM = doIRM;
    fMBitSec = bandwidth;
    fPrefSpeed = prefSpeed;
    fStopProc = stopProc;
    fStopRefCon = stopRefCon;
    fTalker = NULL;
    fListeners = OSSet::withCapacity(1);
    fChannel = 64;	// Illegal channel
    return fListeners != NULL;
}

void IOFWIsochChannel::free()
{
    if(fListeners)
        fListeners->release();
    OSObject::free();
}

IOReturn IOFWIsochChannel::setTalker(IOFWIsochPort *talker)
{
    fTalker = talker;
    return kIOReturnSuccess;
}

IOReturn IOFWIsochChannel::addListener(IOFWIsochPort *listener)
{
    if(fListeners->setObject(listener))
        return kIOReturnSuccess;
    else
        return kIOReturnNoMemory;
}

IOReturn IOFWIsochChannel::updateBandwidth(bool claim)
{
    FWAddress addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable);
    IOFireWireDevice *irm;	// The Isochronous Resource Manager, if there is one
    IOFWReadQuadCommand *readCmd = NULL;
    IOFWCompareAndSwapCommand *lockCmd = NULL;
    UInt32 newVal;
    UInt32 oldVal;
    IOReturn result;
    bool tryAgain;

    do {
        irm = fControl->getIRMDevice();
        if(fBandwidth != 0) {
            if(irm != NULL) {
                readCmd = irm->createReadQuadCommand(addr, &oldVal, 1);
                if(!readCmd) {
                    result = kIOReturnNoMemory;
                    break;
                }
                readCmd->submit();
                result = readCmd->fStatus;
                readCmd->release();
                readCmd = NULL;
                if(kIOReturnSuccess != result) {
                    kprintf("read result 0x%x\n", result);
                    break;
                }
            }
            do {
		if(claim)
                    newVal = oldVal - fBandwidth;
		else
                    newVal = oldVal + fBandwidth;
		if(irm) {
                    lockCmd = fControl->getIRMDevice()->createCompareAndSwapCommand(addr, &oldVal, &newVal, 1);
                    if(!lockCmd) {
                        result = kIOReturnNoMemory;
                        break;
                    }
                    lockCmd->submit();
                    if(kIOReturnSuccess != lockCmd->fStatus)
                        kprintf("bandwidth update result 0x%x\n", lockCmd->fStatus);
                    tryAgain = !lockCmd->locked(&oldVal);
                    lockCmd->release();
                    lockCmd = NULL;
                    if(tryAgain) {
                        kprintf("bandwidth update failed, newval 0x%x\n", oldVal);
                    }
		}
		else
                    tryAgain = false;
            } while (tryAgain);
            if(!claim)
                fBandwidth = 0;
        }
        if(fChannel != 64 && (irm != NULL)) {
            UInt32 mask;
            if(fChannel <= 31) {
                addr.addressLo = kCSRChannelsAvailable31_0;
                mask = 1 << (31-fChannel);
            }
            else {
                addr.addressLo = kCSRChannelsAvailable63_32;
                mask = 1 << (63-fChannel);
            }
            readCmd = irm->createReadQuadCommand(addr, &oldVal, 1);
            if(!readCmd) {
                result = kIOReturnNoMemory;
                break;
            }
            readCmd->submit();
            result = readCmd->fStatus;
            if(kIOReturnSuccess != result) {
                kprintf("read result 0x%x\n", result);
                break;
            }
            do {
                if(claim)
                    newVal = oldVal & ~mask;
		else
                    newVal = oldVal | mask;
                lockCmd = irm->createCompareAndSwapCommand(addr, &oldVal, &newVal, 1);
                if(!lockCmd) {
                    result = kIOReturnNoMemory;
                    break;
                }
                lockCmd->submit();
                if(kIOReturnSuccess != lockCmd->fStatus)
                    kprintf("channel update result 0x%x\n", lockCmd->fStatus);
                tryAgain = !lockCmd->locked(&oldVal);
                lockCmd->release();
                lockCmd = NULL;
                if(tryAgain) {
                    kprintf("channel update failed, newval 0x%x\n", oldVal);
                }
            } while (tryAgain);
            if(!claim)
                fChannel = 64;
        }
    } while (false);
    if(readCmd)
        readCmd->release();
    if(lockCmd)
        lockCmd->release();
    return result;
}

IOReturn IOFWIsochChannel::allocateChannel()
{
    UInt64 portChans;
    UInt64 allowedChans, savedChans;
    IOFireWireDevice *irm;	// The Isochronous Resource Manager, if there is one
    IOFWReadQuadCommand *readCmd = NULL;
    IOFWCompareAndSwapCommand *lockCmd = NULL;
    UInt32 newVal;
    FWAddress addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable);
    OSIterator *listenIterator;
    IOFWIsochPort *listen;
    IOFWSpeed portSpeed;
    UInt32 old[3];
    UInt32 bandwidth;
    UInt32 channel;
    bool tryAgain;	// For locks.
    IOReturn result = kIOReturnSuccess;

    // Get best speed, minimum of requested speed and paths from talker to each listener
    fSpeed = fPrefSpeed;

    do {
        // reduce speed to minimum of so far and what all ports can do,
        // and find valid channels
        allowedChans = (UInt64)-1;
        if(fTalker) {
            fTalker->getSupported(portSpeed, portChans);
            if(portSpeed < fSpeed)
                fSpeed = portSpeed;
            allowedChans &= portChans;
        }
        listenIterator = OSCollectionIterator::withCollection(fListeners);
        if(listenIterator) {
            while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
                listen->getSupported(portSpeed, portChans);
                if(portSpeed < fSpeed)
                    fSpeed = portSpeed;
                allowedChans &= portChans;
            }
        }

        // reserve bandwidth, allocate a channel
        if(fDoIRM && (irm = fControl->getIRMDevice())) {
            savedChans = allowedChans; // In case we have to try a few times
            // Careful of 32 bit overflow!
            bandwidth = fMBitSec / (1000 * (1 << fSpeed));
            bandwidth *= 6144;
            bandwidth /= 100000;
    //kprintf("Bandwidth is %d\n", bandwidth);
            readCmd = irm->createReadQuadCommand(addr, old, 3);
            if(!readCmd) {
                result = kIOReturnNoMemory;
                break;
            }
            readCmd->submit();
            result = readCmd->fStatus;
            if(kIOReturnSuccess != result) {
    		kprintf("read result 0x%x\n", readCmd->fStatus);
		break;
            }
    //kprintf("Allocated Bandwidth is %d, channels are 0x%x 0x%x\n", old[0], old[1], old[2]);
            allowedChans &= old[1] | ((UInt64)old[2] << 32);

            // Claim bandwidth
            tryAgain = false;
            do {
                if(old[0] < bandwidth) {
                    result = kIOReturnNoSpace;
                    break;
                }
                newVal = old[0] - bandwidth;
                lockCmd = irm->createCompareAndSwapCommand(addr, &old[0], &newVal, 1);
                if(!lockCmd) {
                    result = kIOReturnNoMemory;
                    break;
                }
                lockCmd->submit();
                if(kIOReturnSuccess != lockCmd->fStatus)
                    kprintf("bandwith update result 0x%x\n", lockCmd->fStatus);
                tryAgain = !lockCmd->locked(&old[0]);
                lockCmd->release();
                lockCmd = NULL;
                if(tryAgain) {
                    kprintf("Bandwith lock failed, newval 0x%x\n", old[0]);
		}
            } while (tryAgain);
            if(kIOReturnSuccess != result)
		break;
            fBandwidth = bandwidth;
        }

        tryAgain = false;
        do {
            for(channel=0; channel<64; channel++) {
                if(allowedChans & (1<<(63-channel))) {
                     break;
                }
            }
            kprintf("using channel %d\n", channel);
            if(channel == 64) {
                result = kIOReturnNoResources;
                break;
            }

            // Allocate a channel
            if(fDoIRM && irm) {
                UInt32 *oldPtr;
                // Claim channel
                if(channel < 32) {
                    addr.addressLo = kCSRChannelsAvailable31_0;
                    oldPtr = &old[1];
                    newVal = *oldPtr & ~(1<<(31-channel));
		}
		else {
                    addr.addressLo = kCSRChannelsAvailable63_32;
                    oldPtr = &old[2];
                    newVal = *oldPtr & ~(1<<(63-channel));
		}
                lockCmd = irm->createCompareAndSwapCommand(addr, oldPtr, &newVal, 1);
                if(!lockCmd) {
                    result = kIOReturnNoMemory;
                    break;
                }
                lockCmd->submit();
                if(kIOReturnSuccess != lockCmd->fStatus)
                    kprintf("channel update result 0x%x\n", lockCmd->fStatus);
                tryAgain = !lockCmd->locked(oldPtr);
                lockCmd->release();
                lockCmd = NULL;
                if(tryAgain) {
                    kprintf("channel update failed, newval 0x%x\n", old[0]);
                }
            }
            else
		tryAgain = false;
        } while (tryAgain);
        if(kIOReturnSuccess != result)
            break;
        fChannel = channel;
        if(fDoIRM)
            fControl->addAllocatedChannel(this);

        // allocate hardware resources for each port
        if(listenIterator) {
            listenIterator->reset();
            while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
                listen->allocatePort(fSpeed, fChannel);
            }
            listenIterator->release();
        }
        if(fTalker)
            fTalker->allocatePort(fSpeed, fChannel);
    } while (false);

    if(readCmd)
	readCmd->release();
    if(lockCmd)
        lockCmd->release();

    return result;
}


IOReturn IOFWIsochChannel::releaseChannel()
{
    OSIterator *listenIterator;
    IOFWIsochPort *listen;
    IOReturn result;

    if(fTalker) {
        fTalker->releasePort();
    }
    listenIterator = OSCollectionIterator::withCollection(fListeners);
    if(listenIterator) {
        while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
            listen->releasePort();
        }
        listenIterator->release();
    }

    // release bandwidth and channel
    if(fDoIRM) {
        /*
         * Tell the controller that we don't need to know about
         * bus resets bedore doing anything else, since a bus reset
         * sets us into the state we want (no allocated bandwidth).
         */
        fControl->removeAllocatedChannel(this);
	updateBandwidth(false);
    }
    return kIOReturnSuccess;
}

void IOFWIsochChannel::reallocBandwidth()
{
    kprintf("reallocBandwidth\n");
    updateBandwidth(true);
}

void IOFWIsochChannel::handleBusReset()
{
    kprintf("handleBusReset\n");
    thread_call_func(threadFunc, this, true);
}

IOReturn IOFWIsochChannel::start()
{
    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    // Start all listeners, then start the talker
    listenIterator = OSCollectionIterator::withCollection(fListeners);
    if(listenIterator) {
        listenIterator->reset();
        while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
            listen->start();
        }
        listenIterator->release();
    }
    if(fTalker)
	fTalker->start();

    return kIOReturnSuccess;
}

IOReturn IOFWIsochChannel::stop()
{
    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    // Stop all listeners, then stop the talker
    listenIterator = OSCollectionIterator::withCollection(fListeners);
    if(listenIterator) {
         while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
            listen->stop();
        }
        listenIterator->release();
    }
    if(fTalker)
	fTalker->stop();

    return kIOReturnSuccess;
}
