/**
 * QuartzDisplay.c - Show Boot Status via CoreGraphics
 * Wilfredo Sanchez | wsanchez@apple.com
 * $Apple$
 **
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 **
 * Draws the startup screen, progress bar and status text using
 * CoreGraphics (Quartz).
 *
 * WARNING: This code uses private Core Graphics API.  Private API is
 * subject to change at any time without notice, and it's use by
 * parties unknown to the Core Graphics authors is in no way supported.
 * If you borrow code from here for other use and it later breaks, you
 * have no basis for complaint.
 **/

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGPDF.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreGraphics/CGGraphicsPrivate.h>
#include <CoreGraphics/CGFontEncoding.h>
#include "Log.h"
#include "main.h"
#include "QuartzProgressBar.h"

/*
 * This is an opaque (void *) pointer in StarupDisplay.h.
 * The following definition is private to this file.
 */
typedef struct _CGS_DisplayContext {
    CGSConnectionID connectionID;
    CGRect          bootImageRect;
    CGSWindowID     statusWindowID;
    CGContextRef   statusContext;
    ProgressBarRef  progressBar;
} *DisplayContext;
#define _StartupDisplay_C_
#include "StartupDisplay.h"

/* FIXME: Move all of this into a plist. */
/* #define kBootImagePath	BOOT_PANEL_IMAGE  */
#define kAutoFillColor		0.4,0.4,0.6
#define kStatusAreaColor	0.0,0.0,0.0,0.0
#define kStatusAreaHeight	 40.0
#define kStatusAreaWidth	350.0
#define kStatusTextFont		"Charcoal"
#define kStatusTextColor	0.0,0.0,0.0,1.0
#define kStatusTextFontSize	 12.0
#define kStatusBarWidth		250.0
#define kStatusBarHeight	 20.0
#define kStatusBarPosX		((kStatusAreaWidth-kStatusBarWidth)/2.0)
#define kStatusBarPosY		(kStatusAreaHeight-kStatusBarHeight)

static CGSConnectionID _initDisplayConnection()
{
    CGSConnectionID aConnectionID;

    /* Initialize Core Graphics */
    CGSInitialize();

    /* Connect to Window Server */
    if ((CGSNewConnection(NULL, &aConnectionID)) != kCGSErrorSuccess)
        return(NULL);

    return(aConnectionID);
}

static int _closeDisplayConnection(CGSConnectionID aConnectionID)
{
    if ((CGSReleaseConnection(aConnectionID)) != kCGSErrorSuccess)
        return(1);

    return(0);
}

static int _initWindowContext(CGSConnectionID aConnectionID,
                              CGSWindowID*    aWindowID,
                              CGContextRef*   aContext,
                              CGRect          aRectangle,
                              CGSBoolean      aShadowOption)
{
    /**
     * Create a window at (aPosX,aPosY) with size (aWidth,aHeight).
     **/
    {
        CGSRegionObj aRegion;

        CGSNewRegionWithRect(&aRectangle, &aRegion);

        if (CGSNewWindow(aConnectionID, kCGSBufferedBackingType, 0.0, 0.0, aRegion, aWindowID) != kCGSErrorSuccess)
          { warning(CFSTR("CGSNewWindow failed")); return(1); }

        /* Enable/disable the drop-shadow on this window */
        {
            CGSValueObj  aKey, aShadow;

            aKey    = CGSCreateCString("HasShadow");
            aShadow = CGSCreateBoolean(aShadowOption);

            CGSSetWindowProperty(aConnectionID, *aWindowID, aKey, aShadow);

            CGSReleaseGenericObj(aKey);
            CGSReleaseGenericObj(aShadow);
        }

        /* Set background color for window. */
        CGSSetWindowAutofillColor(aConnectionID, *aWindowID, kAutoFillColor);

        if (CGSOrderWindow(aConnectionID, *aWindowID, kCGSOrderAbove, (CGSWindowID)0) != kCGSErrorSuccess)
          { warning(CFSTR("CGSOrderWindow failed")); return(1); }
    }

    /**
     * Create and initialize graphics context.
     **/
    {
        CGSDictionaryObj aDestination = CGSCreateDictionary(4);

        CGSPutCStringForCStringKey (aDestination, "Library",    "RIP"             );
        CGSPutCStringForCStringKey (aDestination, "Class",      "RIPContext"      );
        CGSPutIntegerForCStringKey (aDestination, "Window",     (int)*aWindowID   );
        CGSPutIntegerForCStringKey (aDestination, "Connection", (int)aConnectionID);

        if (CGSBeginContext(aDestination, aContext) !=  kCGSErrorSuccess)
          { warning(CFSTR("CGSBeginContext failed")); return(1); }

        CGSEraseContext (*aContext);

        CGSReleaseObj (aDestination);
    }

    return (0);
}

