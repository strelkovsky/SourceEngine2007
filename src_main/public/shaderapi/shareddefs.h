// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: NOTE: This file is for backward compat! We'll get rid of it soon.
// Most of the contents of this file were moved into shaderpi/ishadershadow.h,
// shaderapi/ishaderdynamic.h, or shaderapi/shareddefs.h

#ifndef SHADERAPI_SHAREDDEFS_H
#define SHADERAPI_SHAREDDEFS_H

//-----------------------------------------------------------------------------
// Important enumerations
//-----------------------------------------------------------------------------
enum ShaderShadeMode_t { SHADER_FLAT = 0, SHADER_SMOOTH };

enum ShaderTexCoordComponent_t {
  SHADER_TEXCOORD_S = 0,
  SHADER_TEXCOORD_T,
  SHADER_TEXCOORD_U
};

enum ShaderTexFilterMode_t {
  SHADER_TEXFILTERMODE_NEAREST,
  SHADER_TEXFILTERMODE_LINEAR,
  SHADER_TEXFILTERMODE_NEAREST_MIPMAP_NEAREST,
  SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST,
  SHADER_TEXFILTERMODE_NEAREST_MIPMAP_LINEAR,
  SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR,
  SHADER_TEXFILTERMODE_ANISOTROPIC
};

enum ShaderTexWrapMode_t {
  SHADER_TEXWRAPMODE_CLAMP,
  SHADER_TEXWRAPMODE_REPEAT,
  SHADER_TEXWRAPMODE_BORDER
  // MIRROR? - probably don't need it.
};

//-----------------------------------------------------------------------------
// Sampler + texture stage identifiers
// NOTE: Texture stages are used only by fixed function shading algorithms
// Samplers are used to enable and bind textures + by programmable shading
// algorithms
//-----------------------------------------------------------------------------
enum TextureStage_t {
  SHADER_TEXTURE_STAGE0 = 0,
  SHADER_TEXTURE_STAGE1,
};

enum Sampler_t : int {
  SHADER_SAMPLER0 = 0,
  SHADER_SAMPLER1,
  SHADER_SAMPLER2,
  SHADER_SAMPLER3,
  SHADER_SAMPLER4,
  SHADER_SAMPLER5,
  SHADER_SAMPLER6,
  SHADER_SAMPLER7,
  SHADER_SAMPLER8,
  SHADER_SAMPLER9,
  SHADER_SAMPLER10,
  SHADER_SAMPLER11,
  SHADER_SAMPLER12,
  SHADER_SAMPLER13,
  SHADER_SAMPLER14,
  SHADER_SAMPLER15,
};

//-----------------------------------------------------------------------------
// Vertex texture sampler identifiers
//-----------------------------------------------------------------------------
enum VertexTextureSampler_t : int {
  SHADER_VERTEXTEXTURE_SAMPLER0 = 0,
  SHADER_VERTEXTEXTURE_SAMPLER1,
  SHADER_VERTEXTEXTURE_SAMPLER2,
  SHADER_VERTEXTEXTURE_SAMPLER3,
};

#if defined(REVERSE_DEPTH_ON_X360)
#define ReverseDepthOnX360() true
#else
#define ReverseDepthOnX360() false
#endif

#endif  // SHADERAPI_SHAREDDEFS_H
