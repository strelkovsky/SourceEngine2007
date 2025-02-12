// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef CBASE_H
#define CBASE_H

struct studiohdr_t;

#include <cstdio>
#include <cstdlib>

#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

#include "tier1/UtlVector.h"
#include "tier1/strtools.h"
#include "vstdlib/random.h"

#include "const.h"

#include "string_t.h"

// These two have to be included very early
#include "predictable_entity.h"
#include "predictableid.h"

#include "cdll_util.h"
#include "util_shared.h"

#include "baseentity_shared.h"
#include "icvar.h"

// This is a precompiled header.  Include a bunch of common stuff.
// This is kind of ugly in that it adds a bunch of dependency where it isn't
// needed. But on balance, the compile time is much lower (even incrementally)
// once the precompiled headers contain these headers.
#include "c_basecombatcharacter.h"
#include "c_basecombatweapon.h"
#include "c_baseplayer.h"
#include "c_recipientfilter.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "gamerules.h"
#include "itempents.h"
#include "physics.h"
#include "precache_register.h"
#include "vphysics_interface.h"
#include "worldsize.h"

#endif  // CBASE_H
