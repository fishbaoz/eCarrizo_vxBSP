/* 00bsp.cdf - BSP configuration file */

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
01f,16may13,jjk  WIND00364942 - Swap RAM_HIGH and RAM_LOW to conform to IA
                 arch in 6.9
01e,09may13,jjk  WIND00364942 - Adding Unified BSP.
01d,31jul12,lll  change RAM_LOW/RAM_HIGH to
                 VXWORKS_ENTRY_ADRS/BOOTAPP_ENTRY_ADRS
01c,25jun12,j_z  sync with itl_nehalem; move AMP & CLK param defaults to
                 config.h
01b,25mar11,lll  unification of itl BSPs
01a,28janll,lll  Adapted from Intel QM57, Intel Atom and pcPentium4
*/

/*
DESCRIPTION
This file overrides generic BSP components in comps/vxWorks/00bsp.cdf with
BSP-specific versions of components and parameters defined in the
generic CDF file.
*/


/*******************************************************************************
*
* Memory definitions
*
*/

Component INCLUDE_MEMORY_CONFIG {
    NAME            BSP Memory Configuration
    SYNOPSIS        Memory configuration parameters for BSP
    CFG_PARAMS      LOCAL_MEM_LOCAL_ADRS \
                    LOCAL_MEM_SIZE \
                    LOCAL_MEM_AUTOSIZE \
                    LOCAL_MEM_PHYS_ADRS \
                    LOCAL_UNMAPPED_BASE_ADRS \
                    NV_RAM_SIZE \
                    NV_BOOT_OFFSET \
                    VEC_BASE_ADRS \
                    EXC_MSG_OFFSET \
                    EXC_MSG_ADRS \
                    BOOT_LINE_SIZE \
                    BOOT_LINE_ADRS \
                    BOOT_LINE_OFFSET \
                    RESERVED \
                    FREE_RAM_ADRS \
                    ROM_WARM_ADRS \
                    STACK_SAVE \
                    RAM_HIGH_ADRS \
                    RAM_LOW_ADRS \
                    ROM_BASE_ADRS \
                    ROM_TEXT_ADRS \
                    ROM_SIZE
}

Parameter LOCAL_MEM_SIZE {
    NAME        Offset from the start of on-board memory to the top of memory
    DEFAULT     (SYSTEM_RAM_SIZE - LOCAL_MEM_LOCAL_ADRS)
}

Parameter LOCAL_MEM_LOCAL_ADRS {
    NAME        Physical memory base address
    DEFAULT     0x00100000
}

Parameter RAM_HIGH_ADRS {
    NAME        Bootrom Copy region
    DEFAULT     (INCLUDE_BOOT_APP)::(0x00608000) \
                0x10008000
}

Parameter RAM_LOW_ADRS {
    NAME        Runtime kernel load address
    DEFAULT     (INCLUDE_BOOT_RAM_IMAGE)::(0x15008000) \
                (INCLUDE_BOOT_APP)::(0x10008000) \
                0x00408000
}

/*******************************************************************************
*
* System Clock, Auxiliary Clock and Timestamp Component and Parameters
*
*/

Component INCLUDE_TIMESTAMP {
#ifdef _WRS_CONFIG_SMP
    REQUIRES INCLUDE_HPET_TIMESTAMP
#else
    REQUIRES        DRV_TIMER_IA_TIMESTAMP
#endif /* _WRS_CONFIG_SMP */
}

Parameter SYS_CLK_RATE_MAX  {
    NAME            system clock configuration parameter
    SYNOPSIS        maximum system clock rate
    TYPE            uint
}

Parameter SYS_CLK_RATE_MIN  {
    NAME            system clock configuration parameter
    SYNOPSIS        minimum system clock rate
    TYPE            uint
}

Parameter SYS_CLK_RATE {
    NAME            system clock configuration parameter
    SYNOPSIS        number of ticks per second
    TYPE            uint
}

Component INCLUDE_AUX_CLK  {
    NAME            Auxiliary clock
    SYNOPSIS        Auxiliary clock component
    REQUIRES        DRV_TIMER_I8253
}

Parameter AUX_CLK_RATE_MAX  {
    NAME            auxiliary clock configuration parameter
    SYNOPSIS        maximum auxiliary clock rate
    TYPE            uint
}

Parameter AUX_CLK_RATE_MIN  {
    NAME            auxiliary clock configuration parameter
    SYNOPSIS        minimum auxiliary clock rate
    TYPE            uint
}

Parameter AUX_CLK_RATE  {
    NAME            auxiliary clock configuration parameter
    SYNOPSIS        default auxiliary clock rate
    TYPE            uint
}

Component INCLUDE_HPET_TIMESTAMP  {
    NAME              HPET timestamp
    SYNOPSIS          HPET timestamp component
    REQUIRES          DRV_TIMER_IA_HPET \
                      DRV_TIMER_IA_TIMESTAMP
    _CHILDREN         FOLDER_DRIVERS
}

/*******************************************************************************
*
* Cache Configuration Parameters
*
*/

Parameter USER_D_CACHE_MODE  {
    NAME            data cache mode
    SYNOPSIS        write-back data cache mode
    TYPE            uint
    DEFAULT         (CACHE_COPYBACK | CACHE_SNOOP_ENABLE)
}

/*******************************************************************************
*
* Additional Intel Architecture show routines
*
*/

