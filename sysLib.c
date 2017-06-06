/* sysLib.c - System-dependent library for Intel IA family BSPs */

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
01k,24may13,jjk  WIND00364942 - Removing AMP and GUEST
01j,17may13,rbc  WIND00364942 - Banner changes for UBSP
01i,16may13,jjk  WIND00364942 - Swap RAM_HIGH and RAM_LOW to conform to IA
                 arch in 6.9
01h,09may13,jjk  WIND00364942 - Adding Unified BSP.
01g,08nov12,lll  add MPTABLE type to sysModelStr
01f,18oct12,c_t  add smi disable.
01e,06aug12,lll  added sysAMP.h and sysAMP.c during AMP refactoring
01d,11jul12,c_t  auto probe SYS_MODE, check if IO hub is SCH for reboot.
01c,25jun12,j_z  sync with 02o verion of itl_nehalem, code clean.
01b,25mar11,lll  unification of itl BSPs
01a,28jan11,lll  Adapted from Intel QM57, Atom and pcPentium4 BSPs
*/

/*
DESCRIPTION
This library provides board-specific routines.  The device configuration
modules and device drivers included are:

    i8259Intr.c - Intel 8259 Programmable Interrupt Controller (PIC) library
    sysNvRam.c  - block device NVRAM library

Other drivers are vxBus driver, please check target.ref to get the corresponding
vxBus driver of the specific module.

INCLUDE FILES: sysLib.h

SEE ALSO:
.pG "Configuration"
*/

/* -------------------- headers -------------------- */

#ifdef  INCLUDE_USER_SYSLIB_HEADERS_PRE
#include "userSyslibHeadersPre.h"                   /* user extension hook */
#endif

#include <vxWorks.h>
#include <vsbConfig.h>
#include <vme.h>
#include <memLib.h>
#include <sysLib.h>
#include <string.h>
#include <intLib.h>
#include <config.h>
#include <logLib.h>
#include <taskLib.h>
#include <vxLib.h>
#include <errnoLib.h>
#include <dosFsLib.h>
#include <stdio.h>
#include <cacheLib.h>
#include <vmLib.h>
#include <arch/i86/ipiI86Lib.h>
#include <arch/i86/pentiumLib.h>
#include <ctype.h>

#ifdef INCLUDE_EXC_TASK
#include <private/excLibP.h>            /* _func_excJobAdd */
#endif /* INCLUDE_EXC_TASK */

#if defined (INCLUDE_VIRTUAL_WIRE_MODE) || defined (INCLUDE_SYMMETRIC_IO_MODE)
#include <hwif/intCtlr/vxbMpApic.h>
#endif  /* INCLUDE_VIRTUAL_WIRE_MODE || INCLUDE_SYMMETRIC_IO_MODE */

#include <hwif/fw/acpiLib.h>

#if defined (_WRS_CONFIG_SMP)
#include <private/kernelLibP.h>
#include <private/windLibP.h>
#include <private/vxSmpP.h>
#include <arch/i86/regsI86.h>
#include <fppLib.h>
#include <cpuset.h>
#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8))
#include <vxCpuLib.h>
#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8) */
#include <vxIpiLib.h>
#endif /* (_WRS_CONFIG_SMP) */

#include <arch/i86/vxCpuArchLib.h>
#include <vxBusLib.h>

#ifdef  INCLUDE_USER_SYSLIB_HEADERS_POST
#include "userSyslibHeadersPost.h"                  /* user extension hook */
#endif

/* -------------------- defines -------------------- */

#ifdef  INCLUDE_USER_SYSLIB_DEFINES_PRE
#include "userSyslibDefinesPre.h"                   /* user extension hook */
#endif

#define ROM_SIGNATURE_SIZE  16

#if (VM_PAGE_SIZE == PAGE_SIZE_4KB)
#if  (LOCAL_MEM_LOCAL_ADRS >= 0x00100000)
#define LOCAL_MEM_SIZE_OS    0x00180000      /* n * VM_PAGE_SIZE */
#else
#define LOCAL_MEM_SIZE_OS    0x00080000      /* n * VM_PAGE_SIZE */
#endif
#else   /* VM_PAGE_SIZE is 2/4MB */
#define LOCAL_MEM_SIZE_OS    VM_PAGE_SIZE    /* n * VM_PAGE_SIZE */
#endif  /* (VM_PAGE_SIZE == PAGE_SIZE_4KB) */

/*
 * IA-32 protected mode physical address space 4GB (2^32) and protected
 * mode physical address space extension 64GB (2^36) size constants.
 */

#define SIZE_ADDR_SPACE_32   (0x100000000ull)
#define SIZE_ADDR_SPACE_36   (0x1000000000ull)

/* maximum address space probed when using memory auto-sizing */

#define PHYS_MEM_MAX         (SIZE_ADDR_SPACE_32)

#define HALF(x)              (((x) + 1) >> 1)

/* TSC calibration duration, can be 1 to 20 ms */

#define TSC_CALIBRATE_DURATION    (2)

#ifdef  INCLUDE_USER_SYSLIB_DEFINES_POST
#include "userSyslibDefinesPost.h"                  /* user extension hook */
#endif

/* -------------------- imports -------------------- */

#if defined (INCLUDE_VIRTUAL_WIRE_MODE) || defined (INCLUDE_SYMMETRIC_IO_MODE)
IMPORT MP_APIC_DATA  *pMpApicData;
#ifdef INCLUDE_USR_MPAPIC
IMPORT STATUS usrMpapicInit (BOOL earlyInit, char * pBuf);
#endif
#endif /* defined (INCLUDE_VIRTUAL_WIRE_MODE) || (INCLUDE_SYMMETRIC_IO_MODE) */

#if defined(INCLUDE_SYMMETRIC_IO_MODE)
/*
 * These globals (glbLoApicOldSvr, glbLoApicOldLint0, glbLoApicOldLint1)
 * are used in sysLib.c, they avoid calling through vxbus and reduce
 * overhead and potential spinLock nesting...
 */

IMPORT UINT32 glbLoApicOldSvr;        /* original SVR */
IMPORT UINT32 glbLoApicOldLint0;      /* original LINT0 */
IMPORT UINT32 glbLoApicOldLint1;      /* original LINT1 */
#endif /* INCLUDE_SYMMETRIC_IO_MODE */

#if defined (_WRS_CONFIG_SMP)
IMPORT cpuset_t vxCpuEnabled;
IMPORT TSS      sysTss;
#endif /* (_WRS_CONFIG_SMP) */

#ifndef _WRS_CONFIG_SMP
IMPORT int         intCnt;                    /* interrupt mode counter */
#endif /* !_WRS_CONFIG_SMP */

IMPORT char        end;                       /* linker defined end-of-image */
IMPORT GDT         sysGdt[];                  /* the global descriptor table */
IMPORT void        elcdetach (int unit);
IMPORT VOIDFUNCPTR intEoiGet;                 /* BOI/EOI function pointer */
IMPORT void        intEnt (void);
#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9))
IMPORT int         sysCpuProbe (void);        /* set a type of CPU family */
#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9) */
IMPORT void        sysInitGDT (void);
IMPORT UINT32      sysCalTSC (int duration);
IMPORT void        sysConfInit0 (void);
IMPORT void        sysConfInit (void);
IMPORT void        sysConfStop (void);

#if defined (_WRS_CONFIG_SMP)
IMPORT void sysInit(void);
IMPORT void sysInitCpuStartup(void);

#if defined (INCLUDE_RTP)
IMPORT STATUS syscallArchInit (void);
#endif /* INCLUDE_RTP */

#if defined INCLUDE_PROTECT_TASK_STACK || \
    defined INCLUDE_PROTECT_INTERRUPT_STACK
#if defined(_WRS_OSM_INIT)
IMPORT STATUS excOsmInit (UINT32, UINT32);
#endif /* defined(_WRS_OSM_INIT) */
#endif /* INCLUDE_PROTECT_TASK_STACK || INCLUDE_PROTECT_INTERRUPT_STACK */

#endif /* (_WRS_CONFIG_SMP) */

#ifdef INCLUDE_VXBUS
#ifdef INCLUDE_SIO_UTILS
IMPORT void    sysSerialConnectAll(void);
#endif  /* INCLUDE_SIO_UTILS */

IMPORT void (*_vxb_delayRtn)(void);
IMPORT void (*_vxb_usDelayRtn)(int);
IMPORT void (*_vxb_msDelayRtn)(int);

IMPORT void hardWareInterFaceInit();
IMPORT FUNCPTR sysIntLvlEnableRtn;
IMPORT FUNCPTR sysIntLvlDisableRtn;
STATUS sysIntEnablePIC (int irqNo);
STATUS sysIntDisablePIC (int irqNo);
LOCAL STATUS sysDevConnect (void);
#endif /* INCLUDE_VXBUS */

IMPORT STATUS mmuPro32Enable (BOOL enable);     /* disable MMU */
IMPORT void acpiIntrIntHelper (void);

#ifdef  INCLUDE_USER_SYSLIB_IMPORTS
#include "userSyslibImports.h"                      /* user extension hook */
#endif

/* -------------------- declarations --------------- */

static UINT64 calibratedTSC = 2000000000ULL;    /* default as 2 GHz */

IMPORT UCHAR    wrs_kernel_data_end [];         /* defined by the loader */
IMPORT UCHAR    _wrs_kernel_text_start [];      /* defined by the loader */

/* declare initial memory map descriptors */

#include "sysPhysMemDesc.c"

#include "sysMultiBoot.c"

#ifndef DRV_SIO_NS16550

/* Use vxBus ns16550 if INCLUDE_VXBUS is defined */

#include "sysSerial.c"

#endif /* ! DRV_SIO_NS16550 */

int sysPhysMemDescNumEnt;       /* number Mmu entries to be mapped */

#ifdef INCLUDE_FD

IMPORT STATUS usrFdConfig (int type, int drive, char *fileName);
FD_TYPE fdTypes[] =
    {
    {2880,18,2,80,2,0x1b,0x54,0x00,0x0c,0x0f,0x02,1,1,"1.44M"},
    {2400,15,2,80,2,0x24,0x50,0x00,0x0d,0x0f,0x02,1,1,"1.2M"},
    };
UINT    sysFdBuf     = FD_DMA_BUF_ADDR; /* floppy disk DMA buffer address */
UINT    sysFdBufSize = FD_DMA_BUF_SIZE; /* floppy disk DMA buffer size */

#endif  /* INCLUDE_FD */

#ifdef  INCLUDE_LPT

LPT_RESOURCE lptResources [N_LPT_CHANNELS] =
    {
    {LPT0_BASE_ADRS, INT_NUM_LPT0, LPT0_INT_LVL,
    TRUE, 10000, 10000, 1, 1, 0
    },

    {LPT1_BASE_ADRS, INT_NUM_LPT1, LPT1_INT_LVL,
    TRUE, 10000, 10000, 1, 1, 0
    },

    {LPT2_BASE_ADRS, INT_NUM_LPT2, LPT2_INT_LVL,
    TRUE, 10000, 10000, 1, 1, 0
    }
    };

#endif  /* INCLUDE_LPT */

int     sysBus = BUS;      /* system bus type (VME_BUS, etc) */
int     sysCpu = CPU;      /* system cpu type (MC680x0) */

char    *sysBootLine = BOOT_LINE_ADRS; /* address of boot line */
char    *sysExcMsg   = EXC_MSG_ADRS; /* catastrophic message area */

int     sysProcNum;         /* processor number of this cpu */

UINT    sysIntIdtType   = SYS_INT_INTGATE;  /* IDT entry type */
UINT    sysProcessor    = (UINT) NONE;      /* 0=386, 1=486, 2=P5, 4=P6, 5=P7 */
UINT    sysCoprocessor  = 0;                /* 0=none, 1=387, 2=487 */

int     sysWarmType     = SYS_WARM_TYPE;    /* system warm boot type */

LOCAL   _WRS_IOCHAR sysColdResetType = SYS_RESET_TYPE; /* ICH,SCH,PCH reset */

UINT    sysStrayIntCount = 0;       /* Stray interrupt count */

char    *memTopPhys = NULL;         /* top of memory */
UINT32  memRom     = 0;             /* saved bootrom image */
UINT32  memRomSize = 0;             /* saved bootrom image size */

/* Initial GDT to be initialized */

GDT     *pSysGdt    = (GDT *)(LOCAL_MEM_LOCAL_ADRS + GDT_BASE_OFFSET);

/*
 * mpCpuIndexTable is used in vxCpuArchLib.c, this global avoids
 * calling through vxbus and reduces overhead, table translations
 * are done everywhere, but once up this table should never change...
 */

UINT8   mpCpuIndexTable[256];

#ifdef _WRS_CONFIG_SMP

/* TSS initialized */

TSS *sysTssCurrent = &sysTss;                   /* current sysTss */

FUNCPTR sysCpuInitTable[VX_MAX_SMP_CPUS] = {NULL};
unsigned int sysCpuLoopCount[VX_MAX_SMP_CPUS] = {0};
#else
TSS     sysTss;
#endif /* _WRS_CONFIG_SMP */

LOCAL BOOL sysCheckSCH (void);

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)
/*
 * These are used in sysLib.c to avoid calling out
 * through vxbus to reduce overhead, and potential
 * spinLock nesting...
 */

#if defined(INCLUDE_SYMMETRIC_IO_MODE)
LOCAL VXB_DEVICE_ID ioApicDevID;

#if defined(_WRS_CONFIG_SMP)
LOCAL UINT8 tramp_code[CPU_AP_AREA];
#endif /* (defined(_WRS_CONFIG_SMP) */
#endif /* INCLUDE_SYMMETRIC_IO_MODE */

LOCAL VXB_DEVICE_ID mpApicDevID;
LOCAL VXB_DEVICE_ID loApicDevID;

STATUS apicIntrIntHelper(VXB_DEVICE_ID pDev, void * unused);

LOCAL STATUS (*getMpApicLoIndexTable)(VXB_DEVICE_ID pInst,
                                      UINT8 ** mploApicIndexTable);
LOCAL STATUS (*getMpApicloBaseGet) (VXB_DEVICE_ID  pInst,
                                    UINT32 *mpApicloBase);
#if defined(INCLUDE_SYMMETRIC_IO_MODE)
LOCAL STATUS (*getMpApicNioApicGet) (VXB_DEVICE_ID  pInst,
                                     UINT32 *mpApicNioApic);
LOCAL STATUS (*getMpApicAddrTableGet) (VXB_DEVICE_ID  pInst,
                                       UINT32 **mpApicAddrTable);

LOCAL STATUS (*getIoApicRedNumEntriesGet)(VXB_DEVICE_ID pInst,
                                          UINT8 *ioApicRedNumEntries);

LOCAL STATUS (*getIoApicIntrIntEnable)(VXB_DEVICE_ID pInst,
                                       INT32 * irqNo);