static int _initBootImageContext (DisplayContext aDisplayContext)
{
    CGSConnectionID aConnectionID = aDisplayContext->connectionID;
    CGSWindowID     aWindowID     = (CGSWindowID  )0;
    CGContextRef    aContext      = (CGContextRef)0;

    if (aConnectionID)
      {
        CGPDFDocumentRef anImageDoc;

        srandom ((time(NULL) & 0xffff) | ((getpid() & 0xffff) << 16));

        if ((anImageDoc = CGPDFDocumentOpenWithFile(kBootImagePath)))
          {
            CGRect       aDisplayRect;
            CGRect       anImageRectCG;
            CGRect       anImageRect;
            CGSRegionObj anImageRegion;

            int aPageCount = CGPDFDocumentPageCount(anImageDoc);
            int aPage = (int)(random() % aPageCount) + 1;

            if (! (aPageCount > 0))
              {
                error(CFSTR("Boot image has no pages."));
                return(1);
              }

            CGSGetDisplayBounds((CGSDisplayNumber)0, &aDisplayRect); /* Bounds for display 0 */

            anImageRectCG = CGPDFDocumentPageMediaBox(anImageDoc, aPage);

            /* Crap. We need to convert the CGRect into a CGSRect. */
            anImageRect.origin.x    = aDisplayRect.origin.x + ((aDisplayRect.size.width  - anImageRectCG.size.width )/2.0);
            anImageRect.origin.y    = aDisplayRect.origin.y + ((aDisplayRect.size.height - anImageRectCG.size.height)/2.0);
            anImageRect.size.width  = anImageRectCG.size.width;
            anImageRect.size.height = anImageRectCG.size.height;

            CGSNewRegionWithRect(&anImageRect, &anImageRegion);
            CGSGetRegionBounds(anImageRegion, &anImageRect);

            aDisplayContext->bootImageRect = anImageRect;

            if (_initWindowContext(aConnectionID, &aWindowID, &aContext, anImageRect, kCGSTrue))
              {
                error(CFSTR("Can't create window context."));
                return(1);
              }

            CGPDFDocumentShowPage(anImageDoc, aPage, aContext);
            CGPDFDocumentClose(anImageDoc);

            CGFlushContext (aContext);
            CGEndContext   (aContext);

            return(0);
          }
        else
            warning(CFSTR("Can't find boot image %s."), kBootImagePath);
      }

    return(1);
}

static int _initStatusWindowContext (DisplayContext aDisplayContext)
{
    CGSConnectionID aConnectionID =  aDisplayContext->connectionID;
    CGSWindowID*    aWindowID     = &aDisplayContext->statusWindowID;
    CGContextRef*  aContext      = &aDisplayContext->statusContext;

    if (aConnectionID)
      {
        CGRect aDisplayRect;
        CGRect aTextFieldRect;

        CGSGetDisplayBounds(0, &aDisplayRect);	/* Bounds for Display 0 */

        aTextFieldRect =
            CGSMakeRect(aDisplayRect.origin.x + ((aDisplayRect.size.width -kStatusAreaWidth )/2.0),
                        aDisplayRect.origin.y + ((aDisplayRect.size.height-kStatusAreaHeight)/2.0) + (aDisplayContext->bootImageRect.size.height/4.0),
                        kStatusAreaWidth, kStatusAreaHeight);

        if (_initWindowContext(aConnectionID, aWindowID, aContext, aTextFieldRect, kCGSFalse))
          {
            error(CFSTR("Can't create window context."));
            return(1);
          }

        /* Enable transparency for this window. */
        CGSSetWindowOpacity(aConnectionID, *aWindowID, kCGSFalse);
      }

    return(0);
}

