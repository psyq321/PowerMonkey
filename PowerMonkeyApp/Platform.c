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

#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/MpService.h>

#include "Platform.h"
#include "MpDispatcher.h"
#include "VFTuning.h"
#include "MiniLog.h"

#include "TurboRatioLimits.h"
#include "PowerLimits.h"
#include "Constants.h"
#include "OcMailbox.h"
#include "DelayX86.h"
#include "PrintStats.h"
#include "CpuData.h"
#include "LowLevel.h"

/*******************************************************************************
 * Globals
 ******************************************************************************/

 //
 // Initialized at startup

extern EFI_MP_SERVICES_PROTOCOL* gMpServices;
extern UINT8 gPrintPackageConfig;
extern UINT8 gPostProgrammingOcLock;
extern PLATFORM* gPlatform;

/*******************************************************************************
 * CoreIdxMap
 ******************************************************************************/

UINTN gNumCores = 0;
VOID* CoreIdxMap[MAX_CORES * MAX_PACKAGES] = { 0 };
UINTN CoreApicIDs[MAX_CORES * MAX_PACKAGES] = { 0 };

/*******************************************************************************
 * ApicIdToCoreNumber
 ******************************************************************************/

UINTN ApicIdToCoreNumber(const UINTN ApicID)
{
  for (UINTN ccount = 0; ccount < gNumCores; ccount++) {
    if (CoreApicIDs[ccount] == ApicID) {
      return ccount;
    }
  }

  return 0;
}


/*******************************************************************************
 * DiscoverVRTopology
 ******************************************************************************/

EFI_STATUS DiscoverVRTopology(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // We can use OC Mailbox Function 0x04 only if we:
  //   a) know how to parse the result
  // and
  //   b) CPU actually supports this
  //

  MiniTraceEx("Detecting VR Topology");

  for (UINTN didx = 0; didx < MAX_DOMAINS; didx++) {

    DOMAIN* dom = &pkg->planes[didx];

    dom->VRaddr = INVALID_VR_ADDR;
    dom->VRtype = NO_SVID_VR;

    if (gActiveCpuData->vtdt) {
      
      VOLTPLANEDESC* vguide = &gActiveCpuData->vtdt->doms[didx];

      //
      // Domain should exist, check if OC Mailbox can be used to probe it

      if (vguide->DomainExists) {

        if (vguide->DomainSupportedForDiscovery) {

          CpuMailbox box;
          MailboxBody* b = &box.b;

          //
          // Use command 0x04 to obtain VR info

          OcMailbox_InitializeAsMSR(&box);
          UINT32 cmd = OcMailbox_BuildInterface(0x04, 0x0, 0x0);
          status = OcMailbox_ReadWrite(cmd, 0, &box);

          if (box.status == 0) {

            const UINT32 amask  = vguide->OCMB_VRAddr_DomainBitMask;
            const UINT32 tmask  = vguide->OCMB_VRsvid_DomainBitMask;
            const UINT32 ashift = vguide->OCMB_VRAddr_DomainBitShift;

            dom->VRaddr = (UINT8)((b->box.data & amask) >> ashift);
            dom->VRtype = (UINT8)((b->box.data & tmask) ? NO_SVID_VR : SVID_VR);

          }
        }
        else {

          //
          // Domain cannot be probed by OC Mailbox
          // This means either incomplete info, 
          // or BIOS PCODE Mailbox must be used

          MiniTraceEx("Domain 0x%x cannot be probed using OC Mailbox", didx);
        }
      }
      else {
        MiniTraceEx("Skipping nonexistent voltage domain 0x%x", didx);
      }
    }
    else {
      MiniTraceEx("CPU information does not contain VR Topology discovery information");
    }
  }

  return status;
}

/*******************************************************************************
 * DomainSupported
 ******************************************************************************/

BOOLEAN VoltageDomainExists(const UINT8 didx)
{
  //
  // Check if we have this info in the table

  if (gActiveCpuData->vtdt) {
    return gActiveCpuData->vtdt->doms[didx].DomainExists;
  }
  
  //
  // Guess...

  if (didx == ECORE) {

    //
    // Check if we run on hybrid CPU

    if (gPlatform->packages[0].CpuInfo.HybridArch) {
      return 1;
    }

    return 0;
  }

  return 1;
}


/*******************************************************************************
 * ProbePackage
 ******************************************************************************/

