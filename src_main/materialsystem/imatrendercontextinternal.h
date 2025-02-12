// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATERIALSYSTEM_IMATRENDERCONTEXTINTERNAL_H_
#define SOURCE_MATERIALSYSTEM_IMATRENDERCONTEXTINTERNAL_H_

#include "materialsystem/imesh.h"

// typedefs to allow use of delegation macros
typedef Vector4D LightCube_t[6];

the_interface IMatRenderContextInternal : public IMatRenderContext {
  // For now, stuck implementing these until IMaterialSystem is reworked
  bool Connect(CreateInterfaceFn) { return true; }
  void Disconnect() {}
  void *QueryInterface(const char *pszInterface) { return nullptr; }
  InitReturnVal_t Init() { return INIT_OK; }
  void Shutdown() {}

 public:
  virtual float GetFloatRenderingParameter(int parm_number) const = 0;
  virtual int GetIntRenderingParameter(int parm_number) const = 0;
  virtual Vector GetVectorRenderingParameter(int parm_number) const = 0;

  virtual void SwapBuffers() = 0;

  virtual void SetCurrentMaterialInternal(IMaterialInternal *
                                          pCurrentMaterial) = 0;
  virtual IMaterialInternal *GetCurrentMaterialInternal() const = 0;
  virtual int GetLightmapPage() = 0;
  virtual void ForceDepthFuncEquals(bool) = 0;

  virtual bool InFlashlightMode() const = 0;
  virtual void BindStandardTexture(Sampler_t, StandardTextureId_t) = 0;
  virtual void GetLightmapDimensions(int *, int *) = 0;
  virtual MorphFormat_t GetBoundMorphFormat() = 0;
  virtual ITexture *GetRenderTargetEx(int) = 0;
  virtual void DrawClearBufferQuad(unsigned char, unsigned char, unsigned char,
                                   unsigned char, bool, bool) = 0;

  virtual bool OnDrawMesh(IMesh * pMesh, int firstIndex, int numIndices) = 0;
  virtual bool OnDrawMesh(IMesh * pMesh, CPrimList * pLists, int nLists) = 0;
  virtual bool OnSetFlexMesh(IMesh * pStaticMesh, IMesh * pMesh,
                             int nVertexOffsetInBytes) = 0;
  virtual bool OnSetColorMesh(IMesh * pStaticMesh, IMesh * pMesh,
                              int nVertexOffsetInBytes) = 0;
  virtual bool OnSetPrimitiveType(IMesh * pMesh,
                                  MaterialPrimitiveType_t type) = 0;
  virtual bool OnFlushBufferedPrimitives() = 0;

  virtual void SyncMatrices() = 0;
  virtual void SyncMatrix(MaterialMatrixMode_t) = 0;

  virtual void ForceHardwareSync() = 0;
  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;

  virtual void SetFrameTime(float frameTime) = 0;
  virtual void SetCurrentProxy(void *pProxy) = 0;

  virtual CMatCallQueue *GetCallQueueInternal() = 0;
};

#endif  // SOURCE_MATERIALSYSTEM_IMATRENDERCONTEXTINTERNAL_H_
