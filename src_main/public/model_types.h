// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#if !defined(MODEL_TYPES_H)
#define MODEL_TYPES_H

#define STUDIO_NONE 0x00000000
#define STUDIO_RENDER 0x00000001
#define STUDIO_VIEWXFORMATTACHMENTS 0x00000002
#define STUDIO_DRAWTRANSLUCENTSUBMODELS 0x00000004
#define STUDIO_TWOPASS 0x00000008
#define STUDIO_STATIC_LIGHTING 0x00000010
#define STUDIO_WIREFRAME 0x00000020
#define STUDIO_ITEM_BLINK 0x00000040
#define STUDIO_NOSHADOWS 0x00000080
#define STUDIO_WIREFRAME_VCOLLIDE 0x00000100

// Not a studio flag, but used to flag model as a non-sorting brush model
#define STUDIO_TRANSPARENCY 0x80000000

// Not a studio flag, but used to flag model as using shadow depth material
// override
#define STUDIO_SHADOWDEPTHTEXTURE 0x40000000

enum modtype_t { mod_bad = 0, mod_brush, mod_sprite, mod_studio };

#endif  // MODEL_TYPES_H
