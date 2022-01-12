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

#if defined(_MSC_VER)
#define UNUSED
#else
#define UNUSED __attribute__((unused))
#endif

/*******************************************************************************
 * Configuration
 ******************************************************************************/

#include "CONFIGURATION.h"

/*******************************************************************************
 * MiniLogEntry - 128 bits, can be cmpxchg16b-ed
 ******************************************************************************/

typedef struct _MiniLogEntry {
  UINT8   operId;
  UINT8   pkgIdx;
  UINT8   coreIdx;
  UINT8   dangerous;
  UINT32  param1;
  UINT64  param2;
} MiniLogEntry;

/*******************************************************************************
 * Operation IDs
 ******************************************************************************/

#define MINILOG_OPID_FREE_MSG                                   0x00
#define MINILOG_OPID_RDMSR64                                    0x01
#define MINILOG_OPID_WRMSR64                                    0x02
#define MINILOG_OPID_MMIO_READ32                                0x03
#define MINILOG_OPID_MMIO_WRITE32                               0x04
#define MINILOG_OPID_MMIO_OR32                                  0x05

/*******************************************************************************
 * Log Codes
 ******************************************************************************/

#define MINILOG_LOGCODE_TRACE                                   0x00

/*******************************************************************************
 *
 ******************************************************************************/

#ifdef ENABLE_MINILOG_TRACING

void InitTrace();

void MiniTrace(
  const UINT8  operId,  
  const UINT8  dangerous,
  const UINT32 param1,
  const UINT64 param2
);

void MiniTraceEx(
  IN  CONST CHAR8* format,
  ...
);

#else

#define InitTrace()
#define MiniTrace(a, b, c, d)

static void UNUSED MiniTraceEx(
  IN  CONST CHAR8* format,
  ...
) {
  
}


#endif