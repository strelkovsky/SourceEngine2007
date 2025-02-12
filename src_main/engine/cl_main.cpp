﻿// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "client_pch.h"

#include "cl_main.h"

#include "GameEventManager.h"
#include "LoadScreenUpdate.h"
#include "LocalNetworkBackdoor.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "SteamInterface.h"
#include "avi/iavi.h"
#include "cbenchmark.h"
#include "cdll_engine_int.h"
#include "checksum_engine.h"
#include "cl_demo.h"
#include "cl_rcon.h"
#include "cl_steamauth.h"
#include "con_nprint.h"
#include "console.h"
#include "debugoverlay.h"
#include "decal.h"
#include "dt_recv_eng.h"
#include "enginethreads.h"
#include "filesystem/IQueuedLoader.h"
#include "filesystem_engine.h"
#include "gl_lightmap.h"
#include "gl_matsysiface.h"
#include "host_phonehome.h"
#include "host_saverestore.h"
#include "host_state.h"
#include "icliententity.h"
#include "inetchannel.h"
#include "iregistry.h"
#include "ispatialpartitioninternal.h"
#include "ivideomode.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "proto_oob.h"
#include "pure_server.h"
#include "r_local.h"
#include "snd_audio_source.h"
#include "sound.h"
#include "staticpropmgr.h"
#include "steam/steam_api.h"
#include "sv_rcon.h"
#include "sys.h"
#include "sys_dll.h"
#include "testscriptmgr.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"
#include "tier0/include/vprof.h"
#include "tier2/tier2.h"
#include "traceinit.h"
#include "vgui/ISystem.h"
#include "vgui_baseui_interface.h"
#include "vox.h"
#include "vstdlib/random.h"

#include "tier0/include/memdbgon.h"

extern IVEngineClient *engineClient;

void R_UnloadSkys();
void CL_ResetEntityBits();
void EngineTool_UpdateScreenshot();
void WriteConfig_f(ConVar *var, const ch *pOldString);

// If we get more than 250 messages in the incoming buffer queue, dump any above
// this #
#define MAX_INCOMING_MESSAGES 250
// Size of command send buffer
#define MAX_CMD_BUFFER 4000

CGlobalVarsBase g_ClientGlobalVariables(true);
AVIHandle_t g_hCurrentAVI = AVIHANDLE_INVALID;

extern ConVar rcon_password;
extern ConVar host_framerate;

ConVar sv_unlockedchapters("sv_unlockedchapters", "1",
                           FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX,
                           "Highest unlocked game chapter.");

static ConVar tv_nochat(
    "tv_nochat", "0", FCVAR_ARCHIVE | FCVAR_USERINFO,
    "Don't receive chat messages from other SourceTV spectators");
static ConVar cl_LocalNetworkBackdoor(
    "cl_localnetworkbackdoor", "1", 0,
    "Enable network optimizations for single player games.");
static ConVar cl_ignorepackets(
    "cl_ignorepackets", "0", FCVAR_CHEAT,
    "Force client to ignore packets (for debugging).");
static ConVar cl_playback_screenshots(
    "cl_playback_screenshots", "0", 0,
    "Allows the client to playback screenshot and jpeg commands in demos.");

MovieInfo_t cl_movieinfo;

// TODO(d.rattman): put these on hunk?
dlight_t cl_dlights[MAX_DLIGHTS];
dlight_t cl_elights[MAX_ELIGHTS];
CFastPointLeafNum g_DLightLeafAccessors[MAX_DLIGHTS];
CFastPointLeafNum g_ELightLeafAccessors[MAX_ELIGHTS];

bool cl_takesnapshot = false;
static bool cl_takejpeg = false;

static int cl_jpegquality = DEFAULT_JPEG_QUALITY;
static ConVar jpeg_quality("jpeg_quality", "98", 0, "jpeg screenshot quality.");

static int cl_snapshotnum = 0;
static ch cl_snapshotname[MAX_OSPATH];
static ch cl_snapshot_subdirname[MAX_OSPATH];

// Must match game .dll definition
// HACK HACK FOR E3 -- Remove this after E3
#define HIDEHUD_ALL (1 << 2)

// This is called when a client receives the whitelist from a pure server (on
// map change). Each pure server (and each map on the server) has a whitelist
// that says which files a client is allowed to load off disk. When the client
// gets the whitelist, it must flush out any files that it has loaded previously
// that were NOT in the Steam cache.
//
// -- pseudocode --
// for all loaded resources (models/sounds/materials/scripts)
//	 for each file related to this resource
//     if (file is not in whitelist)
//       if (file was loaded off disk instead of coming from the Steam cache)
//         flush the file
//
// Note: It could also check in here that the on-disk file is actually different
//       than the Steam one. If it happens to have the same CRC, then there's no
//       need to do all the flushing.
//
void CL_HandlePureServerWhitelist(CPureServerWhitelist *pWhitelist) {
  // Free the old whitelist and get the new one.
  if (cl.m_pPureServerWhitelist) cl.m_pPureServerWhitelist->Release();

  cl.m_pPureServerWhitelist = pWhitelist;

  IFileList *pForceMatchList = nullptr, *pAllowFromDiskList = nullptr;

  if (pWhitelist) {
    pForceMatchList = pWhitelist->GetForceMatchList();
    pAllowFromDiskList = pWhitelist->GetAllowFromDiskList();
  }

  // First, hand the whitelist to the filesystem. Now it will know which files
  // we want it to load from the Steam caches BEFORE files on disk.
  //
  // Note: The filesystem now owns the pointer. It will delete it when it shuts
  // down or next time we call this.
  IFileList *pFilesToReload;
  g_pFileSystem->RegisterFileWhitelist(pForceMatchList, pAllowFromDiskList,
                                       &pFilesToReload);

  if (pFilesToReload) {
  // Handle sounds..
#if 0  // There are problems with the soundemittersystem + sv_pure currently.
		if ( g_pSoundEmitterSystem )
			g_pSoundEmitterSystem->ReloadFilesInList( pFilesToReload );
		else
			Warning( "CL_HandlePureServerWhitelist: No sound emitter system.\n" );
#endif

#if 0  // Still under testing for release in OB engine.
		S_ReloadFilesInList( pFilesToReload );
#endif

    // Handle materials..
    materials->ReloadFilesInList(pFilesToReload);

#if 0  // Still under testing for release in OB engine.
       // Handle models.. NOTE: this MUST come after
       // materials->ReloadFilesInList because the models need to know which
       // materials got flushed.
		modelloader->ReloadFilesInList( pFilesToReload );
#endif

    pFilesToReload->Release();
  }

  // Now that we've flushed any files that shouldn't have been on disk, we
  // should have a CRC set that we can check with the server.
  cl.m_bCheckCRCsWithServer = pForceMatchList && pAllowFromDiskList;
}

void CL_PrintWhitelistInfo() {
  if (cl.m_pPureServerWhitelist) {
    if (cl.m_pPureServerWhitelist->IsInFullyPureMode()) {
      Msg("The server is using sv_pure = 2.\n");
    } else {
      Msg("The server is using sv_pure = 1.\n");
      cl.m_pPureServerWhitelist->PrintWhitelistContents();
    }
  } else {
    Msg("The server is using sv_pure = 0 (no whitelist).\n");
  }
}

// Console command to force a whitelist on the system.
#ifdef _DEBUG
void whitelist_f(const CCommand &args) {
  int pureLevel = 2;
  if (args.ArgC() == 2) {
    pureLevel = atoi(args[1]);
  } else {
    Warning("Whitelist 0, 1, or 2\n");
  }

  if (pureLevel == 0) {
    Warning("whitelist 0: CL_HandlePureServerWhitelist( nullptr )\n");
    CL_HandlePureServerWhitelist(nullptr);
  } else {
    CPureServerWhitelist *pWhitelist =
        CPureServerWhitelist::Create(g_pFileSystem);
    if (pureLevel == 2) {
      Warning("whitelist 2: pWhitelist->EnableFullyPureMode()\n");
      pWhitelist->EnableFullyPureMode();
    } else {
      Warning("whitelist 1: loading pure_server_whitelist.txt\n");
      KeyValues *kv = new KeyValues("");
      bool bLoaded =
          kv->LoadFromFile(g_pFileSystem, "pure_server_whitelist.txt", "game");
      if (bLoaded) bLoaded = pWhitelist->LoadFromKeyValues(kv);

      if (!bLoaded) Warning("Error loading pure_server_whitelist.txt\n");

      kv->deleteThis();
    }

    CL_HandlePureServerWhitelist(pWhitelist);
    pWhitelist->Release();
  }
}
ConCommand whitelist("whitelist", whitelist_f);
#endif

const CPrecacheUserData *CL_GetPrecacheUserData(INetworkStringTable *table,
                                                int index) {
  int user_data_length;
  const auto *data = static_cast<const CPrecacheUserData *>(
      table->GetStringUserData(index, &user_data_length));

  if (data) {
    ErrorIfNot(user_data_length == sizeof(*data),
               ("CL_GetPrecacheUserData(%d,%d) - length (%d) invalid.",
                table->GetTableId(), index, user_data_length));
  }

  return data;
}

// Purpose: setup the demo flag, split from CL_IsHL2Demo so CL_IsHL2Demo can be
// inline
static bool s_bIsHL2Demo = false;
void CL_InitHL2DemoFlag() {
#ifndef NO_STEAM
  static bool initialized = false;
  if (!initialized) {
    if (SteamApps() && !Q_stricmp(COM_GetModDirectory(), "hl2") &&
        g_pFileSystem->IsSteam()) {
      initialized = true;
      int nRet = 0;
      ch szSubscribedValue[10];

      if (VCRGetMode() != VCR_Playback)
        nRet = SteamApps()->GetAppData(220, "subscribed", szSubscribedValue,
                                       sizeof(szSubscribedValue));
#if !defined(NO_VCR)
      VCRGenericValue("e", &nRet, sizeof(nRet));
#endif
      if (nRet && Q_atoi(szSubscribedValue) ==
                      0)  // if they don't own HL2 this must be the demo!
      {
        s_bIsHL2Demo = true;
      }
    }

    if (!Q_stricmp(COM_GetModDirectory(), "hl2") &&
        CommandLine()->CheckParm("-demo")) {
      s_bIsHL2Demo = true;
    }
  }
#endif
}

