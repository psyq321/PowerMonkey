/*******************************************************************************
*  ______                            ______                 _
* (_____ \                          |  ___ \               | |
*  _____) )___   _ _ _   ____   ___ | | _ | |  ___   ____  | |  _  ____  _   _
* |  ____// _ \ | | | | / _  ) / __)| || || | / _ \ |  _ \ | | / )/ _  )| | | |
* | |    | |_| || | | |( (/ / | |   | || || || |_| || | | || |< (( (/ / | |_| |
* |_|     \___/  \____| \____)|_|   |_||_||_| \___/ |_| |_||_| \_)\____) \__  |
*                                                                       (____/
* Copyright (C) 2021-2022 Ivan Dimkovic. All rights reserved.
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
#include <Library/TimerLib.h>

#include "VFTuning.h"
#include "LowLevel.h"
#include "DelayX86.h"
#include "MiniLog.h"

/*******************************************************************************
 *                   !!! WARNING - ACHTUNG - VNIMANIE !!!
 *                !!! HACKS AHEAD, HEAVY POC-GRADE CODE !!!
 *
 * The Proper Way(TM) of handling this would be your equipment OxM exposing
 * firmware setup interfaces to you, the user. And everything would be good.
 *
 * Lacking that, second-best Proper Way(TM) would be to program this exactly
 * as described in your CPU/platform "BIOS Writer's Guide" (BWG) document. If
 * you do have access to the said documentation, this tool is not needed.
 *
 * But since we are here, and this is a POC, I've added a minimum amount of
 * sanity checking (so it is not total "cowboy code"), but it could still fail.
 * You have been warned!
 * 
 * THEORY OF OPERATION: 
 * 
 * Empirical observation shows that most IA platforms (baring Xeon E7s, Phis, 
 * etc.) from the last decade (2011-2021) use MSR 0x1AD (MSR_TURBO_RATIO_LIMIT) 
 * for maximum turbo ratios grouped by the core counts. Sadly, there are many 
 * variations, and additional MSRs (1AE,1AF) all used from time to time with no 
 * apparent system or regularity visible to an outsider.
 * 
 * Since this is a POC, I decided to add the simplest form of code that shall 
 * (maybe) work on most platforms (sold between 2011 and 2011 at the time this
 * code has been written):
 *
 *   1. Read the MSR 0x1AD and parse each 8-bit block
 *   2. Test each 8-bit block for value != 0
 *   3. If the value is 0, this block is likely unused. Mask it out
 *   4. Program the remaining blocks with the desired max ratio
 * 
 * If somebody wants to make release-grade code, this is the first place I'd
 * start and replace this with a gigantic lookup table keyed by CPUID
 ******************************************************************************/

/*******************************************************************************
 * GetTurboRatioLimits
 ******************************************************************************/

UINT64 GetTurboRatioLimits(VOID)
{
  return pm_rdmsr64(MSR_TURBO_RATIO_LIMIT);
}

/*******************************************************************************
 * GetTurboRatioLimits_ECORE
 ******************************************************************************/

UINT64 GetTurboRatioLimits_ECORE(VOID)
{
  return pm_rdmsr64(MSR_TURBO_RATIO_LIMIT_ECORE);
}


/*******************************************************************************
 * SetTurboRatioLimits
 ******************************************************************************/

EFI_STATUS SetTurboRatioLimits(const UINT64 val)
{
  pm_wrmsr64(MSR_TURBO_RATIO_LIMIT, val);
  
  MicroStall(10);

  UINT64 msr = pm_rdmsr64(MSR_TURBO_RATIO_LIMIT);

  if (msr != val) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/*******************************************************************************
 * SetTurboRatioLimits_ECORE
 ******************************************************************************/

EFI_STATUS SetTurboRatioLimits_ECORE(const UINT64 val)
{
  pm_wrmsr64(MSR_TURBO_RATIO_LIMIT_ECORE, val);

  MicroStall(10);

  UINT64 msr = pm_rdmsr64(MSR_TURBO_RATIO_LIMIT_ECORE);

  if (msr != val) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}


/*******************************************************************************
 * ProgramMaxTurboRatios
 ******************************************************************************/

EFI_STATUS IAPERF_ProgramMaxTurboRatios(const UINT8 maxRatio)
{
  UINT64 msr = GetTurboRatioLimits();
  UINT8* msr8 = (UINT8*)&msr;

  //
  // Adjust max ratios for seemingly valid blocks

  for (UINT8 blkidx = 0; blkidx < 8; blkidx++) {
    if ((msr8[blkidx]) && (msr8[blkidx] != 0xFF)) {
      msr8[blkidx] = maxRatio;
    }
  }

  //
  // Program the new values

  MiniTraceEx("Setting all-core max turbo ratio to: %ux", maxRatio);

  return SetTurboRatioLimits(msr);
}


/*******************************************************************************
 * ProgramMaxTurboRatios_ECORE
 ******************************************************************************/

EFI_STATUS IAPERF_ProgramMaxTurboRatios_ECORE(const UINT8 maxRatio)
{
  UINT64 msr = GetTurboRatioLimits_ECORE();
  UINT8* msr8 = (UINT8*)&msr;

  //
  // Adjust max ratios for seemingly valid blocks

  for (UINT8 blkidx = 0; blkidx < 8; blkidx++) {
    if ((msr8[blkidx]) && (msr8[blkidx] != 0xFF)) {
      msr8[blkidx] = maxRatio;
    }
  }

  //
  // Program the new values

  MiniTraceEx("Setting all-core max ECORE turbo ratio to: %ux", maxRatio);

  return SetTurboRatioLimits_ECORE(msr);
}