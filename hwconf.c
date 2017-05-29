/* hwconf.c - Hardware configuration support module */

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
01g,24may13,jjk  WIND00364942 - Removing AMP and GUEST
01f,09may13,jjk  WIND00364942 - Adding Unified BSP.
01e,18oct12,c_t  add panicPrint
01d,24sep12,c_t  add HPET support.
01c,26jun12,j_z  sync up with itl_nehalem bsp 01n version
01b,25mar11,lll  unification of itl BSPs
01a,28janll,lll  Adapted from Intel QM57, Intel Atom and pcPentium4
*/

/* includes */

#include <vxWorks.h>
#include <vsbConfig.h>
#include <sysLib.h>

#include <vxBusLib.h>
#include <hwif/vxbus/vxBus.h>
#include <hwif/vxbus/vxbPlbLib.h>
#include <hwif/vxbus/vxbPciLib.h>
#include <hwif/vxbus/vxbIntrCtlr.h>
#include <hwif/util/vxbParamSys.h>
#include <hwif/vxbus/hwConf.h>
#include <hwif/util/vxbNonVol.h>
#include <drv/pci/pciConfigLib.h>
#include <hwif/fw/acpiLib.h>
#include <bootApp.h>

#ifdef INCLUDE_USB
extern UINT32 usbMemToPci (void * pAddr);
extern void * usbPciToMem (UINT32 addr);
#endif

#include "config.h"

#include "../src/hwif/h/console/pcConsole.h"

#ifdef INCLUDE_ACPI_TABLE_MAP
IMPORT STATUS   acpiTableInit (void);
#endif  /* INCLUDE_ACPI_TABLES */

#ifdef INCLUDE_EARLY_PRINT
IMPORT void earlyPrint (char * outString);
IMPORT void panicPrint (char* outString);
#endif

#if defined (INCLUDE_VIRTUAL_WIRE_MODE) || defined (INCLUDE_SYMMETRIC_IO_MODE)
#include <hwif/intCtlr/vxbMpApic.h>
#endif /* INCLUDE_VIRTUAL_WIRE_MODE || INCLUDE_SYMMETRIC_IO_MODE */

IMPORT VOID sysPciPirqEnable (BOOL);

#define PCI_CLK_FREQ 1843200

#ifndef PCI_MEM_ADRS
#define PCI_MEM_ADRS     0x20000000
#define PCI_MEM_SIZE     0x00010000
#endif /* PCI_MEM_ADRS */

#ifndef PCI_MEMIO_ADRS
#define PCI_MEMIO_ADRS   0x20010000
#define PCI_MEMIO_SIZE   0x1fff0000
#endif /* PCI_MEMIO_ADRS */

#ifndef PCI_ISA_IO_ADRS
#define PCI_ISA_IO_ADRS  0x40000000
#define PCI_ISA_IO_SIZE  0x60000000
#endif /* PCI_ISA_IO_ADRS */

#ifndef PCI_IO_ADRS
#define PCI_IO_ADRS      0xa0000000
#define PCI_IO_SIZE      0x5ff80000
#endif /* PCI_IO_ADRS */

#if defined (DRV_TIMER_MC146818)
#define MC146818_CLK_FREQ   4194304
#endif /* DRV_TIMER_MC146818 */

#if defined (DRV_TIMER_IA_HPET)
#include <hwif/timer/vxbIaHpetTimer.h>
#endif /* DRV_TIMER_IA_HPET */

#if defined (DRV_TIMER_IA_TIMESTAMP)

/*
 * Always set to 0 to auto calibrate.
 */

#define IA_TIMESTAMP_CLK_FREQ    0
#endif /* defined (DRV_TIMER_IA_TIMESTAMP) */

#ifdef INCLUDE_NE2000_VXB_END
const struct hcfResource ne2000Resources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)IO_ADRS_ENE} },
    { "intr",        HCF_RES_INT,   {(void *)(INUM_TO_IVEC(INT_NUM_ENE))} },
    { "intrLevel",   HCF_RES_INT,   {(void *)INT_LVL_ENE} },
};
#define ne2000Num NELEMENTS(ne2000Resources)
#endif /* INCLUDE_NE2000_VXB_END */

