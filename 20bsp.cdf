/* 20bsp.cdf - BSP component description file */

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
01i,24may13,jjk  WIND00364942 - Removing AMP and GUEST from documentation
01h,17may13,rbc  WIND00364942 - Banner changes for UBSP
01g,16may13,jjk  WIND00364942 - Swap RAM_HIGH and RAM_LOW to conform to IA
                 arch in 6.9
01f,09may13,jjk  WIND00364942 - Adding Unified BSP.
01e,08nov12,lll  add sections for clock and early print parameter defaults
01d,08oct12,c_t  add SMI disable option
01c,06jul12,j_z  sync up with itl_nehalem bsp.
01b,25mar11,lll  unification of itl BSPs
01a,28jan11,lll  Adapted from Intel QM57, Intel Atom and pcPentium4
*/

Bsp eKabini_RTL816X_6948 {
    NAME            board support package
    CPU             PENTIUM4
    REQUIRES        INCLUDE_KERNEL \
                    INCLUDE_PCI \
                    INCLUDE_PENTIUM_PCI \
                    INCLUDE_PCI_OLD_CONFIG_ROUTINES \
                    INCLUDE_TIMER_SYS \
                    INCLUDE_TIMER_SYS_DELAY \
                    INCLUDE_MMU_P6_32BIT \
                    SELECT_INTERRUPT_MODE \
                    SELECT_SYS_WARM_TYPE \
                    SELECT_MEM_AUTOSIZE \
                    INCLUDE_ITL_PARAMS \
                    INCLUDE_BOARD_CONFIG
    FP              hard
    MP_OPTIONS      UP SMP
    GUEST_OS_OPTION NOT_SUPPORTED
}

Component INCLUDE_BOARD_CONFIG {
    NAME            Board Configuration Component
    SYNOPSIS        Fundamental Board Configuration Component.
    REQUIRES        DEFAULT_BOOT_LINE \
                    SYS_MODEL
#if (defined _WRS_CONFIG_SMP)
    CFG_PARAMS      DEFAULT_BOOT_LINE \
                    SYS_AP_TIMEOUT \
                    SYS_AP_LOOP_COUNT \
                    SYS_MODEL
#else
    CFG_PARAMS      DEFAULT_BOOT_LINE \
                    SYS_MODEL
#endif /* _WRS_CONFIG_SMP */
}

/*
 * Network Boot Devices for a BSP
 * The REQUIRES line should be modified for a BSP.
 */

Component INCLUDE_BOOT_NET_DEVICES {
    REQUIRES        INCLUDE_FEI8255X_VXB_END \
                    INCLUDE_GEI825XX_VXB_END \
                    INCLUDE_RTL8169_VXB_END \
                    DRV_VXBEND_INTELTOPCLIFF \
                    DRV_RESOURCE_INTELTOPCLIFF_IOH
}

/* Specify boot rom console device for this BSP */

Component INCLUDE_BOOT_SHELL {
    REQUIRES +=     DRV_SIO_NS16550
}

/*
 * Filesystem Boot Devices for a BSP
 * The REQUIRES line should be modified for a BSP.
 */

Component INCLUDE_BOOT_FS_DEVICES {
    REQUIRES        INCLUDE_BOOT_USB_FS_LOADER \
                    INCLUDE_DOSFS \
                    INCLUDE_USB \
                    INCLUDE_USB_INIT \
                    INCLUDE_EHCI \
                    INCLUDE_EHCI_INIT \
                    INCLUDE_UHCI \
                    INCLUDE_UHCI_INIT \
                    INCLUDE_OHCI \
                    INCLUDE_OHCI_INIT \
                    INCLUDE_USB_MS_BULKONLY \
                    INCLUDE_USB_MS_BULKONLY_INIT
}

/* Disable SMI */

Component INCLUDE_DISABLE_SMI {
    NAME            Disable SMI
    SYNOPSIS        Disable the following System Management Interrupt sources:\
                    USB legacy interrupt, USB 2.0 legacy interrupt, TCO Timer
    REQUIRES        INCLUDE_PCI_BUS
    CFG_PARAMS      SMI_GLOBAL_DISABLE
    _CHILDREN       FOLDER_BSP_CONFIG
}

