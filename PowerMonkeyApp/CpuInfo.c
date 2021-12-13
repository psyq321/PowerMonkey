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
#include "SaferAsmHdr.h"
#include "CpuInfo.h"
#include "Constants.h"

/*******************************************************************************
 * 
 ******************************************************************************/

#pragma intrinsic(__cpuid)
#pragma intrinsic(__cpuidex)
extern void* memset(void* str, int c, UINTN n);

/*******************************************************************************
 * GetCpuInfo
 ******************************************************************************/

void GetCpuInfo(CPUINFO* ci)
{
  memset(ci, 0, sizeof(CPUINFO));


  //////////////////
  // Brand String //
  //////////////////
  
  UINT32* venstr = (UINT32 *)ci->venString;
  UINT32* brandstr = (UINT32*)ci->brandString;

  __cpuid(venstr,    0x80000002);
  __cpuid(venstr+4,  0x80000003);
  __cpuid(venstr+12, 0x80000004);

  ///////////////////
  // Vendor String //
  ///////////////////
  
  UINT32 args[4] = {0};

  __cpuid(args, 0x00000000);
  UINT32 hscall = ci->maxf = args[0];
  brandstr[0] = args[1];
  brandstr[1] = args[3];
  brandstr[2] = args[2];

  
  memset(args, 0, sizeof(args));  
  __cpuid(args, 0x01);

  ci->f1 = args[0];
  ci->stepping =  args[0] & 0x0000000F;
  ci->family =   (UINT32)(args[0] & 0x00000F00) >> 8;
  ci->model =    (UINT32)(args[0] & 0x000000F0) >> 4;

  if ((ci->family == 0xF) || (ci->family == 0x6)) {
    ci->model  |= (UINT32)((args[0] & 0x0000F0000) >> 12);
    ci->family |= (UINT32)((args[0] & 0x00FF00000) >> 16);
  }

  
  ///////////////////////////////////
  // Hybrid Architecture Detection //
  ///////////////////////////////////

  memset(args, 0, sizeof(args));
  __cpuid(args, 0x7);

  ci->HybridArch = (args[3] & bit15u32) ? 1 : 0;  

  if (ci->HybridArch) {

    if (hscall >= 0x1A) {
      memset(args, 0, sizeof(args));
      __cpuid(args, 0x1A);

      UINT32 ct = ((args[0] & 0xFF000000) >> 24);

      if (ct == 0x20) {
        ci->ECore = 1;
        ci->PCore = 0;
      }
      else {
        ci->ECore = 0;
        ci->PCore = 1;
      }
    }
  }
  else {
    ci->ECore = 0;
    ci->PCore = 1;
  }
}