// Purpose: Returns true if the user is playing the HL2 Demo (rather than the
// full game)
bool CL_IsHL2Demo() {
  CL_InitHL2DemoFlag();
  return s_bIsHL2Demo;
}

static bool s_bIsPortalDemo = false;
void CL_InitPortalDemoFlag() {
#ifndef NO_STEAM
  static bool initialized = false;
  if (!initialized) {
    if (SteamApps() && !Q_stricmp(COM_GetModDirectory(), "portal") &&
        g_pFileSystem->IsSteam()) {
      initialized = true;
      int nRet = 0;
      ch szSubscribedValue[10];

      if (VCRGetMode() != VCR_Playback)
        nRet = SteamApps()->GetAppData(400, "subscribed", szSubscribedValue,
                                       sizeof(szSubscribedValue));
#if !defined(NO_VCR)
      VCRGenericValue("e", &nRet, sizeof(nRet));
#endif
      if (nRet && Q_atoi(szSubscribedValue) ==
                      0)  // if they don't own HL2 this must be the demo!
      {
        s_bIsPortalDemo = true;
      }
    }

    if (!Q_stricmp(COM_GetModDirectory(), "portal") &&
        CommandLine()->CheckParm("-demo")) {
      s_bIsPortalDemo = true;
    }
  }
#else
  static bool initialized = true;
#endif
}

// Purpose: Returns true if the user is playing the Portal Demo (rather than the
// full game)
bool CL_IsPortalDemo() {
  CL_InitPortalDemoFlag();
  return s_bIsPortalDemo;
}

// Purpose: If the client is in the process of connecting and the cl.signon hits
// is complete, make sure the client thinks its totally connected.
void CL_CheckClientState() {
  // Setup the local network backdoor (we do this each frame so it can be
  // toggled on and off).
  bool useBackdoor =
      cl_LocalNetworkBackdoor.GetInt() &&
      (cl.m_NetChannel ? cl.m_NetChannel->IsLoopback() : false) &&
      sv.IsActive() && !demorecorder->IsRecording() &&
      !demoplayer->IsPlayingBack() && Host_IsSinglePlayerGame();

  CL_SetupLocalNetworkBackDoor(useBackdoor);
}

bool CL_CheckCRCs(const ch *pszMap) {
  // Don't verify CRC if we are running a local server (i.e., we are playing
  // single player, or we are the server in multiplay
  if (sv.IsActive()) return true;

  CRC32_t mapCRC;  // If this is the worldmap, CRC agains server's map
  CRC32_Init(&mapCRC);
  if (!CRC_MapFile(&mapCRC, pszMap)) {
    // Does the file exist?
    FileHandle_t fp = 0;
    int nSize = COM_OpenFile(pszMap, &fp);
    if (fp) g_pFileSystem->Close(fp);

    if (nSize != -1) {
      COM_ExplainDisconnection(true, "Couldn't CRC map %s, disconnecting\n",
                               pszMap);
    } else {
      COM_ExplainDisconnection(true, "Missing map %s,  disconnecting\n",
                               pszMap);
    }

    Host_Error("Disconnected");
    return false;
  }

  // Hacked map
  if (cl.serverCRC != mapCRC && !demoplayer->IsPlayingBack()) {
    COM_ExplainDisconnection(true, "Your map [%s] differs from the server's.\n",
                             pszMap);
    Host_Error("Disconnected");
    return false;
  }

  // Check to see that our copy of the client side dll matches the server's.
  // Client side DLL  CRC check.
  ch client_dll_name[MAX_QPATH];  // Client side DLL being used.
  sprintf_s(client_dll_name, "bin\\client.dll");

  CRC32_t clientDllCRC;
  if (!CRC_File(&clientDllCRC, client_dll_name) &&
      !demoplayer->IsPlayingBack()) {
    COM_ExplainDisconnection(true, "Couldn't CRC client side dll %s.\n",
                             client_dll_name);
    Host_Error("Disconnected");
    return false;
  }

#ifdef NDEBUG
  // These must match.
  // Except during demo playback.  For that just put a warning.
  if (cl.serverClientSideDllCRC != 0xFFFFFFFF &&
      cl.serverClientSideDllCRC != clientDllCRC) {
    if (!demoplayer->IsPlayingBack()) {
      // TODO: allow Valve mods to differ!!
      Warning("Your .dll [%s] differs from the server's.\n", client_dll_name);
      // COM_ExplainDisconnection( true, "Your .dll [%s] differs from the
      // server's.\n", szDllName);  Host_Error( "Disconnected" );  return false;
    }
  }
#endif

  return true;
}

void CL_ReallocateDynamicData(int maxclients) {
  Assert(entitylist);
  if (entitylist) {
    entitylist->SetMaxEntities(MAX_EDICTS);
  }
}

/*
Updates the local time and reads/handles messages on client net connection.
*/
void CL_ReadPackets(bool bFinalTick) {
  VPROF_BUDGET("CL_ReadPackets", VPROF_BUDGETGROUP_OTHER_NETWORKING);

  if (!Host_ShouldRun()) return;

  // update client times/tick

  cl.oldtickcount = cl.GetServerTickCount();
  if (!cl.IsPaused()) {
    cl.SetClientTickCount(cl.GetClientTickCount() + 1);

    // While clock correction is off, we have the old behavior of matching the
    // client and server clocks.
    if (!CClockDriftMgr::IsClockCorrectionEnabled())
      cl.SetServerTickCount(cl.GetClientTickCount());

    g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();
    g_ClientGlobalVariables.curtime = cl.GetTime();
  }
  // 0 or tick_rate if simulating
  g_ClientGlobalVariables.frametime = cl.GetFrameTime();

  // read packets, if any in queue
  if (demoplayer->IsPlayingBack() && cl.m_NetChannel) {
    // process data from demo file
    cl.m_NetChannel->ProcessPlayback();
  } else {
    if (!cl_ignorepackets.GetInt()) {
      // process data from net socket
      NET_ProcessSocket(NS_CLIENT, &cl);
    }
  }

    // check timeout, but not if running _DEBUG engine
#ifdef NDEBUG
  // Only check on final frame because that's when the server might send us a
  // packet in single player.  This avoids
  //  a bug where if you sit in the game code in the debugger then you get a
  //  timeout here on resuming the engine because the timestep is > 1 tick
  //  because of the debugging delay but the server hasn't sent the next packet
  //  yet.  ywb 9/5/03
  if ((cl.m_NetChannel ? cl.m_NetChannel->IsTimedOut() : false) && bFinalTick &&
      !demoplayer->IsPlayingBack() && cl.IsConnected()) {
    ConMsg("\nServer connection timed out.\n");

    // Show the vgui dialog on timeout
    COM_ExplainDisconnection(false, "Lost connection to server.");
    EngineVGui()->ShowErrorMessage();

    Host_Disconnect(true);
  }
#endif
}

void CL_ClearState() {
  CL_ResetEntityBits();

  R_UnloadSkys();

  // clear decal index directories
  Decal_Init();

  StaticPropMgr()->LevelShutdownClient();

  // shutdown this level in the client DLL
  if (g_ClientDLL) {
    if (host_state.worldmodel) {
      ch mapname[256];
      CL_SetupMapName(modelloader->GetName(host_state.worldmodel), mapname,
                      sizeof(mapname));
    }
    audiosourcecache->LevelShutdown();
    g_ClientDLL->LevelShutdown();
  }

  R_LevelShutdown();

  if (g_pLocalNetworkBackdoor) g_pLocalNetworkBackdoor->ClearState();

  // clear other arrays
  memset(cl_dlights, 0, sizeof(cl_dlights));
  memset(cl_elights, 0, sizeof(cl_elights));

  // Wipe the hunk ( unless the server is active )
  Host_FreeStateAndWorld(false);
  Host_FreeToLowMark(false);

  // Wipe the remainder of the structure.
  cl.Clear();
}

// Purpose: Used for sorting sounds
static bool CL_SoundMessageLessFunc(SoundInfo_t const &sound1,
                                    SoundInfo_t const &sound2) {
  return sound1.nSequenceNumber < sound2.nSequenceNumber;
}

static CUtlRBTree<SoundInfo_t, int> g_SoundMessages(0, 0,
                                                    CL_SoundMessageLessFunc);
extern ConVar snd_show;

// Purpose: Add sound to queue
void CL_AddSound(const SoundInfo_t &sound) { g_SoundMessages.Insert(sound); }

