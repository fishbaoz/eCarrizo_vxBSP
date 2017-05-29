/* sysALib.s - System-dependent routines for Intel IA family BSPs */

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
01h,24may13,jjk  WIND00364942 - Removing AMP and GUEST
01g,16may13,jjk  WIND00364942 - Swap RAM_HIGH and RAM_LOW to conform to IA
                 arch in 6.9
01f,09may13,jjk  WIND00364942 - Adding Unified BSP.
01e,25jul12,c_t  fix startType error and compatibility across VxWorks 6.8/6.9
01d,05jul12,c_t  tune TSC calibration.
01c,26jun12,j_z  merge from itl_nehalem 02q version, code clean.
01b,25mar11,lll  unification of itl BSPs
01a,28jan11,lll  add general support for external bootlines including multiboot
*/

/*
DESCRIPTION
This module contains system-dependent routines written in assembly
language.

This module must be the first specified in the \f3ld\f1 command used to
build the system.  The sysInit() routine is the system start-up code.

INTERNAL
Many routines in this module doesn't use the "c" frame pointer %ebp@ !
This is only for the benefit of the stacktrace facility to allow it
to properly trace tasks executing within these routines.

SEE ALSO:
.I "i80386 32-Bit Microprocessor User's Manual"
*/



#define _ASMLANGUAGE
#include <vxWorks.h>
#include <vsbConfig.h>
#include <asm.h>
#include <regs.h>
#include <sysLib.h>
#include <config.h>

#ifdef _WRS_CONFIG_SMP
#include <private/vxSmpP.h>
#endif /* _WRS_CONFIG_SMP */

    .data
    .globl  FUNC(copyright_wind_river)
    .long   FUNC(copyright_wind_river)

    /* externals */

    .globl  FUNC(usrInit)           /* system initialization routine */
    .globl  VAR(sysProcessor)       /* initialized to NONE(-1) in sysLib.c */

#ifdef _WRS_CONFIG_SMP
        _IA32_ASM_EXTERN_KERNEL_GLOBALS
#endif /* _WRS_CONFIG_SMP */

#if defined(_WRS_CONFIG_SMP)
    .globl  FUNC(sysInitCpuStartup) /* the cpu startup */
    .globl  FUNC(cpuInit1)          /* the cpu startup initialization */
    .globl  FUNC(sysCpuInit)        /* the cpu initialization */
#endif /* defined(_WRS_CONFIG_SMP) */

    /* internals */

    .globl  sysInit                         /* start of system code */
    .globl  _sysInit                        /* start of system code */
    .globl  GTEXT(sysInitGDT)
    .globl  GTEXT(sysInByte)
    .globl  GTEXT(sysInWord)
    .globl  GTEXT(sysInLong)
    .globl  GTEXT(sysInWordString)
    .globl  GTEXT(sysInLongString)
    .globl  GTEXT(sysOutByte)
    .globl  GTEXT(sysOutWord)
    .globl  GTEXT(sysOutLong)
    .globl  GTEXT(sysOutWordString)
    .globl  GTEXT(sysOutLongString)
    .globl  GTEXT(sysReboot)
    .globl  GTEXT(sysWait)
#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9))
    .globl  GTEXT(sysCpuProbe)
#endif /* ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9)) */
    .globl  GTEXT(sysLoadGdt)
    .globl  GTEXT(sysGdtr)
    .globl  GTEXT(sysGdt)

    .globl  GDATA(sysCsSuper)               /* code selector: supervisor mode */
    .globl  GDATA(sysCsExc)                 /* code selector: exception */
    .globl  GDATA(sysCsInt)                 /* code selector: interrupt */
    .global GTEXT(sysCalTSC)

    /* locals */

FUNC_LABEL(vendorIdIntel)                   /* CPUID vendor ID - Intel */
    .ascii  "GenuineIntel"

#ifdef INCLUDE_UEFI_BOOT_SUPPORT
    .globl  pSysUefiMemAddr     /* UEFI pointer value for memory block header */
    .globl  pSysUefiAcpiAddr    /* UEFI pointer value for ACPI info */
    .globl  uefiComplianceInfoP /* UEFI read-only compliance string */

    .balign 8, 0x00
pSysUefiMemAddr:
    .long 0x00000000
pSysUefiAcpiAddr:
    .long 0x00000000

#endif /* INCLUDE_UEFI_BOOT_SUPPORT */


    .text
    .balign 16

/*******************************************************************************
*
* sysInit - start after boot
*
* This routine is the system start-up entry point for VxWorks in RAM, the
* first code executed after booting.  It disables interrupts, sets up
* the stack, and jumps to the C routine usrInit() in usrConfig.c.
*
* The initial stack is set to grow down from the address of sysInit().  This
* stack is used only by usrInit() and is never used again.  Memory for the
* stack must be accounted for when determining the system load address.
*
* NOTE: This routine should not be called by the user.
*
* RETURNS: N/A
*
* sysInit ()            /@ THIS IS NOT A CALLABLE ROUTINE @/
*
*/

sysInit:
_sysInit:
    cli                                     /* LOCK INTERRUPT */

    jmp     multiboot_entry

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
    .long   sysInit

