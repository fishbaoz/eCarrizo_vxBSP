/* romInit.s - ROM initialization module */

/*
 * Copyright (c) 2011-2013  Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01d,16may13,jjk  WIND00364942 - Swap RAM_HIGH and RAM_LOW to conform to IA
                 arch in 6.9
01c,09may13,jjk  WIND00364942 - Adding Unified BSP.
01b,26jun12,j_z  dynamic support for multiboot/grub, vxld loader/multi-stage,
                 sync up with itl_nehalem 01f version.
01a,28jan11,lll  unification of itl BSPs (adapted from pcPentium4)
*/

/*
DESCRIPTION
This module contains the entry code for the VxWorks bootrom.

The routine sysToMonitor(2) jumps to an offset location
past the beginning of romInit, to perform a "warm boot".

This code is intended to be generic accross i80x86 boards.
Hardware that requires special register setting or memory
mapping to be done immediately, may do so here.
*/

#define _ASMLANGUAGE
#include <vxWorks.h>
#include <sysLib.h>
#include <asm.h>
#include "config.h"

#ifndef INCLUDE_UEFI_BOOT_SUPPORT /* non-UEFI romInit code */

/**************************************************************
 * Original non-UEFI enabled romInit code starts here.
 **************************************************************/

    .data
    .globl  FUNC(copyright_wind_river)
    .long   FUNC(copyright_wind_river)


    /* internals */

    .globl  romInit                         /* start of system code */
    .globl  _romInit                        /* start of system code */
    .globl  GTEXT(romWait)                  /* wait routine */
    .globl  GTEXT(romA20on)                 /* turn on A20 */
    .globl  romGdt

    .globl  sdata                           /* start of data */
    .globl  _sdata                          /* start of data */

 sdata:
_sdata:
    .asciz  "start of data"
    .text
    .balign 16

/*******************************************************************************
*
* romInit - entry point for VxWorks in ROM
*
*
* romInit (startType)
*   int startType;  /@ only used by 2nd entry point @/
*
*/

    /* cold start entry point for REAL (16-bit) or BOOT32 (32-bit protected) */

romInit:
_romInit:
    cli                             /* LOCK INTERRUPT */

    /*
     * check if loaded by multiboot (in protected mode) or vxld (in real mode), 
     * then jump to the corresponding start for that mode
     */

    xorl    %ecx, %ecx
    movl    %cr0, %ecx
    and     $0x01, %cx
    jnz     rom32ColdStart
    .byte 0x67,0x66
    jmp     cold

    /* warm start entry point in PROTECTED MODE(32 bits) */

    .balign 16,0x90
romWarmHigh:                        /* ROM_WARM_HIGH(0x20) is offset */
    cli                             /* LOCK INTERRUPT */
    movl    SP_ARG1(%esp),%ebx      /* %ebx has the startType */
    jmp     warm

    /* warm start entry point in PROTECTED MODE(32 bits) */

    .balign 16,0x90
romWarmLow:                         /* ROM_WARM_LOW(0x30) is offset */
    cli                             /* LOCK INTERRUPT */
    jmp     warmLow

    /* cold start entry point in PROTECTED MODE(32 bits) */

    .balign 16,0x90

rom32ColdStart:                     /* ROM_COLD_START(0x40) is offset */

    movl    %eax,(MULTIBOOT_SCRATCH)     /* save magic */
    movl    %ebx,(MULTIBOOT_SCRATCH + 8) /* save multi-boot pointer */
    movl    $MULTIBOOT_BOOTLOADER_MAGIC, (MULTIBOOT_BOOT_FLAG_ADRS)

    /* temporary use of stack to get eip */

    movl    $ROM_STACK, %esp
    call    getEip
