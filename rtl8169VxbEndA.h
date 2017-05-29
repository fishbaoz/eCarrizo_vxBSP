/* rtl8169VxbEnd.h - register definitions for the RealTek C+ ethernet chips */

/*
 * Copyright (c) 2006-2010 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01i,16apr10,wap  Add support for RTL8168DP
01h,17feb10,jc0  LP64 adaptation.
01g,27feb09,wap  Add support for RTL8103EL
01f,11aug08,wap  Add support for newer adapters
01e,10jul07,wap  Make this driver SMP safe, convert to new register access API
01d,07dec06,wap  Add jumbo frame support
01c,28jun06,wap  Correct revision code for RTG8111B
01b,17jun06,wap  Use memory mapped BAR instead of I/O space BAR, add
                 MII semaphore
01a,16mar06,wap  written
*/

#ifndef __INCrtl8169VxbEndh
#define __INCrtl8169VxbEndh

#ifdef __cplusplus
extern "C" {
#endif

IMPORT void rtgRegister (void);

#ifndef BSP_VERSION

/* Supported vendor and device IDs */

#define RTG_VENDORID		0x10EC

#define RTG_DEVICEID_8101E	0x8136
#define RTG_DEVICEID_8139	0x8139
#define RTG_DEVICEID_8169SC	0x8167
#define RTG_DEVICEID_8168	0x8168
#define RTG_DEVICEID_8169	0x8169

#define DLINK_VENDORID		0x1186
#define DLINK_DEVICEID_528T	0x4300

#define COREGA_VENDORID		0x1259
#define COREGA_DEVICEID_CGLAPCIGT	0xc107

#define LINKSYS_VENDORID	0x1737
#define LINKSYS_DEVICEID_EG1032	0x1032
#define LINKSYS_SUBDEVICE_EG1032_REV3	0x0024

/* PCI configuration space table */
#define RTG_PCI_PCIE_CAP_OFFSET	0x60

#define USR_VENDORID		0x16EC
#define USR_DEVICEID_8169	0x0116

/*
 * Station ID registers. The RealTek manual documents these
 * as if there are six of them, each a byte wide, but in reality
 * you have to access them using 32 bit reads/writes. So we
 * define them as 2 offsets here.
 */

#define RTG_IDR0		0x0000
#define RTG_IDR1		0x0004

/*
 * Multicast hash registers. The 8139/8169 has a 64-bit hash table.
 * This must also be accesed using 32 bit reads/writes.
 */

#define RTG_MAR0		0x0008
#define RTG_MAR1		0x000C

/* Transmit status registers, 8139 legacy mode */

#define RTG_TXSTAT0		0x0010
#define RTG_TXSTAT1		0x0014
#define RTG_TXSTAT2		0x0018
#define RTG_TXSTAT3		0x001C

#define RTG_TXSTAT_SIZE		0x00001FFF
#define RTG_TXSTAT_OWN		0x00002000
#define RTG_TXSTAT_UFLOW	0x00004000	/* underrun error */
#define RTG_TXSTAT_OK		0x00008000	/* transmission OK */
#define RTG_TXSTAT_THRESH	0x003F0000	/* TX start threshold */
#define RTG_TXSTAT_COLLCNT	0x0F000000	/* collision count */
#define RTG_TXSTAT_HBEAT	0x10000000	/* hearbeat error (10Mbps) */
#define RTG_TXSTAT_LATECOLL	0x20000000	/* late collision */
#define RTG_TXSTAT_TXABRT	0x40000000	/* TX aborted */
#define RTG_TXSTAT_CARRLOSS	0x80000000	/* carrier loss error */

#define RTG_TXTHRESH(x)		((((x) >> 3) << 16) & 0x003F0000)

/* TX statistics command register, C+ mode */

#define RTG_DUMPSTATS_LO	0x0010
#define RTG_DUMPSTSTS_HI	0x0014

#define RTG_DUMPSTATSLO_DUMP	0x00000008	/* initiate stats dump */

/* Transmit buffer pointer registers, 8139 legacy mode */

#define RTG_TXBASE0		0x0020
#define RTG_TXBASE1		0x0024
#define RTG_TXBASE2		0x0028
#define RTG_TXBASE3		0x002C

/*
 * TX DMA ring base address registers, C+ mode.
 * Ring 0 is the low priority transmit ring. Ring 1 is
 * the high priority ring.
 */

#define RTG_TXRINGBASE0_LO	0x0020
#define RTG_TXRINGBASE0_HI	0x0024
#define RTG_TXRINGBASE1_LO	0x0028
#define RTG_TXRINGBASE1_HI	0x002C

/* RX window base address */

#define RTG_RXBASE		0x0030

/* Early receive byte count (read only) */

#define RTG_RXEARLYCNT		0x0034

/* Early receive status (read only) */

#define RTG_RXEARLYSTS		0x0036

#define RTG_RXEARLYSTS_OK	0x01
#define RTG_RXEARLYSTS_OVW	0x02	/* Overwrite */
#define RTG_RXEARLYSTS_BAD	0x04	/* bad frame */
#define RTG_RXEARLYSTS_GOOD	0x08	/* good frame */

/* Command register */

#define RTG_CMD			0x0037

#define RTG_CMD_RX_EMPTY	0x01	/* RX window is empty (read only) */
#define RTG_CMD_TX_ENABLE	0x04	/* enable transmitter */
#define RTG_CMD_RX_ENABLE	0x08	/* enable receiver */
#define RTG_CMD_RESET		0x10	/* reset the controller */

/*
 * Current RX window offset (16 bits).
 * This register indicates the total number of bytes in the RX
 * window waiting to be read. The initial value is 0xFFF0.
 * This register can be updated by software.
 */

#define RTG_CURRXOFF		0x0038

#define RTG_CURRXOFF_INIT	0xFFF0

/* TX priority polling register, 8169 chip */

#define RTG_TXPRIOPOLL_8169	0x0038

/*
 * Current RX buffer address. This reflects the total number
 * of bytes waiting in the RX window. This is read only.
 */

#define RTG_CURRXLEN		0x003A

/*
 * Interrupt mask register (16 bits)
 * See ISR register definition below for bit definitions.
 */

#define RTG_IMR			0x003C

/* Interrupt status register (16 bits) */

#define RTG_ISR			0x003E

/* Interrupt status register (16 bits) */

#define RTG_ISR_RX_OK		0x0001	/* good frame received */
#define RTG_ISR_RX_ERR		0x0002	/* bad frame received */
#define RTG_ISR_TX_OK		0x0004	/* transmit completed successfully */
#define RTG_ISR_TX_ERR		0x0008	/* transmit completed with error */
#define RTG_ISR_RX_NODESC	0x0010	/* C+ mode only */
#define RTG_ISR_LINKCHG		0x0020  /* link change */
#define RTG_ISR_RX_OFLOW	0x0040  /* RX FIFO overflow  */
#define RTG_ISR_TX_NODESC	0x0080	/* C+ mode only */
#define RTG_ISR_SWI		0x0100	/* C+ mode only */
#define RTG_ISR_RX_FIFO_EMPTY	0x0200	/* C+ mode only, 8138 */
#define RTG_ISR_CABLE_LEN_CHGD	0x2000	/* cable length changed */
#define RTG_ISR_TIMER_EXPIRED	0x4000	/* timer ran out */
#define RTG_ISR_SYSTEM_ERR	0x8000	/* PCI system error */

#define RTG_INTRS (RTG_ISR_RX_OK|RTG_ISR_RX_ERR|RTG_ISR_TX_OK|	\
		   RTG_ISR_TX_ERR|RTG_ISR_RX_NODESC|RTG_ISR_TX_NODESC| \
		   RTG_ISR_LINKCHG|RTG_ISR_RX_OFLOW|RTG_ISR_TIMER_EXPIRED| \
		   RTG_ISR_SYSTEM_ERR|RTG_ISR_RX_FIFO_EMPTY)

