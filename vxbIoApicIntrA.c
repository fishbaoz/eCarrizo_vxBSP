/* vxbIoApicIntr.c - VxBus driver for the Intel IO APIC/xAPIC driver */

/*
 * Copyright (c) 2007-2015 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
02q,29jan15,jjk  VXW6-83522 - Lookup ioApic index rather than using ioApicId
02p,04aug14,scm  VXW6-83233 - adjust casting for redirection table get/set 
                 routines, allow for isupport of max entries...
02o,31jan14,jb   VXW6-25191 - with soft reboot, pending interrupts in the
                 ioapic are NOT acknowledged
02n,24sep13,scm  WIND00421749 vxBus IO APIC driver won't support 240 interrupt
                 entries...
02m,29aug13,jb   WIND00431950 - vxbIoApicIntrShowAll page faults
02l,17jun13,slk  add support for 240 interrupt inputs for wrVbX86 Guest
02k,22may13,scm  WIND00345552 Modify IO Apic interrupt priorities
                 initialization.
02j,25apr13,rbc  WIND00414401 - Changes to support Unified BSP.
02i,15jan13,j_z  add MSI vector check for intReroute.(WIND00333514)
02h,14dec12,jb   WIND00368280 - target Apic ID arbitrarily modified
02g,24sep12,c_t  temporary set trigger to edge when disable interrupt.
02f,23sep12,c_t  fix issue when disable interrupt other than CPU 0.
02e,19apr12,fao  move up vxbBusPresent define. (WIND00346008)
02d,21mar12,g_x  Remove unused code. (WIND00288230)
02c,16dec11,scm  WIND00322159 modify vxbIoApicIntrIntReroute to range check
                 routeToCpu...
02b,01dec10,yjw  Get IOAPIC ID from system configuration data.(WIND00243937)
02a,25oct10,sem  Update for new IA cpu tags
01z,10aug10,jb   Fix for WIND00227184 - uninitialized variable
01y,14jul10,cwl  fix interrupt Disable/Enable. (WIND00221671)
01x,01jun10,jb   Modifying to conform to uHLD 1044 and 1069
01w,20may10,scm  Adjust register base index in vxbIoApicIntrShowAll 
01v,24mar10,h_k  LP64 adaptation.
                 increased VxBus version to 5.0.0.
                 removed unnecessary zero clear to ioApicIntrData struct and
                 sysInumTbl.
01v,14jan10,sem  Fix IO APIC vector assignment
01u,05feb09,scm  adjust for ioApicBaseId being out of sequence...
01t,13nov08,scm  modify for multi apic support...
01s,29oct08,wap  Fix interrupt rerouting for PCI devices (WIND00123194)
01r,27aug08,jpb  Renamed VSB header file
01q,21aug08,h_k  fixed a compile error in Diab.
01p,18jul08,scm  WIND00128127 -use vxCpuLoApicId for UP as well...
01o,18jun08,jpb  Renamed _WRS_VX_SMP to _WRS_CONFIG_SMP.  Added include path
                 for kernel configurations options set in vsb.
01n,11jun08,pmr  get regBase directly
01m,09jun08,scm  WIND00123218.
01l,06may08,scm  WIND00103470 - SMP/SYMMETRIC_IO_MODE warm re-boot support.
01k,05may08,tor  update version
01j,05mar08,scm  gnu corrections...
01i,20sep07,tor  VXB_VERSION_3
01h,15sep07,scm  WIND00095242.
01g,05sep07,p_g  modifications done for ApiGen
01f,21jun07,h_k  changed to library build.
01e,21jun07,scm  correct for UP SYMMETRIC_IO_MODE BUILDS...
01d,31may07,tor  Cleanup and add reroute (CPU) routines
01c,23may07,scm  clean up multiple APIC support...
01b,11may07,scm  remove dummy function...
01a,21Mar07,scm  created.
*/

/*
DESCRIPTION
This is the VxBus driver for the Io APIC Interrupt Driver

This module is a driver for the IO APIC/xAPIC (Advanced Programmable
Interrupt Controller) for P6 (PentiumPro, II, III) family processors
and P7 (Pentium4) family processors.  The IO APIC/xAPIC is included
in the Intel's system chip set, such as ICH2.  Software intervention
may be required to enable the IO APIC/xAPIC in some chip sets.

The 8259A interrupt controller is intended for use in a uni-processor
system, IO APIC can be used in either a uni-processor or multi-processor
system.  The IO APIC handles interrupts very differently than the 8259A.
Briefly, these differences are:

- Method of Interrupt Transmission. The IO APIC transmits interrupts
  through a 3-wire bus and interrupts are handled without the need for
  the processor to run an interrupt acknowledge cycle.
- Interrupt Priority. The priority of interrupts in the IO APIC is
  independent of the interrupt number.  For example, interrupt 10 can
  be given a higher priority than interrupt 3.
- More Interrupts. The IO APIC supports a total of 24 interrupts.

The IO APIC unit consists of a set of interrupt input signals, a 24-entry
by 64-bit Interrupt Redirection Table, programmable registers, and a message
unit for sending and receiving APIC messages over the APIC bus or the
Front-Side (system) bus.  IO devices inject interrupts into the system by
asserting one of the interrupt lines to the IO APIC.  The IO APIC selects the
corresponding entry in the Redirection Table and uses the information in that
entry to format an interrupt request message.  Each entry in the Redirection
Table can be individually programmed to indicate edge/level sensitive interrupt
signals, the interrupt vector and priority, the destination processor, and how
the processor is selected (statically and dynamically).  The information in
the table is used to transmit a message to other APIC units (via the APIC bus
or the Front-Side (system) bus).

IO APIC is used in the Symmetric IO Mode (define SYMMETRIC_IO_MODE in the BSP).
The base address of IO APIC is determined in loApicInit() and stored in the
global variable ioApicBase and ioApicData.  ioApicInit() initializes the IO
APIC with information stored in ioApicRed0_15 and ioApicRed16_23.
The ioApicRed0_15 is the default lower 32 bit value of the redirection table
entries for IRQ 0 to 15 that are edge triggered positive high, and
the ioApicRed16_23 is for IRQ 16 to 23 that are level triggered positive low.
ioApicRedSet() and ioApicRedGet() are used to access the redirection table.
ioApicEnable() enables the IO APIC or xAPIC.  ioApicIrqSet() set the specific
IRQ to be delivered to the specific Local APIC.
*/

/* includes */

#include <vxWorks.h>
#include <vsbConfig.h>
#include <intLib.h>
#include <cpuset.h>
#include <stdio.h>
#include <string.h>
#include <logLib.h>
#include <vxCpuLib.h>

#include <vxbTimerLib.h>
#include <hwif/util/hwMemLib.h>
#include <hwif/vxbus/vxBus.h>
#include <hwif/vxbus/vxbPlbLib.h>
#include <hwif/vxbus/vxbPciLib.h>
#include <hwif/vxbus/hwConf.h>
#include <hwif/intCtlr/vxbMpApic.h>

#include <../src/hwif/intCtlr/vxbIntCtlrLib.h>
#include "../h/vxbus/vxbAccess.h"

#include <private/vmLibP.h>
#include <arch/i86/vxCpuArchLib.h>

/* defines */

#define IOAPIC_NAME             "ioApicIntr"

/* IO APIC direct register offset */

#define IOAPIC_IND              0x00    /* Index Register */
#define IOAPIC_DATA             0x10    /* IO window (data) - pc.h */
#define IOAPIC_IRQPA            0x20    /* IRQ Pin Assertion Register */

/* IO APIC indirect register offset */

#define IOAPIC_ID               0x00    /* IOAPIC ID */
#define IOAPIC_VERS             0x01    /* IOAPIC Version */
#define IOAPIC_ARB              0x02    /* IOAPIC Arbitration ID */
#define IOAPIC_BOOT             0x03    /* IOAPIC Boot Configuration */
#define IOAPIC_REDTBL           0x10    /* Redirection Table (24 * 64bit) */
#define IOAPIC_EOI              0x40    /* end of interrupt register */

