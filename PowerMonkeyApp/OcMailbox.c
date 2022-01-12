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

#include "CpuMailboxes.h"
#include "VFTuning.h"

/*******************************************************************************
 * Layout of the CPU overclocking mailbox can be found in academic papers:
 * 
 * DOI:10.1109/SP40000.2020.00057
 * "Figure 1: Layout of the undocumented undervolting MSR with address 0x150"
 *         
 * Command IDs are also often mentioned in BIOS or in officially released Intel
 * Firmware Support Packages, e.g.:
 * 
 * https://github.com/intel/FSP/blob/master/CometLakeFspBinPkg/ \
 * CometLake1/Include/FspmUpd.h
 * 
 * V/F Curve theory-of-operation (for wide audience) can be seen explained
 * in this ScatterBencher video: https://www.youtube.com/watch?v=0TGcKyXBQ6U
 * 
 ******************************************************************************/


/*******************************************************************************
 * OC Mailbox Constants
 ******************************************************************************/

#define OC_MAILBOX_MAX_SPINS                                             999
#define OC_MAILBOX_LATENCY_US                                              9
#define OC_MAILBOX_MAX_RETRIES                                             9

#define OC_MAILBOX_CMD_MASK                                       0x000000ff
#define OC_MAILBOX_BUSY_FLAG_BIT                                  0x80000000
#define OC_MAILBOX_COMPLETION_MASK                                0x000000ff


/*******************************************************************************
 * InitiazeAsMsrOCMailbox
 ******************************************************************************/

EFI_STATUS EFIAPI OcMailbox_InitializeAsMSR(CpuMailbox* b)
{
  EFI_STATUS state = EFI_SUCCESS;

  b->status = 0;
  b->b.u64 =  0;
  
  b->cfg.type =       MAILBOX_MSR;  
  b->cfg.addr =       MSR_OC_MAILBOX;
  b->cfg.busyFlag =   OC_MAILBOX_BUSY_FLAG_BIT;
  b->cfg.cmdBits =    OC_MAILBOX_COMPLETION_MASK;
  b->cfg.statusBits = OC_MAILBOX_COMPLETION_MASK;
  b->cfg.maxRetries = OC_MAILBOX_MAX_RETRIES;
  b->cfg.maxSpins =   OC_MAILBOX_MAX_SPINS;
  b->cfg.latency =    OC_MAILBOX_LATENCY_US;

  return state;
}


/*******************************************************************************
 * OcMailbox_Write
 ******************************************************************************/

EFI_STATUS EFIAPI OcMailbox_ReadWrite( IN CONST UINT32 cmd, 
                                       IN CONST UINT32 data, 
                                       IN OUT CpuMailbox *b )
{
  b->b.box.ifce = cmd;
  b->b.box.data = data;

  return CpuMailbox_ReadWrite(b);
}

/*******************************************************************************
 * OcMailbox_BuildInterface
 ******************************************************************************/

UINT32 OcMailbox_BuildInterface( IN CONST UINT8 cmd,
                                 IN CONST UINT8 param1,
                                 IN CONST UINT8 param2
)
{
  UINT32 iface = cmd;

  iface |= ((UINT32)(param1) << 8);
  iface |= ((UINT32)(param2) << 16);

  return iface;
}
