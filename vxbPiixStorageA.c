/* vxbPiixStorage.c - Intel (82371AB) ATA/IDE and ATAPI CDROM
   vxBus storage device driver */

/*
 * Copyright (c) 2011-2016 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01k,17jun16,hma  add the check for the dmabuffer alloc (VXW6-84798)
01j,18jan16,hma  add rename sata disk or patition function(VXW6-10898)
01i,02mar15,yjl  Fix VXW6-84211, Issue with vxbPiixStorage DMA transfer 64k
                 alignment
01h,04nov14,m_y  reduce reset time (VXW6-83384)
01g,08mar13,j_z  add read/write lock, fix PIO failed issue. (WIND00403643)
01f,06mar13,clx  Add clause to process buffer align 64K issue. (WIND00403736)
01e,24jan13,e_d  fixed read data issue when include dosfs cache. (WIND00396782)
01d,07sep12,sye  updated ports supported for show routines. (WIND00354585)
01c,16jun12,sye  fixed static analyzing issue. (WIND00350515)
01b,08mar12,e_d  moved ATA and ATAPI device initialization
                 to vxbSataLib.c. (WIND00333858)
01a,22oct11,e_d  adapted from vxbIntelIchStorage.c version 01w. (WIND00307297)
*/

/*
DESCRIPTION:
\sh BLOCK DEVICE DRIVER:
This is a low level device driver for ATA/ATAPI devices on PCI-IDE host 
controller. Each host controller will be an instance of a PCI device, and it can 
support up to 4 ATA/ATAPI devices(2 drives on IDE primary channel and 2 drives
on IDE secondary channel).

\sh CONFIGURATION
This driver only supports Native and Legacy IDE mode, please enable ATA support 
in BIOS. To support AHCI mode, please refer to driver vxbAhciStorage.c.
To use this VxBus ATA driver, configure VxWorks with the
INCLUDE_DRV_STORAGE_PIIX component.
The required components for this driver are
 INCLUDE_CDROMFS
 INCLUDE_DOSFS
 INCLUDE_DOSFS_MAIN
 INCLUDE_DOSFS_CHKDSK
 INCLUDE_DOSFS_FMT
 INCLUDE_FS_MONITOR
 INCLUDE_ERF
 INCLUDE_XBD
 INCLUDE_DEVICE_MANAGER
 INCLUDE_XBD_PART_LIB
 INCLUDE_PCI_BUS
 INCLUDE_DMA_SYS

The ATA/ATAPI device instances are assigned as "/ata0:1", "/ata0:2", "/ata1:1"
"ataX:N". 'X'signs the assigned number by sataDevIdxAlloc function,
'N' signs the partition number.

Currently, this driver supports most of the ICH ATA host controller, such as:
ICH0, ICH2, ICH2M, ICH3M, 6300ESB_I, 6300ESB_S, 6321ESB, ICH4, ICH5, ICH5R,
ICH6, ICH6R, ICH6M, ICH7, ICHA, ICHB, ICH10F2, ICH10F5, 3100, TOLAPAI,
CROWNBEACH, ICH9R, etc.

\sh INCLUDE FILES:
vxbPiixStorage.h

\sh SMP CONSIDERATIONS
Most of the processing in this driver occurs in the context of a dedicated
task, and therefore is inherently SMP-safe.  One area of possible concurrence
occurs in the interrupt service routine, and an ISR-callable spin lock
take/give pair has been placed around the code which acknowledges/clears the
ATA controller's interrupt status register.

References:
    1) ATAPI-5 specification "T13-1321D Revision 1b, 7 July 1999"
    2) ATAPI for CD-ROMs "SFF-8020i Revision 2.6, Jan 22,1996"
    3) Intel 82801BA (ICH2), 82801AA (ICH), and 82801AB (ICH0) IDE Controller
       Programmer's Reference Manual, Revision 1.0 July 2000

Source of Reference Documents:
    1) ftp://ftp.t13.org/project/d1321r1b.pdf
    2) http://www.bswd.com/sff8020i.pdf

SEE ALSO:
.pG "I/O System"
*/

/* includes */

#include <vxWorks.h>
#include <taskLib.h>
#include <ioLib.h>
#include <memLib.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <stdio.h>
#include <string.h>
#include <private/semLibP.h>
#include <intLib.h>
#include <iv.h>
#include <wdLib.h>
#include <sysLib.h>
#include <sys/fcntlcom.h>
#include <logLib.h>
#include <drv/erf/erfLib.h>        /* event frame work library header */
#include <drv/pci/pciConfigLib.h>
#include <vxBusLib.h>
#include <hwif/vxbus/vxBus.h>
#include <hwif/vxbus/vxbPciLib.h>
#include <hwif/vxbus/hwConf.h>
#include <xbdBlkDev.h>
#include <fsMonitor.h>
#include <vxbus/vxbAccess.h>
#include <vxbus/vxbPciBus.h>
#include <../src/hwif/h/storage/vxbSataLib.h>
#include <../src/hwif/h/storage/vxbSataXbd.h>
#include <../src/hwif/h/storage/vxbPiixStorage.h>
#include <cbioLib.h>

/* SMP-safe */

#include <spinLockLib.h>

#define ATA_SPIN_ISR_INIT(p,q)                                          \
    SPIN_LOCK_ISR_INIT(&p->spinlock[q], 0);
#define ATA_SPIN_ISR_TAKE(p,q)                                          \
    SPIN_LOCK_ISR_TAKE(&p->spinlock[q])
#define ATA_SPIN_ISR_GIVE(p,q)                                          \
    SPIN_LOCK_ISR_GIVE(&p->spinlock[q])

#define I82371AB_SYS_IN8(pDmaCtl, add)                                  \
    vxbRead8 ((void *)pDmaCtl->pPiix4Handler, (UINT8 *)add)
#define I82371AB_SYS_IN32(pDmaCtl, add)                                 \
    vxbRead32 ((void *)pDmaCtl->pPiix4Handler, (UINT32 *)add)
#define I82371AB_SYS_OUT8(pDmaCtl, add, byte)                           \
    vxbWrite8 ((void *)pDmaCtl->pPiix4Handler, (UINT8 *)add, byte)
#define I82371AB_SYS_OUT32(pDmaCtl, add, word)                          \
    vxbWrite32 ((void *)pDmaCtl->pPiix4Handler, (UINT32 *)add, word)

/* extern */

IMPORT int sataXbdPhyPortTotalNumberGet();
IMPORT void sataXbdPhyPortTotalNumberAdd(int);

/* define */

#ifdef  ICH_DEBUG

#   ifdef  LOCAL
#      undef  LOCAL
#      define LOCAL
#   endif /* LOCAL */

#   define DEBUG_INIT      0x00000001
#   define DEBUG_CMD       0x00000002
#   define DEBUG_MON       0x00000004
#   define DEBUG_INT       0x00000008
#   define DEBUG_REG       0x00000010
#   define DEBUG_BIST      0x00000020
#   define DEBUG_DMA       0x00000040
#   define DEBUG_ALWAYS    0xffffffff
#   define DEBUG_NONE      0x00000000

UINT32 ichSataDebug = DEBUG_NONE ;

IMPORT FUNCPTR _func_logMsg;

#   define ICHSATA_DBG_LOG(mask, string, a, b, c, d, e, f)     \
       if ((ichSataDebug & mask) || (mask == DEBUG_ALWAYS))    \
           {                                                   \
           if (_func_logMsg != NULL)                                  \
               _func_logMsg(string, (_Vx_usr_arg_t)a, (_Vx_usr_arg_t)b, \
                            (_Vx_usr_arg_t)c, (_Vx_usr_arg_t)d,         \
                            (_Vx_usr_arg_t)e, (_Vx_usr_arg_t)f);        \
           }
#else
#   define ICHSATA_DBG_LOG(mask, string, a, b, c, d, e, f)
#endif  /* ICH_DEBUG */

/* forward declarations */

LOCAL STATUS ataDmaCtrlInit (VXB_DEVICE_ID, int);
LOCAL STATUS ataDmaEngineSet (VXB_DEVICE_ID, int, int, char *, UINT32, int);
LOCAL STATUS ataDmaStartEngine (VXB_DEVICE_ID, int);
LOCAL STATUS ataDmaStopEngine (VXB_DEVICE_ID, int);
LOCAL short ataDmaModeNegotiate (short);
LOCAL STATUS ataDmaModeSet (VXB_DEVICE_ID, int, int, short);
LOCAL STATUS ataDmaPRDTblBuild (VXB_DEVICE_ID, int, char *, UINT32);
LOCAL STATUS ataDeviceSelect (SATA_HOST *, int);
LOCAL void ataWdog (PIIX4_ATA_CTRL *);
LOCAL void vxbAtaPiixInstInit (VXB_DEVICE_ID);
LOCAL void vxbAtaPiixInstInit2 (VXB_DEVICE_ID);
LOCAL void vxbAtaPiixInstConnect (VXB_DEVICE_ID);
LOCAL BOOL   vxbPiixDevProbe (VXB_DEVICE_ID);
LOCAL STATUS ataDriveInit (SATA_HOST *, int, int);
LOCAL STATUS ataIntrEdge (SATA_HOST *);
LOCAL STATUS ataIntrLevel (SATA_HOST *);
LOCAL STATUS ataStatusChk (SATA_HOST *, UINT8, UINT8);
LOCAL STATUS ataInit (SATA_HOST *, int, int);
LOCAL STATUS ataDrvInit (PIIX4_DRV_CTRL *, int, int, short, int, int);
LOCAL STATUS ataCtrlReset (SATA_HOST *, int);
LOCAL STATUS ataDevIdentify (SATA_HOST  *, int, int);

STATUS ataCmdIssue(SATA_DEVICE *, FIS_ATA_REG *, SATA_DATA *);
STATUS atapiCmdIssue(SATA_DEVICE *, FIS_ATA_REG *, SATA_DATA *);

LOCAL PCI_DEVVEND vxbAtaPiixIdList[] = {

    {0xFFFF, 0xFFFF}
};

LOCAL DRIVER_INITIALIZATION vxbAtaPiixFuncs = {
    vxbAtaPiixInstInit,        /* devInstanceInit */
    vxbAtaPiixInstInit2,       /* devInstanceInit2 */
    vxbAtaPiixInstConnect      /* devConnect */
};

LOCAL PCI_DRIVER_REGISTRATION vxbAtaPiixPciRegistration =
{
    {
        NULL,             /* pNext == NULL */
        VXB_DEVID_DEVICE, /* This is a device driver */
        VXB_BUSID_PCI,    /* hardware resides on a PCI bus */
        VXB_VER_5_0_0,    /* vxbus version */
        PIIX_DRIVER_NAME, /* driver named piix4 */
        &vxbAtaPiixFuncs, /* set of 3 pass functions */
        NULL,             /* no methods */
        vxbPiixDevProbe,  /* probe function */
    },
    NELEMENTS(vxbAtaPiixIdList),
    &vxbAtaPiixIdList[0],
};

LOCAL int  ataRetry          = 2;       /* max retry count */

/*******************************************************************************
*
* vxbIntelIchStorageRegister - register driver with vxbus
*
* RETURNS: N/A
*
* ERRNO
*/

void vxbPiixStorageRegister ()
    {
    vxbDevRegister ((struct vxbDevRegInfo *)&vxbAtaPiixPciRegistration);
    }

/*******************************************************************************
*
* vxbAtaPiixInstInit - First pass initialization
*
*
* NOTE:
*
* RETURNS: N/A
*
* ERRNO
*/

LOCAL void vxbAtaPiixInstInit
    (
    VXB_DEVICE_ID pDev
    )
    {
    vxbNextUnitGet (pDev);
    }

/******************************************************************************
*
* vxbAtaPiixInstInit2 - Second pass initialization
*
* RETURNS: N/A
*
* ERRNO
*/

