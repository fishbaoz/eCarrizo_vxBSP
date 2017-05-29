/* vxCpuIdALib.s - Intel Architecture CpuId Probe Routine */

/*
 * Copyright (c) 2010-2012 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/*
modification history
--------------------
01e,26feb14,dmh  detect AuthenticAMD
01d,13feb12,yjw  Set max cores of Performance Monitor Features to 32
                 (WIND00293328)
01c,07jul11,scm  WIND00189850, add ability to determine multi-core processor
                 topology
01b,08nov10,sem  Remove sti from exit routines (WIND00240296)
01a,26jul10,sem  Created
*/

/*
DESCRIPTION
These routines execute the cpuid instruction on IA processors.
The output of the cpuid instruction is dependent on the specific
function code loaded in the EAX register.

Support routines have been added to aid in detecting multi-core
processor topology on IA platforms.
*/

#define _ASMLANGUAGE
#include <vxWorks.h>
#include <asm.h>
#include <regs.h>
#include <hwif/cpu/arch/i86/vxCpuIdLib.h>

    .data

    .globl  FUNC(vxCpuIdHWMTSupported)        
    .globl  FUNC(vxCpuIdMaxNumLPPerPkg) 
    .globl  FUNC(vxCpuIdMaxNumCoresPerPkg) 
    .globl  FUNC(vxCpuIdInitialApicId)
    .globl  FUNC(vxCpuIdBitFieldWidth) 

#ifndef  _WRS_CONFIG_LP64   
    .globl  GTEXT(vxCpuIdProbe32)
#else    
    .globl  GTEXT(vxCpuIdProbe64)
#endif /* _WRS_CONFIG_LP64 */    

FUNC_LABEL(vxVendorIdIntel)               /* CPUID vendor ID - Intel */
    .ascii  "GenuineIntel"
FUNC_LABEL(vxVendorIdAmd)
    .ascii  "AuthenticAMD"                /* CPUID vendor ID - Amd   */

#ifndef _WRS_CONFIG_LP64

/*******************************************************************************
*
* vxCpuIdHWMTSupported: indicates if hardware multi-threaded support present
*
* SYNOPSIS
* \ss
* BOOL vxCpuIdHWMTSupported (void)
* \se
*
* This routine indicates the state of hardware multi-threaded support
*
* %eax - hardware multi-threaded support bit #28 status
*
* RETURNS: 0 when the hardware multi-threaded bit is not set
*
*/

FUNC_LABEL(vxCpuIdHWMTSupported)

        pushl   %ebx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %edx, %eax      /* hardware multi-threaded support bit in edx */
        andl    $VX_CPUID_HWMT_BIT, %eax    /* return VX_CPUID_HWMT_BIT value */

        testl   %eax, %eax
        jz      noHWMT_0  /* special case, no hardware multi-threaded support */

        movl    $0x1, %eax                                     /* return True */

        popl    %ebx                                           /* restore ebx */

        ret

noHWMT_0:

        movl    $0x0, %eax                                    /* return FALSE */

        popl    %ebx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdMaxNumLPPerPkg: returns num logical processor per package
*
* SYNOPSIS
* \ss
* unsigned int vxCpuIdMaxNumLPPerPkg (void)
* \se
*
* This routine returns the number of logical processor per package.
*
* RETURNS: unsigned int, representing the number of logical processor per
*          package
*
*/

FUNC_LABEL(vxCpuIdMaxNumLPPerPkg)

        pushl   %ebx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %edx, %eax      /* hardware multi-threaded support bit in edx */
        andl    $VX_CPUID_HWMT_BIT, %eax           /* VX_CPUID_HWMT_BIT value */

        testl   %eax, %eax
        jz      noHWMT_1  /* special case, no hardware multi-threaded support */

        movl    %ebx, %eax   /* max num of addressable IDs for LPs in package */
        andl    $VX_CPUID_LOGICAL_BITS, %eax    /* mask VX_CPUID_LOGICAL_BITS */
        rorl    $VX_CPUID_LOGICAL_SHIFT, %eax    /* log processor per package */

        popl    %ebx                                           /* restore ebx */

        ret

noHWMT_1:

        movl    $0x01, %eax       /* return one logical processor per package */

        popl    %ebx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdMaxNumCoresPerPkg: returns max number of cores per package
*
* SYNOPSIS
* \ss
* unsigned int vxCpuIdMaxNumCoresPerPkg (void)
* \se
*
* This routine returns the maximum number of cores per package.
*
* RETURNS: unsigned int, representing the number of cores per package
*
*/

FUNC_LABEL(vxCpuIdMaxNumCoresPerPkg)

        pushl   %ebx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %edx, %eax      /* hardware multi-threaded support bit in edx */
        andl    $VX_CPUID_HWMT_BIT, %eax           /* VX_CPUID_HWMT_BIT value */

        testl   %eax, %eax
        jz      noHWMT_2  /* special case, no hardware multi-threaded support */

        movl    $VX_CPUID_KEY0, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        cmpl    $VX_CPUID_LEAF_4, %eax           /* does CPUID support leaf 4 */
        jl      noHWMT_2                       /* if not, must be single core */

        movl    $VX_CPUID_LEAF_4, %eax                 /* determine num cores */
        movl    $0, %ecx                 /* start with first level, index = 0 */
        cpuid                                      /* serializing instruction */

                                           /* eax has info on number of cores */
        andl    $VX_CPUID_CORE_BITS, %eax          /* mask VX_CPUID_CORE_BITS */
        rorl    $VX_CPUID_CORE_SHIFT, %eax    /* max number cores per package */
        incl    %eax                                        /* increment by 1 */

        popl    %ebx                                           /* restore ebx */

        ret

noHWMT_2:

        movl    $0x01, %eax              /* must be a single core per package */

        popl    %ebx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdInitialApicId: returns initial Apic Id info for logical processor
