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
#include "LowLevel.h"
#include "DelayX86.h"
#include "CpuMailboxes.h"

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
 * CpuMailbox_MMIOBusyWait
 ******************************************************************************/

EFI_STATUS EFIAPI CpuMailbox_MMIOBusyWait(CpuMailbox* b)
{
  const UINT32 mmioAddr = b->cfg.addr;
  const UINT32 busyFlag = b->cfg.busyFlag;
  const UINT32 maxSpins = b->cfg.maxSpins;

  MailboxBody* mb = &b->b;

  UINT8 busy = 1;
  UINTN nspins = 0;

  do {
    mb->box.ifce = pm_mmio_read32(mmioAddr);

    busy = (mb->box.ifce & busyFlag) ? 1 : 0;

    if (busy) {
      nspins++;
      MicroStall(1);
    }
  } while ((busy) && (nspins < maxSpins));

  //
  // Check if the operation timed out

  if ((busy) && (nspins == maxSpins)) {
    return EFI_ABORTED;
  }

  return EFI_SUCCESS;
}


/*******************************************************************************
 * CpuMailbox_BusyWait_MSR
 ******************************************************************************/

EFI_STATUS EFIAPI CpuMailbox_MsrBusyWait(CpuMailbox *b)
{
  const UINT32 msrIdx =   b->cfg.addr;
  const UINT32 busyFlag = b->cfg.busyFlag;
  const UINT32 maxSpins = b->cfg.maxSpins;

  UINT8 busy = 1;
  UINTN nspins = 0;

  do {
    b->b.u64 = pm_rdmsr64(msrIdx);
    
    busy = (b->b.box.ifce & busyFlag) ? 1 : 0;

    if (busy) {
      nspins++;
      MicroStall(1);
    }
  } while ((busy) && (nspins < maxSpins));

  //
  // Check if the operation timed out

  if ((busy) && (nspins == maxSpins)) {
    return EFI_ABORTED;
  }

  return EFI_SUCCESS;
}

/*******************************************************************************
 * CpuMailbox_MsrReadWrite
 ******************************************************************************/

EFI_STATUS EFIAPI CpuMailbox_MsrReadWrite( CpuMailbox *b )
{
  EFI_STATUS state = EFI_SUCCESS;

  const UINT32 statusBits = b->cfg.statusBits;
  const UINT32 maxRetries = b->cfg.maxRetries;
  const UINT32 boxLatency = b->cfg.latency;
  const UINT32 busyFlag = b->cfg.busyFlag;
  const UINT32 msrIdx = b->cfg.addr;

  UINT32 nRetries = 0;

  do {

    MailboxBody test;
    state = EFI_SUCCESS;

    b->b.box.ifce |= busyFlag;

    pm_wrmsr64(msrIdx, b->b.u64);

    if (EFI_ERROR(CpuMailbox_MsrBusyWait(b))) {
      state = EFI_INVALID_PARAMETER;
    }

    b->b.u64 = pm_rdmsr64(msrIdx);

    MicroStall(boxLatency);

    test.u64 = pm_rdmsr64(msrIdx);

    //
    // Verify if the result is correct

    if (b->b.box.ifce != test.box.ifce) {
      if (b->b.box.data != test.box.data) {
        state = EFI_INVALID_PARAMETER;
      }
    }

    nRetries++;

  } while ((state!=EFI_SUCCESS)&&(nRetries<maxRetries));

  //
  // Status
  
  b->status = b->b.box.ifce & statusBits;

  return state;
}

/*******************************************************************************
 * CpuMailbox_ReadWrite
 ******************************************************************************/

EFI_STATUS EFIAPI CpuMailbox_ReadWrite(CpuMailbox* b)
{
  switch (b->cfg.type)
  {
    case MAILBOX_MSR:
    {
      return CpuMailbox_MsrReadWrite(b);
    }
    break;

    default:
    {
      return EFI_INVALID_PARAMETER;
    }
    break;
  }  
}
