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
#include <Library/UefiLib.h>
#include "SaferAsmHdr.h"
#include "CpuInfo.h"
#include "Constants.h"

/*******************************************************************************
 * 
 ******************************************************************************/

#define CPUID_EAX   0
#define CPUID_EBX   1
#define CPUID_ECX   2
#define CPUID_EDX   3

/*******************************************************************************
 * 
 ******************************************************************************/

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

  _pm_cpuid(0x80000002, venstr);
  _pm_cpuid(0x80000003, venstr+4);
  _pm_cpuid(0x80000004, venstr+12);

  ///////////////////
  // Vendor String //
  ///////////////////
  
  UINT32 regs[4] = {0};

  _pm_cpuid(0, regs);
  
  UINT32 hscall = ci->maxf = regs[CPUID_EAX];
  
  brandstr[0] = regs[CPUID_EBX];
  brandstr[1] = regs[CPUID_EDX];
  brandstr[2] = regs[CPUID_ECX];
 
  _pm_cpuid(0x01, regs);

  ci->f1 = regs[CPUID_EAX];
  ci->stepping =  regs[CPUID_EAX] & 0x0000000F;
  ci->family =   (UINT32)(regs[CPUID_EAX] & 0x00000F00) >> 8;
  ci->model =    (UINT32)(regs[CPUID_EAX] & 0x000000F0) >> 4;

  if ((ci->family == 0xF) || (ci->family == 0x6)) {
    ci->model  |= (UINT32)((regs[CPUID_EAX] & 0x0000F0000) >> 12);
    ci->family |= (UINT32)((regs[CPUID_EAX] & 0x00FF00000) >> 16);
  }
  
  ///////////////////////////////////
  // Hybrid Architecture Detection //
  ///////////////////////////////////
  
  _pm_cpuid(0x7, regs);

  ci->ECore = 0;
  ci->HybridArch = ((regs[CPUID_EDX] & bit15u32)) ? 1 : 0;

  if (ci->HybridArch) {
    if (hscall >= 0x1A) {      
      _pm_cpuid(0x1A, regs);

      UINT32 ct = ((regs[CPUID_EAX] & 0xFF000000) >> 24);

      if (ct == 0x20) {
        ci->ECore = 1;
      }        
    }
  }
}