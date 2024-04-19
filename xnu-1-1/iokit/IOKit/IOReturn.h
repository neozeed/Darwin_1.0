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
 
/*
 * Core IOReturn values. Others may be family defined.
 */

#ifndef __IOKIT_IORETURN_H
#define __IOKIT_IORETURN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mach/error.h>

typedef	kern_return_t		IOReturn;

#ifndef sys_iokit
#define sys_iokit                err_system(0x38)
#endif /* sys_iokit */
#define sub_iokit_common         err_sub(0)
#define sub_iokit_usb         	 err_sub(1)
#define sub_iokit_firewire	 err_sub(2)
#define sub_iokit_reserved       err_sub(-1)
#define	iokit_common_err(return) (sys_iokit|sub_iokit_common|return)

#define kIOReturnSuccess         KERN_SUCCESS          // OK
#define kIOReturnError           iokit_common_err(700) // general error 	
#define kIOReturnNoMemory        iokit_common_err(701) // can't allocate memory 
#define kIOReturnNoResources     iokit_common_err(702) // resource shortage 
#define kIOReturnIPCError        iokit_common_err(703) // error during IPC 
#define kIOReturnNoDevice        iokit_common_err(704) // no such device 
#define kIOReturnNotPrivileged   iokit_common_err(705) // privilege violation 
#define kIOReturnBadArgument     iokit_common_err(706) // invalid argument 
#define kIOReturnLockedRead      iokit_common_err(707) // device read locked 
#define kIOReturnLockedWrite     iokit_common_err(708) // device write locked 
#define kIOReturnExclusiveAccess iokit_common_err(709) // exclusive access and
                                                       //   device already open 
#define kIOReturnBadMessageID    iokit_common_err(710) // sent/received messages
                                                       //   had different msg_id
#define kIOReturnUnsupported     iokit_common_err(711) // unsupported function 
#define kIOReturnVMError         iokit_common_err(712) // misc. VM failure 
#define kIOReturnInternalError   iokit_common_err(713) // internal error 
#define kIOReturnIOError         iokit_common_err(714) // General I/O error 
#define kIOReturnCannotLock      iokit_common_err(716) // can't acquire lock
#define kIOReturnNotOpen         iokit_common_err(717) // device not open 
#define kIOReturnNotReadable     iokit_common_err(718) // read not supported 
#define kIOReturnNotWritable     iokit_common_err(719) // write not supported 
#define kIOReturnNotAligned      iokit_common_err(720) // alignment error 
#define kIOReturnBadMedia        iokit_common_err(721) // Media Error 
#define kIOReturnStillOpen       iokit_common_err(722) // device(s) still open 
#define kIOReturnRLDError        iokit_common_err(723) // rld failure 
#define kIOReturnDMAError        iokit_common_err(724) // DMA failure 
#define kIOReturnBusy            iokit_common_err(725) // Device Busy 
#define kIOReturnTimeout         iokit_common_err(726) // I/O Timeout 
#define kIOReturnOffline         iokit_common_err(727) // device offline 
#define kIOReturnNotReady        iokit_common_err(728) // not ready 
#define kIOReturnNotAttached     iokit_common_err(729) // device not attached 
#define kIOReturnNoChannels      iokit_common_err(730) // no DMA channels left
#define kIOReturnNoSpace         iokit_common_err(731) // no space for data 
#define kIOReturnPortExists      iokit_common_err(733) // port already exists
#define kIOReturnCannotWire      iokit_common_err(734) // can't wire down 
                                                       //   physical memory
#define kIOReturnNoInterrupt     iokit_common_err(735) // no interrupt attached
#define kIOReturnNoFrames        iokit_common_err(736) // no DMA frames enqueued
#define kIOReturnMessageTooLarge iokit_common_err(737) // oversized msg received
                                                       //   on interrupt port
#define kIOReturnNotPermitted    iokit_common_err(738) // not permitted
#define kIOReturnNoPower         iokit_common_err(739) // no power to device
#define kIOReturnNoMedia         iokit_common_err(740) // media not present
#define kIOReturnUnformattedMedia iokit_common_err(741) // media not formatted
#define kIOReturnUnsupportedMode iokit_common_err(742) // no such mode
#define kIOReturnUnderrun        iokit_common_err(743) // data underrun
#define kIOReturnOverrun         iokit_common_err(744) // data overrun
#define kIOReturnDeviceError	 iokit_common_err(745) // The device is not working properly!
#define kIOReturnNoCompletion	 iokit_common_err(746) // A completion routine is required
#define kIOReturnAborted	 iokit_common_err(747) // Operation aborted
#define kIOReturnNoBandwidth	 iokit_common_err(748) // Bus bandwidth would be exceeded
#define kIOReturnNotResponding	 iokit_common_err(749) // Device not responding
#define kIOReturnIsoTooOld	 iokit_common_err(750) // Isochronous I/O request for distant past!
#define kIOReturnIsoTooNew	 iokit_common_err(751) // Isochronous I/O request for distant future
#define kIOReturnInvalid         iokit_common_err(1)   // should never be seen

#ifdef __cplusplus
}
#endif

#endif /* ! __IOKIT_IORETURN_H */
