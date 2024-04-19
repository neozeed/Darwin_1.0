/**
 * StartupDisplay.c - Show Boot Status via the console
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
#include "Log.h"
#include "main.h"

/*
 * This is an opaque (void *) pointer in StarupDisplay.h.
 * The following definition is private to this file.
 */
typedef struct _Text_DisplayContext {
} *DisplayContext;
#define _StartupDisplay_C_
#include "StartupDisplay.h"

DisplayContext initDisplayContext()
{
    DisplayContext aContext = (DisplayContext)malloc(sizeof(struct _Text_DisplayContext));

    return(aContext);
}

void freeDisplayContext (DisplayContext aContext)
{
    if (aContext)
        free(aContext);
}

int displayStatus (DisplayContext aDisplayContext,
                   CFStringRef aMessage, float aPercentage)
{
    if (aDisplayContext)
      {
        /**
         * Draw text.
         **/
        if (aMessage)
          {
            message(CFSTR("%@\n"), aMessage);

            return(0);
          }
      }
    return(1);
}
