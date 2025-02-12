// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A shader that builds the shadow using render-to-texture

#include "shaderlib/CShader.h"

 
#include "tier0/include/memdbgon.h"

DEFINE_FALLBACK_SHADER(ShadowBuild, ShadowBuild_DX6)

BEGIN_SHADER_FLAGS(ShadowBuild_DX6, "Help for ShadowBuild", SHADER_NOT_EDITABLE)
BEGIN_SHADER_PARAMS
END_SHADER_PARAMS

SHADER_INIT_PARAMS() {}

SHADER_INIT {}

SHADER_DRAW {
  SHADOW_STATE {
    pShaderShadow->EnableAlphaWrites(true);
    pShaderShadow->EnableConstantColor(true);
    pShaderShadow->EnableConstantAlpha(true);
    pShaderShadow->EnableDepthWrites(false);
    pShaderShadow->EnableDepthTest(false);
    pShaderShadow->DrawFlags(SHADER_DRAW_POSITION);
    FogToGrey();
  }
  DYNAMIC_STATE {}
  Draw();
}
END_SHADER
