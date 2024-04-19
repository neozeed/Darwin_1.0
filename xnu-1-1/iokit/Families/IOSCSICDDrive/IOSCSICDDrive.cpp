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
#include <IOKit/IOReturn.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/scsi/IOSCSIDeviceInterface.h>
#include <IOKit/storage/scsi/IOSCSICDDrive.h>
#include <IOKit/storage/scsi/IOSCSICDDriveNub.h>

/* Device types that we intend to handle. */

const UInt8	DT_CDROM	= 0x05;		/* CD-ROM */

#define	super	IOSCSIHDDrive
OSDefineMetaClassAndStructors(IOSCSICDDrive,IOSCSIHDDrive)

IOReturn
IOSCSICDDrive::audioPlay(positioningType addressType,cdAddress address,audioPlayMode mode)
{
    return(doAudioPlayCommand(addressType,address,
                              _audioStopAddress.type,_audioStopAddress.address));
}

IOReturn
IOSCSICDDrive::audioPause(bool pause)
{
    //xxxx
    return(kIOReturnUnsupported);
}

IOReturn
IOSCSICDDrive::audioScan(positioningType addressType,cdAddress address,bool reverse)
{
    struct context *cx;
    struct IOAudioScancdb *c;
    IOSCSICommand *req;
    SCSICDBInfo scsiCDB;
    IOReturn result;

    cx = allocateContext();
    if (cx == NULL) {
        return(kIOReturnNoMemory);
    }
    
    req = cx->scsireq;

    bzero( &scsiCDB, sizeof(scsiCDB) );

    c = (struct IOAudioScancdb *)&scsiCDB.cdb;

    c->opcode = SOP_AUDIOSCAN;
    c->lunbits = 0;
    if (!reverse) {
        c->lunbits |= IOAudioScancdb::kForward;
    }

    switch (addressType) {

        case kBlockAddress	:
            				c->start3 = address >> 24;
                                        c->start2 = address >> 16;
                                        c->start1 = address >>  8;
            				c->start0 = address & 0xff;
                                        c->ctlbyte = IOAudioScancdb::kType_lba;
                                        break;
            
        case kAbsoluteTime	:
            				c->start3 = address >> 24;
                                        c->start2 = address >> 16;
                                        c->start1 = address >>  8;
            				c->start0 = address & 0xff;
                                        c->ctlbyte = IOAudioScancdb::kType_msf;
                                        break;
            
        case kTrackNumber	:
            				c->start3 = 0;
                                        c->start2 = 0;
                                        c->start1 = 0;
            				c->start0 = address & 0xff;
                                        c->ctlbyte = IOAudioScancdb::kType_track;
                                       break;
            
        default			:
                                        deleteContext(cx);
            				return(kIOReturnBadArgument);
    };

    scsiCDB.cdbLength = 6;
    scsiCDB.cdbFlags = 0;
    req->setCDB( &scsiCDB );
    req->setPointers(cx->senseDataDesc, 255, false, true);    
    req->setPointers( cx->memory, 0, false );
    req->setTimeout(1 * 1000);

    result = simpleSynchIO(cx);

    deleteContext(cx);

    return(result);
}

IOReturn
IOSCSICDDrive::audioTrackSearch(positioningType addressType,cdAddress address,
                                      		bool startPlay,audioPlayMode mode)
{
    IOReturn result;
    UInt32 lba;
    
    result = convertMSFToLba(&lba,address);
    if (result != kIOReturnSuccess) {
        return(result);
    }

    result = seek(lba);
    if (result != kIOReturnSuccess) {
        return(result);
    }
    
    if (startPlay) {
        return(audioPlay(addressType,address,mode));
    } else {
        return(result);
    }
}

IOReturn
IOSCSICDDrive::convertMSFToLba(UInt32 *lba,cdAddress address)
{
    //xxx wrong! incoming msf values are bcd!
    *lba =	((address >> 16) * 75 * 60)	+	/* min */
                ((address >>  8) * 75)		+	/* sec */
                (address & 0xff);			/* frames */

    return(kIOReturnSuccess);
}

bool
IOSCSICDDrive::deviceTypeMatches(UInt8 inqBuf[],UInt32 inqLen)
{
    if ((inqBuf[0] & 0x1f) == DT_CDROM) {
//        IOLog("%s[IOSCSICDDrive]::deviceTypeMatches, returning TRUE\n",getName());  
        return(true);
    } else {
//        IOLog("%s[IOSCSICDDrive]::deviceTypeMatches, returning FALSE\n",getName());  
        return(false);			/* we don't handle other devices */        
    }
}

