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
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOCDDrive.h>
#include <IOKit/storage/IOCDMedia.h>
#include "IOCDAudioNub.h"
#include <IOKit/storage/IOCDDriveNub.h>

#define	super	IOHDDrive
OSDefineMetaClassAndStructors(IOCDDrive,IOHDDrive)

/* Accept a new piece of media, doing whatever's necessary to make it
 * show up properly to the system.
 */
IOReturn
IOCDDrive::acceptNewMedia(void)
{
    IOReturn result;
    bool ok;
    int i;
    UInt32 nblocks;
    UInt64 nbytes;
    int nDataTracks;
    int nAudioTracks;
    char name[128];
    bool nameSep;

//    IOLog("%s[IOCDDrive]::acceptNewMedia\n",getName());

    /* First, we cache information about the tracks on the disc: */
    
    result = cacheTocInfo();
    if (result != kIOReturnSuccess) {
        IOLog("%s[IOCDDrive]::acceptNewMedia; err '%s' from cacheTocInfo\n",
            getName(),stringFromReturn(result));
        return(result);
    }

    /* Scan thru the track list, counting up the number of Data and Audio tracks.
     * We also total up the number of blocks (bytes) for all data tracks.
     */
    
    nDataTracks = 0;
    nAudioTracks = 0;
    nblocks = 0;
    
    for (i = 1; i <= 99; i++) {		/* only tracks 1-99, not leadout or points */
        if (trackExists(i)) {
            if (trackIsData(i)) {
                nDataTracks++;
                nblocks += getTrackBlocks(i);
            } else {
                nAudioTracks++;
            }
        }
    }

    nbytes = nblocks * 2048;

    /* Instantiate a CD Media nub above ourselves. */

    name[0] = 0;
    nameSep = false;
    if (_provider->getVendorString()) {
        strcat(name, _provider->getVendorString());
        nameSep = true;
    }
    if (_provider->getProductString()) {
        if (nameSep == true)  strcat(name, " ");
        strcat(name, _provider->getProductString());
        nameSep = true;
    }
    if (nameSep == true)  strcat(name, " ");
    strcat(name, "Media");

    result = instantiateMediaObject((IOMedia **)&_mediaNub,0,nbytes,2048,true,name);

    if (_mediaNub) {
        ok = _mediaNub->attach(this);
    } else {
        IOLog("%s[IOCDDrive]::acceptNewMedia; can't instantiate CD media nub.\n",getName());
        return(result);			/* give up now */
    }
    if (!ok) {
        IOLog("%s[IOCDDrive]::acceptNewMedia; can't attach CD media nub.\n",getName());
        _mediaNub->release();
        _mediaNub = NULL;
        return(kIOReturnNoMemory);	/* give up now */
    }
        
    /* Instantiate an audio control nub for the audio portion of the media. */

    _acNub = new IOCDAudioNub;
    if (_acNub) {
        _acNub->init();
        ok = _acNub->attach(this);
        if (!ok) {
            IOLog("%s[IOCDDrive]::acceptNewMedia; can't attach audio control nub.\n",getName());
            _acNub->release();
            _acNub = NULL;
        }
    } else {
        IOLog("%s[IOCDDrive]::acceptNewMedia; can't instantiate audio control nub.\n",
              getName());
    }

    IOLog("%s media: %ld blocks, %ld bytes each, %d data tracks, %d audio tracks.\n",
            getName(),nblocks,2048L,nDataTracks,nAudioTracks);

    /* Now that the nubs are attached, register them. */

    _mediaNub->setProperty("audio tracks",nAudioTracks,32);
    _mediaNub->setProperty("data tracks",nDataTracks,32);
    _mediaNub->registerService();

    if (_acNub) {
        _acNub->registerService();
    }

    _mediaPresent = true;

    return(result);
}

IOReturn
IOCDDrive::audioPlay(positioningType addressType,cdAddress address,audioPlayMode mode)
{
    return(_CDprovider->audioPlay(addressType,address,mode));
}

IOReturn
IOCDDrive::audioPause(bool pause)
{
    return(_CDprovider->audioPause(pause));
}

IOReturn
IOCDDrive::audioScan(positioningType addressType,cdAddress address,bool reverse)
{
    return(_CDprovider->audioScan(addressType,address,reverse));
}

IOReturn
IOCDDrive::audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode)
{
    return(_CDprovider->audioTrackSearch(addressType,address,startPlay,mode));
}