LOCAL STATUS (*getIoApicIntrIntDisable)(VXB_DEVICE_ID pInst,
                                        INT32 * irqNo);

LOCAL STATUS (*getIoApicIntrIntLock)(VXB_DEVICE_ID pInst, int * dummy);
LOCAL STATUS (*getIoApicIntrIntUnlock)(VXB_DEVICE_ID pInst, int * dummy);
#endif  /* INCLUDE_SYMMETRIC_IO_MODE */

LOCAL STATUS (*getLoApicIntrInitAP)(VXB_DEVICE_ID pInst, int *dummy);
LOCAL STATUS (*getLoApicIntrEnable)(VXB_DEVICE_ID pInst, BOOL *enable);

LOCAL STATUS (*getLoApicIntrIntLock)(VXB_DEVICE_ID pInst, int * dummy);
LOCAL STATUS (*getLoApicIntrIntUnlock)(VXB_DEVICE_ID pInst, int * dummy);

void sysStaticMpApicDataGet(MP_APIC_DATA *pMpApicData);
#endif /* defined (INCLUDE_SYMMETRIC_IO_MODE) || */
       /* defined (INCLUDE_VIRTUAL_WIRE_MODE)    */

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8))
#include <hwif/cpu/arch/i86/vxCpuIdLib.h>

IMPORT int vxCpuIdProbe(VX_CPUID *);

UINT vxProcessor = (UINT) NONE; /* 6=core, 7=atom, 8=nehalem, 9=sandybridge */

VX_CPUID vxCpuId = {{0,{0},0,0,0,0,0,0,0,0,{0,1},{0}}}; /* VX_CPUID struct. */
#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8) */

/* for backward compatibility, maintain old CPUID struct */

CPUID sysCpuId = {0,{0},0,0,0,0,0,0,0,0,{0,1},{0}}; /* CPUID struct. */
char  sysModelStr[80]; /* brandstring max len 48. */

#if !defined (_WRS_CONFIG_SMP)
BOOL    sysBp       = TRUE;     /* TRUE for BP, FALSE for AP */
#endif /* !(_WRS_CONFIG_SMP) */

#if defined (INCLUDE_VIRTUAL_WIRE_MODE)

/* IRQ vs IntNum table for INCLUDE_VIRTUAL_WIRE_MODE */

UINT8   vwInumTbl[]    =
    {
    INT_NUM_IRQ0,               /* IRQ  0 Vector No */
    INT_NUM_IRQ0 + 1,           /* IRQ  1 Vector No */
    INT_NUM_IRQ0 + 2,           /* IRQ  2 Vector No */
    INT_NUM_IRQ0 + 3,           /* IRQ  3 Vector No */
    INT_NUM_IRQ0 + 4,           /* IRQ  4 Vector No */
    INT_NUM_IRQ0 + 5,           /* IRQ  5 Vector No */
    INT_NUM_IRQ0 + 6,           /* IRQ  6 Vector No */
    INT_NUM_IRQ0 + 7,           /* IRQ  7 Vector No */
    INT_NUM_IRQ0 + 8,           /* IRQ  8 Vector No */
    INT_NUM_IRQ0 + 9,           /* IRQ  9 Vector No */
    INT_NUM_IRQ0 + 10,          /* IRQ 10 Vector No */
    INT_NUM_IRQ0 + 11,          /* IRQ 11 Vector No */
    INT_NUM_IRQ0 + 12,          /* IRQ 12 Vector No */
    INT_NUM_IRQ0 + 13,          /* IRQ 13 Vector No */
    INT_NUM_IRQ0 + 14,          /* IRQ 14 Vector No */
    INT_NUM_IRQ0 + 15,          /* IRQ 15 Vector No */
    INT_NUM_LOAPIC_TIMER,       /* Local APIC Timer Vector No */
    INT_NUM_LOAPIC_ERROR,       /* Local APIC Error Vector No */
    INT_NUM_LOAPIC_LINT0,       /* Local APIC LINT0 Vector No */
    INT_NUM_LOAPIC_LINT1,       /* Local APIC LINT1 Vector No */
    INT_NUM_LOAPIC_PMC,         /* Local APIC PMC Vector No */
    INT_NUM_LOAPIC_THERMAL,     /* Local APIC Thermal Vector No */
    INT_NUM_LOAPIC_SPURIOUS,    /* Local APIC Spurious Vector No */
    INT_NUM_LOAPIC_SM,          /* Local APIC SM Vector No */
    INT_NUM_LOAPIC_SM + 1,      /* Local APIC SM Vector No */
    INT_NUM_LOAPIC_SM + 2,      /* Local APIC SM Vector No */
    INT_NUM_LOAPIC_SM + 3,      /* Local APIC SM Vector No */
    INT_NUM_LOAPIC_IPI,         /* Local APIC IPI Vector No */
    INT_NUM_LOAPIC_IPI + 1,     /* Local APIC IPI Vector No */
    INT_NUM_LOAPIC_IPI + 2,     /* Local APIC IPI Vector No */
    INT_NUM_LOAPIC_IPI + 3,     /* Local APIC IPI Vector No */
    INT_NUM_LOAPIC_IPI + 4,     /* Local APIC IPI Vector No */
    INT_NUM_LOAPIC_IPI + 5,     /* Local APIC IPI Vector No */
    INT_NUM_LOAPIC_IPI + 6,     /* Local APIC IPI Vector No */
    INT_NUM_LOAPIC_IPI + 7      /* Local APIC IPI Vector No */
    };
#endif  /* defined (INCLUDE_VIRTUAL_WIRE_MODE) */

#if defined(INCLUDE_SYMMETRIC_IO_MODE)
/*
 * Under INCLUDE_SYMMETRIC_IO_MODE sysInumTbl is created dynamically during
 * the IO Apic driver initialization.
 *
 * Multiple IO Apics are used in high-end servers to distribute irq load.
 * At this time there may be as many as 8 IO Apics in a system. The most common
 * number of inputs on a single IO Apic is 24, but they are also designed for
 * 16/32/or 64 inputs.
 *
 * The common case here is a 1:1 irq<=>pin mapping.
 *
 * The mapping is critical to system operation. We need to support the first
 * 16 interrupts as legacy ISA/EISA.
 *
 * irq 0,1,2,3,4,6,8,12,13,14, and 15 are typically consumed by legacy devices.
 *
 * irq 5,7,9,10, and 11 are typically available for general use (though 5 is
 * sometimes used for audio devices and 7 is used for LPT).
 *
 * 16,17,18, and 19 are typically used for standard PCI.
 *
 * Each of the interrupts can be used programmed with any of the 224 vectors the
 * CPU is capable of. The first IO Apic will handle the ISA/EISA interrupts.
 *
 * The way this will work: if (irq <= 16) just map the irq to whatever pin is
 * requested. If (irq > 16) we enforce the mapping irq =
 * (apic*num_ioapic_entriess)+pin.
 *
 * This means that given any irq we can always derive the apic and pin.
 *
 *          apic from irq => irq / num_ioapic_entriess
 *
 *          pin from irq  => irq % num_ioapic_entriess
 */
#endif  /* INCLUDE_SYMMETRIC_IO_MODE */

/* IRQ vs IntNum table for default interrupt handling */

UINT8   dfltInumTbl[]    =
    {
    INT_NUM_IRQ0,               /* IRQ  0 Vector No */
    INT_NUM_IRQ0 + 1,           /* IRQ  1 Vector No */
    INT_NUM_IRQ0 + 2,           /* IRQ  2 Vector No */
    INT_NUM_IRQ0 + 3,           /* IRQ  3 Vector No */
    INT_NUM_IRQ0 + 4,           /* IRQ  4 Vector No */
    INT_NUM_IRQ0 + 5,           /* IRQ  5 Vector No */
    INT_NUM_IRQ0 + 6,           /* IRQ  6 Vector No */
    INT_NUM_IRQ0 + 7,           /* IRQ  7 Vector No */
    INT_NUM_IRQ0 + 8,           /* IRQ  8 Vector No */
    INT_NUM_IRQ0 + 9,           /* IRQ  9 Vector No */
    INT_NUM_IRQ0 + 10,          /* IRQ 10 Vector No */
    INT_NUM_IRQ0 + 11,          /* IRQ 11 Vector No */
    INT_NUM_IRQ0 + 12,          /* IRQ 12 Vector No */
    INT_NUM_IRQ0 + 13,          /* IRQ 13 Vector No */
    INT_NUM_IRQ0 + 14,          /* IRQ 14 Vector No */
    INT_NUM_IRQ0 + 15,          /* IRQ 15 Vector No */
    };

UINT8 *sysInumTbl;
UINT32 sysInumTblNumEnt;

/*
 * The cache control flags and MTRRs operate hierarchically for restricting
 * caching.  That is, if the CD flag is set, caching is prevented globally.
 * If the CD flag is clear, either the PCD flags and/or the MTRRs can be
 * used to restrict caching.  If there is an overlap of page-level caching
 * control and MTRR caching control, the mechanism that prevents caching
 * has precedence.  For example, if an MTRR makes a region of system memory
 * uncachable, a PCD flag cannot be used to enable caching for a page in
 * that region.  The converse is also true; that is, if the PCD flag is
 * set, an MTRR cannot be used to make a region of system memory cacheable.
 * If there is an overlap in the assignment of the write-back and write-
 * through caching policies to a page and a region of memory, the write-
 * through policy takes precedence.  The write-combining policy takes
 * precedence over either write-through or write-back.
 */

LOCAL MTRR sysMtrr =
    {                   /* MTRR table */
    {0,0},              /* MTRR_CAP register */
    {0,0},              /* MTRR_DEFTYPE register */

                        /* Fixed Range MTRRs */

    {
    {{MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB}},
    {{MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB, MTRR_WB}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_WC, MTRR_WC, MTRR_WC, MTRR_WC}},
    {{MTRR_WP, MTRR_WP, MTRR_WP, MTRR_WP, MTRR_WP, MTRR_WP, MTRR_WP, MTRR_WP}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC}},
    {{MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC, MTRR_UC}}
    },
    {
    {0LL, 0LL},         /* Variable Range MTRRs */
    {0LL, 0LL},
    {0LL, 0LL},
    {0LL, 0LL},
    {0LL, 0LL},
    {0LL, 0LL},
    {0LL, 0LL},
    {0LL, 0LL}}
    };

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8))
#ifdef  INCLUDE_CPU_PERFORMANCE_MGMT
#ifndef  INCLUDE_ACPI_CPU_CONFIG
/*
 * ACPI configuration tables.  Used as static tables when ACPI isn't included.
 *
 * Information contained in this table would be considered proprietary by
 * Intel.  ACPI can be used to construct the table by executing
 * sysBoardInfoPrint in a target shell using a vxWorks image created
 * with the INCLUDE_ACPI_CPU_CONFIG component.
 */

ACPI_BOARD_INFO  sysBspBoardInfo =
    {
    0,              /* acpiNumCpus */
    NULL,           /* pAcpiCpu */
    0,              /* acpiNumThermZones */
    NULL,           /* pThermalZone */
    };
ACPI_BOARD_INFO  *pSysBoardInfo = &sysBspBoardInfo;  /* static table */
#else    /* INCLUDE_ACPI_CPU_CONFIG */
ACPI_BOARD_INFO  *pSysBoardInfo = &sysAcpiBoardInfo; /* constructed by ACPI */
#endif   /* INCLUDE_ACPI_CPU_CONFIG */
#else    /* INCLUDE_CPU_PERFORMANCE_MGMT */
ACPI_BOARD_INFO  *pSysBoardInfo = NULL;
#endif   /* INCLUDE_CPU_PERFORMANCE_MGMT */
#endif   /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8) */

/* forward declarations */

void   sysStrayInt   (void);
char * sysPhysMemTop (void);
STATUS sysMmuMapAdd  (void * address, UINT len, UINT initialStateMask,
                      UINT initialState);

void sysIntInitPIC (void);
void sysIntEoiGet  (VOIDFUNCPTR * vector,
                    VOIDFUNCPTR * routineBoi, _Vx_usr_arg_t * parameterBoi,
                    VOIDFUNCPTR * routineEoi, _Vx_usr_arg_t * parameterEoi);

void sys1msDelay ();
void sysUsDelay  (int);
void sysMsDelay  (int);
UINT64 sysCalibrateTSC (void);
UINT64 sysCpuIdCalcFreq (void);
UINT64 sysGetTSCCountPerSec (void);

STATUS sysHpetOpen(void);

#if defined (INCLUDE_DISABLE_SMI)
void smiMmuMap (void);
void smiDisable(void);
#endif

#if defined (_WRS_CONFIG_SMP)
void sysCpuInit (void);

UINT32 sysCpuAvailableGet(void);

STATUS sysCpuStart(int cpuNum, WIND_CPU_STATE *cpuState);
STATUS vxCpuStateInit(unsigned int cpu, WIND_CPU_STATE *cpuState,
                      char *stackBase, FUNCPTR entry);
STATUS sysCpuEnable(unsigned int cpuNum, WIND_CPU_STATE *cpuState);
STATUS sysCpuDisable(int cpuNum);

STATUS sysCpuStop(int cpu);
STATUS sysCpuStop_APs(void);
STATUS sysCpuStop_ABM(void);

STATUS sysCpuReset(int cpu);
STATUS sysCpuReset_APs(void);
STATUS sysCpuReset_ABM(void);
#endif /* (_WRS_CONFIG_SMP) */

#include <hwif/fw/bios/vxBiosE820Lib.h>

#if !defined (INCLUDE_SYMMETRIC_IO_MODE) || !defined (INCLUDE_VIRTUAL_WIRE_MODE)
unsigned int dummyGetCpuIndex (void);
#endif  /* !(INCLUDE_SYMMETRIC_IO_MODE) || !(INCLUDE_VIRTUAL_WIRE_MODE) */

#ifdef  INCLUDE_USER_SYSLIB_DECLARATIONS
#include "userSyslibDeclarations.h"                 /* user extension hook */
#endif

/* -------------------- sources -------------------- */

#ifndef INCLUDE_USR_NVRAM_IMPLEMENTATION
#if (NV_RAM_SIZE != NONE)
#include "sysNvRam.c"
#else   /* default to nullNvRam */
#include <mem/nullNvRam.c>
#endif  /* (NV_RAM_SIZE != NONE) */
#endif  /* !INCLUDE_USR_NVRAM_IMPLEMENTATION */

#if (!defined(INCLUDE_SYMMETRIC_IO_MODE))
#include <intrCtl/i8259Intr.c>
#else
#define SYMMETRIC_IO_MODE
#include <intrCtl/i8259Intr.c>
#endif

#ifdef  INCLUDE_PCI_BUS         /* BSP PCI bus & config support */
#include <drv/pci/pciConfigLib.h>
#include <drv/pci/pciIntLib.h>
#include <drv/pci/pciAutoConfigLib.h>

