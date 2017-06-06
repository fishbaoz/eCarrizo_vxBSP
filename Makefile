# Makefile - makefile skeleton for target/config/itl_<BOARD>
#
# Copyright (c) 2009, 2011, 2013 Wind River Systems, Inc.
#
# modification history
# --------------------
# 01e,16may13,jjk  WIND00364942 - Swap RAM_HIGH and RAM_LOW to conform to IA
#                  arch in 6.9
# 01d,09may13,jjk  WIND00364942 - Adding Unified BSP.
# 01c,05jul11,j_z  add multi-stage boot support, code clean.
# 01b,25mar11,lll  unification of itl BSPs
# 01a,24jul09,scm  written based on pcPentium4/Makefile.
#

CPU     = PENTIUM4
TOOL    = gnu

TGT_DIR = $(WIND_BASE)/target

include $(TGT_DIR)/h/make/defs.bsp

# Only redefine make definitions below this point, or your definitions will
# be overwritten by the makefile stubs above.

TARGET_DIR = amd_ekabini_dbft3
VENDOR     = AMD
BOARD      = x86

#
# The constants ROM_TEXT_ADRS, ROM_SIZE, VXWORKS_ENTRY_ADRS and
# BOOTAPP_ENTRY_ADRS are defined in config.h and Makefile.
# All definitions for these constants must be identical.
#
     
ROM_TEXT_ADRS       = 00008000               # real mode image entry
FIRST_TEXT_ADRS     = 00008000

#
# Only support bootrom_uncmp boot images. It will always require
# more space than an compressed image.
#

ROM_SIZE            = 00800000               # max size of bootrom_uncmp[.bin]

RAM_LOW_ADRS        = 00408000               # VxWorks image entry point
RAM_HIGH_ADRS       = 10008000               # Boot image entry point

EXTRA_DEFINE       += -fno-zero-initialized-in-bss

MACH_EXTRA          = vxCpuIdALib.o vxbIoApicIntrA.o rtl8169VxbEndA.o 

#
# Usually RELEASE has the following targets to build 
# vxWorks vxWorks.st bootrom.hex prj_gnu prj_diab prj_icc 
# Add un-compressed image as the default bootrom target.
#

RELEASE            += bootrom_uncmp

######################################################################
# Multi-stage boot overcomes the bootrom.bin too large issue.
# usage: make vxStage1Boot.bin
#        make bootrom_uncmp.bin
# Make a bootable disk and copy vxStage1Boot.bin to disk renamed as
# bootrom.sys. Then copy bootrom_uncmp.bin to disk renamed as bootapp.sys.
# This version only supports cold reboot, which means the image will
# jump to boot stub code and reload the image from disk.
######################################################################

FIRST_ENTRY         = firstBoot
FIRST_OBJ           = vxStage1Boot.o
FIRST_HIGH_FLAGS    = -Ttext $(FIRST_TEXT_ADRS)
LD_FIRST_CMP_FLAGS  = $(ROM_LDFLAGS) $(RES_LDFLAGS) $(FIRST_HIGH_FLAGS)

%.pxe:  $(BSP_DIR)/pxeBoot.bin %.bin
	cat $+ > $@

bootrom.pxe: bootrom.bin pxeBoot.bin
	cat pxeBoot.bin bootrom.bin > bootrom.pxe

# These are for multi-stage boot:

$(FIRST_OBJ)     : vxStage1Boot.s $(CFG_GEN_FILES) ipcom_ipdomain$(OPT)
vxStage1Boot.bin : vxStage1Boot
vxStage1Boot: $(FIRST_OBJ)
	$(LD) $(LDFLAGS) $(RES_LDFLAGS) $(LD_ENTRY_OPT) $(FIRST_ENTRY) \
		$(LD_FIRST_CMP_FLAGS) -defsym _VX_DATA_ALIGN=1 $(FIRST_OBJ) \
		$(LD_SCRIPT_RAM) -o $@

# Only redefine make definitions above this point, or the expansion of
# makefile target dependencies may be incorrect.

include $(TGT_DIR)/h/make/rules.bsp