LOCAL void vxbAtaPiixInstInit2
    (
    VXB_DEVICE_ID pDev
    )
    {
    PIIX4_ATA_RESOURCE *pAta;
    PIIX4_DRV_CTRL *pDrvCtrl;
    SATA_HOST *pAtaCtrl;
    int i;
    UINT8  regPI;
    STATUS rc;
    struct vxbDev *     pParentDev;
    struct vxbPciDevice * pPciDev = (struct vxbPciDevice *)pDev->pBusSpecificDevInfo;

    pDrvCtrl = (PIIX4_DRV_CTRL *)calloc (1, sizeof (PIIX4_DRV_CTRL));
    if (pDrvCtrl == NULL)
        return;

    pDev->pDrvCtrl = pDrvCtrl;
    pDrvCtrl->pDev = pDev;

    for(i = 0; i < ATA_MAX_CTRLS; i++)
        {
        pDrvCtrl->ataCtrl[i].sataHostDmaParentTag = vxbDmaBufTagParentGet (pDev, 0);

        pDrvCtrl->ataCtrl[i].sataHostDmaTag = vxbDmaBufTagCreate
                                              (pDev,
                                              pDrvCtrl->ataCtrl[i].sataHostDmaParentTag,           /* parent */
                                              _CACHE_ALIGN_SIZE,                     /* alignment */
                                              0,                                     /* boundary */
                                              VXB_SPACE_MAXADDR_32BIT,               /* lowaddr */
                                              VXB_SPACE_MAXADDR,                     /* highaddr */
                                              NULL,                                  /* filter */
                                              NULL,                                  /* filterarg */
                                              ATA_MAX_RW_SECTORS*ATA_BYTES_PER_BLOC, /* max size */
                                              1,                                     /* nSegments */
                                              ATA_MAX_RW_SECTORS*ATA_BYTES_PER_BLOC, /* max seg size */
                                              VXB_DMABUF_ALLOCNOW,                   /* flags */
                                              NULL,                                  /* lockfunc */
                                              NULL,                                  /* lockarg */
                                              NULL);
        
        if (pDrvCtrl->ataCtrl[i].sataHostDmaTag == NULL)
            {       
            ICHSATA_DBG_LOG (DEBUG_INT,
                             "vxbAtaPiixInstInit2:sataHostDmaTag init ERROR \n",
                             0,0,0,0, 0, 0);
            return;                               
            }
        }

    /* ATA_RESOURCE contains controller specific features */

    pAta = calloc (ATA_MAX_CTRLS, sizeof (PIIX4_ATA_RESOURCE));
    if (pAta == NULL)
        {
        free (pDrvCtrl);
        pDev->pDrvCtrl = NULL;
        return;
        }

    for (i = 0; i < ATA_MAX_CTRLS; i++)
        {
        pAta[i].configType = (short)
                             (ATA_GEO_CURRENT | ATA_DMA_AUTO | ATA_BITS_32);
        pAta[i].semTimeout = ATA_SEM_TIMEOUT_DEF;
        pAta[i].wdgTimeout = ATA_WDG_TIMEOUT_DEF;
        pAta[i].devices = ATA_MAX_DRIVES;
        }

    pDrvCtrl->pAtaResources = pAta;

    /*
     * configure the native controller
     * resources or compatibility controller resources
     */

    pParentDev = vxbDevParent (pDev);
    rc = vxbPciConfigInByte (pParentDev, pPciDev->pciBus, pPciDev->pciDev,
                             pPciDev->pciFunc, PCI_CFG_PROGRAMMING_IF, &regPI);

    if (rc != OK)
        {
        free (pAta);
        free (pDrvCtrl);
        pDev->pDrvCtrl = NULL;
        return;
        }

    pAtaCtrl = &pDrvCtrl->ataCtrl[0];
    pAtaCtrl->numPorts = ATA_MAX_DRIVES;
    if (regPI & PRI_NATIVE_EN)
        {
        pDrvCtrl->ataIntr[0] = (VOIDFUNCPTR)ataIntrLevel;
        pAtaCtrl->regBase[0] = pDev->pRegBase[0];
        pAtaCtrl->regBase[1] = (UINT8 *) pDev->pRegBase[1] + 0x2;
        pAtaCtrl->regBase[2] = pDev->pRegBase[4];

        vxbRegMap (pDev, 0, &pAtaCtrl->regHandle);
        }
    else
        {
        pDev->pRegBase[0] = (void *) 0x1F0;
        pDev->regBaseFlags[0] = VXB_REG_IO;
        pDev->pRegBase[1] = (void *) 0x3F4;
        pDev->regBaseFlags[1] = VXB_REG_IO;
        pDrvCtrl->ataIntr[0] = (VOIDFUNCPTR)ataIntrEdge;
        pAtaCtrl->regBase[0] = pDev->pRegBase[0];
        pAtaCtrl->regBase[1] = (UINT8 *) pDev->pRegBase[1] + 0x2;
        pAtaCtrl->regBase[2] = pDev->pRegBase[4];

        vxbRegMap (pDev, 0, &pAtaCtrl->regHandle);
        }

    pAtaCtrl = &pDrvCtrl->ataCtrl[1];
    pAtaCtrl->numPorts = ATA_MAX_DRIVES;
    if (regPI & SEC_NATIVE_EN)
        {
        pDrvCtrl->ataIntr[1] = (VOIDFUNCPTR)ataIntrLevel;
        pAtaCtrl->regBase[0] = pDev->pRegBase[2];
        pAtaCtrl->regBase[1] = (UINT8 *) pDev->pRegBase[3] + 0x2;
        pAtaCtrl->regBase[2] = (UINT8 *) pDev->pRegBase[4] + 0x8;

        vxbRegMap (pDev, 0, &pAtaCtrl->regHandle);
        }
    else
        {
        pDev->pRegBase[2] = (void *) 0x170;
        pDev->regBaseFlags[2] = VXB_REG_IO;
        pDev->pRegBase[3] = (void *) 0x374;
        pDev->regBaseFlags[3] = VXB_REG_IO;
        pDrvCtrl->ataIntr[1] = (VOIDFUNCPTR)ataIntrEdge;
        pAtaCtrl->regBase[0] = pDev->pRegBase[2];
        pAtaCtrl->regBase[1] = (UINT8 *) pDev->pRegBase[3] + 0x2;
        pAtaCtrl->regBase[2] = (UINT8 *) pDev->pRegBase[4] + 0x8;

        /* obtain register map handles */

        vxbRegMap (pDev, 0, &pAtaCtrl->regHandle);
        }
    }

/*******************************************************************************
*
* vxbAtaPiixInstConnect - Final initialization
*
* RETURNS: N/A
*
* ERRNO
*/

LOCAL void vxbAtaPiixInstConnect
    (
    VXB_DEVICE_ID pDev
    )
    {
    UINT8 i;
    PIIX4_ATA_RESOURCE * pAtaResource;
    PIIX4_DRV_CTRL * pDrvCtrl;

    VXB_ASSERT_NONNULL_V(pDev)
    pDrvCtrl = (PIIX4_DRV_CTRL *) pDev->pDrvCtrl;
    if (pDrvCtrl == NULL)
        return;

    for (i = 0; i < ATA_MAX_CTRLS; i++)
        {
        pAtaResource = &pDrvCtrl->pAtaResources[i];
        if ((ataDrvInit (pDrvCtrl, i, pAtaResource->devices,
                     pAtaResource->configType, pAtaResource->semTimeout,
                     pAtaResource->wdgTimeout) == ERROR))
            {
            ICHSATA_DBG_LOG (DEBUG_INIT, "ataDrvInit returned ERROR\n",
                             0, 0, 0, 0, 0, 0);
            }
        }
    }

/*******************************************************************************
*
* vxbPiixDevProbe - vxbus probe function
*
* This function is called by vxBus to probe device.
*
* RETURNS: TRUE if probe passes and assumed a valid intel Piix SATA
* device. FALSE otherwise.
*
* ERRNO: N/A
*/

LOCAL BOOL vxbPiixDevProbe
    (
    VXB_DEVICE_ID pDev
    )
    {
    UINT16 devId = 0;
    UINT16 vendorId = 0;
    UINT32 classValue = 0;

    VXB_PCI_BUS_CFG_READ (pDev, PCI_CFG_VENDOR_ID, 2, vendorId);
    VXB_PCI_BUS_CFG_READ (pDev, PCI_CFG_DEVICE_ID, 2, devId);
    VXB_PCI_BUS_CFG_READ (pDev, PCI_CFG_REVISION, 4, classValue);

    /*AMD*/

    if (vendorId == AMD_VID)
        {
        switch (devId)
            {
            case 0x209A:
            case 0x7800:
                return (TRUE);
            default:
                break;
            }
        }

    if (vendorId == INTEL_VID)
        {
        switch (devId)
            {
            case INTEL_ICH_DEVICE_ID:
            case INTEL_ICH0_DEVICE_ID:
            case INTEL_ICH2_DEVICE_ID:
            case INTEL_ICH2M_DEVICE_ID:
            case INTEL_ICH3M_DEVICE_ID:
            case INTEL_6300ESB_I_DEVICE_ID:
            case INTEL_6300ESB_S_DEVICE_ID:
            case INTEL_6321ESB_DEVICE_ID:
            case INTEL_ICH4_DEVICE_ID:
            case INTEL_ICH5_PATA_DEVICE_ID:
            case INTEL_ICH5_SATA_DEVICE_ID:
            case INTEL_ICH5R_SATA_DEVICE_ID:
            case INTEL_ICH6_DEVICE_ID:
            case INTEL_ICH6_SATA_DEVICE_ID:
            case INTEL_ICH6R_SATA_DEVICE_ID:
            case INTEL_ICH6M_SATA_DEVICE_ID:
            case INTEL_ICH7_PATA_DEVICE_ID:
            case INTEL_ICH7A_SATA_DEVICE_ID:
            case INTEL_ICH7B_SATA_DEVICE_ID:
            case INTEL_ICH8M_SATA_DEVICE_ID:
            case INTEL_ICH8M_PATA_DEVICE_ID:
            case INTEL_ICH10F2_SATA_DEVICE_ID:
            case INTEL_ICH10F5_SATA_DEVICE_ID:
            case INTEL_TOLAPAI_SATA_DEVICE_ID:
            case INTEL_CROWNBEACH_PATA_DEVICE_ID:
            case INTEL_ICH9R_PATA_DEVICE_ID:
            case INTEL_PCH_4PORT_SATA_DEVICE_ID_0:
            case INTEL_PCH_4PORT_SATA_DEVICE_ID_1:
                return (TRUE);
            }
        }

    if ((classValue & 0xFFFF0000) == PIIX_CLASS_ID)
        return (TRUE);
    else
        return (FALSE);

    return (FALSE);
    }
/*******************************************************************************
*
* ataIntrEdge - ATA/IDE controller interrupt handler.
*
* standard interrupt handler for most PC ATA
* controllers attached to IRQ 14 or IRQ 15
*
* RETURNS: N/A
*/

LOCAL STATUS ataIntrEdge
    (
    SATA_HOST * pCtrl
    )
    {
    UINT8 intRegTemp = 0;
    PIIX4_DRV_CTRL * pExtCtrl;

    VXB_ASSERT_NONNULL(pCtrl,ERROR)

    pExtCtrl = (PIIX4_DRV_CTRL *)pCtrl->pCtrlExt;
    if (pExtCtrl == NULL)
        return ERROR;

    intRegTemp = ATA_IO_BYTE_READ (PIIX4_ATA_A_STATUS);
    ATA_SPIN_ISR_TAKE (pExtCtrl,pCtrl->numCtrl);
    if (!(intRegTemp & ATA_STAT_BUSY))
        {
        pExtCtrl->intStatus = ATA_IO_BYTE_READ (PIIX4_ATA_STATUS);
        pExtCtrl->intCount++;
        }
    ATA_SPIN_ISR_GIVE (pExtCtrl,pCtrl->numCtrl);

    /* return from semGive is not needed */

    (void)semGive (&pExtCtrl->syncSem[pCtrl->numCtrl]);
    ICHSATA_DBG_LOG (DEBUG_INT,
                     "ataIntrEdge:ctrl=%d ISR count %d Status= %#x \n",
                     pCtrl->numCtrl, pExtCtrl->intCount, pExtCtrl->intStatus,
                     0, 0, 0);
    return OK;
    }

/*******************************************************************************
*
* ataIntrLevel - ATA/IDE controller interrupt handler.
*
* RETURNS: N/A
*/

