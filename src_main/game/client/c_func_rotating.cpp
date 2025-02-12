// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "tier1/keyvalues.h"

 
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_FuncRotating : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncRotating, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_FuncRotating();

private:
};

extern void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

IMPLEMENT_CLIENTCLASS_DT( C_FuncRotating, DT_FuncRotating, CFuncRotating )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropQAngles( RECVINFO_NAME( m_angNetworkAngles, m_angRotation ) ),
	RecvPropInt( RECVINFO(m_flSimulationTime), 0, RecvProxy_SimulationTime ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_FuncRotating::C_FuncRotating()
{
}
