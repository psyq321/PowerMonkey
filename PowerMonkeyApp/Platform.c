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

#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/MpService.h>

#include "Platform.h"
#include "MpDispatcher.h"
#include "VFTuning.h"

#include "TurboRatioLimits.h"
#include "PowerLimits.h"
#include "DelayX86.h"

//
// Initialized at startup

extern EFI_MP_SERVICES_PROTOCOL* gMpServices;

/*******************************************************************************
 * ProbePackage
 ******************************************************************************/

EFI_STATUS ProbePackage(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // CPUID

  AsmCpuid(0x01,
    &pkg->CpuID,
    NULL, NULL, NULL);

  //
  // Initialize domains

  for (UINT8 didx = 0; didx < MAX_DOMAINS; didx++) {
    DOMAIN* dom = pkg->Domain + didx;

    dom->parent = (void*)pkg;

    if ((didx == IACORE) || (didx == RING)) {
      IAPERF_ProbeDomainVF(didx, dom);
    }
  }

  //
  // Turbo Ratio Limits (this assumes all packages are the same)

  pkg->TurboRatioLimits = GetTurboRatioLimits();

  //
  // cTDP Levels

  pkg->ConfigTdpControl = GetConfigTdpControl();

  GetCTDPLevel(
    &pkg->MaxCTDPLevel,
    &pkg->TdpControLock);

  //
  // Power Limits and units

  pkg->MsrPkgPowerLimits = GetPkgPowerLimits(
    &pkg->MsrPkgMaxTau,
    &pkg->MsrPkgMinPL1,
    &pkg->MsrPkgMaxPL1
  );

  pkg->PkgPowerUnits = GetPkgPowerUnits(
    &pkg->PkgTimeUnits,
    &pkg->PkgEnergyUnits);

  return status;
}

/*******************************************************************************
 * ProgramPackageOrCore
 ******************************************************************************/

EFI_STATUS ProgramPackageOrCore(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // Force max ratio for all turbo core counts
  // (if requested) 

  if (pkg->ForcedRatioForAllCoreCounts) {
    IAPERF_ProgramMaxTurboRatios(pkg->ForcedRatioForAllCoreCounts);
  }

  //
  // Program V/F overrides

  IAPERF_ProgramDomainVF(IACORE, &pkg->Domain[IACORE]);
  IAPERF_ProgramDomainVF(RING, &pkg->Domain[RING]);

  //
  // Program Config TDP Params
  // with no lock 

  SetCTDPLevel(pkg->MaxCTDPLevel);

  ///////////////////
  // Power Limits  //
  /////////////////// 

  //
  // PL1 and PL2

  if (pkg->ProgramPL12_MSR) {

    SetPkgPowerLimit12_MSR(
      pkg->MsrPkgMaxTau,
      pkg->MsrPkgMinPL1,
      pkg->MsrPkgMaxPL1,
      pkg->EnableMsrPkgPL1,
      pkg->EnableMsrPkgPL2,
      pkg->PkgTimeUnits,
      pkg->PkgEnergyUnits,
      pkg->PkgPowerUnits,
      pkg->ClampMsrPkgPL,
      pkg->MsrPkgPL_Time,
      pkg->MsrPkgPL1_Power,
      pkg->MsrPkgPL2_Power);
  }

  //
  // PL3

  if (pkg->ProgramPL3) {

    SetPlatformPowerLimit3(
      pkg->EnableMsrPkgPL3,
      pkg->PkgPowerUnits,
      pkg->MsrPkgPL3_Time,
      pkg->MsrPkgPL3_Power);
  }

  //
  // PL4

  if (pkg->ProgramPL4) {

    SetPlatformPowerLimit4(
      pkg->EnableMsrPkgPL4,
      pkg->MsrPkgPL4_Current);
  }

  //
  // PP0

  if (pkg->ProgramPP0) {

    SetPP0PowerLimit(
      pkg->MsrPkgMaxTau,
      pkg->MsrPkgMinPL1,
      pkg->MsrPkgMaxPL1,
      pkg->EnableMsrPkgPP0,
      pkg->PkgTimeUnits,
      pkg->PkgPowerUnits,
      pkg->ClampMsrPP0,
      pkg->MsrPkgPP0_Time,
      pkg->MsrPkgPP0_Power
    );

  }

  return status;
}

