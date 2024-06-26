/* Copyright (C) RSA Data Security, Inc. created 1986, 1996.  This is an
   unpublished work protected as such under copyright law.  This work
   contains proprietary, confidential, and trade secret information of
   RSA Data Security, Inc.  Use, disclosure or reproduction without the
   express written authorization of RSA Data Security, Inc. is
   prohibited.
 */

#include "global.h"
#include "bigmath.h"

/* BigClrbit (a, v) -- clears v-th bit of a, where v is nonnegative.
 */
void BigClrbit (a, v)
UINT2 *a;
unsigned int v;
{ 
  a[v/16] &= ~ (1 << (v % 16)); 
}

/* BigSetbit (a, v) -- sets v-th bit of a, where v is nonnegative.
 */
void BigSetbit (a, v)
UINT2 *a;
unsigned int v;
{ 
  a[v/16] |= (1 << (v % 16)); 
}