/* Interrupt delivery type */

#define IOAPIC_DT_APIC          0x0     /* APIC serial bus */
#define IOAPIC_DT_FS            0x1     /* Front side bus message*/

/* Version register bits */

#define IOAPIC_MRE_MASK         0x00ff0000      /* Max Red. entry mask */
#define IOAPIC_PRQ              0x00008000      /* this has IRQ reg */
#define IOAPIC_VERSION          0x000000ff      /* version number */

/* Redirection table entry bits: upper 32 bit */

#define IOAPIC_DESTINATION      0xff000000

/* Redirection table entry bits: lower 32 bit */

#define IOAPIC_INT_MASK         0x00010000
#define IOAPIC_LEVEL            0x00008000
#define IOAPIC_EDGE             0x00000000
#define IOAPIC_REMOTE           0x00004000
#define IOAPIC_LOW              0x00002000
#define IOAPIC_HIGH             0x00000000
#define IOAPIC_LOGICAL          0x00000800
#define IOAPIC_PHYSICAL         0x00000000
#define IOAPIC_FIXED            0x00000000
#define IOAPIC_LOWEST           0x00000100
#define IOAPIC_SMI              0x00000200
#define IOAPIC_NMI              0x00000400
#define IOAPIC_INIT             0x00000500
#define IOAPIC_EXTINT           0x00000700
#define IOAPIC_VEC_MASK         0x000000ff

/* Vector Assignment */

/*
 * Interrupt priority is implied by its vector number, according to
 * the following relationship:
 *   priority = vectorNo / 16
 * The lowest priority is 1 and 15 is the highest.  To avoid losing
 * interrupts, software should allocate no more than 2 interrupt
 * vectors per priority.  The recommended priority class are:
 *   priority class 15   : system event interrupt (power fail, etc)
 *   priority class 14   : inter processor interrupt (IPI)
 *   priority class 13   : local interrupt (LINT[1:0] events)
 *   priority class 12   : timer interrupt (local timer and error interrupt)
 *   priority class 11-3 : I/O interrupt
 *   priority class 2    : I/O interrupt (CPU self interrupt)
 *   priority class 1-0  : reserved
 *
 *   IO APIC vector assignments follow the Local APIC vector assignment, so
 *   we begin at 0xBC...
 */

#define IOAPIC_VECTOR_INT_NUM_IRQ0   0xBC    /* IO APIC IRQ 0 vec no */
#define IOAPIC_VECTOR_INT_NUM_STRT   0xBF    /* IO APIC IRQ vec no strt */

#define IOAPIC_VECTOR_INT_NUM_PIRQA  0x3F    /* IO APIC PIRQ A vec no */

#define IOAPIC_ISA_VECTOR_OFFSET     0x08    /* 2 per level */

#define IOAPIC_PCI_VECTOR_OFFSET     0x01    /* try and maximize spread */

/* externs */

IMPORT UINT32 sysProcessor;     /* 0=386, 1=486, 2=P5, 4=P6, 5=P7 */

IMPORT INT8   excCallTbl [];    /* table of Calls in excALib.s */

IMPORT INT32  sysCsInt;         /* CS for interrupt */

IMPORT UINT32 sysIntIdtType;    /* IDT entry type */

IMPORT UINT8  *sysInumTbl;      /* IRQ vs intNum table */

IMPORT UINT32 sysInumTblNumEnt;

IMPORT UINT32 glbMpApicNioint;   /* number of io interrupts (MP Table) */

IMPORT UINT8  glbMpApicIoBaseId; /* base IO Apic Id */

IMPORT MP_INTERRUPT *glbMpApicInterruptTable;

IMPORT UINT8 mpCpuIndexTable[];

IMPORT unsigned int vxCpuLoApicId (void); /* defined in vxCpuALib.s */

IMPORT STATUS plbIntrGet (VXB_DEVICE_ID         pDev,
                          int                   indx,
                          VXB_DEVICE_ID *       ppIntCtl,
                          int *                 pInputPin);

IMPORT struct vxbBusPresent * pPlbBus;

/* typedefs */

LOCAL BOOL vxbIoApicIntrInstProbe(VXB_DEVICE_ID pInst);

LOCAL void vxbIoApicIntrInstInit(VXB_DEVICE_ID pInst);
LOCAL void vxbIoApicIntrInstInit2(VXB_DEVICE_ID pInst);
LOCAL void vxbIoApicIntrIntAck(VXB_DEVICE_ID pInst);

LOCAL struct drvBusFuncs vxbIoApicIntrFuncs =
    {
    vxbIoApicIntrInstInit,  /* devInstanceInit */
    vxbIoApicIntrInstInit2, /* devInstanceInit2 */
    NULL                    /* devConnect */
    };

/* defines */

/* structure holding ioApicIntr details */

struct ioApicIntrData
    {
    VXB_DEVICE_ID       pInst;          /* instance pointer */
    struct intCtlrHwConf isrHandle;
    UINT8 *		mpApicHwAddrTable;

    char *              ioApicBase;     /* def IO APIC addr */
    UINT32 *            ioApicData;     /* def IO APIC data */

    /* IO APIC id register per APIC */

    UINT32              ioApicId;

    /* IO APIC id index */

    UINT8               ioApicIndex;

    /* IO APIC version register per APIC */

    UINT32              ioApicVersion;

    /* IO APIC redirection table number per APIC */

    UINT8               ioApicRedNumEntries;

    UINT32              *ioApicIntTable;

    };

/* globals */

const UINT32 ioApicRed0_15      = IOAPIC_EDGE | IOAPIC_HIGH | IOAPIC_FIXED;
const UINT32 ioApicRed16_23     = IOAPIC_LEVEL | IOAPIC_LOW | IOAPIC_FIXED;

/* locals */

/* Due to vxbus pci initialization restraints and performance
 * requirements these need to be shared with the BSP for
 * proper initialization of interrupt routing and pci
 */

UINT32 ioApicIntrDrvInitializedCount = 0;
UINT8  ioApicRedNumEntries;
UINT32 ioApicNumIoApics;

LOCAL VXB_DEVICE_ID vxbIoApicIntrUnit0Id = NULL;

/* forward declarations */

void vxbIoApicIntrDrvRegister (void);

LOCAL STATUS vxbIoApicIntrInit (VXB_DEVICE_ID pInst);

LOCAL STATUS vxbIoApicIntrIntEnable (VXB_DEVICE_ID pInst, INT32 *irqNo);
LOCAL STATUS vxbIoApicIntrIntDisable (VXB_DEVICE_ID pInst, INT32 *irqNo);

LOCAL STATUS vxbIoApicIntrIntLock (VXB_DEVICE_ID pInst, int *dummy);
LOCAL STATUS vxbIoApicIntrIntUnlock (VXB_DEVICE_ID pInst, int *dummy);
LOCAL STATUS vxbIoApicIntrIdGet
    (
    VXB_DEVICE_ID pInst,
    UINT32 * ioApicId
    );

LOCAL STATUS vxbIoApicIntrRedGet (VXB_DEVICE_ID pInst,
		                  INT32 irq,
			          UINT32 *pUpper32,
			          UINT32 *pLower32);
LOCAL STATUS vxbIoApicIntrRedSet (VXB_DEVICE_ID pInst,
		                  INT32 irq,
			          UINT32 upper32,
			          UINT32 lower32);

LOCAL UINT32  vxbIoApicIntrGet (char * index,
			       UINT32 * data,
			       UINT32 offset);
LOCAL void   vxbIoApicIntrSet (char * index,
			       UINT32 * data,
			       UINT32 offset,
			       UINT32 value);

LOCAL STATUS vxbIoApicIntrVersionGet (VXB_DEVICE_ID  pInst,
		                      UINT32 *ioApicVersion);

LOCAL STATUS vxbIoApicIntrBaseGet   (VXB_DEVICE_ID  pInst,
                                     char ** ioApicBase);