/* vxBus resource files for onboard UARTS */

#ifdef DRV_SIO_NS16550
const struct hcfResource pentiumi82501Resources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)COM1_BASE_ADR} },
    { "irq",         HCF_RES_INT,   {(void *)(INUM_TO_IVEC(INT_NUM_COM1))} },
    { "regInterval", HCF_RES_INT,   {(void *)UART_REG_ADDR_INTERVAL} },
    { "irqLevel",    HCF_RES_INT,   {(void *)COM1_INT_LVL} },
    { "clkFreq",     HCF_RES_INT,   {(void *)PCI_CLK_FREQ} },
    { "fifoLen",     HCF_RES_INT,   {(void *)8} }
};
#define pentiumi82501Num NELEMENTS(pentiumi82501Resources)

const struct hcfResource pentiumi82502Resources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)COM2_BASE_ADR} },
    { "irq",         HCF_RES_INT,   {(void *)(INUM_TO_IVEC(INT_NUM_COM2))} },
    { "regInterval", HCF_RES_INT,   {(void *)UART_REG_ADDR_INTERVAL} },
    { "irqLevel",    HCF_RES_INT,   {(void *)COM2_INT_LVL} },
    { "clkFreq",     HCF_RES_INT,   {(void *)PCI_CLK_FREQ} },
    { "fifoLen",     HCF_RES_INT,   {(void *)8} }
};
#define pentiumi82502Num NELEMENTS(pentiumi82502Resources)
#endif /* DRV_SIO_NS16550 */

#if defined (INCLUDE_PC_CONSOLE) || defined (INCLUDE_WINDML)

/* PS2 keyboard controller 8042 */

const struct hcfResource pentiumi8042KbdResources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)DATA_8042} },
    { "irq",         HCF_RES_INT,   {(void *)(INUM_TO_IVEC(INT_NUM_KBD))} },
    { "regInterval", HCF_RES_INT,   {(void *)4} },
    { "irqLevel",    HCF_RES_INT,   {(void *)KBD_INT_LVL} },
    { "mode",        HCF_RES_INT,   {(void *)KEYBRD_MODE} }
};
#define pentiumi8042KbdNum NELEMENTS(pentiumi8042KbdResources)

/* PS2 mouse controller 8042 */

const struct hcfResource pentiumi8042MseResources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)DATA_8042} },
    { "irq",         HCF_RES_INT,   {(void *)(INUM_TO_IVEC(INT_NUM_MSE))} },
    { "regInterval", HCF_RES_INT,   {(void *)4} },
    { "irqLevel",    HCF_RES_INT,   {(void *)MSE_INT_LVL} }
};
#define pentiumi8042MseNum NELEMENTS(pentiumi8042MseResources)

/* VGA */

const struct hcfResource pentiumM6845VgaResources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)CTRL_SEL_REG} },
    { "memBase",     HCF_RES_INT,   {(void *)CTRL_MEM_BASE} },
    { "colorMode",   HCF_RES_INT,   {(void *)COLOR_MODE} },
    { "colorSetting",HCF_RES_INT,   {(void *)DEFAULT_ATR} }
};
#define pentiumM6845VgaNum NELEMENTS(pentiumM6845VgaResources)
#endif  /* INCLUDE_PC_CONSOLE || INCLUDE_WINDML */