multiboot_entry:

    /* Check if loaded via multiboot -- if so then save a pointer to mbinfo */

    cmpl   $MULTIBOOT_BOOTLOADER_MAGIC, %eax
    jne    setStartType
    movl   %eax,(MULTIBOOT_SCRATCH)         /* save magic */
    movl   %ebx,(MULTIBOOT_SCRATCH + 8)     /* save multi-boot pointer */
    movl   $BOOT_CLEAR,%ebx                 /* multi-boot requires BOOT_CLEAR */
    jmp    multiPrologEnd

setStartType:
    movl   SP_ARG1(%esp),%ebx               /* %ebx gets startType parameter */
multiPrologEnd:
    call   sysInitGDT

#ifdef INCLUDE_UEFI_BOOT_SUPPORT

    jmp overcomplianceinfo

    /* UEFI read-only compliance string */

uefiComplianceInfoP:
   .asciz   "UEFI 001.10 Compliant" /* format XXX.YY, where YY=1, .., 11, etc */
overcomplianceinfo:

    /*
     * The second argument on the stack is going to be the memory address of
     * the UEFI memory data block, pushed by the "go" routine in bootConfig.c
     */

    movl $FUNC(pSysUefiMemAddr), %eax       /* get mem map data buf ptr addr */
    movl SP_ARG2(%esp), %ecx
    movl %ecx, (%eax)                       /* store value into the location */

    /*
     * The third argument on the stack is going to be the memory address of
     * the UEFI ACPI data block, pushed by the "go" routine in bootConfig.c
     */

    movl $FUNC(pSysUefiAcpiAddr), %eax      /* get the ACPI data ptr address */
    movl SP_ARG3(%esp), %ecx
    movl %ecx, (%eax)                       /* store value into the location */

#endif /* INCLUDE_UEFI_BOOT_SUPPORT */

    movl    $ FUNC(sysInit),%esp            /* initialize stack pointer */
    movl    $0,%ebp                         /* initialize frame pointer */

    ARCH_REGS_INIT                          /* initialize DR[0-7] CR0 EFLAGS */

    /* ARCH_CR4_INIT                        initialize CR4 for P5,6,7 */

    xorl    %eax, %eax                      /* zero EAX */
    movl    %eax, %cr4                      /* initialize CR4 */

#ifdef INCLUDE_UEFI_BOOT_SUPPORT
    pushl   FUNC(pSysUefiAcpiAddr)     /* push the pointer to the ACPI Table */
    pushl   FUNC(pSysUefiMemAddr)      /* push the ptr to UEFI mem data block */
#endif
    pushl   %ebx                            /* push the startType */
    movl    $ FUNC(usrInit),%eax
    movl    $ FUNC(sysInit),%edx            /* push return address */
    pushl   %edx                            /*   for emulation for call */
    pushl   $0                              /* push EFLAGS, 0 */
    pushl   $0x0008                         /* a selector 0x08 is 2nd one */
    pushl   %eax                            /* push EIP,  FUNC(usrInit) */
    iret                                    /* iret */

#if defined(_WRS_CONFIG_SMP)
/*******************************************************************************
*
* sysInitCpuStartup - startup initialization for APs...
*
* void sysInitCpuStartup (void)
*
* scratch memory usage:
*
*    scratchMem (scratch memory offset)     scratchPoint
*
*    Standard GDT Entries:
*
*    null descriptor                        scratchMem + 0x04
*
*    kernel text descriptor                 scratchMem + 0x0c
*
*    kernel data descriptor                 scratchMem + 0x14
*
*    exception text descriptor              scratchMem + 0x1C
*
*    interrupt data descriptor              scratchMem + 0x24
*
*    gdt limit << 16                        scratchMem + LIM_GDT_OFFSET
*    gdt base                               scratchMem + BASE_GDT_OFFSET
*    address of page directory              scratchMem + CR3_OFFSET
*
*    idt limit                              scratchMem + LIM_IDT_OFFSET
*    idt base address                       scratchMem + BASE_IDT_OFFSET
*
*    initial AP stack addr                  scratchMem + AP_STK_OFFSET
*/
    .balign 16,0x90
FUNC_LABEL(sysInitCpuStartup)

    /* cold start code in REAL MODE (16 bits) */

    cli                                 /* LOCK INTERRUPT */

    .byte   0x67, 0x66                  /* next inst has 32bit operand */
    xor     %eax, %eax

    mov     %ax, %ds
    mov     %ax, %ss

    .byte   0x67, 0x66                  /* next inst has 32bit operand */
    mov     $CPU_SCRATCH_POINT, %eax    /* find the location of scratch mem */
    .byte   0x67, 0x66                  /* next inst has 32bit operand */
    mov     (%eax), %ecx
    mov     %cx, %sp

    /* load intial IDT */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     (%eax), %ecx
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    add     $CPU_SCRATCH_IDT, %ecx  /* base of initial idt */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    lidt    (%ecx)

    /* load intial GDT */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     (%eax), %ecx
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    add     $CPU_SCRATCH_GDT, %ecx  /* base of initial gdt */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    lgdt    (%ecx)

#if defined (_WRS_CONFIG_SMP)

    /* Enable MMU for SMP only */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     (%eax), %ecx
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    add     $CR3_OFFSET, %ecx       /* stored value for cr3 */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     (%ecx), %edx
    mov     %edx, %cr3

    /* Check for Logical CPU 0 Boot */

    mov     %cr0,%ecx               /* move CR0 to ECX */
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    or      $0x00000001,%ecx        /* set the PE bit */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    cmp     $0, %edx
    je      skipMmuInit


