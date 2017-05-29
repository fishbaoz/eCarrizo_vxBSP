/* rtl8169VxbEnd.c - RealTek 8139C+/8101E/816x/811x VxBus Ethernet driver */

/*
 * Copyright (c) 2006-2010, 2012, 2013 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
16dec13,p_x Correct transaction size when reading PCIe capability ID 
                 register(WIND00444940)
02o,16sep13,xms  fix CHECKED_RETURN error. (WIND00414265)
02n,05jun12,y_c  Fix some static check errors. (WIND00350512)
02m,16apr10,wap  Add support for RTL8168DP
02l,05mar10,h_k  fixed compile errors.
02k,17feb10,jc0  LP64 adaptation.
02j,09sep09,j_z  Unset RTG_RXCFG_RX_ERRPKT bit of register RTG_RXCFG(WIND00179020)
02i,28apr09,wap  Correct TX checksum offload workaround to handle UDP frames
                 correctly
02h,09mar09,wap  Correct multicast filter setup for RTL8103 chip
02g,02feb09,wap  Don't select jumbo buffer sizes for DMA maps unless jumbo
                 frame support is actually enabled (WIND00146049), add
                 support for RTL8103EL
02f,11aug08,wap  Add support for the RTL8168C, RTL8168CP, RTL8168D,
                 RTL8102E and RTL8102EL controllers
02e,10jul08,tor  update version
02d,16jun08,pmr  resource documentation
02c,13jun08,wap  converted busCfgRead to vxbPciDevCfgRead.
02b,25sep07,tor  VXB_VERSION_3
02a,20sep07,dlk  In rtgLinkUpdate(), do not call muxError() or muxTxRestart()
                 directly (WIND00100800). Also, support the EIOCGRCVJOBQ ioctl.
01z,05sep07,ami  apigen related changes
01y,20aug07,wap  Increase PHY access timeout (WIND00101785)
01x,25jul07,wap  Fix WIND000099316 (typo, rxqueue -> rxQueue)
01w,06jul07,wap  Make this driver SMP safe
01v,13jun07,tor  remove VIRT_ADDR
01u,30may07,dlk  Fix for checksum offload under IPNet, which asks for
                 CSUM_TCP or CSUM_UDP, but not CSUM_IP.
01s,29mar07,tor  update methods
01t,20mar07,wap  Move vxbIntConnect() to MuxConnect() method
01s,13mar07,wap  Install ISR later during driver setup
01r,26feb07,wap  Call muxError() when out of mBlks in RX handler
01q,29jan07,wap  Correct problem with jumbo frame support on original
                 RTL8169S, correct RX fixup failure on MIPS arch, add
                 workaround for TCP checksum offload bug in RTL8111B/81168B
                 and RTL8101E chipsets
01p,07dec06,wap  Add jumbo frame support
01o,04dec06,wap  Make sure RFC2233 MIB table exists before trying to update it
01n,30nov06,wap  Correct multicast filter programming for PCIe parts
01m,17oct06,pdg  replaced VXB_METHOD_DRIVER_UNLINK with an address
01l,25sep06,wap  Make sure RFC2233 MIB table exists before trying to update it
01k,21aug06,wap  Harden polled mode support
01i,16nov06,wap  Merge in updates from development branch
01h,25oct06,wap  Make sure RFC2233 MIB table exists before trying to update it
01g,05sep06,kch  Removed unnecessary coreip header includes.
01j,31jul06,wap  Make sure rtgEndTxHandle() keeps the TX channel moving
01i,29jul06,wap  Work around TX checksum offload bug
01h,18jul06,wap  Use RTG_ADJ() only after m_len has been set
01g,14jul06,wap  Avoid alignment error in rtgEndRxConfig(), fix a couple of
                 32-bit accesses to ISR/IMR registers that should have been
                 16-bit
01f,03jul06,wap  Allow ifSpeed to be set to gigE speeds
01e,28jun06,wap  Correct revision code for RTL8111B
01d,17jun06,wap  Only transmit when link is up, use memory mapped BAR
01c,12jun06,wap  Use _func_m2PollStatsIfPoll function pointer.
01b,07jun06,wap  Clean up some documentation
01a,15may06,wap  written
*/

/*
DESCRIPTION
This module implements a driver for the RealTek 'C+' family of PCI
10/100 and 10/100/1000 ethernet controllers. The C+ family is fully
compliant with the IEEE 802.3 10Base-T and 100Base-T specifications.
The original 8169 chip required an external GMII-compliant PHY, however
the 8139C+ and all subsequent chip revs have a copper transceiver
built in.

Unlike the original 8139 family, the C+ series of controllers use
a standard descriptor-based DMA scheme for host/device data transfer.
The first RealTek device to use this mechanism was the 8139C+, which
supported both the original 8139 DMA API and the new API. The 8169
and all later devices use the descriptor-based mechanism only.

There are a couple of minor differences between the original 8139C+
and the 8169 and later devices: the 8139C+ only allows a maximum of
64 descriptors per DMA ring, has a couple of its registers at
different offsets, and uses the original 8139 PHY register access
scheme (the later devices use a single PHY access register). The length
fields in the 8139C+'s DMA descriptors are also slightly smaller, since
it does not handle jumbo frames. All other aspects of the device API
are otherwise identical to the 8169 and later devices.

Note that starting with the RTL8168C/RTL8111C and RTL8102E devices,
the RX and TX DMA descriptor formats have changed a little. The RX
descriptors can now report if a frame contains an IPv4 or IPv4 packet,
with either TCP or UDP transport. In the TX descriptors, the bits
that control TCP/IP hardware checksum generation have been moved from
the command/status word to the VLAN control word. Some of the devices
in this class also support ASF.

All devices in the C+ family support TCP/IP checksum offload (for IPv4),
hardware VLAN tag stripping and insertion and TCP large send. The
gigE devices also support jumbo frames, but only up to 7.5Kb in size.
This driver makes use of the checksum offload and VLAN tagging/stripping
features.

The following is a list of devices supported by this driver:

\tb RealTek 8139C+ 10/100 PCI
\tb ReakTek 8169 10/100/1000 PCI (first rev, external PHY)
\tb RealTek 8169S/8110S 10/100/1000 PCI (integrated PHY)
\tb RealTek 8169SB/8110SB 10/100/1000 PCI (integrated PHY)
\tb RealTek 8169SC/8110SC 10/100/1000 PCI (integrated PHY)
\tb RealTek 8168B/8111B 10/100/1000 PCIe (integrated PHY)
\tb RealTek 8168C/8111C 10/100/1000 PCIe (integrated PHY)
\tb RealTek 8168CP/8111CP 10/100/1000 PCIe (integrated PHY)
\tb RealTek 8168D/8111D 10/100/1000 PCIe (integrated PHY)
\tb RealTek 8168DL/8111DL 10/100/1000 PCIe (integrated PHY)
\tb RealTek 8168DP/8111DP 10/100/1000 PCIe (integrated PHY)
\tb RealTek 8101E 10/100 PCIe (integrated PHY)
\tb RealTek 8102E 10/100 PCIe (integrated PHY)
\tb RealTek 8102EL 10/100 PCIe (integrated PHY)
\tb RealTek 8103EL 10/100 PCIe (integrated PHY)

BOARD LAYOUT
The RealTek 81xx family comprises PCI, PCIe and cardbus controllers.
The PCI devices are available in both standalone PCI card format
and integrated directly onto the system main board. All configurations
are jumperless.

EXTERNAL INTERFACE
The driver provides a vxBus external interface. The only exported
routine is the rtgRegister() function, which registers the driver
with VxBus.

The RealTek gigE devices also support jumbo frames. Note however that
the maximum MTU possible is 7400 bytes (not 9000, which is normal for
most jumbo-capable NICs). This driver supports jumbo frames on the
8169S/8110S, 8169SB/8110SB, 8169SC/8110SC and 8168B/8111B devices.
They are not supported on the 8139C+, 8100E, 8101E, 8102E and 8103E
devices, which are 10/100 only, nor on the original 8169 (MAC only,
no internal PHY) which doesn't seem to be jumbo-capable. (The first
generation 8169 is no longer in production however, so this should
not be a problem for new designs.)
 
Jumbo frame support is disabled by default in order to conserve memory
(jumbo frames require the use of an buffer pool with larger clusters).
Jumbo frames can be enabled on a per-interface basis using a parameter
override entry in the hwconf.c file in the BSP. For example, to enable
jumbo frame support for interface yn0, the following entry should be
added to the VXB_INST_PARAM_OVERRIDE table:

    { "rtg", 0, "jumboEnable", VXB_PARAM_INT32, {(void *)1} }

INCLUDE FILES:
rtl8139VxbEnd.h end.h endLib.h netBufLib.h muxLib.h

SEE ALSO: vxBus, ifLib
\tb RealTek 8139C+ Programming Manual, http://www.realtek.com.tw
\tb RealTek 8169S/8110S Programming Manual, http://www.realtek.com.tw
\tb RealTek 8168S/8111S Programming Manual, http://www.realtek.com.tw
\tb RealTek 8101E Programming Manual, http://www.realtek.com.tw
*/


#include <vxWorks.h>
#include <intLib.h>
#include <muxLib.h>
#include <netLib.h>
#include <netBufLib.h>
#include <semLib.h>
#include <sysLib.h>
#include <taskLib.h>
#include <vxBusLib.h>
#include <wdLib.h>
#include <etherMultiLib.h>
#include <end.h>
#define END_MACROS
#include <endLib.h>
#include <endMedia.h>
#include <vxAtomicLib.h>

#include <hwif/vxbus/vxBus.h>
#include <hwif/vxbus/hwConf.h>
#include <hwif/vxbus/vxbPciLib.h>
#include <hwif/util/vxbDmaBufLib.h>
#include <hwif/util/vxbParamSys.h>
#include <../src/hwif/h/mii/miiBus.h>
#include <../src/hwif/h/vxbus/vxbAccess.h>
#include <../src/hwif/h/hEnd/hEnd.h>
#include <private/funcBindP.h>

/* Need this for PCI config space register definitions */

#include <drv/pci/pciConfigLib.h>

#include <../src/hwif/h/mii/mv88E1x11Phy.h>
#include <../src/hwif/h/mii/rtl8169Phy.h>

#include "rtl8169VxbEndA.h"


LOCAL VXB_DEVICE_ID pDevTemp[10];
LOCAL int CurrentRtgNum;

/* defines */

#ifdef	RTG_LOGMSG
#undef	RTG_LOGMSG
#endif

#define RTG_LOGMSG(x,a,b,c,d,e,f)               \
    do {                                        \
        if (_func_logMsg != NULL)               \
            _func_logMsg (x,                    \
                          (_Vx_usr_arg_t)a,     \
                          (_Vx_usr_arg_t)b,     \
                          (_Vx_usr_arg_t)c,     \
                          (_Vx_usr_arg_t)d,     \
                          (_Vx_usr_arg_t)e,     \
                          (_Vx_usr_arg_t)f);    \
        } while (FALSE)

/* temporary */
LOCAL void rtgDelay (UINT32);
IMPORT STATUS vxbNextUnitGet (VXB_DEVICE_ID);
IMPORT void vxbUsDelay (int);

IMPORT FUNCPTR _func_m2PollStatsIfPoll;

/* VxBus methods */

LOCAL void	rtgInstInit (VXB_DEVICE_ID);
LOCAL void	rtgInstInit2 (VXB_DEVICE_ID);
LOCAL void	rtgInstConnect (VXB_DEVICE_ID);
LOCAL STATUS	rtgInstUnlink (VXB_DEVICE_ID, void *);
LOCAL BOOL	rtgProbe (VXB_DEVICE_ID);

/* miiBus methods */

LOCAL STATUS	rtgPhyRead (VXB_DEVICE_ID, UINT8, UINT8, UINT16 *);
LOCAL STATUS    rtgPhyWrite (VXB_DEVICE_ID, UINT8, UINT8, UINT16);
LOCAL STATUS	rtgGmiiPhyRead (VXB_DEVICE_ID, UINT8, UINT8, UINT16 *);
LOCAL STATUS    rtgGmiiPhyWrite (VXB_DEVICE_ID, UINT8, UINT8, UINT16);
LOCAL STATUS    rtgLinkUpdate (VXB_DEVICE_ID);

/* mux methods */

LOCAL void	rtgMuxConnect (VXB_DEVICE_ID, void *);

LOCAL struct drvBusFuncs rtgFuncs =
    {
    rtgInstInit,	/* devInstanceInit */
    rtgInstInit2,	/* devInstanceInit2 */
    rtgInstConnect	/* devConnect */
    };

LOCAL struct vxbDeviceMethod rtgMethods[] =
   {
   DEVMETHOD(miiRead,		rtgPhyRead),
   DEVMETHOD(miiWrite,		rtgPhyWrite),
   DEVMETHOD(miiMediaUpdate,	rtgLinkUpdate),
   DEVMETHOD(muxDevConnect,	rtgMuxConnect),
   DEVMETHOD(vxbDrvUnlink,	rtgInstUnlink),
   { 0, 0 }
   };   

/*
 * List of supported device IDs.
 */

LOCAL struct vxbPciID rtgPciDevIDList[] =
    {
        /* { devID, vendID } */
        { RTG_DEVICEID_8139, RTG_VENDORID }, /* for 8139 C+ only */
        { RTG_DEVICEID_8169, RTG_VENDORID },
        { RTG_DEVICEID_8169SC, RTG_VENDORID },
        { RTG_DEVICEID_8168, RTG_VENDORID },
        { RTG_DEVICEID_8101E, RTG_VENDORID },
        { DLINK_DEVICEID_528T, DLINK_VENDORID },
        { COREGA_DEVICEID_CGLAPCIGT, COREGA_VENDORID },
        { LINKSYS_DEVICEID_EG1032, LINKSYS_VENDORID },
        { USR_DEVICEID_8169, USR_VENDORID },
    };

/* default queue parameters */
LOCAL HEND_RX_QUEUE_PARAM rtgRxQueueDefault = {
    NULL,                       /* jobQueId */
    0,                          /* priority */
    0,                          /* rbdNum */
    0,                          /* rbdTupleRatio */
    0,                          /* rxBufSize */
    NULL,                       /* pBufMemBase */
    0,                          /* rxBufMemSize */
    0,                          /* rxBufMemAttributes */
    NULL,                       /* rxBufMemFreeMethod */
    NULL,                       /* pRxBdBase */
    0,                          /* rxBdMemSize */
    0,                          /* rxBdMemAttributes */
    NULL                        /* rxBdMemFreeMethod */
};

LOCAL HEND_TX_QUEUE_PARAM rtgTxQueueDefault = {
    NULL,                       /* jobQueId */
    0,                          /* priority */
    0,                          /* tbdNum */
    0,                          /* allowedFrags */
    NULL,                       /* pTxBdBase */
    0,                          /* txBdMemSize */
    0,                          /* txBdMemAttributes */
    NULL                        /* txBdMemFreeMethod */
};

LOCAL VXB_PARAMETERS rtgParamDefaults[] =
    {
       {"rxQueue00", VXB_PARAM_POINTER, {(void *)&rtgRxQueueDefault}},
       {"txQueue00", VXB_PARAM_POINTER, {(void *)&rtgTxQueueDefault}},
       {"jumboEnable", VXB_PARAM_INT32, {(void *)0}},
        {NULL, VXB_PARAM_END_OF_LIST, {NULL}}
    };

LOCAL struct vxbPciRegister rtgDevPciRegistration =
    {
        {
        NULL,			/* pNext */
        VXB_DEVID_DEVICE,	/* devID */
        VXB_BUSID_PCI,		/* busID = PCI */
        VXB_VER_5_0_0,  	/* vxbVersion */
        RTG_NAME,		/* drvName */
        &rtgFuncs,		/* pDrvBusFuncs */
        rtgMethods,		/* pMethods */
        rtgProbe,		/* devProbe */
        rtgParamDefaults	/* pParamDefaults */
        },
    NELEMENTS(rtgPciDevIDList),
    rtgPciDevIDList
    };

/* Driver utility functions */

LOCAL void      rtgEeAddrSet (VXB_DEVICE_ID, UINT32);
LOCAL void      rtgEeWordGet (VXB_DEVICE_ID, UINT32, UINT16 *);
LOCAL void      rtgEepromRead (VXB_DEVICE_ID, UINT8 *, int, int);
LOCAL STATUS	rtgReset (VXB_DEVICE_ID);
#ifdef RTG_RX_FIXUP
LOCAL void	rtgRxFixup (M_BLK_ID);
#endif

/* END functions */

