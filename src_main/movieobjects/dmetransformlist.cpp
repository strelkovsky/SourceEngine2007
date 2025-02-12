// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// An element that contains a transformlist
//
//=============================================================================
#include "movieobjects/dmetransformlist.h"
#include "datamodel/dmelementfactoryhelper.h"

 
#include "tier0/include/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTransformList, CDmeTransformList );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeTransformList::OnConstruction()
{
	m_Transforms.Init( this, "transforms" );
}

void CDmeTransformList::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the transform
//-----------------------------------------------------------------------------
void CDmeTransformList::SetTransform( int nIndex, const matrix3x4_t& mat )
{
	m_Transforms[nIndex]->SetTransform( mat );
}
