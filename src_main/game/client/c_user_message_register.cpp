// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#include "cbase.h"
#include "c_user_message_register.h"

 
#include "tier0/include/memdbgon.h"

CUserMessageRegister *CUserMessageRegister::s_pHead = NULL;


CUserMessageRegister::CUserMessageRegister( const char *pMessageName, pfnUserMsgHook pHookFn )
{
	m_pMessageName = pMessageName;
	m_pHookFn = pHookFn;
	
	// Link it in.
	m_pNext = s_pHead;
	s_pHead = this;
}


void CUserMessageRegister::RegisterAll()
{
	for ( CUserMessageRegister *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		usermessages->HookMessage( pCur->m_pMessageName, pCur->m_pHookFn );
	}
}