getEip:
    popl    %eax
    andl    $0xffffe000, %eax
    movl    %eax, (MULTIBOOT_BOOT_LOAD_ADRS)
    
    /* magic for saved bootrom */
    
    movl    $0xbadbeef, (MULTIBOOT_BOOTROM_SAVED_FLAG_ADRS)
    movl    $ramGrubGdtr, %eax      /* load temporary GDT */
    subl    $FUNC(romInit), %eax
    addl    (MULTIBOOT_BOOT_LOAD_ADRS), %eax
    pushl   %eax
    call    FUNC(romLoadGdt)
    ljmp    $0x08, $romInit5

warmLow:
    cld                             /* copy itself to ROM_TEXT_ADRS */
    movl    $ SYS_RAM_LOW_ADRS,%esi /* get src addr */
    movl    $ ROM_TEXT_ADRS,%edi    /* get dst addr */
    movl    $ ROM_SIZE,%ecx         /* get nBytes to copy */
    shrl    $2,%ecx                 /* get nLongs to copy */
    rep                             /* repeat next inst ECX times */
    movsl                           /* copy nLongs from src to dst */
    movl    SP_ARG1(%esp),%ebx      /* %ebx has the startType */
    jmp     warm                    /* jump to warm */

    /* The folllowing section is for multi-stage boot */

    /*
     * Second Stage Boot entry point PROTECTED MODE(32 bits) from
     * vxStage1Boot.s
     */

    .balign 16,0x90
romForceAlign:
    nop
    nop
    nop
    nop

    .balign 16,0x90                 /* Should be + MULTI_STAGE_RAM_START */

romSecondStageBoot:

    movl    SP_ARG1(%esp),%ebx      /* %ebx has the startType */

    call    FUNC(vxBiosE820MapCopy) /* Finish E820 init */
    jmp     romInit4

    /* copyright notice appears at beginning of ROM (in TEXT segment) */

    .ascii  "Copyright 1984-2012 Wind River Systems, Inc."

    /* Header support for multiboot loaders (GRUB) */

    .balign 4
multiboot_header:
    .long   MULTIBOOT_HEADER_MAGIC
    .long   MULTIBOOT_HEADER_FLAGS
    .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    .long   multiboot_header
    .long   wrs_kernel_text_start
    .long   wrs_kernel_data_end
    .long   wrs_kernel_bss_end
    .long   romInit

    /* cold start code in REAL MODE(16 bits) */

    .balign 16,0x90
cold:

    /*
     * Clear a couple of registers. This seems to be
     * necessary when booting via PXE.
     */

    .byte   0x67
    xorw    %ax, %ax
    .byte   0x67
    movw    %ax, %ds
    .byte   0x67
    movw    %ax, %es

    .byte   0x67, 0x66
    call    FUNC(vxBiosE820MapQuery)  /* query the BIOS memory map */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    lidt    ROM_ADRS(romIdtr)       /* load temporary IDT */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    lgdt    ROM_ADRS(romGdtr)       /* load temporary GDT */

    /* switch to protected mode */

    mov     %cr0,%eax               /* move CR0 to EAX */
    .byte   0x66                    /* next inst has 32bit operand */
    or      $0x00000001,%eax        /* set the PE bit */
    mov     %eax,%cr0               /* move EAX to CR0 */
    jmp     romInit1                /* near jump to flush inst queue */

romInit1:
    .byte   0x66                    /* next inst has 32bit operand */
    mov     $0x0010,%eax      /* set data segment selector 0x10 (the 3rd one) */
    mov     %ax,%ds                 /* set DS */
    mov     %ax,%es                 /* set ES */
    mov     %ax,%fs                 /* set FS */
    mov     %ax,%gs                 /* set GS */
    mov     %ax,%ss                 /* set SS */

    .byte   0x66                    /* next inst has 32bit operand */
    mov     $ROM_STACK,%esp         /* set lower mem stack pointer */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    ljmp    $0x08, $ROM_TEXT_ADRS + romInit2 - romInit

    /* temporary IDTR stored in code segment in ROM */

romIdtr:
    .word   0x0000                  /* size   : 0 */
    .long   0x00000000              /* address: 0 */


    /* temporary GDTR stored in code segment in ROM */

