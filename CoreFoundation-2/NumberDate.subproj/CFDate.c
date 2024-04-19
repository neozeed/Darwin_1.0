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
/*	CFDate.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFTimeZone.h>
#include "CFVeryPrivate.h"
#include "CFInternal.h"
#include <math.h>
#if defined(__MACH__)
    #include <sys/time.h>
#endif
#if defined(__MACOS8__)
    #include <DateTimeUtils.h>
#endif
#if defined(__WIN32__)
    #include <windows.h>
#endif

const CFTimeInterval kCFAbsoluteTimeIntervalSince1970 = 978307200.0L;
const CFTimeInterval kCFAbsoluteTimeIntervalSince1904 = 3061152000.0L;

/* cjk: The Julian Date for the reference date is 2451910.5,
        I think, in case that's ever useful. */

struct __CFDate {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFAbsoluteTime _time;	/* immutable */
};

static double __CFTSRRate = 0.0;
static CFAbsoluteTime __CFBootAbsoluteTime = 0.0;
static CFTimeInterval __CFLastSyncOffset = -1.0E+20;

void __CFInitializeDate(void) {
#if defined(__MACH__)
    struct timeval tv;
    /*
    void MKGetTimeBaseInfo( UInt32 *minAbsoluteTimeDelta,
    			UInt32 *theAbsoluteTimeToNanosecondNumerator,
    			UInt32 *theAbsoluteTimeToNanosecondDenominator,
    			UInt32 *theProcessorToAbsoluteTimeNumerator,
    			UInt32 *theProcessorToAbsoluteTimeDenominator);
    Returns time base constants for the current machine:
    	minAbsoluteTimeDelta
    			The minimum update resolution for absolute time values.
    	theAbsoluteTimeToNanosecondNumerator / theAbsoluteTimeToNanosecondDenominator
    			The ratio between one absolute time unit and one nanosecond.
    	theProcessorToAbsoluteTimeNumerator / theProcessorToAbsoluteTimeDenominator
    			The ratio between one one absolute time unit and one processor
    			time unit. A processor time unit is defined to be the duration
    			of one tick of a processor time base counter as defined by the
    			ISP which is directly accessible from user programs.
    */
    extern void MKGetTimeBaseInfo(UInt32 *, UInt32 *, UInt32 *, UInt32 *, UInt32 *);
    UInt32 min, a2n_n, a2n_d, p2a_n, p2a_d;
    MKGetTimeBaseInfo(&min, &a2n_n, &a2n_d, &p2a_n, &p2a_d);
    /* Carefully execute these computations to keep the numbers as
       close to +- two as possible (which gives highest precision). */
    __CFTSRRate = (((1.0E9 / (double)a2n_n) * (double)p2a_d) / (double)p2a_n) * (double)a2n_d;
    gettimeofday(&tv, NULL);		/* warmup; first call is expensive */
#else
#warning do not know how to initialize __CFTSRRate on this architecture
#endif
}

#if defined(__MACH__)
UInt64 __CFAbsoluteTimeToTSR(CFAbsoluteTime at) {
    CFTimeInterval delta = __CFReadTSR() / __CFTSRRate;
    if (__CFLastSyncOffset + 10.0 < delta) {
	struct timeval tv;
	/* 10.0 seconds is arbitrarily chosen, but keeps the error under
	0.004 seconds generally; we need a number which is large enough
	to cut down on the number of gettimeofday() calls, but small
	enough that radical changes to the calendar clock are noticed
	reasonably quickly. */
	gettimeofday(&tv, NULL);
	__CFLastSyncOffset = delta;
	__CFBootAbsoluteTime = ((double)tv.tv_sec - kCFAbsoluteTimeIntervalSince1970) + 1.0E-6 * (double)tv.tv_usec - delta;
    }
    return (at - __CFBootAbsoluteTime ) * __CFTSRRate;
}

