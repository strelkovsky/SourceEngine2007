// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include "iviewrender.h"
#include "toolframework_client.h"

 
#include "tier0/include/memdbgon.h"

// forward declarations
void ToolFramework_RecordMaterialParams( IMaterial *pMaterial );

// no inputs, assumes that the results go into $CHEAPWATERSTARTDISTANCE and $CHEAPWATERENDDISTANCE
class CWaterLODMaterialProxy : public IMaterialProxy
{
public:
	CWaterLODMaterialProxy();
	virtual ~CWaterLODMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pCheapWaterStartDistanceVar;
	IMaterialVar *m_pCheapWaterEndDistanceVar;
};

CWaterLODMaterialProxy::CWaterLODMaterialProxy()
{
	m_pCheapWaterStartDistanceVar = NULL;
	m_pCheapWaterEndDistanceVar = NULL;
}

CWaterLODMaterialProxy::~CWaterLODMaterialProxy()
{
}


bool CWaterLODMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_pCheapWaterStartDistanceVar = pMaterial->FindVar( "$CHEAPWATERSTARTDISTANCE", &foundVar, false );
	if( !foundVar )
		return false;

	m_pCheapWaterEndDistanceVar = pMaterial->FindVar( "$CHEAPWATERENDDISTANCE", &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

void CWaterLODMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if( !m_pCheapWaterStartDistanceVar || !m_pCheapWaterEndDistanceVar )
	{
		return;
	}
	float start, end;
	view->GetWaterLODParams( start, end );
	m_pCheapWaterStartDistanceVar->SetFloatValue( start );
	m_pCheapWaterEndDistanceVar->SetFloatValue( end );

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

IMaterial *CWaterLODMaterialProxy::GetMaterial()
{
	return m_pCheapWaterStartDistanceVar->GetOwningMaterial();
}

EXPOSE_INTERFACE( CWaterLODMaterialProxy, IMaterialProxy, "WaterLOD" IMATERIAL_PROXY_INTERFACE_VERSION );