#define RTG_RXINTRS (RTG_ISR_RX_OK|RTG_ISR_RX_ERR| \
		     RTG_ISR_RX_NODESC|RTG_ISR_RX_OFLOW| \
		     RTG_ISR_RX_FIFO_EMPTY)

#define RTG_TXINTRS (RTG_ISR_TX_OK|RTG_ISR_TX_ERR|RTG_ISR_TX_NODESC)

/* TX config register */

#define RTG_TXCFG		0x0040

#define RTG_TXCFG_CLRABRT	0x00000001      /* retransmit aborted pkt */
#define RTG_TXCFG_MAXDMA	0x00000700      /* max DMA burst size */
#define RTG_TXCFG_CRCAPPEND	0x00010000      /* CRC append (0 = yes) */
#define RTG_TXCFG_LOOPBKTST	0x00060000      /* loopback test */
#define RTG_TXCFG_IFG2		0x00080000      /* 8169 only */
#define RTG_TXCFG_IFG		0x03000000      /* interframe gap */
#define RTG_TXCFG_HWREV		0x7CC00000

#define RTG_TXDMA_16BYTES	0x00000000
#define RTG_TXDMA_32BYTES	0x00000100
#define RTG_TXDMA_64BYTES	0x00000200
#define RTG_TXDMA_128BYTES	0x00000300
#define RTG_TXDMA_256BYTES	0x00000400
#define RTG_TXDMA_512BYTES	0x00000500
#define RTG_TXDMA_1024BYTES	0x00000600
#define RTG_TXDMA_2048BYTES	0x00000700

#define RTG_LOOPTEST_OFF	0x00000000
#define RTG_LOOPTEST_ON		0x00020000

/* Known revision codes. */

