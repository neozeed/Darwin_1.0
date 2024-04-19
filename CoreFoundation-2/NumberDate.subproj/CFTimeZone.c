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
/*	CFTimeZone.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include "CFInternal.h"
#include <CoreFoundation/CFTimeZone.h>
#include <CoreFoundation/CFString.h>
#include <math.h>
#include <limits.h>

static CFTimeZoneRef __CFSystemTimeZone = NULL;
static CFTimeZoneRef __CFDefaultTimeZone = NULL;
static CFDictionaryRef __CFTimeZoneAbbreviationDict = NULL;
static CFArrayRef __CFKnownTimeZoneList = NULL;
static UInt32 __CFGlobalTZLock1 = 0;

CF_INLINE void __CFGlobalTZLock(void) {
    __CFSpinLock(&__CFGlobalTZLock1);
}

CF_INLINE void __CFGlobalTZUnlock(void) {
    __CFSpinUnlock(&__CFGlobalTZLock1);
}

typedef struct _CFTZPeriod {
    SInt32 startSec;
    CFStringRef abbrev;
    UInt32 info;
} CFTZPeriod;

struct __CFTimeZone {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFStringRef _name;		/* immutable */
    CFDataRef _data;		/* immutable */
    CFTZPeriod *_periods;	/* immutable */
    SInt32 _periodCnt;		/* immutable */
};

/* startSec is the whole integer seconds from a CFAbsoluteTime, giving dates
between 1933 and 2069; info outside these years is discarded on read-in */
/* Bit 31 of the info is used for the is-DST state */
/* Bits 30-17 of the info are unused */
/* Bits 16-0 of the info are used for the signed offset in seconds from GMT */

CF_INLINE void __CFTZPeriodInit(CFTZPeriod *period, SInt32 startTime, CFStringRef abbrev, SInt32 offset, Boolean isDST) {
    period->startSec = startTime;
    if (abbrev) {
	period->abbrev = CFRetain(abbrev);
    } else {
	abbrev = NULL;
    }
    __CFBitfieldSetValue(period->info, 16, 0, offset);
    __CFBitfieldSetValue(period->info, 31, 31, (isDST ? 1 : 0));
}

CF_INLINE SInt32 __CFTZPeriodStartSeconds(const CFTZPeriod *period) {
    return period->startSec;
}

CF_INLINE CFStringRef __CFTZPeriodAbbreviation(const CFTZPeriod *period) {
    return period->abbrev;
}

CF_INLINE SInt32 __CFTZPeriodGMTOffset(const CFTZPeriod *period) {
    return (SInt32)__CFBitfieldValue(period->info, 16, 0);
}

CF_INLINE Boolean __CFTZPeriodIsDST(const CFTZPeriod *period) {
    return (Boolean)__CFBitfieldValue(period->info, 31, 31);
}

