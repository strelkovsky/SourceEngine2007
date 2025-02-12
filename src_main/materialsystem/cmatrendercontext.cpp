// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "pch_materialsystem.h"

#define MATSYS_INTERNAL

#include "cmatrendercontext.h"

#include <cmath>
#include "IHardwareConfigInternal.h"
#include "cmaterialsystem.h"
#include "ctype.h"
#include "occlusionquerymgr.h"
#include "texturemanager.h"
#include "tier1/fmtstr.h"
#include "tier2/renderutils.h"

// NOTE: This must be the last file included!!!
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------

// TODO(d.rattman): right now, always keeping shader API in sync, because debug
// overlays don't seem to work 100% with the delayed matrix loading
#define FORCE_MATRIX_SYNC 1

#ifdef VALIDATE_MATRICES
#define ShouldValidateMatrices() true
#else
#define ShouldValidateMatrices() false
#endif

#ifdef VALIDATE_MATRICES
#define AllowLazyMatrixSync() false
#define ForceSync() ((void)(0))
#elif defined(FORCE_MATRIX_SYNC)
#define AllowLazyMatrixSync() false
#define ForceSync() ForceSyncMatrix(m_MatrixMode)
#else
#define AllowLazyMatrixSync() true
#define ForceSync() ((void)(0))
#endif

void ValidateMatrices(const VMatrix &m1, const VMatrix &m2, float eps = .001) {
  if (!ShouldValidateMatrices()) return;

  for (int i = 0; i < 16; i++) {
    AssertFloatEquals(m1.Base()[i], m1.Base()[i], eps);
  }
}

void SpinPresent() {
  while (true) {
    g_pShaderAPI->ClearColor3ub(0, 0, 0);
    g_pShaderAPI->ClearBuffers(true, true, true, -1, -1);
    g_pShaderDevice->Present();
  }
}

void ReportDirtyDisk() {}

//-----------------------------------------------------------------------------
// Install dirty disk error reporting function (call after SetMode)
//-----------------------------------------------------------------------------
void SetupDirtyDiskReportFunc() {
  g_pFullFileSystem->InstallDirtyDiskReportFunc(ReportDirtyDisk);
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMatRenderContextBase::CMatRenderContextBase()
    : m_pMaterialSystem(NULL),
      m_RenderTargetStack(16, 32),
      m_MatrixMode(NUM_MATRIX_MODES) {
  int i;

  m_bDirtyViewState = true;

  // Put a special element at the top of the RT stack (indicating back buffer is
  // current top of stack) NULL indicates back buffer, -1 indicates full-size
  // viewport
  RenderTargetStackElement_t initialElement = {
      {NULL, NULL, NULL, NULL}, 0, 0, -1, -1};

  m_RenderTargetStack.Push(initialElement);

  for (i = 0; i < MAX_FB_TEXTURES; i++) {
    m_pCurrentFrameBufferCopyTexture[i] = NULL;
  }

  m_pCurrentMaterial = NULL;
  m_pCurrentProxyData = NULL;
  m_pUserDefinedLightmap = NULL;
  m_HeightClipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
  m_HeightClipZ = 0.0f;
  m_bEnableClipping = true;
  m_bFlashlightEnable = false;
  m_bFullFrameDepthIsValid = false;

  for (i = 0; i < NUM_MATRIX_MODES; i++) {
    m_MatrixStacks[i].Push();
    m_MatrixStacks[i].Top().matrix.Identity();
    m_MatrixStacks[i].Top().flags |= (MSF_DIRTY | MSF_IDENTITY);
  }
  m_pCurMatrixItem = &m_MatrixStacks[0].Top();

  m_Viewport.Init(0, 0, 0, 0);

  m_LastSetToneMapScale = Vector(1, 1, 1);
  m_CurToneMapScale = 1.0;
  m_GoalToneMapScale = 1.0f;
}

  //-----------------------------------------------------------------------------
  //
  //-----------------------------------------------------------------------------

#define g_pShaderAPI Cannot_use_ShaderAPI_in_CMatRenderContextBase

void CMatRenderContextBase::InitializeFrom(
    CMatRenderContextBase *pInitialState) {
  int i;

  m_pCurrentMaterial = pInitialState->m_pCurrentMaterial;
  m_pCurrentProxyData = pInitialState->m_pCurrentProxyData;
  m_lightmapPageID = pInitialState->m_lightmapPageID;
  m_pUserDefinedLightmap = pInitialState->m_pUserDefinedLightmap;
  m_pLocalCubemapTexture = pInitialState->m_pLocalCubemapTexture;

  memcpy(m_pCurrentFrameBufferCopyTexture,
         pInitialState->m_pCurrentFrameBufferCopyTexture,
         MAX_FB_TEXTURES * sizeof(ITexture *));

  m_bEnableClipping = pInitialState->m_bEnableClipping;

  m_HeightClipMode = pInitialState->m_HeightClipMode;
  m_HeightClipZ = pInitialState->m_HeightClipZ;

  m_pBoundMorph = pInitialState->m_pBoundMorph;  // not reference counted?

  m_RenderTargetStack.Clear();
  m_RenderTargetStack.EnsureCapacity(
      pInitialState->m_RenderTargetStack.Count());

  for (i = 0; i < pInitialState->m_RenderTargetStack.Count(); i++) {
    m_RenderTargetStack.Push(pInitialState->m_RenderTargetStack[i]);
  }

  m_MatrixMode = pInitialState->m_MatrixMode;
  for (i = 0; i < NUM_MATRIX_MODES; i++) {
    m_MatrixStacks[i].CopyFrom(pInitialState->m_MatrixStacks[i]);
  }

  m_bFlashlightEnable = pInitialState->m_bFlashlightEnable;

  m_CurToneMapScale = pInitialState->m_CurToneMapScale;
  m_LastSetToneMapScale = pInitialState->m_LastSetToneMapScale;
}

void CMatRenderContextBase::Bind(IMaterial *iMaterial, void *proxyData) {
  IMaterialInternal *material = static_cast<IMaterialInternal *>(iMaterial);

  if (!material) {
    Warning("Programming error: CMatRenderContext::Bind: NULL material\n");
    material = static_cast<IMaterialInternal *>(g_pErrorMaterial);
  }
  material =
      material->GetRealTimeVersion();  // always work with the real time
                                       // versions of materials internally

  if (GetCurrentMaterialInternal() != material) {
    if (!material->IsPrecached()) {
      DevWarning(
          "Binding uncached material \"%s\", artificially incrementing "
          "refcount\n",
          material->GetName());
      material->ArtificialAddRef();
      material->Precache();
    }
    SetCurrentMaterialInternal(material);
  }

  SetCurrentProxy(proxyData);
}

void CMatRenderContextBase::BindLightmapPage(int lightmapPageID) {
  m_lightmapPageID = lightmapPageID;
}

void CMatRenderContextBase::SetRenderTargetEx(int nRenderTargetID,
                                              ITexture *pNewTarget) {
  // Verify valid top of RT stack
  Assert(m_RenderTargetStack.Count() > 0);

  // Reset the top of stack to the new target with old viewport
  RenderTargetStackElement_t newTOS = m_RenderTargetStack.Top();
  newTOS.m_pRenderTargets[nRenderTargetID] = pNewTarget;
  m_RenderTargetStack.Pop();
  m_RenderTargetStack.Push(newTOS);
}

void CMatRenderContextBase::BindLocalCubemap(ITexture *pTexture) {
  if (pTexture) {
    m_pLocalCubemapTexture = pTexture;
  } else {
    m_pLocalCubemapTexture = TextureManager()->ErrorTexture();
  }
}

ITexture *CMatRenderContextBase::GetRenderTarget() {
  if (m_RenderTargetStack.Count() > 0) {
    return m_RenderTargetStack.Top().m_pRenderTargets[0];
  } else {
    return NULL;  // should this be something else, since NULL indicates back
                  // buffer?
  }
}

ITexture *CMatRenderContextBase::GetRenderTargetEx(int nRenderTargetID) {
  // Verify valid top of stack
  Assert(m_RenderTargetStack.Count() > 0);

  // Top of render target stack
  ITexture *pTexture =
      m_RenderTargetStack.Top().m_pRenderTargets[nRenderTargetID];
  return pTexture;
}

void CMatRenderContextBase::SetFrameBufferCopyTexture(ITexture *pTexture,
                                                      int textureIndex) {
  if (textureIndex < 0 || textureIndex >= MAX_FB_TEXTURES) {
    Assert(0);
    return;
  }

  // TODO(d.rattman): Do I need to increment/decrement ref counts here, or
  // assume that the app is going to do it?
  m_pCurrentFrameBufferCopyTexture[textureIndex] = pTexture;
}

ITexture *CMatRenderContextBase::GetFrameBufferCopyTexture(int textureIndex) {
  if (textureIndex < 0 || textureIndex >= MAX_FB_TEXTURES) {
    Assert(0);
    return NULL;  // TODO(d.rattman): This should return the error texture.
  }
  return m_pCurrentFrameBufferCopyTexture[textureIndex];
}

void CMatRenderContextBase::MatrixMode(MaterialMatrixMode_t mode) {
  Assert(m_MatrixStacks[mode].Count());
  m_MatrixMode = mode;
  m_pCurMatrixItem = &m_MatrixStacks[mode].Top();
}

void CMatRenderContextBase::CurrentMatrixChanged() {
  if (m_MatrixMode == MATERIAL_VIEW) {
    m_bDirtyViewState = true;
    m_bDirtyViewProjState = true;
  } else if (m_MatrixMode == MATERIAL_PROJECTION) {
    m_bDirtyViewProjState = true;
  }
}

void CMatRenderContextBase::PushMatrix() {
  CUtlStack<MatrixStackItem_t> &curStack = m_MatrixStacks[m_MatrixMode];
  Assert(curStack.Count());
  int iNew = curStack.Push();
  curStack[iNew] = curStack[iNew - 1];
  m_pCurMatrixItem = &curStack.Top();
  CurrentMatrixChanged();
}

void CMatRenderContextBase::PopMatrix() {
  Assert(m_MatrixStacks[m_MatrixMode].Count() > 1);
  m_MatrixStacks[m_MatrixMode].Pop();
  m_pCurMatrixItem = &m_MatrixStacks[m_MatrixMode].Top();
  CurrentMatrixChanged();
}

void CMatRenderContextBase::LoadMatrix(const VMatrix &matrix) {
  m_pCurMatrixItem->matrix = matrix;
  m_pCurMatrixItem->flags = MSF_DIRTY;  // clearing identity implicitly
  CurrentMatrixChanged();
}

void CMatRenderContextBase::LoadMatrix(const matrix3x4_t &matrix) {
  m_pCurMatrixItem->matrix = matrix;
  m_pCurMatrixItem->flags = MSF_DIRTY;  // clearing identity implicitly
  CurrentMatrixChanged();
}

void CMatRenderContextBase::MultMatrix(const VMatrix &matrix) {
  VMatrix result;

  MatrixMultiply(matrix, m_pCurMatrixItem->matrix, result);
  m_pCurMatrixItem->matrix = result;
  m_pCurMatrixItem->flags = MSF_DIRTY;  // clearing identity implicitly
  CurrentMatrixChanged();
}

void CMatRenderContextBase::MultMatrix(const matrix3x4_t &matrix) {
  CMatRenderContextBase::MultMatrix(VMatrix(matrix));
}

void CMatRenderContextBase::MultMatrixLocal(const VMatrix &matrix) {
  VMatrix result;
  MatrixMultiply(m_pCurMatrixItem->matrix, matrix, result);
  m_pCurMatrixItem->matrix = result;
  m_pCurMatrixItem->flags = MSF_DIRTY;  // clearing identity implicitly
  CurrentMatrixChanged();
}

void CMatRenderContextBase::MultMatrixLocal(const matrix3x4_t &matrix) {
  CMatRenderContextBase::MultMatrixLocal(VMatrix(matrix));
}

void CMatRenderContextBase::LoadIdentity() {
  // TODO(d.rattman): possibly track is identity so can call shader API
  // LoadIdentity() later instead of LoadMatrix()?
  m_pCurMatrixItem->matrix.Identity();
  m_pCurMatrixItem->flags = (MSF_DIRTY | MSF_IDENTITY);
  CurrentMatrixChanged();
}

void CMatRenderContextBase::Ortho(double left, double top, double right,
                                  double bottom, double zNear, double zFar) {
  MatrixOrtho(m_pCurMatrixItem->matrix, left, top, right, bottom, zNear, zFar);
  m_pCurMatrixItem->flags = MSF_DIRTY;
}

void CMatRenderContextBase::PerspectiveX(double flFovX, double flAspect,
                                         double flZNear, double flZFar) {
  MatrixPerspectiveX(m_pCurMatrixItem->matrix, flFovX, flAspect, flZNear,
                     flZFar);
  m_pCurMatrixItem->flags = MSF_DIRTY;
}

void CMatRenderContextBase::PerspectiveOffCenterX(double flFovX,
                                                  double flAspect,
                                                  double flZNear, double flZFar,
                                                  double bottom, double top,
                                                  double left, double right) {
  MatrixPerspectiveOffCenterX(m_pCurMatrixItem->matrix, flFovX, flAspect,
                              flZNear, flZFar, bottom, top, left, right);
  m_pCurMatrixItem->flags = MSF_DIRTY;
}

void CMatRenderContextBase::PickMatrix(int x, int y, int nWidth, int nHeight) {
  int vx, vy, vwidth, vheight;
  GetViewport(vx, vy, vwidth, vheight);

  // Compute the location of the pick region in projection space...
  float px = 2.0 * (float)(x - vx) / (float)vwidth - 1;
  float py = 2.0 * (float)(y - vy) / (float)vheight - 1;
  float pw = 2.0 * (float)nWidth / (float)vwidth;
  float ph = 2.0 * (float)nHeight / (float)vheight;

  // we need to translate (px, py) to the origin
  // and scale so (pw,ph) -> (2, 2)
  VMatrix mat;
  MatrixSetIdentity(mat);
  mat.m[0][0] = 2.0 / pw;
  mat.m[1][1] = 2.0 / ph;
  mat.m[0][3] = -2.0 * px / pw;
  mat.m[1][3] = -2.0 * py / ph;

  CMatRenderContextBase::MultMatrixLocal(mat);
}

void CMatRenderContextBase::Rotate(float flAngle, float x, float y, float z) {
  MatrixRotate(m_pCurMatrixItem->matrix, Vector(x, y, z), flAngle);
  m_pCurMatrixItem->flags = MSF_DIRTY;
}

void CMatRenderContextBase::Translate(float x, float y, float z) {
  MatrixTranslate(m_pCurMatrixItem->matrix, Vector(x, y, z));
  m_pCurMatrixItem->flags = MSF_DIRTY;
}

void CMatRenderContextBase::Scale(float x, float y, float z) {
  VMatrix mat;
  MatrixBuildScale(mat, x, y, z);
  CMatRenderContextBase::MultMatrixLocal(mat);
}

void CMatRenderContextBase::GetMatrix(MaterialMatrixMode_t matrixMode,
                                      VMatrix *pMatrix) {
  CUtlStack<MatrixStackItem_t> &stack = m_MatrixStacks[matrixMode];

  if (!stack.Count()) {
    pMatrix->Identity();
    return;
  }

  *pMatrix = stack.Top().matrix;
}

void CMatRenderContextBase::GetMatrix(MaterialMatrixMode_t matrixMode,
                                      matrix3x4_t *pMatrix) {
  CUtlStack<MatrixStackItem_t> &stack = m_MatrixStacks[matrixMode];

  if (!stack.Count()) {
    SetIdentityMatrix(*pMatrix);
    return;
  }

  *pMatrix = stack.Top().matrix.As3x4();
}

void CMatRenderContextBase::RecomputeViewState() {
  if (!m_bDirtyViewState) return;
  m_bDirtyViewState = false;

  // TODO(d.rattman): Cache this off to make it less expensive?
  matrix3x4_t viewMatrix;
  GetMatrix(MATERIAL_VIEW, &viewMatrix);
  m_vecViewOrigin.x = -(viewMatrix[0][3] * viewMatrix[0][0] +
                        viewMatrix[1][3] * viewMatrix[1][0] +
                        viewMatrix[2][3] * viewMatrix[2][0]);
  m_vecViewOrigin.y = -(viewMatrix[0][3] * viewMatrix[0][1] +
                        viewMatrix[1][3] * viewMatrix[1][1] +
                        viewMatrix[2][3] * viewMatrix[2][1]);
  m_vecViewOrigin.z = -(viewMatrix[0][3] * viewMatrix[0][2] +
                        viewMatrix[1][3] * viewMatrix[1][2] +
                        viewMatrix[2][3] * viewMatrix[2][2]);

  // TODO(d.rattman): Implement computation of m_vecViewForward, etc
  m_vecViewForward.Init();
  m_vecViewRight.Init();

  // TODO(d.rattman): Is this correct?
  m_vecViewUp.Init(viewMatrix[1][0], viewMatrix[1][1], viewMatrix[1][2]);
}

void CMatRenderContextBase::GetWorldSpaceCameraPosition(Vector *pCameraPos) {
  RecomputeViewState();
  VectorCopy(m_vecViewOrigin, *pCameraPos);
}

void CMatRenderContextBase::GetWorldSpaceCameraVectors(Vector *pVecForward,
                                                       Vector *pVecRight,
                                                       Vector *pVecUp) {
  RecomputeViewState();

  // TODO(d.rattman): Implement computation of m_vecViewForward
  Assert(0);

  if (pVecForward) {
    VectorCopy(m_vecViewForward, *pVecForward);
  }
  if (pVecRight) {
    VectorCopy(m_vecViewRight, *pVecRight);
  }
  if (pVecUp) {
    VectorCopy(m_vecViewUp, *pVecUp);
  }
}

void CMatRenderContextBase::SyncMatrices() {}

void CMatRenderContextBase::SyncMatrix(MaterialMatrixMode_t mode) {}

void CMatRenderContextBase::SetHeightClipMode(
    enum MaterialHeightClipMode_t heightClipMode) {
  if (m_HeightClipMode != heightClipMode) {
    m_HeightClipMode = heightClipMode;
    UpdateHeightClipUserClipPlane();
    /*if ( HardwareConfig()->MaxUserClipPlanes() >= 1 &&
    !HardwareConfig()->UseFastClipping())
    {
    UpdateHeightClipUserClipPlane();
    }
    else
    {
    g_pShaderAPI->SetHeightClipMode( heightClipMode );
    }*/
  }
}

void CMatRenderContextBase::SetHeightClipZ(float z) {
  if (z != m_HeightClipZ) {
    m_HeightClipZ = z;
    UpdateHeightClipUserClipPlane();
  }
}

bool CMatRenderContextBase::EnableClipping(bool bEnable) {
  if (bEnable != m_bEnableClipping) {
    m_bEnableClipping = bEnable;
    ApplyCustomClipPlanes();

    return !bEnable;
  }
  return bEnable;
}

void CMatRenderContextBase::Viewport(int x, int y, int width, int height) {
  // Verify valid top of RT stack
  Assert(m_RenderTargetStack.Count() > 0);

  // Reset the top of stack to the new viewport
  RenderTargetStackElement_t newTOS;
  memcpy(&newTOS, &(m_RenderTargetStack.Top()), sizeof(newTOS));
  newTOS.m_nViewX = x;
  newTOS.m_nViewY = y;
  newTOS.m_nViewW = width;
  newTOS.m_nViewH = height;

  m_RenderTargetStack.Pop();
  m_RenderTargetStack.Push(newTOS);
}

//-----------------------------------------------------------------------------
// This version will push the current rendertarget + current viewport onto the
// stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport() {
  // Necessary to push the stack top onto itself; realloc could happen otherwise
  m_RenderTargetStack.EnsureCapacity(m_RenderTargetStack.Count() + 1);
  m_RenderTargetStack.Push(m_RenderTargetStack.Top());
  CommitRenderTargetAndViewport();
}

