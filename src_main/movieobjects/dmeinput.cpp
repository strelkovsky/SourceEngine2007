// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeinput.h"
#include "datamodel/dmelementfactoryhelper.h"

 
#include "tier0/include/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_ELEMENT( DmeInput, CDmeInput );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeInput::OnConstruction()
{
}

void CDmeInput::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// IsDirty - ie needs to operate
//-----------------------------------------------------------------------------
bool CDmeInput::IsDirty()
{
	return true;
}
