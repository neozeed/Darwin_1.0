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
/* IOHDTypes.h created by rick on Wed 07-Apr-1999 */

#ifndef	_IOHDTYPES_H
#define	_IOHDTYPES_H

/*!
 * @typedef gdCompletionFunction
 * @discussion
 * Type and arguments of callout C function that is used to complete
 * an asynchronous IO operation.
 * @param target
 * The target C function to be called.
 * @param param
 * A parameter passed to the target C function. It is not validated
 * or changed.
 * @param actualByteCount
 * The actual byte count transferred in the operation.
 * @param result
 * The kIOReturn value from the operation
 */
typedef void (*gdCompletionFunction) ( 
            IOService	*target,
            void	*param,
            UInt64	actualByteCount,
            IOReturn	result);

class IOMemoryDescriptor;

/*!
 * @struct cdbParams
 * @discussion
 * Data provided by a client to an executeCDB operation.
 * @field cdbLength
 * The length of the command bytes (cdb), in bytes.
 * @field cdb
 * The actual command bytes to be sent to the device. These
 * are not checked by the driver.
 * @field senseLength
 * The requested maximum sense length, in bytes.
 * @field actualSenseLength
 * The actual number of sense bytes transferred. If the driver
 * does not support autosense, this field will always return zero.
 * @field senseBuffer
 * The buffer for the sense data. This buffer must have already
 * been prepared for IO by the caller. The buffer must contain the
 * data direction.
 * @field timeoutSeconds
 * The timeout for the command, in seconds.
 * @field dataLength
 * The requested number of data bytes to transfer with the command,
 * or zero if no data is to be transferred.
 * @field actualDataLength
 * The actual number of data bytes transferred.
 * @field dataBuffer
 * The buffer for the data. This buffer must have already
 * been prepared for IO by the caller. The buffer must contain the
 * data direction.
 * @field status
 * The status reported by the device at command completion. The
 * contents of thisfield will depend on the type of
 * transport (e.g. SCSI, ATAPI, etc). Some drivers may not support
 * the return of status information and will zero this field.
 */
struct cdbParams {
    UInt32			cdbLength;
    UInt8			cdb[16];
    UInt32			senseLength;
    UInt32			actualSenseLength;
    IOMemoryDescriptor *	senseBuffer;
    UInt32			timeoutSeconds;
    UInt32			dataLength;
    UInt32			actualDataLength;
    IOMemoryDescriptor *	dataBuffer;
    UInt8			status;
};
#endif