LOCAL END_OBJ	*rtgEndLoad (char *, void *);
LOCAL STATUS	rtgEndUnload (END_OBJ *);
LOCAL int	rtgEndIoctl (END_OBJ *, int, caddr_t);
LOCAL STATUS	rtgEndMCastAddrAdd (END_OBJ *, char *);
LOCAL STATUS	rtgEndMCastAddrDel (END_OBJ *, char *);
LOCAL STATUS	rtgEndMCastAddrGet (END_OBJ *, MULTI_TABLE *);
LOCAL void	rtgEndHashTblPopulate (RTG_DRV_CTRL *);
LOCAL STATUS	rtgEndStatsDump (RTG_DRV_CTRL *);
LOCAL void	rtgEndRxConfig (RTG_DRV_CTRL *);
LOCAL STATUS	rtgEndStart (END_OBJ *);
LOCAL STATUS	rtgEndStop (END_OBJ *);
LOCAL int	rtgEndSend (END_OBJ *, M_BLK_ID);
LOCAL STATUS	rtgEndPollSend (END_OBJ *, M_BLK_ID);
LOCAL int	rtgEndPollReceive (END_OBJ *, M_BLK_ID);
LOCAL void	rtgEndInt (RTG_DRV_CTRL *);
LOCAL void	rtgEndRxHandle (void *);
LOCAL void	rtgEndTxHandle (void *);
LOCAL void	rtgEndIntHandle (void *);

LOCAL NET_FUNCS rtgNetFuncs =
    {
    rtgEndStart,                        /* start func. */
    rtgEndStop,                         /* stop func. */
    rtgEndUnload,                       /* unload func. */
    rtgEndIoctl,                        /* ioctl func. */
    rtgEndSend,                         /* send func. */
    rtgEndMCastAddrAdd,                 /* multicast add func. */
    rtgEndMCastAddrDel,                 /* multicast delete func. */
    rtgEndMCastAddrGet,                 /* multicast get fun. */
    rtgEndPollSend,                     /* polling send func. */
    rtgEndPollReceive,                  /* polling receive func. */
    endEtherAddressForm,                /* put address info into a NET_BUFFER */
    endEtherPacketDataGet,              /* get pointer to data in NET_BUFFER */
    endEtherPacketAddrGet               /* Get packet addresses */
    };

/*****************************************************************************
*
* rtgRegister - register with the VxBus subsystem
*
* This routine registers the RealTek driver with VxBus as a
* child of the PCI bus type.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

void rtgRegister(void)
    {
    vxbDevRegister((struct vxbDevRegInfo *)&rtgDevPciRegistration);
    return;
    }

/*****************************************************************************
*
* rtgInstInit - VxBus instInit handler
*
* This function implements the VxBus instInit handler for an rtg
* device instance. The only thing done here is to select a unit
* number for the device.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgInstInit
    (
    VXB_DEVICE_ID pDev
    )
    {
    vxbNextUnitGet (pDev);
    return;
    }

/*****************************************************************************
*
* rtgProbe - VxBus device probe routine
*
* This function probes the RealTek device to verify it really is one that
* we want to support with this driver. There are two special cases we want
* to check for. Although we support the 8139C+ chip, we don't support any
* other devices in the 8139 family, so we test for the 8139C+ hardware
* revision code before accepting an 8139 device. Also, there are several
* versions of the Linksys EG1032 board: only rev3 has a RealTek chip,
* so we need to test for that one explicitly.
*
* RETURNS: TRUE if we want to handle the device, else FALSE
*
* ERRNO: N/A
*/

LOCAL BOOL rtgProbe
    (
    VXB_DEVICE_ID pDev
    )
    {
    UINT16 venId = 0;
    UINT16 devId = 0;
    UINT16 subDevId = 0;
    UINT32 hwRev;
    int i;
    void * bar;
    void * handle;

    VXB_PCI_BUS_CFG_READ (pDev, PCI_CFG_VENDOR_ID, 2, venId);
    VXB_PCI_BUS_CFG_READ (pDev, PCI_CFG_DEVICE_ID, 2, devId);
    VXB_PCI_BUS_CFG_READ (pDev, LINKSYS_SUBDEVICE_EG1032_REV3, 2, subDevId);

    for (i = 0; i < VXB_MAXBARS; i++)
        {
        if (pDev->regBaseFlags[i] == VXB_REG_MEM)
            break;
        }

    /* Should never happen. */

    if (i == VXB_MAXBARS)
        return (FALSE);

    vxbRegMap (pDev, i, &handle);
    bar = pDev->pRegBase[i];

    hwRev = vxbRead32 (handle, (UINT32 *)((char *)bar + RTG_TXCFG));

    hwRev &= RTG_TXCFG_HWREV;

    vxbRegUnmap(pDev, i);

    /*
     * Don't match a device with the 8139 device ID unless it's
     * an 8139 C+ style chip. The non-C+ devices have their own
     * driver.
     */

    if (devId == RTG_DEVICEID_8139 && hwRev != RTG_HWREV_8139CPLUS)
        return (FALSE);

    /*
     * Only match the LinkSys EG1032 rev3 board. The rev2 board
     * uses a Marvell chipset.
     */

    if (venId == LINKSYS_VENDORID && devId == LINKSYS_DEVICEID_EG1032 &&
        subDevId != LINKSYS_SUBDEVICE_EG1032_REV3)
        return (FALSE);

    return (TRUE);
    }

/*****************************************************************************
*
* rtgInstInit2 - VxBus instInit2 handler
*
* This function implements the VxBus instInit2 handler for an rtg
* device instance. Once we reach this stage of initialization, it's
* safe for us to allocate memory, so we can create our pDrvCtrl
* structure and do some initial hardware setup. The important
* steps we do here are to create a child miiBus instance, connect
* our ISR to our assigned interrupt vector, read the station
* address from the EEPROM, and set up our vxbDma tags and memory
* regions. We need to allocate a 64K region for the RX DMA window
* here.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgInstInit2
    (
    VXB_DEVICE_ID pDev
    )
    {
    RTG_DRV_CTRL *pDrvCtrl;
    VXB_INST_PARAM_VALUE val;
    UINT32 hwRev;
    UINT16 devId;
    ULONG lowAddr;
    UINT8 pciCfgType = 0;
    int i;

    pDrvCtrl = malloc (sizeof(RTG_DRV_CTRL));
    if (pDrvCtrl == NULL)
        {
        logMsg("rtg %d: could not allocate device control memory\n",
            pDev->unitNumber, 0,0,0,0,0);
        return;
        }			
    bzero ((char *)pDrvCtrl, sizeof(RTG_DRV_CTRL));
    pDev->pDrvCtrl = pDrvCtrl;
    pDrvCtrl->rtgDev = pDev;

    /* to check the PCI configuration space, whether this is a PCI express */
    VXB_PCI_BUS_CFG_READ(pDev, RTG_PCI_PCIE_CAP_OFFSET, 1, pciCfgType); 

    if (pciCfgType == PCI_EXT_CAP_EXP)
        lowAddr = VXB_SPACE_MAXADDR;
    else
        lowAddr = VXB_SPACE_MAXADDR_32BIT;

    /*
     * We want to choose the memory mapped BAR. Usually this is
     * BAR 1, however with PCIe devices, it's sometimes BAR 2.
     */

    for (i = 0; i < VXB_MAXBARS; i++)
        {
        if (pDev->regBaseFlags[i] == VXB_REG_MEM)
            break;
        }

    /* Should never happen. */

    if (i == VXB_MAXBARS)
        return;

    vxbRegMap (pDev, i, &pDrvCtrl->rtgHandle);
    pDrvCtrl->rtgBar = pDev->pRegBase[i];

    /* Reset the device */

    rtgReset (pDev);

    /* Figure out the device type */

    hwRev = CSR_READ_4 (pDev, RTG_TXCFG) & RTG_TXCFG_HWREV;
    pDrvCtrl->rtgHwRev = hwRev;

    logMsg("rtg %d: hwRev %08x\n",
        pDev->unitNumber, pDrvCtrl->rtgHwRev,0,0,0,0);

    switch (hwRev)
        {
        case RTG_HWREV_8139CPLUS:
            pDrvCtrl->rtgDevType = RTG_DEVTYPE_8139CPLUS;
            pDrvCtrl->rtgRxDescCnt = RTG_RX_DESC_8139;
            pDrvCtrl->rtgTxDescCnt = RTG_TX_DESC_8139;
            pDrvCtrl->rtgRxLenMask = RTG_RDESC_STAT_FRAGLEN;
            pDrvCtrl->rtgTxStartReg = RTG_TXPRIOPOLL_8139;
            pDrvCtrl->rtgDescV2 = FALSE;
            break;
        case RTG_HWREV_8168_SPIN1:
        case RTG_HWREV_8168_SPIN2:
        case RTG_HWREV_8168_SPIN3:
        case RTG_HWREV_8169:
        case RTG_HWREV_8169S:
        case RTG_HWREV_8110S:
        case RTG_HWREV_8169_8110SC:
        case RTG_HWREV_8169_8110SB:
        case RTG_HWREV_8169_8110SBL:
        case RTG_HWREV_8100E:
        case RTG_HWREV_8101E:
        case RTG_HWREV_8168E:
        case 0x4C000000:
            pDrvCtrl->rtgDevType = RTG_DEVTYPE_8169;
            pDrvCtrl->rtgRxDescCnt = RTG_RX_DESC_8169;
            pDrvCtrl->rtgTxDescCnt = RTG_TX_DESC_8169;
            pDrvCtrl->rtgRxLenMask = RTG_RDESC_STAT_GFRAGLEN;
            pDrvCtrl->rtgTxStartReg = RTG_TXPRIOPOLL_8169;
            pDrvCtrl->rtgDescV2 = FALSE;

            logMsg("rtg %d detect OK\n", pDev->unitNumber, 0,0,0,0,0);
            break;
        case RTG_HWREV_8102E:
        case RTG_HWREV_8102EL:
        case RTG_HWREV_8103EL:
        case RTG_HWREV_8168C:
        case RTG_HWREV_8168C_SPIN2:
        case RTG_HWREV_8168CP:
        case RTG_HWREV_8168D:
        case RTG_HWREV_8168DP:
            pDrvCtrl->rtgDevType = RTG_DEVTYPE_8169;
            pDrvCtrl->rtgRxDescCnt = RTG_RX_DESC_8169;
            pDrvCtrl->rtgTxDescCnt = RTG_TX_DESC_8169;
            pDrvCtrl->rtgRxLenMask = RTG_RDESC_STAT_GFRAGLEN;
            pDrvCtrl->rtgTxStartReg = RTG_TXPRIOPOLL_8169;
            pDrvCtrl->rtgDescV2 = TRUE;

            /*
             * If this device has ASF enabled, turn it off. ASF-capable
             * devices seem to automatically send out special SNMP trap
             * (beacon) frames every few seconds. We want to avoid this,
             * since it puts extra chatter on the wire. So far this has
             * been observed only on the RTL8168CP, but it may show up on
             * other chips in the future.
             */

            if (CSR_READ_2(pDev, RTG_CPLUSCMD) & RTG_CPCMD_ASF_EN)
                {
                CSR_WRITE_1(pDev, RTG_EECMD, RTG_EEMODE_WRITECFG);
                CSR_CLRBIT_1(pDev, RTG_CFG2, RTG_CFG2_ASF_EN);
                CSR_WRITE_1(pDev, RTG_EECMD, RTG_EEMODE_OFF);
                }

            /*
             * If this is an RTL8168DP, we must notify the management
             * processor that the driver is taking over control of the
             * MAC.
             */

            if (hwRev == RTG_HWREV_8168DP)
                {
                CSR_WRITE_4(pDev, RTG_ERIDR, RTG_FW_DRV_LOAD);
                CSR_WRITE_4(pDev, RTG_ERIAR, RTG_FW_ERIADDR);
                CSR_WRITE_4(pDev, RTG_OCPDR, RTG_FW_DRV_LOAD);
                CSR_WRITE_4(pDev, RTG_OCPAR, RTG_FW_OCPADDR);
                }

            break;
        default:
            break;
        }

    /* Determine EEPROM size. */

    pDrvCtrl->rtgEeWidth = 6;
    rtgEepromRead (pDev, (UINT8 *)&devId, 0, 1);
    if (devId != htole16(RTG_EE_SIGNATURE))
        pDrvCtrl->rtgEeWidth = 8;

    /* Save the station address */

    rtgEepromRead (pDev, (UINT8 *)&devId, 0, 1);

    /*
     * If this is an RTL8168DP, then there is no attached
     * EEPROM: the MAC is directly programmed into the MAC
     * by setting fuse bits. If reading the EEPROM fails
     * completely, then just recover the MAC address from
     * the RX filter registers.
     */

    if (devId != htole16(RTG_EE_SIGNATURE))
        {
        UINT32 addr[2];
        addr[0] =  CSR_READ_4(pDev, RTG_IDR0);
        addr[0] = le32toh(addr[0]);
        addr[1] =  CSR_READ_4(pDev, RTG_IDR1);
        addr[1] = le32toh(addr[1]);
        bcopy ((char *)addr, (char *)pDrvCtrl->rtgAddr, ETHER_ADDR_LEN);
        }
    else
    rtgEepromRead (pDev, pDrvCtrl->rtgAddr, RTG_EE_EADDR, 3);

    pDrvCtrl->rtgDevSem = semMCreate (SEM_Q_PRIORITY|
        SEM_DELETE_SAFE|SEM_INVERSION_SAFE);

    /* Create our MII bus. */

    miiBusCreate (pDev, &pDrvCtrl->rtgMiiBus);
    miiBusMediaListGet (pDrvCtrl->rtgMiiBus, &pDrvCtrl->rtgMediaList);
    miiBusModeSet (pDrvCtrl->rtgMiiBus,
         pDrvCtrl->rtgMediaList->endMediaListDefault);

    /* Get a reference to our parent tag. */

    pDrvCtrl->rtgParentTag = vxbDmaBufTagParentGet (pDev, 0);

    /*
     * Now create our DMA tags and allocate resources.
     * The C+ class devices use the 'contiguous array'
     * descriptor layout model. Descriptors are 16 bytes
     * in size, which is not a common cache line size.
     * Consequently, we allocate the descriptor rings from
     * uncached memory. The RealTek also requires that the
     * DMA rings be allocated at a 256-byte aligned address.
     */

    /* Create tag for RX descriptor ring. */

    pDrvCtrl->rtgRxDescTag = vxbDmaBufTagCreate (pDev,
        pDrvCtrl->rtgParentTag,		/* parent */
        256,				/* alignment */
	0,				/* boundary */
	lowAddr,			/* lowaddr */ 
	VXB_SPACE_MAXADDR,		/* highaddr */
	NULL,				/* filter */
	NULL,				/* filterarg */
	sizeof(RTG_DESC) * pDrvCtrl->rtgRxDescCnt,	/* max size */
	1,				/* nSegments */
        sizeof(RTG_DESC) * pDrvCtrl->rtgRxDescCnt,	/* max seg size */
        VXB_DMABUF_ALLOCNOW|VXB_DMABUF_NOCACHE,		/* flags */
	NULL,				/* lockfunc */
	NULL,				/* lockarg */
	NULL);				/* ppDmaTag */

    pDrvCtrl->rtgRxDescMem = vxbDmaBufMemAlloc (pDev,
        pDrvCtrl->rtgRxDescTag, NULL, 0, &pDrvCtrl->rtgRxDescMap);

    /* Create tag for TX descriptor ring. */

    pDrvCtrl->rtgTxDescTag = vxbDmaBufTagCreate (pDev,
        pDrvCtrl->rtgParentTag,		/* parent */
        256,				/* alignment */
	0,				/* boundary */
	lowAddr,			/* lowaddr */ 
	VXB_SPACE_MAXADDR,		/* highaddr */
	NULL,				/* filter */
	NULL,				/* filterarg */
	sizeof(RTG_DESC) * pDrvCtrl->rtgTxDescCnt,	/* max size */
	1,				/* nSegments */
        sizeof(RTG_DESC) * pDrvCtrl->rtgTxDescCnt,	/* max seg size */
        VXB_DMABUF_ALLOCNOW|VXB_DMABUF_NOCACHE,		/* flags */
	NULL,				/* lockfunc */
	NULL,				/* lockarg */
	NULL);				/* ppDmaTag */

    pDrvCtrl->rtgTxDescMem = vxbDmaBufMemAlloc (pDev,
        pDrvCtrl->rtgTxDescTag, NULL, 0, &pDrvCtrl->rtgTxDescMap);

    /*
     * See if the user wants jumbo frame support for this
     * interface. If the "jumboEnable" option isn't specified,
     * or it's set to 0, then jumbo support stays off,
     * otherwise we switch it on.
     *
     * Note: the 8139C+, 8100E and 8101E parts don't support
     * jumbo frames, as they're 10/100 only. The original 8169
     * part doesn't seem support them either.
     */

    /*
     * paramDesc {
     * The jumboEnable parameter specifies whether
     * this instance should support jumbo frames.
     * The default is false. } 
     */
    i = vxbInstParamByNameGet (pDev, "jumboEnable", VXB_PARAM_INT32, &val);

    if (i != OK || val.int32Val == 0 ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8139CPLUS ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8100E ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8101E ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8102E ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8102EL ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8103EL ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8169)
        {
        i = RTG_CLSIZE;
        pDrvCtrl->rtgMaxMtu = RTG_MTU;
        }
    else
        {
        i = END_JUMBO_CLSIZE;
        pDrvCtrl->rtgMaxMtu = RTG_JUMBO_MTU;
        }

    /* Create tag for mBlk mappings */

    pDrvCtrl->rtgMblkTag = vxbDmaBufTagCreate (pDev,
        pDrvCtrl->rtgParentTag,         /* parent */
        32,                             /* alignment */
        0,                              /* boundary */
        lowAddr,			/* lowaddr */ 
        VXB_SPACE_MAXADDR,              /* highaddr */
        NULL,                           /* filter */
        NULL,                           /* filterarg */
        (bus_size_t)i,                  /* max size */
        RTG_MAXFRAG,                    /* nSegments */
        (bus_size_t)i,                  /* max seg size */
        VXB_DMABUF_ALLOCNOW,            /* flags */
        NULL,                           /* lockfunc */
        NULL,                           /* lockarg */
        NULL);                          /* ppDmaTag */

    /* Create DMA maps for mblks. */

    pDrvCtrl->rtgTxMblkMap = malloc(sizeof(VXB_DMA_MAP_ID) *
        pDrvCtrl->rtgTxDescCnt);
    pDrvCtrl->rtgRxMblkMap = malloc(sizeof(VXB_DMA_MAP_ID) *
        pDrvCtrl->rtgRxDescCnt);
    pDrvCtrl->rtgTxMblk = malloc(sizeof(M_BLK_ID) * pDrvCtrl->rtgTxDescCnt);
    pDrvCtrl->rtgRxMblk = malloc(sizeof(M_BLK_ID) * pDrvCtrl->rtgRxDescCnt);

    for (i = 0; i < pDrvCtrl->rtgTxDescCnt; i++)
        {
        if (vxbDmaBufMapCreate (pDev, pDrvCtrl->rtgMblkTag, 0,
            &pDrvCtrl->rtgTxMblkMap[i]) == NULL)
            RTG_LOGMSG("create Tx map %d failed\n", i, 0,0,0,0,0);
        }

    for (i = 0; i < pDrvCtrl->rtgRxDescCnt; i++)
        {
        if (vxbDmaBufMapCreate (pDev, pDrvCtrl->rtgMblkTag, 0,
            &pDrvCtrl->rtgRxMblkMap[i]) == NULL)
            RTG_LOGMSG("create Rx map %d failed\n", i, 0,0,0,0,0);
        }
    return;
    }