#ifdef  INCLUDE_MMU_P6_36BIT
    /* setup PAE, etc... */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     $0x000006F0,%ecx /* set PAE,PSE,OSXMMEXCPT,OSFXSR,PGE,MCE bits */
#else
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     $0x000006D0,%ecx /* set the OSXMMEXCPT,OSFXSR,PGE,PSE,MCE bits */
#endif  /* INCLUDE_MMU_P6_36BIT */
    mov     %ecx,%cr4               /* move ECX to CR4 */

    /* turn on paging & switch to protected mode */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     $0x80010033,%ecx        /* set the PE bit */

skipMmuInit:
    mov     %ecx,%cr0               /* move ECX to CR0 */

#else   /* defined (_WRS_CONFIG_SMP) */

    /* switch to protected mode */

    mov     %cr0,%ecx               /* move CR0 to ECX */
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    or      $0x00000001,%ecx        /* set the PE bit */
    mov     %ecx,%cr0               /* move ECX to CR0 */

#endif /* defined (_WRS_CONFIG_SMP) */

    jmp     clearSegs               /* near jump to flush a inst queue */

clearSegs:
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     $0x0010,%ecx            /* a selector 0x10 is 3rd one */
    mov     %cx,%ds
    mov     %cx,%es
    mov     %cx,%fs
    mov     %cx,%gs
    mov     %cx,%ss

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    mov     (%eax), %ecx
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    add     $AP_STK_OFFSET, %ecx    /* base of initial stk */

    /* jump to protected mode code in high mem */

#ifndef _WRS_CONFIG_SMP
ampEntry:
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    movl    (%ecx), %esp            /* initialise the stack pointer */
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    pushl   $BOOT_COLD
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    pushl   $FUNC(sysInit)
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    ljmp    $0x08,  $FUNC(sysInit)
#else
smpEntry:

    /* Check for Logical CPU 0 Boot */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    cmpl    $0, %edx
    jne     1f

    /* Logical CPU0 Boot */

    .byte   0x67, 0x66              /* next inst has 32bit operand */
    movl    (%ecx), %esp            /* initialise the stack pointer */
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    pushl   $BOOT_COLD
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    pushl   $FUNC(sysInit)
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    ljmp    $0x08,  $FUNC(sysInit)
1:
    .byte   0x67, 0x66              /* next inst has 32bit operand */
    ljmp    $0x08, $RAM_LOW_ADRS + cpuInit1 - sysInit
#endif /* !_WRS_CONFIG_SMP */

    .balign 16,0x90
FUNC_LABEL(cpuInit1)

    /* cold start code in PROTECTED MODE (32 bits) */

    movl    (%ecx), %esp            /* initialise the stack pointer */

    xorl    %eax, %eax              /* zero EAX */
    movl    %eax, %edx              /* zero EDX */

    xorl    %ebp, %ebp              /* initialize the frame pointer */
    pushl   $0                      /* initialise the EFLAGS */
    popfl

    movl    $FUNC(sysCpuInit),%eax  /* jump to cpu init to complete */

    call    *%eax

    /* just in case there's a problem */

cpuInitHlt1:
    hlt
    jmp     cpuInitHlt1
#endif /* defined (_WRS_CONFIG_SMP) */

/*******************************************************************************
*
* sysInitGDT - load the brand new GDT into RAM
*
* void sysInitGDT (void)
*/
        .balign 16,0x90
FUNC_LABEL(sysInitGDT)

    pushal
    movl    %esp,%ebp

    movl    $FUNC(sysGdt),%esi      /* set src addr (&sysGdt) */

#ifdef _WRS_CONFIG_SMP
    _IA32_PER_CPU_VALUE_GET(%edi,%eax,pSysGdt)
#else
    movl    FUNC(pSysGdt), %edi
#endif

    movl    %edi,%eax
    movl    $ GDT_ENTRIES,%ecx      /* number of GDT entries */
    movl    %ecx,%edx
    shll    $1,%ecx                 /* set (nLongs of GDT) to copy */
    cld
    rep
    movsl                           /* copy GDT from src to dst */

    pushl   %eax                    /* push the (GDT base addr) */
    shll    $3,%edx                 /* get (nBytes of GDT) */
    decl    %edx                    /* get (nBytes of GDT) - 1 */
    shll    $16,%edx                /* move it to the upper 16 */
    pushl   %edx                    /* push the nBytes of GDT - 1 */
    leal    2(%esp),%eax            /* get the addr of (size:addr) */
    pushl   %eax                    /* push it as a parameter */
    call    FUNC(sysLoadGdt)        /* load the brand new GDT in RAM */

    movl    %ebp,%esp
    popal                           /* re-initialize frame pointer */
    ret