#define RTG_HWREV_8169		0x00000000
#define RTG_HWREV_8110S		0x00800000
#define RTG_HWREV_8169S		0x04000000
#define RTG_HWREV_8169_8110SB	0x10000000
#define RTG_HWREV_8169_8110SC	0x18000000
#define RTG_HWREV_8102EL	0x24800000
#define RTG_HWREV_8103EL	0x24C00000
#define RTG_HWREV_8168D		0x28000000
#define RTG_HWREV_8168DP	0x28800000
#define RTG_HWREV_8168E		0x2C000000
#define RTG_HWREV_8168_SPIN1	0x30000000
#define RTG_HWREV_8100E		0x30800000
#define RTG_HWREV_8101E		0x34000000
#define RTG_HWREV_8102E		0x34800000
#define RTG_HWREV_8168_SPIN2	0x38000000
#define RTG_HWREV_8168_SPIN3	0x38400000
#define RTG_HWREV_8168C		0x3C000000
#define RTG_HWREV_8168C_SPIN2	0x3C400000
#define RTG_HWREV_8168CP	0x3C800000
#define RTG_HWREV_8139		0x60000000
#define RTG_HWREV_8139A		0x70000000
#define RTG_HWREV_8139AG	0x70800000
#define RTG_HWREV_8139B		0x78000000
#define RTG_HWREV_8130		0x7C000000
#define RTG_HWREV_8139C		0x74000000
#define RTG_HWREV_8139D		0x74400000
#define RTG_HWREV_8139CPLUS	0x74800000
#define RTG_HWREV_8101		0x74c00000
#define RTG_HWREV_8100		0x78800000
#define RTG_HWREV_8169_8110SBL	0x7CC00000

/* RX config register */

#define RTG_RXCFG		0x0044

#define RTG_RXCFG_RX_PROMISC	0x00000001	/* accept all unicast */
#define RTG_RXCFG_RX_INDIV	0x00000002	/* accept filter match */
#define RTG_RXCFG_RX_MULTI	0x00000004      /* accept all multicast */
#define RTG_RXCFG_RX_BROAD	0x00000008      /* accept all broadcast */
#define RTG_RXCFG_RX_RUNT	0x00000010	/* accept runts */
#define RTG_RXCFG_RX_ERRPKT	0x00000020	/* accept error frames */
#define RTG_RXCFG_WRAP		0x00000080	/* RX window wrapping */
#define RTG_RXCFG_MAXDMA	0x00000700
#define RTG_RXCFG_WINSZ		0x00001800	/* RX window size */
#define RTG_RXCFG_FIFOTHRESH	0x0000E000
#define RTG_RXCFG_RER8		0x00010000
#define RTG_RXCFG_EARLYINT	0x00020000
#define RTG_RXCFG_EARLYTHRESH	0x07000000

#define RTG_RXDMA_16BYTES	0x00000000
#define RTG_RXDMA_32BYTES	0x00000100
#define RTG_RXDMA_64BYTES	0x00000200
#define RTG_RXDMA_128BYTES	0x00000300
#define RTG_RXDMA_256BYTES	0x00000400
#define RTG_RXDMA_512BYTES	0x00000500
#define RTG_RXDMA_1024BYTES	0x00000600
#define RTG_RXDMA_UNLIMITED	0x00000700

#define RTG_RXWIN_8K		0x00000000
#define RTG_RXWIN_16K		0x00000800
#define RTG_RXWIN_32K		0x00001000
#define RTG_RXWIN_64K		0x00001800

#define RTG_RXFIFO_16BYTES	0x00000000
#define RTG_RXFIFO_32BYTES	0x00002000
#define RTG_RXFIFO_64BYTES	0x00004000
#define RTG_RXFIFO_128BYTES	0x00006000
#define RTG_RXFIFO_256BYTES	0x00008000
#define RTG_RXFIFO_512BYTES	0x0000A000
#define RTG_RXFIFO_1024BYTES	0x0000C000
#define RTG_RXFIFO_NOTHRESH	0x0000E000

/* Countdown timer register (32 bits) */

#define RTG_TIMER		0x0048

/* Missed packet counter */

#define RTG_MISSEDPKT		0x004C

/* EEPROM access register */

#define RTG_EECMD		0x0050

#define RTG_EECMD_DATAOUT	0x01    /* Data out */
#define RTG_EECMD_DATAIN	0x02    /* Data in */
#define RTG_EECMD_CLK		0x04    /* clock */
#define RTG_EECMD_SEL		0x08    /* chip select */
#define RTG_EECMD_MODE		0xC0

#define RTG_EEMODE_OFF		0x00
#define RTG_EEMODE_AUTOLOAD	0x40
#define RTG_EEMODE_PROGRAM	0x80
#define RTG_EEMODE_WRITECFG	0xC0

/* 9346 EEPROM commands */
#define RTG_9346_WRITE          0x5
#define RTG_9346_READ           0x6
#define RTG_9346_ERASE          0x7
#define RTG_9346_EWEN		0x4
#define RTG_9346_EWEN_ADDR	0x30
#define RTG_9456_EWDS		0x4
#define RTG_9346_EWDS_ADDR	0x00

#define RTG_EE_ID		0x00
#define RTG_EE_PCI_VID		0x01
#define RTG_EE_PCI_DID		0x02
/* Location of station address inside EEPROM */
#define RTG_EE_EADDR		0x07

#define RTG_EE_SIGNATURE	0x8129

/* Strapping config 0 */

#define RTG_CFG0		0x0051

/* Strapping config 1 */

#define RTG_CFG1		0x0052

#define RTG_CFG1_PME_ENABLE	0x01
#define RTG_CFG1_VPD		0x02
#define RTG_CFG1_IOMAP		0x04
#define RTG_CFG1_MEMMAP		0x08
#define RTG_CFG1_SPEED_DOWN	0x10
#define RTG_CFG1_LEDS0		0x40
#define RTG_CFG1_LEDS1		0x80

