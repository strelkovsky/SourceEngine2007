// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: event time stamping for hammer

#include "stdafx.h"

#include "hammer.h"
#include "mathlib/mathlib.h"
#include "tier0/include/platform.h"

#include "tier0/include/memdbgon.h"

static int g_EventTimeCounters[100];
static float g_EventTimes[100];

void SignalUpdate(int ev) {
  g_EventTimes[ev] = Plat_FloatTime();
  g_EventTimeCounters[ev]++;
}

int GetUpdateCounter(int ev) { return g_EventTimeCounters[ev]; }

float GetUpdateTime(int ev) { return g_EventTimes[ev]; }

void SignalGlobalUpdate() {
  float stamp = Plat_FloatTime();
  for (usize i = 0; i < std::size(g_EventTimes); i++) {
    g_EventTimes[i] = stamp;
    g_EventTimeCounters[i]++;
  }
}