/* 24-bit PCI network class ethernet subclass and prog. I/F code */

#include "pciCfgIntStub.c"
#endif  /* INCLUDE_PCI_BUS */

#ifdef  INCLUDE_DEBUG_STORE
#include "sysDbgStr.c"          /* Debug Store support */
#endif  /* INCLUDE_DEBUG_STORE */

#if defined (_WRS_CONFIG_SMP)
#include "sysSMP.c"             /* SMP and multicore CPU support */
#endif  /* defined(_WRS_CONFIG_SMP) */

#ifdef  INCLUDE_USER_SYSLIB_SOURCES
#include "userSyslibSources.h"                      /* user extension hook */
#endif

/* -------------------- vxbus ---------------------- */

#ifdef INCLUDE_VXBUS
#include <hwif/vxbus/vxBus.h>
#ifndef INCLUDE_HWCONF
/* Include hwconf.c if cdf configlette not available */
#include "hwconf.c"
#endif  /* INCLUDE_HWCONF */

IMPORT device_method_t * pSysPlbMethods;

#if defined (INCLUDE_PC_CONSOLE) && defined (DRV_TIMER_I8253)
LOCAL void sysConBeep (BOOL);
IMPORT char * pcConBeep_desc;
#endif /* INCLUDE_PC_CONSOLE && DRV_TIMER_I8253 */

LOCAL struct vxbDeviceMethod pc386PlbMethods[] =
    {
#if defined (INCLUDE_PC_CONSOLE) && defined (DRV_TIMER_I8253)
    DEVMETHOD(pcConBeep, sysConBeep),
#endif /* INCLUDE_PC_CONSOLE && DRV_TIMER_I8253 */
    { 0, 0 }
    };
#endif /* INCLUDE_VXBUS */

#ifdef  INCLUDE_USER_SYSLIB_VXBUS
#include "userSyslibVxbus.h"                        /* user extension hook */
#endif

/* -------------------- functions ------------------ */

#ifdef  INCLUDE_USER_SYSLIB_FUNCTIONS_PRE
#include "userSyslibFunctionsPre.c"                 /* user extension hook */
#endif

/******************************************************************************
*
* sysModelInit - Initialize sys model string
*
* Instead of using a hardcoded model string, this routine will try to use
* brandstring in CPUID. If it exists, then copy it. If it does not exist,
* then set it to a generic default SYS_MODEL string. Also it will copy
* interrupt mode and table type (ACPI | MPTABLE | USRTABLE) into this string.
*
* RETURNS: NONE.
*
* NOMANUAL
*/

void sysModelInit (void)
    {
    char* pStr = (char*)(&(sysCpuId.brandString[0]));
    char* pTblStr;
    int tBlStrSize = 0;

    bzero(sysModelStr, sizeof(sysModelStr));

    /* Set MPTABLE type */

#if defined (INCLUDE_ACPI_BOOT_OP)
    pTblStr = " (ACPI)";
    tBlStrSize = 7;
#elif defined (INCLUDE_MPTABLE_BOOT_OP)
    pTblStr = " (MPTABLE)";
    tBlStrSize = 10;
#elif defined (INCLUDE_USR_BOOT_OP)
    pTblStr = " (USRTABLE)";
    tBlStrSize = 11;
#else
    pTblStr = "";
    tBlStrSize = 0;
#endif

    /* use brandstring as CPU info string */

    if (*pStr != 0)
        {
        /* skip space and then copy */

        while(*pStr != 0 && isspace ((int) (*pStr))) pStr++;
        snprintf (sysModelStr, sizeof (sysModelStr), pStr);

        /* copy interrupt mode */

#if defined (_WRS_CONFIG_SMP)
        strncat(sysModelStr," SMP", 4);
#elif defined (INCLUDE_SYMMETRIC_IO_MODE)
        strncat(sysModelStr," SYMM-IO", 8);
#elif defined (INCLUDE_VIRTUAL_WIRE_MODE)
        strncat(sysModelStr," VIRTWIRE", 9);
#else
        strncat(sysModelStr," PIC", 4);
#endif
        }
    else /* in case the sysCpuId contains no brandstring (before Pentium4) */
        {
        snprintf (sysModelStr, sizeof (sysModelStr), SYS_MODEL);
        }

    strncat(sysModelStr, pTblStr, tBlStrSize);
    }

/******************************************************************************
*
* sysModel - return the model name of the CPU board
*
* This routine returns the model name of the CPU board.
*
* RETURNS: A pointer to the string contains CPU basic information
*/

char *sysModel (void)
   {
    return (SYS_MODEL);
   }

/*******************************************************************************
*
* sysBspRev - return the BSP version and revision number
*
* This routine returns a pointer to a BSP version and revision number, for
* example, 1.1/0. BSP_REV is concatenated to BSP_VERSION and returned.
*
* RETURNS: A pointer to the BSP version/revision string.
*/

char * sysBspRev (void)
    {
    return (BSP_VERSION BSP_REV);
    }

#ifdef INCLUDE_SYS_HW_INIT_0

/*******************************************************************************
*
* sysHwInit0 - BSP-specific hardware initialization
*
* This routine is called from usrInit() to perform BSP-specific initialization
* that must be done before cacheLibInit() is called and/or the BSS is cleared.
*
* The vxCpuIdProbe() routine [or sysCpuProbe() in releases prior to 6.9] is
* called to identify IA target CPU variants, and the features or functions
* supported by the target CPU.  This information must be obtained relatively
* early during system hardware initialization, as some support libraries
* (mmuLib, cacheLib, etc.) will use the processor feature information to
* enable or disable architecture-specific and/or BSP-specific functionality.
*
* RETURNS: N/A
*
* \NOMANUAL
*/

void sysHwInit0 (void)
    {
    VX_CPUID_ENTRY entry = {0x00000F00, VX_CPUID_PENTIUM4}; /* AMD APU A1 eKabini */

#ifdef  INCLUDE_USER_SYSLIB_HWINIT0_PRE
#include "userSyslibHwinit0Pre.c"                   /* user extension hook */
#endif

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)
    _func_cpuIndexGet = NULL;
#else
    _func_cpuIndexGet = &dummyGetCpuIndex;
#endif /* (INCLUDE_SYMMETRIC_IO_MODE) || (INCLUDE_VIRTUAL_WIRE_MODE) */

#ifdef  INCLUDE_VXBUS
    sysConfInit0 ();        /* early bootApp dependent initialization */
#endif /* INCLUDE_VXBUS */

#ifdef _WRS_CONFIG_SMP

    /* temporarily use structure to load the brand new GDT into RAM */

    _WRS_WIND_VARS_ARCH_SET(vxCpuIndexGet(),
                            pSysGdt,
                            pSysGdt);
#endif /* _WRS_CONFIG_SMP */

    sysInitGDT ();

    /* Check for an external or multiboot bootline */

    multibootInit ();

#ifdef  INCLUDE_USER_SYSLIB_HWINIT0_POST
#include "userSyslibHwinit0Post.c"                  /* user extension hook */
#endif

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9))
    (void) sysCpuProbe ();
    sysProcessor = X86CPU_DEFAULT;
#else
    /* initialize CPUID library */

    vxCpuIdLibInit();

    /* add AMD APU A1 eKabini */

    vxCpuIdAdd (&entry);

    /* perform CPUID probe */

    vxProcessor = sysProcessor = vxCpuIdProbe( &vxCpuId );

    /*
     * The following is needed for the AMD platform
     *  we removed MSRs support since we're only able to handle Intel's ones ....
     */
    
    vxCpuId.std.featuresEdx &=~(CPUID_MSR  | CPUID_PSNUM | CPUID_DTS | CPUID_MTRR |
     		                    CPUID_ACPI | CPUID_SS    | CPUID_TM  | VX_CPUID_PBE);

    /* LAURENT */    	

    /*
     * for backward compatibility, populate sysCpuId with
     * vxCpuId standard feature structure contents
     */

    bcopy ((char *)&vxCpuId.std, (char *)&sysCpuId, (int)sizeof(VX_CPUID_STD));
#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9) */
    }

#endif  /* INCLUDE_SYS_HW_INIT_0 */

/*******************************************************************************
*
* sysHwInit - initialize the system hardware
*
* This routine initializes various features of the i386/i486 board.
* It is called from usrInit() in usrConfig.c.
*
* NOTE: This routine should not be called directly by the user application.
*
* RETURNS: N/A
*/

void enable_serial()
{
	sysOutByte(0x4E, 0x55);

	sysOutByte(0x4E, 0x0);
	sysOutByte(0x4F, 0x28);

	sysOutByte(0x4E, 0x1);
	sysOutByte(0x4F, 0x98| 1 << 2);

	sysOutByte(0x4E, 0x2);
	sysOutByte(0x4F, 0x08 | 1 << 7);
	
	sysOutByte(0x4E, 0x7);
	sysOutByte(0x4F, 0x0);
	
	sysOutByte(0x4E, 0xA);
	sysOutByte(0x4F, 0x0 | 1 << 6);

	sysOutByte(0x4E, 0x33);
	sysOutByte(0x4F, 0x0);

	sysOutByte(0x4E, 0xC);
	sysOutByte(0x4F, 0x2);

	sysOutByte(0x4E, 0x34);
	sysOutByte(0x4F, 0x0);

	sysOutByte(0x4E, 0x25);
	sysOutByte(0x4F, 0xFE);

	sysOutByte(0x4E, 0x28);
	sysOutByte(0x4F, 0x4);

	sysOutByte(0x4e, 0xAA);
}

void sysHwInit (void)
    {
    PHYS_MEM_DESC *pMmu;
    int ix = 0;
    /* enable_serial();*/

    /*sysSerialHwInit ();  */    /* initialize serial data structure */
    /* earlyPrint("bao\n"); */
#ifdef  INCLUDE_USER_SYSLIB_HWINIT_PRE
#include "userSyslibHwinitPre.c"                /* user extension hook */
#endif

#ifdef  INCLUDE_EARLY_I_CACHE_ENABLE
    cacheEnable (INSTRUCTION_CACHE);            /* enable instruction cache */
#endif /* INCLUDE_EARLY_I_CACHE_ENABLE */
#ifdef INCLUDE_EARLY_D_CACHE_ENABLE
    cacheEnable (DATA_CACHE);                   /* enable data cache */
#endif

#if defined(INCLUDE_VIRTUAL_WIRE_MODE)
    sysInumTbl = &vwInumTbl[0];
    sysInumTblNumEnt = NELEMENTS (vwInumTbl);
#elif defined(INCLUDE_SYMMETRIC_IO_MODE)
#if defined(_WRS_CONFIG_SMP)
    /* store trampoline code space for restoration on warm reboot */

    bcopy ((char *)(CPU_ENTRY_POINT),
                    (char *)(&tramp_code[0]),
                    (int)CPU_AP_AREA);

#endif /* (defined(_WRS_CONFIG_SMP) */
#else
    sysInumTbl = &dfltInumTbl[0];
    sysInumTblNumEnt = NELEMENTS (dfltInumTbl);
#endif  /* defined(INCLUDE_VIRTUAL_WIRE_MODE) */

#ifdef _WRS_CONFIG_SMP

    /*
     * establish pointers to global descriptor table & task state segment
     * TSS is assigned a cpu unique structure...
     */

    _WRS_WIND_VARS_ARCH_SET(vxCpuIndexGet(),
                            pSysGdt,
                            pSysGdt);

    _WRS_WIND_VARS_ARCH_SET(vxCpuIndexGet(),
                            sysTssCurrent,
                            sysTssCurrent);

    _WRS_WIND_VARS_ARCH_SET(vxCpuIndexGet(),
                            vxIntStackEnabled,
                            1);
#endif /* _WRS_CONFIG_SMP */

    /* enable the MTRR (Memory Type Range Registers) */

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8))
    if ((vxCpuId.std.featuresEdx & CPUID_MTRR) == CPUID_MTRR)
#else
    if ((sysCpuId.featuresEdx & CPUID_MTRR) == CPUID_MTRR)
#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8) */
        {
        pentiumMtrrDisable ();      /* disable MTRR */
#ifdef INCLUDE_MTRR_GET
        (void) pentiumMtrrGet (&sysMtrr); /* get MTRR initialized by BIOS */
#else
        (void) pentiumMtrrSet (&sysMtrr); /* set your own MTRR */
#endif /* INCLUDE_MTRR_GET */
        pentiumMtrrEnable ();       /* enable MTRR */
        }
#ifdef INCLUDE_PMC

    /* enable PMC (Performance Monitoring Counters) */

    pentiumPmcStop ();          /* stop PMC0 and PMC1 */
    pentiumPmcReset ();         /* reset PMC0 and PMC1 */

#endif /* INCLUDE_PMC */

    /* enable the MCA (Machine Check Architecture) */

    pentiumMcaEnable (TRUE);

#ifdef INCLUDE_SHOW_ROUTINES

    /*
     * if excMcaInfoShow is not NULL, it is called in the default
     * exception handler when Machine Check Exception happened
     */

    {
    IMPORT FUNCPTR excMcaInfoShow;
    excMcaInfoShow = (FUNCPTR) pentiumMcaShow;
    }
#endif  /* INCLUDE_SHOW_ROUTINES */

#ifdef INCLUDE_SHOW_ROUTINES
    vxShowInit ();
#endif  /* INCLUDE_SHOW_ROUTINES */

    /* initialize the number of active mappings (sysPhysMemDescNumEnt) */

    pMmu = &sysPhysMemDesc[0];

    for (ix = 0; ix < NELEMENTS (sysPhysMemDesc); ix++)
        if (pMmu->virtualAddr != (VIRT_ADDR)DUMMY_VIRT_ADDR)
            pMmu++;
        else
            break;

    sysPhysMemDescNumEnt = ix;

    /*
     * Make sure autosize completes prior to vxBus initialization
     */

    sysMemTop();

    /* load ACPI tables */

    sysConfInit ();        /* bootApp dependent initialization */

    /* initialize PCI library */

#if defined (INCLUDE_USR_MPAPIC) && \
   (defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE))

    /* ACPI initialzation writes to static _MP_ struct location */

    usrMpapicInit (TRUE, (char *) MPAPIC_DATA_START);
#endif /* INCLUDE_USR_MPAPIC */

    /* Calibrate TSC before VxBus subsystem init */

    (void)sysCalibrateTSC ();

#ifdef  INCLUDE_VXBUS

    pSysPlbMethods = pc386PlbMethods;

    /* Replace the delay functions */

    _vxb_delayRtn   = sys1msDelay;  /* This must be a 1 ms delay */
    _vxb_usDelayRtn = sysUsDelay;
    _vxb_msDelayRtn = sysMsDelay;