LOCAL STATUS ataIntrLevel
    (
    SATA_HOST * pCtrl
    )
    {
    UINT8 bmstat;
    PIIX4_DRV_CTRL * pExtCtrl;

    VXB_ASSERT_NONNULL(pCtrl,ERROR)

    pExtCtrl = (PIIX4_DRV_CTRL *)pCtrl->pCtrlExt;
    if (pExtCtrl == NULL)
        return ERROR;

    bmstat = ATA_IO_BYTE_READ (PIIX4_BM_STATUS);
    if (bmstat & 4)
        {
        ATA_SPIN_ISR_TAKE (pExtCtrl,pCtrl->numCtrl);

        if (!(ATA_IO_BYTE_READ (PIIX4_ATA_A_STATUS) & ATA_STAT_BUSY))
            {

            /* acknowledge interrupt on the controller */

            pExtCtrl->intStatus = ATA_IO_BYTE_READ (PIIX4_ATA_STATUS);
            pExtCtrl->intCount++;
            }

        /* clear PCI Level interrupt */

        ATA_IO_BYTE_WRITE (PIIX4_BM_STATUS, (UINT8) (bmstat & ~2));
        ATA_SPIN_ISR_GIVE (pExtCtrl,pCtrl->numCtrl);

        /* return from semGive is not needed */

        (void)semGive (&pExtCtrl->syncSem[pCtrl->numCtrl]);
        ICHSATA_DBG_LOG (DEBUG_INT,
                         "ataIntrLevel:  ctrl=%d ISR count %d Status= %#x \n",
                         pCtrl->numCtrl, pExtCtrl->intCount, pExtCtrl->intStatus,
                         0, 0, 0);
        return OK;
        }
    return ERROR;
    }

/*******************************************************************************
*
* ataDrvInit - Initialize the ATA driver
*
* This routine initializes the ATA/ATAPI device driver, initializes IDE
* host controller and sets up interrupt vectors for requested controller. This
* function must be called once for each controller, before any access to drive
* on the controller.
*
* If it is called more than once for the same controller, it returns OK with a
* message display 'Host controller already initialized ', and does nothing as
* already required initialization is done.
*
* Additionally it identifies devices available on the controller and
* initializes depending on the type of the device (ATA or ATAPI). Initialization
* of device includes reading parameters of the device and configuring to the
* defaults.
*
* RETURNS: OK, or ERROR if initialization fails.
*
* SEE ALSO: ataPiix4DevCreate()
*/

LOCAL STATUS ataDrvInit
    (
    PIIX4_DRV_CTRL * pDrvCtrl,
    int ctrl,         /* controller no. 0,1   */
    int drives,       /* number of drives 1,2 */
    short configType, /* configuration type   */
    int semTimeout,   /* timeout seconds for sync semaphore */
    int wdgTimeout    /* timeout seconds for watch dog      */
    )
    {
    VXB_DEVICE_ID pDev;
    SATA_HOST * pCtrl;
    PIIX4_ATA_DRIVE * pDrive;
    int drive;
    STATUS dok = OK;
    struct pciIntrEntry * pIntrInfo;
    UINT8 regPI;
    struct vxbDev * pParentDev;
    struct vxbPciDevice * pPciDev;

    VXB_ASSERT_NONNULL(pDrvCtrl,ERROR)

    pDev = pDrvCtrl->pDev;
    pCtrl = &pDrvCtrl->ataCtrl[ctrl];
    if ((pDev == NULL) || (pCtrl == NULL))
        return ERROR;

    pPciDev = (struct vxbPciDevice *)pDev->pBusSpecificDevInfo;
    if (pPciDev == NULL)
        return ERROR;

    ICHSATA_DBG_LOG (DEBUG_INIT, "ataDrv: entered\n\n", 0, 0, 0, 0, 0, 0);
    ICHSATA_DBG_LOG (DEBUG_INIT,
                     "     controller   = %d \n     No of drives = %d \n"
                     "     semTimeout   = %d \n",
                     ctrl, drives, semTimeout, 0, 0, 0);

    pCtrl->ops.cmdIssue = (FUNCPTR)ataCmdIssue;

    /* default open DMA support */

    for(drive = 0; drive < drives; drive++)
        {
        pDrive = malloc(sizeof(PIIX4_ATA_DRIVE));
        if (pDrive == NULL)
            return ERROR;

        pCtrl->sataDev[drive] = (void *)pDrive;
        memset(pDrive, 0, sizeof(PIIX4_ATA_DRIVE));
        }

    pDrvCtrl->ataDmaReset = (FUNCPTR) NULL;
    pDrvCtrl->ataDmaInit = (FUNCPTR) ataDmaCtrlInit;
    pDrvCtrl->ataDmaModeNegotiate = (FUNCPTR) ataDmaModeNegotiate;
    pDrvCtrl->ataDmaModeSet = (FUNCPTR) ataDmaModeSet;
    pDrvCtrl->ataDmaSet = (FUNCPTR) ataDmaEngineSet;
    pDrvCtrl->ataDmaStart = (FUNCPTR) ataDmaStartEngine;
    pDrvCtrl->ataDmaStop = (FUNCPTR) ataDmaStopEngine;
    pDrvCtrl->ataHostDmaSupportOkay = TRUE;

    if ((ctrl >= ATA_MAX_CTRLS) || (drives > ATA_MAX_DRIVES))
        {
        ICHSATA_DBG_LOG (DEBUG_INIT,
                        "ataDrv: Invalid Controller number or Number of drives",
                        0, 0, 0, 0, 0, 0);
        return(ERROR);
        }

    if (!(pCtrl->installed))
        {

        /* save the controller number for quick access */

        pCtrl->numCtrl = ctrl;

        /*
         * setup controller I/O address first
         * This is done early so we can use device access functions
         */

        pCtrl->pCtrlExt = pDrvCtrl; /* reference parent */

        if (ataCtrlReset (pCtrl, ctrl) == ERROR)
            {
            ICHSATA_DBG_LOG (DEBUG_INIT, "ataDrv: Controller %d reset failed\n",
                             ctrl, 0, 0, 0, 0, 0);
            return(ERROR);
            }

        ICHSATA_DBG_LOG (DEBUG_INIT,
                         "ataDrv: Controller %d to be installed, intCount %x\n",
                         ctrl, pDrvCtrl->intCount, 0, 0, 0, 0);

        if (semBInit (&pDrvCtrl->syncSem[ctrl], SEM_Q_FIFO, SEM_EMPTY) != OK)
            {
            ICHSATA_DBG_LOG(DEBUG_INIT, "ataDrv: cannot initialize "
                            "Sync semaphore, returning\n",
                            0,0,0,0,0,0);
            return(ERROR);
            }
        if (semMInit (&pDrvCtrl->muteSem[ctrl],
                      SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE) != OK)
            {
            ICHSATA_DBG_LOG(DEBUG_INIT, "ataDrv: cannot initialize Mutex"
                            "semaphore, returning\n",
                            0,0,0,0,0,0);
            return(ERROR);
            }
        if (semMInit (&pDrvCtrl->rwSem[ctrl],
                      SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE) != OK)
            {
            ICHSATA_DBG_LOG(DEBUG_INIT, "ataDrv: cannot initialize rwSem"
                            "semaphore, returning\n",
                            0,0,0,0,0,0);
            return(ERROR);
            }

        /* SMP-safe */

        ATA_SPIN_ISR_INIT (pDrvCtrl,ctrl);

        /* execute DMA reset function for this controller */

        if (pDrvCtrl->ataDmaReset != NULL)
            {
            (*pDrvCtrl->ataDmaReset) (pDev, ctrl);
            }

        /* execute DMA init function for this controller */

        if (pDrvCtrl->ataDmaInit != NULL)
            {
            if ((*pDrvCtrl->ataDmaInit) (pDev, ctrl) != OK)
                {
                pDrvCtrl->ataHostDmaSupportOkay = FALSE;
                }
            }

        pDrvCtrl->wdgId[ctrl] = wdCreate ();

        if (semTimeout == 0)
            {
            pDrvCtrl->semTimeout[ctrl] = ATA_SEM_TIMEOUT_DEF;
            }
        else
            {
            pDrvCtrl->semTimeout[ctrl] = semTimeout;
            }

        if (wdgTimeout == 0)
            {
            pDrvCtrl->wdgTimeout[ctrl] = ATA_WDG_TIMEOUT_DEF;
            }
        else
            {
            pDrvCtrl->wdgTimeout[ctrl] = wdgTimeout;
            }

        pDrvCtrl->wdgOkay[ctrl] = TRUE;

        /*
         * legacy pci-ide controller interrupt is fixed 0xE,0xF,
         * should acquire it from PCI interrupt line.
         */

        pParentDev = vxbDevParent (pDev);

        dok = vxbPciConfigInByte (pParentDev, pPciDev->pciBus, pPciDev->pciDev,
                                  pPciDev->pciFunc, PCI_CFG_PROGRAMMING_IF, &regPI);

        if (dok != OK)
            {
            ICHSATA_DBG_LOG (DEBUG_INIT,
                             "ataDrv: vxbPciConfigInByte failed on"
                             "Channel %d Device %d\n",
                             ctrl, drive, 0, 0, 0, 0);
            return (dok);
            }
        if ((ctrl == 0) && !(regPI & PRI_NATIVE_EN))
            {
            pIntrInfo = (struct pciIntrEntry *) pDev->pIntrInfo;
            pIntrInfo->intVecInfo[0].index = 0xe;
            pIntrInfo->intVecInfo[0].intVector = INUM_TO_IVEC (0xe);
            }
        if ((ctrl == 1) && !(regPI & SEC_NATIVE_EN))
            {
            pIntrInfo = (struct pciIntrEntry *) pDev->pIntrInfo;
            pIntrInfo->intVecInfo[0].index = 0xf;
            pIntrInfo->intVecInfo[0].intVector = INUM_TO_IVEC (0xf);
            }

        if (vxbIntConnect (pDev, 0, (VOIDFUNCPTR)pDrvCtrl->ataIntr[ctrl],
            (void *)pCtrl) == ERROR)
            {
            ICHSATA_DBG_LOG (DEBUG_INIT,
                             "ataDrv: vxbIntConnect failed: pDev:%p\n",
                             pDev, 0, 0, 0, 0, 0);
            return (dok);
            }

        if (vxbIntEnable (pDev, 0, (VOIDFUNCPTR)pDrvCtrl->ataIntr[ctrl],
            (void *)pCtrl) == ERROR)
            {
            ICHSATA_DBG_LOG (DEBUG_INIT,
                             "ataDrv: vxbIntError failed: pDev:%p\n",
                             pDev, 0, 0, 0, 0, 0);
            return (dok);
            }

        pCtrl->installed = TRUE;
        pCtrl->PortsBaseIndex = sataXbdPhyPortTotalNumberGet();
		  
        (void)sataXbdPhyPortTotalNumberAdd(pCtrl->numPorts);
        
        for (drive = 0; drive < drives; drive++)
            {
            pDrive = (PIIX4_ATA_DRIVE *)pCtrl->sataDev[drive];
            pDrive->sataPortDev.host = pCtrl;
            pDrive->sataPortDev.num = drive;
            pDrive->sataPortDev.logicPortNum = \
                        drive + pCtrl->PortsBaseIndex;
            pDrive->sataPortDev.okSpin = FALSE;
            pDrive->Reset = (void *) ataInit;

            ICHSATA_DBG_LOG (DEBUG_INIT,
                             "ataDrv: Calling ataDriveInit: %x/%x\n", ctrl,
                             drive, 0, 0, 0, 0);
            ataDevIdentify (pCtrl, ctrl, drive);
            if((pDrive->sataPortDev.type & (ATA_TYPE_ATA | ATA_TYPE_ATAPI)) != 0x0)
                {
                if ((ataDriveInit (pCtrl, ctrl, drive)) == ERROR)
                    {
                    dok = ERROR;

                    /* zero entire structure to indicate drive is not present. */

                    memset ((char *) pDrive, 0, sizeof (PIIX4_ATA_DRIVE));

                    ICHSATA_DBG_LOG (DEBUG_INIT,
                                     "ataDrv: ataDriveInit failed on"
                                     "Channel %d Device %d\n",
                                     ctrl, drive, 0, 0, 0, 0);
                    }
                }
            }
        }
    else
        {
        ICHSATA_DBG_LOG (DEBUG_INIT,
                         "Host controller already initialized \n",
                         0, 0, 0, 0, 0, 0);
        }

    return(dok);
    }

