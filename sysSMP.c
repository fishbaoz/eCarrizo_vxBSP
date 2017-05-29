/* sysSMP.c - SMP and multicore CPU utility routines */

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
01d,24may13,jjk  WIND00364942 - Removing AMP and GUEST
01c,09may13,jjk  WIND00364942 - Adding Unified BSP.
01b,25jun12,j_z  sync with itl_nehalem bsp 02o version sysLib.c
01a,25mar11,lll  unification of itl BSPs
*/

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9))
/*******************************************************************************
*
* vxCpuPhysIndexGet - get the physical CPU index.
* Note: this is a dummy routine for version before vx6.9, the physical cpu
* index always equal to logical index since there's no SMT support.
*
* RETURNS:
* Returns the Physical CPU Index for the CPU on which vxCpuPhysIndexGet()
* is executed.
*
*/

unsigned int vxCpuPhysIndexGet
    (
    void
    )
    {
    return vxCpuIndexGet();
    }


#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9) */


/******************************************************************************
*
* sysCpuAvailableGet - return the number of CPUs that are to
*                      be used by vxWorks for the SMP kernel.
*
* RETURNS: The number for the CPUs available
*
*/

UINT32 sysCpuAvailableGet(void)
    {
    /* enforce arch limitation  for SMP configurable cores */

    return ((UINT32) (vxCpuCount));
    }

/*******************************************************************************
*
* vxCpuStateInit - Initialize the state to start a CPU in.
*
* The purpose of this function is somewhat similar to the arch-specific
* function taskRegsInit(), except that this function initializes a
* WIND_CPU_STATE structure suitable for the initial state of a processor,
* whereas taskRegsInit only needs to initialize a REG_SET structure
* for the initial state of a vxWorks task.
*
* RETURNS: OK or ERROR if any failure.
*
*/

STATUS vxCpuStateInit
    (
    unsigned int cpu,
    WIND_CPU_STATE *cpuState,
    char *stackBase,
    FUNCPTR entry
    )
    {
#ifdef _WRS_CONFIG_SMP
    TSS *newTss;
    GDT *newGdt;

    GDTPTR gDtp;
    INT32 nGdtEntries, nGdtBytes;
#endif /* _WRS_CONFIG_SMP */

    if ((cpu < 1) || (cpu > (sysCpuAvailableGet() - 1)))
        return (ERROR);

    if ((cpuState == NULL) || (entry== NULL) || (stackBase == NULL))
        return (ERROR);

    bzero ((char *)cpuState, sizeof(WIND_CPU_STATE));

#ifdef _WRS_CONFIG_SMP
        {
        /*
         * establish pointers to global descriptor table & task state segment
         * TSS is assigned a cpu unique structure...
         */

        /* copy the global descriptor table from RAM/ROM to RAM */

        /* Get the Global Data Table Descriptor */

        vxGdtrGet ( (long long int *)&gDtp );

        /* Extract the number of bytes */

        nGdtBytes = (INT32)gDtp.wGdtr[0];

        /* and calculate the number of entries */

        nGdtEntries = (nGdtBytes + 1 ) / sizeof(GDT);

        newGdt = (GDT *) malloc (nGdtEntries * sizeof (GDT));

        if (newGdt == NULL)
            return (ERROR);

        bcopy ((char *)(&sysGdt), (char *)newGdt,
                (int)(nGdtEntries * sizeof (GDT)));

        _WRS_WIND_VARS_ARCH_SET(cpu,
                                pSysGdt,
                                newGdt);

        newTss = (TSS *) malloc (sizeof (TSS));

        if (newTss == NULL)
            return (ERROR);

        bcopy ((char *)(&sysTss), (char *)newTss, (int)sizeof (TSS));

        /*
         * Update vxKernelVars
         */

        _WRS_WIND_VARS_ARCH_SET(cpu,
                                sysTssCurrent,
                                newTss);

        _WRS_WIND_VARS_ARCH_SET(cpu,
                                vxIntStackEnabled,
                                1);
        }
#endif /*  _WRS_CONFIG_SMP */

    cpuState->regs.eflags = EFLAGS_BRANDNEW;
    cpuState->regs.esp    = (ULONG) stackBase;
    cpuState->regs.pc     = (INSTR *)entry;

    return (OK);
    }

