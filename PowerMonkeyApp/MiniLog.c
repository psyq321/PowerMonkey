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

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Protocol/MpService.h>
#include <Library/SynchronizationLib.h>

#if defined(__clang__)
#include <immintrin.h>
#endif

#include "CONFIGURATION.h"
#include "MiniLog.h"
#include "Platform.h"
#include "LowLevel.h"
#include "DelayX86.h"

extern PLATFORM* gPlatform;

/*******************************************************************************
 *
 ******************************************************************************/

#if defined(__GNUC__) && !defined(__clang__)
#include <x86intrin.h>
#else
#pragma intrinsic(__rdtsc)
#endif


/*******************************************************************************
 * Globals
 ******************************************************************************/

extern EFI_MP_SERVICES_PROTOCOL* gMpServices;
extern EFI_BOOT_SERVICES* gBS;
extern EFI_SYSTEM_TABLE* gST;
extern UINTN gBootCpu;

/*******************************************************************************
 *
 ******************************************************************************/

#ifdef ENABLE_MINILOG_TRACING

typedef struct _TraceOpCatalog
{
  UINT16 operId;
  CHAR8 operName[128];
} TraceOpCatalog;

TraceOpCatalog OpFriendlyNames[] = {
  { MINILOG_OPID_FREE_MSG,          "unknown" },
  { MINILOG_OPID_RDMSR64,           "rdmsr64" },
  { MINILOG_OPID_WRMSR64,           "wrmsr64" },
  { MINILOG_OPID_MMIO_READ32,       "mmio_read32" },
  { MINILOG_OPID_MMIO_WRITE32,      "mmio_write32" },
  { MINILOG_OPID_MMIO_OR32,         "mmio_or32" },
};

CHAR8* GetOperIdString(const UINT16 operId)
{
  CHAR8* rtc = &OpFriendlyNames[0].operName[0];

  for(UINTN cnt=0; cnt<sizeof(OpFriendlyNames)/sizeof(TraceOpCatalog);cnt++)
  {
    if (operId == OpFriendlyNames[cnt].operId) {
      return &OpFriendlyNames[cnt].operName[0];
    }
  }

  return rtc;
}

/*******************************************************************************
 * Simple Text Renderer (direct-to-framebuffer)
 * Code based on POSIX-UEFI, Copyright (C) 2021 bzt*
 ******************************************************************************/

#include "BmpFont.h"

#pragma pack (1)
/* Scalable Screen Font (https://gitlab.com/bztsrc/scalable-font2) */
typedef struct {
  unsigned char  magic[4];
  unsigned int   size;
  unsigned char  type;
  unsigned char  features;
  unsigned char  fbWidth;
  unsigned char  fbHeight;
  unsigned char  baseline;
  unsigned char  underline;
  unsigned short fragments_offs;
  unsigned int   characters_offs;
  unsigned int   ligature_offs;
  unsigned int   kerning_offs;
  unsigned int   cmap_offs;
} ssfn_font_t;
#pragma pack ()

/* framebuffer properties */

unsigned int fbWidth = 0;
unsigned int fbHeight = 0;
unsigned int fbPitch = 0;
BOOLEAN haveConsole = 0;

/* font to be used */
ssfn_font_t* font = (ssfn_font_t *) & _bmp_font[0];
unsigned char* lfb;

EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;


/*******************************************************************************
 * OutputRawString
 * Code based on POSIX-UEFI, Copyright (C) 2021 bzt* 
 ******************************************************************************/

VOID OutputRawString(UINT32 x, UINT32 y, CHAR8* s, UINT32 color)
{
  unsigned char* ptr, * chr, * frg;
  unsigned int c;
  UINT64 o, p;
  int i, j, k, l, m, n;
  while (*s) {
    c = *s; s += 1;
    if (c == '\r') { x = 0; continue; }
    else
      if (c == '\n') { x = 0; y += font->fbHeight; continue; }
    for (ptr = (unsigned char*)font + font->characters_offs, chr = 0, i = 0; i < 0x110000; i++) {
      if (ptr[0] == 0xFF) { i += 65535; ptr++; }
      else if ((ptr[0] & 0xC0) == 0xC0) { j = (((ptr[0] & 0x3F) << 8) | ptr[1]); i += j; ptr += 2; }
      else if ((ptr[0] & 0xC0) == 0x80) { j = (ptr[0] & 0x3F); i += j; ptr++; }
      else { if ((unsigned int)i == c) { chr = ptr; break; } ptr += 6 + ptr[1] * (ptr[0] & 0x40 ? 6 : 5); }
    }
    if (!chr) continue;
    ptr = chr + 6; o = (UINT64)lfb + y * fbPitch + x * 4;
    for (i = n = 0; i < chr[1]; i++, ptr += chr[0] & 0x40 ? 6 : 5) {
      if (ptr[0] == 255 && ptr[1] == 255) continue;
      frg = (unsigned char*)font + (chr[0] & 0x40 ? ((ptr[5] << 24) | (ptr[4] << 16) | (ptr[3] << 8) | ptr[2]) :
        ((ptr[4] << 16) | (ptr[3] << 8) | ptr[2]));
      if ((frg[0] & 0xE0) != 0x80) continue;
      o += (int)(ptr[1] - n) * fbPitch; n = ptr[1];
      k = ((frg[0] & 0x1F) + 1) << 3; j = frg[1] + 1; frg += 2;
      for (m = 1; j; j--, n++, o += fbPitch)
        for (p = o, l = 0; l < k; l++, p += 4, m <<= 1) {
          if (m > 0x80) { frg++; m = 1; }
          if (*frg & m) *((unsigned int*)p) = color;
        }
    }
    x += chr[4] + 1; y += chr[5];
  }


}