/* Disable SMI globally */

Parameter SMI_GLOBAL_DISABLE {
    NAME            Disable SMI globally
    SYNOPSIS        Setting this parameter to true will disable the SMI globally,\
                    so do it carefully, it may damage your hardware. Enable it under\
                    instruction of your hardware vendor.
    TYPE            BOOL
    DEFAULT         (FALSE)
}

/* Delay time after USB Bulk Dev Init */

Parameter BOOT_USB_POST_MS_BULKONLY_INIT_DELAY {
    NAME            Delay time after USB Bulk Dev Init
    SYNOPSIS        Amount of time in system ticks to delay after Bulk Init. \
                    This allows the USB Task to run and discovery to occur \
                    prior to attempting to crack the boot line.
    TYPE            int
    DEFAULT         (2)
}


/*
 * Warm boot device selection required for
 * NVRAM support
 */

Selection SELECT_SYS_WARM_TYPE {
    NAME            Warm start device selection
    COUNT           1-1
    CHILDREN        INCLUDE_SYS_WARM_BIOS \
                    INCLUDE_SYS_WARM_FD \
                    INCLUDE_SYS_WARM_TFFS \
                    INCLUDE_SYS_WARM_USB \
                    INCLUDE_SYS_WARM_ICH \
                    INCLUDE_SYS_WARM_AHCI
    DEFAULTS        INCLUDE_SYS_WARM_BIOS
    _CHILDREN       FOLDER_BSP_CONFIG
}

/*
 * Warm boot device components
 */

Component INCLUDE_SYS_WARM_BIOS {
    NAME            BIOS warm start device component
    CFG_PARAMS      SYS_WARM_TYPE \
                    NV_RAM_SIZE
}

Component INCLUDE_SYS_WARM_FD {
    NAME            FD warm start device component
    CFG_PARAMS      SYS_WARM_TYPE \
                    NV_RAM_SIZE \
                    BOOTROM_DIR
}

Component INCLUDE_SYS_WARM_USB {
    NAME            USB warm start device component
    REQUIRES        INCLUDE_USB \
                    INCLUDE_USB_INIT \
                    INCLUDE_EHCI \
                    INCLUDE_EHCI_INIT \
                    INCLUDE_USB_MS_BULKONLY \
                    INCLUDE_USB_MS_BULKONLY_INIT \
                    INCLUDE_DOSFS
    CFG_PARAMS      SYS_WARM_TYPE \
                    NV_RAM_SIZE \
                    BOOTROM_DIR
}


Component INCLUDE_SYS_WARM_ICH {
    NAME            ICH warm start device component
    REQUIRES        INCLUDE_DRV_STORAGE_INTEL_ICH \
                    INCLUDE_DOSFS
    CFG_PARAMS      SYS_WARM_TYPE \
                    NV_RAM_SIZE \
                    BOOTROM_DIR
}

Component INCLUDE_SYS_WARM_AHCI {
    NAME            AHCI warm start device component
    REQUIRES        INCLUDE_DRV_STORAGE_INTEL_AHCI \
                    INCLUDE_DOSFS
    CFG_PARAMS      SYS_WARM_TYPE \
                    NV_RAM_SIZE \
                    BOOTROM_DIR
}

Parameter SYS_WARM_TYPE {
    NAME            Warm start device parameter
    DEFAULT         (INCLUDE_SYS_WARM_BIOS)::(SYS_WARM_BIOS) \
                    (INCLUDE_SYS_WARM_FD)::(SYS_WARM_FD) \
                    (INCLUDE_SYS_WARM_USB)::(SYS_WARM_USB) \
                    (INCLUDE_SYS_WARM_ICH)::(SYS_WARM_ICH) \
                    (INCLUDE_SYS_WARM_AHCI)::(SYS_WARM_AHCI) \
                    (SYS_WARM_BIOS)
}

Parameter NV_RAM_SIZE {
}

Parameter BOOTROM_DIR {
    DEFAULT         (INCLUDE_SYS_WARM_FD)::("/fd0") \
                    (INCLUDE_SYS_WARM_USB)::("/bd0") \
                    (INCLUDE_SYS_WARM_ICH)::("/ata000:1") \
                    (INCLUDE_SYS_WARM_AHCI)::("/ahci00:1") \
                    (NULL)
}