/***************************************************************************
*
* sysCpuStart - start a CPU
*
* The sysCpuStart() function takes two parameters:
* int cpu;                  /@ core number to start @/
* WIND_CPU_STATE *cpuState; /@ pointer to a WIND_CPU_STATE structure @/
*
* The intent is to implement a function with the basic features of
* vxTaskRegsInit() that sets up the regs structure, then passes the
* cpu number and cpuState pointer to this function, which in turn extracts
* the needed values from the cpuState structure and starts the processor
* running...
*
* RETURNS:  0K - Success
*           ERROR - Fail
*
* \NOMANUAL
*/

STATUS sysCpuStart
    (
    int cpu,
    WIND_CPU_STATE *cpuState
    )
    {

    STATUS status = OK;          /* return value */
    INT32  ix;
    INT32  oldLevel;
    UINT32 oldResetVector;       /* warm reset vector */
    UINT8  oldShutdownCode;      /* CMOS shutdown code */
    unsigned int *tmp_stk;       /* temp stk to pass some env params */
    unsigned int *tmpPtr;        /* temp pointer */
    FUNCPTR * baseAddr;          /* new vector base address */
    UINT8 *mploApicIndexTable;
    char * textLoc;

#if defined(_WRS_CONFIG_SMP)
    UINT apDelayCount = SYS_AP_LOOP_COUNT; /* Times to check for AP startup */
    UINT apDelay = SYS_AP_TIMEOUT;         /* Time between AP startup checks */
#endif /* defined(_WRS_CONFIG_SMP) */

    /* entry point of AP trampoline code */

    UINT32 entryPoint = (UINT32) CPU_ENTRY_POINT;
    UINT32 scratchPoint = (UINT32) CPU_SCRATCH_POINT;
    UINT32 scratchMem = (UINT32) scratchPoint;

    (*getMpApicLoIndexTable)(mpApicDevID, (void *)&mploApicIndexTable);

    textLoc = (char *)&sysInitCpuStartup;

    /*
     * Copy tramp code to low memory. Must be reachable by 16 bit code.
     */

    bcopy (textLoc, (char *)entryPoint, (int)CPU_AP_AREA);

    /*
     * Initialization of Temporary AP Scratch Memory:
     *
     *    scratchMem (scratch memory offset)  scratchPoint
     *
     *    Standard GDT Entries:
     *
     *    null descriptor                     scratchMem + 0x04
     *
     *    kernel text descriptor              scratchMem + 0x0C
     *
     *    kernel data descriptor              scratchMem + 0x14
     *
     *    exception text descriptor           scratchMem + 0x1C
     *
     *    interrupt data descriptor           scratchMem + 0x24
     *
     *    gdt limit << 16                     scratchMem + LIM_GDT_OFFSET
     *    gdt base                            scratchMem + BASE_GDT_OFFSET
     *    address of page directory           scratchMem + CR3_OFFSET
     *
     *    idt limit                           scratchMem + LIM_IDT_OFFSET
     *    idt base address                    scratchMem + BASE_IDT_OFFSET
     *
     *    initial AP stack addr               scratchMem + AP_STK_OFFSET
     */

    /* Setup Initial AP... */

    /*
     * scratchPoint saves location of starting point for AP scratch memory,
     * APs offset based on this value for unique memory space...
     * if AP #1, this location doubles as first valid location of AP
     * scratch memory...
     */

    *((int *)(scratchPoint)) = (unsigned int)scratchMem;

    /*
     * first valid location of scratch memory,
     * setup offset for AP specific scratch mem
     */

    tmp_stk = (unsigned int *)(scratchMem);
    tmp_stk[0] = (unsigned int)scratchMem;

    /* setup intial GDT values */

    tmpPtr = (unsigned int *) &sysGdt;

    /* place tmp_gdt in memory */

    for (ix = 0; ix < NUM_GDT_ENTRIES; ix++)
        tmp_stk[ix+1] = *tmpPtr++;

    *((int *)(scratchMem + LIM_GDT_OFFSET)) = (int)0x270000;
    *((int *)(scratchMem + BASE_GDT_OFFSET)) = (int)(scratchMem + 0x04);
        {

        /* setup initial cr3 */

        *((unsigned int *)(scratchMem + CR3_OFFSET)) = vxCr3Get();

        /* setup initial IDT values */

        baseAddr = intVecBaseGet();
        }

    /* (IDT limit) << 16 */

    *((int *)(scratchMem + LIM_IDT_OFFSET)) = (int)0x07ff0000;

    /* IDT base address */

    *((int *)(scratchMem + BASE_IDT_OFFSET)) = (int)baseAddr;

    /* initial AP Stack Address */

    *((int *)(scratchMem + AP_STK_OFFSET)) = (int)cpuState->regs.esp;

    oldLevel = intCpuLock();               /* LOCK INTERRUPTS */

    /*
     * allow access to page 0, for access to trampoline region
     * default state (VM_STATE_VALID | VM_STATE_WRITABLE | VM_STATE_CACHEABLE)
     */

    vmMap (0, (VIRT_ADDR)0, (PHYS_ADDR)0, VM_PAGE_SIZE);

    /* set the AP entry point address in WARM_REST_VECTOR */

    oldResetVector = *(volatile UINT32 *)WARM_RESET_VECTOR;

    /* selects Shutdown Status Register */

    sysOutByte (RTC_INDEX, BIOS_SHUTDOWN_STATUS);

    /* get BIOS Shutdown code */

    oldShutdownCode = sysInByte (RTC_DATA);

    *(volatile unsigned short *)WARM_RESET_VECTOR = 0;
    *(volatile unsigned short *)(WARM_RESET_VECTOR + 2) = (entryPoint >> 4);

    /* initialze the BIOS shutdown code to be 0xA */

    /* selects Shutdown Status Register */

    sysOutByte (RTC_INDEX, BIOS_SHUTDOWN_STATUS);

    /* set BIOS Shutdown code to 0x0A */

    sysOutByte (RTC_DATA, 0xA);

    intCpuUnlock(oldLevel);                 /* UNLOCK INTERRUPTS */

#ifdef _WRS_VX_IA_IPI_INSTRUMENTATION
    /* Zero IPI instrumentation for this CPU */

    bzero((char *)ipiCounters[cpu], sizeof(struct ipiCounters));
#endif /* _WRS_VX_IA_IPI_INSTRUMENTATION */

    cacheI86Flush();

    /* BSP sends AP an INIT-IPI and STARTUP-IPI */

    if (ipiStartup ((UINT32) mploApicIndexTable[cpu],
                    (UINT32) entryPoint,
                    (UINT32) 2) != OK)
        {
        printf ("\nipiStartup failed: %d\n", mploApicIndexTable[cpu]);
        status = ERROR;
        }

    /*
     * Wait for AP start up.
     * We do not always have to delay. When a processor is enabled it is
     * visible via vxCpuEnabled.
     */

    do
        {
        if ((1 << cpu) & vxCpuEnabled)
            break;
        else
            sysUsDelay (apDelay);

        } while (--apDelayCount > 0);

    /*
     * Set ERROR when AP did not start in time.
     * tRootTask will log CPU startup error to the console device.
     */

    if (((1 << cpu) & vxCpuEnabled) == 0)
        status = ERROR;

    oldLevel = intCpuLock();                /* LOCK INTERRUPTS */

    /* restore the WARM_REST_VECTOR and the BIOS shutdown code */

    *(volatile UINT32 *)WARM_RESET_VECTOR = oldResetVector;
    sysOutByte (RTC_INDEX, BIOS_SHUTDOWN_STATUS); /* Shutdown Status Reg */
    sysOutByte (RTC_DATA, oldShutdownCode); /* set BIOS Shutdown code */

    cacheI86Flush();

    intCpuUnlock(oldLevel);                 /* UNLOCK INTERRUPTS */

    /* unmap the page zero for NULL pointer detection */

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8))
    vmPageUnmap((VM_CONTEXT_ID)0, (VIRT_ADDR)0, (size_t)VM_PAGE_SIZE);