/*******************************************************************************
*
* sysInByte - input one byte from I/O space
*
* RETURNS: Byte data from the I/O port.
*
* UCHAR sysInByte (address)
*     int address;      /@ I/O port address @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysInByte)
    movl    SP_ARG1(%esp),%edx
    movl    $0,%eax
    inb     %dx,%al
    jmp     sysInByte0
sysInByte0:
    ret

/*******************************************************************************
*
* sysInWord - input one word from I/O space
*
* RETURNS: Word data from the I/O port.
*
* USHORT sysInWord (address)
*     int address;      /@ I/O port address @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysInWord)
    movl    SP_ARG1(%esp),%edx
    movl    $0,%eax
    inw     %dx,%ax
    jmp     sysInWord0
sysInWord0:
    ret

/*******************************************************************************
*
* sysInLong - input one long-word from I/O space
*
* RETURNS: Long-Word data from the I/O port.
*
* USHORT sysInLong (address)
*     int address;      /@ I/O port address @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysInLong)
    movl    SP_ARG1(%esp),%edx
    movl    $0,%eax
    inl     %dx,%eax
    jmp     sysInLong0
sysInLong0:
    ret

/*******************************************************************************
*
* sysOutByte - output one byte to I/O space
*
* RETURNS: N/A
*
* void sysOutByte (address, data)
*     int address;      /@ I/O port address @/
*     char data;        /@ data written to the port @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysOutByte)
    movl    SP_ARG1(%esp),%edx
    movl    SP_ARG2(%esp),%eax
    outb    %al,%dx
    jmp     sysOutByte0
sysOutByte0:
    ret

/*******************************************************************************
*
* sysOutWord - output one word to I/O space
*
* RETURNS: N/A
*
* void sysOutWord (address, data)
*     int address;      /@ I/O port address @/
*     short data;       /@ data written to the port @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysOutWord)
    movl    SP_ARG1(%esp),%edx
    movl    SP_ARG2(%esp),%eax
    outw    %ax,%dx
    jmp     sysOutWord0
sysOutWord0:
    ret

/*******************************************************************************
*
* sysOutLong - output one long-word to I/O space
*
* RETURNS: N/A
*
* void sysOutLong (address, data)
*     int address;      /@ I/O port address @/
*     long data;        /@ data written to the port @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysOutLong)
    movl    SP_ARG1(%esp),%edx
    movl    SP_ARG2(%esp),%eax
    outl    %eax,%dx
    jmp     sysOutLong0
sysOutLong0:
    ret

/*******************************************************************************
*
* sysInWordString - input word string from I/O space
*
* RETURNS: N/A
*
* void sysInWordString (port, address, count)
*     int port;         /@ I/O port address @/
*     short *address;   /@ address of data read from the port @/
*     int count;        /@ count @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysInWordString)
    pushl   %edi
    movl    SP_ARG1+4(%esp),%edx
    movl    SP_ARG2+4(%esp),%edi
    movl    SP_ARG3+4(%esp),%ecx
    cld
    rep
    insw    %dx,(%edi)
    movl    %edi,%eax
    popl    %edi
    ret

/*******************************************************************************
*
* sysInLongString - input long string from I/O space
*
* RETURNS: N/A
*
* void sysInLongString (port, address, count)
*     int port;         /@ I/O port address @/
*     long *address;    /@ address of data read from the port @/
*     int count;        /@ count @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysInLongString)
    pushl   %edi
    movl    SP_ARG1+4(%esp),%edx
    movl    SP_ARG2+4(%esp),%edi
    movl    SP_ARG3+4(%esp),%ecx
    cld
    rep
    insl    %dx,(%edi)
    movl    %edi,%eax
    popl    %edi
    ret

/*******************************************************************************
*
* sysOutWordString - output word string to I/O space
*
* RETURNS: N/A
*
* void sysOutWordString (port, address, count)
*     int port;         /@ I/O port address @/
*     short *address;   /@ address of data written to the port @/
*     int count;        /@ count @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysOutWordString)
    pushl   %esi
    movl    SP_ARG1+4(%esp),%edx
    movl    SP_ARG2+4(%esp),%esi
    movl    SP_ARG3+4(%esp),%ecx
    cld
    rep
    outsw   (%esi),%dx
    movl    %esi,%eax
    popl    %esi
    ret

/*******************************************************************************
*
* sysOutLongString - output long string to I/O space
*
* RETURNS: N/A
*
* void sysOutLongString (port, address, count)
*     int port;         /@ I/O port address @/
*     long *address;    /@ address of data written to the port @/
*     int count;        /@ count @/
*
*/

    .balign 16,0x90
FUNC_LABEL(sysOutLongString)
    pushl   %esi
    movl    SP_ARG1+4(%esp),%edx
    movl    SP_ARG2+4(%esp),%esi
    movl    SP_ARG3+4(%esp),%ecx
    cld
    rep
    outsl   (%esi),%dx
    movl    %esi,%eax
    popl    %esi
    ret

/*******************************************************************************
*
* sysWait - wait until the input buffer becomes empty on an 8042 microcontroller
*
* wait until the input buffer becomes empty
*
* NOTE: THIS IS NOT A ROUTINE FOR GENERAL USE.
*       It can only be used on legacy systems with an 8042 PS/2 keyboard
*       controller. Many newer systems based on new generation integrated
*       chipsets may no longer even have or need an 8042.
*
* RETURNS: N/A
*
* void sysWait (void)
*
*/

    .balign 16,0x90
FUNC_LABEL(sysWait)
    xorl    %ecx,%ecx
sysWait0:
    movl    $0x64,%edx               /* Check if it is ready to write */
    inb     %dx,%al
    andb    $2,%al
    loopnz  sysWait0
    ret

