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

#include <Uefi.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>

#include "VfTuning.h"
#include "LowLevel.h"
#include "DelayX86.h"
#include "TimeWindows.h"
#include "Constants.h"

/*******************************************************************************
 * GetPkgPowerUnits
 ******************************************************************************/

UINT32 GetPkgPowerUnits(
  UINT32 *timeUnits,
  UINT32 *energyUnits
  )
{
  UINT64 val = pm_rdmsr64(MSR_PACKAGE_POWER_SKU_UNIT);

  *timeUnits =   (UINT32)((val) & 0x00ff0000) >> 16;
  *energyUnits = (UINT32)((val) & 0x0000ff00) >> 16;
  
  UINT32 powerUnits = (UINT32)(val & 0xFF);

  if (powerUnits == 0) {
    powerUnits = 1;
  }
  else {
    powerUnits = 1;
    powerUnits <<= val;
  }

  return powerUnits;
}

/*******************************************************************************
 * GetPkgPowerLimits
 ******************************************************************************/

UINT64 GetPkgPowerLimits(
  UINT32 *pMsrPkgMaxTau,
  UINT32 *pMsrPkgMinPL1,
  UINT32 *pMsrPkgMaxPL1
)
{
  QWORD msr;
  
  msr.u64 = pm_rdmsr64(MSR_PKG_POWER_INFO);
  
  *pMsrPkgMaxPL1 = msr.u32.hi & 0x00003fff;
  *pMsrPkgMinPL1 = (msr.u32.lo & 0x3fff0000) >> 16;  
  *pMsrPkgMaxTau = (msr.u32.hi & 0x3f0000) >> 16;

  msr.u64 = pm_rdmsr64(MSR_PKG_POWER_INFO);
  return msr.u64;
}


/*******************************************************************************
 * SetPkgPowerLimit1_MSR
 ******************************************************************************/

void SetPkgPowerLimit12_MSR(
    
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
    const UINT32 pl2w )
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;
  const UINT32 notvmask1 = 0xffff8000l;

  //
  // Back to Watts

  UINT8 FX, FY, TNOERR;
  UINT32 xform_pl1w, xform_pl2w, xform_tau;

  xform_pl1w = xform_pl2w = MAX_POWAH;
  xform_tau = 0x7fff;

  TNOERR = FindTauConsts(pl1t, (UINT8)powerUnits, &FX, &FY);

  if (TNOERR) {
    FX &= 0x3;
    FY &= 0x1f;
  }

  if (pl1w != MAX_POWAH) {
    xform_pl1w = pl1w * powerUnits;
    xform_pl1w = (xform_pl1w) ? xform_pl1w / 1000 : 0;
  }

  if (pl2w != MAX_POWAH) {
    xform_pl2w = pl2w * powerUnits;
    xform_pl2w = (xform_pl2w) ? xform_pl2w / 1000 : 0;
  }

  //
  // If limits are enforced by the firmware, ensure that
  // we fall in between

  xform_pl1w = (PkgMinPL1 > 0) ? MAX(xform_pl1w, PkgMinPL1) : xform_pl1w;
  xform_pl1w = (PkgMaxPL1 > 0) ? MIN(xform_pl1w, PkgMaxPL1) : xform_pl1w;
  xform_tau = (PkgMaxTau > 0) ? MIN(xform_tau, PkgMaxTau) : xform_tau;

  msr.u64 = pm_rdmsr64(MSR_PACKAGE_POWER_LIMIT);
  
  /////////
  // PL1 //
  /////////

  msr.u32.lo = (enablePL1) ?
    msr.u32.lo | 0x00008000l :
    msr.u32.lo & 0xffff7fffl;

  if (enablePL1)
  {
    // POWER
    msr.u32.lo &= notvmask1;
    msr.u32.lo |= (xform_pl1w > vmask1) ? vmask1 : xform_pl1w & vmask1;

    if (TNOERR) {
      // TIME    | 
      msr.u32.lo |= (UINT32)(FX) << 21;
      msr.u32.lo |= (UINT32)(FY) << 17;
    }
  }
  else {
    msr.u32.lo &= notvmask1;
  }

  /////////
  // PL2 //
  /////////

  msr.u32.hi = (enablePL2) ?
    msr.u32.hi | 0x00008000l :
    msr.u32.hi & 0xffff7fffl;

  if (enablePL2)
  {
    // POWER
    msr.u32.hi &= notvmask1;
    msr.u32.hi |= (xform_pl2w > vmask1) ? vmask1 : xform_pl2w & vmask1;

    if (TNOERR) {
      // TIME    | 
      msr.u32.hi |= (UINT32)(FX) << 21;
      msr.u32.hi |= (UINT32)(FY) << 17;
    }
  }
  else {
    msr.u32.hi &= notvmask1;
  }

  pm_wrmsr64(MSR_PACKAGE_POWER_LIMIT, msr.u64);
  
  MicroStall(3);

  //
  // Clamp

  msr.u64 = pm_rdmsr64(MSR_PACKAGE_POWER_LIMIT);

  msr.u32.lo = (clamp) ?
    msr.u32.lo | 0x00010000l :
    msr.u32.lo & 0xfffeffffl;

  pm_wrmsr64(MSR_PACKAGE_POWER_LIMIT, msr.u64);
}