CFAbsoluteTime __CFTSRToAbsoluteTime(UInt64 tsr) {
    CFTimeInterval delta = __CFReadTSR() / __CFTSRRate;
    if (__CFLastSyncOffset + 10.0 < delta) {
	struct timeval tv;
	/* 10.0 seconds is arbitrarily chosen, but keeps the error under
	0.004 seconds generally; we need a number which is large enough
	to cut down on the number of gettimeofday() calls, but small
	enough that radical changes to the calendar clock are noticed
	reasonably quickly. */
	gettimeofday(&tv, NULL);
	__CFLastSyncOffset = delta;
	__CFBootAbsoluteTime = ((double)tv.tv_sec - kCFAbsoluteTimeIntervalSince1970) + 1.0E-6 * (double)tv.tv_usec - delta;
    }
    return __CFBootAbsoluteTime + tsr / __CFTSRRate;
}
#endif

CFAbsoluteTime CFAbsoluteTimeGetCurrent(void) {
    CFAbsoluteTime ret;
#if defined(__WIN32__)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ret = (CFTimeInterval)ft.dwHighDateTime * 429.49672960;
    ret += (CFTimeInterval)ft.dwLowDateTime / 10000000.0;
    ret -= (11644473600.0 + kCFAbsoluteTimeIntervalSince1970);
	 /* seconds between 1601 and 1970, 1970 and 2001 */
#endif
#if defined(__MACOS8__)
    /* Resolution only to 1 second */
    unsigned long time;
    GetDateTime(&time);
    ret = (CFTimeInterval)time - kCFAbsoluteTimeIntervalSince1904;
#endif
#if defined(__MACH__)
    ret = __CFTSRToAbsoluteTime(__CFReadTSR());
#endif __MACH__
#if defined(__svr4__) || defined(__hpux__)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ret = (CFTimeInterval)tv.tv_sec - kCFAbsoluteTimeIntervalSince1970;
    ret += (1.0E-6 * (CFTimeInterval)tv.tv_usec);
#endif
    return ret;
}

CFTypeID CFDateGetTypeID(void) {
    return __kCFDateTypeID;
}

Boolean __CFDateEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFDateRef date1 = (CFDateRef)cf1;
    CFDateRef date2 = (CFDateRef)cf2;
    if (date1->_time != date2->_time) return FALSE;
    return TRUE;
}

CFHashCode __CFDateHash(CFTypeRef cf) {
    CFDateRef date = (CFDateRef)cf;
    return (CFHashCode)floor(date->_time);
}

CFStringRef __CFDateCopyDescription(CFTypeRef cf) {
    CFDateRef date = (CFDateRef)cf;
    return CFStringCreateWithFormat(date->_allocator, NULL, CFSTR("<CFDate 0x%x [0x%x]>{time = %0.09g}"), (UInt32)cf, (UInt32)date->_allocator, date->_time);
}

CFAllocatorRef __CFDateGetAllocator(CFTypeRef cf) {
    CFDateRef date = (CFDateRef)cf;
    return date->_allocator;
}

void __CFDateDeallocate(CFTypeRef cf) {
    CFDateRef date = (CFDateRef)cf;
    CFAllocatorRef allocator = date->_allocator;
    CFAllocatorDeallocate(allocator, (void *)date);
    CFRelease(allocator);
}

static CFDateRef __CFDateInit(CFAllocatorRef allocator, CFAbsoluteTime at) {
    CFDateRef memory;
    UInt32 size;
    size = sizeof(struct __CFDate);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase((void *)memory, __CFDateIsa(), __kCFDateTypeID);
    __CFSetLastAllocationEventName(memory, "CFDate");
    ((struct __CFDate *)memory)->_allocator = allocator;
    ((struct __CFDate *)memory)->_time = at;
    return memory;
}

CFDateRef CFDateCreate(CFAllocatorRef allocator, CFAbsoluteTime at) {
   return __CFDateInit(allocator, at);
}

CFTimeInterval CFDateGetAbsoluteTime(CFDateRef date) {
    CF_OBJC_FUNCDISPATCH0(Date, CFTimeInterval, date, "timeIntervalSinceReferenceDate");
    __CFGenericValidateType(date, __kCFDateTypeID);
    return date->_time;
}

CFTimeInterval CFDateGetTimeIntervalSinceDate(CFDateRef date, CFDateRef otherDate) {
    CF_OBJC_FUNCDISPATCH1(Date, CFTimeInterval, date, "timeIntervalSinceDate:", otherDate);
    __CFGenericValidateType(date, __kCFDateTypeID);
    __CFGenericValidateType(otherDate, __kCFDateTypeID);
    return date->_time - otherDate->_time;
}