// Purpose: Play sound packet
void CL_DispatchSound(const SoundInfo_t &sound) {
  CSfxTable *pSfx;

  ch name[MAX_QPATH];
  name[0] = 0;

  if (sound.bIsSentence) {
    // make dummy sfx for sentences
    const ch *pSentenceName = VOX_SentenceNameFromIndex(sound.nSoundNum);
    if (!pSentenceName) pSentenceName = "";

    sprintf_s(name, "%c%s", CHAR_SENTENCE, pSentenceName);
    pSfx = S_DummySfx(name);
  } else {
    pSfx = cl.GetSound(sound.nSoundNum);
    strcpy_s(name, cl.GetSoundName(sound.nSoundNum));
  }

  if (snd_show.GetInt() >= 2) {
    DevMsg(
        "%i (seq %i) %s : src %d : ch %d : %d dB : vol %.2f : time %.3f (%.4f "
        "delay) @%.1f %.1f %.1f\n",
        host_framecount, sound.nSequenceNumber, name, sound.nEntityIndex,
        sound.nChannel, sound.Soundlevel, sound.fVolume, cl.GetTime(),
        sound.fDelay, sound.vOrigin.x, sound.vOrigin.y, sound.vOrigin.z);
  }

  StartSoundParams_t params;
  params.staticsound = sound.nChannel == CHAN_STATIC;
  params.soundsource = sound.nEntityIndex;
  params.entchannel = params.staticsound ? CHAN_STATIC : sound.nChannel;
  params.pSfx = pSfx;
  params.origin = sound.vOrigin;
  params.fvol = sound.fVolume;
  params.soundlevel = sound.Soundlevel;
  params.flags = sound.nFlags;
  params.pitch = sound.nPitch;
  params.fromserver = true;
  params.delay = sound.fDelay;

  // we always want to do this when this flag is set - even if the delay is zero
  // we need to precisely schedule this sound
  if (sound.nFlags & SND_DELAY) {
    // anything adjusted less than 100ms forward was probably scheduled this
    // frame
    if (sound.fDelay > -0.100f) {
      float soundtime = cl.m_flLastServerTickTime + sound.fDelay;
      // this adjusts for host_thread_mode or any other cases where we're
      // running more than one tick at a time, but we get network updates on the
      // first tick
      soundtime -= ((g_ClientGlobalVariables.simTicksThisFrame - 1) *
                    host_state.interval_per_tick);
      // this sound was networked over from the server, use server clock
      params.delay = S_ComputeDelayForSoundtime(soundtime, CLOCK_SYNC_SERVER);
      if (params.delay < 0) {
        params.delay = 0;
      }
    } else {
      params.delay = sound.fDelay;
    }
  }
  params.speakerentity = sound.nSpeakerEntity;

  if (params.staticsound) {
    S_StartSound(params);
  } else {
    // Don't actually play non-static sounds if playing a demo and skipping
    // ahead but always stop sounds
    if (demoplayer->IsSkipping() && !(sound.nFlags & SND_STOP)) {
      return;
    }
    S_StartSound(params);
  }
}

// Purpose: Called after reading network messages to play sounds encoded in the
// network packet
void CL_DispatchSounds() {
  // Walk list in sequence order
  int i = g_SoundMessages.FirstInorder();
  while (i != g_SoundMessages.InvalidIndex()) {
    SoundInfo_t const *msg = &g_SoundMessages[i];
    Assert(msg);
    if (msg) {
      // Play the sound
      CL_DispatchSound(*msg);
    }
    i = g_SoundMessages.NextInorder(i);
  }

  // Reset the queue each time we empty it!!!
  g_SoundMessages.RemoveAll();
}

// Retry last connection (e.g., after we enter a password)
void CL_Retry() {
  if (!cl.m_szRetryAddress[0]) {
    ConMsg("Can't retry, no previous connection\n");
    return;
  }

  ConMsg("Commencing connection retry to %s\n", cl.m_szRetryAddress);
  Cbuf_AddText(va("connect %s\n", cl.m_szRetryAddress));
}

CON_COMMAND_F(retry, "Retry connection to last server.",
              FCVAR_DONTRECORD | FCVAR_SERVER_CAN_EXECUTE |
                  FCVAR_CLIENTCMD_CAN_EXECUTE) {
  CL_Retry();
}

/*
User command to connect to server
*/
CON_COMMAND_F(connect, "Connect to specified server.", FCVAR_DONTRECORD) {
  if (args.ArgC() < 2) {
    ConMsg("Usage:  connect <server>\n");
    return;
  }

  const ch *address = args.ArgS();

  // If it's not a single player connection to "localhost", initialize
  // networking & stop listenserver
  if (strncmp(address, "localhost", 9)) {
    Host_Disconnect(false);

    // allow remote
    NET_SetMutiplayer(true);

    // start progress bar immediately for remote connection
    EngineVGui()->EnabledProgressBarForNextLoad();

    SCR_BeginLoadingPlaque();

    EngineVGui()->UpdateProgressBar(PROGRESS_BEGINCONNECT);
  } else {
    // we are connecting/reconnecting to local game
    // so don't stop listenserver
    cl.Disconnect(false);
  }

  cl.Connect(address);

  // Reset error conditions
  gfExtendedError = false;
}

// Takes the map name, strips path and extension
void CL_SetupMapName(const ch *pName, ch *pFixedName, int maxlen) {
  const ch *pSlash = strrchr(pName, '\\');
  const ch *pSlash2 = strrchr(pName, '/');
  if (pSlash2 > pSlash) pSlash = pSlash2;
  if (pSlash)
    ++pSlash;
  else
    pSlash = pName;

  strcpy_s(pFixedName, maxlen, pSlash);
  ch *pExt = strchr(pFixedName, '.');
  if (pExt) *pExt = 0;
}

CPureServerWhitelist *CL_LoadWhitelist(INetworkStringTable *pTable,
                                       const ch *pName) {
  // If there is no entry for the pure server whitelist, then sv_pure is off and
  // the client can do whatever it wants.
  int iString = pTable->FindStringIndex(pName);
  if (iString == INVALID_STRING_INDEX) return nullptr;

  int dataLen;
  const void *pData = pTable->GetStringUserData(iString, &dataLen);
  if (pData) {
    CUtlBuffer buf(pData, dataLen, CUtlBuffer::READ_ONLY);

    CPureServerWhitelist *pWhitelist =
        CPureServerWhitelist::Create(g_pFullFileSystem);
    pWhitelist->Decode(buf);
    return pWhitelist;
  }
  return nullptr;
}

void CL_CheckForPureServerWhitelist() {
#ifdef DISABLE_PURE_SERVER_STUFF
  return;
#endif

  // Don't do sv_pure stuff in SP games or HLTV
  if (cl.m_nMaxClients <= 1 || cl.ishltv) return;

  CPureServerWhitelist *pWhitelist = nullptr;
  if (cl.m_pServerStartupTable)
    pWhitelist =
        CL_LoadWhitelist(cl.m_pServerStartupTable, "PureServerWhitelist");

  if (pWhitelist) {
    if (pWhitelist->IsInFullyPureMode())
      Msg("Got pure server whitelist: sv_pure = 2.\n");
    else
      Msg("Got pure server whitelist: sv_pure = 1.\n");

    CL_HandlePureServerWhitelist(pWhitelist);
  } else {
    Msg("No pure server whitelist. sv_pure = 0\n");
    CL_HandlePureServerWhitelist(nullptr);
  }
}

int CL_GetServerQueryPort() {
  // Yes, this is ugly getting this data out of a string table. Would be better
  // to have it in our network protocol, but we don't have a way to change the
  // protocol without breaking things for people.
  if (!cl.m_pServerStartupTable) return 0;

  int iString = cl.m_pServerStartupTable->FindStringIndex("QueryPort");
  if (iString == INVALID_STRING_INDEX) return 0;

  int dataLen;
  const void *pData =
      cl.m_pServerStartupTable->GetStringUserData(iString, &dataLen);
  if (pData && dataLen == sizeof(int)) return *((const int *)pData);

  return 0;
}

/*
Clean up and move to next part of sequence.
*/
void CL_RegisterResources() {
  // All done precaching.
  host_state.SetWorldModel(cl.GetModel(1));
  if (!host_state.worldmodel) {
    Host_Error(
        "CL_RegisterResources: "
        "host_state.worldmodel/cl.GetModel(1)==nullptr\n");
  }

  // Force main window to repaint... (only does something if running shaderapi
  videomode->InvalidateWindow();
}

