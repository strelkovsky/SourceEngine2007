// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VSTDLIB_VSTDLIB_H_
#define SOURCE_VSTDLIB_VSTDLIB_H_

#ifdef _WIN32
#pragma once
#endif

#include "tier0/include/platform.h"

#ifdef VSTDLIB_DLL_EXPORT
#define VSTDLIB_INTERFACE SOURCE_API_EXPORT
#define VSTDLIB_OVERLOAD SOURCE_API_GLOBAL_EXPORT
#define VSTDLIB_CLASS SOURCE_API_CLASS_EXPORT
#define VSTDLIB_GLOBAL SOURCE_API_GLOBAL_EXPORT
#else
#define VSTDLIB_INTERFACE SOURCE_API_IMPORT
#define VSTDLIB_OVERLOAD SOURCE_API_GLOBAL_IMPORT
#define VSTDLIB_CLASS SOURCE_API_CLASS_IMPORT
#define VSTDLIB_GLOBAL SOURCE_API_GLOBAL_IMPORT
#endif

#endif  // SOURCE_VSTDLIB_VSTDLIB_H_
