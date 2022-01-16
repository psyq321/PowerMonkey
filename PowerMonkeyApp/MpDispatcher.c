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

//
// Protocols

#include <Protocol/MpService.h>

//
// PowerMonkey

#include "MpDispatcher.h"
#include "LowLevel.h"

//
// Initialized at startup

extern EFI_MP_SERVICES_PROTOCOL* gMpServices;
extern EFI_BOOT_SERVICES* gBS;

/*******************************************************************************
 *
 ******************************************************************************/

///
/// Internal structure for command dispatching 
///

typedef struct _PACKAGE_DISPATCH_DATA
{
  UINTN CpuNumber;
  VOID* opaqueParam;
} PACKAGE_DISPATCH_DATA;


/*******************************************************************************
 * ProcessorIgnite
 ******************************************************************************/

typedef struct _IgniteContext
{
  UINTN CpuNumber;
  VOID* userParam;  
  EFI_AP_PROCEDURE userProc;
} IgniteContext;

VOID EFIAPI ProcessorIgnite(VOID* params)
{
  ///
  /// We are now running on the designated CPU.
  /// Before executing user's function we will set GS to point to per-CPU store
  /// 
    
  IgniteContext* pic = (IgniteContext*)params;

  VOID* coreStructAddr = GetCpuDataBlock();

  //
  // Set the GS register to point to coreStructAddr

  SetCpuGSBase(coreStructAddr);

  ///
  /// Populate CPU core-specific info
  /// 
  
  CPUCORE* core = (CPUCORE*)coreStructAddr;
  
  GetCpuInfo(&core->CpuInfo);
  core->IsECore = core->CpuInfo.ECore;

  //
  // Debug
  {
    UINTN processorNumber = 0;
    gMpServices->WhoAmI(gMpServices, &processorNumber);
    core->ValidateIdx = (UINT32)processorNumber;
  }  
  
  if (pic->userProc) {

    ///
    /// Execute user's call (if supplied)
    ///

    pic->userProc(pic->userParam);
  }  
}


/*******************************************************************************
 * TBD / TODO: Needs Rewrite
 ******************************************************************************/

EFI_STATUS EFIAPI RunOnPackageOrCore(
  const IN PLATFORM* Platform,
  const IN UINTN CpuNumber,
  const IN EFI_AP_PROCEDURE proc,
  IN VOID* param OPTIONAL)
{
  EFI_STATUS status = EFI_SUCCESS;

  if (gMpServices) {
    if (CpuNumber != Platform->BootProcessor) {

      IgniteContext ctx = { 0 };
      
      ctx.userParam = param;
      ctx.userProc = proc;
      ctx.CpuNumber = CpuNumber;

      status = gMpServices->StartupThisAP(
        gMpServices,
        ProcessorIgnite,
        CpuNumber,
        NULL,
        1000000,
        &ctx,
        NULL
      );

      if (EFI_ERROR(status)) {
        Print(L"[ERROR] Unable to execute on CPU %u,"
          "status code: 0x%x\n", CpuNumber, status);
      }

      return status;
    }
  }

  //
  // Platform has no MP services OR we are running on the desired package
  // ... so instead of dispatching, we will just do the work now

  {
    IgniteContext ctx = { 0 };

    ctx.userParam = param;
    ctx.userProc = proc;
    ctx.CpuNumber = CpuNumber;

    ProcessorIgnite(&ctx);
  }
  

  //
  // ... and that's that

  return status;
}

/*******************************************************************************
 *
 ******************************************************************************/

EFI_STATUS EFIAPI RunOnAllProcessors(
  const IN EFI_AP_PROCEDURE proc,
  const BOOLEAN runConcurrent,                  // false = serial execution
  IN VOID* param OPTIONAL)
{
  EFI_STATUS status = EFI_SUCCESS;
  EFI_EVENT mpEvent = NULL;
  UINTN eventIdx = 0;

  ///
  /// Start other processors with our workload 
  ///

  if (gMpServices) {

    if (runConcurrent) {
      status = gBS->CreateEvent(
        EVT_NOTIFY_SIGNAL,
        TPL_NOTIFY,
        EfiEventEmptyFunction,
        NULL,
        &mpEvent);
    }

    if (EFI_ERROR(status)) {
      Print(L"[ERROR] Unable to create EFI_EVENT, code: 0x%x\n", status);
      mpEvent = NULL;
    }
    else {

      IgniteContext ctx = { 0 };

      ctx.CpuNumber = 0xFFFFFFFF;
      ctx.userParam = param;
      ctx.userProc = proc;

      status = gMpServices->StartupAllAPs(
        gMpServices,
        ProcessorIgnite,
        FALSE,
        (runConcurrent) ? &mpEvent : NULL,
        0,
        &ctx,
        NULL
      );

      if (EFI_ERROR(status)) {
        Print(L"[ERROR] Unable to execute on AP CPUs, code: 0x%x\n", status);
        gBS->CloseEvent(mpEvent);
        mpEvent = NULL;
      }
    }
  }

  ///
  /// Execute workload on this CPU (BSP)
  ///
  
  proc(param);
  
  ///
  /// Wait until work is done
  ///

  if ((gMpServices) && (mpEvent) && (!EFI_ERROR(status))) {

    //
    // Wait for APs to finish

    if (runConcurrent) {
      gBS->WaitForEvent(1, &mpEvent, &eventIdx);
      gBS->CloseEvent(mpEvent);
    }    
  }

  return status;
}