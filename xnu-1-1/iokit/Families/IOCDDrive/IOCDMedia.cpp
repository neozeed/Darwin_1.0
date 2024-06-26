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
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOCDDrive.h>

#define	super	IOMedia
OSDefineMetaClassAndStructors(IOCDMedia,IOMedia)

bool
IOCDMedia::attach(IOService *provider)
{
    _provider = (IOCDDrive *)provider;

    return(super::attach(provider));
}

UInt32
IOCDMedia::getTrackBlocks(UInt32 track)
{
    return(_provider->getTrackBlocks(track));
}

UInt32
IOCDMedia::getTrackStartBlock(UInt32 track)
{
    return(_provider->getTrackStartBlock(track));
}

bool
IOCDMedia::trackExists(UInt32 track)
{
    return(_provider->trackExists(track));
}

bool
IOCDMedia::trackIsAudio(UInt32 track)
{
    return(_provider->trackIsAudio(track));
}

bool
IOCDMedia::trackIsData(UInt32 track)
{
    return(_provider->trackIsData(track));
}
