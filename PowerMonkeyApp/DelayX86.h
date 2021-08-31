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

/*******************************************************************************
 * InitializeTscVars() gets and stores the characteristics of the TSC counter 
 * this is necessary for accurate timing
 ******************************************************************************/

EFI_STATUS EFIAPI InitializeTscVars(VOID);

/*******************************************************************************
 * StallCpu
 * Stalls the CPU for specific number of TICKS
 ******************************************************************************/

VOID EFIAPI StallCpu(const UINT64 ticks);

/*******************************************************************************
 * nsDelay
 * Stalls the CPU for specific number of ns (nanoseconds)
 ******************************************************************************/

VOID EFIAPI NanoStall(const UINT64 ns);

/*******************************************************************************
 * usDelay
 * Stalls the CPU for specific number of ns (microseconds)
 ******************************************************************************/

VOID EFIAPI MicroStall(const UINT64 us);