/* Strapping config 2, C+ mode */

#define RTG_CFG2		0x0053

#define RTG_CFG2_BUSFREQ	0x07  
#define RTG_CFG2_BUSWIDTH	0x08
#define RTG_CFG2_AUXPWRSTS	0x10
#define RTG_CFG2_ASF_EN		0x40

#define RTG_BUSFREQ_33MHZ	0x00
#define RTG_BUSFREQ_66MHZ	0x01

#define RTG_BUSWIDTH_32BITS	0x00
#define RTG_BUSWIDTH_64BITS	0x08

/* Strapping config 3 */

#define RTG_CFG3		0x0054

#define RTG_CFG3_BEACON_EN	0x01
#define RTG_CFG3_RDY_TO_L23	0x02
#define RTG_CFG3_JUMBO_EN0	0x04
#define RTG_CFG3_ECRCEN		0x08
#define RTG_CFG3_LINKUP		0x10
#define RTG_CFG4_MAGICPKT	0x20

/* Strapping config 4 */

#define RTG_CFG4		0x0055

#define RTG_CFG4_JUMBO_EN1	0x02

/* Strapping config 5 */

#define RTG_CFG5		0x0056

#define RTG_CFG5_PME_STATUS	0x01
#define RTG_CFG5_LANWAJE	0x02
#define RTG_CFG5_UWF		0x04
#define RTG_CFG5_MWF		0x08
#define RTG_CFG5_BWF		0x10

/* Countdown timer interrupt trigger, C+ mode (32 bits), 8139 chip */

#define RTG_TIMERINT_8139	0x0054

/* Media status */

#define RTG_MEDIASTAT		0x0058

#define RTG_MEDIASTAT_RXPAUSE	0x01
#define RTG_MEDIASTAT_TXPAUSE	0x02
#define RTG_MEDIASTAT_LINK	0x04	/* 0 = link ok, 1 = link down */
#define RTG_MEDIASTAT_SPEED10	0x08	/* 1 = 10Mbps, 0 = 100Mbps */
#define RTG_MEDIASTAT_AUXPRW	0x10
#define RTG_MEDIASTAT_RXFLOWCTL	0x40    /* duplex mode */
#define RTG_MEDIASTAT_TXFLOWCTL	0x80    /* duplex mode */

/* Countdown timer interrupt trigger, C+ mode (32 bits), 8169 chip */

#define RTG_TIMERINT_8169	0x0058

/* Multiple interrupt select */

#define RTG_MULTIINT		0x005C

/* PCI revision ID */

#define RTG_PCIREV		0x005E

/* PHY access register, 8169 chip */

#define RTG_PHYAR		0x0060

#define RTG_PHYAR_PHYDATA	0x0000FFFF
#define RTG_PHYAR_PHYREG	0x001F0000
#define RTG_PHYAR_BUSY		0x80000000

/* PHY registers */

#define RTG_BMCR		0x0062	/* basic mode control register */
#define RTG_BMSR		0x0064	/* basic mode status register */
#define RTG_ANAR		0x0066	/* autoneg advertisement */
#define RTG_ANLPAR		0x0068	/* autoneg partner ability */
#define RTG_ANER		0x006A	/* autoneg expansion */

/* TBI registers, 8169 gigE chip only */

#define RTG_TBI_CSR		0x0064
#define RTG_TBI_ANAR		0x0068
#define RTG_TBI_LPAR		0x006A

/* Gigabit media status, 8169 chip */

#define RTG_GMEDIASTAT		0x006C

#define RTG_GMEDIASTAT_FDX	0x01    /* full duplex */
#define RTG_GMEDIASTAT_LINK	0x02    /* link up */
#define RTG_GMEDIASTAT_10MBPS	0x04    /* 10mps link */
#define RTG_GMEDIASTAT_100MBPS	0x08    /* 100mbps link */
#define RTG_GMEDIASTAT_1000MBPS	0x10    /* gigE link */
#define RTG_GMEDIASTAT_RXFLOW	0x20    /* RX flow control on */
#define RTG_GMEDIASTAT_TXFLOW	0x40    /* TX flow control on */
#define RTG_GMEDIASTAT_TBI	0x80    /* TBI enabled */

/* Disconnect counter */

#define RTG_DISCCNT		0x006C

/* False carrier sense counter */

#define RTG_FALSCARRCNT		0x006E

/* NWAY test register */

#define RTG_NWAYTST		0x0070

#define RTG_NWAYTST_LSC		0x01	/* autoneg link status check */
#define RTG_NWAYTST_PDF		0x02	/* autoneg parallel detection fault */
#define RTG_NWAYTST_ABD		0x04	/* autoneg ability detect state */
#define RTG_NWAYTST_LED0	0x08	/* LED0 indicates link */
#define RTG_NWAYTST_LOOP	0x80	/* enable NWAY loopback mode */

/* RTL8168DP only -- ERI data register */

#define RTG_ERIDR		0x0070

/* RX error count register */

#define RTG_RXERRCNT		0x0072

/* CS configuration register */

#define RTG_CSCFG		0x0074