IOReturn
IOSCSICDDrive::doAudioPlayCommand(positioningType startType,cdAddress startAddress,
                                            positioningType endType,cdAddress endAddress)
{
    struct context *cx;
    struct IOAudioPlaycdb *c;
    struct IOAudioPlayMSFcdb *p;
    IOSCSICommand *req;
    SCSICDBInfo scsiCDB;
    IOReturn result;
    UInt32 length;

    /* Can we return an error here, or must we ensure that start and stop are always
     * locally converted to the same units before we use them?
     */
    if (startType != endType) {
        return(kIOReturnBadArgument);
    }

    if (startType != kAbsoluteTime || startType != kBlockAddress) {
        return(kIOReturnBadArgument);
    }

    cx = allocateContext();
    if (cx == NULL) {
        return(kIOReturnNoMemory);
    }
    
    req = cx->scsireq;

    bzero( &scsiCDB, sizeof(scsiCDB) );

    if (startType == kBlockAddress) {		/* use PlayAudio command */
        c = (struct IOAudioPlaycdb *)scsiCDB.cdb;

        c->opcode	= SOP_PLAYAUDIO;
        c->lunbits	= 0;
        c->lba_3	= startAddress >> 24;
        c->lba_2	= startAddress >> 16;
        c->lba_1	= startAddress >>  8;
        c->lba_0	= startAddress & 0xff;
        c->reserved1	= 0;        
        length = endAddress - startAddress;
        c->len_hi	= length >> 8;
        c->len_lo	= length & 0xff;
        c->ctlbyte	= 0;
        
    } else {					/* absolute time: use PlayAudioMSF */
        
        p = (struct IOAudioPlayMSFcdb *)scsiCDB.cdb;

        p->opcode	= SOP_PLAYAUDIOMSF;
        p->lunbits	= 0;
        p->reserved1	= 0;
        p->start_m	= startAddress >> 16;
        p->start_s	= startAddress >>  8;
        p->start_f	= startAddress & 0xff;
        p->end_m	= endAddress >> 16;
        p->end_s	= endAddress >>  8;
        p->end_f	= endAddress & 0xff;
        p->ctlbyte	= 0;
    }

    scsiCDB.cdbLength = 10;
    req->setCDB( &scsiCDB );
    req->setPointers(cx->senseDataDesc, 255, false, true);    

    req->setPointers( cx->memory, 0, false );
    req->setTimeout( 1000 );

    result = simpleSynchIO(cx);

    deleteContext(cx);

    return(result);
}

IOReturn
IOSCSICDDrive::getAudioStatus(struct audioStatus *status)
{
const UInt8 qBufLen = 16;
    IOReturn result;
    UInt8 *tempBuf;

    /* Get a buffer for the returned data: */
    
    result = allocateTempBuffer(&tempBuf,qBufLen);
    if (result != kIOReturnSuccess) {
        return(kIOReturnNoMemory);
    }

    result = readSubchannel(tempBuf,qBufLen,0,IORSCcdb::kCurrentPosition);

    if ((result == kIOReturnSuccess) && ((tempBuf[5] & 0xf0) == 0x10)) {	/* we got the data */
        status->playMode	= _audioPlayMode;
        status->type		= (trackType)(tempBuf[5] & 0xff);
        status->address		=   tempBuf[ 9] << 16 |
                                    tempBuf[10] <<  8 |
                                    tempBuf[11];
        switch (tempBuf[1]) {			/* convert audio status */


            case 0x11:
                            status->status = kAudioPlayInProgress;
                            break;

            case 0x12:
                            status->status = kHoldTrackMode;
                            break;

            case 0x13:
                            status->status = kAudioPlayCompleted;
                            break;

            case 0x14:
                            status->status = kError;
                            break;

            case 0x15:
                            status->status = kAudioPlayNotRequested;
                            break;

            case 0x00:
            default:
                            status->status = kUnknown;
                            break;
        }
    }
    deleteTempBuffer(tempBuf,qBufLen);

    return(result);
}

const char *
IOSCSICDDrive::getDeviceTypeName(void)
{
    return(kDeviceTypeCDROM);
}

bool
IOSCSICDDrive::init(OSDictionary * properties)
{
    initAudioModes();

    _audioStopAddress.address	= 0L;
    _audioStopAddress.type	= kBlockAddress;

    return(super::init(properties));
}