void CL_FullyConnected() {
  EngineVGui()->UpdateProgressBar(PROGRESS_FULLYCONNECTED);

  // This has to happen here, in phase 3, because it is in this phase
  // that raycasts against the world is supported (owing to the fact
  // that the world entity has been created by this point)
  StaticPropMgr()->LevelInitClient();

  // loading completed
  // can NOW safely purge unused models and their data hierarchy (materials,
  // shaders, etc)
  modelloader->PurgeUnusedModels();

  // Purge the preload stores, oreder is critical
  g_pMDLCache->ShutdownPreloadData();

  // NOTE: purposely disabling for singleplayer, memory spike causing issues,
  // preload's stay in UNDONE: discard preload for TF to save memory
  // g_pFileSystem->DiscardPreloadData();

  // ***************************************************************
  // NO MORE PRELOAD DATA AVAILABLE PAST THIS POINT!!!
  // ***************************************************************

  g_ClientDLL->LevelInitPostEntity();

  // communicate to tracker that we're in a game
  int ip = cl.m_NetChannel->GetRemoteAddress().GetIP();
  short port = cl.m_NetChannel->GetRemoteAddress().GetPort();
  if (!port) {
    ip = net_local_adr.GetIP();
    port = net_local_adr.GetPort();
  }

  int iQueryPort = CL_GetServerQueryPort();
  EngineVGui()->NotifyOfServerConnect(com_gamedir, ip, port, iQueryPort);

  GetTestScriptMgr()->CheckPoint("FinishedMapLoad");

  EngineVGui()->UpdateProgressBar(PROGRESS_READYTOPLAY);

  // Need this to persist for multiplayer respawns
  CM_DiscardEntityString();

  g_pMDLCache->EndMapLoad();

  if (developer.GetInt() > 0) {
    ConDMsg("Signon traffic \"%s\":  incoming %s, outgoing %s\n",
            cl.m_NetChannel->GetName(),
            Q_pretifymem(cl.m_NetChannel->GetTotalData(FLOW_INCOMING), 3),
            Q_pretifymem(cl.m_NetChannel->GetTotalData(FLOW_OUTGOING), 3));
  }

  // allow normal screen updates
  SCR_EndLoadingPlaque();
  EndLoadingUpdates();

  // TODO(d.rattman): Please oh please move this out of this spot...
  // It so does not belong here. Instead, we want some phase of the
  // client DLL where it knows its read in all entities
  size_t i{CommandLine()->FindParm("-buildcubemaps")};
  if (i != 0) {
    int numIterations = 1;

    if (CommandLine()->ParmCount() > i + 1) {
      numIterations = atoi(CommandLine()->GetParm(i + 1));
    }
    if (numIterations == 0) numIterations = 1;

    void R_BuildCubemapSamples(int numIterations);
    R_BuildCubemapSamples(numIterations);

    Cbuf_AddText("quit\n");
  }

  if (CommandLine()->FindParm("-exit")) Cbuf_AddText("quit\n");

  // background maps are for main menu UI, QMS not needed or used, easier
  // context
  if (!engineClient->IsLevelMainMenuBackground()) {
    // map load complete, safe to allow QMS
    Host_AllowQueuedMaterialSystem(true);
  }

  // This is a Hack, but we need to suppress rendering for a bit in single
  // player to let values settle on the client
  if ((cl.m_nMaxClients == 1) && !demoplayer->IsPlayingBack()) {
    scr_nextdrawtick = host_tickcount + TIME_TO_TICKS(0.25f);
  }

  extern double g_flAccumulatedModelLoadTime;
  extern double g_flAccumulatedSoundLoadTime;
  extern double g_flAccumulatedModelLoadTimeStudio;
  extern double g_flAccumulatedModelLoadTimeVCollideSync;
  extern double g_flAccumulatedModelLoadTimeVCollideAsync;
  extern double g_flAccumulatedModelLoadTimeVirtualModel;
  extern double g_flAccumulatedModelLoadTimeStaticMesh;
  extern double g_flAccumulatedModelLoadTimeBrush;
  extern double g_flAccumulatedModelLoadTimeSprite;
  extern double g_flAccumulatedModelLoadTimeMaterialNamesOnly;
  //	extern double g_flLoadStudioHdr;

  Plat_TimestampedLog("Engine::CL_FullyConnected: Sound Loading time %.4f.",
                      g_flAccumulatedSoundLoadTime);
  Plat_TimestampedLog("  Model Loading time %.4f.",
                      g_flAccumulatedModelLoadTime);
  Plat_TimestampedLog("  Model Loading time studio %.4f.",
                      g_flAccumulatedModelLoadTimeStudio);
  Plat_TimestampedLog(
      "  Model Loading time GetVCollide %.4f "
      "-sync.",
      g_flAccumulatedModelLoadTimeVCollideSync);
  Plat_TimestampedLog(
      "  Model Loading time GetVCollide %.4f "
      "-async.",
      g_flAccumulatedModelLoadTimeVCollideAsync);
  Plat_TimestampedLog("  Model Loading time GetVirtualModel %.4f.",
                      g_flAccumulatedModelLoadTimeVirtualModel);
  Plat_TimestampedLog(
      "  Model loading time Mod_GetModelMaterials "
      "only %.4f.",
      g_flAccumulatedModelLoadTimeMaterialNamesOnly);
  Plat_TimestampedLog("Model Loading time world %.4f.",
                      g_flAccumulatedModelLoadTimeBrush);
  Plat_TimestampedLog("Model Loading time sprites %.4f.",
                      g_flAccumulatedModelLoadTimeSprite);
  Plat_TimestampedLog("Model Loading time meshes %.4f.",
                      g_flAccumulatedModelLoadTimeStaticMesh);

  Plat_TimestampedLog("Engine::CL_FullyConnected: MAP LOAD COMPLETE.");
}

// Called to play the next demo in the demo loop
void CL_NextDemo() {
  if (cl.demonum == -1) return;  // don't play demos

  SCR_BeginLoadingPlaque();

  if (!cl.demos[cl.demonum][0] || cl.demonum == MAX_DEMOS) {
    cl.demonum = 0;
    if (!cl.demos[cl.demonum][0]) {
      scr_disabled_for_loading = false;

      ConMsg("No demos listed with startdemos\n");
      cl.demonum = -1;
      return;
    }
  }

  char str[1024];
  sprintf_s(str, "playdemo %s", cl.demos[cl.demonum]);
  Cbuf_AddText(str);
  cl.demonum++;
}

ConVar cl_screenshotname("cl_screenshotname", "", 0, "Custom Screenshot name");

// We'll take a snapshot at the next available opportunity
void CL_TakeScreenshot(const ch *name) {
  cl_takesnapshot = true;
  cl_takejpeg = false;

  if (name != nullptr) {
    strcpy_s(cl_snapshotname, name);
  } else {
    cl_snapshotname[0] = '\0';

    if (strlen(cl_screenshotname.GetString()) > 0) {
      strcpy_s(cl_snapshotname, cl_screenshotname.GetString());
    }
  }

  cl_snapshot_subdirname[0] = '\0';
}

CON_COMMAND_F(screenshot, "Take a screenshot.", FCVAR_CLIENTCMD_CAN_EXECUTE) {
  GetTestScriptMgr()->SetWaitCheckPoint("screenshot");

  // Don't playback screenshots unless specifically requested.
  if (demoplayer->IsPlayingBack() && !cl_playback_screenshots.GetBool()) return;

  CL_TakeScreenshot(args.ArgC() == 2 ? args[1] : nullptr);
}

CON_COMMAND_F(devshots_screenshot,
              "Used by the -makedevshots system to take a screenshot. For "
              "taking your own screenshots, use the 'screenshot' command "
              "instead.",
              FCVAR_DONTRECORD) {
  CL_TakeScreenshot(nullptr);

  // See if we got a subdirectory to store the devshots in
  if (args.ArgC() == 2) {
    strcpy_s(cl_snapshot_subdirname, args[1]);

    // Use the first available shot in each subdirectory
    cl_snapshotnum = 0;
  }
}

// We'll take a snapshot at the next available opportunity
void CL_TakeJpeg(const ch *name, int quality) {
  // Don't playback screenshots unless specifically requested.
  if (demoplayer->IsPlayingBack() && !cl_playback_screenshots.GetBool()) return;

  cl_takesnapshot = true;
  cl_takejpeg = true;
  cl_jpegquality = std::clamp(quality, 1, 100);

  if (name != nullptr) {
    strcpy_s(cl_snapshotname, name);
  } else {
    cl_snapshotname[0] = '\0';
  }
}

CON_COMMAND(jpeg, "Take a jpeg screenshot:  jpeg <filename> <quality 1-100>.") {
  if (args.ArgC() >= 2) {
    CL_TakeJpeg(args[1],
                args.ArgC() == 3 ? atoi(args[2]) : jpeg_quality.GetInt());
  } else {
    CL_TakeJpeg(nullptr, jpeg_quality.GetInt());
  }
}

void CL_TakeSnapshotAndSwap() {
  bool is_read_pixels_from_front_buffer =
      g_pMaterialSystemHardwareConfig->ReadPixelsFromFrontBuffer();
  if (is_read_pixels_from_front_buffer) Shader_SwapBuffers();

  if (cl_takesnapshot) {
    ch base[MAX_OSPATH], filename[MAX_OSPATH];
    IClientEntity *world = entitylist->GetClientEntity(0);

    g_pFileSystem->CreateDirHierarchy("screenshots", "DEFAULT_WRITE_PATH");

    if (world && world->GetModel()) {
      Q_FileBase(modelloader->GetName((model_t *)world->GetModel()), base,
                 sizeof(base));
    } else {
      Q_strncpy(base, "Snapshot", sizeof(base));
    }

    ch extension[MAX_OSPATH];
    Q_snprintf(extension, sizeof(extension), ".%s",
               cl_takejpeg ? "jpg" : "tga");

    // Using a subdir? If so, create it
    if (cl_snapshot_subdirname[0]) {
      Q_snprintf(filename, sizeof(filename), "screenshots/%s/%s", base,
                 cl_snapshot_subdirname);
      g_pFileSystem->CreateDirHierarchy(filename, "DEFAULT_WRITE_PATH");
    }

    if (cl_snapshotname[0]) {
      Q_strncpy(base, cl_snapshotname, sizeof(base));
      Q_snprintf(filename, sizeof(filename), "screenshots/%s%s", base,
                 extension);

      int iNumber = 0;
      ch renamedfile[MAX_OSPATH];

      while (1) {
        Q_snprintf(renamedfile, sizeof(renamedfile), "screenshots/%s_%04d%s",
                   base, iNumber++, extension);
        if (!g_pFileSystem->GetFileTime(renamedfile)) break;
      }

      if (iNumber > 0) {
        g_pFileSystem->RenameFile(filename, renamedfile);
      }

      cl_screenshotname.SetValue("");
    } else {
      while (1) {
        if (cl_snapshot_subdirname[0]) {
          Q_snprintf(filename, sizeof(filename), "screenshots/%s/%s/%s%04d%s",
                     base, cl_snapshot_subdirname, base, cl_snapshotnum++,
                     extension);
        } else {
          Q_snprintf(filename, sizeof(filename), "screenshots/%s%04d%s", base,
                     cl_snapshotnum++, extension);
        }

        if (!g_pFileSystem->GetFileTime(filename)) {
          // woo hoo!  The file doesn't exist already, so use it.
          break;
        }
      }
    }
    if (cl_takejpeg) {
      videomode->TakeSnapshotJPEG(filename, cl_jpegquality);
      g_ServerRemoteAccess.UploadScreenshot(filename);
    } else {
      videomode->TakeSnapshotTGA(filename);
    }
    cl_takesnapshot = false;
    GetTestScriptMgr()->CheckPoint("screenshot");
  }

  // If recording movie and the console is totally up, then write out this frame
  // to movie file.
  if (cl_movieinfo.IsRecording() && !Con_IsVisible() && !scr_drawloading) {
    videomode->WriteMovieFrame(cl_movieinfo);
    ++cl_movieinfo.movieframe;
  }

  if (!is_read_pixels_from_front_buffer) {
    Shader_SwapBuffers();
  }

  // take a screenshot for savegames if necessary
  saverestore->UpdateSaveGameScreenshots();

  // take screenshot for bx movie maker
  EngineTool_UpdateScreenshot();
}