#define RTG_CSCFG_BYPASS_SCR	0x0001	/* bypass scrambler */
#define RTG_CSCFG_LED1_LINKSTS	0x0004	/* set LED1 to show link status */
#define RTG_CSCFG_LINKSTS	0x0008	/* 1 == link, 0 == no link */
#define RTG_CSCFG_FORCE_LINK	0x0020	/* force good link */
#define RTG_CSCFG_FORCE_100	0x0040	/* force 100Mbps link */
#define RTG_CSCFG_JABBER	0x0080	/* enable jabber detect */
#define RTG_CSCFG_HBEAT		0x0100	/* enable heartbeat detect */
#define RTG_CSCFG_LD		0x0200	/* Link disable */
#define RTG_CSCFG_TESTFUN	0x8000	/* auto-neg speeds up internal timer */

/* RTL8168DP only -- ERI address register */

#define RTG_ERIAR		0x0074

/* Low address of TX descriptor with DMA OK, C+ mode */

#define RTG_TXDMALOWOK		0x0082

/* RTL8168DP only -- OC data register */

#define RTG_OCPDR		0x00B0

/* RTL8168DP only -- OC address register */

#define RTG_OCPAR		0x00B4

/* Flash access */

#define RTG_FLASH		0x00D4

#define RTG_FLASH_ADDR		0x0000FFFF	/* address bus */
#define RTG_FLASH_SW_ENABLE	0x00010000	/* enable software read/write */
#define RTG_FLASH_WRITE_ENABLE	0x00020000	/* set WEB pin */
#define RTG_FLASH_OUTPUT_ENABLE	0x00040000	/* set OEB pin */
#define RTG_FLASH_CHIPSEL	0x00080000	/* set ROMCSB pin */
#define RTG_FLASH_DATA		0xFF000000	/* data bus */

/* TX priority polling register, C+ mode */

#define RTG_TXPRIOPOLL_8139	0x00D9

#define RTG_TXPP_SWI		0x01	/* generate interrupt */
#define RTG_TXPP_NPQ		0x40	/* initiate normal queue TX */
#define RTG_TXPP_HPQ		0x80	/* initiate high prio queue TX */

/* RX max frame length, 8169 only */

#define RTG_MAXRXFRAMELEN	0x00DA

/* C+ mode command register */

#define RTG_CPLUSCMD		0x00E0

#define RTG_CPCMD_TX_ENB	0x0001	/* C+ TX enable */
#define RTG_CPCMD_RX_ENB	0x0002	/* C+ RX enable */
#define RTG_CPCMD_PCIMRW_ENB	0x0008	/* PCI multi read/write enable */
#define RTG_CPCMD_PCIDAC_ENB	0x0010	/* PCI dual address cycle enable */
#define RTG_CPCMD_RXCSUM_ENB	0x0020	/* RX checksum offload enable */
#define RTG_CPCMD_RXVLAN_ENB	0x0040	/* RX VLAN stripping enable */
#define RTG_CPCMD_PKTCNT_DIS	0x0080	/* Packet counder disable */
#define RTG_CPCMD_ASF_EN	0x0100 	/* reserved in RTL8168C */
#define RTG_CPCMD_CXPL_DBG_SEL	0x0200	/* reserved in RTL8169B */
#define RTG_CPCMD_TXFLOW_FORCE	0x0400	/* Force TX flow control on */
#define RTG_CPCMD_RXFLOW_FORCE	0x0800	/* Force RX flow control on */
#define RTG_CPCMD_HDX_FORCE	0x1000	/* Force half duplex */
#define RTG_CPCMD_NORMAL_MODE	0x2000	/* Normal mode */
#define RTG_CPCMD_MACDBGO_OE	0x4000
#define RTG_CPCMD_BIST_ENABLE	0x8000

/* RX ring base address, C+ mode */

#define RTG_RXRINGBASE_LO	0x00E4
#define RTG_RXRINGBASE_HI	0x00E8

/*
 * Early TX threshold register, 8139C+ only
 * The threshold field is specified in multiples of 32 bytes.
 */

#define RTG_ETXTHRESH		0x00EC

#define RTG_ETT_TESTMODE	0xC0
#define RTG_ETT_THRESH		0x3F

/*
 * Max TX frame size, all other C+ devices.
 * The frame size is specified in multiples of 128 bytes.
 */

#define RTG_MAXTXFRAMELEN	0x00EC

/*
 * RTL8168DP only
 * The 8168DB has an internal management processor, and the
 * driver must notify the processor when it's been loaded.
 * We do this by writing to the management processor's memory
 * via some indirect registers in the RTL8168DP MAC.
 */

#define RTG_FW_DRV_LOAD		0x00000000
#define RTG_FW_DRV_EXIT		0x00000001
#define RTG_FW_ERIADDR		0x800010E8
#define RTG_FW_OCPADDR		0x80001030

#define RTG_TIMEOUT	10000

#define RTG_RX_FIFOTHRESH	RTG_RXFIFO_NOTHRESH
#define RTG_RX_MAXDMA		RTG_RXDMA_1024BYTES
#define RTG_TX_MAXDMA		RTG_TXDMA_1024BYTES

/*
 * The following DMA data structures are for use with the 8139C+ in
 * C+ mode, or the 8101E, or any of the gigabit ethernet controllers
 * only. 
 *
 * When large send mode is enabled, the lower 11 bits of the TX rtg_cmdsts
 * word are used to hold the MSS, and the checksum offload bits are
 * disabled. The structure layout is the same for RX and TX descriptors.
 */