//-----------------------------------------------------------------------------
// Pushes a render target on the render target stack.  Without a specific
// viewport also being pushed, this function uses dummy values which indicate
// that the viewport should span the the full render target and pushes
// the RenderTargetStackElement_t onto the stack
//
// The push and pop methods also implicitly set the render target to the new top
// of stack
//
// NULL for pTexture indicates rendering to the back buffer
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport(ITexture *pTexture) {
  // Just blindly push the data on the stack with flags indicating full bounds
  RenderTargetStackElement_t element = {
      {pTexture, NULL, NULL, NULL}, 0, 0, -1, -1};
  m_RenderTargetStack.Push(element);
  CommitRenderTargetAndViewport();
}

//-----------------------------------------------------------------------------
// Pushes a render target on the render target stack and sets the viewport
//
// NULL for pTexture indicates rendering to the back buffer
//
// The push and pop methods also implicitly set the render target to the new top
// of stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport(ITexture *pTexture,
                                                        int nViewX, int nViewY,
                                                        int nViewW,
                                                        int nViewH) {
  CMatRenderContextBase::PushRenderTargetAndViewport(pTexture, NULL, nViewX,
                                                     nViewY, nViewW, nViewH);
}

//-----------------------------------------------------------------------------
// Pushes a render target on the render target stack and sets the viewport
// The push and pop methods also implicitly set the render target to the new top
// of stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport(ITexture *pTexture,
                                                        ITexture *pDepthTexture,
                                                        int nViewX, int nViewY,
                                                        int nViewW,
                                                        int nViewH) {
  // Just blindly push the data on the stack
  RenderTargetStackElement_t element = {{pTexture, NULL, NULL, NULL},
                                        pDepthTexture,
                                        nViewX,
                                        nViewY,
                                        nViewW,
                                        nViewH};
  m_RenderTargetStack.Push(element);
  CommitRenderTargetAndViewport();
}

//-----------------------------------------------------------------------------
// Pops from the render target stack
// Also implicitly sets the render target to the new top of stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PopRenderTargetAndViewport() {
  // Check for underflow
  if (m_RenderTargetStack.Count() == 0) {
    Assert(
        !"CMatRenderContext::PopRenderTargetAndViewport:  Stack is empty!!!");
    return;
  }

  // Remove the top of stack
  m_RenderTargetStack.Pop();
  CommitRenderTargetAndViewport();
}

void CMatRenderContextBase::RecomputeViewProjState() {
  if (m_bDirtyViewProjState) {
    VMatrix viewMatrix, projMatrix;

    // TODO(d.rattman): Should consider caching this upon change for projection
    // or view matrix.
    GetMatrix(MATERIAL_VIEW, &viewMatrix);
    GetMatrix(MATERIAL_PROJECTION, &projMatrix);
    m_viewProjMatrix = projMatrix * viewMatrix;
    m_bDirtyViewProjState = false;
  }
}