romGdtr:
    .word   0x0027                              /* size   : 39 (8*5-1) bytes */
    .long   (romGdt - romInit + ROM_TEXT_ADRS)  /* address: romGdt */

    /* temporary GDTR stored in code segment in RAM */

ramGrubGdtr:
    .word   0x0027                              /* size   : 39 (8*5-1) bytes */
    .long   romGdt                              /* address: romGdt */

    /* temporary GDT stored in code segment in ROM */

    .balign 16,0x90
romGdt:
    /* 0(selector=0x0000): Null descriptor */
    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 1(selector=0x0008): Code descriptor */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 2(selector=0x0010): Data descriptor */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x92                    /* Data r/w, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 3(selector=0x0018): Code descriptor, for the nesting interrupt */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 4(selector=0x0020): Code descriptor, for the nesting interrupt */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */


    /* cold start code in PROTECTED MODE(32 bits) */

    .balign 16,0x90
romInit2:
    cli                             /* LOCK INTERRUPT */
    movl    $ ROM_STACK,%esp        /* set a stack pointer */

#if defined (TGT_CPU) && defined (INCLUDE_SYMMETRIC_IO_MODE)
    movl    $ MP_N_CPU, %eax
    lock
    incl    (%eax)
#endif  /* defined (TGT_CPU) && defined (INCLUDE_SYMMETRIC_IO_MODE) */

    /* NOTE: some new chipsets don't support A20 - romA20on now checks this */

    call    FUNC(romA20on)          /* check and enable A20 if needed */
    cmpl    $0, %eax                /* is A20 enabled? */
    jne     romInitHlt              /*   no: jump romInitHlt */

    call    FUNC(vxBiosE820MapCopy)

    movl    $ BOOT_COLD,%ebx        /* %ebx has the startType */

    /* copy bootrom image to dst addr if (romInit != ROM_TEXT_ADRS) */

warm:

    /* for grub load vxWorks bootrom wamboot */

    cmpl    $MULTIBOOT_BOOTLOADER_MAGIC, (MULTIBOOT_BOOT_FLAG_ADRS)
    je      romInit5

    ARCH_REGS_INIT                  /* initialize DR[0-7] CR0 EFLAGS */
    /* ARCH_CR4_INIT                /@ initialize CR4 for P5,6,7 */

    xorl    %eax, %eax              /* zero EAX */
    movl    %eax, %cr4              /* initialize CR4 */

    movl    $romGdtr,%eax           /* load the original GDT */
    subl    $ FUNC(romInit),%eax
    addl    $ ROM_TEXT_ADRS,%eax
    pushl   %eax
    call    FUNC(romLoadGdt)

    movl    $ STACK_ADRS, %esp      /* initialise the stack pointer */
    movl    $ ROM_TEXT_ADRS, %esi   /* get src addr(ROM_TEXT_ADRS) */
    movl    $ romInit, %edi         /* get dst addr(romInit) */
    cmpl    %esi, %edi              /* is src and dst same? */
    je      romInit4                /*   yes: skip copying */
    movl    $ FUNC(end), %ecx       /* get "end" addr */
    subl    %edi, %ecx              /* get nBytes to copy */
    shrl    $2, %ecx                /* get nLongs to copy */
    cld                             /* clear the direction bit */
    rep                             /* repeat next inst ECX times */
    movsl                           /* copy itself to the entry point */

    /* jump to romStart(absolute address, position dependent) */

romInit4:
    xorl    %ebp, %ebp              /* initialize the frame pointer */
    pushl   $0                      /* initialise the EFLAGS */
    popfl
    pushl   %ebx                    /* push the startType */
    movl    $ FUNC(romStart),%eax   /* jump to romStart */
    call    *%eax

    /* for multiboot/grub boot */

romInit5:
    cli                             /* LOCK INTERRUPT */
    movl    $SYS_RAM_HIGH_ADRS,%esp /* set a stack pointer */

#if defined (TGT_CPU) && defined (INCLUDE_SYMMETRIC_IO_MODE)
    movl    $ MP_N_CPU, %eax
    lock
    incl    (%eax)
