/* config.h - Intel CPU configuration header */

/*
 * Copyright (c) 2010-2013 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01i,04jun13,jjk  WIND00364942 - Use USB_GEN2_KEYBOARD_BOOTSHELL_ATTACH when
                 INCLUDE_PC_CONSOLE is in use.
01h,24may13,jjk  WIND00364942 - Removing AMP and GUEST
01g,16may13,jjk  WIND00364942 - Swap RAM_HIGH and RAM_LOW to conform to IA
                 arch in 6.9
01f,09may13,jjk  WIND00364942 - Adding Unified BSP.
01e,18oct12,c_t  add ACPI config parameter
01d,25jun12,j_z  sync up with itl_nehalem bsp
01c,25mar11,lll  unification of itl BSPs
01b,30aug10,jjk  Need to bump BSP_REV from 0 to 1
01a,10apr10,rbc  created based on itl_crownbeach/config.h (01g)
*/

/*
DESCRIPTION
This module contains the configuration parameters for the
Intel generial CPU platform.
*/

#ifndef INCconfigh
#define INCconfigh

#ifdef __cplusplus
extern "C" {
#endif

/* BSP version/revision identification, before configAll.h */

#define BSP_VERSION "2.1"           /* vxWorks 6.9 BSP family */
#define BSP_REV     "/0"            /* increment by whole numbers */

#include <vsbConfig.h>
#include <configAll.h>
#include <pc.h>

/*
 * Define a local macro indicating if a non-prj cmdline build in a
 * BSP directory
 */

#define BSP_CMDLINE_BUILD   (!defined(CDF_OVERRIDE) && !defined(PRJ_BUILD))

/* BSP specific prototypes that must be in config.h */

#ifndef _ASMLANGUAGE
    IMPORT void sysHwInit0 (void);
    IMPORT UINT8 *sysInumTbl;       /* IRQ vs intNum table */
#endif /* !_ASMLANGUAGE */

/* Provide a default BOOT_LINE for non-prj builds */

#if BSP_CMDLINE_BUILD
#define DEFAULT_BOOT_LINE \
    "gei(0,0) host:/VxWorks/eKabini_RTL816X_6948/vxWorks h=172.18.101.168 e=172.18.101.170:ffffff00 u=target pw=target tn=target"
    /* "fs=0,0(0,0)host:/ata0:1/vxWorks o=gei0 h=172.18.101.168 e=172.18.101.170:0xffffff00 u=target f=0x2 pw=target" */
    /* "gei(0,0) host:/VxWorks/eKabini_RTL816X_6948/vxWorks h=172.18.101.168 e=172.18.101.170:ffffff00 u=target pw=target tn=target" */

    /* "fs=0,0(0,0)host:/ata0:1/vxWorks o=rtg0 h=172.18.101.168 e=172.18.101.170:0xffffff00 u=target f=0x2 pw=target" */
    /* "rtg(0,0) host:/VxWorks/eKabini_RTL816X_6948/vxWorks h=172.18.101.168 e=172.18.101.170:ffffff00 u=target pw=target tn=target" */
    /* "rtg(0,0)host:vxWorks h=90.0.0.3 e=90.0.0.50 u=target pw=vxworks" */
#endif /* BSP_CMDLINE_BUILD */

/* MP Table options for non-prj builds (select only one) */

#if BSP_CMDLINE_BUILD
#undef INCLUDE_ACPI_BOOT_OP         /* define if using ACPI for MP table */
#define INCLUDE_MPTABLE_BOOT_OP      /* define if using BIOS MP table */
#undef  INCLUDE_USR_BOOT_OP          /* define if user MP table */
#if (defined INCLUDE_ACPI_BOOT_OP)
#define INCLUDE_ACPI
#define INCLUDE_ACPI_MPAPIC      /* ACPI loads MP apic table */
#define INCLUDE_ACPI_TABLE_MAP   /* map ACPI tables into vxWorks memory */
#define INCLUDE_USR_MPAPIC       /* MPAPIC table initialzation */
#endif /* (defined INCLUDE_ACPI_BOOT_OP) */
#endif /* BSP_CMDLINE_BUILD */

#define ACPI_STATIC_MEM_SIZE    (2048)  /* ACPI memory pool size, in KB */

/*
 * Define this to view ACPI log and error messages during the boot sequence.
 * NOTE: This is a debug tool and is not formally supported.
 */

#undef INCLUDE_ACPI_DEBUG_PRINT
#undef INCLUDE_ACPI_SHOW

/* Define this to allow early print and trace statements */

#define INCLUDE_EARLY_PRINT          /* enables early print. See sysKwrite.c */
#define INCLUDE_EARLY_PRINT_TRACE    /* enables early trace MACROs */

#ifdef  INCLUDE_EARLY_PRINT
#if BSP_CMDLINE_BUILD             /* see 20bsp.cdf for prj-build defaults */
#undef  SYSKWRITE_UART_IS_PCH /* when defined, it indicates a PCH UART */
#define SYSKWRITE_UART_BASE      (COM1_BASE_ADR)
#define SYSKWRITE_UART_REFCLK_HZ (1843200)
#endif /* BSP_CMDLINE_BUILD */

#ifndef _ASMLANGUAGE

/* BSP specific prototypes */

IMPORT void   earlyPrint (char * outString);
IMPORT void   earlyPrintVga (char * outString);
IMPORT void   vgaPutc(UINT offset, unsigned char a);
IMPORT STATUS kwriteSerial (char * pBuffer, size_t len);
IMPORT STATUS kwriteVga (char * pBuffer, size_t len);
IMPORT STATUS (*_func_kwrite) (char * buf, size_t len);
#endif /* !_ASMLANGUAGE */
#ifdef INCLUDE_EARLY_PRINT_TRACE
#define EARLYPRINT_TRACE {IMPORT char EarlyPrint_tracebuff[]; \
            snprintf(EarlyPrint_tracebuff,255,"%s:%d\n",__FILE__,__LINE__); \
            earlyPrint(EarlyPrint_tracebuff);} WRS_ASM("nop;");