CFComparisonResult CFDateCompare(CFDateRef date, CFDateRef otherDate, void *context) {
    CF_OBJC_FUNCDISPATCH1(Date, CFComparisonResult, date, "compare:", otherDate);
    __CFGenericValidateType(date, __kCFDateTypeID);
    __CFGenericValidateType(otherDate, __kCFDateTypeID);
    if (date->_time < otherDate->_time) return kCFCompareLessThan;
    if (date->_time > otherDate->_time) return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

CF_INLINE SInt32 __CFDoubleModToInt(double d, SInt32 modulus) {
    SInt32 result = (SInt32)floor(d - floor(d / modulus) * modulus);
    if (result < 0) result += modulus;
    return result;
}

CF_INLINE double __CFDoubleMod(double d, SInt32 modulus) {
    double result = d - floor(d / modulus) * modulus;
    if (result < 0.0) result += (double)modulus;
    return result;
}

/* year arg is absolute year; Gregorian 2001 == year 0; 2001/1/1 = absolute date 0 */
static SInt32 __CFDaysInMonth(SInt32 month, SInt32 year) {
    switch (month) {
    case 1: return 31;
    case 2: {
	SInt32 y;
	year += 1;	/* correct to nearest multiple-of-400 year */
	y = year % 400;
	return (year % 4 == 0 && y != 100 && y != 200 && y != 300) ? 29 : 28;
    }
    case 3: return 31;
    case 4: return 30;
    case 5: return 31;
    case 6: return 30;
    case 7: return 31;
    case 8: return 31;
    case 9: return 30;
    case 10: return 31;
    case 11: return 30;
    case 12: return 31;
    }
    return 0;
}

/* year arg is absolute year; Gregorian 2001 == year 0; 2001/1/1 = absolute date 0 */
static SInt32 __CFDaysBeforeMonth(SInt32 month, SInt32 year) {
    switch (month) {
    case 1: return 0;
    case 2: return 31;
    case 3: return 31 + __CFDaysInMonth(2, year);
    case 4: return 62 + __CFDaysInMonth(2, year);
    case 5: return 92 + __CFDaysInMonth(2, year);
    case 6: return 123 + __CFDaysInMonth(2, year);
    case 7: return 153 + __CFDaysInMonth(2, year);
    case 8: return 184 + __CFDaysInMonth(2, year);
    case 9: return 215 + __CFDaysInMonth(2, year);
    case 10: return 245 + __CFDaysInMonth(2, year);
    case 11: return 276 + __CFDaysInMonth(2, year);
    case 12: return 306 + __CFDaysInMonth(2, year);
    case 13: return 337 + __CFDaysInMonth(2, year);
    }
    return 0;
}

/* year arg is absolute year; Gregorian 2001 == year 0; 2001/1/1 = absolute date 0 */
static SInt32 __CFDaysAfterMonth(SInt32 month, SInt32 year) {
    switch (month) {
    case 0: return 337 + __CFDaysInMonth(2, year);
    case 1: return 306 + __CFDaysInMonth(2, year);
    case 2: return 306;
    case 3: return 275;
    case 4: return 245;
    case 5: return 214;
    case 6: return 184;
    case 7: return 153;
    case 8: return 122;
    case 9: return 92;
    case 10: return 61;
    case 11: return 31;
    case 12: return 0;
    }
    return 0;
}

/* year arg is absolute year; Gregorian 2001 == year 0; 2001/1/1 = absolute date 0 */
static void __CFYMDFromAbsolute(SInt32 absolute, SInt32 *year, SInt32 *month, SInt32 *day) {
    SInt32 y = 0, ydays;
    while (absolute < 0) {
	y -= 1;
	absolute += __CFDaysAfterMonth(0, y);
    }
    /* Now absolute is non-negative days to add to year */
    ydays = __CFDaysAfterMonth(0, y);
    while (ydays < absolute) {
	y += 1;
	absolute -= ydays;
	ydays = __CFDaysAfterMonth(0, y);
    }
    /* Now we have year and days-into-year */
    if (year) *year = y;
    if (month || day) {
	SInt32 m = absolute / 33 + 1; /* search from the approximation */
	while (__CFDaysBeforeMonth(m + 1, y) <= absolute) m++;
	if (month) *month = m;
	if (day) *day = absolute - __CFDaysBeforeMonth(m, y) + 1;
    }
}

/* year arg is absolute year; Gregorian 2001 == year 0; 2001/1/1 = absolute date 0 */
static SInt32 __CFAbsoluteFromYMD(SInt32 year, SInt32 month, SInt32 day) {
    SInt32 absolute = 0, idx;
    if (year < 0) {
	for (idx = year; idx < 0; idx++)
	    absolute -= __CFDaysAfterMonth(0, idx);
    } else {
	for (idx = year - 1; 0 <= idx; idx--)
	    absolute += __CFDaysAfterMonth(0, idx);
    }
    /* Now add the days into the original year */
    absolute += __CFDaysBeforeMonth(month, year) + day - 1;
    return absolute;
}

Boolean CFGregorianDateIsValid(CFGregorianDate gdate, CFOptionFlags unitFlags) {
    if (gdate.year <= 0) return FALSE;
    if (gdate.month < 1 || 12 < gdate.month) return FALSE;
    if (gdate.day < 1 || 31 < gdate.day) return FALSE;
    if (gdate.hour < 0 || 24 <= gdate.hour) return FALSE;
    if (gdate.minute < 0 || 60 <= gdate.minute) return FALSE;
    if (gdate.second < 0.0 || 60.0 <= gdate.second) return FALSE;
    if (__CFDaysInMonth(gdate.month, gdate.year - 2001) < gdate.day) return FALSE;
    return TRUE;
}

Boolean CFGregorianDateValidate(CFGregorianDate gdate, CFOptionFlags unitFlags) {
// Remove this obsolete function someday
    return CFGregorianDateIsValid(gdate, unitFlags);
}

CFAbsoluteTime CFGregorianDateGetAbsoluteTime(CFGregorianDate gdate, CFTimeZoneRef tz) {
    CFAbsoluteTime at;
    CFTimeInterval offset0, offset1;
    if (NULL != tz) {
	__CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    }
    at = 86400.0 * __CFAbsoluteFromYMD(gdate.year - 2001, gdate.month, gdate.day);
    at += 3600.0 * gdate.hour + 60.0 * gdate.minute + gdate.second;
    if (NULL != tz) {
	offset0 = CFTimeZoneGetSecondsFromGMT(tz, at);
	offset1 = CFTimeZoneGetSecondsFromGMT(tz, at - offset0);
	at -= (offset0 != offset1) ? offset1 : offset0;
    }
    return at;
}

CFGregorianDate CFAbsoluteTimeGetGregorianDate(CFAbsoluteTime at, CFTimeZoneRef tz) {
    CFGregorianDate gdate;
    SInt32 absolute, year, month, day;
    CFAbsoluteTime fixedat;
    if (NULL != tz) {
	__CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    }
    fixedat = at + (NULL != tz ? CFTimeZoneGetSecondsFromGMT(tz, at) : 0.0);
    absolute = (SInt32)floor(fixedat / 86400.0);
    __CFYMDFromAbsolute(absolute, &year, &month, &day);
    gdate.year = year + 2001;
    gdate.month = month;
    gdate.day = day;
    gdate.hour = __CFDoubleModToInt(floor(fixedat / 3600.0), 24);
    gdate.minute = __CFDoubleModToInt(floor(fixedat / 60.0), 60);
    gdate.second = __CFDoubleMod(fixedat, 60);
    return gdate;
}

/* Note that the units of years and months are not equal length, but are treated as such. */
CFAbsoluteTime CFAbsoluteTimeAddGregorianUnits(CFAbsoluteTime at, CFTimeZoneRef tz, CFGregorianUnits units) {
    CFGregorianDate gdate;
    CFGregorianUnits working;
    CFAbsoluteTime candidate_at0, candidate_at1;
    CFTimeInterval correction;
    SInt32 monthdays;

    if (NULL != tz) {
	__CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    }

    /* Most people seem to expect years, then months, then days, etc.
	to be added in that order.  Thus, 27 April + (4 days, 1 month)
	= 31 May, and not 1 June. This is also relatively predictable.

	On another issue, months not being equal length, people also
	seem to expect late day-of-month clamping (don't clamp as you
	go through months), but clamp before adding in the days. Late
	clamping is also more predictable given random starting points
	and random numbers of months added (ie Jan 31 + 2 months could
	be March 28 or March 29 in different years with aggressive
	clamping). Proportionality (28 Feb + 1 month = 31 March) is
	also not expected.

	Also, people don't expect time zone transitions to have any
	effect when adding years and/or months and/or days, only.
	Hours, minutes, and seconds, though, are added in as humans
	woulds experience the passing of that time. What this means
	is that if the date, after adding years, months, and days
	lands on some date, and then adding hours, minutes, and
	seconds crosses a time zone transition, the time zone
	transition is accounted for. If adding years, months, and
	days gets the date into a different time zone offset period,
	that transition is not taken into account.

	However, there is also a second correction that is needed,
	due to the fact that the "get gregorian date from absolute"
	operation is lossy, during the one hour period of "fall back"
	when daylight savings time ends. For example, in US/Pacific
	time, a time of 1h 30m could mean either 1:30 -0700 or
	1:30 -0800, and it's nice if that gets preserved, as they
	are actually different times one hour from one another.
    */
    gdate = CFAbsoluteTimeGetGregorianDate(at, tz);
    /* We must work in a CFGregorianUnits, because the fields in the CFGregorianDate can easily overflow */
    working.years = gdate.year;
    working.months = gdate.month;
    working.days = gdate.day;
    /* Compute correction for lossy conversion, if necessary */
    correction = at - CFGregorianDateGetAbsoluteTime(gdate, tz);
    working.years += units.years;
    working.months += units.months;
    while (12 < working.months) {
	working.months -= 12;
	working.years += 1;
    }
    while (working.months < 1) {
	working.months += 12;
	working.years -= 1;
    }
    monthdays = __CFDaysInMonth(working.months, working.years - 2001);
    if (monthdays < working.days) {	/* Clamp day to new month */
	working.days = monthdays;
    }
    working.days += units.days;
    while (monthdays < working.days) {
	working.months += 1;
	if (12 < working.months) {
	    working.months -= 12;
	    working.years += 1;
	}
	working.days -= monthdays;
	monthdays = __CFDaysInMonth(working.months, working.years - 2001);
    }
    while (working.days < 1) {
	working.months -= 1;
	if (working.months < 1) {
	    working.months += 12;
	    working.years -= 1;
	}
	monthdays = __CFDaysInMonth(working.months, working.years - 2001);
	working.days += monthdays;
    }
    gdate.year = working.years;
    gdate.month = working.months;
    gdate.day = working.days;
    /* Roll in hours, minutes, and seconds */
    candidate_at0 = CFGregorianDateGetAbsoluteTime(gdate, tz);
    candidate_at1 = candidate_at0 + 3600.0 * units.hours + 60.0 * units.minutes + units.seconds;
    /* If summing in the hours, minutes, and seconds delta pushes us
     * into a new time zone offset, that will automatically be taken
     * care of by the fact that we just add the raw time above. To
     * undo that effect, we'd have to get the time zone offsets for
     * candidate_at0 and candidate_at1 here, and subtract the
     * difference (offset1 - offset0) from candidate_at1. */
    return candidate_at1 + correction;
}

/* at2 - at1.  The only constraint here is that this needs to be the inverse
of CFAbsoluteTimeByAddingGregorianUnits(), but that's a very rigid constraint.
Unfortunately, due to the nonuniformity of the year and month units, this
inversion essentially has to approximate until it finds the answer. */
CFGregorianUnits CFAbsoluteTimeGetDifferenceAsGregorianUnits(CFAbsoluteTime at1, CFAbsoluteTime at2, CFTimeZoneRef tz, CFOptionFlags unitFlags) {
    const SInt32 seconds[5] = {366 * 24 * 3600, 31 * 24 * 3600, 24 * 3600, 3600, 60};
    CFGregorianUnits units = {0, 0, 0, 0, 0, 0.0};
    SInt32 idx, incr;
    if (at1 == at2) {
	return units;
    }
    incr = (at1 < at2) ? 1 : -1;
    /* Successive approximation: years, then months, then days, then hours, then minutes. */
    for (idx = 0; idx < 5; idx++) {
	if (unitFlags & (1 << idx)) {
	    CFAbsoluteTime atnew;
	    *((SInt32 *)&units + idx) = -2 * incr + (at2 - at1) / seconds[idx];
	    atnew = CFAbsoluteTimeAddGregorianUnits(at1, tz, units);
	    while ((1 == incr && atnew < at2) || (-1 == incr && at2 < atnew)) {
		*((SInt32 *)&units + idx) += incr;
		atnew = CFAbsoluteTimeAddGregorianUnits(at1, tz, units);
	    }
	    *((SInt32 *)&units + idx) -= incr;
	}
    }
    if (unitFlags & kCFGregorianUnitsSeconds) {
	units.seconds = at2 + incr * CFAbsoluteTimeAddGregorianUnits(at1, tz, units);
    }
    return units;
}

SInt32 CFAbsoluteTimeGetDayOfWeek(CFAbsoluteTime at, CFTimeZoneRef tz) {
    SInt32 absolute;
    CFAbsoluteTime fixedat;
    if (NULL != tz) {
	__CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    }
    fixedat = at + (NULL != tz ? CFTimeZoneGetSecondsFromGMT(tz, at) : 0.0);
    absolute = (SInt32)floor(fixedat / 86400.0);
    return (absolute < 0) ? ((absolute + 1) % 7 + 7) : (absolute % 7 + 1); /* Monday = 1, etc. */
}

SInt32 CFAbsoluteTimeGetDayOfYear(CFAbsoluteTime at, CFTimeZoneRef tz) {
    SInt32 absolute, year, month, day;
    CFAbsoluteTime fixedat;
    if (NULL != tz) {
	__CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    }
    fixedat = at + (NULL != tz ? CFTimeZoneGetSecondsFromGMT(tz, at) : 0.0);
    absolute = (SInt32)floor(fixedat / 86400.0);
    __CFYMDFromAbsolute(absolute, &year, &month, &day);
    return __CFDaysBeforeMonth(month, year) + day;
}

/* "the first week of a year is the one which includes the first Thursday" (ISO 8601) */
SInt32 CFAbsoluteTimeGetWeekOfYear(CFAbsoluteTime at, CFTimeZoneRef tz) {
    SInt32 absolute, year, month, day, absolute0101, dow0101;
    CFAbsoluteTime fixedat;
    if (NULL != tz) {
	__CFGenericValidateType(tz, __kCFTimeZoneTypeID);
    }
    fixedat = at + (NULL != tz ? CFTimeZoneGetSecondsFromGMT(tz, at) : 0.0);
    absolute = (SInt32)floor(fixedat / 86400.0);
    __CFYMDFromAbsolute(absolute, &year, &month, &day);
    absolute0101 = __CFAbsoluteFromYMD(year, 1, 1);
    dow0101 = (absolute0101 < 0) ? ((absolute0101 + 1) % 7 + 7) : (absolute0101 % 7 + 1);
    /* First three and last three days of a year can end up in a week of a different year */
    if (1 == month && day < 4) {
	if ((day < 4 && 5 == dow0101) || (day < 3 && 6 == dow0101) || (day < 2 && 7 == dow0101)) {
	    return 53;
	}
    }
    if (12 == month && 28 < day) {
	SInt32 absolute20101, dow20101;
	absolute20101 = __CFAbsoluteFromYMD(year + 1, 1, 1);
	dow20101 = (absolute20101 < 0) ? ((absolute20101 + 1) % 7 + 7) : (absolute20101 % 7 + 1);
	if ((28 < day && 4 == dow20101) || (29 < day && 3 == dow20101) || (30 < day && 2 == dow20101)) {
	    return 1;
	}
    }
    /* Days into year, plus a week-shifting correction, divided by 7. First week is #1. */
    return (__CFDaysBeforeMonth(month, year) + day + (dow0101 - 11) % 7 + 2) / 7 + 1;
}