LOCAL void SaveRtgVxbDeviceId
    (
    VXB_DEVICE_ID pTemp
    )
    {
    pDevTemp[CurrentRtgNum++] = pTemp;
    }

/*****************************************************************************
*
* rtgInstConnect -  VxBus instConnect handler
*
* This function implements the VxBus instConnect handler for an rtg
* device instance.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgInstConnect
    (
    VXB_DEVICE_ID pDev
    )
    {
	SaveRtgVxbDeviceId(pDev);
    return;
    }

/*****************************************************************************
*
* rtgInstUnlink -  VxBus unlink handler
*
* This function shuts down an rtg device instance in response to an
* unlink event from VxBus. This may occur if our VxBus instance has
* been terminated, or if the rtg driver has been unloaded. When an
* unlink event occurs, we must shut down and unload the END interface
* associated with this device instance and then release all the
* resources allocated during instance creation, such as vxbDma
* memory and maps, and interrupt handles. We also must destroy our
* child miiBus and PHY devices.
*
* RETURNS: OK if device was successfully destroyed, otherwise ERROR
*
* ERRNO: N/A
*/

LOCAL STATUS rtgInstUnlink
    (
    VXB_DEVICE_ID pDev,
    void * unused
    )
    { 
    RTG_DRV_CTRL * pDrvCtrl;
    int i;

    pDrvCtrl = pDev->pDrvCtrl;

    /*
     * Stop the device and detach from the MUX.
     * Note: it's possible someone might try to delete
     * us after our vxBus instantiation has completed,
     * but before anyone has called our muxConnect method.
     * In this case, there'll be no MUX connection to
     * tear down, so we can skip this step.
     */

    if (pDrvCtrl->rtgMuxDevCookie != NULL)
        {
        if (muxDevStop (pDrvCtrl->rtgMuxDevCookie) != OK)
            return (ERROR);

        /* Detach from the MUX. */

        if (muxDevUnload (RTG_NAME, pDev->unitNumber) != OK)
            return (ERROR);
        }

    /*
     * If this is an RTL8168DP, we must notify the management
     * processor that the driver is releasing control of the MAC.
     */

    if (pDrvCtrl->rtgHwRev == RTG_HWREV_8168DP)
        {
        CSR_WRITE_4(pDev, RTG_ERIDR, RTG_FW_DRV_EXIT);
        CSR_WRITE_4(pDev, RTG_ERIAR, RTG_FW_ERIADDR);
        CSR_WRITE_4(pDev, RTG_OCPDR, RTG_FW_DRV_EXIT);
        CSR_WRITE_4(pDev, RTG_OCPAR, RTG_FW_OCPADDR);
        }

    /* Release the memory we allocated for the DMA rings */

    vxbDmaBufMemFree (pDrvCtrl->rtgRxDescTag, pDrvCtrl->rtgRxDescMem,
        pDrvCtrl->rtgRxDescMap);
    vxbDmaBufMemFree (pDrvCtrl->rtgTxDescTag, pDrvCtrl->rtgTxDescMem,
        pDrvCtrl->rtgTxDescMap);

    for (i = 0; i < pDrvCtrl->rtgRxDescCnt; i++)
        vxbDmaBufMapDestroy (pDrvCtrl->rtgMblkTag, pDrvCtrl->rtgRxMblkMap[i]);

    for (i = 0; i < pDrvCtrl->rtgTxDescCnt; i++)
        vxbDmaBufMapDestroy (pDrvCtrl->rtgMblkTag, pDrvCtrl->rtgTxMblkMap[i]);

    free (pDrvCtrl->rtgRxMblk);
    free (pDrvCtrl->rtgTxMblk);
    free (pDrvCtrl->rtgRxMblkMap);
    free (pDrvCtrl->rtgTxMblkMap);

    /* Destroy the tags. */

    vxbDmaBufTagDestroy (pDrvCtrl->rtgRxDescTag);
    vxbDmaBufTagDestroy (pDrvCtrl->rtgTxDescTag);
    vxbDmaBufTagDestroy (pDrvCtrl->rtgMblkTag);

    /* Disconnect the ISR. */

    vxbIntDisconnect (pDev, 0, rtgEndInt, pDrvCtrl);

    /* Destroy our MII bus and child PHYs. */

    miiBusDelete (pDrvCtrl->rtgMiiBus);

    semDelete (pDrvCtrl->rtgDevSem);

    /* Destroy the adapter context. */
    free (pDrvCtrl);
    pDev->pDrvCtrl = NULL;

    /* Goodbye cruel world. */

    return (OK);
    }

/*****************************************************************************
*
* rtgGmiiPhyRead - miiBus miiRead method for 8169 devices
*
* This is a helper routine for rtgPhyRead() used for all devices except
* for the original 8139C+. The C+ 10/100 chip uses the same shortcut PHY
* register mechanism as the 8139/8100, however the 8169 and later use
* an indirect PHY register access mechanism. This is because the first
* rev 8169 devices used external PHY chips and had to provide the host
* with a way to access the MDIO interface.
*
* RETURNS: ERROR if invalid PHY addr or register is specified or the
* read times out, else OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgGmiiPhyRead
    (
    VXB_DEVICE_ID pDev,
    UINT8 phyAddr,
    UINT8 regAddr,
    UINT16 *dataVal
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    UINT32 rval;
    int i;

    /* Pretend the PHY is only at address 1. */

    if (phyAddr != 1)
        {
        *dataVal = 0xFFFF;
        return (ERROR);
        }

    /* Let the PHY driver read the GMEDIASTAT register */

    if (regAddr == RTG_GMEDIASTAT)
        {
        *dataVal = CSR_READ_1(pDev, RTG_GMEDIASTAT);
        return (OK);
        }

    pDrvCtrl = pDev->pDrvCtrl;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);

    CSR_WRITE_4(pDev, RTG_PHYAR, regAddr << 16);

    for (i = 0; i < RTG_TIMEOUT * 10; i++)
        {
        rval = CSR_READ_4(pDev, RTG_PHYAR);
        if (rval & RTG_PHYAR_BUSY)
            break;
        rtgDelay(100);
        }

    semGive (pDrvCtrl->rtgDevSem);
    
    if (i == (RTG_TIMEOUT * 10))
        {
        RTG_LOGMSG("%s%d: phy read timed out\n", RTG_NAME,
            pDev->unitNumber, 0, 0, 0, 0);
        *dataVal = 0xFFFF;
        return (ERROR);
        }

    *dataVal = (UINT16)(rval & RTG_PHYAR_PHYDATA);

    return (OK);
    }

/*****************************************************************************
*
* rtgGmiiPhyWrite - miiBus miiWrite method for 8169 devices
*
* This is a helper routine for rtgPhyWrite() used for all devices except
* for the original 8139C+. Like the rtgGmiiPhyRead() routine above, it's
* used for 8169 and later devices.
*
* RETURNS: ERROR if invalid PHY addr or register is specified or the
* read times out, else OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgGmiiPhyWrite
    (
    VXB_DEVICE_ID pDev,
    UINT8 phyAddr,
    UINT8 regAddr,
    UINT16 dataVal
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    UINT32 rval;
    int i;

    /* Pretend the PHY is only at address 1. */

    if (phyAddr != 1)
        return (ERROR);

    pDrvCtrl = pDev->pDrvCtrl;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);

    CSR_WRITE_4(pDev, RTG_PHYAR, (regAddr << 16) |
        (dataVal & RTG_PHYAR_PHYDATA) | RTG_PHYAR_BUSY);

    for (i = 0; i < RTG_TIMEOUT * 10; i++)
        {
        rval = CSR_READ_4(pDev, RTG_PHYAR);
        if (!(rval & RTG_PHYAR_BUSY))
            break;
        rtgDelay(100);
        }

    semGive (pDrvCtrl->rtgDevSem);
    
    if (i == (RTG_TIMEOUT * 10))
        {
        RTG_LOGMSG("%s%d: phy write timed out\n", RTG_NAME,
            pDev->unitNumber, 0, 0, 0, 0);
        return (ERROR);
        }

    return (OK);
    }

/*****************************************************************************
*
* rtgPhyRead - miiBus miiRead method
*
* This function implements an miiRead() method that allows PHYs
* on the miiBus to access our MII management registers. The RealTek
* has an embedded PHY but does not provide the usual bit-bang MDIO
* access mechanism. Instead, there are shotcut registers for accessing
* the control, status, autoneg advertisement, partner ability and
* autoneg expansion registers. This routine uses these registers to
* make it appear that the controller has an miiBus with a single PHY
* at MII address 0.
*
* RETURNS: ERROR if invalid PHY addr or register is specified, else OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgPhyRead
    (
    VXB_DEVICE_ID pDev,
    UINT8 phyAddr,
    UINT8 regAddr,
    UINT16 *dataVal
    )
    {
    STATUS rval = OK;
    UINT16 rtgReg = 0;
    RTG_DRV_CTRL * pDrvCtrl;

    pDrvCtrl = pDev->pDrvCtrl;

    if (pDrvCtrl->rtgDevType == RTG_DEVTYPE_8169)
        return (rtgGmiiPhyRead (pDev, phyAddr, regAddr, dataVal));

    /* Pretend the PHY is only at address 0. */

    if (phyAddr != 0)
        {
        *dataVal = 0xFFFF;
        return (ERROR);
        }

    switch (regAddr)
        {
        case MII_CTRL_REG:
            rtgReg = RTG_BMCR;
            break;
        case MII_STAT_REG:
            rtgReg = RTG_BMSR;
            break;
        /*
         * Return fake values for the ID registers.
         * This allows the rtgPhy driver to pretend to
         * explicitly support this device.
         */

        case MII_PHY_ID1_REG:
            *dataVal = RTG_VENDORID;
            return (OK);
            break; 
        case MII_PHY_ID2_REG:
            *dataVal = RTG_DEVICEID_8139;
            return (OK);
            break; 
        case MII_AN_ADS_REG:
            rtgReg = RTG_ANAR;
            break;
        case MII_AN_PRTN_REG:
            rtgReg = RTG_ANLPAR;
            break;
        case MII_AN_EXP_REG:
            rtgReg = RTG_ANER;
            break;

        /*
         * Allow the PHY driver to read the media status
         * register. If we have a link partner which does not
         * support NWAY, this is the register which will tell
         * us the results of parallel detection.
         */

        case RTG_MEDIASTAT:
            *dataVal = CSR_READ_1(pDev, RTG_MEDIASTAT);
            return (OK);
            break;
        default:
            rval = ERROR;
            break; 
        }

    if (rval == OK)
        *dataVal = CSR_READ_2(pDev, rtgReg);
    else
        *dataVal = 0x0;

    return (rval);
    }

/*****************************************************************************
*
* rtgPhyWrite - miiBus miiRead method
*
* This function implements an miiWrite() method that allows PHYs
* on the miiBus to access our MII management registers. This routine
* works in much the same way as rtgPhyRead(), using the shortcut
* PHY management registers to make it look like there's a single
* PHY at MII address 0.
*
* RETURNS: ERROR if invalid PHY addr or register is specified, else OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgPhyWrite
    (
    VXB_DEVICE_ID pDev,
    UINT8 phyAddr,
    UINT8 regAddr,
    UINT16 dataVal
    )
    {
    STATUS rval = OK;
    UINT16 rtgReg;
    RTG_DRV_CTRL * pDrvCtrl;

    pDrvCtrl = pDev->pDrvCtrl;

    if (pDrvCtrl->rtgDevType == RTG_DEVTYPE_8169)
        return (rtgGmiiPhyWrite (pDev, phyAddr, regAddr, dataVal));

    /* Pretend the PHY is only at address 0. */

    if (phyAddr != 0)
        return (ERROR);

    switch (regAddr)
        {
        case MII_CTRL_REG:
            rtgReg = RTG_BMCR;
            break;
        case MII_AN_ADS_REG:
            rtgReg = RTG_ANAR;
            break;
        case MII_AN_PRTN_REG:
            rtgReg = RTG_ANLPAR;
            break;
        case MII_AN_EXP_REG:
            rtgReg = RTG_ANER;
            break;
        default:
            rval = ERROR;
            break; 
        }

    if (rval == OK)
        CSR_WRITE_2(pDev, rtgReg, dataVal);

    return (rval);
    }

/*****************************************************************************
*
* rtgLinkUpdate - miiBus miiLinkUpdate method
*
* This function implements an miiLinkUpdate() method that allows
* miiBus to notify us of link state changes. This routine will be
* invoked by the miiMonitor task when it detects a change in link
* status. Normally, the miiMonitor task checks for link events every
* two seconds. However, we may be invoked sooner since the RealTek
* supports a link change interrupt. This allows us to repond to
* link events instantaneously.
*
* Once we determine the new link state, we will announce the change
* to any bound protocols via muxError(). We also update the ifSpeed
* fields in the MIB2 structures so that SNMP queries can detect the
* correct link speed.
*
* RETURNS: ERROR if obtaining the new media setting fails, else OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgLinkUpdate
    (
    VXB_DEVICE_ID pDev
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    UINT32 oldStatus;

    if (pDev->pDrvCtrl == NULL)
        return (ERROR);

    pDrvCtrl = (RTG_DRV_CTRL *)pDev->pDrvCtrl;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);

    if (pDrvCtrl->rtgMiiBus == NULL)
        {
        semGive (pDrvCtrl->rtgDevSem);
        return (ERROR);
        }

    oldStatus = pDrvCtrl->rtgCurStatus;
    if (miiBusModeGet(pDrvCtrl->rtgMiiBus,
        &pDrvCtrl->rtgCurMedia, &pDrvCtrl->rtgCurStatus) == ERROR)
        {
        semGive (pDrvCtrl->rtgDevSem);
        return (ERROR);
        }

    switch (IFM_SUBTYPE(pDrvCtrl->rtgCurMedia))
        {
        case IFM_1000_T:
            pDrvCtrl->rtgEndObj.mib2Tbl.ifSpeed = 100000000;
            break;
        case IFM_100_TX:
            pDrvCtrl->rtgEndObj.mib2Tbl.ifSpeed = 100000000;
            break;
        case IFM_10_T:
            pDrvCtrl->rtgEndObj.mib2Tbl.ifSpeed = 10000000;
            break;
        default:
            pDrvCtrl->rtgEndObj.mib2Tbl.ifSpeed = 0;
            break;
        }

    if (pDrvCtrl->rtgEndObj.pMib2Tbl != NULL)
        pDrvCtrl->rtgEndObj.pMib2Tbl->m2Data.mibIfTbl.ifSpeed =
            pDrvCtrl->rtgEndObj.mib2Tbl.ifSpeed;

    if (!(pDrvCtrl->rtgEndObj.flags & IFF_UP))
        {
        semGive (pDrvCtrl->rtgDevSem);
        return (OK);
        }

    /* If status went from down to up, announce link up. */

    if (pDrvCtrl->rtgCurStatus & IFM_ACTIVE && !(oldStatus & IFM_ACTIVE))
	jobQueueStdPost (pDrvCtrl->rtgJobQueue, NET_TASK_QJOB_PRI,
			 muxLinkUpNotify, &pDrvCtrl->rtgEndObj,
			 NULL, NULL, NULL, NULL);

    /* If status went from up to down, announce link down. */

    else if (!(pDrvCtrl->rtgCurStatus & IFM_ACTIVE) && oldStatus & IFM_ACTIVE)
	jobQueueStdPost (pDrvCtrl->rtgJobQueue, NET_TASK_QJOB_PRI,
			 muxLinkDownNotify, &pDrvCtrl->rtgEndObj,
			 NULL, NULL, NULL, NULL);

    semGive (pDrvCtrl->rtgDevSem);

    return (OK);
    }