#define EARLYPRINTVGA_TRACE {IMPORT char EarlyPrintVga_tracebuff[]; \
            snprintf(EarlyPrintVga_tracebuff,255,"%s:%d\n",__FILE__,__LINE__); \
            earlyPrintVga(EarlyPrintVga_tracebuff);} WRS_ASM("nop;");
#else
#define EARLYPRINT_TRACE
#define EARLYPRINTVGA_TRACE
#endif /* INCLUDE_EARLY_PRINT_TRACE */
#else
#define EARLYPRINT_TRACE
#define EARLYPRINTVGA_TRACE
#endif /* INCLUDE_EARLY_PRINT */

/* User extension hooks in sysLib.c */

#undef  INCLUDE_USER_SYSLIB_HEADERS_PRE
#undef  INCLUDE_USER_SYSLIB_HEADERS_POST
#undef  INCLUDE_USER_SYSLIB_DEFINES_PRE
#undef  INCLUDE_USER_SYSLIB_DEFINES_POST
#undef  INCLUDE_USER_SYSLIB_IMPORTS
#undef  INCLUDE_USER_SYSLIB_DECLARATIONS
#undef  INCLUDE_USER_SYSLIB_SOURCES
#undef  INCLUDE_USER_SYSLIB_VXBUS
#undef  INCLUDE_USER_SYSLIB_FUNCTIONS_PRE
#undef  INCLUDE_USER_SYSLIB_HWINIT0_PRE
#undef  INCLUDE_USER_SYSLIB_HWINIT0_POST
#undef  INCLUDE_USER_SYSLIB_HWINIT_PRE
#undef  INCLUDE_USER_SYSLIB_HWINIT_VXB_PRE
#undef  INCLUDE_USER_SYSLIB_HWINIT_VXB_POST
#undef  INCLUDE_USER_SYSLIB_HWINIT_VXB_PCI
#undef  INCLUDE_USER_SYSLIB_HWINIT_POST
#undef  INCLUDE_USER_SYSLIB_HWINIT2_PRE
#undef  INCLUDE_USER_SYSLIB_HWINIT2_VXB_PRE
#undef  INCLUDE_USER_SYSLIB_HWINIT2_VXB_POST
#undef  INCLUDE_USER_SYSLIB_HWINIT2_VXB_PCI
#undef  INCLUDE_USER_SYSLIB_HWINIT2_POST
#undef  INCLUDE_USER_SYSLIB_FUNCTIONS_POST

