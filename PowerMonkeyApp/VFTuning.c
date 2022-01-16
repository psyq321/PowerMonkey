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

#include "VFTuning.h"
#include "LowLevel.h"
#include "OcMailbox.h"
#include "Constants.h"
#include "FixedPoint.h"
#include "MiniLog.h"
#include "CpuData.h"

/*******************************************************************************
 * Layout of the CPU Overclocking mailbox can be found in academic papers:
 *
 * DOI: 10.1109/SP40000.2020.00057 *
 * "Figure 1: Layout of the undocumented undervolting MSR with address 0x150"
 *
 * Command IDs are also often mentioned in BIOS configuration help, or in 
 * officially released Intel Firmware Support Packages, e.g.:
 *
 * https://github.com/intel/FSP/blob/master/CometLakeFspBinPkg/ \
 * CometLake1/Include/FspmUpd.h  (merge this line with the hyper link)
 *
 * V/F Curve theory-of-operation (for wide audience) can be seen explained
 * in this ScatterBencher video: https://www.youtube.com/watch?v=0TGcKyXBQ6U
 ******************************************************************************/

/*******************************************************************************
 * IAPERF_ProbeDomainVF
 ******************************************************************************/

EFI_STATUS EFIAPI IAPERF_ProbeDomainVF(IN const UINT8 domIdx, OUT DOMAIN* dom)
{
  CpuMailbox box;
  MailboxBody* b = &box.b;
  EFI_STATUS status = EFI_SUCCESS;
  UINT32 cmd = 0;

  //
  // Alderlake: if we are running on E-Core, skip other domains
  
//  CPUCORE *core = (CPUCORE *)GetCpuDataBlock();

// 
//   if (core->IsECore) {
//     if (domIdx != ECORE) {
//       return EFI_SUCCESS;
//     }
//   }

  OcMailbox_InitializeAsMSR(&box);

  /////////////////////////////////
  // Read IccMax for this domain //
  /////////////////////////////////

  MiniTraceEx("Dom: 0x%x, Reading IccMax", domIdx);

  const UINT16 iccMaxMask = (1 << gActiveCpuData->IccMaxBits) - 1;

  cmd = OcMailbox_BuildInterface(0x16, dom->VRaddr, 0);

  if (!EFI_ERROR(OcMailbox_ReadWrite(cmd, 0, &box))) {
    dom->IccMax = b->box.data & iccMaxMask;
  }

  ///////////////////
  // Read V/F Info //
  ///////////////////
  
  //
  // cmd: Read, Domain#, 0

  cmd = OcMailbox_BuildInterface(0x10, domIdx, 0x0);

  if (EFI_ERROR(OcMailbox_ReadWrite(cmd, 0, &box))) {
    return EFI_ABORTED;
  }

  //
  // Convert output to a format readable by a non-engineer

  dom->MaxRatio = (UINT8)(b->box.data & 0xff);
  dom->VoltMode = (UINT8)((b->box.data >> 20) & 0x1);

  INT16 OffsetVoltsFx = (INT16)((b->box.data >> 21) & 0x7ff);
  UINT16 TargetVoltsFx = (UINT16)((b->box.data >> 8 ) & 0xfff);

  dom->OffsetVolts = cvrt_offsetvolts_fxto_i16(OffsetVoltsFx);
  dom->TargetVolts = cvrt_ovrdvolts_fxto_i16(TargetVoltsFx);
  
  MiniTraceEx("Dom: 0x%x, legacy: maxRatio: %u, vmode: %u, voffset: %d, vtarget: %u", 
    domIdx,
    dom->MaxRatio,
    dom->VoltMode,
    dom->OffsetVolts,
    dom->TargetVolts);

  //
  // Discover V/F Points 
  // (CML and above only!!!)

  dom->nVfPoints = 0;

  if (  (gActiveCpuData->VfPointsExposed) && 
        ((domIdx==IACORE)||(domIdx==RING)||(domIdx==ECORE))) {

    UINT8 pidx = 0;

    MiniTraceEx("Discovering VF Pts. for domain: 0x%x", domIdx);

    do {

      //
      // cmd2: Read, Domain#, VfPt#
      // NOTE: OC Mailbox Indices are OUR_IDX+1, 
      // as OC uBOx VFPT[0] is unused!

      const UINT32 cmd2 = OcMailbox_BuildInterface(0x10, domIdx, pidx+1);

      if (EFI_ERROR(OcMailbox_ReadWrite(cmd2, 0, &box))) {
        return EFI_ABORTED;
      }

      //
      // Check if the readout was with no error - if not, stop probing 
      // Either V/F points are not supported or we reached the top V/F point

      if (box.status == 0) {

        VF_POINT* vp = &dom->vfPoint[dom->nVfPoints];

        INT16 voltOffsetFx = (INT16)((b->box.data >> 21) & 0x7ff);

        vp->FusedRatio = (UINT8)(b->box.data & 0xff);;
        vp->VOffset = cvrt_offsetvolts_fxto_i16(voltOffsetFx);
        vp->IsValid = 1;

        MiniTraceEx("VF Pt. found, #%u, mult: %ux, voffset: %d mV, dom: 0x%x",
          dom->nVfPoints,
          vp->FusedRatio,
          vp->VOffset,
          domIdx);

        dom->nVfPoints++;
      }

      pidx++;

    } while ((box.status == 0) && (pidx < MAX_VF_POINTS));
  }

  MiniTraceEx("Dom: 0x%x, V/F discovery done", domIdx);

  return status;
}