#ifdef  INCLUDE_USER_SYSLIB_HWINIT_VXB_PRE
#include "userSyslibHwinitVxbPre.c"                 /* user extension hook */
#endif

    hardWareInterFaceInit();                        /* VXBUS Subsystem Init */

#ifdef  INCLUDE_USER_SYSLIB_HWINIT_VXB_POST
#include "userSyslibHwinitVxbPost.c"                /* user extension hook */
#endif

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)

    /* establish link to vxb APIC methods */

    vxbDevIterate(apicIntrIntHelper, NULL, VXB_ITERATE_INSTANCES);

#endif  /* INCLUDE_SYMMETRIC_IO_MODE || INCLUDE_VIRTUAL_WIRE_MODE */

    /* Connect intEnable/intDisable function pointers */

    sysIntLvlEnableRtn = (FUNCPTR)sysIntEnablePIC;
    sysIntLvlDisableRtn = (FUNCPTR)sysIntDisablePIC;

    /* initialize the PIC (Programmable Interrupt Controller) */

    sysIntInitPIC ();           /* should be after the PCI init for IOAPIC */

    intEoiGet = sysIntEoiGet;   /* function pointer used in intConnect () */

#if defined (_WRS_CONFIG_SMP)

    /* init IPI vector, assign default handlers... */

    ipiVecInit (INT_NUM_LOAPIC_IPI);

#endif /* (_WRS_CONFIG_SMP) */

#ifdef INCLUDE_PCI_BUS

    /* initialize PCI devices */

    /*
     * PCI-to-PCI bridge initialization should be done here (if it is needed).
     */

#ifdef  INCLUDE_USER_SYSLIB_HWINIT_VXB_PCI
#include "userSyslibHwinitVxbPci.c"                 /* user extension hook */
#endif

#endif  /* INCLUDE_PCI_BUS */

#endif  /* INCLUDE_VXBUS */

#if (!defined(DRV_SIO_NS16550))
    sysSerialHwInit ();      /* initialize serial data structure */
#endif /* !defined(DRV_SIO_NS16550) */

#ifdef  INCLUDE_USER_SYSLIB_HWINIT_POST
#include "userSyslibHwinitPost.c"                   /* user extension hook */
#endif
    }

/*******************************************************************************
*
* sysHwInit2 - additional system configuration and initialization
*
* This routine connects system interrupts and does any additional
* configuration necessary.
*
* RETURNS: N/A
*/

void sysHwInit2 (void)
    {
#ifdef  INCLUDE_USER_SYSLIB_HWINIT2_PRE
#include "userSyslibHwinit2Pre.c"                   /* user extension hook */
#endif

#if defined(INCLUDE_FAST_REBOOT) && defined(INCLUDE_MULTI_STAGE_BOOT)

    UINT32 DummyAddress = (UINT32)&sysHwInit2;

    /*
     * Check if we are in the bootrom, works for INCLUDE_BOOT_APP project
     * built bootroms as well. We do this by checking the memory location
     * of the function we are currently executing.
     */

    if (DummyAddress > SYS_RAM_HIGH_ADRS)
        {

        /*
         * save the brand new bootrom image that will be protected by MMU.
         * The last 2 bytes of ROM_SIZE are for the checksum.
         * - compression would minimize the DRAM usage.
         * - when restore, jumping to the saved image would be faster.
         */

         *(UINT32 *)BOOT_IMAGE_ADRS = memRom;

         bcopy ((char *)ROM_TEXT_ADRS, (char *)((ULONG)memRom),
             (*(UINT32 *)BOOT_IMAGE_SIZE));
             *(UINT16 *)((ULONG)memRom + (*(UINT32 *)BOOT_IMAGE_SIZE) - 2) =
             checksum ((UINT16 *)((ULONG)memRom),
                 (*(UINT32 *)BOOT_IMAGE_SIZE) - 2);
         }

#endif /* defined(INCLUDE_FAST_REBOOT) && defined(INCLUDE_MULTI_STAGE_BOOT) */

#ifdef  INCLUDE_DEBUG_STORE
    sysDbgStrInit ();
#endif  /* INCLUDE_DEBUG_STORE */

    /* disable SMI to improve real-time performance */

#ifdef INCLUDE_DISABLE_SMI
    smiMmuMap();
    smiDisable();
#endif

#if (!defined(DRV_SIO_NS16550))
    /* connect serial interrupt */
    sysSerialHwInit2();
#endif /* !defined(DRV_SIO_NS16550) */

    /* connect stray (spurious/phantom) interrupt */

#if defined(INCLUDE_VIRTUAL_WIRE_MODE)
    (void)intConnect (INUM_TO_IVEC (INT_NUM_LOAPIC_SPURIOUS), sysStrayInt, 0);
    (void)intConnect (INUM_TO_IVEC (INT_NUM_GET (LPT_INT_LVL)), sysStrayInt, 0);
#elif defined(INCLUDE_SYMMETRIC_IO_MODE)
    (void)intConnect (INUM_TO_IVEC (INT_NUM_LOAPIC_SPURIOUS), sysStrayInt, 0);
#else
    (void)intConnect (INUM_TO_IVEC (INT_NUM_GET (LPT_INT_LVL)), sysStrayInt, 0);
    (void)intConnect (INUM_TO_IVEC (INT_NUM_GET (PIC_SLAVE_STRAY_INT_LVL)),
                      sysStrayInt, 0);
#endif  /* defined(INCLUDE_VIRTUAL_WIRE_MODE) */

#if defined (_WRS_CONFIG_SMP)

    /* connect IPI handlers, up to IPI_MAX_HANDLERS (=8) */

    ipiConnect ((INT_NUM_LOAPIC_IPI + 0), ipiHandlerShutdown);

#endif /* (_WRS_CONFIG_SMP) */

#ifdef INCLUDE_DEV_PWR_MGMT
    pwrDeviceLibInit ();                 /* device power management */
#endif /* INCLUDE_DEV_PWR_MGMT */

#ifdef INCLUDE_VXBUS

#ifdef  INCLUDE_USER_SYSLIB_HWINIT2_VXB_PRE
#include "userSyslibHwinit2VxbPre.c"                /* user extension hook */
#endif

    vxbDevInit();

#ifdef  INCLUDE_USER_SYSLIB_HWINIT2_VXB_POST
#include "userSyslibHwinit2VxbPost.c"               /* user extension hook */
#endif

#ifdef INCLUDE_SIO_UTILS
    sysSerialConnectAll();
#endif /* INCLUDE_SIO_UTILS */

#ifdef INCLUDE_PCI_BUS

#ifdef  INCLUDE_USER_SYSLIB_HWINIT2_VXB_PCI
#include "userSyslibHwinit2VxbPci.c"                /* user extension hook */
#endif

#endif  /* INCLUDE_PCI_BUS */

    taskSpawn("tDevConn", 11, 0, 10000,
              sysDevConnect, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);

#endif /* INCLUDE_VXBUS */

    /* detect if it is SCH to set the correct reset value */

    if (sysCheckSCH() == FALSE)
        {
        sysColdResetType = SYS_ICH_WARM_RESET;
        }
    else
        {
        sysColdResetType = SYS_SCH_WARM_RESET;
        }

    /* initialize model string */

    sysModelInit();

#ifdef  INCLUDE_USER_SYSLIB_HWINIT2_POST
#include "userSyslibHwinit2Post.c"                  /* user extension hook */
#endif
    }

#ifdef INCLUDE_VXBUS
/*******************************************************************************
*
* sysDevConnect - Task to do HWIF Post-Kernel Connection
*
* RETURNS: N/A
*/

LOCAL STATUS sysDevConnect (void)
    {
    vxbDevConnect();

#ifdef INCLUDE_EXC_TASK
    /*
     * Wait for exception handling package initialization to complete
     * prior to exit
     */

    {
    int i;
    for (i=0; i<20; i++)
        {
        if (_func_excJobAdd != NULL)
            {
            return OK;
            }
        taskDelay(sysClkRateGet()/2);
        }
    }
#endif /* INCLUDE_EXC_TASK */

    return OK;
    }
#endif /* INCLUDE_VXBUS */

#ifdef  INCLUDE_LOCAL_MEM_AUTOSIZE
#include "sysMemAutosize.c"
#endif  /* INCLUDE_LOCAL_MEM_AUTOSIZE */

/*******************************************************************************
*
* sysPhysMemTop - get the address of the top of physical memory
*
* INTERNAL
* In the case of IA-32 processors, PHYS_MEM_MAX will be 4GB (2^32 bytes) or
* 64GB (2^36 bytes) if the 36-bit Physical Address Extension (PAE) is enabled
* on select processor models.  However, because the tool-chain and sysMemTop()
* API are 32-bit, this routine currently will not autosize a 36-bit address
* space.
*
* Any additional 32-bit addressable memory segments above the segment at
* physical address 0x100000 is mapped using the MMU.  All memory locations
* below physical address 0x100000 are assumed to be reserved memory.
*
* RETURNS:  The address of the top of physical memory.
*/

char * sysPhysMemTop (void)
    {
    PHYS_MEM_DESC * pMmu;       /* points to memory desc. table entries */
    UINT32          tempMTP;    /* temporary variable to stop warnings */

    GDTPTR gDtp;
    INT32 nGdtEntries, nGdtBytes;
    unsigned char gdtr[6];      /* temporary GDT register storage */

    if (memTopPhys != NULL)
        {
        return (memTopPhys);
        }

#ifdef  INCLUDE_LOCAL_MEM_AUTOSIZE
    memTopPhys = sysMemAutosize();
#endif  /* INCLUDE_LOCAL_MEM_AUTOSIZE */

    if (memTopPhys == NULL)
        {
        /* use preset memory size default */

        memTopPhys = (char *)(LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE);
        }

    /* copy the global descriptor table from RAM/ROM to RAM */

    /* Get the Global Data Table Descriptor */

    vxGdtrGet ( (long long int *)&gDtp );

    /* Extract the number of bytes */

    nGdtBytes = (INT32)gDtp.wGdtr[0];

    /* and calculate the number of entries */

    nGdtEntries = (nGdtBytes + 1 ) / sizeof(GDT);

#ifdef _WRS_CONFIG_SMP
    bcopy ((char *)sysGdt,
           (char *)(_WRS_WIND_VARS_ARCH_ACCESS(pSysGdt)),
           nGdtEntries * sizeof(GDT));
#else
    bcopy ((char *)sysGdt, (char *)pSysGdt, nGdtEntries * sizeof(GDT));
#endif

    /*
     * We assume that there are no memory mapped IO addresses
     * above the "memTopPhys" if INCLUDE_PCI_BUS is not defined.
     * Thus we set the "limit" to get the General Protection Fault
     * when the memory above the "memTopPhys" is accessed.
     */

#if !defined (INCLUDE_PCI_BUS) && \
    !defined (INCLUDE_MMU_BASIC) && !defined (INCLUDE_MMU_FULL)
    {
    int   ix;
    int   limit = (((UINT32) memTopPhys) >> 12) - 1;
#ifdef _WRS_CONFIG_SMP
    GDT   *pGdt = (_WRS_WIND_VARS_ARCH_ACCESS(pSysGdt));
#else
    GDT   *pGdt = pSysGdt;
#endif /* _WRS_CONFIG_SMP */

    for (ix = 1; ix < GDT_ENTRIES; ++ix)
        {
        ++pGdt;
        pGdt->limit00 = limit & 0x0ffff;
        pGdt->limit01 = ((limit & 0xf0000) >> 16) | (pGdt->limit01 & 0xf0);
        }
    }
#endif  /* INCLUDE_PCI_BUS */

    /* load the global descriptor table. set the MMU table */

    *(short *) &gdtr[0] = GDT_ENTRIES * sizeof (GDT) - 1;
#ifdef _WRS_CONFIG_SMP
    *(int *) &gdtr[2] = (int)(_WRS_WIND_VARS_ARCH_ACCESS(pSysGdt));
#else
    *(int *) &gdtr[2] = (int)(pSysGdt);
#endif  /* _WRS_CONFIG_SMP */

    sysLoadGdt ((char *)gdtr);

    /* set the MMU descriptor table */

    /* VM_PAGE_SIZE aligned */

    tempMTP = (UINT32)memTopPhys & ~(VM_PAGE_SIZE - 1);
    memTopPhys = (char *)tempMTP;

#if (VM_PAGE_SIZE == PAGE_SIZE_4KB)
    pMmu = &sysPhysMemDesc[4];      /* 5th entry: above 1.5MB upper memory */
    pMmu->len = (UINT32) memTopPhys - (UINT32) pMmu->physicalAddr;
#else   /* (VM_PAGE_SIZE == PAGE_SIZE_4KB) */
    pMmu = &sysPhysMemDesc[2];      /* 3rd entry: above 8MB upper memory */
    pMmu->len = (UINT32) memTopPhys - (UINT32) pMmu->physicalAddr;
#endif  /* (VM_PAGE_SIZE == PAGE_SIZE_4KB) */

#if defined (INCLUDE_LOCAL_MEM_AUTOSIZE) && !defined (EXCLUDE_BIOS_E820_REMAP)
    {
    BIOS_E820_MAP_DESC mapDesc;
    unsigned int ix,adr32;

    /* map additional E820 segments above the LOCAL_MEM_LOCAL_ADRS segment */

    for (ix = 0; ; ix++)
        {

        /*
         * for each index continue until vxBiosE820MapDescGet() returns ERROR
         */

        if (vxBiosE820MapDescGet (ix, &mapDesc) == ERROR)
            break;

        /*
         * only map usable entries
         * do not map segment at address 0x0
         * segment at LOCAL_MEM_LOCAL_ADRS already mapped
         * must be a 32 bit addressable segment
         */

        if (( mapDesc.type == BIOS_E820_MEM_TYPE_USABLE) &&
            ( mapDesc.addr != 0x0 ) &&
            ( mapDesc.addr != LOCAL_MEM_LOCAL_ADRS) &&
            ( mapDesc.addr < SIZE_ADDR_SPACE_32 ))
            {
            adr32 = mapDesc.addr;
            if (sysMmuMapAdd ((void *)adr32,mapDesc.len,
                              VM_STATE_MASK_FOR_ALL,
                              VM_STATE_FOR_MEM_APPLICATION) == ERROR)
                break;
            }
        }
    }
#endif  /* defined (INCLUDE_LOCAL_MEM_AUTOSIZE) &&
          !defined (EXCLUDE_BIOS_E820_REMAP) */

    return (memTopPhys);
    }


/*******************************************************************************
*
* sysMemTop - get the address of the top of VxWorks memory
*
* This routine returns a pointer to the first byte of memory not
* controlled or used by VxWorks.
*
* The user can reserve memory space by defining the macro USER_RESERVED_MEM
* in config.h.  This routine returns the address of the reserved memory
* area.  The value of USER_RESERVED_MEM is in bytes.
*
* RETURNS: The address of the top of VxWorks memory.
*/

