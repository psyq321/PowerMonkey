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
#include <Library/TimerLib.h>
#include <Library/IoLib.h>

#include "VFTuning.h"
#include "LowLevel.h"
#include "DelayX86.h"
#include "TimeWindows.h"
#include "Constants.h"
#include "MiniLog.h"

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
  
  *pMsrPkgMaxPL1 = msr.u32.hi  & 0x00003fff;
  *pMsrPkgMinPL1 = (msr.u32.lo & 0x3fff0000) >> 16;  
  *pMsrPkgMaxTau = (msr.u32.hi & 0x3f0000) >> 16;

  msr.u64 = pm_rdmsr64(MSR_PKG_POWER_INFO);
  return msr.u64;
}

/*******************************************************************************
 * SetPkgPowerLimit12
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
  const UINT32 pl2w)
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;

  //
  // Back to Watts

  UINT32 xform_pl1w, xform_pl2w, xform_tau;

  UINT8 FX = 0;
  UINT8 FY = 0;
  UINT8 TNOERR = 0;

  xform_pl1w = xform_pl2w = MAX_POWAH;
  xform_tau = 0x7fff;

  TNOERR = FindTauConsts(pl1t, (UINT8)timeUnits, &FX, &FY);

  if (!TNOERR) {
    return;
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
  xform_tau = (PkgMaxTau > 0) ?  MIN(xform_tau, PkgMaxTau) : xform_tau;

  msr.u64 = pm_xio_read64( dst, (dst==IO_MSR) ? 
    MSR_PACKAGE_POWER_LIMIT :
    MMIO_PACKAGE_POWER_LIMIT );

  if (!(msr.u32.hi & bit31u32)) {        // do not attempt to write locked MSR

    MiniTraceEx("Setting Pkg PL1/2: PL1=0x%x, PL2: 0x%x, tau: %u X=%u, Y=%u",
      xform_pl1w,
      xform_pl2w,
      pl1t,
      FX,
      FY);

    /////////
    // PL1 //
    /////////

    msr.u32.lo = (enablePL1) ?
      msr.u32.lo | bit15u32 :
      msr.u32.lo & ~bit15u32;

    msr.u32.lo &= ~vmask1;

    if (enablePL1)
    {
      // POWER
      msr.u32.lo |= (xform_pl1w > vmask1) ? vmask1 : xform_pl1w & vmask1;

      if (TNOERR) {
        // TIME
        msr.u32.lo &= 0xff01ffff;
        msr.u32.lo |= (UINT32)(FX) << 22;
        msr.u32.lo |= (UINT32)(FY) << 17;
      }
    }
    else {
      msr.u32.lo &= 0xff01ffff;
    }

    /////////
    // PL2 //
    /////////

    msr.u32.hi = (enablePL2) ?
      msr.u32.hi | bit15u32 :
      msr.u32.hi & ~bit15u32;

    msr.u32.hi &= ~vmask1;

    if (enablePL2)
    {
      // POWER    
      msr.u32.hi |= (xform_pl2w > vmask1) ? vmask1 : xform_pl2w & vmask1;

      if (TNOERR) {
        // TIME
        msr.u32.hi &= 0xff01ffff;
        msr.u32.hi |= (UINT32)(FX) << 22;
        msr.u32.hi |= (UINT32)(FY) << 17;
      }
    }
    else {
      msr.u32.hi &= 0xff01ffff;
    }

    pm_xio_write64(dst, (dst == IO_MSR) ?
      MSR_PACKAGE_POWER_LIMIT :
      MMIO_PACKAGE_POWER_LIMIT,
      msr.u64);

    MicroStall(3);

    //
    // Clamp

    msr.u64 = pm_xio_read64(dst, (dst == IO_MSR) ?
      MSR_PACKAGE_POWER_LIMIT :
      MMIO_PACKAGE_POWER_LIMIT);

    msr.u32.lo = (clamp) ?
      msr.u32.lo | bit16u32 :
      msr.u32.lo & ~bit16u32;

    msr.u32.hi = (clamp) ?
      msr.u32.hi | bit16u32 :
      msr.u32.hi & ~bit16u32;

    pm_xio_write64(dst, (dst == IO_MSR) ?
      MSR_PACKAGE_POWER_LIMIT :
      MMIO_PACKAGE_POWER_LIMIT,
      msr.u64 );

    MicroStall(3);
  }
}

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
  const UINT32 pl2w)
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;

  MiniTraceEx("Setting Platform PL1/2 Limits");

  //
  // Back to Watts

  UINT32 xform_pl1w = MAX_POWAH;
  UINT32 xform_pl2w = MAX_POWAH;
  UINT32 xform_pl1t = 0x7F;

  UINT8 FX = 0;
  UINT8 FY = 0;
  UINT8 TNOERR = 0;


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

  xform_pl1t = (pl1t > 0x7F) ? 0x7F : pl1t;

  TNOERR = FindTauConsts(pl1t, (UINT8)unitsT, &FX, &FY);

  if (!TNOERR) {
    return;
  }


  msr.u64 = pm_rdmsr64(MSR_PLATFORM_POWER_LIMIT);

  /////////
  // PL1 //
  /////////

  msr.u32.lo = (enablePL1) ?
    msr.u32.lo | bit15u32 :
    msr.u32.lo & ~bit15u32;

  if (enablePL1) {
    // POWER
    msr.u32.lo &= ~vmask1;
    msr.u32.lo |= (xform_pl1w > vmask1) ? vmask1 : xform_pl1w & vmask1;

    // TIME
    if (TNOERR) {
      msr.u32.lo &= 0xff01ffff;
      msr.u32.lo |= (UINT32)(FX) << 22;
      msr.u32.lo |= (UINT32)(FY) << 17;
    }
    else {
      msr.u32.lo |= xform_pl1t << 17;
    }

  }
  else {
    msr.u32.lo &= ~vmask1;
    msr.u32.lo &= 0xff01ffff;
  }

  /////////
  // PL2 //
  /////////

  msr.u32.hi = (enablePL2) ?
    msr.u32.hi | bit15u32 :
    msr.u32.hi & ~bit15u32;

  if (enablePL2)
  {
    // POWER
    msr.u32.hi &= ~vmask1;
    msr.u32.hi |= (xform_pl2w > vmask1) ? vmask1 : xform_pl2w & vmask1;
  }
  else {
    msr.u32.hi &= ~vmask1;
  }

  pm_wrmsr64(MSR_PLATFORM_POWER_LIMIT, msr.u64);
  MicroStall(3);

  {
    ///////////
    // Clamp //
    ///////////

    QWORD numsr = { 0 };

    msr.u64 = numsr.u64 = pm_rdmsr64(MSR_PLATFORM_POWER_LIMIT);

    //
    // PL1

    numsr.u32.lo = (clamp) ?
      numsr.u32.lo | bit16u32 :
      numsr.u32.lo & ~bit16u32;

    //
    // PL2

    numsr.u32.hi = (clamp) ?
      numsr.u32.hi | bit16u32 :
      numsr.u32.hi & ~bit16u32;

    if (numsr.u64 != msr.u64) {
      pm_wrmsr64(MSR_PLATFORM_POWER_LIMIT, numsr.u64);
      MicroStall(3);
    }
  }
}


/*******************************************************************************
 * SetPL12MSRLock
 ******************************************************************************/

