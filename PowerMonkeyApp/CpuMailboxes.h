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
 *
 ******************************************************************************/

enum MailboxType
{
  MAILBOX_MSR =   0x00,             // Mailbox uses MSR for I/O
  MAILBOX_MMIO =  0x01,
};

/*******************************************************************************
 * CpuMailbox - Generic abstraction for several x64 mailboxes
 ******************************************************************************/

typedef union _MailboxBody
{
  UINT64 u64;
  UINT8  u8[8];

  struct {
    UINT32 data;
    UINT32 ifce;
  } box;

  struct {
    UINT32 lo;
    UINT32 hi;
  } u32;

  struct {
    UINT8 b0;
    UINT8 b1;
    UINT8 b2;
    UINT8 b3;
    UINT8 b4;
    UINT8 b5;
    UINT8 b6;
    UINT8 b7;
  } bytes;

} MailboxBody;

/*******************************************************************************
 * MailboxCfg
 ******************************************************************************/

typedef struct _MailboxCfg
{
  UINT32  addr;                           // MSR Index or MMIO addr
  UINT32  latency;                        // Typical write latency (in ns)
  UINT32  cmdBits;                        // Bit mask of the command
  UINT32  maxSpins;                       // Maximum probes while busy-waiting
  UINT32  busyFlag;                       // Busy Flag to test against
  UINT32  statusBits;                     // Bit mask of the return status
  UINT32  maxRetries;                     // Max. retries before aborting

  UINT8   type;                           // Mailbox type: MSR or MMIO
  UINT8   pad[3];
} MailboxCfg;

/*******************************************************************************
 * CpuMailbox - Generic abstraction for several mailboxes
 ******************************************************************************/

typedef struct _CpuMailbox
{
  MailboxBody b;
  MailboxCfg  cfg;
  UINT32 status;
  UINT8 pad[4];
} CpuMailbox;

/*******************************************************************************
 * CpuMailbox_ReadWrite
 ******************************************************************************/

EFI_STATUS EFIAPI CpuMailbox_ReadWrite(CpuMailbox* b);
