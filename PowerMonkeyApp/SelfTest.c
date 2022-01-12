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
#include <Protocol/MpService.h>

#include "Constants.h"
#include "MpDispatcher.h"
#include "./ASMx64/ComboHell_AVX2.h"
#include "SelfTest.h"

/*******************************************************************************
 * Globals
 ******************************************************************************/

UINT64 gSelfTestErrorCnt = 0;
UINT64 gSelfTestStopReq = 0;

/*******************************************************************************
 * 
 ******************************************************************************/

typedef struct _cbhell_ymm_input
{
  /* Segment 1 (INT) */
  UINT64 seg1[32];                                       // YMM0 - YMM7

  /* Segment 2 (FLOAT) */
  float  seg2[24];                                       // YMM8  - YMM10

  /* Segment 2 (INT) */
  UINT64 seg3[16];                                       // YMM11 - YMM14

} cbhell_ymm_input;


/*******************************************************************************
 * ComboHell_AVX2 Stressor Data
 ******************************************************************************/

ALIGN32 UINT64 combo_scratch2[64] = { 0 };

ALIGN32 cbhell_ymm_input combo_scratch1 = 
{  
  /* Segment 1: YMM0-YMM7 (INT) */
  {
    0xf2d9b69cde7126f6,0x4ef89b1ee3587a88,0x8bade59b01287609,0x1d5908932c140d50,
    0x1d9d15095fef63ef,0xdafb2fec9cbb0809,0xd50739e4e7607e2f,0x6926128fd905c558,
    0x984b80a9c9ad9fcb,0x3df0ca50c7e36d7c,0xa851bf4f41b0574b,0x6323cc9fad1c6c7a,
    0x426136bddceb57cc,0xe276142b7c41cd02,0x3daa5845337b9965,0x83ac0bfc585de72c,
    0xa3053ffc739be9c6,0xe7de4051c3084bed,0xc97e11cbbd96987e,0x4d34a7850493e960,
    0xe0780f67909d2e9c,0x29665376a843b663,0x4321feb725cb3a02,0x8c90989a7c7073b3,
    0x71a16ea6b8c73894,0x5076e42420a7b631,0x3d71cdd4f22e0b94,0xf4bb51ebbfa3d4d3,
    0x82166a2f03eb52d9,0x6419925cec9a4c96,0x9e92c6764771ef41,0x26cc4843178d951a,
  },

  /* Segment 2: YMM7-YMM10 (FLOAT) */
  {
    4.3558821678161621e+00, 1.8015301227569580e+00, 8.5878667831420898e+00, 
    9.2199945449829102e+00, 6.7271840572357178e-01, 5.7031702995300293e+00, 
    6.8538513183593750e+00, 2.1444392204284668e+00, 9.5205793380737305e+00, 
    1.6363885402679443e+00, 3.9323928356170654e+00, 7.4515256881713867e+00, 
    5.3094725608825684e+00, 6.2171483039855957e+00, 4.5293660163879395e+00, 
    5.6793189048767090e+00, 5.2591872215270996e+00, 4.9866528511047363e+00, 
    2.7079889774322510e+00, 7.9869598150253296e-01, 8.2738904953002930e+00, 
    7.4048846960067749e-01, 7.2761530876159668e+00, 8.5576260089874268e-01,
  },

  /* Segment 3: YMM11-YMM14 (INT) */
  {
    0x4fa2a031c4309efd,0xed49b6353e6541f0,0x7bb317121ee437fb,0x2bfbf88fcd07e612,
    0x6653c6fba2c8096a,0x60c94096f3b6995c,0x0cdd64e648e86a62,0xbfd9c8c3205c4c12,
    0x4fa7db6ce5518687,0x709428dd3f549b7f,0xc042d380aa270166,0x1c040a215d4b0886,
    0xccd5d886b8e2fc09,0xccdf032acb6b1bac,0x6a4ba7b79eb12b15,0x9135887e1166f07b,
  },
};

/*******************************************************************************
 * PM_ComboHell_Thread
 ******************************************************************************/

VOID EFIAPI PM_ComboHell_Thread(IN OUT VOID* Buffer)
{
  //
  // Run ASM kernel
  
  combohell_avx2_kernel( (void*)&combo_scratch1, (void*)&combo_scratch2 );
}


/*******************************************************************************
 * PM_SelfTest
 ******************************************************************************/

EFI_STATUS PM_SelfTest(VOID)
{
  EFI_STATUS status = EFI_SUCCESS;

  //
  // Prepare for testing

  ComboHell_TerminateOnError = 0;
  ComboHell_MaxRuns = gSelfTestMaxRuns;

  ComboHell_ErrorCounterPtr = (void*)&gSelfTestErrorCnt;
  ComboHell_StopRequestPtr =  (void*)&gSelfTestStopReq;

  gSelfTestErrorCnt = 0;
  gSelfTestStopReq = 0;

  //
  // Execute the same stress-kernel on every CPU
 
  AsciiPrint("[SelfTest] Running %u Iterations of ComboHell_AVX2 Stressor\n",
    gSelfTestMaxRuns);

  RunOnAllProcessors(PM_ComboHell_Thread, TRUE, NULL);

  AsciiPrint( "Self test completed with %u errors.\n", 
    gSelfTestErrorCnt);

  return status;
}