static float _computeTextLength (CGSContextObj aContext, const char *aString)
{
    CGFontRef aFont;
    float     aScale;
    size_t    aLength;
    size_t    anIterator;
    CGGlyph*  aGlyphs;
    int*      anAdvance;
    float     aTotalAdvance;
    int       aUnitsPerEm;

    aFont       = CGGetFont(aContext);
    aUnitsPerEm = CGFontGetUnitsPerEm(aFont);
    aScale      = CGGetFontScale(aContext);
    aLength     = strlen(aString);

    aGlyphs   = (CGGlyph*)malloc(sizeof(CGGlyph)*aLength);
    anAdvance = (int*    )malloc(sizeof(int    )*aLength);

    for (anIterator = 0; anIterator < aLength; ++anIterator)
        aGlyphs[anIterator] = ((CGGlyph)(aString[anIterator])) & 0xFF;

    /* We're using the outline version, so get outline advances, not device. */
    CGFontGetGlyphAdvances(aFont, aGlyphs, aLength, anAdvance);

    for (anIterator = 0, aTotalAdvance = 0; anIterator < aLength; ++anIterator)
        aTotalAdvance += (((float)anAdvance[anIterator])/(float)aUnitsPerEm) * aScale;

    free(aGlyphs  );
    free(anAdvance);

    return aTotalAdvance;
}

DisplayContext initDisplayContext()
{
    DisplayContext aContext = (DisplayContext)malloc(sizeof(struct _CGS_DisplayContext));

    aContext->connectionID   = _initDisplayConnection();
    aContext->statusWindowID = (CGSWindowID  )0;
    aContext->statusContext  = (CGSContextObj)0;

    _initBootImageContext    (aContext);
    _initStatusWindowContext (aContext);

    aContext->progressBar = ProgressBarCreate (aContext->statusContext,
                                               kStatusBarPosX, kStatusBarPosY, kStatusBarWidth);

    return(aContext);
}

void freeDisplayContext (DisplayContext aContext)
{
    if (aContext)
      {
        if (aContext->progressBar) ProgressBarFree(aContext->progressBar);

        /**
         * FIXME: Verify that this is correct:
         * Window ID's are implicitly cleaned up when the context gets
         * destroyed, so we don't need to release the windows here.
         **/
        if (aContext->statusContext) CGEndContext(aContext->statusContext);

        if (aContext->connectionID) _closeDisplayConnection(aContext->connectionID);

        free(aContext);
      }
}

int displayStatus (DisplayContext aDisplayContext,
                   CFStringRef aMessage, float aPercentage)
{
    if (aDisplayContext)
      {
        CGSConnectionID aConnectionID = aDisplayContext->connectionID;
        CGSWindowID     aWindowID     = aDisplayContext->statusWindowID;
        CGSContextObj   aContext      = aDisplayContext->statusContext;

        if (aConnectionID && aWindowID && aContext)
          {
            {
                CGRect aRectangle = CGSMakeRect(0.0, 0.0, kStatusAreaWidth, kStatusAreaHeight);

                CGSaveGState          (aContext);
                CGSSetGStateAttribute (aContext, CGSUniqueCString("Composite"), CGSUniqueCString("Copy"));
                CGSetRGBFillColor     (aContext, kStatusAreaColor);
                CGFillRect            (aContext, aRectangle);
                CGRestoreGState       (aContext);
            }

            /**
             * Draw status bar.
             **/
            ProgressBarSetPercent (aDisplayContext->progressBar, aPercentage);
            ProgressBarDisplay    (aDisplayContext->progressBar);

            /**
             * Draw text.
             **/
            if (aMessage)
              {
                char *aMessageCString;
                size_t aMessageCLength =
                    CFStringGetMaximumSizeForEncoding(CFStringGetLength(aMessage), CFStringGetSystemEncoding()) + 1;

                if ((aMessageCString = (char*)malloc(aMessageCLength)))
                  {
                    float anOffset;

                    if (!CFStringGetCString(aMessage, aMessageCString, aMessageCLength, CFStringGetSystemEncoding()))
                        *aMessageCString = '\0';

                    CGSaveGState          (aContext);
                    CGSetRGBFillColor     (aContext, kStatusTextColor);
                    CGSelectFont          (aContext, kStatusTextFont, kStatusTextFontSize);
                    CGFontSetTextEncoding (CGGetFont(aContext), "MacRomanEncoding");
                    anOffset = (kStatusAreaWidth - _computeTextLength(aContext, aMessageCString)) / 2.0;
                    CGShowTextAtPoint     (aContext, anOffset, (2.0+(kStatusAreaHeight/2.0)-kStatusTextFontSize)/2.0, aMessageCString, aMessageCLength-1);
                    CGRestoreGState       (aContext);

                    free(aMessageCString);
                  }
                else
                    warning(CFSTR("malloc() failed; out of memory"));
              }

            CGFlushContext (aContext);

            return(0);
          }
      }
    return(1);
}
