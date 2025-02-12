// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "view.h"

#include "IOcclusionSystem.h"
#include "cdll_engine_int.h"
#include "cl_demouipanel.h"
#include "cl_main.h"
#include "client.h"
#include "console.h"
#include "debugoverlay.h"
#include "demo.h"
#include "engine/view_sharedv1.h"
#include "gl_cvars.h"
#include "gl_drawlights.h"
#include "gl_matsysiface.h"
#include "gl_model_private.h"
#include "gl_rmain.h"
#include "gl_rsurf.h"
#include "gl_shader.h"
#include "host.h"
#include "ispatialpartitioninternal.h"
#include "istudiorender.h"
#include "ivideomode.h"
#include "ivrenderview.h"
#include "l_studio.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "mod_vis.h"
#include "quakedef.h"
#include "r_local.h"
#include "sys.h"
#include "tier0/include/vprof.h"
#include "toolframework/itoolframework.h"
#include "vgui_baseui_interface.h"

#include "tier0/include/memdbgon.h"

class IClientEntity;

float r_blend;
float r_colormod[3] = {1, 1, 1};
bool g_bIsBlendingOrModulating = false;

bool g_bIsRenderingVGuiOnly = false;

color32 R_LightPoint(Vector &p);
void R_DrawLightmaps(IWorldRenderList *pList, int pageId);
void R_DrawIdentityBrushModel(IWorldRenderList *pRenderList, model_t *model);

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

extern ConVar r_avglightmap;

