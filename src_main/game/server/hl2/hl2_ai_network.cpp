// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:
//



#include "cbase.h"

#include "ai_networkmanager.h"
#include "npc_strider.h"

 
#include "tier0/include/memdbgon.h"

class CHL2NetworkBuildHelper : public CAI_NetworkBuildHelper
{
	DECLARE_CLASS( CHL2NetworkBuildHelper, CAI_NetworkBuildHelper );

	void PostInitNodePosition( CAI_Network *pNetwork, CAI_Node *pNode )
	{
		AdjustStriderNodePosition( pNetwork, pNode );
	}
};

LINK_ENTITY_TO_CLASS(ai_network_build_helper,CHL2NetworkBuildHelper);