const struct hcfResource pentiumPci0Resources[] = {
    { "regBase",            HCF_RES_INT,   {(void *)0x00} },
    { "mem32Addr",          HCF_RES_ADDR,  {(void *)PCI_MEM_ADRS} },
    { "mem32Size",          HCF_RES_INT,   {(void *)PCI_MEM_SIZE} },
    { "memIo32Addr",        HCF_RES_ADDR,  {(void *)PCI_MEMIO_ADRS} },
    { "memIo32Size",        HCF_RES_INT,   {(void *)PCI_MEMIO_SIZE} },
    { "io32Addr",           HCF_RES_ADDR,  {(void *)PCI_IO_ADRS} },
    { "io32Size",           HCF_RES_INT,   {(void *)PCI_IO_SIZE} },
    { "io16Addr",           HCF_RES_ADDR,  {(void *)PCI_ISA_IO_ADRS} },
    { "io16Size",           HCF_RES_INT,   {(void *)PCI_ISA_IO_SIZE} },
    { "fbbEnable",          HCF_RES_INT,   {(void *)TRUE} },
    { "cacheSize",          HCF_RES_INT,   {(void *)(_CACHE_ALIGN_SIZE / 4)} },
    { "autoIntRouteSet",    HCF_RES_INT,   {(void *)TRUE} },

    /* non vxbPciAutoConfig() values */

    { "pciConfigMechanism", HCF_RES_INT,   {(void *)(1)} },
    { "maxBusSet",          HCF_RES_INT,   {(void *)PCI_MAX_BUS} },
    { "autoConfig",         HCF_RES_INT,   {(void *)(FALSE)} },

#if defined (INCLUDE_SYMMETRIC_IO_MODE)
    { "funcPirqEnable",     HCF_RES_ADDR,  {(void *)sysPciPirqEnable} },
#endif  /* INCLUDE_SYMMETRIC_IO_MODE */

#ifdef INCLUDE_USB
    { "cpuToBus",           HCF_RES_ADDR,  {(void *)usbMemToPci} },
    { "busToCpu",           HCF_RES_ADDR,  {(void *)usbPciToMem} },
#endif /* INCLUDE_USB */

    { "vmStateMaskForAll",  HCF_RES_INT,   {(void *)(VM_STATE_MASK_FOR_ALL)} },
    { "vmStateForPci",      HCF_RES_INT,   {(void *)(VM_STATE_FOR_PCI)} }
};
#define pentiumPci0Num NELEMENTS(pentiumPci0Resources)

#ifdef DRV_TIMER_I8253
struct hcfResource i8253DevResources[] = {
    { "regBase",     HCF_RES_INT, {(void *)PIT_BASE_ADR} },
    { "clkFreq",     HCF_RES_INT, {(void *)PIT_CLOCK} },

#ifdef INCLUDE_SYMMETRIC_IO_MODE
    { "intr0",       HCF_RES_INT, {(void *)INUM_TO_IVEC (INT_NUM_IOAPIC_IRQ2)}},
#else
    { "intr0",       HCF_RES_INT, {(void *)INUM_TO_IVEC (INT_NUM_IRQ0)} },
#endif /* INCLUDE_SYMMETRIC_IO_MODE */

    { "intr0Level",  HCF_RES_INT, {(void *)PIT0_INT_LVL} },
    { "clkRateMin",  HCF_RES_INT, {(void *)SYS_CLK_RATE_MIN} },
    { "clkRateMax",  HCF_RES_INT, {(void *)SYS_CLK_RATE_MAX} },
    { "regInterval", HCF_RES_INT, {(void *)PIT_REG_ADDR_INTERVAL} }
};
#define i8253DevNum NELEMENTS(i8253DevResources)
#endif /* DRV_TIMER_I8253 */

#ifdef DRV_TIMER_LOAPIC
struct hcfResource loApicTimerResources[] = {
    { "regBase",    HCF_RES_INT, {(void *)(LOAPIC_BASE)} },
    { "intr",       HCF_RES_INT, {(void *)INUM_TO_IVEC (INT_NUM_LOAPIC_TIMER)}},
    { "intrLevel",  HCF_RES_INT, {(void *)INT_NUM_LOAPIC_TIMER} },
    { "clkFreq",    HCF_RES_INT, {(void *)APIC_TIMER_CLOCK_HZ} },
    { "clkRateMin", HCF_RES_INT, {(void *)SYS_CLK_RATE_MIN} },
    { "clkRateMax", HCF_RES_INT, {(void *)SYS_CLK_RATE_MAX} }
};
#define loApicTimerDevNum NELEMENTS(loApicTimerResources)
#endif /* DRV_TIMER_LOAPIC */

#if defined (DRV_TIMER_IA_HPET)

/*
 * Here we only add resources for 3 HPET devices since on early ICHs 
 * there are only 3 comparators.
 */

struct hcfResource iaHpetDevResources[] = {
    { "ioApicUnit",         HCF_RES_INT, {(void *)IA_HPET_IOAPIC_UNIT} },