/*
 * VX_SMP_NUM_CPUS is a SMP parameter only and only available for SMP
 * builds. Due to a limitation of the project tool at the time this
 * parameter is created where the tool can not recognize the ifdef SMP
 * selection, this parameter is set up such that _CFG_PARAMS is not
 * specified here. In the 00vxWorks.cdf file, where the parameter
 * VX_SMP_NUM_CPUS is defined, the _CFG_PARAMS is specified only for
 * VxWorks SMP. Hence the redefining of VX_SMP_NUM_CPUS here should only
 * override the value and not the rest of the properties. And for UP, the
 * parameter is ignored since the parameter is not tied to any component
 * (_CFG_PARAMS is not specified).
 */

Parameter VX_SMP_NUM_CPUS {
    NAME            Number of CPUs available to be enabled for VxWorks SMP
    TYPE            UINT
    DEFAULT         (8)
}

/*******************************************************************************
*
* Memory and address configuration
*
*/

Parameter ROM_TEXT_ADRS {
    NAME            ROM text address
    SYNOPSIS        ROM text address
    DEFAULT         (0x00008000)
}

Parameter ROM_SIZE {
    NAME            ROM size
    SYNOPSIS        ROM size
    DEFAULT         (INCLUDE_BOOT_APP)::(0x00800000) \
                    (0x00090000)
}

Parameter ROM_BASE_ADRS {
    NAME            ROM base address
    SYNOPSIS        ROM base address
    DEFAULT         (ROM_TEXT_ADRS)
}

Parameter SYS_MODEL {
    NAME            System Model
    SYNOPSIS        System Model string
    TYPE            string
#if (!defined _WRS_CONFIG_SMP)
    DEFAULT         (INCLUDE_SYMMETRIC_IO_MODE && INCLUDE_ACPI_BOOT_OP)::("AMD eKabini Processor SYMMETRIC IO ACPI") \
                    (INCLUDE_SYMMETRIC_IO_MODE && INCLUDE_MPTABLE_BOOT_OP)::("AMD eKabini Processor SYMMETRIC IO MPTABLE") \
                    (INCLUDE_SYMMETRIC_IO_MODE && INCLUDE_USR_BOOT_OP)::("AMD eKabini Processor SYMMETRIC IO USRTABLE") \
                    (INCLUDE_SYMMETRIC_IO_MODE)::("AMD eKabini Processor SYMMETRIC IO") \
                    (INCLUDE_VIRTUAL_WIRE_MODE && INCLUDE_ACPI_BOOT_OP)::("AMD eKabini Processor VIRTUAL WIRE ACPI") \
                    (INCLUDE_VIRTUAL_WIRE_MODE && INCLUDE_MPTABLE_BOOT_OP)::("AMD eKabini Processor VIRTUAL WIRE MPTABLE") \
                    (INCLUDE_VIRTUAL_WIRE_MODE && INCLUDE_USR_BOOT_OP)::("AMD eKabini Processor VIRTUAL WIRE USRTABLE") \
                    (INCLUDE_VIRTUAL_WIRE_MODE)::("AMD eKabini Processor Pentium4 VIRTUAL WIRE") \
                    (INCLUDE_ACPI_BOOT_OP)::("AMD eKabini Processor ACPI") \
                    (INCLUDE_MPTABLE_BOOT_OP)::("AMD eKabini Processor MPTABLE") \
                    (INCLUDE_USR_BOOT_OP)::("AMD eKabini Processor USRTABLE") \
                    ("AMD eKabini Processor")
#else
    DEFAULT         (INCLUDE_ACPI_BOOT_OP && INCLUDE_SMP_SCHED_SMT_POLICY)::("AMD eKabini Processor SYMMETRIC IO SMP/SMT ACPI") \
                    (INCLUDE_ACPI_BOOT_OP)::("AMD eKabini Processor SYMMETRIC IO SMP ACPI") \
                    (INCLUDE_MPTABLE_BOOT_OP && INCLUDE_SMP_SCHED_SMT_POLICY)::("AMD eKabini Processor SYMMETRIC IO SMP/SMT MPTABLE") \
                    (INCLUDE_MPTABLE_BOOT_OP)::("AMD eKabini Processor SYMMETRIC IO SMP MPTABLE") \
                    (INCLUDE_USR_BOOT_OP && INCLUDE_SMP_SCHED_SMT_POLICY)::("AMD eKabini Processor SYMMETRIC IO SMP/SMT USRTABLE") \
                    (INCLUDE_USR_BOOT_OP)::("AMD eKabini Processor SYMMETRIC IO SMP USRTABLE") \
                    (INCLUDE_SMP_SCHED_SMT_POLICY)::("AMD eKabini Processor SYMMETRIC IO SMP/SMT") \
                    ("AMD eKabini Processor SYMMETRIC IO SMP")
#endif
}