Component INCLUDE_INTEL_CPU_SHOW {
    NAME            Intel Architecture processor show routines
    SYNOPSIS        IA-32 processor show routines
    HDR_FILES       vxLib.h
    MODULES         vxShow.o
    INIT_RTN        vxShowInit ();
    _INIT_ORDER     usrShowInit
    _CHILDREN       FOLDER_SHOW_ROUTINES
    _DEFAULTS       += FOLDER_SHOW_ROUTINES
}

/*******************************************************************************
*
* BSP-specific configuration folder
*
*/

Folder FOLDER_BSP_CONFIG  {
    NAME            BSP configuration options
    SYNOPSIS        BSP-specific configuration
    CHILDREN        INCLUDE_ITL_PARAMS \
                    INCLUDE_BOARD_CONFIG \
                    INCLUDE_DEBUG_STORE
    DEFAULTS        INCLUDE_ITL_PARAMS \
                    INCLUDE_BOARD_CONFIG
    _CHILDREN       FOLDER_HARDWARE
    _DEFAULTS       += FOLDER_HARDWARE
}

/*******************************************************************************
*
* BSP parameters Component
*
*/

Component INCLUDE_ITL_PARAMS  {
    NAME            BSP build parameters
    SYNOPSIS        expose BSP configurable parameters
    LAYER           1
#ifdef _WRS_CONFIG_SMP
    CFG_PARAMS      INCLUDE_MTRR_GET    \
                    SYS_AP_LOOP_COUNT \
                    SYS_AP_TIMEOUT \
                    INCLUDE_PMC
#else
    CFG_PARAMS      INCLUDE_MTRR_GET    \
                    INCLUDE_PMC
#endif /* _WRS_CONFIG_SMP */
    HELP            ITL_HELP
}

Parameter INCLUDE_MTRR_GET  {
    NAME            bsp configuration parameter
    SYNOPSIS        get Memory Type Range Register settings from the BIOS
    TYPE            exists
    DEFAULT         TRUE
}

Parameter INCLUDE_PMC  {
    NAME            bsp configuration parameter
    SYNOPSIS        Performance Monitoring Counter library support
    TYPE            exists
    DEFAULT         TRUE
}


/*******************************************************************************
*
* Physical Address Space Components
*
*/

Component INCLUDE_MMU_P6_32BIT  {
    NAME            32-bit physical address space
    SYNOPSIS        configure 32-bit address space support
    CFG_PARAMS      VM_PAGE_SIZE
    EXCLUDES        INCLUDE_MMU_P6_36BIT
    _CHILDREN       FOLDER_MMU
    _DEFAULTS       += FOLDER_MMU
    HELP            ITL_HELP
}


Parameter VM_PAGE_SIZE {
    NAME            VM page size
    SYNOPSIS        virtual memory page size (PAGE_SIZE_{4KB/2MB/4MB})
    TYPE            uint
    DEFAULT         PAGE_SIZE_4KB
}

/*******************************************************************************
*
* Debug Store BTS/PEBS Component and Parameters
*
*/

Component INCLUDE_DEBUG_STORE  {
    NAME            Debug Store BTS/PEBS support
    SYNOPSIS        configure Debug Store BTS/PEBS support
    CFG_PARAMS      DS_SYS_MODE \
                    BTS_ENABLED \
                    BTS_INT_MODE \
                    BTS_BUF_MODE \
                    PEBS_ENABLED \
                    PEBS_EVENT \
                    PEBS_METRIC \
                    PEBS_OS \
                    PEBS_RESET
    HELP            ITL_HELP
}

Parameter DS_SYS_MODE {
    NAME            Debug Store BTS/PEBS operating mode
    SYNOPSIS        configure the Debug Store BTS/PEBS operating mode
    TYPE            bool
    DEFAULT         FALSE
}

Parameter BTS_ENABLED {
    NAME            enable or disable the BTS
    SYNOPSIS        enable or disable the BTS
    TYPE            bool
    DEFAULT         TRUE
}

Parameter BTS_INT_MODE {
    NAME            configure the BTS interrupt mode
    SYNOPSIS        configure the BTS interrupt mode
    TYPE            bool
    DEFAULT         TRUE
}

Parameter BTS_BUF_MODE {
    NAME            configure the BTS buffering mode
    SYNOPSIS        configure the BTS buffering mode
    TYPE            bool
    DEFAULT         TRUE
}

Parameter PEBS_ENABLED {
    NAME            enable or disable the PEBS
    SYNOPSIS        enable or disable the PEBS
    TYPE            bool
    DEFAULT         TRUE
}

Parameter PEBS_EVENT {
    NAME            specify the PEBS event
    SYNOPSIS        specify the PEBS event
    TYPE            uint
    DEFAULT         PEBS_FRONT_END
}

Parameter PEBS_METRIC {
    NAME            specify the PEBS metric
    SYNOPSIS        specify the PEBS metric
    TYPE            uint
    DEFAULT         PEBS_MEMORY_STORES
}

Parameter PEBS_OS {
    NAME            configure the PEBS execution mode
    SYNOPSIS        configure the PEBS execution mode
    TYPE            bool
    DEFAULT         TRUE
}

Parameter PEBS_RESET {
    NAME            specify the PEBS reset counter value
    SYNOPSIS        specify the PEBS reset counter value
    TYPE            uint
    DEFAULT         (-1LL)
}

/*******************************************************************************
*
* MPAPIC initialization component
*
*/

Component INCLUDE_USR_MPAPIC {
    NAME            MPAPIC boot component
    SYNOPSIS        MPAPIC boot initialization support
    CONFIGLETTES    usrMpapic.c
    _CHILDREN       FOLDER_NOT_VISIBLE
    INCLUDE_WHEN    INCLUDE_VXBUS
}

