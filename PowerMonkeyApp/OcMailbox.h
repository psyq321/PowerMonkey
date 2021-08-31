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
* SPDX-License-Identifier: Apache-2.0
*
* WARNING: This code is a proof of concept for educative purposes. It can
* modify internal computer configuration parameters and cause malfunctions or
* even permanent damage. It has been tested on a limited range of target CPUs
* and has minimal built-in failsafe mechanisms, thus making it unsuitable for
* recommended use by users not skilled in the art. Use it at your own risk.
*
*******************************************************************************/

#pragma once

#include "CpuMailboxes.h"

/*******************************************************************************
 * InitiazeAsMsrOCMailbox
 ******************************************************************************/

EFI_STATUS EFIAPI OcMailbox_InitializeAsMSR(CpuMailbox* b);

/*******************************************************************************
 * OcMailbox_Write
 ******************************************************************************/

EFI_STATUS EFIAPI OcMailbox_ReadWrite(
  IN CONST UINT32 cmd,
  IN CONST UINT32 data,
  IN OUT CpuMailbox* b);

/*******************************************************************************
 * OcMailbox_BuildInterface
 ******************************************************************************/

UINT32 OcMailbox_BuildInterface(IN CONST UINT8 cmd,
  IN CONST UINT8 param1,
  IN CONST UINT8 param2
);

