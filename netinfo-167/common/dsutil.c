/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "dsrecord.h"

dsrecord *
dsutil_parse_netinfo_string_path(char *path)
{
	dsrecord *p;
	char *c;
	u_int32_t i, sl, kv;
	dsdata *k, *v;
	dsattribute *a;
	
	if (path == NULL) return NULL;
	
	p = dsrecord_new();
	
	c = path;
	while (c[0] == '/') c++;

	while (c[0] != '\0')
	{
		kv = 0;
		for (i = 0; (c[i] != '/') && (c[i] != '\0'); i++)
			if (c[i] == '=') kv = i+1;

		sl = 0;
		if (c[i] == '/')
		{
			sl = 1;
			c[i] = '\0';
		}

		if (kv != 0)
		{
			c[kv-1] = '\0';
			k = cstring_to_dsdata(c);
			v = cstring_to_dsdata(c+kv);
			c[kv-1] = '=';
		}
		else
		{
			k = cstring_to_dsdata("name");
			v = cstring_to_dsdata(c);
		}
	
		if (sl == 1) c[i] = '/';

		a = dsattribute_new(k);
		dsattribute_append(a, v);
		dsdata_release(k);
		dsdata_release(v);

		dsrecord_append_attribute(p, a, SELECT_ATTRIBUTE);
		dsattribute_release(a);

		c += i;
		while (c[0] == '/') c++;
	}

	return p;
}