VOID EFIAPI SetPL12MSRLock( const UINT8 lock )
{  
  if (lock < 2) {
    
    QWORD msr = { 0 };

    msr.u64 = pm_rdmsr64(MSR_PACKAGE_POWER_LIMIT);

    if (!(msr.u32.hi & bit31u32)) {        // do not attempt to write locked MSR

      msr.u32.hi = (lock) ?
        msr.u32.hi | bit31u32 :
        msr.u32.hi & ~bit31u32;

      pm_wrmsr64(MSR_PACKAGE_POWER_LIMIT, msr.u64);

      MicroStall(3);

    }
  }
}

/*******************************************************************************
 * SetPL12MMIOLock
 ******************************************************************************/

VOID EFIAPI SetPL12MMIOLock(const UINT8 lock)
{
  if (lock < 2) {

    MiniTraceEx("Setting MMIO PL1/2 Lock");

    QWORD msr = { 0 };

    msr.u64 = pm_rdmsr64(MSR_PACKAGE_POWER_LIMIT);

    msr.u32.hi = (lock) ?
      msr.u32.hi | bit31u32 :
      msr.u32.hi & ~bit31u32;

    if (gMCHBAR) {
      pm_mmio_or32(gMCHBAR + MMIO_PACKAGE_POWER_LIMIT,    msr.u32.lo);
      pm_mmio_or32(gMCHBAR + MMIO_PACKAGE_POWER_LIMIT_HI, msr.u32.hi);
    }

    MicroStall(3);
  }
}

/*******************************************************************************
 * SetPL3Lock
 ******************************************************************************/

VOID EFIAPI SetPL3Lock(const UINT8 lock)
{
  if (lock < 2) {

    MiniTraceEx("Setting PL3 Lock");

    QWORD msr = { 0 };

    msr.u64 = pm_rdmsr64(MSR_PL3_CONTROL);

    msr.u32.lo = (lock) ?
      msr.u32.lo | bit31u32 :
      msr.u32.lo & ~bit31u32;

    pm_wrmsr64(MSR_PL3_CONTROL, msr.u64);

    MicroStall(3);
  }
}

/*******************************************************************************
 * SetPL4Lock
 ******************************************************************************/

VOID EFIAPI SetPL4Lock(const UINT8 lock)
{
  if (lock < 2) {

    MiniTraceEx("Setting PL4 Lock");

    QWORD msr = { 0 };

    msr.u64 = pm_rdmsr64(MSR_VR_CURRENT_CONFIG);

    msr.u32.lo = (lock) ?
      msr.u32.lo | bit31u32 :
      msr.u32.lo & ~bit31u32;

    pm_wrmsr64(MSR_VR_CURRENT_CONFIG, msr.u64);

    MicroStall(3);
  }
}

/*******************************************************************************
 * SetPSysLock
 ******************************************************************************/

VOID EFIAPI SetPSysLock(const UINT8 lock)
{
  if (lock < 2) {

    MiniTraceEx("Setting PSys Lock");

    QWORD msr = { 0 };

    msr.u64 = pm_rdmsr64(MSR_PLATFORM_POWER_LIMIT);

    msr.u32.hi = (lock) ?
      msr.u32.hi | bit31u32 :
      msr.u32.hi & ~bit31u32;

    pm_wrmsr64(MSR_PLATFORM_POWER_LIMIT, msr.u64);

    MicroStall(3);
  }
}


/*******************************************************************************
 * SetPP0Lock
 ******************************************************************************/

VOID EFIAPI SetPP0Lock(const UINT8 lock)
{
  if (lock < 2) {

    MiniTraceEx("Setting PP0 Lock");

    QWORD msr = { 0 };

    msr.u64 = pm_rdmsr64(MSR_PP0_POWER_LIMIT);

    msr.u32.lo = (lock) ?
      msr.u32.lo | bit31u32 :
      msr.u32.lo & ~bit31u32;

    pm_wrmsr64(MSR_PP0_POWER_LIMIT, msr.u64);

    MicroStall(3);
  }
}

/*******************************************************************************
 * SetPlatformPowerLimit3
 ******************************************************************************/