typedef struct rtg_desc
    {
    volatile UINT32	rtg_cmdsts;
    volatile UINT32	rtg_vlanctl;
    volatile UINT32	rtg_bufaddr_lo;
    volatile UINT32	rtg_bufaddr_hi;
    } RTG_DESC;

/* TX command field bits */

#define RTG_TDESC_CMD_FRAGLEN	0x0000FFFF
#define RTG_TDESC_CMD_TCPCSUM	0x00010000      /* TCP checksum enable */
#define RTG_TDESC_CMD_UDPCSUM	0x00020000      /* UDP checksum enable */
#define RTG_TDESC_CMD_IPCSUM	0x00040000      /* IP header checksum enable */
#define RTG_TDESC_CMD_MSSVAL	0x07FF0000      /* Large send MSS value */
#define RTG_TDESC_CMD_LGSEND	0x08000000      /* TCP large send enable */
#define RTG_TDESC_CMD_EOF	0x10000000      /* end of frame marker */
#define RTG_TDESC_CMD_SOF	0x20000000      /* start of frame marker */
#define RTG_TDESC_CMD_EOR	0x40000000      /* end of ring marker */
#define RTG_TDESC_CMD_OWN	0x80000000      /* chip owns descriptor */

#define RTG_TDESC_VLANCTL_TAG	0x00020000      /* Insert VLAN tag */
#define RTG_TDESC_VLANCTL_DATA	0x0000FFFF      /* TAG data */

/* RTL8138C/CP only */

#define RTG_TDESC_VLANCTL_IPCSUM	0x20000000
#define RTG_TDESC_VLANCTL_TCPCSUM	0x40000000
#define RTG_TDESC_VLANCTL_UDPCSUM	0x80000000

/*
 * Error bits are valid only on the last descriptor of a frame
 * (i.e. RTG_TDESC_CMD_EOF == 1)
 */

#define RTG_TDESC_STAT_COLCNT	0x000F0000      /* collision count */
#define RTG_TDESC_STAT_EXCESSCOL 0x00100000      /* excessive collisions */
#define RTG_TDESC_STAT_LINKFAIL	0x00200000      /* link faulure */
#define RTG_TDESC_STAT_OWINCOL	0x00400000      /* out-of-window collision */
#define RTG_TDESC_STAT_TXERRSUM	0x00800000      /* transmit error summary */
#define RTG_TDESC_STAT_UNDERRUN	0x02000000      /* TX underrun occured */
#define RTG_TDESC_STAT_OWN	0x80000000

#define RTG_TDESC_STAT_ERR	\
    (RTG_TDESC_STAT_EXCESSCOL|RTG_TDESC_STAT_LINKFAIL| \
     RTG_TDESC_STAT_OWINCOL|RTG_TDESC_STAT_TXERRSUM|RTG_TDESC_STAT_UNDERRUN)

/*
 * RX descriptor cmd/vlan definitions
 */

#define RTG_RDESC_CMD_EOR	0x40000000	/* end of ring */
#define RTG_RDESC_CMD_OWN	0x80000000
#define RTG_RDESC_CMD_BUFLEN	0x00001FFF

#define RTG_RDESC_STAT_OWN	0x80000000
#define RTG_RDESC_STAT_EOR	0x40000000
#define RTG_RDESC_STAT_SOF	0x20000000
#define RTG_RDESC_STAT_EOF	0x10000000
#define RTG_RDESC_STAT_FRALIGN	0x08000000      /* frame alignment error */
#define RTG_RDESC_STAT_MCAST	0x04000000      /* multicast pkt received */
#define RTG_RDESC_STAT_UCAST	0x02000000      /* unicast pkt received */
#define RTG_RDESC_STAT_BCAST	0x01000000      /* broadcast pkt received */
#define RTG_RDESC_STAT_BUFOFLOW	0x00800000      /* out of buffer space */
#define RTG_RDESC_STAT_FIFOOFLOW 0x00400000      /* FIFO overrun */
#define RTG_RDESC_STAT_GIANT	0x00200000      /* pkt > 4096 bytes */
#define RTG_RDESC_STAT_RXERRSUM	0x00100000      /* RX error summary */
#define RTG_RDESC_STAT_RUNT	0x00080000      /* runt packet received */
#define RTG_RDESC_STAT_CRCERR	0x00040000      /* CRC error */
#define RTG_RDESC_STAT_PROTOID	0x00030000      /* Protocol type */
#define RTG_RDESC_STAT_IPSUMBAD	0x00008000      /* IP header checksum bad */
#define RTG_RDESC_STAT_UDPSUMBAD 0x00004000      /* UDP checksum bad */
#define RTG_RDESC_STAT_TCPSUMBAD 0x00002000      /* TCP checksum bad */
#define RTG_RDESC_STAT_FRAGLEN	0x00001FFF      /* RX'ed frame/frag len */
#define RTG_RDESC_STAT_GFRAGLEN	0x00003FFF      /* RX'ed frame/frag len */
#define RTG_RDESC_STAT_ERRS	(RTG_RDESC_STAT_GIANT|RTG_RDESC_STAT_RUNT| \
                                 RTG_RDESC_STAT_CRCERR)