/*******************************************************************************
 * SetPL12MSRLock
 ******************************************************************************/

VOID EFIAPI SetPL12MSRLock( const UINT8 lock )
{  
  QWORD msr = { 0 };

  UINT64 lck64 = (UINT64)(lock & 0x1);  
  lck64 <<= 63;

  msr.u64 = pm_rdmsr64(MSR_PACKAGE_POWER_LIMIT);

  msr.u64 |= lck64;

  pm_wrmsr64(MSR_PACKAGE_POWER_LIMIT, msr.u64);

  MicroStall(3);
}

/*******************************************************************************
 * SetPL12MMIOLock
 ******************************************************************************/

VOID EFIAPI SetPL12MMIOLock(const UINT8 lock)
{
  QWORD msr = { 0 };

  UINT64 lck64 = (UINT64)(lock & 0x1);
  lck64 <<= 63;

  msr.u64 = pm_rdmsr64(MSR_PACKAGE_POWER_LIMIT);
  msr.u64 |= lck64;

  if (gMCHBAR)
  {
    pm_mmio_or32(gMCHBAR + 0x59a0, msr.u32.lo);
    pm_mmio_or32(gMCHBAR + 0x59a4, msr.u32.hi);
  }

  MicroStall(3);
}

/*******************************************************************************
 * SetPL3Lock
 ******************************************************************************/

VOID EFIAPI SetPL3Lock(const UINT8 lock)
{
  QWORD msr = { 0 };

  UINT64 lck64 = (UINT64)(lock & 0x1);
  lck64 <<= 31;

  msr.u64 = pm_rdmsr64(MSR_PL3_CONTROL);
  msr.u64 |= lck64;

  pm_wrmsr64(MSR_PL3_CONTROL, msr.u64);

  MicroStall(3);
}

/*******************************************************************************
 * SetPL4Lock
 ******************************************************************************/

VOID EFIAPI SetPL4Lock(const UINT8 lock)
{
  QWORD msr = { 0 };

  msr.u64 = pm_rdmsr64(MSR_VR_CURRENT_CONFIG);
  
  msr.u32.lo |= bit31u32;

  pm_wrmsr64(MSR_VR_CURRENT_CONFIG, msr.u64);

  MicroStall(3);
}


/*******************************************************************************
 * SetPP0Lock
 ******************************************************************************/

VOID EFIAPI SetPP0Lock(const UINT8 lock)
{
  QWORD msr = { 0 };

  UINT64 lck64 = (UINT64)(lock & 0x1);
  lck64 <<= 31;

  msr.u64 = pm_rdmsr64(MSR_PP0_POWER_LIMIT);
  msr.u64 |= lck64;

  pm_wrmsr64(MSR_PP0_POWER_LIMIT, msr.u64);

  MicroStall(3);
}


/*******************************************************************************
 * SetPkgPowerLimit12_MMIO
 ******************************************************************************/

