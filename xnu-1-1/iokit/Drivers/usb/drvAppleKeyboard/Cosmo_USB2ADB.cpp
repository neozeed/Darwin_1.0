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

/* Cosmo_USB2ADB.cpp
 *  	Copyright Apple Computers, Inc., 1998
 * Created hastily on 12/2/98 by Adam Wang
 * This file does a quick map of USB scan codes to ADB scan codes, based on 
 * the Cosmo USB Tables 1.0
 * I'm not sure what some of the Norsi key conflicts with standard ADB keyboards are.
 */

unsigned char usb_2_adb_keymap[] = 
{
	0x0,	// 0
	0x0,	// 1
	0x0,	// 2
	0x0,	// 3
	0x0,	// 4
	0x0b,	// 5
	0x08,	// 6
	0x02,	// 7
	0x0e,	// 8
	0x03,	// 9
	0x05,	// a
	0x04,	// b
	0x22,	// c
	0x26,	// d
	0x28,	// e
	0x25,	// f
	0x2e,	// 10
	0x2d,	// 11
	0x1f,	// 12
	0x23,	// 13
	0x0c,	// 14
	0x0f,	// 15
	0x01,	// 16
	0x11,	// 17
	0x20,	// 18
	0x09,	// 19
	0x0d,	// 1a
	0x07,	// 1b
	0x10,	// 1c
	0x06,	// 1d
	0x12,	// 1e
	0x13,	// 1f
	0x14,	// 20
	0x15,	// 21
	0x17,	// 22
	0x16,	// 23
	0x1a,	// 24
	0x1c,	// 25
	0x19,	// 26
	0x1d,	// 27
	0x24,	// 28
	0x35,	// 29
	0x33,	// 2a
	0x30,	// 2b
	0x31,	// 2c
	0x1b,	// 2d
	0x18,	// 2e
	0x21,	// 2f
	0x1e,	// 30
	0x2a,	// 31
	0x2a,	// 32
	0x29,	// 33
	0x27,	// 34
	0x32,	// 35
	0x2b,	// 36
	0x2f,	// 37
	0x2c,	// 38
	0x39,	// 39
	0x7a,	// 3a
	0x78,	// 3b
	0x63,	// 3c
	0x76,	// 3d
	0x60,	// 3e
	0x61,	// 3f
	0x62,	// 40
	0x64,	// 41
	0x65,	// 42
	0x6d,	// 43
	0x67,	// 44
	0x6f,	// 45
	0x69,	// 46
	0x6b,	// 47
	0x71,	// 48
	0x72,	// 49
	0x73,	// 4a
	0x74,	// 4b
	0x75,	// 4c
	0x77,	// 4d
	0x79,	// 4e
	0x3c,	// 4f
	0x3b,	// 50
	0x3d,	// 51
	0x3e,	// 52
	0x47,	// 53
	0x4b,	// 54
	0x43,	// 55
	0x4e,	// 56
	0x45,	// 57
	0x4c,	// 58 Not on Cosmo, but is 0x6A for JIS????
	0x53,	// 59
	0x54,	// 5a
	0x55,	// 5b
	0x56,	// 5c
	0x57,	// 5d
	0x58,	// 5e
	0x59,	// 5f
	0x5b,	// 60
	0x5c,	// 61
	0x52,	// 62
	0x41,	// 63
	0x0a,	// 64   ISO only
	0x6e,	// 65   Microsoft Winodows95 key
	0x7f,	// 66   This is the power key, scan code in ADB is 7f 7f, not 7f ff
	0x51,	// 67
	0x0,	// 68
	0x0,	// 69
	0x0,	// 6a
	0x0,	// 6b
	0x0,	// 6c
	0x0,	// 6d
	0x0,	// 6e
	0x0,	// 6f
	0x0,	// 70
	0x0,	// 71
	0x0,	// 72
	0x0,	// 73
	0x0,	// 74
	0x0,	// 75
	0x0,	// 76
	0x0,	// 77
	0x0,	// 78
	0x0,	// 79
	0x0,	// 7a
	0x0,	// 7b
	0x0,	// 7c
	0x0,	// 7d
	0x0,	// 7e
	0x01,	// 7f Norsi Mute, or maybe 0x4a
	0x03,	// 80 Norsi volume up, otherwise is 0x48 in ADB
	0x02,	// 81 Norsi volume down
	0x0,	// 82
	0x0,	// 83
	0x0,	// 84
	0x5f,	// 85  , JIS only
	0x0,	// 86
	0x5e,	// 87  Ro (JIS)
	0x0,	// 88
	0x5d,	// 89  Yen (JIS)
	0x0,	// 8a
	0x0,	// 8b
	0x0,	// 8c
	0x0,	// 8d
	0x0,	// 8e
	0x0,	// 8f
	0x68,	// 90  Kana
	0x66,	// 91  Eisu
	0x0,	// 92
	0x0,	// 93
	0x0,	// 94
	0x0,	// 95
	0x0,	// 96
	0x0,	// 97
	0x0,	// 98
	0x0,	// 99
	0x0,	// 9a
	0x0,	// 9b
	0x0,	// 9c
	0x0,	// 9d
	0x0,	// 9e
	0x0,	// 9f
	0x0,	// a0
	0x0,	// a1
	0x0,	// a2
	0x0,	// a3
	0x0,	// a4
	0x0,	// a5
	0x0,	// a6
	0x0,	// a7
	0x0,	// a8
	0x0,	// a9
	0x0,	// aa
	0x0,	// ab
	0x0,	// ac
	0x0,	// ad
	0x0,	// ae
	0x0,	// af
	0x0,	// b0
	0x0,	// b1
	0x0,	// b2
	0x0,	// b3
	0x0,	// b4
	0x0,	// b5
	0x0,	// b6
	0x0,	// b7
	0x0,	// b8
	0x0,	// b9
	0x0,	// ba
	0x0,	// bb
	0x0,	// bc
	0x0,	// bd
	0x0,	// be
	0x0,	// bf
	0x0,	// c0
	0x0,	// c1
	0x0,	// c2
	0x0,	// c3
	0x0,	// c4
	0x0,	// c5
	0x0,	// c6
	0x0,	// c7
	0x0,	// c8
	0x0,	// c9
	0x0,	// ca
	0x0,	// cb
	0x0,	// cc
	0x0,	// cd
	0x0,	// ce
	0x0,	// cf
	0x0,	// d0
	0x0,	// d1
	0x0,	// d2
	0x0,	// d3
	0x0,	// d4
	0x0,	// d5
	0x0,	// d6
	0x0,	// d7
	0x0,	// d8
	0x0,	// d9
	0x0,	// da
	0x0,	// db
	0x0,	// dc
	0x0,	// dd
	0x0,	// de
	0x0,	// df
	0x36,	// e0
	0x38,	// e1 Left Shift
	0x3a,	// e2
	0x37,	// e3
	0x7d,	// e4 Right Control, not on iMac Cosmo
	0x7b,	// e5 Right Shift
	0x7c,	// e6 Right Option JIS only
	0x37,	// e7 Right Command ADB is 7e, but ADBK_FLOWER is fixed for 0x37
	0x0,	// e8
	0x0,	// e9
	0x0,	// ea
	0x0,	// eb
	0x0,	// ec
	0x0,	// ed
	0x0,	// ee
	0x0,	// ef
	0x0,	// f0
	0x0,	// f1
	0x0,	// f2
	0x0,	// f3
	0x0,	// f4
	0x0,	// f5
	0x0,	// f6
	0x0,	// f7
	0x0,	// f8
	0x0,	// f9
	0x0,	// fa
	0x0,	// fb
	0x0,	// fc
	0x0,	// fd
	0x0,	// fe
	0x0,	// ff
	0	//Final one
};

