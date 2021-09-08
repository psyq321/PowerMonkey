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

#pragma intrinsic(__rdtsc)
#pragma intrinsic(_mm_pause)
#pragma intrinsic(__cpuid)

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

  __cpuid(regs, 0x15);

  if ((regs[0] == 0) || (regs[1] == 0)) {
    return EFI_INVALID_PARAMETER;
  }
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  
  tmp =  (UINT64)gXtalFreq * (UINT64)regs[1]; 
  tmp += (UINT64)regs[0] >> 1;
  tmp /= (UINT64)regs[0];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
  
  gTscFreq = (UINT32)tmp;

  return EFI_SUCCESS;
}

/*******************************************************************************
 * StallCpu
 * Stalls the CPU for specific number of TICKS
 ******************************************************************************/

VOID EFIAPI StallCpu( const UINT64 ticks)
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