char * sysMemTop (void)
    {
    static char * memTop = NULL;

    if (memTop == NULL)
        {
        memTop = sysPhysMemTop () - USER_RESERVED_MEM;

        if ((*(UINT32 *)(MULTIBOOT_BOOT_FLAG_ADRS) ==
                        (MULTIBOOT_BOOTLOADER_MAGIC)))
            {
            if (*(UINT32 *)MULTIBOOT_BOOTROM_SAVED_FLAG_ADRS == 0xbadbeef)
                {
                memRomSize = wrs_kernel_data_end - _wrs_kernel_text_start + 2;
                memRom = ROUND_DOWN(memTop - memRomSize, VM_PAGE_SIZE);
                *(UINT32 *)MULTIBOOT_BOOTROM_SIZE = memRomSize;
                *(UINT32 *)MULTIBOOT_BOOTROM_ADRS = memRom;

                memTop = (char *)ROUND_DOWN(memRom, VM_PAGE_SIZE);

                bcopy ((char *)(_wrs_kernel_text_start), (char *)memRom,
                    memRomSize - 2);

                *(UINT16 *)(memRom + memRomSize - 2) =
                            checksum ((UINT16 *)memRom, memRomSize -2);
                *(UINT32 *)MULTIBOOT_BOOTROM_SAVED_FLAG_ADRS = 0;
                }
            else
                {
                memTop = (char *)ROUND_DOWN(memTop -
                        *(UINT32 *)MULTIBOOT_BOOTROM_SIZE, VM_PAGE_SIZE);
                }
            }
        else
            {
#if defined(INCLUDE_FAST_REBOOT) && defined(INCLUDE_MULTI_STAGE_BOOT)
           /*
            * Subtract the size of the boot image from the top of memory and
            * record it here via memTop, so that we are globally aware how much
            * are left.
            */

            memRomSize = ROUND_UP((*(UINT32 *)BOOT_IMAGE_SIZE), VM_PAGE_SIZE);
            memTop = memTop - memRomSize;
            memRom = (UINT32) ROUND_DOWN(((UINT32)memTop), VM_PAGE_SIZE);
#endif /* !defined(INCLUDE_SYMMETRIC_IO_MODE) && */
       /* defined(INCLUDE_FAST_REBOOT)           */
             }

#ifdef INCLUDE_EDR_PM

        /* account for ED&R persistent memory */

        memTop = memTop - PM_RESERVED_MEM;
#endif /* INCLUDE_EDR_PM */

       /*
        * This is only for bootrom.
        * Limited bootrom memory may cause bootrom to not boot. The fix is to
        * increase memory space for the bootrom by changing SYS_RAM_LOW_ADRS.
        */
        if ((UINT32)(&end) < 0x100000)      /* this is for bootrom */
            memTop = (char *)EBDA_START;    /* preserve the MP table */
        else if ((UINT32)(&end) < SYS_RAM_LOW_ADRS) /* bootrom in upper mem */
            memTop = (char *)(SYS_RAM_LOW_ADRS & 0xfff00000);

        }

    return (memTop);
    }

/*******************************************************************************
*
* sysCheckSCH - Check if the IO hub is SCH (US15W)
*
* Scan the PCI bus to check if the LPC controller belongs to SCH. This routine
* can only be called after vxbus subsystem initial stage 1 finished, since it
* need the PCI Controller driver (pentiumPCI) support.
*
* RETURNS: TRUE if it is SCH, or FLASE if it is not.
*
* NOMANUAL
*/

LOCAL BOOL sysCheckSCH (void)
    {
    int pciBusNo, pciDevNo, pciFuncNo, pciClass;
    UINT16 value;

    /* try to find the LPC (PCI - ISA bridge), which should always be present */

    pciClass = (int)(PCI_CLASS_BRIDGE_CTLR << 16
                     | PCI_SUBCLASS_ISA_BRIDGE << 8);

    if (pciFindClass(pciClass, 0, &pciBusNo, &pciDevNo, &pciFuncNo) != OK)
        {
        /* No LPC controller found, we assume it is not SCH */

        return FALSE;
        }

    if (pciConfigInWord(pciBusNo, pciDevNo, pciFuncNo, 2, &value) != OK)
        {
        return FALSE;   /* PCI read failure */
        }

    /*
     * the SCH's LPC device no is 0x8119 according to datasheet, here we mask
     * off the lower 8 bit to avoid the possiblity that there's difference
     * between SCH variants, e.g. US15L/US15W/US15X.
     */

#define SCH_IDENTITY 0x8100

    if ((value & 0xFF00) == SCH_IDENTITY)
        {
        return TRUE;
        }

    return FALSE;
    }

/*******************************************************************************
*
* sysHardReset - Perform hard CPU reset
*
* Performs a hard CPU reset (cold boot).
*
* RETURNS: N/A
*
* NOMANUAL
*/

void sysHardReset(void)
    {
    sysOutByte(0xcf9, sysColdResetType);
    }

/*******************************************************************************
*
* sysMultiWarmBoot - grub load bootrom, bootrom and VIP warm reboot
*
* grub load vxWorks bootrom then bootrom load vxWorks, this is for bootrom or vxWorks warm reboot.
*
* RETURNS: warm reboot entry
*
* NOMANUAL
*/

LOCAL FUNCPTR sysMultiWarmBoot (void)
    {
    FUNCPTR pEntry = NULL;

    if (*(UINT32 *)(MULTIBOOT_BOOT_FLAG_ADRS) == MULTIBOOT_BOOTLOADER_MAGIC)
        {
        if (*(UINT16 *)(*(UINT32 *)MULTIBOOT_BOOTROM_ADRS +
                        *(UINT32 *)MULTIBOOT_BOOTROM_SIZE - 2) !=
                        checksum ((UINT16 *)(*(UINT32 *)MULTIBOOT_BOOTROM_ADRS),
                        *(UINT32 *)MULTIBOOT_BOOTROM_SIZE -2))
            {
            /* perform the cold boot since the warm boot is not possible */

            sysClkDisable ();

           /*
            * write ICH/SCH/PCH Reset Control Register to invoke
            * desired reset
            */

            sysHardReset ();
            }
        else
            {
            /* vxWorks image reboot to bootrom */

            if (abs((ULONG)&sysHwInit - SYS_RAM_HIGH_ADRS) >
                abs((ULONG)&sysHwInit - SYS_RAM_LOW_ADRS))
                {
                /* compressed bootrom */

                if (*(UINT32 *)MULTIBOOT_BOOT_LOAD_ADRS == SYS_RAM_LOW_ADRS)
                    {
                    bcopy ((char *)(*(UINT32 *)MULTIBOOT_BOOTROM_ADRS),
                           (char *)SYS_RAM_HIGH_ADRS,
                           (*(UINT32 *)MULTIBOOT_BOOTROM_SIZE) - 2);

                    pEntry = (FUNCPTR)(SYS_RAM_HIGH_ADRS);
                    }
                else
                    {
                    bcopy ((char *)(*(UINT32 *)MULTIBOOT_BOOTROM_ADRS),
                           (char *)(*(UINT32 *)MULTIBOOT_BOOT_LOAD_ADRS),
                           (*(UINT32 *)MULTIBOOT_BOOTROM_SIZE) - 2);

                    pEntry = (FUNCPTR)(*(UINT32 *)MULTIBOOT_BOOT_LOAD_ADRS +
                                                  ROM_WARM_HIGH);
                    }

                }
             else /* bootrom self reboot */
                 {
                 if ((UINT32)_wrs_kernel_text_start !=
                    *(UINT32 *)MULTIBOOT_BOOT_LOAD_ADRS)
                     {
                     /* compress bootrom vxImage reboot  */

                     bcopy ((char *)memRom, (char *)_wrs_kernel_text_start,
                         memRomSize - 2);
                     pEntry = (FUNCPTR)(_wrs_kernel_text_start);
                     }
                 else
                     {
                     /* uncompress bootrom vxImage reboot  */

                     bcopy ((char *)memRom, (char *)
                            (*(UINT32 *)MULTIBOOT_BOOT_LOAD_ADRS),
                            memRomSize - 2);

                     pEntry = (FUNCPTR)(*(UINT32 *)MULTIBOOT_BOOT_LOAD_ADRS +
                                                   ROM_WARM_HIGH);
                     }
                 }
             }
        }
     return pEntry;
    }

/*******************************************************************************
*
* sysToMonitor - transfer control to the ROM monitor
*
* This routine transfers control to the ROM monitor.  It is usually called
* only by reboot() -- which services ^X -- and by bus errors at interrupt
* level.  However, in some circumstances, the user may wish to introduce a
* new <startType> to enable special boot ROM facilities.
*
* RETURNS: Does not return.
*/

STATUS sysToMonitor
    (
    int startType   /* passed to ROM to tell it how to boot */
    )
    {
    FUNCPTR pEntry;
    FUNCPTR pEntryMulti;
    INT16 * pDst;

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)
    BOOL enable = FALSE;
#endif /* defined (INCLUDE_SYMMETRIC_IO_MODE) || */
       /* defined (INCLUDE_VIRTUAL_WIRE_MODE)    */

#if defined(_WRS_CONFIG_SMP)
    int cPu = vxCpuPhysIndexGet();
#endif /* defined(_WRS_CONFIG_SMP) */

#if ((!defined(INCLUDE_FAST_REBOOT)) &&                 \
     (!defined(INCLUDE_MULTI_STAGE_WARM_REBOOT)) &&     \
     defined(INCLUDE_MULTI_STAGE_BOOT))

    /*
     * The GRUB vxWorks image does not support "warm" reboot.
     * Do cold reboots for non-fast-reboot multi-stage boot.
     */

    startType = BOOT_COLD;
#endif

    if ((*(UINT32 *)(MULTIBOOT_BOOT_FLAG_ADRS) != MULTIBOOT_BOOTLOADER_MAGIC)&&
        (*(UINT32 *)(MULTIBOOT_SCRATCH) == MULTIBOOT_BOOTLOADER_MAGIC))

        startType = BOOT_COLD;

    sysConfStop ();  /* shutdown BOOTAPP dependent devices */

    intCpuLock();        /* LOCK INTERRUPTS */

#ifdef _WRS_CONFIG_SMP
    /*
     * At this point we are either the bootstrap processor,
     * or an application processor.
     * Need to reset APs and go out on the bootstrap processor.
     */

    if (vxCpuIndexGet() != 0)
        {
        /* CPCs use ipiId = 0 */

        vxIpiConnect (0,
                      (IPI_HANDLER_FUNC) (sysToMonitor),
                      (void *) startType);

        vxIpiEmit(0, 1);

        taskCpuLock();       /* if not in interrupt context, taskCpuLock */

        ipiShutdownSup();

       }

    sysCpuStop_APs ();   /* Stop APs*/
    sysUsDelay(50000);

    sysCpuReset_APs ();   /* place APs in reset */
    sysDelay();
#endif /* _WRS_CONFIG_SMP */

    /* now running on BSP, APs in reset... */

    mmuPro32Enable (FALSE);          /* disable MMU */

#if defined(INCLUDE_SYMMETRIC_IO_MODE)
#if (defined(_WRS_CONFIG_SMP))

    /* restore trampoline code space for warm reboot for phys CPU 0 only */

    if ( cPu == 0 )
        bcopy ((char *)(&tramp_code[0]), (char *)(CPU_ENTRY_POINT),
               (int)CPU_AP_AREA);

#endif /* (defined(_WRS_CONFIG_SMP) */
#endif  /* defined(INCLUDE_SYMMETRIC_IO_MODE) */

    /* If this is a cold reboot */

    if (startType == BOOT_COLD)
        goto sysToMonitorCold;

    pDst = (short *)SYS_RAM_HIGH_ADRS;  /* copy it in lower mem */
    pEntry = (FUNCPTR)(ROM_TEXT_ADRS + ROM_WARM_HIGH);

#if defined(INCLUDE_MULTI_STAGE_BOOT) && \
    (!(defined(INCLUDE_BOOT_APP))) && \
    (!(defined(INCLUDE_BOOT_RAM_IMAGE)))
    pEntry = (FUNCPTR)(FIRST_TEXT_ADRS + ROM_WARM_HIGH);
#else /* INCLUDE_MULTI_STAGE_BOOT */
    pEntry = (FUNCPTR)(ROM_TEXT_ADRS + ROM_WARM_HIGH);
#endif /* INCLUDE_MULTI_STAGE_BOOT */


#if defined(INCLUDE_FAST_REBOOT) && defined(INCLUDE_MULTI_STAGE_BOOT) && \
    (!defined(INCLUDE_BOOT_APP))

        /*
         * Make sure our stored checksum is still valid, else do a cold boot
         */

        if (*(UINT16 *)((ULONG)memRom + (*(UINT32 *)BOOT_IMAGE_SIZE) - 2) !=
                                checksum ((UINT16 *)((ULONG)memRom),
                                (*(UINT32 *)BOOT_IMAGE_SIZE) - 2))
            {
            *(UINT32 *)BOOT_IMAGE_ADRS = 0;
            *(UINT32 *)BOOT_IMAGE_SIZE = 0;
            goto sysToMonitorCold;
            }

        /*
         * If either the memRomSize is invalid or the location where it was
         * copied, we know it's not valid - do a cold reboot instead.
         */

        if (!((memRom > 0) && (memRomSize > 0)))
            goto sysToMonitorCold;

        /*
         * vxStage1Boot.s uses the stored values in BOOT_IMAGE_ADRS and
         * BOOT_IMAGE_SIZE in the function reBoot, to determine if the
         * bootrom image was copied and can be restored, before doing a
         * restore and then a jump to warm boot.
         * In sysLib.c, function sysPhysMemTop, we fill in BOOT_IMAGE_ADRS
         * when the copy takes place, and the BOOT_IMAGE_SIZE gets filled
         * in by vxStage1Boot.s function bootProtected2, just before jumping
         * to stage 2.
         */

        goto sysToMonitorWarm;

#endif  /* defined(INCLUDE_FAST_REBOOT) && defined(INCLUDE_MULTI_STAGE_BOOT) */

    /* If here because of fatal error during an interrupt just reboot */

#ifdef _WRS_CONFIG_SMP
    if(INT_CONTEXT())
#else
    if(intCnt > 0)
#endif /* _WRS_CONFIG_SMP */
        {
        pEntry = (FUNCPTR)(ROM_TEXT_ADRS + ROM_WARM_HIGH);

        sysClkDisable ();   /* disable the system clock interrupt */
        sysIntLock ();      /* lock the used/owned interrupts */

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)

        /* Shutdown Local APIC */

        (*getLoApicIntrEnable)(loApicDevID, (void *) &enable);