LOCAL STATUS vxbIoApicRedNumEntriesGet (VXB_DEVICE_ID  pInst,
                                        UINT8 *ioApicRedNumEntries);

LOCAL STATUS vxbIoApicIntrCpuReroute
    (
    VXB_DEVICE_ID       pDev,
    void *              destCpu
    );
LOCAL STATUS vxbIoApicIntrIntReroute
    (
    VXB_DEVICE_ID       pDev,
    int                 index,
    cpuset_t            destCpu
    );

#ifdef INCLUDE_SHOW_ROUTINES
void vxbIoApicIntrDataShow
    (
    VXB_DEVICE_ID  pInst,
    int 	   verboseLevel
    );
#endif /* INCLUDE_SHOW_ROUTINES */

void vxbIoApicIntrShowAll (void);

LOCAL device_method_t vxbIoApicIntr_methods[] =
    {
    DEVMETHOD (ioApicIntrIntEnable, vxbIoApicIntrIntEnable),
    DEVMETHOD (ioApicIntrIntDisable, vxbIoApicIntrIntDisable),
    DEVMETHOD (ioApicIntrIntLock, vxbIoApicIntrIntLock),
    DEVMETHOD (ioApicIntrIntUnlock, vxbIoApicIntrIntUnlock),
    DEVMETHOD (ioApicIntrVersionGet, vxbIoApicIntrVersionGet),
    DEVMETHOD (ioApicIntrBaseGet, vxbIoApicIntrBaseGet),
    DEVMETHOD (ioApicRedNumEntriesGet, vxbIoApicRedNumEntriesGet),
    DEVMETHOD (ioApicIntrRedGet, vxbIoApicIntrRedGet),
    DEVMETHOD (ioApicIntrRedSet, vxbIoApicIntrRedSet),
    DEVMETHOD (ioApicIntrIdGet, vxbIoApicIntrIdGet),
    DEVMETHOD (vxbIntCtlrCpuReroute, vxbIoApicIntrCpuReroute),
    DEVMETHOD (vxbIntCtlrIntReroute, vxbIoApicIntrIntReroute),
#ifdef INCLUDE_SHOW_ROUTINES
    DEVMETHOD (busDevShow, vxbIoApicIntrDataShow),
#endif /* INCLUDE_SHOW_ROUTINES */
    { 0, 0}
    };

/* ioApic driver registration data structure */

LOCAL struct vxbDevRegInfo ioApicIntrDrvRegistration =
    {
    NULL,                      /* pNext */
    VXB_DEVID_DEVICE,          /* devID */
    VXB_BUSID_PLB,             /* busID = PLB */
    VXB_VER_5_0_0,             /* vxbVersion */
    IOAPIC_NAME,               /* drvName */
    &vxbIoApicIntrFuncs,       /* pDrvBusFuncs */
    NULL,                      /* pMethods */
    vxbIoApicIntrInstProbe     /* devProbe */
    };

/* local defines       */

/* static forward declarations */

/* driver functions */

/*******************************************************************************
*
* vxbIoApicIntrIntAck - acknowledges all IO APIC interrupts
*
* This routine acknowledges all IO APIC interrupts.
*  
* This may be necessary during initialization, if a soft reboot 
* has occurred and "old" interrupts are pending.
*  
* Requires: EOI Register support, (I.E. For I/O xAPIC version 0x2X or greater).
*         
* RETURNS: N/A
*
* SEE ALSO: vxbIoApicIntrIntUnlock()
*
* \NOMANUAL
*/

LOCAL void vxbIoApicIntrIntAck(VXB_DEVICE_ID pInst)
    {
    INT32 ix;
    UINT32 offset;
    UINT32 eoiRegVal;

    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if((pInst == NULL) ||(pInst->pDrvCtrl == NULL))
        return;

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);
    for (ix = 0; ix < (pIoApicIntrData->ioApicRedNumEntries); ix++)
        {
        offset = IOAPIC_REDTBL + (ix << 1);
        eoiRegVal = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                      pIoApicIntrData->ioApicData,
                                      offset);
        eoiRegVal &= 0x000000ff;

        /*
         * write interrupt vector information to the ioapic EOIR
         * (end of interrupt register)
         */

        *(INT32*)(pIoApicIntrData->ioApicBase + IOAPIC_EOI) = eoiRegVal;
        }
    return;
}

/******************************************************************************
*
* vxbIoApicIntrDrvRegister - register ioApic driver
*
* This routine registers the ioApic driver with the
* vxBus subsystem.
*
* RETURNS: N/A
*
* ERRNO
*/

void vxbIoApicIntrDrvRegister(void)
    {

    /* call the vxBus routine to register the ioApic driver */

    /* the return value of vxbDevRegister() is not used */

    (void) vxbDevRegister(&ioApicIntrDrvRegistration);
    }

/******************************************************************************
*
* vxbIoApicIntrInstProbe - check for valid IO Apics to instantiate.
*
*/
LOCAL BOOL vxbIoApicIntrInstProbe 
    (
    VXB_DEVICE_ID pInst
    )
    {
    UINT32 mpApicNioApic;

    /* check if this will be a valid instance for an IO Apic */

    (void) vxbDevMethodRun( DEVMETHOD_CALL(mpApicNioApicGet),
                            (void *)&mpApicNioApic );

    /* if no more valid IO Apics to instantiate, return FALSE */

    if (pInst->unitNumber > (mpApicNioApic-1))
      return(FALSE);
    else
      return(TRUE);
}

/******************************************************************************
*
* vxbIoApicIntrInstInit - initialize vxbIoApicIntr device
*
* This is the vxbIoApicIntr initialization routine.
*
* NOTE:
*
* This routine is called early during system initialization, and
* *MUST NOT* make calls to OS facilities such as memory allocation
* and I/O.
*
* RETURNS: N/A
*
* ERRNO
*/

LOCAL void vxbIoApicIntrInstInit
    (
    VXB_DEVICE_ID pInst
    )
    {
    struct ioApicIntrData *pIoApicIntrData = NULL;
    HCF_DEVICE            *pHcf = NULL;
    UINT32                 mpApicNioApic;
    UINT32                *mpApicAddrTable;
    void                  *dummy;


    /* get the HCF device from the instance id */

    pHcf = hcfDeviceGet (pInst);

    /* if pHcf is NULL, no device is present in hwconf.c */

    if (pHcf == NULL)
       return;

    /* allocate memory for the ioApicIntr data */

    pIoApicIntrData = (struct ioApicIntrData *)
                      hwMemAlloc(sizeof(struct ioApicIntrData));

    /* if no memory available, return */

    if (pIoApicIntrData == NULL)
      return;

    /* check if this will be a valid instance for an IO Apic */

    (void) vxbDevMethodRun( DEVMETHOD_CALL(mpApicNioApicGet),
                            (void *)&mpApicNioApic );

    if ( pInst->unitNumber == 0 )
      ioApicNumIoApics = mpApicNioApic;

    /*
     * if necessary update the data structure here...
     * by retrieving data from hwconf.c
     */

    /* get the defined IO APIC addr from what was discovered in hardware */

    (void) vxbDevMethodRun( DEVMETHOD_CALL(mpApicAddrTableGet),
                            (void *)&mpApicAddrTable );

    pInst->pRegBase[0] = (void *)((ULONG)mpApicAddrTable[pInst->unitNumber]);
    pInst->pRegBasePhys[0] = pInst->pRegBase[0];

    /*
     * Override the Reg Base Flag to be VXB_REG_MEM to succeed memory
     * mapping (in LP64).
     */

    pInst->regBaseFlags[0] = VXB_REG_MEM;

    /* set the size of the Iocal APIC register space */

    pInst->regBaseSize[0] = IOAPIC_LENGTH;

    /* map the registers */

    (void) vxbRegMap (pInst, 0, &dummy);

    /* set the virtual address */

    pIoApicIntrData->ioApicBase = (char *) pInst->pRegBase[0];

    /* store the instance pointer */

    pIoApicIntrData->pInst = pInst;

    /* update the driver control data pointer */

    pInst->pDrvCtrl = pIoApicIntrData;

    /* initialize the IO APIC(s) */

    (void) vxbIoApicIntrInit (pInst);
    vxbIoApicIntrIntAck(pInst);

    pInst->pMethods = &vxbIoApicIntr_methods[0];

    /* save ID for later use, if necessary */
    if ( pInst->unitNumber == 0 )
        vxbIoApicIntrUnit0Id = pInst;

    /* for (;;); */
    return;
    }