#else
    vmStateSet (
        (VIRT_ADDR) 0x0,
        (PHYS_ADDR) 0x0,
        VM_PAGE_SIZE,
        (VM_STATE_MASK_VALID | VM_STATE_MASK_WRITABLE | VM_STATE_MASK_CACHEABLE)
        ,(VM_STATE_VALID_NOT | VM_STATE_WRITABLE_NOT | VM_STATE_CACHEABLE_NOT));
#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8) */

    return (status);
    }

/*******************************************************************************
*
* sysCpuInit - AP CPU Initialization.
*
* This routine is the first code executed on a secondary processor after
* startup. It is responsible for ensuring that the registers are set
* appropriately.
*
* In a mapped kernel, it is also responsible for setting up the core's MMU.
*
* Upon completion of these tasks, the core is ready to begin accepting
* kernel work. The address of the initial kernel code (typically
* windCpuEntry) has been placed in sysCpuInitTable[coreNum].
*
* RETURNS: NONE
*/

void sysCpuInit (void)
    {
    unsigned int a = 0;
    int cpuNum = 0;
#ifdef _WRS_CONFIG_SMP
    FUNCPTR entry;
    int dummy;
    STATUS retVal = OK;

    sysInitGDT ();

    /* enable the MTRR (Memory Type Range Registers) */

    if ((sysCpuId.featuresEdx & CPUID_MTRR) == CPUID_MTRR)
        {
        pentiumMtrrDisable ();              /* disable MTRR */
#ifdef INCLUDE_MTRR_GET
        (void) pentiumMtrrGet (&sysMtrr);   /* get MTRR initialized by BIOS */
#else
        (void) pentiumMtrrSet (&sysMtrr);   /* set your own MTRR */
#endif  /* INCLUDE_MTRR_GET */
        pentiumMtrrEnable ();               /* enable MTRR */
        }

#if ((_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8))
    fppArchCpuInit();                       /* enable FP features */
#endif /* (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR > 8) */

#ifdef INCLUDE_PMC

    /* enable PMC (Performance Monitoring Counters) */

    pentiumPmcStop ();                      /* stop PMC0 and PMC1 */
    pentiumPmcReset ();                     /* reset PMC0 and PMC1 */
#endif /* INCLUDE_PMC */

    /* enable the MCA (Machine Check Architecture) */

    pentiumMcaEnable (TRUE);

    (*getLoApicIntrInitAP)(loApicDevID, (void *) &dummy);

    cpuNum = vxCpuPhysIndexGet();

    entry = sysCpuInitTable[cpuNum];

    /*
     * update GDT. LDT, ect. depending on configuration.
     *
     * Note: order here is important, GDT updates for RTPs
     *       must come before OSM support, if included.
     */

#if defined (INCLUDE_RTP)
    retVal = syscallArchInit ();
#endif  /* INCLUDE_RTP */

    if (retVal == OK)
        {
#if defined INCLUDE_PROTECT_TASK_STACK ||       \
    defined INCLUDE_PROTECT_INTERRUPT_STACK
#if defined(_WRS_OSM_INIT)
#if defined INCLUDE_PROTECT_TASK_STACK
        retVal = excOsmInit (TASK_USER_EXC_STACK_OVERFLOW_SIZE, VM_PAGE_SIZE);
#else   /* INCLUDE_PROTECT_TASK_STACK */
        retVal = excOsmInit (0, VM_PAGE_SIZE);
#endif  /* INCLUDE_PROTECT_TASK_STACK */
#endif  /* defined(_WRS_OSM_INIT) */
#endif  /* INCLUDE_PROTECT_TASK_STACK || INCLUDE_PROTECT_INTERRUPT_STACK */

        if ((entry != NULL) && (retVal == OK))
            {
            entry ();

            /* should be no return */

            }
        }
#endif /* _WRS_CONFIG_SMP */

    for (;;)
        {
        if (!(++a % 0x10000))
            {
            sysCpuLoopCount[cpuNum]++;
            }
        }
    }

