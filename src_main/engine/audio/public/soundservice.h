// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Applicaton-level hooks for clients of the audio subsystem

#ifndef SOUNDSERVICE_H
#define SOUNDSERVICE_H

#include "tier1/utlrbtree.h"

class Vector;
class QAngle;
class CAudioSource;
typedef int SoundSource;
struct SpatializationInfo_t;
typedef void *FileNameHandle_t;
struct StartSoundParams_t;

// Purpose: Services required by the audio system to function, this facade
// defines the bridge between the audio code and higher level systems.
//
// Note that some of these currently suggest that certain functionality would
// like to exist at a deeper layer so systems like audio can take advantage of
// them directly (toml 05-02-02)
the_interface ISoundServices {
 public:
  // Allocate a block of memory that will be automatically
  // cleaned up on level change
  virtual void *LevelAlloc(int nBytes, const ch *pszTag) = 0;

  // Notification that someone called S_ExtraUpdate()
  virtual void OnExtraUpdate() = 0;

  // Return false if the entity doesn't exist or is out of the PVS, in which
  // case the sound shouldn't be heard.
  virtual bool GetSoundSpatialization(int entIndex,
                                      SpatializationInfo_t &info) = 0;

  // This is the client's clock, which follows the servers and thus isn't 100%
  // smooth all the time (it is in single player)
  virtual float GetClientTime() = 0;

  // This is the engine's filtered timer, it's pretty smooth all the time
  virtual float GetHostTime() = 0;

  virtual int GetViewEntity() = 0;

  virtual float GetHostFrametime() = 0;
  virtual void SetSoundFrametime(float realDt, float hostDt) = 0;

  virtual int GetServerCount() = 0;

  virtual bool IsPlayer(SoundSource source) = 0;

  virtual void OnChangeVoiceStatus(int entity, bool status) = 0;

  // Is the player fully connected (don't do DSP processing if not)
  virtual bool IsConnected() = 0;

  // Calls into client .dll with list of close caption tokens to construct a
  // caption out of
  virtual void EmitSentenceCloseCaption(ch const *tokenstream) = 0;
  // Calls into client .dll with list of close caption tokens to construct a
  // caption out of
  virtual void EmitCloseCaption(ch const *captionname, float duration) = 0;

  virtual ch const *GetGameDir() = 0;

  // If the game is paused, certain audio will pause, too (anything with
  // phoneme/sentence data for now)
  virtual bool IsGamePaused() = 0;

  // restarts the sound system externally
  virtual void RestartSoundSystem() = 0;

  virtual void GetAllSoundFilesReferencedInReslists(
      CUtlRBTree<FileNameHandle_t, int> & list) = 0;
  virtual void GetAllManifestFiles(CUtlRBTree<FileNameHandle_t, int> &
                                   list) = 0;
  virtual void GetAllSoundFilesInManifest(
      CUtlRBTree<FileNameHandle_t, int> & list, ch const *manifestfile) = 0;

  virtual void CacheBuildingStart() = 0;
  virtual void CacheBuildingUpdateProgress(float percent,
                                           ch const *cachefile) = 0;
  virtual void CacheBuildingFinish() = 0;

  // For building sound cache manifests
  virtual int GetPrecachedSoundCount() = 0;
  virtual ch const *GetPrecachedSound(int index) = 0;

  virtual void OnSoundStarted(int guid, StartSoundParams_t &params,
                              ch const *soundname) = 0;
  virtual void OnSoundStopped(int guid, int soundsource, int channel,
                              ch const *soundname) = 0;

  virtual bool GetToolSpatialization(int iUserData, int guid,
                                     SpatializationInfo_t &info) = 0;

  virtual ch const *GetUILanguage() = 0;
};

extern ISoundServices *g_pSoundServices;

#endif  // SOUNDSERVICE_H
