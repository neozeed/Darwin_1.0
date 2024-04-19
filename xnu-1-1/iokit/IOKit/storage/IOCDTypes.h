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
/* IOCDTypes.h created by rick on Wed 07-Apr-1999 */

#ifndef	_IOCDTYPES_H
#define	_IOCDTYPES_H

#include <IOKit/IOTypes.h>

enum positioningType {
    kBlockAddress	= 0,
    kAbsoluteTime	= 1,		/* mm:ss:ff in BCD */
    kTrackNumber	= 2,
    kPlayListIndex	= 3
};

typedef UInt32 cdAddress;		/* content depends on positioningType */

struct qualifiedCDAddress {
    cdAddress		address;
    positioningType	type;
};

/* Track type bits: */

const UInt8 tt_preEmphasis	= 0x01;
const UInt8 tt_copyPermit	= 0x02;
const UInt8 tt_twoChannel	= 0x00;
const UInt8 tt_data		= 0x04;
const UInt8 tt_fourChannel	= 0x08;

/* Allowable track types (combinations of track type bits): */

enum trackType {
    audio2		= tt_twoChannel,
    audio2PreEmphasis	= audio2 | tt_preEmphasis,
    audio2Copyable	= audio2 | tt_copyPermit,
    audio2PreCopyable	= audio2PreEmphasis | tt_copyPermit,
    data		= tt_data,
    dataCopyable	= data | tt_copyPermit,
    audio4		= tt_fourChannel,
    audio4PreEmphasis	= audio4 | tt_preEmphasis,
    audio4Copyable	= audio4 | tt_copyPermit,
    audio4PreCopyable	= audio4PreEmphasis | tt_copyPermit,
};

struct trackTypeInfo {
    trackType	type;
    cdAddress	address;	/* in mm:ss:ff format */
};

struct sessionInfo {
    UInt32	firstSessionNumber;	/* in BCD */
    UInt32	lastSessionNumber;	/* in BCD */

    UInt32	trackNumber;		/* first track of last session */

    struct trackTypeInfo info;		/* info about that track */
};

/* TOC format structures: */

/* kSCSI2xxx formats are a tocHeader followed by zero or more tocTrackDescriptors */
struct tocHeader {
    UInt8	len_hi;
    UInt8	len_lo;
    UInt8	firstTrack;
    UInt8	lastTrack;
};

struct tocTrackDescriptor {
    UInt8	reserved1;
    UInt8	adrControl;	/* Qsub status and trackType */
    UInt8	track;		/* track number */
    UInt8	reserved2;
    UInt8	lba_3;		/* msb */
    UInt8	lba_2;
    UInt8	lba_1;
    UInt8	lba_0;		/* lsb */
};

struct rawToc {
    struct tocHeader		header;
    struct tocTrackDescriptor	descriptors[1 + 1 + 1 + 100];	/* A0, A1, A2, 100 tracks */
};

enum tocFormat {
    ktocSCSI2MSF,
    ktocSCSI2LBA,
    ktocSessionInfo,
    ktocQLeadin
};

struct macTocEntry {		/* Note: doesn't match MacOS 5-byte entry! */
    UInt8	trackPoint;		/* track no. or point */
    trackType	type;			/* from Control Field */
    UInt8	pMin;
    UInt8	pSec;
    UInt8	pFrame;
};

struct macEntireToc {		/* Note: larger than MacOS 512-byte structure! */
    struct macTocEntry entries[1+1+1+99];	/* A0, A1, A2, tracks 1-99 */
};

struct qSubcodeTocInfo {
    UInt16	totalBytesTransferred;
    UInt8	firstSessionNumber;	/* in BCD */
    UInt8	lastSessionNumber;	/* in BCD */

    struct qSubcodeToc {
        UInt8	session;
        UInt8	adrType;		/* Q-sub mode in 0xf0, type in 0x0f bits */
        UInt8	track;
        UInt8	point;
        UInt8	min;
        UInt8	sec;
        UInt8	frame;
        UInt8	zero;
        UInt8	pmin;
        UInt8	psec;
        UInt8	pframe;      
    } qSubcodes [0];				/* open-ended array */

};

struct qSubcode {
    trackType	type;
    UInt8	track;
    UInt8	index;
    cdAddress	relAddress;		/* M:S:F */
    cdAddress	absAddress;		/* M:S:F */
};

enum dataMode {
    kAllBytesZero	= 0,
    kUserData		= 1,
    kUser		= 2
};
struct headerInfo {
    cdAddress	address;	/* in mm:ss:ff format */
    dataMode	mode;
};

/* Audio play mode bits. The play mode consists of a four-bit mask in the following
 * format:
 *
 *  left channel   right channel
 * +------+------+------+------+
 * |  LS     RS  |  LS     RS  |
 * +------+------+------+------+
 *
 * The leftmost bit of the channel pair controls the left-channel SIGNAL routing to
 * the channel output; the rightmost bit controls routing of the right-channel signal.
 * Examples:
 *	VALUE	LEFT OUTPUT	RIGHT OUTPUT
 *	----	------------	------------
 *	1000	left signal    	none (muted)
 *	0001	none (muted)	right signal
 *	0100	right signal	none (muted)
 *	0010	none (muted)	left signal
 *	1001	left signal	right signal	(STEREO)
 *	0110	right signal	left signal	(reversed stereo)
 *	1111	both signals	both signals	(mono both sides)
 *
 * Though all 16 combinations are valid, not all work for all drives.
 */

const UInt8 signal_Right	= 0x01;
const UInt8 signal_Left		= 0x02;
const UInt8 shift_Right_Channel	= 0;
const UInt8 shift_Left_Channel	= 2;

typedef UInt8 audioPlayMode;

const UInt8 stereo		= 0x09;
const UInt8 mono		= 0x0f;

enum playMode {
    kNormalMode		= 0,
    kShuffleMode	= 1,
    kProgMode		= 2
};

enum playStatus {
    kAudioPlayInProgress	= 0,
    kHoldTrackMode		= 1,
    kMuted			= 2,
    kAudioPlayCompleted		= 3,
    kError			= 4,
    kAudioPlayNotRequested	= 5,
    kUnknown			= 6
};

struct audioStatus {
    playStatus		status;
    audioPlayMode	playMode;
    trackType		type;
    cdAddress		address;	/* current Q-subcode address in hh:mm:ff */
};
#endif