Parameter DEFAULT_BOOT_LINE {
    NAME            default boot line
    SYNOPSIS        Default boot line string
    TYPE            string
    DEFAULT         ("gei(0,0)host:vxWorks h=90.0.0.3 e=90.0.0.50 u=target1 s=/ata0:1/start.txt")
}

/*
 * SYS_AP_LOOP_COUNT is a SMP parameter used in function sysCpuStart.
 * It is used to set the count of times to check if an application
 * processor succeeded in starting up.
 */

Parameter SYS_AP_LOOP_COUNT {
    NAME            System AP startup checks
    SYNOPSIS        Maximum times to check if an application processor started
    TYPE            uint
    DEFAULT         (200000)
}

/*
 * SYS_AP_TIMEOUT is a SMP parameter used in function sysCpuStart.
 * It is used to set the time in microseconds to wait between checking
 * if an application processor succeeded in starting up.
 * The time is specified in microseconds and should be short in duration.
 * SYS_AP_LOOP_COUNT * SYS_AP_TIMEOUT gives the total time to wait for an AP
 * before giving up and moving on to the next application processor.
 */

Parameter SYS_AP_TIMEOUT {
    NAME            System AP startup timeout
    SYNOPSIS        Time between each check to see if application processor started
    TYPE            uint
    DEFAULT         (10)
}

/*******************************************************************************
*
* HWCONF component
*
* Required when INCLUDE_VXBUS is defined, however (see config.h & sysLib.c):
*
*   if (defined INCLUDE_HWCONF)
*       <hwconf.c included here as a configlette>
*   else
*       <hwconf.c included via sysLib.c for customer BSP compatibility>
*/

Component INCLUDE_HWCONF {
    NAME            hardware configuration
    SYNOPSIS        hardware configuration support
    CONFIGLETTES    hwconf.c
    _CHILDREN       FOLDER_NOT_VISIBLE
    _REQUIRES       INCLUDE_VXBUS
}

/*******************************************************************************
*
* Interrupt Mode Configuration
*
*/

Folder FOLDER_INTERRUPT_MODE {
    NAME            Interrupt mode options
    CHILDREN        SELECT_INTERRUPT_MODE \
                    SELECT_MP_BOOT_OP
    DEFAULTS        SELECT_INTERRUPT_MODE
    _CHILDREN       FOLDER_BSP_CONFIG
}

Selection SELECT_INTERRUPT_MODE {
    NAME            Interrupt mode selection
    COUNT           1-1
#if (defined _WRS_CONFIG_SMP)
    CHILDREN        INCLUDE_SYMMETRIC_IO_MODE   /* SMP: must be SYMIO */
    DEFAULTS        INCLUDE_SYMMETRIC_IO_MODE
#else
    CHILDREN        INCLUDE_SYMMETRIC_IO_MODE \
                    INCLUDE_VIRTUAL_WIRE_MODE \
                    INCLUDE_PIC_MODE
    DEFAULTS        INCLUDE_SYMMETRIC_IO_MODE
#endif /* _WRS_CONFIG_SMP */
}