IOReturn
IOCDDrive::cacheTocInfo(void)
{
    IOReturn result;
    struct rawToc *toc;
    struct tocTrackDescriptor *d;
    struct tocTrackDescriptor *end;
    int length;
    struct trackInfo *t;
    struct trackInfo *priorTrack = 0;		/* =0 avoids "uninitialized" warning */
    int i;

    for (i = 0; i < kTrackEntries; i++) {		/* max 99 tracks plus leadout */
        bzero((void *)&_trackInfo[i],sizeof(struct trackInfo));
    }

    toc = IONew(struct rawToc,1);
    if (toc == NULL) {
        return(kIOReturnNoMemory);
    }

    /* Read the TOC in Lba format to get most of the info we need: */

    result = _CDprovider->readTOC(toc,sizeof(struct rawToc),ktocSCSI2LBA);

//    IOLog("raw toc in LBA format:\n");
//    dump((char *)toc,sizeof(struct rawToc));
         
    if (result != kIOReturnSuccess) {
        IODelete(toc,struct rawToc,1);
        return(result);
    }

    /* Scan all the tracks in the TOC, setting up our track info.*/

    d = &toc->descriptors[0];
    length = toc->header.len_hi << 8 | toc->header.len_lo - 2;	/* bytes used for descriptors */
    end = &toc->descriptors[length / sizeof(struct tocTrackDescriptor)];

    while (d < end) {
        t = &_trackInfo[d->track];
        setTrackInfoEntry(d,t);
        t->lba = d->lba_3 << 24 | d->lba_2 << 16 | d->lba_1 <<  8 | d->lba_0;

        /* Compute the number of blocks in the prior track, now that we
         * know the starting LBA of this track.
         */
        if (d->track > 1) {
            priorTrack->nblocks = t->lba - priorTrack->lba;
        }
        priorTrack = t;
        d++;
    }

    /* Read the TOC again, this time in MSF format, and grab the MSF info. */

    result = _CDprovider->readTOC(toc,sizeof(struct rawToc),ktocSCSI2MSF);
    if (result != kIOReturnSuccess) {
        IODelete(toc,struct rawToc,1);
        return(result);
    }

    d = &toc->descriptors[0];
    length = toc->header.len_hi << 8 | toc->header.len_lo - 2;	/* bytes used for descriptors */
    end = &toc->descriptors[length / sizeof(struct tocTrackDescriptor)];

    while (d < end) {
        t = &_trackInfo[d->track];
//        setTrackInfoEntry(d,t);
        t->msf = d->lba_3 << 24 | d->lba_2 << 16 | d->lba_1 <<  8 | d->lba_0;
        d++;
    }

    IODelete(toc,struct rawToc,1);

    _tocInfoCached = true;

    IOLog("-CD track ctl isdata    lba10   lba16    M:S:F    nblks10 nblks16\n");

#define SHOWTOC
#ifdef SHOWTOC
    for (i = 1; i < kTrackEntries; i++) {
        if (_trackInfo[i].track != 0) {
            IOLog("     %02x   %02x    %s     %8d %08x %02d:%02d:%02d %8d %08x\n",
                _trackInfo[i].track,
                _trackInfo[i].control,
                _trackInfo[i].isData ? "Y" : "N",
                (int)_trackInfo[i].lba,
                (int)_trackInfo[i].lba,
                (int)_trackInfo[i].msf >> 16 & 0xff,
                (int)_trackInfo[i].msf >>  8 & 0xff,
                (int)_trackInfo[i].msf       & 0xff,
                (int)_trackInfo[i].nblocks,
                (int)_trackInfo[i].nblocks);
        }
    }
#endif

    return(result);
}

/* Decommission all nubs. */
IOReturn
IOCDDrive::decommissionMedia(bool forcible)
{
    IOReturn result;

    result = kIOReturnSuccess;

    lockForArbitration();

    if (_mediaNub) {
        result = tearDown(_mediaNub);
        if (forcible || (result == kIOReturnSuccess)) {
            _mediaNub = 0;
        }
    }

    /* If it's not a forcible teardown, we only attempt to decommission the
     * audio portion of the CD if all the data tracks decommissioned successfully.
     */

    if (forcible || (result == kIOReturnSuccess)) {
        if (_acNub) {
            result = tearDown(_acNub);
            if (forcible || (result == kIOReturnSuccess)) {
                _acNub = 0;
            }
        }
    }

    if (forcible || (result == kIOReturnSuccess)) {
        initMediaStates();			/* deny existence of any media */
        _tocInfoCached = false;
    }

    unlockForArbitration();

    return(result);
}

/* We should check with other clients using the other nubs before we allow
 * the client of the IOCDMediaNub to eject the media.
 */
IOReturn
IOCDDrive::ejectMedia(void)
{
    /* For now, we don't check with the other clients. */
    
    return(super::ejectMedia());
}

IOReturn
IOCDDrive::getAudioStatus(struct audioStatus *status)
{
    return(_CDprovider->getAudioStatus(status));
}

const char *
IOCDDrive::getDeviceTypeName(void)
{
    return(kDeviceTypeCDROM);
}

