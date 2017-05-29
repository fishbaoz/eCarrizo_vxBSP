/* sysPhysMemDesc.c - Initial memory map descriptors included by sysLib.c */

/*
 * Copyright (c) 2011, 2013 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01c,24may13,jjk  WIND00364942 - Removing AMP and GUEST
01b,09may13,jjk  WIND00364942 - Adding Unified BSP.
01a,06apr11,lll  unification of itl BSPs
*/

/*
DESCRIPTION
A code fragment containing declaration of initial memory map descriptors
included by sysLib.c. Static configuration for mapping of additional memory
regions may require addition of specific descriptors for those regions.
Otherwise, additional dummy entries are provided for dynamic mappings.
*/


/* declare initial memory map descriptors */

PHYS_MEM_DESC sysPhysMemDesc [] =
    {

    /* adrs and length parameters must be page-aligned (multiples of 4KB) */

    /*
     * Any code access to the invalid address range will generate
     * a MMU exception and the offending task will be suspended.
     * Then use l(), lkAddr, ti(), and tt() to find the NULL access.
     */

    /* lower memory for invalid access */

    {
    (VIRT_ADDR) 0x0,
    (PHYS_ADDR) 0x0,
    _WRS_BSP_VM_PAGE_OFFSET,
    VM_STATE_MASK_FOR_ALL,
    VM_STATE_FOR_MEM_OS
    },

    /* lower memory for valid access */

    {
    (VIRT_ADDR) _WRS_BSP_VM_PAGE_OFFSET,
    (PHYS_ADDR) _WRS_BSP_VM_PAGE_OFFSET,
    0xa0000 - _WRS_BSP_VM_PAGE_OFFSET,
    VM_STATE_MASK_FOR_ALL,
    VM_STATE_FOR_MEM_OS
    },

    /* video ram, etc */

    {
    (VIRT_ADDR) 0x000a0000,
    (PHYS_ADDR) 0x000a0000,
    0x00060000,
    VM_STATE_MASK_FOR_ALL,
    VM_STATE_FOR_IO
    },

    /* upper memory for OS */

    {
    (VIRT_ADDR) LOCAL_MEM_LOCAL_ADRS,
    (PHYS_ADDR) LOCAL_MEM_LOCAL_ADRS,
    LOCAL_MEM_SIZE_OS,
    VM_STATE_MASK_FOR_ALL,
    VM_STATE_FOR_MEM_OS
    },

    /* upper memory for Application */

    {
    (VIRT_ADDR) LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE_OS,
    (PHYS_ADDR) LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE_OS,
    LOCAL_MEM_SIZE - LOCAL_MEM_SIZE_OS, /* it is changed in sysMemTop() */
    VM_STATE_MASK_FOR_ALL,
    VM_STATE_FOR_MEM_APPLICATION
    },

#   if defined(INCLUDE_SM_NET) && (SM_MEM_ADRS != 0x0)

    /* upper memory for sm net/obj pool */

    {
    (VIRT_ADDR) SM_MEM_ADRS,
    (PHYS_ADDR) SM_MEM_ADRS,
    SM_MEM_SIZE + SM_OBJ_MEM_SIZE,
    VM_STATE_MASK_FOR_ALL,
    VM_STATE_FOR_MEM_APPLICATION
    },

#   endif /* defined(INCLUDE_SM_NET) && (SM_MEM_ADRS != 0x0) */

   /* entries for dynamic mappings - create sufficient entries */

    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY,
    DUMMY_MMU_ENTRY
    };

