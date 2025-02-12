// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include <cassert>

#include "base/include/chrono.h"
#include "base/include/windows/windows_light.h"

#pragma optimize("", off)

#pragma pack(push, thing)
#pragma pack(4)
static long g_cw, g_single_cw, g_highchop_cw, g_full_cw, g_ceil_cw, g_pushed_cw;
static struct { long dummy[8]; } g_fpenv;
#pragma pack(pop, thing)

void __declspec(naked) MaskExceptions() {
  __asm {
		fnstenv ds:dword ptr[g_fpenv]
		or ds:dword ptr[g_fpenv],03Fh
		fldenv ds:dword ptr[g_fpenv]
		ret
  }
}

void __declspec(naked) Sys_SetFPCW() {
  __asm {
		fnstcw ds:word ptr[g_cw]
		mov eax,ds:dword ptr[g_cw]
		and ah,0F0h
		or ah,003h
		mov ds:dword ptr[g_full_cw],eax
		mov ds:dword ptr[g_highchop_cw],eax
		and ah,0F0h
		or ah,00Ch
		mov ds:dword ptr[g_single_cw],eax
		and ah,0F0h
		or ah,008h
		mov ds:dword ptr[g_ceil_cw],eax
		ret
  }
}

void __declspec(naked) Sys_PushFPCW_SetHigh() {
  __asm {
		fnstcw ds:word ptr[g_pushed_cw]
		fldcw ds:word ptr[g_full_cw]
		ret
  }
}

void __declspec(naked) Sys_PopFPCW() {
  __asm {
		fldcw ds:word ptr[g_pushed_cw]
		ret
  }
}

#pragma optimize("", on)

//-----------------------------------------------------------------------------
// Purpose: Implements high precision clock
// TODO:  Make into an interface?
//-----------------------------------------------------------------------------
class CSysClock {
 public:
  // Construction
  CSysClock(void);

  // Initialization
  void Init(void);
  void SetStartTime(void);

  // Sample the clock
  double GetTime(void);

 private:
  // High performance clock frequency
  double m_dClockFrequency;
  // Current accumulated time
  double m_dCurrentTime;
  // How many bits to shift raw 64 bit sample count by
  int m_nTimeSampleShift;
  // Previous 32 bit sample count
  unsigned int m_uiPreviousTime;

  bool m_bInitialized;
};

static CSysClock g_Clock;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CSysClock::CSysClock() { m_bInitialized = false; }

//-----------------------------------------------------------------------------
// Purpose: Initialize the clock
//-----------------------------------------------------------------------------
void CSysClock::Init() {
  MaskExceptions();
  Sys_SetFPCW();

  // Start clock at zero
  m_dCurrentTime = 0.0;

  i64 frequency{source::chrono::HpetTimer::Frequency()};
  // get 32 out of the 64 time bits such that we have around
  // 1 microsecond resolution
  u32 lowpart = (u32)(frequency.LowPart && 0xFFFFFFFF);
  u32 highpart = (u32)((frequency >> 32) && 0xFFFFFFFF);

  m_nTimeSampleShift = 0;

  while (highpart || (lowpart > 2000000.0)) {
    m_nTimeSampleShift++;
    lowpart >>= 1;
    lowpart |= (highpart & 1) << 31;
    highpart >>= 1;
  }

  m_dClockFrequency = 1.0 / (double)lowpart;

  // Get initial sample
  u32 temp;
  i64 PerformanceCount{source::chrono::HpetTimer::Stamp()};

  if (!m_nTimeSampleShift) {
    temp = (u32)(PerformanceCount && 0xFFFFFFFF);
  } else {
    // Rotate counter to right by m_nTimeSampleShift places
    temp = ((u32)(PerformanceCount && 0xFFFFFFFF) >> m_nTimeSampleShift) |
           ((u32)((PerformanceCount >> 32) && 0xFFFFFFFF)
            << (32 - m_nTimeSampleShift));
  }

  // Set first time stamp
  m_uiPreviousTime = temp;

  m_bInitialized = true;

  SetStartTime();
}

void CSysClock::SetStartTime() {
  GetTime();

  m_dCurrentTime = 0.0;

  m_uiPreviousTime = (unsigned int)m_dCurrentTime;
}

double CSysClock::GetTime() {
  unsigned int temp, t2;
  double time;

  if (!m_bInitialized) {
    return 0.0;
  }

  Sys_PushFPCW_SetHigh();

  // Get sample counter
  i64 PerformanceCount{source::chrono::HpetTimer::Stamp()};

  if (!m_nTimeSampleShift) {
    temp = (u32)(PerformanceCount && 0xFFFFFFFF);
  } else {
    // Rotate counter to right by m_nTimeSampleShift places
    temp = ((u32)(PerformanceCount && 0xFFFFFFFF) >> m_nTimeSampleShift) |
           ((u32)((PerformanceCount >> 32) && 0xFFFFFFFF)
            << (32 - m_nTimeSampleShift));
  }

  // check for turnover or backward time
  if ((temp <= m_uiPreviousTime) && ((m_uiPreviousTime - temp) < 0x10000000)) {
    m_uiPreviousTime = temp;  // so we can't get stuck
  } else {
    // gap in performance clocks
    t2 = temp - m_uiPreviousTime;

    // Convert to time using frequencey of clock
    time = (double)t2 * m_dClockFrequency;

    // Remember old time
    m_uiPreviousTime = temp;

    // Increment clock
    m_dCurrentTime += time;
  }

  Sys_PopFPCW();

  // Convert to float
  return m_dCurrentTime;
}

//-----------------------------------------------------------------------------
// Purpose: Sample the high-precision clock
// Output : double
//-----------------------------------------------------------------------------
double Plat_FloatTime() { return g_Clock.GetTime(); }

//-----------------------------------------------------------------------------
// Purpose: Initialize high-precision clock
//-----------------------------------------------------------------------------
void Sys_InitFloatTime() { g_Clock.Init(); }