*
* SYNOPSIS
* \ss
* unsigned char vxCpuIdInitialApicId (void)
* \se
*
* This routine returns the initial Apic Id info for logical processor.
*
* RETURNS: unsigned char, representing the initial Apic Id info for logical
*          processor
*
*/

FUNC_LABEL(vxCpuIdInitialApicId)

        pushl   %ebx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %ebx, %eax      /* initial Apic Id info for logical processor */
        andl    $VX_CPUID_APIC_ID_BITS, %eax 
        rorl    $VX_CPUID_APIC_ID_SHIFT, %eax    /* logical processor Apic Id */

        popl    %ebx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdBitFieldWidth: general purpose routine to determine the width of a bit
*                       field based on the maximum number of unique identifiers
*                       that bit can represent.
*
* SYNOPSIS
* \ss
* unsigned int vxCpuIdBitFieldWidth (unsigned int count_item)
* \se
*
* count_item passed on stack: SP_ARG1(%esp)
*
* This routine returns the width of a bit field.
*
* RETURNS: unsigned int, representing the width of a bit field
*
*/

FUNC_LABEL(vxCpuIdBitFieldWidth)

        movl SP_ARG1(%esp),%edx                  /* get count_item from stack */

        movl $0, %ecx
        movl %ecx, %eax                                   /* initialize width */

        decl %edx
        bsr  %dx, %cx
        jz   done

        inc  %cx
        movl %ecx, %eax                                       /* update width */

done:
        ret

/*******************************************************************************
*
* vxCpuIdProbe32 - perform CPUID if supported and populate the VX_CPUID struct
*
* This routine performs 32-bit CPUID and populates the VX_CPUID struct.
*
* RETURNS: None
*
* UINT vxCpuIdProbe32 (Pointer to updated VX_CPUID structure)
*/

    .text
    .balign 16,0x90
FUNC_LABEL(vxCpuIdProbe32)

    pushl   %ebp
    movl    %esp,%ebp
    pushfl                              /* save EFLAGS */
    pushl   %ebx
    pushl   %ecx
    pushl   %edx
    pushl   %esi
    pushl   %edi
    movl    8(%ebp), %esi               /* Pointer to Updated CPUID Structure */       
    cli                                 /* LOCK INTERRUPT */

    /* execute CPUID to get vendor, family, model, stepping, features */

    /* EAX=0, get the highest value and the vendor ID */

    movl    $0, %eax                    /* set EAX 0 */
    cpuid                               /* execute CPUID */

    /* check for AMD vendor ID */
    
    cmpl    %ebx, VAR(vxVendorIdAmd)+0  /* comp vendor id[0] */
    jne     vxCpuProbeIntel
    cmpl    %edx, VAR(vxVendorIdAmd)+4  /* comp vendor id[1] */
    jne     vxCpuProbeExit
    cmpl    %ecx, VAR(vxVendorIdAmd)+8  /* comp vendor id[2] */
    jne     vxCpuProbeExit


    movl    %eax, VX_CPUID_HIGHVALUE(%esi)  /* save high value */
    movl    %ebx, VX_CPUID_VENDORID(%esi)   /* save vendor id[0] */
    movl    %edx, VX_CPUID_VENDORID+4(%esi) /* save vendor id[1] */
    movl    %ecx, VX_CPUID_VENDORID+8(%esi) /* save vendor id[2] */

    /* EAX=1, get the processor signature and feature flags */

    movl    $1, %eax                          /* set EAX 1 */
    cpuid                                     /* execute CPUID */
    xorl    %ecx,%ecx                         /* LAURENT TODO correctly handle */
    movl    %eax, VX_CPUID_SIGNATURE(%esi)    /* save signature */
    movl    %edx, VX_CPUID_FEATURES_EDX(%esi) /* save feature EDX */
    movl    %ecx, VX_CPUID_FEATURES_ECX(%esi) /* save feature ECX */
    movl    %ebx, VX_CPUID_FEATURES_EBX(%esi) /* save feature EBX */

    /* EAX=0x8000000[234], get the Brand String */

    movl    $0x80000002, %eax                 /* set EAX 0x80000002 */
    cpuid                                     /* execute CPUID */
    
    movl    %eax, VX_CPUID_BRAND_STR(%esi)   /* save brandStr[0] */
    movl    %ebx, VX_CPUID_BRAND_STR+4(%esi) /* save brandStr[1] */
    movl    %ecx, VX_CPUID_BRAND_STR+8(%esi) /* save brandStr[2] */
    movl    %edx, VX_CPUID_BRAND_STR+12(%esi)/* save brandStr[3] */

    movl    $0x80000003, %eax                /* set EAX 0x80000003 */
    cpuid                                    /* execute CPUID */
    
    movl    %eax, VX_CPUID_BRAND_STR+16(%esi) /* save brandStr[4] */
    movl    %ebx, VX_CPUID_BRAND_STR+20(%esi) /* save brandStr[5] */
    movl    %ecx, VX_CPUID_BRAND_STR+24(%esi) /* save brandStr[6] */
    movl    %edx, VX_CPUID_BRAND_STR+28(%esi) /* save brandStr[7] */

    movl    $0x80000004, %eax                 /* set EAX 0x80000004 */
    cpuid                                     /* execute CPUID */
    
    movl    %eax, VX_CPUID_BRAND_STR+32(%esi) /* save brandStr[8] */
    movl    %ebx, VX_CPUID_BRAND_STR+36(%esi) /* save brandStr[9] */
    movl    %ecx, VX_CPUID_BRAND_STR+40(%esi) /* save brandStr[10] */
    movl    %edx, VX_CPUID_BRAND_STR+44(%esi) /* save brandStr[11] */

    jmp     vxCpuProbeExit
    
vxCpuProbeIntel:
    
    /* check the vendor ID */

    cmpl    %ebx, VAR(vxVendorIdIntel)+0  /* comp vendor id[0] */
    jne     vxCpuProbeExit
    cmpl    %edx, VAR(vxVendorIdIntel)+4  /* comp vendor id[1] */
    jne     vxCpuProbeExit
    cmpl    %ecx, VAR(vxVendorIdIntel)+8  /* comp vendor id[2] */
    jne     vxCpuProbeExit

