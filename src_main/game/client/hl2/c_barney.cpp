// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"
#include "c_ai_basenpc.h"

 
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_Barney : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_Barney, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

					C_Barney();
	virtual			~C_Barney();

private:
	C_Barney( const C_Barney & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_Barney, DT_NPC_Barney, CNPC_Barney)
END_RECV_TABLE()

C_Barney::C_Barney()
{
}


C_Barney::~C_Barney()
{
}