UInt32
IOCDDrive::getTrackBlocks(UInt32 track)
{
    if (trackExists(track)) {
        return(_trackInfo[track].nblocks);
    } else {
        return(0);
    }
}

UInt32
IOCDDrive::getTrackStartBlock(UInt32 track)
{
    if (trackExists(track)) {
        return(_trackInfo[track].lba);
    } else {
        return(0);
    }
}

bool
IOCDDrive::init(OSDictionary * properties)
{
    int i;

    for (i = 0; i < kTrackEntries; i++) {
        bzero((void *)&_trackInfo[i],sizeof(struct trackInfo));
    }

    _acNub = NULL;
    _mediaNub = NULL;

    _tocInfoCached = false;
    
    return(super::init(properties));
}

IOMedia *
IOCDDrive::instantiateDesiredMediaObject(void)
{
    return(new IOCDMedia);
}

IOReturn
IOCDDrive::readAudioData(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)
{
    return(_CDprovider->readAudioData(addressType,address,blockCount,buffer));
}

IOReturn
IOCDDrive::readAudioSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)
{
    return(_CDprovider->readAudioSubcodes(addressType,address,blockCount,buffer));
}

IOReturn
IOCDDrive::readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume)
{
    return(_CDprovider->readAudioVolume(leftVolume,rightVolume));
}

IOReturn
IOCDDrive::readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                          UInt8 blockCount,UInt8 *buffer)
{
    return(_CDprovider->readAudioWithQSubcode(addressType,address,blockCount,buffer));
}

IOReturn
IOCDDrive::readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                          UInt8 blockCount,UInt8 *buffer)
{
    return(_CDprovider->readAudioWithAllSubcodes(addressType,address,blockCount,buffer));
}

IOReturn
IOCDDrive::readEntireTOC(struct macEntireToc *buffer)
{
    /* Return our cached TOC info. */
    
    int i;
    int out;

    bzero(buffer,sizeof(struct macEntireToc));

    out = 0;
    
    /* Copy A0, A1, A2: */

    for (i = 0xa0; i <= 0xa2; i++) {
        if (_trackInfo[i].track != 0) {		/* then the track exists */
            buffer->entries[out].trackPoint	= _trackInfo[i].track;
            buffer->entries[out].type		= (trackType)_trackInfo[i].control;
            buffer->entries[out].pMin		= _trackInfo[i].msf >> 16;
            buffer->entries[out].pSec		= _trackInfo[i].msf >>  8;
            buffer->entries[out].pFrame      	= _trackInfo[i].msf & 0xff;
            
            out++;
        }
    }

    /* Copy tracks: */
    
    for (i = 1; i <= 99; i++) {
        if (_trackInfo[i].track != 0) {		/* then the track exists */
            buffer->entries[out].trackPoint	= _trackInfo[i].track;
            buffer->entries[out].type		= (trackType)_trackInfo[i].control;
            buffer->entries[out].pMin		= _trackInfo[i].msf >> 16;
            buffer->entries[out].pSec		= _trackInfo[i].msf >>  8;
            buffer->entries[out].pFrame      	= _trackInfo[i].msf & 0xff;

            out++;
        }
    }
    return(kIOReturnSuccess);
}

IOReturn
IOCDDrive::readHeader(UInt32 blockAddress,struct headerInfo *buffer)
{
    return(_CDprovider->readHeader(blockAddress,buffer));
}

IOReturn
IOCDDrive::readISRC(UInt32 track,UInt8 *buffer,bool *found)
{
    return(_CDprovider->readISRC(track,buffer,found));
}

IOReturn
IOCDDrive::readLeadOutAddress(cdAddress *buffer)
{
    if (!_tocInfoCached) {
        return(kIOReturnNotReady);
    }
        
    *buffer = (cdAddress)_trackInfo[0xaa].msf;
    return(kIOReturnSuccess);
}

IOReturn
IOCDDrive::readMCN(UInt8 *buffer,bool *found)
{
    return(_CDprovider->readMCN(buffer,found));
}

IOReturn
IOCDDrive::readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize)
{
    return(_CDprovider->readQSubcodes(buffer,bufSize));
}

IOReturn
IOCDDrive::readSessionInfo(struct sessionInfo *info)
{
    IOReturn result;
    struct rawToc *toc;
    struct tocTrackDescriptor *d;

    toc = IONew(struct rawToc,1);
    if (toc == NULL) {
        return(kIOReturnNoMemory);
    }

    result = _CDprovider->readTOC(toc,sizeof(struct rawToc),ktocSessionInfo);

    if (result == kIOReturnSuccess) {
        info->firstSessionNumber	= toc->header.firstTrack;
        info->lastSessionNumber	= toc->header.lastTrack;
        d = &toc->descriptors[0];
        info->trackNumber		= d->track;
        info->info.type			= (trackType)(d->adrControl);
        info->info.address		= (cdAddress)(	d->lba_3 << 24 |
                                                    d->lba_2 << 16 |
                                                    d->lba_1 <<  8 |
                                                    d->lba_0);
    }
    
    IODelete(toc,struct rawToc,1);

    return(result);
}

