// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "server_pch.h"

#include "FindSteamServers.h"
#include "filesystem_engine.h"
#include "hltvserver.h"
#include "master.h"
#include "proto_oob.h"
#include "server.h"
#include "sv_main.h"  // SV_GetFakeClientCount()
#include "sv_master_legacy.h"
#include "sv_steamauth.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"

#include "tier0/include/memdbgon.h"

bool g_bEnableMasterServerUpdater = true;

void SetMaster_f(const CCommand &args) {
  if (IsUsingMasterLegacyMode()) {
    master->SetMaster_Legacy_f(args);
    return;
  }
#ifndef NO_STEAM
  ISteamMasterServerUpdater *s = SteamMasterServerUpdater();
  if (!s) return;

  char szMasterAddress[128];  // IP:Port string of master
  const char *pszCmd = NULL;

  int count = args.ArgC();

  // Usage help
  if (count < 2) {
    ConMsg("Usage:\nsetmaster <add | remove | enable | disable> <IP:port>\n");

    if (s->GetNumMasterServers() == 0) {
      ConMsg("Current:  None\n");
    } else {
      ConMsg("Current:\n");

      for (int i = 0; i < s->GetNumMasterServers(); i++) {
        char szAdr[512];
        if (s->GetMasterServerAddress(i, szAdr, sizeof(szAdr)) != 0)
          ConMsg("  %i:  %s\n", i + 1, szAdr);
      }
    }

    return;
  }

  pszCmd = args[1];
  if (!pszCmd || !pszCmd[0]) return;

  // Check for disabling...
  if (!Q_stricmp(pszCmd, "disable")) {
    g_bEnableMasterServerUpdater = false;
  } else if (!Q_stricmp(pszCmd, "enable")) {
    g_bEnableMasterServerUpdater = true;
  } else if (!Q_stricmp(pszCmd, "add") || !Q_stricmp(pszCmd, "remove")) {
    // Figure out what they want to do.
    // build master address
    szMasterAddress[0] = 0;

    for (int i = 2; i < count; i++) {
      Q_strcat(szMasterAddress, args[i], sizeof(szMasterAddress));
    }

    if (!Q_stricmp(pszCmd, "add")) {
      if (s->AddMasterServer(szMasterAddress)) {
        ConMsg("Adding master at %s\n", szMasterAddress);
      } else {
        ConMsg("Master at %s already in list\n", szMasterAddress);
      }

      // If we get here, masters are definitely being used.
      g_bEnableMasterServerUpdater = true;
    } else {
      // Find master server
      if (!s->RemoveMasterServer(szMasterAddress)) {
        ConMsg("Can't remove master %s, not in list\n", szMasterAddress);
      }
    }
  } else {
    ConMsg("Invalid setmaster command\n");
  }

  // Resend the rules just in case we added a new server.
  sv.SetMasterServerRulesDirty();
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Heartbeat_f() {
  if (IsUsingMasterLegacyMode()) {
    master->Heartbeat_Legacy_f();
    return;
  }
#ifndef NO_STEAM
  SteamMasterServerUpdater()->ForceHeartbeat();
#endif
}

static ConCommand setmaster("setmaster", SetMaster_f,
                            "add/remove/enable/disable master servers", 0);
static ConCommand heartbeat("heartbeat", Heartbeat_f,
                            "Force heartbeat of master servers", 0);