/* Extended BIOS Data Area */

#ifndef EBDA_START
#define EBDA_START           0x9d000
#endif

/* BSP specific initialization (before cacheLibInit() is called) */

#define INCLUDE_SYS_HW_INIT_0
#define SYS_HW_INIT_0()         (sysHwInit0())

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9))
#define X86CPU_DEFAULT  X86CPU_PENTIUM4  /* to set sysProcessor the old way */
#endif /* ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9)) */

/* SMP defines */

#if defined(_WRS_CONFIG_SMP)
#define INCLUDE_VXIPI

/*
 * SMP AP specific defines.
 * NOTE: If sysInitCpuStartup changes, CPU_STARTUP_SIZE needs to change
 */

#define CPU_STARTUP_SIZE     0x00A8  /* size of sysInitCpuStartup */
#define CPU_ENTRY_POINT      0xC000  /* cold start code location for APs */
#define CPU_SCRATCH_START    0x0100  /* start of scratch memory */
#define CPU_AP_AREA          0x200   /* work area for APs */

/* offset for scratch memory */

#define CPU_SCRATCH_POINT    (CPU_ENTRY_POINT + CPU_SCRATCH_START)
#define NUM_GDT_ENTRIES      0x0A
#define CPU_SCRATCH_GDT      0x2E
#define CPU_SCRATCH_IDT      0x3A
#define LIM_GDT_OFFSET       0x2C
#define BASE_GDT_OFFSET      0x30
#define CR3_OFFSET           0x34
#define LIM_IDT_OFFSET       0x38
#define BASE_IDT_OFFSET      0x3C
#define AP_STK_OFFSET        0x40

#define CPU_INIT_START_ADR   (MP_ANCHOR_ADRS + 0x10)
#endif /* defined(_WRS_CONFIG_SMP) */

/* Warm boot (reboot) devices and parameters */

#define SYS_WARM_BIOS       0       /* warm start from BIOS */
#define SYS_WARM_FD         1       /* warm start from FD */
#define SYS_WARM_TFFS       2       /* warm start from DiskOnChip */
#define SYS_WARM_USB        3       /* warm start from USB */
#define SYS_WARM_ICH        4       /* warm start from ICH(IDE) */
#define SYS_WARM_AHCI       5       /* warm start from AHCI */

#define SYS_WARM_TYPE       SYS_WARM_BIOS   /* warm start device */

/*
 * Reset options for ICH/SCH/PCH at 0CF9h
 * COLD_RESET with external power cycle not available on all boards.
 * WARM_RESET is typically available on all boards (used by most BIOS vendors).
 */

#define SYS_ICH_COLD_RESET  0x0e   /* ICH/PCH reset cpu,platform, cycle power */
#define SYS_ICH_WARM_RESET  0x06   /* ICH/PCH reset cpu, platform */
#define SYS_SCH_COLD_RESET  0x08   /* SCH reset cpu, platform, cycle power */
#define SYS_SCH_WARM_RESET  0x02   /* SCH reset cpu, platform */

#define SYS_RESET_TYPE      SYS_ICH_WARM_RESET  /* see sysLib.c */

/* Warm boot (reboot) device and filename strings */

/*
 * BOOTROM_DIR is the device name for the device containing
 * the bootrom file. This string is used in sysToMonitor, sysLib.c
 * in dosFsDevCreate().
 */

#define BOOTROM_DIR  "/bd0"

/*
 * BOOTROM_BIN is the default path and file name to either a binary
 * bootrom file or an A.OUT file with its 32 byte header stripped.
 * Note that the first part of this string must match BOOTROM_DIR
 * The "bootrom.sys" file name will work with VxLd 1.5.
 */

#define BOOTROM_BIN  BOOTROM_DIR "/bootrom.sys"

/* IDT entry type options */

#define SYS_INT_TRAPGATE    0x0000ef00  /* trap gate */
#define SYS_INT_INTGATE     0x0000ee00  /* int gate */