#endif  /* defined (TGT_CPU) && defined (INCLUDE_SYMMETRIC_IO_MODE) */

    ARCH_REGS_INIT                  /* initialize DR[0-7] CR0 EFLAGS */
    xorl    %eax, %eax              /* zero EAX */
    movl    %eax, %cr4              /* initialize CR4 */

    movl    $ BOOT_COLD,%ebx        /* %ebx has the startType */

    xorl    %ebp, %ebp              /* initialize the frame pointer */
    pushl   $0                      /* initialise the EFLAGS */
    popfl
    pushl   %ebx                    /* push the startType */
    movl    $ FUNC(romStart),%eax   /* jump to romStart */
    call    *%eax

    /* just in case, if there's a problem in romStart or romA20on */

romInitHlt:
    pushl   %eax
    call    FUNC(romEaxShow)        /* show EAX on your display device */
    hlt
    jmp     romInitHlt

/*
 * MUST include the assembly file at this specific location in
 * romInit.s to ensure correct linkage to 16 bit address space
 */

#include <fw/bios/vxBiosE820ALib.s>

/*******************************************************************************
*
* romA20on - enable A20
*
* enable A20 (intended for newer chipsets, so it uses PORT92 fast A20)
*
* RETURNS: N/A
*
* void romA20on (void)
*
*/

    .balign 16,0x90
FUNC_LABEL(romA20on)

    /* If memory above 1MB is already working, ignore A20 gate set */

    movl    $0x000000,%eax
    movl    $0x100000,%edx
    pushl   (%eax)
    pushl   (%edx)
    movl    $0x0,(%eax)
    movl    $0x0,(%edx)
    movl    $0x01234567,(%eax)
    cmpl    $0x01234567,(%edx)
    popl    (%edx)
    popl    (%eax)
    jne     romA20on0

    /* Fast A20 option -- use System Control Port (PORT92) to enable A20 */

    movl    $0x02,%eax
    outb    %al,$0x92

    movl    $100000, %ecx           /* Loop up to 100000 times if necessary */
romA20on1:
    inb     $0x92,%al
    andb    $0x02,%al               /* Check that port92 is now set */
    loopz   romA20on1               /* Takes an early exit if input matches */

    movl    $0x000000,%eax          /* Check if upper memory works */
    movl    $0x100000,%edx
    pushl   (%eax)
    pushl   (%edx)
    movl    $0x0,(%eax)
    movl    $0x0,(%edx)
    movl    $0x01234567,(%eax)
    cmpl    $0x01234567,(%edx)
    popl    (%edx)
    popl    (%eax)
    jne     romA20on0

    movl    $0xdeaddead,%eax        /* error, can't enable A20 */
    ret

romA20on0:
    xorl    %eax,%eax
    ret

/*******************************************************************************
*
* romLoadGdt - load the global descriptor table.
*
* RETURNS: N/A
*
* NOMANUAL
*
* void romLoadGdt (char *romGdtr)
*
*/

    .balign 16,0x90
FUNC_LABEL(romLoadGdt)
    movl    SP_ARG1(%esp),%eax
    lgdt    (%eax)
    movw    $0x0010,%ax       /* set data segment selector 0x10 (the 3rd one) */
    movw    %ax,%ds
    movw    %ax,%es
    movw    %ax,%fs
    movw    %ax,%gs
    movw    %ax,%ss
    ret

/*******************************************************************************
*
* romWait - wait until the input buffer becomes empty
*
* wait until the input buffer becomes empty
*
* RETURNS: N/A
*
* void romWait (void)
*
*/

    .balign 16,0x90
FUNC_LABEL(romWait)
    movl    $100000, %ecx           /* Loop 100000 times */
romWait0:
    movl    $0x64,%edx              /* Check if it is ready to write */
    inb     %dx,%al
    andb    $2,%al
    loopnz  romWait0
    ret