#endif  /* defined (INCLUDE_SYMMETRIC_IO_MODE) ||
           defined (INCLUDE_VIRTUAL_WIRE_MODE) */

#if defined (INCLUDE_SYMMETRIC_IO_MODE)

        /* Restore Local APIC initial settings prior to enable */

        *(volatile int *)(LOAPIC_BASE + 0x0f0) = /* LOAPIC_SVR */
            (int)glbLoApicOldSvr;
        *(volatile int *)(LOAPIC_BASE + 0x350) = /* LOAPIC_LINT0 */
            (int)glbLoApicOldLint0;
        *(volatile int *)(LOAPIC_BASE + 0x360) = /* LOAPIC_LINT1 */
            (int)glbLoApicOldLint1;
#endif  /*  defined (INCLUDE_SYMMETRIC_IO_MODE) */

         pEntryMulti = sysMultiWarmBoot();
         if (pEntryMulti != NULL)
             pEntry = pEntryMulti;

        (*pEntry) (startType);

        /* Oops, This should not happen - Reset */

        goto sysToMonitorCold;
        }

    /* jump to the warm start entry point */

#if (defined(INCLUDE_FAST_REBOOT) && defined(INCLUDE_MULTI_STAGE_BOOT) && \
    (!defined(INCLUDE_BOOT_APP)))
sysToMonitorWarm:
#endif /* defined(INCLUDE_FAST_REBOOT) && defined(INCLUDE_MULTI_STAGE_BOOT) */

    sysClkDisable ();   /* disable the system clock interrupt */
    sysIntLock ();      /* lock the used/owned interrupts */

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)

    /* Shutdown Local APIC */

    (*getLoApicIntrEnable)(loApicDevID, (void *) &enable);
#endif  /* defined (INCLUDE_SYMMETRIC_IO_MODE) ||
           defined (INCLUDE_VIRTUAL_WIRE_MODE) */

#if defined (INCLUDE_SYMMETRIC_IO_MODE)

    /* Restore Local APIC initial settings prior to enable */

    *(volatile int *)(LOAPIC_BASE + 0x0f0) = /* LOAPIC_SVR */
        (int)glbLoApicOldSvr;
    *(volatile int *)(LOAPIC_BASE + 0x350) = /* LOAPIC_LINT0 */
        (int)glbLoApicOldLint0;
    *(volatile int *)(LOAPIC_BASE + 0x360) = /* LOAPIC_LINT1 */
        (int)glbLoApicOldLint1;
#endif /*  defined (INCLUDE_SYMMETRIC_IO_MODE) */

    pEntryMulti = sysMultiWarmBoot();
    if (pEntryMulti != NULL)
        pEntry = pEntryMulti;

    (*pEntry) (startType);

sysToMonitorCold:

    /* perform the cold boot since the warm boot is not possible */

    sysClkDisable ();

    /* write ICH/SCH/PCH Reset Control Register to invoke desired reset */

    sysHardReset ();

    return (OK);    /* in case we ever continue from ROM monitor */

}

/*******************************************************************************
*
* sysIntInitPIC - initialize the interrupt controller
*
* This routine initializes the interrupt controller.
* Maps APIC Memory space.
*
* RETURNS: N/A
*
*/

void sysIntInitPIC (void)
    {
#if defined(INCLUDE_VIRTUAL_WIRE_MODE)
    {
    /* Map Local APIC Address space... */

    UINT32 mpApicloBase;
    UINT32 lengthLo;    /* length of Local APIC registers */

    i8259Init ();

    /* add an entry to the sysMmuPhysDesc[] for Local APIC */

    (*getMpApicloBaseGet) (mpApicDevID, &mpApicloBase);
    lengthLo = ((UINT32)LOAPIC_LENGTH / VM_PAGE_SIZE) * VM_PAGE_SIZE;

    sysMmuMapAdd ((void *)mpApicloBase, lengthLo,
                  VM_STATE_MASK_FOR_ALL, VM_STATE_FOR_IO);
    }
#elif defined(INCLUDE_SYMMETRIC_IO_MODE)
    {
    /* Map Local and IO APIC Address spaces... */

    UINT32 lengthLo;    /* length of Local APIC registers */
    UINT32 lengthIo;    /* length of IO APIC registers */

    int    i;

    UINT32 mpApicloBase;
    UINT32 mpApicNioApic;

    UINT32 *mpApicAddrTable;

    /*
     * add an entry to the sysMmuPhysDesc[] for Local APIC and IO APIC
     * only do this once...
     */

    (*getMpApicloBaseGet) (mpApicDevID, &mpApicloBase);
    lengthLo = ((UINT32)LOAPIC_LENGTH / VM_PAGE_SIZE) * VM_PAGE_SIZE;

    sysMmuMapAdd ((void *)mpApicloBase, lengthLo,
                  VM_STATE_MASK_FOR_ALL, VM_STATE_FOR_IO);

    (*getMpApicNioApicGet) (mpApicDevID, &mpApicNioApic);
    (*getMpApicAddrTableGet)(mpApicDevID, (void *)&mpApicAddrTable);

    lengthIo = ((UINT32)IOAPIC_LENGTH / VM_PAGE_SIZE) * VM_PAGE_SIZE;

    for(i=0; i<mpApicNioApic; i++)
        {
        sysMmuMapAdd ((void *)(mpApicAddrTable[i]), lengthIo,
                      VM_STATE_MASK_FOR_ALL, VM_STATE_FOR_IO);
        }
    }

    i8259Init ();

#else   /* PIC MODE */
    i8259Init ();
#endif  /* defined(INCLUDE_VIRTUAL_WIRE_MODE) */

    }

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)
/*******************************************************************************
*
* apicIntrIntHelper - initialize pointers to vxBus driver routines.
*
* This routine initializes pointers to vxBus driver routines up front, rather
* than calling into vxBus API...
*
* Called from "sysPciPirqEnable" because we require intEnable/Disable early on
* during PCI initialization...
*
* RETURNS: STATUS
*
*/

STATUS apicIntrIntHelper
    (
    VXB_DEVICE_ID pDev,
    void * unused
    )
    {
    FUNCPTR func;

    /* check for MP APIC */

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(mpApicLoIndexTableGet));
    if ( func != NULL )
        {
        mpApicDevID = pDev;
        getMpApicLoIndexTable = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(mpApicloBaseGet));
    if ( func != NULL )
        {
        mpApicDevID = pDev;
        getMpApicloBaseGet = func;
        }

#if defined(INCLUDE_SYMMETRIC_IO_MODE)

    /* check for IO APIC */

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(mpApicNioApicGet));
    if ( func != NULL )
        {
        mpApicDevID = pDev;
        getMpApicNioApicGet = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(mpApicAddrTableGet));
    if ( func != NULL )
        {
        mpApicDevID = pDev;
        getMpApicAddrTableGet = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(ioApicRedNumEntriesGet));
    if ( func != NULL )
        {
        ioApicDevID = pDev;
        getIoApicRedNumEntriesGet = func;
        }

    getIoApicIntrIntEnable = NULL;
    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(ioApicIntrIntEnable));
    if ( func != NULL )
        {
        ioApicDevID = pDev;
        getIoApicIntrIntEnable = func;
        }

    getIoApicIntrIntDisable = NULL;
    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(ioApicIntrIntDisable));
    if ( func != NULL )
        {
        ioApicDevID = pDev;
        getIoApicIntrIntDisable = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(ioApicIntrIntLock));
    if ( func != NULL )
        {
        ioApicDevID = pDev;
        getIoApicIntrIntLock = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(ioApicIntrIntUnlock));
    if ( func != NULL )
        {
        ioApicDevID = pDev;
        getIoApicIntrIntUnlock = func;
        }
#endif  /* defined(INCLUDE_SYMMETRIC_IO_MODE) */

    /* check for Local APIC */

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(loApicIntrInitAP));
    if ( func != NULL )
        {
        loApicDevID = pDev;
        getLoApicIntrInitAP = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(loApicIntrEnable));
    if ( func != NULL )
        {
        loApicDevID = pDev;
        getLoApicIntrEnable = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(loApicIntrIntLock));
    if ( func != NULL )
        {
        loApicDevID = pDev;
        getLoApicIntrIntLock = func;
        }

    func = vxbDevMethodGet(pDev, DEVMETHOD_CALL(loApicIntrIntUnlock));
    if ( func != NULL )
        {
        loApicDevID = pDev;
        getLoApicIntrIntUnlock = func;
        }

    return(OK);
    }
#else

/*******************************************************************************
*
* dummyGetCpuIndex - dummy up the UP implementation of vxCpuIndexGet
*
* Arch or BSP system-dependent implementation routine,
* used to dummy up the UP implementation of vxCpuIndexGet.
* Under UP always 0.
*
* RETURNS: 0
*
* \NOMANUAL
*/

unsigned int dummyGetCpuIndex (void)
    {
    return (0);
    }
#endif /* defined (INCLUDE_SYMMETRIC_IO_MODE) ||
          defined (INCLUDE_VIRTUAL_WIRE_MODE) */

/*******************************************************************************
*
* sysIntLock - lock out all interrupts
*
* This routine saves the mask and locks out all interrupts.
*
* SEE ALSO: sysIntUnlock()
*
*/

VOID sysIntLock (void)
    {
    INT32 oldLevel = intCpuLock();  /* LOCK INTERRUPTS */

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)
    int dummy = 0xdead;
#endif  /* INCLUDE_SYMMETRIC_IO_MODE || INCLUDE_VIRTUAL_WIRE_MODE */

#if defined(INCLUDE_VIRTUAL_WIRE_MODE)
    (*getLoApicIntrIntLock)(loApicDevID, (void *) &dummy);
    i8259IntLock ();
#elif defined(INCLUDE_SYMMETRIC_IO_MODE)
    (*getLoApicIntrIntLock)(loApicDevID, (void *) &dummy);
    (*getIoApicIntrIntLock)(ioApicDevID, (void *) &dummy);
#else
    i8259IntLock ();
#endif  /* defined(INCLUDE_VIRTUAL_WIRE_MODE) */

    intCpuUnlock(oldLevel);         /* UNLOCK INTERRUPTS */
}

/*******************************************************************************
*
* sysIntUnlock - unlock the PIC interrupts
*
* This routine restores the mask and unlocks the PIC interrupts
*
* SEE ALSO: sysIntLock()
*
*/

VOID sysIntUnlock (void)
    {
    INT32 oldLevel = intCpuLock();  /* LOCK INTERRUPTS */

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)
    int dummy = 0xdead;
#endif  /* INCLUDE_SYMMETRIC_IO_MODE || INCLUDE_VIRTUAL_WIRE_MODE */

#if defined(INCLUDE_VIRTUAL_WIRE_MODE)
    (*getLoApicIntrIntUnlock)(loApicDevID, (void *) &dummy);
    i8259IntUnlock ();
#elif defined(INCLUDE_SYMMETRIC_IO_MODE)
    (*getLoApicIntrIntUnlock)(loApicDevID, (void *) &dummy);
    (*getIoApicIntrIntUnlock)(ioApicDevID, (void *) &dummy);
#else
    i8259IntUnlock ();
#endif  /* defined(INCLUDE_VIRTUAL_WIRE_MODE) */

    intCpuUnlock(oldLevel);         /* UNLOCK INTERRUPTS */
    }

/*******************************************************************************
*
* sysIntDisablePIC - disable a bus interrupt level
*
* This routine disables a specified bus interrupt level.
*
* RETURNS: OK, or ERROR if failed.
*
*/

STATUS sysIntDisablePIC
    (
    int irqNo       /* IRQ(PIC) or INTIN(APIC) number to disable */
    )
    {
#if defined(INCLUDE_SYMMETRIC_IO_MODE)
    if (getIoApicIntrIntDisable != NULL)
        return ((*getIoApicIntrIntDisable)(ioApicDevID, (void *)&irqNo));
    else
        return (vxbDevMethodRun(DEVMETHOD_CALL(ioApicIntrIntDisable),
               (void *)&irqNo));
#else
    return (i8259IntDisable (irqNo));
#endif  /* defined(INCLUDE_SYMMETRIC_IO_MODE) */
    }

/*******************************************************************************
*
* sysIntEnablePIC - enable a bus interrupt level
*
* This routine enables a specified bus interrupt level.
*
* RETURNS: OK, or ERROR if failed.
*
*/

STATUS sysIntEnablePIC
    (
    int irqNo       /* IRQ(PIC) or INTIN(APIC) number to enable */
    )
    {
#if defined(INCLUDE_SYMMETRIC_IO_MODE)
    if (getIoApicIntrIntEnable != NULL)
        return ((*getIoApicIntrIntEnable)(ioApicDevID, (void *)&irqNo));
    else
        return (vxbDevMethodRun(DEVMETHOD_CALL(ioApicIntrIntEnable),
               (void *)&irqNo));
#else
    return (i8259IntEnable (irqNo));
#endif  /* defined(INCLUDE_SYMMETRIC_IO_MODE) */
    }

#ifdef _WRS_VX_IA_IPI_INSTRUMENTATION
extern struct ipiCounters *ipiCounters[VX_MAX_SMP_CPUS];
#endif /* _WRS_VX_IA_IPI_INSTRUMENTATION */

#if defined (INCLUDE_SYMMETRIC_IO_MODE) || defined (INCLUDE_VIRTUAL_WIRE_MODE)
/*******************************************************************************
*
* sysLoApicIntEoi -  send EOI (End Of Interrupt) signal to Local APIC
*
* This routine sends an EOI signal to the Local APIC's interrupting source.
*
* RETURNS: N/A
*/

void sysLoApicIntEoi
    (
    INT32 irqNo         /* INIIN number to send EOI */
    )
    {
    *(int *)(LOAPIC_BASE + LOAPIC_EOI) = 0;
    }

#ifdef _WRS_VX_IA_IPI_INSTRUMENTATION

/*******************************************************************************
* sysLoApicIpiBoi -  Called before the interrupt service routine to increment
*                    IPI instrumentation interrupt count
*
* This routine is called before the interrupt service routine to increment
* IPI instrumentation interrupt count.
*
* RETURNS: N/A
*/

void sysLoApicIpiBoi
    (
    INT32 irqNo
    )
    {
    int ipiNum =  irqNo - INT_NUM_LOAPIC_IPI;
    struct ipiCounters *ipiCnt = ipiCounters[vxCpuPhysIndexGet()];

    if (ipiNum >= 0 && ipiNum < 7)
        vxAtomicInc((atomic_t *)(&(ipiCnt->ipiRecv[ipiNum])));
    }
#endif /* _WRS_VX_IA_IPI_INSTRUMENTATION */
#endif  /* INCLUDE_SYMMETRIC_IO_MODE || INCLUDE_VIRTUAL_WIRE_MODE */

