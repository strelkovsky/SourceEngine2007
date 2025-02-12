// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Client's CObjectSentrygun
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "CommanderOverlay.h"
#include "c_baseobject.h"
#include "vgui_healthbar.h"
#include "c_obj_respawn_station.h"
#include "C_BaseTFPlayer.h"

 
#include "tier0/include/memdbgon.h"


IMPLEMENT_CLIENTCLASS_DT(C_ObjectRespawnStation, DT_ObjectRespawnStation, CObjectRespawnStation)
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectRespawnStation::C_ObjectRespawnStation( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectRespawnStation::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "respawn_station", !bDormant );
}

