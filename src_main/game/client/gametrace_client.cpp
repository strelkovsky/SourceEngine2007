// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#include "cbase.h"
#include "gametrace.h"

 
#include "tier0/include/memdbgon.h"

bool CGameTrace::DidHitWorld() const
{
	return m_pEnt == ClientEntityList().GetBaseEntity( 0 );
}


bool CGameTrace::DidHitNonWorldEntity() const
{
	return m_pEnt != NULL && !DidHitWorld();
}


int CGameTrace::GetEntityIndex() const
{
	if ( m_pEnt )
		return m_pEnt->entindex();
	else
		return -1;
}