/*******************************************************************************
*
* sysCpuEnable - enable a multi core CPU
*
* This routine brings a CPU out of reset
*
* RETURNS:  OK - Success
*           ERROR - Fail
*
*/

STATUS sysCpuEnable
    (
    unsigned int cpuNum,
    WIND_CPU_STATE *cpuState
    )
    {
    if ((cpuNum < 1) || (cpuNum > (sysCpuAvailableGet() - 1)))
        {
        return (ERROR);
        }

#ifdef _WRS_CONFIG_SMP
        sysCpuInitTable[cpuNum] = (FUNCPTR) cpuState->regs.pc;

#endif /* _WRS_CONFIG_SMP */

    return (sysCpuStart(cpuNum, cpuState));
    }

/******************************************************************************
*
* sysCpuRe_Enable - re-enable a multi core CPU
*
* This routine brings an AP CPU through a cpu reset cycle and back on line.
*
* RETURNS:  OK - Success
*           ERROR - Fail
*
*/

STATUS sysCpuRe_Enable
    (
    unsigned int cpuNum
    )
    {
    STATUS status = OK;

    if ((cpuNum < 1) || (cpuNum > (sysCpuAvailableGet() - 1)))
        {
        return (ERROR);
        }

#ifdef  _WRS_CONFIG_SMP
    status = sysCpuStop ((int)cpuNum);
    status = sysCpuReset ((int)cpuNum);
    status = kernelCpuEnable (cpuNum);
#endif /* _WRS_CONFIG_SMP */

    return (status);
    }

/*******************************************************************************
*
* sysCpuDisable - disable a multi core CPU
*
* This routine shuts down the specified core.
*
* RETURNS:  OK - Success
*           ERROR - Fail
*
*/

STATUS sysCpuDisable
    (
    int cpuNum
    )
    {
    if ((cpuNum < 1) || (cpuNum > (sysCpuAvailableGet() - 1)))
        {
        return (ERROR);
        }

    return (sysCpuStop(cpuNum));
    }

/***************************************************************************
*
* sysCpuStop - stop a CPU, for now, it is no longer available
*              for use
*
* The sysCpuStop() function takes one parameter:
* int cpu;              /@ core number to stop @/
*
* RETURNS: OK
*
* \NOMANUAL
*/

STATUS sysCpuStop
    (
    int cpu
    )
    {
    STATUS status = OK;
#ifdef _WRS_CONFIG_SMP
    int key;   /* prevent task migration */
    UINT8 *mploApicIndexTable;

    taskCpuLock();          /* if not in interrupt context, taskCpuLock */

    key = intCpuLock ();    /* prevent task migration */

    (*getMpApicLoIndexTable)(mpApicDevID, (void *)&mploApicIndexTable);

    /* BSP sends AP the shutdown IPI */

    CPUSET_ATOMICCLR (vxCpuEnabled, CPU_PHYS_TO_LOGICAL(cpu));
    status = ipiShutdown ((UINT32) mploApicIndexTable[cpu],
                          INT_NUM_LOAPIC_IPI + 0);

    intCpuUnlock (key);

    taskCpuUnlock();     /* if not in interrupt context, taskCpuUnlock */
#endif /* _WRS_CONFIG_SMP */

    return (status);
    }

/***************************************************************************
*
* sysCpuStop_APs - Stop all Application Processors (APs)
*
* Can only run this routine from boot strap processor (BSP)
*
* RETURNS:  OK - Success
*           ERROR - Fail
*
* \NOMANUAL
*/

STATUS sysCpuStop_APs (void)
    {
    STATUS status = OK;
#ifdef  _WRS_CONFIG_SMP
    int i;
    int key;                /* prevent task migration */
    UINT8 *mploApicIndexTable;
    int numCpus = vxCpuConfiguredGet();

    taskCpuLock();          /* if not in interrupt context, taskCpuLock */

    key = intCpuLock ();    /* prevent task migration */

    (*getMpApicLoIndexTable)(mpApicDevID, (void *)&mploApicIndexTable);

    /* Skip BSP, i == 0 */

    for (i=(vxCpuPhysIndexGet()+1); i < numCpus; i++)
        {
        /* BSP sends AP the shutdown IPI */

        CPUSET_ATOMICCLR (vxCpuEnabled, CPU_PHYS_TO_LOGICAL(i));
        status = ipiShutdown ((UINT32) mploApicIndexTable[i],
                              INT_NUM_LOAPIC_IPI + 0);
        }

    intCpuUnlock (key);

    taskCpuUnlock();     /* if not in interrupt context, taskCpuUnlock */
#endif /* _WRS_CONFIG_SMP */

    return (status);
}

/***************************************************************************
*
* sysCpuStop_ABM - Stop all APs but the one I'm running on, (All but me...)
*
* RETURNS:  OK - Success
*           ERROR - Fail
*
* \NOMANUAL
*/

