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
#include <Library/UefiApplicationEntryPoint.h>
#include <Protocol/MpService.h>

//
// PowerMonkey

#include "Platform.h"
#include "LowLevel.h"
#include "DelayX86.h"
#include "InterruptHook.h"
#include "SelfTest.h"
#include "MiniLog.h"
#include "CpuInfo.h"
#include "CpuData.h"

/*******************************************************************************
 * Globals
 ******************************************************************************/

extern UINT8 gEnableSaferAsm;
extern UINT8 gDisableFirwmareWDT;
extern UINT8 gEmergencyExit;

extern EFI_BOOT_SERVICES* gBS;
extern EFI_SYSTEM_TABLE*  gST;

//
// Initialized at startup

EFI_MP_SERVICES_PROTOCOL* gMpServices = NULL;
BOOLEAN gCpuDetected = 0;
PLATFORM* gPlatform = NULL;
UINTN gBootCpu = 0;

/*******************************************************************************
 * UefiInit
 ******************************************************************************/

EFI_STATUS UefiInit(IN EFI_SYSTEM_TABLE* SystemTable)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // Get handle to MP Services Protocol

  status = SystemTable->BootServices->LocateProtocol(
    &gEfiMpServiceProtocolGuid, NULL, (VOID**)&gMpServices);

  if (EFI_ERROR(status)) {
    Print(L"[ERROR] Unable to locate firmware MP services"
      "protocol, error code: 0x%x\n", status);
  }

  if (gMpServices) {
    gMpServices->WhoAmI(gMpServices, &gBootCpu);
  }

  InitTrace();

  //
  // Hook the BSP with our "SafeAsm" interrupt handler

  if (gEnableSaferAsm) {
    InstallSafeAsmExceptionHandler();
  }

  //
  // Collect the addresses of the buses/devices/etc...
  // so that we do not need to do it every time we need them

  InitializeMMIO();

  //
  // Disable UEFI watchdog timer (if requested)

  if (gDisableFirwmareWDT) {
    SystemTable->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
  }

  return status;
}

/*******************************************************************************
 * EmergencyExit
 ******************************************************************************/

BOOLEAN EmergencyExit(VOID)
{
  if (gEmergencyExit) {

    EFI_STATUS         Status;
    EFI_EVENT          TimerEvent;
    EFI_EVENT          WaitList[2];
    EFI_INPUT_KEY      Key;
    UINTN              Index;

    gST->ConOut->SetAttribute(gST->ConOut, EFI_YELLOW);

    Print(
      L" IMPORTANT: PowerMonkey is about to program the custom CPU voltage/frequency\n"
      " settings. Programming will start in 3 seconds. If you wish to skip programming\n"
      " press ESC key on your keyboard within the next 3 seconds.\n\n");

    gST->ConOut->SetAttribute(gST->ConOut, EFI_LIGHTGRAY);

    Status = gBS->CreateEvent(
      EVT_TIMER, TPL_NOTIFY, NULL, NULL, &TimerEvent);

    //
    // 3 second timeout

    Status = gBS->SetTimer(
      TimerEvent, TimerRelative, 30000000);

    //
    // Wait for a keystroke OR timeout

    WaitList[0] = gST->ConIn->WaitForKey;
    WaitList[1] = TimerEvent;

    Status = gBS->WaitForEvent(2, WaitList, &Index);

    if (!EFI_ERROR(Status) && Index == 1) {
      Status = EFI_TIMEOUT;
    }

    gBS->CloseEvent(TimerEvent);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (Key.ScanCode == SCAN_ESC) {      
      AsciiPrint(
        " Aborting.\n");

      return TRUE;
    }
  }

  return FALSE;
}

/*******************************************************************************
 * UnknownCpuWarning
 ******************************************************************************/