//-----------------------------------------------------------------------------
// This returns the diameter of the sphere in pixels based on
// the current model, view, + projection matrices and viewport.
//-----------------------------------------------------------------------------
float CMatRenderContextBase::ComputePixelDiameterOfSphere(
    const Vector &vecAbsOrigin, float flRadius) {
  RecomputeViewState();
  RecomputeViewProjState();
  // This is sort of faked, but it's faster that way
  // TODO(d.rattman): Also, there's a much faster way to do this with similar
  // triangles but I want to make sure it exactly matches the current matrices,
  // so for now, I do it this conservative way
  Vector4D testPoint1, testPoint2;
  VectorMA(vecAbsOrigin, flRadius, m_vecViewUp, testPoint1.AsVector3D());
  VectorMA(vecAbsOrigin, -flRadius, m_vecViewUp, testPoint2.AsVector3D());
  testPoint1.w = testPoint2.w = 1.0f;

  Vector4D clipPos1, clipPos2;
  Vector4DMultiply(m_viewProjMatrix, testPoint1, clipPos1);
  Vector4DMultiply(m_viewProjMatrix, testPoint2, clipPos2);
  if (clipPos1.w >= 0.001f) {
    clipPos1.y /= clipPos1.w;
  } else {
    clipPos1.y *= 1000;
  }
  if (clipPos2.w >= 0.001f) {
    clipPos2.y /= clipPos2.w;
  } else {
    clipPos2.y *= 1000;
  }
  int vx, vy, vwidth, vheight;
  GetViewport(vx, vy, vwidth, vheight);

  // The divide-by-two here is because y goes from -1 to 1 in projection space
  return vheight * fabs(clipPos2.y - clipPos1.y) / 2.0f;
}

ConVar mat_accelerate_adjust_exposure_down(
    "mat_accelerate_adjust_exposure_down", "3.0", FCVAR_CHEAT);
ConVar mat_hdr_manual_tonemap_rate("mat_hdr_manual_tonemap_rate", "1.0");
ConVar mat_hdr_tonemapscale("mat_hdr_tonemapscale", "1.0", FCVAR_CHEAT);
ConVar mat_tonemap_algorithm("mat_tonemap_algorithm", "1", FCVAR_CHEAT,
                             "0 = Original Algorithm 1 = New Algorithm");

void CMatRenderContextBase::TurnOnToneMapping() {
  if ((HardwareConfig()->GetHDRType() != HDR_TYPE_NONE) &&
      (m_FrameTime > 0.0f)) {
    float elapsed_time = m_FrameTime;
    float goalScale = m_GoalToneMapScale;
    float rate = mat_hdr_manual_tonemap_rate.GetFloat();

    if (mat_tonemap_algorithm.GetInt() == 1) {
      rate *= 2.0f;  // Default 2x for the new tone mapping algorithm so it
                     // feels the same as the original
    }

    if (rate == 0.0f)  // Zero indicates instantaneous tonemap scaling
    {
      m_CurToneMapScale = goalScale;
    } else {
      if (goalScale < m_CurToneMapScale) {
        float acc_exposure_adjust =
            mat_accelerate_adjust_exposure_down.GetFloat();

        // Adjust at up to 4x rate when over-exposed.
        rate = std::min((acc_exposure_adjust * rate),
                        FLerp(rate, (acc_exposure_adjust * rate), 0.0f, 1.5f,
                              (m_CurToneMapScale - goalScale)));
      }

      float flRateTimesTime = rate * elapsed_time;
      if (mat_tonemap_algorithm.GetInt() == 1) {
        // For the new tone mapping algorithm, limit the rate based on the
        // number of bins to help reduce the tone map scalar "riding the wave"
        // of the histogram re-building

        // Warning( "flRateTimesTime = %.4f", flRateTimesTime );
        flRateTimesTime = std::min(
            flRateTimesTime, (1.0f / 16.0f) * 0.25f);  // 16 is number of HDR
                                                       // sample bins defined in
                                                       // viewpostprocess.cpp
        // Warning( " --> %.4f\n", flRateTimesTime );
      }

      float alpha = std::max(0.0f, std::min(1.0f, flRateTimesTime));
      m_CurToneMapScale =
          (goalScale * alpha) + (m_CurToneMapScale * (1.0f - alpha));

      if (!IsFinite(m_CurToneMapScale)) {
        Assert(0);
        m_CurToneMapScale = goalScale;
      }
    }

    SetToneMappingScaleLinear(
        Vector(m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale));
    m_LastSetToneMapScale =
        Vector(m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale);
  }
}

void CMatRenderContextBase::ResetToneMappingScale(float sc) {
  m_CurToneMapScale = sc;
  SetToneMappingScaleLinear(
      Vector(m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale));
  m_LastSetToneMapScale =
      Vector(m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale);
  // mat_hdr_tonemapscale.SetValue(1.0f);
  m_GoalToneMapScale = 1;
}

void CMatRenderContextBase::SetGoalToneMappingScale(float monoscale) {
  Assert(IsFinite(monoscale));
  if (IsFinite(monoscale)) {
    m_GoalToneMapScale = monoscale;
  }
}

Vector CMatRenderContextBase::GetToneMappingScaleLinear() {
  if (HardwareConfig()->GetHDRType() == HDR_TYPE_NONE) return Vector(1, 1, 1);

  return m_LastSetToneMapScale;
}

#undef g_pShaderAPI

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CMatRenderContext::CMatRenderContext() {
  m_pBatchIndices = NULL;
  m_pBatchMesh = NULL;
  m_pCurrentIndexBuffer = NULL;
  m_pMorphRenderContext = NULL;
  m_NonInteractiveMode = MATERIAL_NON_INTERACTIVE_MODE_NONE;
}

InitReturnVal_t CMatRenderContext::Init(CMaterialSystem *pMaterialSystem) {
  m_pMaterialSystem = pMaterialSystem;

  m_pBoundMorph = NULL;

  // Create some lovely textures
  m_pLocalCubemapTexture = TextureManager()->ErrorTexture();
  m_pMorphRenderContext = g_pMorphMgr->AllocateRenderContext();

  return INIT_OK;
}

void CMatRenderContext::Shutdown() {
  if (m_pUserDefinedLightmap) {
    m_pUserDefinedLightmap->DecrementReferenceCount();
    m_pUserDefinedLightmap = NULL;
  }

  if (m_pMorphRenderContext) {
    g_pMorphMgr->FreeRenderContext(m_pMorphRenderContext);
    m_pMorphRenderContext = NULL;
  }
}

void CMatRenderContext::OnReleaseShaderObjects() {
  // alt-tab unbinds the morph
  m_pBoundMorph = NULL;
}

inline IMaterialInternal *CMatRenderContext::GetMaterialInternal(
    MaterialHandle_t h) const {
  return GetMaterialSystem()->GetMaterialInternal(h);
}

inline IMaterialInternal *CMatRenderContext::GetDrawFlatMaterial() {
  return GetMaterialSystem()->GetDrawFlatMaterial();
}

inline IMaterialInternal *CMatRenderContext::GetBufferClearObeyStencil(int i) {
  return GetMaterialSystem()->GetBufferClearObeyStencil(i);
}

inline ShaderAPITextureHandle_t
CMatRenderContext::GetFullbrightLightmapTextureHandle() const {
  return GetMaterialSystem()->GetFullbrightLightmapTextureHandle();
}