vxCpuSkipIntel:
    
    movl    %eax, VX_CPUID_HIGHVALUE(%esi)  /* save high value */
    movl    %ebx, VX_CPUID_VENDORID(%esi)   /* save vendor id[0] */
    movl    %edx, VX_CPUID_VENDORID+4(%esi) /* save vendor id[1] */
    movl    %ecx, VX_CPUID_VENDORID+8(%esi) /* save vendor id[2] */

    cmpl    $1, %eax                        /* is CPUID(1) ok? */
    jl      vxCpuProbeExtended              /* no: extended probe */

    /* EAX=1, get the processor signature and feature flags */

    movl    $1, %eax                          /* set EAX 1 */
    cpuid                                     /* execute CPUID */
    
    movl    %eax, VX_CPUID_SIGNATURE(%esi)    /* save signature */
    movl    %edx, VX_CPUID_FEATURES_EDX(%esi) /* save feature EDX */
    movl    %ecx, VX_CPUID_FEATURES_ECX(%esi) /* save feature ECX */
    movl    %ebx, VX_CPUID_FEATURES_EBX(%esi) /* save feature EBX */
        
    cmpl    $2, VX_CPUID_HIGHVALUE(%esi)      /* is CPUID(2) ok? */
    jl      vxCpuProbeExtended                /* no: extended probe */

    /* EAX=2, get the cache descriptors */

    movl    $0, %edi                     /* set to zero */
    movl    $2, %eax                     /* set EAX 2 */
    cpuid                                /* execute CPUID */
    
    /* The lower 8 bits of EAX contain the value that identifies
       the number of times CPUID must be executed in order to obtain
       a complete image of the processor's caching systems.
     */
    
    movl    %eax, %edi                   /* store counter value */
    cmp     $1, %al                      /* is count > 1? */
    jle     vxCpuProbeDecode             /* yes, continue CPUID execution */ 
    sub     $1, %di                      /* already executed once, so decrement count */
    
vxCpuProbeRep:
    cpuid                                /* execute CPUID */
    sub      $1,  %di                    /* decrement count */
    test     %di, %di                    /* is count 0? */
    jnz    vxCpuProbeRep                 /* no, continue execution until count==0 */
    
vxCpuProbeDecode:    
    movl    %eax, VX_CPUID_CACHE_EAX(%esi)  /* save config EAX */
    movl    %ebx, VX_CPUID_CACHE_EBX(%esi)  /* save config EBX */
    movl    %ecx, VX_CPUID_CACHE_ECX(%esi)  /* save config ECX */
    movl    %edx, VX_CPUID_CACHE_EDX(%esi)  /* save config EDX */
    cmpl    $3, VX_CPUID_HIGHVALUE(%esi)    /* is CPUID(3) ok? */
    jl      vxCpuProbeExtended              /* no: extended probe */
    
    /* EAX=3, processor serial number */
    
    movl    $3, %eax                     /* set EAX 3 */
    cpuid                                /* execute CPUID */
    movl    %edx, CPUID_SERIALNO(%esi)   /* save serialno[2] */
    movl    %ecx, CPUID_SERIALNO+4(%esi) /* save serialno[3] */
    cmpl    $4, VX_CPUID_HIGHVALUE(%esi) /* is CPUID(4) ok? */
    jl      vxCpuProbeExtended           /* no: extended probe */
                
    /* EAX=4, get deterministic cache parameters */
    
    movl    $0, %edi                   /* set to zero */
    addl    $VX_CPUID_CACHE_PARAMS,%esi
        
vxCpuProbeCache:    
    
    /* execute until eax[4:0] == 0, incrementing ecx */

    movl    $4,   %eax                   /* set EAX 4 */
    movl    %edi, %ecx                   /* set ECX   */
    cpuid                                /* execute CPUID */

    /* store cache parameters */
    
    movl    %eax,   (%esi)  /* save parameters EAX */
    movl    %ebx,  4(%esi)  /* save parameters EBX */
    movl    %ecx,  8(%esi)  /* save parameters ECX */
    movl    %edx, 12(%esi)  /* save parameters EDX */
                
    /* check for valid cache info */
    
    andl    $VX_CPUID_CACHE_TYPE, %eax
    jz vxCpuProbeCacheEnd
    
    addl   $1,   %edi
    
    /* Only storage for 4 caches are supported */
    cmpl   $4,   %edi
    je  vxCpuProbeCacheEnd
    
    addl   $16,  %esi
    jmp    vxCpuProbeCache

vxCpuProbeCacheEnd:

    /* restore CPUID struct pointer */
    
    movl    8(%ebp), %esi
    
    movl    %edi, VX_CPUID_CACHE_COUNT(%esi)  

    cmpl    $5, VX_CPUID_HIGHVALUE(%esi)    /* is CPUID(5) ok? */
    jl      vxCpuProbeExtended              /* no: extended probe */
        
    /* EAX=5, MONITOR/MWAIT parameters */
        
    /* if monitor/mwait is supported, execute query */
    
    testl   $VX_CPUID_MON, VX_CPUID_FEATURES_ECX(%esi)
    jz  vxCpuProbeSensor
    
    movl    $0x5 , %eax            /* set EAX 0x5 */         
    cpuid                          /* execute CPUID */              
    
    movl    %eax, VX_CPUID_MONITOR_EAX(%esi)  /* save config EAX */
    movl    %ebx, VX_CPUID_MONITOR_EBX(%esi)  /* save config EBX */
    movl    %ecx, VX_CPUID_MONITOR_ECX(%esi)  /* save config ECX */
    movl    %edx, VX_CPUID_MONITOR_EDX(%esi)  /* save config EDX */        

    cmpl    $6, VX_CPUID_HIGHVALUE(%esi)    /* is CPUID(6) ok? */
    jl      vxCpuProbeExtended              /*   no: extended probe */

    /* EAX=6, Digital Thermal Sensor and Power Management parameters */