/*****************************************************************************
*
* rtgMuxConnect - muxConnect method handler
*
* This function handles muxConnect() events, which may be triggered
* manually or (more likely) by the bootstrap code. Most VxBus
* initialization occurs before the MUX has been fully initialized,
* so the usual muxDevLoad()/muxDevStart() sequence must be defered
* until the networking subsystem is ready. This routine will ultimately
* trigger a call to rtgEndLoad() to create the END interface instance.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgMuxConnect
    (
    VXB_DEVICE_ID pDev,
    void * unused
    )
    {
    RTG_DRV_CTRL *pDrvCtrl;
 
    /*
     * Attach our ISR. For PCI, the index value is always
     * 0, since the PCI bus controller dynamically sets
     * up interrupts for us.
     */

    vxbIntConnect (pDev, 0, rtgEndInt, pDev->pDrvCtrl);

    pDrvCtrl = pDev->pDrvCtrl;

    /* Save the cookie. */

    pDrvCtrl->rtgMuxDevCookie = muxDevLoad (pDev->unitNumber,
        rtgEndLoad, "", TRUE, pDev);

    if (pDrvCtrl->rtgMuxDevCookie != NULL)
        muxDevStart (pDrvCtrl->rtgMuxDevCookie);

    if (_func_m2PollStatsIfPoll != NULL)
        endPollStatsInit (pDrvCtrl->rtgMuxDevCookie, _func_m2PollStatsIfPoll);

    return;
    }

/*****************************************************************************
*
* rtgEeAddrSet - select a word in the EEPROM
*
* This is a helper function used by rtgEeWordGet(), which clocks a
* sequence of bits through to the serial EEPROM attached to the RealTek
* chip which contains a read command and specifies what word we
* want to access. The format of the read command depends on the
* EEPROM: most devices have a 64 word EEPROM, but some have a 256
* word EEPROM which requires a different read command sequence.
* The 256 word variety is most often found on cardbus controllers,
* which need extra space to hold all their CIS data.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEeAddrSet
    (
    VXB_DEVICE_ID pDev,
    UINT32 addr
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    UINT32 d;
    int i;

    pDrvCtrl = pDev->pDrvCtrl;

    d = addr | (RTG_9346_READ << pDrvCtrl->rtgEeWidth);

    for (i = 1 << (pDrvCtrl->rtgEeWidth + 3); i; i >>= 1)
        {
        if (d & i)
            CSR_SETBIT_1(pDev, RTG_EECMD, RTG_EECMD_DATAIN);
        else
            CSR_CLRBIT_1(pDev, RTG_EECMD, RTG_EECMD_DATAIN);
        rtgDelay (100);
        CSR_SETBIT_1(pDev, RTG_EECMD, RTG_EECMD_CLK);
        rtgDelay (100);
        CSR_CLRBIT_1(pDev, RTG_EECMD, RTG_EECMD_CLK);
        rtgDelay (100);
        }

    return;
    }

/*****************************************************************************
*
* rtgEeWordGet - read a word from the EEPROM
*
* This routine reads a single 16 bit word from a specified address
* within the EEPROM attached to the RealTek controller and returns
* it to the caller. After clocking in a read command and the address
* we want to access, we read back the 16 bit datum stored there.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEeWordGet
    (
    VXB_DEVICE_ID pDev,
    UINT32 addr,
    UINT16 *dest
    )
    {
    int i;
    UINT16 word = 0;

    rtgEeAddrSet (pDev, addr);

    for (i = 0x8000; i; i >>= 1)
        {
        CSR_SETBIT_1(pDev, RTG_EECMD, RTG_EECMD_CLK);
        rtgDelay (100);
        if (CSR_READ_1(pDev, RTG_EECMD) & RTG_EECMD_DATAOUT)
            word |= (UINT16)i;
        CSR_CLRBIT_1(pDev, RTG_EECMD, RTG_EECMD_CLK);
        rtgDelay (100);
        }

    *dest = word;

    return;
    }

/*****************************************************************************
*
* rtgEepromRead - read a sequence of words from the EEPROM
*
* This is the top-level EEPROM access function. It will read a
* sequence of words from the specified address into a supplied
* destination buffer. This is used mainly to read the station
* address.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEepromRead
    (
    VXB_DEVICE_ID pDev,
    UINT8 *dest,
    int off,
    int cnt
    )
    {
    int i;
    UINT16 word, *ptr;

    CSR_SETBIT_1(pDev, RTG_EECMD, RTG_EEMODE_PROGRAM);

    rtgDelay(100);

    for (i = 0; i < cnt; i++)
        {
        CSR_SETBIT_1(pDev, RTG_EECMD, RTG_EECMD_SEL);
        rtgDelay(100);
        rtgEeWordGet (pDev, off + i, &word);
        CSR_CLRBIT_1(pDev, RTG_EECMD, RTG_EECMD_SEL);
        ptr = (UINT16 *)(dest + (i * 2));
        *ptr = le16toh(word);
        }

    CSR_WRITE_1(pDev, RTG_EECMD, RTG_EEMODE_OFF);

    return;
    }

/*****************************************************************************
*
* rtgReset - reset the RealTek controller
*
* This function issues a reset command to the controller and waits
* for it to complete. This routine is always used to place the
* controller into a known state prior to configuration.
*
* RETURNS: ERROR if the reset bit never clears, otherwise OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgReset
    (
    VXB_DEVICE_ID pDev
    )
    {
    int i;

    CSR_WRITE_1(pDev, RTG_CMD, RTG_CMD_RESET);

    rtgDelay (10000);

    for (i = 0; i < RTG_TIMEOUT; i++)
        {
	rtgDelay (1);
        if (!(CSR_READ_1(pDev, RTG_CMD) & RTG_CMD_RESET))
            break;
        }

    if (i == RTG_TIMEOUT)
        {
        RTG_LOGMSG("%s%d: reset timed out\n", RTG_NAME,
            pDev->unitNumber, 0, 0, 0, 0);
        return (ERROR);
        }

    CSR_WRITE_2(pDev, RTG_RXCFG, 0);
    CSR_WRITE_2(pDev, RTG_TXCFG, 0);
    CSR_WRITE_2(pDev, RTG_IMR, 0);
    CSR_WRITE_2(pDev, RTG_ISR, 0xFFFF);

    return (OK);
    }

/*****************************************************************************
*
* rtgEndLoad - END driver entry point
*
* This routine initializes the END interface instance associated
* with this device. In traditional END drivers, this function is
* the only public interface, and it's typically invoked by a BSP
* driver configuration stub. With VxBus, the BSP stub code is no
* longer needed, and this function is now invoked automatically
* whenever this driver's muxConnect() method is called.
*
* For older END drivers, the load string would contain various
* configuration parameters, but with VxBus this use is deprecated.
* The load string should just be an empty string. The second
* argument should be a pointer to the VxBus device instance
* associated with this device. Like older END drivers, this routine
* will still return the device name if the init string is empty,
* since this behavior is still expected by the MUX. The MUX will
* invoke this function twice: once to obtain the device name,
* and then again to create the actual END_OBJ instance.
*
* When this function is called the second time, it will initialize
* the END object, perform MIB2 setup, allocate a buffer pool, and
* initialize the supported END capabilities. The only special
* capability we support is VLAN_MTU, since we can receive slightly
* larger than normal frames.
*
* RETURNS: An END object pointer, or NULL on error, or 0 and the name
* of the device if the <loadStr> was empty.
*
* ERRNO: N/A
*/

LOCAL END_OBJ * rtgEndLoad
    (
    char * loadStr,
    void * pArg
    )
    {
    RTG_DRV_CTRL *pDrvCtrl;
    VXB_DEVICE_ID pDev;
    int r;

    /* Make the MUX happy. */

    if (loadStr == NULL)
        return NULL;

    if (loadStr[0] == 0)
        {
        bcopy (RTG_NAME, loadStr, 4);
        return NULL;
        }

    pDev = pArg;
    pDrvCtrl = pDev->pDrvCtrl;

    if (END_OBJ_INIT (&pDrvCtrl->rtgEndObj, NULL, pDev->pName,
        pDev->unitNumber, &rtgNetFuncs, "RealTek 10/100/1000 Driver") == ERROR)
        {
        RTG_LOGMSG("%s%d: END_OBJ_INIT failed\n", RTG_NAME,
            pDev->unitNumber, 0, 0, 0, 0);
        return (NULL);
        }

    endM2Init (&pDrvCtrl->rtgEndObj, M2_ifType_ethernet_csmacd,
        pDrvCtrl->rtgAddr, ETHER_ADDR_LEN, ETHERMTU, 100000000,
        IFF_NOTRAILERS | IFF_SIMPLEX | IFF_MULTICAST | IFF_BROADCAST);

    /* Allocate a buffer pool */

    if (pDrvCtrl->rtgMaxMtu == RTG_JUMBO_MTU)
        r = endPoolJumboCreate (512, &pDrvCtrl->rtgEndObj.pNetPool);
    else
        r = endPoolCreate (512, &pDrvCtrl->rtgEndObj.pNetPool);

    if (r == ERROR)
        {
        RTG_LOGMSG("%s%d: pool creation failed\n", RTG_NAME,
            pDev->unitNumber, 0, 0, 0, 0);
        return (NULL);
        }

    pDrvCtrl->rtgPollBuf = endPoolTupleGet (pDrvCtrl->rtgEndObj.pNetPool);

    /* Set up polling stats. */

    pDrvCtrl->rtgEndStatsConf.ifPollInterval = sysClkRateGet();
    pDrvCtrl->rtgEndStatsConf.ifEndObj = &pDrvCtrl->rtgEndObj;
    pDrvCtrl->rtgEndStatsConf.ifWatchdog = NULL;
    pDrvCtrl->rtgEndStatsConf.ifValidCounters = (END_IFINUCASTPKTS_VALID |
        END_IFINMULTICASTPKTS_VALID | END_IFINBROADCASTPKTS_VALID |
        END_IFINOCTETS_VALID | END_IFINERRORS_VALID | END_IFINDISCARDS_VALID |
        END_IFOUTUCASTPKTS_VALID | END_IFOUTMULTICASTPKTS_VALID |
        END_IFOUTBROADCASTPKTS_VALID | END_IFOUTOCTETS_VALID |
        END_IFOUTERRORS_VALID);

    /*
     * Set up capabilities. All of the chips in the C+ family
     * support TCP/IP checksum offload on both RX and TX, and
     * hardware VLAN tag insertion and stripping.
     */

    pDrvCtrl->rtgCaps.csum_flags_tx = CSUM_VLAN|CSUM_IP|CSUM_UDP|CSUM_TCP;
    pDrvCtrl->rtgCaps.csum_flags_rx = CSUM_VLAN|CSUM_IP|CSUM_UDP|CSUM_TCP;
    pDrvCtrl->rtgCaps.cap_available |= IFCAP_VLAN_MTU|
        IFCAP_RXCSUM|IFCAP_TXCSUM|IFCAP_VLAN_HWTAGGING;
    pDrvCtrl->rtgCaps.cap_enabled |= IFCAP_VLAN_MTU|
        IFCAP_RXCSUM|IFCAP_TXCSUM|IFCAP_VLAN_HWTAGGING;

    if (pDrvCtrl->rtgMaxMtu == RTG_JUMBO_MTU)
        {
        pDrvCtrl->rtgCaps.cap_available |= IFCAP_JUMBO_MTU;
        pDrvCtrl->rtgCaps.cap_enabled |= IFCAP_JUMBO_MTU;
        }

    return (&pDrvCtrl->rtgEndObj);
    }

/*****************************************************************************
*
* rtgEndUnload - unload END driver instance
*
* This routine undoes the effects of rtgEndLoad(). The END object
* is destroyed, our network pool is released, the endM2 structures
* are released, and the polling stats watchdog is terminated.
*
* Note that the END interface instance can't be unloaded if the
* device is still running. The device must be stopped with muxDevStop()
* first.
*
* RETURNS: ERROR if device is still in the IFF_UP state, otherwise OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndUnload
    (
    END_OBJ * pEnd
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;

    /* We must be stopped before we can be unloaded. */

    if (pEnd->flags & IFF_UP)
	return (ERROR);

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;

    netMblkClChainFree (pDrvCtrl->rtgPollBuf);

    /* Relase our buffer pool */
    endPoolDestroy (pDrvCtrl->rtgEndObj.pNetPool);

    /* terminate stats polling */
    wdDelete (pDrvCtrl->rtgEndStatsConf.ifWatchdog);

    endM2Free (&pDrvCtrl->rtgEndObj);

    END_OBJECT_UNLOAD (&pDrvCtrl->rtgEndObj);

    return (EALREADY);  /* prevent freeing of pDrvCtrl */
    }

/*****************************************************************************
*
* rtgEndHashTblPopulate - populate the multicast hash filter
*
* This function programs the RealTek controller's multicast hash
* filter to receive frames sent to the multicast groups specified
* in the multicast address list attached to the END object. If
* the interface is in IFF_ALLMULTI mode, the filter will be
* programmed to receive all multicast packets by setting all the
* bits in the hash table to one.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEndHashTblPopulate
    (
    RTG_DRV_CTRL * pDrvCtrl
    )
    {
    UINT32 crcVal;
    UINT32 hashes[2] = { 0, 0 };
    ETHER_MULTI * mCastNode = NULL;

    CSR_SETBIT_4(pDrvCtrl->rtgDev, RTG_RXCFG, RTG_RXCFG_RX_MULTI);

    if (pDrvCtrl->rtgEndObj.flags & IFF_ALLMULTI)
        {
        CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR0, 0xFFFFFFFF);
        CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR1, 0xFFFFFFFF);
        return;
        }

    /* First, clear out the original filter. */

    CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR0, 0);
    CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR1, 0);

    /* Now repopulate it. */

    for (mCastNode =
        (ETHER_MULTI *) lstFirst (&pDrvCtrl->rtgEndObj.multiList);
         mCastNode != NULL;
         mCastNode = (ETHER_MULTI *) lstNext (&mCastNode->node))
        {
        /* get the CRC for the current address in the list */

        crcVal = endEtherCrc32BeGet ((const UINT8 *) mCastNode->addr,
            ETHER_ADDR_LEN) >> 26;

        /* get the value to be written to the proper hash register */

        if (crcVal < 32)
            hashes[0] |= (1 << crcVal);
        else
            hashes[1] |= (1 << (crcVal - 32));
        }

    /*
     * For some unfathomable reason, RealTek decided to reverse
     * the order of the multicast hash registers in the PCI Express
     * parts. This means we have to write the hash pattern in reverse
     * order for those devices.
     */

    if (pDrvCtrl->rtgHwRev == RTG_HWREV_8100E ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8101E ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8102E ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8102EL ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8103EL ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168_SPIN1 ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168_SPIN2 ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168_SPIN3 ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168C ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168C_SPIN2 ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168CP ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168D ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168DP ||
        pDrvCtrl->rtgHwRev == RTG_HWREV_8168E ||
        pDrvCtrl->rtgHwRev == 0x4C000000)
        {
        CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR0, bswap32(hashes[1]));
        CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR1, bswap32(hashes[0]));
        }
    else
        {
        CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR0, hashes[0]);
        CSR_WRITE_4(pDrvCtrl->rtgDev, RTG_MAR1, hashes[1]);
        }

    return;
    }

