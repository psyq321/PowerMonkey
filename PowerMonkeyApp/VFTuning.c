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

  OcMailbox_InitializeAsMSR(&box);

  /////////////////////////////////
  // Read IccMax for this domain //
  /////////////////////////////////

  cmd = OcMailbox_BuildInterface(0x16, dom->VRaddr, 0);

  if (!EFI_ERROR(OcMailbox_ReadWrite(cmd, 0, &box))) {
    dom->IccMax = b->box.data & 0x3ff;
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
  
  //
  // Discover V/F Points 
  // (CML and above only!!!)

  dom->nVfPoints = 0;

  if ((domIdx==IACORE)||(domIdx==RING)) {

    UINT8 pidx = 0;

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

        vp->FusedRatio = (UINT8)(b->box.data & 0xff);
        vp->OffsetVolts = cvrt_offsetvolts_fxto_i16(voltOffsetFx);

        dom->nVfPoints++;
      }

      pidx++;

    } while ((box.status == 0) && (pidx < MAX_VF_POINTS));
  }

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
        
    dom->IccMax = (dom->IccMax > 0x3FF) ? 0x3FF : dom->IccMax;
    dom->IccMax = (dom->IccMax < 0x4) ? 0x4 : dom->IccMax;

    //
    // TBD: RKL/ICL/TGL - handle "Unlimited IccMax"! 
    
    data = dom->IccMax;    
    
    cmd = OcMailbox_BuildInterface(0x17, dom->VRaddr, 0);
    OcMailbox_ReadWrite(cmd, data, &box);
  }

  //////////////////
  // V/F (Legacy) //
  //////////////////
    
  //
  // Convert the desired voltages in OC Mailbox format

  UINT32 targetVoltsFx = 
    (UINT32) cvrt_ovrdvolts_i16_tofix(dom->TargetVolts) & 0xfff;  
  
  UINT32 offsetVoltsFx = 
    (UINT32) cvrt_offsetvolts_i16_tofix(dom->OffsetVolts) & 0x7ff;

  //
  // Compose the command for the OC mailbox

  data =  dom->MaxRatio;
  data |= ((UINT32)(targetVoltsFx)) << 8;
  data |= ((UINT32)(dom->VoltMode & bit1u8)) << 20;  
  data |= (offsetVoltsFx) << 21;

  cmd = OcMailbox_BuildInterface(0x11, domIdx, 0x0); 

  //
  // Send our VF override request to the OC Mailbox

  if(EFI_ERROR(OcMailbox_ReadWrite( cmd, data, &box))) {
    
    //
    // If we failed here, it is beyond hope
    // (retries already done, etc.) so fail hard
    
    return EFI_ABORTED;
  }

  ///////////////
  // VF Points //
  ///////////////

  if (programVfPoints == 1) {
    for (UINT8 vidx = 0; vidx < dom->nVfPoints; vidx++) {
      VF_POINT *vp = dom->vfPoint + vidx;

      if (vp->FusedRatio > 0) {

        data = cmd = 0;
        
        //
        // Convert voltage offset to mbox format:

        offsetVoltsFx = (UINT32)cvrt_offsetvolts_i16_tofix(
          vp->OffsetVolts) & 0x7ff;

        //
        // Compose the command for the OC mailbox

        data = vp->FusedRatio;
        data |= (offsetVoltsFx) << 21;

        cmd = OcMailbox_BuildInterface(0x11, domIdx, vidx + 1);

        if (EFI_ERROR(OcMailbox_ReadWrite(cmd, data, &box))) {
          return EFI_ABORTED;
        }
      }
    }
  }
  
  return EFI_SUCCESS;
}

/*******************************************************************************
* IaCore_OcLock
******************************************************************************/

VOID IaCore_OcLock(VOID)
{
  QWORD flexRatioMsr;
  
  flexRatioMsr.u64 = pm_rdmsr64(MSR_FLEX_RATIO);
  flexRatioMsr.u32.lo |= bit20u32;

  pm_wrmsr64 (MSR_FLEX_RATIO, flexRatioMsr.u64);
}