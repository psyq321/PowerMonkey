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

#include "Platform.h"
#include "MpDispatcher.h"
#include "VFTuning.h"
#include "TurboRatioLimits.h"

/*******************************************************************************
 *                   !!! WARNING - ACHTUNG - VNIMANIE !!!
 *     PROCEED IF YOU ARE AN EXPERT ONLY - DO NOT RANDOMLY "PUSH BUTTONS"!
 * 
 * You need to roll-your-own EFI binary by compiling this code. Here are some
 * basic steps to keep in mind before compiling and running:
 *
 * 1.   This POC was tested on mobile CML-H (Comet Lake) CPUs. It >should< work 
 *      on RKL/TGL/ICL, but it was not tested, so YMMV. It could also run on KBL
 *      /CFL and SKL platforms without V/F point overrides, but none of those 
 *      systems are tested as of now.
 *
 *      In any case, you must ensure that the programming (MSRs, etc.) are sane
 *      for your system. What is good for one system is not necessarily working
 *      on other. Settings used here are for example purposes only.
 *
 * 2.   Please review this file, adjust values appropriately (and check MSRs)
 *
 * 3.   Never test on production systems! Please consider using debugging rig 
 *      testing.  While it is not expected to end up with a brick whatever you 
 *      enter, it is always recommended to have either a board with brick-proof 
 *      emergency firmware recovery (that works) OR a hardware flash programmer
 * 
 * 4.   If you plan to use the settings as sticky in production, please make 
 *      sure you tested the system under stress to confirm stability!
 ******************************************************************************/

/*******************************************************************************
 * Global Application Settings
 ******************************************************************************/

///
/// Enable safer hardware probing (default: 1)
/// If PowerMonkey.efi cannot start but hangs your system, try disabling this 
/// flag as its mechanism is involving low-level hooking of system interrupts
/// and could trip some paranoid firmware
///

UINT8 gEnableSaferAsm = 1;

///
/// Disable UEFI watchdog timer
/// Will be useful once stress testing is fully implemented 
/// 

UINT8 gDisableFirwmareWDT = 0;

/*******************************************************************************
 * ApplyComputerOwnersPolicy()
 * 
 * This is where it is done. With no external config. Party like it's 1981.
 * 
 * NOTE: Voltage offsets are limited to +/- 250 mV. Please see VoltTables.c
 * if you wish to do more dangerous volt-mod, you will need to adapt the code
 ******************************************************************************/

/*******************************************************************************
 * Settings here are for a particular Core i7 10850H (mine) and need to be
 * adjusted for your CPU(s). Please do not just "run" the code but check your
 * values (e.g. run ThrottleStop if you use Windows) and change accordingly.
 * What you see below is an EXAMPLE ONLY.
 ******************************************************************************/