/******************************************************************************
*
* vxbIoApicIntrInstInit2 - initialize vxbIoApicIntr device
*
* This routine creates a reverse lookup table:
* from SMP CPU number to ApicID
*/

LOCAL void vxbIoApicIntrInstInit2
    (
    VXB_DEVICE_ID pInst
    )
    {
    struct ioApicIntrData *pIoApicIntrData;
    int                   i;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
	(pInst->pDrvCtrl == NULL))
      return;

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

    pIoApicIntrData->mpApicHwAddrTable = (UINT8 *)hwMemAlloc(256);
    if ( pIoApicIntrData->mpApicHwAddrTable == NULL )
        return;

    for ( i = 255 ; i >= 0; i-- )
        {
	if ( mpCpuIndexTable[i] < VX_MAX_SMP_CPUS )
	    pIoApicIntrData->mpApicHwAddrTable[mpCpuIndexTable[i]] = (UINT8) i;
	}
    }

/*******************************************************************************
*
* vxbIoApicIntrInit - initialize the IO APIC or xAPIC
*
* This routine initializes the IO APIC or xAPIC.
*
*/

LOCAL STATUS vxbIoApicIntrInit 
    (
    VXB_DEVICE_ID pInst
    )
    {
    INT32  ix;
    UINT32 lower32, upper32, vector, offset;
    UINT32 loApicId;
    UINT8  irq, ioApicId, ioApicIndex;
    UINT8 start_isa_vector;
    UINT8 start_pci_vector;
    UINT8 start_vector;
    UINT8 * mpApicLogicalTable;
    MP_INTERRUPT *pMpApicInterruptTable;

    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
	(pInst->pDrvCtrl == NULL))
      return (ERROR);

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

    /* get the IOAPIC DATA area */

    pIoApicIntrData->ioApicData = (UINT32 *) (pIoApicIntrData->ioApicBase +
					     IOAPIC_DATA);

    /* get the IOAPIC ID */

    /*
     * Typically assume that the IOAPIC_ID register has been
     * set by the BIOS, however in some cases this isn't what
     * actually happens. If the register has not been initialized,
     * then we must do it ourselves based on system configuration
     * data, either from MPS or ACPI.
     */

    (void) vxbDevMethodRun (DEVMETHOD_CALL(mpApicLogicalTableGet),
                            (void *)&mpApicLogicalTable);

    pIoApicIntrData->ioApicId = mpApicLogicalTable[pInst->unitNumber] << 24;

    /*
     * Look up the ioApic number in the mpApicLogicalTable based on
     * destApicId.
     */

    for (ioApicIndex = 0; ioApicIndex <= ioApicNumIoApics; ioApicIndex++)
        {
        if (mpApicLogicalTable[ioApicIndex] == (pIoApicIntrData->ioApicId >> 24))
            {
            break;
            }
        }

    /* No matching destApicId found in mpApicLogicalTable */

    if (ioApicIndex > ioApicNumIoApics)
        {
        return (ERROR);
        }

    pIoApicIntrData->ioApicIndex = ioApicIndex;

    /* get the IOAPIC version */

    pIoApicIntrData->ioApicVersion =
    vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
		      pIoApicIntrData->ioApicData,
		      IOAPIC_VERS);

    /* get the number of redirection table entries */

    pIoApicIntrData->ioApicRedNumEntries = (UINT8)
       (((pIoApicIntrData->ioApicVersion & IOAPIC_MRE_MASK) >> 16) + 1);

    /* allocate memory for saving the interrupt mask bit */

    pIoApicIntrData->ioApicIntTable = (UINT32 *) 
	   hwMemAlloc((pIoApicIntrData->ioApicRedNumEntries) * sizeof (UINT32));

    if (pIoApicIntrData->ioApicIntTable == NULL)
        return (ERROR);