bool IsIntegralValue(float flValue, float flTolerance = 0.001f) {
  return fabs(RoundFloatToInt(flValue) - flValue) < flTolerance;
}

static float s_flPreviousHostFramerate = 0;
void CL_StartMovie(const ch *filename, int flags, int nWidth, int nHeight,
                   float flFrameRate, int avi_jpeg_quality) {
  Assert(g_hCurrentAVI == AVIHANDLE_INVALID);

  // StartMove depends on host_framerate not being 0.
  s_flPreviousHostFramerate = host_framerate.GetFloat();
  host_framerate.SetValue(flFrameRate);

  cl_movieinfo.Reset();
  strcpy_s(cl_movieinfo.moviename, filename);
  cl_movieinfo.type = flags;
  cl_movieinfo.jpeg_quality = avi_jpeg_quality;

  if (cl_movieinfo.DoAVI() || cl_movieinfo.DoAVISound()) {
// HACK:  THIS MUST MATCH snd_device.h.  Should be exposed more cleanly!!!
#define SOUND_DMA_SPEED 44100  // hardware playback rate

    AVIParams_t params;
    strcpy_s(params.m_pFileName, filename);
    strcpy_s(params.m_pPathID, "MOD");
    params.m_nNumChannels = 2;
    params.m_nSampleBits = 16;
    params.m_nSampleRate = SOUND_DMA_SPEED;
    params.m_nWidth = nWidth;
    params.m_nHeight = nHeight;

    if (IsIntegralValue(flFrameRate)) {
      params.m_nFrameRate = RoundFloatToInt(flFrameRate);
      params.m_nFrameScale = 1;
    } else if (IsIntegralValue(flFrameRate * 1001.0f / 1000.0f)) {
      // 1001 is the ntsc divisor (30*1000/1001 = 29.97, etc)
      params.m_nFrameRate = RoundFloatToInt(flFrameRate * 1001);
      params.m_nFrameScale = 1001;
    } else {
      // arbitrarily choosing 1000 as the divisor
      params.m_nFrameRate = RoundFloatToInt(flFrameRate * 1000);
      params.m_nFrameScale = 1000;
    }

    g_hCurrentAVI = avi->StartAVI(params);
  }

  SND_MovieStart();
}

void CL_EndMovie() {
  if (!CL_IsRecordingMovie()) return;

  host_framerate.SetValue(s_flPreviousHostFramerate);
  s_flPreviousHostFramerate = 0.0f;

  SND_MovieEnd();

  if (cl_movieinfo.DoAVI() || cl_movieinfo.DoAVISound()) {
    avi->FinishAVI(g_hCurrentAVI);
    g_hCurrentAVI = AVIHANDLE_INVALID;
  }

  cl_movieinfo.Reset();
}

bool CL_IsRecordingMovie() { return cl_movieinfo.IsRecording(); }

/*
Sets the engine up to dump frames
*/
CON_COMMAND_F(startmovie, "Start recording movie frames.", FCVAR_DONTRECORD) {
  if (cmd_source != src_command) return;

  if (args.ArgC() < 2) {
    ConMsg("startmovie <filename>\n [\n");
    ConMsg(" (default = TGAs + .wav file)\n");
    ConMsg(" avi = AVI + AVISOUND\n");
    ConMsg(" raw = TGAs + .wav file, same as default\n");
    ConMsg(" tga = TGAs\n");
    ConMsg(" jpg/jpeg = JPegs\n");
    ConMsg(" wav = Write .wav audio file\n");
    ConMsg(
        " jpeg_quality nnn = set jpeq quality to nnn (range 1 to 100), default "
        "%d\n",
        DEFAULT_JPEG_QUALITY);
    ConMsg(" ]\n");
    ConMsg("e.g.:  startmovie testmovie jpg wav jpeg_quality 85\n");
    ConMsg(
        "Using AVI can bring up a dialog for choosing the codec, which may "
        "not show if you are running the engine in fullscreen mode!\n");
    return;
  }

  if (CL_IsRecordingMovie()) {
    ConMsg("Already recording movie!\n");
    return;
  }

  int flags = MovieInfo_t::FMOVIE_TGA | MovieInfo_t::FMOVIE_WAV;
  int movie_jpeg_quality = DEFAULT_JPEG_QUALITY;

  if (args.ArgC() > 2) {
    flags = 0;
    for (int i = 2; i < args.ArgC(); ++i) {
      if (!_stricmp(args[i], "avi")) {
        flags |= MovieInfo_t::FMOVIE_AVI | MovieInfo_t::FMOVIE_AVISOUND;
      } else if (!_stricmp(args[i], "raw")) {
        flags |= MovieInfo_t::FMOVIE_TGA | MovieInfo_t::FMOVIE_WAV;
      } else if (!_stricmp(args[i], "tga")) {
        flags |= MovieInfo_t::FMOVIE_TGA;
      } else if (!_stricmp(args[i], "jpeg") || !_stricmp(args[i], "jpg")) {
        flags &= ~MovieInfo_t::FMOVIE_TGA;
        flags |= MovieInfo_t::FMOVIE_JPG;
      } else if (!_stricmp(args[i], "jpeg_quality")) {
        movie_jpeg_quality = std::clamp(atoi(args[++i]), 1, 100);
      } else if (!_stricmp(args[i], "wav")) {
        flags |= MovieInfo_t::FMOVIE_WAV;
      }
    }
  }

  if (flags == 0) {
    Warning(
        "Missing or unknown recording types, must specify one or both of 'avi' "
        "or 'raw'\n");
    return;
  }

  float flFrameRate = host_framerate.GetFloat();
  if (flFrameRate == 0.0f) flFrameRate = 30.0f;

  CL_StartMovie(args[1], flags, videomode->GetModeWidth(),
                videomode->GetModeHeight(), flFrameRate, movie_jpeg_quality);
  ConMsg(
      "Started recording movie, frames will record after console is "
      "cleared...\n");
}

// Ends frame dumping.
CON_COMMAND_F(endmovie, "Stop recording movie frames.", FCVAR_DONTRECORD) {
  if (CL_IsRecordingMovie()) {
    CL_EndMovie();
    ConMsg("Stopped recording movie...\n");
  } else {
    ConMsg("No movie started.\n");
  }
}

// Send the rest of the command line over as an unconnected command.
CON_COMMAND_F(rcon, "Issue an rcon command.", FCVAR_DONTRECORD) {
  ch message[1024];  // Command message
  ch szParam[256];
  message[0] = 0;

  for (int i = 1; i < args.ArgC(); i++) {
    const ch *pParam = args[i];
    // put quotes around empty arguments so we can pass things like this: rcon
    // sv_password "" otherwise the "" on the end is lost
    if (strchr(pParam, ' ') || strlen(pParam) == 0) {
      sprintf_s(szParam, "\"%s\"", pParam);
      strncat_s(message, szParam, _TRUNCATE);
    } else {
      strncat_s(message, pParam, _TRUNCATE);
    }
    if (i != (args.ArgC() - 1)) {
      strncat_s(message, " ", _TRUNCATE);
    }
  }

  RCONClient().SendCmd(message);
}

CON_COMMAND_F(box, "Draw a debug box.", FCVAR_CHEAT) {
  if (args.ArgC() != 7) {
    ConMsg("box x1 y1 z1 x2 y2 z2\n");
    return;
  }

  Vector mins, maxs;
  for (int i = 0; i < 3; ++i) {
    mins[i] = atof(args[i + 1]);
    maxs[i] = atof(args[i + 4]);
  }
  CDebugOverlay::AddBoxOverlay(vec3_origin, mins, maxs, vec3_angle, 255, 0, 0,
                               0, 100);
}

// Debugging changes the view entity to the specified index
CON_COMMAND_F(cl_view, "Set the view entity index.", FCVAR_CHEAT) {
  if (args.ArgC() != 2) {
    ConMsg("cl_view entity#\nCurrent %i\n", cl.m_nViewEntity);
    return;
  }

  if (cl.m_nMaxClients > 1) return;

  int nNewView = atoi(args[1]);
  if (!nNewView) return;
  if (nNewView > entitylist->GetHighestEntityIndex()) return;

  cl.m_nViewEntity = nNewView;
  videomode->MarkClientViewRectDirty();  // Force recalculation

  ConMsg("View entity set to %i\n", nNewView);
}

static int CL_AllocLightFromArray(dlight_t *pLights, int lightCount, int key) {
  // first look for an exact key match
  if (key) {
    for (int i = 0; i < lightCount; i++) {
      if (pLights[i].key == key) return i;
    }
  }

  // then look for anything else
  for (int i = 0; i < lightCount; i++) {
    if (pLights[i].die < cl.GetTime()) return i;
  }

  return 0;
}

bool g_bActiveDlights = false;
bool g_bActiveElights = false;

dlight_t *CL_AllocDlight(int key) {
  int i = CL_AllocLightFromArray(cl_dlights, MAX_DLIGHTS, key);
  dlight_t *dl = &cl_dlights[i];
  R_MarkDLightNotVisible(i);
  memset(dl, 0, sizeof(*dl));
  dl->key = key;
  r_dlightchanged |= (1 << i);
  r_dlightactive |= (1 << i);
  g_bActiveDlights = true;
  return dl;
}

dlight_t *CL_AllocElight(int key) {
  int i = CL_AllocLightFromArray(cl_elights, MAX_ELIGHTS, key);
  dlight_t *el = &cl_elights[i];
  memset(el, 0, sizeof(*el));
  el->key = key;
  g_bActiveElights = true;
  return el;
}

