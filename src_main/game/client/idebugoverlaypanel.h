// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $

#if !defined( IDEBUGOVERLAYPANEL_H )
#define IDEBUGOVERLAYPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

namespace vgui
{
	class Panel;
}

the_interface IDebugOverlayPanel
{
public:
	virtual void		Create( vgui::VPANEL parent ) = 0;
	virtual void		Destroy( void ) = 0;
};

extern IDebugOverlayPanel *debugoverlaypanel;

#endif // IDEBUGOVERLAYPANEL_H