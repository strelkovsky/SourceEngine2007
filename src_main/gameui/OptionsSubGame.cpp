// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#include "OptionsSubGame.h"
#include "BasePanel.h"

 
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubGame::COptionsSubGame( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	SetDeleteSelfOnClose( true );
	LoadControlSettings( "Resource/OptionsSubGame.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubGame::~COptionsSubGame()
{
}

void COptionsSubGame::OnClose( void )
{
	BasePanel()->RunCloseAnimation( "CloseOptionsSubGame" );
	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubGame::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}