/*******************************************************************************
*
* sysReboot - reboot system
*
* RETURNS: N/A
*
* NOMANUAL
*
* void sysReboot ()
*
*/

    .balign 16,0x90
FUNC_LABEL(sysReboot)

    pushl   $0x2
    call    FUNC(sysToMonitor)       /* initiate cold boot */
    ret

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9))
/*******************************************************************************
*
* sysCpuProbe - perform CPUID if supported and check a type of CPU FAMILY
*
* This routine performs CPUID if supported and check a type of CPU FAMILY.
* This routine is called only once in cacheArchLibInit().  If it is
* called later, it returns the previously obtained result.
*
* RETURNS: a type of CPU FAMILY; 0(386), 1(486), 2(P5/Pentium),
*          4(P6/PentiumPro) or 5(P7/Pentium4).
*
* UINT sysCpuProbe (void)
*
*/

    .balign 16,0x90
FUNC_LABEL(sysCpuProbe)
    cmpl    $ NONE, FUNC(sysProcessor)       /* is it executed already? */
    je      sysCpuProbeStart                 /*   no: start the CPU probing */
    movl    FUNC(sysProcessor), %eax         /* return the sysProcessor */
    ret

sysCpuProbeStart:
    pushfl                                   /* save EFLAGS */
    cli                                      /* LOCK INTERRUPT */

    /* check 386. AC bit is a new bit for 486, 386 can not toggle */

    pushfl                                   /* push EFLAGS */
    popl    %edx                             /* pop EFLAGS on EDX */
    movl    %edx, %ecx                       /* save original EFLAGS to ECX */
    xorl    $ EFLAGS_AC, %edx                /* toggle AC bit */
    pushl   %edx                             /* push new EFLAGS */
    popfl                                    /* set new EFLAGS */
    pushfl                                   /* push EFLAGS */
    popl    %edx                             /* pop EFLAGS on EDX */
    xorl    %edx, %ecx                       /* if AC bit is toggled ? */
    jz      sysCpuProbe386                   /*   no: it is 386 */
    pushl   %ecx                             /* push original EFLAGS */
    popfl                                    /* restore original EFLAGS */

    /* check 486. ID bit is a new bit for Pentium, 486 can not toggle */

    pushfl                                   /* push EFLAGS */
    popl    %edx                             /* pop EFLAGS on EDX */
    movl    %edx, %ecx                       /* save original EFLAGS to ECX */
    xorl    $ EFLAGS_ID, %edx                /* toggle ID bit */
    pushl    %edx                            /* push new EFLAGS */
    popfl                                    /* set new EFLAGS */
    pushfl                                   /* push EFLAGS */
    popl    %edx                             /* pop EFLAGS on EDX */
    xorl    %edx, %ecx                       /* if ID bit is toggled ? */
    jz      sysCpuProbe486                   /*   no: it is 486 */

    /* execute CPUID to get vendor, family, model, stepping, features */

    pushl   %ebx                             /* save EBX */
    movl    $ CPUID_486, FUNC(sysCpuId)+CPUID_SIGNATURE /* set it 486 */

    /* EAX=0, get the highest value and the vendor ID */

    movl    $0, %eax                         /* set EAX 0 */
    cpuid                                    /* execute CPUID */

    /* check the vendor ID */

    cmpl    %ebx, VAR(vendorIdIntel)+0       /* comp vendor id[0] */
    jne     sysCpuProbeUnknown
    cmpl    %edx, VAR(vendorIdIntel)+4       /* comp vendor id[1] */
    jne     sysCpuProbeUnknown
    cmpl    %ecx, VAR(vendorIdIntel)+8       /* comp vendor id[2] */
    jne     sysCpuProbeUnknown

    /* Intel CPUs will be investigated farther, for now */

    movl    %eax, FUNC(sysCpuId)+CPUID_HIGHVALUE     /* save high value */
    movl    %ebx, FUNC(sysCpuId)+CPUID_VENDORID      /* save vendor id[0] */
    movl    %edx, FUNC(sysCpuId)+CPUID_VENDORID+4    /* save vendor id[1] */
    movl    %ecx, FUNC(sysCpuId)+CPUID_VENDORID+8    /* save vendor id[2] */
    cmpl    $1, %eax                                 /* is CPUID(1) ok? */
    jl      sysCpuProbeEnd                           /*   no: end probe */

    /* EAX=1, get the processor signature and feature flags */

    movl    $1, %eax                                 /* set EAX 1 */
    cpuid                                            /* execute CPUID */
    movl    %eax, FUNC(sysCpuId)+CPUID_SIGNATURE     /* save signature */
    movl    %edx, FUNC(sysCpuId)+CPUID_FEATURES_EDX  /* save feature EDX */
    movl    %ecx, FUNC(sysCpuId)+CPUID_FEATURES_ECX  /* save feature ECX */
    movl    %ebx, FUNC(sysCpuId)+CPUID_FEATURES_EBX  /* save feature EBX */
    cmpl    $2, FUNC(sysCpuId)+CPUID_HIGHVALUE       /* is CPUID(2) ok? */
    jl      sysCpuProbeEnd                           /*   no: end probe */

    /* EAX=2, get the configuration parameters */

    movl    $2, %eax                                 /* set EAX 2 */
    cpuid                                            /* execute CPUID */
    movl    %eax, FUNC(sysCpuId)+CPUID_CACHE_EAX     /* save config EAX */
    movl    %ebx, FUNC(sysCpuId)+CPUID_CACHE_EBX     /* save config EBX */
    movl    %ecx, FUNC(sysCpuId)+CPUID_CACHE_ECX     /* save config ECX */
    movl    %edx, FUNC(sysCpuId)+CPUID_CACHE_EDX     /* save config EDX */
    cmpl    $3, FUNC(sysCpuId)+CPUID_HIGHVALUE       /* is CPUID(3) ok? */
    jl      sysCpuProbeEnd                           /*   no: end probe */

    /* EAX=3, get the processor serial no */

    movl    $3, %eax                                 /* set EAX 3 */
    cpuid                                            /* execute CPUID */
    movl    %edx, FUNC(sysCpuId)+CPUID_SERIALNO      /* save serialno[2] */
    movl    %ecx, FUNC(sysCpuId)+CPUID_SERIALNO+4    /* save serialno[3] */

    /* EAX=0x80000000, to see if the Brand String is supported */

    movl    $0x80000000, %eax                        /* set EAX 0x80000000 */
    cpuid                                            /* execute CPUID */
    cmpl    $0x80000000, %eax                        /* Brand String support? */
    jbe     sysCpuProbeEnd                           /*   no: end probe */

    /* EAX=0x8000000[234], get the Brand String */

    movl    $0x80000002, %eax                        /* set EAX 0x80000002 */
    cpuid                                            /* execute CPUID */
    movl    %eax, FUNC(sysCpuId)+CPUID_BRAND_STR     /* save brandStr[0] */
    movl    %ebx, FUNC(sysCpuId)+CPUID_BRAND_STR+4   /* save brandStr[1] */
    movl    %ecx, FUNC(sysCpuId)+CPUID_BRAND_STR+8   /* save brandStr[2] */
    movl    %edx, FUNC(sysCpuId)+CPUID_BRAND_STR+12  /* save brandStr[3] */

    movl    $0x80000003, %eax                        /* set EAX 0x80000003 */
    cpuid                                            /* execute CPUID */
    movl    %eax, FUNC(sysCpuId)+CPUID_BRAND_STR+16  /* save brandStr[4] */
    movl    %ebx, FUNC(sysCpuId)+CPUID_BRAND_STR+20  /* save brandStr[5] */
    movl    %ecx, FUNC(sysCpuId)+CPUID_BRAND_STR+24  /* save brandStr[6] */
    movl    %edx, FUNC(sysCpuId)+CPUID_BRAND_STR+28  /* save brandStr[7] */

    movl    $0x80000004, %eax                        /* set EAX 0x80000004 */
    cpuid                                            /* execute CPUID */
    movl    %eax, FUNC(sysCpuId)+CPUID_BRAND_STR+32  /* save brandStr[8] */
    movl    %ebx, FUNC(sysCpuId)+CPUID_BRAND_STR+36  /* save brandStr[9] */
    movl    %ecx, FUNC(sysCpuId)+CPUID_BRAND_STR+40  /* save brandStr[10] */
    movl    %edx, FUNC(sysCpuId)+CPUID_BRAND_STR+44  /* save brandStr[11] */

