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
#include <Library/BaseMemoryLib.h>
#include "SaferAsmHdr.h"

/*******************************************************************************
* Interrupt Descriptor Table (IDT) and its corresponding register (IDTR)
* See: https://wiki.osdev.org/Interrupt_Descriptor_Table and
* Intel 64 and IA-32 Architectures Software Developer’s Manual, Vol 3A, 5.10
******************************************************************************/

 
/*******************************************************************************
* type_addr: https://wiki.osdev.org/Interrupt_Descriptor_Table
*
* Example:   8E = 10001110
*                 |   ||||
*                 |   Gate type = Interrupt Gate
*                 Used / Unused = Switch it on!!
******************************************************************************/

/////////
// IDT //
/////////

#pragma pack (1)
typedef union {
  struct {
    UINT32  OffsetLow   : 16;
    UINT32  Selector    : 16;
    UINT32  Reserved1   : 8;
    UINT32  TypeAddr    : 8;
    UINT32  OffsetHigh  : 16;
    UINT32  OffsetUpper : 32;
    UINT32  Reserved2   : 32;
  } u1;
  struct {
    UINT64  q[2];
  } u64;
} IDT;

//////////
// IDTR //
//////////

typedef struct _IDTR {
  UINT16  Limit;
  UINT64  Base;
} IDTR;
#pragma pack ()

/*******************************************************************************
* This is a list of interrupts we want to redirect to our ISRs. Remaining IRQs
* will go to the handlers set by UEFI firmware or whoever else operates this.
* 
* NOTE: for voltage stability testing, it is probably a good idea to override
* all exception ISRs (0x00 to 0x1F) as you probably do not want some serious 
* code written 'for revenue' to deal with garbage made by glitched CPU (SOP for 
* any grownup code: reboot immediately, maybe even call tactical police on you)
*******************************************************************************/

typedef struct _ISROVERRIDE {
  UINT8 vector;                             ///< Vector # we want to override
  VOID* isr;                                ///< Address of the ISR
} ISROVERRIDE;

//////////////////////////////////
// Example list for MSR probing //
//////////////////////////////////

ISROVERRIDE isrlist_probingmode[] = {
  { 0x05,   &monkey_isr_5, },               ///< (#BR) Bound Range Exceeded
  { 0x06,   &monkey_isr_6, },               ///< (#UD) Invalid Opcode
  { 0x08,   &monkey_isr_8, },               ///< (#DF) Double Fault
  { 0x0D,   &monkey_isr_13, },              ///< (#GP) General Protection Fault
  { 0x12,   &monkey_isr_18, },              ///< (#MC) Machine Check
};

/*******************************************************************************
* Backup table will be used when completing the hook
*******************************************************************************/

IDT gBackupIDT[256] = { 0 };                ///< Backup of original IDT
IDT* sysIDT;

IDTR origIDTR = { 0 };
UINT32 gISRsPatched = 0;

/*******************************************************************************
* PatchIDTEntry
* 
* type_addr: https://wiki.osdev.org/Interrupt_Descriptor_Table
* 
* Example:   8E = 10001110
*                 |   ||||
*                 |   Gate type = Interrupt Gate
*                 Used / Unused = Switch it on!!
******************************************************************************/

VOID PatchIDTEntry(IDT* idtBase, ISROVERRIDE* isro)
{
  //
  // Locate the position in IDT for this ISR
  
  IDT* dest = idtBase + isro->vector;

  //
  // And patch...

  UINT64 isrAddr = (UINT64)isro->isr;

  dest->u1.OffsetLow = (UINT16) isrAddr;
  dest->u1.OffsetHigh = (UINT16)(isrAddr >> 16);
  dest->u1.OffsetUpper = (UINT32)(isrAddr >> 32);
  dest->u1.TypeAddr = 0x8E;                         ///< See explanation above
}

/*******************************************************************************
* UnpatchIDTEntry
******************************************************************************/

VOID UnpatchIDTEntry(IDT* idtBase, ISROVERRIDE* isro)
{
  //
  // Unpatch 

  IDT* dest = idtBase + isro->vector;
  IDT* orig = gBackupIDT + isro->vector;

  CopyMem(dest, orig, sizeof(IDT));
}


/*******************************************************************************
* ApplyISRPatchTable
*
* Well... it does what it says on the tin. 
* Backup IDT will be used to restore OG ISRs once we are done...
******************************************************************************/

VOID ApplyISRPatchTable( ISROVERRIDE *isrs, 
  const INTN isrCount, 
  const UINT8 doUnapply)
{
  //
  // Probably a good idea to disable interrupts now...
  
  stop_interrupts_on_this_cpu();

  ///////////////////
  // (Un)patch the //
  // entire list   //
  ///////////////////
  
  //
  // Compiler will optimize the loop and move the if() out
  // Let's not make the code uglier than it already is...
    
  for (INTN idx = 0; idx < isrCount; idx++) {
    if (!doUnapply) {
      PatchIDTEntry(sysIDT, isrs + idx);
    }
    else {
      UnpatchIDTEntry(sysIDT, isrs + idx);
    }    
  }

  //
  // And... action
  // (or crash)
  
  gISRsPatched = 1;
  
  resume_interrupts_on_this_cpu();
}


/*******************************************************************************
* InstallSafeAsmExceptionHandler()
*
* We will handle interrupts for general protection and invalid opcode faults
* that could arise using rdmsr / wrmsr instructions.
*
* And of course, perks of running on bare metal are - we can just steal the
* IDT like bandits and get away with it. There is no OS below us to complain.
******************************************************************************/

VOID InstallSafeAsmExceptionHandler(VOID)
{
  //
  // Locate current IDTR

  get_current_idtr(&origIDTR);

  //
  // Copy current IDT table to backup
  // so we can restore it after we are done

  sysIDT = (IDT *)origIDTR.Base;
  
  CopyMem( &gBackupIDT[0], sysIDT, sizeof(IDT) * 256);

  //
  // Now that we have backups, we can start hacking
  // I mean patching...
 
  ApplyISRPatchTable(isrlist_probingmode,
    sizeof(isrlist_probingmode) / sizeof(ISROVERRIDE), 0);
    
}

/******************************************************************************
* RemoveAllInterruptOverrides()
******************************************************************************/

VOID RemoveAllInterruptOverrides(VOID)
{
  //
  // gISRsPatched == 1 means there is a valid IDT backup
  
  if (gISRsPatched) {

    stop_interrupts_on_this_cpu();

    CopyMem(sysIDT, &gBackupIDT[0], sizeof(IDT) * 256);

    gISRsPatched = 0;

    resume_interrupts_on_this_cpu();
  }
}
