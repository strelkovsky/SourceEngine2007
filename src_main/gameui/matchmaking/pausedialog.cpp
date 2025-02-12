// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Multiplayer pause menu
//


#include "pausedialog.h"
#include "gameui_interface.h"

 
#include "tier0/include/memdbgon.h"

//--------------------------------
// CPauseDialog
//--------------------------------
CPauseDialog::CPauseDialog( vgui::Panel *pParent ) : BaseClass( pParent, "PauseDialog" )
{
	// do nothing
} 

void CPauseDialog::Activate( void )
{
	BaseClass::Activate();

	SetDeleteSelfOnClose( false );
	m_Menu.SetFocus( 0 );
}

//-------------------------------------------------------
// Keyboard input
//-------------------------------------------------------
void CPauseDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_XBUTTON_B:
		if ( GameUI().IsInLevel() )
		{
			m_pParent->OnCommand( "ResumeGame" );
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}