/*
 * Interrupt mode options:
 * INCLUDE_SYMMETRIC_IO_MODE, INCLUDE_VIRTUAL_WIRE_MODE or INCLUDE_PIC_MODE
 *
 * See 20bsp.cdf for prj-build defaults.
 */

#if BSP_CMDLINE_BUILD
#define INCLUDE_SYMMETRIC_IO_MODE    /* Uniprocessor: SYMIO, VWIRE, or PIC */
#endif

#define APIC_MODE  (defined (INCLUDE_VIRTUAL_WIRE_MODE) || \
                    defined (INCLUDE_SYMMETRIC_IO_MODE))

#if APIC_MODE
#define DRV_INTCTLR_MPAPIC
#define DRV_INTCTLR_LOAPIC
#define INCLUDE_INTCTLR_DYNAMIC_LIB  /* define this for MSI support */
#if defined (INCLUDE_SYMMETRIC_IO_MODE)
#define DRV_INTCTLR_IOAPIC
#endif
#elif defined (INCLUDE_PIC_MODE)
#define DRV_INTCTLR_I8259
#endif /* APIC_MODE */

/* vxbus timer defaults */

#define INCLUDE_TIMER_SYS               /* timer drv control via vxBus */

#if defined (INCLUDE_SYMMETRIC_IO_MODE)
#define DRV_TIMER_LOAPIC
#if defined (_WRS_CONFIG_SMP)
#define DRV_TIMER_IA_HPET        /* SMP-SYMIO requires HPET */
#else
#define DRV_TIMER_IA_TIMESTAMP   /* UP-SYMIO uses TSC for timestamp */
#endif
#elif defined (INCLUDE_VIRTUAL_WIRE_MODE)
#define DRV_TIMER_LOAPIC
#define DRV_TIMER_I8253
#define DRV_TIMER_IA_TIMESTAMP       /* UP-VWIRE uses TSC for timestamp */
#elif defined (INCLUDE_PIC_MODE)
#define DRV_TIMER_I8253              /* UP-PIC timer driver */
#define DRV_TIMER_IA_TIMESTAMP       /* UP-PIC uses TSC for timestamp */
#endif /* defined (INCLUDE_SYMMETRIC_IO_MODE) */

#if APIC_MODE
#define APIC_TIMER_CLOCK_HZ  0       /* if value is 0, driver autodetects it */
#define TIMER_CLOCK_HZ       APIC_TIMER_CLOCK_HZ
#endif  /* APIC_MODE */

/*
 * default system and auxiliary clock constants
 *
 * Among other things, SYS_CLK_RATE_MAX depends upon the CPU and application
 * work load.  The default value, chosen in order to pass the internal test
 * suite, could go up to PIT_CLOCK. See 20bsp.cdf for prj-build defaults.
 */

#define SYS_CLK_RATE_MIN        (19)    /* minimum system clock rate */
#define SYS_CLK_RATE_MAX        (5000)  /* maximum system clock rate */

#if APIC_MODE
#define AUX_CLK_RATE_MIN     SYS_CLK_RATE_MIN    /* min aux clock rate */
#define AUX_CLK_RATE_MAX     SYS_CLK_RATE_MAX    /* max aux clock rate */
#elif defined (INCLUDE_PIC_MODE)
#define AUX_CLK_RATE_MIN     (2)     /* minimum aux clock rate */
#define AUX_CLK_RATE_MAX     (8192)  /* maximum aux clock rate */
#endif /* DRV_TIMER_LOAPIC */

/* Default non-prj SYS_MODEL definitions (gets updated by CPUID brandstring) */

#if BSP_CMDLINE_BUILD
#undef  SYS_MODEL
#if defined (_WRS_CONFIG_SMP)
#define SYS_MODEL "AMD eKabini Processor SYMMETRIC IO SMP"
#elif defined (INCLUDE_SYMMETRIC_IO_MODE)
#define SYS_MODEL "AMD eKabini Processor SYMMETRIC IO"
#elif defined (INCLUDE_VIRTUAL_WIRE_MODE)
#define SYS_MODEL "AMD eKabini Processor VIRTUAL WIRE"
#else
#define SYS_MODEL "AMD eKabini Processor"
#endif /* _WRS_CONFIG_SMP */
#endif /* BSP_CMDLINE_BUILD */