/*****************************************************************************
*
* rtgEndMCastAddrAdd - add a multicast address for the device
*
* This routine adds a multicast address to whatever the driver
* is already listening for.  It then resets the address filter.
*
* RETURNS: OK or ERROR.
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndMCastAddrAdd
    (
    END_OBJ * pEnd,
    char * pAddr
    )
    {
    int retVal;
    RTG_DRV_CTRL * pDrvCtrl;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);

    if (!(pDrvCtrl->rtgEndObj.flags & IFF_UP))
        {
        semGive (pDrvCtrl->rtgDevSem);
        return (OK);
        }

    retVal = etherMultiAdd (&pEnd->multiList, pAddr);

    if (retVal == ENETRESET)
        {
        pEnd->nMulti++;
        rtgEndHashTblPopulate ((RTG_DRV_CTRL *)pEnd);
        }

    semGive (pDrvCtrl->rtgDevSem);
    return (OK);
    }

/*****************************************************************************
*
* rtgEndMCastAddrDel - delete a multicast address for the device
*
* This routine removes a multicast address from whatever the driver
* is listening for.  It then resets the address filter.
*
* RETURNS: OK or ERROR.
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndMCastAddrDel
    (
    END_OBJ * pEnd,
    char * pAddr
    )
    {
    int retVal;
    RTG_DRV_CTRL * pDrvCtrl;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);

    if (!(pDrvCtrl->rtgEndObj.flags & IFF_UP))
        {
        semGive (pDrvCtrl->rtgDevSem);
        return (OK);
        }

    retVal = etherMultiDel (&pEnd->multiList, pAddr);

    if (retVal == ENETRESET)
        {
        pEnd->nMulti--;
        rtgEndHashTblPopulate ((RTG_DRV_CTRL *)pEnd);
        }

    semGive (pDrvCtrl->rtgDevSem);

    return (OK);
    }

/*****************************************************************************
*
* rtgEndMCastAddrGet - get the multicast address list for the device
*
* This routine gets the multicast list of whatever the driver
* is already listening for.
*
* RETURNS: OK or ERROR.
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndMCastAddrGet
    (
    END_OBJ * pEnd,
    MULTI_TABLE * pTable
    )
    {
    STATUS rval;
    RTG_DRV_CTRL * pDrvCtrl;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);

    if (!(pDrvCtrl->rtgEndObj.flags & IFF_UP))
        {
        semGive (pDrvCtrl->rtgDevSem);
        return (OK);
        }

    rval = etherMultiGet (&pEnd->multiList, pTable);

    semGive (pDrvCtrl->rtgDevSem);

    return (rval);
    }

/*****************************************************************************
*
* rtgEndStatsDump - return polled statistics counts
*
* This routine is automatically invoked periodically by the polled
* statistics watchdog. Normally, we would use this for reading stats
* counter registers from the device, however the RealTek controller
* doesn't have a full set of stats registers. Most statistics are
* counted in software, except for the missed packet count, which can
* be read from hardware. Maintaining the statistics like this is
* still quicker than invoking the endM2Packet() routine for every
* inbound and outbound frame.
*
* RETURNS: always OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndStatsDump
    (
    RTG_DRV_CTRL * pDrvCtrl
    )
    {
    END_IFCOUNTERS *    pEndStatsCounters;

    pEndStatsCounters = &pDrvCtrl->rtgEndStatsCounters;

    pEndStatsCounters->ifInOctets = pDrvCtrl->rtgInOctets;
    pDrvCtrl->rtgInOctets = 0;

    pEndStatsCounters->ifInUcastPkts = pDrvCtrl->rtgInUcasts;
    pDrvCtrl->rtgInUcasts = 0;

    pEndStatsCounters->ifInMulticastPkts = pDrvCtrl->rtgInMcasts;
    pDrvCtrl->rtgInMcasts = 0;

    pEndStatsCounters->ifInBroadcastPkts = pDrvCtrl->rtgInBcasts;
    pDrvCtrl->rtgInBcasts = 0;

    pEndStatsCounters->ifInErrors = pDrvCtrl->rtgInErrors;
    pDrvCtrl->rtgInErrors = 0;

    pEndStatsCounters->ifInDiscards = pDrvCtrl->rtgInDiscards;
    pDrvCtrl->rtgInDiscards = 0;

    pEndStatsCounters->ifOutOctets = pDrvCtrl->rtgOutOctets;
    pDrvCtrl->rtgOutOctets = 0;

    pEndStatsCounters->ifOutUcastPkts = pDrvCtrl->rtgOutUcasts;
    pDrvCtrl->rtgOutUcasts = 0;

    pEndStatsCounters->ifOutMulticastPkts = pDrvCtrl->rtgOutMcasts;
    pDrvCtrl->rtgOutMcasts = 0;

    pEndStatsCounters->ifOutBroadcastPkts = pDrvCtrl->rtgOutBcasts;
    pDrvCtrl->rtgOutBcasts = 0;

    pEndStatsCounters->ifOutErrors = pDrvCtrl->rtgOutErrors;
    pDrvCtrl->rtgOutErrors = 0;

    return (OK);
    }

/*****************************************************************************
*
* rtgEndIoctl - the driver I/O control routine
*
* This function processes ioctl requests supplied via the muxIoctl()
* routine. In addition to the normal boilerplate END ioctls, this
* driver supports the IFMEDIA ioctls, END capabilities ioctls, and
* polled stats ioctls.
*
* RETURNS: A command specific response, usually OK or ERROR.
*
* ERRNO: N/A
*/

LOCAL int rtgEndIoctl
    (
    END_OBJ * pEnd,
    int cmd,
    caddr_t data
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    END_MEDIALIST * mediaList;
    END_CAPABILITIES * hwCaps;
    END_MEDIA * pMedia;
    END_RCVJOBQ_INFO * qinfo;
    UINT32 nQs;
    VXB_DEVICE_ID pDev;
    INT32 value;
    int error = OK;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;
    pDev = pDrvCtrl->rtgDev;

    if (cmd != EIOCPOLLSTART && cmd != EIOCPOLLSTOP)
        semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);

    switch (cmd)
        {
        case EIOCSADDR:
            if (data == NULL)
                error = EINVAL;
            else
                {
                bcopy ((char *)data, (char *)pDrvCtrl->rtgAddr,
                    ETHER_ADDR_LEN);
                bcopy ((char *)data,
                    (char *)pEnd->mib2Tbl.ifPhysAddress.phyAddress,
                    ETHER_ADDR_LEN);
                if (pEnd->pMib2Tbl != NULL)
                    bcopy ((char *)data,
                        (char *)pEnd->pMib2Tbl->m2Data.mibIfTbl.ifPhysAddress.phyAddress,
                        ETHER_ADDR_LEN);
                }
            rtgEndRxConfig (pDrvCtrl);
            break;

        case EIOCGADDR:
            if (data == NULL)
                error = EINVAL;
            else
                bcopy ((char *)pDrvCtrl->rtgAddr, (char *)data,
                    ETHER_ADDR_LEN);
            break;
        case EIOCSFLAGS:

            value = (INT32)((ULONG)data & 0xffffffff);
            if (value < 0)
                {
                value = -value;
                value--;
                END_FLAGS_CLR (pEnd, value);
                }
            else
                END_FLAGS_SET (pEnd, value);

            rtgEndRxConfig (pDrvCtrl);

            break;

        case EIOCGFLAGS:
            if (data == NULL)
                error = EINVAL;
            else
                *(INT32 *)data = END_FLAGS_GET(pEnd);

            break;

        case EIOCMULTIADD:
            error = rtgEndMCastAddrAdd (pEnd, (char *) data);
            break;

        case EIOCMULTIDEL:
            error = rtgEndMCastAddrDel (pEnd, (char *) data);
            break;

        case EIOCMULTIGET:
            error = rtgEndMCastAddrGet (pEnd, (MULTI_TABLE *) data);
            break;

        case EIOCPOLLSTART:
            pDrvCtrl->rtgIntMask = CSR_READ_2(pDev, RTG_IMR);
            CSR_WRITE_2(pDev, RTG_IMR, 0);
            CSR_WRITE_2(pDev, RTG_ISR, RTG_INTRS);
            pDrvCtrl->rtgPolling = TRUE;

            /*
             * We may have been asked to enter polled mode while
             * there are transmissions pending. This is a problem,
             * because the polled transmit routine expects that
             * the TX ring will be empty when it's called. In
             * order to guarantee this, we have to drain the TX
             * ring here. We could also just plain reset and
             * reinitialize the transmitter, but this is faster.
             */

            while (pDrvCtrl->rtgTxFree < pDrvCtrl->rtgTxDescCnt)
                {
                volatile RTG_DESC * pDesc;
                VXB_DMA_MAP_ID pMap;
                M_BLK_ID pMblk;

                pDesc = &pDrvCtrl->rtgTxDescMem[pDrvCtrl->rtgTxCons];

                /* Wait for ownership bit to clear */

                CSR_WRITE_1(pDrvCtrl->rtgDev,
                    pDrvCtrl->rtgTxStartReg, RTG_TXPP_NPQ);
                while (pDesc->rtg_cmdsts & htole32(RTG_TDESC_STAT_OWN))
                    ;

                pMblk = pDrvCtrl->rtgTxMblk[pDrvCtrl->rtgTxCons];

                if (pMblk != NULL)
                    {
                    pMap = pDrvCtrl->rtgTxMblkMap[pDrvCtrl->rtgTxCons];
                    vxbDmaBufMapUnload (pDrvCtrl->rtgMblkTag, pMap);
                    endPoolTupleFree (pMblk);
                    pDrvCtrl->rtgTxMblk[pDrvCtrl->rtgTxCons] = NULL;
                    }

                pDesc->rtg_cmdsts &= htole32(RTG_TDESC_CMD_EOR);
                pDesc->rtg_vlanctl = 0;

                pDrvCtrl->rtgTxFree++;
                RTG_INC_DESC (pDrvCtrl->rtgTxCons, pDrvCtrl->rtgTxDescCnt);
                }

            break;

        case EIOCPOLLSTOP:
            CSR_WRITE_2(pDev, RTG_ISR, RTG_INTRS);
            CSR_WRITE_2(pDev, RTG_IMR, pDrvCtrl->rtgIntMask);
            pDrvCtrl->rtgPolling = FALSE;
            break;

        case EIOCGMIB2233:
        case EIOCGMIB2:
            error = endM2Ioctl (&pDrvCtrl->rtgEndObj, cmd, data);
            break;

        case EIOCGPOLLCONF:
            if (data == NULL)
                error = EINVAL;
            else
                *((END_IFDRVCONF **)data) = &pDrvCtrl->rtgEndStatsConf;
            break;

        case EIOCGPOLLSTATS:
            if (data == NULL)
                error = EINVAL;
            else
                {
                error = rtgEndStatsDump(pDrvCtrl);
                if (error == OK)
                    *((END_IFCOUNTERS **)data) = &pDrvCtrl->rtgEndStatsCounters;
                }
            break;

        case EIOCGMEDIALIST:
            if (data == NULL)
                {
                error = EINVAL;
                break;
                }

            mediaList = (END_MEDIALIST *)data;
            if (mediaList->endMediaListLen <
                pDrvCtrl->rtgMediaList->endMediaListLen)
                {
                mediaList->endMediaListLen =
                    pDrvCtrl->rtgMediaList->endMediaListLen;
                error = ENOSPC;
                break;
                }

            bcopy((char *)pDrvCtrl->rtgMediaList, (char *)mediaList,
                  sizeof(END_MEDIALIST) + (sizeof(UINT32) *
                  pDrvCtrl->rtgMediaList->endMediaListLen));
            break;

        case EIOCGIFMEDIA:
            if (data == NULL)
                error = EINVAL;
            else
                {
                pMedia = (END_MEDIA *)data;
                pMedia->endMediaActive = pDrvCtrl->rtgCurMedia;
                pMedia->endMediaStatus = pDrvCtrl->rtgCurStatus;
                }
            break;

        case EIOCSIFMEDIA:
            if (data == NULL)
                error = EINVAL;
            else
                {
                pMedia = (END_MEDIA *)data;
                miiBusModeSet (pDrvCtrl->rtgMiiBus, pMedia->endMediaActive);
                rtgLinkUpdate (pDrvCtrl->rtgDev);
                error = OK;
                }
            break;

        case EIOCGIFCAP:
            hwCaps = (END_CAPABILITIES *)data;
            if (hwCaps == NULL)
                {
                error = EINVAL;
                break;
                }
            hwCaps->csum_flags_tx = pDrvCtrl->rtgCaps.csum_flags_tx;
            hwCaps->csum_flags_rx = pDrvCtrl->rtgCaps.csum_flags_rx;
            hwCaps->cap_available = pDrvCtrl->rtgCaps.cap_available;
            hwCaps->cap_enabled = pDrvCtrl->rtgCaps.cap_enabled;
            break;

        case EIOCSIFCAP:
            hwCaps = (END_CAPABILITIES *)data;
            if (hwCaps == NULL)
                {
                error = EINVAL;
                break;
                }
            pDrvCtrl->rtgCaps.cap_enabled = hwCaps->cap_enabled;
            break;

        case EIOCGIFMTU:
            if (data == NULL)
                error = EINVAL;
            else
                *(INT32 *)data = (INT32)(pEnd->mib2Tbl.ifMtu);
            break;

        case EIOCSIFMTU:
            value = (INT32)((ULONG)data & 0xffffffff);
            if (value <= 0 || value > pDrvCtrl->rtgMaxMtu)
                error = EINVAL;
            else
                {
                pEnd->mib2Tbl.ifMtu = value;
                if (pEnd->pMib2Tbl != NULL)
                    pEnd->pMib2Tbl->m2Data.mibIfTbl.ifMtu = value;
                }
            break;

	case EIOCGRCVJOBQ:
            if (data == NULL)
		{
                error = EINVAL;
		break;
		}

	    qinfo = (END_RCVJOBQ_INFO *)data;
	    nQs = qinfo->numRcvJobQs;
	    qinfo->numRcvJobQs = 1;
	    if (nQs < 1)
		error = ENOSPC;
	    else
		qinfo->qIds[0] = pDrvCtrl->rtgJobQueue;
	    break;

        default:
            error = EINVAL;
            break;
        }

    if (cmd != EIOCPOLLSTART && cmd != EIOCPOLLSTOP)
        semGive (pDrvCtrl->rtgDevSem);

    return (error);
    }

/*****************************************************************************
*
* rtgEndRxConfig - configure the RealTek RX filter
*
* This is a helper routine used by rtgEndIoctl() and rtgEndStart() to
* configure the controller's RX filter. The unicast address filter is
* programmed with the currently configured MAC address, and the RX
* configuration is set to allow unicast and broadcast frames to be
* received. If the interface is in IFF_PROMISC mode, the RX_PROMISC
* bit is set, which allows all packets to be received.
*
* The rtgEndHashTblPopulate() routine is also called to update the
* multicast filter.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEndRxConfig
    (
    RTG_DRV_CTRL * pDrvCtrl
    )
    {
    VXB_DEVICE_ID pDev;
    UINT32 addr[2];

    pDev = pDrvCtrl->rtgDev;

    /* Copy the address to a buffer we know is 32-bit aligned. */

    bcopy ((char *)pDrvCtrl->rtgAddr, (char *)addr, ETHER_ADDR_LEN);

    /*
     * Init our MAC address.  Even though the chipset
     * documentation doesn't mention it, we need to enter "Config
     * register write enable" mode to modify the ID registers.
     */

    CSR_WRITE_1(pDev, RTG_EECMD, RTG_EEMODE_WRITECFG);
    CSR_WRITE_4(pDev, RTG_IDR0, htole32(addr[0]));
    CSR_WRITE_4(pDev, RTG_IDR1, htole32(addr[1]));
    CSR_WRITE_1(pDev, RTG_EECMD, RTG_EEMODE_OFF);

    /* Program the RX filter to receive unicasts and broadcasts */

    CSR_SETBIT_4(pDev, RTG_RXCFG, RTG_RXCFG_RX_INDIV|RTG_RXCFG_RX_BROAD);

    /* Enable promisc mode, if specified. */

    if (pDrvCtrl->rtgEndObj.flags & IFF_PROMISC)
        CSR_SETBIT_4(pDev, RTG_RXCFG, RTG_RXCFG_RX_PROMISC);
    else
        CSR_CLRBIT_4(pDev, RTG_RXCFG, RTG_RXCFG_RX_PROMISC);

    /* Program the multicast filter. */

    rtgEndHashTblPopulate (pDrvCtrl);

    return;
    }