void
IOSCSICDDrive::initAudioModes(void)
{
    _audioPlayMode = stereo;
    _leftVolume = 0xff;
    _leftPortChannel = playModeToDriveBits(_audioPlayMode,kLeft);
    _rightVolume = 0xff;
    _rightPortChannel = playModeToDriveBits(_audioPlayMode,kRight);
}

IOService *
IOSCSICDDrive::instantiateNub(void)
{
    IOService *nub;

    /* Instantiate a generic CDROM nub so a generic driver can match above us. */
    
    nub = new IOSCSICDDriveNub;
    return(nub);
}

void
IOSCSICDDrive::mediaGone(void)
{
}

/* Convert the audioPlayMode signal routing bits to the drive's bits. */

UInt8
IOSCSICDDrive::playModeToDriveBits(audioPlayMode mode,channel chan)
{
    UInt8 bits;
    int shift;

    bits = 0;

    switch (chan) {

        case kLeft	:	shift = shift_Left_Channel; break;
        case kRight	:	shift = shift_Right_Channel; break;

        default		: 	return(bits);
    }

    if (mode & (signal_Left << shift)) {
        bits |= drive_left_signal;
    }
    if (mode & (signal_Right << shift)) {
        bits |= drive_right_signal;
    }

    return(bits);
}

IOReturn
IOSCSICDDrive::readAudioData(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)
{
    /* what about buffer vs. IOMemoryDescriptor? */
    
    //xxxx
    return(kIOReturnUnsupported);
}

IOReturn
IOSCSICDDrive::readAudioSubcodes(positioningType addressType,cdAddress address,
                                       UInt8 blockCount,UInt8 *buffer)
{
    /* what about buffer vs. IOMemoryDescriptor? */
    
   // xxxx
    return(kIOReturnUnsupported);
}

IOReturn
IOSCSICDDrive::readAudioVolume(UInt8 *leftVolume,UInt8 *rightVolume)
{
    /* Return the cached values. */
    
    *leftVolume		= _leftVolume;
    *rightVolume	= _rightVolume;

    return(kIOReturnSuccess);
}

IOReturn
IOSCSICDDrive::readAudioWithQSubcode(positioningType addressType,cdAddress address,
                                          UInt8 blockCount,UInt8 *buffer)
{
    /* what about buffer vs. IOMemoryDescriptor? */
    
    //xxxx
    return(kIOReturnUnsupported);
}

IOReturn
IOSCSICDDrive::readAudioWithAllSubcodes(positioningType addressType,cdAddress address,
                                          UInt8 blockCount,UInt8 *buffer)
{
    /* what about buffer vs. IOMemoryDescriptor? */
    
    //xxxx
    return(kIOReturnUnsupported);
}

IOReturn
IOSCSICDDrive::readHeader(UInt32 blockAddress,struct headerInfo *buffer)
{
const UInt8 hdrBufLen = 24;
    struct context *cx;
    struct IOReadHeadercdb *c;
    IOSCSICommand *req;
    SCSICDBInfo scsiCDB;
    IOReturn result;
    UInt8 *tempBuf;

    bzero(buffer,sizeof(struct headerInfo));

    /* Get a buffer for the returned data: */
    
    result = allocateTempBuffer(&tempBuf,hdrBufLen);
    if (result != kIOReturnSuccess) {
        return(kIOReturnNoMemory);
    }
    
    cx = allocateContext();
    if (cx == NULL) {
        return(kIOReturnNoMemory);
    }
    
    req = cx->scsireq;

    bzero( &scsiCDB, sizeof(scsiCDB) );

    c = (struct IOReadHeadercdb *)scsiCDB.cdb;

    c->opcode = SOP_READHEADER;
    c->lunbits = 0;
    c->lunbits |= IOReadHeadercdb::kMSF;		/* return results in M:S:F format */
    c->lba_3 = blockAddress >> 24;
    c->lba_2 = blockAddress >> 18;
    c->lba_1 = blockAddress >>  8;
    c->lba_0 = blockAddress & 0xff;        
    c->reserved1 = 0;
    c->len_hi = 0;
    c->len_lo = hdrBufLen;
    c->ctlbyte = 0;
    
    scsiCDB.cdbLength = 10;
    req->setCDB( &scsiCDB );
    req->setPointers(cx->senseDataDesc, 255, false, true);    

    cx->memory = IOMemoryDescriptor::withAddress((void *)tempBuf,
                                                 hdrBufLen,
                                                 kIODirectionIn);
    req->setPointers( cx->memory, hdrBufLen, false );

    req->setTimeout( 1000 );
    
    result = simpleSynchIO(cx);

    deleteContext(cx);

    if (result == kIOReturnSuccess) {		/* return the header info */
        buffer->mode = (dataMode)(tempBuf[0]);
        buffer->address =	tempBuf[4] << 24 ||
                                tempBuf[5] << 16 ||
                                tempBuf[6] <<  8 ||
                                tempBuf[7];
    }
    
    deleteTempBuffer(tempBuf,hdrBufLen);

    return(result);
}