/*
 * Under SYMMETRIC_IO_MODE sysInumTbl is created dynamically here under the 
 * IO Apic driver initialization. It is created once by the first IO Apic
 * instantiation. sysInumTbl is used not only by the IO Apic driver but also
 * by the PCI and AHCI drivers to show the correspondence between the irq,
 * (represented as the index into the sysInumTbl), and the vector value assigned
 * to that irq (value stored at that offset in array).
 *
 * Multiple IO Apics are used in high-end severs to distribute irq load.
 * At this time there may be as many as 8 IO Apics in a system. The most common
 * number of inputs on a single IO Apic is 24, but they also designed for
 * 16/32/or 64 inputs.
 *
 * The common case iused here is a 1:1 irq<=>pin mapping.
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
 * CPU is capable of. The first IO Apic will handle the ISA/EISA interrupts. The
 * IO Apics will assign a priority to the interrupt based on the vector given. 
 * Currently only the 4 top bits of the vector number are used to distinguish
 * priority, so we group 2 interrupts per level where we can.
 *
 * We map only the interrupts discovered by the mpApic driver when it set up the 
 * interrupt routing table. These should represent all the known interrupt 
 * discovered by BIOS or ACPI during startup configuration of the hardware.
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

    if (pInst->unitNumber == 0)
    {
    ioApicRedNumEntries = pIoApicIntrData->ioApicRedNumEntries;

    if (sysInumTbl == NULL)
       {
       /* reset to actual size found */

       sysInumTblNumEnt =
          (ioApicNumIoApics * pIoApicIntrData->ioApicRedNumEntries);

       /* allocate memory for sysInumTbl */

       sysInumTbl =
          (UINT8 *) hwMemAlloc(sysInumTblNumEnt * sizeof (UINT8));
       }  

    pMpApicInterruptTable = (MP_INTERRUPT *) glbMpApicInterruptTable;

    start_isa_vector = IOAPIC_VECTOR_INT_NUM_IRQ0;
    start_pci_vector = IOAPIC_VECTOR_INT_NUM_PIRQA;
    start_vector = IOAPIC_VECTOR_INT_NUM_STRT;
    for (ix = 0; ix < glbMpApicNioint; ix++)
       {
       /*
        * Look up the Io apic number in in the mpApicLogicalTable based on
        * destApicId. This value is used to map the irq to the Io Apic pin
        * number in the sysInumTbl.
        */

       for (ioApicIndex = 0; ioApicIndex <= ioApicNumIoApics; ioApicIndex++)
           {
           if (mpApicLogicalTable[ioApicIndex] == pMpApicInterruptTable->destApicId)
               {
               break;
               }
           }

       /* No matching destApicId found in mpApicLogicalTable */

       if (ioApicIndex > ioApicNumIoApics)
           {
    /*        return (ERROR);*/
           }

       irq = (UINT8)((ioApicIndex * pIoApicIntrData->ioApicRedNumEntries) +
                      pMpApicInterruptTable->destApicIntIn);

       /* First IO APIC gets ISA */

       if (irq < 16)
         {
	  if (sysInumTbl[irq] == 0)
	    {
            sysInumTbl[irq] =
               (UINT8) (IOAPIC_VECTOR_INT_NUM_IRQ0 - ((irq) * IOAPIC_ISA_VECTOR_OFFSET));
	    }
	 }
       else if (irq < 24)
         {
	  if (sysInumTbl[irq] == 0)
	    {
            sysInumTbl[irq] = start_pci_vector;
            start_pci_vector = (UINT8) (start_pci_vector -
					IOAPIC_PCI_VECTOR_OFFSET);
	    }
	 }
       else /* irq => 24, handle multiple IO APICs */
         {
	  if (sysInumTbl[irq] == 0)
	    {
            sysInumTbl[irq] = (UINT8) start_vector;
            start_vector = (UINT8)(start_vector - 1);

	    /* skip vectors used for ISA irqs */

	    if ((start_vector & 0x0f) == 0x0c) 
	      {
              start_vector = (UINT8)(start_vector - 1);
	      }
	    if ((start_vector & 0x0f) == 0x04)
	      {
              start_vector = (UINT8)(start_vector - 1);
	      }
	    }
         }
       pMpApicInterruptTable ++;
       }
    }

    /* If physical CPU 0 */

    if (vxCpuPhysIndexGet() == 0)
        {

        /* set the IOAPIC bus arbitration ID */

        vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                          pIoApicIntrData->ioApicData,
                          IOAPIC_ID,
                          pIoApicIntrData->ioApicId);

        /*
         * Boot Config register does not exist in I/O APIC (version 0x1X).
         * It may or may not exist in I/O xAPIC (version 0x2X). Some Intel
         * chipsets with I/O xAPIC don't have Boot Config reg.
         *
         * Attempt to set Interrupt Delivery Mechanism to Front Side Bus
         * (ie. MSI capable), or APIC Serial Bus. Some I/O xAPIC allow this
         * bit to take effect. Some I/O xAPIC allow the bit to be written
         * (for software compat reason), but it has no effect as the mode is
         * hardwired to FSB delivery.
         */

        if (((pIoApicIntrData->ioApicVersion & IOAPIC_VERSION) >= 0x20))
            {
            if (sysProcessor >= X86CPU_PENTIUM4)
                {

                /* Pentium4 and later use FSB for interrupt delivery */

                vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                                  pIoApicIntrData->ioApicData,
                                  IOAPIC_BOOT,
                                  IOAPIC_DT_FS);
                }
            else
                {

                /* Pentium up to and including P6 use APIC bus */

                vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                                  pIoApicIntrData->ioApicData,
                                  IOAPIC_BOOT,
                                  IOAPIC_DT_APIC);
                }
            }

        /*
         * initialize the redirection table for the first ioapic with the
         * default values
         */

        loApicId = vxCpuLoApicId ();
        upper32 = (loApicId << 24);

        ioApicId = (UINT8) (pIoApicIntrData->ioApicId >> 24);
        for (ix = 0; ix < pIoApicIntrData->ioApicRedNumEntries; ix++)
           {
           if (ioApicId == glbMpApicIoBaseId)
             {
             vector = (IOAPIC_INT_MASK | IOAPIC_PHYSICAL |
                       (IOAPIC_VEC_MASK & sysInumTbl[ix]));

             if (ix < 16)
               lower32 = (ioApicRed0_15 | vector);
             else
               lower32 = (ioApicRed16_23 | vector);
	       }
	    else
           {
             vector = (IOAPIC_INT_MASK | IOAPIC_PHYSICAL |
              (IOAPIC_VEC_MASK & sysInumTbl[ix +
              (pIoApicIntrData->ioApicIndex * pIoApicIntrData->ioApicRedNumEntries)]));

             lower32 = (ioApicRed16_23 | vector);
	       }

           offset = IOAPIC_REDTBL + (ix << 1);

           vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                             pIoApicIntrData->ioApicData,
                             offset + 1,
                             upper32);
           vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                             pIoApicIntrData->ioApicData,
                             offset,
                             lower32);

           pIoApicIntrData->ioApicIntTable[ix] = lower32;
           }
        }
        else
        {

        /* AMP or SMP M-N boot CPU */

        /* Fill out ioApicIntTable */

        for (ix = 0; ix < pIoApicIntrData->ioApicRedNumEntries; ix++)
            {
            offset = IOAPIC_REDTBL + (ix << 1);
            upper32 = vxbIoApicIntrGet(pIoApicIntrData->ioApicBase,
                pIoApicIntrData->ioApicData, offset + 1);
            lower32 = vxbIoApicIntrGet(pIoApicIntrData->ioApicBase,
                pIoApicIntrData->ioApicData, offset);
            pIoApicIntrData->ioApicIntTable[ix] = lower32;
            }
        }

    /* increment IO Apic driver init count */

    ioApicIntrDrvInitializedCount +=1;

    /* for (;;); */
    return (OK);
    }

/*******************************************************************************
*
* vxbIoApicIntrIntEnable - enable a specified APIC interrupt input line
*
* This routine enables a specified APIC interrupt input line.
*
* RETURNS: OK or ERROR if a NULL pointer is passed.
*
* SEE ALSO: vxbIoApicIntrIntDisable()
*
* \ARGSUSED0
*/

LOCAL STATUS vxbIoApicIntrIntEnable
    (
    VXB_DEVICE_ID pInst,
    INT32 *irqNo         /* INTIN number to enable */
    )
    {
    UINT32 offset;
    UINT32 lower32;
    struct ioApicIntrData *pIoApicIntrData;
    UINT8  ioApicIdOffset;
    UINT32 irqNum;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
	(pInst->pDrvCtrl == NULL) ||
        irqNo == NULL)
      return (ERROR);

    /* retrieve the data */

    irqNum = *irqNo;
    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);
    ioApicIdOffset = pIoApicIntrData->ioApicIndex;

    if ((irqNum >= (ioApicIdOffset * (pIoApicIntrData->ioApicRedNumEntries))) &&
        (irqNum < ((ioApicIdOffset + 1) * (pIoApicIntrData->ioApicRedNumEntries))))
        {

        /* retrieve the data */

        irqNum -= ((pIoApicIntrData->ioApicRedNumEntries) * ioApicIdOffset);

        offset = IOAPIC_REDTBL + (irqNum << 1);

        lower32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                    pIoApicIntrData->ioApicData,
                                    offset);

        /* enable the interrupt */

        pIoApicIntrData->ioApicIntTable[irqNum] = (lower32 & ~IOAPIC_INT_MASK);

        vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                          pIoApicIntrData->ioApicData,
                          offset,
                          lower32 & ~IOAPIC_INT_MASK);
        }

    return (OK); /* do not modify wrong APIC if chained... */
    }

/*******************************************************************************
*
* vxbIoApicIntrIntDisable - disable a specified APIC interrupt input line
*
* This routine disables a specified APIC interrupt input line.
*
* RETURNS: OK or ERROR if a NULL pointer is passed.
*
* SEE ALSO: vxbIoApicIntrIntEnable()
*
* \ARGSUSED0
*/

LOCAL STATUS vxbIoApicIntrIntDisable
    (
    VXB_DEVICE_ID pInst,
    INT32 *irqNo         /* INTIN number to disable */
    )
    {
    UINT32 offset;
    UINT32 lower32;
    struct ioApicIntrData *pIoApicIntrData;
    UINT8  ioApicIdOffset;
    UINT32 irqNum;
    UINT32 isLevel;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
        (pInst->pDrvCtrl == NULL) ||
        irqNo == NULL)
      return (ERROR);

    /* retrieve the data */

    irqNum = *irqNo;
    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);
    ioApicIdOffset = pIoApicIntrData->ioApicIndex;

    if ((irqNum >= (ioApicIdOffset * (pIoApicIntrData->ioApicRedNumEntries))) &&
	(irqNum < ((ioApicIdOffset + 1) * (pIoApicIntrData->ioApicRedNumEntries))))
        {
        /* retrieve the data */

        irqNum -= ((pIoApicIntrData->ioApicRedNumEntries) * ioApicIdOffset);

        offset = IOAPIC_REDTBL + (irqNum << 1);

        lower32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                    pIoApicIntrData->ioApicData,
                                    offset);

        pIoApicIntrData->ioApicIntTable[irqNum] = (lower32 | IOAPIC_INT_MASK);

      /*
       * we need to set the trigger mode to edge first, to workaround following
       * situation: when the mask bit is set after the interrupt message has
       * been accepted by a local APIC unit but before the interrupt is
       * dispensed to the processor. In that case, the I/O APIC will always
       * waiting for EOI even re-enable the interrupt(Remote IRR is always set).
       * We switch to edge first, and then the Remote IRR will be cleared, and
       * then we switch back to level if original mode is level.
       */

      isLevel = lower32 & IOAPIC_LEVEL;
      lower32 &= ~IOAPIC_LEVEL;           /* set to edge first */

      /* disable the interrupt */

      offset = IOAPIC_REDTBL + (irqNum << 1);

      vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                        pIoApicIntrData->ioApicData,
                        offset,
                        lower32 | IOAPIC_INT_MASK);

      /* return to original */

      if (isLevel)
          {
          lower32 |= isLevel;
          vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                            pIoApicIntrData->ioApicData,
                            offset,
                            lower32 | IOAPIC_INT_MASK);
          }
        }

    return (OK); /* do not modify wrong APIC if chained... */
    }