BOOLEAN UnknownCpuWarning(VOID)
{
  EFI_STATUS         Status;
  EFI_EVENT          TimerEvent;
  EFI_EVENT          WaitList[2];
  EFI_INPUT_KEY      Key;
  UINTN              Index;

  gST->ConOut->SetAttribute(gST->ConOut, EFI_RED);

  Print(
    L"\n WARNING: Detected CPU (model: %u, family: %u, stepping: %u) is not known!\n"
    L" It is likely that proceeding further with hardware programming will result\n"
    L" result in unpredictable behavior or with the system hang/reboot.\n\n",
    gCpuInfo.model,
    gCpuInfo.family,
    gCpuInfo.stepping );

  Print(
    L" If you are a BIOS engineer or otherwise familiar with the detected CPU params\n"
    L" please edit CpuData.c and extend it with the detected CPU model/family/stepping\n"
  );

  Print(
    L" and its capabilities. Further changes to the PowerMonkey code might be necessary.\n\n"
    L" Press F10 key within the next 30 seconds to IGNORE this warning.\n"
    L" Otherwise, PowerMonkey will exit with no changes to the system.\n\n"
  );

  gST->ConOut->SetAttribute(gST->ConOut, EFI_LIGHTGRAY);

  Status = gBS->CreateEvent(
    EVT_TIMER, TPL_NOTIFY, NULL, NULL, &TimerEvent);

  //
  // 3 second timeout

  Status = gBS->SetTimer(
    TimerEvent, TimerRelative, 300000000);

  //
  // Wait for a keystroke OR timeout

  WaitList[0] = gST->ConIn->WaitForKey;
  WaitList[1] = TimerEvent;

  Status = gBS->WaitForEvent(2, WaitList, &Index);

  if (!EFI_ERROR(Status) && Index == 1) {
    Status = EFI_TIMEOUT;
  }

  gBS->CloseEvent(TimerEvent);
  gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

  if (Key.ScanCode == SCAN_F10) {
    AsciiPrint(
      " Overriding unknown CPU detection...\n");

    return TRUE;
  }
  else {
    AsciiPrint(
      " Programming aborted.\n");
  }

  return FALSE;
}


/*******************************************************************************
 * PrintBanner
 ******************************************************************************/

VOID PrintBanner(VOID)
{

  //
  // YES! THIS was the reason why I wrote PowerMonkey... to be able to
  // print the ASCII art again... just need to find some suitable 8-bit sound

  AsciiPrint(

    "  ______                            ______                 _\n"
    " (_____ \\                          |  ___ \\               | |\n"
    "  _____) )___   _ _ _   ____   ___ | | _ | |  ___   ____  | |  _  ____  _   _\n");

  AsciiPrint(
    " |  ____// _ \\ | | | | / _  ) / __)| || || | / _ \\ |  _ \\ | | / )/ _  )| | | |\n"
    " | |    | |_| || | | |( (/ / | |   | || || || |_| || | | || |< (( (/ / | |_| |\n"
    " |_|     \\___/  \\____| \\____)|_|   |_||_||_| \\___/ |_| |_||_| \\_)\\____) \\__  |\n"
    "                                                         Version 0.2.1 (____/\n"
  );

  AsciiPrint(
    " Copyright (C) 2021-2022 Ivan Dimkovic. All rights reserved.\n"
    "\n"
    " SPDX-License-Identifier: Apache-2.0\n"
    "\n"
  );

  AsciiPrint(
    " WARNING: This code is a proof of concept for educative purposes. It can\n"
    " modify internal computer configuration parameters and cause malfunctions or\n"
    " even permanent damage. It has been tested on a limited range of target CPUs\n");

  AsciiPrint(
    " and has minimal built-in failsafe mechanisms, thus making it unsuitable for\n"
    " recommended use by users not skilled in the art. Use it at your own risk.\n"
    "\n\n"
  );
}

/*******************************************************************************
 * The user Entry Point for Application.
 *
 * @param[in] ImageHandle    The firmware allocated handle for the EFI image.
 * @param[in] SystemTable    A pointer to the EFI System Table.
 *
 * @retval EFI_SUCCESS       The entry point is executed successfully.
 * @retval other             Some error occurs when executing this entry point.
 ******************************************************************************/

EFI_STATUS EFIAPI UefiMain(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE* SystemTable
)
{
  //
  // Gather basic CPU info

  gCpuDetected = DetectCpu();

  if (!gCpuDetected) {

    //
    // Throw warning for UNKNOWN CPUs

    BOOLEAN ovrd = UnknownCpuWarning();

    if (ovrd == FALSE) {
      return EFI_ABORTED;
    }
  }

  ///
  /// Set-up TSC timing
  /// NOTE: not MP-proofed - multiple packages will use the same calibration

  if (EFI_ERROR(InitializeTscVars())) {
    Print(L"[ERROR] Unable to initialize timing using"
      "CPUID leaf 0x15\n");
  }

  ///
  /// Print Banner
  ///

  PrintBanner();

  ///
  /// Emergency Exit
  /// 
  
  if (EmergencyExit()) {
    return EFI_SUCCESS;
  }

  ///
  /// Init
  ///

  UefiInit(SystemTable);

  ///
  /// Discover platform
  ///

  StartupPlatformInit(SystemTable, &gPlatform);

  ///
  /// Program
  ///

  ApplyPolicy(SystemTable, gPlatform);

  ///
  /// Self test
  ///

  if (gSelfTestMaxRuns) {
    PM_SelfTest();
  }

  ///
  /// Teardown
  /// 

  if (gEnableSaferAsm) {
    RemoveAllInterruptOverrides();
  }

  AsciiPrint("Finished.\n");

 return EFI_SUCCESS;
}