    /* Synchronize periodic timer start to periodic interval */

    { "periodicSyncEnabled", HCF_RES_INT, {(void *)IA_HPET_PERIODIC_SYNC} },

    /* 
     * timer0 can be chosen as system clock by default. It can also be chosen
     * as a oneshot 64bit timer, but then it cannot be the system clock.
     */

    { "timer0ClkRateMin",  HCF_RES_INT, {(void *)SYS_CLK_RATE_MIN} },
    { "timer0ClkRateMax",  HCF_RES_INT, {(void *)SYS_CLK_RATE_MAX} },
    { "timer0Mode",        HCF_RES_INT, {(void *)IA_HPET_TIMER_MODE_PERIODIC} },
#if 0    
    { "intr0",       HCF_RES_INT, {(void *)INUM_TO_IVEC(INT_NUM_IOAPIC_PIRQE)}},
    { "intr0Level",        HCF_RES_INT, {(void *) 0x16 /*INT_NUM_IA_HPET_TIMER0 */ } },
#else
    { "intr0",       HCF_RES_INT, {(void *)INUM_TO_IVEC(INT_NUM_IOAPIC_PIRQG)}},
    { "intr0Level",        HCF_RES_INT, {(void *) 0x16  } },
#endif    
    /* timer1 can only be chosen as oneshot 32, or normal period timer */

    { "timer1ClkRateMin",  HCF_RES_INT, {(void *)AUX_CLK_RATE_MIN} },
    { "timer1ClkRateMax",  HCF_RES_INT, {(void *)AUX_CLK_RATE_MAX} },
    { "timer1Mode",        HCF_RES_INT, {(void *)IA_HPET_TIMER_MODE_PERIODIC} },
#if 0    
    { "intr1",       HCF_RES_INT, {(void *)INUM_TO_IVEC(INT_NUM_IOAPIC_PIRQF)}},
    { "intr1Level",        HCF_RES_INT, {(void *) 0x17 /* INT_NUM_IA_HPET_TIMER1 */} },
#else
    { "intr1",       HCF_RES_INT, {(void *)INUM_TO_IVEC(INT_NUM_IOAPIC_PIRQH)}},
    { "intr1Level",        HCF_RES_INT, {(void *) 0x17 } },
#endif    
#if 0
    /* timer2 can only be chosen as oneshot 32, or normal period timer */

    { "timer2Mode",        HCF_RES_INT, {(void *)IA_HPET_TIMER_MODE_ONESHOT32}},
    { "intr2",       HCF_RES_INT, {(void *)INUM_TO_IVEC(INT_NUM_IOAPIC_PIRQG)}},
    { "intr2Level",        HCF_RES_INT, {(void *)INT_NUM_IA_HPET_TIMER2} },
#endif /* LAURENT */    
};
#define iaHpetDevNum NELEMENTS(iaHpetDevResources)
#endif /* DRV_TIMER_IA_HPET */

#if defined (DRV_TIMER_IA_TIMESTAMP)
extern UINT64 sysGetTSCCountPerSec (void);
struct hcfResource iaTimestampResources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)0} },
    { "clkFreq",     HCF_RES_INT,   {(void *)0} }, /* 0 to use clkCal */
    { "clkCal",      HCF_RES_ADDR,  {(void *)sysGetTSCCountPerSec} },
};
#define iaTimestampNum NELEMENTS(iaTimestampResources)
#endif /* DRV_TIMER_IA_TIMESTAMP */

#if defined (DRV_TIMER_MC146818)
struct hcfResource mc146818DevResources[] = {
    { "regBase",     HCF_RES_INT,   {(void *)RTC_INDEX} },
    { "irq",         HCF_RES_INT,   {(void *)INUM_TO_IVEC(INT_NUM_RTC)} },
    { "irqLevel",    HCF_RES_INT,   {(void *)RTC_INT_LVL} },
    { "clkFreq",     HCF_RES_INT,   {(void *)MC146818_CLK_FREQ} },
    { "clkRateMin",  HCF_RES_INT,   {(void *)2} },
    { "clkRateMax",  HCF_RES_INT,   {(void *)8192} },
};
#define mc146818DevNum NELEMENTS(mc146818DevResources)
#endif /* DRV_TIMER_MC146818 */

#if defined (INCLUDE_VIRTUAL_WIRE_MODE) || defined (INCLUDE_SYMMETRIC_IO_MODE)
#ifdef INCLUDE_USR_BOOT_OP
void sysStaticMpApicDataGet(MP_APIC_DATA *pMpApicData);
#endif
struct hcfResource mpApicResources[] = {
    { "regBase",            HCF_RES_INT,   {(void *)ERROR} },
#ifdef INCLUDE_USR_BOOT_OP
    { "mpBootOp",           HCF_RES_INT,   {(void *)USR_MP_STRUCT} },
#elif defined INCLUDE_ACPI_BOOT_OP
    { "mpBootOp",           HCF_RES_INT,   {(void *)ACPI_MP_STRUCT} },
#elif defined INCLUDE_MPTABLE_BOOT_OP
    { "mpBootOp",           HCF_RES_INT,   {(void *)MP_MP_STRUCT} },
#else
    { "mpBootOp",           HCF_RES_INT,   {(void *)NO_MP_STRUCT} },
#endif
    { "mpDataLoc",          HCF_RES_INT,   {(void *)MPAPIC_DATA_START} },
#ifdef INCLUDE_USR_BOOT_OP
    { "mpDataGet",          HCF_RES_ADDR,  {(void *)sysStaticMpApicDataGet} },
#else
    { "mpDataGet",          HCF_RES_ADDR,  {NULL} },
#endif
#if defined (INCLUDE_SYMMETRIC_IO_MODE)
    { "symmetricIoMode",    HCF_RES_INT,   {(void *)TRUE} },
#else
    { "symmetricIoMode",    HCF_RES_INT,   {(void *)FALSE} },
#endif  /* INCLUDE_SYMMETRIC_IO_MODE */
#if defined (INCLUDE_EARLY_PRINT)
    { "panicPrint",         HCF_RES_ADDR,  {(void *)panicPrint} },
#endif  /* INCLUDE_EARLY_PRINT */
};
#define mpApicNum NELEMENTS(mpApicResources)

LOCAL const struct intrCtlrInputs loApicInputs[] = {	
    { VXB_INTR_DYNAMIC, "yn", 0, 0 },
/*
 * The following lines are for using Message Signaled Interrupts (MSI)
 * for GEI Ethernet driver instead of legacy interrupts
 */
    { VXB_INTR_DYNAMIC, "gei", 0, 0 },
    { VXB_INTR_DYNAMIC, "gei", 1, 0 },
    { VXB_INTR_DYNAMIC, "gei", 2, 0 },
    { VXB_INTR_DYNAMIC, "gei", 3, 0 },
    { VXB_INTR_DYNAMIC, "gei", 4, 0 },
    { VXB_INTR_DYNAMIC, "gei", 5, 0 },
    { VXB_INTR_DYNAMIC, "gei", 6, 0 },
    { VXB_INTR_DYNAMIC, "gei", 7, 0 },
    { VXB_INTR_DYNAMIC, "gei", 8, 0 },
    { VXB_INTR_DYNAMIC, "gei", 9, 0 },

    { VXB_INTR_DYNAMIC, "rtg", 0, 0 },
    { VXB_INTR_DYNAMIC, "rtg", 1, 0 },

#ifdef DRV_TIMER_HPET
    { VXB_INTR_DYNAMIC, "iaHpetTimerDev", 0, 0 },
    { VXB_INTR_DYNAMIC, "iaHpetTimerDev", 0, 1 },
    { VXB_INTR_DYNAMIC, "iaHpetTimerDev", 0, 2 },
    { VXB_INTR_DYNAMIC, "iaHpetTimerDev", 0, 3 },
#endif   
};

LOCAL const struct intrCtlrCpu loApicCpu[] = {
#if 0		
    { COM2_INT_LVL, 1 },
#endif /* LAURENT */    
};

