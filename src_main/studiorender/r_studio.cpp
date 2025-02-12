// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Routines for setting up to draw 3DStudio models.

#include "studiorender.h"

#include "datacache/imdlcache.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "studio.h"
#include "studiorendercontext.h"
#include "tier0/include/vprof.h"
#include "tier3/tier3.h"

#include "tier0/include/memdbgon.h"

// Figures out what kind of lighting we're gonna want
SOURCE_FORCEINLINE StudioModelLighting_t CStudioRender::R_StudioComputeLighting(
    IMaterial *pMaterial, int materialFlags, ColorMeshInfo_t *pColorMeshes) {
  // Here, we only do software lighting when the following conditions are met.
  // 1) The material is vertex lit and we don't have hardware lighting
  // 2) We're drawing an eyeball
  // 3) We're drawing mouth-lit stuff

  // TODO(d.rattman): When we move software lighting into the material system,
  // only need to test if it's vertex lit

  Assert(pMaterial);
  bool doMouthLighting = materialFlags && (m_pStudioHdr->nummouths >= 1);
  bool doSoftwareLighting =
      doMouthLighting ||
      (pMaterial->IsVertexLit() && pMaterial->NeedsSoftwareLighting());

  if (!m_pRC->m_Config.m_bSupportsVertexAndPixelShaders) {
    if (!doSoftwareLighting && pColorMeshes) {
      pMaterial->SetUseFixedFunctionBakedLighting(true);
    } else {
      doSoftwareLighting = true;
      pMaterial->SetUseFixedFunctionBakedLighting(false);
    }
  }

  StudioModelLighting_t lighting = LIGHTING_HARDWARE;
  if (doMouthLighting)
    lighting = LIGHTING_MOUTH;
  else if (doSoftwareLighting)
    lighting = LIGHTING_SOFTWARE;

  return lighting;
}

IMaterial *CStudioRender::R_StudioSetupSkinAndLighting(
    IMatRenderContext *pRenderContext, int index, IMaterial **ppMaterials,
    int materialFlags, void /*IClientRenderable*/ *pClientRenderable,
    ColorMeshInfo_t *pColorMeshes, StudioModelLighting_t &lighting) {
  VPROF("R_StudioSetupSkin");
  IMaterial *pMaterial = nullptr;
  bool bCheckForConVarDrawTranslucentSubModels = false;
  if (m_pRC->m_Config.bWireframe && !m_pRC->m_pForcedMaterial) {
    if (m_pRC->m_Config.bDrawZBufferedWireframe)
      pMaterial = m_pMaterialMRMWireframeZBuffer;
    else
      pMaterial = m_pMaterialMRMWireframe;
  } else if (m_pRC->m_Config.bShowEnvCubemapOnly) {
    pMaterial = m_pMaterialModelEnvCubemap;
  } else {
    if (!m_pRC->m_pForcedMaterial &&
        (m_pRC->m_nForcedMaterialType != OVERRIDE_DEPTH_WRITE)) {
      pMaterial = ppMaterials[index];
      if (!pMaterial) {
        Assert(0);
        return 0;
      }
    } else {
      materialFlags = 0;
      pMaterial = m_pRC->m_pForcedMaterial;
      if (m_pRC->m_nForcedMaterialType == OVERRIDE_BUILD_SHADOWS) {
        // Connect the original material up to the shadow building material
        // Also bind the original material so its proxies are in the correct
        // state
        static unsigned int translucentCache = 0;
        IMaterialVar *pOriginalMaterialVar =
            pMaterial->FindVarFast("$translucent_material", &translucentCache);
        Assert(pOriginalMaterialVar);
        IMaterial *pOriginalMaterial = ppMaterials[index];
        if (pOriginalMaterial) {
          // Disable any alpha modulation on the original material that was left
          // over from when it was last rendered
          pOriginalMaterial->AlphaModulate(1.0f);
          pRenderContext->Bind(pOriginalMaterial, pClientRenderable);
          if (pOriginalMaterial->IsTranslucent() ||
              pOriginalMaterial->IsAlphaTested()) {
            pOriginalMaterialVar->SetMaterialValue(pOriginalMaterial);
          } else {
            pOriginalMaterialVar->SetMaterialValue(nullptr);
          }
        } else {
          pOriginalMaterialVar->SetMaterialValue(nullptr);
        }
      } else if (m_pRC->m_nForcedMaterialType == OVERRIDE_DEPTH_WRITE) {
        // Disable any alpha modulation on the original material that was left
        // over from when it was last rendered
        ppMaterials[index]->AlphaModulate(1.0f);

        // Bail if the material is still considered translucent after setting
        // the AlphaModulate to 1.0
        if (ppMaterials[index]->IsTranslucent()) {
          return nullptr;
        }

        static unsigned int originalTextureVarCache = 0;
        IMaterialVar *pOriginalTextureVar = ppMaterials[index]->FindVarFast(
            "$basetexture", &originalTextureVarCache);

        // Select proper override material
        int nAlphaTest = (int)(ppMaterials[index]->IsAlphaTested() &&
                               pOriginalTextureVar
                                   ->IsTexture());  // alpha tested base texture
        int nNoCull = (int)ppMaterials[index]->IsTwoSided();
        pMaterial = m_pDepthWrite[nAlphaTest][nNoCull];

        // If we're alpha tested, we should set up the texture variables from
        // the original material
        if (nAlphaTest != 0) {
          static unsigned int originalTextureFrameVarCache = 0;
          IMaterialVar *pOriginalTextureFrameVar =
              ppMaterials[index]->FindVarFast("$frame",
                                              &originalTextureFrameVarCache);
          static unsigned int originalAlphaRefCache = 0;
          IMaterialVar *pOriginalAlphaRefVar = ppMaterials[index]->FindVarFast(
              "$AlphaTestReference", &originalAlphaRefCache);

          static unsigned int textureVarCache = 0;
          IMaterialVar *pTextureVar =
              pMaterial->FindVarFast("$basetexture", &textureVarCache);
          static unsigned int textureFrameVarCache = 0;
          IMaterialVar *pTextureFrameVar =
              pMaterial->FindVarFast("$frame", &textureFrameVarCache);
          static unsigned int alphaRefCache = 0;
          IMaterialVar *pAlphaRefVar =
              pMaterial->FindVarFast("$AlphaTestReference", &alphaRefCache);

          if (pOriginalTextureVar->IsTexture())  // If $basetexture is defined
          {
            if (pTextureVar && pOriginalTextureVar) {
              pTextureVar->SetTextureValue(
                  pOriginalTextureVar->GetTextureValue());
            }

            if (pTextureFrameVar && pOriginalTextureFrameVar) {
              pTextureFrameVar->SetIntValue(
                  pOriginalTextureFrameVar->GetIntValue());
            }

            if (pAlphaRefVar && pOriginalAlphaRefVar) {
              pAlphaRefVar->SetFloatValue(
                  pOriginalAlphaRefVar->GetFloatValue());
            }
          }
        }
      }
    }

    // Set this bool to check after the bind below
    bCheckForConVarDrawTranslucentSubModels = true;

    if (m_pRC->m_nForcedMaterialType != OVERRIDE_DEPTH_WRITE) {
      // Try to set the alpha based on the blend
      pMaterial->AlphaModulate(m_pRC->m_AlphaMod);

      // Try to set the color based on the colormod
      pMaterial->ColorModulate(m_pRC->m_ColorMod[0], m_pRC->m_ColorMod[1],
                               m_pRC->m_ColorMod[2]);
    }
  }

  lighting = R_StudioComputeLighting(pMaterial, materialFlags, pColorMeshes);
  if (lighting == LIGHTING_MOUTH) {
    if (!m_pRC->m_Config.bTeeth || !R_TeethAreVisible()) return nullptr;
    // skin it and light it, but only if we need to.
    if (m_pRC->m_Config.m_bSupportsVertexAndPixelShaders) {
      R_MouthSetupVertexShader(pMaterial);
    }
  }

  pRenderContext->Bind(pMaterial, pClientRenderable);

  if (bCheckForConVarDrawTranslucentSubModels) {
    bool translucent = pMaterial->IsTranslucent();

    if ((m_bDrawTranslucentSubModels && !translucent) ||
        (!m_bDrawTranslucentSubModels && translucent)) {
      m_bSkippedMeshes = true;
      return nullptr;
    }
  }

  return pMaterial;
}