/*******************************************************************************
*
* ataDriveInit - initialize ATA drive
*
* This routine checks the drive presence, identifies its type, initializes
* the drive and driver control structures with the parameters of the drive.
*
* RETURNS: OK if drive was initialized successfuly, or ERROR.
*/

LOCAL STATUS ataDriveInit
    (
    SATA_HOST  * pCtrl,
    int ctrl,
    int drive
    )
    {
    PIIX4_DRV_CTRL  * pDrvCtrl;
    VXB_DEVICE_ID     pDev;
    PIIX4_ATA_DRIVE * pDrive;
    device_t          xbd;
    STATUS            rc         = OK;

    VXB_ASSERT_NONNULL(pCtrl,ERROR)

    pDrvCtrl = (PIIX4_DRV_CTRL *)(pCtrl->pCtrlExt);
    if (pDrvCtrl == NULL)
        return ERROR;

    pDev = pDrvCtrl->pDev;
    pDrive = (PIIX4_ATA_DRIVE *)(pCtrl->sataDev[drive]);
    if ((pDev == NULL) || (pDrive == NULL))
        return ERROR;

    ICHSATA_DBG_LOG (DEBUG_INIT, "ataDriveInit: ctrl=%d, drive=%d\n",
                     ctrl, drive, 0, 0, 0, 0);

    rc = semTake (&pDrvCtrl->muteSem[ctrl],
                  sysClkRateGet () * pDrvCtrl->semTimeout[ctrl] * 2);

    if (rc != OK)
        {
        ICHSATA_DBG_LOG (DEBUG_INIT,
                         "ataDriveInit: Error getting mutex - ERROR\n",
                         0, 0, 0, 0, 0, 0);
        return(ERROR);
        }

    if (vxbDmaBufMapCreate (pDev, pDrvCtrl->ataCtrl[ctrl].sataHostDmaTag, 0,
                        &(pDrive->sataPortDev.sataDmaMap)) == NULL)
                        
        {
        ICHSATA_DBG_LOG (DEBUG_INIT, "ataDriveInit: Error DMA map create\n",
                         1, 2, 3, 4, 5, 6);
        (void)semGive (&pDrvCtrl->muteSem[ctrl]);
        return(ERROR);
        }

    if (pDrive->sataPortDev.type == ATA_TYPE_ATA)
        {
        rc = sataAtaNormalInit((SATA_DEVICE *)&pDrive->sataPortDev);
        }
    else if (pDrive->sataPortDev.type == ATA_TYPE_ATAPI)
        {
        rc = sataAtapiNormalInit((SATA_DEVICE *)&pDrive->sataPortDev);
        }
    if (rc != OK)
        {
        ICHSATA_DBG_LOG (DEBUG_INIT, "ataDriveInit: ctrl=%d, drive=%d fail\n",
                         ctrl, drive, 0, 0, 0, 0);
        pDrive->sataPortDev.attached = FALSE;
        pDrive->sataPortDev.type = ATA_DEV_NONE;
        pDrive->state = ATA_DEV_NONE;
        goto driveInitExit;
        }
    pDrive->sataPortDev.attached = TRUE;
    pDrive->sataPortDev.prdMaxSize = MAX_BYTES_PER_PRD;
    if(pDrive->sataPortDev.okLba48)
        pDrive->sataPortDev.fisMaxSector = MAX_SECTORS_FISLBA48;
    else
        pDrive->sataPortDev.fisMaxSector = MAX_SECTORS_FISNOMAL;

    if (pDrvCtrl->ataDmaModeSet != NULL)
        {
        (*pDrvCtrl->ataDmaModeSet) (pDev, ctrl, drive, pDrive->sataPortDev.rwMode);
        }

    pDrive->sataPortDev.xbdDev.pPort = (void *)(&pDrive->sataPortDev);

    /* Set multiple mode (multisector read/write) */

    xbd = sataXbdDevCreate(&pDrive->sataPortDev, (char *)NULL);
    if (xbd != (device_t)NULLDEV)
        {
        pDrive->state = ATA_DEV_OK;
        }

driveInitExit:

    /* return from semGive is not needed */

    (void)semGive (&pDrvCtrl->muteSem[ctrl]);

    if (pDrive->state != ATA_DEV_OK)
        return(ERROR);

    ICHSATA_DBG_LOG (DEBUG_INIT,"ataDriveInit %d/%d exited\n",
                     ctrl, drive, 0, 0, 0, 0);
    return(OK);
    }

/*******************************************************************************
*
* atapiCmdIssue - Send ATAPI command a SATA device
*
* This routine sends ATAPI command to a SATA device.
*
* RETURNS: OK or ERROR
*
*/
STATUS atapiCmdIssue
    (
    SATA_DEVICE * pDrv,
    FIS_ATA_REG * pFisAta,
    SATA_DATA * pSataData
    )
    {
    PIIX4_ATA_DRIVE * pDrive;
    SATA_HOST * pCtrl;
    PIIX4_DRV_CTRL * pDrvCtrlExt;
    BOOL        retry    = TRUE;
    int       retryCount = 0;
    int       semStatus;
    int       drive;
    UINT8     direction;
    UINT8     error = 0;

    VXB_ASSERT_NONNULL(pDrv,ERROR)
    VXB_ASSERT_NONNULL(pFisAta,ERROR)

    pDrive = (PIIX4_ATA_DRIVE *)(pDrv->host->sataDev[pDrv->num]);
    pCtrl = pDrv->host;
    if ((pDrive == NULL) || (pCtrl == NULL))
        return ERROR;

    pDrvCtrlExt = (PIIX4_DRV_CTRL *)pCtrl->pCtrlExt;
    if (pDrvCtrlExt == NULL)
        return ERROR;

    drive = pDrv->num;
    while (retry)
        {
        if (ataDeviceSelect(pCtrl, drive) != OK)
            return ERROR;

        ICHSATA_DBG_LOG(DEBUG_CMD, "atapicmd: %d/%d beginning of while\n",
                        pCtrl->numCtrl, pDrv->num, 0, 0, 0, 0);

        if(pDrv->okDma)
            ATA_IO_BYTE_WRITE (PIIX4_ATA_FEATURE,
                               (UINT8)pFisAta->fisCmd.fisAtaCmd[3] | FEAT_DMA);
        else
            ATA_IO_BYTE_WRITE (PIIX4_ATA_FEATURE,
                               (UINT8)pFisAta->fisCmd.fisAtaCmd[3]);
        ATA_IO_BYTE_WRITE (PIIX4_ATA_SECCNT_INTREASON, 0);
        ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLLOW_BCOUNT_LO,
                           (UINT8)((pFisAta->fisCmd.fisAtaCmd[5])));
        ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLHI_BCOUNT_HI,
                           (UINT8)((pFisAta->fisCmd.fisAtaCmd[6])));
        ATA_IO_BYTE_WRITE (PIIX4_ATA_SDH_D_SELECT,
                           (UINT8)((pFisAta->fisCmd.fisAtaCmd[7]) |
                           (drive << ATA_DRIVE_BIT)));

        if((pFisAta->fisCmd.fisCmdFlag & ATA_FLAG_NON_DATA) == 0x0)
            {
            if(pDrv->okDma)
                {
                direction = (UINT8)((pFisAta->fisCmd.fisCmdFlag >> 7) & 0x01);
                if (ataStatusChk(pCtrl, ATA_STAT_DRQ | ATA_STAT_BUSY, 0) != OK)
                    {
                    return (ERROR);
                    }

                if (pDrvCtrlExt->ataDmaSet != NULL)
                    {
                    (void)vxbDmaBufMapLoad (pDrvCtrlExt->pDev,
                                      pDrv->host->sataHostDmaTag,
                                      pDrv->sataDmaMap,
                                      pSataData->buffer,
                                      pSataData->blkNum * pSataData->blkSize,
                                      0);

                    if ((*pDrvCtrlExt->ataDmaSet)(pDrvCtrlExt->pDev,
                                                   pCtrl->numCtrl, pDrv->num,
                                                   (char *)(ULONG)pDrv->sataDmaMap->fragList[0].frag,
                                                   (UINT32)(pSataData->blkNum *
                                                            pSataData->blkSize),
                                                   direction) != OK)
                    return (ERROR);
                    }
                if((pFisAta->fisCmd.fisCmdFlag & ATA_FLAG_OUT_DATA) != 0x0)
                    vxbDmaBufMapSync (pDrvCtrlExt->pDev,
                                      pDrv->host->sataHostDmaTag,
                                      pDrv->sataDmaMap,
                                      0,
                                      pSataData->blkNum * pSataData->blkSize,
                                      _VXB_DMABUFSYNC_DMA_PREWRITE);
                }
        }

        ATA_IO_BYTE_WRITE (PIIX4_ATA_COMMAND, (UINT8)pFisAta->ataReg.command);

        ATA_DELAY_400NSEC;

        /* Wait for DRQ and !BSY */

        if (ataStatusChk(pCtrl, ATA_STAT_DRQ | ATA_STAT_BUSY,
                         ATA_STAT_DRQ | !ATA_STAT_BUSY) != OK)
            {
            ICHSATA_DBG_LOG(DEBUG_CMD, "atapicmd: %d/%d status check error\n",
                           pCtrl->numCtrl, pDrv->num, 0, 0, 0, 0);
            return (ERROR);
            }
        ATA_IO_NWORD_WRITE (PIIX4_ATA_DATA,
                            (void *)(&pFisAta->fisCmd.fisApatiCmd[0]),
                            6);
        if(pDrv->okDma)
            {
            if (pDrvCtrlExt->ataDmaStart != NULL)
                {
                (*pDrvCtrlExt->ataDmaStart)(pDrvCtrlExt->pDev, pCtrl->numCtrl);
                }
            else
                {
                return(ERROR);
                }
            }

        if((!pDrv->okDma) && (pFisAta->fisCmd.fisCmdFlag == ATA_FLAG_OUT_DATA))
            {

            /* Wait for DRQ and !BSY */

            if (ataStatusChk(pCtrl, ATA_STAT_DRQ | ATA_STAT_BUSY,
                             ATA_STAT_DRQ | !ATA_STAT_BUSY) != OK)
                {
                ICHSATA_DBG_LOG(DEBUG_CMD,
                               "atapicmd: %d/%d status check error\n",
                               pCtrl->numCtrl, pDrv->num, 0, 0, 0, 0);
                return (ERROR);
                }

            ATA_IO_NLONG_WRITE (PIIX4_ATA_DATA, (void *)pSataData->buffer,
                                (pSataData->blkNum * pSataData->blkSize) /
                                sizeof(UINT32));
            }

        semStatus = semTake (&pDrvCtrlExt->syncSem[pCtrl->numCtrl],
                             sysClkRateGet() *
                             pDrvCtrlExt->semTimeout[pCtrl->numCtrl]);

        if(pDrive->sataPortDev.okDma)
            {
            if (pDrvCtrlExt->ataDmaStop != NULL)
                {
                (*pDrvCtrlExt->ataDmaStop)(pDrvCtrlExt->pDev, pCtrl->numCtrl);
                }
            if ((pFisAta->fisCmd.fisCmdFlag & ATA_FLAG_IN_DATA) != 0x0)
                vxbDmaBufMapSync (pDrvCtrlExt->pDev,
                                  pDrv->host->sataHostDmaTag,
                                  pDrv->sataDmaMap,
                                  0,
                                  pSataData->blkNum * pSataData->blkSize,
                                  _VXB_DMABUFSYNC_DMA_POSTREAD);

            vxbDmaBufMapUnload(pDrv->host->sataHostDmaTag, pDrv->sataDmaMap);
            }
        if ((pDrvCtrlExt->intStatus & ATA_STAT_ERR) || (semStatus == ERROR))
            {
            if (pDrvCtrlExt->intStatus & ATA_STAT_ERR)
                {
                error = ATA_IO_BYTE_READ (PIIX4_ATA_ERROR);
                if (error & ERR_ABRT)
                    {

                    /* command couldn't complete, return without retry */

                    ICHSATA_DBG_LOG (DEBUG_CMD,
                                     "ataCmd: cmd aborted by device, returning\n"
                                     "  status=0x%x semStatus=%d err=0x%x\n",
                                     pDrvCtrlExt->intStatus, semStatus, error,
                                     0, 0, 0);
                    return (ERROR);
                    }
                }
            else
                ICHSATA_DBG_LOG (DEBUG_CMD,
                                 "atapiCmd: error - retrying "
                                 " status=0x%x semStatus=%d err=0x%x\n",
                                 pDrvCtrlExt->intStatus, semStatus,
                                 error, 0, 0, 0);

            if (++retryCount > ataRetry)
                return(ERROR);
            }
        else
            {
            if((pFisAta->fisCmd.fisCmdFlag == ATA_FLAG_IN_DATA) &&
               (!pDrive->sataPortDev.okDma))
                {
                if (ataStatusChk(pCtrl, ATA_STAT_DRQ | ATA_STAT_BUSY,
                                 ATA_STAT_DRQ | !ATA_STAT_BUSY) != OK)
                    {
                    return (ERROR);
                    }
                ATA_IO_NLONG_READ(PIIX4_ATA_DATA,
                                  (void *)pSataData->buffer,
                                  (pSataData->blkNum * pSataData->blkSize) /
                                  sizeof(UINT32));
                }

            retry = FALSE;
            }
        }

    ICHSATA_DBG_LOG (DEBUG_CMD, "atapiCmd end - ctrl %d, drive %d: Ok\n",
                     pCtrl->numCtrl, pDrv->num, 0, 0, 0, 0);

    return(OK);
    }