static CFComparisonResult __CFCompareTZPeriods(const void *val1, const void *val2, void *context) {
    CFTZPeriod *tzp1 = (CFTZPeriod *)val1;
    CFTZPeriod *tzp2 = (CFTZPeriod *)val2;
#warning CF: possibly this should be less than or equal to
    if (__CFTZPeriodStartSeconds(tzp1) < __CFTZPeriodStartSeconds(tzp2)) return kCFCompareLessThan;
    if (__CFTZPeriodStartSeconds(tzp2) < __CFTZPeriodStartSeconds(tzp1)) return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

CF_INLINE SInt32 __CFBSearchTZPeriods(CFTimeZoneRef tz, CFAbsoluteTime at) {
    CFTZPeriod elem;
    __CFTZPeriodInit(&elem, (SInt32)floor(at), NULL, 0, FALSE);
    return CFBSearch(&elem, sizeof(CFTZPeriod), tz->_periods, tz->_periodCnt, __CFCompareTZPeriods, NULL);
}

/*
** Each time zone data file begins with. . .
*/

struct tzhead {
	char	tzh_reserved[20];	/* reserved for future use */
	char	tzh_ttisgmtcnt[4];	/* coded number of trans. time flags */
	char	tzh_ttisstdcnt[4];	/* coded number of trans. time flags */
	char	tzh_leapcnt[4];		/* coded number of leap seconds */
	char	tzh_timecnt[4];		/* coded number of transition times */
	char	tzh_typecnt[4];		/* coded number of local time types */
	char	tzh_charcnt[4];		/* coded number of abbr. chars */
};

/*
** . . .followed by. . .
**
**	tzh_timecnt (char [4])s		coded transition times a la time(2)
**	tzh_timecnt (UInt8)s	types of local time starting at above
**	tzh_typecnt repetitions of
**		one (char [4])		coded GMT offset in seconds
**		one (UInt8)	used to set tm_isdst
**		one (UInt8)	that's an abbreviation list index
**	tzh_charcnt (char)s		'\0'-terminated zone abbreviations
**	tzh_leapcnt repetitions of
**		one (char [4])		coded leap second transition times
**		one (char [4])		total correction after above
**	tzh_ttisstdcnt (char)s		indexed by type; if 1, transition
**					time is standard time, if 0,
**					transition time is wall clock time
**					if absent, transition times are
**					assumed to be wall clock time
**	tzh_ttisgmtcnt (char)s		indexed by type; if 1, transition
**					time is GMT, if 0,
**					transition time is local time
**					if absent, transition times are
**					assumed to be local time
*/

CF_INLINE SInt32 __CFDetzcode(const char *bufp) {
    SInt32 result = (bufp[0] & 0x80) ? ~0L : 0L;
    result = (result << 8) | (bufp[0] & 0xff);
    result = (result << 8) | (bufp[1] & 0xff);
    result = (result << 8) | (bufp[2] & 0xff);
    result = (result << 8) | (bufp[3] & 0xff);
    return result;
}

CF_INLINE void __CFEntzcode(SInt32 value, char *bufp) {
    bufp[0] = (value >> 24) & 0xff;
    bufp[1] = (value >> 16) & 0xff;
    bufp[2] = (value >> 8) & 0xff;
    bufp[3] = (value >> 0) & 0xff;
}

static Boolean __CFParseTimeZoneData(CFAllocatorRef allocator, CFDataRef data, CFTZPeriod **tzpp, SInt32 *cntp) {
    SInt32 len, timecnt, typecnt, charcnt, idx, cnt;
    const char *p, *timep, *typep, *ttisp, *charp;
    CFStringRef *abbrs;
    Boolean result = TRUE;

    p = CFDataGetBytePtr(data);
    len = CFDataGetLength(data);
    if (len < sizeof(struct tzhead)) {
	return FALSE;
    }
    p += 20 + 4 + 4 + 4;	/* skip reserved, ttisgmtcnt, ttisstdcnt, leapcnt */
    timecnt = __CFDetzcode(p);
    p += 4;
    typecnt = __CFDetzcode(p);
    p += 4;
    charcnt = __CFDetzcode(p);
    p += 4;
    if (typecnt <= 0 || timecnt < 0 || charcnt < 0) {
	return FALSE;
    }
    if (len - sizeof(struct tzhead) < (4 + 1) * timecnt + (4 + 1 + 1) * typecnt + charcnt) {
	return FALSE;
    }
    timep = p;
    typep = timep + 4 * timecnt;
    ttisp = typep + timecnt;
    charp = ttisp + (4 + 1 + 1) * typecnt;
    cnt = (0 < timecnt) ? timecnt : 1;
    *tzpp = CFAllocatorAllocate(allocator, cnt * sizeof(CFTZPeriod), 0);
    CFZeroMemory(*tzpp, cnt * sizeof(CFTZPeriod));
    abbrs = CFAllocatorAllocate(allocator, (charcnt + 1) * sizeof(CFStringRef), 0);
    for (idx = 0; idx < charcnt + 1; idx++) {
	abbrs[idx] = NULL;
    }
    for (idx = 0; idx < cnt; idx++) {
	CFAbsoluteTime at;
	SInt32 time;
	UInt8 type, dst, abbridx;
	SInt32 offset;

	at = (CFAbsoluteTime)__CFDetzcode(timep) - kCFAbsoluteTimeIntervalSince1970;
	if (at < (CFAbsoluteTime)INT_MIN || (CFAbsoluteTime)INT_MAX < at) {
	    timep += 4;
	    typep++;
	    cnt--;
	    idx--;
	    continue;
	}
	time = (0 == timecnt) ? INT_MIN : (SInt32)at;
	timep += 4;	/* harmless if 0 == timecnt */
	type = (0 < timecnt) ? (UInt8)*typep++ : 0;
	if (typecnt <= type) {
	    result = FALSE;
	    break;
	}
	offset = __CFDetzcode(ttisp + 6 * type);
	dst = (UInt8)*(ttisp + 6 * type + 4);
	if (0 != dst && 1 != dst) {
	    result = FALSE;
	    break;
	}
	abbridx = (UInt8)*(ttisp + 6 * type + 5);
	if (charcnt < abbridx) {
	    result = FALSE;
	    break;
	}
	if (NULL == abbrs[abbridx]) {
	    abbrs[abbridx] = CFStringCreateWithCString(allocator, &charp[abbridx], CFStringGetSystemEncoding());
	}
	__CFTZPeriodInit(*tzpp + idx, time, abbrs[abbridx], offset, (dst ? TRUE : FALSE));
    }
    for (idx = 0; idx < charcnt + 1; idx++) {
	if (NULL != abbrs[idx]) {
	    CFRelease(abbrs[idx]);
	}
    }
    CFAllocatorDeallocate(allocator, abbrs);
    if (result) {
	CFQSortArray(*tzpp, cnt, sizeof(CFTZPeriod), __CFCompareTZPeriods, NULL);
	*cntp = cnt;
    } else {
	CFAllocatorDeallocate(allocator, *tzpp);
    }
    return result;
}

Boolean __CFTimeZoneEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFTimeZoneRef tz1 = (CFTimeZoneRef)cf1;
    CFTimeZoneRef tz2 = (CFTimeZoneRef)cf2;
    if (!CFEqual(CFTimeZoneGetName(tz1), CFTimeZoneGetName(tz2))) return FALSE;
    if (!CFEqual(CFTimeZoneGetData(tz1), CFTimeZoneGetData(tz2))) return FALSE;
    return TRUE;
}

