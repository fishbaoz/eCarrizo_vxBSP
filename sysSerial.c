/* sysSerial.c - PC386/486 BSP serial device initialization */

/*
 * Copyright (c) 1995-1996, 1998, 2001-2002, 2005-2009, 2012-2013
 * Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01x,24may13,jjk  WIND00364942 - Removing AMP and GUEST from documentation
01w,09may13,jjk  WIND00364942 - Adding Unified BSP.
01v,08nov12,lll  fix redefine of sysSerialChanGet when excluding DRV_SIO_NS16550
01u,12mar09,jl   add support for com3 and com4 for wrhv (WIND144311)
01t,29aug08,kab  Change _WRS_VX_SMP to _WRS_CONFIG_SMP
01s,01jun07,scm  remove reference to sysCpuNumGet...
01r,07mar07,dee  Move pc console code into sysLib.c  WIND00088868,
                 WIND00089976
01q,02mar07,pmr  VxBus PC Console conversion
01p,02feb07,scm  add SMP support mods...
01o,11jan06,mdo  Change INCLUDE_NS16550_SIO to DRV_SIO_NS16550
01n,15sep05,mdo  SPR 112344 - Add SIO dependency when VXBUS is defined
01m,08aug05,mdo  Change WINDBUS to VXBUS
01l,02aug05,mdo  Add vxBus variation
01k,10apr02,pai  Implement boundary checking on channel value in
                 sysSerialChanGet() (SPR 74369).
01j,14mar02,hdn  added sysBp checking for HTT (spr 73738)
01i,23oct01,dmh  add documentation for additional serial ports. (spr 5704)
01h,12sep01,hdn  renamed COM[12]_INT_VEC to INT_NUM_COM[12].
01g,21sep98,fle  added library description
01f,19jun96,wlf  doc: cleanup.
01e,23oct95,jdi  doc: cleaned up and removed all NOMANUALs.
01d,03aug95,myz  fixed the warning message
01c,20jun95,ms   fixed comments for mangen
01b,15jun95,ms   updated for new serial driver
01a,15mar95,myz  written based on mv162 version.
*/

/*
DESCRIPTION

This library contains routines for PC386/486 BSP serial device initialization
*/

#include "vxWorks.h"
#include <vsbConfig.h>
#include "iv.h"
#include "intLib.h"
#include "config.h"
#include "sysLib.h"
#include "drv/sio/i8250Sio.h"

/* typedefs */

typedef struct
    {
    USHORT vector;
    ULONG  baseAdrs;
    USHORT regSpace;
    USHORT intLevel;
    } I8250_CHAN_PARAS;


/* includes */

extern BOOL sysBp;

/* defines */

#define UART_REG(reg,chan) \
    (devParas[chan].baseAdrs + reg*devParas[chan].regSpace)


/* locals */

static I8250_CHAN  i8250Chan[N_UART_CHANNELS];

static I8250_CHAN_PARAS devParas[] =
    {
    {INT_NUM_COM1,COM1_BASE_ADR,UART_REG_ADDR_INTERVAL,COM1_INT_LVL},
    {INT_NUM_COM2,COM2_BASE_ADR,UART_REG_ADDR_INTERVAL,COM2_INT_LVL}
    };

/******************************************************************************
*
* sysSerialHwInit - initialize the BSP serial devices to a quiescent state
*
* This routine initializes the BSP serial device descriptors and puts the
* devices in a quiescent state.  It is called from sysHwInit() with
* interrupts locked.
*
* RETURNS: N/A
*
* SEE ALSO: sysHwInit()
*/


void sysSerialHwInit (void)
    {
    int i;

    for (i = 0; i < N_UART_CHANNELS; i++)
        {
        i8250Chan[i].int_vec = devParas[i].vector;
        i8250Chan[i].channelMode = 0;
        i8250Chan[i].lcr =  UART_REG(UART_LCR,i);
        i8250Chan[i].data =  UART_REG(UART_RDR,i);
        i8250Chan[i].brdl = UART_REG(UART_BRDL,i);
        i8250Chan[i].brdh = UART_REG(UART_BRDH,i);
        i8250Chan[i].ier =  UART_REG(UART_IER,i);
        i8250Chan[i].iid =  UART_REG(UART_IID,i);
        i8250Chan[i].mdc =  UART_REG(UART_MDC,i);
        i8250Chan[i].lst =  UART_REG(UART_LST,i);
        i8250Chan[i].msr =  UART_REG(UART_MSR,i);

        i8250Chan[i].outByte = sysOutByte;
        i8250Chan[i].inByte  = sysInByte;

#ifdef _WRS_CONFIG_SMP
        if (vxCpuIndexGet() == 0)
#else
        if (sysBp)
#endif /* _WRS_CONFIG_SMP */
            i8250HrdInit(&i8250Chan[i]);
        }
    }

/******************************************************************************
*
* sysSerialHwInit2 - connect BSP serial device interrupts
*
* This routine connects the BSP serial device interrupts.  It is called from
* sysHwInit2().
*
* Serial device interrupts cannot be connected in sysSerialHwInit() because
* the kernel memory allocator is not initialized at that point, and
* intConnect() calls malloc().
*
* RETURNS: N/A
*
* SEE ALSO: sysHwInit2()
*/

void sysSerialHwInit2 (void)
    {
    int i;

    /* connect serial interrupts */

    for (i = 0; i < N_UART_CHANNELS; i++)
        {
        if (i8250Chan[i].int_vec)
            {
            (void) intConnect (INUM_TO_IVEC (i8250Chan[i].int_vec),
                                i8250Int, (int)&i8250Chan[i] );
#ifdef _WRS_CONFIG_SMP
            if (vxCpuIndexGet() == 0)
#else
            if (sysBp)
#endif /* _WRS_CONFIG_SMP */
                sysIntEnablePIC (devParas[i].intLevel);
            }
        }
    }


/******************************************************************************
*
* sysSerialChanGet - get the SIO_CHAN device associated with a serial channel
*
* This routine gets the SIO_CHAN device associated with a specified serial
* channel.
*
* RETURNS: A pointer to the SIO_CHAN structure for the channel, or ERROR
* if the channel is invalid.
*/

#ifdef DRV_SIO_NS16550
#ifdef INCLUDE_VXBUS
SIO_CHAN * bspSerialChanGet
#else
SIO_CHAN * sysSerialChanGet
#endif /* INCLUDE_VXBUS */
    (
    int channel     /* serial channel */
    )
    {
    if ((channel >= 0) && (channel < N_UART_CHANNELS))
        {
        return ((SIO_CHAN * ) &i8250Chan[channel]);
        }

    return ((SIO_CHAN *) ERROR);
    }
#endif /* DRV_SIO_NS16550 */
