// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "CreateMultiplayerGameDialog.h"

#include <cstdio>
#include "CreateMultiplayerGameBotPage.h"
#include "CreateMultiplayerGameGameplayPage.h"
#include "CreateMultiplayerGameServerPage.h"
#include "EngineInterface.h"
#include "FileSystem.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "tier1/keyvalues.h"
#include "vgui/ILocalize.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

CCreateMultiplayerGameDialog::CCreateMultiplayerGameDialog(vgui::Panel *parent)
    : PropertyDialog(parent, "CreateMultiplayerGameDialog") {
  m_bBotsEnabled = false;
  SetDeleteSelfOnClose(true);
  SetSize(348, 460);

  SetTitle("#GameUI_CreateServer", true);
  SetOKButtonText("#GameUI_Start");

  if (!_stricmp(ModInfo().GetGameName(), "Counter-Strike Source")) {
    m_bBotsEnabled = true;
  }

  m_pServerPage = new CCreateMultiplayerGameServerPage(this, "ServerPage");
  m_pGameplayPage =
      new CCreateMultiplayerGameGameplayPage(this, "GameplayPage");
  m_pBotPage = NULL;

  AddPage(m_pServerPage, "#GameUI_Server");
  AddPage(m_pGameplayPage, "#GameUI_Game");

  // create KeyValues object to load/save config options
  m_pSavedData = new KeyValues("ServerConfig");

  // load the config data
  if (m_pSavedData) {
    m_pSavedData->LoadFromFile(g_pFullFileSystem, "ServerConfig.vdf",
                               "GAME");  // this is game-specific data, so it
                                         // should live in GAME, not CONFIG

    const char *startMap = m_pSavedData->GetString("map", "");
    if (startMap[0]) {
      m_pServerPage->SetMap(startMap);
    }
  }

  if (m_bBotsEnabled) {
    // add a page of advanced bot controls
    // NOTE: These controls will use the bot keys to initialize their values
    m_pBotPage =
        new CCreateMultiplayerGameBotPage(this, "BotPage", m_pSavedData);
    AddPage(m_pBotPage, "#GameUI_CPUPlayerOptions");
    m_pServerPage->EnableBots(m_pSavedData);
  }
}

CCreateMultiplayerGameDialog::~CCreateMultiplayerGameDialog() {
  if (m_pSavedData) {
    m_pSavedData->deleteThis();
    m_pSavedData = NULL;
  }
}

// Purpose: runs the server when the OK button is pressed
bool CCreateMultiplayerGameDialog::OnOK(bool applyOnly) {
  // reset server enforced cvars
  g_pCVar->RevertFlaggedConVars(FCVAR_REPLICATED);

  // Cheats were disabled; revert all cheat cvars to their default values.
  // This must be done heading into multiplayer games because people can play
  // demos etc and set cheat cvars with sv_cheats 0.
  g_pCVar->RevertFlaggedConVars(FCVAR_CHEAT);

  DevMsg("FCVAR_CHEAT cvars reverted to defaults.\n");

  BaseClass::OnOK(applyOnly);

  // get these values from m_pServerPage and store them temporarily
  char szMapName[64], szHostName[64], szPassword[64];
  strcpy_s(szMapName, m_pServerPage->GetMapName());
  strcpy_s(szHostName, m_pGameplayPage->GetHostName());
  strcpy_s(szPassword, m_pGameplayPage->GetPassword());

  // save the config data
  if (m_pSavedData) {
    if (m_pServerPage->IsRandomMapSelected()) {
      // it's set to random map, just save an
      m_pSavedData->SetString("map", "");
    } else {
      m_pSavedData->SetString("map", szMapName);
    }

    // save config to a file
    m_pSavedData->SaveToFile(g_pFullFileSystem, "ServerConfig.vdf", "GAME");
  }

  char szMapCommand[1024];

  // create the command to execute
  Q_snprintf(
      szMapCommand, sizeof(szMapCommand),
      "disconnect\nwait\nwait\nsv_lan 1\nsetmaster enable\nmaxplayers "
      "%i\nsv_password \"%s\"\nhostname \"%s\"\nprogress_enable\nmap %s\n",
      m_pGameplayPage->GetMaxPlayers(), szPassword, szHostName, szMapName);

  // exec
  engine->ClientCmd_Unrestricted(szMapCommand);

  return true;
}