IOReturn
IOCDDrive::readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount)
{
    return(_CDprovider->readSubcodeBuffer(buffer,purge,entryCount));
}

IOReturn
IOCDDrive::readTheQSubcode(struct qSubcode *buffer)
{
    return(_CDprovider->readTheQSubcode(buffer));
}

IOReturn
IOCDDrive::readTrackInfo(UInt32 startingTrack,struct trackTypeInfo *buf,UInt32 bufSize)
{
    struct trackTypeInfo *end;

    if (!_tocInfoCached) {
        return(kIOReturnNotReady);
    }
        
    end = &buf[bufSize / sizeof(struct trackTypeInfo)];

    /* Copy till we run past track 99 or the buffer runs out. */
    
    while (startingTrack <= 99) {
        if (buf >= end) {
            return(kIOReturnSuccess);
        }
        if (_trackInfo[startingTrack].track != 0) {	/* then the track exists */
            buf->type = (trackType)_trackInfo[startingTrack].control;
            buf->address = _trackInfo[startingTrack].msf;
            
            buf++;
        }
        
        startingTrack++;
    }

    return(kIOReturnSuccess);
}

IOReturn
IOCDDrive::readTrackLimits(UInt32 *first,UInt32 *last)
{
    int i;
    bool gotFirst;

    if (!_tocInfoCached) {
        return(kIOReturnNotReady);
    }

    gotFirst = false;

    for (i = 1; i < 99; i++) {
        if (_trackInfo[i].track == 0) {		/* track doesn't exist; skip it */
            continue;
        }

        if (!gotFirst) {
            *first = _trackInfo[i].track;
            gotFirst = true;
        }

        /* Last will keep changing till we end the loop and will end up right. */
        
        *last = _trackInfo[i].track;
    }
    
    return(kIOReturnSuccess);
}

IOReturn
IOCDDrive::setAudioStopAddress(positioningType addressType,cdAddress address)
{
    return(_CDprovider->setAudioStopAddress(addressType,address));
}

bool
IOCDDrive::setProvider(IOService * provider)
{
    _CDprovider = OSDynamicCast(IOCDDriveNub,provider);

    if (_CDprovider == NULL) {
        return(false);
    } else {
        super::setProvider(provider);
        return(true);
    }
}

void
IOCDDrive::setTrackInfoEntry(struct tocTrackDescriptor *d,struct trackInfo *t)
{
    t->track = d->track;
    t->control = d->adrControl & 0xff;
    if ((d->adrControl & 0xf0) == 0x10) {	/* then Qsub info is present */
        t->control = d->adrControl & 0xff;
        if (d->adrControl & tt_data) {		/* it's a data track */
            t->isData = true;
        } else {
            t->isData = false;
        }
    }
}

IOReturn
IOCDDrive::setVolume(UInt8 leftVolume,UInt8 rightVolume)
{
    return(_CDprovider->setVolume(leftVolume,rightVolume));
}

bool
IOCDDrive::showStats(void)
{
    return(false);
}

bool
IOCDDrive::trackExists(UInt32 track)
{
    if (_trackInfo[track].track != 0) {
        return(true);
    } else {
        return(false);
    }
}

bool
IOCDDrive::trackIsAudio(UInt32 track)
{
    if (trackExists(track)) {
        return(!_trackInfo[track].isData);
    } else {
        return(false);
    }
}

bool
IOCDDrive::trackIsData(UInt32 track)
{
    if (trackExists(track)) {
        return(_trackInfo[track].isData);
    } else {
        return(false);
    }
}

void
IOCDDrive::dump(char *buf,int count)
{
    static char string[17] = "xxxxxxxxxxxxxxxx";
    
    short i;
    char  c;
    char *cp;	

    if (!count) return;

    i = 0;
    cp = buf;
    do {
	c = *cp;
	IOLog("%02x ",(c & 0xff));
	c &= 0x7f;
	string[i % 16] = /* (isprint(c) || (c == ' ')) ? c : */ '.';
	if ((i % 16)==15)
	    IOLog("%s\n",string);
	cp++; i++;
    } while (--count);

    /* pad out for last line */
    if (i % 16) {	    /* then a shortie left */
	string[i % 16] = '\0';
	while (i % 16) {
	    IOLog("   ");
	    i++;    
	}
	IOLog("%s\n",string);	
    }
}