/*******************************************************************************
*
* ataCmdIssue - Send ATA/ATAPI command to a SATA device
*
* This routine sends ATA/ATAPI command to a SATA device.
*
* RETURNS: OK or ERROR
*
*/

STATUS ataCmdIssue
    (
    SATA_DEVICE * pDrv,
    FIS_ATA_REG * pFisAta,
    SATA_DATA * pSataData
    )
    {
    VXB_ASSERT_NONNULL(pDrv,ERROR)
    VXB_ASSERT_NONNULL(pFisAta,ERROR)
    if((pFisAta->fisCmd.fisCmdFlag & ATA_FLAG_ATAPI) != 0x0)
        return(atapiCmdIssue(pDrv, pFisAta, pSataData));

    SATA_HOST * pCtrl = pDrv->host;
    PIIX4_DRV_CTRL * pDrvCtrlExt = (PIIX4_DRV_CTRL *)pCtrl->pCtrlExt;
    int       semStatus;
    int       drive = pDrv->num;
    UINT8     direction, useLba = 0;
    UINT8     error = 0;
    UINT32    nSectors = 0, block = 1, nWords = 0;

    (void)semTake (&pDrvCtrlExt->rwSem[pCtrl->numCtrl], WAIT_FOREVER);

    (void)ataDeviceSelect (pCtrl, drive);

    ICHSATA_DBG_LOG (DEBUG_CMD, "atacmd: %d/%d beginning of while\n",
                    pCtrl->numCtrl, pDrv->num, 0, 0, 0, 0);

    if ((pFisAta->fisCmd.fisCmdFlag & ATA_FLAG_NON_DATA) != 0x0)
        {
        ATA_IO_BYTE_WRITE (PIIX4_ATA_FEATURE,
                           (UINT8)pFisAta->ataReg.feature);
        ATA_IO_BYTE_WRITE (PIIX4_ATA_SECCNT_INTREASON,
                           (UINT8)pFisAta->ataReg.sector);
        ATA_IO_BYTE_WRITE (PIIX4_ATA_SECTOR,
                           (UINT8)pFisAta->ataReg.lbaLow);
        ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLLOW,
                           (UINT8)pFisAta->ataReg.lbaMid);
        ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLHI,
                           (UINT8)pFisAta->ataReg.lbaHigh);
        ATA_IO_BYTE_WRITE (PIIX4_ATA_SDH_D_SELECT,
                           (UINT8)(drive << ATA_DRIVE_BIT));
        }
    else
        {

        /*
         * The following code determines if the disk drive supports 48bit logical
         * block addressing and uses that during the read or write operation.
         * Internally the controller has FIFO's at the traditional task register
         * locations.  If the drive supports 48bit LBA, 2 register writes are
         * made to the appropriate registers and the appropriate command issued.
         * The sector count is always written as 2 bytes.  If 48 bit LBA is
         * not supported, the double write has no effect since the final register
         * value is the correct one.
         */

         ATA_IO_BYTE_WRITE (PIIX4_ATA_SECCNT_INTREASON,
                           (UINT8)(pFisAta->fisCmd.fisAtaCmd[13]));
         ATA_IO_BYTE_WRITE (PIIX4_ATA_SECCNT_INTREASON,
                           (UINT8)(pFisAta->fisCmd.fisAtaCmd[12]));
        if (pDrv->okLba48)
            {

            ATA_IO_BYTE_WRITE (PIIX4_ATA_SECTOR,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[8])));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_SECTOR,
                               (UINT8)(pFisAta->fisCmd.fisAtaCmd[4]));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLLOW_BCOUNT_LO,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[9])));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLLOW_BCOUNT_LO,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[5])));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLHI_BCOUNT_HI,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[10])));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLHI_BCOUNT_HI,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[6])));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_SDH_D_SELECT,
                               (UINT8)(
                               (drive << ATA_DRIVE_BIT)) | USE_LBA);
            }
        else /* pDrv->okLba CHS */
            {
            if (pDrv->okLba)
                useLba = USE_LBA;

            ICHSATA_DBG_LOG(DEBUG_CMD, "ATA okLba 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                  (pFisAta->fisCmd.fisAtaCmd[12]),
                  (pFisAta->fisCmd.fisAtaCmd[4]),
                  (pFisAta->fisCmd.fisAtaCmd[5]),
                  (pFisAta->fisCmd.fisAtaCmd[6]),
                  ((pFisAta->fisCmd.fisAtaCmd[7]&0xF) |
                               (drive << ATA_DRIVE_BIT) | useLba),
                  0);

            ATA_IO_BYTE_WRITE (PIIX4_ATA_SECTOR,
                               (UINT8)(pFisAta->fisCmd.fisAtaCmd[4]));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLLOW_BCOUNT_LO,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[5])));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_CYLHI_BCOUNT_HI,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[6])));
            ATA_IO_BYTE_WRITE (PIIX4_ATA_SDH_D_SELECT,
                               (UINT8)((pFisAta->fisCmd.fisAtaCmd[7]&0xF) |
                               (drive << ATA_DRIVE_BIT) | useLba));
            }

        if (pDrv->okDma)
            {
            direction = (UINT8)((pFisAta->fisCmd.fisCmdFlag >> 7) & 0x01);

            if (ataStatusChk (pCtrl, ATA_STAT_DRQ | ATA_STAT_BUSY, 0) != OK)
                {
                ICHSATA_DBG_LOG(DEBUG_CMD,
                                "atacmd: %d/%d status check error\n",
                                pCtrl->numCtrl, pDrv->num, 0, 0, 0, 0);
                goto errExit;
                }

            if (pDrvCtrlExt->ataDmaSet != NULL)
                {
                (void)vxbDmaBufMapLoad (pDrvCtrlExt->pDev,
                                  pDrv->host->sataHostDmaTag,
                                  pDrv->sataDmaMap,
                                  pSataData->buffer,
                                  pSataData->blkNum * pSataData->blkSize,
                                  0);

                if ((*pDrvCtrlExt->ataDmaSet)(pDrvCtrlExt->pDev,
                                              pCtrl->numCtrl, pDrv->num,
                                              (char *)(ULONG)pDrv->sataDmaMap->fragList[0].frag,
                                              (UINT32)(pSataData->blkNum *
                                                       pSataData->blkSize),
                                              direction) != OK)
                    goto errExit;
                }

            if ((pFisAta->fisCmd.fisCmdFlag & ATA_FLAG_OUT_DATA) != 0x0)
                vxbDmaBufMapSync (pDrvCtrlExt->pDev,
                                  pDrv->host->sataHostDmaTag,
                                  pDrv->sataDmaMap,
                                  0,
                                  pSataData->blkNum * pSataData->blkSize,
                                  _VXB_DMABUFSYNC_DMA_PREWRITE);
            }
         else
            {
            nSectors = (pSataData->blkNum * pSataData->blkSize)/ATA_SECTOR_SIZE;

            if (pDrv->pioMode == ATA_PIO_MULTI)
                block = pDrv->multiSecs;

            nWords = (ATA_SECTOR_SIZE * block) >> 1;
            }
        }

    ATA_IO_BYTE_WRITE (PIIX4_ATA_COMMAND, (UINT8)pFisAta->ataReg.command);


    if ((pDrv->okDma) && (pFisAta->fisCmd.fisCmdFlag != ATA_FLAG_NON_DATA))
        {
        ATA_DELAY_400NSEC;

        if (pDrvCtrlExt->ataDmaStart != NULL)
            {
            (*pDrvCtrlExt->ataDmaStart)(pDrvCtrlExt->pDev, pCtrl->numCtrl);
            }
        }

    if((!pDrv->okDma) && (pFisAta->fisCmd.fisCmdFlag == ATA_FLAG_OUT_DATA))
        {
        ICHSATA_DBG_LOG(DEBUG_NONE,"PIO write nSectors %d\n",nSectors,2,3,4,5,6);

        while (nSectors > 0)
            {
            if ((pDrv->pioMode == ATA_PIO_MULTI) && (nSectors < block))
                {
                block = nSectors;
                nWords = (ATA_SECTOR_SIZE * block) >> 1;
                }

            /* Wait for DRQ and !BSY */

            if (ataStatusChk (pCtrl, ATA_STAT_DRQ | ATA_STAT_BUSY,
                             ATA_STAT_DRQ | !ATA_STAT_BUSY) != OK)
                {
                goto errExit;
                }

            ATA_IO_NLONG_WRITE (PIIX4_ATA_DATA, (void *)pSataData->buffer,
                nWords >> 1);

            semStatus = semTake (&pDrvCtrlExt->syncSem[pCtrl->numCtrl],
                        sysClkRateGet() *
                        pDrvCtrlExt->semTimeout[pCtrl->numCtrl]);

            if ((pDrvCtrlExt->intStatus & ATA_STAT_ERR) || (semStatus == ERROR)) 
                {
                ICHSATA_DBG_LOG(DEBUG_CMD,"PIO write ERROR %X %X\n",pDrvCtrlExt->intStatus,
                    semStatus,3,4,5,6);

                goto errExit;
                }

            pSataData->buffer  = (void *)((ULONG)pSataData->buffer + nWords * 2);
            nSectors -= block;
            }
        goto okExit;
        }

    semStatus = semTake (&pDrvCtrlExt->syncSem[pCtrl->numCtrl],
                         sysClkRateGet() *
                         pDrvCtrlExt->semTimeout[pCtrl->numCtrl]);

    if ((pDrv->okDma) && (pFisAta->fisCmd.fisCmdFlag != ATA_FLAG_NON_DATA))
        {
        if (pDrvCtrlExt->ataDmaStop != NULL)
            {
            (*pDrvCtrlExt->ataDmaStop)(pDrvCtrlExt->pDev, pCtrl->numCtrl);
            }

        if ((pFisAta->fisCmd.fisCmdFlag & ATA_FLAG_IN_DATA) != 0x0)
            vxbDmaBufMapSync (pDrvCtrlExt->pDev,
                              pDrv->host->sataHostDmaTag,
                              pDrv->sataDmaMap,
                              0,
                              pSataData->blkNum * pSataData->blkSize,
                              _VXB_DMABUFSYNC_DMA_POSTREAD);

        vxbDmaBufMapUnload(pDrv->host->sataHostDmaTag, pDrv->sataDmaMap);
        }

    if ((pDrvCtrlExt->intStatus & ATA_STAT_ERR) || (semStatus == ERROR))
        {
        ICHSATA_DBG_LOG (DEBUG_CMD,
             "pDrvCtrlExt->intStatus 0x%X semStatus 0x%X\n",
             pDrvCtrlExt->intStatus,semStatus,3,4,5,6);

        if (pDrvCtrlExt->intStatus & ATA_STAT_ERR)
            {
            error = ATA_IO_BYTE_READ (PIIX4_ATA_ERROR);
            if (error & ERR_ABRT)

                /* command couldn't complete, return without retry */

                ICHSATA_DBG_LOG (DEBUG_CMD,
                                 "ataCmd: cmd aborted by device, returning\n"
                                 "  status=0x%x semStatus=%d err=0x%x\n",
                                 pDrvCtrlExt->intStatus, semStatus, error,
                                 0, 0, 0);
            }
        else
            ICHSATA_DBG_LOG (DEBUG_CMD,
                            "ataCmd: error - retrying "
                            " status=0x%x semStatus=%d err=0x%x\n",
                             pDrvCtrlExt->intStatus, semStatus,
                             error, 0, 0, 0);
        goto errExit;
        }
    else
        {
        if((pFisAta->fisCmd.fisCmdFlag == ATA_FLAG_IN_DATA) &&
            (!pDrv->okDma))
            {      
            while (nSectors > 0)
                {
                if ((pDrv->pioMode == ATA_PIO_MULTI) && (nSectors < block))
                    {
                    block = nSectors;
                    nWords = (ATA_SECTOR_SIZE * block) >> 1;
                    }

                if (ataStatusChk(pCtrl, ATA_STAT_DRQ | ATA_STAT_BUSY,
                    ATA_STAT_DRQ | !ATA_STAT_BUSY) != OK)
                    {
                    ICHSATA_DBG_LOG (DEBUG_CMD, "ataStatusChk ERROR \n",1,2,3,4,5,6);
                    goto errExit;
                    }

                ICHSATA_DBG_LOG (DEBUG_CMD, "ataCmdIssue PIO read data \n",1,2,3,4,5,6);

                ATA_IO_NLONG_READ(PIIX4_ATA_DATA, (void *)pSataData->buffer,
                    nWords >> 1);

                pSataData->buffer  = (void *)((ULONG)pSataData->buffer + nWords *2);
                nSectors -= block;
               }
            }
        }