/*******************************************************************************
* IAPERF_ProgramDomainVF
******************************************************************************/

EFI_STATUS EFIAPI IAPERF_ProgramDomainVF( IN const UINT8 domIdx, 
  IN OUT DOMAIN *dom, IN const UINT8 programVfPoints, 
  IN const UINT8 programIccMax)
{
  CpuMailbox box;  
  OcMailbox_InitializeAsMSR(&box);

  //
  // Alderlake: if we are running on E-Core, skip other domains

//  CPUCORE* core = (CPUCORE*)GetCpuDataBlock();
//   if (core->CpuInfo.HybridArch && core->IsECore) {
//     if (domIdx != ECORE) {
//       return EFI_SUCCESS;
//     }
//   }

  //
  // Use OC Mailbox MSR
  // to program V/F overrides
  
  UINT32 data = 0;
  UINT32 cmd = 0;

  ////////////
  // IccMax //
  ////////////
  
  if (programIccMax) {
    
    //
    // Sanity

    const UINT16 iccMaxMask = (1 << gActiveCpuData->IccMaxBits) - 1;
        
    dom->IccMax = (dom->IccMax > iccMaxMask) ? iccMaxMask : dom->IccMax;
    dom->IccMax = (dom->IccMax < 0x4) ? 0x4 : dom->IccMax;
    
    data = dom->IccMax;

    MiniTraceEx("Dom: 0x%x, programming IccMax of %u A: %u",
      domIdx,
      data>>2);

    cmd = OcMailbox_BuildInterface(0x17, dom->VRaddr, 0);
    OcMailbox_ReadWrite(cmd, data, &box);

    //
    // RKL/ICL/TGL/ADL require extra step for truly unlocked IccMax
    // NOTE: do not use on CML or older CPUs!

    if (gActiveCpuData->hasUnlimitedIccMaxFlag) {
      if (dom->IccMax == iccMaxMask) {

        data = dom->IccMax;
        data |= bit31u32;

        cmd = OcMailbox_BuildInterface(0x17, dom->VRaddr, 0);
        OcMailbox_ReadWrite(cmd, data, &box);
      }
    }

    MiniTraceEx("Dom: 0x%x, programming IccMax done", domIdx);
  }

  //////////////////
  // V/F (Legacy) //
  //////////////////

  //
  // Do not perform legacy programming
  // if user has chosen to program individual VF points

  //if ((programVfPoints == 0) || (!gActiveCpuData->VfPointsExposed)) 
  {

    //
    // Convert the desired voltages in OC Mailbox format

    UINT32 targetVoltsFx =
      (UINT32)cvrt_ovrdvolts_i16_tofix(dom->TargetVolts) & 0xfff;

    UINT32 offsetVoltsFx =
      (UINT32)cvrt_offsetvolts_i16_tofix(dom->OffsetVolts) & 0x7ff;

    //
    // Compose the command for the OC mailbox

    data = dom->MaxRatio;
    data |= (offsetVoltsFx) << 21;
    data |= ((UINT32)(dom->VoltMode & bit1u8)) << 20;
    data |= ((UINT32)(targetVoltsFx)) << 8;
    
    cmd = OcMailbox_BuildInterface(0x11, domIdx, 0x0);

    MiniTraceEx("Dom: 0x%x programming: max %ux, vmode: %u, voff: %d, vtgt: %u",
      domIdx,
      dom->MaxRatio,
      dom->VoltMode,
      dom->OffsetVolts,
      dom->TargetVolts);

    if (EFI_ERROR(OcMailbox_ReadWrite(cmd, data, &box))) {

      //
      // If we failed here, it is beyond hope
      // (retries already done, etc.) so fail hard

      return EFI_ABORTED;
    }
  }    

  ///////////////
  // VF Points //
  ///////////////

  if ((programVfPoints == 1) && (gActiveCpuData->VfPointsExposed == 1)) {
    for (UINT8 vidx = 0; vidx < dom->nVfPoints; vidx++) {
      VF_POINT *vp = dom->vfPoint + vidx;

      if (vp->IsValid) {
        
        //
        // Convert voltage offset to mbox format:

        UINT32 offsetVoltsFx = (UINT32)cvrt_offsetvolts_i16_tofix(
          vp->VOffset) & 0x7ff;

        //
        // Compose the command for the OC mailbox

        data = (offsetVoltsFx) << 21;

        MiniTraceEx("Dom: 0x%x, VF Pt. #%u programming: voffset: %d mV",
          domIdx,
          vidx+1,
          vp->VOffset );

        cmd = OcMailbox_BuildInterface(0x11, domIdx, vidx + 1);

        if (EFI_ERROR(OcMailbox_ReadWrite(cmd, data, &box))) {

          MiniTraceEx("Dom: 0x%x, aborting programming at vfp #%u, err: 0x%x",
            domIdx,
            vidx + 1,
            box.status);

          return EFI_ABORTED;
        }
      }
    }
  }

  MiniTraceEx("Dom: 0x%x, V/F programming done", domIdx);

  return EFI_SUCCESS;
}

/*******************************************************************************
* IaCore_OcLock
******************************************************************************/

VOID IaCore_OcLock(VOID)
{
  QWORD flexRatioMsr;

  flexRatioMsr.u64 = pm_rdmsr64(MSR_FLEX_RATIO);

  if (!(flexRatioMsr.u32.lo & bit20u32)) {

    MiniTraceEx("Locking OC");

    flexRatioMsr.u32.lo |= bit20u32;
    pm_wrmsr64(MSR_FLEX_RATIO, flexRatioMsr.u64);
   }
}