vxCpuProbeSensor:
            
    movl    $0x6 , %eax            /* set EAX 0x6 */         
    cpuid                          /* execute CPUID */              
    
    movl    %eax, VX_CPUID_DTSPM_EAX(%esi)  /* save config EAX */
    movl    %ebx, VX_CPUID_DTSPM_EBX(%esi)  /* save config EBX */
    movl    %ecx, VX_CPUID_DTSPM_ECX(%esi)  /* save config ECX */
    movl    %edx, VX_CPUID_DTSPM_EDX(%esi)  /* save config EDX */        

    cmpl    $9, VX_CPUID_HIGHVALUE(%esi)    /* is CPUID(9) ok? */
    jl      vxCpuProbeExtended              /* no: extended probe */

    /* EAX=9, Direct Cache Access (DCA) parameters */
            
    movl    $0x9 , %eax            /* set EAX 0x9 */         
    cpuid                          /* execute CPUID */                  
    movl    %eax, VX_CPUID_DCA_EAX(%esi)  /* save config EAX */

    cmpl    $0xa, VX_CPUID_HIGHVALUE(%esi)    /* is CPUID(10) ok? */
    jl      vxCpuProbeExtended                /* no: extended probe */
        
    /* EAX=0xa, Performance Monitor Features */
            
    movl    $0xa , %eax            /* set EAX 0xA */         
    cpuid                          /* execute CPUID */    
                  
    movl    %eax, VX_CPUID_PMON_EAX(%esi)  /* save config EAX */
    movl    %ebx, VX_CPUID_PMON_EBX(%esi)  /* save config EBX */
    movl    %ecx, VX_CPUID_PMON_ECX(%esi)  /* save config ECX */
    movl    %edx, VX_CPUID_PMON_EDX(%esi)  /* save config EDX */        
            
    cmpl    $0xb, VX_CPUID_HIGHVALUE(%esi)    /* is CPUID(11) ok? */
    jl      vxCpuProbeExtended                /* no: extended probe */

    /* EAX=0xb, x2APIC Features / Processor Topology */
    
    movl    $0, %edi                   /* set to zero */
    addl    $VX_CPUID_PTOP_PARAMS,%esi
        
vxCpuProbeTop:    
    
    /* execute until eax = ebx = 0, incrementing ecx */

    movl    $0xb, %eax                   /* set EAX 0xB */
    movl    %edi, %ecx                   /* set ECX   */
    cpuid                                /* execute CPUID */

    /* store features */
    
    movl    %eax,   (%esi)  /* save parameters EAX */
    movl    %ebx,  4(%esi)  /* save parameters EBX */
    movl    %ecx,  8(%esi)  /* save parameters ECX */
    movl    %edx, 12(%esi)  /* save parameters EDX */
                
    /* increment storage pointer */                
    
    addl   $16,  %esi
    
    /* increment ecx counter */
    
    addl   $1,   %edi

    /* Only storage for n cores are supported */
    
    cmpl   $32,   %edi
    je  vxCpuProbeTopEnd
                    
    /* check for exit case */
    
    cmpl  $0,  %eax
    jne vxCpuProbeTop

    cmpl  $0,  %ebx
    jne vxCpuProbeTop
    
vxCpuProbeTopEnd:

    /* restore CPUID struct pointer */
    
    movl    8(%ebp), %esi
    
    movl    %edi, VX_CPUID_PTOP_COUNT(%esi)

    cmpl    $0xd, VX_CPUID_HIGHVALUE(%esi)    /* is CPUID(13) ok? */
    jl      vxCpuProbeExtended                /* no: extended probe */
    
    /* EAX=0xd, XSAVE Features */
            
    movl    $0xd , %eax            /* set EAX 0xD */         
    movl    $0x0 , %ecx            /* set ECX 0x0 */
    cpuid                          /* execute CPUID */                  
    movl    %eax, VX_CPUID_XSAVE_EAX(%esi)  /* save config EAX */
    movl    %ebx, VX_CPUID_XSAVE_EBX(%esi)  /* save config EBX */
    movl    %ecx, VX_CPUID_XSAVE_ECX(%esi)  /* save config ECX */
    movl    %edx, VX_CPUID_XSAVE_EDX(%esi)  /* save config EDX */            
    
