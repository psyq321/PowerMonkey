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

#include "VoltTables.h"
#include "FixedPoint.h"

/*******************************************************************************
 * 
 ******************************************************************************/

typedef union _VOLTS16 {
  UINT32 raw;

  struct {
    INT16  mv;
    UINT16 fx;
  } s;

  struct {
    UINT16 mv;
    UINT16 fx;
  } u;
} VOLTS16;

/*******************************************************************************
 * convert_offsetvolts_int16_fixed
 ******************************************************************************/

UINT16 cvrt_offsetvolts_i16_tofix(const INT16 in)
{
  ///
  /// If you wish to apply larger offset than +/- 250 mV
  /// you will need to replace the code in this routine
  /// 

  UINT64 nvolts = sizeof(OffsetVolts_S11) / (sizeof(VOLTS16));
  VOLTS16* vtbl = (VOLTS16*)&OffsetVolts_S11[0];

  if (in == 0)
    return 0;

  for (UINT64 vidx = 0; vidx < nvolts; vidx++) {
    if (vtbl[vidx].s.mv <= in) {
      return vtbl[vidx].s.fx;
    }
  }

  return VOLT_ERROR16;
}

/*******************************************************************************
 * convert_offsetvolts_fixed_int16
 ******************************************************************************/

INT16 cvrt_offsetvolts_fxto_i16(const UINT16 in)
{
  ///
  /// If you wish to apply larger offset than +/- 250 mV
  /// you will need to replace the code in this routine
  /// 

  UINT64 nvolts = sizeof(OffsetVolts_S11) / (sizeof(VOLTS16));
  VOLTS16* vtbl = (VOLTS16*)&OffsetVolts_S11[0];

  if (in == 0)
    return 0;

  for (UINT64 vidx = 0; vidx < nvolts; vidx++) {
    if (vtbl[vidx].s.fx == in) {
      return vtbl[vidx].s.mv;
    }
  }

  return (INT16)((in&0x400)?~((((INT64)((~in+1)&0x3ff))*1000)>>10) + 1 :
     ((((INT64)(in&0x3ff)))*1000)>>10); // booyah!
}

/*******************************************************************************
 * convert_ovrdvolts_fixed_int16
 ******************************************************************************/

UINT16 cvrt_ovrdvolts_fxto_i16(const UINT16 in)
{
  ///
  /// If you want to work with range larger than 250 mV - 1500 mV
  /// you will need to replace the code in this routine
  /// 

  UINT64 nvolts = sizeof(OverrdVolts_U12) / (sizeof(VOLTS16));
  VOLTS16* vtbl = (VOLTS16*)&OverrdVolts_U12[0];

  if (in == 0)
    return 0;

  for (UINT64 vidx = 0; vidx < nvolts; vidx++) {
    if (vtbl[vidx].u.fx == in) {
      return vtbl[vidx].u.mv;
    }
  }

  return (UINT16)((((INT64)(in&0xfff))*1000)>>10);
}

/*******************************************************************************
 * convert_offsetvolts_int16_fixed
 ******************************************************************************/

UINT16 cvrt_ovrdvolts_i16_tofix(const UINT16 in)
{
  ///
  /// If you want to work with range larger than 250 mV - 1500 mV
  /// you will need to replace the code in this routine
  /// 

  UINT64 nvolts = sizeof(OverrdVolts_U12) / (sizeof(VOLTS16));
  VOLTS16* vtbl = (VOLTS16*)&OverrdVolts_U12[0];

  if (in == 0)
    return 0;

  for (UINT64 vidx = 0; vidx < nvolts; vidx++) {
    if (vtbl[vidx].s.mv <= in) {
      return vtbl[vidx].s.fx;
    }
  }

  return VOLT_ERROR16;
}
