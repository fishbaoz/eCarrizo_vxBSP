/* sysMemAutosize.c - Memory autosize routine included by sysLib.c */

/*
 * Copyright (c) 2011-2013 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01d,24may13,jjk  WIND00364942 - Removing AMP and GUEST from documentation
01c,09may13,jjk  WIND00364942 - Adding Unified BSP.
01b,26jun12,j_z  sync up with itl_nehalem bsp
01a,06apr11,lll  unification of itl BSPs
*/

/*
DESCRIPTION
This routine provides an implementation for memory autosizing and is
included only when INCLUDE_LOCAL_MEM_AUTOSIZE is defined. It is expected
to return memTop as a positive value or a NULL.
*/

char * sysMemAutosize (void)
    {
    char * memTop;
    UINT32 * p = (UINT32 *)(MULTIBOOT_SCRATCH);
    MB_INFO* pMbInfo;

    if (*p == MULTIBOOT_BOOTLOADER_MAGIC)
        {
        p = (UINT32 *)(MULTIBOOT_SCRATCH + 8);

        pMbInfo = (MB_INFO*)*p;

        memTop = (char *)(((ULONG)pMbInfo->mem_upper << 10) + 0x100000);
        }
    else
        {
        /* get the calculated top of usable memory from BIOS E820 map */

        memTop = vxBiosE820MemTopPhys32();
        }
    return (memTop);
    }
