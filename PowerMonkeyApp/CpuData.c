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

#include "CpuInfo.h"
#include "CpuData.h"
#include "OcMailbox.h"
#include "MiniLog.h"
#include "SaferAsmHdr.h"

CPUINFO gCpuInfo = { 0 };

//
// BCLK in kHz

UINT32 gBCLK_bsp = 0;           // I do not think anybody supports 2S/4S/8S
                                // systems with different BCLKs, so we are ok

/*******************************************************************************
 *
 ******************************************************************************/


/*******************************************************************************
 * CPU DATA + PER-CPU PATCHES/WORKAROUNDS
 ******************************************************************************/

///
/// Data Sources,
/// - https,//github.com/erpalma/throttled  (CPU IDs)
/// 

CPUCONFIGTABLE gCpuConfigTable[] = {
  
  //
  // Special entry for undetected CPUs
  
  { {0, 0, 0} , "Unknown", 0, 10, 0, 0, NULL },

  //
  // "Known" CPUs (not necessarily supported or tested)

  { {6, 26, 1} , "Nehalem", 0, 10, 0, 0, NULL },
  { {6, 26, 2} , "Nehalem-EP", 0, 10, 0, 0, NULL },
  { {6, 26, 4} , "Bloomfield", 0, 10, 0, 0, NULL },
  { {6, 28, 2} , "Silverthorne", 0, 10, 0, 0, NULL },
  { {6, 28, 10} , "PineView", 0, 10, 0, 0, NULL },
  { {6, 29, 0} , "Dunnington-6C", 0, 10, 0, 0, NULL },
  { {6, 29, 1} , "Dunnington", 0, 10, 0, 0, NULL },
  { {6, 30, 0} , "Lynnfield", 0, 10, 0, 0, NULL },
  { {6, 30, 5} , "Lynnfield_CPUID", 0, 10, 0, 0, NULL },
  { {6, 31, 1} , "Auburndale", 0, 10, 0, 0, NULL },
  { {6, 37, 2} , "Clarkdale", 0, 10, 0, 0, NULL },
  { {6, 38, 1} , "TunnelCreek", 0, 10, 0, 0, NULL },
  { {6, 39, 2} , "Medfield", 0, 10, 0, 0, NULL },
  { {6, 42, 2} , "SandyBridge", 0, 10, 0, 0, NULL },
  { {6, 42, 6} , "SandyBridge", 0, 10, 0, 0, NULL },
  { {6, 42, 7} , "Sandy Bridge-DT", 0, 10, 0, 0, NULL },
  { {6, 44, 1} , "Westmere-EP", 0, 10, 0, 0, NULL },
  { {6, 44, 2} , "Gulftown", 0, 10, 0, 0, NULL },
  { {6, 45, 5} , "Sandy Bridge-EP", 0, 10, 0, 0, NULL },
  { {6, 45, 6} , "Sandy Bridge-E", 0, 10, 0, 0, NULL },
  { {6, 46, 4} , "Beckton", 0, 10, 0, 0, NULL },
  { {6, 46, 5} , "Beckton", 0, 10, 0, 0, NULL },
  { {6, 46, 6} , "Beckton", 0, 10, 0, 0, NULL },
  { {6, 47, 2} , "Eagleton", 0, 10, 0, 0, NULL },
  { {6, 53, 1} , "Cloverview", 0, 10, 0, 0, NULL },
  { {6, 54, 1} , "Cedarview-D", 0, 10, 0, 0, NULL },
  { {6, 54, 9} , "Centerton", 0, 10, 0, 0, NULL },
  { {6, 55, 3} , "Bay Trail-D", 0, 10, 0, 0, NULL },
  { {6, 55, 8} , "Silvermont", 0, 10, 0, 0, NULL },
  { {6, 58, 9} , "Ivy Bridge-DT", 0, 10, 0, 0, NULL },
  { {6, 60, 3} , "Haswell-DT", 0, 10, 0, 0, NULL },
  { {6, 61, 4} , "Broadwell-U", 0, 10, 0, 0, NULL },
  { {6, 62, 3} , "IvyBridgeEP", 0, 10, 0, 0, NULL },
  { {6, 62, 4} , "Ivy Bridge-E", 0, 10, 0, 0, NULL },
  { {6, 63, 2} , "Haswell-EP", 0, 10, 0, 0, NULL },
  { {6, 69, 1} , "HaswellULT", 0, 10, 0, 0, NULL },
  { {6, 70, 1} , "Crystal Well-DT", 0, 10, 0, 0, NULL },
  { {6, 71, 1} , "Broadwell-H", 0, 10, 0, 0, NULL },
  { {6, 76, 3} , "Braswell", 0, 10, 0, 0, NULL },
  { {6, 77, 8} , "Avoton", 0, 10, 0, 0, NULL },
  { {6, 78, 3} , "Skylake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 79, 1} , "BroadwellE", 0, 10, 0, 0, NULL },
  { {6, 85, 4} , "SkylakeXeon", 0, 10, 0, 0, &vcfg_q_xyzlake_server },
  { {6, 85, 6} , "CascadeLakeSP", 0, 10, 0, 0, &vcfg_q_xyzlake_server },
  { {6, 85, 7} , "CascadeLakeXeon2", 0, 10, 0, 0, &vcfg_q_xyzlake_server },
  { {6, 86, 2} , "BroadwellDE", 0, 10, 0, 0, NULL },
  { {6, 86, 4} , "BroadwellDE", 0, 10, 0, 0, NULL },
  { {6, 87, 0} , "KnightsLanding", 0, 10, 0, 0, NULL },
  { {6, 87, 1} , "KnightsLanding", 0, 10, 0, 0, NULL },
  { {6, 90, 0} , "Moorefield", 0, 10, 0, 0, NULL },
  { {6, 92, 9} , "Apollo Lake", 0, 10, 0, 0, NULL },
  { {6, 93, 1} , "SoFIA", 0, 10, 0, 0, NULL },
  { {6, 94, 0} , "Skylake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 94, 3} , "Skylake-S", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 95, 1} , "Denverton", 0, 10, 0, 0, NULL },
  { {6, 102, 3} , "Cannon Lake-U", 0, 10, 0, 0, NULL },
  { {6, 117, 10} , "Spreadtrum", 0, 10, 0, 0, NULL },
  { {6, 106, 0} , "IceLake-SP", 0, 10, 1, 0, &vcfg_q_xyzlake_server },       // QEMU (debug)
  { {6, 106, 6} , "IceLake-SP", 0, 10, 0, 0, &vcfg_q_xyzlake_server },
  { {6, 122, 1} , "Gemini Lake-D", 0, 10, 0, 0, NULL },
  { {6, 122, 8} , "GoldmontPlus", 0, 10, 0, 0, NULL },
  { {6, 126, 5} , "IceLakeY", 0, 10, 0, 0, NULL },  
  { {6, 138, 1} , "Lakefield", 0, 10, 0, 0, NULL },
  { {6, 140, 1} , "TigerLake", 0, 10, 1, 0, &vcfg_q_tigerlake_client },
  { {6, 141, 1} , "TigerLake", 0, 10, 1, 0, &vcfg_q_tigerlake_client },
  { {6, 142, 9} , "Kabylake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 142, 10} , "Kabylake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 142, 11} , "WhiskeyLake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 142, 12} , "Comet Lake-U", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 156, 0} , "JasperLake", 0, 10, 0, 0, NULL },
  { {6, 158, 9} , "KabylakeG", 0, 10, 0, 0, NULL },
  { {6, 158, 10} , "Coffeelake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 158, 11} , "Coffeelake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 158, 12} , "CoffeeLake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 158, 13} , "CoffeeLake", 0, 10, 0, 0, &vcfg_q_xyzlake_client },
  { {6, 165, 2} , "CometLake", 0, 10, 1, 0, &vcfg_q_xyzlake_client },
  { {6, 165, 4} , "CometLake", 0, 10, 1, 0, &vcfg_q_xyzlake_client },
  { {6, 165, 5} , "CometLake-S", 0, 10, 1, 0, &vcfg_q_xyzlake_client },
  { {6, 166, 0} , "CometLake", 0, 10, 1, 0, &vcfg_q_xyzlake_client },
  
  { {6, 167, 0} , "RocketLake", 0, 10, 1, 0, &vcfg_q_xyzlake_client },       // RKL-S ES
  { {6, 167, 1} , "RocketLake", 0, 10, 1, 0, &vcfg_q_xyzlake_client },       // RKL-S QS/PRQ
  
  { {6, 151, 0} , "AlderLake", 0, 11, 1, 1, &vcfg_q_alderlake_client },      // (90670)
  { {6, 151, 1} , "AlderLake-S", 0, 11, 1, 1, &vcfg_q_alderlake_client },    // ADL-S ES2 (90671)
  { {6, 151, 2} , "AlderLake-S", 0, 11, 1, 1, &vcfg_q_alderlake_client },    // ADL-S QS/PRQ (90672)
  { {6, 151, 4} , "AlderLake-S", 0, 11, 1, 1, &vcfg_q_alderlake_client },    // ADL-S (90674)
  { {6, 151, 5} , "AlderLake-S", 0, 11, 1, 1, &vcfg_q_alderlake_client },    // ADL-S QS/PRQ (90675)
  { {6, 154, 2} , "AlderLake-H/P", 0, 11, 1, 1, &vcfg_q_alderlake_client },    // ADL-H/P (906A2)
  { {6, 154, 3} , "AlderLake-H/P", 0, 11, 1, 1, &vcfg_q_alderlake_client },    // ADL-H/P (906A3)
  { {6, 154, 4} , "AlderLake-H/P", 0, 11, 1, 1, &vcfg_q_alderlake_client },    // ADL-H/P (906A4)
  { {6, 154, 1} , "AlderLake", 0, 11, 1, 1, &vcfg_q_alderlake_client },
};


/*******************************************************************************
 * Active CPU Data (to be used)
 ******************************************************************************/

CPUCONFIGTABLE* gActiveCpuData = &gCpuConfigTable[0];

/*******************************************************************************
 * DetectCpu
 ******************************************************************************/

BOOLEAN DetectCpu()
{
  //
  // CPUID

  GetCpuInfo(&gCpuInfo);

  //
  // Detect BIOS-limited maximum CPUID input value 
  // as it would interfere with our ability to detect features

  if (gCpuInfo.maxf <= 2) 
  {
    AsciiPrint("WARNING: possible maximum CPUID input value limitation detected!\n");
    AsciiPrint("This condition can interfere with CPU feature detection.\n");
    AsciiPrint("For ensuring correct operation, it is advisable to disable CPUID limit.\n");
    AsciiPrint("This is typically done in BIOS Setup.\n");
    AsciiPrint("\n");
  }
   

  //
  // BCLK

  gBCLK_bsp = 100000;

  {
    CpuMailbox box;
    MailboxBody* b = &box.b;
    UINT32 cmd = 0;

    OcMailbox_InitializeAsMSR(&box);

    MiniTraceEx("Reading BCLK frequency from OC Mailbox");

    cmd = OcMailbox_BuildInterface(0x5, 0, 0);

    if ((!EFI_ERROR(OcMailbox_ReadWrite(cmd, 0, &box)))&&((box.status == 0))) {
      gBCLK_bsp = b->box.data;
    }

    //
    // Try this in caes of failure

    if (!gBCLK_bsp) {

      cmd = OcMailbox_BuildInterface(0x5, 1, 0);

      if ((!EFI_ERROR(OcMailbox_ReadWrite(cmd, 0, &box))) && ((box.status == 0))) {
        gBCLK_bsp = b->box.data;
      }
    }

    //
    // Fix bogus values reported under certain virtualized environments

    if (gBCLK_bsp < 5000) {
      gBCLK_bsp = 100000;
    }
  }

  //
  // Detect CPU

  for (UINTN ccnt=0;ccnt<sizeof(gCpuConfigTable)/sizeof(CPUCONFIGTABLE);ccnt++) {

    CPUCONFIGTABLE* pt = &gCpuConfigTable[ccnt];

    if ( 
      (gCpuInfo.model == pt->cpuType.model) && 
      (gCpuInfo.family == pt->cpuType.family) && 
      (gCpuInfo.stepping == pt->cpuType.stepping)) 
    {
      gActiveCpuData = pt;
      
      AsciiPrint("Detected CPU: %a, model: %u, family: %u, stepping: %u\n",
        pt->uArch, pt->cpuType.model, pt->cpuType.family, pt->cpuType.stepping);

      return TRUE;
    }
  }

  return FALSE;
}
