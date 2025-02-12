// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Deals with singleton

#if !defined(DETAILOBJECTSYSTEM_H)
#define DETAILOBJECTSYSTEM_H

#include "IClientEntityInternal.h"
#include "IGameSystem.h"
#include "IVRenderView.h"
#include "engine/IVModelRender.h"
#include "mathlib/vector.h"

struct model_t;

//-----------------------------------------------------------------------------
// Responsible for managing detail objects
//-----------------------------------------------------------------------------
the_interface IDetailObjectSystem : public IGameSystem {
 public:
  // Gets a particular detail object
  virtual IClientRenderable *GetDetailModel(int idx) = 0;

  // Gets called each view
  virtual void BuildDetailObjectRenderLists(const Vector &vViewOrigin) = 0;

  // Renders all opaque detail objects in a particular set of leaves
  virtual void RenderOpaqueDetailObjects(int nLeafCount,
                                         LeafIndex_t *pLeafList) = 0;

  // Call this before rendering translucent detail objects
  virtual void BeginTranslucentDetailRendering() = 0;

  // Renders all translucent detail objects in a particular set of leaves
  virtual void RenderTranslucentDetailObjects(
      const Vector &viewOrigin, const Vector &viewForward,
      const Vector &viewRight, const Vector &viewUp, int nLeafCount,
      LeafIndex_t *pLeafList) = 0;

  // Renders all translucent detail objects in a particular leaf up to a
  // particular point
  virtual void RenderTranslucentDetailObjectsInLeaf(
      const Vector &viewOrigin, const Vector &viewForward,
      const Vector &viewRight, const Vector &viewUp, int nLeaf,
      const Vector *pVecClosestPoint) = 0;
};

//-----------------------------------------------------------------------------
// System for dealing with detail objects
//-----------------------------------------------------------------------------
IDetailObjectSystem *DetailObjectSystem();

#endif  // DETAILOBJECTSYSTEM_H