EFI_STATUS EFIAPI ProbePackage(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  ///////////
  // CPUID //
  ///////////

  if (pkg->probed != 1) {
    
    GetCpuInfo(&pkg->CpuInfo);

    AsmCpuid(0x01,
      &pkg->CpuID,
      NULL, NULL, NULL);

    //////////////////////////
    // Discover VR Topology //
    //////////////////////////

    DiscoverVRTopology(pkg);
  }
  
  ////////////////////////
  // Initialize domains //
  ////////////////////////

  for (UINT8 didx = 0; didx < MAX_DOMAINS; didx++) {
    if (VoltageDomainExists(didx)) {
      DOMAIN* dom = pkg->planes + didx;

      if (pkg->probed != 1) {
        dom->parent = (void*)pkg;
      }
      
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

  pkg->probed = 1;

  return status;
}

/*******************************************************************************
 * ProgramPackageOrCore
 ******************************************************************************/

EFI_STATUS EFIAPI ProgramPackageOrCore(IN OUT PACKAGE* pkg)
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

  for (UINT8 didx = 0; didx < MAX_DOMAINS; didx++) {
    if (VoltageDomainExists(didx)) {
      DOMAIN* dom = pkg->planes + didx;

      if (pkg->Program_VF_Overrides[didx]) {
        IAPERF_ProgramDomainVF(didx, dom, pkg->Program_VF_Points[didx],
          pkg->Program_IccMax[didx]);
      }
    }
  }

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

    SetPkgPowerLimit12(
      IO_MSR,
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
      pkg->PkgTimeUnits,
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

EFI_STATUS EFIAPI ProgramPackage_Stage2(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // Power Limits MMIO Lock

  if (pkg->ProgramPL12_MMIO) {
    SetPL12MMIOLock(pkg->LockMmioPkgPL12);
  }

  return status;
}

/*******************************************************************************
 * ProgramPackage_Stage1
 ******************************************************************************/

EFI_STATUS EFIAPI ProgramPackage_Stage1(IN OUT PACKAGE* pkg)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // MMIO
  //
  // Power Limits (MMIO)

  if (pkg->ProgramPL12_MMIO)
  {
    SetPkgPowerLimit12(
      IO_MMIO,
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

  //
  // Platform (PSys) Power Limits
  
  if (pkg->ProgramPL12_PSys) 
  {
    SetPlatformPowerLimit12(
      pkg->EnablePlatformPL1,
      pkg->EnablePlatformPL2,
      pkg->PkgTimeUnits,
      pkg->PkgEnergyUnits,
      pkg->ClampPlatformPL,
      pkg->PlatformPL_Time,
      pkg->PlatformPL1_Power,
      pkg->PlatformPL2_Power);
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
  UINT32 prevPackage = 0xFFFFFFFF;

  UINTN nPackages = 0;
  UINTN nThreadsTotal = 0;
  UINTN nCoresTotal = 0;

  UINTN localCoreOrThreadCount = 0;

  pac->parent = psys;

  for (UINTN pidx = 0; pidx < psys->PkgCnt; pidx++) {
    PACKAGE* p = psys->packages + pidx;
    p->FirstCoreApicID = 0xFFFFFFFF;
    p->FirstCoreNumber = 0xFFFFFFFF;
  }

  for (UINTN tidx = 0; tidx < psys->LogicalProcessors; tidx++) {

    EFI_PROCESSOR_INFORMATION pi;

    gMpServices->GetProcessorInfo(gMpServices, tidx, &pi);

    if (prevPackage == 0xFFFFFFFF)
      prevPackage = pi.Location.Package;

    CPUCORE* core = &pac->Core[localCoreOrThreadCount];

    core->LocalIdx = (UINT8) localCoreOrThreadCount;
    core->ApicID = pi.ProcessorId;
    core->AbsIdx = tidx;
    core->parent = (VOID*)pac;
    core->PkgIdx = (UINT8) nPackages;
    pac->idx = (UINT64)nPackages;

    CoreIdxMap[tidx] = (VOID*)&pac->Core[localCoreOrThreadCount];
    CoreApicIDs[tidx] = pac->Core[localCoreOrThreadCount].ApicID;

    const BOOLEAN physCore = (pi.Location.Thread == 0) ? 1 : 0;
        
    pac->LogicalCores += 1;
    pac->PhysicalCores += (physCore) ? 1 : 0;

    pac->Core[localCoreOrThreadCount].IsPhysical = physCore;

    if (pac->FirstCoreApicID == 0xFFFFFFFF) {
      pac->FirstCoreApicID = pi.ProcessorId;
    }

    if (pac->FirstCoreNumber == 0xFFFFFFFF) {
      pac->FirstCoreNumber = tidx;
    }      
    
    nThreadsTotal++;
    localCoreOrThreadCount++;
    nCoresTotal += (pi.Location.Thread == 0) ? 1 : 0;

    //AsciiPrint("[Tidx %u] apic id: %lu, abs idx: %lu, pkg idx: %u\n", tidx, core->ApicID, core->AbsIdx, core->PkgIdx);

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

  psys->LogicalProcessors = gNumCores = nThreadsTotal;
  psys->PkgCnt = nPackages + 1;

  return status;
}

/*******************************************************************************
* ProbePackages
******************************************************************************/

EFI_STATUS EFIAPI ProbePackages(IN OUT PLATFORM* ppd)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // Probe each package in its own context

  for (UINTN pidx = 0; pidx < ppd->PkgCnt; pidx++)
  {
    PACKAGE* pac = ppd->packages + pidx;

    status = RunOnPackageOrCore(ppd, pac->FirstCoreNumber, (EFI_AP_PROCEDURE)ProbePackage, pac);

    if (EFI_ERROR(status)) {
      Print(L"[ERROR] CPU package %u, status code: 0x%x\n",
        pac->FirstCoreNumber,
        status);

      return status;
    }
  }

  return status;
}

/*******************************************************************************
* DiscoverPlatform
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
  
  ProbePackages(ppd);
  
  return EFI_SUCCESS;
}

/*******************************************************************************
 * PrintPlatformInfo 
 ******************************************************************************/

VOID PrintPlatformInfo(IN PLATFORM* psys)
{
  PMUNUSED(psys);
}


/*******************************************************************************
 * ProgramCoreLocks
 ******************************************************************************/

EFI_STATUS EFIAPI ProgramCoreKnobs(PLATFORM* psys)
{
  //
  // Hack - assuming all packages are the same!!!!

  PACKAGE* pk = &psys->packages[0];

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

  // PSys Lock

  if (pk->ProgramPL12_PSys) {
    SetPSysLock(pk->LockPlatformPL);
  }

  //
  // cTDP Lock

  SetCTDPLock(pk->TdpControLock);

  //
  // Overclocking Lock

  if (gPostProgrammingOcLock) {
    IaCore_OcLock();
  }

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
  PMUNUSED(SystemTable);

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
  PMUNUSED(SystemTable);
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

    //for (INTN cidx = pk->LogicalCores; cidx >= 0; --cidx)
    for (UINTN cidx = 0; cidx < pk->LogicalCores; cidx++)
    {
      CPUCORE* core = pk->Core + cidx;
      RunOnPackageOrCore(sys, core->AbsIdx, (EFI_AP_PROCEDURE)ProgramPackageOrCore, pk);
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
    RunOnPackageOrCore(sys, pk->FirstCoreNumber, (EFI_AP_PROCEDURE)ProgramPackage_Stage1, pk);
  }

  /////////////////
  // Apply LOCKS //
  /////////////////

  //
  // MSR Locks

  RunOnAllProcessors((EFI_AP_PROCEDURE)ProgramCoreKnobs, FALSE, (void *)sys);

  //
  // MMIO locks

  for (UINTN pidx = 0; pidx < sys->PkgCnt; pidx++)
  {
    PACKAGE* pk = sys->packages + pidx;
    RunOnPackageOrCore(sys, pk->FirstCoreNumber, (EFI_AP_PROCEDURE)ProgramPackage_Stage2, pk);
  }

  ////////////////////
  // PRINT SETTINGS //
  ////////////////////

  {
    ProbePackages(sys);

    PrintPlatformSettings(sys);

    PrintVFPoints(sys);
  }

  return status;
}

/*******************************************************************************
* GetCpuDataBlock
******************************************************************************/

VOID* GetCpuDataBlock()
{
  UINTN processorNumber;
  gMpServices->WhoAmI(gMpServices, &processorNumber);
  VOID* coreStructAddr = CoreIdxMap[processorNumber];
  return coreStructAddr;
}
