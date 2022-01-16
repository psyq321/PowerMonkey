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
#include <Library/IoLib.h>
#include <Library/UefiLib.h>
#include <Library/TimerLib.h>
#include "CpuInfo.h"
#include "CpuData.h"
#include "SaferAsmHdr.h"

#if defined(__clang__)
#include <immintrin.h>
#endif

#if defined(__GNUC__) && !defined(__clang__)
#include <x86intrin.h>
#else

#pragma intrinsic(__rdtsc)                // At this point, code will look so
#pragma intrinsic(_mm_pause)              // fugly that writing it in pure SMM
                                          // ASM would count as an improvement
#endif


/*******************************************************************************
 * Globals that must be initialized
 ******************************************************************************/

UINT64 gTscFreq = 0;
UINT64 gXtalFreq = 0;

/*******************************************************************************
 * InitializeTscVars
 ******************************************************************************/

EFI_STATUS EFIAPI InitializeTscVars(VOID)
{
  UINT64 tmp;
  UINT32 regs[4] = { 0 };

  if(gCpuInfo.maxf >= 0x15)
    _pm_cpuid(0x15, regs);

  gXtalFreq = regs[2];

  if ((regs[0] == 0) || (regs[1] == 0)) {

    //
    // Totally bogus values
    // TODO: calibrate using some known clock source

    regs[0] = regs[1] = 1;
    gTscFreq = 124783;
    
    return EFI_SUCCESS;
  }

  if(gXtalFreq == 0)
  {
    UINT32 regs2[4] = { 0 };

    if (gCpuInfo.maxf >= 0x15)
      _pm_cpuid(0x16, regs2);

    gXtalFreq = (UINT64)regs2[0] * 1000000 * (UINT64)regs[0] / (UINT64)regs[1];

    if (gXtalFreq == 0) {
      gXtalFreq = 23958333;         // TBD: we could at least distinguish SKU
    }
  }

  tmp = (UINT64)gXtalFreq * (UINT64)regs[1];
  
  if (regs[0] > 1) {
    tmp += (UINT64)regs[0] >> 1;
    tmp /= (UINT64)regs[0];
  }

  gTscFreq = (UINT32)tmp;

  return EFI_SUCCESS;
}

/*******************************************************************************
 * StallCpu
 * Stalls the CPU for specific number of TICKS
 ******************************************************************************/

VOID EFIAPI StallCpu(const UINT64 ticks)
{
  ///
  /// Wrap-around is expected 10-years after power-on 
  /// fortunately for us, we change hardware every year :)
  ///

  UINT64 endTicks = __rdtsc() + ticks;

  while (__rdtsc() <= endTicks) {
    _mm_pause();
  }
}

/*******************************************************************************
 * NanoStall
 * Stalls the CPU for specific number of ns (nanoseconds)
 ******************************************************************************/

VOID EFIAPI NanoStall (const UINT64 ns)
{
  UINT64 ticks = ns * gTscFreq / 1000000000u;

  StallCpu(ticks);
}

/*******************************************************************************
 * MicroStall
 * Stalls the CPU for specific number of ns (microseconds)
 ******************************************************************************/

VOID EFIAPI MicroStall(const UINT64 us)
{
  UINT64 ticks = us * gTscFreq / 1000000u;

  StallCpu(ticks);
}

/*******************************************************************************
 * TicksToNanoSeconds
 ******************************************************************************/

UINT64 EFIAPI TicksToNanoSeconds(UINT64 Ticks)  
{
  return (UINT64)(1000000000u * Ticks) / gTscFreq;
}

/*******************************************************************************
 * ReadTsc
 ******************************************************************************/

UINT64 ReadTsc(VOID)
{
  return __rdtsc();
}
