// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#ifndef AI_RESPONSESYSTEM_H
#define AI_RESPONSESYSTEM_H

#include "tier1/UtlVector.h"

#ifdef _WIN32
#pragma once
#endif

#include "AI_Criteria.h"

the_interface IResponseFilter
{
public:
	virtual bool IsValidResponse( ResponseType_t type, const char *pszValue ) = 0;
};

the_interface IResponseSystem
{
public:
	virtual ~IResponseSystem() {}

	virtual bool FindBestResponse( const AI_CriteriaSet& set, AI_Response& response, IResponseFilter *pFilter = NULL ) = 0;
	virtual void GetAllResponses( CUtlVector<AI_Response *> *pResponses ) = 0;
	virtual void PrecacheResponses( bool bEnable ) = 0;
};

IResponseSystem *PrecacheCustomResponseSystem( const char *scriptfile );
IResponseSystem *BuildCustomResponseSystemGivenCriteria( const char *pszBaseFile, const char *pszCustomName, AI_CriteriaSet &criteriaSet, float flCriteriaScore );
void DestroyCustomResponseSystems();

class ISaveRestoreBlockHandler *GetDefaultResponseSystemSaveRestoreBlockHandler();
class ISaveRestoreOps *GetResponseSystemSaveRestoreOps();

#endif // AI_RESPONSESYSTEM_H