okExit:

    semGive (&pDrvCtrlExt->rwSem[pCtrl->numCtrl]);
    ICHSATA_DBG_LOG (DEBUG_CMD, "ataCmd end - ctrl %d, drive %d cmd 0x%x: Ok\n",
                     pCtrl->numCtrl, pDrv->num, pFisAta->fisCmd.fisAtaCmd[2], 0, 0, 0);
    return(OK);

errExit:

    semGive (&pDrvCtrlExt->rwSem[pCtrl->numCtrl]);
    ICHSATA_DBG_LOG (DEBUG_CMD, "ataCmd end - ctrl %d, drive %d cmd 0x%x: ERROR\n",
                     pCtrl->numCtrl, pDrv->num, pFisAta->fisCmd.fisAtaCmd[2], 0, 0, 0);
    return ERROR;
    }

/*******************************************************************************
*
* ataInit - initialize ATA device.
*
* This routine issues a soft reset command to ATA device for initialization.
*
* RETURNS: OK, ERROR if the command didn't succeed.
*/

LOCAL STATUS ataInit
    (
    SATA_HOST * pCtrl,
    int ctrl,
    int drive
    )
    {
    int        ix;
    int        device_signature;
    STATUS     rc;
    PIIX4_DRV_CTRL *pCtrlExt;

    VXB_ASSERT_NONNULL(pCtrl,ERROR)

    ICHSATA_DBG_LOG (DEBUG_INIT, "ataInit: ctrl=%d\n", ctrl, 0, 0, 0, 0, 0);

    /* Set reset bit */

    pCtrlExt = (PIIX4_DRV_CTRL *)(pCtrl->pCtrlExt);

    ATA_IO_BYTE_WRITE (PIIX4_ATA_D_CONTROL, ATA_CTL_SRST);

    for (ix = 0; ix < 20; ix++)     /* >= 5 mks */
        ATA_DELAY_400NSEC;          /* sysDelay() */

    /* Clear reset bit */

    ATA_IO_BYTE_WRITE (PIIX4_ATA_D_CONTROL, 0);

    for (ix = 0; ix < 5000; ix++)   /* 2 ms */
        ATA_DELAY_400NSEC;   /* sysDelay() */

    pCtrlExt->wdgOkay[ctrl] = TRUE;

    /* Start the ATA  watchdog */

    rc = wdStart (pCtrlExt->wdgId[ctrl],
                 (sysClkRateGet() * pCtrlExt->wdgTimeout[ctrl]),
                 (FUNCPTR)ataWdog, ctrl);
    if (rc != OK)
        {
        pCtrlExt->wdgOkay[ctrl] = FALSE;
        return (ERROR);
        }

    /* Wait for BUSY bit to be cleared */

    while ((ATA_IO_BYTE_READ (PIIX4_ATA_A_STATUS) & ATA_STAT_BUSY)
           && (pCtrlExt->wdgOkay[ctrl]))
        ;

    /* Stop the ATA watchdog */


    rc = wdCancel (pCtrlExt->wdgId[ctrl]);
    if (rc != OK)
        return (rc);

    if (!pCtrlExt->wdgOkay[ctrl])
        {
        ICHSATA_DBG_LOG (DEBUG_INIT, "ataInit error:\n", 0, 0, 0, 0, 0, 0);
        pCtrlExt->wdgOkay[ctrl] = TRUE;
        return(ERROR);
        }

    /*
     * The following allows recovery after an interrupt
     * caused by drive software reset
     */

    rc = semBInit(&pCtrlExt->syncSem[ctrl], SEM_Q_FIFO, SEM_EMPTY);
    if (rc != OK)
        return (rc);

    ICHSATA_DBG_LOG (DEBUG_INIT, "ataInit end\n", 0, 0, 0, 0, 0, 0);

    while ((ATA_IO_BYTE_READ (PIIX4_ATA_A_STATUS) & ATA_STAT_BUSY))
        ;
    device_signature = ((ATA_IO_BYTE_READ(PIIX4_ATA_SECCNT_INTREASON) << 24) |
                        (ATA_IO_BYTE_READ(PIIX4_ATA_SECTOR) << 16)  |
                        (ATA_IO_BYTE_READ(PIIX4_ATA_CYLLOW) << 8)    |
                        ATA_IO_BYTE_READ(PIIX4_ATA_CYLHI));
    if (device_signature == ATA_SIGNATURE)
        {
        ICHSATA_DBG_LOG(DEBUG_INIT, "ataInit: Reset: ATA device=0x%x\n",
                        device_signature,0,0,0,0,0);
        }
    else if (device_signature == ATAPI_SIGNATURE)
        {
        ICHSATA_DBG_LOG(DEBUG_INIT, "ataInit: Reset: ATAPI device=0x%x\n",
                        device_signature,0,0,0,0,0);
        }
    else
        ICHSATA_DBG_LOG(DEBUG_INIT, "ataInit: Reset: unknown device=0x%x\n",
                        device_signature,0,0,0,0,0);

    return(OK);
    }

/*******************************************************************************
*
* ataCtrlReset - reset the specified ATA/IDE disk controller
*
* This routine resets the ATA controller specified by ctrl.
* The device control register is written with SRST=1
*
* RETURNS: OK, ERROR if the command didn't succeed.
*/

LOCAL STATUS ataCtrlReset
    (
    SATA_HOST  * pCtrl,
    int ctrl
    )
    {
    PIIX4_ATA_DRIVE   * pDrive;
    int         drive;
    int         ix;
    UINT8       sts;

    VXB_ASSERT_NONNULL(pCtrl,ERROR)
    ICHSATA_DBG_LOG (DEBUG_INIT,
                     "ataCtrlReset: ctrl=%d\n", ctrl, 0, 0, 0, 0, 0);

    /* Set reset bit */

    ATA_IO_BYTE_WRITE (PIIX4_ATA_D_CONTROL, ATA_CTL_SRST | ATA_CTL_NIEN);

    /* Mask the for loop, no need to wait too long time VXW6-83384 */
    /* for (ix = 0; ix < 100; ix++) */
    sysDelay ();

    ATA_IO_BYTE_WRITE (PIIX4_ATA_D_CONTROL, 0);  /* clear control reg */

    /* Mask the for loop, no need to wait too long time VXW6-83384 */
    /* for (ix = 0; ix < 10000; ix++) */
    sysDelay ();

    /*
     * timeout max 30 seconds for controller to reset
     * if it is busy after this timeout, this is an error
     *
     */

    ix = 0;
    while ( (sts = ATA_IO_BYTE_READ (PIIX4_ATA_STATUS)) & ATA_STAT_BUSY)
        {
        /* Modify to 3 for VXW6-83384 */
        if (++ix == 3)
            {
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "\nController %d reset timeout.  ERROR\n",
                            ctrl, 0, 0, 0, 0, 0);
            return (ERROR);
            }
        taskDelay(sysClkRateGet());
        ICHSATA_DBG_LOG(DEBUG_INIT,"vxbPii4Storage AtaCtrlReset taskDelay      \r", 
                0,0,0,0,0,0);
        ICHSATA_DBG_LOG(DEBUG_INIT,"vxbPii4Storage AtaCtrlReset taskDelay   %d 0x%x\r",
                ix,sts,0,0,0,0);
        }

    for (drive = 0; drive < ATA_MAX_DRIVES; drive++)
        {
        pDrive = (PIIX4_ATA_DRIVE *)pCtrl->sataDev[drive];

        (void)ataDeviceSelect (pCtrl, drive);

        pDrive->signature = ((ATA_IO_BYTE_READ(PIIX4_ATA_SECCNT_INTREASON) << 24)  |
                             (ATA_IO_BYTE_READ(PIIX4_ATA_SECTOR) << 16)            |
                             (ATA_IO_BYTE_READ(PIIX4_ATA_CYLLOW) << 8)             |
                              ATA_IO_BYTE_READ(PIIX4_ATA_CYLHI));


        if (pDrive->signature == ATA_SIGNATURE)
            {
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "ataCtrlReset: Reset: ATA device=0x%x\n",
                            pDrive->signature,0,0,0,0,0);
            pDrive->sataPortDev.type = ATA_TYPE_ATA;
            }
        else if (pDrive->signature == ATAPI_SIGNATURE)
            {
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "ataCtrlReset: Reset: ATAPI device=0x%x\n",
                            pDrive->signature,0,0,0,0,0);
            pDrive->sataPortDev.type = ATA_TYPE_ATAPI;
            }
        else
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "ataCtrlReset: Reset: unknown device=0x%x\n",
                            pDrive->signature,0,0,0,0,0);

        }

    ICHSATA_DBG_LOG (DEBUG_INIT,
                     "ataCtrlReset: returning to caller\n",
                     0, 0, 0, 0, 0, 0);
    return(OK);
    }

/*******************************************************************************
*
* ataDeviceSelect - select the specified ATA/IDE disk controller
*
* This routine select the ATA controller specified by ctrl.
*
* RETURNS: OK, ERROR if the command didn't succeed.
*/

LOCAL STATUS ataDeviceSelect
    (
    SATA_HOST   * pCtrl,
    int         device
    )
    {
    int     i;
    STATUS  status;

    VXB_ASSERT_NONNULL(pCtrl,ERROR)

    /* Write device id to dev/head register to select */

    ATA_IO_BYTE_WRITE (PIIX4_ATA_SDH_D_SELECT, (UINT8)(device << ATA_DRIVE_BIT));

    ATA_WAIT_STATUS;   /* delay at least 400 ns before reading status again */

    i = 0;
    while (i++ < 10000000)
        {
        if (((status = ATA_IO_BYTE_READ (PIIX4_ATA_STATUS)) &
            (ATA_STAT_BUSY | ATA_STAT_DRQ)) == 0)
            {
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "ataDeviceSelect: %d OK - select count = %d\n",
                             pCtrl->numCtrl, i, 0,0,0,0);
            return (OK);   /* device is selected */
            }
        }

    return(ERROR);
    }


/*******************************************************************************
*
* ataStatusChk - Check status of drive and compare to requested status.
*
* Wait until the drive is ready.
*
* RETURNS: OK, ERROR if the drive status check times out.
*/

LOCAL STATUS ataStatusChk
    (
    SATA_HOST  * pCtrl,
    UINT8      mask,
    UINT8      status
    )
    {
    int i = 0;
    UINT8 sts;
    VXB_ASSERT_NONNULL(pCtrl,ERROR)

    ICHSATA_DBG_LOG (DEBUG_NONE,
                    "ataStatusChk %d: addr=0x%x mask=0x%x status=0x%x\n",
                    0, ATA_A_STATUS, mask, status, 0, 0);

    while (((sts = ATA_IO_BYTE_READ(PIIX4_ATA_A_STATUS)) & mask) != status)
        {
            if (++i > 1000000)
            {
                ICHSATA_DBG_LOG (DEBUG_INIT,"ataStatusChk: timed out 0x%x\n",
                             sts, 0, 0, 0, 0, 0);
            return (ERROR);

            }
            else
                ATA_DELAY_400NSEC; /* wait ~400 nanoseconds */
        }
    return (OK);

    }