/*******************************************************************************
 * ProgramPackage_Stage2
 * Program package locks in the separate stage, after everything else is done
 ******************************************************************************/

EFI_STATUS ProgramPackage_Stage2(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // Power Limits MMIO Lock

  if (pkg->LockMmioPkgPL12) {
    SetPL12MMIOLock(1);
  }

  return status;
}

/*******************************************************************************
 * ProgramPackage_Stage1
 ******************************************************************************/

EFI_STATUS ProgramPackage_Stage1(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // MMIO
  //
  // Power Limits (MMIO)

  if (pkg->ProgramPL12_MMIO)
  {
    SetPkgPowerLimit12_MMIO(
      pkg->MsrPkgMaxTau,
      pkg->MsrPkgMinPL1,
      pkg->MsrPkgMaxPL1,
      pkg->EnableMmioPkgPL1,
      pkg->EnableMmioPkgPL2,
      pkg->PkgTimeUnits,
      pkg->PkgEnergyUnits,
      pkg->PkgPowerUnits,
      pkg->ClampMmioPkgPL,
      pkg->MmioPkgPL_Time,
      pkg->MmioPkgPL1_Power,
      pkg->MmioPkgPL2_Power);
  }

  return status;
}


/*******************************************************************************
 * DiscoverPackage
 ******************************************************************************/

EFI_STATUS DetectPackages(IN OUT PLATFORM* psys)
{
  EFI_STATUS status = EFI_SUCCESS;

  PACKAGE* pac = &psys->packages[0];
  UINTN prevPackage = 0xFFFFFFF;

  UINTN nPackages = 0;
  UINTN nThreadsTotal = 0;
  UINTN nCoresTotal = 0;

  UINTN localCoreOrThreadCount = 0;

  pac->parent = psys;

  for (UINTN tidx = 0; tidx < psys->LogicalProcessors; tidx++) {

    EFI_PROCESSOR_INFORMATION pi;

    gMpServices->GetProcessorInfo(gMpServices, tidx, &pi);

    pac->LogicalCores += 1;
    pac->PhysicalCores += (pi.Location.Thread == 0) ? 1 : 0;
    pac->FirstCoreApicID = pi.ProcessorId;

    pac->Core[localCoreOrThreadCount].ApicID = pi.ProcessorId;

    nThreadsTotal++;
    nCoresTotal += (pi.Location.Thread == 0) ? 1 : 0;

    if (pi.Location.Package != prevPackage)
    {
      //
      // New package detected

      pac->parent = (VOID*)psys;

      nPackages++;
      prevPackage = pi.Location.Package;
      localCoreOrThreadCount = 0;
      pac++;
    }
  }

  psys->LogicalProcessors = nThreadsTotal;
  psys->PkgCnt = nPackages;

  //
  // Hack - tbd, remove!

  if (psys->PkgCnt == 1) {
    status = gMpServices->WhoAmI(
      gMpServices,
      &psys->packages[0].FirstCoreApicID);
  }

  return status;
}

/*******************************************************************************
* DiscoverPlatform
*
* TODO/TBD/HACK: multi-socket code needs rework.
******************************************************************************/

EFI_STATUS EFIAPI DiscoverPlatform(IN OUT PLATFORM** ppsys)
{

  EFI_STATUS status = EFI_SUCCESS;

  //
  // Allocate memory that will hold platform info

  *ppsys = (PLATFORM*)AllocateZeroPool(sizeof(PLATFORM));

  if (!*ppsys) {
    return EFI_OUT_OF_RESOURCES;
  }

  PLATFORM* ppd = *ppsys;

  //
  // Get bootstrap CPU

  status = gMpServices->WhoAmI(gMpServices, &ppd->BootProcessor);

  if (EFI_ERROR(status)) {

    Print(
      L"[ERROR] Unable to get bootstrap processor (error: 0x%x)\n", status);

    return status;
  }

  //
  // We start with the logical processor count

  status = gMpServices->GetNumberOfProcessors(
    gMpServices,
    &ppd->LogicalProcessors,
    &ppd->EnabledLogicalProcessors
  );

  if (EFI_ERROR(status)) {
    return status;
  }

  ppd->PkgCnt = 1;                    // Boot Processor

  DetectPackages(ppd);

  //
  // Probe each package in its own context

  for (UINTN pidx = 0; pidx < ppd->PkgCnt; pidx++)
  {
    PACKAGE* pac = ppd->packages + pidx;

    status = RunOnPackageOrCore(ppd, pac->FirstCoreApicID, ProbePackage, pac);

    if (EFI_ERROR(status)) {
      Print(L"[ERROR] CPU package %u, status code: 0x%x\n",
        pac->FirstCoreApicID,
        status);

      return status;
    }
  }

  return EFI_SUCCESS;
}