CFHashCode __CFTimeZoneHash(CFTypeRef cf) {
    CFTimeZoneRef tz = (CFTimeZoneRef)cf;
    return CFHash(CFTimeZoneGetName(tz));
}

CFStringRef __CFTimeZoneCopyDescription(CFTypeRef cf) {
    CFTimeZoneRef tz = (CFTimeZoneRef)cf;
    CFStringRef result, abbrev;
    CFAbsoluteTime at;
    at = CFAbsoluteTimeGetCurrent();
    abbrev = CFTimeZoneCopyAbbreviation(tz, at);
    result = CFStringCreateWithFormat(tz->_allocator, NULL, CFSTR("<CFTimeZone 0x%x [0x%x]>{name = %@; abbreviation = %@; GMT offset = %d; is DST = %s}"), (UInt32)cf, (UInt32)tz->_allocator, tz->_name, abbrev, CFTimeZoneGetSecondsFromGMT(tz, at), CFTimeZoneIsDaylightSavingTime(tz, at) ? "TRUE" : "FALSE");
    CFRelease(abbrev);
    return result;
}

CFAllocatorRef __CFTimeZoneGetAllocator(CFTypeRef cf) {
    CFTimeZoneRef tz = (CFTimeZoneRef)cf;
    return tz->_allocator;
}

void __CFTimeZoneDeallocate(CFTypeRef cf) {
    CFTimeZoneRef tz = (CFTimeZoneRef)cf;
    CFAllocatorRef allocator = tz->_allocator;
    CFIndex idx;
    if (tz->_name) CFRelease(tz->_name);
    if (tz->_data) CFRelease(tz->_data);
    for (idx = 0; idx < tz->_periodCnt; idx++) {
	if (tz->_periods[idx].abbrev) CFRelease(tz->_periods[idx].abbrev);
    }
    if (tz->_periods) CFAllocatorDeallocate(allocator, tz->_periods);
    CFAllocatorDeallocate(allocator, (void *)tz);
    CFRelease(allocator);
}

CFTypeID CFTimeZoneGetTypeID(void) {
    return __kCFTimeZoneTypeID;
}

static CFTimeZoneRef __CFTimeZoneCreateSystem(void) {
#warning CF: unimplemented
}

