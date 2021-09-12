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

#include "CpuMailboxes.h"

/*******************************************************************************
 * Constants
 ******************************************************************************/

#define MAX_PACKAGES    8                        // Maximum Packages
#define MAX_CORES       128                      // Max cores per package
#define MAX_DOMAINS     5
#define MAX_VF_POINTS   15

#define MAX_POWAH       0xFFFFFFFF

/*******************************************************************************
 * OC Mailbox - Voltage Domains (NOTE: some might be linked!)
 ******************************************************************************/

enum VoltDomains
{
  IACORE =    0x00,           // IA Core
  GTSLICE =   0x01,           // GT Slice
  RING =      0x02,           // Ring / Cache
  GTUNSLICE = 0x03,           // GT Unslice
  UNCORE =    0x04            // Uncore (SA)
};


/*******************************************************************************
 * OC Mailbox - Voltage Adjustment Modes
 ******************************************************************************/

enum VoltMode
{
  V_IPOLATIVE = 0x00,         // Interpolative
  V_OVERRIDE  = 0x01          // Override
};

/*******************************************************************************
 * "QWORD" struct - useful when dealing with MSRs
 ******************************************************************************/

typedef union _QWORD
{
  UINT64 u64;

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
  } u8;
} QWORD;

/*******************************************************************************
 * VF_POINT - Data describing single Voltage/Frequency (VF) point
 ******************************************************************************/

typedef struct _VF_POINT
{
  UINT8   FusedRatio;               // Read Only
  INT16   OffsetVolts;              // R/W if supported and if V/F point valid  
} VF_POINT;

/*******************************************************************************
 * DOMAIN - Holds data specific to individual unit within package
 * (core, uncore, cache/ring, gt slice, gt unslice, etc.)
 ******************************************************************************/

typedef struct _DOMAIN
{  
  UINT8   MaxRatio;
  UINT8   VoltMode;
  
  UINT16  TargetVolts;
  INT16   OffsetVolts;

  UINTN    nVfPoints;                     // Number of V/F points supported by the package [READ ONLY]
  VF_POINT vfPoint[MAX_VF_POINTS+1];      // V/F Point Data

  void* parent;

} DOMAIN;


/*******************************************************************************
 * CPUCORE - Holds data specific to a single CPU core (logical or physical)
 ******************************************************************************/

typedef struct _CPUCORE
{
  UINTN     ApicID;
  UINT32    CpuID;
  UINT8     CpuidString[128];

  BOOLEAN   IsPhysicalCore;                           // 

  void *parentPackage;

} CPUCORE;


/*******************************************************************************
 * PACKAGE - Holds data specific to a single CPU package (socket, ...)
 ******************************************************************************/