/*****************************************************************************
*
* rtgEndStart - start the device
*
* This function resets the device to put it into a known state and
* then configures it for RX and TX operation. The RX and TX configuration
* registers are initialized, and the address of the RX DMA window is
* loaded into the device. Interrupts are then enabled, and the initial
* link state is configured.
*
* Note that this routine also checks to see if an alternate jobQueue
* has been specified via the vxbParam subsystem. This allows the driver
* to divert its work to an alternate processing task, such as may be
* done with TIPC. This means that the jobQueue can be changed while
* the system is running, but the device must be stopped and restarted
* for the change to take effect.
*
* RETURNS: ERROR if device initialization failed, otherwise OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndStart
    (
    END_OBJ * pEnd
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    VXB_DEVICE_ID pDev;
    VXB_INST_PARAM_VALUE val;
    HEND_RX_QUEUE_PARAM * pRxQueue;
    M_BLK_ID pMblk;
    RTG_DESC * pDesc;
    VXB_DMA_MAP_ID pMap;
    int i;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;
    pDev = pDrvCtrl->rtgDev;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);
    END_TX_SEM_TAKE (pEnd, WAIT_FOREVER);

    if (pEnd->flags & IFF_UP)
        {
        END_TX_SEM_GIVE (pEnd);
        semGive (pDrvCtrl->rtgDevSem);
        return (OK);
        }

    /* First, reset the controller */

    rtgReset (pDev);

    /* Initialize job queues */

    pDrvCtrl->rtgJobQueue = netJobQueueId;

    /* Override the job queue ID if the user supplied an alternate one. */

    /*
     * paramDesc {
     * The rxQueue00 parameter specifies a pointer to a
     * HEND_RX_QUEUE_PARAM structure, which contains,
     * among other things, an ID for the job queue
     * to be used for this instance. }
     */
    if (vxbInstParamByNameGet (pDev, "rxQueue00",
        VXB_PARAM_POINTER, &val) == OK)
        {
        pRxQueue = (HEND_RX_QUEUE_PARAM *) val.pValue;
        if (pRxQueue->jobQueId != NULL)
            pDrvCtrl->rtgJobQueue = pRxQueue->jobQueId;
        }

    QJOB_SET_PRI(&pDrvCtrl->rtgTxJob, NET_TASK_QJOB_PRI);
    pDrvCtrl->rtgTxJob.func = rtgEndTxHandle;
    QJOB_SET_PRI(&pDrvCtrl->rtgRxJob, NET_TASK_QJOB_PRI);
    pDrvCtrl->rtgRxJob.func = rtgEndRxHandle;
    QJOB_SET_PRI(&pDrvCtrl->rtgIntJob, NET_TASK_QJOB_PRI);
    pDrvCtrl->rtgIntJob.func = rtgEndIntHandle;

    vxAtomic32Set (&pDrvCtrl->rtgRxPending, FALSE);
    vxAtomic32Set (&pDrvCtrl->rtgTxPending, FALSE);
    vxAtomic32Set (&pDrvCtrl->rtgIntPending, FALSE);

    bzero ((char *)pDrvCtrl->rtgRxDescMem,
        sizeof(RTG_DESC) * pDrvCtrl->rtgRxDescCnt);
    bzero ((char *)pDrvCtrl->rtgTxDescMem,
        sizeof(RTG_DESC) * pDrvCtrl->rtgTxDescCnt);
    bzero ((char *)pDrvCtrl->rtgTxMblk,
        sizeof(M_BLK_ID) * pDrvCtrl->rtgTxDescCnt);

    /* Set up the RX ring. */

    for (i = 0; i < pDrvCtrl->rtgRxDescCnt; i++)
        {
        pMblk = endPoolTupleGet (pDrvCtrl->rtgEndObj.pNetPool);
        if (pMblk == NULL)
            {
            END_TX_SEM_GIVE (pEnd);
            semGive (pDrvCtrl->rtgDevSem);
            return (ERROR);
            }

        /*
         * Note: buffer length field in an RX descriptor is only 12
         * bits wide, so the maximum size of a single RX buffer is
         * limited to 8192 bytes.
         */

        if (pDrvCtrl->rtgMaxMtu == RTG_JUMBO_MTU)
            pMblk->m_len = pMblk->m_pkthdr.len = END_JUMBO_CLSIZE - 8;

        pMblk->m_next = NULL;
        RTG_ADJ (pMblk);
        pDrvCtrl->rtgRxMblk[i] = pMblk;

        pMap = pDrvCtrl->rtgRxMblkMap[i];

        /* don't need return from function call */

        (void) vxbDmaBufMapMblkLoad (pDev, pDrvCtrl->rtgMblkTag, pMap, pMblk, 0);

        pMap->fragList[0].fragLen -= 8;

        pDesc = &pDrvCtrl->rtgRxDescMem[i];
        pDesc->rtg_cmdsts = htole32(pMap->fragList[0].fragLen |
            RTG_RDESC_CMD_OWN);
        pDesc->rtg_bufaddr_lo = htole32(RTG_ADDR_LO(pMap->fragList[0].frag));
        pDesc->rtg_bufaddr_hi = htole32(RTG_ADDR_HI(pMap->fragList[0].frag));

        if (i == (pDrvCtrl->rtgRxDescCnt - 1))
            pDesc->rtg_cmdsts |= htole32(RTG_RDESC_CMD_EOR);

        }

    vxbDmaBufMapLoad (pDev, pDrvCtrl->rtgRxDescTag,
        pDrvCtrl->rtgRxDescMap, pDrvCtrl->rtgRxDescMem,
            sizeof(RTG_DESC) * pDrvCtrl->rtgRxDescCnt, 0);

    vxbDmaBufMapLoad (pDev, pDrvCtrl->rtgTxDescTag,
        pDrvCtrl->rtgTxDescMap, pDrvCtrl->rtgTxDescMem,
            sizeof(RTG_DESC) * pDrvCtrl->rtgTxDescCnt, 0);

    pDrvCtrl->rtgRxIdx = 0;
    pDrvCtrl->rtgTxCur = 0;
    pDrvCtrl->rtgTxLast = 0;
    pDrvCtrl->rtgTxStall = FALSE;
    pDrvCtrl->rtgTxProd = 0;
    pDrvCtrl->rtgTxCons = 0;
    pDrvCtrl->rtgTxFree = pDrvCtrl->rtgTxDescCnt;

    /* Enable C+ mode. */

    CSR_WRITE_2(pDev, RTG_CPLUSCMD, RTG_CPCMD_TX_ENB|RTG_CPCMD_RX_ENB|
        RTG_CPCMD_RXCSUM_ENB|RTG_CPCMD_RXVLAN_ENB);

    /* Set C+ mode TX threshold */
    if (pDrvCtrl->rtgDevType == RTG_DEVTYPE_8139CPLUS)
        CSR_WRITE_1(pDev, RTG_ETXTHRESH, 16);
    else
        CSR_WRITE_1(pDev, RTG_MAXTXFRAMELEN, 59);

    /* Set max RX frame size (for gigE devices only) */
    if (pDrvCtrl->rtgDevType == RTG_DEVTYPE_8169)
        {
        if (pDrvCtrl->rtgMaxMtu == RTG_JUMBO_MTU)
        CSR_WRITE_2(pDev, RTG_MAXRXFRAMELEN, 7440);
        else
            CSR_WRITE_2(pDev, RTG_MAXRXFRAMELEN, 1522);
        }

    /* Load the RX and TX DMA ring base addresses */

    CSR_WRITE_4(pDev, RTG_RXRINGBASE_HI,
        RTG_ADDR_HI(pDrvCtrl->rtgRxDescMap->fragList[0].frag));

    CSR_WRITE_4(pDev, RTG_RXRINGBASE_LO,
        RTG_ADDR_LO(pDrvCtrl->rtgRxDescMap->fragList[0].frag));

    CSR_WRITE_4(pDev, RTG_TXRINGBASE0_HI,
        RTG_ADDR_HI(pDrvCtrl->rtgTxDescMap->fragList[0].frag));

    CSR_WRITE_4(pDev, RTG_TXRINGBASE0_LO,
        RTG_ADDR_LO(pDrvCtrl->rtgTxDescMap->fragList[0].frag));

    /* Enable receiver and transmitter. */
    CSR_WRITE_1(pDev, RTG_CMD, RTG_CMD_TX_ENABLE|RTG_CMD_RX_ENABLE);

    /* Set initial RX configuration */
    CSR_WRITE_4(pDev, RTG_RXCFG, RTG_RXCFG_RX_RUNT|RTG_RX_MAXDMA|
        RTG_RX_FIFOTHRESH);

    /* Set initial TX configuration */
    CSR_WRITE_4(pDev, RTG_TXCFG, RTG_TXCFG_IFG|RTG_TX_MAXDMA);

    /* Program the RX filter. */
    rtgEndRxConfig (pDrvCtrl);

    /* Enable interrupts */

    CSR_WRITE_2(pDev, RTG_ISR, 0xFFFF);
    pDrvCtrl->rtgIntrs = RTG_INTRS;
    CSR_WRITE_2(pDev, RTG_IMR, pDrvCtrl->rtgIntrs);
    vxbIntEnable (pDev, 0, rtgEndInt, pDrvCtrl);

    /* Set initial link state */

    pDrvCtrl->rtgCurMedia = IFM_ETHER|IFM_NONE;
    pDrvCtrl->rtgCurStatus = IFM_AVALID;

    miiBusModeSet (pDrvCtrl->rtgMiiBus,
        pDrvCtrl->rtgMediaList->endMediaListDefault);

    END_FLAGS_SET (pEnd, (IFF_UP | IFF_RUNNING));

    END_TX_SEM_GIVE (pEnd);
    semGive (pDrvCtrl->rtgDevSem);

    return (OK);
    }

/*****************************************************************************
*
* rtgEndStop - stop the device
*
* This function undoes the effects of rtgEndStart(). The device is shut
* down and all resources are released. Note that the shutdown process
* pauses to wait for all pending RX, TX and link event jobs that may have
* been initiated by the interrupt handler to complete. This is done
* to prevent tNetTask from accessing any data that might be released by
* this routine.
*
* RETURNS: ERROR if device shutdown failed, otherwise OK
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndStop
    (
    END_OBJ * pEnd
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    VXB_DEVICE_ID pDev;
    int i;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;

    semTake (pDrvCtrl->rtgDevSem, WAIT_FOREVER);
    if (!(pEnd->flags & IFF_UP))
        {
        semGive (pDrvCtrl->rtgDevSem);
        return (OK);
        }

    pDev = pDrvCtrl->rtgDev;

    /* Disable interrupts */
    /*vxbIntDisable (pDev, 0, rtgEndInt, pDrvCtrl);*/
    pDrvCtrl->rtgIntrs = RTG_INTRS;
    CSR_WRITE_2(pDev, RTG_IMR, pDrvCtrl->rtgIntrs);
    CSR_WRITE_2(pDev, RTG_ISR, 0xFFFF);

    /*
     * Wait for all jobs to drain.
     * Note: this must be done before we disable the receiver
     * and transmitter below. If someone tries to reboot us via
     * WDB, this routine may be invoked while the RX handler is
     * still running in tNetTask. If we disable the chip while
     * that function is running, it'll start reading inconsistent
     * status from the chip. We have to wait for that job to
     * terminate first, then we can disable the receiver and
     * transmitter.
     */

    for (i = 0; i < RTG_TIMEOUT; i++)
        {
        if (vxAtomic32Get (&pDrvCtrl->rtgRxPending) == FALSE &&
            vxAtomic32Get (&pDrvCtrl->rtgTxPending) == FALSE &&
            vxAtomic32Get (&pDrvCtrl->rtgIntPending) == FALSE)
            break;
        taskDelay(1);
        }

    if (i == RTG_TIMEOUT)
        RTG_LOGMSG("%s%d: timed out waiting for job to complete\n",
            RTG_NAME, pDev->unitNumber, 0, 0, 0, 0);

    /* Disable RX and TX. */
    rtgReset (pDev);
    CSR_WRITE_1(pDev, RTG_CMD, 0);

    END_TX_SEM_TAKE (pEnd, WAIT_FOREVER);

    END_FLAGS_CLR (pEnd, (IFF_UP | IFF_RUNNING));

    /* Release resources */

    vxbDmaBufMapUnload (pDrvCtrl->rtgRxDescTag, pDrvCtrl->rtgRxDescMap);
    vxbDmaBufMapUnload (pDrvCtrl->rtgTxDescTag, pDrvCtrl->rtgTxDescMap);

    for (i = 0; i < pDrvCtrl->rtgRxDescCnt; i++)
        {
        if (pDrvCtrl->rtgRxMblk[i] != NULL)
            {
            netMblkClChainFree (pDrvCtrl->rtgRxMblk[i]);
            pDrvCtrl->rtgRxMblk[i] = NULL;
            vxbDmaBufMapUnload (pDrvCtrl->rtgMblkTag,
                pDrvCtrl->rtgRxMblkMap[i]);
            }
        }

    endMcacheFlush ();

    for (i = 0; i < pDrvCtrl->rtgTxDescCnt; i++)
        {
        if (pDrvCtrl->rtgTxMblk[i] != NULL)
            {
            netMblkClChainFree (pDrvCtrl->rtgTxMblk[i]);
            pDrvCtrl->rtgTxMblk[i] = NULL;
            vxbDmaBufMapUnload (pDrvCtrl->rtgMblkTag,
                pDrvCtrl->rtgTxMblkMap[i]);
            }
        }

    END_TX_SEM_GIVE (pEnd); 
    semGive (pDrvCtrl->rtgDevSem);

    return (OK);
    }

/*****************************************************************************
*
* rtgEndInt - handle device interrupts
*
* This function is invoked whenever the RealTek's interrupt line is asserted.
* Note that because the RealTek is a PCI device, it's possible that it might
* end up sharing an interrupt with another device. The host has no way of
* knowing which device on a shared interrupt line actually asserted the
* interrupt, so it invokes all the interrupt service routines that are
* bound to it. We have to check here if any events are actually pending
* in the interrupt status register, and that they haven't been masked off
* in the interrupt mask register, before proceeding.
*
* Once we know our device really does have an event pending, we mask
* off all interrupts and schedule the task-level interrupt handler to run.
* Interrupts will only be unmasked once the pending events have been
* serviced.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEndInt
    (
    RTG_DRV_CTRL * pDrvCtrl
    )
    {
    VXB_DEVICE_ID pDev;
    UINT16 status;

    pDev = pDrvCtrl->rtgDev;

    status = CSR_READ_2(pDev, RTG_ISR);

    /*
     * Make sure there's really an interrupt event pending for us.
     * Since we're a PCI device, we may be sharing an interrupt line
     * with another device. If we are, our ISR might be invoked
     * as a result of the other device asserting the interrupt, in
     * which case we really don't have any work to do.
     */

    if (!(status & RTG_INTRS))
        return;

    if (CSR_READ_2(pDev, RTG_IMR) == 0)
        return;

    if (vxAtomic32Cas(&pDrvCtrl->rtgIntPending, FALSE, TRUE))
        {
        CSR_WRITE_2(pDev, RTG_IMR, 0);
        jobQueuePost (pDrvCtrl->rtgJobQueue, &pDrvCtrl->rtgIntJob);
        }

    return;
    }