/*******************************************************************************
 * InitMiniConsole
 ******************************************************************************/

VOID InitMiniConsole()
{
  /* set video mode */
  EFI_STATUS status = gBS->LocateProtocol(&gopGuid, NULL, (void**)&gop);

  if (!EFI_ERROR(status) && gop) {
    status = gop->SetMode(gop, 0);
    gST->ConOut->Reset(gST->ConOut, 0);
    gST->StdErr->Reset(gST->StdErr, 0);
    
    if (EFI_ERROR(status)) {
      return;
    }

    /* set up destination buffer */
    lfb = (unsigned char*)gop->Mode->FrameBufferBase;
    fbWidth = gop->Mode->Info->HorizontalResolution;
    fbHeight = gop->Mode->Info->VerticalResolution;
    fbPitch = sizeof(unsigned int) * gop->Mode->Info->PixelsPerScanLine;
  }
  else {
    fbHeight = fbWidth = fbPitch = 0;
  }

  haveConsole = ((fbHeight != 0) && (fbWidth != 0));

}

/*******************************************************************************
 * InitTrace
 ******************************************************************************/

void InitTrace()
{
  InitMiniConsole();
}


/*******************************************************************************
 *
 ******************************************************************************/

void MiniTrace(
               const UINT8  operId,
               const UINT8  dangerous,
               const UINT32 param1,
               const UINT64 param2
                )
{
  ///
  /// Idiot's thread safety with no locks:
  /// each core gets a designated "stripe" of the video frame buffer
  /// I will make something better one day when I have more time...
  ///
  
  if (haveConsole) {
    
    //
    // Timestamp

    UINT64 tsns = TicksToNanoSeconds(ReadTsc());

    //
    // Get absolute core idx and use it to find the Y position for the trace

    VOID* gsbase = GetCpuGSBase();

    CPUCORE* core = gsbase;

    UINT8 pkgIdx = core->PkgIdx;
    UINT8 coreIdx = core->LocalIdx;

    {
      CHAR8 outBuf[80] = { 0 };

      AsciiSPrint(outBuf, 80, "[PKG%u][CORE%u][%u] - %a : 0x%x : 0x%lx",
        pkgIdx,
        coreIdx,
        tsns,
        GetOperIdString(operId),
        param1,
        param2);

      UINTN offsetY = 1;
      UINTN lineOffset = (offsetY + (2 * core->AbsIdx + 1)) * font->fbHeight;
      UINTN lineStride = gop->Mode->Info->PixelsPerScanLine * 4;
      UINTN startPix = lineOffset * lineStride;

      //
      // Delete line first

      memset(lfb + startPix, 0, lineStride*font->fbHeight);

      //
      // Render on screen 

      OutputRawString(0, (UINT32)lineOffset, outBuf, 0x00FFFFFF);
    }
  }
}


/*******************************************************************************
  MiniTraceEx
 ******************************************************************************/

void MiniTraceEx(
  IN  CONST CHAR8* format,
  ...
)
{
  VA_LIST mark;
  UINTN   nprinted;

  if (haveConsole) {

    //
    // Timestamp

    UINT64 tsns = TicksToNanoSeconds(ReadTsc());

    CHAR8 buf[160] = { 0 };
    CHAR8 tbuf[160] = { 0 };
    UINTN BufferSize = 160;

    VA_START(mark, format);
    nprinted = AsciiVSPrint(tbuf, BufferSize, format, mark);
    VA_END(mark);


    //
    // Get absolute core idx and use it to find the Y position for the trace

    VOID* gsbase = GetCpuGSBase();

    CPUCORE* core = gsbase;

    UINT8 pkgIdx = core->PkgIdx;
    UINT8 coreIdx = core->LocalIdx;

    AsciiSPrint(buf, 160, "[PKG%u][CORE%u][%u] %a", pkgIdx, coreIdx, tsns, tbuf);

    UINTN offsetY = 1;
    UINTN lineOffset = (offsetY + 2*core->AbsIdx) * font->fbHeight;
    UINTN lineStride = gop->Mode->Info->PixelsPerScanLine * 4;
    UINTN startPix = lineOffset * lineStride;

    //
    // Delete line first

    memset(lfb + startPix, 0, lineStride * font->fbHeight);

    //
    // Render on screen 

    OutputRawString(0, (UINT32)lineOffset, buf, 0x00FFFF80);    
  }
}


#endif