IOReturn
IOSCSICDDrive::readISRC(UInt32 track,UInt8 *buffer,bool *found)
{
    return(readISRCMCN(IORSCcdb::kISRC,track,buffer,found));
}

/* Since the ISRC and MCN are the same size and behave the same, we use common code. */
IOReturn
IOSCSICDDrive::readISRCMCN(UInt8 dataformat,UInt32 track,UInt8 *buffer,bool *found)
{
const UInt8 mcnISRCBufLen = 24;
    IOReturn result;
    UInt8 *tempBuf;

    *found = false;			/* assume no ISRC/MCN will be found */
    bzero(buffer,mcnISRCBufLen);
    
    /* Get a buffer for the returned data: */
    
    result = allocateTempBuffer(&tempBuf,mcnISRCBufLen);
    if (result != kIOReturnSuccess) {
        return(kIOReturnNoMemory);
    }

    result = readSubchannel(tempBuf,mcnISRCBufLen,track,dataformat);

    if ((result == kIOReturnSuccess) && (tempBuf[8] & 0x80)) {	/* return the ISRC/MCN */
        bcopy(buffer,&tempBuf[9],15);
        *found = true;
    }
    
    deleteTempBuffer(tempBuf,mcnISRCBufLen);

    return(result);
}

IOReturn
IOSCSICDDrive::readMCN(UInt8 *buffer,bool *found)
{
    return(readISRCMCN(IORSCcdb::kMCN,0,buffer,found));		/* any track will do */
}


IOReturn
IOSCSICDDrive::readQSubcodes(struct qSubcodeTocInfo *buffer,UInt32 bufSize)
{
    /* what about buffer vs. IOMemoryDescriptor? */
    
    //xxxx
    return(kIOReturnUnsupported);
}

IOReturn
IOSCSICDDrive::readSubcodeBuffer(UInt8 *buffer,bool purge,UInt32 entryCount)
{
    /* what about buffer vs. IOMemoryDescriptor? */
    
    //xxxx
    return(kIOReturnUnsupported);
}

IOReturn
IOSCSICDDrive::readSubchannel(UInt8 *buffer,UInt32 length,UInt8 track,UInt8 dataFormat)
{
    struct context *cx;
    struct IORSCcdb *c;
    IOSCSICommand *req;
    SCSICDBInfo scsiCDB;
    IOReturn result;

    cx = allocateContext();
    if (cx == NULL) {
        return(kIOReturnNoMemory);
    }
    
    req = cx->scsireq;

    bzero( &scsiCDB, sizeof(scsiCDB) );

    bzero(buffer,length);
    
    c = (struct IORSCcdb *)(scsiCDB.cdb);

    c->opcode = SOP_READSUBCHANNEL;
    c->lunbits = 0;
    c->lunbits |= IORSCcdb::kMSF;
    c->subq = IORSCcdb::kSubq;
    c->dataformat = dataFormat;
    c->track = track;			/* any valid track will do */
    c->reserved1 = 0;
    c->reserved2 = 0;
    c->len_hi = length >> 8;
    c->len_lo = length & 0xff;
    c->ctlbyte = 0;
    
    scsiCDB.cdbLength = 10;
    req->setCDB( &scsiCDB );
    req->setPointers(cx->senseDataDesc, 255, false, true);    

    cx->memory = IOMemoryDescriptor::withAddress((void *)buffer,
                                                 length,
                                                 kIODirectionIn);
    req->setPointers( cx->memory, length, false );

    req->setTimeout( 1000 );
    
    result = simpleSynchIO(cx);

    deleteContext(cx);

    return(result);
}

