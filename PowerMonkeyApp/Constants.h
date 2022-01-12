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

#pragma once

/*******************************************************************************
 * 
 ******************************************************************************/

#if defined(_MSC_VER)
#define ALIGN8  __declspec(align(8))
#define ALIGN16 __declspec(align(16))
#define ALIGN32 __declspec(align(32))
#else
#define ALIGN8  __attribute((aligned(8)))
#define ALIGN16 __attribute((aligned(16)))
#define ALIGN32 __attribute((aligned(32)))
#endif
#define IUNUSED(x) (void)x;

/*******************************************************************************
 *
 ******************************************************************************/

#define bitinv8(a)   ((UINT8)~(a))
#define bitinv16(a)  ((UINT16)~(a))

#define one8u     0x01u 
#define one16u    0x0001u
#define one32u    0x00000001ul 

#define bit0u8    ((UINT8)one8u)
#define bit1u8    ((UINT8)one8u<<1)
#define bit2u8    ((UINT8)one8u<<2)
#define bit3u8    ((UINT8)one8u<<3)
#define bit4u8    ((UINT8)one8u<<4)
#define bit5u8    ((UINT8)one8u<<5)
#define bit6u8    ((UINT8)one8u<<6)
#define bit7u8    ((UINT8)one8u<<7)

#define bit0u16   ((UINT16)one16u)
#define bit1u16   ((UINT16)one16u<<1)
#define bit2u16   ((UINT16)one16u<<2)
#define bit3u16   ((UINT16)one16u<<3)
#define bit4u16   ((UINT16)one16u<<4)
#define bit5u16   ((UINT16)one16u<<5)
#define bit6u16   ((UINT16)one16u<<6)
#define bit7u16   ((UINT16)one16u<<7)
#define bit8u16   ((UINT16)one16u<<8)
#define bit9u16   ((UINT16)one16u<<9)
#define bit10u16  ((UINT16)one16u<<10)
#define bit11u16  ((UINT16)one16u<<11)
#define bit12u16  ((UINT16)one16u<<12)
#define bit13u16  ((UINT16)one16u<<13)
#define bit14u16  ((UINT16)one16u<<14)
#define bit15u16  ((UINT16)one16u<<15)

#define bit0u32   ((UINT32)one32u)
#define bit1u32   ((UINT32)one32u<<1)
#define bit2u32   ((UINT32)one32u<<2)
#define bit3u32   ((UINT32)one32u<<3)
#define bit4u32   ((UINT32)one32u<<4)
#define bit5u32   ((UINT32)one32u<<5)
#define bit6u32   ((UINT32)one32u<<6)
#define bit7u32   ((UINT32)one32u<<7)
#define bit8u32   ((UINT32)one32u<<8)
#define bit9u32   ((UINT32)one32u<<9)
#define bit10u32  ((UINT32)one32u<<10)
#define bit11u32  ((UINT32)one32u<<11)
#define bit12u32  ((UINT32)one32u<<12)
#define bit13u32  ((UINT32)one32u<<13)
#define bit14u32  ((UINT32)one32u<<14)
#define bit15u32  ((UINT32)one32u<<15)
#define bit16u32  ((UINT32)one32u<<16)
#define bit17u32  ((UINT32)one32u<<17)
#define bit18u32  ((UINT32)one32u<<18)
#define bit19u32  ((UINT32)one32u<<19)
#define bit20u32  ((UINT32)one32u<<20)
#define bit21u32  ((UINT32)one32u<<21)
#define bit22u32  ((UINT32)one32u<<22)
#define bit23u32  ((UINT32)one32u<<23)
#define bit24u32  ((UINT32)one32u<<24)
#define bit25u32  ((UINT32)one32u<<25)
#define bit26u32  ((UINT32)one32u<<26)
#define bit27u32  ((UINT32)one32u<<27)
#define bit28u32  ((UINT32)one32u<<28)
#define bit29u32  ((UINT32)one32u<<29)
#define bit30u32  ((UINT32)one32u<<30)
#define bit31u32  ((UINT32)one32u<<31)
