//====== Copyright � 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: VGUI panel which can play back video, in-engine
//
//=============================================================================

#include "cbase.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include "tier1/keyvalues.h"
#include "vgui_video.h"
#include "tf_vgui_video.h"
#include "engine/ienginesound.h"


 
#include "tier0/include/memdbgon.h"


DECLARE_BUILD_FACTORY( CTFVideoPanel );

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CTFVideoPanel::CTFVideoPanel( vgui::Panel *parent, const char *panelName ) : VideoPanel( 0, 0, 50, 50 )
{
	SetParent( parent );
	SetProportional( true );
	SetKeyBoardInputEnabled( false );

	SetBlackBackground( false );

	m_flStartAnimDelay = 0.0f;
	m_flEndAnimDelay = 0.0f;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CTFVideoPanel::~CTFVideoPanel()
{
	ReleaseVideo();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTFVideoPanel::ReleaseVideo()
{
	enginesound->NotifyEndMoviePlayback();

	// Destroy any previously allocated video
	if ( m_BIKHandle != BIKHANDLE_INVALID )
	{
		bik->DestroyMaterial( m_BIKHandle );
		m_BIKHandle = BIKHANDLE_INVALID;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTFVideoPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	SetExitCommand( inResourceData->GetString( "command", "" ) );
	m_flStartAnimDelay = inResourceData->GetFloat( "start_delay", 0.0 );
	m_flEndAnimDelay = inResourceData->GetFloat( "end_delay", 0.0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFVideoPanel::GetPanelPos( int &xpos, int &ypos )
{
	vgui::ipanel()->GetAbsPos( GetVPanel(), xpos, ypos );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFVideoPanel::OnVideoOver()
{
	BaseClass::OnVideoOver();
	PostMessage( GetParent(), new KeyValues( "IntroFinished" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFVideoPanel::OnClose()
{
	// Fire an exit command if we're asked to do so
	if ( m_szExitCommand[0] )
	{
		engine->ClientCmd( m_szExitCommand );
	}

	// intentionally skipping VideoPanel::OnClose()
	EditablePanel::OnClose();

	SetVisible( false );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFVideoPanel::Shutdown()
{
	OnClose();
	ReleaseVideo();
}