IOReturn
IOSCSICDDrive::readTheQSubcode(struct qSubcode *buffer)
{
const UInt8 qBufLen = 16;
    IOReturn result;
    UInt8 *tempBuf;

    bzero(buffer,sizeof(struct qSubcode));
    
    /* Get a buffer for the returned data: */
    
    result = allocateTempBuffer(&tempBuf,qBufLen);
    if (result != kIOReturnSuccess) {
        return(kIOReturnNoMemory);
    }

    result = readSubchannel(tempBuf,qBufLen,0, IORSCcdb::kCurrentPosition);

    if ((result == kIOReturnSuccess) && ((tempBuf[5] & 0xf0) == 0x10)) {	/* we got the data */
        buffer->type		= (trackType)(tempBuf[ 5] & 0x0f);
        buffer->track		= tempBuf[ 6];
        buffer->index		= tempBuf[ 7];
        buffer->absAddress	=   tempBuf[ 9] << 16 |
                                    tempBuf[10] <<  8 |
                                    tempBuf[11];
        buffer->relAddress	=   tempBuf[13] << 16 |
                                    tempBuf[14] <<  8 |
                                    tempBuf[15];
    }
    
    deleteTempBuffer(tempBuf,qBufLen);

    return(result);
}

IOReturn
IOSCSICDDrive::readTOC(struct rawToc *buffer,UInt32 length,tocFormat format)
{
    struct context *cx;
    struct IOReadToccdb *c;
    IOSCSICommand *req;
    SCSICDBInfo scsiCDB;
    IOReturn result;

    cx = allocateContext();
    if (cx == NULL) {
        return(kIOReturnNoMemory);
    }
    
    req = cx->scsireq;

    bzero( &scsiCDB, sizeof(scsiCDB) );

    c = (struct IOReadToccdb *)scsiCDB.cdb;

    c->opcode = SOP_READTOC;
    c->lunbits = 0;
    if (format == ktocSCSI2MSF) {
        c->lunbits |= IOReadToccdb::kMSF;
    }
    c->reserved1 = 0;
    c->reserved2 = 0;
    c->reserved3 = 0;
    c->reserved4 = 0;
    c->len_hi = length >> 8;
    c->len_lo = length & 0xff;
    c->ctlbyte = 0;
    
    switch (format) {
        case ktocSCSI2MSF :
        case ktocSCSI2LBA :
                                c->ctlbyte |= IOReadToccdb::kSCSI2;
                                break;

        case ktocSessionInfo :
                                c->ctlbyte |= IOReadToccdb::kSessionInfo;
                                break;

        case ktocQLeadin :
                                c->ctlbyte |= IOReadToccdb::kQLeadIn;
                                break;
    }
    
    scsiCDB.cdbLength = 10;
    req->setCDB( &scsiCDB );
    req->setPointers(cx->senseDataDesc, 255, false, true);    

    cx->memory = IOMemoryDescriptor::withAddress((void *)buffer,
                                                 length,
                                                 kIODirectionIn);
    req->setPointers( cx->memory, length, false );

    req->setTimeout( 1000 );    

    result = simpleSynchIO(cx);

    if ( result == kIOReturnUnderrun ) result = kIOReturnSuccess; 

    deleteContext(cx);

    return(result);
}

/* We track media changes so we can:
 *  - set default audio playMode and volume
 */

IOReturn
IOSCSICDDrive::reportMediaState(bool *mediaPresent,bool *changed)
{
    IOReturn result;
    IOReturn internalResult;

    result = super::reportMediaState(mediaPresent,changed);

/**
    IOLog("%s[IOSCSICDDrive]::reportMediaState; result=%s, changed = %s, present = %s\n",
                getName(),stringFromReturn(result),*changed ? "Y" : "N", *mediaPresent ? "Y" : "N");
**/
    
    if ((result == kIOReturnSuccess) && *changed) {		/* the media state changed */
        if (*mediaPresent) {				/* new media inserted */
            internalResult = setDefaultAudioModes();
        } else {					/* media went away */
            mediaGone();
        }
    }

    /* We don't return the result of our internal operations. But since they
     * indicate a problem, we probably should report some kind of problem,
     * or maybe just ignore the media change.
     */

    return(result);
}