void CL_DecayLights() {
  float frame_time = cl.GetFrameTime();
  if (frame_time <= 0.0f) return;

  g_bActiveDlights = false;
  g_bActiveElights = false;
  dlight_t *dl = cl_dlights;

  r_dlightchanged = 0;
  r_dlightactive = 0;

  for (int i = 0; i < MAX_DLIGHTS; i++, dl++) {
    if (!dl->IsRadiusGreaterThanZero()) {
      R_MarkDLightNotVisible(i);
      continue;
    }

    if (dl->die < cl.GetTime()) {
      r_dlightchanged |= (1 << i);
      dl->radius = 0;
    } else if (dl->decay) {
      r_dlightchanged |= (1 << i);

      dl->radius -= frame_time * dl->decay;
      if (dl->radius < 0) {
        dl->radius = 0;
      }
    }

    if (dl->IsRadiusGreaterThanZero()) {
      g_bActiveDlights = true;
      r_dlightactive |= (1 << i);
    } else {
      R_MarkDLightNotVisible(i);
    }
  }

  dl = cl_elights;
  for (int i = 0; i < MAX_ELIGHTS; i++, dl++) {
    if (!dl->IsRadiusGreaterThanZero()) continue;

    if (dl->die < cl.GetTime()) {
      dl->radius = 0;
      continue;
    }

    dl->radius -= frame_time * dl->decay;
    if (dl->radius < 0) {
      dl->radius = 0;
    }
    if (dl->IsRadiusGreaterThanZero()) {
      g_bActiveElights = true;
    }
  }
}

void CL_ExtraMouseUpdate(float frametime) {
  // Not ready for commands yet.
  if (!cl.IsActive()) return;
  if (!Host_ShouldRun()) return;

  // Don't create usercmds here during playback, they were encoded into the
  // packet already
  if (demoplayer->IsPlayingBack() && !cl.ishltv) return;

  // Have client .dll create and store usercmd structure
  g_ClientDLL->ExtraMouseSample(frametime, !cl.m_bPaused);
}

// Constructs the movement command and sends it to the server if it's time.
void CL_SendMove() {
  uint8_t data[MAX_CMD_BUFFER];

  int nextcommandnr = cl.lastoutgoingcommand + cl.chokedcommands + 1;

  // send the client update packet

  CLC_Move moveMsg;

  moveMsg.m_DataOut.StartWriting(data, sizeof(data));

  // Determine number of backup commands to send along
  int cl_cmdbackup = 2;
  moveMsg.m_nBackupCommands = std::clamp(cl_cmdbackup, 0, MAX_BACKUP_COMMANDS);

  // How many real new commands have queued up
  moveMsg.m_nNewCommands = 1 + cl.chokedcommands;
  moveMsg.m_nNewCommands =
      std::clamp(moveMsg.m_nNewCommands, 0, MAX_NEW_COMMANDS);

  int numcmds = moveMsg.m_nNewCommands + moveMsg.m_nBackupCommands;

  int from = -1;  // first command is deltaed against zeros

  bool bOK = true;

  for (int to = nextcommandnr - numcmds + 1; to <= nextcommandnr; to++) {
    bool isnewcmd = to >= (nextcommandnr - moveMsg.m_nNewCommands + 1);

    // first valid command number is 1
    bOK = bOK && g_ClientDLL->WriteUsercmdDeltaToBuffer(&moveMsg.m_DataOut,
                                                        from, to, isnewcmd);
    from = to;
  }

  if (bOK) {
    // only write message if all usercmds were written correctly, otherwise
    // parsing would fail
    cl.m_NetChannel->SendNetMsg(moveMsg);
  }
}

void CL_Move(float accumulated_extra_samples, bool bFinalTick) {
  if (!cl.IsConnected()) return;
  if (!Host_ShouldRun()) return;

  // only send packets on the final tick in one engine frame
  bool bSendPacket = true;

  // Don't create usercmds here during playback, they were encoded into the
  // packet already
  if (demoplayer->IsPlayingBack()) {
    if (cl.ishltv) {
      // still do it when playing back a HLTV demo
      bSendPacket = false;
    } else {
      return;
    }
  }

  // don't send packets if update time not reached or chnnel still sending
  // in loopback mode don't send only if host_limitlocal is enabled

  if ((!cl.m_NetChannel->IsLoopback() || host_limitlocal.GetInt()) &&
      ((net_time < cl.m_flNextCmdTime) || !cl.m_NetChannel->CanPacket() ||
       !bFinalTick)) {
    bSendPacket = false;
  }

  if (cl.IsActive()) {
    VPROF("CL_Move");

    int nextcommandnr = cl.lastoutgoingcommand + cl.chokedcommands + 1;

    // Have client .dll create and store usercmd structure
    g_ClientDLL->CreateMove(
        nextcommandnr, host_state.interval_per_tick - accumulated_extra_samples,
        !cl.IsPaused());

    // Store new usercmd to dem file
    if (demorecorder->IsRecording()) {
      // Back up one because we've incremented outgoing_sequence each frame by 1
      // unit
      demorecorder->RecordUserInput(nextcommandnr);
    }

    if (bSendPacket) {
      CL_SendMove();
    } else {
      // netchanll will increase internal outgoing sequnce number too
      cl.m_NetChannel->SetChoked();
      // Mark command as held back so we'll send it next time
      cl.chokedcommands++;
    }
  }

  if (!bSendPacket) return;

  // Request non delta compression if high packet loss, show warning message
  bool hasProblem = cl.m_NetChannel->IsTimingOut() &&
                    !demoplayer->IsPlayingBack() && cl.IsActive();

  // Request non delta compression if high packet loss, show warning message
  if (hasProblem) {
    con_nprint_t np;
    np.time_to_live = 1.0f;
    np.index = 2;
    np.fixed_width_font = false;
    np.color[0] = 1.0f;
    np.color[1] = 0.2f;
    np.color[2] = 0.2f;

    float flTimeOut = cl.m_NetChannel->GetTimeoutSeconds();
    Assert(flTimeOut != -1.0f);
    float flRemainingTime =
        flTimeOut - cl.m_NetChannel->GetTimeSinceLastReceived();
    Con_NXPrintf(&np, "WARNING:  Connection Problem");
    np.index = 3;
    Con_NXPrintf(&np, "Auto-disconnect in %.1f seconds", flRemainingTime);

    cl.ForceFullUpdate();  // sets m_nDeltaTick to -1
  }

  if (cl.IsActive()) {
    NET_Tick mymsg(cl.m_nDeltaTick, host_frametime_unbounded,
                   host_frametime_stddeviation);
    cl.m_NetChannel->SendNetMsg(mymsg);
  }

  // Remember outgoing command that we are sending
  cl.lastoutgoingcommand = cl.m_NetChannel->SendDatagram(nullptr);
  cl.chokedcommands = 0;

  // calc next packet send time
  if (cl.IsActive()) {
    // use full update rate when active
    float commandInterval = 1.0f / cl_cmdrate->GetFloat();
    float maxDelta = std::min(host_state.interval_per_tick, commandInterval);
    float delta =
        std::clamp((float)(net_time - cl.m_flNextCmdTime), 0.0f, maxDelta);
    cl.m_flNextCmdTime = net_time + commandInterval - delta;
  } else {
    // during signon process send only 5 packets/second
    cl.m_flNextCmdTime = net_time + (1.0f / 5.0f);
  }
}

#define TICK_INTERVAL (host_state.interval_per_tick)
#define ROUND_TO_TICKS(t) (TICK_INTERVAL * TIME_TO_TICKS(t))

void CL_LatchInterpolationAmount() {
  if (!cl.IsConnected()) return;

  float dt = cl.m_NetChannel->GetTimeSinceLastReceived();
  float flClientInterpolationAmount =
      ROUND_TO_TICKS(cl.GetClientInterpAmount());

  float flInterp = 0.0f;
  if (flClientInterpolationAmount > 0.001) {
    flInterp = std::clamp(dt / flClientInterpolationAmount, 0.0f, 3.0f);
  }
  cl.m_NetChannel->SetInterpolationAmount(flInterp);
}

void CL_HudMessage(const ch *pMessage) {
  if (g_ClientDLL) g_ClientDLL->HudText(pMessage);
}

