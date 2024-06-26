/* Copyright (C) RSA Data Security, Inc. created 1990, 1996.  This is an
   unpublished work protected as such under copyright law.  This work
   contains proprietary, confidential, and trade secret information of
   RSA Data Security, Inc.  Use, disclosure or reproduction without the
   express written authorization of RSA Data Security, Inc. is
   prohibited.
 */

#include "global.h"
#include "bsafe2.h"
#include "bkey.h"
#include "balg.h"
#include "ainfotyp.h"
#include "ainull.h"

int AITNullAddInfo (infoType, algorithm, info)
B_AlgorithmInfoType *infoType;
B_Algorithm *algorithm;
POINTER info;
{
UNUSED_ARG (info)
  /* Cache null parameters. */
  return (B_InfoCacheAddInfo
          (&algorithm->infoCache, (POINTER)infoType, NULL_PTR));
}