vxCpuProbeExtended:
    
    /* EAX=0x80000000, get the highest value and Brand String if supported */
    
    movl    $0x80000000, %eax            /* set EAX 0x80000000 */         
    cpuid                                /* execute CPUID */              
    
    movl    %eax, VX_CPUID_HIGHVALUE_EXT(%esi) /* save high value */
    cmpl    $0x80000001, %eax                  /* is CPUID(0x80000001) ok? */
    jl      vxCpuProbeExit                     /* no: end probe */


        /* EAX=0x8000001, Get extended Features */

    movl    $0x80000001, %eax            /* set EAX 0x80000001 */
    cpuid                                /* execute CPUID */
    
    movl    %edx, VX_CPUID_EXT_FEATURES_EDX(%esi) /* save feature EDX */   
    movl    %ecx, VX_CPUID_EXT_FEATURES_ECX(%esi) /* save feature ECX */   
    cmpl    $0x80000002, VX_CPUID_HIGHVALUE_EXT(%esi) /* is CPUID(0x80000002) ok? */
    jl      vxCpuProbeExit                        /* no: end probe */

    
    /* EAX=0x8000000[234], get the Brand String */

    movl    $0x80000002, %eax            /* set EAX 0x80000002 */
    cpuid                                /* execute CPUID */
    
    movl    %eax, VX_CPUID_BRAND_STR(%esi)   /* save brandStr[0] */
    movl    %ebx, VX_CPUID_BRAND_STR+4(%esi) /* save brandStr[1] */
    movl    %ecx, VX_CPUID_BRAND_STR+8(%esi) /* save brandStr[2] */
    movl    %edx, VX_CPUID_BRAND_STR+12(%esi)/* save brandStr[3] */
    cmpl    $0x80000003, VX_CPUID_HIGHVALUE_EXT(%esi) /* is CPUID(0x80000003) ok? */
    jl      vxCpuProbeExit                   /* no: end probe */

    movl    $0x80000003, %eax            /* set EAX 0x80000003 */
    cpuid                                /* execute CPUID */
    
    movl    %eax, VX_CPUID_BRAND_STR+16(%esi) /* save brandStr[4] */
    movl    %ebx, VX_CPUID_BRAND_STR+20(%esi) /* save brandStr[5] */
    movl    %ecx, VX_CPUID_BRAND_STR+24(%esi) /* save brandStr[6] */
    movl    %edx, VX_CPUID_BRAND_STR+28(%esi) /* save brandStr[7] */
    cmpl    $0x80000004, VX_CPUID_HIGHVALUE_EXT(%esi) /* is CPUID(0x80000004) ok? */
    jl      vxCpuProbeExit                    /* no: end probe */    

    movl    $0x80000004, %eax            /* set EAX 0x80000004 */
    cpuid                                /* execute CPUID */
    
    movl    %eax, VX_CPUID_BRAND_STR+32(%esi) /* save brandStr[8] */
    movl    %ebx, VX_CPUID_BRAND_STR+36(%esi) /* save brandStr[9] */
    movl    %ecx, VX_CPUID_BRAND_STR+40(%esi) /* save brandStr[10] */
    movl    %edx, VX_CPUID_BRAND_STR+44(%esi) /* save brandStr[11] */

    cmpl    $0x80000006, VX_CPUID_HIGHVALUE_EXT(%esi) /* is CPUID(0x80000006) ok? */
    jl      vxCpuProbeExit                    /* no: end probe */    

    /* EAX=0x80000006 L2 Cache Features */
    
    movl    $0x80000006, %eax            /* set EAX 0x80000006 */
    cpuid                                /* execute CPUID */
    
    movl    %eax, VX_CPUID_L2CACHE_EAX(%esi) /* save feature EAX */
    movl    %ebx, VX_CPUID_L2CACHE_EBX(%esi) /* save feature EBX */
    movl    %ecx, VX_CPUID_L2CACHE_ECX(%esi) /* save feature ECX */
    movl    %edx, VX_CPUID_L2CACHE_EDX(%esi) /* save feature EDX */
 
    cmpl    $0x80000007, VX_CPUID_HIGHVALUE_EXT(%esi) /* is CPUID(0x80000007) ok? */
    jl      vxCpuProbeExit                    /* no: end probe */    

    /* EAX=0x80000007 APM Features */
    
    movl    $0x80000007, %eax            /* set EAX 0x80000007 */
    cpuid                                /* execute CPUID */
    
    movl    %eax, VX_CPUID_APM_EAX(%esi) /* save feature EAX */
    movl    %ebx, VX_CPUID_APM_EBX(%esi) /* save feature EBX */
    movl    %ecx, VX_CPUID_APM_ECX(%esi) /* save feature ECX */
    movl    %edx, VX_CPUID_APM_EDX(%esi) /* save feature EDX */

    cmpl    $0x80000008, VX_CPUID_HIGHVALUE_EXT(%esi) /* is CPUID(0x80000008) ok? */
    jl      vxCpuProbeExit                    /* no: end probe */    

    /* EAX=0x80000008 Virt/Phys Adr Sizes */
    
    movl    $0x80000008, %eax            /* set EAX 0x80000008 */
    cpuid                                /* execute CPUID */
    
    movl    %eax, VX_CPUID_VPADRS_EAX(%esi) /* save feature EAX */
    movl    %ebx, VX_CPUID_VPADRS_EBX(%esi) /* save feature EBX */
    movl    %ecx, VX_CPUID_VPADRS_ECX(%esi) /* save feature ECX */
    movl    %edx, VX_CPUID_VPADRS_EDX(%esi) /* save feature EDX */
                            
vxCpuProbeExit:
    popl    %edi
    popl    %esi
    popl    %edx
    popl    %ecx
    popl    %ebx
    popfl                                  /* restore EFLAGS */
    leave
    ret

#else /* _WRS_CONFIG_LP64 */

/*******************************************************************************
*
* vxCpuIdHWMTSupported: indicates if hardware multi-threaded support present
*
* SYNOPSIS
* \ss
* BOOL vxCpuIdHWMTSupported (void)
* \se
*
* This routine indicates the state of hardware multi-threaded support
*
* %eax - hardware multi-threaded support bit #28 status
*
* RETURNS: 0 when the hardware multi-threaded bit is not set
*
*/

FUNC_LABEL(vxCpuIdHWMTSupported)

        push    %rbx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %edx, %eax      /* hardware multi-threaded support bit in edx */
        andl    $VX_CPUID_HWMT_BIT, %eax    /* return VX_CPUID_HWMT_BIT value */

        testl   %eax, %eax
        jz      noHWMT_0  /* special case, no hardware multi-threaded support */

        mov     $0x1, %rax                                     /* return True */

        pop     %rbx                                           /* restore ebx */

        ret

noHWMT_0:

        mov     $0x0, %rax                                    /* return False */

        pop     %rbx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdMaxNumLPPerPkg: returns num logical processor per package
*
* SYNOPSIS
* \ss
* unsigned int vxCpuIdMaxNumLPPerPkg (void)
* \se
*
* This routine returns the number of logical processor per package.
*
* RETURNS: unsigned int, representing the number of logical processor per
*          package
*
*/