/*******************************************************************************
*
* ataWdog - ATA/IDE controller watchdog handler.
*
* RETURNS: N/A
*/

LOCAL void ataWdog
    (
    PIIX4_ATA_CTRL * pCtrl
    )
    {
    VXB_ASSERT_NONNULL_V(pCtrl)

    pCtrl->wdgOkay = FALSE;
    }

/*******************************************************************************
*
* ataDevIdentify - identify device
*
* This routine checks whether the device is connected to the controller,
* if it is, this routine determines drive type. The routine set `type'
* field in the corresponding PIIX4_ATA_DRIVE structure. If device identification
* failed, the routine set `state' field in the corresponding PIIX4_ATA_DRIVE
* structure to PIIX4_ATA_DEV_NONE.
*
* RETURNS: TRUE if a device present, FALSE otherwise
*/

LOCAL STATUS ataDevIdentify
    (
    SATA_HOST  * pCtrl,
    int ctrl,
    int dev
    )
    {
    VXB_ASSERT_NONNULL(pCtrl,ERROR)

    PIIX4_ATA_DRIVE * pDrive  = pCtrl->sataDev[dev];
    if (pDrive == NULL)
        return ERROR;

    if ((pDrive->signature != ATA_SIGNATURE) &&
        (pDrive->signature != ATAPI_SIGNATURE))
        {
        pDrive->sataPortDev.type = 0;
        pDrive->signature = 0xffffffff;
        ICHSATA_DBG_LOG(DEBUG_INIT,
                        "ichAtaDeviceIdentify: Unknown device found on %d/%d\n",
                        ctrl, dev, 0,0,0,0);
        return(ERROR);
        }

    /* Select device */

    if (ataDeviceSelect(pCtrl, dev) != OK)
        {
        ICHSATA_DBG_LOG(DEBUG_INIT,
                        "ataDevIdentify: device %d not selected.  ERROR\n",
                        dev, 0,0,0,0,0);
        return(ERROR);
        }

    /*
     * Find if the device is present or not before trying to find if it is
     * ATA or ATAPI. For this one logic is to write some value to a R/W
     * register in the drive and read it back. If we get correct value,
     * device is present other wise not. But due to the capacitance of the
     * bus (IDE bus), some times we get the correct value even the device is
     * not present. To solve this problem, write a value to one R/W register
     * in the drive, and write other value to another R/W register in the
     * drive. Now read the first register.
     */

    ATA_IO_BYTE_WRITE (PIIX4_ATA_SECCNT_INTREASON, 0xaa);
    ATA_IO_BYTE_WRITE (PIIX4_ATA_SECTOR, 0x55);

    if (ATA_IO_BYTE_READ (PIIX4_ATA_SECCNT_INTREASON) == 0xaa)
        {

        /*
         * device is present, now find out the type.
         * The signature field has already been filled in by the
         * ataCtrlReset() and is used to get device type.
         */

        if (pDrive->signature == ATAPI_SIGNATURE)
            {
            pDrive->sataPortDev.type = ATA_TYPE_ATAPI;
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "ataDeviceIdentify: Found ATAPI device on %d/%d  ",
                            ctrl, dev, 0,0,0,0);
            return(OK);
            }
        else if (pDrive->signature == ATA_SIGNATURE)
            {
            pDrive->sataPortDev.type = ATA_TYPE_ATA;
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "ataDeviceIdentify: Found ATA device on %d/%d  ",
                            ctrl, dev, 0,0,0,0);
            return(OK);
            }
        else
            {
            pDrive->sataPortDev.type = 0;
            pDrive->signature = 0;
            ICHSATA_DBG_LOG(DEBUG_INIT,
                            "ataDeviceIdentify: Unknown device found on %d/%d\n",
                            ctrl, dev, 0,0,0,0);
            return(ERROR);
            }
        }
    else
        {
        pDrive->sataPortDev.type = 0;
        pDrive->signature = 0xffffffff;
        ICHSATA_DBG_LOG(DEBUG_INIT,
                        "ataDeviceIdentify: Unknown device found on %d/%d\n",
                        ctrl, dev, 0,0,0,0);
        return(ERROR);
        }
    }

/*******************************************************************************
*
* ataDmaCtrlInit - Initialize the PCI IDE DMA controller.
*
* Init the IDE DMA controller.
*
* RETURNS: OK or ERROR if memory not avaialable.
*/

LOCAL STATUS ataDmaCtrlInit
    (
    VXB_DEVICE_ID pDev,
    int ctrl
    )
    {
    struct vxbPciDevice * pPciDev = (struct vxbPciDevice *)pDev->pBusSpecificDevInfo;
    VXB_DEVICE_ID  pParentDev = vxbDevParent(pDev);
    PIIX4_DRV_CTRL *pCtrl = pDev->pDrvCtrl;
    PCI_IDE_DMA_CTL * pDmactl =  &pCtrl->Piix4DMACtl;
    STATUS rc;

    rc = vxbPciConfigInLong (pParentDev, pPciDev->pciBus, pPciDev->pciDev,
                             pPciDev->pciFunc,
                             PCI_CFG_BMIBA, (UINT32 *)&pDmactl->ioBaseAddress);
    if (rc != OK)
        return (rc);

    vxbRegMap (pDev, 4, &pDmactl->pPiix4Handler);

    ICHSATA_DBG_LOG (DEBUG_DMA,"PCI_CFG_BMIBA = %#x \n", pDmactl->ioBaseAddress,
                     0, 0, 0, 0, 0);

    pDmactl->ioBaseAddress &= I82371AB_BMBIA_ADD_MASK;

    if (pDmactl->pPRDTable[ctrl] == NULL)
        {

        /* Allocate memory on a DWORD boundary */
        /* use same value for size to avoid passing 64K boundary */

        pDmactl->pPRDTable[ctrl] = (UINT32 *)cacheDma32Malloc(I82371AB_MAC_512);

        ICHSATA_DBG_LOG (DEBUG_DMA,"ideController.pPRDTable[ctrl] = %p \n ",
                         pDmactl->pPRDTable[ctrl], 0, 0, 0, 0, 0);

        if (pDmactl->pPRDTable[ctrl] != NULL)
            {

            /* Put the PRD Table pointer in BMIDTP */

            I82371AB_SYS_OUT32 (pDmactl,
                                (UINT32 *)(ULONG)I82371AB_BMIDTPadd(pDmactl,ctrl),
                                (UINT32)(VXB_DMA_VIRT_TO_PHYS(pDmactl->pPRDTable[ctrl])));
            }
        else
            return(ERROR);
        }
    else
        {
        ICHSATA_DBG_LOG(DEBUG_DMA,
                        "IDE controller %d is already initialized\n",
                        ctrl, 0, 0, 0, 0, 0);
        }
    ICHSATA_DBG_LOG (DEBUG_DMA,"pPRDT = %p \n ", pDmactl->pPRDTable[ctrl],
                     0, 0, 0, 0, 0);

    return(OK);
    }

/*******************************************************************************
*
* ataDmaEngineSet - configure the DMA engine before start transfer.
*
* This function initializes IDE controller busmaster for Bus master DMA
* operation.
* Sets PCI config registers of IDE interface, sets bus master interface
* registers. Builds PRD table for the required length of data trasferred
* in the specified direction.
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS ataDmaEngineSet
    (
    VXB_DEVICE_ID  pDev,
    int            ctrl,
    int            drive,
    char           * pBuf,
    UINT32         bufLength,
    int            direction
    )
    {
    struct vxbPciDevice * pPciDev = (struct vxbPciDevice *)pDev->pBusSpecificDevInfo;
    VXB_DEVICE_ID  pParentDev = vxbDevParent(pDev);
    PIIX4_DRV_CTRL *pCtrl = pDev->pDrvCtrl;
    PCI_IDE_DMA_CTL * pDmactl =  &pCtrl->Piix4DMACtl;
    STATUS rc;

    ICHSATA_DBG_LOG (DEBUG_DMA,
                     "ataDmaEngineSet() entered.\n",0,0,0,0,0,0);

    /* Initialize pciConfig Registers */

    pDmactl->pciHeaderCommand = (I82371AB_PCISTS_BME | I82371AB_PCISTS_IOSE);

    rc = vxbPciConfigOutWord (pParentDev, pPciDev->pciBus, pPciDev->pciDev,
                              pPciDev->pciFunc,
                              PCI_CFG_COMMAND, pDmactl->pciHeaderCommand);
    if (rc != OK)
        return (rc);

    pDmactl->ideTim[ctrl] = (I82371AB_IDETIM_ENE |
                             I82371AB_IDETIM_PPE0 | I82371AB_IDETIM_IE0);

    rc = vxbPciConfigOutWord (pParentDev, pPciDev->pciBus, pPciDev->pciDev,
                              pPciDev->pciFunc,
                              PCI_CFG_IDETIM(ctrl), pDmactl->ideTim[ctrl]);
    if (rc != OK)
        return (rc);

    /* update I/O space Registers */

    pDmactl->bmiCom[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                             (UINT8 *)(ULONG)
                                             I82371AB_BMICOMadd(pDmactl,ctrl));
    pDmactl->bmiSta[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                             (UINT8 *)(ULONG)
                                             I82371AB_BMISTAadd(pDmactl,ctrl));
    pDmactl->bmiDtp[ctrl] = (UINT32)I82371AB_SYS_IN32(pDmactl,
                                                      (UINT32 *)(ULONG)
                                                      I82371AB_BMIDTPadd(pDmactl,ctrl));

    /* clear Start/Stop bus master bit */

    I82371AB_SYS_OUT8 (pDmactl,
                       (UINT8 *)(ULONG)I82371AB_BMICOMadd (pDmactl, ctrl),
                       (UINT8)(pDmactl->bmiCom[ctrl] & 0xFE));

    I82371AB_DELAY ();

    /* clear interrupts if any */

    I82371AB_SYS_OUT8 (pDmactl,
                       (UINT8 *)(ULONG)I82371AB_BMISTAadd (pDmactl, ctrl),
                       pDmactl->bmiSta[ctrl] | 0x66);

    /* Print I/O space Registers */

    ICHSATA_DBG_LOG(DEBUG_DMA,
                    "\n       bmiCom[%d] = %#x \n"
                    "       bmiSta[%d] = %#x \n"
                    "       bmiDtp[%d] = %#x \n",
                    ctrl,
                    I82371AB_SYS_IN8(pDmactl, (UINT8 *)(ULONG)
                                     I82371AB_BMICOMadd(pDmactl,ctrl)),
                    ctrl,
                    I82371AB_SYS_IN8(pDmactl, (UINT8 *)(ULONG)
                                     I82371AB_BMISTAadd(pDmactl,ctrl)),
                    ctrl,
                    I82371AB_SYS_IN32(pDmactl, (UINT32 *)(ULONG)
                                      I82371AB_BMIDTPadd(pDmactl,ctrl)));

    ataDmaPRDTblBuild (pDev, ctrl, pBuf, bufLength);

    pDmactl->bmiCom[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                             (UINT8 *)(ULONG)
                                             I82371AB_BMICOMadd(pDmactl, ctrl));

    ICHSATA_DBG_LOG (DEBUG_DMA,
                     "I82371AB_BMICOMadd(%d)= %#x\n",
                     ctrl, pDmactl->bmiCom[ctrl],
                     0, 0, 0, 0);

    pDmactl->bmiCom[ctrl] = (UINT8)(pDmactl->bmiCom[ctrl] |
                            ((direction == OUT_DATA) ? 0x00 : I82371AB_RWCON));

    I82371AB_SYS_OUT8(pDmactl,
                      (UINT8 *)(ULONG)I82371AB_BMICOMadd(pDmactl, ctrl),
                      pDmactl->bmiCom[ctrl]);

    return(OK);
    }

/*******************************************************************************
*
* ataDmaPRDTblBuild - Build the PRD Table.
*
* This function fills PRD table with the given buffer pointer.
*
* RETURNS: OK, or ERROR
*/

