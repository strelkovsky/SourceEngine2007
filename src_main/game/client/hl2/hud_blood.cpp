// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#include "cbase.h"
#include "ClientEffectPrecacheSystem.h"
#include "c_te_effect_dispatch.h"
#include "hud.h"

 
#include "tier0/include/memdbgon.h"

void BloodSplatCallback( const CEffectData & data )
{
/*
	Msg("SPLAT!\n");

	int		x,y;

	// Find our screen position to start from
	x = XRES(320);
	y = YRES(240);

	// Draw the ammo label
	CHudTexture	*pSplat = gHUD.GetIcon( "hud_blood1" );
	
  // TODO(d.rattman):  This can only occur during vgui::Paint() stuff
	pSplat->DrawSelf( x, y, gHUD.m_clrNormal);
*/
}

DECLARE_CLIENT_EFFECT( "HudBloodSplat", BloodSplatCallback );