Component INCLUDE_SYMMETRIC_IO_MODE {
    NAME            APIC mode - Symmetric
    SYNOPSIS        Symmetric I/O interrupt mode
#if (defined _WRS_CONFIG_SMP)
    REQUIRES        INCLUDE_APIC_MODE \
                    DRV_INTCTLR_IOAPIC \
                    DRV_TIMER_LOAPIC \
                    DRV_TIMER_IA_HPET
#else
    REQUIRES        INCLUDE_APIC_MODE \
                    DRV_INTCTLR_IOAPIC \
                    DRV_TIMER_LOAPIC \
                    DRV_TIMER_IA_TIMESTAMP
#endif /* _WRS_CONFIG_SMP */
    CFG_PARAMS      APIC_TIMER_CLOCK_HZ
}

Component INCLUDE_VIRTUAL_WIRE_MODE {
    NAME            APIC mode - Virtual Wire
    SYNOPSIS        Virtual wire interrupt mode
    REQUIRES        INCLUDE_APIC_MODE \
                    DRV_TIMER_LOAPIC \
                    DRV_TIMER_I8253 \
                    DRV_TIMER_IA_TIMESTAMP
    CFG_PARAMS      APIC_TIMER_CLOCK_HZ
}

Component INCLUDE_PIC_MODE {
    NAME            PIC mode
    SYNOPSIS        PIC interrupt mode
    REQUIRES        DRV_INTCTLR_I8259 \
                    DRV_TIMER_I8253 \
                    DRV_TIMER_IA_TIMESTAMP \
                    INCLUDE_NO_BOOT_OP
}

Component INCLUDE_APIC_MODE {
    NAME            APIC mode
    SYNOPSIS        APIC mode base settings
    REQUIRES        INCLUDE_INTCTLR_DYNAMIC_LIB \
                    DRV_INTCTLR_MPAPIC \
                    DRV_INTCTLR_LOAPIC \
                    SELECT_MP_BOOT_OP
}

Parameter APIC_TIMER_CLOCK_HZ  {
    NAME            APIC timer clock rate parameter
    SYNOPSIS        APIC timer clock rate (0 is auto-calculate)
    TYPE            uint
    DEFAULT         (0)
}

/*******************************************************************************
*
* MP Table Configuration Options
*
*/

Selection SELECT_MP_BOOT_OP {
    NAME            MP table options for APIC mode
    SYNOPSIS        Selects MP APIC struct creation method (DO NOT use with PIC mode)
    COUNT           1-1
    CHILDREN        INCLUDE_ACPI_BOOT_OP \
                    INCLUDE_MPTABLE_BOOT_OP \
                    INCLUDE_USR_BOOT_OP \
                    INCLUDE_NO_BOOT_OP
    DEFAULTS        INCLUDE_MPTABLE_BOOT_OP
}

Component INCLUDE_ACPI_MPAPIC {
    NAME            ACPI MP APIC
    SYNOPSIS        ACPI MP APIC component
    REQUIRES        INCLUDE_ACPI_BOOT_OP
}

Component INCLUDE_ACPI_BOOT_OP {
    NAME            ACPI MP table
    SYNOPSIS        ACPI provides MP table
    REQUIRES        INCLUDE_ACPI \
                    INCLUDE_ACPI_MPAPIC \
                    INCLUDE_ACPI_TABLE_MAP \
                    INCLUDE_USR_MPAPIC
}

Component INCLUDE_MPTABLE_BOOT_OP {
    NAME            BIOS MP table
    SYNOPSIS        BIOS provides MP table
}

Component INCLUDE_USR_BOOT_OP {
    NAME            User defined MP table
    SYNOPSIS        User provides MP table via userMpApicData.c
}

Component INCLUDE_NO_BOOT_OP {
    NAME            No MP APIC boot
    SYNOPSIS        No MP APIC boot option
    REQUIRES        SELECT_INTERRUPT_MODE
    EXCLUDES        INCLUDE_MPTABLE_BOOT_OP \
                    INCLUDE_ACPI_BOOT_OP \
                    INCLUDE_USR_BOOT_OP
}

/*******************************************************************************
*
* Memory Auto-size
*
*/

Selection SELECT_MEM_AUTOSIZE {
    NAME            IA memory autosize
    COUNT           1-1
    _CHILDREN       FOLDER_BSP_CONFIG
    CHILDREN        INCLUDE_LOCAL_MEM_AUTOSIZE \
                    INCLUDE_NO_MEM_AUTOSIZE
    DEFAULTS        INCLUDE_LOCAL_MEM_AUTOSIZE
}

