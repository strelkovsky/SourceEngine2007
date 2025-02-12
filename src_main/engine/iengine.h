// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef IENGINE_H
#define IENGINE_H

#include "base/include/base_types.h"
#include "tier1/interface.h"

the_interface IEngine {
 public:
  enum { QUIT_NOTQUITTING = 0, QUIT_TODESKTOP, QUIT_RESTART };

  // Engine State Flags
  enum EngineState_t {
    DLL_INACTIVE = 0,  // no dll
    DLL_ACTIVE,        // engine is focused
    DLL_CLOSE,         // closing down dll
    DLL_RESTART,       // engine is shutting down but will restart right away
    DLL_PAUSED,        // engine is paused, can become active from this state
  };

  virtual ~IEngine() {}

  virtual bool Load(bool dedicated, const ch *rootdir) = 0;
  virtual void Unload() = 0;

  virtual void SetNextState(EngineState_t iNextState) = 0;
  virtual EngineState_t GetState() const = 0;

  virtual void Frame() = 0;

  virtual f32 GetFrameTime() const = 0;
  virtual f32 GetCurTime() const = 0;

  virtual i32 GetQuitting() const = 0;
  virtual void SetQuitting(i32 quittype) = 0;
};

extern IEngine *eng;

#endif  // IENGINE_H
