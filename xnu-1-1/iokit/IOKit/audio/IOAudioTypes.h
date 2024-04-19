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

/*!
 * @header AudioTypes 
 * Common include file for data types used by Audio clients and device drivers.
 */

#ifndef _IOAUDIOTYPES_H
#define _IOAUDIOTYPES_H

/*!
 * @enum IOAudioMem
 * @constant kSampleBuffer
 * @constant kStatus
 * @constant kMixBuffer
*/
typedef enum _IOAudioMem {
    kSampleBuffer = 0,
    kStatus = 1,
    kMixBuffer = 2
} IOAudioMem;

/*!
 * @enum IOAudioCalls
 * @constant kCallSetFlow
 * @constant kCallFlush
 * @constant kCallSetErase
 * @constant kNumCalls
 */
typedef enum _IOAudioCalls {
    kCallSetFlow = 0,
    kCallFlush = 1,
    kCallSetErase = 2,
    kNumCalls
} IOAudioCalls;

/*!
 * @enum IOAudioNotifications
 * @constant kIOAudioInputNotification
 */
typedef enum _IOAudioNotifications {
 kIOAudioInputNotification = 12
} IOAudioNotifications;

/*!
 * @typedef IOAudioStreamPosition
 * @abstract Represents a position in an audio stream based on the block within a loop, and the loop count
 * @field fBlock The current block
 * @field fLoopCount The number of times the ring buffer has looped
 */
typedef struct _IOAudioStreamPosition
{
    UInt32	fBlock;
    UInt32	fLoopCount;
} IOAudioStreamPosition;

/*!
 * @typedef IOAudioStreamStatus
 * @abstract Shared-memory structure giving stream status
 * @discussion !!! WARNING !!! Subject to change.
 *  This structure is used for both play and record streams, most driver code
 *  doesn't care which way data is flowing. The only clue here is fErases.
 * @field fVersion Indicates version of this structure
 * @field fErases Should the driver zero the buffer after playing it?
 * @field fRunning Positive if stream is playing/recording
 * @field fConnections Number of users
 * @field fBufSize Size of mapped buffer
 * @field fBlockSize Size of each block transferred to/from hardware
 * @field fNumBlocks Number of blocks
 * @field fSampleSize Bytes per sample
 * @field fChannels Channels in stream (eg. 2 for stereo)
 * @field fDataRate Bytes per second (sample rate * sample size * channels)
 * @field fCurrentBlock Block currently being transferred by hardware
 * @field fCurrentLoopCount Counts times around the ring buffer
 * @field fLastLoopTime Timestamp of the last time the ring buffer wrapped
 * @field fEraseHeadBlock Block location of the erase head - erased up to but not including the block
 * @field fFlushEnd The ending block after which the sound engine will be stopped if no clients exist
 * @field fMixBufferInUse True if the mix buffer is in use
 */

typedef struct _IOAudioStreamStatus {
    UInt32				fVersion;	// Indicates version of this structure
    int					fErases;	// Should the driver zero the buffer after playing it? 
    int					fRunning;	// Positive if stream is playing/recording
    int					fConnections;	// Number of users
    int					fBufSize;	// Size of mapped buffer
    int					fBlockSize;	// Size of each block transferred to/from hardware
    int					fNumBlocks;	// Number of blocks
    int					fSampleSize;	// Bytes per sample
    int					fChannels;	// Channels in stream (eg. 2 for stereo)
    int					fDataRate;	// Bytes per second (sample rate * sample size * channels)
    volatile UInt32			fCurrentBlock;	// Block currently being transferred by hardware
    volatile UInt32			fCurrentLoopCount; // Counts times around the ring buffer
    volatile AbsoluteTime		fLastLoopTime;	// Timestamp of the last time the ring buffer wrapped
    volatile UInt32			fEraseHeadBlock;// Block location of the erase head
                                                        // erased up to but not including the block
    volatile IOAudioStreamPosition	fFlushEnd;	// The ending block after which the sound engine
                                                        // will be stopped if no clients exist
    Boolean				fMixBufferInUse;// true if mix buffer is in use
} IOAudioStreamStatus;

/*!
 * @defined MIX_BUFFER_SAMPLE_SIZE
 */

#define MIX_BUFFER_SAMPLE_SIZE	sizeof(float)

/*!
 * @struct IOAudioNotifyMsg
 * @field h
 * @field refCon
 */
typedef struct _notifyMsg {
    mach_msg_header_t h;
    UInt32	      refCon;
} IOAudioNotifyMsg;

#endif /* _IOAUDIOTYPES_H */