LOCAL struct hcfResource loApicIntrResources[] = {
    { "regBase",            HCF_RES_INT,   {(void *)ERROR} },
    { "input",              HCF_RES_ADDR,  {(void *)&loApicInputs[0]} },
    { "inputTableSize",     HCF_RES_INT,   {(void *)NELEMENTS(loApicInputs)} },
    { "cpuRoute",           HCF_RES_ADDR,  {(void *)&loApicCpu[0]} },
    { "cpuRouteTableSize",  HCF_RES_INT,   {(void *)NELEMENTS(loApicCpu)} },
#if defined (INCLUDE_SYMMETRIC_IO_MODE)
    { "symmetricIoMode",    HCF_RES_INT,   {(void *)TRUE} },
#else
    { "symmetricIoMode",    HCF_RES_INT,   {(void *)FALSE} },
#endif  /* INCLUDE_SYMMETRIC_IO_MODE */
#if defined (INCLUDE_VIRTUAL_WIRE_MODE)
    { "virtualWireMode",    HCF_RES_INT,   {(void *)TRUE} },
#else
    { "virtualWireMode",    HCF_RES_INT,   {(void *)FALSE} },
#endif  /* INCLUDE_VIRTUAL_WIRE_MODE */
};
#define loApicIntrNum NELEMENTS(loApicIntrResources)
#endif /* INCLUDE_VIRTUAL_WIRE_MODE || INCLUDE_SYMMETRIC_IO_MODE */

#if defined (INCLUDE_SYMMETRIC_IO_MODE)
/*
 * May use the same info here for multiple IO Apics since
 * everything is determined via mpApic self-discovery during
 * initialization.
 */

struct hcfResource ioApicIntr0Resources[] = {
    { "regBase",            HCF_RES_INT,   {(void *)ERROR} },
};
#define ioApicIntr0Num NELEMENTS(ioApicIntr0Resources)
#endif /* INCLUDE_SYMMETRIC_IO_MODE */

NVRAM_SEGMENT flNvRam0Segments[] = {
    { "bootline", 0, NV_BOOT_OFFSET, BOOT_LINE_SIZE },
};

const struct hcfResource flNvRam0Resources[] = {
    { "regBase",         HCF_RES_INT,   {(void *)-1} },
    { "segments",        HCF_RES_ADDR,  {(void *)&flNvRam0Segments[0]} },
    { "numSegments",     HCF_RES_INT,   {(void *)NELEMENTS(flNvRam0Segments)} },
    { "size",            HCF_RES_INT,   {(void *)NV_RAM_SIZE} },
    { "fileName",        HCF_RES_STRING,{(void *)"/bd0/nvram.txt"} },
};
#define flNvRam0Num NELEMENTS(flNvRam0Resources)