FUNC_LABEL(vxCpuIdMaxNumLPPerPkg)

        push    %rbx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %edx, %eax      /* hardware multi-threaded support bit in edx */
        andl    $VX_CPUID_HWMT_BIT, %eax           /* VX_CPUID_HWMT_BIT value */

        testl   %eax, %eax
        jz      noHWMT_1  /* special case, no hardware multi-threaded support */

        movl    %ebx, %eax   /* max num of addressable IDs for LPs in package */
        andl    $VX_CPUID_LOGICAL_BITS, %eax    /* mask VX_CPUID_LOGICAL_BITS */
        rorl    $VX_CPUID_LOGICAL_SHIFT, %eax    /* log processor per package */

        pop     %rbx                                           /* restore ebx */

        ret

noHWMT_1:

        movl    $0x01, %eax       /* return one logical processor per package */

        pop     %rbx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdMaxNumCoresPerPkg: returns max number of cores per package
*
* SYNOPSIS
* \ss
* unsigned int vxCpuIdMaxNumCoresPerPkg (void)
* \se
*
* This routine returns the maximum number of cores per package.
*
* RETURNS: unsigned int, representing the number of cores per package
*
*/

FUNC_LABEL(vxCpuIdMaxNumCoresPerPkg)

        push    %rbx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %edx, %eax      /* hardware multi-threaded support bit in edx */
        andl    $VX_CPUID_HWMT_BIT, %eax           /* VX_CPUID_HWMT_BIT value */

        testl   %eax, %eax
        jz      noHWMT_2  /* special case, no hardware multi-threaded support */

        movl    $VX_CPUID_KEY0, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        cmpl    $VX_CPUID_LEAF_4, %eax           /* does CPUID support leaf 4 */
        jl      noHWMT_2                       /* if not, must be single core */

        movl    $VX_CPUID_LEAF_4, %eax                 /* determine num cores */
        movl    $0, %ecx                 /* start with first level, index = 0 */
        cpuid                                      /* serializing instruction */

                                           /* eax has info on number of cores */
        andl    $VX_CPUID_CORE_BITS, %eax          /* mask VX_CPUID_CORE_BITS */
        rorl    $VX_CPUID_CORE_SHIFT, %eax    /* max number cores per package */
        incl    %eax                                        /* increment by 1 */

        pop     %rbx                                           /* restore ebx */

        ret

noHWMT_2:

        movl    $0x01, %eax              /* must be a single core per package */

        pop     %rbx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdInitialApicId: returns initial Apic Id info for logical processor
*
* SYNOPSIS
* \ss
* unsigned char vxCpuIdInitialApicId (void)
* \se
*
* This routine returns the initial Apic Id info for logical processor.
*
* RETURNS: unsigned char, representing the initial Apic Id info for logical
*          processor
*
*/

FUNC_LABEL(vxCpuIdInitialApicId)

        push    %rbx                       /* save ebx which is used by cpuid */

        movl    $VX_CPUID_KEY1, %eax               /* basic CPUID Information */
        cpuid                                      /* serializing instruction */

        movl    %ebx, %eax      /* initial Apic Id info for logical processor */
        andl    $VX_CPUID_APIC_ID_BITS, %eax 
        rorl    $VX_CPUID_APIC_ID_SHIFT, %eax    /* logical processor Apic Id */

        pop     %rbx                                           /* restore ebx */

        ret

/*******************************************************************************
*
* vxCpuIdBitFieldWidth: general purpose routine to determine the width of a bit
*                       field based on the maximum number of unique identifiers
*                       that bit can represent.
*
* SYNOPSIS
* \ss
* unsigned int vxCpuIdBitFieldWidth (unsigned int count_item)
* \se
*
* count_item passed in register %rdi 
*
* This routine returns the width of a bit field.
*
* RETURNS: unsigned int, representing the width of a bit field
*
*/

FUNC_LABEL(vxCpuIdBitFieldWidth)

        movl %edi,%edx                            /* get count_item passed in */

        movl $0, %ecx
        movl %ecx, %eax                                   /* initialize width */

        decl %edx
        bsr  %dx, %cx
        jz   done

        inc  %cx
        movl %ecx, %eax                                       /* update width */

done:
        ret

/*******************************************************************************
*
* vxCpuIdProbe64 - perform CPUID if supported and populate the VX_CPUID struct
*
* This routine performs 64-bit CPUID and populates the VX_CPUID struct.
*
* RETURNS: a type of CPU FAMILY; 
*          2:P5/Pentium,5:P7/Pentium4,
*          6:Core/Core2, 7:Atom, 8:Nehalem
*
* UINT vxCpuIdProbe64 (Pointer to updated VX_CPUID structure)
*/
        .text
        .balign 16,0x90