CFTimeZoneRef CFTimeZoneCopySystem(void) {
    CFTimeZoneRef tz;
    __CFGlobalTZLock();
    if (NULL == __CFSystemTimeZone) {
	__CFSystemTimeZone = __CFTimeZoneCreateSystem();
    }
    tz = CFRetain(__CFSystemTimeZone);
    __CFGlobalTZUnlock();
    return tz;
}

void CFTimeZoneResetSystem(void) {
    __CFGlobalTZLock();
    if (__CFDefaultTimeZone == __CFSystemTimeZone) {
	if (__CFDefaultTimeZone) CFRelease(__CFDefaultTimeZone);
	__CFDefaultTimeZone = NULL;
    }
    if (__CFSystemTimeZone) CFRelease(__CFSystemTimeZone);
    __CFSystemTimeZone = NULL;
    __CFGlobalTZUnlock();
}

CFTimeZoneRef CFTimeZoneCopyDefault(void) {
    CFTimeZoneRef tz;
    __CFGlobalTZLock();
    if (NULL == __CFDefaultTimeZone) {
	__CFDefaultTimeZone = CFTimeZoneCopySystem();
    }
    tz = CFRetain(__CFDefaultTimeZone);
    __CFGlobalTZUnlock();
    return tz;
}

void CFTimeZoneSetDefault(CFTimeZoneRef tz) {
    __CFGlobalTZLock();
    if (tz != __CFDefaultTimeZone) {
	CFRelease(__CFDefaultTimeZone);
	__CFDefaultTimeZone = (struct __CFTimeZone *)tz;
	CFRetain(tz);
    }
    __CFGlobalTZUnlock();
}

CFArrayRef CFTimeZoneCopyKnownNames(void) {
#warning CF: unfinished
    __CFGlobalTZLock();

    __CFGlobalTZUnlock();
    return CFArrayCreate(NULL, NULL, 0, NULL);
}

CFDictionaryRef CFTimeZoneCopyAbbreviationDictionary(void) {
    CFDictionaryRef dict;
    __CFGlobalTZLock();
    if (NULL == __CFTimeZoneAbbreviationDict) {
#warning CF: load the time zone abbreviation map plist from somewhere here
	__CFTimeZoneAbbreviationDict = NULL;
    }
    if (NULL == __CFTimeZoneAbbreviationDict) {
	__CFTimeZoneAbbreviationDict = CFDictionaryCreate(NULL, NULL, NULL, 0, NULL, NULL);
    }
    dict = CFRetain(__CFTimeZoneAbbreviationDict);
    __CFGlobalTZUnlock();
    return dict;
}

void CFTimeZoneSetAbbreviationDictionary(CFDictionaryRef dict) {
    __CFGlobalTZLock();
    if (dict != __CFTimeZoneAbbreviationDict) {
	CFRelease(__CFTimeZoneAbbreviationDict);
	CFRetain(dict);
	__CFTimeZoneAbbreviationDict = dict;
    }
    __CFGlobalTZUnlock();
}

CFTimeZoneRef CFTimeZoneCreate(CFAllocatorRef allocator, CFStringRef name, CFDataRef data) {
#warning CF: unimplemented
}

static CFTimeZoneRef __CFTimeZoneCreateFixed(CFAllocatorRef allocator, int seconds, CFStringRef name, int isDST) {
    CFTimeZoneRef result;
    CFDataRef data;
    SInt32 nameLen = CFStringGetLength(name);
#if !defined(__MACOS8__)
    char dataBytes[52 + nameLen + 1];
#else
	char *dataBytes = CFAllocatorAllocate(allocator, 52 + nameLen + 1, 0);
	if (!dataBytes) return NULL;
#endif
    CFZeroMemory(dataBytes, sizeof(dataBytes));
    __CFEntzcode(1, dataBytes + 20);
    __CFEntzcode(1, dataBytes + 24);
    __CFEntzcode(1, dataBytes + 36);
    __CFEntzcode(nameLen + 1, dataBytes + 40);
    __CFEntzcode(seconds, dataBytes + 44);
    dataBytes[48] = isDST ? 1 : 0;
    CFStringGetCString(name, dataBytes + 50, nameLen + 1, kCFStringEncodingASCII);
    data = CFDataCreate(allocator, dataBytes, sizeof(dataBytes));
    result = CFTimeZoneCreate(allocator, name, data);
    CFRelease(data);
	CFAllocatorDeallocate(allocator, dataBytes);
    return result;
}