sysCpuProbeEnd:
    popl    %ebx                                 /* restore EBX */
    movl    FUNC(sysCpuId)+CPUID_SIGNATURE, %eax /* get the signature */
    andl    $ CPUID_FAMILY, %eax                 /* mask it with FAMILY */
    cmpl    $ CPUID_486, %eax                    /* is the CPU FAMILY 486 ? */
    je      sysCpuProbe486                       /*   yes: jump to ..486 */
    cmpl    $ CPUID_PENTIUM, %eax                /* is CPU FAMILY PENTIUM ? */
    je      sysCpuProbePentium                   /*   yes: jump to ..Pentium */
    cmpl    $ CPUID_PENTIUMPRO, %eax             /* is CPU FAMILY PENTIUMPRO ?*/
    je      sysCpuProbePentiumpro                /*   yes: jump to Pentiumpro */
    cmpl    $ CPUID_EXTENDED, %eax               /* is CPU FAMILY EXTENDED ? */
    je      sysCpuProbeExtended                  /*   yes: jump to ..Extended */

sysCpuProbeUnknown:
    popl    %ebx                                 /* restore EBX */
    movl    $ X86CPU_DEFAULT, %eax               /* it is defined in config.h */
    jmp     sysCpuProbeExit

sysCpuProbe486:
    movl    $ X86CPU_486, %eax                   /* set 1 for 486 */
    jmp     sysCpuProbeExit

sysCpuProbePentium:
    movl    $ X86CPU_PENTIUM, %eax               /* set 2 for P5/Pentium */
    jmp     sysCpuProbeExit

sysCpuProbePentiumpro:
    movl    $ X86CPU_PENTIUMPRO, %eax            /* set 4 for P6/PentiumPro */
    jmp     sysCpuProbeExit

sysCpuProbeExtended:
    movl    FUNC(sysCpuId)+CPUID_SIGNATURE, %eax /* get the signature */
    andl    $ CPUID_EXT_FAMILY, %eax             /* mask with EXTENDED FAMILY */
    cmpl    $ CPUID_PENTIUM4, %eax               /* is the CPU FAMILY 486 ? */
    je      sysCpuProbePentium4                  /* yes: jump to ..Pentium4 */

    jmp    sysCpuProbe486                        /* unknown CPU. assume 486 */