/* Based on the body part, figure out which mesh it should be using. */
int R_StudioSetupModel(int bodypart, int entity_body,
                       mstudiomodel_t **ppSubModel,
                       const studiohdr_t *pStudioHdr) {
  if (bodypart > pStudioHdr->numbodyparts) {
    ConDMsg("R_StudioSetupModel: no such bodypart %d\n", bodypart);
    bodypart = 0;
  }

  mstudiobodyparts_t *pbodypart = pStudioHdr->pBodypart(bodypart);

  int index = entity_body / pbodypart->base;
  index = index % pbodypart->nummodels;

  Assert(ppSubModel);
  *ppSubModel = pbodypart->pModel(index);

  return index;
}

// Computes PoseToWorld from BoneToWorld
void ComputePoseToWorld(matrix3x4_t *pPoseToWorld, studiohdr_t *pStudioHdr,
                        int boneMask, const Vector &vecViewOrigin,
                        const matrix3x4_t *pBoneToWorld) {
  if (pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP) {
    // by definition, these always have an identity poseToBone transform
    MatrixCopy(pBoneToWorld[0], pPoseToWorld[0]);
    return;
  }

  if (!pStudioHdr->pLinearBones()) {
    // convert bone to world transformations into pose to world transformations
    for (int i = 0; i < pStudioHdr->numbones; i++) {
      mstudiobone_t *pCurBone = pStudioHdr->pBone(i);
      if (!(pCurBone->flags & boneMask)) continue;

      ConcatTransforms(pBoneToWorld[i], pCurBone->poseToBone, pPoseToWorld[i]);
    }
  } else {
    mstudiolinearbone_t *pLinearBones = pStudioHdr->pLinearBones();

    // convert bone to world transformations into pose to world transformations
    for (int i = 0; i < pStudioHdr->numbones; i++) {
      if (!(pLinearBones->flags(i) & boneMask)) continue;

      ConcatTransforms(pBoneToWorld[i], pLinearBones->poseToBone(i),
                       pPoseToWorld[i]);
    }
  }
}
