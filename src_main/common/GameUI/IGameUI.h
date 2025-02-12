// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef IGAMEUI_H
#define IGAMEUI_H

#include "base/include/base_types.h"
#include "tier1/interface.h"
#include "vgui/IPanel.h"

#include "xbox/xboxstubs.h"

// reasons why the user can't connect to a game server
enum ESteamLoginFailure : u8 {
  STEAMLOGINFAILURE_NONE,
  STEAMLOGINFAILURE_BADTICKET,
  STEAMLOGINFAILURE_NOSTEAMLOGIN,
  STEAMLOGINFAILURE_VACBANNED,
  STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE
};

enum ESystemNotify {
  SYSTEMNOTIFY_STORAGEDEVICES_CHANGED,
  SYSTEMNOTIFY_USER_SIGNEDIN,
  SYSTEMNOTIFY_USER_SIGNEDOUT,
  SYSTEMNOTIFY_XUIOPENING,
  SYSTEMNOTIFY_XUICLOSED,
  // Cross-game invite is causing us to shutdown
  SYSTEMNOTIFY_INVITE_SHUTDOWN,
};

// Purpose: contains all the functions that the GameUI dll exports
the_interface IGameUI {
 public:
  // initialization/shutdown
  virtual void Initialize(CreateInterfaceFn appFactory) = 0;
  virtual void PostInit() = 0;

  // connect to other interfaces at the same level
  // (gameui.dll/server.dll/client.dll)
  virtual void Connect(CreateInterfaceFn gameFactory) = 0;

  virtual void Start() = 0;
  virtual void Shutdown() = 0;
  virtual void RunFrame() = 0;

  // notifications
  virtual void OnGameUIActivated() = 0;
  virtual void OnGameUIHidden() = 0;

  // OLD: Use OnConnectToServer2
  virtual void OLD_OnConnectToServer(const char *game, int IP, int port) = 0;

  virtual void OnDisconnectFromServer_OLD(ESteamLoginFailure eSteamLoginFailure,
                                          const char *username) = 0;
  virtual void OnLevelLoadingStarted(bool bShowProgressDialog) = 0;
  virtual void OnLevelLoadingFinished(bool bError, const char *failureReason,
                                      const char *extendedReason) = 0;

  // level loading progress, returns true if the screen needs updating
  virtual bool UpdateProgressBar(float progress, const char *statusText) = 0;
  // Shows progress desc, returns previous setting... (used with custom progress
  // bars )
  virtual bool SetShowProgressText(bool show) = 0;

  // !!!!!!!!!members added after "GameUI011" initial release!!!!!!!!!!!!!!!!!!!
  virtual void ShowNewGameDialog(int chapter) = 0;

  // Xbox 360
  virtual void SessionNotification(const int notification,
                                   const int param = 0) = 0;
  virtual void SystemNotification(const int notification) = 0;
  virtual void ShowMessageDialog(const uint32_t nType, vgui::Panel *pOwner) = 0;
  virtual void UpdatePlayerInfo(uint64_t nPlayerId, const char *pName,
                                int nTeam, uint8_t cVoiceState,
                                int nPlayersNeeded, bool bHost) = 0;
  virtual void SessionSearchResult(int searchIdx, void *pHostData,
                                   XSESSION_SEARCHRESULT *pResult,
                                   int ping) = 0;
  virtual void OnCreditsFinished(void) = 0;

  // inserts specified panel as background for level load dialog
  virtual void SetLoadingBackgroundDialog(vgui::VPANEL panel) = 0;

  // Bonus maps interfaces
  virtual void BonusMapUnlock(const char *pchFileName = NULL,
                              const char *pchMapName = NULL) = 0;
  virtual void BonusMapComplete(const char *pchFileName = NULL,
                                const char *pchMapName = NULL) = 0;
  virtual void BonusMapChallengeUpdate(
      const char *pchFileName, const char *pchMapName,
      const char *pchChallengeName, int iBest) = 0;
  virtual void BonusMapChallengeNames(char *pchFileName, size_t file_name_size,
                                      char *pchMapName, size_t map_name_size,
                                      char *pchChallengeName,
                                      size_t challenge_name_size) = 0;
  virtual void BonusMapChallengeObjectives(int &iBronze, int &iSilver,
                                           int &iGold) = 0;
  virtual void BonusMapDatabaseSave(void) = 0;
  virtual int BonusMapNumAdvancedCompleted(void) = 0;
  virtual void BonusMapNumMedals(int piNumMedals[3]) = 0;

  virtual void OnConnectToServer2(const char *game, int IP, int connectionPort,
                                  int queryPort) = 0;

  // X360 Storage device validation:
  //		returns true right away if storage device has been previously
  // selected. 		otherwise returns false and will set the variable
  // pointed by pStorageDeviceValidated to 1 	  once the storage device is
  // selected by user.
  virtual bool ValidateStorageDevice(int *pStorageDeviceValidated) = 0;

  virtual void SetProgressOnStart() = 0;
  virtual void OnDisconnectFromServer(
      ESteamLoginFailure eSteamLoginFailure) = 0;
};

#define GAMEUI_INTERFACE_VERSION "GameUI012"

#endif  // IGAMEUI_H
