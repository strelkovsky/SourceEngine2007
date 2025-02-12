// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//

#include "cbase.h"
#include "model_types.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "c_breakableprop.h"
#include "view.h"

 
#include "tier0/include/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_BreakableProp, DT_BreakableProp, CBreakableProp)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BreakableProp::C_BreakableProp( void )
{
	m_takedamage = DAMAGE_YES;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BreakableProp::SetFadeMinMax( float fademin, float fademax )
{
	m_fadeMinDist = fademin;
	m_fadeMaxDist = fademax;
}

//-----------------------------------------------------------------------------
// Copy fade from another breakable prop
//-----------------------------------------------------------------------------
void C_BreakableProp::CopyFadeFrom( C_BreakableProp *pSource )
{
	m_flFadeScale = pSource->m_flFadeScale;
}
