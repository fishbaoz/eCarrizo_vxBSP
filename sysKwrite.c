/* sysKwrite.c - early print and kwrite debug output routines */

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
01e,24may13,jjk  WIND00364942 - Removing AMP and GUEST from documentation
01d,09may13,jjk  WIND00364942 - Adding Unified BSP.
01c,18oct12,c_t  add panicPrint
01b,06jul12,j_z  code clean
01a,25mar11,lll  unification of itl BSPs
*/

UINT    kwriteSerialInitialized = 0;
UINT    vgaScreenOffset = 0;
UINT    vgaStatusOffset = 24*80; /* 1920 */

#ifdef  INCLUDE_EARLY_PRINT_TRACE
char    EarlyPrint_tracebuff[255];
char    EarlyPrintVga_tracebuff[255];
#endif

/*******************************************************************************
*
* earlyPrint - early serial output service
*
* This routine provides a method to output serial characters to the
* RS232 UART port before printf services are available.
*
* RETURNS: N/a
*
* NOMANUAL
*/

void earlyPrint
    (
    char * outString
    )
    {
    if (outString)
        kwriteSerial(outString, strlen(outString));
    }

/*******************************************************************************
*
* kwriteSerial - early serial output to a console UART port
*
* This routine provides a method to output serial characters from a buffer to
* a console serial port before printf services are available and before
* sysHwInit(). No initialization is required. It sends directly to the UART and
* does not need SIO serial driver support.
*
* This function can be used as a user kwrite routine to support kputs or kprintf
* formatted output, and since no spin locks are involved it can be used with
* no restriction in SMP.
*
* kprintf and kputs prototypes are available by including <stdio.h> while
* the kwriteSerial function prototype is available by including <config.h>.
* The _func_kwrite prototype is obtained from either <config.h> or alternately
* <private/kwriteLibP.h>. kwriteSerial can be dynamically registered or
* reregistered as the current user kwrite routine with:
*
*    _func_kwrite = &kwriteSerial;
*
* See target/src/os/kwrite/{kprintfLib.c,kputsLib.c} for additional information
* on including kprintf/kputs support and registering this function as a
* user kwrite routine (DEBUG_KWRITE_USR_RTN) in a VxWorks Image Project.
*
* The following external macro definitions are required in config.h (or cdf):
*    SYSKWRITE_UART_IS_PCH      (when defined, it indicates a PCH/PCI uart)
*    SYSKWRITE_UART_BASE        (uart base address)
*    SYSKWRITE_UART_REFCLK_HZ   (uart reference input clock frequency in Hz)
*    CONSOLE_BAUD_RATE
*
* RETURNS: STATUS
*
* NOMANUAL
*/

STATUS kwriteSerial
    (
    char * pBuffer,
    UINT32 len
    )
    {
#define _UART_THR   0
#define _UART_LCR   3
#define _UART_LSR   5
#define _UART_DLL   0
#define _UART_DLM   1
#define _UART_FCR   2
#define DIV_ROUND(a, b) (((a) + ((b)/2)) / (b))

#ifdef  SYSKWRITE_UART_IS_PCH
#define uartInByte(reg)          *(pUart + reg)
#define uartOutByte(reg, val)    *(pUart + reg) = val
    UINT8 * pUart = (UINT8 *) SYSKWRITE_UART_BASE;
#else
#define uartInByte(reg)          sysInByte(SYSKWRITE_UART_BASE + reg)
#define uartOutByte(reg, val)    sysOutByte(SYSKWRITE_UART_BASE + reg, val)
#endif
    int     i;
    UINT32  divisor;

    if (!kwriteSerialInitialized)
        {
        /* LDR on, set baud rate, LDR off with 8-bits|no parity|1 stop bit */

        divisor = DIV_ROUND(SYSKWRITE_UART_REFCLK_HZ/16, CONSOLE_BAUD_RATE);
        uartOutByte(_UART_LCR, 0x80);
        uartOutByte(_UART_DLL, (divisor & 0x00FFU));
        uartOutByte(_UART_DLM, ((divisor >> 8) & 0x00FFU));
        uartOutByte(_UART_LCR, 0x03);
        kwriteSerialInitialized = 1;
        }

    if (pBuffer)
        {
        for (i = 0; i < len; i++)
            {
            if (pBuffer[i] == '\n')
                {
                while ((uartInByte(_UART_LSR) & 0x20) != 0x20) {;}
                uartOutByte(_UART_THR, '\r');
                }
            while ((uartInByte(_UART_LSR) & 0x20) != 0x20) {;}
            uartOutByte(_UART_THR, pBuffer[i]);
            }
        return OK;
        }
    else
        return ERROR;
    }

/*******************************************************************************
*
* earlyPrintVga - early VGA output service
*
* This routine provides a method to output serial characters to a
* VGA graphics device before printf services are available.
*
* RETURNS: N/a
*
* NOMANUAL
*/

void earlyPrintVga
    (
    char * outString
    )
    {
    if (outString)
        kwriteVga(outString, strlen(outString));
    }

/*******************************************************************************
*
* kwriteVga - early serial output to a console VGA device
*
* This routine provides a method to output serial characters from a buffer to
* a console VGA device before printf services are available and before
* sysHwInit(). No initialization is required.
* It sends directly to the VGA device memory and does not require PC_CONSOLE
* driver support. kwriteVga provides very fast output without requiring wait
* states and is the least intrusive for use inside interrupt routines.
*
* This function can be used as a user kwrite routine to support kputs or kprintf
* formatted output, and since no spin locks are involved it can be used with
* no restriction in SMP.
*
* When using kwriteVga output it is best to not include PC_CONSOLE or FB_CONSOLE
* support since these consoles do their own independent screen scrolling which
* results in interleaved or overwritten output between kwriteVga and these
* consoles. However, both earlyPrint and earlyPrintVga (and thus kwriteSerial
* and kwriteVga) can be used simultaneously.
*
* kprintf and kputs prototypes are available by including <stdio.h> while
* the kwriteVga function prototype is available by including <config.h>.
* The _func_kwrite prototype is obtained from either <config.h> or alternately
* <private/kwriteLibP.h>. kwriteVga can be dynamically registered or
* reregistered as the current user kwrite routine with:
*
*    _func_kwrite = &kwriteVga;
*
* See target/src/os/kwrite/{kprintfLib.c,kputsLib.c} for additional information
* on including kprintf/kputs support and registering this function as a
* user kwrite routine (DEBUG_KWRITE_USR_RTN) in a VxWorks Image Project.
*
* RETURNS: STATUS
*
* NOMANUAL
*/

void vgaPutc
    (
    UINT offset,
    UINT8 a
    )
    {

    /*
     * A utility function to print a white character on red background
     * at a given offset within the VGA Color Text address range.
     */

    UINT16 *addr = (void*)0xB8000;
    UINT16 c = 0;
    c = a|0x4f00;
    addr += offset%(25*80);  /* valid offset is 0..(25*80)-1, otherwise wrap */
    *addr = c;
    }

/*******************************************************************************
*
* kwriteVga - early vga output to a console
*
* This routine provides a method to output  characters from a buffer to
* a console vga port before printf services are available and before
* sysHwInit(). No initialization is required. It sends directly to the vga and
* does not need driver support.
*
* RETURNS: STATUS
*
* NOMANUAL
*/

STATUS kwriteVga
    (
    char * pBuffer,
    UINT32 len
    )
    {

    /*
     * Assumes a video mode with 80 columns and 25 rows, but uses only 80x24
     * reserving row 25 as a user controllable status area. The 80 character
     * status area can be written to independently with vgaPutc starting at
     * vgaStatusOffset = 24*80 = 1920.
     */

    if (pBuffer)
        {
        while (len--)
            {
            if (*pBuffer == '\n')
                {
                vgaScreenOffset -= vgaScreenOffset%80;
                vgaScreenOffset += 80;
                }
            else
                vgaPutc(vgaScreenOffset++, *pBuffer++);

            if (vgaScreenOffset >= (24*80))
                vgaScreenOffset = 0;
            }

        return OK;
        }
    else
        return ERROR;
}

/*******************************************************************************
*
* panicPrint - print information and pause when system panic
*
* This routine provides a method to output a string on both serial and VGA,
* then pause, which could be useful for early system initialization failure.
*
* RETURNS: N/a
*
* NOMANUAL
*/

void panicPrint
    (
    char * outString
    )
    {
    /* print panic information */

    if (outString)
        {
        earlyPrint(outString);
        earlyPrintVga(outString);
        }

    /* then pause here */

    while (1)
        {
        _WRS_ASM ("pause");
        }
    }
