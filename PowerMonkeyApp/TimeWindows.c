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
* SPDX-License-Identifier: Apache-2.0
*
* WARNING: This code is a proof of concept for educative purposes. It can
* modify internal computer configuration parameters and cause malfunctions or
* even permanent damage. It has been tested on a limited range of target CPUs
* and has minimal built-in failsafe mechanisms, thus making it unsuitable for
* recommended use by users not skilled in the art. Use it at your own risk.
*
*******************************************************************************/

#include <Uefi.h>
#include "VoltTables.h"
#include "Platform.h"

/*******************************************************************************
 * FindTauConsts
 ******************************************************************************/

UINT8 FindTauConsts(
  IN const UINT32 timeMs,         // Input: time in 1/1000s (ms) or MAX_POWAH
  IN const UINT8 units,           // from MSR
  OUT UINT8* PX,                  // [OUT] Calculated
  OUT UINT8* PY                   // [OUT] Calculated
)
{
  const UINT64* ptbl = &lookup_taus_5b2b_x1000_shl22[0];

  if (timeMs != MAX_POWAH)
  {
    for (UINT8 yidx = 0; yidx < 32; yidx++) {
      for (UINT8 xidx = 0; xidx < 4; xidx++) {

        UINT64 entry = *ptbl;
        UINT64 shift = 22 + units;

        ptbl++;

        if ((entry>>shift) >= (UINT64)timeMs) 
        {
          *PX = xidx;
          *PY = yidx;

          return 1;
        }
      }
    }
  }
  else {
    *PX = 2;
    *PY = 31;

    return 1;
  }

  return 0;
}