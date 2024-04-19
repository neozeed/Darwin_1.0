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
#include "debug_font.h"


#define kNormal  0
#define kBold    1
#define kInverse 2


#define kCharHeight   12 // pixels
#define kCharWidth     6 // pixels
#define kHeaderHeight 18 // pixels

/* Internal routines */
void init_display_putc(unsigned char *baseaddr, int rowbytes, int height);
void display_putc(char c);
void scroll(void);
void checkXY(void);
void copyNormalChar(unsigned char *bptr, unsigned char *fptr);
void copyBoldChar(unsigned char *bptr, unsigned char *fptr);
void copyInverseChar(unsigned char *bptr, unsigned char *fptr);

/* globals */
static unsigned char *gBaseAddr;
static int gRowBytes, gHeight;
static int gCursorX, gCursorY;		// Where the cursor is at all times
static int gMaxX, gMaxY;		// screen limits
static int gStyle;			// normal, bold, inverse, etc.

void init_display_putc(unsigned char *baseaddr, int rowbytes, int height)
{
	if (!baseaddr)
		gBaseAddr  = 0;
	else
	{
		gBaseAddr  = baseaddr;
		gRowBytes  = rowbytes;
		gHeight    = height;
		gCursorX   = 0;
		gCursorY   = 2;
		gStyle     = kNormal;
		gMaxX      = gRowBytes / kCharWidth;
		gMaxY      = gHeight / kCharHeight;
	}
}

void display_putc(char c)
{
	register unsigned char *bptr;
	register unsigned char *fptr;


	// handle special characters first
	switch (c)
	{
		case '\t': gCursorX += 4; 				return;
		case '\b': gCursorX -= 4; 				return;
		case '\n': gCursorX  = 0; gCursorY++;	return;
		case '\r': gCursorX  = 0; gCursorY++;	return;
		case '\300': gStyle = kNormal;			return;
		case '\301': gStyle = kBold;			return;
		case '\302': gStyle = kInverse;			return;
	}

	// scroll if needed
	checkXY();

	// non-printable chars are a space
	if ((c < 0x20) || (c > 0x7e)) c = 0x20;

	// our font starts with a space
	c -= 0x20;

	bptr = gBaseAddr
				 + (gRowBytes * kCharHeight * gCursorY)
				 + (kCharWidth * gCursorX);
	fptr = Font[(int)c];

	switch (gStyle)
	{
		case kNormal:	copyNormalChar(bptr, fptr); break;
		case kBold:		copyBoldChar(bptr, fptr);	break;
		case kInverse:	copyInverseChar(bptr, fptr);break;
	}
	gCursorX++;
}


void checkXY(void)
{
	if (gCursorX >= gMaxX)
	{
		gCursorX = 0;
		gCursorY++;
	}

	if (gCursorY >= gMaxY)
	{
		scroll();
		gCursorY--;
	}
}

void scroll(void)
{
	unsigned long *src;
	unsigned long *dst;
	int xlimit  = gRowBytes / 4;
	int ylimit  = gHeight - kCharHeight - kHeaderHeight;
	int x,y;

	src = (unsigned long*)(gBaseAddr + (gRowBytes * (kCharHeight + kHeaderHeight)));
	dst = (unsigned long*)(gBaseAddr + (gRowBytes * kHeaderHeight));

	for (y = 0; y < ylimit; y++)
		for (x = 0; x < xlimit; x++)
			*dst++ = *src++;
}

void copyNormalChar(register unsigned char *bptr, register unsigned char *fptr)
{
	int i;

	for (i = 0; i < kCharHeight; i++)
	{
		*bptr++ = *fptr++; *bptr++ = *fptr++; *bptr++ = *fptr++;
		*bptr++ = *fptr++; *bptr++ = *fptr++; *bptr++ = *fptr++;
		bptr += gRowBytes - kCharWidth;
	}
}

void copyBoldChar(register unsigned char *bptr, register unsigned char *fptr)
{
	int i;

	for (i = 0; i < kCharHeight; i++)
	{
		*bptr++ = *fptr++; if (!(*fptr)) *(bptr-1) = *fptr;
		*bptr++ = *fptr++; if (!(*fptr)) *(bptr-1) = *fptr;
		*bptr++ = *fptr++; if (!(*fptr)) *(bptr-1) = *fptr;
		*bptr++ = *fptr++; if (!(*fptr)) *(bptr-1) = *fptr;
		*bptr++ = *fptr++; if (!(*fptr)) *(bptr-1) = *fptr;
		*bptr++ = *fptr++; if (!(*fptr)) *(bptr-1) = *fptr;
		bptr += gRowBytes - kCharWidth;
	}
}

void copyInverseChar(register unsigned char *bptr, register unsigned char *fptr)
{
	int i;

	for (i = 0; i < kCharHeight; i++)
	{
		*bptr++ = ~(*fptr++);
		*bptr++ = ~(*fptr++);
		*bptr++ = ~(*fptr++);
		*bptr++ = ~(*fptr++);
		*bptr++ = ~(*fptr++);
		*bptr++ = ~(*fptr++);
		bptr += gRowBytes - kCharWidth;
	}
}


#if DEBUG
void dump_kprintf_state(void)
{
	printf("gBaseAddr=%lx\tgRowBytes=%d\tgHeight=%d\tgStyle=%d\n",
					(long)gBaseAddr, gRowBytes, gHeight, gStyle);
}
#endif /* DEBUG */