STATUS sysCpuStop_ABM (void)
    {
    STATUS status = OK;
#ifdef  _WRS_CONFIG_SMP
    int i, skipProc;
    int key;   /* prevent task migration */
    UINT8 *mploApicIndexTable;
    int numCpus = vxCpuConfiguredGet();

    taskCpuLock();       /* if not in interrupt context, taskCpuLock */

    key = intCpuLock ();   /* prevent task migration */

    (*getMpApicLoIndexTable)(mpApicDevID, (void *)&mploApicIndexTable);

    skipProc = vxCpuPhysIndexGet ();

    for (i=1; i < numCpus; i++)
        {
        if (i != skipProc)
            {
            /* sends AP the shutdown IPI */

            CPUSET_ATOMICCLR (vxCpuEnabled, CPU_PHYS_TO_LOGICAL(i));
            status = ipiShutdown ((UINT32) mploApicIndexTable[i],
                                  INT_NUM_LOAPIC_IPI + 0);
            }
        }

    intCpuUnlock (key);

    taskCpuUnlock();     /* if not in interrupt context, taskCpuUnlock */
#endif /* _WRS_CONFIG_SMP */

    return (status);
    }

/***************************************************************************
*
* sysCpuReset - Reset a CPU
*
* Places the specified Application Processor (AP) into the
* INIT reset state, i.e. wait-for-SIPI state.
*
* The sysCpuReset() function takes one parameter:
* int cpu;              /@ core number to stop @/
*
* RETURNS: OK
*
* \NOMANUAL
*/

STATUS sysCpuReset
    (
    int cpu
    )
    {
    STATUS status = OK;
    int key;                /* prevent task migration */
    UINT8 *mploApicIndexTable;

    taskCpuLock();          /* if not in interrupt context, taskCpuLock */

    key = intCpuLock ();    /* prevent task migration */

    (*getMpApicLoIndexTable)(mpApicDevID, (void *)&mploApicIndexTable);

    /* sends AP the INIT IPI */

    CPUSET_ATOMICCLR (vxCpuEnabled, cpu);
    status = ipiReset ((UINT32) mploApicIndexTable[cpu]);

    intCpuUnlock (key);

    taskCpuUnlock();     /* if not in interrupt context, taskCpuUnlock */

    return (status);
    }

/***************************************************************************
*
* sysCpuReset_APs - Reset all Application Processors (APs)
*
* Can only run this routine from BSP.
*
* RETURNS:  OK - Success
*           ERROR - Fail
*
* \NOMANUAL
*/

STATUS sysCpuReset_APs (void)
    {
    STATUS status = OK;
    int i, pCpu;
    int key;   /* prevent task migration */
    UINT8 *mploApicIndexTable;
    int numCpus = vxCpuConfiguredGet();

    pCpu = vxCpuPhysIndexGet();

    taskCpuLock();       /* if not in interrupt context, taskCpuLock */

    key = intCpuLock ();   /* prevent task migration */

    (*getMpApicLoIndexTable)(mpApicDevID, (void *)&mploApicIndexTable);

    for (i = 1; i < numCpus; i++)
        {
        /* BSP sends AP the INIT IPI */

        CPUSET_ATOMICCLR (vxCpuEnabled, CPU_PHYS_TO_LOGICAL(pCpu + i));
        status = ipiReset ((UINT32) mploApicIndexTable[pCpu + i]);
        }

    intCpuUnlock (key);

    taskCpuUnlock();     /* if not in interrupt context, taskCpuUnlock */

    return (status);
    }

/***************************************************************************
*
* sysCpuReset_ABM - Reset all APs but the one I'm running on, (All but me...)
*
* RETURNS:  OK - Success
*           ERROR - Fail
*
* \NOMANUAL
*/

STATUS sysCpuReset_ABM (void)
    {
    STATUS status = OK;
    int i, skipProc;
    int key;   /* prevent task migration */
    UINT8 *mploApicIndexTable;
#ifdef _WRS_CONFIG_SMP
    int numCpus = vxCpuConfiguredGet();
#else
    int numCpus = vxCpuCount;
#endif

    taskCpuLock();          /* if not in interrupt context, taskCpuLock */

    key = intCpuLock ();    /* prevent task migration */

    (*getMpApicLoIndexTable)(mpApicDevID, (void *)&mploApicIndexTable);

    skipProc = vxCpuPhysIndexGet ();

    for (i = 1; i < numCpus; i++)
        {
        if (i != skipProc)
            {
            /* sends AP the INIT IPI */

            CPUSET_ATOMICCLR (vxCpuEnabled, CPU_PHYS_TO_LOGICAL(i));
            status = ipiReset ((UINT32) mploApicIndexTable[i]);
            }
        }

    intCpuUnlock (key);

    taskCpuUnlock();     /* if not in interrupt context, taskCpuUnlock */

    return (status);
    }

