// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//

#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"

 
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		tempents->EjectBrass( data.m_vOrigin, data.m_vAngles, pRenderable->GetRenderAngles(), 0 );
	}
}

DECLARE_CLIENT_EFFECT( "ShellEject", ShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RifleShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		tempents->EjectBrass( data.m_vOrigin, data.m_vAngles, pRenderable->GetRenderAngles(), 1 );
	}
}

DECLARE_CLIENT_EFFECT( "RifleShellEject", RifleShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShotgunShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		tempents->EjectBrass( data.m_vOrigin, data.m_vAngles, pRenderable->GetRenderAngles(), 2 );
	}
}

DECLARE_CLIENT_EFFECT( "ShotgunShellEject", ShotgunShellEjectCallback );