VOID EFIAPI SetPlatformPowerLimit3(
  const UINT8 enablePL3,
  const UINT32 unitsT,
  const UINT32 unitsW,
  const UINT32 pl3t,
  const UINT32 pl3w )
{
  QWORD msr = { 0 };

  const UINT32 vmask1 = 0x7fff;

  MiniTraceEx("Setting Platform PL3 Limit");


  //
  // Back to Watts

  UINT32 xform_pl3w = MAX_POWAH;
  
  UINT8 FX, FY, TNOERR;

  TNOERR = FindTauConsts(pl3t, (UINT8)unitsT, &FX, &FY);

  if (!TNOERR) {
    return;
  }

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
    msr.u32.lo | bit15u32 :
    msr.u32.lo & ~bit15u32;

  msr.u32.lo &= ~vmask1;

  if ((enablePL3) && (xform_pl3w != 0))
  {
    // POWER    
    msr.u32.lo |= (xform_pl3w > vmask1) ? vmask1 : xform_pl3w & vmask1;

    // TIME

    if (TNOERR) {
      // TIME
      msr.u32.lo &= 0xff01ffff;
      msr.u32.lo |= (UINT32)(FX) << 22;
      msr.u32.lo |= (UINT32)(FY) << 17;
    }
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

    MiniTraceEx("Setting Platform PL4 Limit");

    const UINT32 vmask1 = 0x1fff;

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
      msr.u32.lo &= ~vmask1;
      msr.u32.lo |= (xform_pl4 > vmask1) ? vmask1 : xform_pl4 & vmask1;
    }
    else {
      msr.u32.lo &= ~vmask1;
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

  MiniTraceEx("Setting PP0 Limit");

  //
  // Back to Watts

  UINT8 FX, FY, TNOERR;
  UINT32 xform_pp0w, xform_tau;

  xform_pp0w = MAX_POWAH;
  xform_tau = 0x7fff;

  TNOERR = FindTauConsts(pp0t, (UINT8)powerUnits, &FX, &FY);

  if (!TNOERR) {
    return;
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
    msr.u32.lo | bit15u32 : msr.u32.lo & ~bit15u32;

  msr.u32.lo &= ~vmask1;

  if (enablePP0)
  {
    // POWER
    
    msr.u32.lo |= (xform_pp0w > vmask1) ? vmask1 : xform_pp0w & vmask1;

    if (TNOERR) {
      // TIME
      msr.u32.lo &= 0xff01ffff;
      msr.u32.lo |= (UINT32)(FX) << 22;
      msr.u32.lo |= (UINT32)(FY) << 17;
    }
  }

  pm_wrmsr64(MSR_PP0_POWER_LIMIT, msr.u64);
  MicroStall(3);

  //
  // Clamp

  msr.u64 = pm_rdmsr64(MSR_PP0_POWER_LIMIT);

  msr.u32.lo = (clamp) ?
    msr.u32.lo | bit16u32 :
    msr.u32.lo & ~bit16u32;

  pm_wrmsr64(MSR_PP0_POWER_LIMIT, msr.u64);
}

/*******************************************************************************
 * GetConfigTdpControl
 ******************************************************************************/

UINT64 EFIAPI GetConfigTdpControl(VOID)
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
  QWORD msr = { 0 };

  msr.u64 = pm_rdmsr64(MSR_CONFIG_TDP_CONTROL);

  if (!(msr.u32.lo & bit31u32)) {        // do not attempt to write locked MSR
    
    msr.u64 &= 0xfffffffffffffffc;
    msr.u64 |= (UINT64)(level) & 0x03;

    pm_wrmsr64(MSR_CONFIG_TDP_CONTROL, msr.u64);
  }
}

/*******************************************************************************
 * SetCTDPLock
 ******************************************************************************/

VOID EFIAPI SetCTDPLock(const UINT8 lock)
{
  QWORD msr = { 0 };

  msr.u64 = pm_rdmsr64(MSR_CONFIG_TDP_CONTROL);

  if (!(msr.u32.lo & bit31u32)) {        // do not attempt to write locked MSR
    
    if (lock < 2) {

      msr.u32.lo = (lock) ?
        msr.u32.lo | bit31u32 :
        msr.u32.lo & ~bit31u32;
    }

    pm_wrmsr64(MSR_CONFIG_TDP_CONTROL, msr.u64);
  }
}

/*******************************************************************************
 * ProgramPowerCtl
 ******************************************************************************/

VOID EFIAPI ProgramPowerCtl(const UINT8 eeTurbo, const UINT8 rtHlt)
{
  UINT8 wrt = 0;
  QWORD msr = { 0 };

  msr.u64 = pm_rdmsr64(MSR_POWER_CONTROL);

  ////////////////////////////
  // Energy Efficient Turbo //
  ////////////////////////////

  if (eeTurbo < 2) {

    //
    // MSR_POWER_CONTROL[20]: 1=EET DISABLED, 0=EET ENABLED

    msr.u32.lo = (eeTurbo) ?
      msr.u32.lo & ~bit19u32 :
      msr.u32.lo | bit19u32;

    wrt |= 0x1;
  }

  //////////////////
  // Race to Halt //
  //////////////////

  if (rtHlt < 2) {

    //
    // MSR_POWER_CONTROL[20]: 1=RTH DISABLED, 0=RTH ENABLED
    
    msr.u32.lo = (rtHlt) ?
      msr.u32.lo & ~bit20u32 :
      msr.u32.lo | bit20u32;

    wrt |= 0x1;
  }

  if (wrt) {
    pm_wrmsr64(MSR_POWER_CONTROL, msr.u64);
  }  
}
