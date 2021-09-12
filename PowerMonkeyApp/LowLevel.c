/*******************************************************************************
*  ______                            ______                 _
* (_____ \                          |  ___ \               | |
*  _____) )___   _ _ _   ____   ___ | | _ | |  ___   ____  | |  _  ____  _   _
* |  ____// _ \ | | | | / _  ) / __)| || || | / _ \ |  _ \ | | / )/ _  )| | | |
* | |    | |_| || | | |( (/ / | |   | || || || |_| || | | || |< (( (/ / | |_| |
* |_|     \___/  \____| \____)|_|   |_||_||_| \___/ |_| |_||_| \_)\____) \__  |
*                                                                       (____/
* Copyright (C) 2021 Ivan Dimkovic. All rights reserved.
*
* All trademarks, logos and brand names are the property of their respective
* owners. All company, product and service names used are for identification
* purposes only. Use of these names, trademarks and brands does not imply
* endorsement.
*
* SPDX-License-Identifier: Apache-2.0
* Full text of the license is available in project root directory (LICENSE)
*
* WARNING: This code is a proof of concept for educative purposes. It can
* modify internal computer configuration parameters and cause malfunctions or
* even permanent damage. It has been tested on a limited range of target CPUs
* and has minimal built-in failsafe mechanisms, thus making it unsuitable for
* recommended use by users not skilled in the art. Use it at your own risk.
*
*******************************************************************************/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>

#include "SaferAsmHdr.h"
#include "LowLevel.h"
#include "MiniLog.h"

/*******************************************************************************
 * Compiler Overrides
 ******************************************************************************/

#pragma warning( disable : 4090 )

/*******************************************************************************
 * Globals
 ******************************************************************************/

UINT64 gFatalErrorAsserted = 0;
UINT32 gPCIeBaseAddr = 0;
UINT32 gMCHBAR = 0;

/*******************************************************************************
 *
 ******************************************************************************/

typedef struct _MINISTAT {
  
  UINT8 errtxt[128];

  UINT64 param1;
  UINT64 param2;
  UINT64 param3;

} MINISTAT;

/*******************************************************************************
 * HandleProbingFault
 * 
 * Probing fault means that either wrong MSR has been poked (e.g. system does
 * not support it), attempt to write wrong value has been made, or there has 
 * been a security exception of some sort. 
 * 
 * The best course of action is just to inform the user, and freeze the system.
 ******************************************************************************/

VOID EFIAPI HandleProbingFault(MINISTAT *pst)
{

  //
  // Ensure display happens only once
  
  UINT64 cnt = hlp_atomic_increment_u64(&gFatalErrorAsserted);

  if (cnt == 1)
  {
    /////////////////////////////////////////////////////////////
    // TODO: Fix for MP needed - this will not work on non-BSP //
    /////////////////////////////////////////////////////////////
    
    AsciiPrint("\n\n");
    AsciiPrint("PowerMonkey has encountered a fatal error during operation\n");
    AsciiPrint("This can be caused by unsupported hardware or cfg. value\n");
    AsciiPrint("Details about the crash:\n");
    AsciiPrint("Error: ");
    AsciiPrint(pst->errtxt);
    AsciiPrint("\n");

    AsciiPrint("Param1: 0x%llx\n", pst->param1);
    AsciiPrint("Param2: 0x%llx\n", pst->param2);
    AsciiPrint("Param3: 0x%llx\n", pst->param3);
    AsciiPrint("\n\n");

    AsciiPrint("System will halt now...\n");
  }

  //
  // Freeze this CPU
  
  stop_interrupts_on_this_cpu();

  for (;;) {
    _mm_pause();
  }
}


/*******************************************************************************
 * GetPCIeBaseAddress
 ******************************************************************************/

