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
/* IOCDMedia.h created by rick on Mon 22-Mar-1999
 *
 * This class is the protocol for generic CD Media data functionality, independent of
 * the physical connection protocol (e.g. SCSI, ATA, USB).
 *
 * A subclass implements relay methods that translate our requests into
 * calls to a protocol- and device-specific provider.
 */

#ifndef	_IOCDMEDIA_H
#define	_IOCDMEDIA_H

#include <IOKit/storage/IOMedia.h>
#include "IOCDTypes.h"

class IOCDDrive;

class IOCDMedia : public IOMedia {

    OSDeclareDefaultStructors(IOCDMedia)

public:

    /* Overrides from IOService: */

    virtual bool	attach(IOService *provider);

    /* end of IOService overrides */

    /* ----------------------------------------*/
    /* APIs used by a track-based partitioner. */
    /* ----------------------------------------*/
    
    virtual UInt32	getTrackBlocks(UInt32 track);
    virtual UInt32	getTrackStartBlock(UInt32 track);
    virtual bool	trackExists(UInt32 track);
    virtual bool	trackIsAudio(UInt32 track);
    virtual bool	trackIsData(UInt32 track);
   
protected:

    IOCDDrive *	_provider;
};
#endif