VOID EFIAPI SetPkgPowerLimit12_MMIO(

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
  const UINT32 pl2w)
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;
  const UINT32 notvmask1 = 0xffff8000l;

  //
  // Back to Watts

  UINT8 FX, FY, TNOERR;
  UINT32 xform_pl1w, xform_pl2w, xform_tau;

  xform_pl1w = xform_pl2w = MAX_POWAH;
  xform_tau = 0x7fff;

  TNOERR = FindTauConsts(pl1t, (UINT8)powerUnits, &FX, &FY);

  if (TNOERR) {
    FX &= 0x3;
    FY &= 0x1f;
  }

  if (pl1w != MAX_POWAH) {
    xform_pl1w = pl1w * powerUnits;
    xform_pl1w = (xform_pl1w) ? xform_pl1w / 1000 : 0;
  }

  if (pl2w != MAX_POWAH) {
    xform_pl2w = pl2w * powerUnits;
    xform_pl2w = (xform_pl2w) ? xform_pl2w / 1000 : 0;
  }

  //
  // If limits are enforced by the firmware, ensure that
  // we fall in between

  xform_pl1w = (PkgMinPL1 > 0) ? MAX(xform_pl1w, PkgMinPL1) : xform_pl1w;
  xform_pl1w = (PkgMaxPL1 > 0) ? MIN(xform_pl1w, PkgMaxPL1) : xform_pl1w;
  xform_tau = (PkgMaxTau > 0) ? MIN(xform_tau, PkgMaxTau) : xform_tau;

  msr.u64 = pm_rdmsr64(MSR_PACKAGE_POWER_LIMIT);

  /////////
  // PL1 //
  /////////

  msr.u32.lo = (enablePL1) ?
    msr.u32.lo | 0x00008000l :
    msr.u32.lo & 0xffff7fffl;

  if (enablePL1)
  {
    // POWER
    msr.u32.lo &= notvmask1;
    msr.u32.lo |= (xform_pl1w > vmask1) ? vmask1 : xform_pl1w & vmask1;

    if (TNOERR) {
      // TIME    | 
      msr.u32.lo |= (UINT32)(FX) << 21;
      msr.u32.lo |= (UINT32)(FY) << 17;
    }
  }
  else {
    msr.u32.lo &= notvmask1;
  }

  /////////
  // PL2 //
  /////////

  msr.u32.hi = (enablePL2) ?
    msr.u32.hi | 0x00008000l :
    msr.u32.hi & 0xffff7fffl;

  if (enablePL2)
  {
    // POWER
    msr.u32.hi &= notvmask1;
    msr.u32.hi |= (xform_pl2w > vmask1) ? vmask1 : xform_pl2w & vmask1;

    if (TNOERR) {
      // TIME    | 
      msr.u32.hi |= (UINT32)(FX) << 21;
      msr.u32.hi |= (UINT32)(FY) << 17;
    }
  }
  else {
    msr.u32.hi &= notvmask1;
  }

  if (gMCHBAR) {

    pm_mmio_write32(gMCHBAR + 0x59A0, msr.u32.lo);
    pm_mmio_write32(gMCHBAR + 0x59A4, msr.u32.hi);

    msr.u32.lo = (clamp) ?
      msr.u32.lo | 0x00010000l :
      msr.u32.lo & 0xfffeffffl;

    pm_mmio_or32(gMCHBAR + 0x59A0, msr.u32.lo);
    pm_mmio_or32(gMCHBAR + 0x59A4, msr.u32.hi);
  }

}


/*******************************************************************************
 * SetPlatformPowerLimit12
 ******************************************************************************/