/*******************************************************************************
*
* romEaxShow - show EAX register
*
* show EAX register in your display device
*
* RETURNS: N/A
*
* void romEaxShow (void)
*
*/

    .balign 16,0x90
FUNC_LABEL(romEaxShow)

    /* show EAX register in your display device */

    ret

#else /* INCLUDE_UEFI_BOOT_SUPPORT */

/***************************************
 * Start of UEFI-enabled romInit code
 ***************************************/

    .data
    .globl  FUNC(copyright_wind_river)
    .long   FUNC(copyright_wind_river)

    /* internals */

    .globl  romInit                 /* start of system code */
    .globl  _romInit                /* start of system code */

    .globl  sdata                   /* start of data */
    .globl  _sdata                  /* start of data */
    .globl  pRomUefiMemAddr         /* UEFI ptr value for memory block header */
    .globl  pRomUefiAcpiAddr        /* UEFI ptr value for ACPI info */
    .globl  romUefiComplianceInfoP  /* UEFI read-only compliance string */

 sdata:
_sdata:
    .asciz  "start of data"

    .balign 8, 0x00
pRomUefiMemAddr:        .long   0x00000000
pRomUefiAcpiAddr:       .long   0x00000000

    .balign 8, 0x00

    /*
     * signature marking the start of
     * external bootline params
     */

bootSignature:          .ascii  BOOTLINE_EXTERN_MARKER
signatureLen:           .long   .-bootSignature
    .balign 8, 0x00
bootlineEnd:            .long   BOOTLINE_EXTERN_ADDR + BOOTLINE_EXTERN_MAXLEN


    .text
    .balign 16

/*******************************************************************************
*
* romInit - entry point for VxWorks in ROM
*
* For cold boots:
* romInit (uefiBaseAddress, uefiMemoryblockAddr)
*     void *uefiBaseAddress;     /@ only used by 1st (cold boot) entry point @/
*     void *uefiMemoryBlockAddr; /@ only used by 1st (cold boot) entry point @/
*
* For warm boots:
* romInit (startType)
*     int startType;             /@ only used by 2nd entry point @/
*/

/*
 * cold start entry point in PROTECTED MODE (32 bits)
 * EFI will ensure that the GDT and segmentation select registers are
 * set to wide open 32 bit flat access.
 *
 * This code will start executing on a cold boot at the address given
 * by the first parameter, uefiBaseAddress.
 * It will copy itself to the ROM_TEXT_ADRS location ASAP and then jump to
 * copied location.  Note that even then the code is probably built to run
 * from yet another address, but since the bootrom image is probably
 * compressed the code will have to uncompress itself to the final destination
 * address and then jump to that.
 */

romInit:
_romInit:
    cli                             /* LOCK INTERRUPT */
    jmp     rom32ColdStart

    /* warm start entry point in PROTECTED MODE(32 bits)
     * These entry points will be offsets from the base address that
     * the bootrom is built for.
     */

    .balign 16,0x90
rom32WarmHigh:                      /* ROM_WARM_HIGH(0x10) is offset */
    cli                             /* LOCK INTERRUPT */
    movl    SP_ARG1(%esp),%ebx      /* %ebx has the startType */
    jmp     warmHigh

    /* warm start entry point in PROTECTED MODE(32 bits) */

    .balign 16,0x90
rom32WarmLow:                       /* ROM_WARM_LOW(0x20) is offset */
    cli                             /* LOCK INTERRUPT */
    cld                             /* copy itself to ROM_TEXT_ADRS */
    movl    $ SYS_RAM_LOW_ADRS,%esi /* get src addr */
    movl    $ ROM_TEXT_ADRS,%edi    /* get dst addr */
    movl    $ ROM_SIZE,%ecx         /* get nBytes to copy */
    shrl    $2,%ecx                 /* get nLongs to copy */
    rep                             /* repeat next inst ECX time */
    movsl                           /* copy nLongs from src to dst */
    movl    SP_ARG1(%esp),%ebx      /* %ebx has the startType */
    jmp     warm                    /* jump to warm */

    /* copyright notice appears at beginning of ROM (in TEXT segment) */

    .ascii  "Copyright 1984-2011 Wind River Systems, Inc."
    .balign 4,0x00