CON_COMMAND_F(cl_showents, "Dump entity list to console.", FCVAR_CHEAT) {
  for (int i = 0; i < entitylist->GetMaxEntities(); i++) {
    ch entStr[256], classStr[256];
    IClientNetworkable *client_networkable;

    if ((client_networkable = entitylist->GetClientNetworkable(i)) != nullptr) {
      entStr[0] = 0;
      sprintf_s(classStr, "'%s'",
                client_networkable->GetClientClass()->m_pNetworkName);
    } else {
      sprintf_s(entStr, "(missing), ");
      sprintf_s(classStr, "(missing)");
    }

    if (client_networkable)
      ConMsg("Ent %3d: %s class %s\n", i, entStr, classStr);
  }
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the background level should be loaded on startup
//-----------------------------------------------------------------------------
bool CL_ShouldLoadBackgroundLevel(const CCommand &args) {
  if (InEditMode()) return false;

  // If TF2 and PC we don't want to load the background map.
  bool bIsTF2 = (Q_stricmp(COM_GetModDirectory(), "tf") == 0);
  if (bIsTF2) return false;

  if (args.ArgC() == 2) {
    // presence of args identifies an end-of-game situation
    if (!Q_stricmp(args[1], "force")) {
      // Adrian: Have to do this so the menu shows up if we ever call this while
      // in a level.
      Host_Disconnect(true);
      // pc can't get into background maps fast enough, so just show main menu
      return false;
    }

    if (!Q_stricmp(args[1], "playendgamevid")) {
      // Bail back to the menu and play the end game video.
      CommandLine()->AppendParm("-endgamevid", nullptr);
      CommandLine()->RemoveParm("-recapvid");
      HostState_Restart();
      return false;
    }

    if (!Q_stricmp(args[1], "playrecapvid")) {
      // Bail back to the menu and play the recap video
      CommandLine()->AppendParm("-recapvid", nullptr);
      CommandLine()->RemoveParm("-endgamevid");
      HostState_Restart();
      return false;
    }
  }

  // if force is set, then always return true
  if (CommandLine()->CheckParm("-forcestartupmenu")) return true;

  // don't load the map in developer or console mode
  if (developer.GetInt() || CommandLine()->CheckParm("-console") ||
      CommandLine()->CheckParm("-dev"))
    return false;

  // don't load the map if we're going straight into a level
  if (CommandLine()->CheckParm("+map") ||
      CommandLine()->CheckParm("+connect") ||
      CommandLine()->CheckParm("+playdemo") ||
      CommandLine()->CheckParm("+timedemo") ||
      CommandLine()->CheckParm("+timedemoquit") ||
      CommandLine()->CheckParm("+load") ||
      CommandLine()->CheckParm("-makereslists"))
    return false;

  // nothing else is going on, so load the startup level
  return true;
}

#define DEFAULT_BACKGROUND_NAME "background01"

int g_iRandomChapterIndex = -1;

int CL_GetBackgroundLevelIndex(int nNumChapters) {
  if (g_iRandomChapterIndex != -1) return g_iRandomChapterIndex;

  int iChapterIndex = sv_unlockedchapters.GetInt();
  if (iChapterIndex <= 0) {
    // expected to be [1..N]
    iChapterIndex = 1;
  }

  if (sv_unlockedchapters.GetInt() >= (nNumChapters - 1)) {
    RandomSeed(Plat_MSTime());
    g_iRandomChapterIndex = iChapterIndex = RandomInt(1, nNumChapters);
  }

  return iChapterIndex;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the background level to load
//-----------------------------------------------------------------------------
void CL_GetBackgroundLevelName(ch *pszBackgroundName, size_t bufSize,
                               bool bMapName) {
  strcpy_s(pszBackgroundName, bufSize, DEFAULT_BACKGROUND_NAME);

  KeyValues *pChapterFile = new KeyValues(pszBackgroundName);

  if (pChapterFile->LoadFromFile(g_pFileSystem,
                                 "scripts/ChapterBackgrounds.txt")) {
    KeyValues *pChapterRoot = pChapterFile;

    const ch *szChapterIndex;
    int nNumChapters = 1;
    KeyValues *pChapters = pChapterFile->GetNextKey();
    if (bMapName && pChapters) {
      const ch *pszName = pChapters->GetName();
      if (pszName && pszName[0] && !strncmp("BackgroundMaps", pszName, 14)) {
        pChapterRoot = pChapters;
        pChapters = pChapters->GetFirstSubKey();
      } else {
        pChapters = nullptr;
      }
    } else {
      pChapters = nullptr;
    }

    if (!pChapters) {
      pChapters = pChapterFile->GetFirstSubKey();
    }

    // Find the highest indexed chapter
    while (pChapters) {
      szChapterIndex = pChapters->GetName();

      if (szChapterIndex) {
        int nChapter = atoi(szChapterIndex);

        if (nChapter > nNumChapters) nNumChapters = nChapter;
      }

      pChapters = pChapters->GetNextKey();
    }

    int nChapterToLoad = CL_GetBackgroundLevelIndex(nNumChapters);

    // Find the chapter background with this index
    ch buf[4];
    sprintf_s(buf, "%d", nChapterToLoad);
    KeyValues *pLoadChapter = pChapterRoot->FindKey(buf);

    // Copy the background name
    if (pLoadChapter) {
      strcpy_s(pszBackgroundName, bufSize, pLoadChapter->GetString());
    }
  }

  pChapterFile->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Callback to open the game menus
//-----------------------------------------------------------------------------
void CL_CheckToDisplayStartupMenus(const CCommand &args) {
  if (CL_ShouldLoadBackgroundLevel(args)) {
    ch szBackgroundName[SOURCE_MAX_PATH];
    CL_GetBackgroundLevelName(szBackgroundName, sizeof(szBackgroundName), true);

    char cmd[SOURCE_MAX_PATH];
    sprintf_s(cmd, "map_background %s\n", szBackgroundName);
    Cbuf_AddText(cmd);
  }
}

static float s_fDemoRevealGameUITime = -1;
float s_fDemoPlayMusicTime = -1;
static bool s_bIsRavenHolmn = false;

// Purpose: run the special demo logic when transitioning from the trainstation
// levels
void CL_DemoTransitionFromTrainstation() {
  // kick them out to GameUI instead and bring up the chapter page with raveholm
  // unlocked
  sv_unlockedchapters.SetValue(6);  // unlock ravenholm
  Cbuf_AddText("sv_cheats 1; fadeout 1.5; sv_cheats 0;");
  Cbuf_Execute();
  s_fDemoRevealGameUITime = Plat_FloatTime() + 1.5;
  s_bIsRavenHolmn = false;
}

void CL_DemoTransitionFromRavenholm() {
  Cbuf_AddText("sv_cheats 1; fadeout 2; sv_cheats 0;");
  Cbuf_Execute();
  s_fDemoRevealGameUITime = Plat_FloatTime() + 1.9;
  s_bIsRavenHolmn = true;
}

void CL_DemoTransitionFromTestChmb() {
  Cbuf_AddText("sv_cheats 1; fadeout 2; sv_cheats 0;");
  Cbuf_Execute();
  s_fDemoRevealGameUITime = Plat_FloatTime() + 1.9;
}

// Purpose: make the gameui appear after a certain interval
void V_RenderVGuiOnly();
bool V_CheckGamma();
void CL_DemoCheckGameUIRevealTime() {
  if (s_fDemoRevealGameUITime > 0) {
    if (s_fDemoRevealGameUITime < Plat_FloatTime()) {
      s_fDemoRevealGameUITime = -1;

      SCR_BeginLoadingPlaque();
      Cbuf_AddText("disconnect;");

      CCommand args;
      CL_CheckToDisplayStartupMenus(args);

      s_fDemoPlayMusicTime = Plat_FloatTime() + 1.0;
    }
  }

  if (s_fDemoPlayMusicTime > 0) {
    V_CheckGamma();
    V_RenderVGuiOnly();
    if (s_fDemoPlayMusicTime < Plat_FloatTime()) {
      s_fDemoPlayMusicTime = -1;
      EngineVGui()->ActivateGameUI();

      if (CL_IsHL2Demo()) {
        if (s_bIsRavenHolmn) {
          Cbuf_AddText("play music/ravenholm_1.mp3;");
        } else {
          EngineVGui()->ShowNewGameDialog(
              6);  // bring up the new game dialog in game UI
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Purpose: setup a debug string that is uploaded on crash
//----------------------------------------------------------------------------
extern bool g_bV3SteamInterface;
ch g_minidumpinfo[4096] = {0};
void DisplaySystemVersion(ch *osversion, size_t maxlen);

void CL_SetPagedPoolInfo() {}

void CL_SetSteamCrashComment() {
  ch map[80];
  ch videoinfo[2048];
  ch misc[256];
  ch driverinfo[2048];
  ch osversion[256];

  map[0] = 0;
  driverinfo[0] = 0;
  videoinfo[0] = 0;
  misc[0] = 0;
  osversion[0] = 0;

  if (host_state.worldmodel) {
    CL_SetupMapName(modelloader->GetName(host_state.worldmodel), map,
                    std::size(map));
  }

  DisplaySystemVersion(osversion, std::size(osversion));

  MaterialAdapterInfo_t info;
  materials->GetDisplayAdapterInfo(materials->GetCurrentAdapter(), info);

  const ch *dxlevel = "Unk";
  if (g_pMaterialSystemHardwareConfig) {
    dxlevel = COM_DXLevelToString(
        g_pMaterialSystemHardwareConfig->GetDXSupportLevel());
  }

  // Make a string out of the high part and low parts of driver version
  ch szDXDriverVersion[64];
  Q_snprintf(szDXDriverVersion, std::size(szDXDriverVersion), "%u.%u.%u.%u",
             info.m_nDriverVersionHigh >> 16,
             info.m_nDriverVersionHigh & 0xffff, info.m_nDriverVersionLow >> 16,
             info.m_nDriverVersionLow & 0xffff);

  Q_snprintf(
      driverinfo, std::size(driverinfo),
      "Driver Name:  %s\nDriver Version: %s\nVendorId / DeviceId:  0x%x / "
      "0x%x\nSubSystem / Rev:  0x%x / 0x%x\nDXLevel:  %s\nVid:  %i x %i",
      info.m_pDriverName, szDXDriverVersion, info.m_VendorID, info.m_DeviceID,
      info.m_SubSysID, info.m_Revision, dxlevel ? dxlevel : "Unk",
      videomode->GetModeWidth(), videomode->GetModeHeight());

  ConVarRef mat_picmip("mat_picmip");
  ConVarRef mat_forceaniso("mat_forceaniso");
  ConVarRef mat_trilinear("mat_trilinear");
  ConVarRef mat_antialias("mat_antialias");
  ConVarRef mat_aaquality("mat_aaquality");
  ConVarRef r_shadowrendertotexture("r_shadowrendertotexture");
  ConVarRef r_flashlightdepthtexture("r_flashlightdepthtexture");
  ConVarRef r_waterforceexpensive("r_waterforceexpensive");
  ConVarRef r_waterforcereflectentities("r_waterforcereflectentities");
  ConVarRef mat_vsync("mat_vsync");
  ConVarRef r_rootlod("r_rootlod");
  ConVarRef mat_reducefillrate("mat_reducefillrate");
  ConVarRef mat_motion_blur_enabled("mat_motion_blur_enabled");
  ConVarRef mat_queue_mode("mat_queue_mode");

  Q_snprintf(videoinfo, std::size(videoinfo),
             "picmip: %i forceansio: %i trilinear: %i antialias: %i vsync: %i "
             "rootlod: %i reducefillrate: %i\n"
             "shadowrendertotexture: %i r_flashlightdepthtexture %i "
             "waterforceexpensive: %i waterforcereflectentities: %i "
             "mat_motion_blur_enabled: %i mat_queue_mode %i",
             mat_picmip.GetInt(), mat_forceaniso.GetInt(),
             mat_trilinear.GetInt(), mat_antialias.GetInt(),
             mat_aaquality.GetInt(), mat_vsync.GetInt(), r_rootlod.GetInt(),
             mat_reducefillrate.GetInt(), r_shadowrendertotexture.GetInt(),
             r_flashlightdepthtexture.GetInt(), r_waterforceexpensive.GetInt(),
             r_waterforcereflectentities.GetInt(),
             mat_motion_blur_enabled.GetInt(), mat_queue_mode.GetInt());

  float latency = 0;
  if (cl.m_NetChannel) {
    latency = 1000.0f * cl.m_NetChannel->GetAvgLatency(FLOW_OUTGOING);
  }

  Q_snprintf(misc, std::size(misc),
             "skill:%i rate %i update %.2f cmd %.2f latency %.2f msec",
             skill.GetInt(), cl_rate->GetInt(), cl_updaterate->GetFloat(),
             cl_cmdrate->GetFloat(), latency);

  const ch *pNetChannel = "Not Connected";
  if (cl.m_NetChannel) {
    pNetChannel = cl.m_NetChannel->GetRemoteAddress().ToString();
  }

  CL_SetPagedPoolInfo();

  Q_snprintf(g_minidumpinfo, std::size(g_minidumpinfo),
             "Map: %s\n"
             "Game: %s\n"
             "Build: %i\n"
             "Misc: %s\n"
             "Net: %s\n"
             "cmdline:%s\n"
             "driver: %s\n"
             "video: %s\n"
             "OS: %s\n",
             map, com_gamedir, build_number(), misc, pNetChannel,
             CommandLine()->GetCmdLine(), driverinfo, videoinfo, osversion);

  ch full[4096];
  Q_snprintf(full, std::size(full), "%s\n", g_minidumpinfo);

#ifndef NO_STEAM
  SteamAPI_SetMiniDumpComment(full);
#endif
}

//
// register commands
//
static ConCommand startupmenu("startupmenu", &CL_CheckToDisplayStartupMenus,
                              "Opens initial menu screen and loads the "
                              "background bsp, but only if no other level is "
                              "being loaded, and we're not in developer mode.");

ConVar cl_language("cl_language", "english", FCVAR_USERINFO,
                   "Language (from HKCU\\Software\\Valve\\Steam\\Language)");
void CL_InitLanguageCvar() {
// !! bug do i need to do something linux-wise here.
#ifdef _WIN32
  ch language[64];
  if (IsPC()) {
    memset(language, 0, sizeof(language));
    vgui::system()->GetRegistryString(
        "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language,
        sizeof(language) - 1);
    if (Q_strlen(language) == 0) {
      Q_strncpy(language, "english", sizeof(language));
    }
  } else {
    Q_strncpy(language, XBX_GetLanguageString(), sizeof(language));
  }
  cl_language.SetValue(language);
#endif
}

void CL_Init() {
  cl.Clear();

  ch szRate[128];
  szRate[0] = '\0';

  // get rate from registry
  Sys_GetRegKeyValue("Software\\Valve\\Steam", "Rate", szRate, sizeof(szRate),
                     "10000");

  if (strlen(szRate) > 0) {
    cl_rate->SetValue(std::clamp(atoi(szRate), MIN_RATE, MAX_RATE));
  }

  CL_InitLanguageCvar();
}

void CL_Shutdown() {}

CON_COMMAND_F(cl_fullupdate, "Forces the server to send a full update packet",
              FCVAR_CHEAT) {
  cl.ForceFullUpdate();
}

#ifdef _DEBUG
CON_COMMAND(cl_download, "Downloads a file from server.") {
  if (args.ArgC() != 2) return;

  if (!cl.m_NetChannel) return;

  cl.m_NetChannel->RequestFile(args[1]);  // just for testing stuff
}
#endif  // _DEBUG

CON_COMMAND_F(setinfo, "Addes a new user info value",
              FCVAR_CLIENTCMD_CAN_EXECUTE) {
  if (args.ArgC() != 3) {
    Msg("Syntax: setinfo <key> <value>\n");
    return;
  }

  const ch *name = args[1], *value = args[2];
  ConCommandBase *pCommand = g_pCVar->FindCommandBase(name);

  if (pCommand) {
    if (pCommand->IsCommand()) {
      Msg("Name %s is already registered as console command\n", name);
      return;
    }

    if (!pCommand->IsFlagSet(FCVAR_USERINFO)) {
      Msg("Convar %s is already registered but not as user info value\n", name);
      return;
    }
  } else {
    // cvar not found, create it now
    size_t size = strlen(name) + 1;
    ch *pszString = new ch[size];
    strcpy_s(pszString, size, name);

    pCommand =
        new ConVar(pszString, "", FCVAR_USERINFO, "Custom user info value");
  }

  ConVar *pConVar = (ConVar *)pCommand;
  pConVar->SetValue(value);

  if (cl.IsConnected()) {
    // send changed cvar to server
    NET_SetConVar convar(name, value);
    cl.m_NetChannel->SendNetMsg(convar);
  }
}

CON_COMMAND(cl_precacheinfo, "Show precache info (client).") {
  if (args.ArgC() == 2) {
    cl.DumpPrecacheStats(args[1]);
    return;
  }

  // Show all data
  cl.DumpPrecacheStats(MODEL_PRECACHE_TABLENAME);
  cl.DumpPrecacheStats(DECAL_PRECACHE_TABLENAME);
  cl.DumpPrecacheStats(SOUND_PRECACHE_TABLENAME);
  cl.DumpPrecacheStats(GENERIC_PRECACHE_TABLENAME);
}

void Callback_ModelChanged(void *object, INetworkStringTable *stringTable,
                           int stringNumber, const ch *newString,
                           const void *newData) {
  if (stringTable == cl.m_pModelPrecacheTable) {
    // Index 0 is always nullptr, just ignore it
    // Index 1 == the world, don't
    if (stringNumber >= 1) {
      cl.SetModel(stringNumber);
    }
  } else {
    Assert(0);  // Callback_*Changed called with wrong stringtable
  }
}

void Callback_GenericChanged(void *object, INetworkStringTable *stringTable,
                             int stringNumber, const ch *newString,
                             const void *newData) {
  if (stringTable == cl.m_pGenericPrecacheTable) {
    // Index 0 is always nullptr, just ignore it
    if (stringNumber >= 1) {
      cl.SetGeneric(stringNumber);
    }
  } else {
    Assert(0);  // Callback_*Changed called with wrong stringtable
  }
}

void Callback_SoundChanged(void *object, INetworkStringTable *stringTable,
                           int stringNumber, const ch *newString,
                           const void *newData) {
  if (stringTable == cl.m_pSoundPrecacheTable) {
    // Index 0 is always nullptr, just ignore it
    if (stringNumber >= 1) {
      cl.SetSound(stringNumber);
    }
  } else {
    Assert(0);  // Callback_*Changed called with wrong stringtable
  }
}

void Callback_DecalChanged(void *object, INetworkStringTable *stringTable,
                           int stringNumber, const ch *newString,
                           const void *newData) {
  if (stringTable == cl.m_pDecalPrecacheTable) {
    cl.SetDecal(stringNumber);
  } else {
    Assert(0);  // Callback_*Changed called with wrong stringtable
  }
}

void Callback_InstanceBaselineChanged(void *object,
                                      INetworkStringTable *stringTable,
                                      int stringNumber, const ch *newString,
                                      const void *newData) {
  Assert(stringTable == cl.m_pInstanceBaselineTable);
  // cl.UpdateInstanceBaseline( stringNumber );
}

void Callback_UserInfoChanged(void *object, INetworkStringTable *stringTable,
                              int stringNumber, const ch *newString,
                              const void *newData) {
  Assert(stringTable == cl.m_pUserInfoTable);

  // stringnumber == player slot

  player_info_t *player = (player_info_t *)newData;

  if (!player) return;  // player left the game

  // request custom user files if necessary
  for (int i = 0; i < MAX_CUSTOM_FILES; i++) {
    cl.CheckOthersCustomFile(player->customFiles[i]);
  }

  // fire local client event game event
  IGameEvent *event = g_GameEventManager.CreateEvent("player_info");

  if (event) {
    event->SetInt("userid", player->userID);
    event->SetInt("friendsid", player->friendsID);
    event->SetInt("index", stringNumber);
    event->SetString("name", player->name);
    event->SetString("networkid", player->guid);
    event->SetBool("bot", player->fakeplayer);

    g_GameEventManager.FireEventClientSide(event);
  }
}

void CL_HookClientStringTables() {
  // install hooks
  int numTables = cl.m_StringTableContainer->GetNumTables();

  for (int i = 0; i < numTables; i++) {
    // iterate through server tables
    auto *pTable = assert_cast<CNetworkStringTable *>(
        cl.m_StringTableContainer->GetTable(i));

    if (!pTable) continue;

    cl.HookClientStringTable(pTable->GetTableName());
  }
}
// Installs the all, and invokes cb for all existing items
void CL_InstallAndInvokeClientStringTableCallbacks() {
  // install hooks
  int numTables = cl.m_StringTableContainer->GetNumTables();

  for (int i = 0; i < numTables; i++) {
    // iterate through server tables
    CNetworkStringTable *pTable =
        (CNetworkStringTable *)cl.m_StringTableContainer->GetTable(i);

    if (!pTable) continue;

    pfnStringChanged pOldFunction = pTable->GetCallback();

    cl.InstallStringTableCallback(pTable->GetTableName());

    pfnStringChanged pNewFunction = pTable->GetCallback();
    if (!pNewFunction) continue;

    // We already had it installed (e.g., from client .dll) so all of the
    // callbacks have been called and don't need a second dose
    if (pNewFunction == pOldFunction) continue;

    for (int j = 0; j < pTable->GetNumStrings(); ++j) {
      int userDataSize;
      const void *pUserData = pTable->GetStringUserData(j, &userDataSize);
      (*pNewFunction)(nullptr, pTable, j, pTable->GetString(j), pUserData);
    }
  }
}

// Singleton client state
CClientState cl;
