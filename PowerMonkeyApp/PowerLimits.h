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

/*******************************************************************************
 * GetPkgPowerUnits
 ******************************************************************************/

UINT32 GetPkgPowerUnits(
  UINT32* timeUnits,
  UINT32* energyUnits );

/*******************************************************************************
 * GetPkgPowerLimits
 ******************************************************************************/

UINT64 GetPkgPowerLimits(
  UINT32* pMsrPkgMaxTau,
  UINT32* pMsrPkgMinPL1,
  UINT32* pMsrPkgMaxPL1
);

/*******************************************************************************
 * SetPkgPowerLimit1_MSR
 ******************************************************************************/

VOID EFIAPI SetPkgPowerLimit12_MSR(
  
  // platform limits
  // (if they exist)
  const UINT32 PkgMaxTau,
  const UINT32 PkgMinPL1,
  const UINT32 PkgMaxPL1,

  const UINT8 enablePL1,
  const UINT8 enablePL2,
  
  const UINT32 timeUnits,
  const UINT32 energyUnits,
  const UINT32 powerUnits,

  const UINT8 clamp,
  const UINT32 pl1t,
  const UINT32 pl1w,  
  const UINT32 pl2w);

/*******************************************************************************
 * SetPkgPowerLimit12_MMIO
 ******************************************************************************/

VOID EFIAPI SetPkgPowerLimit12_MMIO(

  // platform limits
  // (if they exist)
  const UINT32 PkgMaxTau,
  const UINT32 PkgMinPL1,
  const UINT32 PkgMaxPL1,

  const UINT8 enablePL1,
  const UINT8 enablePL2,

  const UINT32 timeUnits,
  const UINT32 energyUnits,
  const UINT32 powerUnits,

  const UINT8 clamp,
  const UINT32 pl1t,
  const UINT32 pl1w,
  const UINT32 pl2w);

/*******************************************************************************
 * SetPkgPowerLimit1
 ******************************************************************************/

void SetPkgPowerLimit12(

  const UINT8 dst,            // 0=MSR, 1=MMIO

  // firmware limits
  // (if they exist)
  const UINT32 PkgMaxTau,
  const UINT32 PkgMinPL1,
  const UINT32 PkgMaxPL1,

  const UINT8 enablePL1,
  const UINT8 enablePL2,

  const UINT32 timeUnits,
  const UINT32 energyUnits,
  const UINT32 powerUnits,

  const UINT8 clamp,
  const UINT32 pl1t,
  const UINT32 pl1w,
  const UINT32 pl2w);


/*******************************************************************************
 * SetPL12MSRLock
 ******************************************************************************/

VOID EFIAPI SetPL12MSRLock(const UINT8 lock);

/*******************************************************************************
 * SetPlatformPowerLimit3
 ******************************************************************************/

VOID EFIAPI SetPlatformPowerLimit3(
  const UINT8 enablePL3,
  const UINT32 unitsT,
  const UINT32 unitsW,
  const UINT32 pl3t,
  const UINT32 pl3w);

/*******************************************************************************
 * SetPL3Lock
 ******************************************************************************/

VOID EFIAPI SetPL3Lock(const UINT8 lock);

/*******************************************************************************
 * SetPL4Lock
 ******************************************************************************/

VOID EFIAPI SetPL4Lock(const UINT8 lock);

/*******************************************************************************
 * SetPP0Lock
 ******************************************************************************/

VOID EFIAPI SetPP0Lock(const UINT8 lock);

/*******************************************************************************
 * SetPSysLock
 ******************************************************************************/

VOID EFIAPI SetPSysLock(const UINT8 lock);

/*******************************************************************************
 * SetPlatformPowerLimit4
 ******************************************************************************/

VOID EFIAPI SetPlatformPowerLimit4(
  const UINT8 enablePL4,
  const UINT32 pl4i);

/*******************************************************************************
 * SetPL12MMIOLock
 ******************************************************************************/

VOID EFIAPI SetPL12MMIOLock(const UINT8 lock);

/*******************************************************************************
 * SetPlatformPowerLimit12
 ******************************************************************************/

VOID EFIAPI SetPlatformPowerLimit12(
  const UINT8 enablePL1,
  const UINT8 enablePL2,
  const UINT32 unitsT,
  const UINT32 unitsW,
  const UINT8 clamp,
  const UINT32 pl1t,
  const UINT32 pl1w,    
  const UINT32 pl2w);

/*******************************************************************************
 * SetPP0PowerLimit
 ******************************************************************************/

void SetPP0PowerLimit(

  // firmware limits
  // (if they exist)
  const UINT32 PkgMaxTau,
  const UINT32 PkgMinPow,
  const UINT32 PkgMaxPow,

  const UINT8 enablePP0,

  const UINT32 timeUnits,
  const UINT32 powerUnits,

  const UINT8 clamp,
  const UINT32 pp0t,
  const UINT32 pp0w);

/*******************************************************************************
 * GetConfigTdpControl
 ******************************************************************************/

UINT64 EFIAPI GetConfigTdpControl(VOID);

/*******************************************************************************
  GetCTDPLevel
 ******************************************************************************/

VOID EFIAPI GetCTDPLevel(UINT8* level, UINT8* locked);

/*******************************************************************************
  SetCTDPLevel
 ******************************************************************************/

VOID EFIAPI SetCTDPLevel(const UINT8 level);

/*******************************************************************************
  SetCTDPLock
 ******************************************************************************/

VOID EFIAPI SetCTDPLock(const UINT8 lock);

/*******************************************************************************
 * ProgramPowerCtl
 ******************************************************************************/

VOID EFIAPI ProgramPowerCtl(const UINT8 eeTurbo, const UINT8 rtHlt);
