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
#include "aichencn.h"

B_AlgorithmInfoType AIT_RSAPrivate = {&AITChooseEncryptNull_V_TABLE};

int AI_RSAPrivate (infoType)
POINTER *infoType;
{
  *infoType = (POINTER)&AIT_RSAPrivate;

  /* Return 0 to indicate a B_AlgorithmInfoType, not a B_KeyInfoType */
  return (0);
}