/* Driver and file system options */

#undef  INCLUDE_FD                      /* floppy disk */
#undef  INCLUDE_LPT                     /* parallel port */

/* Include ONLY ONE OF the following: */

#undef INCLUDE_DRV_STORAGE_INTEL_ICH    /* SATA IDE Gen1 (vxbus ATA driver)   */
#define INCLUDE_DRV_STORAGE_PIIX         /* SATA IDE Gen2 (6.9.2.1 and later)  */

/* Include ONLY ONE OF the following: */

#undef INCLUDE_DRV_STORAGE_INTEL_AHCI   /* SATA AHCI Gen1 */
#undef INCLUDE_DRV_STORAGE_AHCI         /* SATA AHCI Gen2 (6.9.2.1 and later) */

#if defined(INCLUDE_DRV_STORAGE_PIIX) || \
    defined(INCLUDE_DRV_STORAGE_INTEL_ICH) || \
    defined(INCLUDE_DRV_STORAGE_AHCI) || \
    defined(INCLUDE_DRV_STORAGE_INTEL_AHCI)
#if defined (INCLUDE_BOOT_APP) || defined (BOOTAPP)
#define INCLUDE_BOOT_FILESYSTEMS
#endif /* INCLUDE_BOOT_APP */
#endif /* INCLUDE_DRV_STORAGE_PIIX || INCLUDE_DRV_STORAGE_INTEL_ICH */

/*
 * Definitions for base USB support
 *
 * NOTE 1: Required for supporting USB nvram.txt access
 * NOTE 2: USB Gen2 drivers are the preferred default since VxWorks 6.8
 * NOTE 3: USB Gen1 and Gen2 drivers for the same class are mutually exclusive
 */

#define INCLUDE_USB
/* #undef INCLUDE_USB */

#ifdef  INCLUDE_USB
#define INCLUDE_USB_INIT
#define INCLUDE_EHCI
#define INCLUDE_EHCI_INIT
#define INCLUDE_UHCI
#define INCLUDE_UHCI_INIT
#define INCLUDE_OHCI
#define INCLUDE_OHCI_INIT
#define INCLUDE_USB_GEN2_STORAGE
#define INCLUDE_USB_GEN2_STORAGE_INIT
#if defined (INCLUDE_BOOT_APP) || defined (BOOTAPP)
#define INCLUDE_BOOT_USB_FS_LOADER
#endif /* INCLUDE_BOOT_APP */
#endif /* INCLUDE_USB */

/* Definitions for base file system support */

#if defined(INCLUDE_USB) || defined(INCLUDE_TFFS) || defined(INCLUDE_FD) || \
    defined(INCLUDE_DRV_STORAGE_PIIX) ||  \
    defined(INCLUDE_DRV_STORAGE_AHCI) || \
    defined(INCLUDE_DRV_STORAGE_INTEL_ICH) || \
    defined(INCLUDE_DRV_STORAGE_INTEL_AHCI)
#define INCLUDE_RAWFS            /* include raw FS */
#define INCLUDE_DOSFS            /* dosFs file system */
#define INCLUDE_DOSFS_MAIN       /* DOSFS2 file system primary module */
#define INCLUDE_DOSFS_FAT        /* DOS file system FAT12/16/32 handler */
#define INCLUDE_DOSFS_DIR_FIXED  /* DOS File system old directory format */
#define INCLUDE_DOSFS_DIR_VFAT   /* DOS file system VFAT directory handler */
#define INCLUDE_DOSFS_CHKDSK     /* DOS file system consistency checker */
#define INCLUDE_DOSFS_FMT        /* DOS file system volume formatter */
#define INCLUDE_FS_MONITOR       /* include file system monitor */
#define INCLUDE_FS_EVENT_UTIL    /* include file event utility */
#define INCLUDE_ERF              /* include event report framework */
#define INCLUDE_XBD              /* include extended block devices */
#define INCLUDE_XBD_BLK_DEV      /* XBD Block Device Wrapper */
#define INCLUDE_DEVICE_MANAGER   /* include device manager */
#define INCLUDE_XBD_PART_LIB     /* Support partition tables on XBD devices */
#undef  INCLUDE_HRFS             /* include HRFS file system */
#endif /* INCLUDE_USB || INCLUDE_DRV_STORAGE_PIIX */


