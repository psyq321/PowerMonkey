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

#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/MpService.h>
#include "Platform.h"

/*******************************************************************************
 * Globals
 ******************************************************************************/

//
// Initialized at startup

extern EFI_MP_SERVICES_PROTOCOL* gMpServices;
extern UINT8 gPrintVFPoints_PostProgram;
extern UINT8 gPrintPackageConfig;

/*******************************************************************************
 *
 ******************************************************************************/

UINT16 vrDomainColStr[8][12] = {
  {L" CORE (IA) "},
  {L"  GTSLICE  "},
  {L" RING (C$) "},
  {L" GTUNSLICE "},
  {L" UNCORE/SA "},
  {L"  E-CORE   "},
  {L"           "},
  {L"           "},
};

UINT16 vrDomainPrStr[8][12] = {
  {L"IACORE"},
  {L"GTSLICE"},
  {L"RING"},
  {L"GTUNSLICE"},
  {L"UNCORE"},
  {L"E-CORE"},
  {L"           "},
  {L"           "},
};

UINT16 svidColStr[2][7] = {
  {L" YES  "},
  {L" N/A  "},
};

UINT16 voltModeColStr[2][11] = {
  {L" I-polate "},
  {L" Override "},
};

/*******************************************************************************
 * PrintVFPoints
 ******************************************************************************/

VOID PrintVFPoints(IN PLATFORM* psys)
{

  for (UINTN pidx = 0; pidx < psys->PkgCnt; pidx++)
  {
    PACKAGE* pac = psys->packages + pidx;

    AsciiPrint("Package #%u\n", pidx);

    for (UINTN didx = 0; didx < MAX_DOMAINS; didx++) {
      if (DomainSupported((UINT8)didx)) {
        if ((didx == IACORE) || (didx == RING)|| (didx==ECORE)) {

          DOMAIN* dom = pac->planes + didx;

          if ((pac->Program_VF_Points[didx] == 2) ||
            (gPrintVFPoints_PostProgram != 0))
          {
            AsciiPrint("    Domain: %s, number of reported V/F points: %u\n",
              &vrDomainPrStr[didx][0], dom->nVfPoints);

            for (UINTN vidx = 0; vidx < dom->nVfPoints; vidx++) {
              VF_POINT* vp = dom->vfPoint + vidx;

              AsciiPrint("      [%s][VF point #%u]     fused ratio: %ux  -  voltage offset: %d mV\n",
                &vrDomainPrStr[didx][0], vidx, vp->FusedRatio, vp->OffsetVolts);
            }
          }

          AsciiPrint("\n");
        }
      }
    }
  }
}


/*******************************************************************************
 * PrintPlatformSettings
 ******************************************************************************/

VOID PrintPlatformSettings(IN PLATFORM* psys)
{
  if (gPrintPackageConfig) {
    for (UINTN pidx = 0; pidx < psys->PkgCnt; pidx++) {

      PACKAGE* pac = psys->packages + pidx;

      Print(
        L"+---------------------------------------------------------------------+\n"
      );

      Print(L"| Package %u |  ", pidx);

      AsciiPrint((CHAR8 *)pac->CpuInfo.venString);
      AsciiPrint("\n");


      Print(
        L"+-----------+------+------+--------+----------+-----------+----------+\n"
        L"| Vt Domain |  VR  | SVID | IccMax | VoltMode |  Vtarget  | Voffset  |\n"
        L"|-----------|------|------|--------|----------|-----------|----------|\n"
      );

      for (UINTN didx = 0; didx < MAX_DOMAINS; didx++) {
        if (DomainSupported((UINT8)didx)) {
          DOMAIN* dom = pac->planes + didx;

          Print(
            (dom->OffsetVolts < 0) ?
            L"|%s| 0x%02x |%s| %03u A  |%s|  %04u mV  | %04d mV  |\n" :
            L"|%s| 0x%02x |%s| %03u A  |%s|  %04u mV  |  %03d mV  |\n",

            vrDomainColStr[didx & 0x7],

            dom->VRaddr,

            &svidColStr[dom->VRtype & 0x1][0],
            (dom->IccMax) ? dom->IccMax >> 2 : 0,

            &voltModeColStr[dom->VoltMode & 0x1][0],

            (UINT32)dom->TargetVolts,
            (INT32)dom->OffsetVolts
          );
        }
      }

      Print(
        L"+-----------+------+------+--------+----------+-----------+----------+\n"
        L"\n");
    }
  }
}
