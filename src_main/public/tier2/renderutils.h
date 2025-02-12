// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A set of utilities to render standard shapes.

#ifndef SOURCE_TIER2_RENDERUTILS_H_
#define SOURCE_TIER2_RENDERUTILS_H_

#ifdef _WIN32
#pragma once
#endif

#include "Color.h"
#include "base/include/base_types.h"
#include "tier2/tier2.h"

class Vector;
class QAngle;
the_interface IMaterial;
struct matrix3x4_t;

// Renders a wireframe sphere.
void RenderWireframeSphere(const Vector &vCenter, f32 flRadius, int nTheta,
                           int nPhi, Color c, bool bZBuffer);

// Renders a sphere.
void RenderSphere(const Vector &vCenter, f32 flRadius, int nTheta, int nPhi,
                  Color c, bool bZBuffer);
void RenderSphere(const Vector &vCenter, f32 flRadius, int nTheta, int nPhi,
                  Color c, IMaterial *pMaterial);

// Renders a wireframe box relative to an origin.
void RenderWireframeBox(const Vector &vOrigin, const QAngle &angles,
                        const Vector &vMins, const Vector &vMaxs, Color c,
                        bool bZBuffer);

// Renders a swept wireframe box.
void RenderWireframeSweptBox(const Vector &vStart, const Vector &vEnd,
                             const QAngle &angles, const Vector &vMins,
                             const Vector &vMaxs, Color c, bool bZBuffer);

// Renders a solid box.
void RenderBox(const Vector &origin, const QAngle &angles, const Vector &mins,
               const Vector &maxs, Color c, bool bZBuffer,
               bool bInsideOut = false);
void RenderBox(const Vector &origin, const QAngle &angles, const Vector &mins,
               const Vector &maxs, Color c, IMaterial *pMaterial,
               bool bInsideOut = false);

// Renders axes, red->x, green->y, blue->z (axis aligned).
void RenderAxes(const Vector &vOrigin, f32 flScale, bool bZBuffer);
void RenderAxes(const matrix3x4_t &transform, f32 flScale, bool bZBuffer);

// Render a line.
void RenderLine(const Vector &v1, const Vector &v2, Color c, bool bZBuffer);

// Draws a triangle.
void RenderTriangle(const Vector &p1, const Vector &p2, const Vector &p3,
                    Color c, bool bZBuffer);
void RenderTriangle(const Vector &p1, const Vector &p2, const Vector &p3,
                    Color c, IMaterial *pMaterial);

// Draws a axis-aligned quad.
void RenderQuad(IMaterial *pMaterial, f32 x, f32 y, f32 w, f32 h, f32 z, f32 s0,
                f32 t0, f32 s1, f32 t1, const Color &clr);

// Renders a screen space quad.
void DrawScreenSpaceRectangle(
    IMaterial *pMaterial, int nDestX, int nDestY, int nWidth,
    int nHeight,  // Rect to draw into in screen space
    f32 flSrcTextureX0,
    f32 flSrcTextureY0,  // which texel you want to appear at destx/y
    f32 flSrcTextureX1, f32 flSrcTextureY1,       // which texel you want to
                                                  // appear at destx+width-1,
                                                  // desty+height-1
    int nSrcTextureWidth, int nSrcTextureHeight,  // needed for fixup
    void *pClientRenderable = nullptr,  // Used to pass to the bind proxies
    int nXDice = 1, int nYDice = 1);

#endif  // SOURCE_TIER2_RENDERUTILS_H_