VOID PrintPlatformInfo(IN PLATFORM* Platform)
{

}

/*******************************************************************************
 * ProgramCoreLocks
 ******************************************************************************/

EFI_STATUS EFIAPI ProgramCoreKnobs(PLATFORM* Platform)
{
  //
  // Hack - assuming all packages are the same!!!!

  PACKAGE* pk = &Platform->packages[0];

  //
  // Power Control
  
  if (pk->ProgramPowerControl) {
    ProgramPowerCtl(pk->EnableEETurbo, pk->EnableRaceToHalt);
  }

  //
  // PL1/2 Lock (MSR)

  if (pk->ProgramPL12_MSR) {
    SetPL12MSRLock(pk->LockMsrPkgPL12);
  }

  //
  // PL3 Lock

  if (pk->ProgramPL3) {
    SetPL3Lock(pk->LockMsrPkgPL3);
  }

  // PL4 Lock

  if (pk->ProgramPL4) {
    SetPL4Lock(pk->LockMsrPkgPL4);
  }

  //
  // PP0 Lock

  if (pk->ProgramPP0) {
    SetPP0Lock(pk->LockMsrPP0);
  }

  //
  // cTDP Lock

  SetCTDPLock(pk->TdpControLock);

  //
  // Overclocking Lock

  IaCore_OcLock();

  return EFI_SUCCESS;
}


/*******************************************************************************
 * TBD / TODO: Needs Rewrite
 ******************************************************************************/

EFI_STATUS
EFIAPI
StartupPlatformInit(
  IN EFI_SYSTEM_TABLE* SystemTable,
  IN OUT PLATFORM** Platform
) {
  EFI_STATUS status = EFI_SUCCESS;

  status = DiscoverPlatform(Platform);

  if (EFI_ERROR(status)) {
    return status;
  }

  PLATFORM* sys = *Platform;

  PrintPlatformInfo(sys);

  return status;
}

/*******************************************************************************
 * TBD / TODO: Needs Rewrite
 ******************************************************************************/

EFI_STATUS EFIAPI ApplyPolicy(IN EFI_SYSTEM_TABLE* SystemTable,
  IN OUT PLATFORM* sys)
{
  EFI_STATUS status = EFI_SUCCESS;

  /////////////////
  // PROGRAMMING //
  /////////////////

  ApplyComputerOwnersPolicy(sys);

  //
  // Strictly speaking, we do not need to program every core
  // Performing programming once per package would be sufficient (except for
  // parameters not yet supported here anyway). However, there is one scenario 
  // where isolated core would use its own programmed settings (one would need 
  // to power off other cores in package, though)
  //
  // So we will program every core, for the sake of completeness...

  for (UINTN pidx = 0; pidx < sys->PkgCnt; pidx++)
  {
    PACKAGE* pk = sys->packages + pidx;

    for (UINTN cidx = 0; cidx < sys->LogicalProcessors; cidx++)
    {
      CPUCORE* core = pk->Core + cidx;
      RunOnPackageOrCore(sys, core->ApicID, ProgramPackageOrCore, pk);
    }
  }

  //////////
  // MISC //
  //////////

  //
  // Power Limits (MMIO)

  for (UINTN pidx = 0; pidx < sys->PkgCnt; pidx++)
  {
    PACKAGE* pk = sys->packages + pidx;
    RunOnPackageOrCore(sys, pk->FirstCoreApicID, ProgramPackage_Stage1, pk);
  }

  /////////////////
  // Apply LOCKS //
  /////////////////

  //
  // MSR Locks

  RunOnAllProcessors(sys, ProgramCoreKnobs, sys);

  //
  // MMIO locks

  for (UINTN pidx = 0; pidx < sys->PkgCnt; pidx++)
  {
    PACKAGE* pk = sys->packages + pidx;
    RunOnPackageOrCore(sys, pk->FirstCoreApicID, ProgramPackage_Stage2, pk);
  }

  return status;
}