/*******************************************************************************
*
* vxbIoApicIntrIntLock - lock out all IO APIC interrupts
*
* This routine saves the mask and locks out all IO APIC interrupts.
* It should be called in the interrupt disable state(IF bit is 0).
*
* RETURNS: N/A
*
* SEE ALSO: vxbIoApicIntrIntUnlock()
*
* \ARGSUSED0
*/

LOCAL STATUS vxbIoApicIntrIntLock 
    (
    VXB_DEVICE_ID pInst,
    int *dummy
    )
    {
    INT32 ix;
    UINT32 offset;
    UINT32 upper32;
    UINT32 lower32;

    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
	(pInst->pDrvCtrl == NULL))
      return (ERROR);

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

       for (ix = 0; ix < (pIoApicIntrData->ioApicRedNumEntries); ix++)
          {
           offset = IOAPIC_REDTBL + (ix << 1);

           upper32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                       pIoApicIntrData->ioApicData,
                                       offset + 1);
           lower32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                       pIoApicIntrData->ioApicData,
                                       offset);

           /* lock if the destination is unlocked */

           if ( (lower32 & IOAPIC_INT_MASK) == 0)
             {
             offset = IOAPIC_REDTBL + (ix << 1);

             vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                               pIoApicIntrData->ioApicData,
                               offset + 1,
                               upper32);
             vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                               pIoApicIntrData->ioApicData,
                               offset,
                               lower32 | IOAPIC_INT_MASK);

             pIoApicIntrData->ioApicIntTable[ix] = lower32; /* save the old state */
             }
          }
    return (OK);
    }

/*******************************************************************************
*
* vxbIoApicIntrIntUnlock - unlock the IO APIC interrupts
*
* This routine restores the mask and unlocks the IO APIC interrupts
* It should be called in the interrupt disable state(IF bit is 0).
*
* RETURNS: N/A
*
* SEE ALSO: vxbIoApicIntrIntLock()
*
* \ARGSUSED0
*/

LOCAL STATUS vxbIoApicIntrIntUnlock 
    (
    VXB_DEVICE_ID pInst,
    int *dummy
    )
    {
    INT32 ix;
    UINT32 offset;
    UINT32 upper32;
    UINT32 lower32;

    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
	(pInst->pDrvCtrl == NULL))
      return (ERROR);

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

       for (ix = 0; ix < (pIoApicIntrData->ioApicRedNumEntries); ix++)
          {
           offset = IOAPIC_REDTBL + (ix << 1);

           upper32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                         pIoApicIntrData->ioApicData,
                                         offset + 1);
           lower32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                         pIoApicIntrData->ioApicData,
                                         offset);

          /* unlock if the destination is locked */

          if ((pIoApicIntrData->ioApicIntTable[ix] & IOAPIC_INT_MASK) == 0)
            {
            offset = IOAPIC_REDTBL + (ix << 1);

            vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                              pIoApicIntrData->ioApicData,
                              offset + 1,
                              upper32);
            vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                              pIoApicIntrData->ioApicData,
                              offset,
                              lower32 & ~IOAPIC_INT_MASK);

            pIoApicIntrData->ioApicIntTable[ix] = lower32; /* save the old state */
            }
          }
    return (OK);
    }

/*******************************************************************************
*
* vxbIoApicIntrGet - get a value from the IO APIC register.
*
* This routine gets a value from the IO APIC register.
*
* RETURNS: A value of the IO APIC register.
*/

LOCAL UINT32 vxbIoApicIntrGet
    (
    char *	index,		/* IO register select (index) */
    UINT32 *	data,		/* IO window (data) */
    UINT32	offset		/* offset to the register */
    )
    {
    UINT32 value;
    INT32 oldLevel;

    oldLevel = intCpuLock ();                     /* LOCK INTERRUPT */

    /*
     * for vxWorks guest write all offset bits so redirection registers for
     * 240 interrupt sources is supported
     */

    *((volatile UINT32*)index) = (UINT32) offset; /* select the register  */
    value =  *((volatile UINT32*)data);           /* must be a 32bit read */

    intCpuUnlock (oldLevel);                      /* UNLOCK */

    return (value);
    }

/*******************************************************************************
*
* vxbIoApicIntrSet - set a value into the IO APIC register.
*
* This routine sets a value into the IO APIC register.
*
* RETURNS: N/A
*/

LOCAL void vxbIoApicIntrSet
    (
    char *	index,		/* IO register select (index) */
    UINT32 * data,		/* IO window (data) */
    UINT32	offset,		/* offset to the register */
    UINT32	value		/* value to set the register */
    )
    {
    INT32 oldLevel;

    oldLevel = intCpuLock ();           /* LOCK INTERRUPT */

    /* for vxWorks guest write all offset bits so redirection registers for
     * 240 interrupt sources is supported
     */

    *((volatile UINT32*)index) = (UINT32) offset; /* select the register */
    *((volatile UINT32*)data) = value;          /* must be a 32bit write */

    intCpuUnlock (oldLevel);            /* UNLOCK */
    }

/*******************************************************************************
*
* vxbIoApicIntrRedGet - get a specified entry in the Redirection Table
*
* This routine gets a specified entry in the Redirection Table
*
* RETURNS: OK, or ERROR if intNum is out of range
*/

LOCAL STATUS vxbIoApicIntrRedGet
    (
    VXB_DEVICE_ID pInst,
    INT32 irq,                  /* index of the table */
    UINT32 * pUpper32,          /* upper 32 bit (MS INT32) */
    UINT32 * pLower32           /* lower 32 bit (LS INT32) */
    )
    {
    UINT32 offset;
    UINT8 ioApicIdOffset;

    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
	(pInst->pDrvCtrl == NULL))
      return (OK);

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);
    ioApicIdOffset = pIoApicIntrData->ioApicIndex;

    if ((irq >= (ioApicIdOffset * (pIoApicIntrData->ioApicRedNumEntries))) &&
	(irq < ((ioApicIdOffset + 1) * (pIoApicIntrData->ioApicRedNumEntries))))
      {
      irq -= ((pIoApicIntrData->ioApicRedNumEntries) * ioApicIdOffset);

      offset = IOAPIC_REDTBL + (irq << 1);

      *pUpper32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                    pIoApicIntrData->ioApicData,
                                    offset + 1);
      *pLower32 = vxbIoApicIntrGet (pIoApicIntrData->ioApicBase,
                                    pIoApicIntrData->ioApicData,
                                    offset);

      return (OK);
      }

    return (OK); /* do not modify wrong APIC if chained... */
    }

/*******************************************************************************
*
* vxbIoApicIntrIdGet - get the I/O APIC index of this device
*
* This routine get the I/O APIC index of this device
*
* RETURNS: OK, or ERROR if intNum is out of range
*/

LOCAL STATUS vxbIoApicIntrIdGet
    (
    VXB_DEVICE_ID pInst,
    UINT32 * ioApicId
    )
    {
    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) || (ioApicId == NULL) || (pInst->pDrvCtrl == NULL))
        return (ERROR);

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);
    *ioApicId = (pIoApicIntrData->ioApicId >> 24);

    return (OK);
    }

/*******************************************************************************
*
* vxbIoApicIntrRedSet - set a specified entry in the Redirection Table
*
* This routine sets a specified entry in the Redirection Table
*
* RETURNS: OK, or ERROR if intNum is out of range
*/