typedef struct _PACKAGE
{
  ///////////////////////////
  //    V/F Overrides      //
  // (per voltage domain)  //
  /////////////////////////// 

  UINT8 Program_VF_Overrides[MAX_DOMAINS];     // Domains to program

  DOMAIN Domain[MAX_DOMAINS];                  // IA Cores, Ring, SA, ...

  UINT8 Program_VF_Points;                     // Program individual VF Points
                                               // (VF Curve Points)

  //
  // Turbo Ratio Limits
  
  UINT64  TurboRatioLimits;                    // [READ] MSR_TURBO_RATIO_LIMIT
    
  UINT8   ForcedRatioForAllCoreCounts;         // [WRITE] If != 0, we will 
                                               // program this ratio for all 
                                               // core-counts
  //////////////////
  // Power limits //
  //////////////////

  UINT64  MsrPkgPowerLimits;                   // [READ] MSR_PACKAGE_POWER_LIMIT
  UINT32  PkgPowerUnits;                       // [READ] Power Units
  UINT32  PkgTimeUnits;
  UINT32  PkgEnergyUnits;


  UINT32  MsrPkgMaxTau;                        // [READ] Maximum Tau
  UINT32  MsrPkgMinPL1;                        // [READ] Minimum PL1 accepted
  UINT32  MsrPkgMaxPL1;                        // [READ] Maximum PL1 accepted

  UINT8   ProgramPL12_MSR;                     // [WRITE] Program MSR PL1/2
  UINT8   ProgramPL12_MMIO;                    // [WRITE] Program MMIO PL1/2
  UINT8   ProgramPL12_PSys;                    // [WRITE] Program Platform PLs
  UINT8   ProgramPL3;                          // [WRITE] Program PL3
  UINT8   ProgramPL4;                          // [WRITE] Program PL4
  UINT8   ProgramPP0;                          // [WRITE] Program PP0

  //
  // Power Control

  UINT8 ProgramPowerControl;                   // Enable Programming of PowCtl
  UINT8 EnableEETurbo;                         // Enable Energy Efficient Turbo
  UINT8 EnableRaceToHalt;                      // Enable Race To Halt

  //
  // MSR PL1/PL2 

  UINT8   EnableMsrPkgPL1;          // [WRITE] Enable MSR Pkg Power Limit 1
  UINT8   EnableMsrPkgPL2;          // [WRITE] Enable MSR Pkg Power Limit 2
  UINT8   ClampMsrPkgPL;            // [WRITE] MSR Pkg PL Clamp
  UINT8   LockMsrPkgPL12;           // [WRITE] Lock PL1/PL2 MSR
  UINT32  MsrPkgPL1_Power;          // [WRITE] if(!0) PL1 in mW
  UINT32  MsrPkgPL2_Power;          // [WRITE] if(!0) PL2 in mW
  UINT32  MsrPkgPL_Time;            // [WRITE] if(!0) PL time

  //
  // MSR PL3

  UINT8   EnableMsrPkgPL3;          // [WRITE] Enable Power Limit
  UINT8   LockMsrPkgPL3;            // [WRITE] Lock
  UINT32  MsrPkgPL3_Power;          // [WRITE] if(!0) PL3 in mW
  UINT32  MsrPkgPL3_Time;           // [WRITE] if(!0) PL 

  //
  // MSR PL4

  UINT8   EnableMsrPkgPL4;          // [WRITE] Enable Power Limit
  UINT8   LockMsrPkgPL4;            // [WRITE] Lock PL4 
  UINT32  MsrPkgPL4_Current;        // [WRITE] Maximum PL4 Current in A 
                                    // (or MAX_POWAH)

  //
  // MSR PP0 (not commonly used in notebooks)

  UINT8   EnableMsrPkgPP0;          // [WRITE] Enable PP0
  UINT8   LockMsrPP0;               // [WRITE] Lock
  UINT8   ClampMsrPP0;              // [WRITE] Clamp
  UINT32  MsrPkgPP0_Power;          // [WRITE] Power in mW or MAX_POWAH
  UINT32  MsrPkgPP0_Time;           // [WRITE] Time in ms or MAX_POWAH


  //
  // MMIO PL1, PL2

  UINT8   EnableMmioPkgPL1;         // [WRITE] Enable MMIO Pkg Power Limit 1
  UINT8   EnableMmioPkgPL2;         // [WRITE] Enable MMIO Pkg Power Limit 2
  UINT8   ClampMmioPkgPL;           // [WRITE] MMIO Pkg PL Clamp
  UINT8   LockMmioPkgPL12;          // [WRITE] Lock MMIO PL1/PL2
  UINT32  MmioPkgPL1_Power;         // [WRITE] if(!0) PL1 in mW
  UINT32  MmioPkgPL2_Power;         // [WRITE] if(!0) PL2 in mW
  UINT32  MmioPkgPL_Time;           // [WRITE] if(!0) PL time

  //
  // Platform PL1, PL2

  UINT8   EnablePlatformPL1;        // [WRITE] Enable MSR Pkg Power Limit 1
  UINT8   EnablePlatformPL2;        // [WRITE] Enable MSR Pkg Power Limit 2
  UINT8   ClampPlatformPL;          // [WRITE] Platform PL Clamp
  UINT8   LockPlatformPL;           // [WRITE] Lock Platform PL
  UINT32  PlatformPL1_Power;        // [WRITE] if(!0) PL1 in mW
  UINT32  PlatformPL2_Power;        // [WRITE] if(!0) PL2 in mW
  UINT32  PlatformPL_Time;          // [WRITE] if(!0) PL time

  //
  // cTDP Levels

  UINT64  ConfigTdpControl;                    // [READ] MSR_CONFIG_TDP_CONTROL

  UINT8   MaxCTDPLevel;                        // [READ/WRITE] Set max TDP level 
  UINT8   TdpControLock;                       // [READ/WRITE] Set TDP Control Lock 

  ///////////////////////////
  // General Package Stuff //
  ///////////////////////////    

  CPUCORE Core[MAX_CORES];

  UINTN   PackageID;
  UINTN   FirstCoreApicID;

  UINTN   PhysicalCores;
  UINTN   LogicalCores;

  UINT32  CpuID;
  UINT8   CpuidString[128];

  //////////////////
  // Private Data //
  //////////////////
 
  VOID* parent;                             // Parent platform object

} PACKAGE;


/*******************************************************************************
 * PLATFORM - Holds data specific to complete system
 ******************************************************************************/

typedef struct _PLATFORM
{
  UINTN PkgCnt;

  UINTN BootProcessor;
  UINTN LogicalProcessors;
  UINTN PhysicalProcessors;
  UINTN EnabledLogicalProcessors;

  PACKAGE packages[MAX_PACKAGES];
} PLATFORM;


/*******************************************************************************
 * DiscoverPlatform
 ******************************************************************************/

EFI_STATUS EFIAPI DiscoverPlatform(
  IN OUT PLATFORM** Platform
);

/*******************************************************************************
 * StartupPlatformInit
 ******************************************************************************/

EFI_STATUS EFIAPI StartupPlatformInit(
  IN EFI_SYSTEM_TABLE* SystemTable,
  IN OUT PLATFORM** Platform
);

/*******************************************************************************
 * StartupPlatformInit
 ******************************************************************************/

VOID ApplyComputerOwnersPolicy(IN PLATFORM* Platform);

/*******************************************************************************
 * ApplyPolicy
 ******************************************************************************/

EFI_STATUS EFIAPI ApplyPolicy(IN EFI_SYSTEM_TABLE* SystemTable,
  IN OUT PLATFORM* sys);