FUNC_LABEL(vxCpuIdProbe64)

        pushf                           /* save EFLAGS */
        cli                             /* LOCK INTERRUPT */
        
        /* execute VX_CPUID to get vendor, family, model, stepping, features */

        push    %rbx                    /* save EBX */
        push    %rsi                    /* save ESI */

        /* EAX=0, get the highest value and the vendor ID */

        movl    $0, %eax                /* set EAX 0 */
        cpuid                           /* execute VX_CPUID */

        /* check the vendor ID */

        cmpl    %ebx, VAR(vxVendorIdIntel)+0              /* comp vendor id[0] */
        jne     vxCpuProbe64Exit
        cmpl    %edx, VAR(vxVendorIdIntel)+4              /* comp vendor id[1] */
        jne     vxCpuProbe64Exit
        cmpl    %ecx, VAR(vxVendorIdIntel)+8              /* comp vendor id[2] */
        jne     vxCpuProbe64Exit

        /* Probe additional functions */

        movl    %eax, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE    /* save high value */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_VENDORID     /* save vendor id[0] */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_VENDORID+4   /* save vendor id[1] */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_VENDORID+8   /* save vendor id[2] */
        cmpl    $1, %eax                                  /* is CPUID(1) ok? */
        jl      vxCpuProbe64Extended                      /* no: extended probe */

        /* EAX=1, get the processor signature and feature flags */

        movl    $1, %eax                /* set EAX 1 */
        cpuid                           /* execute VX_CPUID */
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_SIGNATURE    /* save signature */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_FEATURES_EDX /* save feature EDX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_FEATURES_ECX /* save feature ECX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_FEATURES_EBX /* save feature EBX */
        cmpl    $2,   FUNC(vxCpuId)+VX_CPUID_HIGHVALUE    /* is CPUID(2) ok? */
        jl      vxCpuProbe64Extended                      /* no: extended probe */

        /* EAX=2, get the cache descriptors */

        movl    $0, %edi                     /* set to zero */
        movl    $2, %eax                     /* set EAX 2 */
        cpuid                                /* execute CPUID */

        /* The lower 8 bits of EAX contain the value that identifies
           the number of times CPUID must be executed in order to obtain
           a complete image of the processor's caching systems.
         */
         
        movl    %eax, %esi                   /* store counter value */
        cmp     $1, %al                      /* is count > 1?  */
        jle     vxCpuProbeDecode             /* yes, continue CPUID execution */ 
        sub     $1,  %di                     /* already executed once, so decrement count */

    vxCpuProbeRep:
        cpuid                                /* execute CPUID */
        sub      $1,  %di                    /* decrement count */
        test     %di, %di                    /* is count 0? */
        jnz      vxCpuProbeRep               /* no, continue execution until count==0 */

    vxCpuProbeDecode:    
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_CACHE_EAX  /* save config EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_CACHE_EBX  /* save config EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_CACHE_ECX  /* save config ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_CACHE_EDX  /* save config EDX */
        cmpl    $3,   FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(3) ok? */
        jl      vxCpuProbe64Extended                    /* no: extended probe */

         /* EAX=3, get the processor serial no */

        movl    $3, %eax                /* set EAX 3 */
        cpuid                           /* execute VX_CPUID */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_SERIALNO     /* save serialno[2] */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_SERIALNO+4   /* save serialno[3] */
        
        cmpl    $4,   FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(4) ok? */
        jl      vxCpuProbe64Extended                    /* no: extended probe */

        /* EAX=4, get deterministic cache parameters */

        movl    $0, %edi                   /* set to zero */
        mov     $FUNC(vxCpuId)+VX_CPUID_CACHE_PARAMS,%rsi

    vxCpuProbeCache:    

        /* execute until eax[4:0] == 0, incrementing ecx */

        movl    $4,   %eax                   /* set EAX 4 */
        movl    %edi, %ecx                   /* set ECX   */
        cpuid                                /* execute CPUID */

        /* store cache parameters */

        movl    %eax,    (%rsi)   /* save parameters EAX */
        movl    %ebx,   4(%rsi)   /* save parameters EBX */
        movl    %ecx,   8(%rsi)   /* save parameters ECX */
        movl    %edx,  12(%rsi)   /* save parameters EDX */

        /* check for valid cache info */

        andl    $VX_CPUID_CACHE_TYPE, %eax
        jz vxCpuProbeCacheEnd

        addl   $1,   %edi

        /* Only storage for 4 caches are supported */
        cmpl   $4,   %edi
        je  vxCpuProbeCacheEnd

        add    $16,  %rsi
        jmp    vxCpuProbeCache

    vxCpuProbeCacheEnd:

        movl    %edi, FUNC(vxCpuId)+VX_CPUID_CACHE_COUNT

        cmpl    $5,   FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(5) ok? */
        jl      vxCpuProbe64Extended                    /* no: extended probe */

        /* EAX=5, MONITOR/MWAIT parameters */

        /* if monitor/mwait is supported, execute query */

        testl   $VX_CPUID_MON, FUNC(vxCpuId)+VX_CPUID_FEATURES_ECX
        jz  vxCpuProbeSensor

        movl    $0x5 , %eax            /* set EAX 0x5 */         
        cpuid                          /* execute CPUID */              

        movl    %eax, FUNC(vxCpuId)+VX_CPUID_MONITOR_EAX  /* save config EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_MONITOR_EBX  /* save config EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_MONITOR_ECX  /* save config ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_MONITOR_EDX  /* save config EDX */        

        cmpl    $6,   FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(6) ok? */
        jl      vxCpuProbe64Extended                    /* no: extended probe */

        /* EAX=6, Digital Thermal Sensor and Power Management parameters */

    vxCpuProbeSensor:

        movl    $0x6 , %eax            /* set EAX 0x6 */         
        cpuid                          /* execute CPUID */              

        movl    %eax, FUNC(vxCpuId)+VX_CPUID_DTSPM_EAX  /* save config EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_DTSPM_EBX  /* save config EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_DTSPM_ECX  /* save config ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_DTSPM_EDX  /* save config EDX */        

        cmpl    $9,   FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(9) ok? */
        jl      vxCpuProbe64Extended                    /* no: extended probe */

        /* EAX=9, Direct Cache Access (DCA) parameters */

        movl    $0x9 , %eax            /* set EAX 0x9 */         
        cpuid                          /* execute CPUID */                  
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_DCA_EAX  /* save config EAX */

        cmpl    $0xa, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(10) ok? */
        jl      vxCpuProbe64Extended                    /*   no: extended probe */

        /* EAX=0xa, Performance Monitor Features */

        movl    $0xa , %eax            /* set EAX 0xA */         
        cpuid                          /* execute CPUID */                  
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_PMON_EAX  /* save config EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_PMON_EBX  /* save config EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_PMON_ECX  /* save config ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_PMON_EDX  /* save config EDX */        

        cmpl    $0xb, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(11) ok? */
        jl      vxCpuProbe64Extended                    /* no: extended probe */

        /* EAX=0xb, x2APIC Features / Processor Topology */

        movl    $0, %edi                   /* set to zero */
        mov     $FUNC(vxCpuId)+VX_CPUID_PTOP_PARAMS,%rsi

    vxCpuProbeTop:    

        /* execute until eax = ebx = 0, incrementing ecx */

        movl    $0xb, %eax                   /* set EAX 0xB */
        movl    %edi, %ecx                   /* set ECX   */
        cpuid                                /* execute CPUID */

        /* store features */

        movl    %eax,   (%rsi)  /* save parameters EAX */
        movl    %ebx,  4(%rsi)  /* save parameters EBX */
        movl    %ecx,  8(%rsi)  /* save parameters ECX */
        movl    %edx, 12(%rsi)  /* save parameters EDX */

        /* increment storage pointer */                

        add    $16,  %rsi

        /* increment ecx counter */

        addl   $1,   %edi

        /* Only storage for n cores are supported */

        cmpl   $64,   %edi
        je  vxCpuProbeTopEnd

        /* check for exit case */

        cmpl  $0,  %eax
        jne vxCpuProbeTop

        cmpl  $0,  %ebx
        jne vxCpuProbeTop

    vxCpuProbeTopEnd:

        movl    %edi, FUNC(vxCpuId)+VX_CPUID_PTOP_COUNT

        cmpl    $0xd, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE  /* is CPUID(13) ok? */
        jl      vxCpuProbe64Extended                    /* no: extended probe */

        /* EAX=0xd, XSAVE Features */

        movl    $0xd , %eax            /* set EAX 0xD */         
        movl    $0x0 , %ecx            /* set ECX 0x0 */
        cpuid                          /* execute CPUID */                  
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_XSAVE_EAX  /* save config EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_XSAVE_EBX  /* save config EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_XSAVE_ECX  /* save config ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_XSAVE_EDX  /* save config EDX */            

    vxCpuProbe64Extended:
    
        /* EAX=0x80000000, to see if the Brand String is supported */

        movl    $0x80000000, %eax       /* set EAX 0x80000000 */
        cpuid                           /* execute VX_CPUID */
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT     /* save high value */
        
        cmpl    $0x80000001, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT  /* is CPUID(0x80000001) ok? */
        jl      vxCpuProbe64Exit                               /*  no: end probe */

        /* EAX=0x8000001, Get extended Features */

        movl    $0x80000001, %eax            /* set EAX 0x80000001 */
        cpuid                                /* execute CPUID */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_EXT_FEATURES_EDX /* save feature EDX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_EXT_FEATURES_ECX /* save feature ECX */

        cmpl    $0x80000002, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT  /* is CPUID(0x80000002) ok? */
        jl      vxCpuProbe64Exit                               /* no: end probe */
        
        /* EAX=0x8000000[234], get the Brand String */

        movl    $0x80000002, %eax       /* set EAX 0x80000002 */
        cpuid                           /* execute VX_CPUID */
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_BRAND_STR    /* save brandStr[0] */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+4  /* save brandStr[1] */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+8  /* save brandStr[2] */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+12 /* save brandStr[3] */

        cmpl    $0x80000003, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT  /* is CPUID(0x80000003) ok? */
        jl      vxCpuProbe64Exit                               /* no: end probe */

        movl    $0x80000003, %eax       /* set EAX 0x80000003 */
        cpuid                           /* execute VX_CPUID */
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+16 /* save brandStr[4] */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+20 /* save brandStr[5] */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+24 /* save brandStr[6] */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+28 /* save brandStr[7] */

        cmpl    $0x80000004, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT  /* is CPUID(0x80000004) ok? */
        jl      vxCpuProbe64Exit                               /* no: end probe */

        movl    $0x80000004, %eax       /* set EAX 0x80000004 */
        cpuid                           /* execute VX_CPUID */
        movl    %eax, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+32 /* save brandStr[8] */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+36 /* save brandStr[9] */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+40 /* save brandStr[10] */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_BRAND_STR+44 /* save brandStr[11] */

        cmpl    $0x80000006, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT /* is CPUID(0x80000006) ok? */
        jl      vxCpuProbe64Exit                    /* no: end probe */    


        /* EAX=0x80000006 L2 Cache Features */

        movl    $0x80000006, %eax            /* set EAX 0x80000006 */
        cpuid                                /* execute CPUID */

        movl    %eax, FUNC(vxCpuId)+VX_CPUID_L2CACHE_EAX /* save feature EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_L2CACHE_EBX /* save feature EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_L2CACHE_ECX /* save feature ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_L2CACHE_EDX /* save feature EDX */

        cmpl    $0x80000007, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT /* is CPUID(0x80000007) ok? */
        jl      vxCpuProbe64Exit                    /* no: end probe */    

        /* EAX=0x80000007 APM Features */

        movl    $0x80000007, %eax            /* set EAX 0x80000007 */
        cpuid                                /* execute CPUID */

        movl    %eax, FUNC(vxCpuId)+VX_CPUID_APM_EAX /* save feature EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_APM_EBX /* save feature EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_APM_ECX /* save feature ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_APM_EDX /* save feature EDX */

        cmpl    $0x80000008, FUNC(vxCpuId)+VX_CPUID_HIGHVALUE_EXT /* is CPUID(0x80000008) ok? */
        jl      vxCpuProbe64Exit                    /* no: end probe */    

        /* EAX=0x80000008 Virt/Phys Adr Sizes */

        movl    $0x80000008, %eax            /* set EAX 0x80000008 */
        cpuid                                /* execute CPUID */

        movl    %eax, FUNC(vxCpuId)+VX_CPUID_VPADRS_EAX /* save feature EAX */
        movl    %ebx, FUNC(vxCpuId)+VX_CPUID_VPADRS_EBX /* save feature EBX */
        movl    %ecx, FUNC(vxCpuId)+VX_CPUID_VPADRS_ECX /* save feature ECX */
        movl    %edx, FUNC(vxCpuId)+VX_CPUID_VPADRS_EDX /* save feature EDX */

vxCpuProbe64Exit:
        pop     %rsi                    /* restore ESI */
        pop     %rbx                    /* restore EBX */
        popf                            /* restore EFLAGS */
        ret

#endif /* !_WRS_CONFIG_LP64 */