/*******************************************************************************
*
* sysIntEoiGet - get EOI/BOI function and its parameter
*
* This routine gets EOI function and its parameter for the interrupt controller.
* If returned EOI/BOI function is NULL, intHandlerCreateI86() replaces
* "call _routineBoi/Eoi" in intConnectCode[] with NOP instruction.
*
* RETURNS: N/A
*
*/

void sysIntEoiGet
    (
    VOIDFUNCPTR * vector,           /* interrupt vector to attach to */
    VOIDFUNCPTR * routineBoi,       /* BOI function */
    _Vx_usr_arg_t * parameterBoi,   /* a parameter of the BOI function */
    VOIDFUNCPTR * routineEoi,       /* EOI function */
    _Vx_usr_arg_t * parameterEoi    /* a parameter of the EOI function */
    )
    {
    int intNum = IVEC_TO_INUM (vector);
    int irqNo;

    /* set default BOI routine & parameter */

    *routineBoi   = NULL;
    *parameterBoi = 0;

    /* find a match in sysInumTbl[] */

    for (irqNo = 0; irqNo < sysInumTblNumEnt; irqNo++)
        {
        if (sysInumTbl[irqNo] == intNum)
            break;
        }

    *parameterEoi = irqNo;          /* irq is sysInumTblNumEnt, if no match */

#if defined (INCLUDE_SYMMETRIC_IO_MODE)
    if (*parameterEoi == sysInumTblNumEnt)
        *parameterEoi = intNum;  /* irq is requested intNum, if no match */
    if (intNum == INT_NUM_LOAPIC_SPURIOUS)
        *routineEoi = NULL;         /* no EOI is necessary */
    else
        *routineEoi = sysLoApicIntEoi; /* set Local APIC's EOI routine */
#ifdef _WRS_VX_IA_IPI_INSTRUMENTATION
    /*
     * Add Ipi Counter BOI
     */

    if (intNum >= INT_NUM_LOAPIC_IPI &&
        intNum < (INT_NUM_LOAPIC_IPI + IPI_MAX_HANDLERS))
        {
        *routineBoi = sysLoApicIpiBoi;
        *parameterBoi = intNum;
        }
#endif /* _WRS_VX_IA_IPI_INSTRUMENTATION */
#else

#if defined (INCLUDE_VIRTUAL_WIRE_MODE)

    if (irqNo >= N_PIC_IRQS)        /* IRQ belongs to the Local APIC */
        {
        if (intNum == INT_NUM_LOAPIC_SPURIOUS)
            *routineEoi = NULL;     /* no EOI is necessary */
        else
            *routineEoi = sysLoApicIntEoi; /* set Local APIC's EOI routine */

    return;
    }

#endif  /* INCLUDE_VIRTUAL_WIRE_MODE */

    /* set the [BE]OI parameter for the master & slave PIC */

    *parameterBoi = irqNo;
    *parameterEoi = irqNo;

    /* set the right BOI routine */

    if (irqNo == 0)                 /* IRQ0 BOI routine */
        {
#if (PIC_IRQ0_MODE == PIC_AUTO_EOI)
        *routineBoi   = NULL;
#elif   (PIC_IRQ0_MODE == PIC_EARLY_EOI_IRQ0)
        *routineBoi   = i8259IntBoiEem;
#elif   (PIC_IRQ0_MODE == PIC_SPECIAL_MASK_MODE_IRQ0)
        *routineBoi   = i8259IntBoiSmm;
#else
        *routineBoi   = NULL;
#endif  /* (PIC_IRQ0_MODE == PIC_AUTO_EOI) */
        }
    else if ((irqNo == PIC_MASTER_STRAY_INT_LVL) ||
             (irqNo == PIC_SLAVE_STRAY_INT_LVL))
        {
        *routineBoi   = i8259IntBoi;
        }

    /* set the right EOI routine */

    if (irqNo == 0)                 /* IRQ0 EOI routine */
        {
#if (PIC_IRQ0_MODE == PIC_AUTO_EOI) || \
    (PIC_IRQ0_MODE == PIC_EARLY_EOI_IRQ0)
        *routineEoi   = NULL;
#elif   (PIC_IRQ0_MODE == PIC_SPECIAL_MASK_MODE_IRQ0)
        *routineEoi   = i8259IntEoiSmm;
#else
        *routineEoi   = i8259IntEoiMaster;
#endif  /* (PIC_IRQ0_MODE == PIC_AUTO_EOI) || (PIC_EARLY_EOI_IRQ0) */
        }
    else if (irqNo < 8)             /* IRQ[1-7] EOI routine */
        {
#if (PIC_IRQ0_MODE == PIC_AUTO_EOI)
        *routineEoi   = NULL;
#else
        *routineEoi   = i8259IntEoiMaster;
#endif  /* (PIC_IRQ0_MODE == PIC_AUTO_EOI) */
        }
    else                            /* IRQ[8-15] EOI routine */
        {
#if defined (PIC_SPECIAL_FULLY_NESTED_MODE)
        *routineEoi   = i8259IntEoiSlaveSfnm;
#else
        *routineEoi   = i8259IntEoiSlaveNfnm;
#endif  /* defined (PIC_SPECIAL_FULLY_NESTED_MODE) */
        }

#endif  /* INCLUDE_SYMMETRIC_IO_MODE */
    }

/*******************************************************************************
*
* sysIntLevel - get an IRQ(PIC) or INTIN(APIC) number in service
*
* This routine gets an IRQ(PIC) or INTIN(APIC) number in service.
* We assume the following:
*   - this function is called in intEnt()
*   - IRQ number of the interrupt is at intConnectCode [29]
*
* RETURNS: 0 - (sysInumTblNumEnt - 1), or sysInumTblNumEnt if we failed to get
*          it.
*
*/

int sysIntLevel
    (
    int arg     /* parameter to get the stack pointer */
    )
    {
    UINT32 * pStack;
    UCHAR * pInst;
    INT32 ix;
    INT32 irqNo = sysInumTblNumEnt; /* return sysInumTblNumEnt if we failed */

    pStack = (UINT32 *)&arg;        /* get the stack pointer */
    pStack += 3;                    /* skip pushed volatile registers */

    /*
     * we are looking for a return address on the stack which point
     * to the next instruction of "call _intEnt" in the malloced stub.
     * Then get the irqNo at intConnectCode [29].
     */

    for (ix = 0; ix < 10; ix++, pStack++)
        {
        pInst = (UCHAR *)*pStack;   /* return address */
        if ((*pInst == 0x50) &&     /* intConnectCode [5] */
            ((*(int *)(pInst - 4) + (int)pInst) == (int)intEnt))
            {
            irqNo = *(int *)(pInst + 24);   /* intConnectCode [29] */
            break;
            }
        }

    return (irqNo);
}

/****************************************************************************
*
* sysProcNumGet - get the processor number
*
* This routine returns the processor number for the CPU board, which is
* set with sysProcNumSet().
*
* RETURNS: The processor number for the CPU board.
*
* SEE ALSO: sysProcNumSet()
*/

int sysProcNumGet (void)
    {
    return (sysProcNum);
    }

/****************************************************************************
*
* sysProcNumSet - set the processor number
*
* Set the processor number for the CPU board.  Processor numbers should be
* unique on a single backplane.
*
* NOTE: By convention, only Processor 0 should dual-port its memory.
*
* RETURNS: N/A
*
* SEE ALSO: sysProcNumGet()
*/

void sysProcNumSet
    (
    int procNum     /* processor number */
    )
    {
    sysProcNum = procNum;
    }

/****************************************************************************
*
* sysCpuIdCalcFreq - get frequency from CPUID
*
* This routine will extract brandstring from CPUID structure, and
* calculate the CPU frequency in UINT64.
*
* RETURNS: N/A
*
* SEE ALSO: sysCalibrateTSC()
*/

UINT64 sysCpuIdCalcFreq (void)
    {
    char   intBuf[5] = {0};
    char   minBuf[5] = {0};
    CPUID *tmpCpuId = &sysCpuId;
    char*  pStr;
    char*  pFreqInt;
    char*  pFreqMin;
    UINT64  tmpFreq;
    UINT64  fixer = 0;       /* make it more precise */
    UINT64  freq;
    int matchingItems = 0;

    /* get frequency string from CPUID */

    pFreqInt = (char*)&intBuf[0];
    pFreqMin = (char*)&minBuf[0];
    pStr = (char*)&(tmpCpuId->brandString[0]);

    /* using pattern matching first */

    matchingItems = sscanf(pStr, "%*[^@]@ %[^.].%[^G]", pFreqInt, pFreqMin);

    /*
     * matching failed, withdraw to lookup manually. In this mode, the
     * integer part can only be 1-9, so if the CPU frequency is over 10 GHz
     * this function should be updated. However, this case (matching failed)
     * was only observed on Tolapai.
     */

    if (*pFreqInt == 0 || *pFreqMin == 0 || (matchingItems != 2))
        {
        /* Go until the radix point */

        while (*pStr != '.' && *pStr != 0) pStr++;
        *pFreqInt++ = *(pStr-1);
        *pFreqInt = 0;

        /* jump over radix point, go until the GHz */

        pStr++;
        while (*pStr != 'G' && *pStr != 0)
            {
            *pFreqMin++ = *pStr++;
            }
        *pFreqMin = 0;
        }

    /*
     * get frequency in Hz, note: can not use floating compute, or it will
     * cause exception at initial stage.
     */

    tmpFreq = atoi (intBuf);
    freq = tmpFreq * 1000000000; /* GHz */
    tmpFreq = atoi (minBuf);

    /*
     * actually the frequency is multiplied by bus frequency, fix it:
     * the theory here is that the bus frequency is multiple of 33Mhz, or,
     * 100,000,000/3 Hz. for example, 100Mhz = 33Mhz * 3, 133 = 33Mhz * 4,
     * 167Mhz = 33Mhz * 5, etc. And, for any number divided by 3, the fractional
     * part can only be in two case, 1/3, or 2/3.
     */

    if (tmpFreq % 10 == 3)
        {
        fixer = 3333333; /* 10000000 / 3 */
        }
    else if (tmpFreq % 10 == 6 )
        {
        /* 6 for round down */

        fixer = 6666667; /* 20000000 / 3 */
        }
    else if (tmpFreq % 10 == 7 )
        {
        /* 7 for round up */

        fixer = 6666667;
        tmpFreq = tmpFreq - 1;
        }
    freq = freq + tmpFreq * 10000000 + fixer;

    return (freq);
    }

void print_one_hex(UINT32 val)
{
	char out_hex[2];
	out_hex [1] = 0;
	val &= 0xF;
	if (val < 0xA) out_hex[0]= val + '0';
	else out_hex[0] = val - 0xA + 'A';
	earlyPrintVga(out_hex);
}
void print_hex(UINT32 val)
{
	print_one_hex(val >> 28);
	print_one_hex(val >> 24);
	print_one_hex(val >> 20);
	print_one_hex(val >> 16);
	print_one_hex(val >> 12);
	print_one_hex(val >> 8);
	print_one_hex(val >> 4);
	print_one_hex(val);
	 earlyPrintVga(" ");
}
UINT64 sysCalcAMDFreq(void)
{
 UINT64 coreCof, cpuFid, cpuDid;
	 
	 /*
	  * Assumption is that CPU is in P0-state
	  * 
	  * CPU core freq computation is :
	  * CoreCOF = 100 * (MSRC001_00[6B:64][CpuFid] + 10h) / (2^MSRC001_00[6B:64][CpuDid]).
	  * 
	  */
	UINT32 twopow[]={1,2,4,8,16};
	 
	 pentiumMsrGet(0xC0010064, &coreCof);
	 
	 cpuFid= coreCof & 0x2F;
	 cpuDid= ( coreCof & 0x01C0 ) >> 6;
	 coreCof = 100 * ((cpuFid + 0x10) / twopow[cpuDid]);
	 
	 return(coreCof * 1000000);
}
/****************************************************************************
*
* sysCalibrateTSC - Calibrate the TSC use PIT and CPUID
*
* This routine will calibrate TSC and save the value to calibratedTSC
*
* RETURNS: N/A
*
* SEE ALSO: sysCpuIdCalcFreq()
*/

UINT64 sysCalibrateTSC (void)
    {
    UINT64 pitCalTsc;
    UINT64 cpuIdTsc;
    UINT64 diff;
    UINT32 duration = TSC_CALIBRATE_DURATION;   /* in milliseconds */

    /* get cpu max qualified frequency from brandstring */
#if 0
    cpuIdTsc = sysCpuIdCalcFreq();
#else
    cpuIdTsc = sysCalcAMDFreq();
#endif    
    /*
     * use PIT to calibrate the TSC frequency.
     * In initial stage it should be safe to not call
     * intLock/intUnlock to protect this code.
     */

    pitCalTsc = sysCalTSC (duration);
    pitCalTsc = (pitCalTsc * 1000) / duration;  /* extend to Hz */

    if (cpuIdTsc > pitCalTsc)
        {
        diff = cpuIdTsc - pitCalTsc;
        }
    else
        {
        diff = pitCalTsc - cpuIdTsc;
        }
    if(cpuIdTsc != 0)
        diff = (diff * 100) / cpuIdTsc;

    /*
     * If the difference between CPUID frequency and PIT calibrated TSC
     * frequency more than 50%, then the BIOS settings may be wrong, or
     * broken BIOS.
     */

    if (diff > 50)
        {
        /* Overclocking detected */
        }

    calibratedTSC = pitCalTsc;
    return pitCalTsc;
    }

/****************************************************************************
*
* sysGetTSCCountPerSec - get the calibrated TSC frequency (Hz)
*
* This routine will return the calibrated TSC frequency in Hz.
*
* RETURNS: N/A
*
* SEE ALSO: sysCalibrateTSC()
*/

UINT64 sysGetTSCCountPerSec (void)
    {
    return (calibratedTSC);
    }

/*******************************************************************************
*
* sysDelay - allow recovery time for port accesses
*
* This routine provides a brief delay used between accesses to the same serial
* port chip.
*
* RETURNS: N/A
*/

void sysDelay
    (
    void
    )
    {
    char ix;

    ix = sysInByte (UNUSED_ISA_IO_ADDRESS);
    }

/*******************************************************************************
*
* sysUsDelay - delay specified number of microseconds
*
* This routine provides a delay, based on TSC.
*
* RETURNS: N/A
*/