IOReturn
IOSCSICDDrive::seek(UInt32 lba)
{
    struct context *cx;
    struct IOSeekcdb *c;
    IOSCSICommand *req;
    SCSICDBInfo scsiCDB;
    IOReturn result;

    cx = allocateContext();
    if (cx == NULL) {
        return(kIOReturnNoMemory);
    }
    
    req = cx->scsireq;

    bzero( &scsiCDB, sizeof(scsiCDB) );

    c = (struct IOSeekcdb *)(scsiCDB.cdb);

    c->opcode = SOP_SEEK;
    c->lunbits = 0;
    c->lba_3 = lba >> 24;
    c->lba_2 = lba >> 16;
    c->lba_1 = lba >>  8;
    c->lba_0 = lba & 0xff;
    c->reserved1 = 0;
    c->reserved2 = 0;
    c->reserved3 = 0;
    c->ctlbyte = 0;

    scsiCDB.cdbLength = 10;
    req->setCDB( &scsiCDB );
    req->setPointers(cx->senseDataDesc, 255, false, true);    

    req->setPointers( cx->memory, 0, false );

    req->setTimeout( 5000 );

    result = simpleSynchIO(cx);

    deleteContext(cx);

    return(result);
}

IOReturn
IOSCSICDDrive::setAudioStopAddress(positioningType addressType,cdAddress address)
{
    /* We have to stash the stop address locally, because the client often sets
     * the stop address before providing the start address. We can't send the
     * stop address to the drive without a valid start address, because the pickup
     * could be currently over a data track, which would return an error.
     */

    _audioStopAddress.type	= addressType;
    _audioStopAddress.address	= address;

    /* If the address is for block=0, we must stop any audio play in progress. */
    
    if (addressType == kBlockAddress && address == 0L) {	/* stop play */
        return(doAudioPlayCommand(kAbsoluteTime,0xffffff,kAbsoluteTime,0));
    } else {							/* set new stop addr */
        return(doAudioPlayCommand(kAbsoluteTime,0xffffff,addressType,address));
    }
}

IOReturn
IOSCSICDDrive::setDefaultAudioModes(void)
{
    initAudioModes();
    return(setVolume(_leftVolume,_rightVolume));
}

struct audioPage {
    UInt8 pagecode;
    UInt8 paramlen;
    UInt8 immed;
    UInt8 reserved[5];
    UInt8 leftportchannel;
    UInt8 leftvolume;
    UInt8 rightportchannel;
    UInt8 rightvolume;
    UInt8 port2channel;
    UInt8 port2volume;
    UInt8 port3channel;
    UInt8 port3volume;
};

IOReturn
IOSCSICDDrive::setVolume(UInt8 leftVolume,UInt8 rightVolume)
{
    struct context *cx;
    struct IOModeSelectcdb *c;
    IOSCSICommand *req;
    SCSICDBInfo scsiCDB;
    IOReturn result;
    struct audioPage *page;

    cx = allocateContext();
    if (cx == NULL) {
        return(kIOReturnNoMemory);
    }
    
    req = cx->scsireq;

    bzero( &scsiCDB, sizeof(scsiCDB) );

    /* Set up a Mode Select for page 0x0e. We set volume and audioPlay mode. */
    
    result = allocateTempBuffer((UInt8 **)(&page),sizeof(struct audioPage));
    if (result != kIOReturnSuccess) {
        deleteContext(cx);
        return(kIOReturnNoMemory);
    }

    bzero(page,sizeof(struct audioPage)),
    page->pagecode = 0x0e;
    page->paramlen = 0x0e;

    page->leftportchannel = playModeToDriveBits(_audioPlayMode,kLeft);
    page->leftvolume = leftVolume;
    page->rightportchannel = playModeToDriveBits(_audioPlayMode,kRight);
    page->rightvolume = rightVolume;
    
    c = (struct IOModeSelectcdb *)(scsiCDB.cdb);

    c->opcode = SOP_MODESELECT;
    c->lunbits = 0;
    c->reserved1 = 0;
    c->reserved2 = 0;
    c->paramlen = sizeof(struct audioPage);   
    c->ctlbyte = 0;
    
//    IOLog("%s[IOSCSICDDrive::setVolume\n",getName());
    
    scsiCDB.cdbLength = 6;
    req->setCDB( &scsiCDB );
    req->setPointers(cx->senseDataDesc, 255, false, true);    

    cx->memory = IOMemoryDescriptor::withAddress((void *)page,
                                                 c->paramlen,
                                                 kIODirectionOut);
    req->setPointers( cx->memory, c->paramlen, true );

    req->setTimeout( 1000 );

    result = simpleSynchIO(cx);

    deleteTempBuffer((UInt8 *)page,sizeof(struct audioPage));
    deleteContext(cx);

    if (result == kIOReturnSuccess) {	/* only remember settings if they worked */
        _leftVolume = leftVolume;
        _rightVolume = rightVolume;
    }
    
    return(result);
}