CFTimeZoneRef CFTimeZoneCreateWithTimeIntervalFromGMT(CFAllocatorRef allocator, CFTimeInterval ti) {
    CFTimeZoneRef result;
    CFStringRef name;
    int hour = ((0 < ti) ? -ti : ti) / 3600;
    if (0.0 == ti) {
	name = CFRetain(CFSTR("GMT"));
    } else if (0 < ti) {
	name = CFStringCreateWithFormat(allocator, NULL, CFSTR("GMT+%02d%02d"), hour, (ti - hour * 3600 + 30) / 60);
    } else {
	name = CFStringCreateWithFormat(allocator, NULL, CFSTR("GMT-%02d%02d"), hour, (ti - hour * 3600 - 30) / 60);
    }
    result = __CFTimeZoneCreateFixed(allocator, (int)floor(ti), name, 0);
    CFRelease(name);
    return result;
}

CFTimeZoneRef CFTimeZoneCreateWithName(CFAllocatorRef allocator, CFStringRef name, Boolean tryAbbrev) {
    CFTimeZoneRef result = NULL;
    CFStringRef tzName;
    CFDataRef data = NULL;
    CFDictionaryRef abbrevs = CFTimeZoneCopyAbbreviationDictionary();
    tzName = CFDictionaryGetValue(abbrevs, name);
    if (NULL == tzName) {
	tzName = name;
    }
#warning unfinished
#if 0
    tzName = [isa replacementTimeZoneNameForName:tzName];
    data = [isa dataForTimeZone:tzName];
#endif
    if (data) {
	result = CFTimeZoneCreate(allocator, tzName, data);
    }
    CFRelease(abbrevs);
    return result;
}

CFStringRef CFTimeZoneGetName(CFTimeZoneRef tz) {
    __CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    return tz->_name;
}

CFDataRef CFTimeZoneGetData(CFTimeZoneRef tz) {
    __CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    return tz->_data;
}

CFTimeInterval CFTimeZoneGetSecondsFromGMT(CFTimeZoneRef tz, CFAbsoluteTime at) {
    SInt32 idx;
    __CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    idx = __CFBSearchTZPeriods(tz, at);
    if (tz->_periodCnt < idx)
	idx = tz->_periodCnt;
    else if (0 == idx)
	idx = 1;
    return __CFTZPeriodGMTOffset(&(tz->_periods[idx - 1]));
}

CFStringRef CFTimeZoneCopyAbbreviation(CFTimeZoneRef tz, CFAbsoluteTime at) {
    SInt32 idx;
    __CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    idx = __CFBSearchTZPeriods(tz, at);
    if (tz->_periodCnt < idx)
	idx = tz->_periodCnt;
    else if (0 == idx)
	idx = 1;
    return CFRetain(__CFTZPeriodAbbreviation(&(tz->_periods[idx - 1])));
}

Boolean CFTimeZoneIsDaylightSavingTime(CFTimeZoneRef tz, CFAbsoluteTime at) {
    SInt32 idx;
    __CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    idx = __CFBSearchTZPeriods(tz, at);
    if (tz->_periodCnt < idx)
	idx = tz->_periodCnt;
    else if (0 == idx)
	idx = 1;
    return __CFTZPeriodIsDST(&(tz->_periods[idx - 1]));
}

#if defined(__MACOS8__)

void __CFTimeZoneCleanup(void) {
    /* in case library is unloaded, release store for the allocated data */
    if (__CFSystemTimeZone != NULL) {
	CFRelease(__CFSystemTimeZone);
	__CFSystemTimeZone = NULL;
    }
    if (__CFDefaultTimeZone != NULL) {
	CFRelease(__CFDefaultTimeZone);
	__CFDefaultTimeZone = NULL;
    }
    if (__CFTimeZoneAbbreviationDict != NULL) {
	CFRelease(__CFTimeZoneAbbreviationDict);
	__CFTimeZoneAbbreviationDict = NULL;
    }
    if (__CFKnownTimeZoneList != NULL) {
	CFRelease(__CFKnownTimeZoneList);
	__CFKnownTimeZoneList = NULL;
    }
}

#endif


