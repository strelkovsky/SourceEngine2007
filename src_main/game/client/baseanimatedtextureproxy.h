// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:
//
// $NoKeywords: $

#ifndef BASEANIMATEDTEXTUREPROXY
#define BASEANIMATEDTEXTUREPROXY

#include "materialsystem/IMaterialProxy.h"

class IMaterial;
class IMaterialVar;

class CBaseAnimatedTextureProxy : public IMaterialProxy {
 public:
  CBaseAnimatedTextureProxy();
  virtual ~CBaseAnimatedTextureProxy();

  virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
  virtual void OnBind(void *pC_BaseEntity);
  virtual void Release() { delete this; }
  virtual IMaterial *GetMaterial();

 protected:
  // derived classes must implement this; it returns the time
  // that the animation began
  virtual float GetAnimationStartTime(void *pBaseEntity) = 0;

  // Derived classes may implement this if they choose;
  // this method is called whenever the animation wraps...
  virtual void AnimationWrapped(void *pBaseEntity) {}

 protected:
  void Cleanup();

  IMaterialVar *m_AnimatedTextureVar;
  IMaterialVar *m_AnimatedTextureFrameNumVar;
  float m_FrameRate;
  bool m_WrapAnimation;
};

#endif  // BASEANIMATEDTEXTUREPROXY