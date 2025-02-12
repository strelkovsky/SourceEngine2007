// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// All of our code is completely Unicode?

#ifndef SOURCE_TIER0_INCLUDE_WCHARTYPES_H_
#define SOURCE_TIER0_INCLUDE_WCHARTYPES_H_

// Temporarily turn off Valve defines.
#include "tier0/include/valve_off.h"

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
// This is a Unicode version of __FILE__.
#define SOURCE_WFILE__ WIDEN(__FILE__)

#ifdef STEAM
#ifndef _UNICODE
#define FORCED_UNICODE
#endif
#define _UNICODE
#endif

#ifdef FORCED_UNICODE
#undef _UNICODE
#endif

// Turn valve defines back on.
#include "tier0/include/valve_on.h"

#endif  // SOURCE_TIER0_INCLUDE_WCHARTYPES_H_