sysCpuProbePentium4:
    movl    $ X86CPU_PENTIUM4, %eax              /* set 5 for P7/Pentium4 */
    jmp     sysCpuProbeExit

sysCpuProbe386:
        movl    $ X86CPU_386, %eax               /* set 0 for 386 */

sysCpuProbeExit:
    popfl                                        /* restore EFLAGS */
    movl    %eax, FUNC(sysProcessor)             /* set the CPU FAMILY */
    ret
#endif /* ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9)) */

/*******************************************************************************
*
* sysCalTSC - Calibrate TSC (Time Stamp Counter)
* This routine uses PIT running at mode 3 for a specified milisecond duration
* to get the TSC frequency.
* Note:
* 1. duration must be greater than 0, and no greater than 20.
* 2. to avoid unnecessary error, make sure 1000 can be be exactly divided by
* input duration.
*
* RETURNS: N/A
*
* NOMANUAL
* UINT32 sysCalTSC ( int duration )
*/

    .balign 16,0x90
FUNC_LABEL(sysCalTSC)
    movl    SP_ARG1(%esp),%edx  /* get duration */
    pushfl

    /*
     * pre-compute the end tick:
     * PIT clockrate is 1193182, and it is a count down clock. Fill it with
     * 65535, and wait to computed count to get specific delay. Considering
     * all the IO read/write in the loop will consume about 5us in most
     * targets, so compensate 5us to the end PIT count (about 12 PIT ticks).
     */

    movl $2386, %eax
    mull %edx
    movl $0xffff, %edx
    subl %eax, %edx
    addl $12, %edx                  /* 12 PIT tick compensation */
    pushl %edx                      /* save the value in stack */

    /* select counter2, working at mode 3 */

    movb $0xb6, %al
    outb %al, $0x43

    /* set the initial count value to be 65535 */

    movb $0xff, %al
    outb %al, $0x42
    outb %al, $0x42

    /* enable counter 2 clock */

    inb  $0x61, %al
    orb  $1, %al
    outb %al, $0x61

    /* read the start count value of TSC, make it serialized */

    lfence
    rdtsc
    movl %eax, %ebx                 /* save start count in ebx */
    popl %edx

sysCalTSC0:

    movl $0x080, %eax
    outb %al, $0x43
    inb  $0x42, %al
    movl %eax, %ecx                 /* save lower 8bit */
    inb  $0x42, %al
    shl  $8, %eax                   /* lshift 8bit */
    addl %eax, %ecx

    cmpl %edx, %ecx                 /* compare PIT end count */
    jge  sysCalTSC0

    /* read the end count of TSC */

    lfence
    rdtsc
    subl %ebx, %eax
    movl %eax, %ebx                 /* save the result */

    /* disable timer 2 clock */

    inb  $0x61,%al
    andb $0xfe,%al
    outb %al,$0x61

    movl %ebx, %eax
    popfl
    ret

/*******************************************************************************
*
* sysLoadGdt - load the global descriptor table.
*
* RETURNS: N/A
*
* NOMANUAL
*
* void sysLoadGdt (char *sysGdtr)
*
*/

        .balign 16,0x90
FUNC_LABEL(sysLoadGdt)
    movl    4(%esp),%eax
    lgdt    (%eax)
    movw    $0x0010,%ax     /* set data segment selector 0x10 (the 3rd one) */
    movw    %ax,%ds
    movw    %ax,%es
    movw    %ax,%fs
    movw    %ax,%gs
    movw    %ax,%ss
    ret

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR == 9) && \
     (_WRS_VXWORKS_MAINT < 3))

/*******************************************************************************
*
* sysGdt - the global descriptor table.
*
* RETURNS: N/A
*
* NOMANUAL
*
*/

    .text
    .balign 16,0x90
FUNC_LABEL(sysGdtr)
    .word   0x0027                  /* size   : 39(8 * 5 - 1) bytes */
    .long   FUNC(sysGdt)

    .balign 16,0x90
FUNC_LABEL(sysGdt)
    /* 0(selector=0x0000): Null descriptor */
    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 1(selector=0x0008): Code descriptor, for the supervisor mode task */
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

    /* 3(selector=0x0018): Code descriptor, for the exception */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 4(selector=0x0020): Code descriptor, for the interrupt */
    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9a                    /* Code e/r, Present, DPL0 */
    .byte   0xcf                    /* limit: fxxxx, Page Gra, 32bit */
    .byte   0x00                    /* base : 00xxxxxx */


    .data
    .balign 32,0x90
FUNC_LABEL(sysCsSuper)
    .long   0x00000008              /* CS for supervisor mode task */
FUNC_LABEL(sysCsExc)
    .long   0x00000018              /* CS for exception */
FUNC_LABEL(sysCsInt)
    .long   0x00000020              /* CS for interrupt */

#else /* New GDT layout for sysCall */

/*******************************************************************************
*
* sysGdt - the global descriptor table.
*
* RETURNS: N/A
*
* NOMANUAL
*
*/

    .text
    .balign 16,0x90
FUNC_LABEL(sysGdtr)
    .word   0x004f          /* size   : 79(8 * 10 - 1) bytes */
    .long   FUNC(sysGdt)

    .balign 16,0x90