VOID EFIAPI SetPlatformPowerLimit12(
  const UINT8 enablePL1,
  const UINT8 enablePL2,
  const UINT32 unitsW,
  const UINT8 clamp,
  const UINT32 pl1t,
  const UINT32 pl1w,    
  const UINT32 pl2w)
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;
  const UINT32 notvmask1 = 0xffff8000l;

  //
  // Back to Watts

  UINT32 xform_pl1w = MAX_POWAH;
  UINT32 xform_pl2w = MAX_POWAH;

  if (pl1w != MAX_POWAH)
  {
    xform_pl1w = pl1w * unitsW;
    xform_pl1w = (xform_pl1w) ? xform_pl1w / 1000 : 0;
  }
  
  if (pl2w != MAX_POWAH)
  {
    xform_pl2w = pl2w * unitsW;
    xform_pl2w = (xform_pl2w) ? xform_pl2w / 1000 : 0;
  }

  msr.u64 = pm_rdmsr64(MSR_PLATFORM_POWER_LIMIT);

  /////////
  // PL1 //
  /////////

  msr.u32.lo = (enablePL1) ?
    msr.u32.lo | 0x00008000l :
    msr.u32.lo & 0xffff7fffl;

  if (enablePL1)
  {
    // POWER
    msr.u32.lo &= notvmask1;
    msr.u32.lo |= (xform_pl1w > vmask1) ? vmask1 : xform_pl1w & vmask1;

    // TIME
    msr.u32.lo |= 0x00fe0000;
  }
  else {
    msr.u32.lo &= notvmask1;
  }

  /////////
  // PL2 //
  /////////

  msr.u32.hi = (enablePL2) ?
    msr.u32.hi | 0x00008000l :
    msr.u32.hi & 0xffff7fffl;

  if (enablePL2)
  {
    // POWER
    msr.u32.hi &= notvmask1;
    msr.u32.hi |= (xform_pl2w > vmask1) ? vmask1 : xform_pl2w & vmask1;

    // TIME
    msr.u32.hi |= 0x00fe0000;
  }
  else {
    msr.u32.hi &= notvmask1;
  }

  pm_wrmsr64(MSR_PLATFORM_POWER_LIMIT, msr.u64);

  MicroStall(3);

  //
  // Clamp

  msr.u64 = pm_rdmsr64(MSR_PLATFORM_POWER_LIMIT);

  msr.u32.lo = (clamp) ?
    msr.u32.lo | 0x00010000l :
    msr.u32.lo & 0xfffeffffl;

  pm_wrmsr64(MSR_PLATFORM_POWER_LIMIT, msr.u64);
}


/*******************************************************************************
 * SetPlatformPowerLimit3
 ******************************************************************************/

VOID EFIAPI SetPlatformPowerLimit3(
  const UINT8 enablePL3,
  const UINT32 unitsW,
  const UINT32 pl3t,
  const UINT32 pl3w )
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;
  const UINT32 notvmask1 = 0xffff8000l;

  //
  // Back to Watts

  UINT32 xform_pl3w = MAX_POWAH;

  if (pl3w != MAX_POWAH)
  {
    xform_pl3w = pl3w * unitsW;
    xform_pl3w = (xform_pl3w) ? xform_pl3w / 1000 : 0;
  }
  
  msr.u64 = pm_rdmsr64(MSR_PL3_CONTROL);

  /////////
  // PL3 //
  /////////

  msr.u32.lo = (enablePL3 && (xform_pl3w != 0)) ?
    msr.u32.lo | 0x00008000l :
    msr.u32.lo & 0xffff7fffl;

  if ((enablePL3) && (xform_pl3w != 0))
  {
    // POWER
    msr.u32.lo &= notvmask1;
    msr.u32.lo |= (xform_pl3w > vmask1) ? vmask1 : xform_pl3w & vmask1;

    // TIME
    msr.u32.lo |= 0x00fe0000;
  }
  else {
    msr.u32.lo &= notvmask1;
  }

  pm_wrmsr64(MSR_PL3_CONTROL, msr.u64);

  MicroStall(3);
}



/*******************************************************************************
 * SetPlatformPowerLimit4
 ******************************************************************************/