Component INCLUDE_LOCAL_MEM_AUTOSIZE {
    NAME            memory autosize
    SYNOPSIS        Run-time (dynamic) sizing
}

Component INCLUDE_NO_MEM_AUTOSIZE {
    NAME            No memory autosize
    SYNOPSIS        Fixed memory size
}

/*******************************************************************************
*
* PC Console Configuration Options
*
* Overrides for options initially defined in 40pcConsole.cdf:
*
*   SELECT_PC_CONSOLE_VGA is the only required selection for a PC console
*
*   SELECT_PC_CONSOLE_KBD (legacy PS2 i8042 keyboard) is no longer required
*   since newer boards may not support this keyboard. The user must manually
*   select a desired keyboard driver. The preferred default in config.h is now
*   a USB keyboard. DRV_KBD_I8042 can remain undefined if the board does
*   not support it.
*/

Component INCLUDE_PC_CONSOLE {
    HDR_FILES       ../src/hwif/h/console/pcConsole.h \
                    stdio.h \
                    sysLib.h
    REQUIRES        SELECT_PC_CONSOLE_VGA
}

Selection SELECT_PC_CONSOLE_KBD {
    NAME            PC Console PS2 keyboard driver
    SYNOPSIS        Keyboard drivers compatible with PC Console
    COUNT           1-
    _CHILDREN       FOLDER_DRIVERS
    CHILDREN        DRV_KBD_I8042
    DEFAULTS        DRV_KBD_I8042
}

/*******************************************************************************
*
* Clock Parameter Defaults
*
*/

Parameter AUX_CLK_RATE_MAX  {
    NAME            auxiliary clock configuration parameter
    SYNOPSIS        maximum auxiliary clock rate
    TYPE            uint
    DEFAULT         (INCLUDE_APIC_MODE)::((SYS_CLK_RATE_MAX)) \
                    (INCLUDE_PIC_MODE)::((8192)) \
                    (8192)
}

Parameter AUX_CLK_RATE_MIN  {
    NAME            auxiliary clock configuration parameter
    SYNOPSIS        minimum auxiliary clock rate
    TYPE            uint
    DEFAULT         (INCLUDE_APIC_MODE)::((SYS_CLK_RATE_MIN)) \
                    (INCLUDE_PIC_MODE)::((2)) \
                    (2)
}

/*******************************************************************************
*
* Early Print Parameter Defaults
*
*/

Component INCLUDE_EARLY_PRINT {
    NAME            Early Print UART parameters
    SYNOPSIS        Early Print UART parameters
    CFG_PARAMS      SYSKWRITE_UART_IS_PCH \
                    SYSKWRITE_UART_BASE \
                    SYSKWRITE_UART_REFCLK_HZ
    _CHILDREN       FOLDER_BSP_CONFIG
}

Parameter SYSKWRITE_UART_IS_PCH {
    NAME            Enable PCH/PCI UART Mode
    SYNOPSIS        Indicates whether this is a PCH/PCI UART using\
                    memory-mapped I/O access mode
    TYPE            exists
    DEFAULT         FALSE
}

Parameter SYSKWRITE_UART_BASE {
    NAME            UART base address
    SYNOPSIS        Specifies the UART base address
    TYPE            uint
    DEFAULT         (COM1_BASE_ADR)
}

Parameter SYSKWRITE_UART_REFCLK_HZ {
    NAME            UART reference clock frequency
    SYNOPSIS        Specifies the UART reference clock frequency in Hertz
    TYPE            uint
    DEFAULT         (1843200)
}

/*******************************************************************************
 *
 * Force unsupported components to be unavailable.
 *
 * The following component definition(s) forces the named component(s)
 * to be "unavailable" as far as the project build facility (vxprj) is
 * concerned. The required component (COMPONENT_NOT_SUPPORTED) should
 * never be defined, and hence the named component(s) will never be
 * available. This little trick is used by the BSPVTS build scripts
 * (suiteBuild, bspBuildTest.tcl) to automatically exclude test modules
 * that are not applicable to a BSP because the BSP does not support a
 * given component and/or test module. If and when support is added,
 * the following definition(s) should be removed.
 */