void sysUsDelay
    (
    int uSec
    )
    {
    UINT64  startTick;
    UINT64  endTick;
    UINT64  delayTick;
    UINT64  tscFreq;
#if defined( _WRS_CONFIG_SMP) && !defined (TSC_SYNC_CORES)
    TASK_ID myId;
#endif /*defined( _WRS_CONFIG_SMP) && !defined (TSC_SYNC_CORES)*/

    tscFreq = calibratedTSC;

    /* Calculate delay tick, tscFreq is in Hz */

    delayTick = (UINT64)uSec * tscFreq / 1000000;

    /*
     * Busy waiting, should happen in single thread. Also ensure in SMP mode,
     * reading TSC will not be migrated to other CPU.
     */

#if defined(_WRS_CONFIG_SMP) && !defined (TSC_SYNC_CORES)
    myId = taskIdSelf();

    /*
     * If taskId is invalid, then we are now either in ISR context or too early
     * before the task get ready. In both case we should not call taskCpuLock
     * taskCpuUnlock()
     */

    if (myId != (TASK_ID)NULL && myId != (TASK_ID)ERROR)
        {
        taskCpuLock();
        }
#endif /*defined( _WRS_CONFIG_SMP) && !defined (TSC_SYNC_CORES)*/

    pentiumTscGet64((INT64*)(&startTick));

    endTick = startTick +1;
    while (endTick-startTick < delayTick)
        {
        _WRS_ASM ("pause");
        pentiumTscGet64((INT64*) (&endTick));
        }

#if defined(_WRS_CONFIG_SMP) && !defined (TSC_SYNC_CORES)
    if (myId != (TASK_ID)NULL && myId != (TASK_ID)ERROR)
        taskCpuUnlock();
#endif /*defined( _WRS_CONFIG_SMP) && !defined (TSC_SYNC_CORES)*/
    }

/*******************************************************************************
*
* sys1msDelay - Delay 1 ms
*
* _vxb_delayRtn needs to point to a routine that takes exactly 1ms
* NOTE: _vxb_delayRtn must be set, else the vxbTimerLib.c will overwrite
* vxbUsDelay and vxbMsDelay function pointers and replace them with the
* software default pointers.
*
* RETURNS: N/A
*/

void sys1msDelay (void)
    {
    sysUsDelay(1000);
    }

/*******************************************************************************
*
* sysMsDelay - delay specified number of milliseconds
*
* This routine provides a delay, based on calibrated sysDelay() calls.
*
* RETURNS: N/A
*/

void sysMsDelay
    (
    int mSec
    )
    {
    sysUsDelay (1000 * mSec);
    }

/*******************************************************************************
*
* sysHpetOpen - map and initialize the HPET hardware interface.
*
* This function supplied to vxbIntelTimestamp driver that make the driver being
* able to use the HPET timer hardware in its first level initialization.
*
* RETURNS: OK or ERROR depending on supported of HPET.
*/

STATUS sysHpetOpen(void)
    {
#if defined (INCLUDE_HPET_TIMESTAMP)
    return OK;
#else  /* defined (INCLUDE_HPET_TIMESTAMP) */
    return ERROR;
#endif /* defined (INCLUDE_HPET_TIMESTAMP) */
    }

/*******************************************************************************
*
* sysStrayInt - Do nothing for stray interrupts.
*
* Do nothing for stray interrupts.
*/

void sysStrayInt (void)
    {
    sysStrayIntCount++;
    }

/*******************************************************************************
*
* sysMmuMapAdd - insert a new MMU mapping
*
* This routine will create a new <sysPhysMemDesc> table entry for a memory
* region of specified <length> in bytes and with a specified base
* <address>.  The <initialStateMask> and <initialState> parameters specify
* a PHYS_MEM_DESC type state mask and state for the memory region.
*
* CAVEATS
* This routine must be used before the <sysPhysMemDesc> table is
* referenced for the purpose of initializing the MMU or processor address
* space (us. in usrMmuInit()).
*
* The <length> in bytes will be rounded up to a multiple of VM_PAGE_SIZE
* bytes if necessary.
*
* The current implementation assumes a one-to-one mapping of physical to
* virtual addresses.
*
* If the current memory region contains some pages which had been mapped
* with different attributes previously, this routine will do nothing and
* return an ERROR directly.
*
* RETURNS: OK or ERROR depending on availability of free mappings.
*
* SEE ALSO: vmLib
*/

STATUS sysMmuMapAdd
    (
    void * address,           /* memory region base address */
    UINT   length,            /* memory region length in bytes*/
    UINT   initialStateMask,  /* PHYS_MEM_DESC state mask */
    UINT   initialState       /* PHYS_MEM_DESC state */
    )
    {
    PHYS_MEM_DESC * pMmu;
    PHYS_MEM_DESC * pPmd;

    UINT32   pageAddress = 0;
    UINT32   pageLength = 0;
    UINT32   pageAddressTop = 0;
    UINT32   pageLengthTop = 0;
    UINT32   pageAddressBottom = 0;
    UINT32   pageLengthBottom = 0;

    UINT32   pageStart;
    UINT32   pageEnd;
    UINT32   pageCount;

    UINT32   mapStart;
    UINT32   mapEnd;

    BOOL     pageNotMapped = FALSE;
    BOOL     noOverlap = FALSE;
    BOOL     topOverlap = FALSE;
    BOOL     bottomOverlap = FALSE;

    UINT32   i;

    STATUS   result = OK;

    mapStart = 0;
    mapEnd   = 0;

    /* Calculate(align) the start/end of page address, and the count of pages */

    pageLength = ROUND_UP (length, VM_PAGE_SIZE);

    pageStart = ROUND_DOWN (address, VM_PAGE_SIZE);
    pageEnd   = ((pageStart + (pageLength - 1)) / VM_PAGE_SIZE) * VM_PAGE_SIZE;
    pageCount = (pageEnd - pageStart) / VM_PAGE_SIZE + 1;

    pageAddress  = pageStart;

    for (i=0; i<sysPhysMemDescNumEnt; i++)
        {
        pageNotMapped = TRUE;
        noOverlap = TRUE;
        topOverlap = FALSE;
        bottomOverlap = FALSE;

        pPmd = &sysPhysMemDesc[i];

        mapStart = (UINT32) pPmd->virtualAddr;
        mapEnd   = (UINT32) pPmd->virtualAddr + pPmd->len - VM_PAGE_SIZE;

        if(pageStart >= mapStart && pageEnd <= mapEnd)
            {
            /* Mapping fully contained */

            if (pPmd->initialStateMask != initialStateMask  ||
                pPmd->initialState != initialState  ||
                pPmd->virtualAddr != pPmd->physicalAddr)
                {
                result = ERROR; /* Attributes are different */
                break;
                }

            noOverlap = FALSE;
            pageNotMapped = FALSE;
            break;
            }

        if((pageStart >= mapStart) && (pageEnd > mapEnd) &&
           (pageStart < mapEnd))
            {
            /* Top Overlap */

            if (pPmd->initialStateMask != initialStateMask  ||
                pPmd->initialState != initialState  ||
                pPmd->virtualAddr != pPmd->physicalAddr)
                {
                result = ERROR; /* Attributes are different */
                break;
                }

            /* Need to add entry for (pageEnd-mapEnd) */

            topOverlap = TRUE;
            pageAddressTop = (mapEnd + VM_PAGE_SIZE);
            pageLengthTop = (pageEnd - mapEnd);
            }

        if((pageStart < mapStart) && (pageEnd <= mapEnd) &&
           (pageEnd > mapStart))
            {
            /* Bottom Overlap */

            if (pPmd->initialStateMask != initialStateMask  ||
                pPmd->initialState != initialState  ||
                pPmd->virtualAddr != pPmd->physicalAddr)
                {
                result = ERROR; /* Attributes are different */
                break;
                }

            /* Need to add entry for (mapStart-pageStart) */

            bottomOverlap = TRUE;
            pageAddressBottom = pageStart;
            pageLengthBottom = (mapStart-pageStart);
            }

        if(topOverlap || bottomOverlap)
            break;

        }

    if (pageNotMapped && (result != ERROR))
        {
        pMmu = &sysPhysMemDesc[sysPhysMemDescNumEnt];
        if (pMmu->virtualAddr != (VIRT_ADDR) DUMMY_VIRT_ADDR)
            {
            result = ERROR;
            }
        else
            {
            if(noOverlap)
                {
                pMmu->virtualAddr       = (VIRT_ADDR)pageAddress;
                pMmu->physicalAddr      = (PHYS_ADDR)pageAddress;
                pMmu->len               = pageLength;
                pMmu->initialStateMask  = initialStateMask;
                pMmu->initialState      = initialState;
                sysPhysMemDescNumEnt    += 1;
                }

            if(topOverlap)
                {
                pMmu->virtualAddr       = (VIRT_ADDR)pageAddressTop;
                pMmu->physicalAddr      = (PHYS_ADDR)pageAddressTop;
                pMmu->len               = pageLengthTop;
                pMmu->initialStateMask  = initialStateMask;
                pMmu->initialState      = initialState;
                sysPhysMemDescNumEnt    += 1;
                }

            if(bottomOverlap)
                {
                pMmu->virtualAddr       = (VIRT_ADDR)pageAddressBottom;
                pMmu->physicalAddr      = (PHYS_ADDR)pageAddressBottom;
                pMmu->len               = pageLengthBottom;
                pMmu->initialStateMask  = initialStateMask;
                pMmu->initialState      = initialState;
                sysPhysMemDescNumEnt    += 1;
                }
            }
        }

    return (result);
    }

/*******************************************************************************
*
* bspSerialChanGet - get the SIO_CHAN device associated with a serial channel
*
* The sysSerialChanGet() routine returns a pointer to the SIO_CHAN
* device associated with a specified serial channel. It is called
* by usrRoot() to obtain pointers when creating the system serial
* devices, `/tyCo/x'. It is also used by the WDB agent to locate its
* serial channel.  The VxBus function requires that the BSP provide a
* function named bspSerialChanGet() to provide the information about
* any non-VxBus serial channels, provided by the BSP.  As this BSP
* does not support non-VxBus serial channels, this routine always
* returns ERROR.
*
* RETURNS: ERROR, always
*
*/

SIO_CHAN * bspSerialChanGet
    (
    int channel     /* serial channel */
    )
    {
    return ((SIO_CHAN *) ERROR);
    }

#if defined (INCLUDE_VXBUS)
#if defined(INCLUDE_PC_CONSOLE) && defined (DRV_TIMER_I8253)
/*******************************************************************************
*
* sysConBeep - sound beep function (using timer 2 for tone)
*
* This function is responsible for producing the beep
*
* RETURNS: N/A
*
* \NOMANUAL
*/

LOCAL void sysConBeep
    (
    BOOL    mode    /* TRUE:long beep  FALSE:short beep */
    )
    {
    int beepTime;
    int beepPitch;
    FAST int oldlevel;

    if (mode)
        {
        beepPitch = BEEP_PITCH_L;
        beepTime  = BEEP_TIME_L;   /* long beep */
        }
    else
        {
        beepPitch = BEEP_PITCH_S;
        beepTime  = BEEP_TIME_S;   /* short beep */
        }

    oldlevel = intCpuLock ();

    /* set command for counter 2, 2 byte write */

    sysOutByte(PIT_BASE_ADR + 3, (char)0xb6);
    sysOutByte(PIT_BASE_ADR + 2, (beepPitch & 0xff));
    sysOutByte(PIT_BASE_ADR + 2, (beepPitch >> 8));

    /* enable counter 2 */

    sysOutByte(DIAG_CTRL, sysInByte(DIAG_CTRL) | 0x03);

    taskDelay (beepTime);

    /* disable counter 2 */

    sysOutByte(DIAG_CTRL, sysInByte(DIAG_CTRL) & ~0x03);

    intCpuUnlock (oldlevel);
    return;
    }
#endif  /* defined(INCLUDE_PC_CONSOLE) && defined (DRV_TIMER_I8253) */
#endif  /* defined (INCLUDE_VXBUS) */

#ifdef INCLUDE_DISABLE_SMI

#define LPC_PMBASE_MASK   0x0000ff80    /* PMBASE address mask */
#define LPC_PMBASE_OFFSET 0x40          /* PMBASE offset address */
#define LPC_SMI_EN_OFFSET 0x30          /* SMI_EN i/o address offset */
#define SMI_GBL_EN        0x00000001    /* SMI interrupt global enable */
#define SMI_LEGACY_USB    0x00000008    /* Legacy USB SMI int enable */
#define SMI_LEGACY_USB2   0x00040000    /* Legacy USB2.0 SMI int enable */
#define SMI_TCO           0x00002000    /* TCO int enable */

UINT32 smi_en_addr = 0;

/*******************************************************************************
*
* smiMmuMap - Maps SMI register space
*
* This routine adds an entry to the sysMmuPhysDesc[] for SMI
*
* RETURNS: N/A
*/

void smiMmuMap (void)
    {
    UINT32 pmbase;

    /*
     * Get the PM base address
     */

    if (pciConfigInLong (0, 0x1f, 0, LPC_PMBASE_OFFSET, &pmbase) == ERROR)
        {
        return;
        }

    pmbase &= LPC_PMBASE_MASK;
    smi_en_addr  = pmbase + LPC_SMI_EN_OFFSET;
    }

/*******************************************************************************
*
* smiDisable - Disable SMI options according to configuration
*
* RETURNS: N/A
*/

void smiDisable (void)
    {
    UINT32 value;

    value = sysInLong(smi_en_addr);

    /* disable SMI USB/USB2 legacy interrupt, TCO interrupt */

    value &= ~(SMI_LEGACY_USB|SMI_LEGACY_USB2|SMI_TCO);

    /* be careful doing this unless you know what you are doing!! */

#if SMI_GLOBAL_DISABLE == TRUE
    value &= ~SMI_GBL_EN;
#endif

    sysOutLong(smi_en_addr, value);
    }
#endif  /* INCLUDE_DISABLE_SMI */

#if defined (INCLUDE_VIRTUAL_WIRE_MODE) || defined (INCLUDE_SYMMETRIC_IO_MODE)
#ifdef INCLUDE_USR_BOOT_OP

/*******************************************************************************
*
* Static configuration table for mpApicData structure, which allows
* for manual configuration of the mpApic driver. This also includes
* definition of the sysStaticMpApicDataGet() routine which loads
* the mpApicData structure (see also hwconf.c).
*
* One must be very careful when manually configuring APICs,
* since incorrect data will cause catastrophic error, i.e. BSP
* failing to boot properly.
*
* The data here is dependent on the number of cores involved,
* interrupt routing tables, and basically the specific configuration
* of your hardware.
*
* Access to this capability is obtained by defining INCLUDE_USR_BOOT_OP
* in config.h.
*/

#include "userMpApicData.c"

#endif  /* INCLUDE_USR_BOOT_OP */
#endif  /* defined (INCLUDE_VIRTUAL_WIRE_MODE) ||
           defined (INCLUDE_SYMMETRIC_IO_MODE) */

#ifdef INCLUDE_EARLY_PRINT
#include "sysKwrite.c"
#endif /* INCLUDE_EARLY_PRINT */

#ifdef  INCLUDE_USER_SYSLIB_FUNCTIONS_POST
#include "userSyslibFunctionsPost.c"                /* user extension hook */
#endif