LOCAL STATUS vxbIoApicIntrRedSet
    (
    VXB_DEVICE_ID pInst,
    INT32 irq,                  /* index of the table */
    UINT32 upper32,             /* upper 32 bit (MS INT32) */
    UINT32 lower32              /* lower 32 bit (LS INT32) */
    )
    {
    UINT32 offset;
    UINT8 ioApicIdOffset;

    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
	(pInst->pDrvCtrl == NULL))
      return (OK);

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);
    ioApicIdOffset = pIoApicIntrData->ioApicIndex;

    if ((irq >= (ioApicIdOffset * (pIoApicIntrData->ioApicRedNumEntries))) &&
        (irq < ((ioApicIdOffset + 1) * (pIoApicIntrData->ioApicRedNumEntries))))
      {
      sysInumTbl[irq] = (UINT8) (0x000000ff & lower32);

      irq -= ((pIoApicIntrData->ioApicRedNumEntries) * ioApicIdOffset);

      offset = IOAPIC_REDTBL + (irq << 1);

      vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                        pIoApicIntrData->ioApicData,
                        offset + 1,
                        upper32);
      vxbIoApicIntrSet (pIoApicIntrData->ioApicBase,
                        pIoApicIntrData->ioApicData,
                        offset,
                        lower32);  

      pIoApicIntrData->ioApicIntTable[irq] = lower32;

      return (OK);
      }

    return (OK);
    }

/*******************************************************************************
*
* vxbIoApicIntrVersionGet - method to retrieve IO Apic version
*
* This function is the driver method used to retrieve the IO Apic version.
*
* RETURNS: OK or ERROR if functionality is not retrieved.
*
* ERRNO
*/

LOCAL STATUS vxbIoApicIntrVersionGet
    (
    VXB_DEVICE_ID  pInst,
    UINT32 *ioApicVersion
    )
    {
    struct ioApicIntrData *pIoApicIntrData;

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

    *ioApicVersion =
           (UINT32)(pIoApicIntrData->ioApicVersion);

    return OK;
    }

/*******************************************************************************
*
* vxbIoApicIntrBaseGet - method to retrieve IO APIC address
*
* This function is the driver method used to retrieve the IO APIC
* address.
*
* RETURNS: OK or ERROR if functionality is not retrieved.
*
* ERRNO
*/

LOCAL STATUS vxbIoApicIntrBaseGet
    (
    VXB_DEVICE_ID  pInst,
    char ** ioApicBase
    )
    {
    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
        (ioApicBase == NULL) ||
        (pInst->pDrvCtrl == NULL))
      return ERROR;

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

    *ioApicBase = pIoApicIntrData->ioApicBase;

    return OK;
    }

/*******************************************************************************
*
* vxbIoApicRedNumEntriesGet - method to retrieve the number of redirection 
*                             table entries
*
* This function is the driver method used to retrieve the number of redirection 
* table entries.
*
* RETURNS: OK or ERROR if functionality is not retrieved.
*
* ERRNO
*/

LOCAL STATUS vxbIoApicRedNumEntriesGet
    (
    VXB_DEVICE_ID  pInst,
    UINT8 *ioApicRedNumEntries
    )
    {
    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
        (ioApicRedNumEntries == NULL) ||
        (pInst->pDrvCtrl == NULL))
      return ERROR;

    /* get the number of redirection table entries */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

    *ioApicRedNumEntries = (UINT8)(pIoApicIntrData->ioApicRedNumEntries);

    return OK;
    }

/******************************************************************************
*
* vxbIoApicIntrCpuReroute - reroute interrupts to specified CPU
*
* Reroute interrupts that are configured in hwconf.c for a CPU other
* than the default CPU to that CPU.  Also set enabledCpus in the
* driver control structure with this CPUs ID so it is marked as
* active
*
* RETURNS: OK if operational successful else ERROR
*
* ERRNO
*/

LOCAL STATUS vxbIoApicIntrCpuReroute
    (
    VXB_DEVICE_ID       pDev,
    void *              destCpu
    )
    {
    return (OK);
    }

/******************************************************************************
*
* vxbIoApicIntrIntByIrqReroute - reroute interrupts based on IRQ
*
* Reroute the specified IRQ to the specified CPU
*
* RETURNS: OK if operational successful else ERROR
*
* ERRNO
*/

LOCAL STATUS vxbIoApicIntrIntByIrqReroute
    (
    VXB_DEVICE_ID	intCtlrId,
    INT32		irq,
    int	                destCpuIndex
    )
    { 
    struct ioApicIntrData *	pDrvCtrl;
    INT32			apicId;
    UINT32			upper32;
    UINT32			upper32new;
    INT32			oldLevel;
    UINT32			offset;

    /* if parameters are invalid, return ERROR */

    if (intCtlrId == NULL)
      return (ERROR);

    pDrvCtrl = intCtlrId->pDrvCtrl;

    offset = IOAPIC_REDTBL + (irq * 2);

    /* convert from CPU index to Apic ID */

    apicId = pDrvCtrl->mpApicHwAddrTable[destCpuIndex];

    /* lock interrupts */

    oldLevel = intCpuLock ();              /* LOCK INTERRUPTS */

    if ((vxbIoApicIntrIntDisable (intCtlrId, &irq)) != OK)
        {
        intCpuUnlock (oldLevel);               /* UNLOCK INTERRUPTS */
        return ERROR;
        }

    vxbUsDelay(50);

    /* set the loApicId */

    upper32 = vxbIoApicIntrGet (pDrvCtrl->ioApicBase,
		                pDrvCtrl->ioApicData,
				offset + 1);
    upper32 = (upper32 & ~IOAPIC_DESTINATION) | (apicId << 24);

    vxbIoApicIntrSet (pDrvCtrl->ioApicBase,
		      pDrvCtrl->ioApicData,
		      offset + 1,
		      upper32);

    vxbUsDelay(50);

    upper32new = vxbIoApicIntrGet (pDrvCtrl->ioApicBase,
		                pDrvCtrl->ioApicData,
				offset + 1);

    /* unlock interrupts */

    if ((vxbIoApicIntrIntEnable (intCtlrId, &irq)) != OK)
        {
        intCpuUnlock (oldLevel);               /* UNLOCK INTERRUPTS */
        return ERROR;
        }

    intCpuUnlock (oldLevel);               /* UNLOCK INTERRUPTS */

    return (OK);
    }

/******************************************************************************
*
* vxbIoApicIntrIntReroute - reroute interrupt to specified CPU
*
* Reroute device interrupt to requested CPU.  note that the cpu is specified
* in a cpuset_t type.  this would allow for multiple cpus to be specified
* but only 1 cpu being specified is valid.  specifying more than 1 cpu
* will return an ERROR.
*
* RETURNS: OK if operational successful else ERROR
*
* ERRNO
*/