VOID ApplyComputerOwnersPolicy(IN PLATFORM* sys)
{
  //
  // We will loop through all packages (processor sockets)
  // and override (program) things we like changed.

  for (UINTN pidx = 0; pidx < sys->PkgCnt; pidx++)
  {
    PACKAGE* pk = sys->packages + pidx;

    /////////////////////////////////////
    /// Voltage / Frequency Overrides ///
    /////////////////////////////////////
    
    pk->Domain[IACORE].VoltOverrideMode = 0;  // Adaptive
    pk->Domain[IACORE].OffsetVolts =   -125;  // in mV (negative = undervolt)

    //
    // Note: some domains are sharing the same voltage plane! Check yours!
    // 
    // E.g.: for CML-H, IACORE (CPU cores) and RING (cache) share a common VR
    // if you don't program both linked domains to exactly the same voltage, 
    // CPU's pcode will take one (higher, which means less undervolt!) and
    // apply it to both domains - but without adjusting values submitted by the 
    // user so it appears everything went as planned. Some might believe they 
    // won the 'chip lottery' seeing their CPU seemingly undervolt to -250 mV 
    // or smth. while in reality pcode is doing exactly nothing because cache
    // is programmed to 0 mV offset. Don't be that guy!

    pk->Domain[RING].VoltOverrideMode = 0;   // IACORE linked, set to the same
    pk->Domain[RING].OffsetVolts = -125;     // IACORE linked, set to the same

    ////////////////////
    /// Turbo Ratios ///
    ////////////////////
    
    // 
    // Enable this to program "max ratio" for all turbo core counts
    // (e.g. 1C, 2C, 4C, 8C, = use this ratio). Remove or set to 0
    // if you do not wish to set it

    pk->ForcedRatioForAllCoreCounts = 51;


    ////////////////////
    /// Power Limits ///
    ////////////////////
    
    //
    // Modify settings below only if you believe your, e.g.notebook, is over
    // aggressively limiting power(e.g.in 'desk' mode on AC).Please note
    // that many notebooks are very nicely configured, or their cooling system
    // cannot take more heat.Also, just because some value is higher (say,
    // 135W instead of 68W), this doesn't mean you will end up with more 
    // performance, as the CPU might end up in throttle - fest due to thermal
    // overloading of the cooling system or just reaching CPU's fused limits.

    // If you want to do this properly, you need to set up a benchmark script
    // simulating your workloads and carefully tweak the PLx values until you
    // hit the best scores. New systems also have more flexibility, such as
    // "dual Tau boost" that add more control levels and possibly make your
    // system >actually< faster at the end.Once you are done with benchmarking
    // and testing, PowerMonkey will happily set those values for you before 
    // OS or Hypervisor kick in.

    // Note: MAX_POWAH constant will still respect maximum PLx as reported by
    // the firmware.Some firmware(e.g.mine) report 0 instead, in which case
    // one will end up with programmed values like "4095 Watts".If you do not
    // like the checks done, you need to modify the code.But in that case, the
    // firmware might still refuse to program too high of a setting.

    // Bonus special : once there used to be one set of limits, you could
    // configure using an MSR.Then, vendors started introducing moar PLs(pkg,
    // platform, etc.) and also more ways to program(MSR, MMIO).Then, someone
    // had the great idea to use more than one record(say, MSRand MMIO) and
    // combine them into a more flexible config space.Of course, OEMs also
    // need more control(PL3, PL4, ...) and in the case of a notebook, say,
    // you wish OEM to have the final say so that EC can override anything with
    // emergency values.That would save you from melting your notebook, or
    // worse.There is no guide here to deal with this - please consult forums
    // if you do not have access to the relevant BWG doc.Or ignore all this
    // noise and set everything to MAX_POWAH as many will promptly do).
    // 
    // MAX_POWAH is a dummy value indicating to the policy updater that the
    // user prefers to set the value to "maximum allowed range". Useful for 
    // isolating sources of throttling. But it will not magically allow your
    // 13"-thin-and-light notebook to generate 500W of heat. Finding optimal
    // PLx values is much better idea.

    //
    // Select which parameters you want to program
    // Other parameters will be ignored during programming step

    pk->ProgramPL12_MSR =  1;               // [WRITE] Program MSR PL1/2
    pk->ProgramPL12_MMIO = 1;               // [WRITE] Program MMIO PL1/2
    pk->ProgramPL12_PLAT = 1;               // [WRITE] Program Platform PLs
    pk->ProgramPL3 = 1;                     // [WRITE] Program PL3
    pk->ProgramPL4 = 1;                     // [WRITE] Program PL4
    pk->ProgramPP0 = 1;                     // [WRITE] Program PP0

    //////////////
    // Settings //
    //////////////

    //
    // Configurable TDP (cTDP)

    pk->MaxCTDPLevel = 0;                   // 0 disables cTDP
    pk->TdpControLock = 1;                  // Locks TDP config

    //
    // Package PL1/PL2 (MSR)
    
    pk->EnableMsrPkgPL1 = 1;                // Enable PL1 
    pk->EnableMsrPkgPL2 = 1;                // Enable PL2
    pk->MsrPkgPL1_Power = MAX_POWAH;        // PL1 in mW or MAX_POWAH
    pk->MsrPkgPL2_Power = MAX_POWAH;        // PL2 in mW or MAX_POWAH
    pk->MsrPkgPL_Time =   MAX_POWAH;        // Tau in ms or MAX_POWAH
    pk->ClampMsrPkgPL =   0;                // Allow clamping
    pk->LockMsrPkgPL12 =  1;                // Lock after programming

    //
    // Package PL1/PL2 (MMIO)

    pk->EnableMmioPkgPL1 = 1;               // Enable MMIO PL1
    pk->EnableMmioPkgPL2 = 1;               // Enable MMIO PL2
    pk->MmioPkgPL1_Power = MAX_POWAH;       // MMIO PL1 in mW or MAX_POWAH
    pk->MmioPkgPL2_Power = MAX_POWAH;       // MMIO PL2 in mW or MAX_POWAH
    pk->MmioPkgPL_Time   = MAX_POWAH;       // Tau in ms or MAX_POWAH
    pk->ClampMmioPkgPL =  0;                // Allow clamping
    pk->LockMmioPkgPL12 = 1;                // Lock after programming

    //
    // Platform (PSys) PL1/PL2 

    pk->EnablePlatformPL1 = 1;              // Enable MSR PL1 
    pk->EnablePlatformPL2 = 0;              // Enable MSR PL2
    pk->PlatformPL1_Power = MAX_POWAH;      // PSys PL1 in mW or MAX_POWAH
    pk->PlatformPL_Time =   MAX_POWAH;
    pk->PlatformPL2_Power = 0;              // PSys PL2 in mW or MAX_POWAH
    pk->ClampPlatformPL = 0;
    pk->LockPlatformPL = 1;                 // Lock after programming

    //
    // Package PL3

    pk->EnableMsrPkgPL3 = 1;                // Enable MSR PL4
    pk->MsrPkgPL3_Power = MAX_POWAH;        // PL3 in mW or MAX_POWAH
    pk->MsrPkgPL3_Time  = MAX_POWAH;        // Tau in ms or MAX_POWAH
    pk->LockMsrPkgPL3 = 1;                  // Lock PL1 after programming

    //
    // Package PL4
    //
    // Note: I do not recommend pushing this setting up. It sits near the
    // end of possible strategies to avoid power escalation. Setting it too
    // high might bring your system so out of spec that physical damage can
    // occur. While some systems have additional protections, one should not
    // test this damage can be irreversible. I'd consider changing this only
    // if I have strong reason to suspect it is too low (seeing CPU throwing
    // a lot of "

    pk->EnableMsrPkgPL4 = 1;                // Enable PL4
    pk->MsrPkgPL4_Current = MAX_POWAH;      // MSR PL4 Amperes or MAX_POWAH
    pk->LockMsrPkgPL4 = 1;                  // Lock PL4 after programming

    //
    // PP0

    pk->EnableMsrPkgPP0 = 1;                // Enable MSR PP0
    pk->MsrPkgPP0_Power = MAX_POWAH;        // Power in mW or MAX_POWAH
    pk->MsrPkgPP0_Time =  MAX_POWAH;        // Time in ms or MAX_POWAH
    pk->ClampMsrPP0 = 0;                    // Clamp
    pk->LockMsrPP0 = 1;                     // Lock after programming
  }
}

