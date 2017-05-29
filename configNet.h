/* configNet.h - Network configuration header file */

/*
 * Copyright (c) 2012,2013  Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01b,09may13,jjk  WIND00364942 - Adding Unified BSP.
01a,26jun12,j_z  created
*/

#ifndef INCconfigNeth
#define INCconfigNeth

#ifdef __cplusplus
extern "C" {
#endif

#include "vxWorks.h"
#include "end.h"

/* max number of END ipAttachments we can have */

#ifndef IP_MAX_UNITS
#   define IP_MAX_UNITS (NELEMENTS (endDevTbl) - 1)
#endif



/******************************************************************************
*
* END DEVICE TABLE
* ----------------
* Specifies END device instances that will be loaded to the MUX at startup.
*/

END_TBL_ENTRY endDevTbl [] =
    {
    {0, END_TBL_END, NULL, 0, NULL, FALSE}
    };

#ifdef __cplusplus
}
#endif

#endif /* INCconfigNeth */

