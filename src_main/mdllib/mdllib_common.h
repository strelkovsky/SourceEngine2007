// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MDLLIB_COMMON_H_
#define SOURCE_MDLLIB_COMMON_H_

#include "mdllib/mdllib.h"

#include "tier0/include/platform.h"

//-----------------------------------------------------------------------------
// Purpose: Interface to accessing P4 commands
//-----------------------------------------------------------------------------
class CMdlLib : public CBaseAppSystem<IMdlLib> {
 public:
  // Destructor
  virtual ~CMdlLib();

  //////////////////////////////////////////////////////////////////////////
  //
  // Methods of IAppSystem
  //
  //////////////////////////////////////////////////////////////////////////
 public:
  virtual bool Connect(CreateInterfaceFn factory);
  virtual InitReturnVal_t Init();
  virtual void *QueryInterface(const char *pInterfaceName);
  virtual void Shutdown();
  virtual void Disconnect();

  //////////////////////////////////////////////////////////////////////////
  //
  // Methods of IMdlLib
  //
  //////////////////////////////////////////////////////////////////////////
 public:
  //
  // StripModelBuffers
  //	The main function that strips the model buffers
  //		mdlBuffer			- mdl buffer, updated, no size
  // change
  //		vvdBuffer			- vvd buffer, updated, size
  // reduced
  //		vtxBuffer			- vtx buffer, updated, size
  // reduced
  //		ppStripInfo			- if nonzero on return will be
  //filled  with  the stripping info
  //
  virtual bool StripModelBuffers(CUtlBuffer &mdlBuffer, CUtlBuffer &vvdBuffer,
                                 CUtlBuffer &vtxBuffer,
                                 IMdlStripInfo **ppStripInfo);

  //
  // CreateNewStripInfo
  //	Creates an empty strip info so that it can be reused.
  //
  virtual bool CreateNewStripInfo(IMdlStripInfo **ppStripInfo);
};

#endif  // SOURCE_MDLLIB_COMMON_H_
