// Copyright � 2005, Valve Corporation, All rights reserved.

#ifndef PCH_MATERIALSYSTEM_H
#define PCH_MATERIALSYSTEM_H

#ifdef _WIN32
#include "base/include/windows/windows_light.h"
#endif

#include <malloc.h>
#include <cstring>
#include "crtmemdebug.h"

#include "tier0/include/dbg.h"
#include "tier0/include/fasttimer.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/platform.h"
#include "tier0/include/vprof.h"

#include "icvar.h"
#include "mathlib/vmatrix.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "tier1/generichash.h"
#include "tier1/strtools.h"
#include "tier1/tier1.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlstack.h"
#include "tier1/utlsymbol.h"

#include "bitmap/imageformat.h"
#include "bitmap/tgaloader.h"
#include "bitmap/tgawriter.h"
#include "datacache/idatacache.h"
#include "filesystem.h"
#include "pixelwriter.h"
#include "tier2/tier2.h"

#include "materialsystem/IColorCorrection.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem_global.h"

#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"

#endif  // PCH_MATERIALSYSTEM_H