inline ShaderAPITextureHandle_t
CMatRenderContext::GetFullbrightBumpedLightmapTextureHandle() const {
  return GetMaterialSystem()->GetFullbrightBumpedLightmapTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetBlackTextureHandle()
    const {
  return GetMaterialSystem()->GetBlackTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetFlatNormalTextureHandle()
    const {
  return GetMaterialSystem()->GetFlatNormalTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetGreyTextureHandle()
    const {
  return GetMaterialSystem()->GetGreyTextureHandle();
}

inline ShaderAPITextureHandle_t
CMatRenderContext::GetGreyAlphaZeroTextureHandle() const {
  return GetMaterialSystem()->GetGreyAlphaZeroTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetWhiteTextureHandle()
    const {
  return GetMaterialSystem()->GetWhiteTextureHandle();
}

inline const CMatLightmaps *CMatRenderContext::GetLightmaps() const {
  return GetMaterialSystem()->GetLightmaps();
}

inline CMatLightmaps *CMatRenderContext::GetLightmaps() {
  return GetMaterialSystem()->GetLightmaps();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetMaxDepthTextureHandle()
    const {
  return GetMaterialSystem()->GetMaxDepthTextureHandle();
}

void CMatRenderContext::BeginRender() { g_MatSysMutex.Lock(); }

void CMatRenderContext::EndRender() { g_MatSysMutex.Unlock(); }

void CMatRenderContext::Flush(bool flushHardware) {
  VPROF("CMatRenderContextBase::Flush");

  g_pShaderAPI->FlushBufferedPrimitives();
  if (flushHardware) {
    g_pShaderAPI->FlushBufferedPrimitives();
  }
}

bool CMatRenderContext::TestMatrixSync(MaterialMatrixMode_t mode) {
  if (!ShouldValidateMatrices()) {
    return true;
  }

  VMatrix transposeMatrix, matrix;
  g_pShaderAPI->GetMatrix(mode, (float *)transposeMatrix.m);
  MatrixTranspose(transposeMatrix, matrix);

  ValidateMatrices(matrix, m_MatrixStacks[mode].Top().matrix);

  return true;
}

void CMatRenderContext::MatrixMode(MaterialMatrixMode_t mode) {
  CMatRenderContextBase::MatrixMode(mode);
  g_pShaderAPI->MatrixMode(mode);
  if (ShouldValidateMatrices()) {
    TestMatrixSync(mode);
  }
}
void CMatRenderContext::PushMatrix() {
  if (ShouldValidateMatrices()) {
    TestMatrixSync(m_MatrixMode);
  }

  CMatRenderContextBase::PushMatrix();
  g_pShaderAPI->PushMatrix();

  if (ShouldValidateMatrices()) {
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::PopMatrix() {
  if (ShouldValidateMatrices()) {
    TestMatrixSync(m_MatrixMode);
  }

  CMatRenderContextBase::PopMatrix();
  g_pShaderAPI->PopMatrix();

  if (ShouldValidateMatrices()) {
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::LoadMatrix(const VMatrix &matrix) {
  CMatRenderContextBase::LoadMatrix(matrix);
  ForceSync();
  if (ShouldValidateMatrices()) {
    VMatrix transposeMatrix;
    MatrixTranspose(matrix, transposeMatrix);
    g_pShaderAPI->LoadMatrix(transposeMatrix.Base());
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::LoadMatrix(const matrix3x4_t &matrix) {
  CMatRenderContextBase::LoadMatrix(matrix);
  ForceSync();
  if (ShouldValidateMatrices()) {
    VMatrix transposeMatrix;
    MatrixTranspose(VMatrix(matrix), transposeMatrix);
    g_pShaderAPI->LoadMatrix(transposeMatrix.Base());
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::MultMatrix(const VMatrix &matrix) {
  CMatRenderContextBase::MultMatrix(matrix);
  ForceSync();
  if (ShouldValidateMatrices()) {
    VMatrix transposeMatrix;
    MatrixTranspose(matrix, transposeMatrix);
    g_pShaderAPI->MultMatrix(transposeMatrix.Base());
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::MultMatrix(const matrix3x4_t &matrix) {
  CMatRenderContextBase::MultMatrix(VMatrix(matrix));
  ForceSync();
  if (ShouldValidateMatrices()) {
    VMatrix transposeMatrix;
    MatrixTranspose(VMatrix(matrix), transposeMatrix);
    g_pShaderAPI->MultMatrix(transposeMatrix.Base());
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::MultMatrixLocal(const VMatrix &matrix) {
  CMatRenderContextBase::MultMatrixLocal(matrix);
  ForceSync();
  if (ShouldValidateMatrices()) {
    VMatrix transposeMatrix;
    MatrixTranspose(matrix, transposeMatrix);
    g_pShaderAPI->MultMatrixLocal(transposeMatrix.Base());
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::MultMatrixLocal(const matrix3x4_t &matrix) {
  CMatRenderContextBase::MultMatrixLocal(VMatrix(matrix));
  ForceSync();
  if (ShouldValidateMatrices()) {
    VMatrix transposeMatrix;
    MatrixTranspose(VMatrix(matrix), transposeMatrix);
    g_pShaderAPI->MultMatrixLocal(transposeMatrix.Base());
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::LoadIdentity() {
  CMatRenderContextBase::LoadIdentity();
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->LoadIdentity();
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::Ortho(double left, double top, double right,
                              double bottom, double zNear, double zFar) {
  CMatRenderContextBase::Ortho(left, top, right, bottom, zNear, zFar);
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->Ortho(left, top, right, bottom, zNear, zFar);
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::PerspectiveX(double flFovX, double flAspect,
                                     double flZNear, double flZFar) {
  CMatRenderContextBase::PerspectiveX(flFovX, flAspect, flZNear, flZFar);
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->PerspectiveX(flFovX, flAspect, flZNear, flZFar);
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::PerspectiveOffCenterX(double flFovX, double flAspect,
                                              double flZNear, double flZFar,
                                              double bottom, double top,
                                              double left, double right) {
  CMatRenderContextBase::PerspectiveOffCenterX(
      flFovX, flAspect, flZNear, flZFar, bottom, top, left, right);
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->PerspectiveOffCenterX(flFovX, flAspect, flZNear, flZFar,
                                        bottom, top, left, right);
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::PickMatrix(int x, int y, int nWidth, int nHeight) {
  CMatRenderContextBase::PickMatrix(x, y, nWidth, nHeight);
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->PickMatrix(x, y, nWidth, nHeight);
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::Rotate(float flAngle, float x, float y, float z) {
  CMatRenderContextBase::Rotate(flAngle, x, y, z);
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->Rotate(flAngle, x, y, z);
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::Translate(float x, float y, float z) {
  CMatRenderContextBase::Translate(x, y, z);
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->Translate(x, y, z);
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::Scale(float x, float y, float z) {
  CMatRenderContextBase::Scale(x, y, z);
  ForceSync();
  if (ShouldValidateMatrices()) {
    g_pShaderAPI->Scale(x, y, z);
    TestMatrixSync(m_MatrixMode);
  }
}

void CMatRenderContext::GetMatrix(MaterialMatrixMode_t matrixMode,
                                  VMatrix *pMatrix) {
  CMatRenderContextBase::GetMatrix(matrixMode, pMatrix);

  ForceSync();
  if (ShouldValidateMatrices()) {
    VMatrix testMatrix;
    VMatrix transposeMatrix;
    g_pShaderAPI->GetMatrix(matrixMode, (float *)transposeMatrix.m);
    MatrixTranspose(transposeMatrix, testMatrix);

    ValidateMatrices(testMatrix, *pMatrix);
  }
}

void CMatRenderContext::GetMatrix(MaterialMatrixMode_t matrixMode,
                                  matrix3x4_t *pMatrix) {
  if (!ShouldValidateMatrices()) {
    CMatRenderContextBase::GetMatrix(matrixMode, pMatrix);
  } else {
    VMatrix matrix;
    CMatRenderContext::GetMatrix(matrixMode, &matrix);
    *pMatrix = matrix.As3x4();
  }
}

void CMatRenderContext::SyncMatrices() {
  if constexpr (!ShouldValidateMatrices() && AllowLazyMatrixSync()) {
    for (int i = 0; i < NUM_MATRIX_MODES; i++) {
      MatrixStackItem_t &top = m_MatrixStacks[i].Top();
      if (top.flags & MSF_DIRTY) {
        g_pShaderAPI->MatrixMode((MaterialMatrixMode_t)i);
        if (!(top.flags & MSF_IDENTITY)) {
          VMatrix transposeTop;
          MatrixTranspose(top.matrix, transposeTop);
          g_pShaderAPI->LoadMatrix(transposeTop.Base());
        } else {
          g_pShaderAPI->LoadIdentity();
        }

        top.flags &= ~MSF_DIRTY;
      }
    }
  }
}

void CMatRenderContext::ForceSyncMatrix(MaterialMatrixMode_t mode) {
  MatrixStackItem_t &top = m_MatrixStacks[mode].Top();
  if (top.flags & MSF_DIRTY) {
    bool bSetMode = (m_MatrixMode != mode);
    if (bSetMode) {
      g_pShaderAPI->MatrixMode((MaterialMatrixMode_t)mode);
    }

    if (!(top.flags & MSF_IDENTITY)) {
      VMatrix transposeTop;
      MatrixTranspose(top.matrix, transposeTop);
      g_pShaderAPI->LoadMatrix(transposeTop.Base());
    } else {
      g_pShaderAPI->LoadIdentity();
    }

    if (bSetMode) {
      g_pShaderAPI->MatrixMode((MaterialMatrixMode_t)mode);
    }

    top.flags &= ~MSF_DIRTY;
  }
}

void CMatRenderContext::SyncMatrix(MaterialMatrixMode_t mode) {
  if constexpr (!ShouldValidateMatrices() && AllowLazyMatrixSync()) {
    ForceSyncMatrix(mode);
  }
}

//-----------------------------------------------------------------------------
// Swap buffers
//-----------------------------------------------------------------------------
void CMatRenderContext::SwapBuffers() {
  g_pMorphMgr->AdvanceFrame();
  g_pOcclusionQueryMgr->AdvanceFrame();
  g_pShaderDevice->Present();
}

//-----------------------------------------------------------------------------
// Custom clip planes
//-----------------------------------------------------------------------------
void CMatRenderContext::PushCustomClipPlane(const float *pPlane) {
  PlaneStackElement psePlane;
  memcpy(psePlane.fValues, pPlane, sizeof(float) * 4);
  psePlane.bHack_IsHeightClipPlane = false;
  m_CustomClipPlanes.AddToTail(psePlane);  // we're doing this as a UtlVector so
                                           // height clip planes never change
                                           // their cached index
  ApplyCustomClipPlanes();
}

void CMatRenderContext::PopCustomClipPlane() {
  Assert(m_CustomClipPlanes.Count());

  // remove the endmost non-height plane found
  int i;
  for (i = m_CustomClipPlanes.Count(); --i >= 0;) {
    if (m_CustomClipPlanes[i].bHack_IsHeightClipPlane == false) {
      m_CustomClipPlanes.Remove(i);
      break;
    }
  }
  Assert(i != -1);  // only the height clip plane was found, which means this
                    // pop had no associated push
  ApplyCustomClipPlanes();
}

void CMatRenderContext::ApplyCustomClipPlanes() {
  int iMaxClipPlanes = HardwareConfig()->MaxUserClipPlanes();
  int iCustomPlanes;

  if (m_bEnableClipping)
    iCustomPlanes = m_CustomClipPlanes.Count();
  else
    iCustomPlanes = 0;

  float fFakePlane[4];
  unsigned int iFakePlaneVal = 0xFFFFFFFF;
  fFakePlane[0] = fFakePlane[1] = fFakePlane[2] = fFakePlane[3] =
      *((float *)&iFakePlaneVal);

  SyncMatrices();

  if (iMaxClipPlanes >= 1 && !HardwareConfig()->UseFastClipping()) {
    // yay, we get to be the cool clipping club
    if (iMaxClipPlanes >= iCustomPlanes) {
      int i;
      for (i = 0; i < iCustomPlanes; ++i) {
        g_pShaderAPI->SetClipPlane(i, m_CustomClipPlanes[i].fValues);
        g_pShaderAPI->EnableClipPlane(i, true);
      }
      for (; i < iMaxClipPlanes; ++i)  // disable unused planes
      {
        g_pShaderAPI->EnableClipPlane(i, false);
        g_pShaderAPI->SetClipPlane(i, fFakePlane);
      }
    } else {
      int iCustomPlaneOffset = iCustomPlanes - iMaxClipPlanes;

      // can't enable them all
      for (int i = iCustomPlaneOffset; i < iCustomPlanes; ++i) {
        g_pShaderAPI->SetClipPlane(i % iMaxClipPlanes,
                                   m_CustomClipPlanes[i].fValues);
        g_pShaderAPI->EnableClipPlane(i % iMaxClipPlanes, true);
      }
    }
  } else {
    // hmm, at most we can make 1 clip plane work
    if (iCustomPlanes == 0) {
      // no planes at all
      g_pShaderAPI->EnableFastClip(false);
      g_pShaderAPI->SetFastClipPlane(fFakePlane);
    } else {
      // we have to wire the topmost plane into the fast clipping scheme
      g_pShaderAPI->EnableFastClip(true);
      g_pShaderAPI->SetFastClipPlane(
          m_CustomClipPlanes[iCustomPlanes - 1].fValues);
    }
  }
}

//-----------------------------------------------------------------------------
// Creates/destroys morph data associated w/ a particular material
//-----------------------------------------------------------------------------
IMorph *CMatRenderContext::CreateMorph(MorphFormat_t format,
                                       const char *pDebugName) {
  Assert(format != 0);
  IMorphInternal *pMorph = g_pMorphMgr->CreateMorph();
  pMorph->Init(format, pDebugName);
  return pMorph;
}

void CMatRenderContext::DestroyMorph(IMorph *pMorph) {
  g_pMorphMgr->DestroyMorph(static_cast<IMorphInternal *>(pMorph));
}

void CMatRenderContext::BindMorph(IMorph *pMorph) {
  IMorphInternal *pMorphInternal = static_cast<IMorphInternal *>(pMorph);

  if (m_pBoundMorph != pMorphInternal) {
    g_pShaderAPI->FlushBufferedPrimitives();
    g_pShaderAPI->EnableHWMorphing(pMorph != NULL);
    m_pBoundMorph = pMorphInternal;
    if (pMorphInternal && pMorphInternal != MATERIAL_MORPH_DECAL) {
      pMorphInternal->Bind(m_pMorphRenderContext);
    }
  }
}

IMesh *CMatRenderContext::GetDynamicMesh(bool buffered, IMesh *pVertexOverride,
                                         IMesh *pIndexOverride,
                                         IMaterial *pAutoBind) {
  VPROF_ASSERT_ACCOUNTED("CMatRenderContext::GetDynamicMesh");
  if (pAutoBind) {
    Bind(pAutoBind, NULL);
  }

  if (pVertexOverride) {
    if (CompressionType(pVertexOverride->GetVertexFormat()) !=
        VERTEX_COMPRESSION_NONE) {
      // UNDONE: support compressed dynamic meshes if needed (pro: less VB
      // memory, con: time spent compressing)
      DebuggerBreak();
      return NULL;
    }
  }

  // For anything more than 1 bone, imply the last weight from the 1 - the sum
  // of the others.
  int nCurrentBoneCount = GetCurrentNumBones();
  Assert(nCurrentBoneCount <= 4);
  if (nCurrentBoneCount > 1) {
    --nCurrentBoneCount;
  }

  return g_pShaderAPI->GetDynamicMesh(GetCurrentMaterialInternal(),
                                      nCurrentBoneCount, buffered,
                                      pVertexOverride, pIndexOverride);
}

IMesh *CMatRenderContext::GetDynamicMeshEx(VertexFormat_t vertexFormat,
                                           bool bBuffered,
                                           IMesh *pVertexOverride,
                                           IMesh *pIndexOverride,
                                           IMaterial *pAutoBind) {
  VPROF_ASSERT_ACCOUNTED("CMatRenderContext::GetDynamicMesh");
  if (pAutoBind) Bind(pAutoBind, NULL);

  if (pVertexOverride) {
    if (CompressionType(pVertexOverride->GetVertexFormat()) !=
        VERTEX_COMPRESSION_NONE) {
      // UNDONE: support compressed dynamic meshes if needed (pro: less VB
      // memory, con: time spent compressing)
      DebuggerBreak();
      return NULL;
    }
  }

  // For anything more than 1 bone, imply the last weight from the 1 - the sum
  // of the others.
  // TODO(d.rattman): this seems wrong - in common_vs_fxc.h, we only infer the
  // last weight if we have 3 (not 2)
  int nCurrentBoneCount = GetCurrentNumBones();
  Assert(nCurrentBoneCount <= 4);
  if (nCurrentBoneCount > 1) {
    --nCurrentBoneCount;
  }

  return g_pShaderAPI->GetDynamicMeshEx(
      GetCurrentMaterialInternal(), vertexFormat, nCurrentBoneCount, bBuffered,
      pVertexOverride, pIndexOverride);
}

//-----------------------------------------------------------------------------
// Deals with depth range
//-----------------------------------------------------------------------------
void CMatRenderContext::DepthRange(float zNear, float zFar) {
  m_Viewport.m_flMinZ = zNear;
  m_Viewport.m_flMaxZ = zFar;
  g_pShaderAPI->SetViewports(1, &m_Viewport);
}

// Private utility function to actually commit the top of the RT/Viewport stack
// to the device.  Only called by the push and pop routines above.
void CMatRenderContext::CommitRenderTargetAndViewport() {
  Assert(m_RenderTargetStack.Count() > 0);

  const RenderTargetStackElement_t &element = m_RenderTargetStack.Top();

  for (int rt = 0; rt < std::size(element.m_pRenderTargets); rt++) {
    // If we're dealing with the back buffer
    if (element.m_pRenderTargets[rt] == NULL) {
      // No texture parameter here indicates back buffer
      g_pShaderAPI->SetRenderTargetEx(rt);

      Assert(ImageLoader::SizeInBytes(g_pShaderDevice->GetBackBufferFormat()) <=
             4);
      g_pShaderAPI->EnableLinearColorSpaceFrameBuffer(false);

      // the first rt sets the viewport
      if (rt == 0) {
        // If either dimension is negative, set to full bounds of back buffer
        if ((element.m_nViewW < 0) || (element.m_nViewH < 0)) {
          m_Viewport.m_nTopLeftX = 0;
          m_Viewport.m_nTopLeftY = 0;
          g_pShaderAPI->GetBackBufferDimensions(m_Viewport.m_nWidth,
                                                m_Viewport.m_nHeight);
          g_pShaderAPI->SetViewports(1, &m_Viewport);
        } else  // use the bounds in the element
        {
          m_Viewport.m_nTopLeftX = element.m_nViewX;
          m_Viewport.m_nTopLeftY = element.m_nViewY;
          m_Viewport.m_nWidth = element.m_nViewW;
          m_Viewport.m_nHeight = element.m_nViewH;
          g_pShaderAPI->SetViewports(1, &m_Viewport);
        }
      }
    } else  // We're dealing with a texture
    {
      ITextureInternal *pTexInt =
          static_cast<ITextureInternal *>(element.m_pRenderTargets[rt]);
      pTexInt->SetRenderTarget(rt, element.m_pDepthTexture);

      if (rt == 0) {
        if (element.m_pRenderTargets[rt]->GetImageFormat() ==
            IMAGE_FORMAT_RGBA16161616F) {
          g_pShaderAPI->EnableLinearColorSpaceFrameBuffer(true);
        } else {
          g_pShaderAPI->EnableLinearColorSpaceFrameBuffer(false);
        }

        // If either dimension is negative, set to full bounds of target
        if ((element.m_nViewW < 0) || (element.m_nViewH < 0)) {
          m_Viewport.m_nTopLeftX = 0;
          m_Viewport.m_nTopLeftY = 0;
          m_Viewport.m_nWidth = element.m_pRenderTargets[rt]->GetActualWidth();
          m_Viewport.m_nHeight =
              element.m_pRenderTargets[rt]->GetActualHeight();
          g_pShaderAPI->SetViewports(1, &m_Viewport);
        } else  // use the bounds passed in
        {
          m_Viewport.m_nTopLeftX = element.m_nViewX;
          m_Viewport.m_nTopLeftY = element.m_nViewY;
          m_Viewport.m_nWidth = element.m_nViewW;
          m_Viewport.m_nHeight = element.m_nViewH;
          g_pShaderAPI->SetViewports(1, &m_Viewport);
        }
      }
    }
  }
}

void CMatRenderContext::SetFrameBufferCopyTexture(ITexture *pTexture,
                                                  int textureIndex) {
  if (textureIndex < 0 || textureIndex >= MAX_FB_TEXTURES) {
    Assert(0);
    return;
  }
  if (m_pCurrentFrameBufferCopyTexture[textureIndex] != pTexture) {
    g_pShaderAPI->FlushBufferedPrimitives();
  }
  // TODO(d.rattman): Do I need to increment/decrement ref counts here, or
  // assume that the app is going to do it?
  m_pCurrentFrameBufferCopyTexture[textureIndex] = pTexture;
}

void CMatRenderContext::BindLocalCubemap(ITexture *pTexture) {
  ITexture *pPreviousTexture = m_pLocalCubemapTexture;

  CMatRenderContextBase::BindLocalCubemap(pTexture);

  if (m_pLocalCubemapTexture != pPreviousTexture) {
    g_pShaderAPI->FlushBufferedPrimitives();
  }
}

void CMatRenderContext::SetNonInteractivePacifierTexture(
    ITexture *pTexture, float flNormalizedX, float flNormalizedY,
    float flNormalizedSize) {
  m_pNonInteractivePacifier.Init(pTexture);
  m_flNormalizedX = flNormalizedX;
  m_flNormalizedY = flNormalizedY;
  m_flNormalizedSize = flNormalizedSize;
}

void CMatRenderContext::SetNonInteractiveTempFullscreenBuffer(
    ITexture *pTexture, MaterialNonInteractiveMode_t mode) {
  if (mode != MATERIAL_NON_INTERACTIVE_MODE_NONE) {
    m_pNonInteractiveTempFullscreenBuffer[mode].Init(pTexture);
  }
}

void CMatRenderContext::RefreshFrontBufferNonInteractive() {
  g_pShaderDevice->RefreshFrontBufferNonInteractive();
}

void CMatRenderContext::EnableNonInteractiveMode(
    MaterialNonInteractiveMode_t mode) {
  m_NonInteractiveMode = mode;
  if (mode == MATERIAL_NON_INTERACTIVE_MODE_NONE) {
    g_pShaderDevice->EnableNonInteractiveMode(mode);
  } else {
    ShaderNonInteractiveInfo_t info;
    info.m_flNormalizedX = m_flNormalizedX;
    info.m_flNormalizedY = m_flNormalizedY;
    info.m_flNormalizedSize = m_flNormalizedSize;

    ITextureInternal *pTexInternal = static_cast<ITextureInternal *>(
        (ITexture *)m_pNonInteractiveTempFullscreenBuffer[mode]);
    info.m_hTempFullscreenTexture = pTexInternal
                                        ? pTexInternal->GetTextureHandle(0)
                                        : INVALID_SHADERAPI_TEXTURE_HANDLE;
    ITextureInternal *pTexPacifierInternal =
        static_cast<ITextureInternal *>((ITexture *)m_pNonInteractivePacifier);
    info.m_nPacifierCount = pTexPacifierInternal
                                ? pTexPacifierInternal->GetNumAnimationFrames()
                                : 0;
    for (int i = 0; i < info.m_nPacifierCount; ++i) {
      info.m_pPacifierTextures[i] = pTexPacifierInternal->GetTextureHandle(i);
    }
    g_pShaderDevice->EnableNonInteractiveMode(mode, &info);
  }
}

void CMatRenderContext::SetRenderTargetEx(int nRenderTargetID,
                                          ITexture *pNewTarget) {
  // Verify valid top of RT stack
  Assert(m_RenderTargetStack.Count() > 0);

  // Grab the old target
  ITexture *pOldTarget =
      m_RenderTargetStack.Top().m_pRenderTargets[nRenderTargetID];

  CMatRenderContextBase::SetRenderTargetEx(nRenderTargetID, pNewTarget);

  // If we're actually changing render targets
  if (pNewTarget != pOldTarget) {
    // If we're going to render to the back buffer
    if (pNewTarget == NULL) {
      if (nRenderTargetID == 0)  // reset viewport on set of rt 0
      {
        m_Viewport.m_nTopLeftX = 0;
        m_Viewport.m_nTopLeftY = 0;
        g_pShaderAPI->GetBackBufferDimensions(m_Viewport.m_nWidth,
                                              m_Viewport.m_nHeight);
        g_pShaderAPI->SetViewports(1, &m_Viewport);
      }
      g_pShaderAPI->SetRenderTargetEx(
          nRenderTargetID);  // No parameter here indicates back buffer
    } else {
      // If we're going to render to a texture
      // Make sure the texture is a render target...
      bool reset = true;
      if (nRenderTargetID == 0) {
        // reset vp on change of rt#0
        m_Viewport.m_nTopLeftX = 0;
        m_Viewport.m_nTopLeftY = 0;
        m_Viewport.m_nWidth = pNewTarget->GetActualWidth();
        m_Viewport.m_nHeight = pNewTarget->GetActualHeight();
        g_pShaderAPI->SetViewports(1, &m_Viewport);
      }
      ITextureInternal *pTexInt = static_cast<ITextureInternal *>(pNewTarget);
      reset = !pTexInt->SetRenderTarget(nRenderTargetID);
      if (reset) {
        g_pShaderAPI->SetRenderTargetEx(nRenderTargetID);
      }

      if (pNewTarget->GetImageFormat() == IMAGE_FORMAT_RGBA16161616F) {
        g_pShaderAPI->EnableLinearColorSpaceFrameBuffer(true);
      } else {
        g_pShaderAPI->EnableLinearColorSpaceFrameBuffer(false);
      }
    }
  }
  CommitRenderTargetAndViewport();
}

void CMatRenderContext::GetRenderTargetDimensions(int &width,
                                                  int &height) const {
  // Target at top of stack
  ITexture *pTOS = m_RenderTargetStack.Top().m_pRenderTargets[0];

  // If top of stack isn't the back buffer, get dimensions from the texture
  if (pTOS != NULL) {
    width = pTOS->GetActualWidth();
    height = pTOS->GetActualHeight();
  } else  // otherwise, get them from the shader API
  {
    g_pShaderAPI->GetBackBufferDimensions(width, height);
  }
}

//-----------------------------------------------------------------------------
// What are the lightmap dimensions?
//-----------------------------------------------------------------------------
void CMatRenderContext::GetLightmapDimensions(int *w, int *h) {
  *w = GetMaterialSystem()->GetLightmapWidth(GetLightmapPage());
  *h = GetMaterialSystem()->GetLightmapHeight(GetLightmapPage());
}

//-----------------------------------------------------------------------------
// TODO(d.rattman): This is a hack required for NVidia/XBox, can they fix in
// drivers?
//-----------------------------------------------------------------------------
void CMatRenderContext::DrawScreenSpaceQuad(IMaterial *pMaterial) {
  // This is required because the texture coordinates for NVidia reading
  // out of non-power-of-two textures is borked
  int w, h;

  GetRenderTargetDimensions(w, h);
  if ((w == 0) || (h == 0)) return;

  // This is the size of the back-buffer we're reading from.
  int bw, bh;
  bw = w;
  bh = h;

  /* TODO(d.rattman): Get this to work in hammer/engine integration
  if ( m_pRenderTargetTexture )
  {
  }
  else
  {
          MaterialVideoMode_t mode;
          GetDisplayMode( mode );
          if ( ( mode.m_Width == 0 ) || ( mode.m_Height == 0 ) )
                  return;
          bw = mode.m_Width;
          bh = mode.m_Height;
  }
  */

  float s0, t0;
  float s1, t1;

  float flOffsetS = (bw != 0.0f) ? 1.0f / bw : 0.0f;
  float flOffsetT = (bh != 0.0f) ? 1.0f / bh : 0.0f;
  s0 = 0.5f * flOffsetS;
  t0 = 0.5f * flOffsetT;
  s1 = (w - 0.5f) * flOffsetS;
  t1 = (h - 0.5f) * flOffsetT;

  Bind(pMaterial);
  IMesh *pMesh = GetDynamicMesh(true);

  MatrixMode(MATERIAL_VIEW);
  PushMatrix();
  LoadIdentity();

  MatrixMode(MATERIAL_PROJECTION);
  PushMatrix();
  LoadIdentity();

  CMeshBuilder meshBuilder;
  meshBuilder.Begin(pMesh, MATERIAL_QUADS, 1);

  meshBuilder.Position3f(-1.0f, -1.0f, 0.0f);
  meshBuilder.TangentS3f(0.0f, 1.0f, 0.0f);
  meshBuilder.TangentT3f(1.0f, 0.0f, 0.0f);
  meshBuilder.Normal3f(0.0f, 0.0f, 1.0f);
  meshBuilder.TexCoord2f(0, s0, t1);
  meshBuilder.AdvanceVertex();

  meshBuilder.Position3f(-1.0f, 1, 0.0f);
  meshBuilder.TangentS3f(0.0f, 1.0f, 0.0f);
  meshBuilder.TangentT3f(1.0f, 0.0f, 0.0f);
  meshBuilder.Normal3f(0.0f, 0.0f, 1.0f);
  meshBuilder.TexCoord2f(0, s0, t0);
  meshBuilder.AdvanceVertex();

  meshBuilder.Position3f(1, 1, 0.0f);
  meshBuilder.TangentS3f(0.0f, 1.0f, 0.0f);
  meshBuilder.TangentT3f(1.0f, 0.0f, 0.0f);
  meshBuilder.Normal3f(0.0f, 0.0f, 1.0f);
  meshBuilder.TexCoord2f(0, s1, t0);
  meshBuilder.AdvanceVertex();

  meshBuilder.Position3f(1, -1.0f, 0.0f);
  meshBuilder.TangentS3f(0.0f, 1.0f, 0.0f);
  meshBuilder.TangentT3f(1.0f, 0.0f, 0.0f);
  meshBuilder.Normal3f(0.0f, 0.0f, 1.0f);
  meshBuilder.TexCoord2f(0, s1, t1);
  meshBuilder.AdvanceVertex();

  meshBuilder.End();
  pMesh->Draw();

  MatrixMode(MATERIAL_VIEW);
  PopMatrix();

  MatrixMode(MATERIAL_PROJECTION);
  PopMatrix();
}

void CMatRenderContext::DrawScreenSpaceRectangle(
    IMaterial *pMaterial, int destx, int desty, int width, int height,
    float src_texture_x0,
    float src_texture_y0,  // which texel you want to appear at
    // destx/y
    float src_texture_x1,
    float src_texture_y1,  // which texel you want to appear at
    // destx+width-1, desty+height-1
    int src_texture_width, int src_texture_height,  // needed for fixup
    void *pClientRenderable, int nXDice,
    int nYDice)  // Amount to tessellate the quad
{
  pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion();

  ::DrawScreenSpaceRectangle(
      pMaterial, destx, desty, width, height, src_texture_x0, src_texture_y0,
      src_texture_x1, src_texture_y1, src_texture_width, src_texture_height,
      pClientRenderable, nXDice, nYDice);
  return;
}

static int CompareVertexFormats(VertexFormat_t Fmt1, VertexFormat_t Fmt2) {
  if (Fmt1 != Fmt2) {
    return (Fmt1 > Fmt2) ? 1 : -1;
  }

  return 0;
}

int CMatRenderContext::CompareMaterialCombos(IMaterial *pMaterial1,
                                             IMaterial *pMaterial2,
                                             int lightMapID1, int lightMapID2) {
  pMaterial1 =
      ((IMaterialInternal *)pMaterial1)->GetRealTimeVersion();  // always work
                                                                // with the real
                                                                // time version
                                                                // of materials
                                                                // internally.
  pMaterial2 =
      ((IMaterialInternal *)pMaterial2)->GetRealTimeVersion();  // always work
                                                                // with the real
                                                                // time version
                                                                // of materials
                                                                // internally.

  IMaterialInternal *pMat1 = (IMaterialInternal *)pMaterial1;
  IMaterialInternal *pMat2 = (IMaterialInternal *)pMaterial2;
  ShaderRenderState_t *pState1 = pMat1->GetRenderState();
  ShaderRenderState_t *pState2 = pMat2->GetRenderState();
  int dPass =
      pState2->m_pSnapshots->m_nPassCount - pState1->m_pSnapshots->m_nPassCount;
  if (dPass) return dPass;

  if (pState1->m_pSnapshots->m_nPassCount > 1) {
    int dFormat = CompareVertexFormats(pMat1->GetVertexFormat(),
                                       pMat2->GetVertexFormat());
    if (dFormat) return dFormat;
  }

  for (int i = 0; i < pState1->m_pSnapshots->m_nPassCount; i++) {
    // UNDONE: Compare snapshots in the shaderapi?
    int dSnapshot = pState1->m_pSnapshots->m_Snapshot[i] -
                    pState2->m_pSnapshots->m_Snapshot[i];
    if (dSnapshot) {
      dSnapshot =
          g_pShaderAPI->CompareSnapshots(pState1->m_pSnapshots->m_Snapshot[i],
                                         pState2->m_pSnapshots->m_Snapshot[i]);
      if (dSnapshot) return dSnapshot;
    }
  }

  int dFormat =
      CompareVertexFormats(pMat1->GetVertexFormat(), pMat2->GetVertexFormat());
  if (dFormat) return dFormat;

  IMaterialVar **pParams1 = pMat1->GetShaderParams();
  IMaterialVar **pParams2 = pMat2->GetShaderParams();

  if (pParams1[BASETEXTURE]->GetType() == MATERIAL_VAR_TYPE_TEXTURE ||
      pParams2[BASETEXTURE]->GetType() == MATERIAL_VAR_TYPE_TEXTURE) {
    if (pParams1[BASETEXTURE]->GetType() != pParams2[BASETEXTURE]->GetType()) {
      return pParams2[BASETEXTURE]->GetType() -
             pParams1[BASETEXTURE]->GetType();
    }
    int dBaseTexture =
        Q_stricmp(pParams1[BASETEXTURE]->GetTextureValue()->GetName(),
                  pParams2[BASETEXTURE]->GetTextureValue()->GetName());
    if (dBaseTexture) return dBaseTexture;
  }

  int dLightmap = lightMapID1 - lightMapID2;
  if (dLightmap) return dLightmap;

  return (int)pMat1 - (int)pMat2;
}

void CMatRenderContext::Bind(IMaterial *iMaterial, void *proxyData) {
  IMaterialInternal *material = static_cast<IMaterialInternal *>(iMaterial);

  if (!material) {
    if (!g_pErrorMaterial) return;
    Warning("Programming error: CMatRenderContext::Bind: NULL material\n");
    material = static_cast<IMaterialInternal *>(g_pErrorMaterial);
  }
  material =
      material->GetRealTimeVersion();  // always work with the real time
                                       // versions of materials internally

  if (g_config.bDrawFlat && !material->NoDebugOverride()) {
    material = static_cast<IMaterialInternal *>(GetDrawFlatMaterial());
  }

  CMatRenderContextBase::Bind(iMaterial, proxyData);

  // We've always gotta call the bind proxy
  SyncMatrices();
  if (GetMaterialSystem()->GetThreadMode() == MATERIAL_SINGLE_THREADED) {
    GetCurrentMaterialInternal()->CallBindProxy(proxyData);
  }
  g_pShaderAPI->Bind(GetCurrentMaterialInternal());
}

void CMatRenderContext::CopyRenderTargetToTextureEx(ITexture *pTexture,
                                                    int nRenderTargetID,
                                                    Rect_t *pSrcRect,
                                                    Rect_t *pDstRect) {
  if (!pTexture) {
    Assert(0);
    return;
  }

  GetMaterialSystem()->Flush(false);

  ITextureInternal *pTextureInternal = (ITextureInternal *)pTexture;
  pTextureInternal->CopyFrameBufferToMe(nRenderTargetID, pSrcRect, pDstRect);
}

void CMatRenderContext::CopyRenderTargetToTexture(ITexture *pTexture) {
  CopyRenderTargetToTextureEx(pTexture, NULL, NULL);
}

void CMatRenderContext::ClearBuffers(bool bClearColor, bool bClearDepth,
                                     bool bClearStencil) {
  int width, height;
  GetRenderTargetDimensions(width, height);
  g_pShaderAPI->ClearBuffers(bClearColor, bClearDepth, bClearStencil, width,
                             height);
}

void CMatRenderContext::DrawClearBufferQuad(unsigned char r, unsigned char g,
                                            unsigned char b, unsigned char a,
                                            bool bClearColor,
                                            bool bClearDepth) {
  IMaterialInternal *pClearMaterial =
      GetBufferClearObeyStencil(bClearColor + (bClearDepth << 1));
  Bind(pClearMaterial);

  IMesh *pMesh = GetDynamicMesh(true);

  MatrixMode(MATERIAL_MODEL);
  PushMatrix();
  LoadIdentity();

  MatrixMode(MATERIAL_VIEW);
  PushMatrix();
  LoadIdentity();

  MatrixMode(MATERIAL_PROJECTION);
  PushMatrix();
  LoadIdentity();

  float flDepth = GetMaterialSystem()->GetConfig().bReverseDepth ? 0.0f : 1.0f;

  CMeshBuilder meshBuilder;
  meshBuilder.Begin(pMesh, MATERIAL_QUADS, 1);

  // 1.1 instead of 1.0 to fix small borders around the edges in full screen
  // with anti-aliasing enabled
  meshBuilder.Position3f(-1.1f, -1.1f, flDepth);
  meshBuilder.Color4ub(r, g, b, a);
  meshBuilder.AdvanceVertex();

  meshBuilder.Position3f(-1.1f, 1.1f, flDepth);
  meshBuilder.Color4ub(r, g, b, a);
  meshBuilder.AdvanceVertex();

  meshBuilder.Position3f(1.1f, 1.1f, flDepth);
  meshBuilder.Color4ub(r, g, b, a);
  meshBuilder.AdvanceVertex();

  meshBuilder.Position3f(1.1f, -1.1f, flDepth);
  meshBuilder.Color4ub(r, g, b, a);
  meshBuilder.AdvanceVertex();

  meshBuilder.End();
  pMesh->Draw();

  MatrixMode(MATERIAL_MODEL);
  PopMatrix();

  MatrixMode(MATERIAL_VIEW);
  PopMatrix();

  MatrixMode(MATERIAL_PROJECTION);
  PopMatrix();
}

//-----------------------------------------------------------------------------
// Should really be called SetViewport
//-----------------------------------------------------------------------------
void CMatRenderContext::Viewport(int x, int y, int width, int height) {
  CMatRenderContextBase::Viewport(x, y, width, height);

  // If either dimension is negative, set to full bounds of current target
  if ((width < 0) || (height < 0)) {
    ITexture *pTarget = m_RenderTargetStack.Top().m_pRenderTargets[0];

    // If target is the back buffer
    if (pTarget == NULL) {
      m_Viewport.m_nTopLeftX = 0;
      m_Viewport.m_nTopLeftY = 0;
      g_pShaderAPI->GetBackBufferDimensions(m_Viewport.m_nWidth,
                                            m_Viewport.m_nHeight);
      g_pShaderAPI->SetViewports(1, &m_Viewport);
    } else  // target is a texture
    {
      m_Viewport.m_nTopLeftX = 0;
      m_Viewport.m_nTopLeftY = 0;
      m_Viewport.m_nWidth = pTarget->GetActualWidth();
      m_Viewport.m_nHeight = pTarget->GetActualHeight();
      g_pShaderAPI->SetViewports(1, &m_Viewport);
    }
  } else  // use the bounds passed in
  {
    m_Viewport.m_nTopLeftX = x;
    m_Viewport.m_nTopLeftY = y;
    m_Viewport.m_nWidth = width;
    m_Viewport.m_nHeight = height;
    g_pShaderAPI->SetViewports(1, &m_Viewport);
  }
}

void CMatRenderContext::GetViewport(int &x, int &y, int &width,
                                    int &height) const {
  // Verify valid top of RT stack
  Assert(m_RenderTargetStack.Count() > 0);

  // Grab the top of stack
  const RenderTargetStackElement_t &element = m_RenderTargetStack.Top();

  // If either dimension is not positive, set to full bounds of current target
  if ((element.m_nViewW <= 0) || (element.m_nViewH <= 0)) {
    // Viewport origin at target origin
    x = y = 0;

    // If target is back buffer
    if (element.m_pRenderTargets[0] == NULL) {
      g_pShaderAPI->GetBackBufferDimensions(width, height);
    } else  // if target is texture
    {
      width = element.m_pRenderTargets[0]->GetActualWidth();
      height = element.m_pRenderTargets[0]->GetActualHeight();
    }
  } else  // use the bounds from the stack directly
  {
    x = element.m_nViewX;
    y = element.m_nViewY;
    width = element.m_nViewW;
    height = element.m_nViewH;
  }
}

//-----------------------------------------------------------------------------
// Methods related to user clip planes
//-----------------------------------------------------------------------------
void CMatRenderContext::UpdateHeightClipUserClipPlane() {
  PlaneStackElement pse;
  pse.bHack_IsHeightClipPlane = true;

  int iExistingHeightClipPlaneIndex;
  for (iExistingHeightClipPlaneIndex = m_CustomClipPlanes.Count();
       --iExistingHeightClipPlaneIndex >= 0;) {
    if (m_CustomClipPlanes[iExistingHeightClipPlaneIndex]
            .bHack_IsHeightClipPlane)
      break;
  }

  switch (m_HeightClipMode) {
    case MATERIAL_HEIGHTCLIPMODE_DISABLE:
      if (iExistingHeightClipPlaneIndex != -1)
        m_CustomClipPlanes.Remove(iExistingHeightClipPlaneIndex);
      break;
    case MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT:
      pse.fValues[0] = 0.0f;
      pse.fValues[1] = 0.0f;
      pse.fValues[2] = 1.0f;
      pse.fValues[3] = m_HeightClipZ;
      if (iExistingHeightClipPlaneIndex != -1) {
        memcpy(m_CustomClipPlanes.Base() + iExistingHeightClipPlaneIndex, &pse,
               sizeof(PlaneStackElement));
      } else {
        m_CustomClipPlanes.AddToTail(pse);
      }
      break;
    case MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT:
      pse.fValues[0] = 0.0f;
      pse.fValues[1] = 0.0f;
      pse.fValues[2] = -1.0f;
      pse.fValues[3] = -m_HeightClipZ;
      if (iExistingHeightClipPlaneIndex != -1) {
        memcpy(m_CustomClipPlanes.Base() + iExistingHeightClipPlaneIndex, &pse,
               sizeof(PlaneStackElement));
      } else {
        m_CustomClipPlanes.AddToTail(pse);
      }
      break;
  };

  ApplyCustomClipPlanes();

  /*switch( m_HeightClipMode )
  {
  case MATERIAL_HEIGHTCLIPMODE_DISABLE:
          g_pShaderAPI->EnableClipPlane( 0, false );
          break;
  case MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT:
          plane[0] = 0.0f;
          plane[1] = 0.0f;
          plane[2] = 1.0f;
          plane[3] = m_HeightClipZ;
          g_pShaderAPI->SetClipPlane( 0, plane );
          g_pShaderAPI->EnableClipPlane( 0, true );
          break;
  case MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT:
          plane[0] = 0.0f;
          plane[1] = 0.0f;
          plane[2] = -1.0f;
          plane[3] = -m_HeightClipZ;
          g_pShaderAPI->SetClipPlane( 0, plane );
          g_pShaderAPI->EnableClipPlane( 0, true );
          break;
  }*/
}

//-----------------------------------------------------------------------------
// Lightmap stuff
//-----------------------------------------------------------------------------
void CMatRenderContext::BindLightmapPage(int lightmapPageID) {
  if (m_lightmapPageID == lightmapPageID) return;

  // We gotta make sure there's no buffered primitives 'cause this'll
  // change the render state.
  g_pShaderAPI->FlushBufferedPrimitives();

  CMatRenderContextBase::BindLightmapPage(lightmapPageID);
}

void CMatRenderContext::BindLightmapTexture(ITexture *pLightmapTexture) {
  if ((m_lightmapPageID == MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED) &&
      (m_pUserDefinedLightmap == pLightmapTexture))
    return;

  // We gotta make sure there's no buffered primitives 'cause this'll
  // change the render state.
  g_pShaderAPI->FlushBufferedPrimitives();

  m_lightmapPageID = MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED;
  if (pLightmapTexture) {
    pLightmapTexture->IncrementReferenceCount();
  }
  if (m_pUserDefinedLightmap) {
    m_pUserDefinedLightmap->DecrementReferenceCount();
  }
  m_pUserDefinedLightmap = static_cast<ITextureInternal *>(pLightmapTexture);
}

void CMatRenderContext::BindLightmap(Sampler_t sampler) {
  switch (m_lightmapPageID) {
    default:
      Assert((m_lightmapPageID == 0 &&
              GetLightmaps()->GetNumLightmapPages() == 0) ||
             (m_lightmapPageID >= 0 &&
              m_lightmapPageID < GetLightmaps()->GetNumLightmapPages()));
      if (m_lightmapPageID >= 0 &&
          m_lightmapPageID < GetLightmaps()->GetNumLightmapPages()) {
        g_pShaderAPI->BindTexture(
            sampler,
            GetLightmaps()->GetLightmapPageTextureHandle(m_lightmapPageID));
      }
      break;

    case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
      AssertOnce(m_pUserDefinedLightmap);
      g_pShaderAPI->BindTexture(sampler,
                                m_pUserDefinedLightmap->GetTextureHandle(0));
      break;

    case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
      BindFullbrightLightmap(sampler);
      break;

    case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
      BindBumpedFullbrightLightmap(sampler);
      break;
  }
}

void CMatRenderContext::BindBumpLightmap(Sampler_t sampler) {
  switch (m_lightmapPageID) {
    default:
      Assert(m_lightmapPageID >= 0 &&
             m_lightmapPageID < GetLightmaps()->GetNumLightmapPages());
      if (m_lightmapPageID >= 0 &&
          m_lightmapPageID < GetLightmaps()->GetNumLightmapPages()) {
        g_pShaderAPI->BindTexture(
            sampler,
            GetLightmaps()->GetLightmapPageTextureHandle(m_lightmapPageID));
        g_pShaderAPI->BindTexture(
            (Sampler_t)(sampler + 1),
            GetLightmaps()->GetLightmapPageTextureHandle(m_lightmapPageID));
        g_pShaderAPI->BindTexture(
            (Sampler_t)(sampler + 2),
            GetLightmaps()->GetLightmapPageTextureHandle(m_lightmapPageID));
      }
      break;
    case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
      AssertOnce(m_pUserDefinedLightmap);
      g_pShaderAPI->BindTexture(sampler,
                                m_pUserDefinedLightmap->GetTextureHandle(0));
      g_pShaderAPI->BindTexture((Sampler_t)(sampler + 1),
                                m_pUserDefinedLightmap->GetTextureHandle(0));
      g_pShaderAPI->BindTexture((Sampler_t)(sampler + 2),
                                m_pUserDefinedLightmap->GetTextureHandle(0));
      break;
    case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
      BindBumpedFullbrightLightmap(sampler);
      BindBumpedFullbrightLightmap((Sampler_t)(sampler + 1));
      BindBumpedFullbrightLightmap((Sampler_t)(sampler + 2));
      break;
    case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
      BindBumpedFullbrightLightmap(sampler);
      BindBumpedFullbrightLightmap((Sampler_t)(sampler + 1));
      BindBumpedFullbrightLightmap((Sampler_t)(sampler + 2));
      break;
  }
}

void CMatRenderContext::BindFullbrightLightmap(Sampler_t sampler) {
  g_pShaderAPI->BindTexture(sampler, GetFullbrightLightmapTextureHandle());
}

void CMatRenderContext::BindBumpedFullbrightLightmap(Sampler_t sampler) {
  g_pShaderAPI->BindTexture(sampler,
                            GetFullbrightBumpedLightmapTextureHandle());
}

//-----------------------------------------------------------------------------
// Bind standard textures
//-----------------------------------------------------------------------------
void CMatRenderContext::BindStandardTexture(Sampler_t sampler,
                                            StandardTextureId_t id) {
  switch (id) {
    case TEXTURE_LIGHTMAP:
      BindLightmap(sampler);
      return;

    case TEXTURE_LIGHTMAP_BUMPED:
      BindBumpLightmap(sampler);
      return;

    case TEXTURE_LIGHTMAP_FULLBRIGHT:
      BindFullbrightLightmap(sampler);
      return;

    case TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT:
      BindBumpedFullbrightLightmap(sampler);
      return;

    case TEXTURE_WHITE:
      g_pShaderAPI->BindTexture(sampler, GetWhiteTextureHandle());
      return;

    case TEXTURE_BLACK:
      g_pShaderAPI->BindTexture(sampler, GetBlackTextureHandle());
      return;

    case TEXTURE_GREY:
      g_pShaderAPI->BindTexture(sampler, GetGreyTextureHandle());
      return;

    case TEXTURE_GREY_ALPHA_ZERO:
      g_pShaderAPI->BindTexture(sampler, GetGreyAlphaZeroTextureHandle());
      return;

    case TEXTURE_NORMALMAP_FLAT:
      g_pShaderAPI->BindTexture(sampler, GetFlatNormalTextureHandle());
      return;

    case TEXTURE_NORMALIZATION_CUBEMAP:
      TextureManager()->NormalizationCubemap()->Bind(sampler);
      return;

    case TEXTURE_NORMALIZATION_CUBEMAP_SIGNED:
      TextureManager()->SignedNormalizationCubemap()->Bind(sampler);
      return;

    case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0:
    case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1: {
      int nTextureIndex = id - TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0;
      if (m_pCurrentFrameBufferCopyTexture[nTextureIndex]) {
        ((ITextureInternal *)m_pCurrentFrameBufferCopyTexture[nTextureIndex])
            ->Bind(sampler);
      }
    }
      return;

    case TEXTURE_COLOR_CORRECTION_VOLUME_0:
    case TEXTURE_COLOR_CORRECTION_VOLUME_1:
    case TEXTURE_COLOR_CORRECTION_VOLUME_2:
    case TEXTURE_COLOR_CORRECTION_VOLUME_3: {
      ITextureInternal *pTexture = TextureManager()->ColorCorrectionTexture(
          id - TEXTURE_COLOR_CORRECTION_VOLUME_0);
      if (pTexture) {
        pTexture->Bind(sampler);
      }
    }
      return;

    case TEXTURE_SHADOW_NOISE_2D:
      TextureManager()->ShadowNoise2D()->Bind(sampler);
      return;

    case TEXTURE_IDENTITY_LIGHTWARP:
      TextureManager()->IdentityLightWarp()->Bind(sampler);
      return;

    case TEXTURE_MORPH_ACCUMULATOR:
      g_pMorphMgr->MorphAccumulator()->Bind(sampler);
      return;

    case TEXTURE_MORPH_WEIGHTS:
      g_pMorphMgr->MorphWeights()->Bind(sampler);
      return;

    case TEXTURE_FRAME_BUFFER_FULL_DEPTH:
      if (m_bFullFrameDepthIsValid)
        TextureManager()->FullFrameDepthTexture()->Bind(sampler);
      else
        g_pShaderAPI->BindTexture(sampler, GetMaxDepthTextureHandle());
      return;

    default:
      Assert(0);
  }
}

void CMatRenderContext::BindStandardVertexTexture(
    VertexTextureSampler_t sampler, StandardTextureId_t id) {
  switch (id) {
    case TEXTURE_MORPH_ACCUMULATOR:
      g_pMorphMgr->MorphAccumulator()->BindVertexTexture(sampler);
      return;

    case TEXTURE_MORPH_WEIGHTS:
      g_pMorphMgr->MorphWeights()->BindVertexTexture(sampler);
      return;

    default:
      Assert(0);
  }
}

void CMatRenderContext::GetStandardTextureDimensions(int *pWidth, int *pHeight,
                                                     StandardTextureId_t id) {
  ITexture *pTexture = NULL;
  switch (id) {
    case TEXTURE_LIGHTMAP:
    case TEXTURE_LIGHTMAP_BUMPED:
    case TEXTURE_LIGHTMAP_FULLBRIGHT:
    case TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT:
      // NOTE: Doesn't exactly work since we may be in fullbright mode
      //		GetLightmapDimensions( pWidth, pHeight );
      //		break;

    case TEXTURE_WHITE:
    case TEXTURE_BLACK:
    case TEXTURE_GREY:
    case TEXTURE_GREY_ALPHA_ZERO:
    case TEXTURE_NORMALMAP_FLAT:
    default:
      Assert(0);
      Warning(
          "GetStandardTextureDimensions: still unimplemented for this type!\n");
      *pWidth = *pHeight = -1;
      break;

    case TEXTURE_NORMALIZATION_CUBEMAP:
      pTexture = TextureManager()->NormalizationCubemap();
      break;

    case TEXTURE_NORMALIZATION_CUBEMAP_SIGNED:
      pTexture = TextureManager()->SignedNormalizationCubemap();
      break;

    case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0:
    case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1: {
      int nTextureIndex = id - TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0;
      pTexture = m_pCurrentFrameBufferCopyTexture[nTextureIndex];
    } break;

    case TEXTURE_COLOR_CORRECTION_VOLUME_0:
    case TEXTURE_COLOR_CORRECTION_VOLUME_1:
    case TEXTURE_COLOR_CORRECTION_VOLUME_2:
    case TEXTURE_COLOR_CORRECTION_VOLUME_3:
      pTexture = TextureManager()->ColorCorrectionTexture(
          id - TEXTURE_COLOR_CORRECTION_VOLUME_0);
      break;

    case TEXTURE_SHADOW_NOISE_2D:
      pTexture = TextureManager()->ShadowNoise2D();
      break;

    case TEXTURE_IDENTITY_LIGHTWARP:
      pTexture = TextureManager()->IdentityLightWarp();
      return;

    case TEXTURE_MORPH_ACCUMULATOR:
      pTexture = g_pMorphMgr->MorphAccumulator();
      break;

    case TEXTURE_MORPH_WEIGHTS:
      pTexture = g_pMorphMgr->MorphWeights();
      break;
  }

  if (pTexture) {
    *pWidth = pTexture->GetActualWidth();
    *pHeight = pTexture->GetActualHeight();
  } else {
    Warning(
        "GetStandardTextureDimensions: Couldn't find the texture to get the "
        "dimensions!\n");
    *pWidth = *pHeight = -1;
  }
}

void CMatRenderContext::FogColor3f(float r, float g, float b) {
  unsigned char fogColor[3];
  fogColor[0] = std::clamp((int)(r * 255.0f), 0, 255);
  fogColor[1] = std::clamp((int)(g * 255.0f), 0, 255);
  fogColor[2] = std::clamp((int)(b * 255.0f), 0, 255);
  g_pShaderAPI->SceneFogColor3ub(fogColor[0], fogColor[1], fogColor[2]);
}

void CMatRenderContext::FogColor3fv(const float *rgb) {
  unsigned char fogColor[3];
  fogColor[0] = std::clamp((int)(rgb[0] * 255.0f), 0, 255);
  fogColor[1] = std::clamp((int)(rgb[1] * 255.0f), 0, 255);
  fogColor[2] = std::clamp((int)(rgb[2] * 255.0f), 0, 255);
  g_pShaderAPI->SceneFogColor3ub(fogColor[0], fogColor[1], fogColor[2]);
}

void CMatRenderContext::SetFlashlightMode(bool bEnable) {
  if (bEnable != m_bFlashlightEnable) {
    g_pShaderAPI->FlushBufferedPrimitives();
    m_bFlashlightEnable = bEnable;
  }
}

bool CMatRenderContext::GetFlashlightMode() const {
  return m_bFlashlightEnable;
}

void CMatRenderContext::SetFlashlightStateEx(
    const FlashlightState_t &state, const VMatrix &worldToTexture,
    ITexture *pFlashlightDepthTexture) {
  g_pShaderAPI->SetFlashlightStateEx(state, worldToTexture,
                                     pFlashlightDepthTexture);
  if (g_config.dxSupportLevel <= 70) {
    // Going to go ahead and set a single hardware light here to do all lighting
    // except for the spotlight falloff function, which is done with a texture.
    SetAmbientLight(0.0f, 0.0f, 0.0f);
    static Vector4D blackCube[6];
    int i;
    for (i = 0; i < 6; i++) {
      blackCube[i].Init(0.0f, 0.0f, 0.0f, 0.0f);
    }
    SetAmbientLightCube(blackCube);

    // Disable all the lights except for the first one.
    for (i = 1; i < HardwareConfig()->MaxNumLights(); ++i) {
      LightDesc_t desc;
      desc.m_Type = MATERIAL_LIGHT_DISABLE;
      SetLight(i, desc);
    }

    LightDesc_t desc;
    desc.m_Type = MATERIAL_LIGHT_POINT;
    desc.m_Attenuation0 = state.m_fConstantAtten;
    desc.m_Attenuation1 = state.m_fLinearAtten;
    desc.m_Attenuation2 = state.m_fQuadraticAtten;
    // flashlightfixme: I don't know why this scale has to be here to get fixed
    // function lighting to work.
    desc.m_Color.x = state.m_Color[0] * 17000.0f;
    desc.m_Color.y = state.m_Color[1] * 17000.0f;
    desc.m_Color.z = state.m_Color[2] * 17000.0f;
    desc.m_Position = state.m_vecLightOrigin;

    QAngle angles;
    QuaternionAngles(state.m_quatOrientation, angles);
    AngleVectors(angles, &desc.m_Direction);

    desc.m_Range = state.m_FarZ;
    desc.m_Falloff = 0.0f;
    SetLight(0, desc);
  }
}

void CMatRenderContext::SetScissorRect(const int nLeft, const int nTop,
                                       const int nRight, const int nBottom,
                                       const bool bEnableScissor) {
  g_pShaderAPI->SetScissorRect(nLeft, nTop, nRight, nBottom, bEnableScissor);
}

void CMatRenderContext::SetToneMappingScaleLinear(const Vector &scale) {
  g_pShaderAPI->SetToneMappingScaleLinear(scale);
}

void CMatRenderContext::BeginBatch(IMesh *pIndices) {
  Assert(!m_pBatchMesh && !m_pBatchIndices);
  m_pBatchIndices = pIndices;
}

void CMatRenderContext::BindBatch(IMesh *pVertices, IMaterial *pAutoBind) {
  m_pBatchMesh = GetDynamicMesh(false, pVertices, m_pBatchIndices, pAutoBind);
}

void CMatRenderContext::DrawBatch(int firstIndex, int numIndices) {
  Assert(m_pBatchMesh);
  m_pBatchMesh->Draw(firstIndex, numIndices);
}

void CMatRenderContext::EndBatch() {
  m_pBatchIndices = NULL;
  m_pBatchMesh = NULL;
}

bool CMatRenderContext::OnDrawMesh(IMesh *pMesh, int firstIndex,
                                   int numIndices) {
  SyncMatrices();
  return true;
}

bool CMatRenderContext::OnDrawMesh(IMesh *pMesh, CPrimList *pLists,
                                   int nLists) {
  SyncMatrices();
  return true;
}

//-----------------------------------------------------------------------------
// Methods related to morph accumulation
//-----------------------------------------------------------------------------
void CMatRenderContext::BeginMorphAccumulation() {
  g_pMorphMgr->BeginMorphAccumulation(m_pMorphRenderContext);
}

void CMatRenderContext::EndMorphAccumulation() {
  g_pMorphMgr->EndMorphAccumulation(m_pMorphRenderContext);
}

void CMatRenderContext::AccumulateMorph(IMorph *pMorph, int nMorphCount,
                                        const MorphWeight_t *pWeights) {
  g_pMorphMgr->AccumulateMorph(m_pMorphRenderContext, pMorph, nMorphCount,
                               pWeights);
}

bool CMatRenderContext::GetMorphAccumulatorTexCoord(Vector2D *pTexCoord,
                                                    IMorph *pMorph,
                                                    int nVertex) {
  return g_pMorphMgr->GetMorphAccumulatorTexCoord(m_pMorphRenderContext,
                                                  pTexCoord, pMorph, nVertex);
}

//-----------------------------------------------------------------------------
// Occlusion query support
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t CMatRenderContext::CreateOcclusionQueryObject() {
  OcclusionQueryObjectHandle_t h =
      g_pOcclusionQueryMgr->CreateOcclusionQueryObject();
  g_pOcclusionQueryMgr->OnCreateOcclusionQueryObject(h);
  return h;
}

int CMatRenderContext::OcclusionQuery_GetNumPixelsRendered(
    OcclusionQueryObjectHandle_t h) {
  return g_pOcclusionQueryMgr->OcclusionQuery_GetNumPixelsRendered(h, true);
}

void CMatRenderContext::SetFullScreenDepthTextureValidityFlag(bool bIsValid) {
  m_bFullFrameDepthIsValid = bIsValid;
}
