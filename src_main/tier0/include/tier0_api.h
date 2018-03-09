// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_TIER0_API_H_
#define SOURCE_TIER0_INCLUDE_TIER0_API_H_

#include "base/include/compiler_specific.h"

#ifndef TIER0_DLL_EXPORT
#define SOURCE_TIER0_API SOURCE_API_IMPORT
#define SOURCE_TIER0_API_GLOBAL SOURCE_API_GLOBAL_IMPORT
#define SOURCE_TIER0_API_CLASS SOURCE_API_CLASS_IMPORT
#else  // TIER0_DLL_EXPORT
#define SOURCE_TIER0_API SOURCE_API_EXPORT
#define SOURCE_TIER0_API_GLOBAL SOURCE_API_GLOBAL_EXPORT
#define SOURCE_TIER0_API_CLASS SOURCE_API_CLASS_EXPORT
#endif  // !TIER0_DLL_EXPORT

#endif  // !SOURCE_TIER0_INCLUDE_TIER0_API_H_
