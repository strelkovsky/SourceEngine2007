// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: steam state machine that handles authenticating steam users

#ifndef CL_STEAMUAUTH_H
#define CL_STEAMUAUTH_H

#include "baseclient.h"
#include "steam/steam_api.h"
#include "tier1/UtlVector.h"
#include "tier1/netadr.h"

class CSteam3Client {
 public:
  CSteam3Client();
  ~CSteam3Client();

  void Activate();
  void Shutdown();

  int InitiateConnection(void *pData, int cbMaxData, uint32_t unIP,
                         uint16_t usPort, uint64_t unGSSteamID, bool bSecure,
                         void *pvSteam2GetEncryptionKey,
                         int cbSteam2GetEncryptionKey);
  void TerminateConnection(uint32_t unIP, uint16_t usPort);
  bool BGSSecure() { return m_bGSSecure; }
  void RunFrame();
#if !defined(NO_STEAM)
  STEAM_CALLBACK(CSteam3Client, OnClientGameServerDeny, ClientGameServerDeny_t,
                 m_CallbackClientGameServerDeny);
  STEAM_CALLBACK(CSteam3Client, OnGameServerChangeRequested,
                 GameServerChangeRequested_t,
                 m_CallbackGameServerChangeRequested);
  STEAM_CALLBACK(CSteam3Client, OnGameOverlayActivated, GameOverlayActivated_t,
                 m_CallbackGameOverlayActivated);
  STEAM_CALLBACK(CSteam3Client, OnPersonaUpdated, PersonaStateChange_t,
                 m_CallbackPersonaStateChanged);
#endif

 private:
  bool m_bActive;
  bool m_bGSSecure;
};

// singleton accessor
CSteam3Client &Steam3Client();

#endif  // CL_STEAMUAUTH_H