#ifdef RTG_RX_FIXUP
/******************************************************************************
*
* rtgRxFixup - fix up alignment of received frames
*
* This routine is used on architectures where unaligned accesses are
* not transparent. The TCP/IP stack sometimes uses 32-bit loads and
* stores to access fields within the packet headers: for this to work
* correctly on some architectures, the packet must be aligned on a
* 32-bit boundary. However the RealTek hardware requires that all
* ethernet frame buffers be 8-byte aligned. This forces the frame
* payload to a word alignment.
*
* To make up for this, we have to shift the RX data in order to
* longword-align the payload. This is not required on Pentium,
* PowerPC and Coldfire architectures, but is required everywhere
* else.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgRxFixup
    (
    M_BLK_ID pMblk
    )
    {
    int     i;
    UINT8  *src, *dst;

    src = mtod(pMblk, UINT8 *);
    dst = src - sizeof(UINT16);

    for (i = 0; i < pMblk->m_len; i++)
        dst[i] = src[i];

    pMblk->m_data -= sizeof(UINT16);

    return;
    }
#endif

/******************************************************************************
*
* rtgEndRxHandle - process received frames
*
* This function is scheduled by the ISR to run in the context of tNetTask
* whenever an RX interrupt is received. It processes packets from the
* RX DMA ring and encapsulates them into mBlk tuples which are handed up
* to the MUX.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEndRxHandle
    (
    void * pArg
    )
    {
    QJOB *pJob;
    RTG_DRV_CTRL *pDrvCtrl;
    VXB_DEVICE_ID pDev;
    M_BLK_ID pMblk;
    M_BLK_ID pNewMblk;
    UINT32 rxSts;
    UINT32 rxVlan;
    UINT16 rxLen;
    volatile RTG_DESC * pDesc;
    VXB_DMA_MAP_ID pMap;
    int loopCounter = RTG_MAX_RX;

    pJob = pArg;
    pDrvCtrl = member_to_object (pJob, RTG_DRV_CTRL, rtgRxJob);
    pDev = pDrvCtrl->rtgDev;

    pDesc = &pDrvCtrl->rtgRxDescMem[pDrvCtrl->rtgRxIdx];

    while (loopCounter && !(pDesc->rtg_cmdsts & htole32(RTG_RDESC_CMD_OWN)))
        {

        rxSts = le32toh(pDesc->rtg_cmdsts);
        rxVlan = le32toh(pDesc->rtg_vlanctl);
        rxLen = (UINT16)(rxSts & pDrvCtrl->rtgRxLenMask);

        if ((rxSts & (RTG_RDESC_STAT_SOF|RTG_RDESC_STAT_EOF)) !=
            (RTG_RDESC_STAT_SOF|RTG_RDESC_STAT_EOF))
            goto skip;

        if ((rxSts & (RTG_RDESC_STAT_SOF|RTG_RDESC_STAT_EOF)) !=
            (RTG_RDESC_STAT_SOF|RTG_RDESC_STAT_EOF))
            goto skip;

        /*
         * NOTE: for the 8139C+, the frame length field
         * is always 12 bits in size, but for the gigE chips,
         * it is 13 bits (since the max RX frame length is 16K).
         * Unfortunately, all 32 bits in the status word
         * were already used, so to make room for the extra
         * length bit, RealTek took out the 'frame alignment
         * error' bit and shifted the other status bits
         * over one slot. The OWN, EOR, FS and LS bits are
         * still in the same places. We have already extracted
         * the frame length and checked the OWN bit, so rather
         * than using an alternate bit mapping, we shift the
         * status bits one space to the right so we can evaluate
         * them using the 8169 status as though it was in the
         * same format as that of the 8139C+.
         */

        if (pDrvCtrl->rtgDevType == RTG_DEVTYPE_8169)
            rxSts >>= 1;

        if (rxSts & RTG_RDESC_STAT_RXERRSUM)
            {
            pDrvCtrl->rtgInErrors++;
            RTG_LOGMSG("%s%d: bad packet, sts: %x (%p %d)\n", RTG_NAME,
                pDev->unitNumber, rxSts, pDesc, pDrvCtrl->rtgRxIdx, 0);
            goto skip;
            }

        pNewMblk = endPoolTupleGet (pDrvCtrl->rtgEndObj.pNetPool);

        if (pNewMblk == NULL)
            {
            pDrvCtrl->rtgInDiscards++;
            RTG_LOGMSG("%s%d: out of mBlks at %d\n", RTG_NAME,
                pDev->unitNumber, pDrvCtrl->rtgRxIdx,0,0,0);
            pDrvCtrl->rtgLastError.errCode = END_ERR_NO_BUF;
            muxError (&pDrvCtrl->rtgEndObj, &pDrvCtrl->rtgLastError);
skip:
            pMap = pDrvCtrl->rtgRxMblkMap[pDrvCtrl->rtgRxIdx];
            pDesc->rtg_bufaddr_lo =
                htole32(RTG_ADDR_LO(pMap->fragList[0].frag));
            pDesc->rtg_bufaddr_hi =
                htole32(RTG_ADDR_HI(pMap->fragList[0].frag));
            if (pDrvCtrl->rtgRxIdx == (pDrvCtrl->rtgRxDescCnt - 1))
               pDesc->rtg_cmdsts = htole32(pMap->fragList[0].fragLen |
                    RTG_RDESC_CMD_OWN | RTG_RDESC_CMD_EOR);
            else
                pDesc->rtg_cmdsts = htole32(pMap->fragList[0].fragLen |
                    RTG_RDESC_CMD_OWN);
            RTG_INC_DESC(pDrvCtrl->rtgRxIdx, pDrvCtrl->rtgRxDescCnt);
            loopCounter--;
            pDesc = &pDrvCtrl->rtgRxDescMem[pDrvCtrl->rtgRxIdx];
            continue;
            }

        /* Get the map for this mBlk. */

        pMap = pDrvCtrl->rtgRxMblkMap[pDrvCtrl->rtgRxIdx];

        /* Sync the packet buffer and unload the map. */

        vxbDmaBufSync (pDev, pDrvCtrl->rtgMblkTag,
            pMap, VXB_DMABUFSYNC_PREREAD);
        vxbDmaBufMapUnload (pDrvCtrl->rtgMblkTag, pMap);

        /* Swap the mBlks. */

        pMblk = pDrvCtrl->rtgRxMblk[pDrvCtrl->rtgRxIdx];
        pDrvCtrl->rtgRxMblk[pDrvCtrl->rtgRxIdx] = pNewMblk;
        pNewMblk->m_next = NULL;
        if (pDrvCtrl->rtgMaxMtu == RTG_JUMBO_MTU)
            pNewMblk->m_len = pNewMblk->m_pkthdr.len = END_JUMBO_CLSIZE;
        RTG_ADJ (pNewMblk);

        /* don't need return from function call */

        (void) vxbDmaBufMapMblkLoad (pDev, pDrvCtrl->rtgMblkTag, pMap, pNewMblk, 0);

        pDesc->rtg_bufaddr_lo = htole32(RTG_ADDR_LO(pMap->fragList[0].frag));
        pDesc->rtg_bufaddr_hi = htole32(RTG_ADDR_HI(pMap->fragList[0].frag));

        pMap->fragList[0].fragLen -= 8;

        /* Reset this descriptor's CMD/STS fields. */

        if (pDrvCtrl->rtgRxIdx == (pDrvCtrl->rtgRxDescCnt - 1))
            pDesc->rtg_cmdsts = htole32(pMap->fragList[0].fragLen |
                RTG_RDESC_CMD_OWN | RTG_RDESC_CMD_EOR);
        else
            pDesc->rtg_cmdsts = htole32(pMap->fragList[0].fragLen |
                RTG_RDESC_CMD_OWN);

        pMblk->m_len = pMblk->m_pkthdr.len = rxLen - ETHER_CRC_LEN;
        pMblk->m_flags = M_PKTHDR|M_EXT;

#ifdef RTG_RX_FIXUP
        rtgRxFixup (pMblk);
#endif
        /* Handle checksum offload. */

        if (pDrvCtrl->rtgCaps.cap_enabled & IFCAP_RXCSUM)
            {
            if (rxSts & RTG_RDESC_STAT_PROTOID)
                pMblk->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
            if (!(rxSts & RTG_RDESC_STAT_IPSUMBAD))
                pMblk->m_pkthdr.csum_flags |= CSUM_IP_VALID;
            if ((RTG_TCPPKT(rxSts) && !(rxSts & RTG_RDESC_STAT_TCPSUMBAD)) ||
                (RTG_UDPPKT(rxSts) && !(rxSts & RTG_RDESC_STAT_UDPSUMBAD)))
                {
                pMblk->m_pkthdr.csum_flags |= CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
                pMblk->m_pkthdr.csum_data = 0xffff;
                }
            }

        if (pDrvCtrl->rtgCaps.cap_enabled & IFCAP_VLAN_HWTAGGING)
           {
           if (rxVlan & RTG_RDESC_VLANCTL_TAG)
               {
               pMblk->m_pkthdr.csum_flags |= CSUM_VLAN;
               pMblk->m_pkthdr.vlan = (UINT16)ntohs(rxVlan & RTG_RDESC_VLANCTL_DATA);
               }
           }

        /* Advance to the next descriptor */

        RTG_INC_DESC(pDrvCtrl->rtgRxIdx, pDrvCtrl->rtgRxDescCnt);
        loopCounter--;

        pDrvCtrl->rtgInOctets += pMblk->m_len;
        if (rxSts & RTG_RDESC_STAT_UCAST)
            pDrvCtrl->rtgInUcasts++;
        if (rxSts & RTG_RDESC_STAT_MCAST)
            pDrvCtrl->rtgInMcasts++;
        if (rxSts & RTG_RDESC_STAT_BCAST)
            pDrvCtrl->rtgInBcasts++;

        END_RCV_RTN_CALL (&pDrvCtrl->rtgEndObj, pMblk);

        pDesc = &pDrvCtrl->rtgRxDescMem[pDrvCtrl->rtgRxIdx];
        }

    if (loopCounter == 0)
        {
        jobQueuePost (pDrvCtrl->rtgJobQueue, &pDrvCtrl->rtgRxJob);
        return;
        }

    vxAtomic32Set (&pDrvCtrl->rtgRxPending, FALSE);

    return;
    }

/******************************************************************************
*
* rtgEndTxHandle - process TX completion events
*
* This function is scheduled by the ISR to run in the context of tNetTask
* whenever an TX interrupt is received. It runs through all of the
* TX register pairs and checks the TX status to see how many have
* completed. For each completed transmission, the associated TX mBlk
* is released, and the outbound packet stats are updated.
*
* In the event that a TX underrun error is detected, the TX FIFO
* threshold is increased. This will continue until the maximum TX
* FIFO threshold is reached.
*
* If the transmitter has stalled, this routine will also call muxTxRestart()
* to drain any packets that may be waiting in the protocol send queues,
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEndTxHandle
    (
    void * pArg
    )
    {
    QJOB *pJob;
    RTG_DRV_CTRL *pDrvCtrl;
    VXB_DEVICE_ID pDev;
    VXB_DMA_MAP_ID pMap;
    RTG_DESC * pDesc;
    UINT32 txSts;
    BOOL restart = FALSE;
    M_BLK_ID pMblk;

    pJob = pArg;
    pDrvCtrl = member_to_object (pJob, RTG_DRV_CTRL, rtgTxJob);
    pDev = pDrvCtrl->rtgDev;

    END_TX_SEM_TAKE (&pDrvCtrl->rtgEndObj, WAIT_FOREVER); 

    while (pDrvCtrl->rtgTxFree < pDrvCtrl->rtgTxDescCnt)
        {
        pDesc = &pDrvCtrl->rtgTxDescMem[pDrvCtrl->rtgTxCons];
        txSts = le32toh(pDesc->rtg_cmdsts);

        if (txSts & RTG_TDESC_STAT_OWN)
            break;

        pMblk = pDrvCtrl->rtgTxMblk[pDrvCtrl->rtgTxCons];
        pMap = pDrvCtrl->rtgTxMblkMap[pDrvCtrl->rtgTxCons];

        if (pMblk != NULL)
            {
            pDrvCtrl->rtgOutOctets += pMblk->m_pkthdr.len;
            if ((UINT8)pMblk->m_data[0] == 0xFF)
                pDrvCtrl->rtgOutBcasts++;
            else if ((UINT8)pMblk->m_data[0] & 0x1)
                pDrvCtrl->rtgOutMcasts++;
            else
                pDrvCtrl->rtgOutUcasts++;
            vxbDmaBufMapUnload (pDrvCtrl->rtgMblkTag, pMap);
            endPoolTupleFree (pMblk);
            pDrvCtrl->rtgTxMblk[pDrvCtrl->rtgTxCons] = NULL;
            }

        pDesc->rtg_cmdsts &= htole32(RTG_TDESC_CMD_EOR);
        pDesc->rtg_vlanctl = 0;

        pDrvCtrl->rtgTxFree++;
        RTG_INC_DESC (pDrvCtrl->rtgTxCons, pDrvCtrl->rtgTxDescCnt);

        /*
         * We released at least one descriptor: if the transmit
         * channel is stalled, unstall it.
         */

        if (pDrvCtrl->rtgTxStall == TRUE)
            {
            pDrvCtrl->rtgTxStall = FALSE;
            restart = TRUE;
            }
 
        }

    END_TX_SEM_GIVE (&pDrvCtrl->rtgEndObj); 

    vxAtomic32Set (&pDrvCtrl->rtgTxPending, FALSE);

    /*
     * Some chips will ignore a second TX request issued while an
     * existing transmission is in progress. If the transmitter goes
     * idle but there are still packets waiting to be sent, we need
     * to restart the channel here to flush them out. This only seems
     * to be required with the PCIe devices.
     */

    if (pDrvCtrl->rtgTxFree < pDrvCtrl->rtgTxDescCnt)
        CSR_WRITE_1(pDrvCtrl->rtgDev, pDrvCtrl->rtgTxStartReg, RTG_TXPP_NPQ);

    if (restart == TRUE)
        muxTxRestart (pDrvCtrl);

    return;
    }

/*****************************************************************************
*
* rtgEndIntHandle - task level interrupt handler
*
* This routine is scheduled to run in tNetTask by the interrupt service
* routine whenever a chip interrupt occurs. This function will check
* what interrupt events are pending and schedule additional jobs to
* service them. Once there are no more events waiting to be serviced
* (i.e. the chip has gone idle), interrupts will be unmasked so that
* the ISR can fire again.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

LOCAL void rtgEndIntHandle
    (
    void * pArg
    )
    {
    QJOB *pJob;
    RTG_DRV_CTRL *pDrvCtrl;
    VXB_DEVICE_ID pDev;
    UINT16 status;

    pJob = pArg;
    pDrvCtrl = member_to_object (pJob, RTG_DRV_CTRL, rtgIntJob);
    pDev = pDrvCtrl->rtgDev;

    status = CSR_READ_2(pDev, RTG_ISR);
    CSR_WRITE_2(pDev, RTG_ISR, status);

    if (status & RTG_RXINTRS &&
        vxAtomic32Set (&pDrvCtrl->rtgRxPending, TRUE) == FALSE)
        jobQueuePost (pDrvCtrl->rtgJobQueue, &pDrvCtrl->rtgRxJob);

    if (status & RTG_TXINTRS &&
        vxAtomic32Set (&pDrvCtrl->rtgTxPending, TRUE) == FALSE)
        jobQueuePost (pDrvCtrl->rtgJobQueue, &pDrvCtrl->rtgTxJob);

    /* May as well just do this one directly. */
    if (status & (RTG_ISR_LINKCHG|RTG_ISR_CABLE_LEN_CHGD))
        rtgLinkUpdate (pDev);

    if (CSR_READ_2(pDev, RTG_ISR) & RTG_INTRS)
        {
        jobQueuePost (pDrvCtrl->rtgJobQueue, &pDrvCtrl->rtgIntJob);
        return;
        }

    vxAtomic32Set (&pDrvCtrl->rtgIntPending, FALSE);
    CSR_WRITE_2(pDev, RTG_IMR, pDrvCtrl->rtgIntrs);

    return;
    }

/******************************************************************************
*
* rtgEndEncap - encapsulate an outbound packet in the TX DMA ring
*
* This function sets up a descriptor for a packet transmit operation.
* With the RealTek 'cplus' style devices, the TX DMA ring consists of
* descriptors that each describe a single packet fragment. We consume
* as many descriptors as there are mBlks in the outgoing packet, unless
* the chain is too long. The length is limited by the number of DMA
* segments we want to allow in a given DMA map. If there are too many
* segments, this routine will fail, and the caller must coalesce the
* data into fewer buffers and try again.
*
* This routine will also fail if there aren't enough free descriptors
* available in the ring, in which case the caller must defer the
* transmission until more descriptors are completed by the chip.
*
* RETURNS: ENOSPC if there are too many fragments in the packet, EAGAIN
* if the DMA ring is full, otherwise OK.
*
* ERRNO: N/A
*/

LOCAL int rtgEndEncap
    (
    RTG_DRV_CTRL * pDrvCtrl,
    M_BLK_ID pMblk
    )
    {
    VXB_DEVICE_ID pDev;
    VXB_DMA_MAP_ID pMap;
    RTG_DESC * pDesc, * pFirst;
    UINT32 firstIdx, lastIdx = 0;
    UINT32 cmdSts = 0;
    int i;

    pDev = pDrvCtrl->rtgDev;
    firstIdx = pDrvCtrl->rtgTxProd;
    pMap = pDrvCtrl->rtgTxMblkMap[pDrvCtrl->rtgTxProd];

    if (pDrvCtrl->rtgTxMblk[pDrvCtrl->rtgTxProd] != NULL)
        return (EAGAIN);

    /*
     * Load the DMA map to build the segment list.
     * This will fail if there are too many segments.
     */

    if (vxbDmaBufMapMblkLoad (pDev, pDrvCtrl->rtgMblkTag,
        pMap, pMblk, 0) != OK || (pMap->nFrags > pDrvCtrl->rtgTxFree))
        {
        vxbDmaBufMapUnload (pDrvCtrl->rtgMblkTag, pMap);
        return (ENOSPC);
        }

    pFirst = &pDrvCtrl->rtgTxDescMem[pDrvCtrl->rtgTxProd];

    for (i = 0; i < pMap->nFrags; i++)
        {
        pDesc = &pDrvCtrl->rtgTxDescMem[pDrvCtrl->rtgTxProd];
        pDesc->rtg_bufaddr_lo = htole32(RTG_ADDR_LO(pMap->fragList[i].frag));
        pDesc->rtg_bufaddr_hi = htole32(RTG_ADDR_HI(pMap->fragList[i].frag));

        cmdSts = (UINT32)(pMap->fragList[i].fragLen & RTG_TDESC_CMD_FRAGLEN);

        if (i == 0)
            cmdSts |= RTG_TDESC_CMD_SOF;
        else
            cmdSts |= RTG_TDESC_CMD_OWN;

        if (i == (pMap->nFrags - 1))
            cmdSts |= RTG_TDESC_CMD_EOF;
        if (pDrvCtrl->rtgTxProd == (pDrvCtrl->rtgTxDescCnt - 1))
            cmdSts |= RTG_TDESC_CMD_EOR;

        /*
         * Checksum offload bits must be set in all descriptors
         * for a given packet where we want checksum offloading
         * to be performed.
         */

        if (pDrvCtrl->rtgDescV2 == FALSE &&
            pDrvCtrl->rtgCaps.cap_enabled & IFCAP_TXCSUM)
            {
	    /*
	     * Even when the stack wants only the transport checksum offloaded,
	     * the device needs to be asked to do the IP checksum also,
	     * or it doesn't do the transport checksum.
	     * The IP checksum offload is not adversely affected by the fact
	     * that the IP header already has the checksum stored in the
	     * checksum field; the device apparently skips this field when
	     * calculating the IP header checksum.
	     */
            if (pMblk->m_pkthdr.csum_flags & CSUM_TCP)
                cmdSts |= (RTG_TDESC_CMD_TCPCSUM | RTG_TDESC_CMD_IPCSUM);
            else if (pMblk->m_pkthdr.csum_flags & CSUM_UDP)
                cmdSts |= (RTG_TDESC_CMD_UDPCSUM | RTG_TDESC_CMD_IPCSUM);
            else if (pMblk->m_pkthdr.csum_flags & CSUM_IP)
                cmdSts |= RTG_TDESC_CMD_IPCSUM;
            }

        pDesc->rtg_cmdsts = htole32(cmdSts);
        pDrvCtrl->rtgTxFree--;
        lastIdx = pDrvCtrl->rtgTxProd;
        RTG_INC_DESC(pDrvCtrl->rtgTxProd, pDrvCtrl->rtgTxDescCnt);
        }

    /* Save the mBlk for later. */
    pDrvCtrl->rtgTxMblk[lastIdx] = pMblk;

    /*
     * Insure that the map for this transmission
     * is placed at the array index of the last descriptor
     * in this chain.  (Swap last and first dmamaps.)
     */

    pDrvCtrl->rtgTxMblkMap[firstIdx] = pDrvCtrl->rtgTxMblkMap[lastIdx];

    pDrvCtrl->rtgTxMblkMap[lastIdx] = pMap;

    /* VLAN tags go in the first descriptor only */

    if (pDrvCtrl->rtgCaps.cap_enabled & IFCAP_VLAN_HWTAGGING)
        {
        if (pMblk->m_pkthdr.csum_flags & CSUM_VLAN)
            pFirst->rtg_vlanctl = htole32(htons(pMblk->m_pkthdr.vlan) |
                RTG_TDESC_VLANCTL_TAG);
        /*
         * For the RTL8168C, RTL8168CP, RTL8168D and RTL8102E devices,
         * the checksum offload bits have been moved to the vlanctl
         * control word.
         */

        if (pDrvCtrl->rtgDescV2 == TRUE)
            {
            if (pMblk->m_pkthdr.csum_flags & CSUM_TCP)
                pFirst->rtg_vlanctl |=
                    htole32(RTG_TDESC_VLANCTL_TCPCSUM|RTG_TDESC_VLANCTL_IPCSUM);
            else if (pMblk->m_pkthdr.csum_flags & CSUM_UDP)
                pFirst->rtg_vlanctl |=
                    htole32(RTG_TDESC_VLANCTL_UDPCSUM|RTG_TDESC_VLANCTL_IPCSUM);
            else if (pMblk->m_pkthdr.csum_flags & CSUM_IP)
                pFirst->rtg_vlanctl |= htole32(RTG_TDESC_VLANCTL_IPCSUM);
            }
        }

    /* Sync the buffer. */

    vxbDmaBufSync (pDev, pDrvCtrl->rtgMblkTag, pMap, VXB_DMABUFSYNC_POSTWRITE);

    /* Transfer descriptors to the chip. */

    pFirst->rtg_cmdsts |= htole32(RTG_TDESC_CMD_OWN);

    return (OK);
    }