LOCAL STATUS ataDmaPRDTblBuild
    (
    VXB_DEVICE_ID pDev,
    int           ctrl,
    char          * pBuf,
    UINT32        bufLength
    )
    {
    UINT32 i;
    UINT32 * pPRDT;
    PIIX4_DRV_CTRL * pCtrl = pDev->pDrvCtrl;
    PCI_IDE_DMA_CTL * pDmactl = &pCtrl->Piix4DMACtl;

    if ((pBuf == NULL) || (bufLength == 0x0))
        {
        ICHSATA_DBG_LOG (DEBUG_DMA,
                         "PRDTblBuild: buffer problems\n"
                         "   ctrl=%d  pBuf=0x%x  bufLength=0x%x\n",
                         ctrl, (int)pBuf, bufLength, 0, 0, 0);
        return(ERROR);
        }

    pPRDT = pDmactl->pPRDTable[ctrl];

    ICHSATA_DBG_LOG (DEBUG_DMA,
                    "ataDmaPRDTblBuild() entered.\n"
                    " pPRDT     = %p \n"
                    " pBuf      = %p \n"
                    " bufLength = %#x \n",
                    pDmactl->pPRDTable[ctrl], pBuf, bufLength, 0, 0, 0);
    I82371AB_SYS_OUT32 (pDmactl,
                        (UINT32 *)(ULONG)I82371AB_BMIDTPadd (pDmactl, ctrl),
                        (UINT32) (VXB_DMA_VIRT_TO_PHYS (pPRDT)));

    /* Fill PRD Table */

    i = ((~((UINT32)(ULONG)pBuf & 0xffff)) + 1) & 0xffff;

    ICHSATA_DBG_LOG (DEBUG_DMA, "i = %#x \n", i, 0, 0, 0, 0, 0);
    
    if ((bufLength <= i) || ((i == 0) && (bufLength <= I82371AB_MAC_64_K)))
        {
        *pPRDT = (UINT32) (ULONG)pBuf;
        *(pPRDT+1) = ((bufLength & 0xffff) | 0x80000000);
        bufLength = 0;
        }
    else if (i != 0)
        {
        *(pPRDT++) = (UINT32) (ULONG)pBuf;
        bufLength = bufLength - i;
        *(pPRDT++) = i;
        pBuf += i;
        }

    while (bufLength > I82371AB_MAC_64_K)
        {
        *(pPRDT++) = (UINT32) (ULONG)pBuf;
        *(pPRDT++) = 0x00; /* I82371AB_MAC_64_K; */
        pBuf += I82371AB_MAC_64_K;
        bufLength -= I82371AB_MAC_64_K;
        } 

    if (bufLength)
        {
        *(pPRDT++) = (UINT32) (ULONG)pBuf;
        *(pPRDT) = (bufLength | 0x80000000);
        } 

    ICHSATA_DBG_LOG (DEBUG_CMD,
                     "Built PRD table - Returning Ok\n",
                     0, 0, 0, 0, 0, 0);
    return(OK);

    }

/*******************************************************************************
*
* ataDmaStartEngine - start bus master operation of Ide controller.
*
* This function starts the bus master operation of IDE controller. It checks
* if the IDE controller is already in operation, and if not it set the IDE
* command register for start bus master operation.
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS ataDmaStartEngine
    (
    VXB_DEVICE_ID pDev,
    int           ctrl
    )
    {
    PIIX4_DRV_CTRL * pCtrl = pDev->pDrvCtrl;
    PCI_IDE_DMA_CTL * pDmactl =  &pCtrl->Piix4DMACtl;

    ICHSATA_DBG_LOG (DEBUG_DMA,"ataDmaStartEngine() entered.\n",0,0,0,0,0,0);

    if ((I82371AB_SYS_IN8 (pDmactl,
                           (UINT8 *)(ULONG)I82371AB_BMISTAadd(pDmactl,ctrl)) &
                           I82371AB_BMIDE_ACTIVE) == 0)
        {
        pDmactl->bmiCom[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                                 (UINT8 *)(ULONG)
                                                 I82371AB_BMICOMadd(pDmactl, ctrl));

        pDmactl->bmiCom[ctrl] |= I82371AB_SSBM;

        I82371AB_SYS_OUT8(pDmactl,
                          (UINT8 *)(ULONG)I82371AB_BMICOMadd(pDmactl, ctrl),
                          pDmactl->bmiCom[ctrl]);
        ICHSATA_DBG_LOG (DEBUG_DMA,"I82371AB_BMICOM(%d) = %#x\n",
                         ctrl,
                         I82371AB_SYS_IN8(pDmactl,I82371AB_BMICOMadd(pDmactl, ctrl)),
                         0, 0, 0, 0);
        return(OK);
        }
    return(ERROR);
    }

/*******************************************************************************
*
* ataDmaStopEngine - stop bus master operation.
*
* This function stops bus master operation.
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS ataDmaStopEngine
    (
    VXB_DEVICE_ID pDev,
    int           ctrl
    )
    {
    PIIX4_DRV_CTRL * pCtrl = pDev->pDrvCtrl;
    PCI_IDE_DMA_CTL * pDmactl =  &pCtrl->Piix4DMACtl;

    ICHSATA_DBG_LOG (DEBUG_DMA,"ataDmaStopEngine()entered.\n",
                      0, 0, 0, 0, 0, 0);

    pDmactl->bmiCom[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                             (UINT8 *)(ULONG)
                                             I82371AB_BMICOMadd(pDmactl,ctrl));
    pDmactl->bmiSta[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                             (UINT8 *)(ULONG)
                                             I82371AB_BMISTAadd(pDmactl,ctrl));
    pDmactl->bmiDtp[ctrl] = (UINT32)I82371AB_SYS_IN32(pDmactl,
                                                      (UINT32 *)(ULONG)
                                                      I82371AB_BMIDTPadd(pDmactl,ctrl));
    ICHSATA_DBG_LOG(DEBUG_DMA,
                    "\n       bmiCom[%d] = %#x \n"
                    "       bmiSta[%d] = %#x \n"
                    "       bmiDtp[%d] = %#x \n",
                    ctrl, pDmactl->bmiCom[ctrl],
                    ctrl, pDmactl->bmiSta[ctrl],
                    ctrl, pDmactl->bmiDtp[ctrl]);

    I82371AB_SYS_OUT8 (pDmactl, (UINT8 *)(ULONG)I82371AB_BMISTAadd(pDmactl,ctrl),
                           pDmactl->bmiSta[ctrl] | 0x05);
    I82371AB_DELAY();

    I82371AB_SYS_OUT8 (pDmactl,
                       (UINT8 *)(ULONG)I82371AB_BMICOMadd(pDmactl,ctrl), 0x00);

    /* Print I/O space Registers */

    pDmactl->bmiCom[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                             (UINT8 *)(ULONG)
                                             I82371AB_BMICOMadd(pDmactl,ctrl));
    pDmactl->bmiSta[ctrl] = I82371AB_SYS_IN8(pDmactl,
                                             (UINT8 *)(ULONG)
                                             I82371AB_BMISTAadd(pDmactl,ctrl));
    pDmactl->bmiDtp[ctrl] = (UINT32)I82371AB_SYS_IN32(pDmactl,
                                                      (UINT32 *)(ULONG)
                                                      I82371AB_BMIDTPadd(pDmactl,ctrl));
    ICHSATA_DBG_LOG(DEBUG_DMA,
                    "\n       bmiCom[%d] = %#x \n"
                    "       bmiSta[%d] = %#x \n"
                    "       bmiDtp[%d] = %#x \n",
                    ctrl, pDmactl->bmiCom[ctrl],
                    ctrl, pDmactl->bmiSta[ctrl],
                    ctrl, pDmactl->bmiDtp[ctrl]);
    return(OK);
    }

/*******************************************************************************
*
* ataDmaModeNegotiate - Get DMA mode supported by controller
*
* This function returns Host controller supported UDMA/DMA mode. It takes
* input as the read/write mode supported by device and checks with the host
* supported mode, and returns the least mode supported by both device and
* host.
*
* RETURNS: rwMode
*/

LOCAL short ataDmaModeNegotiate
    (
    short rwmode
    )
    {
    switch (rwmode)
        {
        case ATA_DMA_ULTRA_5:
        case ATA_DMA_ULTRA_4:
        case ATA_DMA_ULTRA_3:
        case ATA_DMA_ULTRA_2:
            rwmode = ATA_DMA_ULTRA_2;
            break;
        case ATA_DMA_ULTRA_1:
        case ATA_DMA_ULTRA_0:
        case ATA_DMA_MULTI_2:
        case ATA_DMA_MULTI_1:
        case ATA_DMA_MULTI_0:
        case ATA_PIO_4:
        case ATA_PIO_3:
        case ATA_PIO_2:
        case ATA_PIO_1:
        case ATA_PIO_0:
        default:
            break;
        }

    return(rwmode);
    }

/*******************************************************************************
*
* ataDmaModeSet - set the host controller for required R/W mode.
*
* This function sets the host controllers configuration registers to
* required read/write UDMA/DMA mode.
*
* RETURNS: OK
*/

LOCAL STATUS ataDmaModeSet
    (
    VXB_DEVICE_ID pDev,
    int ctrl,
    int drive,
    short rwmode
    )
    {
    PIIX4_DRV_CTRL * pCtrl = pDev->pDrvCtrl;
    PCI_IDE_DMA_CTL * pDmactl = &pCtrl->Piix4DMACtl;
    struct vxbPciDevice * pPciDev = (struct vxbPciDevice *)
                                    pDev->pBusSpecificDevInfo;
    VXB_DEVICE_ID pParentDev = vxbDevParent (pDev);

    UINT16 temp = 0x00;
    STATUS rc;

    switch (rwmode)
        {
        case ATA_DMA_ULTRA_5:
        case ATA_DMA_ULTRA_4:
        case ATA_DMA_ULTRA_3:
        case ATA_DMA_ULTRA_2:
        case ATA_DMA_ULTRA_1:
        case ATA_DMA_ULTRA_0:
            rc = vxbPciConfigInByte (pParentDev, pPciDev->pciBus, 
                                     pPciDev->pciDev, pPciDev->pciFunc,
                                     PCI_CFG_UDMACTL,
                                     (UINT8 *) &pDmactl->uDmaCtl);
            if (rc != OK)
                return (rc);
            pDmactl->uDmaCtl |= (UINT8) (0x01 << (ctrl * 2 + drive));

            rc = vxbPciConfigOutByte (pParentDev, pPciDev->pciBus, 
                                      pPciDev->pciDev, pPciDev->pciFunc, 
                                      PCI_CFG_UDMACTL, pDmactl->uDmaCtl);
            if (rc != OK)
                return (rc);
            rc = vxbPciConfigInWord (pParentDev, pPciDev->pciBus, 
                                     pPciDev->pciDev, pPciDev->pciFunc, 
                                     PCI_CFG_UDMATIM,
                                     (UINT16 *) &pDmactl->uDmaTime);
            if (rc != OK)
                return (rc);

            pDmactl->uDmaTime &= (UINT16) (~(0x03 << ((ctrl * 2 + drive) * 4)));
            switch (rwmode)
                {
                case ATA_DMA_ULTRA_5:
                case ATA_DMA_ULTRA_3:
                case ATA_DMA_ULTRA_1:
                    temp = 0x01;
                    break;
                case ATA_DMA_ULTRA_4:
                case ATA_DMA_ULTRA_2:
                    temp = 0x02;
                    break;
                case ATA_DMA_ULTRA_0:
                default:
                    break;
                }
            pDmactl->uDmaTime |= (UINT16) (temp << ((ctrl * 2 + drive) * 4));
            ICHSATA_DBG_LOG (DEBUG_DMA, "uDmaTime = %#x \n",
                             pDmactl->uDmaTime, 0, 0, 0,
                             0, 0);

            rc = vxbPciConfigOutWord (pParentDev, pPciDev->pciBus, 
                                      pPciDev->pciDev, pPciDev->pciFunc,
                                      PCI_CFG_UDMATIM, pDmactl->uDmaTime);
            if (rc != OK)
                return (rc);

        case ATA_DMA_MULTI_2:
        case ATA_DMA_MULTI_1:
        case ATA_DMA_MULTI_0:
        case ATA_PIO_4:
        case ATA_PIO_3:
        case ATA_PIO_2:
        case ATA_PIO_1:
        case ATA_PIO_0:
        default:
            break;
        }
    return(OK);
    }