LOCAL STATUS vxbIoApicIntrIntReroute
    (
    VXB_DEVICE_ID       instId,
    int                 index,
    cpuset_t            routeToCpu
    )
    {
    cpuset_t           	 	cpuCopy;
    int				destCpu = -1;
    INT32			irq;
    VXB_DEVICE_ID		intCtlrId = NULL;
    STATUS                      stat;
    int                         i;
    VXB_INTR_ENTRY            * pIntInfo;

    /* this version uses vectors, not the intCtlr design.  So jump through hoops */

    /* if parameters are invalid, return ERROR */

    if (instId == NULL)
      return (ERROR);

    pIntInfo = instId->pIntrInfo;

    /* LoAPIC process MSI */

    if (VXB_IS_MSI_INT(pIntInfo->intrFlag) == TRUE)
        return ERROR;

    /* verify that exactly one CPU is specified */

    if ( CPUSET_ISZERO(routeToCpu) )
        return(ERROR);

    for ( i = 0 ; i < VX_MAX_SMP_CPUS ; i++ )
        {
	if (i >= vxCpuConfiguredGet())
	    return (ERROR);

	if ( CPUSET_ISSET(routeToCpu, i) )
	    {
	    cpuCopy = routeToCpu;
	    CPUSET_CLR(cpuCopy, i);
	    if ( ! CPUSET_ISZERO(cpuCopy) )
		return(ERROR);
	    destCpu = i;
	    break;
	    }
	}

    if (destCpu < 0)
        return (ERROR);
                
    /* find our own instId */

    stat = plbIntrGet(instId, index, &intCtlrId, &irq);
    if ( stat != OK || intCtlrId == NULL )
        {
        IMPORT VOIDFUNCPTR * vxbIntVectorGet (VXB_DEVICE_ID, int);
        IMPORT int    sysPciIvecToIrq(_Vx_usr_arg_t vector);

        /* requires fix WIND00123216 */

        VOIDFUNCPTR * ivec = vxbIntVectorGet (instId, index);

        if (ivec == NULL)
            return (ERROR);
                
        irq = sysPciIvecToIrq ((_Vx_usr_arg_t) ivec);
        if (irq == ERROR)
        	return (ERROR);

        intCtlrId = vxbIoApicIntrUnit0Id;

        }

    return(vxbIoApicIntrIntByIrqReroute(intCtlrId,irq,destCpu));
    }

#ifdef INCLUDE_SHOW_ROUTINES

void vxbIoApicIntrpDrvCtrlShow
    (
    VXB_DEVICE_ID  pInst,
    int            verboseLevel
    );

void vxbIoApicIntrpDrvCtrlShow
    (
    VXB_DEVICE_ID  pInst,
    int            verboseLevel
    )
    {
    struct ioApicIntrData *pDrvCtrl = pInst->pDrvCtrl;
    (void) printf("instID = %p\n",pInst);
    (void) printf("isrHandle @ %p\n", &pDrvCtrl->isrHandle);
    (void) printf("mpApicHwAddrTable @ %p\n", pDrvCtrl->mpApicHwAddrTable);
    (void) printf("ioApicBase = %p\n", pDrvCtrl->ioApicBase);
    (void) printf("ioApicData = %p\n", pDrvCtrl->ioApicData);
    (void) printf("ioApicId = 0x%08x\n", pDrvCtrl->ioApicId);
    (void) printf("ioApicVersion = 0x%08x\n", pDrvCtrl->ioApicVersion);
    (void) printf("ioApicRedNumEntries = 0x%02x\n", pDrvCtrl->ioApicRedNumEntries);
    (void) printf("ioApicIntTable @ %p\n", pDrvCtrl->ioApicIntTable);
    };

/*******************************************************************************
*
* vxbIoApicIntrDataShow - show IO APIC register data acquired by vxBus
*
* This routine shows IO APIC register data acquired by vxBus.
*
* RETURNS: N/A
*/

void vxbIoApicIntrDataShow
    (
    VXB_DEVICE_ID  pInst,
    int 	   verboseLevel
    )
    {
    int i;
    struct ioApicIntrData *pIoApicIntrData;

    /* if parameters are invalid, return ERROR */

    if ((pInst == NULL) ||
        (pInst->pDrvCtrl == NULL))
      return;

    (void) printf("        %s unit %d on %s @ %p with busInfo %p\n",
	    pInst->pName, pInst->unitNumber,
	    vxbBusTypeString(pInst->busID), pInst,
	    pInst->u.pSubordinateBus);

    if ( verboseLevel == 0 )
	return;

    /* retrieve the data */

    pIoApicIntrData = (struct ioApicIntrData *)(pInst->pDrvCtrl);

    (void) printf ("\nIO APIC Configuration Data acquired by vxBus\n\n");

    (void) printf ("\n  IO APIC Info :                       \n");

    (void) printf ("  ioApicBase              = %p\n",
                    pIoApicIntrData->ioApicBase);
    (void) printf ("  ioApicData              = %p\n",
                    pIoApicIntrData->ioApicData);
    (void) printf ("  ioApicId                = 0x%08x\n",
                    pIoApicIntrData->ioApicId);
    (void) printf ("  ioApicVersion           = 0x%08x\n",
                    pIoApicIntrData->ioApicVersion);
    (void) printf ("  ioApicRedNumEntries     = 0x%08x\n",
                    pIoApicIntrData->ioApicRedNumEntries);

    (void) printf ("\n  ioApicIntTable        = %p\n\n",
		    pIoApicIntrData->ioApicIntTable);

    for (i=0; i<pIoApicIntrData->ioApicRedNumEntries; i++)
       (void) printf ("    ioApicIntTable[%d]        = 0x%08x\n",
		    i,
		    pIoApicIntrData->ioApicIntTable[i]);

    }
#endif /* INCLUDE_SHOW_ROUTINES */

/*******************************************************************************
* vxbIoApicIntrShowAll - show All IO APIC registers
*
* temporary -since it depends on MP table being present
*
* This routine shows all IO APIC registers
*
* RETURNS: N/A
*/

void vxbIoApicIntrShowAll (void)
    {
    int ix,id;
    int version;
    char * ioApicBase;
    UINT32 * ioApicData;
    UINT32 mpApicNioApic;
    UINT32 *mpApicAddrTable;

    (void) vxbDevMethodRun (DEVMETHOD_CALL(mpApicAddrTableGet),
                            (void *)&mpApicAddrTable);

    (void) vxbDevMethodRun (DEVMETHOD_CALL(mpApicNioApicGet),
                            (void *)&mpApicNioApic);

    for (id = 0; id < mpApicNioApic; id++)
        {
        struct vxbDev inst;
        struct vxbDev * pInst = &inst;
        void * dummy;

        /* Initialize the structure */

        bzero((char *)&inst, sizeof ( struct vxbDev ));

        pInst->pParentBus = pPlbBus;
        pInst->pRegBase[0] = (void *)((ULONG)mpApicAddrTable[id]);
        pInst->pRegBasePhys[0] = pInst->pRegBase[0];

        /*
         * Set the Reg Base Flag to be VXB_REG_MEM to succeed memory
         * mapping (in LP64).
         */

        pInst->regBaseFlags[0] = VXB_REG_MEM;

        /* set the size of the Iocal APIC register space */

        pInst->regBaseSize[0] = IOAPIC_LENGTH;

        /* temporary map the registers */

        (void) vxbRegMap (pInst, 0, &dummy);

        /* set the virtual address */

        ioApicBase = (char *) pInst->pRegBase[0];
        ioApicData = (UINT32 *) (ioApicBase + IOAPIC_DATA);

        (void) printf ("IOAPIC_BASE    = %p\n",ioApicBase);

        (void) printf ("IOAPIC_ID    = 0x%08x\n",
                vxbIoApicIntrGet (ioApicBase, ioApicData, IOAPIC_ID));

        version = vxbIoApicIntrGet (ioApicBase, ioApicData, IOAPIC_VERS);

        (void) printf ("IOAPIC_VER   = 0x%08x\n", version);

        (void) printf ("IOAPIC_ARB   = 0x%08x\n",
                vxbIoApicIntrGet (ioApicBase, ioApicData, IOAPIC_ARB));

        if (version & IOAPIC_PRQ)  /* use PRQ bit XXX */
            (void) printf ("IOAPIC_BOOT  = 0x%08x\n",
                           vxbIoApicIntrGet (ioApicBase, ioApicData, IOAPIC_BOOT));

        for (ix = 0; ix <= ((version & IOAPIC_MRE_MASK) >> 16); ix++)
            {
            (void) printf ("IOAPIC_TBL%02d = 0x%08x ", ix,
                           vxbIoApicIntrGet (ioApicBase, ioApicData,
                                             IOAPIC_REDTBL + ix * 2 + 1));

            (void) printf ("%08x\n",
                           vxbIoApicIntrGet (ioApicBase, ioApicData,
                                             IOAPIC_REDTBL + ix * 2));
            }

        /* unmap the registers */

        (void) vxbRegUnmap (pInst, 0);
        }
    }