romUefiComplianceInfoP:
    
    /* format is XXX.YY, where YY=1,2,...,9,10,11, etc */

    .asciz  "UEFI 001.10 Compliant"

    /* Header support for multiboot loaders (GRUB) */

    /* NOTE:
     * This section for Multiboot used in conjunction with UEFI needs to be
     * upgraded to use the new MULTIBOOT2 spec rather than just MULTIBOOT
     * since only MULTIBOOT2 includes the proper extended support for UEFI.
     * Therefore, the Multiboot code included here now should be considered
     * only as a place-holder for the upcoming MULTIBOOT2. If a multiboot
     * environment is not being used, then this multiboot code is ignored.
     */

    .balign 4
multiboot_header:
    .long   MULTIBOOT_HEADER_MAGIC
    .long   MULTIBOOT_HEADER_FLAGS
    .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    .long   multiboot_header
    .long   wrs_kernel_text_start
    .long   wrs_kernel_data_end
    .long   wrs_kernel_bss_end
    .long   romInit

    /* UEFI cold start code in PROTECTED MODE (32 bits)
     * Executing at an unknown address, must use the first parameter to get it.
     */

    .balign 16,0x90
rom32ColdStart:
    /*
     * Check for an external bootline signature marker residing in reserved
     * memory at BOOTLINE_EXTERN_ADDR (see also sysLib.c and pc.h)
     * A multiboot bootline is saved when BOOTLINE_EXTERN_ADDR is empty.
     */

    cmpl    $MULTIBOOT_BOOTLOADER_MAGIC, %eax
    jne     doneWithBootline

    mov     %eax,(MULTIBOOT_SCRATCH)     /* save magic */
    mov     %ebx,(MULTIBOOT_SCRATCH + 8) /* save multi-boot pointer */

checkBootline:
    movl    $BOOTLINE_EXTERN_ADDR - 1, %edx /* initialize char pointer */
    movl    signatureLen, %ecx              /* set length of signature string */
    cld
findMarker:
    incl    %edx                            /* advance char pointer */
    cmpl    bootlineEnd, %edx    /* signature must reside within bootline buf */
    je      markerNotFound
    movl    %edx, %edi                      /* point EDI to next char */
    movl    $bootSignature, %esi            /* point ESI to our signature */
    rep     cmpsb                           /* check: do strings agree? */
    je      doneWithBootline                /* yes, search successful! */
    jmp     findMarker                      /* not found yet, continue */
markerNotFound:
    cld                                     /* begin multiboot bootline copy: */
    movl    (MULTIBOOT_SCRATCH), %edx
    movl    16(%edx), %esi                  /*   set source addr */
    movl    $BOOTLINE_EXTERN_ADDR, %edi     /*   set dest addr */
    movl    $BOOTLINE_EXTERN_MAXLEN, %ecx   /*   num bytes to copy */
    shrl    $2, %ecx                        /*   num longs to copy */
    rep     movsl
