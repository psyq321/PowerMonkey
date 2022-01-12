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

#include <Uefi.h>
#include <Library/UefiLib.h>

#include "CpuInfo.h"
#include "CpuData.h"

/*******************************************************************************
 * Templates for VR Topology Discovery
 ******************************************************************************/

/////////////////////////
/// CFL/CML/RKL Client //
/////////////////////////

VOLTCFGTEMPLATE vcfg_q_xyzlake_client = 
{

  {

    ///
    /// IA Cores
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0x1E0,                          // Bit mask for retrieving VR Addr (OCMB)
      5,                              // VR Addr bits to shift right

      0x200,                          // Bit mask for asking if this VR is SVID
      9,                              // VR svid capa bits to shift right

      "CORE",                         // Friendly Name (Short)
    },

    ///
    /// GT Slice
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0xF00000,                       // Bit mask for retrieving VR Addr (OCMB)
      20,                             // VR Addr bits to shift right

      0x1000000,                      // Bit mask for asking if this VR is SVID
      24,                             // VR svid capa bits to shift right

      "GTSLICE",
    },

    ///
    /// Ring / Cache
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0x3C00,                         // Bit mask for retrieving VR Addr (OCMB)
      10,                             // VR Addr bits to shift right

      0x4000,                         // Bit mask for asking if this VR is SVID
      14,                             // VR svid capa bits to shift right

      "RING",
    },

    ///
    /// GT Unslice
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0x78000,                        // Bit mask for retrieving VR Addr (OCMB)
      15,                             // VR Addr bits to shift right

      0x80000,                        // Bit mask for asking if this VR is SVID
      19,                             // VR svid capa bits to shift right

      "GTUNSLICE",
    },

    ///
    /// UNCORE / SA
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0xF,                            // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0x10,                           // Bit mask for asking if this VR is SVID
      0x4,                            // VR svid capa bits to shift right

      "UNCORE",
    },

    ///
    ///
    /// 

    {
      0x00,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "RESERVED",
    }
  }
};

//////////////////////
/// SKX/CLX HEDT/WS //
//////////////////////

VOLTCFGTEMPLATE vcfg_q_xyzlake_server =
{

  {

    ///
    /// IA Cores
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0x1E0,                          // Bit mask for retrieving VR Addr (OCMB)
      5,                              // VR Addr bits to shift right

      0x200,                          // Bit mask for asking if this VR is SVID
      9,                              // VR svid capa bits to shift right

      "CORE",                         // Friendly Name (Short)
    },

    ///
    /// 
    /// 

    {
      0x00,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "RESERVED",
    },

    ///
    /// Ring / Cache
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0x3C00,                         // Bit mask for retrieving VR Addr (OCMB)
      10,                             // VR Addr bits to shift right

      0x4000,                         // Bit mask for asking if this VR is SVID
      14,                             // VR svid capa bits to shift right

      "RING",
    },

    ///
    /// 
    /// 

    {
      0x00,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "RESERVED",
    },

    ///
    /// UNCORE / SA
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0xF,                            // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0x10,                           // Bit mask for asking if this VR is SVID
      0x4,                            // VR svid capa bits to shift right

      "UNCORE",
    },

    ///
    ///
    /// 

    {
      0x00,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "RESERVED",
    }
  }
};


////////////////////////////////
/// TGL Target --> INCOMPLETE //
////////////////////////////////

//
// Note - TGL has more complex VR topology
// at this moment, IccMax cannot be set (TODO: VR addresses) 

VOLTCFGTEMPLATE vcfg_q_tigerlake_client =
{

  {

    ///
    /// IA Cores
    /// 

    {
      0x01,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "CORE",                         // Friendly Name (Short)
    },

    ///
    /// GT
    /// 

    {
      0x01,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "GT",
    },

    ///
    /// Ring / Cache
    /// 

    {
      0x01,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "RING",
    },

    ///
    /// RESERVED
    /// 

    {
      0x00,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "RESERVED",
    },

    ///
    /// UNCORE / SA
    /// 

    {
      0x01,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0x0,                            // Bit mask for asking if this VR is SVID
      0x0,                            // VR svid capa bits to shift right

      "UNCORE",
    },

    ///
    /// RESERVED
    /// 

    {
      0x00,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0,                              // Bit mask for retrieving VR Addr (OCMB)
      0,                              // VR Addr bits to shift right

      0,                              // Bit mask for asking if this VR is SVID
      0,                              // VR svid capa bits to shift right

      "RESERVED",
    }
  }
};


//////////////////////////
/// Alder Lake Client  ///
//////////////////////////

VOLTCFGTEMPLATE vcfg_q_alderlake_client = {

  {

    ///
    /// IA Cores
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0xF00,                          // Bit mask for retrieving VR Addr (OCMB)
      8,                              // VR Addr bits to shift right

      0x1000,                         // Bit mask for asking if this VR is SVID
      12,                             // VR svid capa bits to shift right

      "PCORE",                        // Friendly name - short
    },

    ///
    /// GT Slice
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0x1E000,                        // Bit mask for retrieving VR Addr (OCMB)
      13,                             // VR Addr bits to shift right

      0x800000,                       // Bit mask for asking if this VR is SVID
      17,                             // VR svid capa bits to shift right

      "GTSLICE",
    },

    ///
    /// Ring / Cache
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0xF00,                          // Bit mask for retrieving VR Addr (OCMB)
      8,                              // VR Addr bits to shift right

      0x1000,                         // Bit mask for asking if this VR is SVID
      12,                             // VR svid capa bits to shift right

      "RING",
    },

    ///
    /// GT Unslice
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0x1E000,                        // Bit mask for retrieving VR Addr (OCMB)
      13,                             // VR Addr bits to shift right

      0x800000,                       // Bit mask for asking if this VR is SVID
      17,                             // VR svid capa bits to shift right

      "GTUNSLICE",
    },

    ///
    /// UNCORE / SA
    /// 

    {
      0x01,                           // Domain exists?
      0x00,                           // Domain Supported for OCMB discovery?

      0x0,                            // Bit mask for retrieving VR Addr (OCMB)
      0x0,                            // VR Addr bits to shift right

      0x0,                            // Bit mask for asking if this VR is SVID
      0x0,                            // VR svid capa bits to shift right

      "UNCORE",
    },

    ///
    /// E-Cores
    /// 

    {
      0x01,                           // Domain exists?
      0x01,                           // Domain Supported for OCMB discovery?

      0xF00,                          // Bit mask for retrieving VR Addr (OCMB)
      8,                              // VR Addr bits to shift right

      0x1000,                         // Bit mask for asking if this VR is SVID
      12,                             // VR svid capa bits to shift right

      "ECORE",
    }
  }
};