/* VxBus configuration */

#define INCLUDE_VXBUS

#ifdef  INCLUDE_VXBUS

/* VxBus HW resources */

/* define to include hwconf.c via cdf rather than sysLib.c */

#define INCLUDE_HWCONF

/* VxBus utils */

#define VXBUS_TABLE_CONFIG
#define INCLUDE_VXB_CMDLINE
#define INCLUDE_SIO_UTILS
#define INCLUDE_HWMEM_ALLOC
#define HWMEM_POOL_SIZE  100000
#define INCLUDE_PARAM_SYS
#define INCLUDE_DMA_SYS
#undef  INCLUDE_NON_VOLATILE_RAM

/* VxBus bus types */

#define INCLUDE_PLB_BUS
#define INCLUDE_PCI_BUS
#define INCLUDE_PCI

/* VxBus drivers */

#define INCLUDE_PENTIUM_PCI
#define INCLUDE_PCI_OLD_CONFIG_ROUTINES

/* undef this and INCLUDE_SIO_UTILS to use sysSerial.c */

#define DRV_SIO_NS16550
#undef  DRV_NVRAM_FILE

/* Network driver options: VxBus drivers */

#undef  INCLUDE_FEI8255X_VXB_END
#define INCLUDE_GEI825XX_VXB_END
#undef  INCLUDE_RTL8139_VXB_END
#undef  INCLUDE_RTL8169_VXB_END
#undef DRV_VXBEND_INTELTOPCLIFF
#undef DRV_RESOURCE_INTELTOPCLIFF_IOH

/* PHY and MII bus support */

#define INCLUDE_MII_BUS
#define INCLUDE_GENERICPHY
#undef  INCLUDE_MV88E1X11PHY
#undef  INCLUDE_RTL8201PHY
#undef  INCLUDE_RTL8169PHY

#endif /* INCLUDE_VXBUS */

#define INCLUDE_END             /* Enhanced Network Driver Support */
#define INCLUDE_NETWORK

/* default MMU options and PHYS_MEM_DESC type state constants */

#define INCLUDE_MMU_BASIC       /* bundled MMU support */

#undef  VM_PAGE_SIZE
#define VM_PAGE_SIZE        PAGE_SIZE_4KB   /* default page size */

#define INCLUDE_MMU_P6_32BIT    /* include 32bit MMU for Pentium4 */

#if defined (INCLUDE_MMU_P6_32BIT)
#undef  VM_STATE_MASK_FOR_ALL
#undef  VM_STATE_FOR_IO
#undef  VM_STATE_FOR_MEM_OS
#undef  VM_STATE_FOR_MEM_APPLICATION
#undef  VM_STATE_FOR_PCI
#define VM_STATE_MASK_FOR_ALL \
           VM_STATE_MASK_VALID | VM_STATE_MASK_WRITABLE | \
           VM_STATE_MASK_WBACK | VM_STATE_MASK_GLOBAL
#define VM_STATE_FOR_IO \
           VM_STATE_VALID | VM_STATE_WRITABLE | \
           VM_STATE_CACHEABLE_NOT | VM_STATE_GLOBAL_NOT
#define VM_STATE_FOR_MEM_OS \
           VM_STATE_VALID | VM_STATE_WRITABLE | \
           VM_STATE_WBACK | VM_STATE_GLOBAL_NOT
#define VM_STATE_FOR_MEM_APPLICATION \
           VM_STATE_VALID | VM_STATE_WRITABLE | \
           VM_STATE_WBACK | VM_STATE_GLOBAL_NOT
#define VM_STATE_FOR_PCI \
           VM_STATE_VALID | VM_STATE_WRITABLE | \
           VM_STATE_CACHEABLE_NOT | VM_STATE_GLOBAL_NOT
#endif  /* defined (INCLUDE_MMU_P6_32BIT) */

/* CPU family type-specific macros and options */