const struct hcfDevice hcfDeviceList[] = {
#if defined (INCLUDE_VIRTUAL_WIRE_MODE) || defined (INCLUDE_SYMMETRIC_IO_MODE)
    { "mpApic",     0, VXB_BUSID_PLB, 0, mpApicNum,     mpApicResources },
    { "loApicIntr", 0, VXB_BUSID_PLB, 0, loApicIntrNum, loApicIntrResources },
#ifdef INCLUDE_SYMMETRIC_IO_MODE
    /* The majority of boards only have 2, but just incase... */
    { "ioApicIntr", 0, VXB_BUSID_PLB, 0, ioApicIntr0Num, ioApicIntr0Resources },
    { "ioApicIntr", 1, VXB_BUSID_PLB, 0, ioApicIntr0Num, ioApicIntr0Resources },
    { "ioApicIntr", 2, VXB_BUSID_PLB, 0, ioApicIntr0Num, ioApicIntr0Resources },
    { "ioApicIntr", 3, VXB_BUSID_PLB, 0, ioApicIntr0Num, ioApicIntr0Resources },
#endif /* INCLUDE_SYMMETRIC_IO_MODE */
#endif /* INCLUDE_VIRTUAL_WIRE_MODE || INCLUDE_SYMMETRIC_IO_MODE */
#ifdef INCLUDE_NE2000_VXB_END
    { "ene",        0, VXB_BUSID_PLB, 0, ne2000Num, ne2000Resources },
#endif /* INCLUDE_NE2000_VXB_END */
#ifdef DRV_SIO_NS16550
    { "ns16550", 0, VXB_BUSID_PLB, 0, pentiumi82501Num, pentiumi82501Resources},
    { "ns16550", 1, VXB_BUSID_PLB, 0, pentiumi82502Num, pentiumi82502Resources},
#endif /* DRV_SIO_NS16550 */
    { "pentiumPci", 0, VXB_BUSID_NEXUS,0, pentiumPci0Num, pentiumPci0Resources},
#if defined (INCLUDE_PC_CONSOLE) || defined (INCLUDE_WINDML)
    { "i8042Kbd",0,VXB_BUSID_PLB,0,pentiumi8042KbdNum,pentiumi8042KbdResources},
    { "i8042Mse",0,VXB_BUSID_PLB,0,pentiumi8042MseNum,pentiumi8042MseResources},
    { "m6845Vga",0,VXB_BUSID_PLB,0,pentiumM6845VgaNum,pentiumM6845VgaResources},
#endif /* INCLUDE_PC_CONSOLE || INCLUDE_WINDML */
#if defined (DRV_TIMER_I8253)
    { "i8253TimerDev", 0, VXB_BUSID_PLB, 0, i8253DevNum, i8253DevResources },
#endif /* DRV_TIMER_I8253 */
#if defined (DRV_TIMER_LOAPIC)
    { "loApicTimer",0,VXB_BUSID_PLB,0,loApicTimerDevNum, loApicTimerResources },
#endif /* DRV_TIMER_LOAPIC */
#if defined (DRV_TIMER_IA_HPET)
    { "iaHpetTimerDev", 0, VXB_BUSID_PLB, 0,iaHpetDevNum, iaHpetDevResources },
#endif /* DRV_TIMER_IA_HPET */
#if defined (DRV_TIMER_IA_TIMESTAMP)
    { "iaTimestamp", 0, VXB_BUSID_PLB, 0, iaTimestampNum, iaTimestampResources},
#endif /* DRV_TIMER_IA_TIMESTAMP */
#if defined (DRV_TIMER_MC146818)
    { "mc146818Rtc", 0, VXB_BUSID_PLB, 0, mc146818DevNum, mc146818DevResources},
#endif /* DRV_TIMER_MC146818 */
    { "fileNvRam",   0, VXB_BUSID_PLB, 0, flNvRam0Num, flNvRam0Resources },
};
const int hcfDeviceNum = NELEMENTS(hcfDeviceList);

VXB_INST_PARAM_OVERRIDE sysInstParamTable[] =
    {
    { NULL, 0, NULL, VXB_PARAM_END_OF_LIST, {(void *)0} }
    };

/*******************************************************************************
*
* sysConfInit0 - Early BSP configuration initialization
*
* This routine initializes devices that are BootApp dependent.
*
* It is placed in this file because the project facility parses bootapp files
* differently than normal files only within hwconf.
*
* RETURNS: N/A
*
*/
void sysConfInit0 (void)
    {
#ifdef INCLUDE_ACPI_DEBUG_PRINT
    func_acpiEarlyPrint = earlyPrint;   /* print ACPI debug info */
#endif  /* INCLUDE_ACPI_DEBUG_PRINT */
    }

/*******************************************************************************
*
* sysConfInit - BSP configuration initialization
*
* This routine initializes devices that are BootApp dependent.
*
* It is placed in this file because the project facility parses bootapp files
* differently than normal files only within hwconf.
*
* RETURNS: N/A
*
*/
void sysConfInit (void)
    {
#ifdef INCLUDE_ACPI_TABLE_MAP
    acpiTableInit ();        /* map in ACPI tables (if found) */
#endif  /* INCLUDE_ACPI_TABLES */
    }

/*******************************************************************************
*
* sysConfStop - BSP configuration termination
*
* This routine terminates devices that are BootApp dependent.
*
* It is placed in this file because the project facility parses bootapp files
* differently than normal files only within hwconf.
*
* RETURNS: N/A
*
*/
void sysConfStop (void)
    {
#ifdef INCLUDE_CPU_PERFORMANCE_MGMT
    cpuPwrArchDisable ();      /* shutdown performance management */
#endif
    }
