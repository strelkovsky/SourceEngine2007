// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/dbg.h"
#include "tier0/include/memalloc.h"
#include "tier0/include/platform.h"
#include "tier0/include/vcrmode.h"

#include <sys/time.h>
#include <unistd.h>

double Plat_FloatTime() {
  struct timeval tp;
  static int secbase = 0;

  gettimeofday(&tp, NULL);

  if (!secbase) {
    secbase = tp.tv_sec;
    return (tp.tv_usec / 1000000.0);
  }

  if (VCRGetMode() == VCR_Disabled)
    return ((tp.tv_sec - secbase) + tp.tv_usec / 1000000.0);

  return VCRHook_Sys_FloatTime((tp.tv_sec - secbase) + tp.tv_usec / 1000000.0);
}

unsigned long Plat_MSTime() {
  struct timeval tp;
  static int secbase = 0;

  gettimeofday(&tp, NULL);

  if (!secbase) {
    secbase = tp.tv_sec;
    return (tp.tv_usec / 1000.0);
  }

  return (unsigned long)((tp.tv_sec - secbase) * 1000.0f + tp.tv_usec / 1000.0);
}

bool vtune(bool resume) {}

SOURCE_TIER0_API void Plat_DefaultAllocErrorFn(unsigned long size) {}

typedef void (*Plat_AllocErrorFn)(unsigned long size);
Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;

SOURCE_TIER0_API void *Plat_Alloc(unsigned long size) {
  void *pRet = g_pMemAlloc->Alloc(size);
  if (pRet) {
    return pRet;
  } else {
    g_AllocError(size);
    return 0;
  }
}

SOURCE_TIER0_API void *Plat_Realloc(void *ptr, unsigned long size) {
  void *pRet = g_pMemAlloc->Realloc(ptr, size);
  if (pRet) {
    return pRet;
  } else {
    g_AllocError(size);
    return 0;
  }
}

SOURCE_TIER0_API void Plat_Free(void *ptr) { g_pMemAlloc->Free(ptr); }

SOURCE_TIER0_API void Plat_SetAllocErrorFn(Plat_AllocErrorFn fn) {
  g_AllocError = fn;
}

static char g_CmdLine[2048];
SOURCE_TIER0_API void Plat_SetCommandLine(const char *cmdLine) {
  strncpy(g_CmdLine, cmdLine, sizeof(g_CmdLine));
  g_CmdLine[sizeof(g_CmdLine) - 1] = 0;
}

SOURCE_TIER0_API const char *Plat_GetCommandLine() { return g_CmdLine; }