#undef  INCLUDE_SW_FP           /* Pentium4 has hardware FPP */
#undef  USER_D_CACHE_MODE       /* Pentium4 write-back data cache support */
#define USER_D_CACHE_MODE   (CACHE_COPYBACK | CACHE_SNOOP_ENABLE)
#define INCLUDE_MTRR_GET        /* get MTRR to sysMtrr[] */
#define INCLUDE_PMC             /* include PMC */

#undef  TSC_SYNC_CORES          /* Invariant TSC synchronized between cores */

#define INCLUDE_EARLY_I_CACHE_ENABLE /* enable instruction cache early in sysHwInit */
#define INCLUDE_EARLY_D_CACHE_ENABLE /* enable data cache early in sysHwInit */

#if APIC_MODE
#undef  INCLUDE_DEBUG_STORE      /* Debug Store (BTS/PEBS) */
#ifdef  INCLUDE_DEBUG_STORE
#define DS_SYS_MODE   FALSE  /* TRUE system mode, FALSE task mode */
#define BTS_ENABLED   TRUE   /* BTS TRUE enable, FALSE disable */
#define BTS_INT_MODE  TRUE   /* BTS TRUE int mode, FALSE circular */
#define BTS_BUF_MODE  TRUE   /* BTS TRUE buffer mode, FALSE bus */
#define PEBS_ENABLED  TRUE   /* PEBS TRUE enable, FALSE disable */
#define PEBS_EVENT    PEBS_REPLAY        /* PEBS event */
#define PEBS_METRIC   PEBS_2NDL_CACHE_LOAD_MISS  /* PEBS metric */
#define PEBS_OS       TRUE   /* PEBS TRUE supervisor, FALSE usr */
#define PEBS_RESET    -1LL   /* PEBS default reset counter value */
#endif /* INCLUDE_DEBUG_STORE */
#endif  /* APIC_MODE */

/* console definitions  */

#undef  NUM_TTY
#define NUM_TTY     (N_UART_CHANNELS)   /* number of tty channels */

#undef  CONSOLE_BAUD_RATE
#define CONSOLE_BAUD_RATE       115200  /* console baud rate */

#define INCLUDE_PC_CONSOLE              /* PC keyboard and VGA console */
#ifdef  INCLUDE_PC_CONSOLE
#   define PC_CONSOLE           (0)     /* console number */
#   define N_VIRTUAL_CONSOLES   (2)     /* shell + application */
#   ifdef INCLUDE_VXBUS
#       define DRV_KBD_I8042            /* newer boards may not support PS2 */
#       define DRV_VGA_M6845
#   endif /* INCLUDE_VXBUS */
#endif /* INCLUDE_PC_CONSOLE */

#ifdef INCLUDE_USB
#   if (defined INCLUDE_PC_CONSOLE)     /* PC_CONSOLE USB keyboard attach */
#       define INCLUDE_USB_GEN2_KEYBOARD
#       define INCLUDE_USB_GEN2_KEYBOARD_INIT
#       define INCLUDE_USB_GEN2_KEYBOARD_BOOTSHELL_ATTACH
#   else                                /* TTY_CONSOLE USB keyboard attach */
#       define INCLUDE_USB_GEN2_KEYBOARD
#       define INCLUDE_USB_GEN2_KEYBOARD_INIT
#       define INCLUDE_USB_GEN2_KEYBOARD_SHELL_ATTACH
#   endif /* INCLUDE_PC_CONSOLE */
#endif /* INCLUDE_USB */

/* PS/2 101-key default keyboard type (use PC_XT_83_KBD for 83-key) */

#define PC_KBD_TYPE   (PC_PS2_101_KBD)

/* NVRAM definitions */

#undef  INCLUDE_USR_NVRAM_IMPLEMENTATION     /* define to exclude sysNvRam.c */
#ifdef  INCLUDE_USR_NVRAM_IMPLEMENTATION
#define NV_RAM_SIZE          (0x1000)        /* user non-volatile RAM size */
#else   /* default */
#if (SYS_WARM_TYPE == SYS_WARM_BIOS)
#define NV_RAM_SIZE      (NONE)
#else
#define NV_RAM_SIZE      (0x1000)
#endif
#endif /* INCLUDE_USR_NVRAM_IMPLEMENTATION */

