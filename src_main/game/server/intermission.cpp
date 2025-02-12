// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $

#include "cbase.h"
#include "entitylist.h"

 
#include "tier0/include/memdbgon.h"

//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission:public CPointEntity
{
public:
	DECLARE_CLASS( CInfoIntermission, CPointEntity );

	void Spawn( void );
	void Think( void );
};

void CInfoIntermission::Spawn( void )
{
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	SetLocalAngles( vec3_angle );
	SetNextThink( gpGlobals->curtime + 2 );// let targets spawn !
}

void CInfoIntermission::Think ( void )
{
	CBaseEntity *pTarget;

	// find my target
	pTarget = gEntList.FindEntityByName( NULL, m_target );

	if ( pTarget )
	{
		Vector dir = pTarget->GetLocalOrigin() - GetLocalOrigin();
		VectorNormalize( dir );
		QAngle angles;
		VectorAngles( dir, angles );
		SetLocalAngles( angles );
	}
}

LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );
