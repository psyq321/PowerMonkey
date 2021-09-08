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

#pragma once

/*******************************************************************************
 * Configuration
 ******************************************************************************/

//#define ENABLE_MINILOG_TRACING
#define MINILOG_TRACE_TARGET_FILE

/*******************************************************************************
 * MiniLogEntry - 128 bits, can be cmpxchg16b-ed
 ******************************************************************************/

typedef struct _MiniLogEntry {
  UINT16  cpuIdx;                           // Index of the CPU (core, unique)
  UINT16  operId;                           // Operation ID
  UINT16  logCode;                          // Log Code
  UINT16  data16;                           // Data1 - 16-bit
  UINT64  data64;                           // Data2 - 64-bit
} MiniLogEntry;

/*******************************************************************************
 * Operation IDs
 ******************************************************************************/

#define MINILOG_OPID_FREE_MSG                                   0x0000
#define MINILOG_OPID_RDMSR64                                    0x0001
#define MINILOG_OPID_WRMSR64                                    0x0002
#define MINILOG_OPID_MMIO_READ32                                0x0003
#define MINILOG_OPID_MMIO_WRITE32                               0x0004
#define MINILOG_OPID_MMIO_OR32                                  0x0005

 /*******************************************************************************
  * Log Codes
  ******************************************************************************/

#define MINILOG_LOGCODE_TRACE                                   0x0000

/*******************************************************************************
 *
 ******************************************************************************/

#ifdef ENABLE_MINILOG_TRACING

void InitTrace();

void MiniTrace(const UINT16 operId,
  const UINT16 data16,
  const UINT64 data64,
  const UINT8 dangerous );

#else

#define InitTrace(a)
#define MiniTrace(a, b, c, d)

#endif