#define RTG_RDESC_VLANCTL_TAG    0x00010000      /* VLAN tag available
                                                   (rtg_vlandata valid)*/
#define RTG_RDESC_VLANCTL_DATA   0x0000FFFF      /* TAG data */

/* RTL8168C/CP/D/DP only */

#define RTG_PROTOID_UDP		0x00020000
#define RTG_PROTOID_TCP		0x00010000

#define RTG_RDESC_VLANCTL_IPV4	0x40000000
#define RTG_RDESC_VLANCTL_IPV6	0x80000000

#define RTG_PROTOID_NONIP        0x00000000
#define RTG_PROTOID_TCPIP        0x00010000
#define RTG_PROTOID_UDPIP        0x00020000
#define RTG_PROTOID_IP           0x00030000
#define RTG_TCPPKT(x)            (((x) & RTG_RDESC_STAT_PROTOID) == \
                                 RTG_PROTOID_TCPIP)
#define RTG_UDPPKT(x)            (((x) & RTG_RDESC_STAT_PROTOID) == \
                                 RTG_PROTOID_UDPIP)

/*
 * Statistics counter structure (8139C+ and 8169 only)
 */
typedef struct rtg_stats
    {
    volatile UINT32                rtg_tx_pkts_lo;
    volatile UINT32                rtg_tx_pkts_hi;
    volatile UINT32                rtg_tx_errs_lo;
    volatile UINT32                rtg_tx_errs_hi;
    volatile UINT32                rtg_tx_errs;
    volatile UINT16                rtg_missed_pkts;
    volatile UINT16                rtg_rx_framealign_errs;
    volatile UINT32                rtg_tx_onecoll;
    volatile UINT32                rtg_tx_multicolls;
    volatile UINT32                rtg_rx_ucasts_hi;
    volatile UINT32                rtg_rx_ucasts_lo;
    volatile UINT32                rtg_rx_bcasts_lo;
    volatile UINT32                rtg_rx_bcasts_hi;
    volatile UINT32                rtg_rx_mcasts;
    volatile UINT16                rtg_tx_aborts;
    volatile UINT16                rtg_rx_underruns;
    } RTG_STATS;

/*
 * The RealTek PCIe chips require RX buffers to be aligned on a
 * quadword boundary. This is a bit of a problem for us, because
 * the TCP/IP stack sometimes performs 32 bit loads and stores on
 * packet payloads, which means if the payload isn't longword
 * aligned, the TCP/IP stack accesses will be misaligned. This
 * isn't a big deal on some architectures, but on others we'll get
 * an unaligned access trap, or read/write incorrect data. On those
 * architectures, we have to copy the data to fix it up. We know
 * for sure that misaligned accesses are okay on Pentium, PPC and
 * coldfire. Everywhere else, we apply the fixup.
 */

#if (CPU_FAMILY == I80X86) || (CPU_FAMILY == PPC) || (CPU_FAMILY == COLDFIRE)
#define RTG_ADJ(x)
#else
#define RTG_RX_FIXUP
#define RTG_ADJ(x)	m_adj((x), 8)
#endif

#define RTG_MTU		1500
#define RTG_JUMBO_MTU	7400
#define RTG_CLSIZE	1536
#define RTG_MAX_RX	16
#define RTG_MAXFRAG	16

/*
 * The 8139C+ allows a maximum of 64 descriptors per DMA ring. The
 * 8169 and other gigabit controllers allow up to 1024.
 */

#define RTG_RX_DESC_8139	64
#define RTG_TX_DESC_8139	64

#define RTG_RX_DESC_8169	128
#define RTG_TX_DESC_8169	128

#define RTG_INC_DESC(x, y)	(x) = ((x) + 1) % (y)

#define RTG_ADDR_LO(y)           ((UINT32)((UINT64)(y) & 0xFFFFFFFF))
#define RTG_ADDR_HI(y)           (((UINT32)((UINT64)(y) >> 32) & 0xFFFFFFFF))

#define RTG_NAME	"rtg"

#define RTG_DEVTYPE_8139CPLUS	1
#define RTG_DEVTYPE_8101E	2
#define RTG_DEVTYPE_8169	3
#define RTG_DEVTYPE_8169S	4
#define RTG_DEVTYPE_8169SB	5
#define RTG_DEVTYPE_8110S	6
#define RTG_DEVTYPE_8110SB	7
#define RTG_DEVTYPE_8168	8
#define RTG_DEVTYPE_8111	9

/*
 * Private adapter context structure.
 */

