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
/*
 * Copyright (C) 1989, NeXT Computer, Inc.
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 */
#ifndef _SYS_METALINK_H_
#define _SYS_METALINK_H_


/*
 * metalink.h
 *
 * This structure describes the different expressions that will be
 * expanded into current kernel values when found in symbolic links.
 */

#define	ML_ESCCHAR	'$'	/* The escape character */

struct metalink {
	char *ml_token;		/* The token to be changed */
	char *ml_variable;	/* The kernel variable to substitute with */
	char *ml_default;	/* The default character */
};

extern char boot_file[];

struct metalink metalinks[] = {
	{ "BOOTFILE", boot_file, NULL },
	{ NULL, NULL, NULL }
};

#endif /* ! _SYS_METALINK_H_ */