#ifdef  BOOTCODE_IN_RAM
#undef  ROMSTART_BOOT_CLEAR
#endif /* BOOTCODE_IN_RAM */

/* UP and SMP memory addresses, offsets, and size constants */

#if BSP_CMDLINE_BUILD
#define LOCAL_MEM_LOCAL_ADRS    (0x00100000)   /* system memory start address */
#endif /* BSP_CMDLINE_BUILD */

#define USER_RESERVED_MEM       (0)            /* user reserved memory */

#undef  SYSTEM_RAM_SIZE         /* 256MB RAM is default if not autosizing */
#define SYSTEM_RAM_SIZE         (0x80000000)    /* 256-(16MB stolen) = 240MB */

/* LOCAL_MEM_SIZE is the size of RAM available to the OS */

#if BSP_CMDLINE_BUILD
#define LOCAL_MEM_SIZE          (SYSTEM_RAM_SIZE - LOCAL_MEM_LOCAL_ADRS)
#endif /* BSP_CMDLINE_BUILD */

/*
 * Memory auto-sizing is supported when the following option is defined.
 * See sysPhysMemTop() in the sysLib.c file.
 */

#define INCLUDE_LOCAL_MEM_AUTOSIZE

/*
 * The following parameters are defined here and in the BSP Makefile.
 * They must be kept synchronized.  Any changes made here must be made
 * in the Makefile and vice versa.
 */

#if BSP_CMDLINE_BUILD
#define ROM_TEXT_ADRS        (0x00008000)    /* real mode image entry */
#define FIRST_TEXT_ADRS      (0x00008000)

#if defined (INCLUDE_BOOT_APP) || defined (BOOTAPP)
#define ROM_SIZE         (0x00800000)    /* max size of bootrom_uncmp[.bin] */
#else
#define ROM_SIZE         (0x00090000)    /* max size of bootrom[.bin] */
#endif

#endif /* BSP_CMDLINE_BUILD */

/* Aliases that maintain backward compatibility with RAM_LOW/RAM_HIGH */

#define SYS_RAM_HIGH_ADRS   RAM_HIGH_ADRS   /* Boot image entry point */
#define SYS_RAM_LOW_ADRS    RAM_LOW_ADRS

/* Multistage Loader definitions */

#define BOOT_IMAGE_ADRS_OFFSET       (BOOT_MULTI_STAGE_DATA_OFFSET)
#define BOOT_IMAGE_ADRS_OFFSET_SIZE  4
#define BOOT_IMAGE_SIZE_OFFSET       (BOOT_IMAGE_ADRS_OFFSET +          \
                                      BOOT_IMAGE_ADRS_OFFSET_SIZE)
#define BOOT_IMAGE_SIZE_OFFSET_SIZE  4
#define BOOT_MULTI_STAGE_DATA_SIZE   (BOOT_IMAGE_SIZE_OFFSET +          \
                                      BOOT_IMAGE_SIZE_OFFSET_SIZE       \
                                      - BOOT_MULTI_STAGE_DATA_OFFSET)
#define BOOT_MULTI_STAGE_DATA        (LOCAL_MEM_LOCAL_ADRS +            \
                                      BOOT_MULTI_STAGE_DATA_OFFSET)
#define BOOT_IMAGE_ADRS              (LOCAL_MEM_LOCAL_ADRS +    \
                                      BOOT_IMAGE_ADRS_OFFSET)
#define BOOT_IMAGE_SIZE              (LOCAL_MEM_LOCAL_ADRS +    \
                                      BOOT_IMAGE_SIZE_OFFSET)

/* power management definitions */

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9))

/* force disable CPU light power management, it's obsolete */

#undef INCLUDE_CPU_PWR_MGMT
#endif /* ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9)) */

/* interrupt mode/number definitions */

#include "configInum.h"

#define _WRS_BSP_VM_PAGE_OFFSET     (VM_PAGE_SIZE)

#ifdef __cplusplus
}
#endif

#endif  /* INCconfigh */

#if defined(PRJ_BUILD)
#   include "prjParams.h"
#endif