typedef struct rtg_drv_ctrl
    {
    END_OBJ		rtgEndObj;
    VXB_DEVICE_ID	rtgDev;
    void *		rtgBar;
    void *		rtgHandle;
    void *		rtgMuxDevCookie;

    JOB_QUEUE_ID	rtgJobQueue;

    QJOB		rtgIntJob;
    atomic32Val_t		rtgIntPending;

    QJOB		rtgRxJob;
    atomic32Val_t		rtgRxPending;

    QJOB		rtgTxJob;
    atomic32Val_t		rtgTxPending;

    UINT8		rtgTxCur;
    UINT8		rtgTxLast;
    volatile BOOL	rtgTxStall;

    BOOL		rtgPolling;
    M_BLK_ID		rtgPollBuf;
    UINT16		rtgIntMask;
    UINT16		rtgIntrs;

    UINT8		rtgAddr[ETHER_ADDR_LEN];

    END_CAPABILITIES	rtgCaps;

    END_IFDRVCONF	rtgEndStatsConf;
    END_IFCOUNTERS	rtgEndStatsCounters;
    UINT32		rtgInErrors;
    UINT32		rtgInDiscards;
    UINT32		rtgInUcasts;
    UINT32		rtgInMcasts;
    UINT32		rtgInBcasts;
    UINT32		rtgInOctets;
    UINT32		rtgOutErrors;
    UINT32		rtgOutUcasts;
    UINT32		rtgOutMcasts;
    UINT32		rtgOutBcasts;
    UINT32		rtgOutOctets;

    /* Begin MII/ifmedia required fields. */
    END_MEDIALIST	*rtgMediaList;
    END_ERR		rtgLastError;
    UINT32		rtgCurMedia;
    UINT32		rtgCurStatus;
    VXB_DEVICE_ID	rtgMiiBus;
    /* End MII/ifmedia required fields */

    /* RX DMA tags and maps. */
    VXB_DMA_TAG_ID	rtgParentTag;

    int			rtgDevType;
    UINT32		rtgHwRev;
    BOOL		rtgDescV2;

    int			rtgRxDescCnt;
    int			rtgTxDescCnt;

    UINT32		rtgRxLenMask;
    UINT8		rtgTxStartReg;

    VXB_DMA_TAG_ID	rtgRxDescTag;
    VXB_DMA_MAP_ID	rtgRxDescMap;
    RTG_DESC		*rtgRxDescMem;

    VXB_DMA_TAG_ID	rtgTxDescTag;
    VXB_DMA_MAP_ID	rtgTxDescMap;
    RTG_DESC		*rtgTxDescMem;

    UINT32		rtgTxProd;
    UINT32		rtgTxCons;
    UINT32		rtgTxFree;
    UINT32		rtgRxIdx;

    VXB_DMA_TAG_ID	rtgMblkTag;

    VXB_DMA_MAP_ID	*rtgRxMblkMap;
    VXB_DMA_MAP_ID	*rtgTxMblkMap;

    M_BLK_ID            *rtgTxMblk;
    M_BLK_ID            *rtgRxMblk;

    int			rtgEeWidth;

    SEM_ID		rtgDevSem;

    int			rtgMaxMtu;
    } RTG_DRV_CTRL;

#define RTG_BAR(p)   ((RTG_DRV_CTRL *)(p)->pDrvCtrl)->rtgBar
#define RTG_HANDLE(p)   ((RTG_DRV_CTRL *)(p)->pDrvCtrl)->rtgHandle

#define CSR_READ_4(pDev, addr)                                  \
    vxbRead32 (RTG_HANDLE(pDev), (UINT32 *)((char *)RTG_BAR(pDev) + addr))

#define CSR_WRITE_4(pDev, addr, data)                           \
    vxbWrite32 (RTG_HANDLE(pDev),                             \
        (UINT32 *)((char *)RTG_BAR(pDev) + addr), data)

#define CSR_READ_2(pDev, addr)                                  \
    vxbRead16 (RTG_HANDLE(pDev), (UINT16 *)((char *)RTG_BAR(pDev) + addr))

#define CSR_WRITE_2(pDev, addr, data)                           \
    vxbWrite16 (RTG_HANDLE(pDev),                             \
        (UINT16 *)((char *)RTG_BAR(pDev) + addr), data)

#define CSR_READ_1(pDev, addr)                                  \
    vxbRead8 (RTG_HANDLE(pDev), (UINT8 *)((char *)RTG_BAR(pDev) + addr))

#define CSR_WRITE_1(pDev, addr, data)                           \
    vxbWrite8 (RTG_HANDLE(pDev),                              \
        (UINT8 *)((char *)RTG_BAR(pDev) + addr), (UINT8)data)

#define CSR_SETBIT_1(pDev, offset, val)          \
        CSR_WRITE_1(pDev, offset, CSR_READ_1(pDev, offset) | (val))

#define CSR_CLRBIT_1(pDev, offset, val)          \
        CSR_WRITE_1(pDev, offset, (CSR_READ_1(pDev, offset) & ~(val)))

#define CSR_SETBIT_2(pDev, offset, val)          \
        CSR_WRITE_2(pDev, offset, CSR_READ_2(pDev, offset) | (val))

#define CSR_CLRBIT_2(pDev, offset, val)          \
        CSR_WRITE_2(pDev, offset, CSR_READ_2(pDev, offset) & ~(val))

#define CSR_SETBIT_4(pDev, offset, val)          \
        CSR_WRITE_4(pDev, offset, CSR_READ_4(pDev, offset) | (val))

#define CSR_CLRBIT_4(pDev, offset, val)          \
        CSR_WRITE_4(pDev, offset, CSR_READ_4(pDev, offset) & ~(val))

#endif /* BSP_VERSION */

#ifdef __cplusplus
}
#endif

#endif /* __INCrtl8169VxbEndh */
