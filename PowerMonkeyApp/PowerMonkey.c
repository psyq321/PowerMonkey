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
* Full text of license (LICENSE-2.0.txt) is available in project directory
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

/*******************************************************************************
 * Globals
 ******************************************************************************/

extern UINT8 gEnableSaferAsm;
extern UINT8 gDisableFirwmareWDT;

//
// Initialized at startup

EFI_MP_SERVICES_PROTOCOL* gMpServices = NULL;
PLATFORM* gPlatform = NULL;

/*******************************************************************************
 * UefiInit
 ******************************************************************************/

EFI_STATUS UefiInit(IN EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS status = EFI_SUCCESS;

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
  // Set-up TSC timing
  // NOTE: not MP-proofed - multiple packages will use the same calibration
  
  if (EFI_ERROR(InitializeTscVars())) {
    Print(L"[ERROR] Unable to initialize timing using"
      "CPUID leaf 0x15, error code: 0x%x\n", status);
  }

  //
  // Get handle to MP Services Protocol

	status = SystemTable->BootServices->LocateProtocol(
    	&gEfiMpServiceProtocolGuid,	NULL,	(VOID**)&gMpServices);

	if (EFI_ERROR(status)) {
		Print(L"[ERROR] Unable to locate firmware MP services"
      "protocol, error code: 0x%x\n",	status );
	}

  //
  // Disable UEFI watchdog timer (if requested)

  if (gDisableFirwmareWDT)
  {
    SystemTable->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
  }

	return status;
}

/*******************************************************************************
 * PrintBanner
 ******************************************************************************/

VOID PrintBanner()
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
    "                                                                       (____/\n"
  );

  AsciiPrint(
    " Copyright (C) 2021 Ivan Dimkovic. All rights reserved.\n"
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

EFI_STATUS EFIAPI UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  ///
  /// Init
  ///

  UefiInit(SystemTable);

  ///
  /// Print Banner
  ///
  
  PrintBanner();

  ///
  /// Discover platform
  ///

  StartupPlatformInit(SystemTable, &gPlatform);
 
  ///
  /// Program
  ///
  
  ApplyPolicy(SystemTable, gPlatform);

  ///
  /// Teardown
  /// 

  if (gEnableSaferAsm) {
    RemoveAllInterruptOverrides();
  }

  AsciiPrint("Finished. Returning to Boot Selector.\n");

  return EFI_SUCCESS;
}