doneWithBootline:

    /* First argument is the location we're running from.
     * Second argument is the UEFI memory block.
     */

    /* get the base address */
    movl SP_ARG1(%esp), %edx            /* base address is arg 1 */

    /*
     * get the memory block address ptr relative to our current execution
     * address, and store the memory block address.  This in turn will be
     * copied to ROM_TEXT_ADRS, and then probably somewhere else too, before
     * bootInit picks it up and uses it.
     */

    movl SP_ARG2(%esp), %ebx            /* memory block address is arg 2 */
    movl $FUNC(pRomUefiMemAddr), %eax   /* get the base address */
    subl $FUNC(romInit), %eax           /* now has offset to base */
    addl %edx, %eax                     /* add the base address we got*/
    movl %ebx, (%eax)                   /* store the value into the location*/


    /*
     * get the ACPI address ptr relative to our current execution
     * address, and store the memory block address.  This in turn will be
     * copied to ROM_TEXT_ADRS, and then probably somewhere else too, before
     * bootInit picks it up and uses it.
     */

    movl SP_ARG3(%esp), %ebx            /* ACPI address is arg 3 */
    movl $FUNC(pRomUefiAcpiAddr), %eax  /* get the base address */
    subl $FUNC(romInit), %eax           /* now has offset to base */
    addl %edx, %eax                     /* add the base address we got*/
    movl %ebx, (%eax)                   /* store the value into the location*/


    /*
     * copy the bootrom image loaded by the
     * UEFI OS loader to ROM_TEXT_ADRS
     */

    cld
    movl    %edx, %esi                  /* start at the base address we got */

    movl    $ROM_TEXT_ADRS,%edi         /* get dst addr (ROM_TEXT_ADRS) fixed */
    movl    $FUNC(end), %ecx            /* end of the image from linker */
    subl    $FUNC(romInit),%ecx         /* get nBytes to copy */
    shrl    $2,%ecx                     /* get nLongs to copy */
    rep                                 /* repeat next inst ECX time */
    movsl                               /* copy nLongs from src to dst */

    /* jump to romInitFixedLocation (absolute address, position dependent) */

    .balign 16,0x90
callroutine:
    movl $FUNC(romInitFixedLocation), %eax
    subl $FUNC(romInit), %eax
    addl $ROM_TEXT_ADRS, %eax

    /* push the memory pointer for use by romInit */

    call *%eax

    /* we should not return */

    ret

/*
 * This part of romInit is fixed in UEFI systems at
 * ROM_TEXT_ADDR + (romInitFixedLocation-romInit)
 */
    .balign 16,0x90
romInitFixedLocation:
    movl    $FUNC(romIdtr), %eax
    subl    $FUNC(romInit), %eax
    addl    $ROM_TEXT_ADRS, %eax
    lidt    (%eax)                  /* load temporary IDT */

    movl    $FUNC(romGdtr), %eax
    subl    $FUNC(romInit), %eax
    addl    $ROM_TEXT_ADRS, %eax
    lgdt    (%eax)                  /* load temporary GDT */

    jmp     romInit1                /* near jump to flush inst queue */

romInit1:
    movl    $0x0010,%eax            /* set data segment selector 0x10 (the 3rd one) */
    movw    %ax,%ds                 /* set DS */
    movw    %ax,%es                 /* set ES */
    movw    %ax,%fs                 /* set FS */
    movw    %ax,%gs                 /* set GS */
    movw    %ax,%ss                 /* set SS */

   /* EFI code ROM_STACK is at the end of
    * the bootrom ROM buffer; its use isn't great.
    */

    movl    $ROM_STACK, %esp  /* set stack to end of bootrom ROM buffer */

    /* jump to romInit2, setting CS segment register */

    ljmp    $0x08, $ROM_TEXT_ADRS+romInit2-romInit

    /* temporary IDTR stored in code segment in ROM */

romIdtr:
    .word   0x0000                  /* size   : 0 */
    .long   0x00000000              /* address: 0 */


    /* temporary GDTR stored in code segment in ROM */

romGdtr:
    .word   0x0027                              /* size   : 39 (8*5-1) bytes */
    .long   (romGdt - romInit + ROM_TEXT_ADRS)  /* address: romGdt */

    /* temporary GDTR stored in code segment in ROM */

ramGrubGdtr:
    .word   0x0027                              /* size   : 39 (8*5-1) bytes */
    .long   romGdt                              /* address: romGdt */

    /* temporary GDT stored in code segment in ROM */

    .balign 16,0x90
romGdt:
    /* 0(selector=0x0000): Null descriptor */
    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 1(selector=0x0008): Code descriptor */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 2(selector=0x0010): Data descriptor */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x92                    /* Data r/w, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 3(selector=0x0018): Code descriptor, for the nesting interrupt */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 4(selector=0x0020): Code descriptor, for the nesting interrupt */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */


    /* cold start code in PROTECTED MODE(32 bits) */

    .balign 16,0x90
