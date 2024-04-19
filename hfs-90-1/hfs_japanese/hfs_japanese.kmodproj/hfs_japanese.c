/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <mach/mach_types.h>
#include <mach/kmod.h>

#include <MacTypes.h>
#include <CFStringEncodingExt.h>

#include <hfs/hfs_encodings.h>


#define kCFStringEncodingUseCanonical (1 << 5)  /* Always use canonical form */

extern UInt32 __CFToMacJapanese(UInt32 flags, const UniChar *characters,
		UInt32 numChars, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen);

extern UInt32 __CFFromMacJapanese(UInt32 flags, const UInt8 *bytes, UInt32 numBytes,
		UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen);


int
MacJapaneseToUnicode(Str31 hfs_str, UniChar *uni_str, UniCharCount maxCharLen, UniCharCount *usedCharLen)
{
	UInt32 processedChars;

	processedChars = __CFFromMacJapanese(kCFStringEncodingUseCanonical,
					&hfs_str[1],
					hfs_str[0],
					uni_str,
					maxCharLen,
					usedCharLen);
	return (0);
}

int
UnicodeToMacJapanese(UniChar *uni_str, UniCharCount unicodeChars, Str31 hfs_str)
{
	UniCharCount srcCharsUsed;
	UInt32 usedByteLen = 0;

	srcCharsUsed = __CFToMacJapanese(kCFStringEncodingUseCanonical,
					uni_str,
					unicodeChars,
					(UInt8*)&hfs_str[1],
					sizeof(Str31) - 1,
					&usedByteLen);

	hfs_str[0] = usedByteLen;

	if (srcCharsUsed == unicodeChars)
		return (0);
	else
		return (-1);
}


__private_extern__ int
hfs_japanese_start(kmod_info_t *ki, void *data)
{
	int result;

	printf("loading hfs_japanese encoding converter into kernel...\n");

	result = hfs_addconverter(ki->id, kCFStringEncodingMacJapanese,
			MacJapaneseToUnicode, UnicodeToMacJapanese);

	return (result == 0 ? KERN_SUCCESS : KERN_FAILURE);
}

__private_extern__ int
hfs_japanese_stop(kmod_info_t *ki, void *data)
{
	int result;

	printf("unloading hfs_japanese encoding converter from kernel...\n");

	result = hfs_remconverter(ki->id, kCFStringEncodingMacJapanese);

	return (result == 0 ? KERN_SUCCESS : KERN_FAILURE);
}