/******************************************************************************
*
* rtgEndSend - transmit a packet
*
* This function transmits the packet specified in <pMblk>. The RealTek
* 8139C+/8169 controllers implement true descriptor based DMA (unlike
* the earlier 8139 devices). Each descriptor describes a single frame
* fragment, and transfers are done in-place (zero copy). Frames will be
* coalesced into a single buffer if not enough descriptors are available
* to handle all the fragments. The transmitter performs automatic short
* frame padding.
*
* RETURNS: OK, ERROR, or END_ERR_BLOCK.
*
* ERRNO: N/A
*/

LOCAL int rtgEndSend
    (
    END_OBJ * pEnd,
    M_BLK_ID pMblk
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    VXB_DEVICE_ID pDev;
    M_BLK_ID pTmp;
    int rval, len;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;

    if (pDrvCtrl->rtgPolling == TRUE)
        {
        netMblkClChainFree (pMblk);
        return (ERROR);
        }

    pDev = pDrvCtrl->rtgDev;

    END_TX_SEM_TAKE (pEnd, WAIT_FOREVER); 

    if (!pDrvCtrl->rtgTxFree || !(pDrvCtrl->rtgCurStatus & IFM_ACTIVE))
        goto blocked;

    /*
     * First, try to do an in-place transmission, using
     * gather-write DMA.
     *
     * With some of the RealTek chips, using the checksum offload
     * support in conjunction with the autopadding feature results
     * in the transmission of corrupt frames. For example, if we
     * need to send a really small IP fragment that's less than 60
     * bytes in size, and IP header checksumming is enabled, the
     * resulting ethernet frame that appears on the wire will
     * have a garbled payload. To work around this, if we're asked
     * to transmit a short frame with checksum offload, we manually
     * pad it out to the minimum ethernet frame size. We do this by
     * pretending in-place DMA failed, and failing over to the
     * coalesce case below. This is somewhat inefficient, but these
     * kinds of runt transmissions occur fairly infrequently so
     * the overall impact on performance is low.
     *
     * Note: the 8111B/8168B and 8101B PCIe devices seem to have
     * a different checksum problem. The checksum offload support
     * works correctly in conjunction with the hardware autopadding,
     * however the chip will calculate incorrect TCP checksums for
     * short TCP segments that have been manually padded as a result
     * of the workaround for the IP header checksum botch described
     * above. To compensate for this, we skip the manual padding of
     * short frames if the frame contains a TCP segment. The earlier
     * devices with the IP header checksum bug seem to handle TCP
     * segments correctly, and this allows the PCIe devices to work
     * properly too.
     *
     * We must also exclude UDP packets here too. Further testing
     * has shown that on the 8111C chip, if we manually pad a short
     * UDP frame, the chip will calculate an incorrect payload checksum.
     * Presumably the device is using the frame length to determine
     * the length of the UDP payload (it really ought to be using
     * the length from the UDP header instead).
     *
     * Note that the IPNET TCP/IP stack currently never requests
     * hardware IP header checksum offloading. We still support it
     * in the driver though, just in case a custom protocol has
     * need of the feature.
     */

    if (pMblk->m_pkthdr.csum_flags & (CSUM_VLAN|CSUM_IP) &&
        !(pMblk->m_pkthdr.csum_flags & (CSUM_UDP|CSUM_TCP)) &&
        pMblk->m_pkthdr.len < ETHERSMALL)
        rval = ENOSPC;
    else
        rval = rtgEndEncap (pDrvCtrl, pMblk);

    /*
     * If rtgEndEncap() returns ENOSPC, it means it ran out
     * of TBD entries and couldn't encapsulare the whole
     * packet fragment chain. In that case, we need to
     * coalesce everything into a single buffer and try
     * again. If any other error is returned, then something
     * went wrong, and we have to abort the transmission
     * entirely.
     */
 
    if (rval == ENOSPC)
        {
        if ((pTmp = endPoolTupleGet (pDrvCtrl->rtgEndObj.pNetPool)) == NULL)
            goto blocked;
 
        len = netMblkToBufCopy (pMblk, mtod(pTmp, char *), NULL);

        /*
         * Manually pad short frames, and zero the pad space to
         * avoid leaking data.
         */

        if (len < ETHERSMALL)
            {
            bzero (mtod(pTmp, char *) + len, ETHERSMALL - len);
            len += ETHERSMALL - len;
            }
        pTmp->m_len = pTmp->m_pkthdr.len = len;
        pTmp->m_flags = pMblk->m_flags;
        pTmp->m_pkthdr.csum_flags = pMblk->m_pkthdr.csum_flags;
        pTmp->m_pkthdr.csum_data = pMblk->m_pkthdr.csum_data;
        pTmp->m_pkthdr.vlan = pMblk->m_pkthdr.vlan;
        /* Try transmission again, should succeed this time. */
        rval = rtgEndEncap (pDrvCtrl, pTmp);
        if (rval == OK)
            netMblkClChainFree (pMblk);
        else
            netMblkClChainFree (pTmp);
        }
 
    if (rval != OK)
        goto blocked;

    /* Issue transmit command */

    CSR_WRITE_1(pDrvCtrl->rtgDev, pDrvCtrl->rtgTxStartReg, RTG_TXPP_NPQ);

    END_TX_SEM_GIVE (pEnd); 
    return (OK);

blocked:
    pDrvCtrl->rtgTxStall = TRUE;
    END_TX_SEM_GIVE (pEnd); 

    return (END_ERR_BLOCK);
    }

/******************************************************************************
*
* rtgEndPollSend - polled mode transmit routine
*
* This function is similar to the rtgEndSend() routine shown above, except
* it performs transmissions synchronously with interrupts disabled. After
* the transmission is initiated, the routine will poll the state of the
* TX status register associated with the current slot until transmission
* completed. If transmission times out, this routine will return ERROR.
*
* RETURNS: OK, EAGAIN, or ERROR
*
* ERRNO: N/A
*/

LOCAL STATUS rtgEndPollSend
    (
    END_OBJ * pEnd,
    M_BLK_ID pMblk
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    VXB_DEVICE_ID pDev;
    RTG_DESC * pDesc;
    VXB_DMA_MAP_ID pMap;
    UINT32 txSts;
    UINT16 status;
    M_BLK_ID pTmp;
    int len, i;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;

    if (pDrvCtrl->rtgPolling == FALSE)
        return (ERROR);

    pDev = pDrvCtrl->rtgDev;

    pTmp = pDrvCtrl->rtgPollBuf;

    len = netMblkToBufCopy (pMblk, mtod(pTmp, char *), NULL);
    pTmp->m_len = pTmp->m_pkthdr.len = len;
    pTmp->m_flags = pMblk->m_flags;
    pTmp->m_pkthdr.csum_flags = pMblk->m_pkthdr.csum_flags;
    pTmp->m_pkthdr.csum_data = pMblk->m_pkthdr.csum_data;
    pTmp->m_pkthdr.vlan = pMblk->m_pkthdr.vlan;

    if (rtgEndEncap (pDrvCtrl, pTmp) != OK)
        return (EAGAIN);

    /* Issue transmit command */

    CSR_WRITE_1(pDrvCtrl->rtgDev, pDrvCtrl->rtgTxStartReg, RTG_TXPP_NPQ);

    pMap = pDrvCtrl->rtgTxMblkMap[pDrvCtrl->rtgTxCons];
    pDesc = &pDrvCtrl->rtgTxDescMem[pDrvCtrl->rtgTxCons];

    for (i = 0; i < RTG_TIMEOUT; i++)
        {
        txSts = le32toh(pDesc->rtg_cmdsts);
        status = CSR_READ_2(pDev, RTG_ISR);
        if (status & RTG_TXINTRS && !(txSts & RTG_TDESC_STAT_OWN))
           break;
        }

    CSR_WRITE_2(pDev, RTG_ISR, status);

    /* Remember to unload the map once transmit completes. */
 
    vxbDmaBufMapUnload (pDrvCtrl->rtgMblkTag, pMap);
    pDrvCtrl->rtgTxMblk[pDrvCtrl->rtgTxCons] = NULL;
    pDesc->rtg_cmdsts &= htole32(RTG_TDESC_CMD_EOR);
    pDesc->rtg_vlanctl = 0;
    pDrvCtrl->rtgTxFree++;
    RTG_INC_DESC(pDrvCtrl->rtgTxCons, pDrvCtrl->rtgTxDescCnt);

    if (i == RTG_TIMEOUT || (txSts & RTG_TDESC_STAT_ERR))
        {
        pDrvCtrl->rtgOutErrors++;
        return (ERROR);
        }

    pDrvCtrl->rtgOutOctets += pMblk->m_pkthdr.len;
    if ((UINT8)pMblk->m_data[0] == 0xFF)
        pDrvCtrl->rtgOutBcasts++;
    else if ((UINT8)pMblk->m_data[0] & 0x1)
        pDrvCtrl->rtgOutMcasts++;
    else
        pDrvCtrl->rtgOutUcasts++;

    return (OK);
    }

/******************************************************************************
*
* rtgEndPollReceive - polled mode receive routine
*
* This function receive a packet in polled mode, with interrupts disabled.
* It's similar in operation to the rtgEndRxHandle() routine, except it
* doesn't process more than one packet at a time and does not load out
* buffers. Instead, the caller supplied an mBlk tuple into which this
* function will place the received packet.
*
* If no packet is available, this routine will return EAGAIN. If the
* supplied mBlk is too small to contain the received frame, the routine
* will return ERROR.
*
* RETURNS: OK, EAGAIN, or ERROR
*
* ERRNO: N/A
*/

LOCAL int rtgEndPollReceive
    (
    END_OBJ * pEnd,
    M_BLK_ID pMblk
    )
    {
    RTG_DRV_CTRL * pDrvCtrl;
    VXB_DEVICE_ID pDev;
    VXB_DMA_MAP_ID pMap;
    RTG_DESC * pDesc;
    M_BLK_ID pPkt;
    UINT32 rxSts, rxLen, rxVlan;
    UINT16 status;
    int rval = ERROR;

    pDrvCtrl = (RTG_DRV_CTRL *)pEnd;
    if (pDrvCtrl->rtgPolling == FALSE)
        return (ERROR);

    if (!(pMblk->m_flags & M_EXT))
        return (ERROR);

    pDev = pDrvCtrl->rtgDev;

    status = CSR_READ_2(pDev, RTG_ISR);
    CSR_WRITE_2(pDev, RTG_ISR, status);

    pDesc = &pDrvCtrl->rtgRxDescMem[pDrvCtrl->rtgRxIdx];
    pPkt = pDrvCtrl->rtgRxMblk[pDrvCtrl->rtgRxIdx];
    pMap = pDrvCtrl->rtgRxMblkMap[pDrvCtrl->rtgRxIdx];

    rxSts = le32toh(pDesc->rtg_cmdsts);    

    if (rxSts & RTG_RDESC_STAT_OWN)
        return (EAGAIN);

    rxLen = (rxSts & pDrvCtrl->rtgRxLenMask) - ETHER_CRC_LEN;
    rxVlan = le32toh(pDesc->rtg_vlanctl);

    if ((rxSts & (RTG_RDESC_STAT_SOF|RTG_RDESC_STAT_EOF)) !=
        (RTG_RDESC_STAT_SOF|RTG_RDESC_STAT_EOF))
        goto skip;

    if (pDrvCtrl->rtgDevType == RTG_DEVTYPE_8169)
        rxSts >>= 1;

    if (pMblk->m_len < rxLen)
        return (ERROR);

    if (rxSts & RTG_RDESC_STAT_RXERRSUM)
        pDrvCtrl->rtgInErrors++;
    else
        {
        vxbDmaBufSync (pDev, pDrvCtrl->rtgMblkTag,
            pMap, VXB_DMABUFSYNC_PREREAD);
        m_adj (pMblk, 2);
        pMblk->m_flags |= M_PKTHDR;
        pMblk->m_len = pMblk->m_pkthdr.len = rxLen;
        bcopy (mtod(pPkt, char *), mtod(pMblk, char *), rxLen);

        /* Handle checksum offload. */

        if (pDrvCtrl->rtgCaps.cap_enabled & IFCAP_RXCSUM)
            {
            if (rxSts & RTG_RDESC_STAT_PROTOID)
                pMblk->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
            if (!(rxSts & RTG_RDESC_STAT_IPSUMBAD))
                pMblk->m_pkthdr.csum_flags |= CSUM_IP_VALID;
            if ((RTG_TCPPKT(rxSts) && !(rxSts & RTG_RDESC_STAT_TCPSUMBAD)) ||
                (RTG_UDPPKT(rxSts) && !(rxSts & RTG_RDESC_STAT_UDPSUMBAD)))
                {
                pMblk->m_pkthdr.csum_flags |= CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
                pMblk->m_pkthdr.csum_data = 0xffff;
                }
            }

        if (pDrvCtrl->rtgCaps.cap_enabled & IFCAP_VLAN_HWTAGGING)
           {
           if (rxVlan & RTG_RDESC_VLANCTL_TAG)
               {
               pMblk->m_pkthdr.csum_flags |= CSUM_VLAN;
               pMblk->m_pkthdr.vlan = (UINT16)ntohs(rxVlan & RTG_RDESC_VLANCTL_DATA);
               }
           }

        pDrvCtrl->rtgInOctets += pMblk->m_len;
        if (rxSts & RTG_RDESC_STAT_UCAST)
            pDrvCtrl->rtgInUcasts++;
        if (rxSts & RTG_RDESC_STAT_MCAST)
            pDrvCtrl->rtgInMcasts++;
        if (rxSts & RTG_RDESC_STAT_BCAST)
            pDrvCtrl->rtgInBcasts++;

        rval = OK;
        }

skip:

    /* Reset the descriptor */

    pDesc->rtg_bufaddr_lo = htole32(RTG_ADDR_LO(pMap->fragList[0].frag));
    pDesc->rtg_bufaddr_hi = htole32(RTG_ADDR_HI(pMap->fragList[0].frag));
    if (pDrvCtrl->rtgRxIdx == (pDrvCtrl->rtgRxDescCnt - 1))
        pDesc->rtg_cmdsts = htole32(pMap->fragList[0].fragLen |
            RTG_RDESC_CMD_OWN | RTG_RDESC_CMD_EOR);
    else
        pDesc->rtg_cmdsts = htole32(pMap->fragList[0].fragLen |
            RTG_RDESC_CMD_OWN);

    RTG_INC_DESC(pDrvCtrl->rtgRxIdx, pDrvCtrl->rtgRxDescCnt);

    return (rval);
    }

LOCAL void rtgDelay
    (
    UINT32 usec
    )
    {
    vxbUsDelay (usec);
    return;
    }

LOCAL void doRtgMuxConnect
    (
    void
    )
    {
    void * unused;
    int i; 

    for ( i = 0; i < CurrentRtgNum; ++i )
        {
        printf("unit = %d name = %s\n", pDevTemp[i]->unitNumber, pDevTemp[i]->pName);
        rtgMuxConnect(pDevTemp[i], unused);
        }
    }

STATUS sysRtgEndInit
    (
    void
    )
    {
    mvPhyRegister();
    rtgPhyRegister();

    rtgRegister();
    doRtgMuxConnect();
    }
