// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "matchmakingqos.h"

#include "tier0/include/threadtools.h"

#include "tier0/include/memdbgon.h"

// Default implementation of QOS
static struct Default_MM_QOS_t : public MM_QOS_t {
  Default_MM_QOS_t() {
    nPingMsMin = 50;    // 50 ms default ping
    nPingMsMed = 50;    // 50 ms default ping
    flBwUpKbs = 32.0f;  // 32 KB/s = 256 kbps
    flBwDnKbs = 32.0f;  // 32 KB/s = 256 kbps
    flLoss = 0.0f;      // 0% packet loss
  }
} s_DefaultQos;

MM_QOS_t MM_GetQos() { return s_DefaultQos; }