/*
=================
V_CheckGamma

TODO(d.rattman):  Define this as a change function to the ConVar's below rather
than polling it every frame.  Note, still need to make sure it gets called very
first time through frame loop.
=================
*/
bool V_CheckGamma() {
  if (IsX360()) return false;

  static int lastLightmap = -1;
  extern void GL_RebuildLightmaps(void);

  // Refresh all lightmaps if r_avglightmap changes
  if (r_avglightmap.GetInt() != lastLightmap) {
    lastLightmap = r_avglightmap.GetInt();
    GL_RebuildLightmaps();
  }

  return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the view renderer
// Output : void V_Init
//-----------------------------------------------------------------------------
void V_Init() { BuildGammaTable(2.2f, 2.2f, 0.0f, 2); }

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void V_Shutdown() {
  // TODO, cleanup
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :  -
//-----------------------------------------------------------------------------
void V_RenderVGuiOnly_NoSwap() {
  // Need to clear the screen in this case, cause we're not drawing
  // the loading screen.
  UpdateMaterialSystemConfig();

  CMatRenderContextPtr pRenderContext(materials);

  pRenderContext->ClearBuffers(true, true);

  EngineVGui()->Paint(PAINT_UIPANELS);
}

//-----------------------------------------------------------------------------
// Purpose: Renders only vgui (for loading progress) including buffer swapping
// and vgui simulation
//-----------------------------------------------------------------------------
void V_RenderVGuiOnly() {
  materials->BeginFrame(host_frametime);
  EngineVGui()->Simulate();

  g_EngineRenderer->FrameBegin();

  toolframework->RenderFrameBegin();

  V_RenderVGuiOnly_NoSwap();

  toolframework->RenderFrameEnd();

  g_EngineRenderer->FrameEnd();
  materials->EndFrame();

  Shader_SwapBuffers();
}

//-----------------------------------------------------------------------------
// Purpose: Render the world
//-----------------------------------------------------------------------------
void V_RenderView() {
  VPROF("V_RenderView");
  MDLCACHE_COARSE_LOCK_(g_pMDLCache);

  bool bCanRenderWorld = (host_state.worldmodel != NULL) && cl.IsActive();

  bCanRenderWorld = bCanRenderWorld && toolframework->ShouldGameRenderView();

  if (bCanRenderWorld && g_bTextMode) {
    // Sleep to let the other textmode clients get some cycles.
    Sys_Sleep(15);
    bCanRenderWorld = false;
  }

  if (!bCanRenderWorld) {
    // Because we now do a lot of downloading before spawning map, don't render
    // anything world related until we are an active client.
    V_RenderVGuiOnly_NoSwap();
  } else if (!g_LostVideoMemory) {
    // We can get into situations where some other material system app
    // is trying to start up; in those cases, we shouldn't render...
    Rect_t scr_vrect = videomode->GetClientViewRect();
    g_ClientDLL->View_Render(&scr_vrect);
  }
}

void Linefile_Draw(void);

//-----------------------------------------------------------------------------
// Purpose: Expose rendering interface to client .dll
//-----------------------------------------------------------------------------
class CVRenderView : public IVRenderView, public ISpatialLeafEnumerator {
 public:
  void TouchLight(dlight_t *light) {
    int i;

    i = light - cl_dlights;
    if (i >= 0 && i < MAX_DLIGHTS) {
      r_dlightchanged |= (1 << i);
    }
  }

  void DrawBrushModel(IClientEntity *baseentity, model_t *model,
                      const Vector &origin, const QAngle &angles, bool bSort) {
    R_DrawBrushModel(baseentity, model, origin, angles, bSort, false);
  }

  // Draw brush model shadow
  void DrawBrushModelShadow(IClientRenderable *pRenderable) {
    R_DrawBrushModelShadow(pRenderable);
  }

  void DrawIdentityBrushModel(IWorldRenderList *pList, model_t *model) {
    R_DrawIdentityBrushModel(pList, model);
  }

  void Draw3DDebugOverlays() {
    DrawSavedModelDebugOverlays();

    if (g_pDemoUI) {
      g_pDemoUI->DrawDebuggingInfo();
    }

    if (g_pDemoUI2) {
      g_pDemoUI2->DrawDebuggingInfo();
    }

    SpatialPartition()->DrawDebugOverlays();

    CDebugOverlay::Draw3DOverlays();

    // Render occlusion debugging info
    OcclusionSystem()->DrawDebugOverlays();
  }

  SOURCE_FORCEINLINE void CheckBlend() {
    g_bIsBlendingOrModulating = (r_blend != 1.0) || (r_colormod[0] != 1.0) ||
                                (r_colormod[1] != 1.0) ||
                                (r_colormod[2] != 1.0);
  }
  void SetBlend(float blend) {
    r_blend = blend;
    CheckBlend();
  }

  float GetBlend() { return r_blend; }

  void SetColorModulation(float const *blend) {
    VectorCopy(blend, r_colormod);
    CheckBlend();
  }

  void GetColorModulation(float *blend) { VectorCopy(r_colormod, blend); }

  void SceneBegin() { g_EngineRenderer->DrawSceneBegin(); }

  void SceneEnd() { g_EngineRenderer->DrawSceneEnd(); }

  void GetVisibleFogVolume(const Vector &vEyePoint,
                           VisibleFogVolumeInfo_t *pInfo) {
    R_GetVisibleFogVolume(vEyePoint, pInfo);
  }

  IWorldRenderList *CreateWorldList() {
    return g_EngineRenderer->CreateWorldList();
  }

  void BuildWorldLists(IWorldRenderList *pList, WorldListInfo_t *pInfo,
                       int iForceFViewLeaf, const VisOverrideData_t *pVisData,
                       bool bShadowDepth, float *pReflectionWaterHeight) {
    g_EngineRenderer->BuildWorldLists(pList, pInfo, iForceFViewLeaf, pVisData,
                                      bShadowDepth, pReflectionWaterHeight);
  }

  void DrawWorldLists(IWorldRenderList *pList, unsigned long flags,
                      float waterZAdjust) {
    g_EngineRenderer->DrawWorldLists(pList, flags, waterZAdjust);
  }

  // Optimization for top view
  void DrawTopView(bool enable) { R_DrawTopView(enable); }

  void TopViewBounds(Vector2D const &mins, Vector2D const &maxs) {
    R_TopViewBounds(mins, maxs);
  }

  void DrawLights() {
    DrawLightSprites();

    DrawLightDebuggingInfo();
  }

  void DrawMaskEntities() {
    // UNDONE: Don't do this with masked brush models, they should probably be
    // in a separate list R_DrawMaskEntities()
  }

  void DrawTranslucentSurfaces(IWorldRenderList *pList, int sortIndex,
                               unsigned long flags, bool bShadowDepth) {
    Shader_DrawTranslucentSurfaces(pList, sortIndex, flags, bShadowDepth);
  }

  bool LeafContainsTranslucentSurfaces(IWorldRenderList *pList, int sortIndex,
                                       unsigned long flags) {
    return Shader_LeafContainsTranslucentSurfaces(pList, sortIndex, flags);
  }

  void DrawLineFile() { Linefile_Draw(); }

  void DrawLightmaps(IWorldRenderList *pList, int pageId) {
    R_DrawLightmaps(pList, pageId);
  }

  void ViewSetupVis(bool novis, int numorigins, const Vector origin[]) {
    g_EngineRenderer->ViewSetupVis(novis, numorigins, origin);
  }

  void ViewSetupVisEx(bool novis, int numorigins, const Vector origin[],
                      unsigned int &returnFlags) {
    g_EngineRenderer->ViewSetupVisEx(novis, numorigins, origin, returnFlags);
  }

  bool AreAnyLeavesVisible(int *leafList, int nLeaves) {
    return Map_AreAnyLeavesVisible(*host_state.worldbrush, leafList, nLeaves);
  }

  // For backward compatibility only!!!
  void VguiPaint() { EngineVGui()->BackwardCompatibility_Paint(); }

  void VGui_Paint(int mode) { EngineVGui()->Paint((PaintMode_t)mode); }

  void ViewDrawFade(uint8_t *color, IMaterial *pFadeMaterial) {
    VPROF_BUDGET("ViewDrawFade", VPROF_BUDGETGROUP_WORLD_RENDERING);
    g_EngineRenderer->ViewDrawFade(color, pFadeMaterial);
  }

  void OLD_SetProjectionMatrix(float fov, float zNear, float zFar) {
    // Here to preserve backwards compat
  }

  void OLD_SetOffCenterProjectionMatrix(float fov, float zNear, float zFar,
                                        float flAspectRatio, float flBottom,
                                        float flTop, float flLeft,
                                        float flRight) {
    // Here to preserve backwards compat
  }

  void OLD_SetProjectionMatrixOrtho(float left, float top, float right,
                                    float bottom, float zNear, float zFar) {
    // Here to preserve backwards compat
  }

  color32 GetLightAtPoint(Vector &pos) { return R_LightPoint(pos); }

  int GetViewEntity() { return cl.m_nViewEntity; }

  float GetFieldOfView() { return g_EngineRenderer->GetFov(); }

  unsigned char **GetAreaBits() {
    return cl.GetAreaBits_BackwardCompatibility();
  }

  virtual void SetAreaState(
      unsigned char chAreaBits[MAX_AREA_STATE_BYTES],
      unsigned char chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES]) {
    *cl.GetAreaBits_BackwardCompatibility() =
        0;  // Clear the b/w compatibiltiy thing.
    memcpy(cl.m_chAreaBits, chAreaBits, MAX_AREA_STATE_BYTES);
    memcpy(cl.m_chAreaPortalBits, chAreaPortalBits,
           MAX_AREA_PORTAL_STATE_BYTES);
    cl.m_bAreaBitsValid = true;
  }

  // World fog for world rendering
  void SetFogVolumeState(int fogVolume, bool useHeightFog) {
    R_SetFogVolumeState(fogVolume, useHeightFog);
  }

  virtual void InstallBrushSurfaceRenderer(IBrushRenderer *pBrushRenderer) {
    R_InstallBrushRenderOverride(pBrushRenderer);
  }

  struct BoxIntersectWaterContext_t {
    bool m_bFoundWaterLeaf;
    int m_nLeafWaterDataID;
  };

  bool EnumerateLeaf(int leaf, int context) {
    BoxIntersectWaterContext_t *pSearchContext =
        (BoxIntersectWaterContext_t *)context;
    mleaf_t *pLeaf = &host_state.worldmodel->brush.pShared->leafs[leaf];
    if (pLeaf->leafWaterDataID == pSearchContext->m_nLeafWaterDataID) {
      pSearchContext->m_bFoundWaterLeaf = true;
      // found it . . stop enumeration
      return false;
    }
    return true;
  }

  bool DoesBoxIntersectWaterVolume(const Vector &mins, const Vector &maxs,
                                   int leafWaterDataID) {
    BoxIntersectWaterContext_t context;
    context.m_bFoundWaterLeaf = false;
    context.m_nLeafWaterDataID = leafWaterDataID;
    g_pToolBSPTree->EnumerateLeavesInBox(mins, maxs, this, (int)&context);
    return context.m_bFoundWaterLeaf;
  }

  // Push, pop views
  virtual void Push3DView(const CViewSetup &view, int nFlags,
                          ITexture *pRenderTarget, Frustum frustumPlanes) {
    g_EngineRenderer->Push3DView(view, nFlags, pRenderTarget, frustumPlanes,
                                 NULL);
  }

  virtual void Push2DView(const CViewSetup &view, int nFlags,
                          ITexture *pRenderTarget, Frustum frustumPlanes) {
    g_EngineRenderer->Push2DView(view, nFlags, pRenderTarget, frustumPlanes);
  }

  virtual void PopView(Frustum frustumPlanes) {
    g_EngineRenderer->PopView(frustumPlanes);
  }

  virtual void SetMainView(const Vector &vecOrigin, const QAngle &angles) {
    g_EngineRenderer->SetMainView(vecOrigin, angles);
  }

  void OverrideViewFrustum(Frustum custom) {
    g_EngineRenderer->OverrideViewFrustum(custom);
  }

  void DrawBrushModelShadowDepth(IClientEntity *baseentity, model_t *model,
                                 const Vector &origin, const QAngle &angles,
                                 bool bSort) {
    R_DrawBrushModel(baseentity, model, origin, angles, bSort, true);
  }

  void UpdateBrushModelLightmap(model_t *model,
                                IClientRenderable *pRenderable) {
    g_EngineRenderer->UpdateBrushModelLightmap(model, pRenderable);
  }

  void BeginUpdateLightmaps() { g_EngineRenderer->BeginUpdateLightmaps(); }

  void EndUpdateLightmaps() { g_EngineRenderer->EndUpdateLightmaps(); }

  virtual void Push3DView(const CViewSetup &view, int nFlags,
                          ITexture *pRenderTarget, Frustum frustumPlanes,
                          ITexture *pDepthTexture) {
    g_EngineRenderer->Push3DView(view, nFlags, pRenderTarget, frustumPlanes,
                                 pDepthTexture);
  }

  void GetMatricesForView(const CViewSetup &view, VMatrix *pWorldToView,
                          VMatrix *pViewToProjection,
                          VMatrix *pWorldToProjection,
                          VMatrix *pWorldToPixels) {
    ComputeViewMatrices(pWorldToView, pViewToProjection, pWorldToProjection,
                        view);
    ComputeWorldToScreenMatrix(pWorldToPixels, *pWorldToProjection, view);
  }
};

static CVRenderView s_RenderView;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CVRenderView, IVRenderView,
                                  VENGINE_RENDERVIEW_INTERFACE_VERSION,
                                  s_RenderView);