VOID EFIAPI InitializeMMIO(VOID)
{

  //
  // We need base of the PCIe address space

  gPCIeBaseAddr = get_pciex_base_addr();

  //
  // Just in case
  // 
  if (!(gPCIeBaseAddr & 0x1))
  {
    gPCIeBaseAddr = 0;
    gMCHBAR = 0;
  }    

  //
  // Locate MCHBAR

  gMCHBAR = pm_mmio_read32((gPCIeBaseAddr & 0xFC000000) + 0x48) & 0xfffffffe;

}

/*******************************************************************************
 * Below routines are wrappers for ASM + Error Handling
 ******************************************************************************/

UINT64 pm_rdmsr64(const UINT32 msr_idx)
{ 
  UINT32 err = 0;
  UINT64 val = safer_rdmsr64(msr_idx, &err);

  MiniTrace(MINILOG_OPID_RDMSR64, (UINT16)msr_idx, (err)?0xBAAD : val, 0);

  if (err) {

    MINISTAT bug = { 0 };

    AsciiStrCpyS(&bug.errtxt[0], 128, "rdmsr64");
    
    bug.param1 = 0;
    bug.param2 = (UINT64) msr_idx;
    bug.param3 = val;

    HandleProbingFault(&bug); 
  }

  return val;
}

/*******************************************************************************
 * 
 ******************************************************************************/

UINT32 pm_wrmsr64(const UINT32 msr_idx, const UINT64 value)
{
  MiniTrace(MINILOG_OPID_WRMSR64, (UINT16)msr_idx, value, 1);

  UINT32 err = safer_wrmsr64(msr_idx, value);

  if (err) {

    MINISTAT bug = { 0 };

    AsciiStrCpyS(&bug.errtxt[0], 128, "wrmsr64");

    bug.param1 = 0;
    bug.param2 = (UINT64)msr_idx;
    bug.param3 = value;

    HandleProbingFault(&bug);
  }

  return err;
}

/*******************************************************************************
 *
 ******************************************************************************/

UINT32 pm_mmio_read32(const UINT32 addr)
{  
  UINT32 err = 0;
  UINT32 val = safer_mmio_read32(addr, &err);

  MiniTrace(MINILOG_OPID_MMIO_READ32, 0, (UINT64)((err) ? 0xBAAD : (UINT64)val) | (UINT64)addr<<32, 0);

  if (err) {

    MINISTAT bug = { 0 };

    AsciiStrCpyS(&bug.errtxt[0], 128, "mmio_read32");

    bug.param1 = 0;
    bug.param2 = (UINT64)addr;
    bug.param3 = 0;

    HandleProbingFault(&bug);
  }

  return val;
}

/*******************************************************************************
 *
 ******************************************************************************/

UINT32 pm_mmio_or32(const UINT32 addr, const UINT32 value)
{
  MiniTrace(MINILOG_OPID_MMIO_OR32, 0, (UINT64)value | (UINT64)addr<<32, 1);

  UINT32 err = safer_mmio_or32(addr, value);

  if (err) {

    MINISTAT bug = { 0 };

    AsciiStrCpyS(&bug.errtxt[0], 128, "mmio_or32");

    bug.param1 = 0;
    bug.param2 = (UINT64)addr;
    bug.param3 = (UINT64)value;

    HandleProbingFault(&bug);
  }

  return value;
}

/*******************************************************************************
 *
 ******************************************************************************/

UINT32 pm_mmio_write32(const UINT32 addr, const UINT32 value)
{
  MiniTrace(MINILOG_OPID_MMIO_WRITE32, 0, (UINT64)value | (UINT64)addr << 32, 1);

  UINT32 err = safer_mmio_write32(addr, value);

  if (err) {

    MINISTAT bug = { 0 };

    AsciiStrCpyS(&bug.errtxt[0], 128, "mmio_write32");

    bug.param1 = 0;
    bug.param2 = (UINT64)addr;
    bug.param3 = (UINT64)value;

    HandleProbingFault(&bug);
  }

  return value;
}