FUNC_LABEL(sysGdt)

    /* 0(selector=0x0000): Null descriptor */

    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /*
     * The global descriptor table will be populated with some, if not    
     * all, of the following entries based on the image configuration:
     *
     *    Index         Usage
     *    -----         -----
     *      0            RESERVED
     *                  
     *      1            Supervisor Code Segment  (0x00000008)
     *      2            Supervisor Data Segment  (0x00000010)
     *                                             
     *     (3 - 5 exist if RTPs enabled...)
     *
     *      3            User Code Segment        (0x00000018)
     *      4            User Data Segment        (0x00000020)
     *      5            RTP TSS                  (0x00000028)
     *
     *     (5 - 7 exist if OSM enabled...)
     *
     *      5            OSM/RTP Save TSS         (0x00000028)
     *      6            OSM Restore TSS          (0x00000030)
     *      7            OSM Task Gate            (0x00000038)
     *
     *     (8 - 9 allways present...)
     *
     *      8            Trap Segment             (0x00000040)
     *      9            Interrupt Segment        (0x00000048)
     *
     * Note GDT entries 1,2,8,and 9 always present.
     */

    /* 1(selector=0x0008): Code descriptor, for the supervisor mode task */

    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9b        /*
                         * (P) Segment Present = 0x1, 
                         * (DPL) Privilege Level = 0x00 (DPL0),
                         * (S) Descriptor Type = 0x1 (code/data),
                         * (TYPE) Segement type = 0xB (exec, read, non-conforming)
                         */
    .byte   0xcf        /*
                         * (G) Granularity = 0x1,
                         * (D/B) Default Op Size = 0x1 (32-bit segment),
                         * (L) 64 bit Code Segment = 0x0 (IA-32e mode only), 
                         * (AVL) Available for use - 0x0,
                         * Seg Limit 19:16 = 0xf
                         */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 2(selector=0x0010): Data descriptor */

    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x93        /*
                         * (P) Segment Present = 0x1, 
                         * (DPL) Privilege Level = 0x00 (DPL0),
                         * (S) Descriptor Type = 0x1 (code/data),
                         * (TYPE) Segement type = 0x3 (expand-up, read, write)
                         */
    .byte   0xcf        /*
                         * (G) Granularity = 0x1,
                         * (D/B) Default Op Size = 0x1 (32-bit segment),
                         * (L) 64 bit Code Segment = 0x0 (IA-32e mode only), 
                         * (AVL) Available for use - 0x0,
                         * Seg Limit 19:16 = 0xf
                         */
    .byte   0x00                    /* base : 00xxxxxx */

    /* 3 (selector=0x0018): place holder for User Code Segment*/

    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 4 (selector=0x0020): place holder for User Data Segment */

    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 5 (selector=0x0028): place holder for OSM/RTP Save TSS */

    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 6 (selector=0x0030): place holder for OSM Restore TSS */

    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 7 (selector=0x0038): place holder for OSM Task Gate */

    .word   0x0000
    .word   0x0000
    .byte   0x00
    .byte   0x00
    .byte   0x00
    .byte   0x00

    /* 8 (selector=0x0040): Code descriptor, for the exception */

    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9B        /*
                         * (P) Segment Present = 0x1, 
                         * (DPL) Privilege Level = 0x00 (DPL0),
                         * (S) Descriptor Type = 0x1 (code/data),
                         * (TYPE) Segement type = 0xB (exec, read, non-conforming)
                         */
    .byte   0xcf        /*
                         * (G) Granularity = 0x1,
                         * (D/B) Default Op Size = 0x1 (32-bit segment),
                         * (L) 64 bit Code Segment = 0x0 (IA-32e mode only), 
                         * (AVL) Available for use - 0x0,
                         * Seg Limit 19:16 = 0xf
                         */
    .byte   0x00                    /* base : 00xxxxxx */

        /* 9 (selector=0x0048): Code descriptor, for the interrupt */

    .word   0xffff                  /* limit: xffff */
    .word   0x0000                  /* base : xxxx0000 */
    .byte   0x00                    /* base : xx00xxxx */
    .byte   0x9B        /*
                         * (P) Segment Present = 0x1, 
                         * (DPL) Privilege Level = 0x00 (DPL0),
                         * (S) Descriptor Type = 0x1 (code/data),
                         * (TYPE) Segement type = 0xB (exec, read,
                         * non-conforming)
                         */
    .byte   0xcf        /*
                         * (G) Granularity = 0x1,
                         * (D/B) Default Op Size = 0x1 (32-bit segment),
                         * (L) 64 bit Code Segment = 0x0 (IA-32e mode only), 
                         * (AVL) Available for use - 0x0,
                         * Seg Limit 19:16 = 0xf
                         */
    .byte   0x00                    /* base : 00xxxxxx */

    .data
    .balign 32,0x90
FUNC_LABEL(sysCsSuper)
    .long   0x00000008                  /* CS for supervisor mode task */
FUNC_LABEL(sysCsExc)
        .long   0x00000040              /* CS for exception */
FUNC_LABEL(sysCsInt)
        .long   0x00000048              /* CS for interrupt */

#endif /* ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR == 9) &&
           (_WRS_VXWORKS_MAINT < 3)) */
