// Copyright � 1996-2018, Valve Corporation, All rights reserved. ====
//
// An entity that allows level designer control over the fog parameters.
//
//=============================================================================

#include "cbase.h"
#include "c_env_fog_controller.h"

 
#include "tier0/include/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( FogController, DT_FogController )

//-----------------------------------------------------------------------------
// Datatable
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CFogController, DT_FogController )
	// fog data
	RecvPropInt( RECVINFO( m_fog.enable ) ),
	RecvPropInt( RECVINFO( m_fog.blend ) ),
	RecvPropVector( RECVINFO( m_fog.dirPrimary ) ),
	RecvPropInt( RECVINFO( m_fog.colorPrimary ) ),
	RecvPropInt( RECVINFO( m_fog.colorSecondary ) ),
	RecvPropFloat( RECVINFO( m_fog.start ) ),
	RecvPropFloat( RECVINFO( m_fog.end ) ),
	RecvPropFloat( RECVINFO( m_fog.farz ) ),
	RecvPropFloat( RECVINFO( m_fog.maxdensity ) ),

	RecvPropInt( RECVINFO( m_fog.colorPrimaryLerpTo ) ),
	RecvPropInt( RECVINFO( m_fog.colorSecondaryLerpTo ) ),
	RecvPropFloat( RECVINFO( m_fog.startLerpTo ) ),
	RecvPropFloat( RECVINFO( m_fog.endLerpTo ) ),
	RecvPropFloat( RECVINFO( m_fog.lerptime ) ),
	RecvPropFloat( RECVINFO( m_fog.duration ) ),
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_FogController::C_FogController()
{
	// Make sure that old maps without fog fields don't get wacked out fog values.
	m_fog.enable = false;
	m_fog.maxdensity = 1.0f;
}
