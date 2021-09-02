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
* Full text of license (LICENSE-2.0.txt) is available in project directory
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

#define MSR_OC_MAILBOX                  0x150
#define MSR_FLEX_RATIO                  0x194
#define MSR_TURBO_RATIO_LIMIT           0x1AD
#define MSR_VR_CURRENT_CONFIG           0x601
#define MSR_PACKAGE_POWER_SKU_UNIT      0x606
#define MSR_PACKAGE_POWER_LIMIT         0x610
#define MSR_PKG_POWER_INFO              0x614
#define MSR_PL3_CONTROL                 0x615
#define MSR_PP0_POWER_LIMIT             0x638
#define MSR_PLATFORM_POWER_LIMIT        0x65C
#define MSR_CONFIG_TDP_CONTROL          0x64B


/*******************************************************************************
 *
 ******************************************************************************/

EFI_STATUS EFIAPI IAPERF_ProbeDomainVF(
  IN const UINT8 domain, OUT DOMAIN *desc );

EFI_STATUS EFIAPI IAPERF_ProgramDomainVF(
  IN const UINT8 domain, OUT DOMAIN *desc );

VOID IaCore_OcLock();
