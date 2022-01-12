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

#pragma once

#include "Platform.h"

/*******************************************************************************
 * 
 ******************************************************************************/

#define INVALID_VR_ADDR                                                 0xFF
#define NO_SVID_VR                                                      0x01
#define SVID_VR                                                         0x00

/*******************************************************************************
 * VR Configuration Templates (per domain, used during topo discovery)
 ******************************************************************************/

typedef struct _VOLTPLANEDESC {

  UINT8 DomainExists;
  UINT8 DomainSupportedForDiscovery;

  //
  // For SKUs with OC Mailbox support for VR topology discovery

  UINT32 OCMB_VRAddr_DomainBitMask;         // Bit mask for VR Addr
  UINT32 OCMB_VRAddr_DomainBitShift;        // bits to shift 

  UINT32 OCMB_VRsvid_DomainBitMask;         // Bit mask for VR SVID capa bit
  UINT32 OCMB_VRsvid_DomainBitShift;        // bits to shift

  CHAR8 FriendlyNameShort[32];

} VOLTPLANEDESC;

/*******************************************************************************
 * VR Configuration
 ******************************************************************************/

typedef struct _VOLTCFGTEMPLATE
{
  VOLTPLANEDESC doms[MAX_DOMAINS];

} VOLTCFGTEMPLATE;

/*******************************************************************************
 * 
 ******************************************************************************/

typedef struct _CPUTYPE {
  UINT32 family;
  UINT32 model;
  UINT32 stepping;
} CPUTYPE;

typedef struct _CPUCONFIGTABLE {

  CPUTYPE cpuType;
  CHAR8 uArch[32];

  BOOLEAN hasUnlimitedIccMaxFlag;   // Extra bit controlling unlimited IccMax
  UINT8 IccMaxBits;                 // ADL=11bits, RKL and lower=10bits
  UINT8 VfPointsExposed;            // OC Mailbox exposes V/F points (param2)
  UINT8 HasEcores;                  // Has E Cores

  VOLTCFGTEMPLATE* vtdt;            // Template for VR topo discovery (or null)

} CPUCONFIGTABLE;

/*******************************************************************************
 * DetectCpu
 ******************************************************************************/

BOOLEAN DetectCpu();

/*******************************************************************************
 * Detected CPU Data
 ******************************************************************************/

extern CPUCONFIGTABLE* gActiveCpuData;
extern CPUINFO gCpuInfo;
extern UINT32 gBCLK_bsp;

/*******************************************************************************
 * Available VR discovery templates
 ******************************************************************************/

extern VOLTCFGTEMPLATE vcfg_q_xyzlake_client;       // SKL/CFL/CML/RKL Client
extern VOLTCFGTEMPLATE vcfg_q_xyzlake_server;       // SKX/CLX HEDT/WS/Server
extern VOLTCFGTEMPLATE vcfg_q_tigerlake_client;     // TGL Client
extern VOLTCFGTEMPLATE vcfg_q_alderlake_client;     // ADL Client