VOID EFIAPI SetPlatformPowerLimit4(
  const UINT8 enablePL4,
  const UINT32 pl4i)
{  
  {
    QWORD msr = { 0 };

    const UINT32 vmask1 = 0x1fff;
    const UINT32 notvmask1 = 0xffffe000;

    UINT32 xform_pl4 = MAX_POWAH;

    if (pl4i != MAX_POWAH) {
      xform_pl4 = pl4i * 8;
    }

    msr.u64 = pm_rdmsr64(MSR_VR_CURRENT_CONFIG);

    /////////
    // PL4 //
    /////////

    if (enablePL4 && (xform_pl4 != 0))
    {
      // POWER
      msr.u32.lo &= notvmask1;
      msr.u32.lo |= (xform_pl4 > vmask1) ? vmask1 : xform_pl4 & vmask1;
    }
    else {
      msr.u32.lo &= notvmask1;
    }

    pm_wrmsr64(MSR_VR_CURRENT_CONFIG, msr.u64);

    MicroStall(3);
  }
}

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
  const UINT32 pp0w)
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;
  const UINT32 notvmask1 = 0xffff8000l;

  //
  // Back to Watts

  UINT8 FX, FY, TNOERR;
  UINT32 xform_pp0w, xform_tau;

  xform_pp0w = MAX_POWAH;
  xform_tau = 0x7fff;

  TNOERR = FindTauConsts(pp0t, (UINT8)powerUnits, &FX, &FY);

  if (TNOERR) {
    FX &= 0x3;
    FY &= 0x1f;
  }

  if (pp0w != MAX_POWAH) {
    xform_pp0w = pp0w * powerUnits;
    xform_pp0w = (xform_pp0w) ? xform_pp0w / 1000 : 0;
  }

  //
  // If limits are enforced by the firmware, ensure that
  // we fall in between

  xform_pp0w = (PkgMinPow > 0) ? MAX(xform_pp0w, PkgMinPow) : xform_pp0w;
  xform_pp0w = (PkgMaxPow > 0) ? MIN(xform_pp0w, PkgMinPow) : xform_pp0w;
  xform_tau =  (PkgMaxTau > 0) ? MIN(xform_tau,  PkgMaxTau) : xform_tau;

  msr.u64 = pm_rdmsr64(MSR_PP0_POWER_LIMIT);

  /////////
  // PP0 //
  /////////

  msr.u32.lo = (enablePP0) ? 
    msr.u32.lo | 0x00008000l : msr.u32.lo & 0xffff7fffl;

  if (enablePP0)
  {
    // POWER
    msr.u32.lo &= notvmask1;
    msr.u32.lo |= (xform_pp0w > vmask1) ? vmask1 : xform_pp0w & vmask1;

    if (TNOERR) {
      // TIME
      msr.u32.lo |= (UINT32)(FX) << 21;
      msr.u32.lo |= (UINT32)(FY) << 17;
    }
  }
  else {
    msr.u32.lo &= notvmask1;
  }

  pm_wrmsr64(MSR_PP0_POWER_LIMIT, msr.u64);

  MicroStall(3);

  //
  // Clamp

  msr.u64 = pm_rdmsr64(MSR_PP0_POWER_LIMIT);

  msr.u32.lo = (clamp) ?
    msr.u32.lo | 0x00010000l :
    msr.u32.lo & 0xfffeffffl;

  pm_wrmsr64(MSR_PP0_POWER_LIMIT, msr.u64);
}

/*******************************************************************************
 * GetConfigTdpControl
 ******************************************************************************/

UINT64 EFIAPI GetConfigTdpControl()
{
  UINT64 val = pm_rdmsr64(MSR_CONFIG_TDP_CONTROL);
  return val;
}


/*******************************************************************************
 * GetConfigTdpLevel
 ******************************************************************************/

VOID EFIAPI GetCTDPLevel(UINT8 *level, UINT8 *locked)
{
  UINT64 val = pm_rdmsr64(MSR_CONFIG_TDP_CONTROL);

  *level = val & 0x03;
  *locked = (UINT8)((val >> 31) & 0x01);
}

/*******************************************************************************
 * SetCTDPLevel
 ******************************************************************************/

VOID EFIAPI SetCTDPLevel(const UINT8 level)
{
  UINT64 val = pm_rdmsr64(MSR_CONFIG_TDP_CONTROL);

  val &= 0xfffffffffffffffc;
  val |= (UINT64)(level) & 0x03;

  pm_wrmsr64(MSR_CONFIG_TDP_CONTROL, val);
}

/*******************************************************************************
 * SetCTDPLock
 ******************************************************************************/

VOID EFIAPI SetCTDPLock(const UINT8 lock)
{
  UINT64 val = pm_rdmsr64(MSR_CONFIG_TDP_CONTROL);
  UINT64 llock = (UINT64)(lock) << 31;

  val |= llock;

  pm_wrmsr64(MSR_CONFIG_TDP_CONTROL, val);
}