romInit2:
    cli                             /* LOCK INTERRUPT */

    /* UEFI code ROM_STACK is at end of bootrom ROM buffer */

    movl    $ ROM_STACK,%esp        /* set a stack pointer */

#if defined (TGT_CPU) && defined (INCLUDE_SYMMETRIC_IO_MODE)
    movl    $ MP_N_CPU, %eax
    lock
    incl    (%eax)
#endif  /* defined (TGT_CPU) && defined (INCLUDE_SYMMETRIC_IO_MODE) */

    /*
     * Don't call romA20on here anymore.  Done by EFI.
     */

    movl    $ BOOT_COLD,%ebx        /* %ebx has the startType */

    jmp     warm                    /* jump over warmHigh debug output */

    /* handle warmHigh reset - send out debug statement */

warmHigh:
warm:

    /* copy bootrom image to dst addr if (romInit != ROM_TEXT_ADRS) */

    ARCH_REGS_INIT                  /* initialize DR[0-7] CR0 EFLAGS */

    /* ARCH_CR4_INIT                initialize CR4 for P5,6,7 */

    xorl    %eax, %eax              /* zero EAX */
    movl    %eax, %cr4              /* initialize CR4 */

    movl    $romGdtr,%eax           /* load the original GDT */
    subl    $ FUNC(romInit),%eax
    addl    $ ROM_TEXT_ADRS,%eax
    pushl   %eax
    call    FUNC(romLoadGdt)

    movl    $ STACK_ADRS, %esp      /* initialise the stack pointer */
    movl    $ ROM_TEXT_ADRS, %esi   /* get src addr(ROM_TEXT_ADRS) */
    movl    $ romInit, %edi         /* get dst addr(romInit) */
    cmpl    %esi, %edi              /* is src and dst same? */
    je      romInit4                /*   yes: skip copying */
    movl    $ FUNC(end), %ecx       /* get "end" addr */
    subl    %edi, %ecx              /* get nBytes to copy */
    shrl    $2, %ecx                /* get nLongs to copy */
    cld                             /* clear the direction bit */
    rep                             /* repeat next inst ECX time */
    movsl                           /* copy itself to the entry point */

    /* jump to romStart(absolute address, position dependent) */

romInit4:
    xorl    %ebp, %ebp              /* initialize the frame pointer */
    pushl   $0                      /* initialise the EFLAGS */
    popfl
    pushl   %ebx                    /* push the startType */

    /* no need to push the memory pointer here too */

    movl    $ FUNC(romStart),%eax   /* jump to romStart */
    call    *%eax

    /* just in case, if there's a problem in romStart or romA20on */

romInitHlt:
    pushl   %eax
    call    FUNC(romEaxShow)        /* show EAX on your display device */
    hlt
    jmp     romInitHlt


/*******************************************************************************
*
* romLoadGdt - load the global descriptor table.
*
* RETURNS: N/A
*
* NOMANUAL
*
* void romLoadGdt (char *romGdtr)
*
*/

    .balign 16,0x90
FUNC_LABEL(romLoadGdt)
    movl    SP_ARG1(%esp),%eax
    lgdt    (%eax)
    movw    $0x0010,%ax             /* selector 0x10 is 3rd one */
    movw    %ax,%ds
    movw    %ax,%es
    movw    %ax,%fs
    movw    %ax,%gs
    movw    %ax,%ss
    ret


/*******************************************************************************
*
* romEaxShow - show EAX register
*
* show EAX register in your display device
*
* RETURNS: N/A
*
* void romEaxShow (void)
*
*/

    .balign 16,0x90
FUNC_LABEL(romEaxShow)

    /* show EAX register in your display device available */

    ret

/**************************************************************
 * End of the UEFI enabled romInit code.
 **************************************************************/

#endif /* #ifndef INCLUDE_UEFI_BOOT_SUPPORT */
