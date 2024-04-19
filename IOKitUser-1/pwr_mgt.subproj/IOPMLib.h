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

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CFArray.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@function IOPMFindPowerManagement.
@abstract Finds the Root Power Domain IOService.
@param master_device_port  Caller gets this from IOMasterPort.
@result Returns a representation of the Root Domain or zero if request failed.
 */
io_connect_t IOPMFindPowerManagement( mach_port_t master_device_port );
    
    /*!
    @function IOPMSetAggressiveness.
    @abstract Sets one of the aggressiveness factors in IOKit Power Management.
    @param fb  Representation of the Root Power Domain from IOPMFindPowerManagement.
     @param type Specifies which aggressiveness factor is being set.
     @param type New value of the aggressiveness factor.
     @result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOPMSetAggressiveness (io_connect_t fb, unsigned long type, unsigned long aggressiveness );

    /*!
@function IOPMGetAggressiveness.
@abstract Retrieves the current value of one of the aggressiveness factors in IOKit Power Management.
@param fb  Representation of the Root Power Domain from IOPMFindPowerManagement.
@param type Specifies which aggressiveness factor is being retrieved.
@param type Points to where to store the retrieved value of the aggressiveness factor.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOPMGetAggressiveness ( io_connect_t fb, unsigned long type, unsigned long * aggressiveness );

    /*!
        @function IORegisterApp.
        @abstract Connects the caller to an IOService for the purpose of receiving power state
             change notifications for the device controlled by the IOService.
        @param theDriver  Representation of the IOService, probably  from IOIteratorNext.
        @param recvPort Port on which to receive power state change notifications, probably
             from IOCreateReceivePort.
        @param refcon Data returned on power state change notifications and not used by
             the kernel.
        @result Returns a representation of the IOService or zero if request failed.
             */
io_connect_t IORegisterApp ( io_object_t theDriver, mach_port_t recvPort, long refcon );

    /*!
    @function IORegisterForSystemPower.
    @abstract Connects the caller to the Root Power Domain  IOService for the purpose of receiving
     Sleep, Wake, ShutDown, PowerUp notifications for the System.
    @param recvPort Port on which to receive power state change notifications, probably
         from IOCreateReceivePort.
    @param refcon Data returned on power state change notifications and not used by
         the kernel.
    @result Returns a representation of the Root Power Domain IOService or zero if request failed.
         */
io_connect_t IORegisterForSystemPower ( mach_port_t recvPort, long refcon );

    /*!
@function IODeregisterApp.
@abstract Disconnects the caller from an IOService after receiving power state
     change notifications from the IOService.
@param kernelPort  Port used to communicate to the kernel,  from IORegisterApp.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IODeregisterApp ( io_connect_t kernelPort );

    /*!
@function IODeregisterForSystemPower.
@abstract Disconnects the caller from the Root Power Domain  IOService after receiving
     system power state change notifications.
@param kernelPort  Port used to communicate to the kernel,  from IORegisterForSystemPower.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IODeregisterForSystemPower ( io_connect_t kernelPort );

    /*!
@function IOAllowPowerChange.
@abstract The caller acknowledges notification of a power state change on a device
     it is interested in.
@param kernelPort  Port used to communicate to the kernel,  from IORegisterApp or
     IORegisterForSystemPower.
@param notificationID A copy of the notification ID which came as part of the power
     state change notification being acknowledged.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOAllowPowerChange ( io_connect_t kernelPort, long notificationID );

    /*!
@function IOCancelPowerChange.
@abstract The caller negatively acknowledges notification of a power state change on a device
     it is interested in.  This prevents the state change.
@param kernelPort  Port used to communicate to the kernel,  from IORegisterApp or
     IORegisterForSystemPower.
@param notificationID A copy of the notification ID which came as part of the power
     state change notification being acknowledged.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOCancelPowerChange ( io_connect_t kernelPort, long notificationID );

IOReturn IOPMSleepSystem ( io_connect_t fb );
IOReturn IOPMWakeSystem ( io_connect_t fb );

IOReturn IOPMCopyBatteryInfo( mach_port_t masterPort, CFArrayRef * info );
    
#ifdef __cplusplus
}
#endif
