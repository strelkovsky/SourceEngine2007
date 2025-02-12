// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_COMPRESSED_LIGHT_CUBE_H_
#define SOURCE_MATHLIB_COMPRESSED_LIGHT_CUBE_H_

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "public/datamap.h"

struct CompressedLightCube {
  DECLARE_BYTESWAP_DATADESC();
  ColorRGBExp32 m_Color[6];
};

#endif  // SOURCE_MATHLIB_COMPRESSED_LIGHT_CUBE_H_
